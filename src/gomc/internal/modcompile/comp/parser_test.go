// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package comp

import (
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/internal/modcompile/ast"
)

// Root of the linuxcnc source tree, relative to this test file.
func sourceRoot() string {
	dir, _ := os.Getwd()
	return filepath.Join(dir, "..", "..", "..", "..", "..")
}

func findCompDir() string {
	return filepath.Join(sourceRoot(), "src", "hal", "components")
}

func findDriverCompDir() string {
	return filepath.Join(sourceRoot(), "src", "hal", "drivers")
}

func findUserCompDir() string {
	return filepath.Join(sourceRoot(), "src", "hal", "user_comps")
}

func TestParseSimpleComp(t *testing.T) {
	src := `component and2 "Two-input AND gate";
pin in bit in0;
pin in bit in1;
pin out bit out "out is computed from the value of in0 and in1";
function _ nofp;
license "GPL";
;;
FUNCTION(_) { out = in0 && in1; }
`
	pkg, err := Parse("and2.comp", src)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}
	c := pkg.Component
	if c.Name != "and2" {
		t.Errorf("Name = %q, want %q", c.Name, "and2")
	}
	if c.Summary != "Two-input AND gate" {
		t.Errorf("Summary = %q, want %q", c.Summary, "Two-input AND gate")
	}
	if c.License != "GPL" {
		t.Errorf("License = %q, want %q", c.License, "GPL")
	}
	if len(c.Pins) != 3 {
		t.Fatalf("len(Pins) = %d, want 3", len(c.Pins))
	}
	if c.Pins[0].Name != "in0" || c.Pins[0].Dir != ast.PinIn || c.Pins[0].Type != ast.HALBit {
		t.Errorf("Pin[0] = %+v", c.Pins[0])
	}
	if c.Pins[2].Name != "out" || c.Pins[2].Dir != ast.PinOut {
		t.Errorf("Pin[2] = %+v", c.Pins[2])
	}
	if len(c.Functions) != 1 {
		t.Fatalf("len(Functions) = %d, want 1", len(c.Functions))
	}
	if c.Functions[0].Name != "_" || c.Functions[0].FP != false {
		t.Errorf("Function[0] = %+v", c.Functions[0])
	}
	if !strings.Contains(c.VerbatimC, "FUNCTION(_)") {
		t.Errorf("VerbatimC doesn't contain FUNCTION: %q", c.VerbatimC)
	}
}

func TestParsePersonalityPin(t *testing.T) {
	src := `component test "test";
pin in bit hall1 if personality & 0x01 "Hall sensor";
function _;
license "GPL";
;;
`
	pkg, err := Parse("test.comp", src)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}
	if len(pkg.Component.Pins) != 1 {
		t.Fatalf("len(Pins) = %d, want 1", len(pkg.Component.Pins))
	}
	pin := pkg.Component.Pins[0]
	if pin.Personality != "personality & 0x01" {
		t.Errorf("Personality = %q, want %q", pin.Personality, "personality & 0x01")
	}
}

func TestParseArrayPin(t *testing.T) {
	src := `component test "test";
pin out bit bit-##[32:personality] = false "Output bits";
function _;
license "GPL";
;;
`
	pkg, err := Parse("test.comp", src)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}
	pin := pkg.Component.Pins[0]
	if pin.Name != "bit-##" {
		t.Errorf("Name = %q, want %q", pin.Name, "bit-##")
	}
	if pin.ArraySize != 32 {
		t.Errorf("ArraySize = %d, want 32", pin.ArraySize)
	}
	if pin.ArrayPersonality != "personality" {
		t.Errorf("ArrayPersonality = %q, want %q", pin.ArrayPersonality, "personality")
	}
	if pin.Default != "false" {
		t.Errorf("Default = %q, want %q", pin.Default, "false")
	}
}

func TestParseOptions(t *testing.T) {
	src := `component test "test";
pin out bit x;
function _;
option extra_setup;
option data internal;
license "GPL";
;;
`
	pkg, err := Parse("test.comp", src)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}
	opts := pkg.Component.Options
	if opts["extra_setup"] != "1" {
		t.Errorf("extra_setup = %q", opts["extra_setup"])
	}
	if opts["data"] != "internal" {
		t.Errorf("data = %q", opts["data"])
	}
}

