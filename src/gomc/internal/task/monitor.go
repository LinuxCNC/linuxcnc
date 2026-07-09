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

// monitor runs a periodic loop checking for external estop, motion errors,
// soft limits, and inihal parameter changes. This is the Go equivalent of
// the C milltask main loop's subordinate health checks.
type monitor struct {
	task   *Task
	mc     MotionConfig
	inihal *iniHal
	halui  *halUI         // nil if halui not configured
	ioStat IOStatusReader // nil if IO doesn't support status read
	stopCh chan struct{}
	doneCh chan struct{}

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
}

func (m *monitor) stop() {
	if m.stopCh == nil {
		return
	}
	close(m.stopCh)
	<-m.doneCh
}

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
			if m.halui != nil {
				m.halui.check(m.task)
				m.halui.updateOutputs(m.task)
			}
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
	m.task.state = StateEstop
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

	// Signal phase — WITHOUT cmdMu, so the machine stops even while a command
	// holds the lock (the seqAbort close below is also what unblocks a command
	// stuck in EnqueueCmd backpressure so it can release cmdMu).

	// Abort sequencer and motion
	m.task.AbortSequencer()
	m.task.mcodeAbort()
	_ = m.task.motion.Abort()
	_ = m.task.motion.Disable()

	// Abort IO
	_ = m.task.io.IoAbort(emcAbortAuxEstop)

	// Stop all spindles
	for i := 0; i < numSpindles; i++ {
		_ = m.task.motion.SpindleOff(int32(i))
	}

	// Turn off coolant
	_ = m.task.io.CoolantFloodOff()
	_ = m.task.io.CoolantMistOff()

	// Unhome all joints (joint -2 = all)
	_ = m.task.motion.JointUnhome(-2)

	// Cleanup phase — serialized with commands: the interpreter reset must not
	// race a command that owns the interpreter (e.g. an MDI in ExecuteString).
	m.task.cmdMu.Lock()

	m.task.mu.Lock()
	m.task.floodOn = false
	m.task.mistOn = false
	m.task.interpState = InterpIdle
	m.task.execState = ExecDone
	m.task.mdiQueue = m.task.mdiQueue[:0]
	m.task.taskCommand = ""
	m.task.stepping = false
	m.task.mu.Unlock()

	// Wait for the producer to stop touching the interpreter before reset.
	m.task.waitRunProgramDone()
	// Notify interpreter
	if m.task.interp != nil {
		_ = m.task.interp.Abort(0, "external estop")
		_ = m.task.interp.Close()
		_ = m.task.interp.Reset()
		_ = m.task.interp.Synch()
		m.task.updateActiveCodes(m.task.interp)
	}

	// Restart sequencer for clean state
	m.task.StartSequencer()

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
	m.task.stepping = false
	numSpindles := m.task.numSpindles
	m.task.mu.Unlock()

	m.task.logger.Warn("motion disabled — switching machine off")

	// Signal phase — without cmdMu (see checkEstop).
	// Abort everything (matches old emcTaskUpdate when state left ON).
	m.task.AbortSequencer()
	m.task.mcodeAbort()
	_ = m.task.motion.Abort()
	_ = m.task.io.IoAbort(emcAbortTaskStateNotOn)

	for i := 0; i < numSpindles; i++ {
		_ = m.task.motion.SpindleOff(int32(i))
	}

	// Cleanup phase — serialized with commands (interpreter ownership).
	m.task.cmdMu.Lock()
	defer m.task.cmdMu.Unlock()

	m.task.waitRunProgramDone()
	if m.task.interp != nil {
		_ = m.task.interp.Abort(0, "motion disabled")
		_ = m.task.interp.Close()
		_ = m.task.interp.Reset()
		m.task.updateActiveCodes(m.task.interp)
	}

	m.task.StartSequencer()

	// Latch ExecError AFTER the teardown: AbortSequencer makes the old
	// sequencerLoop set ExecDone as it exits, which would clobber an error set
	// earlier. The fresh sequencer from StartSequencer is idle, so this sticks
	// (until cleared by the next estop-reset / off→on recovery).
	m.task.setExecState(ExecError)
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
	m.task.interpState = InterpIdle
	// ExecError is latched AFTER the teardown below (see StartSequencer): setting
	// it here would be clobbered by AbortSequencer, whose exiting sequencerLoop
	// sets ExecDone — same hazard handled in checkMotionEnabled.
	m.task.mdiQueue = m.task.mdiQueue[:0]
	m.task.taskCommand = ""
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

	// Signal phase — without cmdMu (see checkEstop).
	// Abort everything.
	m.task.AbortSequencer()
	m.task.mcodeAbort()
	_ = m.task.motion.Abort()
	_ = m.task.io.IoAbort(emcAbortMotionOrIoRcsError)

	// Stop spindles.
	for i := 0; i < numSpindles; i++ {
		_ = m.task.motion.SpindleOff(int32(i))
	}

	// Cleanup phase — serialized with commands (interpreter ownership).
	m.task.cmdMu.Lock()
	defer m.task.cmdMu.Unlock()

	// Wait for the producer to stop touching the interpreter before reset.
	m.task.waitRunProgramDone()
	// Notify interpreter.
	if m.task.interp != nil {
		_ = m.task.interp.Abort(0, "motion/IO error")
		_ = m.task.interp.Close()
		_ = m.task.interp.Reset()
		m.task.updateActiveCodes(m.task.interp)
	}

	// Restart sequencer.
	m.task.StartSequencer()

	// Latch ExecError now that the old sequencer (which sets ExecDone on exit)
	// is gone and the fresh one is idle — so the error is visible to the UI as
	// an error-stop, not a clean ExecDone. Matches C++ EMC_TASK_EXEC_ERROR.
	m.task.setExecState(ExecError)
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
