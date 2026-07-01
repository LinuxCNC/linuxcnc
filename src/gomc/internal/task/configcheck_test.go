// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

// parseTestINI is a test helper that writes INI content to a temp file and parses it.
func parseTestINI(t *testing.T, content string) *inifile.IniFile {
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

// ccContainsString returns true if any element in ss contains substr.
func ccContainsString(ss []string, substr string) bool {
	for _, s := range ss {
		if strings.Contains(s, substr) {
			return true
		}
	}
	return false
}

func TestConfigCheck_NoKinematics_SkipsValidation(t *testing.T) {
	ini := parseTestINI(t, `[EMC]
MACHINE = Test
`)
	r, err := runConfigCheck(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if r.hasErrors() {
		t.Errorf("configs without [KINS] should pass cleanly, got errors: %v", r.Errors)
	}
}

func TestConfigCheck_MissingJointsOnly(t *testing.T) {
	ini := parseTestINI(t, `[KINS]
KINEMATICS = trivkins
`)
	r, err := runConfigCheck(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !r.hasErrors() {
		t.Fatal("expected error for missing [KINS]JOINTS")
	}
	if !ccContainsString(r.Errors, "JOINTS") {
		t.Error("expected JOINTS error")
	}
}

func TestConfigCheck_NonTrivkins(t *testing.T) {
	ini := parseTestINI(t, `[KINS]
KINEMATICS = genserkins
JOINTS = 6
`)
	r, err := runConfigCheck(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if r.hasErrors() {
		t.Errorf("non-trivkins should not produce errors, got: %v", r.Errors)
	}
	if !ccContainsString(r.Warnings, "Unchecked") {
		t.Error("expected 'Unchecked' warning for non-trivkins")
	}
}

func TestConfigCheck_TrivkinsHappyPath(t *testing.T) {
	ini := parseTestINI(t, `[KINS]
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
	r, err := runConfigCheck(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if r.hasErrors() {
		t.Errorf("happy path should have no errors, got: %v", r.Errors)
	}
	if len(r.Warnings) > 0 {
		t.Errorf("happy path should have no warnings, got: %v", r.Warnings)
	}
}

func TestConfigCheck_JointMinLimitGreaterThanAxis(t *testing.T) {
	ini := parseTestINI(t, `[KINS]
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
	r, err := runConfigCheck(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !r.hasErrors() {
		t.Fatal("expected error for JOINT_0 MIN_LIMIT > AXIS_X MIN_LIMIT")
	}
	if !ccContainsString(r.Errors, "[JOINT_0]MIN_LIMIT > [AXIS_X]MIN_LIMIT") {
		t.Errorf("expected MIN_LIMIT error, got: %v", r.Errors)
	}
}

func TestConfigCheck_JointMaxLimitLessThanAxis(t *testing.T) {
	ini := parseTestINI(t, `[KINS]
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
	r, err := runConfigCheck(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !r.hasErrors() {
		t.Fatal("expected error for JOINT_0 MAX_LIMIT < AXIS_X MAX_LIMIT")
	}
	if !ccContainsString(r.Errors, "[JOINT_0]MAX_LIMIT < [AXIS_X]MAX_LIMIT") {
		t.Errorf("expected MAX_LIMIT error, got: %v", r.Errors)
	}
}

func TestConfigCheck_ExtraJointsWarning(t *testing.T) {
	ini := parseTestINI(t, `[KINS]
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
	r, err := runConfigCheck(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !ccContainsString(r.Warnings, "Extra joints specified=2") {
		t.Errorf("expected extra joints warning, got warnings: %v", r.Warnings)
	}
}

func TestConfigCheck_MultipleValuesWarning(t *testing.T) {
	ini := parseTestINI(t, `[KINS]
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
	r, err := runConfigCheck(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !ccContainsString(r.Warnings, "Unexpected multiple values [JOINT_0]MAX_VELOCITY") {
		t.Errorf("expected multiple values warning, got warnings: %v", r.Warnings)
	}
}

func TestConfigCheck_MultipleValuesWarningNamespace(t *testing.T) {
	ini := parseTestINI(t, `[KINS]
KINEMATICS = trivkins coordinates=X
JOINTS = 1

[TRAJ]
COORDINATES = X

[JOINT_0]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10

[AXIS_X]
MAX_VELOCITY = 10
MAX_ACCELERATION = 100
MIN_LIMIT = -10
MAX_LIMIT = 10

[mill2:JOINT_0]
MIN_LIMIT = -20
MAX_LIMIT = 20

[mill2:AXIS_X]
MIN_LIMIT = -20
MAX_LIMIT = 20
`)
	// With namespace "mill2", the overrides should NOT trigger false warnings.
	nsIni := ini.WithNamespace("mill2")
	r, err := runConfigCheck(nsIni)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	for _, w := range r.Warnings {
		if strings.Contains(w, "Unexpected multiple values") {
			t.Errorf("unexpected false positive warning with namespace: %s", w)
		}
	}
}

func TestConfigCheck_InconsistentCoordinates(t *testing.T) {
	ini := parseTestINI(t, `[KINS]
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
	r, err := runConfigCheck(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !ccContainsString(r.Warnings, "INCONSISTENT") {
		t.Errorf("expected inconsistent coordinates warning, got warnings: %v", r.Warnings)
	}
}

func TestConfigCheck_MissingVelocityAcceleration(t *testing.T) {
	ini := parseTestINI(t, `[KINS]
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
	r, err := runConfigCheck(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if r.hasErrors() {
		t.Errorf("missing velocity/accel should not be errors, got: %v", r.Errors)
	}
	if !ccContainsString(r.Warnings, "Unspecified [JOINT_0]MAX_VELOCITY") {
		t.Error("expected warning for missing JOINT_0 MAX_VELOCITY")
	}
	if !ccContainsString(r.Warnings, "Unspecified [JOINT_0]MAX_ACCELERATION") {
		t.Error("expected warning for missing JOINT_0 MAX_ACCELERATION")
	}
	if !ccContainsString(r.Warnings, "Unspecified [AXIS_X]MAX_VELOCITY") {
		t.Error("expected warning for missing AXIS_X MAX_VELOCITY")
	}
	if !ccContainsString(r.Warnings, "Unspecified [AXIS_X]MAX_ACCELERATION") {
		t.Error("expected warning for missing AXIS_X MAX_ACCELERATION")
	}
}

func TestConfigCheck_DefaultCoordinates(t *testing.T) {
	ini := parseTestINI(t, `[KINS]
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
	r, err := runConfigCheck(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if r.hasErrors() {
		t.Errorf("default coordinates should not produce errors: %v", r.Errors)
	}
	if ccContainsString(r.Warnings, "INCONSISTENT") {
		t.Error("should not warn about inconsistent coordinates when trivkins uses default set")
	}
}

func TestConfigCheck_DuplicateCoordinates(t *testing.T) {
	ini := parseTestINI(t, `[KINS]
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
	r, err := runConfigCheck(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if r.hasErrors() {
		t.Errorf("gantry config with consistent limits should not error: %v", r.Errors)
	}
}

func TestConfigCheck_FormatWarnings(t *testing.T) {
	r := &configCheckResult{
		KinsModule: "trivkins",
		Warnings:   []string{"warning one", "warning two"},
	}
	out := r.formatWarnings()
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

func TestConfigCheck_FormatErrors(t *testing.T) {
	r := &configCheckResult{
		KinsModule: "trivkins",
		Errors:     []string{"error one"},
	}
	out := r.formatErrors()
	if !strings.Contains(out, "(trivkins kinematics) ERROR:") {
		t.Error("output should contain kins module error header")
	}
	if !strings.Contains(out, "  error one") {
		t.Error("output should contain indented error")
	}
}

func TestConfigCheck_FormatWarnings_Empty(t *testing.T) {
	r := &configCheckResult{}
	if r.formatWarnings() != "" {
		t.Error("no warnings should produce empty string")
	}
}

func TestConfigCheck_FormatErrors_Empty(t *testing.T) {
	r := &configCheckResult{}
	if r.formatErrors() != "" {
		t.Error("no errors should produce empty string")
	}
}

func TestConfigCheck_ParseKinematics(t *testing.T) {
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

func TestConfigCheck_JointsForTrivkins(t *testing.T) {
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

func TestConfigCheck_MissingAxisMinLimitWarning(t *testing.T) {
	ini := parseTestINI(t, `[KINS]
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
	r, err := runConfigCheck(ini)
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if !r.hasErrors() {
		t.Fatal("expected error when joint MIN_LIMIT > default axis MIN_LIMIT")
	}
	if !ccContainsString(r.Warnings, "Unspecified [AXIS_X]MIN_LIMIT") {
		t.Error("expected warning about unspecified AXIS_X MIN_LIMIT")
	}
}
