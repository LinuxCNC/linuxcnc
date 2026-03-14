// Package launcher — prelaunch.go implements the pre-launch validation checks
// that run after INI parsing but before startServer().
//
// These correspond to scripts/linuxcnc.in lines 495–530 and 791–812.
package launcher

import (
	"errors"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"

	"github.com/sittner/linuxcnc/src/launcher/config"
	"github.com/sittner/linuxcnc/src/launcher/inifile"
)

// ErrUpdateCancelled is returned by checkVersion when the user cancels the
// update_ini GUI (exit code 42).  The caller should treat this as a clean exit.
var ErrUpdateCancelled = errors.New("update_ini cancelled by user")

// ErrPlasmaC signals that the INI file is a PlasmaC configuration that has
// been handled by the migration tool.  The caller should treat this as a clean
// exit and never continue to start LinuxCNC.
var ErrPlasmaC = errors.New("PlasmaC configuration detected")

// checkVersion validates [EMC]VERSION and runs update_ini if needed.
//
// This mirrors scripts/linuxcnc.in lines 495–508:
//
//	GetFromIni VERSION EMC
//	if [ "$retval" != "1.1" ]; then
//	    update_ini -d "$INIFILE"
//	    ...
//	fi
//
// Returns ErrUpdateCancelled (exit 42) for a user-cancelled update, which the
// caller should convert to a nil (clean exit).  After a successful update the
// INI file is re-parsed since update_ini modifies it in place.
func (l *Launcher) checkVersion() error {
	version := l.ini.Get("EMC", "VERSION")
	if version == "1.1" {
		return nil
	}

	l.logger.Info("INI file [EMC]VERSION indicates update is needed", "version", version)

	if os.Getenv("DISPLAY") == "" {
		return fmt.Errorf("INI file [EMC]VERSION indicates update is needed, but the update GUI can't run without an X display")
	}

	cmd := exec.Command("update_ini", "-d", l.opts.IniFile)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		var exitErr *exec.ExitError
		if errors.As(err, &exitErr) {
			if exitErr.ExitCode() == 42 {
				return ErrUpdateCancelled
			}
		}
		return fmt.Errorf("update_ini failed: %w", err)
	}

	// update_ini modifies the INI file in place; re-parse it.
	l.logger.Info("re-parsing INI file after update_ini", "path", l.opts.IniFile)
	updated, err := inifile.Parse(l.opts.IniFile)
	if err != nil {
		return fmt.Errorf("re-parsing INI after update_ini: %w", err)
	}
	l.ini = updated

	return nil
}

// checkPlasmaC detects PlasmaC configurations and delegates to the migration
// tool qtplasmac-plasmac2qt.
//
// This mirrors scripts/linuxcnc.in lines 511–522.
//
// Returns ErrPlasmaC to signal "stop but don't treat as error" when a PlasmaC
// config is detected (regardless of the migration tool's exit code).
func (l *Launcher) checkPlasmaC() error {
	if l.ini.Get("PLASMAC", "MODE") == "" {
		return nil
	}

	l.logger.Info("This is a PlasmaC configuration, it requires migrating to QtPlasmac.")

	cmd := exec.Command("qtplasmac-plasmac2qt", l.opts.IniFile)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		var exitErr *exec.ExitError
		if errors.As(err, &exitErr) {
			switch exitErr.ExitCode() {
			case 2:
				l.logger.Info("Migration cancelled by user. PlasmaC is not available in LinuxCNC 2.9 and later.")
			default:
				l.logger.Error("QtPlasmaC migration failed with an unknown error.")
			}
		} else {
			l.logger.Error("QtPlasmaC migration failed", "error", err)
		}
	} else {
		// Exit code 0 — migration tool ran successfully; its output is already
		// written to stdout.
	}

	// Always exit after detecting PlasmaC — never continue to start LinuxCNC.
	return ErrPlasmaC
}

// resolveTclsh returns the tclsh binary to use.
// Priority: config.Tclsh (set at build time via -ldflags) → "tclsh" on PATH.
func resolveTclsh() (string, error) {
	if config.Tclsh != "" {
		return config.Tclsh, nil
	}
	p, err := exec.LookPath("tclsh")
	if err != nil {
		return "", fmt.Errorf("tclsh not found on PATH: %w", err)
	}
	return p, nil
}

// checkConfig runs check_config.tcl to validate the INI file before launching
// any LinuxCNC processes.
//
// This mirrors scripts/linuxcnc.in lines 524–529:
//
//	@TCLSH@ $HALLIB_DIR/check_config.tcl "$INIFILE"
//
// A non-zero exit code is returned as an error.
func (l *Launcher) checkConfig() error {
	tclsh, err := resolveTclsh()
	if err != nil {
		return fmt.Errorf("checkConfig: %w", err)
	}

	script := filepath.Join(config.HalibDir, "check_config.tcl")
	l.logger.Debug("running check_config.tcl", "script", script)

	cmd := exec.Command(tclsh, script, l.opts.IniFile)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		return fmt.Errorf("check_config validation failed: %w", err)
	}

	return nil
}

// showIntroGraphic displays the intro graphic popup if one is configured.
//
// This mirrors scripts/linuxcnc.in lines 791–812.
//
// The popup is launched in the background (fire-and-forget); errors are logged
// but never propagate to the caller.
func (l *Launcher) showIntroGraphic() {
	img := l.ini.Get("DISPLAY", "INTRO_GRAPHIC")
	if img == "" {
		return
	}

	imgTimeStr := l.ini.Get("DISPLAY", "INTRO_TIME")
	imgTime := 5
	if imgTimeStr != "" {
		if t, err := strconv.Atoi(strings.TrimSpace(imgTimeStr)); err == nil {
			imgTime = t
		}
	}

	// Resolve the image path: try as-is, then INI_DIR/img, then LINUXCNC_IMAGEDIR/img.
	iniDirPath := filepath.Join(filepath.Dir(l.opts.IniFile), img)
	imgDirPath := filepath.Join(config.EMC2ImageDir, img)
	resolvedImg := ""
	switch {
	case fileExists(img):
		resolvedImg = img
	case fileExists(iniDirPath):
		resolvedImg = iniDirPath
	case fileExists(imgDirPath):
		resolvedImg = imgDirPath
	}

	if resolvedImg == "" {
		l.logger.Debug("intro graphic not found, skipping", "image", img)
		return
	}

	popimage := filepath.Join(config.EMC2TclDir, "bin", "popimage")
	if !isExecutable(popimage) {
		l.logger.Debug("popimage not found or not executable, skipping intro graphic", "path", popimage)
		return
	}

	l.logger.Debug("showing intro graphic", "image", resolvedImg, "time", imgTime)
	cmd := exec.Command(popimage, resolvedImg, strconv.Itoa(imgTime))
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Start(); err != nil {
		l.logger.Debug("failed to start popimage", "error", err)
		return
	}
	// Fire-and-forget: reap child to avoid zombie.
	go func() { _ = cmd.Wait() }()
}

// fileExists reports whether the given path is a regular file (not a directory).
func fileExists(path string) bool {
	info, err := os.Stat(path)
	return err == nil && !info.IsDir()
}
