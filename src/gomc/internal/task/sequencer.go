// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"context"
	"errors"
	"fmt"
	"time"
)

// WaitType describes what a queued command waits for after execution.
type WaitType int

const (
	WaitNone            WaitType = iota
	WaitMotion                   // wait for motion queue to drain
	WaitIO                       // wait for IO acknowledgment
	WaitMotionAndIO              // wait for both
	WaitDelay                    // timed dwell
	WaitSpindleOriented          // wait for orient complete
)

// QueuedCmd is one interpreter-generated action queued for sequential execution.
type QueuedCmd interface {
	// Execute performs the command (e.g. calls motctl.SetLine).
	Execute(t *Task) error
	// Wait returns what to wait for after Execute completes.
	Wait() WaitType
	// String returns a description for logging/debugging.
	String() string
}

// SeqError is returned when the sequencer encounters an error.
type SeqError struct {
	Cmd QueuedCmd
	Err error
}

func (e *SeqError) Error() string {
	return fmt.Sprintf("sequencer: %s: %v", e.Cmd, e.Err)
}

func (e *SeqError) Unwrap() error { return e.Err }

const interpQueueSize = 64

// closeOnceLocked closes ch if it is non-nil and not already closed. The caller
// must hold t.mu, which serializes this idempotent check-then-close against the
// other closers and against StartSequencer reassigning the channel field.
// Getting this idiom wrong is a double-close panic, so it lives in one place.
func (t *Task) closeOnceLocked(ch chan struct{}) {
	if ch == nil {
		return
	}
	select {
	case <-ch:
	default:
		close(ch)
	}
}

// closeOnce is closeOnceLocked for callers that hold a channel value but not
// t.mu (e.g. a channel captured under mu earlier, then closed after unlocking).
func (t *Task) closeOnce(ch chan struct{}) {
	t.mu.Lock()
	defer t.mu.Unlock()
	t.closeOnceLocked(ch)
}

// StartSequencer launches the sequencer goroutine. Must be called with mu NOT held.
// If a previous sequencer goroutine is still winding down, StartSequencer waits
// for it to exit before creating new channels. This prevents a race where the old
// goroutine could pick up the new (non-closed) seqAbort channel and keep running.
//
// seqLifeMu serializes concurrent generation changes: without it, two callers
// (e.g. a monitor fault teardown racing faultProgram or a user abort) could
// both wait on the same old seqDone and then both spawn a loop — two consumers
// on one queue plus a double-close panic on seqDone.
func (t *Task) StartSequencer() {
	t.seqLifeMu.Lock()
	defer t.seqLifeMu.Unlock()

	t.mu.Lock()
	oldDone := t.seqDone
	oldAbort := t.seqAbort
	// Abort the previous sequencer under t.mu so this check-then-close is atomic
	// against sequencerLoop's own abort-close (also under t.mu) — otherwise both
	// could close the same channel and panic.
	t.closeOnceLocked(oldAbort)
	t.mu.Unlock()

	// Wait for previous goroutine to finish (must NOT hold t.mu — the goroutine
	// may need it to exit).
	if oldDone != nil {
		<-oldDone
	}

	t.mu.Lock()
	t.interpQueue = make(chan QueuedCmd, interpQueueSize)
	t.seqDone = make(chan struct{})
	t.seqAbort = make(chan struct{})
	t.seqPauseCh = make(chan struct{})
	t.seqResumeCh = make(chan struct{})
	t.mu.Unlock()

	go t.sequencerLoop()
}

// StopSequencer aborts and waits for the sequencer goroutine to exit.
func (t *Task) StopSequencer() {
	t.seqLifeMu.Lock()
	defer t.seqLifeMu.Unlock()

	t.mu.Lock()
	abort := t.seqAbort
	done := t.seqDone
	// Close under t.mu (atomic check-then-close vs sequencerLoop / other closers).
	t.closeOnceLocked(abort)
	t.mu.Unlock()

	if abort == nil {
		return
	}
	// Wait for goroutine to finish (lock released — it may need t.mu to exit).
	if done != nil {
		<-done
	}
}

// AbortSequencer signals the sequencer to stop and drain.
// Unlike StopSequencer, it does not wait for exit.
func (t *Task) AbortSequencer() {
	t.mu.Lock()
	abort := t.seqAbort
	q := t.interpQueue
	// Close under t.mu (atomic check-then-close vs sequencerLoop / other closers).
	t.closeOnceLocked(abort)
	t.mu.Unlock()

	if abort == nil {
		return
	}

	// Drain any pending commands
	if q != nil {
		for {
			select {
			case <-q:
			default:
				return
			}
		}
	}
}

