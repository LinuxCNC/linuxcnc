// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"errors"
	"log/slog"
	"os"
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/motstat"
)

// mockMotion implements MotionController for testing.
type mockMotion struct {
	lastCall string
}

func (m *mockMotion) SetLine(Pose, float64, float64, float64, int32, int64, float64, int32) error {
	m.lastCall = "SetLine"
	return nil
}
func (m *mockMotion) SetCircle(Pose, Cartesian, Cartesian, int32, float64, float64, float64, int32, int64, float64) error {
	return nil
}
func (m *mockMotion) Probe(Pose, float64, float64, float64, int32, uint8, int64, float64) error {
	return nil
}
func (m *mockMotion) RigidTap(Pose, float64, float64, float64, float64, int64, float64) error {
	return nil
}
func (m *mockMotion) Abort() error     { m.lastCall = "Abort"; return nil }
func (m *mockMotion) Pause() error     { m.lastCall = "Pause"; return nil }
func (m *mockMotion) Resume() error    { m.lastCall = "Resume"; return nil }
func (m *mockMotion) Step(int64) error { m.lastCall = "Step"; return nil }
func (m *mockMotion) Reverse() error   { m.lastCall = "Reverse"; return nil }
func (m *mockMotion) Forward() error   { m.lastCall = "Forward"; return nil }
func (m *mockMotion) SetFree() error   { m.lastCall = "SetFree"; return nil }
func (m *mockMotion) SetCoord() error  { m.lastCall = "SetCoord"; return nil }
func (m *mockMotion) SetTeleop() error { m.lastCall = "SetTeleop"; return nil }
func (m *mockMotion) Enable() error    { m.lastCall = "Enable"; return nil }
func (m *mockMotion) Disable() error   { m.lastCall = "Disable"; return nil }
func (m *mockMotion) JogCont(int32, float64, int32) error {
	m.lastCall = "JogCont"
	return nil
}
func (m *mockMotion) JogIncr(int32, float64, float64, int32) error {
	m.lastCall = "JogIncr"
	return nil
}
func (m *mockMotion) JogAbs(int32, float64, float64, int32) error {
	m.lastCall = "JogAbs"
	return nil
}
func (m *mockMotion) JogAbort(int32, int32) error {
	m.lastCall = "JogAbort"
	return nil
}
func (m *mockMotion) SpindleOn(int32, float64, float64, float64, int32) error {
	m.lastCall = "SpindleOn"
	return nil
}
func (m *mockMotion) SpindleOff(int32) error                    { m.lastCall = "SpindleOff"; return nil }
func (m *mockMotion) SpindleOrient(int32, float64, int32) error { return nil }
func (m *mockMotion) SpindleIncrease(int32) error               { m.lastCall = "SpindleIncrease"; return nil }
func (m *mockMotion) SpindleDecrease(int32) error               { m.lastCall = "SpindleDecrease"; return nil }
func (m *mockMotion) SpindleBrakeEngage(int32) error            { m.lastCall = "SpindleBrakeEngage"; return nil }
func (m *mockMotion) SpindleBrakeRelease(int32) error           { m.lastCall = "SpindleBrakeRelease"; return nil }
func (m *mockMotion) SetSpindleScale(int32, float64) error {
	m.lastCall = "SetSpindleScale"
	return nil
}
func (m *mockMotion) SetFeedScale(float64) error                   { m.lastCall = "SetFeedScale"; return nil }
func (m *mockMotion) SetRapidScale(float64) error                  { m.lastCall = "SetRapidScale"; return nil }
func (m *mockMotion) SetMaxFeedOverride(float64) error             { return nil }
func (m *mockMotion) FeedScaleEnable(int32) error                  { return nil }
func (m *mockMotion) SpindleScaleEnable(int32, int32) error        { return nil }
func (m *mockMotion) AdaptiveFeedEnable(int32) error               { return nil }
func (m *mockMotion) FeedHoldEnable(int32) error                   { return nil }
func (m *mockMotion) OverrideLimits(int32) error                   { m.lastCall = "OverrideLimits"; return nil }
func (m *mockMotion) JointHome(int32) error                        { m.lastCall = "JointHome"; return nil }
func (m *mockMotion) JointUnhome(int32) error                      { m.lastCall = "JointUnhome"; return nil }
func (m *mockMotion) SetVel(float64) error                         { return nil }
func (m *mockMotion) SetVelLimit(float64) error                    { m.lastCall = "SetVelLimit"; return nil }
func (m *mockMotion) SetAcc(float64) error                         { return nil }
func (m *mockMotion) SetTermCond(int32, float64) error             { return nil }
func (m *mockMotion) SetOffset(Pose) error                         { return nil }
func (m *mockMotion) SetDebug(int32) error                         { m.lastCall = "SetDebug"; return nil }
func (m *mockMotion) SetDout(int32, int32) error                   { return nil }
func (m *mockMotion) SetDoutSynched(int32, int32, int32) error     { return nil }
func (m *mockMotion) SetAout(int32, float64) error                 { return nil }
func (m *mockMotion) SetAoutSynched(int32, float64, float64) error { return nil }
func (m *mockMotion) SetSpindlesync(float64, int32) error          { return nil }

