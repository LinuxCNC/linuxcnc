// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"errors"
	"log/slog"
	"os"
	"sync/atomic"
	"testing"
	"time"
)

// testMotion extends mockMotion with tracking for sequencer tests.
type testMotion struct {
	mockMotion
	calls     []string
	failAt    int // -1 = don't fail
	callCount int
}

func (m *testMotion) SetLine(pos Pose, vel, iniMaxvel, acc float64, motionType int32, id int32, feedUpm float64, indexerJnum int32) error {
	m.calls = append(m.calls, "SetLine")
	m.callCount++
	if m.failAt >= 0 && m.callCount > m.failAt {
		return errors.New("injected error")
	}
	return nil
}

func (m *testMotion) SpindleOn(spindle int32, speed, css_factor, css_max float64, wait int32) error {
	m.calls = append(m.calls, "SpindleOn")
	m.callCount++
	return nil
}

func (m *testMotion) SpindleOff(spindle int32) error {
	m.calls = append(m.calls, "SpindleOff")
	m.callCount++
	return nil
}

// testStatus allows controlling InPosition from tests.
type testStatus struct {
	mockStatus
	inPosition atomic.Bool
}

func (s *testStatus) GetInpos() (int32, error) {
	if s.inPosition.Load() {
		return 1, nil
	}
	return 0, nil
}

func newSeqTestTask() (*Task, *testMotion, *mockIO, *testStatus) {
	mot := &testMotion{failAt: -1}
	io := &mockIO{}
	stat := &testStatus{}
	stat.inPosition.Store(true) // default: in position (no waiting)
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	t := NewTask(mot, io, stat, logger)
	return t, mot, io, stat
}

func TestSequencer_ExecutesInOrder(t *testing.T) {
	task, mot, _, _ := newSeqTestTask()
	restore := SetPollInterval(10 * time.Microsecond)
	defer restore()

	task.StartSequencer()
	defer task.StopSequencer()

	// Enqueue 3 linear moves
	for i := 0; i < 3; i++ {
		cmd := &LinearMoveCmd{ID: int32(i + 1)}
		if err := task.EnqueueCmd(cmd); err != nil {
			t.Fatalf("enqueue %d: %v", i, err)
		}
	}

	// Close and wait for drain
	task.DrainQueue()

	if len(mot.calls) != 3 {
		t.Fatalf("expected 3 calls, got %d: %v", len(mot.calls), mot.calls)
	}
	for _, c := range mot.calls {
		if c != "SetLine" {
			t.Fatalf("expected SetLine, got %s", c)
		}
	}
}

func TestSequencer_AbortClearsQueue(t *testing.T) {
	task, mot, _, stat := newSeqTestTask()
	restore := SetPollInterval(10 * time.Microsecond)
	defer restore()

	// Don't be in position — force the sequencer to wait
	stat.inPosition.Store(false)

	task.StartSequencer()

	// Enqueue a command that waits for motion + more commands behind it
	task.EnqueueCmd(waitForMotionSingleton)
	task.EnqueueCmd(&LinearMoveCmd{ID: 1})
	task.EnqueueCmd(&LinearMoveCmd{ID: 2})

	// Give sequencer time to start waiting
	time.Sleep(5 * time.Millisecond)

	// Abort — should cancel the wait and drain remaining
	task.AbortSequencer()

	// Wait for goroutine exit
	<-task.seqDone

	// The motion commands after the wait should NOT have been executed
	if len(mot.calls) > 0 {
		t.Fatalf("expected no motion calls after abort, got %v", mot.calls)
	}

	// State should be reset
	task.mu.Lock()
	es := task.execState
	is := task.interpState
	task.mu.Unlock()

	if es != ExecDone {
		t.Fatalf("expected ExecDone, got %d", es)
	}
	if is != InterpIdle {
		t.Fatalf("expected InterpIdle, got %d", is)
	}
}

func TestSequencer_ErrorStopsExecution(t *testing.T) {
	task, mot, _, _ := newSeqTestTask()
	restore := SetPollInterval(10 * time.Microsecond)
	defer restore()

	mot.failAt = 1 // fail on 2nd call

	task.StartSequencer()

	task.EnqueueCmd(&LinearMoveCmd{ID: 1})
	task.EnqueueCmd(&LinearMoveCmd{ID: 2})
	task.EnqueueCmd(&LinearMoveCmd{ID: 3})

	// Wait for sequencer to stop on error
	<-task.seqDone

	// First call succeeds, then motion command is retried up to maxMotionRetries (1000).
	// Expect 1 success + 1001 failed attempts = 1002 total calls.
	if len(mot.calls) < 2 {
		t.Fatalf("expected at least 2 calls, got %d: %v", len(mot.calls), mot.calls)
	}

	task.mu.Lock()
	es := task.execState
	task.mu.Unlock()

	if es != ExecError {
		t.Fatalf("expected ExecError, got %d", es)
	}
}

func TestSequencer_WaitForMotion(t *testing.T) {
	task, mot, _, stat := newSeqTestTask()
	restore := SetPollInterval(100 * time.Microsecond)
	defer restore()

	stat.inPosition.Store(false)

	task.StartSequencer()

	// Queue: WaitForMotion, then a linear move
	task.EnqueueCmd(waitForMotionSingleton)
	task.EnqueueCmd(&LinearMoveCmd{ID: 1})

	// Sequencer should be stuck waiting for motion
	time.Sleep(5 * time.Millisecond)
	if len(mot.calls) != 0 {
		t.Fatalf("expected 0 calls while waiting, got %v", mot.calls)
	}

	// Signal in-position
	stat.inPosition.Store(true)

	// Drain
	task.DrainQueue()

	if len(mot.calls) != 1 {
		t.Fatalf("expected 1 call after motion done, got %d: %v", len(mot.calls), mot.calls)
	}
}

func TestSequencer_Dwell(t *testing.T) {
	task, _, _, _ := newSeqTestTask()
	restore := SetPollInterval(10 * time.Microsecond)
	defer restore()

	task.StartSequencer()

	start := time.Now()
	task.EnqueueCmd(&DwellCmd{Seconds: 0.01}) // 10ms dwell
	task.DrainQueue()
	elapsed := time.Since(start)

	if elapsed < 8*time.Millisecond {
		t.Fatalf("dwell too short: %v", elapsed)
	}
}

func TestSequencer_MixedCommands(t *testing.T) {
	task, mot, _, _ := newSeqTestTask()
	restore := SetPollInterval(10 * time.Microsecond)
	defer restore()

	task.StartSequencer()

	task.EnqueueCmd(&SpindleOnCmd{Spindle: 0, Speed: 1000})
	task.EnqueueCmd(&LinearMoveCmd{ID: 1})
	task.EnqueueCmd(&LinearMoveCmd{ID: 2})
	task.EnqueueCmd(&SpindleOffCmd{Spindle: 0})

	task.DrainQueue()

	expected := []string{"SpindleOn", "SetLine", "SetLine", "SpindleOff"}
	if len(mot.calls) != len(expected) {
		t.Fatalf("expected %v, got %v", expected, mot.calls)
	}
	for i, e := range expected {
		if mot.calls[i] != e {
			t.Fatalf("call[%d]: expected %s, got %s", i, e, mot.calls[i])
		}
	}
}

func TestEnqueueCmd_NotRunning(t *testing.T) {
	task, _, _, _ := newSeqTestTask()

	err := task.EnqueueCmd(&LinearMoveCmd{ID: 1})
	if err == nil {
		t.Fatal("expected error when sequencer not running")
	}
}
