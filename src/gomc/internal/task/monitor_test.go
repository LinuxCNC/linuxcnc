// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"log/slog"
	"os"
	"sync"
	"sync/atomic"
	"testing"
	"time"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/motstat"
)

// mockIOWithStatus extends mockIO with IOStatusReader support.
type mockIOWithStatus struct {
	mockIO
	mu     sync.Mutex
	estop  bool
	status int32
	reason int32
}

func (m *mockIOWithStatus) GetIOFullStatus() (IOFullStatus, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	return IOFullStatus{
		Estop:  m.estop,
		Status: m.status,
		Reason: m.reason,
	}, nil
}

func (m *mockIOWithStatus) setEstop(v bool) {
	m.mu.Lock()
	m.estop = v
	m.mu.Unlock()
}

func (m *mockIOWithStatus) setError(reason int32) {
	m.mu.Lock()
	m.status = IOStatusError
	m.reason = reason
	m.mu.Unlock()
}

func (m *mockIOWithStatus) clearError() {
	m.mu.Lock()
	m.status = IOStatusDone
	m.reason = 0
	m.mu.Unlock()
}

// mockStatusWithError extends mockStatus with configurable error state.
type mockStatusWithError struct {
	mu            sync.Mutex
	enabled       int32 // motion self-enabled flag; 0 => checkMotionEnabled fires
	commandStatus int32 // cmd_status_t (motion.h): >=2 = rejected motion command
	onSoftLimit   int32
	joints        [16]motstat.JointStatus
}

func (m *mockStatusWithError) GetStatus() (motstat.MotionStatus, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	ms := motstat.MotionStatus{
		Enabled:       m.enabled,
		CommandStatus: m.commandStatus,
		OnSoftLimit:   m.onSoftLimit,
	}
	ms.Joints = m.joints
	return ms, nil
}

// setEnabled sets the motion self-enabled flag. Error-detection tests set it to
// 1 so checkMotionEnabled stays quiet and the abort is attributable to the
// injected error rather than the Enabled==0 self-disable path.
func (m *mockStatusWithError) setEnabled(v int32) {
	m.mu.Lock()
	m.enabled = v
	m.mu.Unlock()
}
func (m *mockStatusWithError) GetPosCmd() (motstat.Pose, error)      { return motstat.Pose{}, nil }
func (m *mockStatusWithError) GetPosFb() (motstat.Pose, error)       { return motstat.Pose{}, nil }
func (m *mockStatusWithError) GetInpos() (int32, error)              { return 1, nil }
func (m *mockStatusWithError) GetExecId() (int32, error)             { return 0, nil }
func (m *mockStatusWithError) GetQueueDepth() (int32, error)         { return 0, nil }
func (m *mockStatusWithError) GetCommandNumEcho() (int32, error)     { return 0, nil }
func (m *mockStatusWithError) GetCommandStatus() (int32, error)      { return 0, nil }
func (m *mockStatusWithError) GetSynchDi(int32) (int32, error)       { return 0, nil }
func (m *mockStatusWithError) GetAnalogInput(int32) (float64, error) { return 0, nil }

func (m *mockStatusWithError) setMotionError() {
	m.mu.Lock()
	m.commandStatus = 3 // INVALID_PARAMS: a rejected motion command (cmd_status_t >= 2)
	m.mu.Unlock()
}

func (m *mockStatusWithError) setSoftLimit() {
	m.mu.Lock()
	m.onSoftLimit = 1
	m.mu.Unlock()
}

func (m *mockStatusWithError) clearSoftLimit() {
	m.mu.Lock()
	m.onSoftLimit = 0
	m.mu.Unlock()
}

// mockErrorPublisher captures operator messages.
type mockErrorPublisher struct {
	mu       sync.Mutex
	errors   []string
	texts    []string
	displays []string
}

func (p *mockErrorPublisher) OperatorError(text string) {
	p.mu.Lock()
	p.errors = append(p.errors, text)
	p.mu.Unlock()
}

func (p *mockErrorPublisher) OperatorText(text string) {
	p.mu.Lock()
	p.texts = append(p.texts, text)
	p.mu.Unlock()
}

func (p *mockErrorPublisher) OperatorDisplay(text string) {
	p.mu.Lock()
	p.displays = append(p.displays, text)
	p.mu.Unlock()
}

func (p *mockErrorPublisher) getErrors() []string {
	p.mu.Lock()
	defer p.mu.Unlock()
	return append([]string(nil), p.errors...)
}