func TestParseSingletonRejected(t *testing.T) {
	src := `component test "test";
pin out bit x;
function _;
option singleton yes;
license "GPL";
;;
`
	_, err := Parse("test.comp", src)
	if err == nil {
		t.Fatal("expected error for 'option singleton'")
	}
	if !strings.Contains(err.Error(), "singleton") {
		t.Errorf("error should mention singleton: %v", err)
	}
}

func TestParseVariable(t *testing.T) {
	src := `component test "test";
pin out bit x;
function _;
variable double counter = 0;
variable int old_pattern = -1;
variable unsigned *ptr;
variable unsigned data[8];
license "GPL";
;;
`
	pkg, err := Parse("test.comp", src)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}
	vars := pkg.Component.Variables
	if len(vars) != 4 {
		t.Fatalf("len(Variables) = %d, want 4", len(vars))
	}
	if vars[0].CType != "double" || vars[0].Name != "counter" || vars[0].Default != "0" {
		t.Errorf("var[0] = %+v", vars[0])
	}
	if vars[1].CType != "int" || vars[1].Name != "old_pattern" || vars[1].Default != "-1" {
		t.Errorf("var[1] = %+v", vars[1])
	}
	if vars[2].CType != "unsigned" || vars[2].Name != "*ptr" {
		t.Errorf("var[2] = %+v", vars[2])
	}
	if vars[3].CType != "unsigned" || vars[3].Name != "data" || vars[3].Array != 8 {
		t.Errorf("var[3] = %+v", vars[3])
	}
}

func TestParseModparam(t *testing.T) {
	src := `component test "test";
pin out bit x;
function _;
modparam dummy cfg "configuration string";
modparam int test_encoder = 0 "test encoder";
license "GPL";
;;
`
	pkg, err := Parse("test.comp", src)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}
	mps := pkg.Component.Modparams
	if len(mps) != 2 {
		t.Fatalf("len(Modparams) = %d, want 2", len(mps))
	}
	if mps[0].Type != "dummy" || mps[0].Name != "cfg" || mps[0].Doc != "configuration string" {
		t.Errorf("modparam[0] = %+v", mps[0])
	}
	if mps[1].Type != "int" || mps[1].Name != "test_encoder" || mps[1].Default != "0" {
		t.Errorf("modparam[1] = %+v", mps[1])
	}
}

func TestParseInclude(t *testing.T) {
	src := `component test "test";
pin out bit x;
function _;
include "rtapi_math.h";
license "GPL";
;;
`
	pkg, err := Parse("test.comp", src)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}
	if len(pkg.Component.Includes) != 1 {
		t.Fatalf("len(Includes) = %d, want 1", len(pkg.Component.Includes))
	}
	if pkg.Component.Includes[0] != `"rtapi_math.h"` {
		t.Errorf("Include[0] = %q", pkg.Component.Includes[0])
	}
}

func TestParseTripleQuotedString(t *testing.T) {
	src := `component test """This is a
multi-line description""";
pin out bit x;
function _;
license "GPL";
;;
`
	pkg, err := Parse("test.comp", src)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}
	if !strings.Contains(pkg.Component.Summary, "multi-line") {
		t.Errorf("Summary = %q", pkg.Component.Summary)
	}
}

