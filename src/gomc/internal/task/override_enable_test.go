// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"log/slog"
	"os"
	"testing"
)

// recordingEnableMotion records the override-enable calls (mockMotion leaves
// them as no-ops).
type recordingEnableMotion struct {
	mockMotion
	feedScale       int32
	feedHold        int32
	spindleScale    int32
	spindleScaleIdx int32
}

func (m *recordingEnableMotion) FeedScaleEnable(e int32) error { m.feedScale = e; return nil }
func (m *recordingEnableMotion) FeedHoldEnable(e int32) error  { m.feedHold = e; return nil }
func (m *recordingEnableMotion) SpindleScaleEnable(s, e int32) error {
	m.spindleScaleIdx = s
	m.spindleScale = e
	return nil
}

// The GUI-immediate override-enable toggles must reach motion with the right
// bool→int and (for spindle) the spindle index.
func TestSetOverrideEnable(t *testing.T) {
	mot := &recordingEnableMotion{}
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	task := NewTask(mot, &mockIO{}, &mockStatus{}, logger)

	if err := task.SetFeedOverrideEnable(false); err != nil {
		t.Fatal(err)
	}
	if mot.feedScale != 0 {
		t.Errorf("FeedScaleEnable(false) => %d, want 0", mot.feedScale)
	}
	if err := task.SetFeedOverrideEnable(true); err != nil {
		t.Fatal(err)
	}
	if mot.feedScale != 1 {
		t.Errorf("FeedScaleEnable(true) => %d, want 1", mot.feedScale)
	}

	_ = task.SetFeedHoldEnable(true)
	if mot.feedHold != 1 {
		t.Errorf("FeedHoldEnable(true) => %d, want 1", mot.feedHold)
	}
	_ = task.SetFeedHoldEnable(false)
	if mot.feedHold != 0 {
		t.Errorf("FeedHoldEnable(false) => %d, want 0", mot.feedHold)
	}

	_ = task.SetSpindleOverrideEnable(true, 1)
	if mot.spindleScale != 1 || mot.spindleScaleIdx != 1 {
		t.Errorf("SpindleScaleEnable(spindle=1,true) => idx=%d en=%d, want idx=1 en=1",
			mot.spindleScaleIdx, mot.spindleScale)
	}
}
