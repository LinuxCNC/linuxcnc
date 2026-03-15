package hal_test

import (
	"testing"

	"linuxcnc.org/hal"
)

// TestStartThreadsSignature verifies that StartThreads has the correct signature.
// The function must exist and return an error value.
func TestStartThreadsSignature(t *testing.T) {
	// Verify the function exists and has the right signature.
	// We do not call it because it requires a running HAL/RTAPI environment.
	var fn func() error = hal.StartThreads
	if fn == nil {
		t.Fatal("StartThreads must not be nil")
	}
}

// TestStopThreadsSignature verifies that StopThreads has the correct signature.
func TestStopThreadsSignature(t *testing.T) {
	var fn func() error = hal.StopThreads
	if fn == nil {
		t.Fatal("StopThreads must not be nil")
	}
}

// TestListComponentsSignature verifies that ListComponents has the correct signature.
func TestListComponentsSignature(t *testing.T) {
	var fn func() ([]string, error) = hal.ListComponents
	if fn == nil {
		t.Fatal("ListComponents must not be nil")
	}
}

// TestUnloadAllSignature verifies that UnloadAll has the correct signature.
func TestUnloadAllSignature(t *testing.T) {
	var fn func(int) error = hal.UnloadAll
	if fn == nil {
		t.Fatal("UnloadAll must not be nil")
	}
}

// TestListComponentsReturnType verifies that ListComponents returns the expected types.
// When HAL is not available this will return an error, which is acceptable.
func TestListComponentsReturnType(t *testing.T) {
	names, err := hal.ListComponents()
	if err != nil {
		// HAL not available in this test environment — skip runtime check.
		t.Skipf("HAL not available: %v", err)
	}
	// names must be a non-nil slice when no error is returned.
	if names == nil {
		t.Fatal("ListComponents returned nil slice without error")
	}
}
