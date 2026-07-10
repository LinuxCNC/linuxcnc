// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
//
// Regression tests for pre-merge review findings whose fixes were correct but
// previously unprotected by a test that would catch a reversion:
//   - C4: rigid-tap velocity must use the per-axis straight-move max, unclamped
//         by the F word (else tp's position-sync phase throttles the tap).
//   - C5: M66 rise/fall are true two-phase edges, not levels (an input already
//         at the target level must NOT satisfy the wait immediately).
//   - C3: an ESTOP received while already estopped must keep the sequencer
//         alive (restart it), so the machine still produces motion after
//         recovery.
package task

import (
	"log/slog"
	"os"
	"sync/atomic"
	"testing"
	"time"
)

// C4 — rigid tap velocity is dictated by spindle sync, not F. With a slow
// active feed far below the per-axis straight-move max, RigidTap must still
// command the straight max (both vel and ini_maxvel); if it were clamped to the
// feed rate (the old feedLimits path), tp would cap the synced move at that
// slow value and break the thread. Mutation: reverting RigidTap to feedLimits
// makes cmd.Vel == the feed rate and this fails.
func TestCanon_RigidTapVelocityUnclampedByFeed(t *testing.T) {
	task, _, _ := newCanonTestTask()
	applyBlendLimits(task) // per-axis straight max: X40 Y25 Z8 (mm/s)
	c := task.canon

	// Deliberately slow feed, well below the Z straight-move max (8 mm/s).
	c.state.linearFeedRate = 2.0 // mm/s (≈F120)
	from := c.state.endPoint

	// Provide queue channels without a running sequencer so the enqueued
	// command survives for inspection (same idiom as TestCanon_Dwell_Args).
	task.mu.Lock()
	task.interpQueue = make(chan QueuedCmd, 8)
	task.seqAbort = make(chan struct{})
	task.mu.Unlock()

	c.RigidTap(7, 0, 0, -10, 1.0) // G33.1 Z-10, pure-Z so the Z axis limit governs

	close(task.interpQueue)
	var cmd *RigidTapCmd
	for q := range task.interpQueue {
		if rt, ok := q.(*RigidTapCmd); ok {
			cmd = rt
		}
	}
	if cmd == nil {
		t.Fatal("no RigidTapCmd enqueued by RigidTap")
	}

	wantVel, wantAcc, _, _ := task.straightLimits(from, cmd.Pos)
	if wantVel <= c.state.linearFeedRate {
		t.Fatalf("test setup broken: straight max %.3f must exceed feed %.3f", wantVel, c.state.linearFeedRate)
	}
	if cmd.Vel != wantVel {
		t.Fatalf("RigidTap vel = %.3f, want per-axis straight max %.3f (a feed clamp would give %.3f)",
			cmd.Vel, wantVel, c.state.linearFeedRate)
	}
	// Explicit anti-regression: the tap must not be throttled to the feed rate.
	if cmd.Vel <= c.state.linearFeedRate {
		t.Fatalf("RigidTap vel %.3f clamped to/below feed %.3f — the feed clamp regressed", cmd.Vel, c.state.linearFeedRate)
	}
	if cmd.Acc != wantAcc {
		t.Fatalf("RigidTap acc = %.3f, want %.3f", cmd.Acc, wantAcc)
	}
}

// edgeInputStatus makes digital input 0 scriptable so a test can drive the M66
// poll loop through a specific level sequence.
type edgeInputStatus struct {
	testStatus
	di func() int32
}

func (s *edgeInputStatus) GetSynchDi(i int32) (int32, error) {
	if s.di != nil && i == 0 {
		return s.di(), nil
	}
	return 0, nil
}

func newEdgeInputTask(t *testing.T, di func() int32) *Task {
	t.Helper()
	restore := SetPollInterval(time.Millisecond)
	t.Cleanup(restore)
	stat := &edgeInputStatus{di: di}
	stat.inPosition.Store(true) // waitMotionDone returns immediately
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	task := NewTask(&testMotion{failAt: -1}, &mockIO{}, stat, logger)
	task.mu.Lock()
	task.seqAbort = make(chan struct{}) // non-nil so Execute's abort select is valid
	task.mu.Unlock()
	return task
}

