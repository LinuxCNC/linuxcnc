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
	commandStatus int32 // 3 = RCS_ERROR
	onSoftLimit   int32
	joints        [16]motstat.JointStatus
}

func (m *mockStatusWithError) GetStatus() (motstat.MotionStatus, error) {
	m.mu.Lock()
	defer m.mu.Unlock()
	ms := motstat.MotionStatus{
		CommandStatus: m.commandStatus,
		OnSoftLimit:   m.onSoftLimit,
	}
	ms.Joints = m.joints
	return ms, nil
}
func (m *mockStatusWithError) GetPosCmd() (motstat.Pose, error)  { return motstat.Pose{}, nil }
func (m *mockStatusWithError) GetPosFb() (motstat.Pose, error)   { return motstat.Pose{}, nil }
func (m *mockStatusWithError) GetInpos() (int32, error)          { return 1, nil }
func (m *mockStatusWithError) GetExecId() (int32, error)         { return 0, nil }
func (m *mockStatusWithError) GetQueueDepth() (int32, error)     { return 0, nil }
func (m *mockStatusWithError) GetCommandNumEcho() (int32, error) { return 0, nil }
func (m *mockStatusWithError) GetCommandStatus() (int32, error)  { return 0, nil }

func (m *mockStatusWithError) setMotionError() {
	m.mu.Lock()
	m.commandStatus = 3 // RCS_ERROR
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

	// Wait for monitor to detect it (max 50ms).
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

	// Bring to ON.
	task.SetState(int32(StateEstopReset))
	task.SetState(int32(StateOn))
	task.StartSequencer()

	mon := newMonitor(task, nil, nil, io)
	mon.start()
	defer mon.stop()

	// Trigger motion error.
	stat.setMotionError()

	// Wait for detection (monitor calls Abort).
	deadline := time.Now().Add(50 * time.Millisecond)
	for time.Now().Before(deadline) {
		if mot.abortCount.Load() > 0 {
			break
		}
		time.Sleep(2 * time.Millisecond)
	}

	if mot.abortCount.Load() == 0 {
		t.Fatal("expected motion Abort on motion error")
	}

	// Verify spindles stopped.
	if mot.spindleOffs.Load() < 2 {
		t.Errorf("expected 2 SpindleOff calls, got %d", mot.spindleOffs.Load())
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
	task, mot, io, _, _ := newMonitorTestTask()

	// Bring to ON.
	task.SetState(int32(StateEstopReset))
	task.SetState(int32(StateOn))
	task.StartSequencer()

	mon := newMonitor(task, nil, nil, io)
	mon.start()
	defer mon.stop()

	// Trigger IO error with reason <= 0 (hard fault).
	io.setError(-1)

	// Wait for detection.
	deadline := time.Now().Add(50 * time.Millisecond)
	for time.Now().Before(deadline) {
		if mot.abortCount.Load() > 0 {
			break
		}
		time.Sleep(2 * time.Millisecond)
	}

	if mot.abortCount.Load() == 0 {
		t.Fatal("expected motion Abort on IO error")
	}

	// Verify spindles stopped.
	if mot.spindleOffs.Load() < 2 {
		t.Errorf("expected 2 SpindleOff calls, got %d", mot.spindleOffs.Load())
	}
}
