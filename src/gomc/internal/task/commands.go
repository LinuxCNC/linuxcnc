// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"fmt"
	"time"
)

// mcodeAbort signals the M-code handler worker to stop.
func (t *Task) mcodeAbort() {
	if t.mcode != nil {
		t.mcode.Abort()
	}
}

// This file implements the 27 emccmd GMI methods.
// Each method corresponds to a UI command entry point.
// Pattern: guard → action → state update.

// AutoCmd constants.
const (
	AutoRun     int32 = 0
	AutoPause   int32 = 1
	AutoResume  int32 = 2
	AutoStep    int32 = 3
	AutoReverse int32 = 4
)

// JogType constants.
const (
	JogStop       int32 = 0
	JogContinuous int32 = 1
	JogIncrement  int32 = 2
	JogAbsolute   int32 = 3
)

// SpindleCmd constants. Values MUST match the emccmd SpindleCmd wire enum
// (generated/gmi/emccmd) — the GMI provider forwards the raw wire value, and
// halui calls these by name, so both paths go through the same numbering.
// Increase/Decrease previously used 2/-2 and never matched the wire enum's
// 10/11, so spindle +/- override was a silent no-op from GMI/WebSocket clients.
const (
	SpindleOff      int32 = 0
	SpindleForward  int32 = 1
	SpindleReverse  int32 = -1
	SpindleIncrease int32 = 10
	SpindleDecrease int32 = 11
	SpindleConstant int32 = 12
)

// IO abort reason codes (EMC_IO_ABORT_REASON_ENUM, iocontrol_stat.h).
const (
	emcAbortAuxEstop           int32 = 2 // EMC_ABORT_AUX_ESTOP
	emcAbortMotionOrIoRcsError int32 = 3 // EMC_ABORT_MOTION_OR_IO_RCS_ERROR
	emcAbortTaskStateOff       int32 = 4 // EMC_ABORT_TASK_STATE_OFF
	emcAbortTaskStateNotOn     int32 = 7 // EMC_ABORT_TASK_STATE_NOT_ON
	emcAbortTaskAbort          int32 = 8 // EMC_ABORT_TASK_ABORT
)

// machineShutdown performs the full machine teardown shared by ESTOP and
// STATE_OFF: abort the sequencer/mcode/motion, disable motion, stop all
// spindles, coolant and lube, abort IO, unhome volatile-home joints, and reset
// the interpreter. Mirrors C++ emcTaskSetState(OFF/ESTOP). Must be called
// WITHOUT t.mu held. ioReason is the EMC_ABORT_* code passed to io.IoAbort.
func (t *Task) machineShutdown(numSpindles int, ioReason int32) {
	t.AbortSequencer()
	t.mcodeAbort()
	_ = t.motion.Abort()
	_ = t.motion.Disable()
	for i := 0; i < numSpindles; i++ {
		_ = t.motion.SpindleOff(int32(i))
	}
	_ = t.io.CoolantFloodOff()
	_ = t.io.CoolantMistOff()
	_ = t.io.LubeOff()
	_ = t.io.IoAbort(ioReason)
	_ = t.motion.JointUnhome(-2) // unhome only volatile-home joints
	if t.interp != nil {
		_ = t.interp.Abort(0, "machine off")
		_ = t.interp.Close()
		_ = t.interp.Reset()
		t.canon.syncEndPointFromMachine()
		_ = t.interp.Synch()
	}
	t.StartSequencer()
}

