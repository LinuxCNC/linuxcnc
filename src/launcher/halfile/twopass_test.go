package halfile

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/sittner/linuxcnc/src/launcher/inifile"
)

// ---------------------------------------------------------------------------
// findTwopassTcl search-order tests
// ---------------------------------------------------------------------------

func TestFindTwopassTcl_HalibDir(t *testing.T) {
	dir := t.TempDir()
	tclPath := filepath.Join(dir, "twopass.tcl")
	if err := os.WriteFile(tclPath, []byte("# twopass"), 0o600); err != nil {
		t.Fatalf("creating twopass.tcl: %v", err)
	}

	e := New(nil, "/usr/bin/halcmd", dir, nil)
	got, err := e.findTwopassTcl()
	if err != nil {
		t.Fatalf("findTwopassTcl() error: %v", err)
	}
	if got != tclPath {
		t.Errorf("findTwopassTcl() = %q; want %q", got, tclPath)
	}
}

func TestFindTwopassTcl_LinuxcncTclDir(t *testing.T) {
	tclDir := t.TempDir()
	tclPath := filepath.Join(tclDir, "twopass.tcl")
	if err := os.WriteFile(tclPath, []byte("# twopass"), 0o600); err != nil {
		t.Fatalf("creating twopass.tcl: %v", err)
	}

	t.Setenv("LINUXCNC_TCL_DIR", tclDir)

	// Use an empty halibDir so the HALLIB_DIR candidate is not generated.
	e := New(nil, "/usr/bin/halcmd", "", nil)
	got, err := e.findTwopassTcl()
	if err != nil {
		t.Fatalf("findTwopassTcl() error: %v", err)
	}
	if got != tclPath {
		t.Errorf("findTwopassTcl() = %q; want %q", got, tclPath)
	}
}

func TestFindTwopassTcl_LinuxcncHome(t *testing.T) {
	home := t.TempDir()
	tclDir := filepath.Join(home, "tcl")
	if err := os.MkdirAll(tclDir, 0o700); err != nil {
		t.Fatalf("mkdir: %v", err)
	}
	tclPath := filepath.Join(tclDir, "twopass.tcl")
	if err := os.WriteFile(tclPath, []byte("# twopass"), 0o600); err != nil {
		t.Fatalf("creating twopass.tcl: %v", err)
	}

	t.Setenv("LINUXCNC_TCL_DIR", "")
	t.Setenv("LINUXCNC_HOME", home)

	e := New(nil, "/usr/bin/halcmd", "", nil)
	got, err := e.findTwopassTcl()
	if err != nil {
		t.Fatalf("findTwopassTcl() error: %v", err)
	}
	if got != tclPath {
		t.Errorf("findTwopassTcl() = %q; want %q", got, tclPath)
	}
}

func TestFindTwopassTcl_RIPFallback(t *testing.T) {
	// Build a directory layout that matches a RIP build:
	//   <base>/lib/hallib/   ← halibDir
	//   <base>/tcl/          ← parent + "tcl"
	base := t.TempDir()
	halibDir := filepath.Join(base, "lib", "hallib")
	if err := os.MkdirAll(halibDir, 0o700); err != nil {
		t.Fatalf("mkdir: %v", err)
	}
	tclDir := filepath.Join(base, "lib", "tcl")
	if err := os.MkdirAll(tclDir, 0o700); err != nil {
		t.Fatalf("mkdir: %v", err)
	}
	tclPath := filepath.Join(tclDir, "twopass.tcl")
	if err := os.WriteFile(tclPath, []byte("# twopass"), 0o600); err != nil {
		t.Fatalf("creating twopass.tcl: %v", err)
	}

	t.Setenv("LINUXCNC_TCL_DIR", "")
	t.Setenv("LINUXCNC_HOME", "")

	e := New(nil, "/usr/bin/halcmd", halibDir, nil)
	got, err := e.findTwopassTcl()
	if err != nil {
		t.Fatalf("findTwopassTcl() error: %v", err)
	}
	if got != tclPath {
		t.Errorf("findTwopassTcl() = %q; want %q", got, tclPath)
	}
}

func TestFindTwopassTcl_NotFound(t *testing.T) {
	t.Setenv("LINUXCNC_TCL_DIR", "")
	t.Setenv("LINUXCNC_HOME", "")

	e := New(nil, "/usr/bin/halcmd", t.TempDir(), nil)
	_, err := e.findTwopassTcl()
	if err == nil {
		t.Fatal("findTwopassTcl() should return error when twopass.tcl is not found")
	}
	if !strings.Contains(err.Error(), "twopass.tcl not found") {
		t.Errorf("error %q should mention 'twopass.tcl not found'", err.Error())
	}
	if !strings.Contains(err.Error(), "searched:") {
		t.Errorf("error %q should list searched paths", err.Error())
	}
}

