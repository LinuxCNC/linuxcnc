// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"fmt"
	"math"
	"time"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/canon"
)

// Canon unit systems.
const (
	CanonUnitsInches = 1
	CanonUnitsMM     = 2
	CanonUnitsCM     = 3
)

// Canon plane selection.
const (
	CanonPlaneXY = 1
	CanonPlaneYZ = 2
	CanonPlaneXZ = 3
	CanonPlaneUV = 4
	CanonPlaneVW = 5
	CanonPlaneWU = 6
)

// Canon motion modes.
const (
	CanonExactStop  = 1
	CanonExactPath  = 2
	CanonContinuous = 3
)

// TP termination conditions (must match tc_types.h).
const (
	tpTermCondStop      = 0 // TC_TERM_COND_STOP
	tpTermCondExact     = 1 // TC_TERM_COND_EXACT
	tpTermCondParabolic = 2 // TC_TERM_COND_PARABOLIC (blend)
)

// canonModeToTPTermCond maps interpreter canon motion modes to TP term conditions.
func canonModeToTPTermCond(mode int32) int32 {
	switch mode {
	case CanonContinuous:
		return tpTermCondParabolic
	case CanonExactPath:
		return tpTermCondExact
	default:
		return tpTermCondStop
	}
}

// Canon feed reference.
const (
	CanonWorkpiece = 1
	CanonXYZ       = 2
)

// CanonState holds the interpreter-side state that tracks coordinate
// systems, units, feed rates, and other modal state needed to convert
// G-code interpreter callbacks into motctl commands.
type CanonState struct {
	// Coordinate offsets (stored in mm, absolute)
	g5xOffset  Pose
	g5xIndex   int32 // active G5x system (1=G54, 2=G55, ...)
	g92Offset  Pose
	xyRotation float64

	// Current endpoint (absolute, mm)
	endPoint Pose

	// Units and plane
	lengthUnits int32 // CanonUnitsInches/MM/CM
	activePlane int32

	// Tool offset (mm)
	toolOffset Pose

	// Motion parameters
	motionMode      int32   // CanonExact/Continuous/ExactPath
	motionTolerance float64 // blending tolerance (mm)
	naivecamTol     float64 // naive CAM tolerance (mm)
	feedMode        int32   // 0=normal, 1=inverse-time, 2=units-per-rev

	// Feed rates (internal, mm/sec or deg/sec)
	linearFeedRate  float64
	angularFeedRate float64
	traverseRate    float64

	// Spindle
	spindleNum   int32 // current spindle for synch motion
	spindleSpeed [8]float64
	spindleMode  float64

	// Flags
	feedOverrideEnabled  bool
	speedOverrideEnabled [8]bool
	feedHoldEnabled      bool
	adaptiveFeedEnabled  bool
	optionalProgramStop  bool
	blockDelete          bool
	floodOn              bool
	mistOn               bool

	// Rotary unlock: joint number to unlock for traverse (-1 = none)
	rotaryUnlockForTraverse int32

	// Motion line ID counter
	lineNo int32
}

// NewCanonState returns a CanonState with sensible defaults.
func NewCanonState() *CanonState {
	cs := &CanonState{
		lengthUnits:             CanonUnitsMM,
		activePlane:             CanonPlaneXY,
		g5xIndex:                1, // G54 default (same as C canon)
		motionMode:              CanonContinuous,
		motionTolerance:         0.0254, // 0.001 inch default (same as C canon)
		linearFeedRate:          0,      // set by SET_FEED_RATE or synch
		traverseRate:            0,      // set from INI
		feedOverrideEnabled:     true,
		feedHoldEnabled:         true,
		rotaryUnlockForTraverse: -1,
	}
	for i := range cs.speedOverrideEnabled {
		cs.speedOverrideEnabled[i] = true
	}
	return cs
}

// unitScale returns the conversion factor from program units to mm.
func (cs *CanonState) unitScale() float64 {
	switch cs.lengthUnits {
	case CanonUnitsInches:
		return 25.4
	case CanonUnitsCM:
		return 10.0
	default:
		return 1.0
	}
}

// fromProg converts a program-unit length to internal mm.
func (cs *CanonState) fromProg(v float64) float64 {
	return v * cs.unitScale()
}

// toProg converts internal mm to program units.
func (cs *CanonState) toProg(v float64) float64 {
	return v / cs.unitScale()
}

// rotate applies the XY rotation to x,y coordinates.
func rotate(x, y, angle float64) (float64, float64) {
	if angle == 0 {
		return x, y
	}
	sin, cos := math.Sincos(angle * math.Pi / 180.0)
	return x*cos - y*sin, x*sin + y*cos
}

// toAbsolute converts program coordinates to absolute machine coordinates (mm),
// applying offsets in the same order as the C canon: G92 → rotate → G5x → tool.
func (cs *CanonState) toAbsolute(x, y, z, a, b, c, u, v, w float64) Pose {
	// Convert from program units to mm
	xm := cs.fromProg(x)
	ym := cs.fromProg(y)
	zm := cs.fromProg(z)
	um := cs.fromProg(u)
	vm := cs.fromProg(v)
	wm := cs.fromProg(w)

	// Step 1: add G92 offset
	xm += cs.g92Offset.X
	ym += cs.g92Offset.Y
	zm += cs.g92Offset.Z
	um += cs.g92Offset.U
	vm += cs.g92Offset.V
	wm += cs.g92Offset.W

	// Step 2: apply XY rotation
	xm, ym = rotate(xm, ym, cs.xyRotation)

	// Step 3: add G5x offset
	xm += cs.g5xOffset.X
	ym += cs.g5xOffset.Y
	zm += cs.g5xOffset.Z
	um += cs.g5xOffset.U
	vm += cs.g5xOffset.V
	wm += cs.g5xOffset.W

	// Step 4: add tool offset
	xm += cs.toolOffset.X
	ym += cs.toolOffset.Y
	zm += cs.toolOffset.Z
	um += cs.toolOffset.U
	vm += cs.toolOffset.V
	wm += cs.toolOffset.W

	return Pose{
		X: xm, Y: ym, Z: zm,
		A: a + cs.g92Offset.A + cs.g5xOffset.A + cs.toolOffset.A,
		B: b + cs.g92Offset.B + cs.g5xOffset.B + cs.toolOffset.B,
		C: c + cs.g92Offset.C + cs.g5xOffset.C + cs.toolOffset.C,
		U: um, V: vm, W: wm,
	}
}