// trackingMotion wraps mockMotion to count specific calls.
type trackingMotion struct {
	mockMotion
	abortCount   atomic.Int32
	disableCount atomic.Int32
	unhomeCount  atomic.Int32
	unhomeJoint  atomic.Int32
	spindleOffs  atomic.Int32
}

func (m *trackingMotion) Abort() error {
	m.abortCount.Add(1)
	return nil
}
func (m *trackingMotion) Disable() error {
	m.disableCount.Add(1)
	return nil
}
func (m *trackingMotion) JointUnhome(joint int32) error {
	m.unhomeCount.Add(1)
	m.unhomeJoint.Store(joint)
	return nil
}
func (m *trackingMotion) SpindleOff(int32) error {
	m.spindleOffs.Add(1)
	return nil
}

func newMonitorTestTask() (*Task, *trackingMotion, *mockIOWithStatus, *mockStatusWithError, *mockErrorPublisher) {
	mot := &trackingMotion{}
	io := &mockIOWithStatus{}
	io.status = IOStatusDone
	stat := &mockStatusWithError{}
	ep := &mockErrorPublisher{}
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	t := NewTask(mot, io, stat, logger)
	t.SetIOStatusReader(io)
	t.SetErrorPublisher(ep)
	t.numSpindles = 2
	t.numJoints = 3
	return t, mot, io, stat, ep
}

// waitForCond polls cond every 2ms until it returns true or the deadline
// elapses, returning cond's final value. Used to synchronize on a monitor
// handler's LAST-set signal, so all of the handler's side effects are complete
// before the test asserts them (the handlers set state/counters/errors in
// sequence, not atomically).
func waitForCond(d time.Duration, cond func() bool) bool {
	deadline := time.Now().Add(d)
	for {
		if cond() {
			return true
		}
		if !time.Now().Before(deadline) {
			return false
		}
		time.Sleep(2 * time.Millisecond)
	}
}

// waitExecState polls until task.execState == want (read under the task lock).
func waitExecState(task *Task, want ExecState, d time.Duration) bool {
	return waitForCond(d, func() bool {
		task.mu.Lock()
		defer task.mu.Unlock()
		return task.execState == want
	})
}

func TestMonitor_ExternalEstop(t *testing.T) {
	task, mot, io, stat, ep := newMonitorTestTask()
	_ = stat

	// Bring machine to ON state.
	task.SetState(int32(StateEstopReset))
	task.SetState(int32(StateOn))
	task.StartSequencer()

	// Start monitor.
	mon := newMonitor(task, nil, nil, io)
	mon.start()
	defer mon.stop()

	// Assert machine is ON.
	task.mu.Lock()
	if task.state != StateOn {
		t.Fatalf("expected StateOn, got %s", task.state)
	}
	task.mu.Unlock()

	// Trigger external estop.
	io.setEstop(true)

	// checkEstop sets state=StateEstop FIRST, then aborts/disables/unhomes/stops
	// spindles and publishes the operator error LAST. Synchronize on that last
	// signal so every side effect below is guaranteed complete (polling on the
	// early state==StateEstop was the source of this test's flakiness).
	estopHandled := waitForCond(500*time.Millisecond, func() bool {
		for _, e := range ep.getErrors() {
			if e == "External E-Stop asserted" {
				return true
			}
		}
		return false
	})
	if !estopHandled {
		t.Fatal("external estop not fully handled: no 'External E-Stop asserted' within deadline")
	}

	// Verify state transitioned to ESTOP.
	task.mu.Lock()
	state := task.state
	interpState := task.interpState
	task.mu.Unlock()

	if state != StateEstop {
		t.Fatalf("expected StateEstop after external estop, got %s", state)
	}
	if interpState != InterpIdle {
		t.Fatalf("expected InterpIdle, got %d", interpState)
	}

	// C11: an external estop must turn lube off and clear lubeOn, matching the
	// commanded estop/off teardown (which it now shares via machineShutdown's
	// stopSignals). Machine-on set lubeOn=true; the estop must clear it.
	task.mu.Lock()
	lubeOn := task.lubeOn
	task.mu.Unlock()
	if lubeOn {
		t.Error("expected lubeOn=false after external estop (C11: lube left on)")
	}

	// Verify motion was aborted and disabled.
	if mot.abortCount.Load() == 0 {
		t.Error("expected motion Abort to be called")
	}
	if mot.disableCount.Load() == 0 {
		t.Error("expected motion Disable to be called")
	}

	// Verify all joints unhomed with -2.
	if mot.unhomeCount.Load() == 0 {
		t.Error("expected JointUnhome to be called")
	}
	if mot.unhomeJoint.Load() != -2 {
		t.Errorf("expected JointUnhome(-2), got JointUnhome(%d)", mot.unhomeJoint.Load())
	}

	// Verify spindles stopped.
	if mot.spindleOffs.Load() < 2 {
		t.Errorf("expected 2 SpindleOff calls, got %d", mot.spindleOffs.Load())
	}

	// Verify operator error was sent.
	errs := ep.getErrors()
	found := false
	for _, e := range errs {
		if e == "External E-Stop asserted" {
			found = true
			break
		}
	}
	if !found {
		t.Errorf("expected 'External E-Stop asserted' operator error, got %v", errs)
	}
}

