package main

import (
	"strings"
	"testing"
)

func TestParseConfigSimple(t *testing.T) {
	cfg := `
stDISPLAY_DATA
  in bErrRest bool
  out nState dint
`
	pins, err := ParseConfig(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseConfig error: %v", err)
	}
	if len(pins) != 2 {
		t.Fatalf("expected 2 pins, got %d", len(pins))
	}

	// First pin
	if pins[0].Dir != DirIn {
		t.Errorf("pins[0].Dir = %q, want %q", pins[0].Dir, DirIn)
	}
	if pins[0].HALPath != "stDISPLAY_DATA.bErrRest" {
		t.Errorf("pins[0].HALPath = %q", pins[0].HALPath)
	}
	if pins[0].ADSName != "stDISPLAY_DATA.bErrRest" {
		t.Errorf("pins[0].ADSName = %q", pins[0].ADSName)
	}
	if pins[0].TypeName != "BOOL" {
		t.Errorf("pins[0].TypeName = %q, want BOOL", pins[0].TypeName)
	}

	// Second pin
	if pins[1].Dir != DirOut {
		t.Errorf("pins[1].Dir = %q, want %q", pins[1].Dir, DirOut)
	}
	if pins[1].HALPath != "stDISPLAY_DATA.nState" {
		t.Errorf("pins[1].HALPath = %q", pins[1].HALPath)
	}
	if pins[1].TypeName != "DINT" {
		t.Errorf("pins[1].TypeName = %q, want DINT", pins[1].TypeName)
	}
}

func TestParseConfigArray(t *testing.T) {
	cfg := `
stROOT
  stPOOL[1..3]
    in bReady bool
    out nCount dint
`
	pins, err := ParseConfig(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseConfig error: %v", err)
	}
	// 3 instances × 2 pins = 6
	if len(pins) != 6 {
		t.Fatalf("expected 6 pins, got %d: %+v", len(pins), pins)
	}

	// Check HAL path uses dot notation for index.
	if pins[0].HALPath != "stROOT.stPOOL.1.bReady" {
		t.Errorf("pins[0].HALPath = %q", pins[0].HALPath)
	}
	// Check ADS name uses bracket notation.
	if pins[0].ADSName != "stROOT.stPOOL[1].bReady" {
		t.Errorf("pins[0].ADSName = %q", pins[0].ADSName)
	}

	// Third instance.
	if pins[4].HALPath != "stROOT.stPOOL.3.bReady" {
		t.Errorf("pins[4].HALPath = %q", pins[4].HALPath)
	}
	if pins[4].ADSName != "stROOT.stPOOL[3].bReady" {
		t.Errorf("pins[4].ADSName = %q", pins[4].ADSName)
	}
}

func TestParseConfigStringType(t *testing.T) {
	cfg := `
stBlock
  in sName string(32)
`
	pins, err := ParseConfig(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseConfig error: %v", err)
	}
	if len(pins) != 1 {
		t.Fatalf("expected 1 pin, got %d", len(pins))
	}
	if pins[0].TypeName != "STRING(32)" {
		t.Errorf("TypeName = %q, want STRING(32)", pins[0].TypeName)
	}
}

func TestParseConfigComments(t *testing.T) {
	cfg := `
# This is a comment
stBlock
  # nested comment
  in bFlag bool
`
	pins, err := ParseConfig(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseConfig error: %v", err)
	}
	if len(pins) != 1 {
		t.Fatalf("expected 1 pin, got %d", len(pins))
	}
}

func TestParseConfigArrayFollowedBySibling(t *testing.T) {
	// Verify that a sibling line after an array sub-block is NOT dropped.
	cfg := `
stROOT
  stPOOL[1..2]
    in bReady bool
  in bGlobal bool
`
	pins, err := ParseConfig(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseConfig error: %v", err)
	}
	// 2 array instances × 1 pin + 1 global pin = 3
	if len(pins) != 3 {
		t.Fatalf("expected 3 pins, got %d: %+v", len(pins), pins)
	}
	// Last pin should be the global one.
	last := pins[2]
	if last.HALPath != "stROOT.bGlobal" {
		t.Errorf("last pin HALPath = %q, want %q", last.HALPath, "stROOT.bGlobal")
	}
	if last.ADSName != "stROOT.bGlobal" {
		t.Errorf("last pin ADSName = %q", last.ADSName)
	}
}


func TestParseConfigInvalidArray(t *testing.T) {
	cfg := `
stBad[1..0]
  in bFlag bool
`
	_, err := ParseConfig(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for invalid array range, got nil")
	}
}

func TestExpandContainer(t *testing.T) {
	// Simple name.
	inst, err := expandContainer("stFoo")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(inst) != 1 || inst[0].halSeg != "stFoo" {
		t.Errorf("expected [{stFoo stFoo}], got %+v", inst)
	}

	// Array.
	inst, err = expandContainer("arr[2..4]")
	if err != nil {
		t.Fatalf("unexpected error: %v", err)
	}
	if len(inst) != 3 {
		t.Fatalf("expected 3 instances, got %d", len(inst))
	}
	if inst[0].adsSeg != "arr[2]" {
		t.Errorf("inst[0].adsSeg = %q", inst[0].adsSeg)
	}
	if inst[2].adsSeg != "arr[4]" {
		t.Errorf("inst[2].adsSeg = %q", inst[2].adsSeg)
	}
}

func TestParseTypeInfo(t *testing.T) {
	tests := []struct {
		typeName string
		wantSize uint32
		wantErr  bool
	}{
		{"BOOL", 1, false},
		{"DINT", 4, false},
		{"REAL", 4, false},
		{"LREAL", 8, false},
		{"STRING(32)", 33, false},
		{"STRING(0)", 0, true},
		{"UNKNOWN_TYPE", 0, true},
	}
	for _, tc := range tests {
		ti, err := parseTypeInfo(tc.typeName)
		if tc.wantErr {
			if err == nil {
				t.Errorf("parseTypeInfo(%q) want error, got nil", tc.typeName)
			}
			continue
		}
		if err != nil {
			t.Errorf("parseTypeInfo(%q) unexpected error: %v", tc.typeName, err)
			continue
		}
		if ti.byteSize != tc.wantSize {
			t.Errorf("parseTypeInfo(%q).byteSize = %d, want %d", tc.typeName, ti.byteSize, tc.wantSize)
		}
	}
}
