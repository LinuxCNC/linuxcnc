package realtime

import (
	"os"
	"testing"
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
}

// TestStartDevZeroAccessible verifies that Start() succeeds when /dev/zero is
// accessible (which it always should be on Linux).
func TestStartDevZeroAccessible(t *testing.T) {
	if _, err := os.Stat(shmDev); os.IsNotExist(err) {
		t.Skipf("%s does not exist, skipping", shmDev)
	}

	m := New(nil)

	// Start() should succeed: /dev/zero is accessible.
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