// SetState handles state transitions: estop, estop_reset, off, on.
func (t *Task) SetState(state int32) error {
	t.mu.Lock()

	target := TaskState(state)
	switch target {
	case StateEstop:
		wasOn := t.state == StateOn
		numSpindles := t.numSpindles
		t.state = StateEstop
		t.interpState = InterpIdle
		t.execState = ExecDone
		t.mdiQueue = t.mdiQueue[:0]
		t.taskCommand = ""
		t.stepping = false
		t.programOpen = false
		t.programFile = ""
		t.mu.Unlock()

		if wasOn {
			t.machineShutdown(numSpindles, emcAbortAuxEstop)
		} else {
			_ = t.motion.Disable()
			_ = t.io.LubeOff()
		}
		_ = t.io.EstopOn()

		t.mu.Lock()
		t.floodOn = false
		t.mistOn = false
		t.lubeOn = false
		t.mu.Unlock()
		return nil

	case StateEstopReset:
		if t.state == StateEstopReset {
			t.mu.Unlock()
			return nil // idempotent
		}
		if t.state != StateEstop {
			t.mu.Unlock()
			return ErrEstop
		}
		// Request estop-off from IO (sets user-enable-out=1).
		// The actual state transition only happens when emc-enable-in
		// confirms it, matching 2.9's determineState() behavior.
		t.mu.Unlock()
		_ = t.io.EstopOff()
		_ = t.io.LubeOff() // C++ emcTaskSetState(ESTOP_RESET) turns lube off
		t.mu.Lock()
		t.lubeOn = false
		t.mu.Unlock()

		// Check if IO confirms estop cleared (immediate HAL loopback case).
		// If IO status is not available, trust that the monitor will handle it.
		if t.ioStat != nil {
			if st, err := t.ioStat.GetIOFullStatus(); err == nil && !st.Estop {
				t.mu.Lock()
				t.state = StateEstopReset
				t.mu.Unlock()
			}
		}
		return nil

	case StateOff:
		if err := t.requireNotEstop(); err != nil {
			t.mu.Unlock()
			return err
		}
		wasOn := t.state == StateOn
		numSpindles := t.numSpindles
		// Report ESTOP_RESET: the C milltask has no distinct OFF state —
		// determineState()/the monitor return ESTOP_RESET when traj is
		// disabled and not in estop, and Axis toggles ESTOP_RESET ↔ ON.
		t.state = StateEstopReset
		t.interpState = InterpIdle
		t.execState = ExecDone
		t.mdiQueue = t.mdiQueue[:0]
		t.taskCommand = ""
		t.stepping = false
		t.mu.Unlock()

		// Full safe shutdown (matches C++ emcTaskSetState(OFF)) — previously
		// this only did motion.Disable(), leaving spindles/coolant/lube on and
		// volatile-home joints homed.
		if wasOn {
			t.machineShutdown(numSpindles, emcAbortTaskStateOff)
		} else {
			_ = t.motion.Disable()
			_ = t.io.LubeOff()
		}

		t.mu.Lock()
		t.floodOn = false
		t.mistOn = false
		t.lubeOn = false
		t.mu.Unlock()
		return nil

	case StateOn:
		if t.state == StateOn {
			t.mu.Unlock()
			return nil // idempotent
		}
		if t.state != StateEstopReset && t.state != StateOff {
			t.mu.Unlock()
			return ErrNotOn
		}
		t.mu.Unlock()
		if err := t.motion.Enable(); err != nil {
			return err
		}
		// Enable override scaling so feed/spindle override controls work.
		_ = t.motion.FeedScaleEnable(1)
		_ = t.motion.FeedHoldEnable(1)
		for s := int32(0); s < int32(t.numSpindles); s++ {
			_ = t.motion.SpindleScaleEnable(s, 1)
		}
		_ = t.io.LubeOn() // C++ emcTaskSetState(ON) turns lube on
		t.mu.Lock()
		t.state = StateOn
		t.lubeOn = true
		// Clear any lingering error state from a previous fault so the
		// machine can accept commands again after off/on recovery.
		if t.execState == ExecError {
			t.execState = ExecDone
			t.interpState = InterpIdle
		}
		t.mu.Unlock()
		return nil
	}
	t.mu.Unlock()
	return nil
}

// SetMode switches between MANUAL, MDI, and AUTO.
// This is an explicit mode switch (e.g. from a mode selector or HAL pin).
// It clears any transactional mode save — the user deliberately chose this mode.
func (t *Task) SetMode(mode int32) error {
	t.mu.Lock()

	// Accepted in any state (incl. ESTOP/OFF), matching C++ EMC_TASK_SET_MODE —
	// a UI mode selector must be honored even before the machine is enabled, so
	// the reported mode tracks the selector. The motion-mode calls below are
	// no-ops while motion is disabled. (Only the AUTO-running guard applies.)
	target := TaskMode(mode)

	// Reject mode switch while AUTO is running (matches C milltask behavior).
	if t.mode == ModeAuto && t.interpState != InterpIdle && target != ModeAuto {
		t.mu.Unlock()
		t.operatorError("Can't switch mode while mode is AUTO and interpreter is not IDLE")
		return ErrBusy
	}

	// Explicit mode switch cancels any transactional save.
	t.modeTx = false

	switch target {
	case ModeManual:
		if t.mode != ModeManual {
			t.abortLocked() // unlocks/re-locks internally for I/O
		}
		t.mode = ModeManual
		homed := t.allHomed()
		t.mu.Unlock()
		// A fully-homed machine defaults to teleop (axis) jog in manual, like
		// C++ emcTaskSetMode(MANUAL); otherwise free (joint) mode.
		if homed {
			return t.motion.SetTeleop()
		}
		return t.motion.SetFree()
	case ModeMDI:
		if t.mode != ModeMDI {
			t.abortLocked()
		}
		t.mode = ModeMDI
		t.mu.Unlock()
		_ = t.motion.SetCoord()
		if t.interp != nil {
			t.canon.syncEndPointFromMachine()
			_ = t.interp.Synch()
		}
		return nil
	case ModeAuto:
		if t.mode != ModeAuto {
			t.abortLocked()
		}
		t.mode = ModeAuto
		t.mu.Unlock()
		_ = t.motion.SetCoord()
		if t.interp != nil {
			t.canon.syncEndPointFromMachine()
			_ = t.interp.Synch()
		}
		return nil
	}
	t.mu.Unlock()
	return ErrWrongMode
}

