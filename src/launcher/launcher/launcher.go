// Package launcher provides the main Launcher struct and orchestration logic
// for the LinuxCNC Go launcher.
//
// This package is a skeleton for M3–M7 implementation.  Currently it wires
// together the parsed CLI options, INI file, and compile-time configuration
// and provides stub methods for the full startup/shutdown sequence.
package launcher

import (
	"fmt"
	"log/slog"
	"os"
	"path/filepath"
	"strings"

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
	opts   Options
	ini    *inifile.IniFile
	logger *slog.Logger
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
// Current implementation (M1/M2/M4):
//  1. Sets up environment variables.
//  2. Acquires the lock file.
//  3. Parses the INI file.
//  4. Starts the realtime environment (M4).
//  5–7. Stubs for later milestones (M3/M5–M7).
func (l *Launcher) Run() error {
	l.setupEnvironment()

	// Export INI file path and config directory so that child processes
	// (linuxcncsvr, iocontrol, task, etc.) can find the configuration.
	if l.opts.IniFile != "" {
		os.Setenv("INI_FILE_NAME", l.opts.IniFile)
		os.Setenv("CONFIG_DIR", filepath.Dir(l.opts.IniFile))
	}

	l.logger.Info("acquiring lock file")
	if err := lockfile.Acquire(); err != nil {
		return err
	}
	defer func() {
		l.logger.Info("releasing lock file")
		if err := lockfile.Release(); err != nil {
			l.logger.Error("releasing lock file", "error", err)
		}
	}()

	l.logger.Info("parsing INI file", "path", l.opts.IniFile)
	ini, err := inifile.Parse(l.opts.IniFile)
	if err != nil {
		return err
	}
	l.ini = ini
	l.logConfiguration()

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

	// --- Stubs for M3/M5–M7 ---
	l.logger.Info("would start linuxcncsvr (M5)")

	// Inject tp= and hp= arguments into [EMCMOT]EMCMOT so that halcmd
	// receives e.g. "loadrt motmod tp=tpmod hp=homemod ..." instead of
	// just "loadrt motmod ...".  This mirrors the logic in scripts/linuxcnc.in.
	l.injectEmcmotModules()

	// M3: Load HAL files.
	halExec := halfile.New(l.ini, filepath.Join(config.EMC2BinDir, "halcmd"), os.Getenv("HALLIB_PATH"), l.logger)
	if err := halExec.ExecuteAll(); err != nil {
		if !l.opts.ContinueOnError {
			return fmt.Errorf("HAL file execution failed: %w", err)
		}
		l.logger.Warn("HAL file execution error (continuing)", "error", err)
	}

	l.logger.Info("would start task / display (M5)")
	l.logger.Info("would wait for display to exit (M7)")

	return nil
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

// injectEmcmotModules modifies the in-memory [EMCMOT]EMCMOT INI value to
// append "tp=<TPMOD> hp=<HOMEMOD>".  This mirrors the logic in
// scripts/linuxcnc.in so that halcmd receives the full module load string,
// e.g. "loadrt motmod tp=tpmod hp=homemod servo_period_nsec=...".
//
// Priority for TPMOD: CLI flag (-t) > [TRAJ]TPMOD > "tpmod".
// Priority for HOMEMOD: CLI flag (-m) > [EMCMOT]HOMEMOD > "homemod".
func (l *Launcher) injectEmcmotModules() {
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

	emcmot := l.ini.Get("EMCMOT", "EMCMOT")
	if emcmot == "" {
		l.logger.Warn("EMCMOT not set in [EMCMOT] section; tp/hp injection skipped")
		return
	}
	newEmcmot := emcmot + " tp=" + tpMod + " hp=" + homeMod
	l.ini.Set("EMCMOT", "EMCMOT", newEmcmot)
	l.logger.Debug("injected TPMOD/HOMEMOD into EMCMOT", "tpmod", tpMod, "homemod", homeMod, "emcmot", newEmcmot)
}
