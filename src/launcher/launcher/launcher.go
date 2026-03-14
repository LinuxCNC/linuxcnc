// Package launcher provides the main Launcher struct and orchestration logic
// for the LinuxCNC Go launcher.
//
// This package implements M1–M5 of the Go launcher and provides stub
// methods for the remaining milestones (M6–M7).
package launcher

import (
	"fmt"
	"log/slog"
	"os"
	"os/exec"
	"path/filepath"
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
// Implemented milestones M1–M5; stubs remain for M6–M7.
// Startup order matches scripts/linuxcnc.in:
//  1. Sets up environment variables (INI_FILE_NAME exported before any subprocess).
//  2. Acquires the lock file.
//  3. Parses the INI file.
//  4. Starts linuxcncsvr (NML server) — must precede realtime (M5).
//  5. Starts the realtime environment (M4).
//  6. Starts iocontrol via halcmd loadusr -Wn (M5).
//  7. Starts halui via halcmd loadusr -Wn if configured (M5).
//  8. Preloads tpmod/homemod (M4).
//  9. Executes HAL files (M3).
// 10. Stubs for task and display (M6–M7).
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
	halExec := halfile.New(l.ini, filepath.Join(config.EMC2BinDir, "halcmd"), os.Getenv("HALLIB_PATH"), l.logger)
	if err := halExec.ExecuteAll(); err != nil {
		if !l.opts.ContinueOnError {
			return fmt.Errorf("HAL file execution failed: %w", err)
		}
		l.logger.Warn("HAL file execution error (continuing)", "error", err)
	}

	l.logger.Info("would start task / display (M6)")
	l.logger.Info("would wait for display to exit (M7)")

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
