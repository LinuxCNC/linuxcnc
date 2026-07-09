// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"errors"
	"log/slog"
	"os"
	"sync"
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/motstat"
)

// mockMotion implements MotionController for testing.
// lastCall is written from concurrent goroutines (signal-class task paths run
// concurrently with serialized commands by design), so writes are locked.
type mockMotion struct {
	mu       sync.Mutex
	lastCall string
}

func (m *mockMotion) setCall(s string) {
	m.mu.Lock()
	m.lastCall = s
	m.mu.Unlock()
}

func (m *mockMotion) last() string {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.lastCall
}

func (m *mockMotion) SetLine(Pose, float64, float64, float64, int32, int32, float64, int32) error {
	m.setCall("SetLine")
	return nil
}
func (m *mockMotion) SetCircle(Pose, Cartesian, Cartesian, int32, float64, float64, float64, int32, int32, float64) error {
	return nil
}
func (m *mockMotion) Probe(Pose, float64, float64, float64, int32, uint8, int32, float64) error {
	return nil
}
func (m *mockMotion) RigidTap(Pose, float64, float64, float64, float64, int32, float64) error {
	return nil
}
func (m *mockMotion) Abort() error     { m.setCall("Abort"); return nil }
func (m *mockMotion) Pause() error     { m.setCall("Pause"); return nil }
func (m *mockMotion) Resume() error    { m.setCall("Resume"); return nil }
func (m *mockMotion) Step(int32) error { m.setCall("Step"); return nil }
func (m *mockMotion) Reverse() error   { m.setCall("Reverse"); return nil }
func (m *mockMotion) Forward() error   { m.setCall("Forward"); return nil }
func (m *mockMotion) SetFree() error   { m.setCall("SetFree"); return nil }
func (m *mockMotion) SetCoord() error  { m.setCall("SetCoord"); return nil }
func (m *mockMotion) SetTeleop() error { m.setCall("SetTeleop"); return nil }
func (m *mockMotion) Enable() error    { m.setCall("Enable"); return nil }
func (m *mockMotion) Disable() error   { m.setCall("Disable"); return nil }
func (m *mockMotion) JogCont(int32, float64, int32) error {
	m.setCall("JogCont")
	return nil
}
func (m *mockMotion) JogIncr(int32, float64, float64, int32) error {
	m.setCall("JogIncr")
	return nil
}
func (m *mockMotion) JogAbs(int32, float64, float64, int32) error {
	m.setCall("JogAbs")
	return nil
}
func (m *mockMotion) JogAbort(int32, int32) error {
	m.setCall("JogAbort")
	return nil
}
func (m *mockMotion) SpindleOn(int32, float64, float64, float64, int32) error {
	m.setCall("SpindleOn")
	return nil
}
func (m *mockMotion) SpindleOff(int32) error                    { m.setCall("SpindleOff"); return nil }
func (m *mockMotion) SpindleOrient(int32, float64, int32) error { return nil }
func (m *mockMotion) SpindleIncrease(int32) error               { m.setCall("SpindleIncrease"); return nil }
func (m *mockMotion) SpindleDecrease(int32) error               { m.setCall("SpindleDecrease"); return nil }
func (m *mockMotion) SpindleBrakeEngage(int32) error            { m.setCall("SpindleBrakeEngage"); return nil }
func (m *mockMotion) SpindleBrakeRelease(int32) error           { m.setCall("SpindleBrakeRelease"); return nil }
func (m *mockMotion) SetSpindleScale(int32, float64) error {
	m.setCall("SetSpindleScale")
	return nil
}
func (m *mockMotion) SetFeedScale(float64) error                   { m.setCall("SetFeedScale"); return nil }
func (m *mockMotion) SetRapidScale(float64) error                  { m.setCall("SetRapidScale"); return nil }
func (m *mockMotion) SetMaxFeedOverride(float64) error             { return nil }
func (m *mockMotion) FeedScaleEnable(int32) error                  { return nil }
func (m *mockMotion) SpindleScaleEnable(int32, int32) error        { return nil }
func (m *mockMotion) AdaptiveFeedEnable(int32) error               { return nil }
func (m *mockMotion) FeedHoldEnable(int32) error                   { return nil }
func (m *mockMotion) OverrideLimits(int32) error                   { m.setCall("OverrideLimits"); return nil }
func (m *mockMotion) JointHome(int32) error                        { m.setCall("JointHome"); return nil }
func (m *mockMotion) JointUnhome(int32) error                      { m.setCall("JointUnhome"); return nil }
func (m *mockMotion) SetVel(float64) error                         { return nil }
func (m *mockMotion) SetVelLimit(float64) error                    { m.setCall("SetVelLimit"); return nil }
func (m *mockMotion) SetAcc(float64) error                         { return nil }
func (m *mockMotion) SetTermCond(int32, float64) error             { return nil }
func (m *mockMotion) SetOffset(Pose) error                         { return nil }
func (m *mockMotion) SetDebug(int32) error                         { m.setCall("SetDebug"); return nil }
func (m *mockMotion) SetDout(int32, int32) error                   { return nil }
func (m *mockMotion) SetDoutSynched(int32, int32, int32) error     { return nil }
func (m *mockMotion) SetAout(int32, float64) error                 { return nil }
func (m *mockMotion) SetAoutSynched(int32, float64, float64) error { return nil }
func (m *mockMotion) SetSpindlesync(float64, int32) error          { return nil }