// ProgramOpen opens a G-code file for execution.
func (t *Task) ProgramOpen(file string) error {
	t.mu.Lock()
	defer t.mu.Unlock()

	// Opening a file is allowed in any machine state (incl. ESTOP) and any
	// mode, but NOT while a program is running/paused: closing and reopening
	// t.interp here would race the runProgram goroutine's use of it. C++
	// likewise rejects PROGRAM_OPEN in the READING/PAUSED/WAITING states.
	if t.programBusy() {
		t.operatorError("Can't open a program while one is running")
		return ErrBusy
	}
	if t.interp != nil {
		// Close any previously open file before opening a new one.
		_ = t.interp.Close()
		if err := t.interp.Open(file); err != nil {
			t.operatorError(fmt.Sprintf("can't open %s", file))
			return err
		}
	}
	t.programFile = file
	t.programOpen = true
	t.previewSeq++
	return nil
}

// AutoCommand handles run/pause/resume/step/reverse in AUTO mode.
func (t *Task) AutoCommand(cmd int32, line int32) error {
	t.mu.Lock()

	if err := t.requireOn(); err != nil {
		t.mu.Unlock()
		return err
	}
	if err := t.ensureMode(ModeAuto); err != nil {
		t.mu.Unlock()
		return err
	}

	switch cmd {
	case AutoRun:
		if err := t.requireProgram(); err != nil {
			t.mu.Unlock()
			return err
		}
		if err := t.requireHomed(); err != nil {
			t.mu.Unlock()
			t.operatorError("Can't run a program when not homed")
			return fmt.Errorf("can't run program when not homed")
		}
		if t.externalOffsetApplied() {
			t.mu.Unlock()
			t.operatorError("Can't run a program with external offsets applied")
			return fmt.Errorf("can't run program with external offsets applied")
		}
		if t.interp == nil {
			t.mu.Unlock()
			return fmt.Errorf("no interpreter configured")
		}
		// Running a program is a deliberate mode commitment — no restore.
		t.modeTx = false
		t.interpState = InterpReading
		t.stepping = false
		interp := t.interp
		startLine := line
		file := t.programFile
		t.mu.Unlock()
		t.StartSequencer()
		// Ensure motion controller is in coord mode (may have been lost
		// after abort or error — matches C milltask emcTaskSetMode AUTO).
		_ = t.motion.SetCoord()
		// Re-open the program file to reset the interpreter to the beginning.
		// Without this, a second Run after completion or stop would fail with
		// "File ended with no percent sign" because the interpreter is at EOF.
		_ = interp.Close()
		if err := interp.Open(file); err != nil {
			t.setInterpState(InterpIdle)
			return fmt.Errorf("re-open program: %w", err)
		}
		t.canon.syncEndPointFromMachine()
		if err := interp.Synch(); err != nil {
			t.logger.Error("interp synch failed before run", "err", err)
		}
		go t.runProgram(interp, startLine)
		return nil

	case AutoPause:
		t.interpState = InterpPaused
		// Signal sequencer to pause
		if t.seqPauseCh != nil {
			select {
			case <-t.seqPauseCh:
			default:
				close(t.seqPauseCh)
			}
		}
		t.mu.Unlock()
		return t.motion.Pause()

	case AutoResume:
		t.interpState = InterpReading
		t.stepping = false
		// Wake sequencer from pause
		if t.seqResumeCh != nil {
			select {
			case <-t.seqResumeCh:
			default:
				close(t.seqResumeCh)
			}
		}
		t.mu.Unlock()
		return t.motion.Resume()

	case AutoStep:
		if err := t.requireProgram(); err != nil {
			t.mu.Unlock()
			return err
		}
		t.stepping = true

		if t.interpState == InterpIdle {
			// Program not started yet — start it in step mode.
			if err := t.requireHomed(); err != nil {
				t.stepping = false
				t.mu.Unlock()
				t.operatorError("Can't run a program when not homed")
				return fmt.Errorf("can't run program when not homed")
			}
			if t.interp == nil {
				t.stepping = false
				t.mu.Unlock()
				return fmt.Errorf("no interpreter configured")
			}
			t.interpState = InterpReading
			interp := t.interp
			file := t.programFile
			t.mu.Unlock()
			t.StartSequencer()
			_ = t.motion.SetCoord()
			_ = interp.Close()
			if err := interp.Open(file); err != nil {
				t.setInterpState(InterpIdle)
				return fmt.Errorf("re-open program: %w", err)
			}
			t.canon.syncEndPointFromMachine()
			if err := interp.Synch(); err != nil {
				t.logger.Error("interp synch failed before step", "err", err)
			}
			go t.runProgram(interp, line)
			return nil
		}

		// Program is paused — step one motion segment.
		if t.interpState == InterpPaused {
			t.mu.Unlock()

			// Check if TP has queued segments from the run phase.
			qd, _ := t.status.GetQueueDepth()
			if qd > 0 {
				// TP has moves — use low-level motion step to execute
				// one segment. The TP re-pauses after the ID changes.
				_ = t.motion.Step(0)
				return nil
			}

			// TP is empty — wake sequencer to push more commands.
			t.mu.Lock()
			resumeCh := t.seqResumeCh
			t.mu.Unlock()

			if resumeCh != nil {
				select {
				case <-resumeCh:
				default:
					close(resumeCh)
				}
			}
			return nil
		}

		// Already running — just enable stepping. Sequencer will pause
		// after the current motion cmd completes.
		t.mu.Unlock()
		return nil

	case AutoReverse:
		t.mu.Unlock()
		return t.motion.Reverse()
	}
	t.mu.Unlock()
	return nil
}

