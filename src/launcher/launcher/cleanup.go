// Package launcher — cleanup.go implements the ordered shutdown sequence (M7).
package launcher

import (
	"os"
	"time"

	hal "linuxcnc.org/hal"

	"github.com/sittner/linuxcnc/src/launcher/halfile"
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
	// Uses the native Go HAL file parser instead of shelling out to halcmd.
	if l.ini != nil {
		if shutdown := l.ini.Get("HAL", "SHUTDOWN"); shutdown != "" {
			halExec := halfile.New(l.ini, os.Getenv("HALLIB_PATH"), l.logger, l.opts.IniFile)
			if err := halExec.ExecuteShutdown(); err != nil {
				l.logger.Debug("HAL shutdown script returned error", "error", err)
			}
		}
	}

	// Step 4 — stop all realtime threads.
	// mirrors scripts/linuxcnc.in line 711.
	l.logger.Debug("stopping realtime threads")
	if err := hal.StopThreads(); err != nil {
		l.logger.Debug("hal stop threads returned error", "error", err)
	}

	// Step 5 — unload all HAL components.
	// mirrors scripts/linuxcnc.in line 713.
	// Pass 0 as exceptCompID — halcmd doesn't exclude itself either.
	l.logger.Debug("unloading HAL components")
	if err := hal.UnloadAll(0); err != nil {
		l.logger.Debug("hal unload all returned error", "error", err)
	}

	// Step 6 — wait for HAL components to unload.
	// Polls up to 10 times (200 ms apart) until only 1 component remains.
	// mirrors scripts/linuxcnc.in lines 715–719.
	l.logger.Debug("waiting for HAL components to unload")
	for i := 0; i < 10; i++ {
		comps, err := hal.ListComponents()
		if err != nil {
			break
		}
		if len(comps) <= 1 {
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

	// Step 8 — Stop in-process NML server.
	// stopServer() signals the server to stop, waits for the goroutine to
	// exit, then calls emcsvr.Cleanup() which runs the C++ NML destructors.
	// Those destructors call shmdt() for each attached segment and invoke
	// shmctl(IPC_RMID) once nattch reaches zero, so no manual ipcrm is
	// needed here.  Attempting ipcrm while other processes (e.g. linuxcnctask)
	// may still be attached causes consistent failures; the OS reclaims the
	// segments automatically once every process has detached.
	l.logger.Debug("stopping NML server")
	l.stopServer()

	// Step 9 — Release lock file.
	// mirrors scripts/linuxcnc.in lines 733–735.
	if l.lock != nil {
		l.logger.Info("releasing lock file")
		if err := l.lock.Release(); err != nil {
			l.logger.Error("releasing lock file", "error", err)
		}
	}

	// Final step — exit the launcher's HAL connection.
	// hal_exit() must be the last HAL operation, after everything else is done.
	if l.halComp != nil {
		if err := l.halComp.Exit(); err != nil {
			l.logger.Debug("hal exit returned error", "error", err)
		}
	}
}