func TestMonitor_ExternalEstop_AlreadyEstopped(t *testing.T) {
	task, mot, io, _, _ := newMonitorTestTask()

	// Machine is already in ESTOP (initial state).
	io.setEstop(true)

	mon := newMonitor(task, nil, nil, io)
	mon.start()

	// Give monitor time to run a few cycles.
	time.Sleep(30 * time.Millisecond)
	mon.stop()

	// Should NOT have called abort/disable since already estopped.
	if mot.abortCount.Load() != 0 {
		t.Error("should not abort when already in ESTOP")
	}
}

func TestMonitor_EstopClearedByHAL(t *testing.T) {
	task, _, io, _, _ := newMonitorTestTask()

	// Machine starts in ESTOP with emc-enable-in low.
	io.setEstop(true)

	mon := newMonitor(task, nil, nil, io)
	mon.start()
	defer mon.stop()

	// Verify still in ESTOP.
	time.Sleep(20 * time.Millisecond)
	task.mu.Lock()
	if task.state != StateEstop {
		t.Fatalf("expected StateEstop, got %s", task.state)
	}
	task.mu.Unlock()

	// Simulate emc-enable-in going high (external estop released,
	// user-enable-out already high from a prior EstopOff call).
	io.setEstop(false)

	// Wait for monitor to detect it.
	deadline := time.Now().Add(50 * time.Millisecond)
	for time.Now().Before(deadline) {
		task.mu.Lock()
		s := task.state
		task.mu.Unlock()
		if s == StateEstopReset {
			break
		}
		time.Sleep(2 * time.Millisecond)
	}

	task.mu.Lock()
	state := task.state
	task.mu.Unlock()

	if state != StateEstopReset {
		t.Fatalf("expected StateEstopReset when emc-enable-in goes high, got %s", state)
	}
}

func TestSetState_EstopReset_BlockedByHAL(t *testing.T) {
	task, _, io, _, _ := newMonitorTestTask()

	// Machine in ESTOP with emc-enable-in still low (external estop active).
	io.setEstop(true)

	// Try to reset estop — should NOT transition because HAL says no.
	err := task.SetState(int32(StateEstopReset))
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}

	// State should remain ESTOP (HAL didn't confirm).
	task.mu.Lock()
	state := task.state
	task.mu.Unlock()

	if state != StateEstop {
		t.Fatalf("expected StateEstop (HAL estop active), got %s", state)
	}
}

func TestMonitor_MachineOff_EstopSilent(t *testing.T) {
	task, mot, io, _, ep := newMonitorTestTask()

	// Bring machine to EstopReset (machine off but not in estop).
	task.SetState(int32(StateEstopReset))

	task.mu.Lock()
	if task.state != StateEstopReset {
		t.Fatalf("expected StateEstopReset, got %s", task.state)
	}
	task.mu.Unlock()

	mon := newMonitor(task, nil, nil, io)
	mon.start()
	defer mon.stop()

	// Simulate emc-enable-in going low (HW drops it when machine is off).
	io.setEstop(true)

	// Wait for monitor to detect it.
	deadline := time.Now().Add(50 * time.Millisecond)
	for time.Now().Before(deadline) {
		task.mu.Lock()
		s := task.state
		task.mu.Unlock()
		if s == StateEstop {
			break
		}
		time.Sleep(2 * time.Millisecond)
	}

	task.mu.Lock()
	state := task.state
	task.mu.Unlock()

	if state != StateEstop {
		t.Fatalf("expected StateEstop, got %s", state)
	}

	// Should NOT have called abort/disable (machine was already off).
	if mot.abortCount.Load() != 0 {
		t.Error("should not abort when machine was already off")
	}

	// Should NOT have produced an operator error.
	if errs := ep.getErrors(); len(errs) > 0 {
		t.Errorf("expected no operator errors, got %v", errs)
	}
}

