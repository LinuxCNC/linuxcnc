// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"time"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/emcstat"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/motstat"
)

// BuildStat constructs a complete StatFull from task state + motion status.
// This is the single source of truth for all stat consumers (REST, WS, halui).
func (t *Task) BuildStat() *emcstat.StatFull {
	t.mu.Lock()
	// Refresh active G/M codes from interpreter (C milltask does this every cycle).
	var callLevel int32
	if t.interp != nil {
		t.activeGcodes = t.interp.ActiveGCodes()
		t.activeMcodes = t.interp.ActiveMCodes()
		t.activeSettings = t.interp.ActiveSettings()
		callLevel = int32(t.interp.CallLevel())
	}
	// Bump the liveness counter once per status build (mirrors NML heartbeat).
	t.heartbeat++
	heartbeat := t.heartbeat
	// Grab canon state for offset reporting
	cs := t.canon.state
	// Remaining G4 dwell time (only meaningful while waiting for the delay).
	var delayLeft float64
	if t.execState == ExecWaitingForDelay {
		if d := time.Until(t.dwellEnd); d > 0 {
			delayLeft = d.Seconds()
		}
	}
	// Compute RCS command status (matches NML stat.state: 1=DONE,2=EXEC,3=ERROR).
	rcsStatus := int32(1) // RCS_DONE
	switch t.execState {
	case ExecError:
		rcsStatus = 3 // RCS_ERROR
	case ExecDone:
		rcsStatus = 1 // RCS_DONE
	default:
		rcsStatus = 2 // RCS_EXEC (any waiting state)
	}
	stat := &emcstat.StatFull{
		Task: emcstat.StatTaskInfo{
			Mode:              emcstat.TaskMode(t.mode),
			State:             emcstat.TaskState(t.state),
			InterpState:       emcstat.InterpState(t.interpState),
			ExecState:         emcstat.ExecState(t.execState),
			File:              t.programFile,
			OptionalStop:      t.optionalStop,
			BlockDelete:       t.blockDelete,
			TaskPaused:        t.interpState == InterpPaused,
			G5xIndex:          cs.g5xIndex,
			QueuedMdiCommands: int32(len(t.mdiQueue)),
			ReadLine:          t.readLine,
			CurrentLine:       t.currentLine,
			Line:              t.currentLine,
			ProgramUnits:      cs.lengthUnits,
			DelayLeft:         delayLeft,
			Command:           t.taskCommand,
			CallLevel:         callLevel,
			InputTimeout:      t.inputTimeout,
		},
		Flood:          t.floodOn,
		Mist:           t.mistOn,
		JointsCount:    int32(t.numJoints),
		AxisMask:       t.axisMask,
		LinearUnits:    t.linearUnits,
		State:          rcsStatus,
		RcsStatus:      rcsStatus,
		KinematicsType: emcstat.KinematicsType_IDENTITY,
		JogAxis:        t.jogAxis,
		JogIncrement:   t.jogIncrement,
		JogSpeed:       t.jogSpeed,
		AjogSpeed:      t.ajogSpeed,
		ActiveGcodes:   append([]int32(nil), t.activeGcodes...),
		ActiveMcodes:   append([]int32(nil), t.activeMcodes...),
		ActiveSettings: append([]float64(nil), t.activeSettings...),
		G5xOffset: emcstat.Position{
			X: cs.g5xOffset.X, Y: cs.g5xOffset.Y, Z: cs.g5xOffset.Z,
			A: cs.g5xOffset.A, B: cs.g5xOffset.B, C: cs.g5xOffset.C,
			U: cs.g5xOffset.U, V: cs.g5xOffset.V, W: cs.g5xOffset.W,
		},
		G92Offset: emcstat.Position{
			X: cs.g92Offset.X, Y: cs.g92Offset.Y, Z: cs.g92Offset.Z,
			A: cs.g92Offset.A, B: cs.g92Offset.B, C: cs.g92Offset.C,
			U: cs.g92Offset.U, V: cs.g92Offset.V, W: cs.g92Offset.W,
		},
		RotationXy: cs.xyRotation,
		PreviewSeq: t.previewSeq,
		Heartbeat:  heartbeat,
	}
	numJoints := t.numJoints
	numSpindles := t.numSpindles
	axisMask := t.axisMask
	t.mu.Unlock()

	// Always allocate axes/joints/spindle slices so consumers never see nil.
	nAxes := countAxes(axisMask)
	if nAxes > 0 {
		stat.Axis = make([]emcstat.AxisInfo, nAxes)
	}
	if numJoints > 0 {
		stat.Joints = make([]emcstat.JointInfo, numJoints)
	}
	if numSpindles > 0 {
		stat.Spindle = make([]emcstat.SpindleInfo, numSpindles)
	}

	// Read motion status (lock-free triple buffer, never fails).
	ms, err := t.status.GetStatus()
	if err != nil {
		// Should not happen with triple buffer, but handle gracefully.
		if !t.hasMotionStatus {
			return stat
		}
		ms = t.lastMotionStatus
	} else {
		t.lastMotionStatus = ms
		t.hasMotionStatus = true
	}

	// Kinematics type from motion module.
	stat.KinematicsType = emcstat.KinematicsType(ms.KinType)

	// Motion info.
	switch {
	case ms.Coord != 0:
		stat.Motion.Mode = emcstat.TrajMode_COORD
	case ms.Teleop != 0:
		stat.Motion.Mode = emcstat.TrajMode_TELEOP
	default:
		stat.Motion.Mode = emcstat.TrajMode_FREE
	}
	stat.Motion.Enabled = ms.Enabled != 0
	stat.Motion.InPosition = ms.Inpos != 0
	stat.Motion.Paused = ms.Paused != 0
	stat.Motion.Feedrate = ms.FeedScale
	stat.Motion.Rapidrate = ms.RapidScale
	stat.Motion.MaxVelocity = ms.LimitVel
	stat.Motion.Velocity = ms.RequestedVel
	stat.Motion.CurrentVel = ms.CurrentVel
	stat.Motion.DistanceToGo = ms.DistanceToGo
	stat.Motion.MotionId = ms.Id
	stat.Motion.MotionLine = t.lookupMotionLine(ms.Id)
	stat.Motion.MotionType = ms.MotionType
	stat.Motion.FeedOverrideEnabled = ms.FeedScaleEnabled != 0
	stat.Motion.AdaptiveFeedEnabled = ms.AdaptiveFeedEnabled != 0
	stat.Motion.FeedHoldEnabled = ms.FeedHoldEnabled != 0
	stat.Motion.Queue = ms.QueueDepth
	stat.Motion.QueueFull = ms.QueueFull != 0
	stat.Task.MotionLine = t.lookupMotionLine(ms.Id)
	// Resolve the active G/M codes and current line from the state tag of the
	// segment actually executing (motion echoes only the id back). This makes
	// status reflect what the machine is running now rather than the
	// interpreter's readahead. Falls back to the readahead codes set above when
	// the moving segment has no tag (idle, MDI, or before the first tagged move).
	if info, ok := t.lookupMotionInfo(ms.Id); ok && info.Gcodes != nil {
		stat.ActiveGcodes = append([]int32(nil), info.Gcodes...)
		stat.ActiveMcodes = append([]int32(nil), info.Mcodes...)
		stat.ActiveSettings = append([]float64(nil), info.Settings...)
		stat.Task.CurrentLine = info.LineNo
		stat.Task.Line = info.LineNo
	}
	t.pruneMotionMap(ms.Id)
	stat.Motion.Dtg = emcstat.Position{
		X: ms.Dtg.X, Y: ms.Dtg.Y, Z: ms.Dtg.Z,
		A: ms.Dtg.A, B: ms.Dtg.B, C: ms.Dtg.C,
		U: ms.Dtg.U, V: ms.Dtg.V, W: ms.Dtg.W,
	}

	// Positions.
	stat.Position = poseToPosition(ms.CartePosCmd)
	stat.ActualPosition = poseToPosition(ms.CartePosFb)
	stat.ToolOffset = poseToPosition(ms.ToolOffset)
	stat.ProbedPosition = poseToPosition(ms.Probe.Pos)

	// Tool info from IO controller.
	if t.io != nil {
		if v, err := t.io.GetToolInSpindle(); err == nil {
			stat.ToolInSpindle = v
		}
		if v, err := t.io.GetPocketPrepped(); err == nil {
			stat.PocketPrepped = v
		}
	}

	// Joint actual positions (feedback).
	for i := 0; i < numJoints && i < 16; i++ {
		stat.JointActualPosition[i] = ms.Joints[i].PosFb
	}

	// Joints array.
	for i := 0; i < numJoints; i++ {
		j := &ms.Joints[i]
		stat.Joints[i] = emcstat.JointInfo{
			Homed:          j.Homed != 0,
			Homing:         j.Homing != 0,
			Enabled:        j.Enabled != 0,
			Fault:          j.Fault != 0,
			MinSoftLimit:   j.MinPosLimit,
			MaxSoftLimit:   j.MaxPosLimit,
			MinHardLimit:   j.OnNegLimit != 0,
			MaxHardLimit:   j.OnPosLimit != 0,
			// A non-zero override mask means limit checking is currently
			// overridden; report it on every joint (matches 2.9 taskintf.cc,
			// which UIs read via joint[0] as a global indicator).
			OverrideLimits: ms.OverrideLimitMask != 0,
			Velocity:       j.VelCmd,
			Input:          j.PosFb,
			Output:         j.PosCmd,
		}
		stat.Homed[i] = j.Homed != 0
		if j.OnPosLimit != 0 {
			stat.Limit[i] = 1
		} else if j.OnNegLimit != 0 {
			stat.Limit[i] = -1
		}
	}

	// Axes array (from axis_mask).
	for i := 0; i < nAxes && i < 9; i++ {
		ax := &ms.Axes[i]
		stat.Axis[i] = emcstat.AxisInfo{
			MinPositionLimit: ax.MinPosLimit,
			MaxPositionLimit: ax.MaxPosLimit,
			// Commanded teleop velocity, matching C++ axis->teleop_vel_cmd
			// (taskintf.cc:574) — NOT the static vel_limit.
			Velocity: ax.Velocity,
		}
	}

	// Spindles.
	for i := 0; i < numSpindles && i < 8; i++ {
		sp := &ms.Spindles[i]
		stat.Spindle[i] = emcstat.SpindleInfo{
			Speed:           sp.Speed,
			Direction:       sp.Direction,
			Brake:           sp.Brake != 0,
			Enabled:         sp.State != 0,
			Override:        sp.Scale,
			OverrideEnabled: ms.SpindleScaleEnabled != 0,
			Homed:           sp.Homed != 0,
			OrientState:     sp.OrientState,
			OrientFault:     sp.OrientFault,
		}
	}

	return stat
}

// poseToPosition converts a motstat.Pose to emcstat.Position.
func poseToPosition(p motstat.Pose) emcstat.Position {
	return emcstat.Position{
		X: p.X, Y: p.Y, Z: p.Z,
		A: p.A, B: p.B, C: p.C,
		U: p.U, V: p.V, W: p.W,
	}
}

// countAxes returns the number of set bits in axis_mask.
func countAxes(mask int32) int {
	n := 0
	for m := uint32(mask); m != 0; m >>= 1 {
		n += int(m & 1)
	}
	return n
}