// EnqueueCmd pushes a command to the interpreter queue.
// Blocks if the queue is full (backpressure from paused sequencer).
// Unblocks on abort only.
func (t *Task) EnqueueCmd(cmd QueuedCmd) error {
	t.mu.Lock()
	q := t.interpQueue
	abort := t.seqAbort
	t.mu.Unlock()

	if q == nil {
		return fmt.Errorf("sequencer not running")
	}

	t.logger.Debug("enqueue", "cmd", cmd.String())
	select {
	case <-abort:
		return fmt.Errorf("sequencer aborted")
	case q <- cmd:
		return nil
	}
}

// sequencerLoop is the main execution loop. It reads commands from interpQueue
// and executes them sequentially, waiting as required between commands.
// Pause and step are handled at this level.
func (t *Task) sequencerLoop() {
	defer close(t.seqDone)
	defer t.setSeqInflight(false)

	for {
		// Check if pause was requested before reading next command
		if t.seqCheckPause() {
			return // aborted
		}

		select {
		case <-t.seqAbort:
			// Externally aborted: write NO state here. The aborter owns the
			// terminal execState/interpState and commits it after joining
			// this goroutine (via StartSequencer) — a free-running write at
			// exit is exactly the clobber that forced every teardown to
			// latch its terminal state after the join.
			return

		case cmd, ok := <-t.interpQueue:
			if !ok {
				// Channel closed — normal shutdown
				t.setExecState(ExecDone)
				t.setInterpState(InterpIdle)
				return
			}

			t.logger.Debug("sequencer exec", "cmd", cmd.String())

			// Mark a command in flight so waitSequencerDrain cannot observe
			// the empty-queue/ExecDone gap between dequeue and execution.
			t.setSeqInflight(true)

			// Update currentLine from motion commands that carry a line ID.
			if lc, ok := cmd.(interface{ LineID() int32 }); ok {
				if id := lc.LineID(); id > 0 {
					t.mu.Lock()
					if info, ok := t.motionMap[id]; ok {
						t.currentLine = info.LineNo
					}
					t.mu.Unlock()
				}
			}

			// Motion-drain precondition (D7): a command that must act *at* a
			// point (dwell, probe, rigid tap, spindle change, coolant, orient)
			// declares Precondition()==WaitMotion, and the sequencer drains the
			// preceding motion before executing it — centralizing what the
			// scattered canon-side syncBefore() barriers used to do per op. An
			// already-drained queue returns immediately (E1 fast path).
			if pc, ok := cmd.(interface{ Precondition() WaitType }); ok && pc.Precondition() == WaitMotion {
				if err := t.waitMotionDone(); err != nil {
					return // aborted — aborter owns the terminal state
				}
			}

			// Execute the command, retrying motion commands on queue-full errors.
			const maxMotionRetries = 1000 // ~10s at 10ms poll interval
			retries := 0
			for {
				// For motion commands, wait for TP queue space before sending.
				// This prevents overflowing the 2000-entry TP queue which the
				// C motion controller treats as a fatal abort.
				switch cmd.(type) {
				case *LinearMoveCmd, *CircularMoveCmd, *RigidTapCmd:
					// Mark motion dispatched so the next drain applies its servo
					// settle skip (empty barriers stay on the fast path — E1).
					t.motionDispatched.Store(true)
					if err := t.waitForQueueSpace(); err != nil {
						return // aborted — aborter owns the terminal state
					}
				}

				err := cmd.Execute(t)
				if err == nil {
					break
				}
				if errors.Is(err, context.Canceled) {
					return // aborted — aborter owns the terminal state
				}
				switch cmd.(type) {
				case *LinearMoveCmd, *CircularMoveCmd, *RigidTapCmd, *SetMotionParamsCmd:
					// Distinguish genuine TP-queue-full backpressure from a hard
					// motion fault. waitForQueueSpace already gates sends on the
					// high-water mark, and the motion command's error is a
					// cmd_status rejection (bad params / can't-handle-now), NOT a
					// "retry later" signal. So retry ONLY while the queue is
					// actually full; otherwise the send failed with space
					// available => a real fault, surfaced immediately instead of
					// spinning ~10s (fail-fast on hard faults).
					qFull := false
					if t.status != nil {
						if qd, qerr := t.status.GetQueueDepth(); qerr == nil && qd >= tpQueueHighWater {
							qFull = true
						}
					}
					if qFull && retries < maxMotionRetries {
						retries++
						if retries == 1 {
							t.logger.Info("sequencer: TP full, retrying", "cmd", cmd.String())
						}
						// Wait for TP to drain one slot, then retry.
						select {
						case <-t.seqAbort:
							return // aborted — aborter owns the terminal state
						case <-time.After(pollInterval):
							continue
						}
					}
					// Hard fault (queue had space) or backpressure budget
					// exhausted — surface immediately.
					if qFull {
						t.logger.Error("sequencer motion cmd failed after retries", "cmd", cmd.String(), "err", err)
					} else {
						t.logger.Error("sequencer motion cmd failed", "cmd", cmd.String(), "err", err)
					}
					t.operatorError(fmt.Sprintf("Motion command failed: %s", err))
					t.setExecState(ExecError)
					t.setInterpState(InterpIdle)
					t.mu.Lock()
					t.closeOnceLocked(t.seqAbort)
					t.mu.Unlock()
					return
				default:
					// Non-motion command failed — check if it was due to abort
					select {
					case <-t.seqAbort:
						// Abort was requested — command failure is expected, not an
						// error; the aborter owns the terminal state.
						t.logger.Info("sequencer command aborted", "cmd", cmd.String())
						return
					default:
					}
					t.logger.Error("sequencer command failed", "cmd", cmd.String(), "err", err)
					t.operatorError(fmt.Sprintf("Command failed: %s: %s", cmd.String(), err))
					_ = t.motion.Abort() // stop in-flight motion on a command fault (e.g. failed M1xx)
					t.setExecState(ExecError)
					t.setInterpState(InterpIdle)
					// Signal abort so interpreter goroutine unblocks from EnqueueCmd
					t.mu.Lock()
					t.closeOnceLocked(t.seqAbort)
					t.mu.Unlock()
					return
				}
			}

			// Wait as required
			if err := t.waitForCompletion(cmd); err != nil {
				if errors.Is(err, context.Canceled) {
					// Abort — not an error condition; aborter owns terminal state.
					t.logger.Info("sequencer aborted during wait", "cmd", cmd.String())
					return
				}
				t.logger.Error("sequencer wait failed", "cmd", cmd.String(), "err", err)
				t.operatorError(fmt.Sprintf("Wait failed: %s: %s", cmd.String(), err))
				t.setExecState(ExecError)
				t.setInterpState(InterpIdle)
				// Signal abort like the other error exits — otherwise the
				// producer can block forever in EnqueueCmd/waitSequencerDrain
				// with no consumer left.
				t.mu.Lock()
				t.closeOnceLocked(t.seqAbort)
				t.mu.Unlock()
				return
			}

			// Post-wait hook for commands that need state changes after motion completes
			if pw, ok := cmd.(interface{ PostWait(*Task) }); ok {
				pw.PostWait(t)
			}

			// In step mode: after a motion-producing command, wait for
			// motion to complete and then enter pause.
			if t.isSeqStepping() && isMotionCmd(cmd) {
				if err := t.waitMotionDone(); err != nil {
					return // aborted — aborter owns the terminal state
				}
				t.seqEnterPause()
				if t.seqCheckPause() {
					return // aborted while paused
				}
			}

			t.setSeqInflight(false)
		}
	}
}

