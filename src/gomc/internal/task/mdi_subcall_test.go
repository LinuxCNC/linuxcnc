// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"log/slog"
	"os"
	"sync"
	"sync/atomic"
	"testing"
	"time"
)

// recordMotion counts LinearMove (SetLine) dispatches so a test can prove the
// moves an MDI o-word sub queues *after* a queue-buster actually reach motion.
type recordMotion struct {
	mockMotion
	mu    sync.Mutex
	lines int
}

func (m *recordMotion) SetLine(_ Pose, _, _, _ float64, _ int32, _ int32, _ float64, _ int32) error {
	m.mu.Lock()
	m.lines++
	m.mu.Unlock()
	return nil
}

func (m *recordMotion) lineCount() int {
	m.mu.Lock()
	defer m.mu.Unlock()
	return m.lines
}

// TestMDI_MultiLevelSubContinuation is the regression test for parity gap #13.
//
// An MDI o-word subroutine that hits a queue-buster (probe, M66, dwell, tool
// change) makes Interp::_execute return INTERP_EXECUTE_FINISH *mid-sub* — the
// interpreter has more of the sub to run and expects the caller to drain the
// motion queue and call execute() again (C++ re-issues emcTaskPlanExecute(0)
// until the call level unwinds). The old executeMDI called ExecuteString once
// and treated EXECUTE_FINISH as "done", so everything in the sub after the
// first queue-buster (record the probe result, retract, further moves) never
// ran. finishMDI now drives the continuation while interp.CallLevel() > 0.
//
// The fake interpreter models a sub with two queue-busters: the opening
// ExecuteString and one continuation each queue a move then FINISH with the
// call level still up; the final continuation queues the last move and returns
// OK with the call level unwound. Without the fix the interpreter is driven
// exactly once (only move 1 dispatches) and the task reports done with the sub
// half-finished.
func TestMDI_MultiLevelSubContinuation(t *testing.T) {
	restore := SetPollInterval(time.Millisecond)
	t.Cleanup(restore)

	mot := &recordMotion{}
	io := &mockIO{}
	stat := &mockStatus{}
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	task := NewTask(mot, io, stat, logger)
	task.SetIOStatusReader(io)
	task.noForceHoming = true

	var level int32 // interpreter subroutine nesting level, as CallLevel() reports it
	fi := &fakeInterp{}
	fi.onExecuteString = func(string) (int, error) {
		// Enter the sub: queue a move, then a queue-buster forces EXECUTE_FINISH
		// with the sub still on the call stack.
		activeCanon().task.EnqueueCmd(&LinearMoveCmd{ID: 1})
		atomic.StoreInt32(&level, 1)
		return InterpExecuteFinish, nil
	}
	fi.onExecute = func(call int) (int, error) {
		tk := activeCanon().task
		if call == 1 {
			// Second queue-buster inside the sub — still mid-sub.
			tk.EnqueueCmd(&LinearMoveCmd{ID: 2})
			return InterpExecuteFinish, nil
		}
		// Sub runs to its end: final move, call level unwinds.
		tk.EnqueueCmd(&LinearMoveCmd{ID: 3})
		atomic.StoreInt32(&level, 0)
		return InterpOK, nil
	}
	fi.onCallLevel = func() int { return int(atomic.LoadInt32(&level)) }

	task.SetInterpreter(fi)
	bringUp(task)
	if err := task.SetMode(int32(ModeMDI)); err != nil {
		t.Fatalf("SetMode(MDI): %v", err)
	}
	task.StartSequencer()
	t.Cleanup(task.StopSequencer)

	if err := task.MDI("o<probe> call"); err != nil {
		t.Fatalf("MDI: %v", err)
	}
	waitIdle(t, task, 5*time.Second)

	// The continuation must have re-driven the interpreter twice (once per
	// queue-buster after the opening ExecuteString) until the call level hit 0.
	fi.mu.Lock()
	execs := fi.execs
	fi.mu.Unlock()
	if execs != 2 {
		t.Fatalf("interp.Execute() called %d times, want 2 (sub not continued to completion)", execs)
	}

	// All three sub moves must dispatch — the two queued *after* queue-busters
	// are exactly what the old single-shot path dropped.
	if got := mot.lineCount(); got != 3 {
		t.Fatalf("SetLine dispatched %d times, want 3 (sub left half-executed)", got)
	}

	task.mu.Lock()
	is, es := task.interpState, task.execState
	task.mu.Unlock()
	if is != InterpIdle || es != ExecDone {
		t.Fatalf("final state = (%v, %v), want (InterpIdle, ExecDone)", is, es)
	}
}