// MDI executes an MDI command string.
func (t *Task) MDI(command string) error {
	t.mu.Lock()

	if err := t.requireOn(); err != nil {
		t.mu.Unlock()
		return err
	}
	if err := t.ensureMode(ModeMDI); err != nil {
		t.mu.Unlock()
		t.operatorError("Must be in MDI mode to issue MDI command")
		return err
	}
	if err := t.requireHomed(); err != nil {
		t.mu.Unlock()
		t.operatorError("Can't issue MDI command when not homed")
		return fmt.Errorf("can't issue MDI command when not homed")
	}
	if t.interp == nil {
		t.mu.Unlock()
		return fmt.Errorf("no interpreter configured")
	}

	// If interpreter is busy, queue the command for later execution.
	if t.interpState != InterpIdle {
		if len(t.mdiQueue) >= t.maxMDIQueued {
			t.mu.Unlock()
			t.operatorError("MDI queue full")
			return ErrBusy
		}
		t.mdiQueue = append(t.mdiQueue, command)
		t.mu.Unlock()
		return nil
	}

	t.mu.Unlock()
	return t.executeMDI(command)
}

// executeMDI runs a single MDI command through the interpreter.
// Must be called with mu NOT held and interpState == InterpIdle.
func (t *Task) executeMDI(command string) error {
	t.mu.Lock()
	t.taskCommand = command // echoed to stat.task.command (C++ EMC_TASK_PLAN_EXECUTE)
	t.mu.Unlock()
	t.setInterpState(InterpReading)

	interp := t.interp

	// Set active canon for M-code callbacks (no ctx parameter).
	setActiveCanon(t.canon)

	// Sync canon endpoint from actual machine position before interp synch,
	// matching C canon's GET_EXTERNAL_POSITION which reads from STAT.
	t.canon.syncEndPointFromMachine()

	// Synch interpreter with current machine position before MDI.
	if err := interp.Synch(); err != nil {
		t.logger.Error("interp synch failed before MDI", "err", err)
	}

	// Execute the MDI string — this triggers canon callbacks that enqueue
	// motion commands to the sequencer.
	rc, err := interp.ExecuteString(command)
	t.updateActiveCodes(interp)
	if err != nil {
		t.setInterpState(InterpIdle)
		return fmt.Errorf("MDI execute: %w", err)
	}

	switch rc {
	case InterpError:
		t.setInterpState(InterpIdle)
		return fmt.Errorf("MDI interpreter error")
	default:
		// Mark exec state as busy so WaitComplete doesn't return prematurely.
		t.setExecState(ExecWaitingForMotion)
		// Wait for queued motion to finish before going idle.
		t.EnqueueCmd(&mdiDoneCmd{})
	}
	return nil
}

// faultProgram handles a fatal interpreter error while a program is running.
// It stops any in-flight motion and halts the sequencer so the read-ahead
// segments already queued past the erroring line are NOT dispatched, then
// leaves the task in ExecError. Without this the machine keeps moving through
// the queued motion after a G-code runtime error. Mirrors the C++
// interp_list.clear() + emcAbortCleanup(EMC_ABORT_INTERPRETER_ERROR). Runs in
// the runProgram goroutine.
func (t *Task) faultProgram(msg string) {
	t.operatorError(msg)
	_ = t.motion.Abort()   // stop what is already moving
	t.StartSequencer()     // aborts+drains the old sequencer, starts a fresh one
	t.setInterpState(InterpIdle)
	t.setExecState(ExecError) // set after StartSequencer so it is not clobbered
}

// runProgram runs the interpreter read/execute loop for the open program.
// Called in a goroutine from AutoRun/AutoStep.
//
// The interpreter is a "dumb producer": it reads lines, executes canon
// callbacks (which enqueue commands via EnqueueCmd), and only stops on
// abort or end-of-file. Pause and step are handled at the sequencer level.
func (t *Task) runProgram(interp Interpreter, startLine int32) {
	t.mu.Lock()
	t.interpActive = true
	t.mu.Unlock()
	defer func() {
		t.mu.Lock()
		t.interpActive = false
		t.mu.Unlock()
	}()

	// Set active canon for M-code callbacks (no ctx parameter).
	setActiveCanon(t.canon)
	defer clearActiveCanon()

	// Run-from-line: if startLine > 0, read/execute lines without enqueuing
	// motion commands until we reach startLine (matching C milltask behavior).
	if startLine > 0 {
		if aborted := t.seekToLine(interp, startLine); aborted {
			return
		}
	}

	for {
		// Check abort between interpreter lines
		select {
		case <-t.seqAbort:
			return
		default:
		}

		rc, err := interp.Read()
		if err != nil {
			t.logger.Error("interpreter read error", "err", err, "rc", rc)
			t.faultProgram(fmt.Sprintf("Interpreter read error: %v", err))
			return
		}
		if rc == InterpEndfile || rc == InterpExit {
			t.EnqueueCmd(&interpDoneCmd{})
			return
		}

		// Update readLine after successful read.
		t.mu.Lock()
		t.readLine = int32(interp.Line())
		t.mu.Unlock()

		startSerial := t.canon.serial()
		rc, err = interp.Execute()
		if err != nil {
			t.logger.Error("interpreter execute error", "err", err, "rc", rc)
			t.faultProgram(fmt.Sprintf("Interpreter error: %v", err))
			return
		}
		// Capture this line's active codes and tag the motion segments it just
		// queued, so status reports the executing segment's codes, not readahead.
		gc, mc, st := t.updateActiveCodes(interp)
		t.tagMotionRange(startSerial, t.canon.serial(), gc, mc, st)

		switch rc {
		case InterpExecuteFinish:
			// Interpreter says "wait for motion/IO to complete before
			// continuing" (tool change, probe, dwell, M-code, etc.)
			t.EnqueueCmd(waitForMotionSingleton)
			// Wait for sequencer to drain before reading next line
			if t.waitSequencerDrain() {
				return // aborted
			}
			t.canon.syncEndPointFromMachine()
			if err := interp.Synch(); err != nil {
				t.logger.Error("interp synch after execute_finish", "err", err)
			}
		case InterpExit:
			t.EnqueueCmd(&interpDoneCmd{})
			return
		case InterpError:
			t.logger.Error("interpreter error", "rc", rc)
			t.faultProgram("interpreter error")
			return
		case InterpOK:
			// Normal — continue reading
		}
	}
}