// setSeqInflight marks whether the sequencer is processing a dequeued command.
func (t *Task) setSeqInflight(v bool) {
	t.seqInflight.Store(v)
}

// isMotionCmd returns true for commands that produce TP motion segments.
func isMotionCmd(cmd QueuedCmd) bool {
	switch cmd.(type) {
	case *LinearMoveCmd, *CircularMoveCmd, *RigidTapCmd:
		return true
	}
	return false
}

// isSeqStepping returns the current stepping flag (thread-safe).
func (t *Task) isSeqStepping() bool {
	t.mu.Lock()
	defer t.mu.Unlock()
	return t.stepping
}

// seqEnterPause sets the sequencer into paused state and prepares
// for the next seqCheckPause call to block.
func (t *Task) seqEnterPause() {
	t.setInterpState(InterpPaused)
	t.mu.Lock()
	// Close seqPauseCh so the next seqCheckPause detects paused state
	t.closeOnceLocked(t.seqPauseCh)
	t.mu.Unlock()
}

// seqCheckPause checks if the sequencer should pause. If seqPauseCh is
// closed, blocks until seqResumeCh is closed (resume or step) or abort.
// Returns true if aborted.
func (t *Task) seqCheckPause() bool {
	t.mu.Lock()
	pauseCh := t.seqPauseCh
	abort := t.seqAbort
	t.mu.Unlock()

	select {
	case <-abort:
		return true // aborted — aborter owns the terminal state
	case <-pauseCh:
		// Paused — block until resumed or aborted
		t.setInterpState(InterpPaused)
		t.mu.Lock()
		resumeCh := t.seqResumeCh
		t.mu.Unlock()
		select {
		case <-abort:
			return true // aborted — aborter owns the terminal state
		case <-resumeCh:
			t.setInterpState(InterpReading)
			// Allocate fresh channels for next pause/resume cycle
			t.mu.Lock()
			t.seqPauseCh = make(chan struct{})
			t.seqResumeCh = make(chan struct{})
			t.mu.Unlock()
			return false
		}
	default:
		return false
	}
}