// toAbsoluteXYZ converts XYZ program coordinates to absolute machine
// coordinates, applying the same offset chain as toAbsolute but only for XYZ.
// Used for arc center computation.
func (cs *CanonState) toAbsoluteXYZ(x, y, z float64) Cartesian {
	xm := cs.fromProg(x) + cs.g92Offset.X
	ym := cs.fromProg(y) + cs.g92Offset.Y
	zm := cs.fromProg(z) + cs.g92Offset.Z
	xm, ym = rotate(xm, ym, cs.xyRotation)
	xm += cs.g5xOffset.X + cs.toolOffset.X
	ym += cs.g5xOffset.Y + cs.toolOffset.Y
	zm += cs.g5xOffset.Z + cs.toolOffset.Z
	return Cartesian{X: xm, Y: ym, Z: zm}
}

// fromAbsolute converts absolute machine coordinates back to program coordinates,
// inverting toAbsolute: subtract tool → subtract G5x → unrotate → subtract G92 → toProg.
func (cs *CanonState) fromAbsolute(p Pose) Pose {
	// Step 1: subtract tool offset
	x := p.X - cs.toolOffset.X
	y := p.Y - cs.toolOffset.Y
	z := p.Z - cs.toolOffset.Z
	u := p.U - cs.toolOffset.U
	v := p.V - cs.toolOffset.V
	w := p.W - cs.toolOffset.W
	a := p.A - cs.toolOffset.A - cs.g5xOffset.A - cs.g92Offset.A
	b := p.B - cs.toolOffset.B - cs.g5xOffset.B - cs.g92Offset.B
	c := p.C - cs.toolOffset.C - cs.g5xOffset.C - cs.g92Offset.C

	// Step 2: subtract G5x offset
	x -= cs.g5xOffset.X
	y -= cs.g5xOffset.Y
	z -= cs.g5xOffset.Z
	u -= cs.g5xOffset.U
	v -= cs.g5xOffset.V
	w -= cs.g5xOffset.W

	// Step 3: unrotate (negate the angle)
	x, y = rotate(x, y, -cs.xyRotation)

	// Step 4: subtract G92 offset
	x -= cs.g92Offset.X
	y -= cs.g92Offset.Y
	z -= cs.g92Offset.Z
	u -= cs.g92Offset.U
	v -= cs.g92Offset.V
	w -= cs.g92Offset.W

	// Step 5: convert to program units
	return Pose{
		X: cs.toProg(x), Y: cs.toProg(y), Z: cs.toProg(z),
		A: a, B: b, C: c,
		U: cs.toProg(u), V: cs.toProg(v), W: cs.toProg(w),
	}
}

// Canon is the set of canon callback implementations that push QueuedCmds
// to the Task's sequencer queue. It holds the CanonState and a reference
// to the owning Task.
type Canon struct {
	state             *CanonState
	task              *Task
	parameterFileName string
	discard           bool // when true, enqueue is a no-op (used during seek)
	nextSerial        int32
}

// Compile-time check that Canon implements the generated CanonCallbacks interface.
var _ canon.CanonCallbacks = (*Canon)(nil)

// NewCanon creates a Canon instance tied to a Task.
func NewCanon(t *Task) *Canon {
	cs := NewCanonState()
	return &Canon{
		state: cs,
		task:  t,
	}
}

// setDiscard enables/disables discard mode for run-from-line seeking.
// When discard is true, enqueue drops commands instead of queueing them.
func (c *Canon) setDiscard(d bool) {
	c.discard = d
}

// allocSerial returns the next segment serial id and registers the G-code
// location in the task's side table. Serials start at 1; 0 is reserved as
// the "nothing executing" sentinel used by UI code.
func (c *Canon) allocSerial(lineno int32) int32 {
	if c.nextSerial == 0 {
		c.nextSerial = 1
	}
	id := c.nextSerial
	c.nextSerial++
	file := ""
	if c.task.interp != nil {
		file = c.task.interp.FileName()
	}
	c.task.registerMotion(id, file, lineno)
	return id
}

// --- State-setting callbacks (modify canon state, no queued commands) ---

func (c *Canon) InitCanon() {
	// Preserve coordinate offsets across reset — these are persistent
	// interpreter-managed state (set during interp.init from the var file
	// and by explicit G10/G54-G59 commands). The C canon's INIT_CANON
	// zeros them too, but in the C milltask interp.init() is always
	// called at startup to restore them. For the Go milltask, opening a
	// file triggers reset→InitCanon without a subsequent init(), so we
	// must preserve them here to avoid losing work offsets.
	saved := struct {
		g5xOffset  Pose
		g5xIndex   int32
		g92Offset  Pose
		xyRotation float64
		toolOffset Pose
	}{
		g5xOffset:  c.state.g5xOffset,
		g5xIndex:   c.state.g5xIndex,
		g92Offset:  c.state.g92Offset,
		xyRotation: c.state.xyRotation,
		toolOffset: c.state.toolOffset,
	}
	*c.state = *NewCanonState()
	c.state.g5xOffset = saved.g5xOffset
	c.state.g5xIndex = saved.g5xIndex
	c.state.g92Offset = saved.g92Offset
	c.state.xyRotation = saved.xyRotation
	c.state.toolOffset = saved.toolOffset
}