// seekToLine reads and executes interpreter lines without enqueuing motion
// commands (they are discarded) until we reach startLine. This implements
// "Run from line N": the interpreter processes preamble (offsets, units, tool
// changes) so it's in the correct state, but no actual motion is queued.
// Returns true if aborted.
func (t *Task) seekToLine(interp Interpreter, startLine int32) bool {
	// Temporarily redirect canon enqueue to a discard sink.
	t.canon.setDiscard(true)
	defer t.canon.setDiscard(false)

	for {
		select {
		case <-t.seqAbort:
			return true
		default:
		}

		rc, err := interp.Read()
		if err != nil {
			t.logger.Error("interpreter read error during seek", "err", err)
			t.setInterpState(InterpIdle)
			return true
		}
		if rc == InterpEndfile || rc == InterpExit {
			// Reached end before startLine — run from beginning
			t.logger.Warn("seekToLine: reached end before target line", "target", startLine)
			return true
		}

		lineNow := int32(interp.Line())

		rc, err = interp.Execute()
		if err != nil {
			t.logger.Error("interpreter execute error during seek", "err", err)
			t.setInterpState(InterpIdle)
			return true
		}
		t.updateActiveCodes(interp)

		// Handle EXECUTE_FINISH during seek (needed for tool changes etc.)
		if rc == InterpExecuteFinish {
			t.canon.syncEndPointFromMachine()
			if err := interp.Synch(); err != nil {
				t.logger.Error("interp synch during seek", "err", err)
			}
		}

		// Check if we've reached the target line.
		if lineNow >= startLine {
			// Sync interpreter position with actual machine position
			// so the first real move goes to the right place.
			t.canon.syncEndPointFromMachine()
			if err := interp.Synch(); err != nil {
				t.logger.Error("interp synch after seek", "err", err)
			}
			return false
		}
	}
}

// waitSequencerDrain waits until the sequencer queue is empty and the
// sequencer is idle (ExecDone). Returns true if aborted.
func (t *Task) waitSequencerDrain() bool {
	ticker := time.NewTicker(pollInterval)
	defer ticker.Stop()

	for {
		t.mu.Lock()
		qLen := len(t.interpQueue)
		exec := t.execState
		abort := t.seqAbort
		t.mu.Unlock()

		if qLen == 0 && exec == ExecDone {
			return false
		}

		select {
		case <-abort:
			return true
		case <-ticker.C:
		}
	}
}

// Jog handles continuous, incremental, and absolute jogs.
func (t *Task) Jog(jogType int32, jjogmode bool, axisOrJoint int32, velocity, distance float64) error {
	return t.jogInternal(jogType, jjogmode, axisOrJoint, velocity, distance, false)
}

// JogFromHAL is like Jog but marks the jog as HAL-pin-driven (no watchdog timeout).
func (t *Task) JogFromHAL(jogType int32, jjogmode bool, axisOrJoint int32, velocity, distance float64) error {
	return t.jogInternal(jogType, jjogmode, axisOrJoint, velocity, distance, true)
}

