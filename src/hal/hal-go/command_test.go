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

// ===== Signal command signature tests =====

// TestNewSigSignature verifies that NewSig has the correct signature.
func TestNewSigSignature(t *testing.T) {
	var fn func(string, hal.PinType) error = hal.NewSig
	_ = fn
}

// TestDelSigSignature verifies that DelSig has the correct signature.
func TestDelSigSignature(t *testing.T) {
	var fn func(string) error = hal.DelSig
	_ = fn
}

// TestSetSSignature verifies that SetS has the correct signature.
func TestSetSSignature(t *testing.T) {
	var fn func(string, string) error = hal.SetS
	_ = fn
}

// TestGetSSignature verifies that GetS has the correct signature.
func TestGetSSignature(t *testing.T) {
	var fn func(string) (string, error) = hal.GetS
	_ = fn
}

// TestSTypeSignature verifies that SType has the correct signature.
func TestSTypeSignature(t *testing.T) {
	var fn func(string) (hal.PinType, error) = hal.SType
	_ = fn
}

// ===== Pin/param value command signature tests =====

// TestSetPSignature verifies that SetP has the correct signature.
func TestSetPSignature(t *testing.T) {
	var fn func(string, string) error = hal.SetP
	_ = fn
}

// TestGetPSignature verifies that GetP has the correct signature.
func TestGetPSignature(t *testing.T) {
	var fn func(string) (string, error) = hal.GetP
	_ = fn
}

// TestPTypeSignature verifies that PType has the correct signature.
func TestPTypeSignature(t *testing.T) {
	var fn func(string) (hal.PinType, error) = hal.PType
	_ = fn
}

// ===== Link/net command signature tests =====

// TestNetSignature verifies that Net accepts a signal name plus variadic pin names.
func TestNetSignature(t *testing.T) {
	var fn func(string, ...string) error = hal.Net
	_ = fn
}

// TestLinkPSSignature verifies that LinkPS has the correct signature.
func TestLinkPSSignature(t *testing.T) {
	var fn func(string, string) error = hal.LinkPS
	_ = fn
}

// TestLinkSPSignature verifies that LinkSP has the correct signature.
func TestLinkSPSignature(t *testing.T) {
	var fn func(string, string) error = hal.LinkSP
	_ = fn
}

// TestUnlinkPSignature verifies that UnlinkP has the correct signature.
func TestUnlinkPSignature(t *testing.T) {
	var fn func(string) error = hal.UnlinkP
	_ = fn
}

// ===== Thread function command signature tests =====

// TestAddFSignature verifies that AddF has the correct signature.
func TestAddFSignature(t *testing.T) {
	var fn func(string, string, int) error = hal.AddF
	_ = fn
}

// TestDelFSignature verifies that DelF has the correct signature.
func TestDelFSignature(t *testing.T) {
	var fn func(string, string) error = hal.DelF
	_ = fn
}

// ===== RT component management signature tests =====

// TestLoadRTSignature verifies that LoadRT has the correct signature.
func TestLoadRTSignature(t *testing.T) {
	var fn func(string, ...string) error = hal.LoadRT
	_ = fn
}

// TestUnloadRTSignature verifies that UnloadRT has the correct signature.
func TestUnloadRTSignature(t *testing.T) {
	var fn func(string) error = hal.UnloadRT
	_ = fn
}

// ===== User-space component management signature tests =====

// TestLoadUSROptionsType verifies that LoadUSROptions has the expected fields.
func TestLoadUSROptionsType(t *testing.T) {
	opts := &hal.LoadUSROptions{
		WaitReady:   true,
		WaitName:    "mycomp",
		WaitExit:    false,
		NoStdin:     true,
		TimeoutSecs: 10,
	}
	_ = opts
}

// TestLoadUSRSignature verifies that LoadUSR has the correct signature.
func TestLoadUSRSignature(t *testing.T) {
	var fn func(*hal.LoadUSROptions, string, ...string) error = hal.LoadUSR
	_ = fn
}

// TestUnloadUSRSignature verifies that UnloadUSR has the correct signature.
func TestUnloadUSRSignature(t *testing.T) {
	var fn func(string) error = hal.UnloadUSR
	_ = fn
}

// TestUnloadSignature verifies that Unload has the correct signature.
func TestUnloadSignature(t *testing.T) {
	var fn func(string) error = hal.Unload
	_ = fn
}

// TestWaitUSRSignature verifies that WaitUSR has the correct signature.
func TestWaitUSRSignature(t *testing.T) {
	var fn func(string) error = hal.WaitUSR
	_ = fn
}

// ===== Lock/unlock signature tests =====

// TestLockSignature verifies that Lock has the correct signature.
func TestLockSignature(t *testing.T) {
	var fn func(string) error = hal.Lock
	_ = fn
}

// TestUnlockSignature verifies that Unlock has the correct signature.
func TestUnlockSignature(t *testing.T) {
	var fn func(string) error = hal.Unlock
	_ = fn
}

// ===== Query command signature tests =====

// TestListSignature verifies that List has the correct signature.
func TestListSignature(t *testing.T) {
	var fn func(string, ...string) ([]string, error) = hal.List
	_ = fn
}

// ===== Functional tests (no HAL required) =====

// TestNetArrowFiltering verifies that Net strips arrow tokens before passing to HAL.
// Since HAL is not running, the call will return an error — we only verify that
// providing only arrow tokens returns a Go-level error (not a panic).
func TestNetArrowFiltering(t *testing.T) {
	err := hal.Net("mysig", "=>", "<=", "<=>")
	// Must return an error (no pins after filtering), not panic.
	if err == nil {
		t.Fatal("Net with only arrow tokens must return an error")
	}
}

// TestLockInvalidLevel verifies that Lock rejects unknown level strings.
func TestLockInvalidLevel(t *testing.T) {
	err := hal.Lock("invalid_level")
	if err == nil {
		t.Fatal("Lock must return an error for unknown level")
	}
}

// TestListUnknownType verifies that List rejects unknown type strings.
func TestListUnknownType(t *testing.T) {
	_, err := hal.List("notatype")
	if err == nil {
		t.Fatal("List must return an error for unknown type")
	}
}