func TestMonitor_MotionError(t *testing.T) {
	task, mot, io, stat, _ := newMonitorTestTask()

	// Keep motion self-enabled so checkMotionEnabled stays quiet; otherwise the
	// Enabled==0 self-disable path would abort on the very first tick and this
	// test would pass regardless of whether the motion-error path works at all.
	stat.setEnabled(1)

	// Bring to ON.
	task.SetState(int32(StateEstopReset))
	task.SetState(int32(StateOn))
	task.StartSequencer()

	mon := newMonitor(task, nil, nil, io)
	mon.start()
	defer mon.stop()

	// Trigger a rejected motion command (cmd_status_t >= 2).
	stat.setMotionError()

	// checkMotionErrors sets ExecError LAST (after abort + spindle-off +
	// StartSequencer), so synchronizing on it guarantees those side effects are
	// done — and avoids the transient ExecDone that the aborted old sequencer
	// briefly sets (the flake).
	if !waitExecState(task, ExecError, 500*time.Millisecond) {
		t.Fatal("motion error not handled: execState never reached ExecError")
	}

	if mot.abortCount.Load() == 0 {
		t.Fatal("expected motion Abort on motion error")
	}
	if mot.spindleOffs.Load() < 2 {
		t.Errorf("expected 2 SpindleOff calls, got %d", mot.spindleOffs.Load())
	}
	// checkMotionErrors keeps the machine ON; the self-disable path
	// (checkMotionEnabled) would instead drop to EstopReset. Still-ON proves the
	// abort came from error detection, not the Enabled==0 fallback the previous
	// version of this test relied on.
	task.mu.Lock()
	gotState := task.state
	task.mu.Unlock()
	if gotState != StateOn {
		t.Errorf("state = %v, want StateOn (abort must come from error detection, not self-disable)", gotState)
	}
}

// TestMonitor_MotionDisabled covers the checkMotionEnabled path: motion
// disabling itself while the task believes the machine is on (following error,
// amp fault, watchdog, external enable drop) must abort and drop to EstopReset.
// This is the path the old MotionError/IOError tests accidentally exercised via
// Enabled==0; it now has an intentional owner.
func TestMonitor_MotionDisabled(t *testing.T) {
	task, mot, io, stat, _ := newMonitorTestTask()
	stat.setEnabled(0) // motion reports itself disabled (no command error injected)

	task.SetState(int32(StateEstopReset))
	task.SetState(int32(StateOn))
	task.StartSequencer()

	mon := newMonitor(task, nil, nil, io)
	mon.start()
	defer mon.stop()

	// checkMotionEnabled sets ExecError LAST; synchronize on it.
	if !waitExecState(task, ExecError, 500*time.Millisecond) {
		t.Fatal("self-disable not handled: execState never reached ExecError")
	}

	if mot.abortCount.Load() == 0 {
		t.Fatal("expected Abort when motion disables itself")
	}
	if mot.spindleOffs.Load() < 2 {
		t.Errorf("expected 2 SpindleOff calls, got %d", mot.spindleOffs.Load())
	}
	// Unexpected self-disable drops the machine out of ON to EstopReset.
	task.mu.Lock()
	gotState := task.state
	task.mu.Unlock()
	if gotState != StateEstopReset {
		t.Errorf("state = %v, want StateEstopReset", gotState)
	}
}

func TestMonitor_SoftLimit(t *testing.T) {
	task, _, io, stat, ep := newMonitorTestTask()

	// Bring to ON.
	task.SetState(int32(StateEstopReset))
	task.SetState(int32(StateOn))

	mon := newMonitor(task, nil, nil, io)
	mon.start()
	defer mon.stop()

	// Trigger soft limit.
	stat.setSoftLimit()

	// Wait for operator error.
	deadline := time.Now().Add(50 * time.Millisecond)
	for time.Now().Before(deadline) {
		if errs := ep.getErrors(); len(errs) > 0 {
			break
		}
		time.Sleep(2 * time.Millisecond)
	}

	errs := ep.getErrors()
	found := false
	for _, e := range errs {
		if e == "On Soft Limit" {
			found = true
			break
		}
	}
	if !found {
		t.Errorf("expected 'On Soft Limit' error, got %v", errs)
	}

	// Clear soft limit — should not report again.
	stat.clearSoftLimit()
	time.Sleep(30 * time.Millisecond)

	// Count how many soft limit messages (should be exactly 1).
	count := 0
	for _, e := range ep.getErrors() {
		if e == "On Soft Limit" {
			count++
		}
	}
	if count != 1 {
		t.Errorf("expected exactly 1 soft limit message, got %d", count)
	}
}