// waitForCompletion blocks until the specified condition is met or abort.
func (t *Task) waitForCompletion(cmd QueuedCmd) error {
	switch cmd.Wait() {
	case WaitNone:
		return nil

	case WaitMotion:
		return t.waitMotionDone()

	case WaitIO:
		return t.waitIODone()

	case WaitMotionAndIO:
		if err := t.waitMotionDone(); err != nil {
			return err
		}
		return t.waitIODone()

	case WaitDelay:
		// Delay is handled by DwellCmd itself
		return nil

	case WaitSpindleOriented:
		// Carry the M19 orient timeout (seconds) through to the wait.
		var timeout float64
		if c, ok := cmd.(*WaitSpindleOrientedCmd); ok {
			timeout = c.Timeout
		}
		return t.waitSpindleOriented(timeout)
	}
	return nil
}

// waitForQueueSpace polls motion queue depth and blocks until it drops below
// the high-water mark. This prevents overflowing the TP queue (which the C
// motion controller treats as a fatal abort). Returns context.Canceled on abort.
func (t *Task) waitForQueueSpace() error {
	if t.status == nil {
		return nil
	}
	qd, err := t.status.GetQueueDepth()
	if err != nil || qd < tpQueueHighWater {
		return nil // can't read or plenty of space — proceed
	}

	t.logger.Debug("waiting for TP queue space", "depth", qd)
	ticker := time.NewTicker(pollInterval)
	defer ticker.Stop()
	for {
		select {
		case <-t.seqAbort:
			return context.Canceled
		case <-ticker.C:
			qd, err = t.status.GetQueueDepth()
			if err != nil {
				continue // transient read error, keep waiting
			}
			if qd < tpQueueHighWater {
				return nil
			}
		}
	}
}

// motionDrained reports whether the machine is in position with an empty TP
// queue right now (no wait). Used by waitMotionDone's fast path.
func (t *Task) motionDrained() bool {
	if t.status == nil {
		return true
	}
	v, err := t.status.GetInpos()
	if err != nil || v != 1 {
		return false
	}
	qd, err := t.status.GetQueueDepth()
	return err == nil && qd == 0
}

// waitMotionDone polls motion status until in-position and queue empty, or abort.
// A communication watchdog detects if the motion controller stops responding.
func (t *Task) waitMotionDone() error {
	t.setExecState(ExecWaitingForMotion)
	ticker := time.NewTicker(pollInterval)
	defer ticker.Stop()

	if t.motionDispatched.Load() {
		// Motion was dispatched since the last drain — skip a few poll intervals
		// so the servo thread reflects it before the first inpos check (avoids a
		// stale-inPos race after a rapid queue fill).
		for i := 0; i < 5; i++ {
			select {
			case <-t.seqAbort:
				return context.Canceled
			case <-ticker.C:
			}
		}
	} else if t.motionDrained() {
		// No motion since the last drain and already drained — a back-to-back
		// barrier (e.g. S1200 M3 M8, each self-barriering) returns immediately
		// instead of paying the settle floor.
		t.setExecState(ExecDone)
		return nil
	}

	commErrors := 0
	for {
		select {
		case <-t.seqAbort:
			return context.Canceled
		case <-ticker.C:
			if t.status == nil {
				t.setExecState(ExecDone)
				return nil
			}
			v, err := t.status.GetInpos()
			if err != nil {
				commErrors++
				if commErrors >= commFailureThreshold {
					t.logger.Error("waitMotionDone: motion controller not responding")
					t.operatorError("Motion controller not responding")
					t.setExecState(ExecError)
					return fmt.Errorf("waitMotionDone: comm failure (%d consecutive errors)", commErrors)
				}
				continue
			}
			commErrors = 0 // valid response resets counter
			if v != 1 {
				continue
			}
			qd, err := t.status.GetQueueDepth()
			if err == nil && qd == 0 {
				t.motionDispatched.Store(false) // queue drained
				t.setExecState(ExecDone)
				return nil
			}
		}
	}
}

// waitIODone polls until IO command status is DONE/ERROR, abort, or comm failure.
func (t *Task) waitIODone() error {
	t.setExecState(ExecWaitingForIO)
	commErrors := 0
	ticker := time.NewTicker(pollInterval)
	defer ticker.Stop()

	for {
		select {
		case <-t.seqAbort:
			return context.Canceled
		case <-ticker.C:
			if t.io == nil {
				t.setExecState(ExecDone)
				return nil
			}
			st, err := t.io.GetCmdStatus()
			if err != nil {
				commErrors++
				if commErrors >= commFailureThreshold {
					t.logger.Error("waitIODone: IO controller not responding")
					t.operatorError("IO controller not responding")
					t.setExecState(ExecError)
					return fmt.Errorf("waitIODone: comm failure (%d consecutive errors)", commErrors)
				}
				continue
			}
			commErrors = 0
			if st == IOStatusDone || st == IOStatusError {
				t.setExecState(ExecDone)
				return nil
			}
		}
	}
}

