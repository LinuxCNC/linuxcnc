package halfile

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/sittner/linuxcnc/src/launcher/inifile"
)

// writeTemp creates a temporary file with the given content and returns its path.
// The caller is responsible for removing it.
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
// Substitution tests
// ---------------------------------------------------------------------------

func TestSubstituteLine_NoMatch(t *testing.T) {
	ini := parseIni(t, "[EMC]\nMACHINE = TestMachine\n")
	e := New(ini, "/usr/bin/halcmd", "", nil, "")

	in := "loadrt trivkins"
	got := e.substituteLine(in)
	if got != in {
		t.Errorf("substituteLine(%q) = %q; want unchanged %q", in, got, in)
	}
}

func TestSubstituteLine_SimpleSubstitution(t *testing.T) {
	ini := parseIni(t, "[EMCMOT]\nSERVO_PERIOD = 1000000\n")
	e := New(ini, "/usr/bin/halcmd", "", nil, "")

	in := "loadrt motmod servo_period_nsec=[EMCMOT]SERVO_PERIOD"
	want := "loadrt motmod servo_period_nsec=1000000"
	got := e.substituteLine(in)
	if got != want {
		t.Errorf("substituteLine(%q) = %q; want %q", in, got, want)
	}
}

func TestSubstituteLine_MultiplePatterns(t *testing.T) {
	ini := parseIni(t, "[AXIS_X]\nMIN_LIMIT = -100\nMAX_LIMIT = 100\n")
	e := New(ini, "/usr/bin/halcmd", "", nil, "")

	in := "setp axis.0.min-limit [AXIS_X]MIN_LIMIT"
	want := "setp axis.0.min-limit -100"
	got := e.substituteLine(in)
	if got != want {
		t.Errorf("substituteLine(%q) = %q; want %q", in, got, want)
	}
}

func TestSubstituteLine_UnknownPatternUnchanged(t *testing.T) {
	ini := parseIni(t, "[EMC]\nMACHINE = TestMachine\n")
	e := New(ini, "/usr/bin/halcmd", "", nil, "")

	in := "setp something [UNKNOWN]KEY"
	got := e.substituteLine(in)
	if got != in {
		t.Errorf("substituteLine(%q) = %q; want unchanged %q", in, got, in)
	}
}

func TestSubstituteLine_NilIni(t *testing.T) {
	e := New(nil, "/usr/bin/halcmd", "", nil, "")

	in := "loadrt trivkins"
	got := e.substituteLine(in)
	if got != in {
		t.Errorf("substituteLine with nil INI(%q) = %q; want unchanged %q", in, got, in)
	}
}

// ---------------------------------------------------------------------------
// Path resolution tests
// ---------------------------------------------------------------------------

func TestResolvePath_AbsoluteExists(t *testing.T) {
	dir := t.TempDir()
	halPath := writeTemp(t, dir, "test.hal", "# empty")

	e := New(nil, "/usr/bin/halcmd", "", nil, "")
	got, err := e.resolvePath(halPath)
	if err != nil {
		t.Fatalf("resolvePath(%q) error: %v", halPath, err)
	}
	if got != halPath {
		t.Errorf("resolvePath(%q) = %q; want %q", halPath, got, halPath)
	}
}

func TestResolvePath_AbsoluteMissing(t *testing.T) {
	e := New(nil, "/usr/bin/halcmd", "", nil, "")
	_, err := e.resolvePath("/nonexistent/path/file.hal")
	if err == nil {
		t.Error("resolvePath for missing absolute path should return error")
	}
}

