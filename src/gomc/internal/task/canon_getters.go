// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import "github.com/sittner/linuxcnc/src/gomc/pkg/hal"

// Canon getter callbacks — called by the interpreter to query current state.
// These read from the Task's MotionStatus interface or from canon state.

func (c *Canon) GetExternalFeedRate() (float64, error) {
	return c.state.toProg(c.state.linearFeedRate * 60.0), nil // mm/sec → units/min
}

func (c *Canon) GetExternalTraverseRate() (float64, error) {
	// Return in program units per minute (matching C emccanon behavior).
	// Read from task.maxVelocity (set by loadConfig) like C reads from STAT.
	return c.state.toProg(c.task.maxVelocity) * 60.0, nil
}

func (c *Canon) GetExternalLengthUnitType() (int32, error) {
	return c.state.lengthUnits, nil
}

func (c *Canon) GetExternalLengthUnits() (float64, error) {
	// Return the machine's native linear units (from [TRAJ]LINEAR_UNITS).
	// This must NOT vary with the active G20/G21 setting — it tells the
	// interpreter what units parameters and external data are stored in.
	return c.task.linearUnits, nil
}

func (c *Canon) GetExternalAngleUnits() (float64, error) {
	return 1.0, nil // always degrees
}

func (c *Canon) GetExternalMotionControlMode() (int32, error) {
	return c.state.motionMode, nil
}

func (c *Canon) GetExternalMotionControlTolerance() (float64, error) {
	return c.state.toProg(c.state.motionTolerance), nil
}

func (c *Canon) GetExternalMotionControlNaivecamTolerance() (float64, error) {
	return c.state.toProg(c.state.naivecamTol), nil
}

func (c *Canon) GetExternalFlood() (int32, error) {
	if c.state.floodOn {
		return 1, nil
	}
	return 0, nil
}

func (c *Canon) GetExternalMist() (int32, error) {
	if c.state.mistOn {
		return 1, nil
	}
	return 0, nil
}

// Position getters — return current position in program units.
// Like the C canon (GET_EXTERNAL_POSITION), these read from endPoint
// (absolute machine coordinates, synced from CartePosFb before each synch)
// and return the position with offsets removed, in program units.
// This matches the C canon's unoffset_and_unrotate_pos + to_prog.

func (c *Canon) syncEndPointFromMachine() {
	if c.task == nil || c.task.status == nil {
		return
	}
	ms, err := c.task.status.GetStatus()
	if err != nil {
		return
	}
	p := ms.CartePosFb
	c.state.endPoint = Pose{
		X: p.X, Y: p.Y, Z: p.Z,
		A: p.A, B: p.B, C: p.C,
		U: p.U, V: p.V, W: p.W,
	}
}

// getExternalPosition returns the current position with offsets removed,
// matching the C canon's unoffset_and_unrotate_pos + to_prog.
func (c *Canon) getExternalPosition() Pose {
	return c.state.fromAbsolute(c.state.endPoint)
}

func (c *Canon) GetExternalPositionX() (float64, error) {
	return c.getExternalPosition().X, nil
}

func (c *Canon) GetExternalPositionY() (float64, error) {
	return c.getExternalPosition().Y, nil
}

func (c *Canon) GetExternalPositionZ() (float64, error) {
	return c.getExternalPosition().Z, nil
}

func (c *Canon) GetExternalPositionA() (float64, error) {
	return c.getExternalPosition().A, nil
}

func (c *Canon) GetExternalPositionB() (float64, error) {
	return c.getExternalPosition().B, nil
}

func (c *Canon) GetExternalPositionC() (float64, error) {
	return c.getExternalPosition().C, nil
}

func (c *Canon) GetExternalPositionU() (float64, error) {
	return c.getExternalPosition().U, nil
}

func (c *Canon) GetExternalPositionV() (float64, error) {
	return c.getExternalPosition().V, nil
}

func (c *Canon) GetExternalPositionW() (float64, error) {
	return c.getExternalPosition().W, nil
}

// Probe position getters — return probe trip position in program units.

func (c *Canon) getProbePos() Pose {
	if c.task.status == nil {
		return Pose{}
	}
	ms, err := c.task.status.GetStatus()
	if err != nil {
		return Pose{}
	}
	// Convert motstat Pose to task Pose
	machinePos := Pose{
		X: ms.Probe.Pos.X, Y: ms.Probe.Pos.Y, Z: ms.Probe.Pos.Z,
		A: ms.Probe.Pos.A, B: ms.Probe.Pos.B, C: ms.Probe.Pos.C,
		U: ms.Probe.Pos.U, V: ms.Probe.Pos.V, W: ms.Probe.Pos.W,
	}
	return c.state.fromAbsolute(machinePos)
}