func (t *Task) jogInternal(jogType int32, jjogmode bool, axisOrJoint int32, velocity, distance float64, fromHAL bool) error {
	t.mu.Lock()
	defer t.mu.Unlock()

	if err := t.canJog(); err != nil {
		return err
	}

	isTeleop := int32(0)
	if !jjogmode {
		isTeleop = 1
	}

	// Ensure motion is in the correct mode for the requested jog type.
	// JOG_STOP never needs a mode switch — it just aborts whatever is running.
	if jogType != JogStop {
		if isTeleop == 1 {
			ms, err := t.status.GetStatus()
			if err == nil && ms.Teleop == 0 {
				t.mu.Unlock()
				ok := t.waitMotionTeleop()
				t.mu.Lock()
				if !ok {
					return fmt.Errorf("cannot jog: motion did not switch to teleop mode")
				}
			}
		} else {
			ms, err := t.status.GetStatus()
			if err == nil && (ms.Coord != 0 || ms.Teleop != 0) {
				_ = t.motion.SetFree()
				t.mu.Unlock()
				t.waitMotionFree()
				t.mu.Lock()
			}
		}
	}

	// Clamp velocity to joint/axis max (matches C milltask emcJogCont/Incr).
	velocity = t.clampJogVel(velocity, axisOrJoint, jjogmode)

	switch jogType {
	case JogStop:
		if axisOrJoint >= 0 && int(axisOrJoint) < len(t.activeJogs) {
			t.activeJogs[axisOrJoint].active = false
		}
		return t.motion.JogAbort(axisOrJoint, isTeleop)
	case JogContinuous:
		if axisOrJoint >= 0 && int(axisOrJoint) < len(t.activeJogs) {
			t.activeJogs[axisOrJoint] = activeJog{
				active:   true,
				isTeleop: isTeleop,
				fromHAL:  fromHAL,
				lastSeen: time.Now(),
			}
		}
		return t.motion.JogCont(axisOrJoint, velocity, isTeleop)
	case JogIncrement:
		return t.motion.JogIncr(axisOrJoint, velocity, distance, isTeleop)
	case JogAbsolute:
		// For absolute jog, `distance` carries the target position (C++ emcJogAbs
		// maps the message's distance onto emcmotCommand.offset).
		return t.motion.JogAbs(axisOrJoint, velocity, distance, isTeleop)
	}
	return nil
}

// clampJogVel clamps velocity to the joint or axis max velocity.
// Must be called with t.mu held.
func (t *Task) clampJogVel(vel float64, nr int32, jjogmode bool) float64 {
	var maxVel float64
	if jjogmode {
		if nr >= 0 && int(nr) < len(t.jointMaxVel) {
			maxVel = t.jointMaxVel[nr]
		}
	} else {
		if nr >= 0 && int(nr) < len(t.axisMaxVel) {
			maxVel = t.axisMaxVel[nr]
		}
	}
	if maxVel <= 0 {
		return vel
	}
	if vel > maxVel {
		return maxVel
	} else if vel < -maxVel {
		return -maxVel
	}
	return vel
}

// JogStop stops a jog on the specified axis/joint.
func (t *Task) JogStop(jjogmode bool, axisOrJoint int32) error {
	t.mu.Lock()
	defer t.mu.Unlock()

	if axisOrJoint >= 0 && int(axisOrJoint) < len(t.activeJogs) {
		t.activeJogs[axisOrJoint].active = false
	}

	isTeleop := int32(0)
	if !jjogmode {
		isTeleop = 1
	}
	return t.motion.JogAbort(axisOrJoint, isTeleop)
}

// Spindle controls spindle on/off/direction/increase/decrease.
func (t *Task) Spindle(cmd int32, speed float64, spindleNum, wait int32) error {
	t.mu.Lock()
	defer t.mu.Unlock()

	if err := t.requireOn(); err != nil {
		return err
	}

	// The program owns the spindle while it is running: reject manual on/off
	// from a UI/halui during an AUTO run, matching C++ (only spindle-override
	// increase/decrease/constant are accepted while READING). Manual control is
	// allowed when idle or paused.
	switch cmd {
	case SpindleForward:
		if t.interpRunning() {
			return t.spindleRunErr()
		}
		return t.motion.SpindleOn(spindleNum, speed, 0, 0, wait)
	case SpindleReverse:
		if t.interpRunning() {
			return t.spindleRunErr()
		}
		return t.motion.SpindleOn(spindleNum, -speed, 0, 0, wait)
	case SpindleOff:
		if t.interpRunning() {
			return t.spindleRunErr()
		}
		return t.motion.SpindleOff(spindleNum)
	case SpindleIncrease:
		return t.motion.SpindleIncrease(spindleNum) // override, allowed while running
	case SpindleDecrease:
		return t.motion.SpindleDecrease(spindleNum) // override, allowed while running
	}
	return nil
}

// interpRunning reports whether an AUTO program is actively executing — not
// idle and not paused — i.e. the interpreter owns the machine. Must be called
// with t.mu held.
func (t *Task) interpRunning() bool {
	return t.mode == ModeAuto && t.interpState != InterpIdle && t.interpState != InterpPaused
}

// programBusy reports whether a program or MDI command is loaded and mid-run
// (executing or paused) — i.e. the interpreter/sequencer owns the machine.
// Unlike interpRunning it also covers MDI execution and the paused state, so it
// is the right guard for operations that would corrupt an in-flight run. Stays
// false when idle/estop/off/manual, so it does not re-break the any-state
// operations (e.g. unhome in estop). Must be called with t.mu held.
func (t *Task) programBusy() bool {
	return t.interpState != InterpIdle || t.interpActive
}

// spindleRunErr rejects a manual spindle/brake command issued during a run.
func (t *Task) spindleRunErr() error {
	t.operatorError("Can't control the spindle manually while a program is running")
	return ErrBusy
}

