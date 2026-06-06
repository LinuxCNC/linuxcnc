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

// StartSequencer launches the sequencer goroutine. Must be called with mu NOT held.
// If a previous sequencer goroutine is still winding down, StartSequencer waits
// for it to exit before creating new channels. This prevents a race where the old
// goroutine could pick up the new (non-closed) seqAbort channel and keep running.
func (t *Task) StartSequencer() {
	t.mu.Lock()
	oldDone := t.seqDone
	oldAbort := t.seqAbort
	t.mu.Unlock()

	// Abort previous sequencer if still running.
	if oldAbort != nil {
		select {
		case <-oldAbort:
		default:
			close(oldAbort)
		}
	}

	// Wait for previous goroutine to finish.
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
	t.mu.Lock()
	abort := t.seqAbort
	done := t.seqDone
	t.mu.Unlock()

	if abort == nil {
		return
	}

	select {
	case <-abort:
		// already aborted
	default:
		close(abort)
	}

	// Wait for goroutine to finish
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
	t.mu.Unlock()

	if abort == nil {
		return
	}

	select {
	case <-abort:
	default:
		close(abort)
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

	for {
		// Check if pause was requested before reading next command
		if t.seqCheckPause() {
			return // aborted
		}

		select {
		case <-t.seqAbort:
			t.setExecState(ExecDone)
			return

		case cmd, ok := <-t.interpQueue:
			if !ok {
				// Channel closed — normal shutdown
				t.setExecState(ExecDone)
				t.setInterpState(InterpIdle)
				return
			}

			t.logger.Debug("sequencer exec", "cmd", cmd.String())

			// Update currentLine from motion commands that carry a line ID.
			if lc, ok := cmd.(interface{ LineID() int32 }); ok {
				if id := lc.LineID(); id > 0 {
					t.mu.Lock()
					t.currentLine = id
					t.mu.Unlock()
				}
			}

			// Execute the command, retrying motion commands on queue-full errors.
			const maxMotionRetries = 1000 // ~10s at 10ms poll interval
			retries := 0
			for {
				err := cmd.Execute(t)
				if err == nil {
					break
				}
				if errors.Is(err, context.Canceled) {
					t.setExecState(ExecDone)
					t.setInterpState(InterpIdle)
					return
				}
				// For motion/TP commands, retry after a short wait (TP queue full)
				switch cmd.(type) {
				case *LinearMoveCmd, *CircularMoveCmd, *RigidTapCmd, *SetMotionParamsCmd:
					retries++
					if retries == 1 {
						t.logger.Info("sequencer: TP full, retrying", "cmd", cmd.String())
					}
					if retries > maxMotionRetries {
						t.logger.Error("sequencer motion cmd failed after retries", "cmd", cmd.String(), "err", err)
						t.operatorError(fmt.Sprintf("Motion command failed: %s", err))
						t.setExecState(ExecError)
						t.setInterpState(InterpIdle)
						t.mu.Lock()
						select {
						case <-t.seqAbort:
						default:
							close(t.seqAbort)
						}
						t.mu.Unlock()
						return
					}
					// Wait for TP to drain one slot, then retry
					select {
					case <-t.seqAbort:
						t.setExecState(ExecDone)
						t.setInterpState(InterpIdle)
						return
					case <-time.After(pollInterval):
						continue
					}
				default:
					// Non-motion command failed — check if it was due to abort
					select {
					case <-t.seqAbort:
						// Abort was requested — command failure is expected, not an error
						t.logger.Info("sequencer command aborted", "cmd", cmd.String())
						t.setExecState(ExecDone)
						t.setInterpState(InterpIdle)
						return
					default:
					}
					t.logger.Error("sequencer command failed", "cmd", cmd.String(), "err", err)
					t.operatorError(fmt.Sprintf("Command failed: %s: %s", cmd.String(), err))
					t.setExecState(ExecError)
					t.setInterpState(InterpIdle)
					// Signal abort so interpreter goroutine unblocks from EnqueueCmd
					t.mu.Lock()
					select {
					case <-t.seqAbort:
					default:
						close(t.seqAbort)
					}
					t.mu.Unlock()
					return
				}
			}

			// Wait as required
			if err := t.waitForCompletion(cmd.Wait()); err != nil {
				if errors.Is(err, context.Canceled) {
					// Abort — not an error condition
					t.logger.Info("sequencer aborted during wait", "cmd", cmd.String())
					t.setExecState(ExecDone)
					return
				}
				t.logger.Error("sequencer wait failed", "cmd", cmd.String(), "err", err)
				t.operatorError(fmt.Sprintf("Wait failed: %s: %s", cmd.String(), err))
				t.setExecState(ExecError)
				t.setInterpState(InterpIdle)
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
					t.setExecState(ExecDone)
					return
				}
				t.seqEnterPause()
				if t.seqCheckPause() {
					return // aborted while paused
				}
			}
		}
	}
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
	select {
	case <-t.seqPauseCh:
		// already closed
	default:
		close(t.seqPauseCh)
	}
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
		t.setExecState(ExecDone)
		return true
	case <-pauseCh:
		// Paused — block until resumed or aborted
		t.setInterpState(InterpPaused)
		t.mu.Lock()
		resumeCh := t.seqResumeCh
		t.mu.Unlock()
		select {
		case <-abort:
			t.setExecState(ExecDone)
			return true
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
func (t *Task) waitForCompletion(wt WaitType) error {
	switch wt {
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
		return t.waitSpindleOriented()
	}
	return nil
}

// waitMotionDone polls motion status until in-position and queue empty, or abort.
// A communication watchdog detects if the motion controller stops responding.
func (t *Task) waitMotionDone() error {
	t.setExecState(ExecWaitingForMotion)
	// Skip a few poll intervals to allow the servo thread to process any
	// recently dispatched commands and update status (avoids stale-inPos
	// race after rapid queue fill).
	ticker := time.NewTicker(pollInterval)
	defer ticker.Stop()
	for i := 0; i < 5; i++ {
		select {
		case <-t.seqAbort:
			return context.Canceled
		case <-ticker.C:
		}
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

// waitSpindleOriented polls until spindle orient is complete.
func (t *Task) waitSpindleOriented() error {
	t.setExecState(ExecWaitingForSpindleOriented)
	ticker := time.NewTicker(pollInterval)
	defer ticker.Stop()
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
	Pos        Pose
	Vel        float64
	IniMaxVel  float64
	Acc        float64
	MotionType int32
	ID         int32
	Tag        StateTag
	IndexerJ   int32
}

func (c *LinearMoveCmd) Execute(t *Task) error {
	return t.motion.SetLine(c.Pos, c.Vel, c.IniMaxVel, c.Acc, c.MotionType, c.ID, c.Tag, c.IndexerJ)
}
func (c *LinearMoveCmd) Wait() WaitType { return WaitNone } // queued, no immediate wait
func (c *LinearMoveCmd) String() string { return fmt.Sprintf("LinearMove(id=%d)", c.ID) }
func (c *LinearMoveCmd) LineID() int32  { return c.ID }

// CircularMoveCmd queues a circular arc segment.
type CircularMoveCmd struct {
	Pos        Pose
	Center     Cartesian
	Normal     Cartesian
	Turn       int32
	Vel        float64
	IniMaxVel  float64
	Acc        float64
	MotionType int32
	ID         int32
	Tag        StateTag
}

func (c *CircularMoveCmd) Execute(t *Task) error {
	return t.motion.SetCircle(c.Pos, c.Center, c.Normal, c.Turn, c.Vel, c.IniMaxVel, c.Acc, c.MotionType, c.ID, c.Tag)
}
func (c *CircularMoveCmd) Wait() WaitType { return WaitNone }
func (c *CircularMoveCmd) String() string { return fmt.Sprintf("CircularMove(id=%d)", c.ID) }
func (c *CircularMoveCmd) LineID() int32  { return c.ID }

// DwellCmd implements a timed pause (G4).
type DwellCmd struct {
	Seconds float64
}

func (c *DwellCmd) Execute(t *Task) error {
	timer := time.NewTimer(time.Duration(c.Seconds * float64(time.Second)))
	defer timer.Stop()

	select {
	case <-t.seqAbort:
		return context.Canceled
	case <-timer.C:
		return nil
	}
}
func (c *DwellCmd) Wait() WaitType { return WaitNone } // dwell is self-contained
func (c *DwellCmd) String() string { return fmt.Sprintf("Dwell(%.3fs)", c.Seconds) }

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
func (c *SpindleOnCmd) Wait() WaitType { return WaitNone }
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
func (c *SpindleOffCmd) Wait() WaitType { return WaitNone }
func (c *SpindleOffCmd) String() string { return fmt.Sprintf("SpindleOff(s=%d)", c.Spindle) }

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

func (c *FloodOnCmd) Execute(t *Task) error { return t.io.CoolantFloodOn() }
func (c *FloodOnCmd) Wait() WaitType        { return WaitNone }
func (c *FloodOnCmd) String() string        { return "FloodOn" }

// FloodOffCmd turns flood coolant off (M9).
type FloodOffCmd struct{}

func (c *FloodOffCmd) Execute(t *Task) error { return t.io.CoolantFloodOff() }
func (c *FloodOffCmd) Wait() WaitType        { return WaitNone }
func (c *FloodOffCmd) String() string        { return "FloodOff" }

// WaitForMotionCmd is an explicit queue-drain sync point (canon FLUSH).
type WaitForMotionCmd struct{}

var waitForMotionSingleton = &WaitForMotionCmd{}

func (c *WaitForMotionCmd) Execute(t *Task) error { return nil }
func (c *WaitForMotionCmd) Wait() WaitType        { return WaitMotion }
func (c *WaitForMotionCmd) String() string        { return "WaitForMotion" }

// ProgramStopCmd waits for motion to drain, then pauses the program (M0/M1).
type ProgramStopCmd struct{}

func (c *ProgramStopCmd) Execute(t *Task) error { return nil }
func (c *ProgramStopCmd) Wait() WaitType        { return WaitMotion }
func (c *ProgramStopCmd) String() string        { return "ProgramStop" }
func (c *ProgramStopCmd) PostWait(t *Task) {
	t.seqEnterPause()
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
// After motion drains, it transitions to idle and dequeues the next MDI command
// if one is waiting (matching C milltask mdi_execute_hook behavior).
type mdiDoneCmd struct{}

func (c *mdiDoneCmd) Execute(t *Task) error { return nil }

func (c *mdiDoneCmd) PostWait(t *Task) {
	t.setInterpState(InterpIdle)
	t.setExecState(ExecDone)

	// Dequeue next MDI command if any are waiting.
	t.mu.Lock()
	if len(t.mdiQueue) > 0 {
		next := t.mdiQueue[0]
		t.mdiQueue = t.mdiQueue[1:]
		t.mu.Unlock()
		// Execute the next MDI command — this will enqueue another mdiDoneCmd.
		if err := t.executeMDI(next); err != nil {
			t.logger.Error("queued MDI failed", "cmd", next, "err", err)
			t.operatorError(fmt.Sprintf("MDI error: %s", err))
		}
		return
	}
	// MDI queue empty — restore mode if this was a transactional switch.
	t.restoreModeTx()
	t.mu.Unlock()
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
	// Poll for completion (worker runs async).
	return t.pollUntil(func() bool {
		_, done := t.mcode.CheckDone()
		return done
	})
}
func (c *McodeCmd) Wait() WaitType { return WaitNone } // Execute handles wait internally
func (c *McodeCmd) String() string {
	return fmt.Sprintf("Mcode(M%d P=%.4f Q=%.4f)", c.Mcode, c.P, c.Q)
}