// waitSpindleOriented polls until spindle orient is complete, faults, or times
// out. A spindle that never reports oriented (and never faults) must not hang
// the run forever — matching C++ EMC_TASK_EXEC_WAITING_FOR_SPINDLE_ORIENTED,
// which errors with "wait for orient complete: TIMED OUT" once the M19 timeout
// (seconds) elapses (emctaskmain.cc:2711). A non-positive timeout waits
// indefinitely (only abort or fault ends it); a nil timer channel is never
// ready in the select, so that case simply never fires.
func (t *Task) waitSpindleOriented(timeout float64) error {
	t.setExecState(ExecWaitingForSpindleOriented)
	ticker := time.NewTicker(pollInterval)
	defer ticker.Stop()

	var deadline <-chan time.Time
	if timeout > 0 {
		timer := time.NewTimer(time.Duration(timeout * float64(time.Second)))
		defer timer.Stop()
		deadline = timer.C
	}
	commErrors := 0

	for {
		select {
		case <-t.seqAbort:
			return context.Canceled
		case <-deadline:
			t.setExecState(ExecError)
			return fmt.Errorf("orient timed out after %gs", timeout)
		case <-ticker.C:
			if t.status == nil {
				t.setExecState(ExecDone)
				return nil
			}
			ms, err := t.status.GetStatus()
			if err != nil {
				commErrors++
				if commErrors >= commFailureThreshold {
					t.logger.Error("waitSpindleOriented: motion controller not responding")
					t.setExecState(ExecError)
					return fmt.Errorf("waitSpindleOriented: comm failure")
				}
				continue
			}
			commErrors = 0
			// OrientState: 0=idle/complete, 1=in progress, 2=fault
			// Check all spindles (the orient command targets one, but status is per-spindle)
			for i := range ms.Spindles {
				if ms.Spindles[i].OrientState == 2 {
					t.setExecState(ExecError)
					return fmt.Errorf("spindle orient fault")
				}
			}
			// All spindles either idle or none in-progress means done
			allDone := true
			for i := range ms.Spindles {
				if ms.Spindles[i].OrientState == 1 {
					allDone = false
					break
				}
			}
			if allDone {
				t.setExecState(ExecDone)
				return nil
			}
		}
	}
}

// pollUntil polls the condition at servo rate until true, abort, or comm failure.
// The condition function should return (done, commOK). If commOK is false for
// too many consecutive polls, a comm failure is declared.
func (t *Task) pollUntil(cond func() bool) error {
	ticker := time.NewTicker(pollInterval)
	defer ticker.Stop()

	for {
		select {
		case <-t.seqAbort:
			return context.Canceled
		case <-ticker.C:
			if cond() {
				t.setExecState(ExecDone)
				return nil
			}
		}
	}
}

// setExecState sets the execution state (thread-safe).
func (t *Task) setExecState(s ExecState) {
	t.mu.Lock()
	t.execState = s
	t.mu.Unlock()
}

// setInterpState sets the interpreter state (thread-safe).
func (t *Task) setInterpState(s InterpState) {
	t.mu.Lock()
	t.interpState = s
	t.mu.Unlock()
}

// --- Concrete QueuedCmd types ---

// LinearMoveCmd queues a linear motion segment.
type LinearMoveCmd struct {
	Pos          Pose
	Vel          float64
	IniMaxVel    float64
	Acc          float64
	MotionType   int32
	ID           int32
	FeedMmPerMin float64
	IndexerJ     int32
}

func (c *LinearMoveCmd) Execute(t *Task) error {
	return t.motion.SetLine(c.Pos, c.Vel, c.IniMaxVel, c.Acc, c.MotionType, c.ID, c.FeedMmPerMin, c.IndexerJ)
}
func (c *LinearMoveCmd) Wait() WaitType { return WaitNone } // queued, no immediate wait
func (c *LinearMoveCmd) String() string { return fmt.Sprintf("LinearMove(id=%d)", c.ID) }
func (c *LinearMoveCmd) LineID() int32  { return c.ID }

// CircularMoveCmd queues a circular arc segment.
type CircularMoveCmd struct {
	Pos          Pose
	Center       Cartesian
	Normal       Cartesian
	Turn         int32
	Vel          float64
	IniMaxVel    float64
	Acc          float64
	MotionType   int32
	ID           int32
	FeedMmPerMin float64
}

func (c *CircularMoveCmd) Execute(t *Task) error {
	return t.motion.SetCircle(c.Pos, c.Center, c.Normal, c.Turn, c.Vel, c.IniMaxVel, c.Acc, c.MotionType, c.ID, c.FeedMmPerMin)
}
func (c *CircularMoveCmd) Wait() WaitType { return WaitNone }
func (c *CircularMoveCmd) String() string { return fmt.Sprintf("CircularMove(id=%d)", c.ID) }
func (c *CircularMoveCmd) LineID() int32  { return c.ID }

