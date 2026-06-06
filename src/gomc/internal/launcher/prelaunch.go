// Package launcher — prelaunch.go implements the pre-launch validation checks
// that run after INI parsing but before startServer().
//
// These correspond to scripts/linuxcnc.in lines 495–530.
package launcher

import (
	"errors"
	"fmt"
	"os"
	"os/exec"

	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
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