// mockIO implements IOController and IOStatusReader for testing.
type mockIO struct {
	mu       sync.Mutex
	lastCall string
}

func (m *mockIO) setCall(s string) {
	m.mu.Lock()
	m.lastCall = s
	m.mu.Unlock()
}

func (m *mockIO) last() string {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.lastCall
}

func (m *mockIO) CoolantFloodOn() error     { m.setCall("FloodOn"); return nil }
func (m *mockIO) CoolantFloodOff() error    { m.setCall("FloodOff"); return nil }
func (m *mockIO) CoolantMistOn() error      { m.setCall("MistOn"); return nil }
func (m *mockIO) CoolantMistOff() error     { m.setCall("MistOff"); return nil }
func (m *mockIO) LubeOn() error             { m.setCall("LubeOn"); return nil }
func (m *mockIO) LubeOff() error            { m.setCall("LubeOff"); return nil }
func (m *mockIO) ToolPrepare(int32) error   { return nil }
func (m *mockIO) ToolLoad() error           { return nil }
func (m *mockIO) ToolUnload() error         { return nil }
func (m *mockIO) ToolStartChange() error    { return nil }
func (m *mockIO) ToolSetNumber(int32) error { return nil }
func (m *mockIO) ToolSetOffset(int32, int32, float64, float64, float64, float64, float64, float64, float64, float64, float64, float64, float64, float64, int32) error {
	return nil
}
func (m *mockIO) ToolLoadTable(string) error       { return nil }
func (m *mockIO) EstopOn() error                   { m.setCall("Estop"); return nil }
func (m *mockIO) EstopOff() error                  { m.setCall("EstopReset"); return nil }
func (m *mockIO) IoAbort(int32) error              { return nil }
func (m *mockIO) SetDebug(int32) error             { return nil }
func (m *mockIO) GetCmdStatus() (int32, error)     { return IOStatusDone, nil }
func (m *mockIO) GetToolInSpindle() (int32, error) { return 0, nil }
func (m *mockIO) GetPocketPrepped() (int32, error) { return 0, nil }
func (m *mockIO) GetIOFullStatus() (IOFullStatus, error) {
	return IOFullStatus{Estop: false}, nil
}

// mockStatus implements MotionStatusReader for testing.
type mockStatus struct{}

func (m *mockStatus) GetStatus() (motstat.MotionStatus, error) { return motstat.MotionStatus{}, nil }
func (m *mockStatus) GetPosCmd() (motstat.Pose, error)         { return motstat.Pose{}, nil }
func (m *mockStatus) GetPosFb() (motstat.Pose, error)          { return motstat.Pose{}, nil }
func (m *mockStatus) GetInpos() (int32, error)                 { return 1, nil }
func (m *mockStatus) GetExecId() (int32, error)                { return 0, nil }
func (m *mockStatus) GetQueueDepth() (int32, error)            { return 0, nil }
func (m *mockStatus) GetCommandNumEcho() (int32, error)        { return 0, nil }
func (m *mockStatus) GetCommandStatus() (int32, error)         { return 0, nil }

func newTestTask() (*Task, *mockMotion, *mockIO) {
	mot := &mockMotion{}
	io := &mockIO{}
	stat := &mockStatus{}
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	t := NewTask(mot, io, stat, logger)
	t.SetIOStatusReader(io)
	return t, mot, io
}

// bringUp transitions the task from estop to ON.
func bringUp(t *Task) {
	t.SetState(int32(StateEstopReset))
	t.SetState(int32(StateOn))
}

