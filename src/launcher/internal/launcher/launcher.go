// Package launcher provides the main Launcher struct and orchestration logic
// for the LinuxCNC Go launcher.
//
// This package implements M1–M7 of the Go launcher (task + display launch +
// orderly shutdown).
package launcher

import (
	"bufio"
	"errors"
	"fmt"
	"log/slog"
	"os"
	"os/exec"
	"os/signal"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"syscall"
	"time"
	"unsafe"

	hal "github.com/sittner/linuxcnc/src/launcher/pkg/hal"

	halcmd "github.com/sittner/linuxcnc/src/launcher/internal/halcmd"

	"github.com/sittner/linuxcnc/src/launcher/internal/config"
	"github.com/sittner/linuxcnc/src/launcher/internal/halfile"
	"github.com/sittner/linuxcnc/src/launcher/internal/lockfile"
	"github.com/sittner/linuxcnc/src/launcher/internal/realtime"
	"github.com/sittner/linuxcnc/src/launcher/pkg/gomodule"
	"github.com/sittner/linuxcnc/src/launcher/pkg/inifile"
)

// Options holds the parsed command-line options.
type Options struct {
	// Debug enables verbose script tracing (corresponds to -d flag).
	Debug bool
	// Verbose enables verbose message printing (corresponds to -v flag).
	Verbose bool
	// NoRedirect disables stdout/stderr redirection; used for tests (-r flag).
	NoRedirect bool
	// UseLast causes the last-used INI file to be loaded (-l flag).
	UseLast bool
	// ContinueOnError instructs HAL file execution to continue despite errors (-k flag).
	ContinueOnError bool
	// TpMod is the optional custom trajectory-planning module name (-t flag).
	TpMod string
	// HomeMod is the optional custom homing module name (-m flag).
	HomeMod string
	// HalLibDirs are additional directories prepended to HALLIB_PATH (-H flag).
	HalLibDirs []string
	// IniFile is the resolved absolute path to the INI configuration file.
	IniFile string
}

// Launcher orchestrates the LinuxCNC startup and shutdown sequence.
type Launcher struct {
	opts         Options
	ini          *inifile.IniFile
	logger       *slog.Logger
	lock         *lockfile.LockFile // flock-based instance lock
	rtMgr        *realtime.Manager  // realtime environment manager
	cleanupOnce  sync.Once          // ensures cleanup runs exactly once
	appProcesses []*exec.Cmd        // [APPLICATIONS]APP background processes
	halComp      *hal.Component     // launcher's HAL component (like halcmd's hal_init)
	goModules    []gomodule.Module  // Go plugin modules loaded via "load" command
	cModules     []*cModule         // C plugin modules loaded via "load" command
	cModArena    []unsafe.Pointer   // arena-tracked C strings freed in destroyCModules
}

// New creates a new Launcher with the given options and logger.
// If logger is nil, a default structured logger writing to stderr is used.
func New(opts Options, logger *slog.Logger) *Launcher {
	if logger == nil {
		logger = slog.New(slog.NewTextHandler(os.Stderr, nil))
	}
	return &Launcher{opts: opts, logger: logger}
}