func TestResolvePath_RelativeInConfigDir(t *testing.T) {
	dir := t.TempDir()

	// Create the HAL file and an INI file in the same directory.
	writeTemp(t, dir, "spindle.hal", "# spindle")
	iniContent := "[HAL]\nHALFILE = spindle.hal\n"
	iniPath := writeTemp(t, dir, "machine.ini", iniContent)
	ini, err := inifile.Parse(iniPath)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}

	e := New(ini, "/usr/bin/halcmd", "", nil, "")
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

	e := New(nil, "/usr/bin/halcmd", halibDir, nil, "")
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
	e := New(nil, "/usr/bin/halcmd", "/nonexistent", nil, "")
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
	e := New(nil, "/usr/bin/halcmd", halibPath, nil, "")
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
	writeTemp(t, halibDir, "basic_sim.tcl", "# basic_sim")

	e := New(nil, "/usr/bin/halcmd", halibDir, nil, "")
	got, err := e.resolvePath("LIB:basic_sim.tcl")
	if err != nil {
		t.Fatalf("resolvePath(LIB:basic_sim.tcl) error: %v", err)
	}
	want := filepath.Join(halibDir, "basic_sim.tcl")
	if got != want {
		t.Errorf("resolvePath = %q; want %q", got, want)
	}
}

func TestResolvePath_LibPrefix_SkipsConfigDir(t *testing.T) {
	// The file exists ONLY in configDir, not in halibPath.
	// LIB: prefix must NOT find it in configDir.
	dir := t.TempDir()
	writeTemp(t, dir, "only_in_config.tcl", "# config-only")

	iniPath := writeTemp(t, dir, "machine.ini", "[HAL]\n")
	ini, err := inifile.Parse(iniPath)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}

	// Use a separate empty hallib dir so the file won't be found there.
	halibDir := t.TempDir()
	e := New(ini, "/usr/bin/halcmd", halibDir, nil, "")

	_, err = e.resolvePath("LIB:only_in_config.tcl")
	if err == nil {
		t.Error("resolvePath(LIB:...) should not find file in configDir")
	}
}

func TestResolvePath_LibPrefix_MultipleHalibDirs(t *testing.T) {
	dir1 := t.TempDir()
	dir2 := t.TempDir()
	writeTemp(t, dir2, "lib.tcl", "# lib")

	halibPath := dir1 + ":" + dir2
	e := New(nil, "/usr/bin/halcmd", halibPath, nil, "")
	got, err := e.resolvePath("LIB:lib.tcl")
	if err != nil {
		t.Fatalf("resolvePath(LIB:lib.tcl) error: %v", err)
	}
	want := filepath.Join(dir2, "lib.tcl")
	if got != want {
		t.Errorf("resolvePath = %q; want %q", got, want)
	}
}

func TestResolvePath_LibPrefix_NotFound(t *testing.T) {
	e := New(nil, "/usr/bin/halcmd", t.TempDir(), nil, "")
	_, err := e.resolvePath("LIB:missing.tcl")
	if err == nil {
		t.Error("resolvePath(LIB:missing.tcl) should return error when file not found")
	}
}

// ---------------------------------------------------------------------------
// ExecuteAll order test (no actual halcmd, only verifying INI reading)
// ---------------------------------------------------------------------------

func TestExecuteAll_ReadsHalfileEntriesInOrder(t *testing.T) {
	dir := t.TempDir()

	// Create two dummy HAL files.
	writeTemp(t, dir, "first.hal", "# first")
	writeTemp(t, dir, "second.hal", "# second")

	iniContent := "[HAL]\nHALFILE = first.hal\nHALFILE = second.hal\n"
	iniPath := writeTemp(t, dir, "machine.ini", iniContent)
	ini, err := inifile.Parse(iniPath)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}

	// Verify GetAll returns entries in order (underlying INI behaviour).
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

	// resolvePath should find both files in the config directory.
	e := New(ini, "/usr/bin/halcmd", "", nil, "")
	for _, f := range files {
		if _, err := e.resolvePath(f); err != nil {
			t.Errorf("resolvePath(%q) error: %v", f, err)
		}
	}
}

func TestExecuteAll_EmptyIni(t *testing.T) {
	ini := parseIni(t, "[EMC]\nMACHINE = TestMachine\n")
	e := New(ini, "/usr/bin/halcmd", "", nil, "")
	// ExecuteAll with no HALFILE entries should not error.
	// We cannot actually run halcmd in tests, so we rely on the INI having no
	// HALFILE entries so that no subprocess is spawned.
	files := ini.GetAll("HAL", "HALFILE")
	if len(files) != 0 {
		t.Skipf("unexpected HALFILE entries: %v", files)
	}
	if err := e.ExecuteAll(); err != nil {
		t.Errorf("ExecuteAll on empty HAL section error: %v", err)
	}
}