func TestMonitor_IOError(t *testing.T) {
	task, mot, io, stat, _ := newMonitorTestTask()
	stat.setEnabled(1) // keep checkMotionEnabled quiet — the abort must come from the IO fault

	// Bring to ON.
	task.SetState(int32(StateEstopReset))
	task.SetState(int32(StateOn))
	task.StartSequencer()

	mon := newMonitor(task, nil, nil, io)
	mon.start()
	defer mon.stop()

	// Trigger IO error with reason <= 0 (hard fault).
	io.setError(-1)

	// Synchronize on the terminal ExecError (set last by checkMotionErrors).
	if !waitExecState(task, ExecError, 500*time.Millisecond) {
		t.Fatal("IO hard fault not handled: execState never reached ExecError")
	}

	if mot.abortCount.Load() == 0 {
		t.Fatal("expected motion Abort on IO hard fault")
	}
	if mot.spindleOffs.Load() < 2 {
		t.Errorf("expected 2 SpindleOff calls, got %d", mot.spindleOffs.Load())
	}
	// Same discriminator as MotionError: still ON proves the abort came from
	// IO-fault detection, not the Enabled==0 self-disable path.
	task.mu.Lock()
	gotState := task.state
	task.mu.Unlock()
	if gotState != StateOn {
		t.Errorf("state = %v, want StateOn (abort must come from IO fault, not self-disable)", gotState)
	}
}

// TestMonitor_IOSoftFault_NoAbort pins the inverse safety case: an IO fault with
// reason > 0 is a soft/recoverable fault (e.g. a toolchanger prompt) and must
// NOT abort the machine. Only reason <= 0 is a hard fault. Without this, a
// regression that aborted on every soft fault (e.g. every M6) would pass.
func TestMonitor_IOSoftFault_NoAbort(t *testing.T) {
	task, mot, io, stat, _ := newMonitorTestTask()
	stat.setEnabled(1)

	task.SetState(int32(StateEstopReset))
	task.SetState(int32(StateOn))
	task.StartSequencer()

	mon := newMonitor(task, nil, nil, io)
	mon.start()
	defer mon.stop()

	io.setError(1) // reason > 0: soft fault, must be ignored by the safety monitor

	// Give the monitor several ticks to (wrongly) react, then assert it didn't.
	time.Sleep(30 * time.Millisecond)

	if got := mot.abortCount.Load(); got != 0 {
		t.Errorf("soft IO fault (reason>0) must NOT abort, got %d Abort calls", got)
	}
	task.mu.Lock()
	gotState := task.state
	task.mu.Unlock()
	if gotState != StateOn {
		t.Errorf("state = %v, want StateOn (soft fault must not change machine state)", gotState)
	}
}

// TestMonitor_EstopDetectionWhileCmdMuHeld pins the safety-loop property the
// monitor/halui split exists for: external-estop detection and its stop
// signals must fire even while a command (or a halui-dispatched command in
// the halui loop) holds cmdMu. Only the teardown *cleanup* is allowed to wait
// for the lock.
func TestMonitor_EstopDetectionWhileCmdMuHeld(t *testing.T) {
	task, mot, io, _, _ := newMonitorTestTask()
	task.StartSequencer()
	defer task.StopSequencer()
	bringUp(task)

	mon := newMonitor(task, nil, nil, io)
	mon.start()

	// Wedge cmdMu, simulating a long-running command.
	task.cmdMu.Lock()

	io.setEstop(true)

	// Detection + signal phase must complete without cmdMu: state flips to
	// ESTOP and motion is aborted+disabled.
	if !waitForCond(2*time.Second, func() bool {
		task.mu.Lock()
		st := task.state
		task.mu.Unlock()
		return st == StateEstop && mot.abortCount.Load() > 0 && mot.disableCount.Load() > 0
	}) {
		task.cmdMu.Unlock()
		t.Fatal("estop signals did not fire while cmdMu was held")
	}

	// Release the "command" — the monitor's cleanup phase (interp reset,
	// sequencer restart) can now finish and the monitor must shut down cleanly.
	task.cmdMu.Unlock()
	mon.stop()
}