// Run executes the full LinuxCNC startup sequence.
//
// Implemented milestones M1–M7:
// Startup order matches scripts/linuxcnc.in:
//  1. Sets up environment variables (INI_FILE_NAME exported before any subprocess).
//  2. Acquires the lock file.
//  3. Parses the INI file.
//  4. Validates cross-section INI dependencies (validateDependencies).
//  5. Starts NML server (emcsvr cmod plugin) — only if [TASK]TASK is configured (M5).
//  6. Starts the realtime environment (M4).
//     6.5. Loads threads HAL component (creates servo-thread, optionally base-thread).
//  7. Starts iocontrol via halcmd loadusr -Wn — only if [TASK]TASK is configured (M5).
//  8. Starts halui via halcmd loadusr -Wn — only if [HAL]HALUI is configured (M5).
//  9. Preloads tpmod/homemod — only if [TASK]TASK is configured (M4).
//  10. Executes [HAL]HALFILE entries (M3, step 4.3.6).
//  11. Starts the task controller in background — only if [TASK]TASK is configured (M6, step 4.3.7).
//  12. Executes [HAL]HALCMD entries (M6, step 4.3.8).
//  13. Loads retained signals if any are present (M6, step 4.3.9).
//  14. Starts HAL threads (M6, step 4.3.10).
//  15. Launches [APPLICATIONS]APP entries in background (M6, step 4.3.11).
//  16. If [DISPLAY]DISPLAY is configured: launches the display in the foreground — blocks
//     until the user closes the GUI (M6, step 4.3.12).
//     Otherwise (HAL-only mode): blocks until SIGINT/SIGTERM is received.
//  17. Shuts down in ordered sequence (M7): display helpers → AXIS quit → [HAL]SHUTDOWN →
//     user-space → halcmd stop → halcmd unload all → wait → realtime stop → NML shm → lock.
//
// Operational modes:
//   - Full CNC mode: [TASK]TASK is set → linuxcncsvr, iocontrol, task controller and
//     optionally halui and a display are started.
//   - HAL-only mode: [TASK]TASK is not set → only the realtime environment, HAL files,
//     and HAL threads are started.  The launcher blocks until a signal is received.
//     This mode is suitable for machines that only require HAL-based automation without
//     the full CNC stack (G-code interpreter, trajectory planner, etc.).
//
// Note: POSTGUI_HALFILE loading is intentionally omitted here; it is the
// responsibility of the display GUI (AXIS, QtVCP, gmoccapy, etc.) to load
// its own post-GUI HAL files after creating its HAL pins.
func (l *Launcher) Run() (runErr error) {
	l.setupEnvironment()

	// Export INI file path and config directory so that child processes
	// (linuxcncsvr, iocontrol, task, etc.) can find the configuration.
	// These must be set before startServer() is called.
	l.setConfigEnv()

	l.logger.Info("acquiring lock file")
	l.lock = lockfile.New(lockfile.LockFilePath)
	if err := l.lock.Acquire(); err != nil {
		return err
	}
	// M7: Single deferred cleanup replaces individual defers.
	// cleanup() is idempotent (sync.Once) so it is also safe to call from
	// the signal handler goroutine below.
	defer func() {
		if runErr != nil {
			l.logger.Error("startup failed", "error", runErr)
		}
		l.cleanup()
	}()

	// M7: Trap SIGINT and SIGTERM so that Ctrl-C triggers an ordered shutdown
	// instead of an abrupt process exit that leaves HAL loaded.
	// This mirrors scripts/linuxcnc.in line 778:
	//   trap 'Cleanup ; exit 0' SIGINT SIGTERM
	sigCh := make(chan os.Signal, 1)
	signal.Notify(sigCh, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		sig := <-sigCh
		l.logger.Info("received signal, shutting down", "signal", sig)
		l.cleanup()
		os.Exit(0)
	}()

	l.logger.Info("parsing INI file", "path", l.opts.IniFile)
	ini, err := inifile.Parse(l.opts.IniFile)
	if err != nil {
		return err
	}
	l.ini = ini

	// If the INI file contains #INCLUDE directives, write a fully-expanded copy
	// alongside the original (e.g. "foo.ini.expanded") and update the path used
	// for all subsequent subprocess launches.  Subprocesses such as iocontrol,
	// halcmd, and milltask use the C IniFile class which does not handle
	// #INCLUDE, so they must receive the pre-expanded file.
	// This mirrors the handle_includes() function in scripts/linuxcnc.in.
	effectiveIni, err := inifile.WriteExpanded(l.opts.IniFile)
	if err != nil {
		return fmt.Errorf("expanding INI file includes: %w", err)
	}
	if effectiveIni != l.opts.IniFile {
		l.logger.Info("INI file expanded", "original", l.opts.IniFile, "expanded", effectiveIni)
		l.opts.IniFile = effectiveIni
		l.setConfigEnv()
	}

	// Change to the INI file's directory so that relative paths in the INI
	// (e.g., sim.var, sim.tbl, position.txt) resolve correctly.
	// This mirrors scripts/linuxcnc.in line 783: cd "$INI_DIR"
	iniDir := filepath.Dir(l.opts.IniFile)
	if err := os.Chdir(iniDir); err != nil {
		return fmt.Errorf("chdir to INI directory %s: %w", iniDir, err)
	}
	l.logger.Info("changed working directory", "dir", iniDir)

	l.logConfiguration()

	// Determine operational mode from the INI configuration.
	// hasTask is true when [TASK]TASK is set; this enables the full CNC stack
	// (NML server, iocontrol, motion modules, task controller).
	// hasDisplay is true when [DISPLAY]DISPLAY is set; if false the launcher
	// runs in HAL-only mode and blocks waiting for a shutdown signal.
	hasTask := l.ini.Get("TASK", "TASK") != ""
	hasDisplay := l.ini.Get("DISPLAY", "DISPLAY") != ""

	// Validate cross-section INI dependencies before starting any processes.
	// This catches contradictory configurations (e.g. [HAL]HALUI without
	// [TASK]TASK) and missing required sections early, rather than failing
	// with cryptic errors at runtime.
	if err := l.validateDependencies(); err != nil {
		return fmt.Errorf("configuration error: %w", err)
	}

	if hasTask {
		l.logger.Info("mode: full CNC (task controller enabled)")
	} else {
		l.logger.Info("mode: HAL-only (no task controller)")
	}

	// Pre-launch validation checks (mirrors scripts/linuxcnc.in lines 495–530
	// and 791–812): run after INI parsing + include expansion + chdir, but
	// before startServer().

	// 1. [EMC]VERSION check + update_ini (lines 495–508).
	if err := l.checkVersion(); err != nil {
		if errors.Is(err, ErrUpdateCancelled) {
			return nil // user cancelled — clean exit
		}
		return err
	}

	// 2. PlasmaC migration check (lines 511–522).
	if err := l.checkPlasmaC(); err != nil {
		if errors.Is(err, ErrPlasmaC) {
			return nil // PlasmaC handled — clean exit
		}
		return err
	}

	// 3. check_config.tcl validation (lines 524–529).
	// Only run for full CNC mode: the Tcl validator expects [KINS], [TRAJ],
	// [EMCMOT], and [RS274NGC] sections which are not required in HAL-only mode.
	if hasTask {
		if err := l.checkConfig(); err != nil {
			return err
		}
	}

	// 4. Intro graphic popup (lines 791–812).
	l.showIntroGraphic()

	// --- M5: Process Manager ---

	// Start in-process NML server before realtime — only when the task
	// controller is running.  The NML server creates the shared memory buffers
	// that realtime components depend on.  In HAL-only mode there are no NML
	// buffers to serve.
	// This mirrors scripts/linuxcnc.in lines 817–825.
	if hasTask {
		if err := l.startServer(); err != nil {
			return fmt.Errorf("starting NML server: %w", err)
		}
	}

	// --- M4: Realtime Manager ---
	l.rtMgr = realtime.New(l.logger)
	l.logger.Info("starting realtime environment")
	if err := l.rtMgr.Start(); err != nil {
		return fmt.Errorf("realtime start failed: %w", err)
	}

	// Initialize the in-process RTAPI/HAL environment.  Sets up HAL shared
	// memory, starts the message queue thread, and prepares for RT module
	// loading via dlopen.
	// Must be called before hal.NewComponent() / hal_init().
	l.logger.Info("initializing RTAPI app (in-process)")
	if err := halcmd.RtapiAppInit(); err != nil {
		return fmt.Errorf("rtapi app init: %w", err)
	}

	// Initialize HAL connection — same as halcmd calling hal_init("halcmd").
	// This is required before any hal-go API calls (StartThreads, StopThreads, etc.).
	halComp, err := hal.NewComponent("launcher")
	if err != nil {
		return fmt.Errorf("hal init: %w", err)
	}
	l.halComp = halComp
	// Mark the launcher's HAL component ready — halcmd always calls hal_ready()
	// immediately after hal_init(). Without this, other components that poll for
	// the launcher component being ready will time out.
	if err := halComp.Ready(); err != nil {
		return fmt.Errorf("hal ready: %w", err)
	}

	// Load the threads HAL component to create RT threads (servo-thread,
	// optionally base-thread). Thread creation has been decoupled from
	// motmod — the launcher now loads the threads component which runs
	// in-process via dlopen with proper RT scheduling.
	// This must happen before motmod, HAL files, or any component that
	// uses addf to attach functions to threads.
	if err := l.loadThreads(); err != nil {
		return fmt.Errorf("loading threads: %w", err)
	}

	// Load the iocontrol C module plugin — only when the task controller
	// is running.  iocontrol is a HAL userspace component that communicates
	// with the task controller via NML.  Its Start() (which spawns
	// the NML processing thread) is called later in startCModules().
	// validateDependencies() ensures [EMCIO]EMCIO is not set without [TASK]TASK.
	if hasTask {
		if err := l.startIOControl(); err != nil {
			return fmt.Errorf("starting iocontrol: %w", err)
		}
	}

	// Start halui if configured.
	// halui is also a HAL userspace component managed by HAL's lifecycle.
	if err := l.startHalUI(); err != nil {
		return fmt.Errorf("starting halui: %w", err)
	}

	// Pre-load trajectory planner and homing modules before HAL file execution
	// — only for full CNC mode.
	// This mirrors scripts/linuxcnc.in lines 865-868.
	if hasTask {
		if err := l.preloadMotionModules(); err != nil {
			return fmt.Errorf("preloading motion modules: %w", err)
		}
	}

	// M3: Load HAL files.
	halExec := halfile.New(l.ini, os.Getenv("HALLIB_PATH"), l.logger, l.opts.IniFile)
	halResult, err := halExec.ParseAll()
	if err != nil {
		return fmt.Errorf("HAL file parsing failed: %w", err)
	}

	// Phase 1: Load components (loadusr + load plugins + loadrt).
	// Plugin modules ("load" command) are loaded between loadusr and loadrt
	// so that userspace plugins that prepare shared state are ready when
	// realtime modules initialise.
	if err := halResult.Load(func(path string, name string, args []string) error {
		cmodPath := resolveCModulePath(path)
		if cModuleExists(cmodPath) {
			return l.loadCPlugin(cmodPath, name, args)
		}
		return l.loadGoPlugin(resolveGoModulePath(path), name, args)
	}); err != nil {
		if !l.opts.ContinueOnError {
			return fmt.Errorf("HAL component loading failed: %w", err)
		}
		l.logger.Warn("HAL component loading error (continuing)", "error", err)
	}

	// Phase 2: Execute HAL wiring commands (net, addf, setp, etc.).
	if err := halResult.Execute(); err != nil {
		if !l.opts.ContinueOnError {
			return fmt.Errorf("HAL file execution failed: %w", err)
		}
		l.logger.Warn("HAL file execution error (continuing)", "error", err)
	}

	// --- M6: Task + Display Launch ---

	// 6a. Fire-and-forget task controller — only if [TASK]TASK is configured
	// (step 4.3.7). The task controller is started without waiting for the
	// "inihal" component to register; HAL threads (step 6d) must be running
	// before milltask can complete its motion initialization.
	if hasTask {
		if err := l.startTask(); err != nil {
			return fmt.Errorf("starting task: %w", err)
		}
	}

	// 6b. Execute [HAL]HALCMD entries (step 4.3.8).
	// These run after the task controller has started, matching the bash launcher.
	if err := halExec.ExecuteHalCommands(); err != nil {
		if !l.opts.ContinueOnError {
			return fmt.Errorf("HAL command execution failed: %w", err)
		}
		l.logger.Warn("HAL command execution error (continuing)", "error", err)
	}

	// 6c. Load retained signals if any are present (step 4.3.9).
	if err := l.loadRetain(); err != nil {
		if !l.opts.ContinueOnError {
			return fmt.Errorf("loading retain: %w", err)
		}
		l.logger.Warn("retain load error (continuing)", "error", err)
	}

	// 6d. Lock C plugin memory and start HAL threads (step 4.3.10).
	l.lockCModules()
	if err := l.startHalThreads(); err != nil {
		return fmt.Errorf("hal start threads: %w", err)
	}

	// 6d.3. Start Go plugin modules (Start() is called after HAL threads are running).
	if err := l.startGoModules(); err != nil {
		return fmt.Errorf("Go module start failed: %w", err)
	}

	// 6d.4. Start C plugin modules.
	if err := l.startCModules(); err != nil {
		return fmt.Errorf("C module start failed: %w", err)
	}

	// 6e. Launch application entries ([APPLICATIONS]APP) in background (step 4.3.11).
	if err := l.runApplications(); err != nil {
		l.logger.Warn("application launch error", "error", err)
	}

	// 6f. Launch display in foreground or wait in HAL-only mode (step 4.3.12).
	if hasDisplay {
		if err := l.startDisplay(); err != nil {
			return fmt.Errorf("display: %w", err)
		}
	} else {
		// HAL-only mode: no display is configured.  Log and block until the
		// signal handler goroutine calls os.Exit(0) on SIGINT/SIGTERM.
		l.logger.Info("HAL-only mode: no display configured, waiting for shutdown signal (Ctrl+C to stop)")
		// select{} blocks indefinitely.  The signal handler goroutine calls
		// l.cleanup() and os.Exit(0), so deferred cleanup is not needed here.
		select {}
	}

	// Display has exited — cleanup runs via deferred l.cleanup():
	//   kill displays → axis-remote quit → [HAL]SHUTDOWN → kill user-space →
	//   halcmd stop → halcmd unload all → wait → realtime stop → NML shm → lock
	return nil
}