func (c *Canon) SetG5xOffset(origin int32, x, y, z, a, b, _c, u, v, w float64) {
	s := c.state
	s.g5xIndex = origin
	s.g5xOffset = Pose{
		X: s.fromProg(x), Y: s.fromProg(y), Z: s.fromProg(z),
		A: a, B: b, C: _c,
		U: s.fromProg(u), V: s.fromProg(v), W: s.fromProg(w),
	}
	c.task.previewSeq++
}

func (c *Canon) SetG92Offset(x, y, z, a, b, _c, u, v, w float64) {
	s := c.state
	s.g92Offset = Pose{
		X: s.fromProg(x), Y: s.fromProg(y), Z: s.fromProg(z),
		A: a, B: b, C: _c,
		U: s.fromProg(u), V: s.fromProg(v), W: s.fromProg(w),
	}
	c.task.previewSeq++
}

func (c *Canon) SetXyRotation(t float64) {
	c.state.xyRotation = t
	c.task.previewSeq++
}

func (c *Canon) UseLengthUnits(units int32) {
	c.state.lengthUnits = units
}

func (c *Canon) SelectPlane(plane int32) {
	c.state.activePlane = plane
}

func (c *Canon) SetTraverseRate(rate float64) {
	c.state.traverseRate = rate
}

func (c *Canon) SetFeedRate(rate float64) {
	s := c.state
	s.linearFeedRate = s.fromProg(rate) / 60.0 // input is units/min → mm/sec
}

func (c *Canon) SetFeedReference(reference int32) {
	// Stored but rarely used in modern configs
}

func (c *Canon) SetFeedMode(spindle, mode int32) {
	c.state.feedMode = mode
	c.state.spindleNum = spindle
}

func (c *Canon) SetMotionControlMode(mode int32, tolerance float64) {
	s := c.state
	s.motionMode = mode
	s.motionTolerance = s.fromProg(tolerance)
}

func (c *Canon) SetNaivecamTolerance(tolerance float64) {
	c.state.naivecamTol = c.state.fromProg(tolerance)
}

func (c *Canon) SetCutterRadiusCompensation(radius float64) {
	// Cutter comp is handled by the interpreter, not the canon layer
}

func (c *Canon) StartCutterRadiusCompensation(direction int32) {}
func (c *Canon) StopCutterRadiusCompensation()                 {}

func (c *Canon) UpdateEndPoint(x, y, z, a, b, _c, u, v, w float64) {
	// C canon only does FROM_PROG_LEN here (no offsets applied).
	// This is called by the interpreter to sync position without offset transform.
	s := c.state
	s.endPoint = Pose{
		X: s.fromProg(x), Y: s.fromProg(y), Z: s.fromProg(z),
		A: a, B: b, C: _c,
		U: s.fromProg(u), V: s.fromProg(v), W: s.fromProg(w),
	}
}

// UpdateTag is called by the interpreter to pass interpreter state alongside
// motion segments. StateTag has been removed from the GMI API; this is a no-op.
func (c *Canon) UpdateTag(_ uint64) {}

func (c *Canon) UseToolLengthOffset(x, y, z, a, b, _c, u, v, w float64) {
	s := c.state
	s.toolOffset = Pose{
		X: x, Y: y, Z: z,
		A: a, B: b, C: _c,
		U: u, V: v, W: w,
	}
}

// --- Action callbacks (push QueuedCmd to sequencer) ---

func (c *Canon) StraightTraverse(lineno int32, x, y, z, a, b, _c, u, v, w float64) {
	s := c.state
	pos := s.toAbsolute(x, y, z, a, b, _c, u, v, w)
	s.endPoint = pos
	s.lineNo = lineno

	trav := c.task.maxVelocity
	cmd := &LinearMoveCmd{
		Pos:        pos,
		Vel:        trav,
		IniMaxVel:  trav,
		Acc:        c.task.maxAcceleration,
		MotionType: 1, // EMC_MOTION_TYPE_TRAVERSE
		ID:         c.allocSerial(lineno),
		FeedUpm:    0, // traverse: no programmed feed
		IndexerJ:   s.rotaryUnlockForTraverse,
	}
	c.enqueue(cmd)
}

func (c *Canon) StraightFeed(lineno int32, x, y, z, a, b, _c, u, v, w float64) {
	s := c.state
	pos := s.toAbsolute(x, y, z, a, b, _c, u, v, w)
	s.endPoint = pos
	s.lineNo = lineno

	// Set motion parameters before the move (tp uses termCond at add time)
	c.enqueueMotionParams()

	cmd := &LinearMoveCmd{
		Pos:        pos,
		Vel:        s.linearFeedRate,
		IniMaxVel:  c.task.maxVelocity,
		Acc:        c.task.maxAcceleration,
		MotionType: 2, // EMC_MOTION_TYPE_FEED
		ID:         c.allocSerial(lineno),
		FeedUpm:    s.linearFeedRate * 60,
		IndexerJ:   -1,
	}
	c.enqueue(cmd)
}

