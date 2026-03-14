// Package launcher provides the main Launcher struct and orchestration logic
// for the LinuxCNC Go launcher.
//
// This package implements M1–M6 of the Go launcher (task + display launch).
package launcher

import (
	"bufio"
	"fmt"
	"log/slog"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
	"syscall"
	"time"

	"github.com/sittner/linuxcnc/src/launcher/config"
	"github.com/sittner/linuxcnc/src/launcher/halfile"
	"github.com/sittner/linuxcnc/src/launcher/inifile"
	"github.com/sittner/linuxcnc/src/launcher/lockfile"
	"github.com/sittner/linuxcnc/src/launcher/realtime"
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
	opts          Options
	ini           *inifile.IniFile
	logger        *slog.Logger
	lock          *lockfile.LockFile // flock-based instance lock
	serverProcess *exec.Cmd          // background linuxcncsvr process
	serverDone    chan error          // receives the result of cmd.Wait() for linuxcncsvr
	taskProcess   *exec.Cmd          // background milltask/linuxcnctask process
	taskDone      chan error          // receives the result of cmd.Wait() for task
	appProcesses  []*exec.Cmd        // [APPLICATIONS]APP background processes
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
// Implemented milestones M1–M6:
// Startup order matches scripts/linuxcnc.in:
//  1. Sets up environment variables (INI_FILE_NAME exported before any subprocess).
//  2. Acquires the lock file.
//  3. Parses the INI file.
//  4. Starts linuxcncsvr (NML server) — must precede realtime (M5).
//  5. Starts the realtime environment (M4).
//  6. Starts iocontrol via halcmd loadusr -Wn (M5).
//  7. Starts halui via halcmd loadusr -Wn if configured (M5).
//  8. Preloads tpmod/homemod (M4).
//  9. Executes [HAL]HALFILE entries (M3, step 4.3.6).
// 10. Starts the task controller in background (M6, step 4.3.7).
// 11. Executes [HAL]HALCMD entries (M6, step 4.3.8).
// 12. Loads retained signals if any are present (M6, step 4.3.9).
// 13. Starts HAL threads (M6, step 4.3.10).
// 14. Launches [APPLICATIONS]APP entries in background (M6, step 4.3.11).
// 15. Launches the display in the foreground — blocks until the user closes the GUI (M6, step 4.3.12).
//
// Note: POSTGUI_HALFILE loading is intentionally omitted here; it is the
// responsibility of the display GUI (AXIS, QtVCP, gmoccapy, etc.) to load
// its own post-GUI HAL files after creating its HAL pins.
func (l *Launcher) Run() error {
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
	defer func() {
		l.logger.Info("releasing lock file")
		if err := l.lock.Release(); err != nil {
			l.logger.Error("releasing lock file", "error", err)
		}
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

	// --- M5: Process Manager ---

	// Start NML server (linuxcncsvr) before realtime.  linuxcncsvr creates
	// the NML shared memory buffers that realtime components depend on.
	// This mirrors scripts/linuxcnc.in lines 817–825.
	if err := l.startServer(); err != nil {
		return fmt.Errorf("starting linuxcncsvr: %w", err)
	}
	defer l.stopServer()

	// --- M4: Realtime Manager ---
	rtMgr := realtime.New(l.logger)
	l.logger.Info("starting realtime environment")
	if err := rtMgr.Start(); err != nil {
		return fmt.Errorf("realtime start failed: %w", err)
	}
	defer func() {
		if err := rtMgr.Stop(); err != nil {
			l.logger.Error("realtime stop failed", "error", err)
		}
	}()

	// Start iocontrol via halcmd loadusr -Wn iocontrol.
	// iocontrol is a HAL userspace component; HAL manages its lifecycle and
	// will terminate it when halcmd exits or HAL is shut down.
	if err := l.startIOControl(); err != nil {
		return fmt.Errorf("starting iocontrol: %w", err)
	}

	// Start halui if configured.
	// halui is also a HAL userspace component managed by HAL's lifecycle.
	if err := l.startHalUI(); err != nil {
		return fmt.Errorf("starting halui: %w", err)
	}

	// Pre-load trajectory planner and homing modules before HAL file execution.
	// This mirrors scripts/linuxcnc.in lines 865-868.
	if err := l.preloadMotionModules(); err != nil {
		return fmt.Errorf("preloading motion modules: %w", err)
	}

	// M3: Load HAL files.
	halExec := halfile.New(l.ini, filepath.Join(config.EMC2BinDir, "halcmd"), os.Getenv("HALLIB_PATH"), l.logger, l.opts.IniFile)
	if err := halExec.ExecuteAll(); err != nil {
		if !l.opts.ContinueOnError {
			return fmt.Errorf("HAL file execution failed: %w", err)
		}
		l.logger.Warn("HAL file execution error (continuing)", "error", err)
	}

	// --- M6: Task + Display Launch ---

	// 6a. Start task controller in background (step 4.3.7).
	if err := l.startTask(); err != nil {
		return fmt.Errorf("starting task: %w", err)
	}
	defer l.stopTask()

	// 6b. Execute [HAL]HALCMD entries (step 4.3.8).
	// These run after the task controller has started, matching the bash launcher.
	if err := halExec.ExecuteHalCommands(); err != nil {
		if !l.opts.ContinueOnError {
			return fmt.Errorf("HAL command execution failed: %w", err)
		}
		l.logger.Warn("HAL command execution error (continuing)", "error", err)
	}

	// 6c. Load retained signals if any are present (step 4.3.9).
	if err := l.loadRetain(halExec); err != nil {
		if !l.opts.ContinueOnError {
			return fmt.Errorf("loading retain: %w", err)
		}
		l.logger.Warn("retain load error (continuing)", "error", err)
	}

	// 6d. Start HAL threads (step 4.3.10).
	if err := l.startHalThreads(); err != nil {
		return fmt.Errorf("halcmd start: %w", err)
	}

	// 6e. Launch application entries ([APPLICATIONS]APP) in background (step 4.3.11).
	if err := l.runApplications(); err != nil {
		l.logger.Warn("application launch error", "error", err)
	}
	defer l.stopApplications()

	// 6f. Launch display in foreground (blocks until user closes GUI, step 4.3.12).
	if err := l.startDisplay(); err != nil {
		return fmt.Errorf("display: %w", err)
	}

	// Display has exited — cleanup happens via defers:
	//   stopApplications → stopTask → realtime.Stop → stopServer → lock.Release
	return nil
}

// startServer starts linuxcncsvr as a background subprocess.
//
// This mirrors scripts/linuxcnc.in lines 817–825:
//
//	export INI_FILE_NAME="$INIFILE"
//	$EMCSERVER -ini "$INIFILE"
//
// After starting, a brief 100 ms window is checked for immediate failure
// (e.g., bad INI path, port conflict).  A background goroutine calls
// cmd.Wait() and signals serverDone so that both this check and
// stopServer() share the single Wait() call.
func (l *Launcher) startServer() error {
	serverBin := filepath.Join(config.EMC2BinDir, "linuxcncsvr")
	l.logger.Info("starting NML server", "binary", serverBin)

	cmd := exec.Command(serverBin, "-n", "-ini", l.opts.IniFile)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.SysProcAttr = &syscall.SysProcAttr{
		Pdeathsig: syscall.SIGTERM,
	}

	if err := cmd.Start(); err != nil {
		return fmt.Errorf("exec %s: %w", serverBin, err)
	}

	l.serverProcess = cmd
	l.serverDone = make(chan error, 1)
	go func() { l.serverDone <- cmd.Wait() }()

	// Give the server a brief window to fail fast (e.g., bad INI, port conflict).
	select {
	case err := <-l.serverDone:
		return fmt.Errorf("linuxcncsvr (pid %d) exited immediately: %w", cmd.Process.Pid, err)
	case <-time.After(100 * time.Millisecond):
		// Still running — good.
	}

	l.logger.Info("linuxcncsvr started", "pid", cmd.Process.Pid)
	return nil
}

// stopServer terminates the linuxcncsvr background process gracefully.
//
// It sends SIGTERM first and waits up to 2 seconds for a clean exit.
// If the process has not exited by then, SIGKILL is sent.
func (l *Launcher) stopServer() {
	if l.serverProcess == nil || l.serverProcess.Process == nil {
		return
	}
	l.logger.Info("stopping linuxcncsvr")
	if err := l.serverProcess.Process.Signal(syscall.SIGTERM); err != nil {
		l.logger.Debug("SIGTERM failed (process may have already exited)", "error", err)
	}
	select {
	case <-l.serverDone:
		l.logger.Debug("linuxcncsvr exited cleanly")
	case <-time.After(2 * time.Second):
		l.logger.Warn("linuxcncsvr did not exit in time, sending SIGKILL")
		_ = l.serverProcess.Process.Kill()
		<-l.serverDone
	}
}

// startIOControl starts the IO controller process via halcmd loadusr.
//
// This mirrors scripts/linuxcnc.in lines 839–850:
//
//	$HALCMD loadusr -Wn iocontrol $EMCIO -ini "$INIFILE"
//
// EMCIO resolution: [IO]IO → [EMCIO]EMCIO → default "io".
func (l *Launcher) startIOControl() error {
	emcio := l.ini.Get("IO", "IO")
	if emcio == "" {
		emcio = l.ini.Get("EMCIO", "EMCIO")
	}
	if emcio == "" {
		emcio = "io"
	}

	l.logger.Info("starting IO controller", "program", emcio)

	halcmdPath := filepath.Join(config.EMC2BinDir, "halcmd")
	cmd := exec.Command(halcmdPath, "loadusr", "-Wn", "iocontrol", emcio, "-ini", l.opts.IniFile)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.SysProcAttr = &syscall.SysProcAttr{
		Pdeathsig: syscall.SIGTERM,
	}

	if err := cmd.Run(); err != nil {
		return fmt.Errorf("halcmd loadusr -Wn iocontrol %s: %w", emcio, err)
	}

	return nil
}

// startHalUI starts the halui process via halcmd loadusr, if configured.
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

	halcmdPath := filepath.Join(config.EMC2BinDir, "halcmd")
	cmd := exec.Command(halcmdPath, "loadusr", "-Wn", "halui", halui, "-ini", l.opts.IniFile)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.SysProcAttr = &syscall.SysProcAttr{
		Pdeathsig: syscall.SIGTERM,
	}

	if err := cmd.Run(); err != nil {
		return fmt.Errorf("halcmd loadusr -Wn halui %s: %w", halui, err)
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

// setupEnvironment configures process environment variables to match what
// the bash linuxcnc.in script sets before launching components.
func (l *Launcher) setupEnvironment() {
	setIfEmpty := func(key, val string) {
		if os.Getenv(key) == "" {
			os.Setenv(key, val)
		}
	}
	prependPath := func(key, dir string) {
		cur := os.Getenv(key)
		if cur == "" {
			os.Setenv(key, dir)
		} else {
			os.Setenv(key, dir+":"+cur)
		}
	}

	// PATH – add bin dir and (for RIP) scripts dir.
	if config.EMC2BinDir != "" {
		prependPath("PATH", config.EMC2BinDir)
	}
	if config.RunInPlace == "yes" && config.EMC2Home != "" {
		scriptsDir := filepath.Join(config.EMC2Home, "scripts")
		if info, err := os.Stat(scriptsDir); err == nil && info.IsDir() {
			prependPath("PATH", scriptsDir)
		}
	}

	// HOME/.local/bin
	if home := os.Getenv("HOME"); home != "" {
		localBin := filepath.Join(home, ".local", "bin")
		if info, err := os.Stat(localBin); err == nil && info.IsDir() {
			if !strings.Contains(os.Getenv("PATH"), ".local/bin") {
				prependPath("PATH", localBin)
			}
		}
	}

	// LD_LIBRARY_PATH (RIP only).
	if config.RunInPlace == "yes" && config.EMC2Home != "" {
		prependPath("LD_LIBRARY_PATH", filepath.Join(config.EMC2Home, "lib"))
	}

	// PYTHONPATH.
	if config.EMC2Home != "" {
		prependPath("PYTHONPATH", filepath.Join(config.EMC2Home, "lib", "python"))
	}

	// TCLLIBPATH (space-separated).
	if config.EMC2Home != "" {
		tclDir := filepath.Join(config.EMC2Home, "tcl")
		cur := os.Getenv("TCLLIBPATH")
		if cur == "" {
			os.Setenv("TCLLIBPATH", tclDir)
		} else {
			os.Setenv("TCLLIBPATH", tclDir+" "+cur)
		}
	}

	// LINUXCNC_TCL_DIR — needed by twopass.tcl and other TCL scripts.
	setIfEmpty("LINUXCNC_TCL_DIR", config.EMC2TclDir)

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

	// Force GLX/X11 backends (matching linuxcnc.in behaviour).
	if plat := os.Getenv("LINUXCNC_OPENGL_PLATFORM"); plat == "" || plat == "glx" {
		setIfEmpty("PYOPENGL_PLATFORM", "x11")
		setIfEmpty("GDK_BACKEND", "x11")
		setIfEmpty("QT_QPA_PLATFORM", "xcb")
	}
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
		"twopass", l.ini.Get("HAL", "TWOPASS"),
	}
	l.logger.Debug("INI configuration loaded", fields...)
}

// preloadMotionModules loads the trajectory planner and homing modules via
// separate "halcmd loadrt" commands before any HAL files execute.
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

	halcmdPath := filepath.Join(config.EMC2BinDir, "halcmd")

	tpCmd := exec.Command(halcmdPath, "loadrt", tpMod)
	tpCmd.Stdout = os.Stdout
	tpCmd.Stderr = os.Stderr
	tpCmd.SysProcAttr = &syscall.SysProcAttr{
		Pdeathsig: syscall.SIGTERM,
	}
	if err := tpCmd.Run(); err != nil {
		return fmt.Errorf("halcmd loadrt %s: %w", tpMod, err)
	}

	homeCmd := exec.Command(halcmdPath, "loadrt", homeMod)
	homeCmd.Stdout = os.Stdout
	homeCmd.Stderr = os.Stderr
	homeCmd.SysProcAttr = &syscall.SysProcAttr{
		Pdeathsig: syscall.SIGTERM,
	}
	if err := homeCmd.Run(); err != nil {
		return fmt.Errorf("halcmd loadrt %s: %w", homeMod, err)
	}

	return nil
}

// startTask starts the task controller (milltask/linuxcnctask) as a background
// process via halcmd loadusr.
//
// This mirrors scripts/linuxcnc.in line 954:
//
//	halcmd loadusr -Wn inihal $EMCTASK -ini "$INIFILE" &
//
// The -Wn inihal flag makes halcmd wait for the inihal HAL component to register
// before returning; the & means the launcher doesn't block on this.
// INI resolution: [TASK]TASK → default "linuxcnctask"; legacy rename "emctask" → "linuxcnctask".
func (l *Launcher) startTask() error {
	emctask := l.ini.Get("TASK", "TASK")
	if emctask == "" || emctask == "emctask" {
		emctask = "linuxcnctask"
	}

	l.logger.Info("starting task controller", "program", emctask)

	halcmdPath := filepath.Join(config.EMC2BinDir, "halcmd")
	cmd := exec.Command(halcmdPath, "loadusr", "-Wn", "inihal", emctask, "-ini", l.opts.IniFile)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.SysProcAttr = &syscall.SysProcAttr{
		Pdeathsig: syscall.SIGTERM,
	}

	if err := cmd.Start(); err != nil {
		return fmt.Errorf("exec halcmd loadusr -Wn inihal %s: %w", emctask, err)
	}

	l.taskProcess = cmd
	l.taskDone = make(chan error, 1)
	go func() { l.taskDone <- cmd.Wait() }()

	// Give the task a brief window to fail fast.
	select {
	case err := <-l.taskDone:
		return fmt.Errorf("task process (pid %d) exited immediately: %w", cmd.Process.Pid, err)
	case <-time.After(100 * time.Millisecond):
		// Still running — good.
	}

	l.logger.Info("task controller started", "pid", cmd.Process.Pid)
	return nil
}

// stopTask terminates the task controller background process gracefully.
//
// It sends SIGTERM first and waits up to 2 seconds for a clean exit.
// If the process has not exited by then, SIGKILL is sent.
func (l *Launcher) stopTask() {
	if l.taskProcess == nil || l.taskProcess.Process == nil {
		return
	}
	l.logger.Info("stopping task controller")
	if err := l.taskProcess.Process.Signal(syscall.SIGTERM); err != nil {
		l.logger.Debug("SIGTERM failed (process may have already exited)", "error", err)
	}
	select {
	case <-l.taskDone:
		l.logger.Debug("task controller exited cleanly")
	case <-time.After(2 * time.Second):
		l.logger.Warn("task controller did not exit in time, sending SIGKILL")
		_ = l.taskProcess.Process.Kill()
		<-l.taskDone
	}
}

// startHalThreads executes "halcmd start" to start all HAL threads.
//
// This mirrors scripts/linuxcnc.in line 999:
//
//	$HALCMD start
func (l *Launcher) startHalThreads() error {
	l.logger.Info("starting HAL threads")

	halcmdPath := filepath.Join(config.EMC2BinDir, "halcmd")
	cmd := exec.Command(halcmdPath, "start")
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	cmd.SysProcAttr = &syscall.SysProcAttr{
		Pdeathsig: syscall.SIGTERM,
	}

	if err := cmd.Run(); err != nil {
		return fmt.Errorf("halcmd start: %w", err)
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
func (l *Launcher) loadRetain(halExec *halfile.Executor) error {
	halcmdPath := filepath.Join(config.EMC2BinDir, "halcmd")

	// Check whether any retained signals exist.
	out, err := exec.Command(halcmdPath, "list", "retain").Output()
	if err != nil {
		// "halcmd list retain" errors when no retain component is loaded — treat
		// as no retained signals.
		l.logger.Debug("halcmd list retain returned error (no retain signals)", "error", err)
		return nil
	}
	if len(strings.TrimSpace(string(out))) == 0 {
		l.logger.Debug("no retained signals found, skipping retain load")
		return nil
	}

	l.logger.Info("Loading retain")

	// Load the realtime retain component.
	if err := halExec.RunHalcmdArgs([]string{"loadrt", "retain"}); err != nil {
		return fmt.Errorf("halcmd loadrt retain: %w", err)
	}

	// Determine the sync thread.
	syncThread := l.ini.Get("RETAIN", "SYNC_THREAD")
	if syncThread == "" {
		syncThread = "servo-thread"
	}
	if err := halExec.RunHalcmdArgs([]string{"addf", "retain.sync", syncThread}); err != nil {
		return fmt.Errorf("halcmd addf retain.sync %s: %w", syncThread, err)
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

	// Build loadusr args: pass varFile and (optionally) pollPeriod as separate
	// arguments so that paths containing spaces are handled correctly.
	loadusrArgs := []string{"loadusr", "-W", "retain_usr", varFile}
	if pollPeriod != "" {
		loadusrArgs = append(loadusrArgs, pollPeriod)
	}
	if err := halExec.RunHalcmdArgs(loadusrArgs); err != nil {
		return fmt.Errorf("halcmd loadusr retain_usr: %w", err)
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
//   - tklinuxcnc / mini: $LINUXCNC_TCL_DIR/<display>.tcl -ini <ini> [args]
//   - dummy:             prints a prompt and waits for Enter
//   - linuxcncrsh:       <display> [args] -- -ini <ini>
//   - default:           <display> -ini <ini> [args]
//
// cmd.Run() blocks until the user closes the GUI.
func (l *Launcher) startDisplay() error {
	displayVal := l.ini.Get("DISPLAY", "DISPLAY")
	if displayVal == "" {
		return fmt.Errorf("no [DISPLAY]DISPLAY configured")
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
	case "tklinuxcnc", "mini":
		tclScript := filepath.Join(config.EMC2TclDir, emcDisplay+".tcl")
		args := append([]string{"-ini", l.opts.IniFile}, displayArgs...)
		cmd = exec.Command(tclScript, args...)

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