// startServer loads and starts the NML server as a cmod plugin.
//
// The NML server creates shared memory buffers that realtime components and
// NML clients (iocontrol, task) depend on, so it must be started before
// realtime init.  It is loaded via the standard cmod mechanism (dlopen +
// New/Start) and its Stop/Destroy are handled by the normal cmod teardown.
//
// After Start(), a brief 100 ms window is given to let NML channels initialize.
func (l *Launcher) startServer() error {
	l.logger.Info("loading NML server (cmod plugin)")

	path := resolveCModulePath("emcsvr")
	if !cModuleExists(path) {
		return fmt.Errorf("NML server C module not found: %s", path)
	}

	if err := l.loadCPlugin(path, "emcsvr", nil); err != nil {
		return fmt.Errorf("loading NML server cmod: %w", err)
	}

	if err := l.startCModuleByName("emcsvr"); err != nil {
		return fmt.Errorf("starting NML server cmod: %w", err)
	}

	// Brief startup window to let NML channels initialize.
	time.Sleep(100 * time.Millisecond)
	l.logger.Info("NML server running (cmod plugin)")
	return nil
}

// startIOControl loads and initialises the IO controller as a C module plugin.
//
// EMCIO resolution: [IO]IO → [EMCIO]EMCIO → default "io".
// The resolved name is looked up in EMC2_CMOD_DIR as a .so file.
func (l *Launcher) startIOControl() error {
	emcio := l.ini.Get("IO", "IO")
	if emcio == "" {
		emcio = l.ini.Get("EMCIO", "EMCIO")
	}
	if emcio == "" {
		emcio = "io"
	}

	path := resolveCModulePath(emcio)
	if !cModuleExists(path) {
		return fmt.Errorf("IO controller C module not found: %s", path)
	}

	return l.loadCPlugin(path, "iocontrol", nil)
}