// ---------------------------------------------------------------------------
// ExecuteAll TWOPASS delegation tests
// ---------------------------------------------------------------------------

// TestExecuteAll_TwopassDelegates verifies that when [HAL]TWOPASS is set,
// ExecuteAll invokes the twopass.tcl script via haltcl rather than running
// HAL files individually.  We use a fake haltcl script that records its
// arguments to a temp file.
func TestExecuteAll_TwopassDelegates(t *testing.T) {
	dir := t.TempDir()

	// Create a minimal twopass.tcl in the halibDir so findTwopassTcl finds it.
	halibDir := t.TempDir()
	tclPath := filepath.Join(halibDir, "twopass.tcl")
	if err := os.WriteFile(tclPath, []byte("# twopass stub"), 0o600); err != nil {
		t.Fatalf("creating twopass.tcl: %v", err)
	}

	// Create a fake haltcl that records the path it was called with.
	recordFile := filepath.Join(dir, "called_with.txt")
	fakeHaltcl := filepath.Join(dir, "haltcl")
	script := "#!/bin/sh\necho \"$@\" > " + recordFile + "\n"
	if err := os.WriteFile(fakeHaltcl, []byte(script), 0o700); err != nil {
		t.Fatalf("creating fake haltcl: %v", err)
	}

	// Prepend our fake haltcl to PATH.
	origPath := os.Getenv("PATH")
	t.Setenv("PATH", dir+":"+origPath)

	// Build INI with TWOPASS set.
	iniContent := "[HAL]\nTWOPASS = on\nHALFILE = some.hal\n"
	iniPath := writeTemp(t, dir, "machine.ini", iniContent)
	ini, err := inifile.Parse(iniPath)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}

	e := New(ini, "/usr/bin/halcmd", halibDir, nil)
	if err := e.ExecuteAll(); err != nil {
		t.Fatalf("ExecuteAll() error: %v", err)
	}

	// The record file should exist (haltcl was called).
	data, err := os.ReadFile(recordFile)
	if err != nil {
		t.Fatalf("reading record file: %v — haltcl was not called", err)
	}

	// The arguments should include the path to twopass.tcl.
	args := strings.TrimSpace(string(data))
	if !strings.Contains(args, "twopass.tcl") {
		t.Errorf("haltcl was called with %q; expected twopass.tcl in arguments", args)
	}
}

// TestExecuteAll_NoTwopassNormalExecution verifies that when [HAL]TWOPASS is
// absent, ExecuteAll does NOT attempt to find twopass.tcl and proceeds with
// normal per-file execution.
func TestExecuteAll_NoTwopassNormalExecution(t *testing.T) {
	dir := t.TempDir()

	// INI without TWOPASS and without any HALFILE entries that would require
	// running a real halcmd.
	iniContent := "[HAL]\n"
	iniPath := writeTemp(t, dir, "machine.ini", iniContent)
	ini, err := inifile.Parse(iniPath)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}

	// Use an empty halibDir — findTwopassTcl would fail if called.
	e := New(ini, "/usr/bin/halcmd", t.TempDir(), nil)

	// Should succeed without calling haltcl (no TWOPASS, no HALFILE entries).
	if err := e.ExecuteAll(); err != nil {
		t.Errorf("ExecuteAll() without TWOPASS error: %v", err)
	}
}

// TestExecuteAll_TwopassMissingTclError verifies that a descriptive error is
// returned when [HAL]TWOPASS is set but twopass.tcl cannot be found.
func TestExecuteAll_TwopassMissingTclError(t *testing.T) {
	dir := t.TempDir()

	t.Setenv("LINUXCNC_TCL_DIR", "")
	t.Setenv("LINUXCNC_HOME", "")

	iniContent := "[HAL]\nTWOPASS = on\n"
	iniPath := writeTemp(t, dir, "machine.ini", iniContent)
	ini, err := inifile.Parse(iniPath)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}

	// Use a temp dir that has no twopass.tcl.
	e := New(ini, "/usr/bin/halcmd", t.TempDir(), nil)
	err = e.ExecuteAll()
	if err == nil {
		t.Fatal("ExecuteAll() should return error when twopass.tcl not found")
	}
	if !strings.Contains(err.Error(), "twopass.tcl") {
		t.Errorf("error %q should mention twopass.tcl", err.Error())
	}
}
