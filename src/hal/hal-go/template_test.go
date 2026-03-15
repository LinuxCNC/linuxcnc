package hal

import (
	"strings"
	"testing"
)

func TestRenderHalTemplate_NoDirectives(t *testing.T) {
	input := "loadrt trivkins\naddf servo-thread\nstart\n"
	out, err := RenderHalTemplate("test.hal", input, &HalTemplateData{})
	if err != nil {
		t.Fatal(err)
	}
	if out != input {
		t.Errorf("expected passthrough, got %q", out)
	}
}

func TestRenderHalTemplate_INISubstitution(t *testing.T) {
	data := &HalTemplateData{
		INI: map[string]map[string]string{
			"AXIS_X": {"SCALE": "1000"},
		},
	}
	input := `setp axis.x.scale {{index .INI "AXIS_X" "SCALE"}}`
	out, err := RenderHalTemplate("test.hal", input, data)
	if err != nil {
		t.Fatal(err)
	}
	expected := "setp axis.x.scale 1000"
	if out != expected {
		t.Errorf("expected %q, got %q", expected, out)
	}
}

// TestRenderHalTemplate_INIFunction verifies the ini closure function works
// without the caller needing to pass .INI explicitly.
func TestRenderHalTemplate_INIFunction(t *testing.T) {
	data := &HalTemplateData{
		INI: map[string]map[string]string{
			"JOINT_0": {"P": "50"},
		},
	}
	input := `setp pid.0.Pgain {{ini "JOINT_0" "P"}}`
	out, err := RenderHalTemplate("test.hal", input, data)
	if err != nil {
		t.Fatal(err)
	}
	expected := "setp pid.0.Pgain 50"
	if out != expected {
		t.Errorf("expected %q, got %q", expected, out)
	}
}

func TestRenderHalTemplate_RangeAxes(t *testing.T) {
	data := &HalTemplateData{
		Axes: []string{"X", "Y", "Z"},
		INI:  map[string]map[string]string{},
	}
	input := "{{range .Axes}}loadrt pid names=pid.{{lower .}}\n{{end}}"
	out, err := RenderHalTemplate("test.hal", input, data)
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(out, "pid.x") || !strings.Contains(out, "pid.y") || !strings.Contains(out, "pid.z") {
		t.Errorf("expected pid.x/y/z, got %q", out)
	}
}

func TestRenderHalTemplate_MathFunctions(t *testing.T) {
	data := &HalTemplateData{INI: map[string]map[string]string{}}
	input := "setp comp.gain {{add 1.5 2.5}}"
	out, err := RenderHalTemplate("test.hal", input, data)
	if err != nil {
		t.Fatal(err)
	}
	if out != "setp comp.gain 4" {
		t.Errorf("expected 'setp comp.gain 4', got %q", out)
	}
}

// TestRenderHalTemplate_MathIntegerArgs verifies that integer literals work
// with math functions (Go templates do not auto-convert int → float64).
func TestRenderHalTemplate_MathIntegerArgs(t *testing.T) {
	data := &HalTemplateData{INI: map[string]map[string]string{}}
	input := "setp comp.out {{add 1 2}}"
	out, err := RenderHalTemplate("test.hal", input, data)
	if err != nil {
		t.Fatalf("add with integer args failed: %v", err)
	}
	if out != "setp comp.out 3" {
		t.Errorf("expected 'setp comp.out 3', got %q", out)
	}
}

// TestRenderHalTemplate_MathMixedArgs verifies mixed int/float inputs work.
func TestRenderHalTemplate_MathMixedArgs(t *testing.T) {
	data := &HalTemplateData{INI: map[string]map[string]string{}}
	input := "{{mul 3 2.5}}"
	out, err := RenderHalTemplate("test.hal", input, data)
	if err != nil {
		t.Fatalf("mul with mixed args failed: %v", err)
	}
	if out != "7.5" {
		t.Errorf("expected '7.5', got %q", out)
	}
}

func TestRenderHalTemplate_SeqIteration(t *testing.T) {
	data := &HalTemplateData{INI: map[string]map[string]string{}}
	input := "{{range seq 0 3}}joint.{{.}}.enable\n{{end}}"
	out, err := RenderHalTemplate("test.hal", input, data)
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(out, "joint.0.enable") || !strings.Contains(out, "joint.2.enable") {
		t.Errorf("expected joint.0-2, got %q", out)
	}
}

// TestRenderHalTemplate_CountFunction verifies the count helper produces
// a slice [0, 1, ..., n-1].
func TestRenderHalTemplate_CountFunction(t *testing.T) {
	data := &HalTemplateData{INI: map[string]map[string]string{}}
	input := "{{range count 3}}axis.{{.}}\n{{end}}"
	out, err := RenderHalTemplate("test.hal", input, data)
	if err != nil {
		t.Fatal(err)
	}
	if !strings.Contains(out, "axis.0") || !strings.Contains(out, "axis.1") || !strings.Contains(out, "axis.2") {
		t.Errorf("expected axis.0/1/2, got %q", out)
	}
}

// TestRenderHalTemplate_DivByZero verifies that dividing by zero returns an
// error rather than silently rendering NaN into the HAL output.
func TestRenderHalTemplate_DivByZero(t *testing.T) {
	data := &HalTemplateData{INI: map[string]map[string]string{}}
	_, err := RenderHalTemplate("test.hal", "{{div 1.0 0.0}}", data)
	if err == nil {
		t.Fatal("expected error for division by zero, got nil")
	}
	if !strings.Contains(err.Error(), "division by zero") {
		t.Errorf("expected 'division by zero' in error, got: %v", err)
	}
}

// TestRenderHalTemplate_ParseError verifies that a malformed template returns
// an error rather than panicking or returning empty output.
func TestRenderHalTemplate_ParseError(t *testing.T) {
	data := &HalTemplateData{INI: map[string]map[string]string{}}
	input := "loadrt pid {{range .Axes}" // missing {{end}}
	_, err := RenderHalTemplate("test.hal", input, data)
	if err == nil {
		t.Error("expected parse error for malformed template, got nil")
	}
}

func TestNewHalTemplateData_Axes(t *testing.T) {
	ini := map[string]map[string]string{
		"TRAJ": {"COORDINATES": "X Y Z"},
		"KINS": {"JOINTS": "3"},
	}
	data := NewHalTemplateData(ini)
	if len(data.Axes) != 3 {
		t.Errorf("expected 3 axes, got %d: %v", len(data.Axes), data.Axes)
	}
	if data.Joints != 3 {
		t.Errorf("expected 3 joints, got %d", data.Joints)
	}
}

// TestNewHalTemplateData_AxesConcatenated verifies that COORDINATES without
// spaces ("XYZ") is also parsed correctly into individual axes.
func TestNewHalTemplateData_AxesConcatenated(t *testing.T) {
	ini := map[string]map[string]string{
		"TRAJ": {"COORDINATES": "XYZ"},
		"KINS": {"JOINTS": "3"},
	}
	data := NewHalTemplateData(ini)
	if len(data.Axes) != 3 {
		t.Errorf("expected 3 axes for 'XYZ', got %d: %v", len(data.Axes), data.Axes)
	}
}
