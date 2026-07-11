// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"time"
)

// monitorInterval is the polling rate for the monitoring goroutine.
// 10ms = 100Hz, matching the C milltask cycle time.
const monitorInterval = 10 * time.Millisecond

// IOFullStatus holds the fields needed by the monitor from the IO controller.
type IOFullStatus struct {
	Estop  bool
	Status int32 // IOStatusDone/Exec/Error
	Reason int32
}

// IOStatusReader can read the full IO status (estop + command status).
// Implemented by ioAdapter; optional interface tested at startup.
type IOStatusReader interface {
	GetIOFullStatus() (IOFullStatus, error)
}

// monitor runs the periodic health checks. It owns TWO goroutines:
//
//   - the safety loop (loop): external estop, motion-enabled, motion/IO
//     errors, soft limits, jog watchdog, inihal parameter changes. Nothing in
//     it dispatches commands, so its detection and signal phases can never
//     stall behind a command holding cmdMu (teardown *cleanup* serializes on
//     cmdMu, but only after the stop signals have fired).
//   - the halui loop (haluiLoop): scans halui pins and dispatches the
//     resulting commands. Dispatch is synchronous and may block on cmdMu for
//     the duration of a running command (exactly like any other UI client) —
//     which is why it must not share a goroutine with the safety checks.
//
// This is the Go equivalent of the C milltask main loop's subordinate health
// checks, with halui (a separate process there) given back its own thread.
type monitor struct {
	task   *Task
	mc     MotionConfig
	inihal *iniHal
	halui  *halUI         // nil if halui not configured
	ioStat IOStatusReader // nil if IO doesn't support status read
	stopCh chan struct{}
	doneCh chan struct{}
	// haluiDoneCh is closed when the halui loop exits (nil if halui is not
	// configured and the loop was never started).
	haluiDoneCh chan struct{}

	// Latch: suppress repeated error handling until error clears.
	errorLatched bool
}

func newMonitor(t *Task, mc MotionConfig, ih *iniHal, ioStat IOStatusReader) *monitor {
	return &monitor{
		task:   t,
		mc:     mc,
		inihal: ih,
		ioStat: ioStat,
	}
}

func (m *monitor) start() {
	m.stopCh = make(chan struct{})
	m.doneCh = make(chan struct{})
	go m.loop()
	if m.halui != nil {
		m.haluiDoneCh = make(chan struct{})
		go m.haluiLoop()
	}
}

func (m *monitor) stop() {
	if m.stopCh == nil {
		return
	}
	close(m.stopCh)
	<-m.doneCh
	if m.haluiDoneCh != nil {
		<-m.haluiDoneCh
	}
}

// loop is the safety loop: detection and stop signals only, no command
// dispatch, so it can never wedge behind a command holding cmdMu.
func (m *monitor) loop() {
	defer close(m.doneCh)

	ticker := time.NewTicker(monitorInterval)
	defer ticker.Stop()

	var softLimitReported bool

	for {
		select {
		case <-m.stopCh:
			return
		case <-ticker.C:
			m.checkEstop()
			m.checkMotionEnabled()
			m.checkMotionErrors(&softLimitReported)
			m.checkJogWatchdog()
			if m.inihal != nil {
				m.inihal.check(m.mc)
			}
		}
	}
}

// haluiLoop scans halui pins and dispatches the resulting commands, then
// mirrors task state back to the output pins. A dispatched command can block
// on cmdMu like any UI client's would — that only delays the next pin scan,
// never the safety loop. Note the consequence: halui's soft pins (including
// estop-activate) are serviced with UI-command latency; the hard estop path
// is emc-enable-in, which the safety loop's checkEstop watches independently.
func (m *monitor) haluiLoop() {
	defer close(m.haluiDoneCh)

	ticker := time.NewTicker(monitorInterval)
	defer ticker.Stop()

	for {
		select {
		case <-m.stopCh:
			return
		case <-ticker.C:
			m.halui.check(m.task)
			m.halui.updateOutputs(m.task)
		}
	}
}