// Home initiates homing for the specified joint.
func (t *Task) Home(joint int32) error {
	t.mu.Lock()
	defer t.mu.Unlock()

	if err := t.requireOn(); err != nil {
		return err
	}
	if err := t.ensureMode(ModeManual); err != nil {
		return err
	}
	// Home requires joint (FREE) mode — motion may be in teleop even when
	// task mode is already ModeManual.
	t.mu.Unlock()
	_ = t.motion.SetFree()
	t.waitMotionFree()
	t.mu.Lock()
	return t.motion.JointHome(joint)
}

// Unhome un-homes the specified joint.
func (t *Task) Unhome(joint int32) error {
	// Rejected only while a program or MDI command is mid-run (or paused):
	// clearing a joint's home reference while the interpreter is driving motion
	// would corrupt the run (and, mid-motion, the position reference). Motion's
	// per-joint guard only catches free-mode jog/homing, not coordinated moves,
	// so this guard belongs here.
	t.mu.Lock()
	busy := t.programBusy()
	t.mu.Unlock()
	if busy {
		t.operatorError("Can't unhome while a program is running")
		return ErrBusy
	}
	// Otherwise unhoming is allowed in ANY state (including ESTOP and OFF) and
	// any mode, matching C++ which accepts EMC_JOINT_UNHOME broadly — you often
	// need to clear a homed joint precisely when the machine is in estop. Motion
	// clears the homed flag and forces FREE mode internally, so no requireOn /
	// mode switch is needed (and requireOn would wrongly reject it in estop/off).
	return t.motion.JointUnhome(joint)
}

// OverrideLimits temporarily overrides soft limits for homing.
// OverrideLimits overrides tripped soft limits for a joint so it can be jogged
// off the limit. joint < 0 resumes normal limit checking (mirrors C++
// emcJointOverrideLimits / EMCMOT_OVERRIDE_LIMITS). Manual-mode only, matching
// C++ (EMC_JOINT_OVERRIDE_LIMITS is issued only when task.mode == MANUAL).
func (t *Task) OverrideLimits(joint int32) error {
	t.mu.Lock()
	defer t.mu.Unlock()

	if err := t.requireOn(); err != nil {
		return err
	}
	if t.mode != ModeManual {
		return fmt.Errorf("cannot override limits: not in manual mode")
	}
	return t.motion.OverrideLimits(joint)
}

// TeleopEnable enables/disables teleop mode.
// TeleopEnable is accepted in any state (matches C++ TRAJ_SET_TELEOP_ENABLE in
// OFF/ESTOP); it just sets the motion mode.
func (t *Task) TeleopEnable(enable bool) error {
	if enable {
		return t.motion.SetTeleop()
	}
	return t.motion.SetFree()
}

// SetFeedOverride sets the feed override percentage.
// Override setters are accepted in ANY state (including ESTOP/OFF), matching
// C++ — the scale is stored in the motion controller and takes effect when the
// machine runs, so a UI can set the sliders before enabling. No requireOn.
func (t *Task) SetFeedOverride(rate float64) error {
	return t.motion.SetFeedScale(rate)
}

// SetSpindleOverride sets spindle speed override.
func (t *Task) SetSpindleOverride(rate float64, spindleNum int32) error {
	return t.motion.SetSpindleScale(spindleNum, rate)
}

// SetRapidOverride sets the rapid override percentage.
func (t *Task) SetRapidOverride(rate float64) error {
	return t.motion.SetRapidScale(rate)
}

// SetMaxVelocity sets the maximum trajectory velocity.
func (t *Task) SetMaxVelocity(velocity float64) error {
	return t.motion.SetVelLimit(velocity)
}

// Flood turns flood coolant on or off.
func (t *Task) Flood(on bool) error {
	t.mu.Lock()
	defer t.mu.Unlock()

	if err := t.requireOn(); err != nil {
		return err
	}
	if on {
		if err := t.io.CoolantFloodOn(); err != nil {
			return err
		}
		t.floodOn = true
		return nil
	}
	if err := t.io.CoolantFloodOff(); err != nil {
		return err
	}
	t.floodOn = false
	return nil
}

// Mist turns mist coolant on or off.
func (t *Task) Mist(on bool) error {
	t.mu.Lock()
	defer t.mu.Unlock()

	if err := t.requireOn(); err != nil {
		return err
	}
	if on {
		if err := t.io.CoolantMistOn(); err != nil {
			return err
		}
		t.mistOn = true
		return nil
	}
	if err := t.io.CoolantMistOff(); err != nil {
		return err
	}
	t.mistOn = false
	return nil
}

// Brake engages/disengages spindle brake.
func (t *Task) Brake(on bool, spindleNum int32) error {
	t.mu.Lock()
	defer t.mu.Unlock()

	if err := t.requireOn(); err != nil {
		return err
	}
	if t.interpRunning() {
		return t.spindleRunErr() // program owns the spindle while running
	}
	if on {
		return t.motion.SpindleBrakeEngage(spindleNum)
	}
	return t.motion.SpindleBrakeRelease(spindleNum)
}