// startHalUI starts the halui process via hal.LoadUSR, if configured.
//
// This mirrors scripts/linuxcnc.in lines 852–861:
//
//	$HALCMD loadusr -Wn halui $HALUI -ini "$INIFILE"
//
// If [HAL]HALUI is not set, this is a no-op.
func (l *Launcher) startHalUI() error {
	halui := l.ini.Get("HAL", "HALUI")
	if halui == "" {
		l.logger.Debug("HALUI not configured, skipping")
		return nil
	}

	l.logger.Info("starting HAL user interface", "program", halui)

	if err := halcmd.LoadUSR(&halcmd.LoadUSROptions{WaitReady: true, WaitName: "halui"}, halui, "-ini", l.opts.IniFile); err != nil {
		return fmt.Errorf("loadusr halui %s: %w", halui, err)
	}

	return nil
}

// setConfigEnv exports INI_FILE_NAME and CONFIG_DIR to the process environment
// so that child processes (linuxcncsvr, iocontrol, task, etc.) can find the
// configuration.  It is called both at initial startup and again after INI
// include expansion (which may change the effective path).
func (l *Launcher) setConfigEnv() {
	if l.opts.IniFile != "" {
		os.Setenv("INI_FILE_NAME", l.opts.IniFile)
		os.Setenv("CONFIG_DIR", filepath.Dir(l.opts.IniFile))
	}
}

