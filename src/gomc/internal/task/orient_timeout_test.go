// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"log/slog"
	"os"
	"testing"
	"time"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/motstat"
)

// orientStatus reports a fixed OrientState for spindle 0 (1 = in progress,
// 0 = complete/idle, 2 = fault), so waitSpindleOriented can be driven directly.
type orientStatus struct {
	mockStatus
	orientState int32
}

func (s *orientStatus) GetStatus() (motstat.MotionStatus, error) {
	var ms motstat.MotionStatus
	ms.Spindles[0].OrientState = s.orientState
	return ms, nil
}

func newOrientTask(orientState int32) *Task {
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	t := NewTask(&testMotion{failAt: -1}, &mockIO{}, &orientStatus{orientState: orientState}, logger)
	t.seqAbort = make(chan struct{}) // never closed: exercises the timeout/complete paths, not abort
	return t
}

// A spindle stuck IN_PROGRESS must fault the wait once the M19 timeout elapses,
// not hang the run forever. Regression guard for the dropped orient timeout.
func TestWaitSpindleOriented_Timeout(t *testing.T) {
	restore := SetPollInterval(1 * time.Millisecond)
	defer restore()
	task := newOrientTask(1) // OrientState=1: never completes

	start := time.Now()
	err := task.waitSpindleOriented(0.03) // 30ms timeout
	elapsed := time.Since(start)

	if err == nil {
		t.Fatal("expected a timeout error from a stuck orient, got nil (would hang the run)")
	}
	if elapsed > 500*time.Millisecond {
		t.Errorf("orient wait returned after %v — timeout not enforced promptly", elapsed)
	}
	task.mu.Lock()
	es := task.execState
	task.mu.Unlock()
	if es != ExecError {
		t.Errorf("execState = %v, want ExecError after timeout", es)
	}
}

// The timeout must not disturb the normal completion path: an already-oriented
// spindle returns cleanly, well within the timeout.
func TestWaitSpindleOriented_CompletesBeforeTimeout(t *testing.T) {
	restore := SetPollInterval(1 * time.Millisecond)
	defer restore()
	task := newOrientTask(0) // OrientState=0: complete

	if err := task.waitSpindleOriented(10.0); err != nil {
		t.Fatalf("expected clean completion, got %v", err)
	}
	task.mu.Lock()
	es := task.execState
	task.mu.Unlock()
	if es != ExecDone {
		t.Errorf("execState = %v, want ExecDone", es)
	}
}

// A non-positive timeout keeps the legacy wait-indefinitely behavior; abort
// still ends it. (Confirms the nil-timer select case never fires spuriously.)
func TestWaitSpindleOriented_ZeroTimeoutWaitsThenAborts(t *testing.T) {
	restore := SetPollInterval(1 * time.Millisecond)
	defer restore()
	task := newOrientTask(1) // stuck in progress

	done := make(chan error, 1)
	go func() { done <- task.waitSpindleOriented(0) }()

	// With no timeout it must still be waiting after a spell...
	select {
	case err := <-done:
		t.Fatalf("expected indefinite wait with timeout=0, returned early: %v", err)
	case <-time.After(30 * time.Millisecond):
	}
	// ...and abort must end it.
	close(task.seqAbort)
	select {
	case <-done:
	case <-time.After(500 * time.Millisecond):
		t.Fatal("abort did not end the indefinite orient wait")
	}
}