func (c *Canon) ArcFeed(lineno int32, firstEnd, secondEnd, firstAxis, secondAxis float64,
	rotation int32, axisEndPoint, a, b, _c, u, v, w float64) {

	s := c.state
	s.lineNo = lineno

	// Convert arc endpoints and center based on active plane.
	// The interpreter passes first_axis/second_axis as ABSOLUTE center
	// coordinates in program units (same coordinate frame as endpoints).
	var pos Pose
	var center Cartesian
	var normal Cartesian

	switch s.activePlane {
	case CanonPlaneXY:
		pos = s.toAbsolute(firstEnd, secondEnd, axisEndPoint, a, b, _c, u, v, w)
		center = s.toAbsoluteXYZ(firstAxis, secondAxis, axisEndPoint)
		normal = Cartesian{X: 0, Y: 0, Z: 1}
	case CanonPlaneXZ:
		pos = s.toAbsolute(secondEnd, axisEndPoint, firstEnd, a, b, _c, u, v, w)
		center = s.toAbsoluteXYZ(secondAxis, axisEndPoint, firstAxis)
		normal = Cartesian{X: 0, Y: 1, Z: 0}
	case CanonPlaneYZ:
		pos = s.toAbsolute(axisEndPoint, firstEnd, secondEnd, a, b, _c, u, v, w)
		center = s.toAbsoluteXYZ(axisEndPoint, firstAxis, secondAxis)
		normal = Cartesian{X: 1, Y: 0, Z: 0}
	default:
		pos = s.toAbsolute(firstEnd, secondEnd, axisEndPoint, a, b, _c, u, v, w)
		center = s.toAbsoluteXYZ(firstAxis, secondAxis, axisEndPoint)
		normal = Cartesian{X: 0, Y: 0, Z: 1}
	}

	s.endPoint = pos

	// C canon: turn = rotation-1 for positive, rotation for negative.
	turn := rotation
	if rotation > 0 {
		turn = rotation - 1
	}

	cmd := &CircularMoveCmd{
		Pos:        pos,
		Center:     center,
		Normal:     normal,
		Turn:       turn,
		Vel:        s.linearFeedRate,
		IniMaxVel:  c.task.maxVelocity,
		Acc:        c.task.maxAcceleration,
		MotionType: 3, // EMC_MOTION_TYPE_ARC
		ID:         c.allocSerial(lineno),
		FeedUpm:    s.linearFeedRate * 60,
	}
	c.enqueue(cmd)
	c.enqueueMotionParams()
}

func (c *Canon) RigidTap(lineno int32, x, y, z, scale float64) {
	s := c.state
	pos := s.toAbsolute(x, y, z, 0, 0, 0, 0, 0, 0)
	s.lineNo = lineno

	cmd := &RigidTapCmd{
		Pos:     pos,
		Vel:     s.linearFeedRate,
		Acc:     c.task.maxAcceleration,
		Scale:   scale,
		ID:      c.allocSerial(lineno),
		FeedUpm: s.linearFeedRate * 60,
	}
	c.enqueue(cmd)
}

func (c *Canon) StraightProbe(lineno int32, x, y, z, a, b, _c, u, v, w float64, probeType uint8) {
	s := c.state
	pos := s.toAbsolute(x, y, z, a, b, _c, u, v, w)
	s.lineNo = lineno

	cmd := &ProbeCmd{
		Pos:        pos,
		Vel:        s.linearFeedRate,
		IniMaxVel:  c.task.maxVelocity,
		Acc:        c.task.maxAcceleration,
		MotionType: 4, // EMC_MOTION_TYPE_PROBING
		ProbeType:  probeType,
		ID:         c.allocSerial(lineno),
		FeedUpm:    s.linearFeedRate * 60,
	}
	c.enqueue(cmd)
}

func (c *Canon) Dwell(seconds float64) {
	c.enqueue(&DwellCmd{Seconds: seconds})
}

func (c *Canon) Stop() {
	c.enqueue(waitForMotionSingleton)
}

func (c *Canon) Finish() {
	c.enqueue(waitForMotionSingleton)
}

func (c *Canon) StartSpindleClockwise(spindle, waitForAtspeed int32) {
	s := c.state
	c.enqueue(&SpindleOnCmd{
		Spindle:  spindle,
		Speed:    s.spindleSpeed[spindle],
		WaitFlag: waitForAtspeed,
	})
}

func (c *Canon) StartSpindleCounterclockwise(spindle, waitForAtspeed int32) {
	s := c.state
	c.enqueue(&SpindleOnCmd{
		Spindle:  spindle,
		Speed:    -s.spindleSpeed[spindle],
		WaitFlag: waitForAtspeed,
	})
}

func (c *Canon) SetSpindleSpeed(spindle int32, rpm float64) {
	c.state.spindleSpeed[spindle] = rpm
}

func (c *Canon) StopSpindleTurning(spindle int32) {
	c.enqueue(waitForMotionSingleton)
	c.enqueue(&SpindleOffCmd{Spindle: spindle})
}

func (c *Canon) OrientSpindle(spindle int32, orientation float64, mode int32) {
	c.enqueue(&SpindleOrientCmd{Spindle: spindle, Orientation: orientation, Mode: mode})
}

func (c *Canon) WaitSpindleOrientComplete(spindle int32, timeout float64) {
	c.enqueue(&WaitSpindleOrientedCmd{Spindle: spindle, Timeout: timeout})
}

func (c *Canon) SelectTool(tool int32) {
	// T word — record selected tool for subsequent M6
	c.enqueue(&ToolPrepareCmd{Tool: tool})
}

func (c *Canon) StartChange() {
	// M6 start — wait for motion to complete first
	c.enqueue(waitForMotionSingleton)
}

func (c *Canon) ChangeTool(slot int32) {
	c.enqueue(&ToolChangeCmd{})
}

func (c *Canon) FloodOn()  { c.state.floodOn = true; c.enqueue(&FloodOnCmd{}) }
func (c *Canon) FloodOff() { c.state.floodOn = false; c.enqueue(&FloodOffCmd{}) }
func (c *Canon) MistOn()   { c.state.mistOn = true; c.enqueue(&MistOnCmd{}) }
func (c *Canon) MistOff()  { c.state.mistOn = false; c.enqueue(&MistOffCmd{}) }

func (c *Canon) EnableFeedOverride() {
	c.state.feedOverrideEnabled = true
	c.enqueue(&FeedOverrideEnableCmd{Enable: true})
}

func (c *Canon) DisableFeedOverride() {
	c.state.feedOverrideEnabled = false
	c.enqueue(&FeedOverrideEnableCmd{Enable: false})
}

func (c *Canon) EnableSpeedOverride(spindle int32) {
	c.state.speedOverrideEnabled[spindle] = true
}

