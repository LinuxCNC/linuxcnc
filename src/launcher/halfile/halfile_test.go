package halfile

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/sittner/linuxcnc/src/launcher/inifile"
)

// writeTemp creates a temporary file with the given content and returns its path.
func writeTemp(t *testing.T, dir, name, content string) string {
	t.Helper()
	path := filepath.Join(dir, name)
	if err := os.WriteFile(path, []byte(content), 0o600); err != nil {
		t.Fatalf("writing temp file %q: %v", path, err)
	}
	return path
}

// parseIni parses an INI string from a temp file and returns the IniFile.
func parseIni(t *testing.T, content string) *inifile.IniFile {
	t.Helper()
	dir := t.TempDir()
	path := writeTemp(t, dir, "test.ini", content)
	ini, err := inifile.Parse(path)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}
	return ini
}

// ---------------------------------------------------------------------------
// Path resolution tests
// ---------------------------------------------------------------------------

func TestResolvePath_AbsoluteExists(t *testing.T) {
	dir := t.TempDir()
	halPath := writeTemp(t, dir, "test.hal", "# empty")

	e := New(nil, "", nil, "")
	got, err := e.resolvePath(halPath)
	if err != nil {
		t.Fatalf("resolvePath(%q) error: %v", halPath, err)
	}
	if got != halPath {
		t.Errorf("resolvePath(%q) = %q; want %q", halPath, got, halPath)
	}
}

func TestResolvePath_AbsoluteMissing(t *testing.T) {
	e := New(nil, "", nil, "")
	_, err := e.resolvePath("/nonexistent/path/file.hal")
	if err == nil {
		t.Error("resolvePath for missing absolute path should return error")
	}
}

func TestResolvePath_RelativeInConfigDir(t *testing.T) {
	dir := t.TempDir()

	writeTemp(t, dir, "spindle.hal", "# spindle")
	iniContent := "[HAL]\nHALFILE = spindle.hal\n"
	iniPath := writeTemp(t, dir, "machine.ini", iniContent)
	ini, err := inifile.Parse(iniPath)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}

	e := New(ini, "", nil, "")
	got, err := e.resolvePath("spindle.hal")
	if err != nil {
		t.Fatalf("resolvePath error: %v", err)
	}
	want := filepath.Join(dir, "spindle.hal")
	if got != want {
		t.Errorf("resolvePath = %q; want %q", got, want)
	}
}

func TestResolvePath_RelativeInHalibPath(t *testing.T) {
	halibDir := t.TempDir()
	writeTemp(t, halibDir, "common.hal", "# common")

	e := New(nil, halibDir, nil, "")
	got, err := e.resolvePath("common.hal")
	if err != nil {
		t.Fatalf("resolvePath error: %v", err)
	}
	want := filepath.Join(halibDir, "common.hal")
	if got != want {
		t.Errorf("resolvePath = %q; want %q", got, want)
	}
}

func TestResolvePath_NotFound(t *testing.T) {
	e := New(nil, "/nonexistent", nil, "")
	_, err := e.resolvePath("missing.hal")
	if err == nil {
		t.Error("resolvePath for missing file should return error")
	}
}

func TestResolvePath_MultipleHalibDirs(t *testing.T) {
	dir1 := t.TempDir()
	dir2 := t.TempDir()
	writeTemp(t, dir2, "target.hal", "# target")

	halibPath := dir1 + ":" + dir2
	e := New(nil, halibPath, nil, "")
	got, err := e.resolvePath("target.hal")
	if err != nil {
		t.Fatalf("resolvePath error: %v", err)
	}
	want := filepath.Join(dir2, "target.hal")
	if got != want {
		t.Errorf("resolvePath = %q; want %q", got, want)
	}
}

func TestResolvePath_LibPrefix(t *testing.T) {
	halibDir := t.TempDir()
	writeTemp(t, halibDir, "basic_sim.hal", "# basic_sim")

	e := New(nil, halibDir, nil, "")
	got, err := e.resolvePath("LIB:basic_sim.hal")
	if err != nil {
		t.Fatalf("resolvePath(LIB:basic_sim.hal) error: %v", err)
	}
	want := filepath.Join(halibDir, "basic_sim.hal")
	if got != want {
		t.Errorf("resolvePath = %q; want %q", got, want)
	}
}

