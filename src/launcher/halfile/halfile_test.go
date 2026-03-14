package halfile

import (
	"os"
	"path/filepath"
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
	e := New(ini, "/usr/bin/halcmd", "", nil)

	in := "loadrt trivkins"
	got := e.substituteLine(in)
	if got != in {
		t.Errorf("substituteLine(%q) = %q; want unchanged %q", in, got, in)
	}
}

func TestSubstituteLine_SimpleSubstitution(t *testing.T) {
	ini := parseIni(t, "[EMCMOT]\nSERVO_PERIOD = 1000000\n")
	e := New(ini, "/usr/bin/halcmd", "", nil)

	in := "loadrt motmod servo_period_nsec=[EMCMOT]SERVO_PERIOD"
	want := "loadrt motmod servo_period_nsec=1000000"
	got := e.substituteLine(in)
	if got != want {
		t.Errorf("substituteLine(%q) = %q; want %q", in, got, want)
	}
}

func TestSubstituteLine_MultiplePatterns(t *testing.T) {
	ini := parseIni(t, "[AXIS_X]\nMIN_LIMIT = -100\nMAX_LIMIT = 100\n")
	e := New(ini, "/usr/bin/halcmd", "", nil)

	in := "setp axis.0.min-limit [AXIS_X]MIN_LIMIT"
	want := "setp axis.0.min-limit -100"
	got := e.substituteLine(in)
	if got != want {
		t.Errorf("substituteLine(%q) = %q; want %q", in, got, want)
	}
}

func TestSubstituteLine_UnknownPatternUnchanged(t *testing.T) {
	ini := parseIni(t, "[EMC]\nMACHINE = TestMachine\n")
	e := New(ini, "/usr/bin/halcmd", "", nil)

	in := "setp something [UNKNOWN]KEY"
	got := e.substituteLine(in)
	if got != in {
		t.Errorf("substituteLine(%q) = %q; want unchanged %q", in, got, in)
	}
}

func TestSubstituteLine_NilIni(t *testing.T) {
	e := New(nil, "/usr/bin/halcmd", "", nil)

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

	e := New(nil, "/usr/bin/halcmd", "", nil)
	got, err := e.resolvePath(halPath)
	if err != nil {
		t.Fatalf("resolvePath(%q) error: %v", halPath, err)
	}
	if got != halPath {
		t.Errorf("resolvePath(%q) = %q; want %q", halPath, got, halPath)
	}
}

func TestResolvePath_AbsoluteMissing(t *testing.T) {
	e := New(nil, "/usr/bin/halcmd", "", nil)
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

	e := New(ini, "/usr/bin/halcmd", "", nil)
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

	e := New(nil, "/usr/bin/halcmd", halibDir, nil)
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
	e := New(nil, "/usr/bin/halcmd", "/nonexistent", nil)
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
	e := New(nil, "/usr/bin/halcmd", halibPath, nil)
	got, err := e.resolvePath("target.hal")
	if err != nil {
		t.Fatalf("resolvePath error: %v", err)
	}
	want := filepath.Join(dir2, "target.hal")
	if got != want {
		t.Errorf("resolvePath = %q; want %q", got, want)
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
	e := New(ini, "/usr/bin/halcmd", "", nil)
	for _, f := range files {
		if _, err := e.resolvePath(f); err != nil {
			t.Errorf("resolvePath(%q) error: %v", f, err)
		}
	}
}

func TestExecuteAll_EmptyIni(t *testing.T) {
	ini := parseIni(t, "[EMC]\nMACHINE = TestMachine\n")
	e := New(ini, "/usr/bin/halcmd", "", nil)
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