// TestParseAllComponentFiles discovers and parses all .comp files
// under src/hal/components/ and src/hal/drivers/. This validates the
// parser handles real-world input.
func TestParseAllComponentFiles(t *testing.T) {
	dirs := []struct {
		name string
		path string
	}{
		{"components", findCompDir()},
		{"drivers", findDriverCompDir()},
	}
	for _, d := range dirs {
		if _, err := os.Stat(d.path); os.IsNotExist(err) {
			t.Skipf("%s directory not found at %s", d.name, d.path)
		}
		entries, err := os.ReadDir(d.path)
		if err != nil {
			t.Fatalf("ReadDir(%s): %v", d.path, err)
		}
		parsed := 0
		for _, e := range entries {
			if e.IsDir() || !strings.HasSuffix(e.Name(), ".comp") {
				continue
			}
			path := filepath.Join(d.path, e.Name())
			t.Run(d.name+"/"+e.Name(), func(t *testing.T) {
				src, err := os.ReadFile(path)
				if err != nil {
					t.Fatalf("ReadFile: %v", err)
				}
				pkg, err := Parse(e.Name(), string(src))
				if err != nil {
					// Skip files with unsupported features (singleton, RTAPI_MP_ARRAY_*).
					if strings.Contains(err.Error(), "singleton") ||
						strings.Contains(err.Error(), "RTAPI_MP_ARRAY_") {
						t.Skipf("unsupported feature: %v", err)
					}
					t.Fatalf("Parse error: %v", err)
				}
				if pkg.Component.Name == "" {
					t.Error("Component name is empty")
				}
			})
			parsed++
		}
		t.Logf("%s: parsed %d .comp files", d.name, parsed)
	}
}

// TestParseUserComps tests parsing the userspace .comp files.
func TestParseUserComps(t *testing.T) {
	baseDir := findUserCompDir()
	if _, err := os.Stat(baseDir); os.IsNotExist(err) {
		t.Skipf("user_comps directory not found at %s", baseDir)
	}
	var files []string
	filepath.Walk(baseDir, func(path string, info os.FileInfo, err error) error {
		if err == nil && !info.IsDir() && strings.HasSuffix(info.Name(), ".comp") {
			files = append(files, path)
		}
		return nil
	})
	if len(files) == 0 {
		t.Skip("no .comp files found in user_comps")
	}
	for _, path := range files {
		name := filepath.Base(path)
		t.Run(name, func(t *testing.T) {
			src, err := os.ReadFile(path)
			if err != nil {
				t.Fatalf("ReadFile: %v", err)
			}
			pkg, err := Parse(name, string(src))
			if err != nil {
				// Skip files with unsupported features (singleton, RTAPI_MP_ARRAY_*).
				if strings.Contains(err.Error(), "singleton") ||
					strings.Contains(err.Error(), "RTAPI_MP_ARRAY_") {
					t.Skipf("unsupported feature: %v", err)
				}
				t.Fatalf("Parse error: %v", err)
			}
			if pkg.Component.Name == "" {
				t.Error("Component name is empty")
			}
		})
	}
}

func TestParseGMIProvideConsume(t *testing.T) {
	src := `component mykins "Custom kinematics";
pin out s32 fpin;
function fdemo;
gmi_provide kins;
gmi_consume tp;
license "GPL";
;;
// user code
`
	pkg, err := Parse("mykins.comp", src)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}
	c := pkg.Component
	if len(c.GMIProvide) != 1 || c.GMIProvide[0] != "kins" {
		t.Errorf("GMIProvide = %v, want [kins]", c.GMIProvide)
	}
	if len(c.GMIConsume) != 1 || c.GMIConsume[0].API != "tp" {
		t.Errorf("GMIConsume = %v, want [{tp }]", c.GMIConsume)
	}
	if c.GMIConsume[0].From != "" {
		t.Errorf("GMIConsume[0].From = %q, want empty", c.GMIConsume[0].From)
	}
}

func TestParseGMIConsumeFrom(t *testing.T) {
	src := `component homecomp "Homing";
pin out s32 fpin;
function fdemo;
gmi_provide home;
gmi_consume mot from motmod;
license "GPL";
;;
`
	pkg, err := Parse("homecomp.comp", src)
	if err != nil {
		t.Fatalf("Parse error: %v", err)
	}
	c := pkg.Component
	if len(c.GMIConsume) != 1 {
		t.Fatalf("GMIConsume length = %d, want 1", len(c.GMIConsume))
	}
	if c.GMIConsume[0].API != "mot" {
		t.Errorf("GMIConsume[0].API = %q, want \"mot\"", c.GMIConsume[0].API)
	}
	if c.GMIConsume[0].From != "motmod" {
		t.Errorf("GMIConsume[0].From = %q, want \"motmod\"", c.GMIConsume[0].From)
	}
}
