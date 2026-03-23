package halcmd_test

import (
	"strings"
	"testing"

	halcmd "github.com/sittner/linuxcnc/src/launcher/internal/halcmd"
	hal "linuxcnc.org/hal"
)

// TestStartThreadsSignature verifies that StartThreads has the correct signature.
func TestStartThreadsSignature(t *testing.T) {
	var fn func() error = halcmd.StartThreads
	if fn == nil {
		t.Fatal("StartThreads must not be nil")
	}
}

// TestStopThreadsSignature verifies that StopThreads has the correct signature.
func TestStopThreadsSignature(t *testing.T) {
	var fn func() error = halcmd.StopThreads
	if fn == nil {
		t.Fatal("StopThreads must not be nil")
	}
}

// TestListComponentsSignature verifies that ListComponents has the correct signature.
func TestListComponentsSignature(t *testing.T) {
	var fn func() ([]string, error) = halcmd.ListComponents
	if fn == nil {
		t.Fatal("ListComponents must not be nil")
	}
}

// TestUnloadAllSignature verifies that UnloadAll has the correct signature.
func TestUnloadAllSignature(t *testing.T) {
	var fn func(int) error = halcmd.UnloadAll
	if fn == nil {
		t.Fatal("UnloadAll must not be nil")
	}
}

// TestListComponentsReturnType verifies that ListComponents returns the expected types.
// When HAL is not available this will return an error, which is acceptable.
func TestListComponentsReturnType(t *testing.T) {
	names, err := halcmd.ListComponents()
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
	var fn func(string, hal.PinType) error = halcmd.NewSig
	_ = fn
}

// TestDelSigSignature verifies that DelSig has the correct signature.
func TestDelSigSignature(t *testing.T) {
	var fn func(string) error = halcmd.DelSig
	_ = fn
}

// TestSetSSignature verifies that SetS has the correct signature.
func TestSetSSignature(t *testing.T) {
	var fn func(string, string) error = halcmd.SetS
	_ = fn
}

// TestGetSSignature verifies that GetS has the correct signature.
func TestGetSSignature(t *testing.T) {
	var fn func(string) (string, error) = halcmd.GetS
	_ = fn
}

// TestSTypeSignature verifies that SType has the correct signature.
func TestSTypeSignature(t *testing.T) {
	var fn func(string) (hal.PinType, error) = halcmd.SType
	_ = fn
}

// ===== Pin/param value command signature tests =====

// TestSetPSignature verifies that SetP has the correct signature.
func TestSetPSignature(t *testing.T) {
	var fn func(string, string) error = halcmd.SetP
	_ = fn
}

// TestGetPSignature verifies that GetP has the correct signature.
func TestGetPSignature(t *testing.T) {
	var fn func(string) (string, error) = halcmd.GetP
	_ = fn
}

// TestPTypeSignature verifies that PType has the correct signature.
func TestPTypeSignature(t *testing.T) {
	var fn func(string) (hal.PinType, error) = halcmd.PType
	_ = fn
}

// ===== Link/net command signature tests =====

// TestNetSignature verifies that Net accepts a signal name plus variadic pin names.
func TestNetSignature(t *testing.T) {
	var fn func(string, ...string) error = halcmd.Net
	_ = fn
}

// TestLinkPSSignature verifies that LinkPS has the correct signature.
func TestLinkPSSignature(t *testing.T) {
	var fn func(string, string) error = halcmd.LinkPS
	_ = fn
}

// TestLinkSPSignature verifies that LinkSP has the correct signature.
func TestLinkSPSignature(t *testing.T) {
	var fn func(string, string) error = halcmd.LinkSP
	_ = fn
}

// TestUnlinkPSignature verifies that UnlinkP has the correct signature.
func TestUnlinkPSignature(t *testing.T) {
	var fn func(string) error = halcmd.UnlinkP
	_ = fn
}

// ===== Thread function command signature tests =====