// mockIO implements IOController for testing.
type mockIO struct {
	lastCall string
}

func (m *mockIO) CoolantFloodOn() error     { m.lastCall = "FloodOn"; return nil }
func (m *mockIO) CoolantFloodOff() error    { m.lastCall = "FloodOff"; return nil }
func (m *mockIO) CoolantMistOn() error      { m.lastCall = "MistOn"; return nil }
func (m *mockIO) CoolantMistOff() error     { m.lastCall = "MistOff"; return nil }
func (m *mockIO) LubeOn() error             { m.lastCall = "LubeOn"; return nil }
func (m *mockIO) LubeOff() error            { m.lastCall = "LubeOff"; return nil }
func (m *mockIO) ToolPrepare(int32) error   { return nil }
func (m *mockIO) ToolLoad() error           { return nil }
func (m *mockIO) ToolUnload() error         { return nil }
func (m *mockIO) ToolStartChange() error    { return nil }
func (m *mockIO) ToolSetNumber(int32) error { return nil }
func (m *mockIO) ToolSetOffset(int32, int32, float64, float64, float64, float64, float64, float64, float64, float64, float64, float64, float64, float64, int32) error {
	return nil
}
func (m *mockIO) ToolLoadTable(string) error       { return nil }
func (m *mockIO) EstopOn() error                   { m.lastCall = "Estop"; return nil }
func (m *mockIO) EstopOff() error                  { m.lastCall = "EstopReset"; return nil }
func (m *mockIO) IoAbort(int32) error              { return nil }
func (m *mockIO) SetDebug(int32) error             { return nil }
func (m *mockIO) GetCmdStatus() (int32, error)     { return IOStatusDone, nil }
func (m *mockIO) GetToolInSpindle() (int32, error) { return 0, nil }
func (m *mockIO) GetPocketPrepped() (int32, error) { return 0, nil }

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
	if mot.lastCall != "Enable" {
		t.Fatalf("expected Enable, got %s", mot.lastCall)
	}
	if task.state != StateOn {
		t.Fatalf("expected StateOn, got %s", task.state)
	}
}

func TestSetMode_RequiresOn(t *testing.T) {
	task, _, _ := newTestTask()

	err := task.SetMode(int32(ModeAuto))
	if !errors.Is(err, ErrNotOn) {
		t.Fatalf("expected ErrNotOn, got %v", err)
	}
}

func TestSetMode_AutoSetCoord(t *testing.T) {
	task, mot, _ := newTestTask()
	bringUp(task)

	if err := task.SetMode(int32(ModeAuto)); err != nil {
		t.Fatalf("set_mode auto: %v", err)
	}
	if mot.lastCall != "SetCoord" {
		t.Fatalf("expected SetCoord, got %s", mot.lastCall)
	}
	if task.mode != ModeAuto {
		t.Fatalf("expected ModeAuto, got %s", task.mode)
	}
}

func TestSetMode_ManualSetFree(t *testing.T) {
	task, mot, _ := newTestTask()
	bringUp(task)
	task.SetMode(int32(ModeAuto))

	if err := task.SetMode(int32(ModeManual)); err != nil {
		t.Fatalf("set_mode manual: %v", err)
	}
	if mot.lastCall != "SetFree" {
		t.Fatalf("expected SetFree, got %s", mot.lastCall)
	}
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
	if mot.lastCall != "JogCont" {
		t.Fatalf("expected JogCont, got %s", mot.lastCall)
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

func TestAutoCommand_EnsureModeWhenIdle(t *testing.T) {
	task, _, _ := newTestTask()
	bringUp(task)

	// In Manual mode with idle interpreter, AutoCommand should auto-switch
	// to AUTO mode. Since no program is loaded, we get ErrNoProgram (not ErrWrongMode).
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
	if mot.lastCall != "Pause" {
		t.Fatalf("expected Pause, got %s", mot.lastCall)
	}
}

func TestFlood_On(t *testing.T) {
	task, _, io := newTestTask()
	bringUp(task)

	if err := task.Flood(true); err != nil {
		t.Fatalf("flood on: %v", err)
	}
	if io.lastCall != "FloodOn" {
		t.Fatalf("expected FloodOn, got %s", io.lastCall)
	}
}

func TestAbort_AlwaysSucceeds(t *testing.T) {
	task, mot, _ := newTestTask()

	if err := task.Abort(); err != nil {
		t.Fatalf("abort: %v", err)
	}
	if mot.lastCall != "Abort" {
		t.Fatalf("expected Abort, got %s", mot.lastCall)
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
	if mot.lastCall != "SpindleOn" {
		t.Fatalf("expected SpindleOn, got %s", mot.lastCall)
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
	if mot.lastCall != "JointHome" {
		t.Fatalf("expected JointHome, got %s", mot.lastCall)
	}
}