func (c *Canon) DisableSpeedOverride(spindle int32) {
	c.state.speedOverrideEnabled[spindle] = false
}

func (c *Canon) EnableFeedHold() {
	c.state.feedHoldEnabled = true
	c.enqueue(&FeedHoldEnableCmd{Enable: true})
}

func (c *Canon) DisableFeedHold() {
	c.state.feedHoldEnabled = false
	c.enqueue(&FeedHoldEnableCmd{Enable: false})
}

func (c *Canon) EnableAdaptiveFeed() {
	c.state.adaptiveFeedEnabled = true
	c.enqueue(&AdaptiveFeedEnableCmd{Enable: true})
}

func (c *Canon) DisableAdaptiveFeed() {
	c.state.adaptiveFeedEnabled = false
	c.enqueue(&AdaptiveFeedEnableCmd{Enable: false})
}

func (c *Canon) SetMotionOutputBit(index int32) {
	c.enqueue(&SetDoutSyncCmd{Index: index, StartValue: 1, EndValue: 1})
}

func (c *Canon) ClearMotionOutputBit(index int32) {
	c.enqueue(&SetDoutSyncCmd{Index: index, StartValue: 0, EndValue: 0})
}

func (c *Canon) SetAuxOutputBit(index int32) {
	c.enqueue(&SetDoutCmd{Index: index, Value: 1})
}

func (c *Canon) ClearAuxOutputBit(index int32) {
	c.enqueue(&SetDoutCmd{Index: index, Value: 0})
}

func (c *Canon) SetMotionOutputValue(index int32, value float64) {
	c.enqueue(&SetAoutSyncCmd{Index: index, StartValue: value, EndValue: value})
}

func (c *Canon) SetAuxOutputValue(index int32, value float64) {
	c.enqueue(&SetAoutCmd{Index: index, Value: value})
}

func (c *Canon) ProgramStop() {
	c.enqueue(&ProgramStopCmd{})
}

func (c *Canon) OptionalProgramStop() {
	if c.state.optionalProgramStop {
		c.enqueue(&ProgramStopCmd{})
	}
}

func (c *Canon) ProgramEnd() {
	c.enqueue(waitForMotionSingleton)
}

func (c *Canon) Comment(s string)   {}
func (c *Canon) Message(s string)   { c.task.logger.Info("MSG: " + s) }
func (c *Canon) LogMsg(s string)    {}
func (c *Canon) Logopen(s string)   {}
func (c *Canon) Logappend(s string) {}
func (c *Canon) Logclose()          {}

func (c *Canon) CanonError(msg string) {
	c.task.logger.Error("canon error", "msg", msg)
}

func (c *Canon) SetBlockDelete(enabled int32) {
	c.state.blockDelete = enabled != 0
}

func (c *Canon) GetBlockDelete() (int32, error) {
	if c.state.blockDelete {
		return 1, nil
	}
	return 0, nil
}

func (c *Canon) SetOptionalProgramStop(enabled int32) {
	c.state.optionalProgramStop = enabled != 0
}

func (c *Canon) GetOptionalProgramStop() (int32, error) {
	if c.state.optionalProgramStop {
		return 1, nil
	}
	return 0, nil
}

func (c *Canon) OnReset() {
	// The C canon's ON_RESET only drops queued segments — it does NOT
	// reinitialize state like feed rate, spindle speed, etc.  Those must
	// persist across interpreter resets (which happen on every mode switch
	// to Manual) so that MDI state is preserved between commands.
	// InitCanon() must only be called during true initialization.
}

func (c *Canon) TurnProbeOn()  {}
func (c *Canon) TurnProbeOff() {}

func (c *Canon) StartSpeedFeedSynch(spindle int32, feedPerRev float64, velocityMode int32) {
	c.state.feedMode = 2 // units per rev
	c.state.spindleNum = spindle
	c.enqueue(&SpindleSyncCmd{
		Sync:       c.state.fromProg(feedPerRev),
		MotionType: velocityMode,
	})
}

func (c *Canon) StopSpeedFeedSynch() {
	c.state.feedMode = 0
	c.enqueue(&SpindleSyncCmd{Sync: 0, MotionType: 0})
}

// --- Stub methods (required by canon_callbacks_t, not yet fully implemented) ---

func (c *Canon) ClampAxis(axis int32)   {}
func (c *Canon) UnclampAxis(axis int32) {}
func (c *Canon) PalletShuttle()         {}

func (c *Canon) WaitInput(index, inputType, waitType int32, timeout float64) (int32, error) {
	// M66: Wait for digital/analog input condition.
	// inputType: 1=digital, 2=analog
	// waitType: 0=immediate, 1=rise, 2=fall, 3=high, 4=low
	// timeout: seconds (0 = immediate read)
	// Returns: 0 on success, -1 on error/timeout

	if c.task.status == nil {
		return -1, nil
	}

	// Immediate mode — just return, interp will read via GetExternalDigitalInput/AnalogInput
	if timeout == 0 || waitType == 0 {
		return 0, nil
	}

	deadline := time.Now().Add(time.Duration(timeout * float64(time.Second)))
	ticker := time.NewTicker(pollInterval)
	defer ticker.Stop()

	for {
		select {
		case <-c.task.seqAbort:
			return -1, nil
		case <-ticker.C:
			ms, err := c.task.status.GetStatus()
			if err != nil {
				continue
			}

			var satisfied bool
			if inputType == 1 { // digital
				if index < 0 || index >= 64 {
					return -1, nil
				}
				val := ms.SynchDi[index]
				switch waitType {
				case 1: // rise (high)
					satisfied = val != 0
				case 2: // fall (low)
					satisfied = val == 0
				case 3: // high
					satisfied = val != 0
				case 4: // low
					satisfied = val == 0
				}
			} else { // analog
				if index < 0 || index >= 64 {
					return -1, nil
				}
				val := ms.AnalogInput[index]
				// For analog: rise=above 0, fall=below 0, high=above 0, low=below/equal 0
				switch waitType {
				case 1, 3:
					satisfied = val > 0
				case 2, 4:
					satisfied = val <= 0
				}
			}

			if satisfied {
				return 0, nil
			}
			if time.Now().After(deadline) {
				return -1, nil
			}
		}
	}
}

