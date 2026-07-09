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

// checkPos asserts a recorded move's kind and absolute (mm) position.
func checkPos(t *testing.T, got recMove, kind string, x, y, z float64) {
	t.Helper()
	if got.kind != kind {
		t.Errorf("kind=%s, want %s", got.kind, kind)
	}
	if math.Abs(got.pos.X-x) > blendEps || math.Abs(got.pos.Y-y) > blendEps || math.Abs(got.pos.Z-z) > blendEps {
		t.Errorf("pos=[%.4f,%.4f,%.4f], want [%.4f,%.4f,%.4f]", got.pos.X, got.pos.Y, got.pos.Z, x, y, z)
	}
}

// TestCanon_StraightFeed_UnitsAndOffsets exercises the toAbsolute transform that
// no other test covers: program->mm unit conversion, then the
// G92 -> rotate(XY) -> G5x -> tool offset chain, in the exact order C++
// rotate_and_offset_pos uses (emccanon.cc:242) with FROM_PROG_LEN applied first.
// Offsets are stored internally in mm. A wrong scale, dropped offset, or wrong
// offset/rotation ordering would change the asserted position.
func TestCanon_StraightFeed_UnitsAndOffsets(t *testing.T) {
	task, mot := newBlendCanonTask(t)
	c := task.canon
	s := c.state
	s.lengthUnits = CanonUnitsInches // program units = inches (x25.4)
	s.g92Offset = Pose{X: 1}         // mm
	s.g5xOffset = Pose{X: 10, Y: 5}  // mm
	s.toolOffset = Pose{Z: 2}        // mm
	c.SetFeedRate(60)

	// Move 0 (no rotation): prog X2 Y3 Z1 inches.
	//   fromProg: (50.8, 76.2, 25.4); +G92 X: (51.8,76.2,25.4);
	//   +G5x (10,5): (61.8,81.2,25.4); +tool Z2: (61.8,81.2,27.4)
	c.StraightFeed(1, 2, 3, 1, 0, 0, 0, 0, 0, 0)

	// Move 1 with a 90-degree XY rotation, which must apply AFTER G92 but BEFORE
	// G5x (locks the offset ordering): prog X2 Y0 ->
	//   fromProg (50.8,0); +G92 (51.8,0); rotate90 (0,51.8); +G5x (10,56.8);
	//   Z: 0 + tool 2 = 2  => (10, 56.8, 2)
	s.xyRotation = 90
	c.StraightFeed(2, 2, 0, 0, 0, 0, 0, 0, 0, 0)

	m := collect(t, task, mot)
	if len(m) != 2 {
		t.Fatalf("expected 2 moves, got %d: %+v", len(m), m)
	}
	checkPos(t, m[0], "line", 61.8, 81.2, 27.4)
	if m[0].motionType != 2 {
		t.Errorf("m[0].motionType=%d, want 2 (feed)", m[0].motionType)
	}
	checkPos(t, m[1], "line", 10, 56.8, 2)
}

func TestCanon_StraightTraverse_Args(t *testing.T) {
	task, mot := newBlendCanonTask(t) // MM units
	c := task.canon
	c.state.g5xOffset = Pose{X: 10} // mm

	c.StraightTraverse(1, 100, 0, 0, 0, 0, 0, 0, 0, 0) // prog X100 mm

	m := collect(t, task, mot)
	if len(m) != 1 {
		t.Fatalf("expected 1 move, got %d: %+v", len(m), m)
	}
	checkPos(t, m[0], "line", 110, 0, 0) // 100 + G5x 10
	if m[0].motionType != 1 {
		t.Errorf("motionType=%d, want 1 (traverse)", m[0].motionType) // EMC_MOTION_TYPE_TRAVERSE
	}
	if m[0].feed != 0 {
		t.Errorf("traverse FeedMmPerMin=%v, want 0 (no programmed feed)", m[0].feed)
	}
}