func TestSetState_PowerOn(t *testing.T) {
	task, mot, _ := newTestTask()

	// Can't go directly to ON from estop
	err := task.SetState(int32(StateOn))
	if err == nil {
		t.Fatal("expected error going from ESTOP to ON")
	}

	// estop → estop_reset → on
	if err := task.SetState(int32(StateEstopReset)); err != nil {
		t.Fatalf("estop_reset: %v", err)
	}
	if err := task.SetState(int32(StateOn)); err != nil {
		t.Fatalf("on: %v", err)
	}
	if mot.last() != "Enable" {
		t.Fatalf("expected Enable, got %s", mot.last())
	}
	if task.state != StateOn {
		t.Fatalf("expected StateOn, got %s", task.state)
	}
}

// SetMode is accepted in any machine state, including ESTOP/OFF, matching C++:
// EMC_TASK_SET_MODE is handled under the OFF/ESTOP/ESTOP_RESET case
// (emctaskmain.cc:825) and emcTaskSetMode itself has no on-state gate
// (emctask.cc:264). A UI mode selector must be honored before the machine is
// enabled so the reported mode tracks the selector; the motion-mode side
// effects are no-ops while motion is disabled.
func TestSetMode_AllowedInEstop(t *testing.T) {
	task, _, _ := newTestTask() // fresh task is in ESTOP

	if err := task.SetMode(int32(ModeAuto)); err != nil {
		t.Fatalf("SetMode(Auto) in estop: unexpected error %v", err)
	}
	if task.mode != ModeAuto {
		t.Fatalf("mode = %v, want ModeAuto", task.mode)
	}
}

func TestSetMode_AutoSetCoord(t *testing.T) {
	task, mot, _ := newTestTask()
	bringUp(task)

	if err := task.SetMode(int32(ModeAuto)); err != nil {
		t.Fatalf("set_mode auto: %v", err)
	}
	if mot.last() != "SetCoord" {
		t.Fatalf("expected SetCoord, got %s", mot.last())
	}
	if task.mode != ModeAuto {
		t.Fatalf("expected ModeAuto, got %s", task.mode)
	}
}

// homedStatus reports the first `homed` joints as homed; used to drive
// allHomed() in SetMode's jog-mode selection.
type homedStatus struct {
	mockStatus
	homed int
}

func (h *homedStatus) GetStatus() (motstat.MotionStatus, error) {
	var ms motstat.MotionStatus
	for j := 0; j < h.homed && j < len(ms.Joints); j++ {
		ms.Joints[j].Homed = 1
	}
	return ms, nil
}

// SetMode(MANUAL) picks the motion jog mode by homing state, matching C++
// emcTaskSetMode(EMC_TASK_MODE_MANUAL) (emctask.cc:274): all joints homed ->
// TELEOP (world/axis jog), otherwise FREE (per-joint jog).
func TestSetMode_ManualJogMode(t *testing.T) {
	t.Run("unhomed->SetFree", func(t *testing.T) {
		task, mot, _ := newTestTask()
		task.numJoints = 3 // mockStatus reports all joints unhomed
		bringUp(task)
		task.SetMode(int32(ModeAuto))

		if err := task.SetMode(int32(ModeManual)); err != nil {
			t.Fatalf("SetMode(Manual): %v", err)
		}
		if mot.last() != "SetFree" {
			t.Fatalf("lastCall = %s, want SetFree", mot.last())
		}
	})

	t.Run("homed->SetTeleop", func(t *testing.T) {
		task, mot, _ := newTestTask()
		task.status = &homedStatus{homed: 3}
		task.numJoints = 3
		bringUp(task)
		task.SetMode(int32(ModeAuto))

		if err := task.SetMode(int32(ModeManual)); err != nil {
			t.Fatalf("SetMode(Manual): %v", err)
		}
		if mot.last() != "SetTeleop" {
			t.Fatalf("lastCall = %s, want SetTeleop", mot.last())
		}
	})
}

func TestJog_RequiresOn(t *testing.T) {
	task, _, _ := newTestTask()

	err := task.Jog(JogContinuous, true, 0, 100, 0)
	if !errors.Is(err, ErrNotOn) {
		t.Fatalf("expected ErrNotOn, got %v", err)
	}
}

func TestJog_ManualMode(t *testing.T) {
	task, mot, _ := newTestTask()
	bringUp(task)

	if err := task.Jog(JogContinuous, true, 0, 100, 0); err != nil {
		t.Fatalf("jog: %v", err)
	}
	if mot.last() != "JogCont" {
		t.Fatalf("expected JogCont, got %s", mot.last())
	}
}

func TestJog_MDIBusyRejects(t *testing.T) {
	task, _, _ := newTestTask()
	bringUp(task)
	task.SetMode(int32(ModeMDI))
	task.interpState = InterpReading

	err := task.Jog(JogContinuous, true, 0, 100, 0)
	if !errors.Is(err, ErrBusy) {
		t.Fatalf("expected ErrBusy, got %v", err)
	}
}