// DwellCmd implements a timed pause (G4).
type DwellCmd struct {
	Seconds float64
}

func (c *DwellCmd) Execute(t *Task) error {
	// Report the waiting-for-delay state for the duration of the dwell (G4), so
	// UIs show it rather than a stale ExecDone. (The preceding motion is already
	// drained by the syncBefore() barrier in Canon.Dwell.)
	t.setExecState(ExecWaitingForDelay)
	dur := time.Duration(c.Seconds * float64(time.Second))
	t.mu.Lock()
	t.dwellEnd = time.Now().Add(dur)
	t.mu.Unlock()
	timer := time.NewTimer(dur)
	defer timer.Stop()

	select {
	case <-t.seqAbort:
		return context.Canceled
	case <-timer.C:
		t.setExecState(ExecDone)
		return nil
	}
}
func (c *DwellCmd) Wait() WaitType         { return WaitNone }   // dwell is self-contained
func (c *DwellCmd) Precondition() WaitType { return WaitMotion } // G4: dwell AT the point
func (c *DwellCmd) String() string         { return fmt.Sprintf("Dwell(%.3fs)", c.Seconds) }

// SpindleOnCmd turns a spindle on.
type SpindleOnCmd struct {
	Spindle   int32
	Speed     float64
	CSSFactor float64
	CSSMax    float64
	WaitFlag  int32
}

func (c *SpindleOnCmd) Execute(t *Task) error {
	return t.motion.SpindleOn(c.Spindle, c.Speed, c.CSSFactor, c.CSSMax, c.WaitFlag)
}
func (c *SpindleOnCmd) Wait() WaitType         { return WaitNone }
func (c *SpindleOnCmd) Precondition() WaitType { return WaitMotion } // M3/M4/S: spindle change after preceding motion
func (c *SpindleOnCmd) String() string {
	return fmt.Sprintf("SpindleOn(s=%d,rpm=%.0f)", c.Spindle, c.Speed)
}

// SpindleOffCmd turns a spindle off.
type SpindleOffCmd struct {
	Spindle int32
}

func (c *SpindleOffCmd) Execute(t *Task) error {
	return t.motion.SpindleOff(c.Spindle)
}
func (c *SpindleOffCmd) Wait() WaitType         { return WaitNone }
func (c *SpindleOffCmd) Precondition() WaitType { return WaitMotion } // M5: after preceding motion
func (c *SpindleOffCmd) String() string         { return fmt.Sprintf("SpindleOff(s=%d)", c.Spindle) }

// ToolPrepareCmd prepares a tool (T word).
type ToolPrepareCmd struct {
	Tool int32
}

func (c *ToolPrepareCmd) Execute(t *Task) error {
	return t.io.ToolPrepare(c.Tool)
}
func (c *ToolPrepareCmd) Wait() WaitType { return WaitIO }
func (c *ToolPrepareCmd) String() string { return fmt.Sprintf("ToolPrepare(T%d)", c.Tool) }

// ToolChangeCmd executes a tool change (M6).
type ToolChangeCmd struct{}

func (c *ToolChangeCmd) Execute(t *Task) error {
	if err := t.io.ToolStartChange(); err != nil {
		// ToolStartChange is optional — some IO controllers don't implement it
		t.logger.Info("tool_start_change skipped", "err", err)
	}
	return t.io.ToolLoad()
}
func (c *ToolChangeCmd) Wait() WaitType { return WaitIO }
func (c *ToolChangeCmd) String() string { return "ToolChange" }

// PostWait updates state after tool change completes.
func (c *ToolChangeCmd) PostWait(t *Task) {
	// After tool change completes, the IO controller has loaded the tool.
	// Sync interpreter so it knows the new tool-in-spindle value.
	// The interpreter will handle applying tool length offsets via canon
	// USE_TOOL_LENGTH_OFFSET calls (G43/G43.1).
	if t.interp != nil {
		if err := t.interp.Synch(); err != nil {
			t.logger.Warn("tool change: interpreter synch failed", "err", err)
		}
	}
	t.logger.Info("tool change complete")
}

// SetToolTableEntryCmd updates a single tool table entry via IO controller.
type SetToolTableEntryCmd struct {
	Pocket, Toolno                  int32
	X, Y, Z, A, B, C, U, V, W       float64
	Diameter, Frontangle, Backangle float64
	Orientation                     int32
}

func (c *SetToolTableEntryCmd) Execute(t *Task) error {
	return t.io.ToolSetOffset(c.Pocket, c.Toolno, c.X, c.Y, c.Z, c.A, c.B, c.C, c.U, c.V, c.W, c.Diameter, c.Frontangle, c.Backangle, c.Orientation)
}
func (c *SetToolTableEntryCmd) Wait() WaitType { return WaitIO }
func (c *SetToolTableEntryCmd) String() string {
	return fmt.Sprintf("SetToolTableEntry(P%d T%d)", c.Pocket, c.Toolno)
}