// checkEstop polls the IO controller for external estop.
// This implements the equivalent of 2.9's determineState() for the estop
// axis: the internal state is always derived from emc-enable-in.
// If estop is asserted and machine is not already in estop state,
// force full shutdown (matching C milltask behavior).
// If estop is cleared and machine is in estop state, transition to
// estop-reset (matching determineState() returning ESTOP_RESET).
func (m *monitor) checkEstop() {
	if m.ioStat == nil {
		return
	}

	st, err := m.ioStat.GetIOFullStatus()
	if err != nil {
		return
	}

	if !st.Estop {
		// emc-enable-in is HIGH — estop cleared.
		// If we're in StateEstop, transition to EstopReset
		// (matching 2.9's determineState() which returns ESTOP_RESET
		// when io.aux.estop is false and motion is disabled).
		m.task.mu.Lock()
		if m.task.state == StateEstop {
			m.task.state = StateEstopReset
			m.task.mu.Unlock()
			m.task.logger.Info("estop cleared by emc-enable-in — state now estop-reset")
		} else {
			m.task.mu.Unlock()
		}
		return
	}

	// External estop asserted — check if we need to react.
	m.task.mu.Lock()
	if m.task.state == StateEstop {
		m.task.mu.Unlock()
		return // already in estop, nothing to do
	}
	wasEnabled := m.task.state == StateOn
	numSpindles := m.task.numSpindles
	// Commit the ENTIRE observable transition in this one hold, like the
	// sibling setState(ESTOP) path — a reader must never see state=Estop
	// with interpState still Reading and a populated MDI queue. (The values
	// are provisional against a still-draining sequencer; the authoritative
	// re-commit happens after the StartSequencer join below.)
	m.task.state = StateEstop
	m.task.interpState = InterpIdle
	m.task.execState = ExecDone
	m.task.mdiQueue = m.task.mdiQueue[:0]
	m.task.taskCommand = ""
	m.task.mdiGen++ // invalidate any pending finishMDI (R2)
	m.task.stepping = false
	m.task.floodOn = false
	m.task.mistOn = false
	m.task.mu.Unlock()

	// Set user-enable-out=0 to match the detected state.
	_ = m.task.io.EstopOn()

	if !wasEnabled {
		// Machine was already off (EstopReset/Off) — emc-enable-in going
		// low is expected (e.g. HW drops it when motion is disabled).
		// Just transition state silently, matching 2.9's determineState()
		// which returns ESTOP without any side effects.
		return
	}

	// Machine was ON — this is a real external estop event.
	m.task.logger.Warn("external estop detected — forcing shutdown")

	// Full estop-off teardown, shared with commanded estop/off (machineShutdown)
	// so this can no longer drift from it (it previously omitted the lube-off and
	// the endpoint re-synch). Signal phase runs WITHOUT cmdMu so the machine
	// stops even while a command holds the lock (the seqAbort close unblocks a
	// command stuck in EnqueueCmd backpressure); the interp-reset cleanup runs
	// WITH cmdMu so it cannot race a command that owns the interpreter.
	o := fullShutdownOpts(emcAbortAuxEstop, "external estop")
	m.task.stopSignals(numSpindles, o)

	m.task.cmdMu.Lock()
	m.task.finishShutdown(o)
	m.task.cmdMu.Unlock()

	m.task.operatorError("External E-Stop asserted")
}

// checkMotionEnabled detects when motion has disabled itself (e.g. due to
// following error, amp fault, limit switch). This mirrors the old milltask's
// determineState() which checked traj.enabled every cycle and transitioned
// the task from ON to ESTOP_RESET when motion was no longer enabled.
func (m *monitor) checkMotionEnabled() {
	if m.task.status == nil {
		return
	}

	ms, err := m.task.status.GetStatus()
	if err != nil {
		return
	}

	if ms.Enabled != 0 {
		return // motion still enabled, nothing to do
	}

	m.task.mu.Lock()
	if m.task.state != StateOn {
		m.task.mu.Unlock()
		return
	}
	// Motion disabled itself while the task believed the machine was on — an
	// unexpected disable (hard limit, following error, amp fault, watchdog, or
	// an external enable drop). Latch ExecError so the interruption is visible
	// rather than a plain ExecDone that reads like a clean stop. (A deliberate
	// off goes through SetState, which sets state != StateOn first, so this path
	// is never reached for a normal stop.) The specific cause is reported
	// separately via the motion module's operator-error message. ExecError is
	// cleared on the next estop-reset / off→on, so it does not wedge recovery.
	m.task.state = StateEstopReset
	m.task.interpState = InterpIdle
	m.task.mdiQueue = m.task.mdiQueue[:0]
	m.task.taskCommand = ""
	m.task.mdiGen++ // invalidate any pending finishMDI (R2)
	m.task.stepping = false
	numSpindles := m.task.numSpindles
	m.task.mu.Unlock()

	m.task.logger.Warn("motion disabled — switching machine off")

	// Error stop (motion disabled itself): abort motion+IO, stop spindles, and
	// latch ExecError — but leave coolant/lube/homing untouched (this is not a
	// full off; motion already disabled itself). Signal phase without cmdMu,
	// cleanup with it (see checkEstop / stopSignals). finishShutdown commits the
	// terminal ExecError after the join; it stays latched until the next
	// estop-reset / off→on recovery clears it.
	o := shutdownOpts{ioReason: emcAbortTaskStateNotOn, terminalExec: ExecError, reason: "motion disabled"}
	m.task.stopSignals(numSpindles, o)

	m.task.cmdMu.Lock()
	defer m.task.cmdMu.Unlock()
	m.task.finishShutdown(o)
}