func (c *Canon) LockRotary(lineno, joint int32) (int32, error) {
	c.state.rotaryUnlockForTraverse = -1
	return 0, nil
}

func (c *Canon) UnlockRotary(lineno, joint int32) (int32, error) {
	// Enqueue a zero-length traverse to interrupt blending and reach final
	// position before unlocking (matches C canon UNLOCK_ROTARY behavior).
	s := c.state
	cmd := &LinearMoveCmd{
		Pos:        s.endPoint,
		Vel:        1,
		IniMaxVel:  1,
		Acc:        1,
		MotionType: 1, // EMC_MOTION_TYPE_TRAVERSE
		ID:         c.allocSerial(lineno),
		FeedUpm:    0,
		IndexerJ:   -1,
	}
	c.enqueue(cmd)
	// The next traverse will carry this joint number for unlock/lock.
	c.state.rotaryUnlockForTraverse = joint
	return 0, nil
}

func (c *Canon) SetParameterFileName(name string) {
	c.parameterFileName = name
}

func (c *Canon) SetSpindleMode(spindle int32, mode float64) {
	c.state.spindleMode = mode
}

func (c *Canon) SetToolTableEntry(pocket, toolno int32, ox, oy, oz, oa, ob, oc, ou, ov, ow, diameter, frontangle, backangle float64, orientation int32) {
	c.enqueue(&SetToolTableEntryCmd{
		Pocket: pocket, Toolno: toolno,
		X: ox, Y: oy, Z: oz, A: oa, B: ob, C: oc, U: ou, V: ov, W: ow,
		Diameter: diameter, Frontangle: frontangle, Backangle: backangle,
		Orientation: orientation,
	})
}

func (c *Canon) ReloadTooldata() {
	c.enqueue(&ReloadTooldataCmd{})
}

func (c *Canon) ChangeToolNumber(number int32) {
	c.enqueue(&ChangeToolNumberCmd{Number: number})
}

func (c *Canon) NurbsFeed(lineno int32, controlPoints []ControlPoint, k uint32) {
	n := uint32(len(controlPoints)) - 1
	umax := float64(n - k + 2)
	div := uint32(len(controlPoints)) * 4
	knotVector := nurbsKnotVector(n, k)

	p0 := nurbsPoint(0, k, controlPoints, knotVector)
	p0t := nurbsTangent(0, k, n, controlPoints, knotVector)

	for i := uint32(1); i <= div; i++ {
		u := umax * float64(i) / float64(div)
		p1 := nurbsPoint(u, k, controlPoints, knotVector)
		p1t := nurbsTangent(u, k, n, controlPoints, knotVector)
		c.nurbsBiarc(lineno, p0.X, p0.Y, p0t.X, p0t.Y, p1.X, p1.Y, p1t.X, p1t.Y)
		p0 = p1
		p0t = p1t
	}
}

// ControlPoint is an alias for the generated canon.ControlPoint type.
type ControlPoint = canon.ControlPoint

// --- NURBS helper functions ---

// nurbsKnotVector creates a uniform knot vector for a B-spline of degree k-1
// with n+1 control points.
func nurbsKnotVector(n, k uint32) []uint32 {
	kv := make([]uint32, 0, n+k+1)
	for i := uint32(0); i <= n+k; i++ {
		if i < k {
			kv = append(kv, 0)
		} else if i <= n {
			kv = append(kv, i-k+1)
		} else {
			kv = append(kv, n-k+2)
		}
	}
	return kv
}

// nurbsBasis evaluates the B-spline basis function N_{i,k}(u) recursively.
func nurbsBasis(i, k uint32, u float64, kv []uint32) float64 {
	if k == 1 {
		if u >= float64(kv[i]) && u <= float64(kv[i+1]) {
			return 1
		}
		return 0
	}
	denom1 := float64(kv[i+k-1] - kv[i])
	denom2 := float64(kv[i+k] - kv[i+1])
	var result float64
	if denom1 != 0 {
		result += (u - float64(kv[i])) * nurbsBasis(i, k-1, u, kv) / denom1
	}
	if denom2 != 0 {
		result += (float64(kv[i+k]) - u) * nurbsBasis(i+1, k-1, u, kv) / denom2
	}
	return result
}

// nurbsRden computes the rational denominator sum(N_i * W_i).
func nurbsRden(u float64, k uint32, pts []ControlPoint, kv []uint32) float64 {
	var d float64
	for i := uint32(0); i < uint32(len(pts)); i++ {
		d += nurbsBasis(i, k, u, kv) * pts[i].W
	}
	return d
}

// nurbsPoint evaluates the NURBS curve at parameter u.
func nurbsPoint(u float64, k uint32, pts []ControlPoint, kv []uint32) ControlPoint {
	den := nurbsRden(u, k, pts, kv)
	var p ControlPoint
	for i := uint32(0); i < uint32(len(pts)); i++ {
		basis := nurbsBasis(i, k, u, kv) * pts[i].W / den
		p.X += pts[i].X * basis
		p.Y += pts[i].Y * basis
	}
	return p
}

// nurbsTangent evaluates the tangent direction at parameter u using finite differences.
func nurbsTangent(u float64, k, n uint32, pts []ControlPoint, kv []uint32) ControlPoint {
	const du = 1e-5
	umax := float64(n - k + 2)
	ulo := math.Max(0, u-du)
	uhi := math.Min(umax, u+du)
	p1 := nurbsPoint(ulo, k, pts, kv)
	p3 := nurbsPoint(uhi, k, pts, kv)
	span := uhi - ulo
	t := ControlPoint{X: (p3.X - p1.X) / span, Y: (p3.Y - p1.Y) / span}
	h := math.Hypot(t.X, t.Y)
	if h != 0 {
		t.X /= h
		t.Y /= h
	}
	return t
}