// TestCanon_ArcFeed_Args pins the XY-plane arc geometry: end position, absolute
// center (interp passes first/second axis as ABSOLUTE center coords), plane
// normal, and the turn adjustment (turn = rotation-1 for CCW). The old test used
// geometrically-inconsistent data (center -10,0 for a 10,0->0,10 arc) and checked
// only the command type.
func TestCanon_ArcFeed_Args(t *testing.T) {
	task, mot := newBlendCanonTask(t) // MM units, XY plane default
	c := task.canon
	c.state.endPoint = Pose{X: 10} // start at (10,0,0)
	c.SetFeedRate(600)

	// CCW (G3, rotation=+1) quarter circle: start (10,0), center (0,0), end (0,10).
	// ArcFeed(lineno, firstEnd=X, secondEnd=Y, firstAxis=cX, secondAxis=cY,
	//         rotation, axisEndPoint=Z, a,b,c,u,v,w)
	c.ArcFeed(1, 0, 10, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0)

	m := collect(t, task, mot)
	if len(m) != 1 {
		t.Fatalf("expected 1 move, got %d: %+v", len(m), m)
	}
	g := m[0]
	checkPos(t, g, "circle", 0, 10, 0)
	if math.Abs(g.center.X) > blendEps || math.Abs(g.center.Y) > blendEps || math.Abs(g.center.Z) > blendEps {
		t.Errorf("center=%+v, want (0,0,0)", g.center)
	}
	if g.normal != (Cartesian{X: 0, Y: 0, Z: 1}) {
		t.Errorf("normal=%+v, want (0,0,1) for XY plane", g.normal)
	}
	if g.turn != 0 {
		t.Errorf("turn=%d, want 0 (rotation +1 -> rotation-1)", g.turn)
	}
	if g.motionType != 3 {
		t.Errorf("motionType=%d, want 3 (arc)", g.motionType)
	}
}

func (m *testMotion) SetCircle(pos Pose, center, normal Cartesian, turn int32, vel, iniMaxvel, acc float64, motionType int32, id int32, feedUpm float64) error {
	m.calls = append(m.calls, "SetCircle")
	m.callCount++
	return nil
}

// TestCanon_Dwell_Args asserts the dwell duration reaches the queued DwellCmd.
// Dwell is sequencer-internal (never reaches motion), so we inspect the queue
// directly instead of running the sequencer.
func TestCanon_Dwell_Args(t *testing.T) {
	task, _, _ := newCanonTestTask()
	// Provide the queue channels without a running sequencer so nothing consumes
	// the enqueued commands before we inspect them.
	task.mu.Lock()
	task.interpQueue = make(chan QueuedCmd, 8)
	task.seqAbort = make(chan struct{})
	task.mu.Unlock()

	task.canon.Dwell(0.25)

	close(task.interpQueue)
	var dwell *DwellCmd
	for cmd := range task.interpQueue {
		if d, ok := cmd.(*DwellCmd); ok {
			dwell = d
		}
	}
	if dwell == nil {
		t.Fatal("no DwellCmd enqueued by Dwell(0.25)")
	}
	if dwell.Seconds != 0.25 {
		t.Errorf("DwellCmd.Seconds=%v, want 0.25", dwell.Seconds)
	}
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
	calls        []string
	preparedTool int32 // last tool number passed to ToolPrepare
}

func (m *trackingIO) CoolantFloodOn() error  { m.calls = append(m.calls, "FloodOn"); return nil }
func (m *trackingIO) CoolantFloodOff() error { m.calls = append(m.calls, "FloodOff"); return nil }
func (m *trackingIO) CoolantMistOn() error   { m.calls = append(m.calls, "MistOn"); return nil }
func (m *trackingIO) CoolantMistOff() error  { m.calls = append(m.calls, "MistOff"); return nil }
func (m *trackingIO) ToolPrepare(t int32) error {
	m.calls = append(m.calls, "ToolPrepare")
	m.preparedTool = t
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
	// The selected tool number (T5) must propagate to ToolPrepare, not just the
	// call happen — a tool-index bug would otherwise be invisible.
	if tio.preparedTool != 5 {
		t.Errorf("ToolPrepare tool = %d, want 5", tio.preparedTool)
	}
}