func (c *Canon) GetExternalProbePositionX() (float64, error) { return c.getProbePos().X, nil }
func (c *Canon) GetExternalProbePositionY() (float64, error) { return c.getProbePos().Y, nil }
func (c *Canon) GetExternalProbePositionZ() (float64, error) { return c.getProbePos().Z, nil }
func (c *Canon) GetExternalProbePositionA() (float64, error) { return c.getProbePos().A, nil }
func (c *Canon) GetExternalProbePositionB() (float64, error) { return c.getProbePos().B, nil }
func (c *Canon) GetExternalProbePositionC() (float64, error) { return c.getProbePos().C, nil }
func (c *Canon) GetExternalProbePositionU() (float64, error) { return c.getProbePos().U, nil }
func (c *Canon) GetExternalProbePositionV() (float64, error) { return c.getProbePos().V, nil }
func (c *Canon) GetExternalProbePositionW() (float64, error) { return c.getProbePos().W, nil }

func (c *Canon) GetExternalProbeValue() (float64, error) {
	if c.task.status == nil {
		return 0, nil
	}
	ms, err := c.task.status.GetStatus()
	if err != nil {
		return 0, nil
	}
	return float64(ms.Probe.Val), nil
}

func (c *Canon) GetExternalProbeTrippedValue() (int32, error) {
	if c.task.status == nil {
		return 0, nil
	}
	ms, err := c.task.status.GetStatus()
	if err != nil {
		return 0, nil
	}
	return ms.Probe.Tripped, nil
}

// Spindle getters.

func (c *Canon) GetExternalSpeed(spindle int32) (float64, error) {
	if spindle >= 0 && spindle < 8 {
		return c.state.spindleSpeed[spindle], nil
	}
	return 0, nil
}

func (c *Canon) GetExternalSpindle(spindle int32) (int32, error) {
	// CANON_STOPPED=1, CANON_CLOCKWISE=2, CANON_COUNTERCLOCKWISE=3
	if int(spindle) < len(c.state.spindleSpeed) {
		speed := c.state.spindleSpeed[spindle]
		if speed > 0 {
			return 2, nil // CANON_CLOCKWISE
		} else if speed < 0 {
			return 3, nil // CANON_COUNTERCLOCKWISE
		}
	}
	return 1, nil // CANON_STOPPED
}

// Tool getters.

func (c *Canon) GetExternalToolLengthXoffset() (float64, error) {
	return c.state.toolOffset.X, nil
}
func (c *Canon) GetExternalToolLengthYoffset() (float64, error) {
	return c.state.toolOffset.Y, nil
}
func (c *Canon) GetExternalToolLengthZoffset() (float64, error) {
	return c.state.toolOffset.Z, nil
}
func (c *Canon) GetExternalToolLengthAoffset() (float64, error) {
	return c.state.toolOffset.A, nil
}
func (c *Canon) GetExternalToolLengthBoffset() (float64, error) {
	return c.state.toolOffset.B, nil
}
func (c *Canon) GetExternalToolLengthCoffset() (float64, error) {
	return c.state.toolOffset.C, nil
}
func (c *Canon) GetExternalToolLengthUoffset() (float64, error) {
	return c.state.toolOffset.U, nil
}
func (c *Canon) GetExternalToolLengthVoffset() (float64, error) {
	return c.state.toolOffset.V, nil
}
func (c *Canon) GetExternalToolLengthWoffset() (float64, error) {
	return c.state.toolOffset.W, nil
}

func (c *Canon) GetExternalToolSlot() (int32, error) {
	if c.task.io == nil {
		return 0, nil
	}
	v, err := c.task.io.GetToolInSpindle()
	if err != nil {
		return 0, nil
	}
	return v, nil
}

func (c *Canon) GetExternalSelectedToolSlot() (int32, error) {
	if c.task.io == nil {
		return 0, nil
	}
	v, err := c.task.io.GetPocketPrepped()
	if err != nil {
		return 0, nil
	}
	return v, nil
}

func (c *Canon) GetExternalToolTable(pocket int32) (int32, int32, [9]float64, float64, float64, float64, int32, error) {
	retval, toolno, offset, diameter, frontangle, backangle, orientation := getToolByPocket(pocket)
	return retval, toolno, offset, diameter, frontangle, backangle, orientation, nil
}

