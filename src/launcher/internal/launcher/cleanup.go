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

// doCleanup executes the full shutdown sequence in strict reverse of startup
// order.  The critical invariant is that hal_stop_threads() is called (and
// blocks until all RT threads are idle) BEFORE any module Stop/Destroy calls
// that free resources accessed by RT functions.
//
// Startup order (from Run()):
//  1. RtapiAppInit
//  2. hal.NewComponent (launcher)
//  3. loadThreads (hal_create_thread)
//  4. load modules (cmod New, loadrt, loadusr)
//  5. wire HAL (addf, net, setp)
//  6. startTask
//  7. startThreads               ← RT functions start executing
//  8. startGoModules, startCModules
//  9. startApplications, startDisplay
//
// Shutdown order (strict reverse):
//  1. stopApplications            (reverse of 9)
//  2. stopTask                    (reverse of 6)
//  3. StopThreads — SYNCHRONOUS   (reverse of 7, waits for all RT idle)
//     ── barrier: no RT function executes past this point ──
//  4. stopCModules                (reverse of 8, safe — threads are idle)
//  5. stopGoModules               (reverse of 8)
//  6. SHUTDOWN halfile
//  7. destroyCModules             (reverse of 4, frees EC masters etc.)
//  8. destroyGoModules            (reverse of 4)
//  9. UnloadAll                   (reverse of 4, unloads loadrt components)
//  10. wait for unload
//  11. halComp.Exit                (reverse of 2)
//  12. RtapiAppCleanup             (reverse of 1)
//  13. Release lock file
//
// All errors are logged but not returned so that every step runs even if a
// prior step fails.
func (l *Launcher) doCleanup() {
	l.logger.Info("shutting down and cleaning up LinuxCNC...")

	// Step 1 — Stop tracked application processes (reverse of startApplications).
	l.logger.Debug("stopping application processes")
	l.stopApplications()

	// Step 2 — Stop tracked task process (reverse of startTask).
	l.logger.Debug("stopping task process")
	l.stopTask()

	// Steps 3–12 require the RTAPI/HAL environment to have been initialized
	// (RtapiAppInit + hal.NewComponent succeeded).  When startup fails before
	// that point (e.g. INI file not found), these would crash by accessing
	// uninitialized HAL shared memory.
	if l.halComp != nil {
		// Step 3 — Stop all realtime threads SYNCHRONOUSLY.
		// hal_stop_threads() now clears threads_running AND waits for every
		// RT thread to finish its current cycle (idle flag).  After this
		// returns, no HAL RT function is executing and none will execute
		// again.  This makes it safe to free module resources below.
		l.logger.Debug("stopping realtime threads (synchronous)")
		if err := halcmd.StopThreads(); err != nil {
			l.logger.Debug("hal stop threads returned error", "error", err)
		}

		// ── RT barrier: all threads are idle past this point ──

		// Step 4 — Stop C plugin modules (reverse of startCModules).
		// Safe because RT threads are idle — EtherCAT domain/datagram
		// structures will not be accessed during ecrt_master_deactivate().
		l.logger.Debug("stopping C plugin modules")
		l.stopCModules()

		// Step 5 — Stop Go plugin modules (reverse of startGoModules).
		l.logger.Debug("stopping Go plugin modules")
		l.stopGoModules()

		// Step 6 — Run [HAL]SHUTDOWN script if configured.
		if l.ini != nil {
			if shutdown := l.ini.Get("HAL", "SHUTDOWN"); shutdown != "" {
				halExec := halfile.New(l.ini, os.Getenv("HALLIB_PATH"), l.logger, l.opts.IniFile)
				if err := halExec.ExecuteShutdown(); err != nil {
					l.logger.Debug("HAL shutdown script returned error", "error", err)
				}
			}
		}

		// Step 7 — Destroy C plugin modules (reverse of load, frees resources).
		l.logger.Debug("destroying C plugin modules")
		l.destroyCModules()

		// Step 8 — Destroy Go plugin modules.
		l.logger.Debug("destroying Go plugin modules")
		l.destroyGoModules()

		// Step 9 — Unload all HAL components (loadrt, loadusr).
		l.logger.Debug("unloading HAL components")
		if err := halcmd.UnloadAll(0); err != nil {
			l.logger.Debug("hal unload all returned error", "error", err)
		}

		// Step 10 — Wait for HAL components to unload.
		// Polls up to 10 times (200 ms apart) until only 1 component remains.
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

		// Step 11 — Exit the launcher's own HAL component.
		// Must happen while HAL shared memory is still valid, i.e. before
		// RtapiAppCleanup() tears it down.
		if err := l.halComp.Exit(); err != nil {
			l.logger.Debug("hal exit returned error", "error", err)
		}

		// Step 12 — Shut down the in-process RTAPI/HAL environment.
		// Releases shared memory (rtapi_shmem_delete → shmdt/shmctl) and
		// stops the message queue thread.
		l.logger.Debug("shutting down RTAPI app (in-process)")
		halcmd.RtapiAppCleanup()
	} else {
		// HAL was never initialized — still run module cleanup if any
		// modules were loaded before the failure.
		l.stopCModules()
		l.stopGoModules()
		l.destroyCModules()
		l.destroyGoModules()

		if l.ini != nil {
			if shutdown := l.ini.Get("HAL", "SHUTDOWN"); shutdown != "" {
				halExec := halfile.New(l.ini, os.Getenv("HALLIB_PATH"), l.logger, l.opts.IniFile)
				if err := halExec.ExecuteShutdown(); err != nil {
					l.logger.Debug("HAL shutdown script returned error", "error", err)
				}
			}
		}
	}

	// Step 13 — Release lock file.
	if l.lock != nil {
		l.logger.Info("releasing lock file")
		if err := l.lock.Release(); err != nil {
			l.logger.Error("releasing lock file", "error", err)
		}
	}
}
