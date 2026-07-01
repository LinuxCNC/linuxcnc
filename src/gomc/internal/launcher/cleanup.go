// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package launcher — cleanup.go implements the ordered shutdown sequence (M7).
package launcher

import (
	"strings"
	"time"

	halcmd "github.com/sittner/linuxcnc/src/gomc/internal/halcmd"

	"github.com/sittner/linuxcnc/src/gomc/internal/halfile"
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
//  3. createThreads (hal_create_thread_cpu)
//  4. load modules (cmod New)
//  5. wire HAL (addf, net, setp)
//  6. startThreads               <- RT functions start executing
//  7. startGoModules, startCModules
//
// Shutdown order (strict reverse):
//
//  1. stopCModules, stopGoModules (reverse of 7)
//
//  2. StopThreads — SYNCHRONOUS   (reverse of 6, waits for all RT idle)
//     ── barrier: no RT function executes past this point ──
//
//  3. SHUTDOWN halfile
//
//  4. destroyCModules             (reverse of 4, frees EC masters etc.)
//
//  5. destroyGoModules            (reverse of 4)
//
//  6. UnloadAll                   (reverse of 4, unloads components)
//
//  7. wait for unload             (userspace processes may still be exiting)
//
//  8. halComp.Exit                (reverse of 2)
//
//  9. RtapiAppCleanup             (reverse of 1)
//
// 10. Release lock file
//
// All errors are logged but not returned so that every step runs even if a
// prior step fails.
func (l *Launcher) doCleanup() {
	l.logger.Info("shutting down and cleaning up LinuxCNC...")

	// Step 0 — Stop REST API server (reverse of startAPIServer).
	l.logger.Debug("stopping REST API server")
	l.stopAPIServer()

	// Step 2 — Stop C plugin modules (reverse of startCModules).
	// Runs BEFORE StopThreads so that modules can perform graceful
	// shutdown while RT threads are still processing (e.g. milltask
	// communicates with motmod via servo-thread during emcMotionHalt).
	// Resource release (Destroy) happens later, after the RT barrier.
	l.logger.Debug("stopping C plugin modules")
	l.stopCModules()

	// Step 2b — Stop Go plugin modules (reverse of startGoModules).
	l.logger.Debug("stopping Go plugin modules")
	l.stopGoModules()

	// Step 2d — Stop retain goroutine (final sync runs while RT is still active).
	l.logger.Debug("stopping retain")
	l.stopRetain()

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

		// Step 3b — Destroy retain component (safe — threads are idle).
		l.logger.Debug("destroying retain component")
		l.destroyRetain()

		// Step 6 — Run [HAL]SHUTDOWN script if configured.
		if l.ini != nil {
			if shutdown := l.ini.Get("HAL", "SHUTDOWN"); shutdown != "" {
				halExec := halfile.New(l.ini, l.halibPath, l.logger, l.opts.IniFile)
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

		// Step 9 — Unload all HAL components.
		l.logger.Debug("unloading HAL components")
		if err := halcmd.UnloadAll(0); err != nil {
			l.logger.Debug("hal unload all returned error", "error", err)
		}

		// Step 9b — Delete HAL threads.
		// Threads created by newthread in HAL files have __<name>
		// pseudo-components that UnloadAll does not remove.  Delete them
		// now so the wait loop below does not stall on them.
		if comps, err := halcmd.ListComponents(); err == nil {
			for _, name := range comps {
				if strings.HasPrefix(name, "__") {
					threadName := name[2:]
					l.logger.Debug("deleting HAL thread", "name", threadName)
					if err := halcmd.ThreadDelete(threadName); err != nil {
						l.logger.Debug("hal thread delete returned error", "name", threadName, "error", err)
					}
				}
			}
		}

		// Step 10 — Wait for remaining HAL components to unload.
		// After UnloadAll, only the launcher component should remain.
		// Userspace processes (e.g. hal_manualtoolchange) may still be
		// exiting after SIGTERM.
		l.logger.Debug("waiting for HAL components to unload")
		for i := 0; i < 10; i++ {
			comps, err := halcmd.ListComponents()
			if err != nil {
				break
			}
			if len(comps) <= 1 {
				break
			}
			l.logger.Debug("still waiting for HAL components", "remaining", comps)
			time.Sleep(200 * time.Millisecond)
		}

		// Step 12 — Exit the launcher's own HAL component.
		// Must happen while HAL shared memory is still valid, i.e. before
		// RtapiAppCleanup() tears it down.
		if err := l.halComp.Exit(); err != nil {
			l.logger.Debug("hal exit returned error", "error", err)
		}

		// Step 12b — Tear down the log ring.
		// Disconnect the RTAPI message handler first so that any stray
		// rtapi_print_msg calls during RtapiAppCleanup are silently
		// discarded rather than writing to freed memory.
		halcmd.ClearMsgHandler()
		if l.logRing != nil {
			l.logRing.stopDrain(l.logger)
			l.logRing.destroy()
			l.logRing = nil
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
				halExec := halfile.New(l.ini, l.halibPath, l.logger, l.opts.IniFile)
				if err := halExec.ExecuteShutdown(); err != nil {
					l.logger.Debug("HAL shutdown script returned error", "error", err)
				}
			}
		}
	}

}