// nurbsBiarc approximates a curve segment between two points with known tangents
// using a biarc (two circular arcs). Falls back to a straight line if degenerate.
func (c *Canon) nurbsBiarc(lineno int32, p0x, p0y, tsx, tsy, p4x, p4y, tex, tey float64) {
	// Normalize tangents
	h := math.Hypot(tsx, tsy)
	if h != 0 {
		tsx /= h
		tsy /= h
	}
	h = math.Hypot(tex, tey)
	if h != 0 {
		tex /= h
		tey /= h
	}

	r := 1.0
	vx := p0x - p4x
	vy := p0y - p4y
	cv := vx*vx + vy*vy
	b := 2 * (vx*(r*tsx+tex) + vy*(r*tsy+tey))
	a := 2 * r * (tsx*tex + tsy*tey - 1)

	discr := b*b - 4*a*cv
	if discr < 0 {
		return
	}
	disq := math.Sqrt(discr)
	beta1 := (-b - disq) / (2 * a)
	beta2 := (-b + disq) / (2 * a)
	if beta1 > 0 && beta2 > 0 {
		return
	}
	beta := math.Max(beta1, beta2)
	alpha := beta * r
	ab := alpha + beta

	p2x := (((p0x + alpha*tsx) * beta) + ((p4x - beta*tex) * alpha)) / ab
	p2y := (((p0y + alpha*tsy) * beta) + ((p4y - beta*tey) * alpha)) / ab
	p3x := p4x - beta*tex
	p3y := p4y - beta*tey
	tmx := p3x - p2x
	tmy := p3y - p2y
	h = math.Hypot(tmx, tmy)
	if h != 0 {
		tmx /= h
		tmy /= h
	}

	c.nurbsArc(lineno, p0x, p0y, p2x, p2y, tsx, tsy)
	c.nurbsArc(lineno, p2x, p2y, p4x, p4y, tmx, tmy)
}

// nurbsArc emits a single arc (or line) segment for NURBS approximation.
func (c *Canon) nurbsArc(lineno int32, x0, y0, x1, y1, dx, dy float64) {
	const small = 1e-6
	x := x1 - x0
	y := y1 - y0
	den := 2 * (y*dx - x*dy)

	// Get current other-axis positions in program coordinates
	s := c.state
	p := s.fromAbsolute(s.endPoint)

	if math.Abs(den) > small {
		r := -(x*x + y*y) / den
		cx := x0 + dy*r
		cy := y0 + (-dx)*r
		rotation := int32(1)
		if r >= 0 {
			rotation = -1
		}
		c.ArcFeed(lineno, x1, y1, cx, cy, rotation, p.Z, p.A, p.B, p.C, p.U, p.V, p.W)
	} else {
		c.StraightFeed(lineno, x1, y1, p.Z, p.A, p.B, p.C, p.U, p.V, p.W)
	}
}

// --- Internal helpers ---

func (c *Canon) enqueue(cmd QueuedCmd) {
	if c.discard {
		return
	}
	if err := c.task.EnqueueCmd(cmd); err != nil {
		c.task.logger.Error("canon enqueue failed", "cmd", cmd.String(), "err", err)
	}
}

// enqueueMotionParams sets vel/acc/term-cond before a feed move.
func (c *Canon) enqueueMotionParams() {
	s := c.state
	c.enqueue(&SetMotionParamsCmd{
		Vel:       s.linearFeedRate,
		Acc:       c.task.maxAcceleration,
		TermCond:  canonModeToTPTermCond(s.motionMode),
		Tolerance: s.motionTolerance,
	})
}

// --- Additional QueuedCmd types for canon ---

// RigidTapCmd queues a rigid tap.
type RigidTapCmd struct {
	Pos     Pose
	Vel     float64
	Acc     float64
	Scale   float64
	ID      int32
	FeedUpm float64
}

func (c *RigidTapCmd) Execute(t *Task) error {
	return t.motion.RigidTap(c.Pos, c.Vel, c.Vel, c.Acc, c.Scale, c.ID, c.FeedUpm)
}
func (c *RigidTapCmd) Wait() WaitType { return WaitNone }
func (c *RigidTapCmd) String() string { return fmt.Sprintf("RigidTap(id=%d)", c.ID) }
func (c *RigidTapCmd) LineID() int32  { return c.ID }

// ProbeCmd queues a probe move.
type ProbeCmd struct {
	Pos        Pose
	Vel        float64
	IniMaxVel  float64
	Acc        float64
	MotionType int32
	ProbeType  uint8
	ID         int32
	FeedUpm    float64
}

func (c *ProbeCmd) Execute(t *Task) error {
	return t.motion.Probe(c.Pos, c.Vel, c.IniMaxVel, c.Acc, c.MotionType, c.ProbeType, c.ID, c.FeedUpm)
}
func (c *ProbeCmd) Wait() WaitType { return WaitMotion }
func (c *ProbeCmd) String() string { return fmt.Sprintf("Probe(id=%d)", c.ID) }
func (c *ProbeCmd) LineID() int32  { return c.ID }

// SpindleOrientCmd orients a spindle.
type SpindleOrientCmd struct {
	Spindle     int32
	Orientation float64
	Mode        int32
}

func (c *SpindleOrientCmd) Execute(t *Task) error {
	return t.motion.SpindleOrient(c.Spindle, c.Orientation, c.Mode)
}
func (c *SpindleOrientCmd) Wait() WaitType { return WaitNone }
func (c *SpindleOrientCmd) String() string {
	return fmt.Sprintf("SpindleOrient(s=%d)", c.Spindle)
}