// TestExecuteAll_InterleavedOrder verifies that HALFILE and HALCMD entries are
// processed in INI-file order, not grouped by key type.
func TestExecuteAll_InterleavedOrder(t *testing.T) {
	dir := t.TempDir()

	iniContent := "[HAL]\nHALFILE = a.hal\nHALCMD = setp foo 1\nHALFILE = b.hal\n"
	iniPath := writeTemp(t, dir, "machine.ini", iniContent)
	ini, err := inifile.Parse(iniPath)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}

	// Verify that GetSection returns entries in the correct interleaved order.
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
}

// ---------------------------------------------------------------------------
// halibDir helper tests
// ---------------------------------------------------------------------------

func TestHalibDir_LastEntry(t *testing.T) {
	e := New(nil, "/usr/bin/halcmd", ".:/usr/share/linuxcnc/hallib", nil, "")
	got := e.halibDir()
	want := "/usr/share/linuxcnc/hallib"
	if got != want {
		t.Errorf("halibDir() = %q; want %q", got, want)
	}
}

func TestHalibDir_SingleEntry(t *testing.T) {
	e := New(nil, "/usr/bin/halcmd", "/some/hallib", nil, "")
	got := e.halibDir()
	want := "/some/hallib"
	if got != want {
		t.Errorf("halibDir() = %q; want %q", got, want)
	}
}

func TestHalibDir_Empty(t *testing.T) {
	e := New(nil, "/usr/bin/halcmd", "", nil, "")
	got := e.halibDir()
	if got != "" {
		t.Errorf("halibDir() with empty halibPath = %q; want empty string", got)
	}
}

// ---------------------------------------------------------------------------
// HALFILE field splitting tests
// ---------------------------------------------------------------------------

// TestHalfileFieldSplitting verifies that a HALFILE value with arguments is
// correctly split so that the filename is resolved and args are preserved.
// This tests the resolution step only (no halcmd/haltcl subprocess is spawned).
func TestHalfileFieldSplitting_ResolveFilename(t *testing.T) {
	dir := t.TempDir()
	writeTemp(t, dir, "test.hal", "# test")

	iniPath := writeTemp(t, dir, "machine.ini", "[HAL]\nHALFILE = test.hal -some-arg\n")
	ini, err := inifile.Parse(iniPath)
	if err != nil {
		t.Fatalf("parsing INI: %v", err)
	}

	e := New(ini, "/usr/bin/halcmd", "", nil, "")

	// Verify that resolvePath works on the filename portion after splitting.
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

	// The filename part should resolve correctly.
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
	writeTemp(t, halibDir, "basic_sim.tcl", "# basic_sim")

	e := New(nil, "/usr/bin/halcmd", halibDir, nil, "")

	// Simulate what ExecuteAll does: split "LIB:basic_sim.tcl -no_sim_spindle"
	raw := "LIB:basic_sim.tcl -no_sim_spindle"
	fields := strings.Fields(raw)
	if len(fields) != 2 {
		t.Fatalf("expected 2 fields, got %d", len(fields))
	}

	resolved, err := e.resolvePath(fields[0])
	if err != nil {
		t.Fatalf("resolvePath(%q) error: %v", fields[0], err)
	}
	want := filepath.Join(halibDir, "basic_sim.tcl")
	if resolved != want {
		t.Errorf("resolvePath = %q; want %q", resolved, want)
	}
	if !strings.HasSuffix(resolved, ".tcl") {
		t.Errorf("resolved path %q should have .tcl suffix", resolved)
	}
	if fields[1] != "-no_sim_spindle" {
		t.Errorf("arg = %q; want %q", fields[1], "-no_sim_spindle")
	}
}
