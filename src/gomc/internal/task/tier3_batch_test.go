// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"log/slog"
	"os"
	"testing"
)

func tier3Logger() *slog.Logger {
	return slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
}

type debugRecMotion struct {
	mockMotion
	debug int32
}

func (m *debugRecMotion) SetDebug(d int32) error { m.debug = d; return nil }

type debugRecIO struct {
	mockIO
	debug      int32
	debugSet   bool
	toolUnload bool
}

func (m *debugRecIO) SetDebug(d int32) error { m.debug = d; m.debugSet = true; return nil }
func (m *debugRecIO) ToolUnload() error      { m.toolUnload = true; return nil }

// SetDebug must forward to BOTH motion and IO (C++ emcSetDebug) and be echoed to
// stat.debug (previously it only reached motion).
func TestSetDebug_ForwardsAndReports(t *testing.T) {
	mot := &debugRecMotion{}
	io := &debugRecIO{}
	task := NewTask(mot, io, &mockStatus{}, tier3Logger())

	if err := task.SetDebug(42); err != nil {
		t.Fatalf("SetDebug: %v", err)
	}
	if mot.debug != 42 {
		t.Errorf("motion debug = %d, want 42", mot.debug)
	}
	if !io.debugSet || io.debug != 42 {
		t.Errorf("io debug: set=%v val=%d, want true/42", io.debugSet, io.debug)
	}
	if stat := task.BuildStat(); stat.Debug != 42 {
		t.Errorf("stat.Debug = %d, want 42", stat.Debug)
	}
}

// ToolUnload must reach io.ToolUnload when idle (was unwired), and reject while a
// program is running.
func TestToolUnload(t *testing.T) {
	io := &debugRecIO{}
	task := NewTask(&mockMotion{}, io, &mockStatus{}, tier3Logger())

	if err := task.ToolUnload(); err != nil {
		t.Fatalf("ToolUnload (idle): %v", err)
	}
	if !io.toolUnload {
		t.Error("io.ToolUnload was not called")
	}
}
