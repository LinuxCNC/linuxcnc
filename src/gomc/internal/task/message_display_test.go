// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"sync"
	"testing"
)

// recordingDisplayPub records operator-display messages.
type recordingDisplayPub struct {
	mu       sync.Mutex
	displays []string
}

func (p *recordingDisplayPub) OperatorError(string)   {}
func (p *recordingDisplayPub) OperatorText(string)    {}
func (p *recordingDisplayPub) OperatorDisplay(s string) {
	p.mu.Lock()
	p.displays = append(p.displays, s)
	p.mu.Unlock()
}
func (p *recordingDisplayPub) got() []string {
	p.mu.Lock()
	defer p.mu.Unlock()
	return append([]string(nil), p.displays...)
}

// A G-code (MSG,...) must reach the operator-display channel (it previously only
// hit the logger), and in program order — Message enqueues a DisplayMsgCmd that
// the sequencer executes, rather than firing inline during read-ahead.
func TestCanon_MessageReachesOperatorDisplay(t *testing.T) {
	task, _, _ := newCanonTestTask()
	ep := &recordingDisplayPub{}
	task.SetErrorPublisher(ep)
	task.StartSequencer()
	defer task.StopSequencer()

	task.canon.Message("hello operator")
	task.DrainQueue()

	got := ep.got()
	if len(got) != 1 || got[0] != "hello operator" {
		t.Fatalf("operator display = %v, want [\"hello operator\"]", got)
	}
}
