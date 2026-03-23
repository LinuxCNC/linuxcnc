package configcheck

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/sittner/linuxcnc/src/launcher/pkg/inifile"
)

// parseINI is a test helper that writes INI content to a temp file and parses it.
func parseINI(t *testing.T, content string) *inifile.IniFile {
	t.Helper()
	dir := t.TempDir()
	path := filepath.Join(dir, "test.ini")
	if err := os.WriteFile(path, []byte(content), 0o644); err != nil {
		t.Fatalf("writing test INI: %v", err)
	}
	ini, err := inifile.Parse(path)
	if err != nil {
		t.Fatalf("parsing test INI: %v", err)
	}
	return ini
}

// containsString returns true if any element in ss contains substr.
func containsString(ss []string, substr string) bool {
	for _, s := range ss {
		if strings.Contains(s, substr) {
			return true
		}
	}
	return false
}

func TestCheck_MissingKinematics(t *testing.T) {
	ini := parseINI(t, `[EMC]
MACHINE = Test
`)
	r, err := Check(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !r.HasErrors() {
		t.Fatal("expected errors for missing [KINS]KINEMATICS and JOINTS")
	}
	if !containsString(r.Errors, "Missing [KINS]KINEMATICS=") {
		t.Error("expected missing KINEMATICS error")
	}
	if !containsString(r.Errors, "Missing [KINS]JOINTS=") {
		t.Error("expected missing JOINTS error")
	}
}

func TestCheck_MissingJointsOnly(t *testing.T) {
	ini := parseINI(t, `[KINS]
KINEMATICS = trivkins
`)
	r, err := Check(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !r.HasErrors() {
		t.Fatal("expected error for missing [KINS]JOINTS")
	}
	if containsString(r.Errors, "KINEMATICS") {
		t.Error("should not complain about KINEMATICS when it is present")
	}
}

func TestCheck_NonTrivkins(t *testing.T) {
	ini := parseINI(t, `[KINS]
KINEMATICS = genserkins
JOINTS = 6
`)
	r, err := Check(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if r.HasErrors() {
		t.Errorf("non-trivkins should not produce errors, got: %v", r.Errors)
	}
	if !containsString(r.Warnings, "Unchecked") {
		t.Error("expected 'Unchecked' warning for non-trivkins")
	}
}

func TestCheck_TrivkinsHappyPath(t *testing.T) {
	ini := parseINI(t, `[KINS]
KINEMATICS = trivkins coordinates=XZ
JOINTS = 2

[TRAJ]
COORDINATES = XZ

[JOINT_0]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10

[JOINT_1]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -5
MAX_LIMIT = 5

[AXIS_X]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10

[AXIS_Z]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -5
MAX_LIMIT = 5
`)
	r, err := Check(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if r.HasErrors() {
		t.Errorf("happy path should have no errors, got: %v", r.Errors)
	}
	if len(r.Warnings) > 0 {
		t.Errorf("happy path should have no warnings, got: %v", r.Warnings)
	}
}

func TestCheck_JointMinLimitGreaterThanAxis(t *testing.T) {
	ini := parseINI(t, `[KINS]
KINEMATICS = trivkins coordinates=X
JOINTS = 1

[TRAJ]
COORDINATES = X

[JOINT_0]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -5
MAX_LIMIT = 10

[AXIS_X]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10
`)
	r, err := Check(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !r.HasErrors() {
		t.Fatal("expected error for JOINT_0 MIN_LIMIT > AXIS_X MIN_LIMIT")
	}
	if !containsString(r.Errors, "[JOINT_0]MIN_LIMIT > [AXIS_X]MIN_LIMIT") {
		t.Errorf("expected MIN_LIMIT error, got: %v", r.Errors)
	}
}

func TestCheck_JointMaxLimitLessThanAxis(t *testing.T) {
	ini := parseINI(t, `[KINS]
KINEMATICS = trivkins coordinates=X
JOINTS = 1

[TRAJ]
COORDINATES = X

[JOINT_0]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 5

[AXIS_X]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10
`)
	r, err := Check(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !r.HasErrors() {
		t.Fatal("expected error for JOINT_0 MAX_LIMIT < AXIS_X MAX_LIMIT")
	}
	if !containsString(r.Errors, "[JOINT_0]MAX_LIMIT < [AXIS_X]MAX_LIMIT") {
		t.Errorf("expected MAX_LIMIT error, got: %v", r.Errors)
	}
}

func TestCheck_ExtraJointsWarning(t *testing.T) {
	ini := parseINI(t, `[KINS]
KINEMATICS = trivkins coordinates=X
JOINTS = 3

[TRAJ]
COORDINATES = X

[EMCMOT]
EMCMOT = motmod num_extrajoints=2

[JOINT_0]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10

[JOINT_1]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100

[JOINT_2]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100

[AXIS_X]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10
`)
	r, err := Check(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !containsString(r.Warnings, "Extra joints specified=2") {
		t.Errorf("expected extra joints warning, got warnings: %v", r.Warnings)
	}
}

func TestCheck_MultipleValuesWarning(t *testing.T) {
	ini := parseINI(t, `[KINS]
KINEMATICS = trivkins coordinates=X
JOINTS = 1

[TRAJ]
COORDINATES = X

[JOINT_0]
MAX_VELOCITY = 10
MAX_VELOCITY = 20
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10

[AXIS_X]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10
`)
	r, err := Check(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !containsString(r.Warnings, "Unexpected multiple values [JOINT_0]MAX_VELOCITY") {
		t.Errorf("expected multiple values warning, got warnings: %v", r.Warnings)
	}
}

func TestCheck_InconsistentCoordinates(t *testing.T) {
	ini := parseINI(t, `[KINS]
KINEMATICS = trivkins coordinates=XZ
JOINTS = 2

[TRAJ]
COORDINATES = XYZ

[JOINT_0]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10

[JOINT_1]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -5
MAX_LIMIT = 5

[AXIS_X]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10

[AXIS_Z]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -5
MAX_LIMIT = 5
`)
	r, err := Check(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !containsString(r.Warnings, "INCONSISTENT") {
		t.Errorf("expected inconsistent coordinates warning, got warnings: %v", r.Warnings)
	}
}

func TestCheck_MissingVelocityAcceleration(t *testing.T) {
	ini := parseINI(t, `[KINS]
KINEMATICS = trivkins coordinates=X
JOINTS = 1

[TRAJ]
COORDINATES = X

[JOINT_0]
MIN_LIMIT = -10
MAX_LIMIT = 10

[AXIS_X]
MIN_LIMIT = -10
MAX_LIMIT = 10
`)
	r, err := Check(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if r.HasErrors() {
		t.Errorf("missing velocity/accel should not be errors, got: %v", r.Errors)
	}
	if !containsString(r.Warnings, "Unspecified [JOINT_0]MAX_VELOCITY") {
		t.Error("expected warning for missing JOINT_0 MAX_VELOCITY")
	}
	if !containsString(r.Warnings, "Unspecified [JOINT_0]MAX_ACCELERATION") {
		t.Error("expected warning for missing JOINT_0 MAX_ACCELERATION")
	}
	if !containsString(r.Warnings, "Unspecified [AXIS_X]MAX_VELOCITY") {
		t.Error("expected warning for missing AXIS_X MAX_VELOCITY")
	}
	if !containsString(r.Warnings, "Unspecified [AXIS_X]MAX_ACCELERATION") {
		t.Error("expected warning for missing AXIS_X MAX_ACCELERATION")
	}
}

func TestCheck_DefaultCoordinates(t *testing.T) {
	// When coordinates= is not specified, all 9 axes are assumed,
	// and [TRAJ]COORDINATES consistency check is skipped.
	ini := parseINI(t, `[KINS]
KINEMATICS = trivkins
JOINTS = 3

[JOINT_0]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10
[JOINT_1]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10
[JOINT_2]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10

[AXIS_X]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10
[AXIS_Y]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10
[AXIS_Z]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10

[TRAJ]
COORDINATES = XYZ
`)
	r, err := Check(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if r.HasErrors() {
		t.Errorf("default coordinates should not produce errors: %v", r.Errors)
	}
	// With default coordinates (XYZABCUVW), no inconsistency warning should appear
	// even though [TRAJ]COORDINATES=XYZ differs.
	if containsString(r.Warnings, "INCONSISTENT") {
		t.Error("should not warn about inconsistent coordinates when trivkins uses default set")
	}
}

func TestCheck_DuplicateCoordinates(t *testing.T) {
	// e.g. gantry: trivkins coordinates=XYYZ  -> joint 0=X, 1=Y, 2=Y, 3=Z
	ini := parseINI(t, `[KINS]
KINEMATICS = trivkins coordinates=XYYZ
JOINTS = 4

[TRAJ]
COORDINATES = XYYZ

[JOINT_0]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10
[JOINT_1]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10
[JOINT_2]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10
[JOINT_3]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -5
MAX_LIMIT = 5

[AXIS_X]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10
[AXIS_Y]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10
[AXIS_Z]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -5
MAX_LIMIT = 5
`)
	r, err := Check(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if r.HasErrors() {
		t.Errorf("gantry config with consistent limits should not error: %v", r.Errors)
	}
}

func TestFormatWarnings(t *testing.T) {
	r := &Result{
		KinsModule: "trivkins",
		Warnings:   []string{"warning one", "warning two"},
	}
	out := r.FormatWarnings()
	if !strings.Contains(out, "check_config") {
		t.Error("output should contain progname")
	}
	if !strings.Contains(out, "(trivkins kinematics) WARNING:") {
		t.Error("output should contain kins module warning header")
	}
	if !strings.Contains(out, "  warning one") {
		t.Error("output should contain indented warning")
	}
}

func TestFormatErrors(t *testing.T) {
	r := &Result{
		KinsModule: "trivkins",
		Errors:     []string{"error one"},
	}
	out := r.FormatErrors()
	if !strings.Contains(out, "(trivkins kinematics) ERROR:") {
		t.Error("output should contain kins module error header")
	}
	if !strings.Contains(out, "  error one") {
		t.Error("output should contain indented error")
	}
}

func TestFormatWarnings_Empty(t *testing.T) {
	r := &Result{}
	if r.FormatWarnings() != "" {
		t.Error("no warnings should produce empty string")
	}
}

func TestFormatErrors_Empty(t *testing.T) {
	r := &Result{}
	if r.FormatErrors() != "" {
		t.Error("no errors should produce empty string")
	}
}

func TestParseKinematics(t *testing.T) {
	tests := []struct {
		input      string
		wantModule string
		wantParams map[string]string
	}{
		{"trivkins", "trivkins", map[string]string{}},
		{"trivkins coordinates=XZ", "trivkins", map[string]string{"coordinates": "XZ"}},
		{"trivkins coordinates=XZ kinstype=BOTH", "trivkins", map[string]string{"coordinates": "XZ", "kinstype": "BOTH"}},
		{"genserkins", "genserkins", map[string]string{}},
		{"", "", map[string]string{}},
	}
	for _, tc := range tests {
		module, params := parseKinematics(tc.input)
		if module != tc.wantModule {
			t.Errorf("parseKinematics(%q): module=%q, want %q", tc.input, module, tc.wantModule)
		}
		for k, v := range tc.wantParams {
			if params[k] != v {
				t.Errorf("parseKinematics(%q): params[%q]=%q, want %q", tc.input, k, params[k], v)
			}
		}
	}
}

func TestJointsForTrivkins(t *testing.T) {
	// XYYZ -> X:[0], Y:[1,2], Z:[3]
	idx := jointsForTrivkins([]byte("XYYZ"))
	if got := idx['X']; len(got) != 1 || got[0] != 0 {
		t.Errorf("X joints = %v, want [0]", got)
	}
	if got := idx['Y']; len(got) != 2 || got[0] != 1 || got[1] != 2 {
		t.Errorf("Y joints = %v, want [1,2]", got)
	}
	if got := idx['Z']; len(got) != 1 || got[0] != 3 {
		t.Errorf("Z joints = %v, want [3]", got)
	}
}

func TestCheck_MissingAxisMinLimitWarning(t *testing.T) {
	// When AXIS_X has no MIN_LIMIT and JOINT_0 MIN_LIMIT > default (-1e99),
	// both a warning (unspecified) and an error (limit mismatch) should appear.
	ini := parseINI(t, `[KINS]
KINEMATICS = trivkins coordinates=X
JOINTS = 1

[TRAJ]
COORDINATES = X

[JOINT_0]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -5
MAX_LIMIT = 10

[AXIS_X]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MAX_LIMIT = 10
`)
	r, err := Check(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !r.HasErrors() {
		t.Fatal("expected error when joint MIN_LIMIT > default axis MIN_LIMIT")
	}
	if !containsString(r.Warnings, "Unspecified [AXIS_X]MIN_LIMIT") {
		t.Error("expected warning about unspecified AXIS_X MIN_LIMIT")
	}
}