// ChangeToolNumberCmd tells IO to update the current tool number (G43.1 etc).
type ChangeToolNumberCmd struct {
	Number int32
}

func (c *ChangeToolNumberCmd) Execute(t *Task) error {
	return t.io.ToolSetNumber(c.Number)
}
func (c *ChangeToolNumberCmd) Wait() WaitType { return WaitIO }
func (c *ChangeToolNumberCmd) String() string {
	return fmt.Sprintf("ChangeToolNumber(%d)", c.Number)
}

// ReloadTooldataCmd signals the IO controller to reload the tool table file.
type ReloadTooldataCmd struct{}

func (c *ReloadTooldataCmd) Execute(t *Task) error {
	return t.io.ToolLoadTable("")
}
func (c *ReloadTooldataCmd) Wait() WaitType { return WaitIO }
func (c *ReloadTooldataCmd) String() string { return "ReloadTooldata" }

// FloodOnCmd turns flood coolant on (M8).
type FloodOnCmd struct{}

func (c *FloodOnCmd) Execute(t *Task) error  { return t.io.CoolantFloodOn() }
func (c *FloodOnCmd) Wait() WaitType         { return WaitNone }
func (c *FloodOnCmd) Precondition() WaitType { return WaitMotion }
func (c *FloodOnCmd) String() string         { return "FloodOn" }

// FloodOffCmd turns flood coolant off (M9).
type FloodOffCmd struct{}

func (c *FloodOffCmd) Execute(t *Task) error  { return t.io.CoolantFloodOff() }
func (c *FloodOffCmd) Wait() WaitType         { return WaitNone }
func (c *FloodOffCmd) Precondition() WaitType { return WaitMotion }
func (c *FloodOffCmd) String() string         { return "FloodOff" }

// WaitForMotionCmd is an explicit queue-drain sync point (canon FLUSH).
type WaitForMotionCmd struct{}

var waitForMotionSingleton = &WaitForMotionCmd{}

func (c *WaitForMotionCmd) Execute(t *Task) error { return nil }
func (c *WaitForMotionCmd) Wait() WaitType        { return WaitMotion }
func (c *WaitForMotionCmd) String() string        { return "WaitForMotion" }

// ProgramStopCmd waits for motion to drain, then pauses the program (M0).
type ProgramStopCmd struct{}

func (c *ProgramStopCmd) Execute(t *Task) error { return nil }
func (c *ProgramStopCmd) Wait() WaitType        { return WaitMotion }
func (c *ProgramStopCmd) String() string        { return "ProgramStop" }
func (c *ProgramStopCmd) PostWait(t *Task) {
	t.seqEnterPause()
}

// OptionalProgramStopCmd implements M1: like M0, but pauses only when the
// operator's optional-stop toggle is enabled — evaluated at execution time
// (after preceding motion drains), matching 2.9. When disabled it is a no-op
// and the program continues.
type OptionalProgramStopCmd struct{}

func (c *OptionalProgramStopCmd) Execute(t *Task) error { return nil }
func (c *OptionalProgramStopCmd) Wait() WaitType        { return WaitMotion }
func (c *OptionalProgramStopCmd) String() string        { return "OptionalProgramStop" }
func (c *OptionalProgramStopCmd) PostWait(t *Task) {
	t.mu.Lock()
	os := t.optionalStop
	t.mu.Unlock()
	if os {
		t.seqEnterPause()
	}
}

// --- Helper: check sequencer is alive from enqueue side ---

var (
	errSeqNotRunning = fmt.Errorf("sequencer not running")
	errSeqAborted    = fmt.Errorf("sequencer aborted")
)

// SeqRunning reports whether the sequencer goroutine is alive.
func (t *Task) SeqRunning() bool {
	t.mu.Lock()
	done := t.seqDone
	t.mu.Unlock()

	if done == nil {
		return false
	}
	select {
	case <-done:
		return false
	default:
		return true
	}
}

// DrainQueue closes the interpQueue channel and waits for the sequencer to finish.
// Used for normal end-of-program (interpreter done sending commands).
func (t *Task) DrainQueue() {
	t.mu.Lock()
	q := t.interpQueue
	done := t.seqDone
	t.mu.Unlock()

	if q != nil {
		close(q)
	}
	if done != nil {
		<-done
	}
}

// Waiter allows tests to inject a mock for pollUntil.
// Not exported — tests use the concrete mockStatus.InPosition approach.
type waitFunc func() bool

// Queue-depth throttling: do not send motion commands when the TP queue is
// above this high-water mark. The TP has DEFAULT_TC_QUEUE_SIZE=2000 slots;
// if we hit 2000, the C code treats it as fatal (tp_abort + error flag).
// Keep headroom to avoid that scenario entirely.
const tpQueueHighWater = 1800

