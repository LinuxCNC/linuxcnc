// Package launcher — cleanup.go implements the ordered shutdown sequence (M7).
package launcher

import (
	"os"
	"time"

	halcmd "github.com/sittner/linuxcnc/src/launcher/internal/halcmd"

	"github.com/sittner/linuxcnc/src/launcher/internal/halfile"
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

	// Step 2.3 — Stop Go plugin modules.
	l.logger.Debug("stopping Go plugin modules")
	l.stopGoModules()

	// Step 2.4 — Stop C plugin modules.
	l.logger.Debug("stopping C plugin modules")
	l.stopCModules()

	// Step 2.5 — Destroy all plugin modules (release resources after all are stopped).
	l.logger.Debug("destroying Go plugin modules")
	l.destroyGoModules()
	l.logger.Debug("destroying C plugin modules")
	l.destroyCModules()

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
	if err := halcmd.StopThreads(); err != nil {
		l.logger.Debug("hal stop threads returned error", "error", err)
	}

	// Step 5 — unload all HAL components.
	// mirrors scripts/linuxcnc.in line 713.
	// Pass 0 as exceptCompID — halcmd doesn't exclude itself either.
	l.logger.Debug("unloading HAL components")
	if err := halcmd.UnloadAll(0); err != nil {
		l.logger.Debug("hal unload all returned error", "error", err)
	}

	// Step 6 — wait for HAL components to unload.
	// Polls up to 10 times (200 ms apart) until only 1 component remains.
	// mirrors scripts/linuxcnc.in lines 715–719.
	l.logger.Debug("waiting for HAL components to unload")
	for i := 0; i < 10; i++ {
		comps, err := halcmd.ListComponents()
		if err != nil {
			break
		}
		if len(comps) <= 1 {
			break
		}
		time.Sleep(200 * time.Millisecond)
	}

	// Step 7 — Exit the launcher's own HAL component.
	// Must happen while HAL shared memory is still valid, i.e. before
	// RtapiAppCleanup() tears it down.
	if l.halComp != nil {
		if err := l.halComp.Exit(); err != nil {
			l.logger.Debug("hal exit returned error", "error", err)
		}
	}

	// Step 8 — Shut down the in-process RTAPI/HAL environment.
	// This tears down HAL threads, releases shared memory (via
	// rtapi_shmem_delete → shmdt/shmctl), and stops the message queue
	// thread.  Must happen after all HAL components (including the
	// launcher's own) have exited.  No external ipcrm is needed.
	l.logger.Debug("shutting down RTAPI app (in-process)")
	halcmd.RtapiAppCleanup()

	// Step 9 — NML server shutdown.
	// The NML server is now a cmod plugin — it was stopped in step 2.4
	// (stopCModules, reverse order) and destroyed in step 2.5
	// (destroyCModules).  The C++ NML destructors call shmdt() for each
	// attached segment and invoke shmctl(IPC_RMID) once nattch reaches
	// zero, so no manual ipcrm is needed.

	// Step 10 — Release lock file.
	// mirrors scripts/linuxcnc.in lines 733–735.
	if l.lock != nil {
		l.logger.Info("releasing lock file")
		if err := l.lock.Release(); err != nil {
			l.logger.Error("releasing lock file", "error", err)
		}
	}
}