func (c *Canon) GetToolByNumber(toolno int32) (int32, int32, [9]float64, float64, float64, float64, int32, error) {
	if pkgTTClient == nil {
		return -1, 0, [9]float64{}, 0, 0, 0, 0, nil
	}
	entry, err := pkgTTClient.GetTool(toolno)
	if err != nil {
		return -1, 0, [9]float64{}, 0, 0, 0, 0, nil
	}
	offset := [9]float64{
		entry.XOffset, entry.YOffset, entry.ZOffset,
		entry.AOffset, entry.BOffset, entry.COffset,
		entry.UOffset, entry.VOffset, entry.WOffset,
	}
	return 0, entry.Pocketno, offset, entry.Diameter, entry.Frontangle, entry.Backangle, entry.Orientation, nil
}

func (c *Canon) GetExternalTcFault() (int32, error)  { return 0, nil }
func (c *Canon) GetExternalTcReason() (int32, error) { return 0, nil }

// Queue/status getters.

func (c *Canon) GetExternalQueueEmpty() (int32, error) {
	if c.task.status != nil {
		v, err := c.task.status.GetInpos()
		if err == nil && v != 0 {
			return 1, nil
		}
	}
	return 0, nil
}

func (c *Canon) GetExternalAxisMask() (int32, error) {
	return c.task.axisMask, nil
}

func (c *Canon) GetExternalDigitalInput(index, def int32) (int32, error) {
	// An M66 wait that timed out returns -1 so the interpreter stores -1 into
	// #5399 (C++ GET_EXTERNAL_DIGITAL_INPUT returns -1 when input_timeout==1).
	if c.task.inputTimedOut() {
		return -1, nil
	}
	if c.task.status == nil {
		return def, nil
	}
	ms, err := c.task.status.GetStatus()
	if err != nil || index < 0 || index >= 64 {
		return def, nil
	}
	return ms.SynchDi[index], nil
}

func (c *Canon) GetExternalAnalogInput(index int32, def float64) (float64, error) {
	// See GetExternalDigitalInput: a timed-out M66 wait returns -1 (#5399).
	if c.task.inputTimedOut() {
		return -1, nil
	}
	if c.task.status == nil {
		return def, nil
	}
	ms, err := c.task.status.GetStatus()
	if err != nil || index < 0 || index >= 64 {
		return def, nil
	}
	return ms.AnalogInput[index], nil
}

func (c *Canon) GetExternalFeedOverrideEnable() (int32, error) {
	if c.state.feedOverrideEnabled {
		return 1, nil
	}
	return 0, nil
}

func (c *Canon) GetExternalSpindleOverrideEnable(spindle int32) (int32, error) {
	if spindle >= 0 && spindle < 8 && c.state.speedOverrideEnabled[spindle] {
		return 1, nil
	}
	return 0, nil
}

func (c *Canon) GetExternalAdaptiveFeedEnable() (int32, error) {
	if c.state.adaptiveFeedEnabled {
		return 1, nil
	}
	return 0, nil
}

func (c *Canon) GetExternalFeedHoldEnable() (int32, error) {
	if c.state.feedHoldEnabled {
		return 1, nil
	}
	return 0, nil
}

func (c *Canon) GetExternalPlane() (int32, error) {
	return c.state.activePlane, nil
}

func (c *Canon) GetExternalParameterFileName() string {
	return c.parameterFileName
}

func (c *Canon) GetExternalOffsetApplied() (int32, error) {
	if c.task.status == nil {
		return 0, nil
	}
	ms, err := c.task.status.GetStatus()
	if err != nil {
		return 0, nil
	}
	return ms.ExternalOffsetsApplied, nil
}

func (c *Canon) GetExternalOffsets() [9]float64 {
	if c.task.status == nil {
		return [9]float64{}
	}
	ms, err := c.task.status.GetStatus()
	if err != nil {
		return [9]float64{}
	}
	return [9]float64{
		ms.EoffsetPose.X, ms.EoffsetPose.Y, ms.EoffsetPose.Z,
		ms.EoffsetPose.A, ms.EoffsetPose.B, ms.EoffsetPose.C,
		ms.EoffsetPose.U, ms.EoffsetPose.V, ms.EoffsetPose.W,
	}
}

func (c *Canon) GetUserDefinedResult() (float64, error) {
	return 0, nil
}
func (c *Canon) GetExternalHalValue(name string) (float64, int32, error) {
	val, found := hal.LookupValue(name)
	if found {
		return val, 1, nil
	}
	return 0, 0, nil
}