// setupEnvironment configures process environment variables that are not
// provided by rip-environment (for RIP builds), system packages (for installed
// builds), or the linuxcnc wrapper script.
func (l *Launcher) setupEnvironment() {
	setIfEmpty := func(key, val string) {
		if os.Getenv(key) == "" {
			os.Setenv(key, val)
		}
	}

	// HALLIB_PATH – start with ".:HALLIB_DIR", then prepend -H dirs.
	halibPath := "."
	if config.HalibDir != "" {
		halibPath += ":" + config.HalibDir
	}
	for _, d := range l.opts.HalLibDirs {
		halibPath = d + ":" + halibPath
	}
	os.Setenv("HALLIB_PATH", halibPath)
	os.Setenv("HALLIB_DIR", config.HalibDir)

	// HAL_RTMOD_DIR.
	setIfEmpty("HAL_RTMOD_DIR", config.EMC2RtlibDir)

	// LINUXCNC_HOME.
	setIfEmpty("LINUXCNC_HOME", config.EMC2Home)
}

// logConfiguration logs key INI settings at debug level.
func (l *Launcher) logConfiguration() {
	if l.ini == nil {
		return
	}
	fields := []any{
		"machine", l.ini.Get("EMC", "MACHINE"),
		"display", l.ini.Get("DISPLAY", "DISPLAY"),
		"task", l.ini.Get("TASK", "TASK"),
	}
	l.logger.Debug("INI configuration loaded", fields...)
}