// WaitSpindleOrientedCmd waits for orient to complete.
type WaitSpindleOrientedCmd struct {
	Spindle int32
	Timeout float64
}

func (c *WaitSpindleOrientedCmd) Execute(t *Task) error { return nil }
func (c *WaitSpindleOrientedCmd) Wait() WaitType        { return WaitSpindleOriented }
func (c *WaitSpindleOrientedCmd) String() string        { return "WaitSpindleOriented" }

// MistOnCmd turns mist on.
type MistOnCmd struct{}

func (c *MistOnCmd) Execute(t *Task) error { return t.io.CoolantMistOn() }
func (c *MistOnCmd) Wait() WaitType        { return WaitNone }
func (c *MistOnCmd) String() string        { return "MistOn" }

// MistOffCmd turns mist off.
type MistOffCmd struct{}

func (c *MistOffCmd) Execute(t *Task) error { return t.io.CoolantMistOff() }
func (c *MistOffCmd) Wait() WaitType        { return WaitNone }
func (c *MistOffCmd) String() string        { return "MistOff" }

// SetMotionParamsCmd sets velocity/acceleration/termination before a move.
type SetMotionParamsCmd struct {
	Vel       float64
	Acc       float64
	TermCond  int32
	Tolerance float64
}

func (c *SetMotionParamsCmd) Execute(t *Task) error {
	if err := t.motion.SetVel(c.Vel); err != nil {
		return err
	}
	if c.Acc > 0 {
		if err := t.motion.SetAcc(c.Acc); err != nil {
			return err
		}
	}
	return t.motion.SetTermCond(c.TermCond, c.Tolerance)
}
func (c *SetMotionParamsCmd) Wait() WaitType { return WaitNone }
func (c *SetMotionParamsCmd) String() string { return "SetMotionParams" }

// FeedOverrideEnableCmd enables/disables feed override.
type FeedOverrideEnableCmd struct{ Enable bool }

func (c *FeedOverrideEnableCmd) Execute(t *Task) error {
	v := int32(0)
	if c.Enable {
		v = 1
	}
	return t.motion.FeedScaleEnable(v)
}
func (c *FeedOverrideEnableCmd) Wait() WaitType { return WaitNone }
func (c *FeedOverrideEnableCmd) String() string { return "FeedOverrideEnable" }

// FeedHoldEnableCmd enables/disables feed hold.
type FeedHoldEnableCmd struct{ Enable bool }

func (c *FeedHoldEnableCmd) Execute(t *Task) error {
	v := int32(0)
	if c.Enable {
		v = 1
	}
	return t.motion.FeedHoldEnable(v)
}
func (c *FeedHoldEnableCmd) Wait() WaitType { return WaitNone }
func (c *FeedHoldEnableCmd) String() string { return "FeedHoldEnable" }

// AdaptiveFeedEnableCmd enables/disables adaptive feed.
type AdaptiveFeedEnableCmd struct{ Enable bool }

func (c *AdaptiveFeedEnableCmd) Execute(t *Task) error {
	v := int32(0)
	if c.Enable {
		v = 1
	}
	return t.motion.AdaptiveFeedEnable(v)
}
func (c *AdaptiveFeedEnableCmd) Wait() WaitType { return WaitNone }
func (c *AdaptiveFeedEnableCmd) String() string { return "AdaptiveFeedEnable" }

// SetDoutCmd sets a digital output immediately.
type SetDoutCmd struct {
	Index int32
	Value int32
}

func (c *SetDoutCmd) Execute(t *Task) error { return t.motion.SetDout(c.Index, c.Value) }
func (c *SetDoutCmd) Wait() WaitType        { return WaitNone }
func (c *SetDoutCmd) String() string        { return fmt.Sprintf("SetDout(%d=%d)", c.Index, c.Value) }

// SetDoutSyncCmd sets a digital output synchronized with motion.
type SetDoutSyncCmd struct {
	Index      int32
	StartValue int32
	EndValue   int32
}

func (c *SetDoutSyncCmd) Execute(t *Task) error {
	return t.motion.SetDoutSynched(c.Index, c.StartValue, c.EndValue)
}
func (c *SetDoutSyncCmd) Wait() WaitType { return WaitNone }
func (c *SetDoutSyncCmd) String() string {
	return fmt.Sprintf("SetDoutSync(%d=%d→%d)", c.Index, c.StartValue, c.EndValue)
}

// SetAoutCmd sets an analog output immediately.
type SetAoutCmd struct {
	Index int32
	Value float64
}

func (c *SetAoutCmd) Execute(t *Task) error { return t.motion.SetAout(c.Index, c.Value) }
func (c *SetAoutCmd) Wait() WaitType        { return WaitNone }
func (c *SetAoutCmd) String() string        { return fmt.Sprintf("SetAout(%d=%.2f)", c.Index, c.Value) }

// SetAoutSyncCmd sets an analog output synchronized with motion.
type SetAoutSyncCmd struct {
	Index      int32
	StartValue float64
	EndValue   float64
}

func (c *SetAoutSyncCmd) Execute(t *Task) error {
	return t.motion.SetAoutSynched(c.Index, c.StartValue, c.EndValue)
}
func (c *SetAoutSyncCmd) Wait() WaitType { return WaitNone }
func (c *SetAoutSyncCmd) String() string {
	return fmt.Sprintf("SetAoutSync(%d=%.2f→%.2f)", c.Index, c.StartValue, c.EndValue)
}

// SpindleSyncCmd sets spindle synchronization for subsequent moves.
type SpindleSyncCmd struct {
	Sync       float64
	MotionType int32
}

func (c *SpindleSyncCmd) Execute(t *Task) error {
	return t.motion.SetSpindlesync(c.Sync, c.MotionType)
}
func (c *SpindleSyncCmd) Wait() WaitType { return WaitNone }
func (c *SpindleSyncCmd) String() string { return fmt.Sprintf("SpindleSync(%.3f)", c.Sync) }
