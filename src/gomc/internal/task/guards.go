// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"fmt"
	"time"
)

// Guard errors returned when a command is rejected due to state/mode.
var (
	ErrNotOn     = fmt.Errorf("machine not on")
	ErrWrongMode = fmt.Errorf("wrong mode for command")
	ErrEstop     = fmt.Errorf("machine in estop")
	ErrBusy      = fmt.Errorf("interpreter busy")
	ErrNoProgram = fmt.Errorf("no program loaded")
	ErrNotHomed  = fmt.Errorf("not homed")
)

// requireState checks that the machine is in the required state.
func (t *Task) requireState(required TaskState) error {
	if t.state != required {
		return fmt.Errorf("%w: need %s, have %s", ErrNotOn, required, t.state)
	}
	return nil
}

// requireOn checks that the machine is powered on.
func (t *Task) requireOn() error {
	if t.state != StateOn {
		return fmt.Errorf("%w: state is %s", ErrNotOn, t.state)
	}
	return nil
}

// requireMode checks that the machine is in the specified mode.
func (t *Task) requireMode(required TaskMode) error {
	if t.mode != required {
		return fmt.Errorf("%w: need %s, have %s", ErrWrongMode, required, t.mode)
	}
	return nil
}

// ensureMode switches to the required mode if safe (interpreter idle), or
// returns nil if already in the right mode. This replaces client-side
// ensure_mode() calls — the server decides whether a mode switch is allowed.
// The previous mode is saved for transactional restore after command completion.
// Must be called with t.mu held. May temporarily unlock t.mu for I/O.
func (t *Task) ensureMode(required TaskMode) error {
	if t.mode == required {
		return nil
	}
	// Cannot switch mode while interpreter is active.
	if t.interpState != InterpIdle {
		return fmt.Errorf("%w: interpreter busy, cannot switch to %s", ErrBusy, required)
	}
	// Cannot switch mode while homing is in progress.
	if t.anyJointHoming() {
		return fmt.Errorf("%w: homing in progress", ErrBusy)
	}
	// Save previous mode for transactional restore (only first switch in a tx).
	if !t.modeTx {
		t.modeBeforeTx = t.mode
		t.modeTx = true
	}
	// Perform the mode switch inline (same logic as SetMode but already holding mu).
	switch required {
	case ModeManual:
		t.abortLocked()
		t.mode = ModeManual
		t.mu.Unlock()
		_ = t.motion.SetFree()
		t.waitMotionFree()
		t.mu.Lock()
	case ModeMDI:
		t.abortLocked()
		t.mode = ModeMDI
		t.mu.Unlock()
		_ = t.motion.SetCoord()
		if t.interp != nil {
			_ = t.interp.Synch()
		}
		t.mu.Lock()
	case ModeAuto:
		t.abortLocked()
		t.mode = ModeAuto
		t.mu.Unlock()
		_ = t.motion.SetCoord()
		if t.interp != nil {
			_ = t.interp.Synch()
		}
		t.mu.Lock()
	default:
		return fmt.Errorf("%w: unknown mode %d", ErrWrongMode, required)
	}
	return nil
}

// restoreModeTx restores the mode saved by ensureMode after a transactional
// command sequence completes. Must be called with t.mu held.
// May temporarily unlock t.mu for I/O.
func (t *Task) restoreModeTx() {
	if !t.modeTx {
		return
	}
	t.modeTx = false
	target := t.modeBeforeTx
	if t.mode == target {
		return
	}
	// Perform restore (same as ensureMode switch but no save).
	switch target {
	case ModeManual:
		t.mode = ModeManual
		t.mu.Unlock()
		if t.allHomed() {
			_ = t.motion.SetTeleop()
			t.waitMotionTeleop()
		} else {
			_ = t.motion.SetFree()
			t.waitMotionFree()
		}
		t.mu.Lock()
	case ModeMDI:
		t.mode = ModeMDI
		t.mu.Unlock()
		_ = t.motion.SetCoord()
		if t.interp != nil {
			_ = t.interp.Synch()
		}
		t.mu.Lock()
	case ModeAuto:
		t.mode = ModeAuto
		t.mu.Unlock()
		_ = t.motion.SetCoord()
		if t.interp != nil {
			_ = t.interp.Synch()
		}
		t.mu.Lock()
	}
}