// loadThreads loads the threads HAL component to create RT threads.
//
// Thread creation has been decoupled from motmod — motmod now only exports
// functions, so the threads must exist before motmod or HAL files run.
//
// The threads component is loaded in-process via hal.LoadRT() which
// dlopen()s threads.so and calls hal_create_thread(), ensuring the RT
// pthreads get proper RT scheduling.
//
// Logic (reads [EMCMOT]SERVO_PERIOD and [EMCMOT]BASE_PERIOD from INI):
//   - If [EMCMOT]BASE_PERIOD is set and > 0:
//     loadrt threads name1=base-thread period1=<BASE_PERIOD> name2=servo-thread period2=<SERVO_PERIOD>
//   - Otherwise (no BASE_PERIOD or BASE_PERIOD=0):
//     loadrt threads name1=servo-thread period1=<SERVO_PERIOD>
//
// Threads are created fastest-first (base-thread before servo-thread) for
// proper rate monotonic priority scheduling.
func (l *Launcher) loadThreads() error {
	servoPeriodStr := l.ini.Get("EMCMOT", "SERVO_PERIOD")
	if servoPeriodStr == "" {
		return fmt.Errorf("[EMCMOT]SERVO_PERIOD is required but not set")
	}

	basePeriodStr := l.ini.Get("EMCMOT", "BASE_PERIOD")

	var args []string
	if basePeriodStr != "" && basePeriodStr != "0" {
		// Two threads: base-thread (fast, no FP) + servo-thread (slow, FP)
		l.logger.Info("loading threads component",
			"base_period", basePeriodStr, "servo_period", servoPeriodStr)
		args = []string{
			"name1=base-thread", "period1=" + basePeriodStr,
			"name2=servo-thread", "period2=" + servoPeriodStr,
		}
	} else {
		// One thread: servo-thread only
		l.logger.Info("loading threads component",
			"servo_period", servoPeriodStr)
		args = []string{
			"name1=servo-thread", "period1=" + servoPeriodStr,
		}
	}

	if err := halcmd.LoadRT("threads", args...); err != nil {
		return fmt.Errorf("loadrt threads: %w", err)
	}

	return nil
}

// preloadMotionModules loads the trajectory planner and homing modules via
// separate hal.LoadRT() calls before any HAL files execute.
//
// This mirrors scripts/linuxcnc.in lines 865-868:
//
//	eval $HALCMD loadrt "$TPMOD"
//	eval $HALCMD loadrt "$HOMEMOD"
//
// Priority for TPMOD: CLI flag (-t) > [TRAJ]TPMOD > "tpmod".
// Priority for HOMEMOD: CLI flag (-m) > [EMCMOT]HOMEMOD > "homemod".
func (l *Launcher) preloadMotionModules() error {
	tpMod := l.opts.TpMod
	if tpMod == "" {
		tpMod = l.ini.Get("TRAJ", "TPMOD")
	}
	if tpMod == "" {
		tpMod = "tpmod"
	}

	homeMod := l.opts.HomeMod
	if homeMod == "" {
		homeMod = l.ini.Get("EMCMOT", "HOMEMOD")
	}
	if homeMod == "" {
		homeMod = "homemod"
	}

	l.logger.Debug("preloading motion modules", "tpmod", tpMod, "homemod", homeMod)

	if err := halcmd.LoadRT(tpMod); err != nil {
		return fmt.Errorf("loadrt %s: %w", tpMod, err)
	}

	if err := halcmd.LoadRT(homeMod); err != nil {
		return fmt.Errorf("loadrt %s: %w", homeMod, err)
	}

	return nil
}

// startTask launches the task controller (milltask/linuxcnctask) as a
// fire-and-forget background process without waiting for the "inihal" HAL
// component to register as ready.
//
// This mirrors the bash launcher (scripts/linuxcnc.in line 957):
//
//	halcmd loadusr -Wn inihal $EMCTASK -ini "$INIFILE" &
//
// Note the trailing "&" — the bash launcher does NOT block on this call.
// milltask calls emcMotionInit() during startup, which requires the motion
// controller (motmod) to process commands via servo-thread. Those functions
// are not running until startHalThreads() (step 6d) completes. Waiting here
// would create a deadlock:
//  1. startTask() blocks waiting for inihal to be ready
//  2. milltask waits for motion controller to process commands
//  3. motion controller waits for servo-thread to run
//  4. servo-thread won't run until startHalThreads() is called
//  5. startHalThreads() can't run because startTask() hasn't returned
//
// INI resolution: [TASK]TASK is required; legacy rename "emctask" → "linuxcnctask".
// If [TASK]TASK is not set, startTask is a no-op (Run() skips calling it anyway).
func (l *Launcher) startTask() error {
	emctask := l.ini.Get("TASK", "TASK")
	if emctask == "" {
		l.logger.Debug("TASK not configured, skipping task controller")
		return nil
	}
	// Legacy rename: "emctask" was renamed to "linuxcnctask" in 2.9.
	if emctask == "emctask" {
		emctask = "linuxcnctask"
	}

	l.logger.Info("starting task controller", "program", emctask)

	// Fire-and-forget: do NOT wait for inihal to register as ready.
	// HAL threads (servo-thread) must be running before milltask can finish
	// motion initialization. Threads are started in step 6d, after this call.
	if err := halcmd.LoadUSR(&halcmd.LoadUSROptions{}, emctask, "-ini", l.opts.IniFile); err != nil {
		return fmt.Errorf("loadusr %s: %w", emctask, err)
	}

	return nil
}

