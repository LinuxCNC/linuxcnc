// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"log/slog"
	"math"
	"os"
	"testing"
)

func newCanonTestTask() (*Task, *testMotion, *mockIO) {
	mot := &testMotion{failAt: -1}
	io := &mockIO{}
	stat := &testStatus{}
	stat.inPosition.Store(true)
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	t := NewTask(mot, io, stat, logger)
	return t, mot, io
}

func TestCanon_UnitConversion(t *testing.T) {
	cs := NewCanonState()

	// Default is MM
	if cs.unitScale() != 1.0 {
		t.Fatalf("expected 1.0 for MM, got %f", cs.unitScale())
	}

	cs.lengthUnits = CanonUnitsInches
	if cs.unitScale() != 25.4 {
		t.Fatalf("expected 25.4 for inches, got %f", cs.unitScale())
	}

	// 1 inch = 25.4mm
	if cs.fromProg(1.0) != 25.4 {
		t.Fatalf("expected 25.4, got %f", cs.fromProg(1.0))
	}
	if cs.toProg(25.4) != 1.0 {
		t.Fatalf("expected 1.0, got %f", cs.toProg(25.4))
	}
}

func TestCanon_Rotation(t *testing.T) {
	x, y := rotate(1, 0, 90)
	if math.Abs(x) > 1e-10 || math.Abs(y-1) > 1e-10 {
		t.Fatalf("rotate(1,0,90°) = (%f,%f), want (0,1)", x, y)
	}

	x, y = rotate(1, 0, 0)
	if x != 1 || y != 0 {
		t.Fatalf("rotate(1,0,0°) should be identity")
	}
}

func TestCanon_StraightFeedEnqueues(t *testing.T) {
	task, mot, _ := newCanonTestTask()
	task.StartSequencer()
	defer task.StopSequencer()

	c := task.canon
	c.state.traverseRate = 1000

	// Feed a straight line in MM mode
	c.SetFeedRate(600) // 600 mm/min = 10 mm/sec
	c.StraightFeed(1, 10, 20, 30, 0, 0, 0, 0, 0, 0)

	task.DrainQueue()

	// Should have SetMotionParams + SetLine (via LinearMoveCmd)
	found := false
	for _, call := range mot.calls {
		if call == "SetLine" {
			found = true
		}
	}
	if !found {
		t.Fatalf("expected SetLine in calls, got %v", mot.calls)
	}
}

func TestCanon_StraightTraverseEnqueues(t *testing.T) {
	task, mot, _ := newCanonTestTask()
	task.StartSequencer()
	defer task.StopSequencer()

	c := task.canon
	c.state.traverseRate = 5000

	c.StraightTraverse(1, 100, 0, 0, 0, 0, 0, 0, 0, 0)
	task.DrainQueue()

	found := false
	for _, call := range mot.calls {
		if call == "SetLine" {
			found = true
		}
	}
	if !found {
		t.Fatalf("expected SetLine, got %v", mot.calls)
	}
}

func TestCanon_ArcFeedEnqueues(t *testing.T) {
	task, mot, _ := newCanonTestTask()
	task.StartSequencer()
	defer task.StopSequencer()

	c := task.canon
	c.state.traverseRate = 1000
	c.SetFeedRate(600)

	// Quarter circle in XY plane: start (10,0), end (0,10), center at origin offset (relative)
	c.state.endPoint = Pose{X: 10, Y: 0, Z: 0}
	c.ArcFeed(1, 0, 10, -10, 0, 1, 0, 0, 0, 0, 0, 0, 0)

	task.DrainQueue()

	foundCircle := false
	for _, call := range mot.calls {
		if call == "SetCircle" {
			foundCircle = true
		}
	}
	if !foundCircle {
		t.Fatalf("expected SetCircle, got %v", mot.calls)
	}
}

func (m *testMotion) SetCircle(pos Pose, center, normal Cartesian, turn int32, vel, iniMaxvel, acc float64, motionType int32, id int32, feedUpm float64) error {
	m.calls = append(m.calls, "SetCircle")
	m.callCount++
	return nil
}

func TestCanon_DwellEnqueues(t *testing.T) {
	task, _, _ := newCanonTestTask()
	task.StartSequencer()
	defer task.StopSequencer()

	c := task.canon
	c.Dwell(0.001)
	task.DrainQueue()
	// Just verify no panic/error — dwell is self-contained
}