// waitMotionFree polls motion status until the motion controller is in FREE
// mode (not coord and not teleop). Must be called WITHOUT t.mu held.
func (t *Task) waitMotionFree() {
	deadline := time.Now().Add(500 * time.Millisecond)
	for time.Now().Before(deadline) {
		ms, err := t.status.GetStatus()
		if err == nil && ms.Coord == 0 && ms.Teleop == 0 {
			return
		}
		time.Sleep(pollInterval)
	}
}

// waitMotionTeleop waits until motion is in teleop mode (up to 500ms).
// Retries sending SetTeleop if motion hasn't switched (e.g. wasn't INPOS).
func (t *Task) waitMotionTeleop() bool {
	deadline := time.Now().Add(500 * time.Millisecond)
	for time.Now().Before(deadline) {
		ms, err := t.status.GetStatus()
		if err == nil && ms.Teleop != 0 {
			return true
		}
		// Retry sending SetTeleop (motion may have rejected due to !INPOS).
		_ = t.motion.SetTeleop()
		time.Sleep(pollInterval)
	}
	return false
}

// requireNotEstop checks that we are not in estop.
func (t *Task) requireNotEstop() error {
	if t.state == StateEstop {
		return ErrEstop
	}
	return nil
}

// requireInterpIdle checks that the interpreter is idle (for jog-while-idle).
func (t *Task) requireInterpIdle() error {
	if t.interpState != InterpIdle {
		return ErrBusy
	}
	return nil
}

// requireProgram checks that a program file is loaded (for AUTO RUN).
func (t *Task) requireProgram() error {
	if !t.programOpen {
		return ErrNoProgram
	}
	return nil
}

// allHomed returns true if all joints are homed.
func (t *Task) allHomed() bool {
	ms, err := t.status.GetStatus()
	if err != nil {
		return false
	}
	for j := 0; j < t.numJoints; j++ {
		if ms.Joints[j].Homed == 0 {
			return false
		}
	}
	return true
}

// anyJointHoming returns true if any joint is currently in a homing sequence.
func (t *Task) anyJointHoming() bool {
	if t.status == nil {
		return false
	}
	ms, err := t.status.GetStatus()
	if err != nil {
		return false
	}
	for j := 0; j < t.numJoints; j++ {
		if ms.Joints[j].Homing != 0 {
			return true
		}
	}
	return false
}

// requireHomed checks that all joints are homed (unless NO_FORCE_HOMING is set).
func (t *Task) requireHomed() error {
	if t.noForceHoming {
		return nil
	}
	if !t.allHomed() {
		return ErrNotHomed
	}
	return nil
}

// canJog returns true if jogging is allowed in current state.
// Jogging is allowed in MANUAL mode, or in AUTO/MDI when interpreter is idle.
func (t *Task) canJog() error {
	if err := t.requireOn(); err != nil {
		return err
	}
	switch t.mode {
	case ModeManual:
		return nil
	case ModeAuto, ModeMDI:
		// Allow jog while idle (not running a program or MDI)
		return t.requireInterpIdle()
	default:
		return ErrWrongMode
	}
}

// externalOffsetApplied checks if external offsets are currently applied.
// Must be called with t.mu held (reads t.status which is immutable after init,
// but the check itself is a motion status read).
func (t *Task) externalOffsetApplied() bool {
	if t.status == nil {
		return false
	}
	ms, err := t.status.GetStatus()
	if err != nil {
		return false
	}
	return ms.ExternalOffsetsApplied != 0
}