// readTimeoutFlag returns t.inputTimeout (0=met/cleared, 1=timed out, 2=waiting).
func readTimeoutFlag(t *Task) int32 {
	t.mu.Lock()
	defer t.mu.Unlock()
	return t.inputTimeout
}

// C5 — M66 rise/fall must be true edges. A wait for a rising edge on a line
// that is already high must NOT complete on the first poll (it has to observe
// the low level first); the level implementation (satisfied = high) would
// complete immediately. Symmetric for fall on an already-low line. Mutation:
// reverting the edge cases to `satisfied = high` / `= !high` makes the
// "already at target level" subtests satisfy instead of timing out, and they
// fail.
func TestCanon_WaitInputRiseFallAreEdges(t *testing.T) {
	// WaitType: 1=rise, 2=fall, 3=high, 4=low. Timeout>0 to arm the wait.
	t.Run("rise-on-already-high-times-out", func(t *testing.T) {
		task := newEdgeInputTask(t, func() int32 { return 1 }) // stuck high
		cmd := &WaitInputCmd{Index: 0, InputType: 1, WaitType: 1, Timeout: 0.05}
		if err := cmd.Execute(task); err != nil {
			t.Fatalf("Execute: %v", err)
		}
		if got := readTimeoutFlag(task); got != 1 {
			t.Fatalf("rise wait on an already-high input: inputTimeout=%d, want 1 (timed out). "+
				"A rise must not be satisfied by a level that was already high (level-not-edge regression).", got)
		}
	})

	t.Run("fall-on-already-low-times-out", func(t *testing.T) {
		task := newEdgeInputTask(t, func() int32 { return 0 }) // stuck low
		cmd := &WaitInputCmd{Index: 0, InputType: 1, WaitType: 2, Timeout: 0.05}
		if err := cmd.Execute(task); err != nil {
			t.Fatalf("Execute: %v", err)
		}
		if got := readTimeoutFlag(task); got != 1 {
			t.Fatalf("fall wait on an already-low input: inputTimeout=%d, want 1 (timed out).", got)
		}
	})

	t.Run("rise-completes-on-real-low-to-high-edge", func(t *testing.T) {
		// Value sequence keyed on read count (timing-independent): high while the
		// edge is unarmed, then a low to arm, then high to complete.
		var reads int32
		task := newEdgeInputTask(t, func() int32 {
			switch n := atomic.AddInt32(&reads, 1); {
			case n <= 3:
				return 1 // already high: rise must wait, not complete
			case n == 4:
				return 0 // low: arms the edge
			default:
				return 1 // high again: the rising edge completes here
			}
		})
		cmd := &WaitInputCmd{Index: 0, InputType: 1, WaitType: 1, Timeout: 2.0}
		if err := cmd.Execute(task); err != nil {
			t.Fatalf("Execute: %v", err)
		}
		if got := readTimeoutFlag(task); got != 0 {
			t.Fatalf("rise wait after a genuine low→high edge: inputTimeout=%d, want 0 (satisfied)", got)
		}
	})
}