// stopTask sends SIGTERM to the task controller (identified by the "inihal"
// HAL component) and waits for it to unregister from HAL.
func (l *Launcher) stopTask() {
	l.logger.Info("stopping task controller")
	if err := halcmd.UnloadUSR("inihal"); err != nil {
		l.logger.Debug("unload inihal returned error (may already be gone)", "error", err)
		return
	}
	// Wait up to 2 seconds for the component to unregister.
	if err := halcmd.WaitUSR("inihal"); err != nil {
		l.logger.Debug("wait for inihal to unregister returned error", "error", err)
	}
}

// startHalThreads starts all HAL realtime threads via the hal-go API.
//
// This mirrors scripts/linuxcnc.in line 999:
//
//	$HALCMD start
func (l *Launcher) startHalThreads() error {
	l.logger.Info("starting HAL threads")
	if err := halcmd.StartThreads(); err != nil {
		return fmt.Errorf("hal start threads: %w", err)
	}
	return nil
}

// isExecutable reports whether the given path is a regular, executable file.
func isExecutable(path string) bool {
	info, err := os.Stat(path)
	return err == nil && !info.IsDir() && info.Mode()&0o111 != 0
}

// loadRetain checks for retained HAL signals and, if any are found, loads the
// retain component and the retain_usr userspace process.
//
// This mirrors scripts/linuxcnc.in lines 975–996 (step 4.3.9):
//
//	if $HALCMD list retain | grep -q '.'; then
//	    $HALCMD loadrt retain
//	    $HALCMD addf retain.sync <SYNC_THREAD>
//	    $HALCMD loadusr -W retain_usr <VAR_FILE> <POLL_PERIOD>
//	fi
//
// Note: the bash script has a bug where it checks the wrong variable before
// setting RETAIN_SYNC_THREAD; the Go implementation checks the correct one.
func (l *Launcher) loadRetain() error {
	// Check whether any retained signals exist.
	signals, err := halcmd.List("retain")
	if err != nil {
		// halcmd.List errors when no retain component is loaded — treat as no signals.
		l.logger.Debug("hal list retain returned error (no retain signals)", "error", err)
		return nil
	}
	if len(signals) == 0 {
		l.logger.Debug("no retained signals found, skipping retain load")
		return nil
	}

	l.logger.Info("Loading retain")

	// Load the realtime retain component.
	if err := halcmd.LoadRT("retain"); err != nil {
		return fmt.Errorf("loadrt retain: %w", err)
	}

	// Determine the sync thread.
	syncThread := l.ini.Get("RETAIN", "SYNC_THREAD")
	if syncThread == "" {
		syncThread = "servo-thread"
	}
	if err := halcmd.AddF("retain.sync", syncThread, 0); err != nil {
		return fmt.Errorf("addf retain.sync %s: %w", syncThread, err)
	}

	// Determine the variable file path.
	varFile := l.ini.Get("RETAIN", "VAR_FILE")
	if varFile == "" {
		varFile = "retain.var"
	}
	// Resolve relative paths against the INI directory.
	if !filepath.IsAbs(varFile) {
		varFile = filepath.Join(filepath.Dir(l.opts.IniFile), varFile)
	}

	// Determine the poll period (may be empty — retain_usr handles a missing arg).
	pollPeriod := l.ini.Get("RETAIN", "POLL_PERIOD")

	// Start retain_usr, passing varFile and (optionally) pollPeriod as separate
	// arguments so that paths containing spaces are handled correctly.
	usrArgs := []string{varFile}
	if pollPeriod != "" {
		usrArgs = append(usrArgs, pollPeriod)
	}
	if err := halcmd.LoadUSR(&halcmd.LoadUSROptions{WaitReady: true}, "retain_usr", usrArgs...); err != nil {
		return fmt.Errorf("loadusr retain_usr: %w", err)
	}

	return nil
}