// checkMotionErrors polls motion status for errors and soft limits.
func (m *monitor) checkMotionErrors(softLimitReported *bool) {
	if m.task.status == nil {
		return
	}

	ms, err := m.task.status.GetStatus()
	if err != nil {
		return // split-read, skip this cycle
	}

	// Check soft limit.
	if ms.OnSoftLimit != 0 {
		if !*softLimitReported {
			*softLimitReported = true
			m.task.operatorError("On Soft Limit")
			// Matches C milltask: warn if identity kinematics are misconfigured.
			if ms.KinType == 1 { // KINEMATICS_IDENTITY
				m.task.operatorError("Identity kinematics are MISCONFIGURED")
			}
		}
	} else {
		*softLimitReported = false
	}

	// Check motion error (any non-OK command status or Error bit set by control loop).
	motionError := ms.CommandStatus >= 2 || ms.Error != 0

	// Check IO error.
	var ioError bool
	if m.ioStat != nil {
		if ioSt, err := m.ioStat.GetIOFullStatus(); err == nil {
			ioError = ioSt.Status == IOStatusError && ioSt.Reason <= 0
		}
	}

	if !motionError && !ioError {
		// Error cleared — release latch so we can detect the next error.
		m.errorLatched = false
		return
	}

	// Don't re-trigger if we already handled this error.
	if m.errorLatched {
		return
	}

	// Only react if machine is on.
	m.task.mu.Lock()
	if m.task.state != StateOn {
		m.task.mu.Unlock()
		return
	}
	m.errorLatched = true
	numSpindles := m.task.numSpindles
	// Provisional commit (see checkEstop); the authoritative terminal state
	// is re-committed after the StartSequencer join below.
	m.task.interpState = InterpIdle
	m.task.mdiQueue = m.task.mdiQueue[:0]
	m.task.taskCommand = ""
	m.task.mdiGen++ // invalidate any pending finishMDI (R2)
	m.task.stepping = false
	m.task.mu.Unlock()

	if motionError && ms.OnSoftLimit == 0 {
		m.task.logger.Error("motion error detected — aborting")
		// The specific error message (e.g. "joint N following error") is
		// reported by the motion module via gomc_log_errorf and forwarded
		// to the operator message list through the log error hook.
	}
	if ioError {
		m.task.logger.Error("IO error detected — aborting")
	}

	// Error stop: abort motion+IO, stop spindles, and latch ExecError, leaving
	// coolant/lube/homing alone. Signal phase without cmdMu, cleanup with it
	// (see checkEstop / stopSignals). finishShutdown commits the terminal
	// ExecError after the join, so the stop is visible to the UI as an
	// error-stop. Matches C++ EMC_TASK_EXEC_ERROR.
	o := shutdownOpts{ioReason: emcAbortMotionOrIoRcsError, terminalExec: ExecError, reason: "motion/IO error"}
	m.task.stopSignals(numSpindles, o)

	m.task.cmdMu.Lock()
	defer m.task.cmdMu.Unlock()
	m.task.finishShutdown(o)
}

// checkJogWatchdog stops continuous jogs that haven't been refreshed
// within jogTimeout. This prevents runaway jogs if a client disconnects.
func (m *monitor) checkJogWatchdog() {
	now := time.Now()
	t := m.task

	t.mu.Lock()
	defer t.mu.Unlock()

	for i := range t.activeJogs {
		j := &t.activeJogs[i]
		if !j.active || j.fromHAL {
			continue
		}
		if now.Sub(j.lastSeen) > jogTimeout {
			j.active = false
			isTeleop := j.isTeleop // copy — j points into state mutated under mu
			t.mu.Unlock()
			_ = t.motion.JogAbort(int32(i), isTeleop)
			t.logger.Warn("jog watchdog: stopped expired jog", "axis_or_joint", i)
			t.mu.Lock()
		}
	}
}