// C3 — an ESTOP received while the machine is already estopped/off used to close
// seqAbort and kill the sequencer goroutine without restarting it (only the
// was-ON branch restarted it), silently stranding all later canon motion. After
// recovery the sequencer must be alive: a queued motion command must dispatch.
//
// The test deliberately does NOT switch mode during recovery: SetMode(MDI) would
// revive the sequencer via abortLocked and mask the bug (the wedge only persists
// when the machine stays in one mode across the killing estop). It therefore
// probes sequencer liveness directly via EnqueueCmd rather than through MDI.
// Mutation: making the not-was-ON estop branch skip restartSequencer leaves the
// sequencer dead — EnqueueCmd errors / the move never dispatches — and this fails.
func TestSetState_EstopWhileEstoppedKeepsSequencerAlive(t *testing.T) {
	restore := SetPollInterval(time.Millisecond)
	t.Cleanup(restore)

	mot := &recordMotion{}
	io := &mockIO{}
	stat := &mockStatus{}
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	task := NewTask(mot, io, stat, logger)
	task.SetIOStatusReader(io)
	task.noForceHoming = true
	task.numSpindles = 1

	bringUp(task) // estop-reset → on
	task.StartSequencer()
	t.Cleanup(task.StopSequencer)

	// First ESTOP from ON: was-ON, machineShutdown restarts the sequencer.
	if err := task.SetState(int32(StateEstop)); err != nil {
		t.Fatalf("estop 1: %v", err)
	}
	// Second ESTOP while ALREADY estopped: not-was-ON — the branch that used to
	// close seqAbort and leave the sequencer goroutine dead with no restart.
	if err := task.SetState(int32(StateEstop)); err != nil {
		t.Fatalf("estop 2 (while estopped): %v", err)
	}
	// Recover — no mode switch (would mask the bug via abortLocked's restart).
	if err := task.SetState(int32(StateEstopReset)); err != nil {
		t.Fatalf("estop-reset: %v", err)
	}
	if err := task.SetState(int32(StateOn)); err != nil {
		t.Fatalf("on: %v", err)
	}

	// The sequencer must be alive: a queued motion command must reach motion.
	if err := task.EnqueueCmd(&LinearMoveCmd{ID: 1}); err != nil {
		t.Fatalf("EnqueueCmd after estop-while-estopped + recovery failed: %v "+
			"(the second estop killed the sequencer and it was never restarted)", err)
	}
	if !waitForCond(2*time.Second, func() bool { return mot.lineCount() == 1 }) {
		t.Fatalf("queued move never dispatched after recovery — sequencer dead (dispatched %d)", mot.lineCount())
	}
}

// C1 — an interpreter error in a multi-block MDI must abort the motion the
// earlier blocks already queued and leave the task in ExecError, not just set
// state and return. The fake interp queues a move (block 1) then errors: MDI
// must return an error, motion.Abort must fire, execState must be ExecError, and
// the MDI queue/echo must be cleared. Mutation: reverting the executeMDI error
// paths to `setInterpState(InterpIdle); return err` (no faultMDI) leaves
// motion.Abort uncalled and execState != ExecError — this fails.
func TestMDI_InterpErrorAbortsMotionAndFaults(t *testing.T) {
	restore := SetPollInterval(time.Millisecond)
	t.Cleanup(restore)

	mot := &recordMotion{}
	io := &mockIO{}
	stat := &mockStatus{}
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	task := NewTask(mot, io, stat, logger)
	task.SetIOStatusReader(io)
	task.noForceHoming = true

	fi := &fakeInterp{}
	fi.onExecuteString = func(string) (int, error) {
		// Block 1 of the MDI queues motion; a later block hits a runtime error.
		activeCanon().task.EnqueueCmd(&LinearMoveCmd{ID: 1})
		return InterpError, nil
	}
	task.SetInterpreter(fi)
	bringUp(task)
	if err := task.SetMode(int32(ModeMDI)); err != nil {
		t.Fatalf("SetMode(MDI): %v", err)
	}
	task.StartSequencer()
	t.Cleanup(task.StopSequencer)

	if err := task.MDI("o<x> call"); err == nil {
		t.Fatal("MDI with an interpreter error returned nil, want an error")
	}
	if !waitForCond(2*time.Second, func() bool { return mot.hasCall("Abort") }) {
		t.Fatal("MDI interpreter error did not abort in-flight motion (C1)")
	}
	task.mu.Lock()
	es, is, qn, tc := task.execState, task.interpState, len(task.mdiQueue), task.taskCommand
	task.mu.Unlock()
	if es != ExecError {
		t.Fatalf("execState = %v, want ExecError after MDI interpreter error (C1)", es)
	}
	if is != InterpIdle {
		t.Fatalf("interpState = %v, want InterpIdle", is)
	}
	if qn != 0 || tc != "" {
		t.Fatalf("MDI queue/echo not cleared on fault: len=%d command=%q (C1/C9)", qn, tc)
	}
}