// runApplications launches [APPLICATIONS]APP entries from the INI file in the
// background.  This mirrors the run_applications() function in
// scripts/linuxcnc.in lines 394–428.
//
// If no APP entries are found, this is a no-op.
// [APPLICATIONS]DELAY specifies a sleep (in seconds) before each app launch.
func (l *Launcher) runApplications() error {
	apps := l.ini.GetAll("APPLICATIONS", "APP")
	if len(apps) == 0 {
		return nil
	}

	delayStr := l.ini.Get("APPLICATIONS", "DELAY")
	if delayStr == "" {
		delayStr = "0"
	}
	delaySecs, err := strconv.ParseFloat(delayStr, 64)
	if err != nil {
		l.logger.Warn("invalid [APPLICATIONS]DELAY, defaulting to 0", "value", delayStr)
		delaySecs = 0
	}
	delay := time.Duration(delaySecs * float64(time.Second))

	configDir := filepath.Dir(l.opts.IniFile)

	for _, app := range apps {
		fields := strings.Fields(app)
		if len(fields) == 0 {
			continue
		}
		name := fields[0]
		args := fields[1:]

		// Resolve the executable path.
		var execPath string
		switch {
		case filepath.IsAbs(name):
			// Absolute path — use as-is.
			execPath = name
		case strings.HasPrefix(name, "./"):
			// Relative path — resolve relative to CONFIG_DIR.
			execPath = filepath.Join(configDir, name)
		default:
			// Try CONFIG_DIR/name first; fall back to PATH lookup.
			candidate := filepath.Join(configDir, name)
			if isExecutable(candidate) {
				execPath = candidate
			} else {
				found, lookErr := exec.LookPath(name)
				if lookErr != nil {
					l.logger.Warn("application not found, skipping", "app", name)
					continue
				}
				execPath = found
			}
		}

		// Verify the resolved path is executable.
		if !isExecutable(execPath) {
			l.logger.Warn("application not executable, skipping", "path", execPath)
			continue
		}

		// Sleep before launching if a delay is configured.
		if delay > 0 {
			time.Sleep(delay)
		}

		cmd := exec.Command(execPath, args...)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		cmd.SysProcAttr = &syscall.SysProcAttr{
			Pdeathsig: syscall.SIGTERM,
		}

		if err := cmd.Start(); err != nil {
			l.logger.Warn("failed to start application", "app", execPath, "error", err)
			continue
		}

		// Reap the child process to prevent zombies if it exits before stopApplications().
		go func() { _ = cmd.Wait() }()

		l.logger.Info("started application", "app", execPath, "pid", cmd.Process.Pid)
		l.appProcesses = append(l.appProcesses, cmd)
	}

	return nil
}

// stopApplications kills all background application processes launched by
// runApplications().  Best-effort — errors are logged but not returned.
// Note: cmd.Wait() was already called by the reaper goroutine in runApplications(),
// so we only send signals here without waiting.
func (l *Launcher) stopApplications() {
	for _, cmd := range l.appProcesses {
		if cmd.Process == nil {
			continue
		}
		l.logger.Debug("stopping application", "pid", cmd.Process.Pid)
		if err := cmd.Process.Signal(syscall.SIGTERM); err != nil {
			l.logger.Debug("SIGTERM failed for application (may have already exited)", "pid", cmd.Process.Pid, "error", err)
			continue
		}
		// Give the process a moment to exit after SIGTERM.
		time.Sleep(500 * time.Millisecond)
		// Best-effort SIGKILL if still running.
		_ = cmd.Process.Kill()
	}
}

// startDisplay launches the configured display GUI in the foreground (blocking).
//
// This mirrors scripts/linuxcnc.in lines 1004–1036.  The display program is
// read from [DISPLAY]DISPLAY and dispatched based on the program name:
//
//   - dummy:         prints a prompt and waits for Enter
//   - linuxcncrsh:   <display> [args] -- -ini <ini>
//   - default:       <display> -ini <ini> [args]
//
// cmd.Run() blocks until the user closes the GUI.
// startDisplay is only called when [DISPLAY]DISPLAY is configured; Run()
// handles the no-display (HAL-only) case separately.
func (l *Launcher) startDisplay() error {
	displayVal := l.ini.Get("DISPLAY", "DISPLAY")
	if displayVal == "" {
		// This should not be reached — Run() only calls startDisplay when
		// hasDisplay is true.  Return nil for safety (HAL-only wait is in Run()).
		l.logger.Debug("no [DISPLAY]DISPLAY configured, startDisplay is a no-op")
		return nil
	}

	fields := strings.Fields(displayVal)
	emcDisplay := fields[0]
	displayArgs := fields[1:]

	// Legacy rename.
	if emcDisplay == "tkemc" {
		emcDisplay = "tklinuxcnc"
	}

	l.logger.Info("starting display", "display", emcDisplay)

	var cmd *exec.Cmd

	switch emcDisplay {
	case "dummy":
		fmt.Println("DUMMY DISPLAY MODULE, press <ENTER> to continue.")
		_, _ = bufio.NewReader(os.Stdin).ReadString('\n')
		return nil

	case "linuxcncrsh":
		// Note the -- separator before -ini.
		args := make([]string, 0, len(displayArgs)+3)
		args = append(args, displayArgs...)
		args = append(args, "--", "-ini", l.opts.IniFile)
		cmd = exec.Command(emcDisplay, args...)

	default:
		args := append([]string{"-ini", l.opts.IniFile}, displayArgs...)
		cmd = exec.Command(emcDisplay, args...)
	}

	cmd.Stdin = os.Stdin
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.SysProcAttr = &syscall.SysProcAttr{
		Pdeathsig: syscall.SIGTERM,
	}

	if err := cmd.Run(); err != nil {
		l.logger.Warn("display exited with error", "display", emcDisplay, "error", err)
	}
	return nil
}
