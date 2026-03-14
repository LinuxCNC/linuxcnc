// Package launcher — cleanup.go implements the ordered shutdown sequence (M7).
package launcher

import (
	"bufio"
	"os"
	"os/exec"
	"path/filepath"
	"strings"
	"time"

	"github.com/sittner/linuxcnc/src/launcher/config"
)

// cleanup is the single entry point for ordered shutdown.  It is idempotent —
// the first call runs the full sequence and any subsequent calls return
// immediately.  sync.Once ensures exactly one execution even when called
// concurrently from the signal handler goroutine and from a deferred call in
// Run().
func (l *Launcher) cleanup() {
	l.cleanupOnce.Do(l.doCleanup)
}

// doCleanup executes the full shutdown sequence, matching the bash Cleanup()
// function in scripts/linuxcnc.in lines 666–736.
//
// All errors are logged but not returned so that every step runs even if a
// prior step fails.
func (l *Launcher) doCleanup() {
	l.logger.Info("shutting down and cleaning up LinuxCNC...")

	// Step 1 — Stop tracked application processes.
	l.logger.Debug("stopping application processes")
	l.stopApplications()

	// Step 2 — Stop tracked task process.
	l.logger.Debug("stopping task process")
	l.stopTask()

	// Step 3 — Run [HAL]SHUTDOWN script if configured.
	// mirrors scripts/linuxcnc.in lines 697–701.
	if l.ini != nil {
		if shutdown := l.ini.Get("HAL", "SHUTDOWN"); shutdown != "" {
			l.logger.Info("running HAL shutdown script", "script", shutdown)
			halcmdPath := filepath.Join(config.EMC2BinDir, "halcmd")
			cmd := exec.Command(halcmdPath, "-f", shutdown)
			cmd.Stdout = os.Stdout
			cmd.Stderr = os.Stderr
			if err := cmd.Run(); err != nil {
				l.logger.Debug("HAL shutdown script returned error", "error", err)
			}
		}
	}

	halcmdPath := filepath.Join(config.EMC2BinDir, "halcmd")

	// Step 4 — halcmd stop (stop all realtime threads).
	// mirrors scripts/linuxcnc.in line 711.
	l.logger.Debug("stopping realtime threads")
	stopCmd := exec.Command(halcmdPath, "stop")
	stopCmd.Stdout = os.Stdout
	stopCmd.Stderr = os.Stderr
	if err := stopCmd.Run(); err != nil {
		l.logger.Debug("halcmd stop returned error", "error", err)
	}

	// Step 5 — halcmd unload all (unload all HAL components).
	// mirrors scripts/linuxcnc.in line 713.
	l.logger.Debug("unloading HAL components")
	unloadCmd := exec.Command(halcmdPath, "unload", "all")
	unloadCmd.Stdout = os.Stdout
	unloadCmd.Stderr = os.Stderr
	if err := unloadCmd.Run(); err != nil {
		l.logger.Debug("halcmd unload all returned error", "error", err)
	}

	// Step 6 — Wait for HAL component unload.
	// Polls up to 10 times (200 ms apart) until only 1 component remains
	// (halcmd itself).  mirrors scripts/linuxcnc.in lines 715–719.
	l.logger.Debug("waiting for HAL components to unload")
	for i := 0; i < 10; i++ {
		out, err := exec.Command(halcmdPath, "list", "comp").Output()
		if err != nil {
			break
		}
		if len(strings.Fields(string(out))) <= 1 {
			break
		}
		time.Sleep(200 * time.Millisecond)
	}

	// Step 7 — Stop realtime environment.
	// mirrors scripts/linuxcnc.in line 722.
	if l.rtMgr != nil {
		if err := l.rtMgr.Stop(); err != nil {
			l.logger.Error("realtime stop failed", "error", err)
		}
	}

	// Step 8 — Stop tracked server process.
	l.logger.Debug("stopping server process")
	l.stopServer()

	// Step 9 — Remove NML shared memory segments.
	// mirrors scripts/linuxcnc.in lines 724–729.
	if nmlFile := l.resolveNmlFile(); nmlFile != "" {
		l.cleanNmlSharedMemory(nmlFile)
	}

	// Step 10 — Release lock file.
	// mirrors scripts/linuxcnc.in lines 733–735.
	if l.lock != nil {
		l.logger.Info("releasing lock file")
		if err := l.lock.Release(); err != nil {
			l.logger.Error("releasing lock file", "error", err)
		}
	}
}

// resolveNmlFile returns the path to the NML configuration file.
//
// Priority: [LINUXCNC]NML_FILE → [EMC]NML_FILE → build-time DefaultNmlFile.
// This mirrors scripts/linuxcnc.in line 580:
//
//	GetFromIniEx NML_FILE LINUXCNC NML_FILE EMC @DEFAULT_NMLFILE@
func (l *Launcher) resolveNmlFile() string {
	if l.ini != nil {
		if nml := l.ini.Get("LINUXCNC", "NML_FILE"); nml != "" {
			return l.resolveRelativePath(nml)
		}
		if nml := l.ini.Get("EMC", "NML_FILE"); nml != "" {
			return l.resolveRelativePath(nml)
		}
	}
	return config.DefaultNmlFile
}

// resolveRelativePath resolves a path that may be relative against the INI
// file's directory.  Absolute paths are returned unchanged.
func (l *Launcher) resolveRelativePath(p string) string {
	if filepath.IsAbs(p) {
		return p
	}
	if l.opts.IniFile != "" {
		return filepath.Join(filepath.Dir(l.opts.IniFile), p)
	}
	return p
}

// cleanNmlSharedMemory reads the NML file and removes each BSHMEM shared
// memory segment via "ipcrm -M <key>".
//
// This mirrors scripts/linuxcnc.in lines 724–729:
//
//	while read b x t x x x x x x m x; do
//	    case $b$t in BSHMEM) ipcrm -M $m 2>/dev/null;; esac
//	done < $NMLFILE
//
// Field indices (0-based): b=0, t=2, m=9.
func (l *Launcher) cleanNmlSharedMemory(nmlFile string) {
	l.logger.Debug("removing NML shared memory segments", "nml", nmlFile)

	f, err := os.Open(nmlFile)
	if err != nil {
		l.logger.Debug("cannot open NML file for shm cleanup", "nml", nmlFile, "error", err)
		return
	}
	defer f.Close()

	scanner := bufio.NewScanner(f)
	for scanner.Scan() {
		fields := strings.Fields(scanner.Text())
		// Need at least 10 fields: b(0) x(1) t(2) x(3) x(4) x(5) x(6) x(7) x(8) m(9)
		if len(fields) < 10 {
			continue
		}
		b := fields[0]
		t := fields[2]
		m := fields[9]
		if b+t == "BSHMEM" {
			l.logger.Debug("removing NML shared memory segment", "key", m)
			if err := exec.Command("ipcrm", "-M", m).Run(); err != nil {
				l.logger.Debug("ipcrm failed", "key", m, "error", err)
			}
		}
	}
}