// Lube turns lubrication on or off.
func (t *Task) Lube(on bool) error {
	t.mu.Lock()
	defer t.mu.Unlock()

	if err := t.requireOn(); err != nil {
		return err
	}
	if on {
		if err := t.io.LubeOn(); err != nil {
			return err
		}
		t.lubeOn = true
		return nil
	}
	if err := t.io.LubeOff(); err != nil {
		return err
	}
	t.lubeOn = false
	return nil
}

// Abort aborts all motion and interpreter execution.
// Matches C milltask emcTaskAbort behavior: abort motion, abort IO,
// stop spindles, turn off coolant, clear interpreter queue, close program.
func (t *Task) Abort() error {
	t.mu.Lock()
	t.abortLocked()
	t.mu.Unlock()
	return nil
}

// abortLocked performs the full abort sequence.
// Caller MUST hold t.mu on entry; t.mu is held on return.
// Internally unlocks t.mu for external I/O calls to avoid blocking stat reads.
func (t *Task) abortLocked() {
	t.interpState = InterpIdle
	t.execState = ExecDone
	t.mdiQueue = t.mdiQueue[:0]
	t.stepping = false
	t.readLine = 0
	t.currentLine = 0

	numSpindles := t.numSpindles
	interp := t.interp
	t.mu.Unlock()

	// External calls (no mutex held — won't block stat reads).
	t.AbortSequencer()
	t.mcodeAbort()
	_ = t.motion.Abort()
	_ = t.io.IoAbort(emcAbortTaskAbort)
	for i := 0; i < numSpindles; i++ {
		_ = t.motion.SpindleOff(int32(i))
	}
	_ = t.io.CoolantFloodOff()
	_ = t.io.CoolantMistOff()

	if interp != nil {
		_ = interp.Abort(0, "user abort")
		_ = interp.Close()
		_ = interp.Reset()
		_ = interp.Synch()
	}

	t.StartSequencer()

	t.mu.Lock()
	t.floodOn = false
	t.mistOn = false
}

// TaskPlanSynch forces a sync between interpreter and motion.
func (t *Task) TaskPlanSynch() error {
	t.mu.Lock()
	defer t.mu.Unlock()

	if t.interp == nil {
		return nil
	}
	t.canon.syncEndPointFromMachine()
	return t.interp.Synch()
}

// SetOptionalStop enables/disables optional stop (M1).
func (t *Task) SetOptionalStop(on bool) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.optionalStop = on
	return nil
}

// SetBlockDelete enables/disables block delete (/).
func (t *Task) SetBlockDelete(on bool) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.blockDelete = on
	return nil
}

// LoadToolTable reloads the tool table from file.
// LoadToolTable reloads the tool table. An empty file uses the default table
// (matches C++ emcToolLoadToolTable, which accepts the requested filename).
func (t *Task) LoadToolTable(file string) error {
	err := t.io.ToolLoadTable(file)
	if err == nil {
		// Synch interpreter so it re-reads tool_table[] from the
		// tooltable module via GET_EXTERNAL_TOOL_TABLE callbacks.
		if t.interp != nil {
			_ = t.interp.Synch()
		}
		t.mu.Lock()
		t.previewSeq++
		t.mu.Unlock()
	}
	return err
}

// WaitComplete waits for motion to complete (with timeout).
func (t *Task) WaitComplete(timeout float64) error {
	deadline := time.Now().Add(time.Duration(timeout * float64(time.Second)))
	ticker := time.NewTicker(pollInterval)
	defer ticker.Stop()

	for {
		t.mu.Lock()
		exec := t.execState
		t.mu.Unlock()
		if exec == ExecDone || exec == ExecError {
			return nil
		}
		if timeout > 0 && time.Now().After(deadline) {
			return fmt.Errorf("WaitComplete: timeout after %.1fs", timeout)
		}
		<-ticker.C
	}
}

// SetDebug sets the debug level.
func (t *Task) SetDebug(debug int32) error {
	t.mu.Lock()
	defer t.mu.Unlock()

	return t.motion.SetDebug(debug)
}

// SetJogAxis sets the selected jog axis (0=X .. 8=W, -1=none).
func (t *Task) SetJogAxis(axis int32) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	if axis < -1 || axis >= int32(maxAxes) {
		return fmt.Errorf("SetJogAxis: invalid axis %d", axis)
	}
	t.jogAxis = axis
	return nil
}

// SetJogIncrement sets the jog increment distance (0 = continuous).
func (t *Task) SetJogIncrement(increment float64) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	if increment < 0 {
		return fmt.Errorf("SetJogIncrement: invalid increment %f", increment)
	}
	t.jogIncrement = increment
	return nil
}

// SetJogSpeed sets the linear jog speed (units/sec).
func (t *Task) SetJogSpeed(speed float64) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	if speed < 0 {
		return fmt.Errorf("SetJogSpeed: invalid speed %f", speed)
	}
	t.jogSpeed = speed
	return nil
}

// SetAjogSpeed sets the angular jog speed (deg/sec).
func (t *Task) SetAjogSpeed(speed float64) error {
	t.mu.Lock()
	defer t.mu.Unlock()
	if speed < 0 {
		return fmt.Errorf("SetAjogSpeed: invalid speed %f", speed)
	}
	t.ajogSpeed = speed
	return nil
}