// C8 (a) — finishMDI carries the MDI generation captured at issue and must exit
// without touching state when it is stale (a newer MDI, or an abort, bumped
// mdiGen after the handoff). Simulate MDI-2 mid-run (mdiGen advanced) and fire a
// leftover finishMDI from MDI-1: it must NOT synch/commit against MDI-2's state.
// Mutation: dropping the `gen != t.mdiGen` staleness check makes the stale
// finishMDI drive InterpIdle/ExecDone over MDI-2 and this fails.
func TestFinishMDI_StaleGenerationIsNoop(t *testing.T) {
	task, _, _ := newTestTask()
	task.noForceHoming = true
	task.SetInterpreter(&fakeInterp{})

	task.mu.Lock()
	task.mdiGen = 5                    // MDI-2 was issued after MDI-1
	task.interpState = InterpReading   // MDI-2 is mid-run
	task.execState = ExecWaitingForMotion
	task.mu.Unlock()

	task.finishMDI(3) // leftover completion from MDI-1 (gen 3)

	task.mu.Lock()
	is, es := task.interpState, task.execState
	task.mu.Unlock()
	if is != InterpReading || es != ExecWaitingForMotion {
		t.Fatalf("stale finishMDI clobbered MDI-2 state: (%v,%v), want (InterpReading, ExecWaitingForMotion) (C8)", is, es)
	}
}

// C8 (b) — finishMDI must never downgrade a latched ExecError to ExecDone (a
// monitor fault teardown that ran after the MDI's motion drained). Mutation:
// removing the `execState == ExecError` early-return in finishMDI makes it
// commit ExecDone and this fails.
func TestFinishMDI_DoesNotDowngradeExecError(t *testing.T) {
	task, _, _ := newTestTask()
	task.noForceHoming = true
	task.SetInterpreter(&fakeInterp{})

	task.mu.Lock()
	task.mdiGen = 2
	task.interpState = InterpIdle // fault teardown set idle
	task.execState = ExecError    // fault latched error
	task.mu.Unlock()

	task.finishMDI(2) // matching generation, but ExecError must survive

	task.mu.Lock()
	es := task.execState
	task.mu.Unlock()
	if es != ExecError {
		t.Fatalf("finishMDI downgraded ExecError to %v (C8: never downgrade outside recovery)", es)
	}
}

// C9 — when a dequeued MDI fails, the rest of the MDI queue must be flushed so a
// later entry cannot execute out of order. finishMDI dequeues "cmd2" (which
// errors) with "cmd3" still queued; after the fault the queue must be empty and
// cmd3 must never run. Mutation: reverting executeMDI's error path to log-only
// (no faultMDI flush) leaves cmd3 queued and a subsequent finishMDI would run it.
func TestFinishMDI_FailedDequeuedMDIFlushesQueue(t *testing.T) {
	restore := SetPollInterval(time.Millisecond)
	t.Cleanup(restore)

	mot := &recordMotion{}
	io := &mockIO{}
	stat := &mockStatus{}
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	task := NewTask(mot, io, stat, logger)
	task.SetIOStatusReader(io)
	task.noForceHoming = true

	var executed []string
	fi := &fakeInterp{}
	fi.onExecuteString = func(cmd string) (int, error) {
		executed = append(executed, cmd)
		if cmd == "cmd2" {
			return InterpError, nil // the dequeued MDI fails
		}
		return InterpOK, nil
	}
	task.SetInterpreter(fi)
	bringUp(task)
	if err := task.SetMode(int32(ModeMDI)); err != nil {
		t.Fatalf("SetMode(MDI): %v", err)
	}
	task.StartSequencer()
	t.Cleanup(task.StopSequencer)

	// MDI-1 "running" with two more queued; finishMDI(1) dequeues cmd2.
	task.mu.Lock()
	task.mdiGen = 1
	task.interpState = InterpReading
	task.execState = ExecDone
	task.mdiQueue = []string{"cmd2", "cmd3"}
	task.mu.Unlock()

	task.finishMDI(1)

	task.mu.Lock()
	qn := len(task.mdiQueue)
	task.mu.Unlock()
	if qn != 0 {
		t.Fatalf("mdiQueue not flushed after a failed dequeued MDI: len=%d (C9 out-of-order replay)", qn)
	}
	for _, c := range executed {
		if c == "cmd3" {
			t.Fatal("cmd3 executed after cmd2 failed — stale queue replayed out of order (C9)")
		}
	}
}