func TestResolvePath_LibPrefix_SkipsConfigDir(t *testing.T) {
	// The file exists ONLY in configDir, not in halibPath.
	// LIB: prefix must NOT find it in configDir.
	dir := t.TempDir()
	writeTemp(t, dir, "only_in_config.hal", "# config-only")

	iniPath := writeTemp(t, dir, "machine.ini", "[HAL]\n")
	ini, err := inifile.Parse(iniPath)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}

	halibDir := t.TempDir()
	e := New(ini, halibDir, nil, "")

	_, err = e.resolvePath("LIB:only_in_config.hal")
	if err == nil {
		t.Error("resolvePath(LIB:...) should not find file in configDir")
	}
}

func TestResolvePath_LibPrefix_MultipleHalibDirs(t *testing.T) {
	dir1 := t.TempDir()
	dir2 := t.TempDir()
	writeTemp(t, dir2, "lib.hal", "# lib")

	halibPath := dir1 + ":" + dir2
	e := New(nil, halibPath, nil, "")
	got, err := e.resolvePath("LIB:lib.hal")
	if err != nil {
		t.Fatalf("resolvePath(LIB:lib.hal) error: %v", err)
	}
	want := filepath.Join(dir2, "lib.hal")
	if got != want {
		t.Errorf("resolvePath = %q; want %q", got, want)
	}
}

func TestResolvePath_LibPrefix_NotFound(t *testing.T) {
	e := New(nil, t.TempDir(), nil, "")
	_, err := e.resolvePath("LIB:missing.hal")
	if err == nil {
		t.Error("resolvePath(LIB:missing.hal) should return error when file not found")
	}
}

// ---------------------------------------------------------------------------
// ExecuteAll tests (INI reading / path resolution; no actual HAL execution)
// ---------------------------------------------------------------------------

func TestExecuteAll_ReadsHalfileEntriesInOrder(t *testing.T) {
	dir := t.TempDir()

	writeTemp(t, dir, "first.hal", "# first")
	writeTemp(t, dir, "second.hal", "# second")

	iniContent := "[HAL]\nHALFILE = first.hal\nHALFILE = second.hal\n"
	iniPath := writeTemp(t, dir, "machine.ini", iniContent)
	ini, err := inifile.Parse(iniPath)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}

	files := ini.GetAll("HAL", "HALFILE")
	if len(files) != 2 {
		t.Fatalf("expected 2 HALFILE entries, got %d", len(files))
	}
	if files[0] != "first.hal" {
		t.Errorf("first HALFILE = %q; want %q", files[0], "first.hal")
	}
	if files[1] != "second.hal" {
		t.Errorf("second HALFILE = %q; want %q", files[1], "second.hal")
	}

	e := New(ini, "", nil, "")
	for _, f := range files {
		if _, err := e.resolvePath(f); err != nil {
			t.Errorf("resolvePath(%q) error: %v", f, err)
		}
	}
}

func TestExecuteAll_EmptyIni(t *testing.T) {
	ini := parseIni(t, "[EMC]\nMACHINE = TestMachine\n")
	e := New(ini, "", nil, "")
	files := ini.GetAll("HAL", "HALFILE")
	if len(files) != 0 {
		t.Skipf("unexpected HALFILE entries: %v", files)
	}
	if err := e.ExecuteAll(); err != nil {
		t.Errorf("ExecuteAll on empty HAL section error: %v", err)
	}
}

// TestExecuteAll_TclFileReturnsError verifies that a .tcl HALFILE entry
// causes a hard error (TCL HAL files are no longer supported).
func TestExecuteAll_TclFileReturnsError(t *testing.T) {
	dir := t.TempDir()
	writeTemp(t, dir, "twopass.tcl", "# tcl file")

	iniPath := writeTemp(t, dir, "machine.ini", "[HAL]\nHALFILE = twopass.tcl\n")
	ini, err := inifile.Parse(iniPath)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}

	e := New(ini, "", nil, "")
	err = e.ExecuteAll()
	if err == nil {
		t.Error("ExecuteAll with .tcl HALFILE should return an error")
	}
	if !strings.Contains(err.Error(), ".tcl") && !strings.Contains(err.Error(), "TCL") {
		t.Errorf("error should mention TCL, got: %v", err)
	}
}

