// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"log/slog"
	"os"
	"sync"
	"testing"
)

// probeSeqMotion records the order of ClearProbeFlags vs Probe.
type probeSeqMotion struct {
	mockMotion
	mu  sync.Mutex
	seq []string
}

func (m *probeSeqMotion) ClearProbeFlags() error {
	m.mu.Lock()
	m.seq = append(m.seq, "clear")
	m.mu.Unlock()
	return nil
}
func (m *probeSeqMotion) Probe(Pose, float64, float64, float64, int32, uint8, int32, float64) error {
	m.mu.Lock()
	m.seq = append(m.seq, "probe")
	m.mu.Unlock()
	return nil
}
func (m *probeSeqMotion) order() []string {
	m.mu.Lock()
	defer m.mu.Unlock()
	return append([]string(nil), m.seq...)
}

// TURN_PROBE_ON must clear the motion probe-tripped flag in program order,
// before the STRAIGHT_PROBE move — matching C++ TURN_PROBE_ON (which appends
// CLEAR_PROBE_TRIPPED_FLAG to the interp_list). Previously TurnProbeOn was a
// no-op, so the flag was never cleared at probe start.
func TestCanon_TurnProbeOnClearsBeforeProbe(t *testing.T) {
	mot := &probeSeqMotion{}
	st := &testStatus{}
	st.inPosition.Store(true)
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	task := NewTask(mot, &mockIO{}, st, logger)
	task.StartSequencer()
	defer task.StopSequencer()

	c := task.canon
	c.TurnProbeOn()
	c.StraightProbe(1, 0, 0, -5, 0, 0, 0, 0, 0, 0, 1) // G38.2 toward Z-5
	task.DrainQueue()

	got := mot.order()
	if len(got) != 2 || got[0] != "clear" || got[1] != "probe" {
		t.Fatalf("motion call order = %v, want [clear probe]", got)
	}
}
