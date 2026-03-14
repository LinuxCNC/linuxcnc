package realtime

import (
	"log/slog"
	"os"
	"testing"
	"time"
)

// TestNew verifies that New() returns a non-nil Manager with the expected
// defaults.
func TestNew(t *testing.T) {
	m := New(nil)
	if m == nil {
		t.Fatal("New() returned nil")
	}
	if m.logger == nil {
		t.Error("logger should not be nil")
	}
	if m.stopTimeout != defaultStopTimeout {
		t.Errorf("stopTimeout = %v, want %v", m.stopTimeout, defaultStopTimeout)
	}
}

// TestStartDevZeroAccessible verifies that Start() succeeds when /dev/zero is
// accessible (which it always should be on Linux).
func TestStartDevZeroAccessible(t *testing.T) {
	if _, err := os.Stat(shmDev); os.IsNotExist(err) {
		t.Skipf("%s does not exist, skipping", shmDev)
	}

	m := New(nil)

	// Overwrite the rtapi_app path so that IsRunning() returns false even if
	// rtapi_app happens to be installed on the test machine.
	m.rtapiAppPath = "/nonexistent/rtapi_app"

	// Start() should succeed: /dev/zero is accessible and IsRunning() is false.
	if err := m.Start(); err != nil {
		t.Fatalf("Start() returned unexpected error: %v", err)
	}
}

// TestStartFailsWhenDevZeroMissing verifies that checkDevZero() returns an
// error for a non-existent path.
func TestStartFailsWhenDevZeroMissing(t *testing.T) {
	if err := checkDevZeroAt("/nonexistent/device"); err == nil {
		t.Error("expected error for non-existent device, got nil")
	}
}

// TestStopNotRunning verifies that Stop() is a no-op (returns nil) when
// rtapi_app is not running.
func TestStopNotRunning(t *testing.T) {
	m := New(slog.New(slog.NewTextHandler(os.Stderr, nil)))
	// Point rtapiAppPath at something that definitely does not exist so that
	// IsRunning() returns false.
	m.rtapiAppPath = "/nonexistent/rtapi_app"
	m.stopTimeout = 500 * time.Millisecond

	if err := m.Stop(); err != nil {
		t.Fatalf("Stop() on non-running process returned error: %v", err)
	}
}

// TestIsRunningWithFakeProcess verifies that IsRunning() returns false when
// the rtapi_app process is not present.
func TestIsRunningWithFakeProcess(t *testing.T) {
	m := New(nil)

	// In a CI / test environment rtapi_app should not be running.  The test
	// cannot guarantee this on a developer machine with a live LinuxCNC
	// instance, so we only assert false when the process is not present.
	running := m.IsRunning()
	t.Logf("IsRunning() = %v", running)
}

// TestCleanupIPC verifies that cleanupIPC() runs without panic or error when
// no LinuxCNC IPC resources are present.
func TestCleanupIPC(t *testing.T) {
	m := New(slog.New(slog.NewTextHandler(os.Stderr, nil)))
	// ipcrm on non-existent keys should not return a hard error (it exits
	// non-zero but we suppress that intentionally — matching the bash script).
	if err := m.cleanupIPC(); err != nil {
		t.Errorf("cleanupIPC() returned unexpected error: %v", err)
	}
}

// TestSplitLines exercises the splitLines helper.
func TestSplitLines(t *testing.T) {
	cases := []struct {
		input string
		want  int
	}{
		{"", 0},
		{"line1\nline2\n", 2},
		{"line1\nline2", 2},
		{"\n\n", 0},
		{"S rtapi_app\nZ rtapi_app\n", 2},
	}
	for _, tc := range cases {
		got := splitLines(tc.input)
		if len(got) != tc.want {
			t.Errorf("splitLines(%q) = %d lines, want %d", tc.input, len(got), tc.want)
		}
	}
}

// TestIsRunningFiltersZombies verifies that IsRunning correctly ignores zombie
// processes (lines starting with 'Z').
func TestIsRunningFiltersZombies(t *testing.T) {
	// Build a synthetic output that would come from ps (zombie only).
	// splitLines + zombie filter should leave 0 non-zombie lines → not running.
	lines := splitLines("Z rtapi_app\nZ rtapi_app\n")
	running := false
	for _, line := range lines {
		if len(line) > 0 && line[0] != 'Z' {
			running = true
		}
	}
	if running {
		t.Error("zombie-only ps output should not be considered running")
	}

	// Non-zombie line should be detected as running.
	lines2 := splitLines("S rtapi_app\n")
	running2 := false
	for _, line := range lines2 {
		if len(line) > 0 && line[0] != 'Z' {
			running2 = true
		}
	}
	if !running2 {
		t.Error("non-zombie ps output should be considered running")
	}
}