// TestAutoCommand_EnsureModeWhenIdle pins an INTENTIONAL divergence from C++
// milltask. In an idle Manual mode, a RUN command auto-switches the machine to
// AUTO (ensureMode) and proceeds; C++ instead rejects it ("Can't do that in
// manual mode", emctaskmain.cc:1003-1007). This is the deliberate gomc
// "auto change mode" model: a mode selector on the command, not a precondition.
// Verified: with no program loaded the error is ErrNoProgram (the mode switch
// happened and the run path ran), NOT ErrWrongMode (rejected in manual).
func TestAutoCommand_EnsureModeWhenIdle(t *testing.T) {
	task, _, _ := newTestTask()
	bringUp(task)

	err := task.AutoCommand(AutoRun, 0)
	if !errors.Is(err, ErrNoProgram) {
		t.Fatalf("expected ErrNoProgram (mode auto-switched), got %v", err)
	}
}

func TestEnsureMode_RejectsWhenBusy(t *testing.T) {
	task, _, _ := newTestTask()
	bringUp(task)
	task.SetMode(int32(ModeAuto))

	// Simulate interpreter busy.
	task.mu.Lock()
	task.interpState = InterpReading
	task.mu.Unlock()

	// MDI requires MDI mode; ensureMode should fail because interp is busy.
	err := task.MDI("G0 X0")
	if !errors.Is(err, ErrBusy) {
		t.Fatalf("expected ErrBusy when interp active, got %v", err)
	}
}

func TestAutoCommand_RunRequiresProgram(t *testing.T) {
	task, _, _ := newTestTask()
	bringUp(task)
	task.SetMode(int32(ModeAuto))

	err := task.AutoCommand(AutoRun, 0)
	if !errors.Is(err, ErrNoProgram) {
		t.Fatalf("expected ErrNoProgram, got %v", err)
	}
}

func TestAutoCommand_PauseCallsMotion(t *testing.T) {
	task, mot, _ := newTestTask()
	bringUp(task)
	task.SetMode(int32(ModeAuto))

	if err := task.AutoCommand(AutoPause, 0); err != nil {
		t.Fatalf("auto pause: %v", err)
	}
	if mot.last() != "Pause" {
		t.Fatalf("expected Pause, got %s", mot.last())
	}
}

func TestFlood_On(t *testing.T) {
	task, _, io := newTestTask()
	bringUp(task)

	if err := task.Flood(true); err != nil {
		t.Fatalf("flood on: %v", err)
	}
	if io.last() != "FloodOn" {
		t.Fatalf("expected FloodOn, got %s", io.last())
	}
}

func TestAbort_AlwaysSucceeds(t *testing.T) {
	task, mot, _ := newTestTask()

	if err := task.Abort(); err != nil {
		t.Fatalf("abort: %v", err)
	}
	if mot.last() != "Abort" {
		t.Fatalf("expected Abort, got %s", mot.last())
	}
	if task.interpState != InterpIdle {
		t.Fatalf("expected InterpIdle after abort")
	}
}

func TestSpindle_Forward(t *testing.T) {
	task, mot, _ := newTestTask()
	bringUp(task)

	if err := task.Spindle(SpindleForward, 1000, 0, 0); err != nil {
		t.Fatalf("spindle fwd: %v", err)
	}
	if mot.last() != "SpindleOn" {
		t.Fatalf("expected SpindleOn, got %s", mot.last())
	}
}

func TestProgramOpen_AnyModeAnyState(t *testing.T) {
	task, _, _ := newTestTask()
	bringUp(task)

	// Works in MANUAL mode (no mode guard)
	if err := task.ProgramOpen("test.ngc"); err != nil {
		t.Fatalf("program_open in MANUAL: %v", err)
	}
	if !task.programOpen {
		t.Fatal("expected programOpen=true")
	}

	// Also works in AUTO
	task.SetMode(int32(ModeAuto))
	if err := task.ProgramOpen("test2.ngc"); err != nil {
		t.Fatalf("program_open in AUTO: %v", err)
	}
}

func TestHome_RequiresOn(t *testing.T) {
	task, _, _ := newTestTask()

	err := task.Home(0)
	if !errors.Is(err, ErrNotOn) {
		t.Fatalf("expected ErrNotOn, got %v", err)
	}
}

func TestHome_CallsMotion(t *testing.T) {
	task, mot, _ := newTestTask()
	bringUp(task)

	if err := task.Home(0); err != nil {
		t.Fatalf("home: %v", err)
	}
	if mot.last() != "JointHome" {
		t.Fatalf("expected JointHome, got %s", mot.last())
	}
}