// TestAddFSignature verifies that AddF has the correct signature.
func TestAddFSignature(t *testing.T) {
	var fn func(string, string, int) error = halcmd.AddF
	_ = fn
}

// TestDelFSignature verifies that DelF has the correct signature.
func TestDelFSignature(t *testing.T) {
	var fn func(string, string) error = halcmd.DelF
	_ = fn
}

// ===== RT component management signature tests =====

// TestLoadRTSignature verifies that LoadRT has the correct signature.
func TestLoadRTSignature(t *testing.T) {
	var fn func(string, ...string) error = halcmd.LoadRT
	_ = fn
}

// TestUnloadRTSignature verifies that UnloadRT has the correct signature.
func TestUnloadRTSignature(t *testing.T) {
	var fn func(string) error = halcmd.UnloadRT
	_ = fn
}

// ===== User-space component management signature tests =====

// TestLoadUSROptionsType verifies that LoadUSROptions has the expected fields.
func TestLoadUSROptionsType(t *testing.T) {
	opts := &halcmd.LoadUSROptions{
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
	var fn func(*halcmd.LoadUSROptions, string, ...string) error = halcmd.LoadUSR
	_ = fn
}

// TestUnloadUSRSignature verifies that UnloadUSR has the correct signature.
func TestUnloadUSRSignature(t *testing.T) {
	var fn func(string) error = halcmd.UnloadUSR
	_ = fn
}

// TestUnloadSignature verifies that Unload has the correct signature.
func TestUnloadSignature(t *testing.T) {
	var fn func(string) error = halcmd.Unload
	_ = fn
}

// TestWaitUSRSignature verifies that WaitUSR has the correct signature.
func TestWaitUSRSignature(t *testing.T) {
	var fn func(string) error = halcmd.WaitUSR
	_ = fn
}

// ===== Lock/unlock signature tests =====

// TestLockSignature verifies that Lock has the correct signature.
func TestLockSignature(t *testing.T) {
	var fn func(string) error = halcmd.Lock
	_ = fn
}

// TestUnlockSignature verifies that Unlock has the correct signature.
func TestUnlockSignature(t *testing.T) {
	var fn func(string) error = halcmd.Unlock
	_ = fn
}

// ===== Query command signature tests =====

// TestListSignature verifies that List has the correct signature.
func TestListSignature(t *testing.T) {
	var fn func(string, ...string) ([]string, error) = halcmd.List
	_ = fn
}

// ===== Functional tests (no HAL required) =====

// TestNetArrowFiltering verifies that Net strips arrow tokens before passing to HAL.
func TestNetArrowFiltering(t *testing.T) {
	err := halcmd.Net("mysig", "=>", "<=", "<=>")
	// No pins after filtering — must return nil, not an error.
	if err != nil {
		t.Fatalf("Net with only arrow tokens must return nil, got: %v", err)
	}
}

// TestLockInvalidLevel verifies that Lock rejects unknown level strings.
func TestLockInvalidLevel(t *testing.T) {
	err := halcmd.Lock("invalid_level")
	if err == nil {
		t.Fatal("Lock must return an error for unknown level")
	}
}

// TestListUnknownType verifies that List rejects unknown type strings.
func TestListUnknownType(t *testing.T) {
	_, err := halcmd.List("notatype")
	if err == nil {
		t.Fatal("List must return an error for unknown type")
	}
}

// TestListRetainAccepted verifies that List("retain") is accepted as a valid type
// and does not return an "unknown type" error.
func TestListRetainAccepted(t *testing.T) {
	_, err := halcmd.List("retain")
	if err != nil {
		if strings.Contains(err.Error(), "unknown type") {
			t.Fatalf(`List("retain") must not return an "unknown type" error, got: %v`, err)
		}
		// Any other error (e.g. HAL not available) is acceptable.
		t.Skipf("HAL not available: %v", err)
	}
}

// TestNetNoPins verifies that Net with no pins (after arrow filtering) returns nil.
func TestNetNoPins(t *testing.T) {
	err := halcmd.Net("mysig")
	if err != nil {
		t.Fatalf("Net with no pins must return nil, got: %v", err)
	}
}

// TestUnlockSemantics verifies that Unlock("all") does not return an "unknown level" error.
func TestUnlockSemantics(t *testing.T) {
	err := halcmd.Unlock("all")
	if err != nil {
		const unknownLvl = "unknown lock level"
		if len(err.Error()) > len(unknownLvl) && err.Error()[:len(unknownLvl)] == unknownLvl {
			t.Fatalf("Unlock(\"all\") must not return an unknown-level error: %v", err)
		}
		t.Skipf("HAL not available: %v", err)
	}
}

// ===== Show/Save/Status/SetDebug signature tests =====

// TestShowSignature verifies that Show has the correct signature.
func TestShowSignature(t *testing.T) {
	var fn func(string, ...string) (*halcmd.ShowResult, error) = halcmd.Show
	_ = fn
}

// TestSaveSignature verifies that Save has the correct signature.
func TestSaveSignature(t *testing.T) {
	var fn func(string, string) ([]string, error) = halcmd.Save
	_ = fn
}

// TestStatusSignature verifies that Status has the correct signature.
func TestStatusSignature(t *testing.T) {
	var fn func() (*halcmd.StatusInfo, error) = halcmd.Status
	_ = fn
}

// TestSetDebugSignature verifies that SetDebug has the correct signature.
func TestSetDebugSignature(t *testing.T) {
	var fn func(int) error = halcmd.SetDebug
	_ = fn
}

// ===== Struct field tests =====

// TestPinInfoFields verifies that PinInfo has the expected fields.
func TestPinInfoFields(t *testing.T) {
	pi := halcmd.PinInfo{
		Name:      "mycomp.in",
		Type:      "bit",
		Direction: "IN",
		Value:     "FALSE",
		Signal:    "mysig",
		Owner:     "mycomp",
	}
	_ = pi
}

// TestSigInfoFields verifies that SigInfo has the expected fields.
func TestSigInfoFields(t *testing.T) {
	si := halcmd.SigInfo{
		Name:  "mysig",
		Type:  "float",
		Value: "0",
	}
	_ = si
}

// TestParamInfoFields verifies that ParamInfo has the expected fields.
func TestParamInfoFields(t *testing.T) {
	pi := halcmd.ParamInfo{
		Name:      "mycomp.gain",
		Type:      "float",
		Direction: "RW",
		Value:     "1.0",
		Owner:     "mycomp",
	}
	_ = pi
}

// TestFunctInfoFields verifies that FunctInfo has the expected fields.
func TestFunctInfoFields(t *testing.T) {
	fi := halcmd.FunctInfo{
		Name:  "mycomp.update",
		Owner: "mycomp",
	}
	_ = fi
}

// TestThreadInfoFields verifies that ThreadInfo has the expected fields.
func TestThreadInfoFields(t *testing.T) {
	ti := halcmd.ThreadInfo{
		Name:    "servo-thread",
		Period:  1000000,
		Functs:  []string{"mycomp.update"},
		Running: false,
	}
	_ = ti
}

// TestCompInfoFields verifies that CompInfo has the expected fields.
func TestCompInfoFields(t *testing.T) {
	ci := halcmd.CompInfo{
		Name: "mycomp",
		ID:   42,
		Type: "realtime",
	}
	_ = ci
}

// TestShowResultFields verifies that ShowResult has the expected fields.
func TestShowResultFields(t *testing.T) {
	sr := halcmd.ShowResult{
		Comps:   []halcmd.CompInfo{},
		Pins:    []halcmd.PinInfo{},
		Params:  []halcmd.ParamInfo{},
		Signals: []halcmd.SigInfo{},
		Functs:  []halcmd.FunctInfo{},
		Threads: []halcmd.ThreadInfo{},
	}
	_ = sr
}