// TestExecuteAll_SkipsHalcmdEntries verifies that ExecuteAll() only processes
// HALFILE entries and skips HALCMD entries.
func TestExecuteAll_SkipsHalcmdEntries(t *testing.T) {
	dir := t.TempDir()

	iniContent := "[HAL]\nHALFILE = a.hal\nHALCMD = setp foo 1\nHALFILE = b.hal\n"
	iniPath := writeTemp(t, dir, "machine.ini", iniContent)
	ini, err := inifile.Parse(iniPath)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}

	entries := ini.GetSection("HAL")
	if len(entries) != 3 {
		t.Fatalf("expected 3 HAL entries, got %d", len(entries))
	}

	wantKeys := []string{"HALFILE", "HALCMD", "HALFILE"}
	wantVals := []string{"a.hal", "setp foo 1", "b.hal"}
	for i, e := range entries {
		if e.Key != wantKeys[i] {
			t.Errorf("entry[%d].Key = %q; want %q", i, e.Key, wantKeys[i])
		}
		if e.Value != wantVals[i] {
			t.Errorf("entry[%d].Value = %q; want %q", i, e.Value, wantVals[i])
		}
	}

	var halfiles []string
	for _, e := range entries {
		if e.Key == "HALFILE" {
			halfiles = append(halfiles, e.Value)
		}
	}
	if len(halfiles) != 2 {
		t.Errorf("expected 2 HALFILE entries, got %d", len(halfiles))
	}
}

// ---------------------------------------------------------------------------
// ExecuteHalCommands tests
// ---------------------------------------------------------------------------

func TestExecuteHalCommands_ReadsHalcmdEntries(t *testing.T) {
	dir := t.TempDir()

	iniContent := "[HAL]\nHALFILE = a.hal\nHALCMD = setp foo 1\nHALCMD = setp bar 2\n"
	iniPath := writeTemp(t, dir, "machine.ini", iniContent)
	ini, err := inifile.Parse(iniPath)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}

	cmds := ini.GetAll("HAL", "HALCMD")
	if len(cmds) != 2 {
		t.Fatalf("expected 2 HALCMD entries, got %d", len(cmds))
	}
	if cmds[0] != "setp foo 1" {
		t.Errorf("cmds[0] = %q; want %q", cmds[0], "setp foo 1")
	}
	if cmds[1] != "setp bar 2" {
		t.Errorf("cmds[1] = %q; want %q", cmds[1], "setp bar 2")
	}
}

func TestExecuteHalCommands_EmptyIni(t *testing.T) {
	ini := parseIni(t, "[EMC]\nMACHINE = TestMachine\n")
	e := New(ini, "", nil, "")
	if err := e.ExecuteHalCommands(); err != nil {
		t.Errorf("ExecuteHalCommands on INI with no HALCMD entries: %v", err)
	}
}

// ---------------------------------------------------------------------------
// HALFILE field splitting tests
// ---------------------------------------------------------------------------

func TestHalfileFieldSplitting_ResolveFilename(t *testing.T) {
	dir := t.TempDir()
	writeTemp(t, dir, "test.hal", "# test")

	iniPath := writeTemp(t, dir, "machine.ini", "[HAL]\nHALFILE = test.hal -some-arg\n")
	ini, err := inifile.Parse(iniPath)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}

	e := New(ini, "", nil, "")

	entry := ini.GetSection("HAL")[0]
	fields := strings.Fields(entry.Value)
	if len(fields) != 2 {
		t.Fatalf("expected 2 fields, got %d: %v", len(fields), fields)
	}
	if fields[0] != "test.hal" {
		t.Errorf("fields[0] = %q; want %q", fields[0], "test.hal")
	}
	if fields[1] != "-some-arg" {
		t.Errorf("fields[1] = %q; want %q", fields[1], "-some-arg")
	}

	resolved, err := e.resolvePath(fields[0])
	if err != nil {
		t.Fatalf("resolvePath(%q) error: %v", fields[0], err)
	}
	want := filepath.Join(dir, "test.hal")
	if resolved != want {
		t.Errorf("resolvePath = %q; want %q", resolved, want)
	}
}

func TestHalfileFieldSplitting_LibWithArgs(t *testing.T) {
	halibDir := t.TempDir()
	writeTemp(t, halibDir, "basic_sim.hal", "# basic_sim")

	e := New(nil, halibDir, nil, "")

	raw := "LIB:basic_sim.hal -no_sim_spindle"
	fields := strings.Fields(raw)
	if len(fields) != 2 {
		t.Fatalf("expected 2 fields, got %d", len(fields))
	}

	resolved, err := e.resolvePath(fields[0])
	if err != nil {
		t.Fatalf("resolvePath(%q) error: %v", fields[0], err)
	}
	want := filepath.Join(halibDir, "basic_sim.hal")
	if resolved != want {
		t.Errorf("resolvePath = %q; want %q", resolved, want)
	}
	if fields[1] != "-no_sim_spindle" {
		t.Errorf("arg = %q; want %q", fields[1], "-no_sim_spindle")
	}
}
