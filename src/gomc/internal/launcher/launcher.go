// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package launcher provides the main Launcher struct and orchestration logic
// for the LinuxCNC Go launcher.
//
// This package implements M1–M7 of the Go launcher (task + display launch +
// orderly shutdown).
package launcher

import (
	"errors"
	"fmt"
	"log/slog"
	"os"
	"os/signal"
	"path/filepath"
	"sync"
	"syscall"
	"unsafe"

	hal "github.com/sittner/linuxcnc/src/gomc/pkg/hal"

	halcmd "github.com/sittner/linuxcnc/src/gomc/internal/halcmd"

	"github.com/sittner/linuxcnc/src/gomc/internal/apiserver"
	"github.com/sittner/linuxcnc/src/gomc/internal/config"
	"github.com/sittner/linuxcnc/src/gomc/internal/halfile"
	"github.com/sittner/linuxcnc/src/gomc/internal/halrest"
	"github.com/sittner/linuxcnc/src/gomc/internal/inirest"
	"github.com/sittner/linuxcnc/src/gomc/internal/lockfile"
	"github.com/sittner/linuxcnc/src/gomc/internal/realtime"
	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

// Options holds the parsed command-line options.
type Options struct {
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
	opts        Options
	ini         *inifile.IniFile
	logger      *slog.Logger
	lock        *lockfile.LockFile // flock-based instance lock
	rtMgr       *realtime.Manager  // realtime environment manager
	cleanupOnce sync.Once          // ensures cleanup runs exactly once
	halComp     *hal.Component     // launcher's HAL component (like halcmd's hal_init)
	goModules   []*goModule        // Go modules loaded via "load" command (compiled-in)
	cModules    []*cModule         // C plugin modules loaded via "load" command
	cModArena   []unsafe.Pointer   // arena-tracked C strings freed in destroyCModules
	logRing     *gomcLogRing       // shared log ring buffer for C module FIFO logging
	retain      *retainInstance    // integrated retain subsystem (nil if unused)
	apiServer   *apiserver.Server  // REST API server for halcmd and external tools
	shutdownCh  chan struct{}      // closed by signal handler to unblock wait
}

// New creates a new Launcher with the given options and logger.
// If logger is nil, a default structured logger writing to stderr is used.
func New(opts Options, logger *slog.Logger) *Launcher {
	if logger == nil {
		logger = slog.New(slog.NewTextHandler(os.Stderr, nil))
	}
	return &Launcher{opts: opts, logger: logger, shutdownCh: make(chan struct{})}
}

// ensureLogRing creates the shared log ring buffer and starts the drain
// goroutine if not already running.  Called lazily on the first loadCPlugin.
func (l *Launcher) ensureLogRing() {
	if l.logRing != nil {
		return
	}
	l.logRing = newGomcLogRing()
	l.logRing.startDrain(l.logger)
}

// Run executes the full LinuxCNC startup sequence.
//
// Implemented milestones M1–M7:
// Startup order:
//  1. Acquires the lock file.
//  2. Parses the INI file.
//  3. Validates cross-section INI dependencies (validateDependencies).
//  4. Starts the realtime environment.
//  5. Initializes HAL (hal_init).
//  6. Executes [HAL]HALFILE entries (load → initCModules → net/addf/setp).
//  7. Executes [HAL]HALCMD entries.
//  8. Loads retained signals if any are present.
//  9. Locks HAL memory (if configured).
//  10. Starts HAL threads.
//  11. Starts Go modules and C modules.
//  12. Starts the REST API server.
//  13. Blocks until SIGINT/SIGTERM is received.
//  14. Shuts down in ordered sequence (reverse of above).
//
// Note: POSTGUI_HALFILE loading is intentionally omitted here; it is the
// responsibility of the display GUI (AXIS, QtVCP, gmoccapy, etc.) to load
// its own post-GUI HAL files after creating its HAL pins.
func (l *Launcher) Run() (runErr error) {
	// Initialize the API registry so cmod plugins can register/lookup APIs.
	apiserver.SetDefaultRegistry(apiserver.NewRegistry())

	// Create the API server early so that stream_server registrations
	// from cmod plugins (which happen during HAL file loading) can find it.
	// The server won't start listening until startAPIServer() is called later.
	l.createAPIServer()

	// Register the halcmd REST API handler (uses internal HAL access, not liblinuxcnchal.so).
	if err := halrest.Register(apiserver.DefaultRegistry()); err != nil {
		l.logger.Warn("failed to register halcmd REST API", "error", err)
	}

	// Set the load-module hook so halcmd's \"load\" command can dynamically
	// load cmod plugins at runtime via the REST API.
	halrest.SetLoadModuleFunc(l.runtimeLoadModule)
	halrest.SetUnloadModuleFunc(l.UnloadModule)

	l.setupEnvironment()

	// Export INI file path and config directory so that child processes
	// (linuxcncsvr, iocontrol, task, etc.) can find the configuration.
	// These must be set before realtime is started.
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
		// Signal the main wait loop to unblock so that the deferred
		// cleanup runs through the normal exit path.  Calling os.Exit()
		// here would race with C plugin destructors causing segfaults.
		l.shutdown()
	}()

	l.logger.Info("parsing INI file", "path", l.opts.IniFile)
	ini, err := inifile.Parse(l.opts.IniFile)
	if err != nil {
		return err
	}
	l.ini = ini

	// Register the INI REST API handler (exposes parsed INI via /api/v1/ini/query).
	if err := inirest.Register(apiserver.DefaultRegistry(), l.ini); err != nil {
		l.logger.Warn("failed to register INI REST API", "error", err)
	}

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

	// Validate cross-section INI dependencies before starting any processes.
	if err := l.validateDependencies(); err != nil {
		return fmt.Errorf("configuration error: %w", err)
	}

	// Pre-launch validation checks: run after INI parsing + include expansion
	// + chdir, but before realtime init.

	// 1. [EMC]VERSION check + update_ini.
	if err := l.checkVersion(); err != nil {
		if errors.Is(err, ErrUpdateCancelled) {
			return nil // user cancelled — clean exit
		}
		return err
	}

	// 2. PlasmaC migration check.
	if err := l.checkPlasmaC(); err != nil {
		if errors.Is(err, ErrPlasmaC) {
			return nil // PlasmaC handled — clean exit
		}
		return err
	}

	// --- M5: Process Manager ---

	// --- M4: Realtime Manager ---
	l.rtMgr = realtime.New(l.logger)
	l.logger.Info("starting realtime environment")
	if err := l.rtMgr.Start(); err != nil {
		return fmt.Errorf("realtime start failed: %w", err)
	}

	// Initialize the in-process RTAPI/HAL environment.  Sets up HAL shared
	// memory and routes RTAPI messages through the gomc_log ring.
	// Must be called before hal.NewComponent() / hal_init().
	l.ensureLogRing()
	halcmd.SetLogRing(unsafe.Pointer(l.logRing.ring))
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

	// Initialize the CPU affinity pool for HAL thread creation.
	// Detects isolated physical cores for automatic assignment by the
	// newthread HAL command (executed from HAL files below).
	if err := halcmd.InitCPUPool(l.logger); err != nil {
		return fmt.Errorf("initializing CPU pool: %w", err)
	}

	// Load and execute HAL files.
	halExec := halfile.New(l.ini, os.Getenv("HALLIB_PATH"), l.logger, l.opts.IniFile)
	halResult, err := halExec.ParseAll()
	if err != nil {
		return fmt.Errorf("HAL file parsing failed: %w", err)
	}

	// Load plugin modules (cmod or Go).
	if err := halResult.IterLoads(func(path string, name string, args []string) error {
		cmodPath := resolveCModulePath(path)
		if cModuleExists(cmodPath) {
			return l.loadCPlugin(cmodPath, name, args)
		}
		return l.loadGoModule(path, name, args)
	}); err != nil {
		if !l.opts.ContinueOnError {
			return fmt.Errorf("plugin module loading failed: %w", err)
		}
		l.logger.Warn("plugin module loading error (continuing)", "error", err)
	}

	// Phase 1b: Initialize all plugin modules (Init phase).
	// All modules' New() have completed and APIs are registered.  Init()
	// looks up other modules' APIs and performs cross-module initialization
	// (e.g. wiring function pointers, initializing subsystems).
	if err := l.initCModules(); err != nil {
		return fmt.Errorf("C module init failed: %w", err)
	}

	// Phase 2: Execute HAL wiring commands (net, addf, setp, etc.).
	if err := halResult.Execute(); err != nil {
		if !l.opts.ContinueOnError {
			return fmt.Errorf("HAL file execution failed: %w", err)
		}
		l.logger.Warn("HAL file execution error (continuing)", "error", err)
	}

	// Execute [HAL]HALCMD entries.
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
	l.lockRTModules()
	if err := l.startHalThreads(); err != nil {
		return fmt.Errorf("hal start threads: %w", err)
	}

	// 6d.3. Start Go plugin modules.
	if err := l.startGoModules(); err != nil {
		return fmt.Errorf("Go module start failed: %w", err)
	}

	// 6d.4. Start C plugin modules.
	if err := l.startCModules(); err != nil {
		return fmt.Errorf("C module start failed: %w", err)
	}

	// 6d.5. Start the REST API server for halcmd and external tools.
	l.startAPIServer()

	// 6e. Wait for shutdown signal (Ctrl+C / SIGTERM).
	l.logger.Info("ready, waiting for shutdown signal (Ctrl+C to stop)")
	<-l.shutdownCh

	// Shutdown signal received — cleanup runs via deferred l.cleanup():
	//   kill apps → stop modules → halcmd stop → halcmd unload all →
	//   wait → realtime stop → lock
	return nil
}

// resolveRelativePath resolves path against the INI file's directory when it
// is relative.  Absolute paths are returned unchanged.
func (l *Launcher) resolveRelativePath(path string) string {
	if filepath.IsAbs(path) || l.opts.IniFile == "" {
		return path
	}
	return filepath.Join(filepath.Dir(l.opts.IniFile), path)
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

// shutdown signals the main wait loop to unblock.  Called from the signal
// handler goroutine to trigger an ordered shutdown through the normal defer path.
func (l *Launcher) shutdown() {
	select {
	case <-l.shutdownCh:
		// already closed
	default:
		close(l.shutdownCh)
	}
}