func TestCanon_SpindleOnOff(t *testing.T) {
	task, mot, _ := newCanonTestTask()
	task.StartSequencer()
	defer task.StopSequencer()

	c := task.canon
	c.SetSpindleSpeed(0, 1000)    // S1000 while stopped
	c.StartSpindleClockwise(0, 1) // M3
	c.StopSpindleTurning(0)       // M5
	task.DrainQueue()

	// C++ parity: SET_SPINDLE_SPEED always re-emits a SPINDLE_ON command
	// (emccanon.cc:1918), with speed = dir*rpm — which is 0 while the spindle is
	// stopped (dir=0). So S1000 issued *before* M3 produces an extra SpindleOn
	// (speed=0), not a no-op. Matches the C milltask oracle capture (spindle.log):
	//   SPINDLE_ON  s=0 speed=0    wait=0   <- SetSpindleSpeed (dir=0)
	//   SPINDLE_ON  s=0 speed=1000 wait=1   <- StartSpindleClockwise
	//   SPINDLE_OFF s=0                      <- StopSpindleTurning
	want := []string{"SpindleOn", "SpindleOn", "SpindleOff"}
	if len(mot.calls) != len(want) {
		t.Fatalf("spindle call sequence = %v, want %v", mot.calls, want)
	}
	for i, w := range want {
		if mot.calls[i] != w {
			t.Fatalf("spindle call[%d] = %s, want %s (full: %v)", i, mot.calls[i], w, mot.calls)
		}
	}
}

func TestCanon_CoolantEnqueues(t *testing.T) {
	mot := &testMotion{failAt: -1}
	tio := &trackingIO{}
	stat := &testStatus{}
	stat.inPosition.Store(true)
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	task := NewTask(mot, tio, stat, logger)
	task.StartSequencer()
	defer task.StopSequencer()

	c := task.canon
	c.FloodOn()
	c.MistOn()
	c.FloodOff()
	c.MistOff()
	task.DrainQueue()

	expected := []string{"FloodOn", "MistOn", "FloodOff", "MistOff"}
	if len(tio.calls) != len(expected) {
		t.Fatalf("expected %v, got %v", expected, tio.calls)
	}
	for i, e := range expected {
		if tio.calls[i] != e {
			t.Fatalf("call[%d]: expected %s, got %s", i, e, tio.calls[i])
		}
	}
}

// trackingIO tracks all calls in order.
type trackingIO struct {
	mockIO
	calls []string
}

func (m *trackingIO) CoolantFloodOn() error  { m.calls = append(m.calls, "FloodOn"); return nil }
func (m *trackingIO) CoolantFloodOff() error { m.calls = append(m.calls, "FloodOff"); return nil }
func (m *trackingIO) CoolantMistOn() error   { m.calls = append(m.calls, "MistOn"); return nil }
func (m *trackingIO) CoolantMistOff() error  { m.calls = append(m.calls, "MistOff"); return nil }
func (m *trackingIO) ToolPrepare(t int32) error {
	m.calls = append(m.calls, "ToolPrepare")
	return nil
}
func (m *trackingIO) ToolLoad() error { m.calls = append(m.calls, "ToolChange"); return nil }

func TestCanon_Getters(t *testing.T) {
	task, _, _ := newCanonTestTask()
	task.linearUnits = 1.0 // mm machine
	c := task.canon

	// Set state
	c.state.lengthUnits = CanonUnitsInches
	c.SetFeedRate(60) // 60 in/min

	// GetExternalFeedRate should return in program units/min
	rate, _ := c.GetExternalFeedRate()
	if math.Abs(rate-60) > 0.01 {
		t.Fatalf("expected 60, got %f", rate)
	}

	// GetExternalLengthUnits returns machine native units (always 1.0 for mm)
	lu, _ := c.GetExternalLengthUnits()
	expected := 1.0
	if math.Abs(lu-expected) > 1e-6 {
		t.Fatalf("expected %f, got %f", expected, lu)
	}

	// Position getter
	c.state.endPoint = Pose{X: 25.4, Y: 50.8, Z: 0}
	posX, _ := c.GetExternalPositionX()
	if math.Abs(posX-1.0) > 1e-6 {
		t.Fatalf("expected 1.0 inch, got %f", posX)
	}
	posY, _ := c.GetExternalPositionY()
	if math.Abs(posY-2.0) > 1e-6 {
		t.Fatalf("expected 2.0 inches, got %f", posY)
	}
}

func TestCanon_ToolChange(t *testing.T) {
	mot := &testMotion{failAt: -1}
	tio := &trackingIO{}
	stat := &testStatus{}
	stat.inPosition.Store(true)
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	task := NewTask(mot, tio, stat, logger)
	task.StartSequencer()
	defer task.StopSequencer()

	c := task.canon
	c.SelectTool(5)
	c.StartChange()
	c.ChangeTool(5)
	task.DrainQueue()

	// Should see ToolPrepare then ToolChange
	foundPrepare := false
	foundChange := false
	for _, call := range tio.calls {
		if call == "ToolPrepare" {
			foundPrepare = true
		}
		if call == "ToolChange" {
			foundChange = true
		}
	}
	if !foundPrepare {
		t.Fatalf("expected ToolPrepare, got %v", tio.calls)
	}
	if !foundChange {
		t.Fatalf("expected ToolChange, got %v", tio.calls)
	}
}