// Mutex-free accessors for sequencer goroutine to read status.
// These avoid holding mu during polling.
var pollInterval = 10 * time.Millisecond

// Timeouts for wait loops — detect communication failure (controller not
// responding), NOT slow execution. Motion moves and tool changes can take
// arbitrarily long; what we guard against is a dead controller.
var (
	// If status reads fail for this many consecutive polls, declare comm failure.
	commFailureThreshold = 100 // 100ms at 1ms poll = 100ms of no valid response
)

// SetPollInterval allows tests to speed up polling. Not thread-safe — call before StartSequencer.
func SetPollInterval(d time.Duration) func() {
	old := pollInterval
	pollInterval = d
	return func() { pollInterval = old }
}

func init() {
	// Ensure pollInterval is used in pollUntil. Override the hardcoded value.
}

// mu-free helpers that call locked setters above (no change needed, just documenting)
// The sequencer goroutine accesses t.status directly (read-only interface, no mu needed).
// The sequencer goroutine accesses t.seqAbort directly (immutable after StartSequencer).
// The sequencer goroutine calls setExecState/setInterpState which lock mu.

// interpDoneCmd is enqueued after interpreter execution completes,
// to transition interp state back to idle after motion finishes.
type interpDoneCmd struct{}

func (c *interpDoneCmd) Execute(t *Task) error {
	// Do NOT set InterpIdle/ExecDone here — we must wait for motion
	// to finish first (via WaitMotion). State is set in PostWait.
	return nil
}

func (c *interpDoneCmd) PostWait(t *Task) {
	// Only transition to idle if not externally paused.
	// AutoPause may have set InterpPaused while the sequencer was
	// still draining; we must not overwrite that.
	t.mu.Lock()
	if t.interpState != InterpPaused {
		t.interpState = InterpIdle
	}
	t.execState = ExecDone
	t.readLine = 0
	t.currentLine = 0
	t.mu.Unlock()
}

func (c *interpDoneCmd) Wait() WaitType { return WaitMotion }
func (c *interpDoneCmd) String() string { return "interp_done" }

// mdiDoneCmd is enqueued after an MDI command's interpreter execution completes.
// After motion drains, it hands off to finishMDI on a fresh goroutine, which
// transitions to idle and dequeues the next MDI command if one is waiting
// (matching C milltask mdi_execute_hook behavior).
//
// The handoff is deliberate: finishMDI runs the interpreter (executeMDI) and
// must go through the cmdMu front door like any other command. Running it here,
// on the sequencer goroutine, would (a) bypass command serialization and
// (b) self-deadlock as soon as the next MDI expands to more commands than the
// interpQueue can buffer — the sequencer would block enqueueing into the very
// queue only it can drain.
type mdiDoneCmd struct{ gen uint64 }

func (c *mdiDoneCmd) Execute(t *Task) error { return nil }

func (c *mdiDoneCmd) PostWait(t *Task) {
	go t.finishMDI(c.gen)
}

func (c *mdiDoneCmd) Wait() WaitType { return WaitMotion }
func (c *mdiDoneCmd) String() string { return "mdi_done" }

// McodeCmd submits an M-code (M100-M199) to the handler worker and waits
// for completion. This blocks the sequencer until the handler finishes or abort.
type McodeCmd struct {
	Mcode int32
	P     float64
	Q     float64
}

func (c *McodeCmd) Execute(t *Task) error {
	if t.mcode == nil {
		return fmt.Errorf("mcode_handler: not initialized")
	}
	if err := t.mcode.Submit(int(c.Mcode), c.P, c.Q); err != nil {
		return err
	}
	// Poll for completion (worker runs async). CheckDone consumes the result
	// (it clears the done flag), so capture it in the poll callback rather than
	// re-reading afterwards.
	var result int
	if err := t.pollUntil(func() bool {
		r, done := t.mcode.CheckDone()
		if done {
			result = r
		}
		return done
	}); err != nil {
		return err // aborted during the wait
	}
	// Propagate a failed handler so the sequencer faults the program instead of
	// silently continuing (the old fork/exec path made a non-zero exit an
	// EMC_TASK_EXEC_ERROR). Result convention: 0 = success; 32-63 = user-defined
	// result (read by the interpreter); -2 = aborted; anything else = error.
	switch {
	case result == 0 || (result >= 32 && result <= 63):
		return nil
	case result == -2:
		return context.Canceled
	default:
		return fmt.Errorf("M%d handler failed (result %d)", c.Mcode, result)
	}
}
func (c *McodeCmd) Wait() WaitType { return WaitNone } // Execute handles wait internally
func (c *McodeCmd) String() string {
	return fmt.Sprintf("Mcode(M%d P=%.4f Q=%.4f)", c.Mcode, c.P, c.Q)
}
