package main

import (
	"fmt"
	"os"
	"strings"
	"testing"
)

// TestTypeSize checks the size and alignment for all basic ADS types.
func TestTypeSize(t *testing.T) {
	tests := []struct {
		typeName  string
		wantSize  uint32
		wantAlign uint32
		wantErr   bool
	}{
		// 1-byte types
		{"BOOL", 1, 1, false},
		{"BYTE", 1, 1, false},
		{"SINT", 1, 1, false},
		{"USINT", 1, 1, false},
		// 2-byte types
		{"WORD", 2, 2, false},
		{"UINT", 2, 2, false},
		{"INT", 2, 2, false},
		// 4-byte types
		{"DWORD", 4, 4, false},
		{"UDINT", 4, 4, false},
		{"DINT", 4, 4, false},
		{"REAL", 4, 4, false},
		{"TIME", 4, 4, false},
		{"TOD", 4, 4, false},
		{"DATE", 4, 4, false},
		{"DT", 4, 4, false},
		// 8-byte types
		{"LREAL", 8, 8, false},
		// String types
		{"STRING(1)", 2, 1, false},
		{"STRING(31)", 32, 1, false},
		{"STRING(32)", 33, 1, false},
		// Invalid
		{"STRING(0)", 0, 0, true},
		{"UNKNOWN", 0, 0, true},
	}
	for _, tc := range tests {
		sz, al, err := TypeSize(tc.typeName)
		if tc.wantErr {
			if err == nil {
				t.Errorf("TypeSize(%q): expected error, got nil", tc.typeName)
			}
			continue
		}
		if err != nil {
			t.Errorf("TypeSize(%q): unexpected error: %v", tc.typeName, err)
			continue
		}
		if sz != tc.wantSize {
			t.Errorf("TypeSize(%q): size = %d, want %d", tc.typeName, sz, tc.wantSize)
		}
		if al != tc.wantAlign {
			t.Errorf("TypeSize(%q): align = %d, want %d", tc.typeName, al, tc.wantAlign)
		}
	}
}

// TestAlignUp validates the alignUp helper.
func TestAlignUp(t *testing.T) {
	cases := []struct{ n, align, want uint32 }{
		{0, 1, 0},
		{0, 4, 0},
		{1, 4, 4},
		{4, 4, 4},
		{5, 4, 8},
		{6, 4, 8},
		{7, 4, 8},
		{8, 4, 8},
		{3, 2, 4},
		{0, 8, 0},
		{1, 8, 8},
	}
	for _, c := range cases {
		got := alignUp(c.n, c.align)
		if got != c.want {
			t.Errorf("alignUp(%d, %d) = %d, want %d", c.n, c.align, got, c.want)
		}
	}
}

// TestLayoutBoolFollowedByDword checks that a BOOL followed by a DWORD
// produces a 3-byte alignment gap.
func TestLayoutBoolFollowedByDword(t *testing.T) {
	cfg := `
stFoo
  in bFlag BOOL
  in nVal DWORD
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree: %v", err)
	}
	pins, err := ComputeLayout(roots)
	if err != nil {
		t.Fatalf("ComputeLayout: %v", err)
	}
	// Non-pad pins only.
	if len(pins) != 2 {
		t.Fatalf("expected 2 pins, got %d: %+v", len(pins), pins)
	}
	if pins[0].Offset != 0 || pins[0].Size != 1 {
		t.Errorf("bFlag: offset=%d size=%d, want 0/1", pins[0].Offset, pins[0].Size)
	}
	// DWORD needs align 4: next 4-aligned after 1 is 4.
	if pins[1].Offset != 4 || pins[1].Size != 4 {
		t.Errorf("nVal: offset=%d size=%d, want 4/4", pins[1].Offset, pins[1].Size)
	}
}

// TestLayoutStructTailPadding checks that a struct ending with a smaller-align
// field gets tail-padded to the struct's max alignment. This is important for
// arrays of structs.
func TestLayoutStructTailPadding(t *testing.T) {
	// struct { REAL r; BOOL b; }
	// REAL → offset 0 (4B), BOOL → offset 4 (1B)
	// raw end = 5; max_align = 4; tail padding → 8
	// array element size must be 8.
	cfg := `
@struct ST_TAIL_TEST 00000000-0000-0000-0000-000000000000
  in fVal REAL
  in bFlag BOOL

stRoot
  struct aSt[1..2] ST_TAIL_TEST
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	pins, err := ComputeLayout(roots, aliases)
	if err != nil {
		t.Fatalf("ComputeLayout: %v", err)
	}
	// 2 elements × 2 fields = 4 pins.
	if len(pins) != 4 {
		t.Fatalf("expected 4 pins, got %d", len(pins))
	}
	// Element [1]: fVal at 0, bFlag at 4.
	if pins[0].Offset != 0 {
		t.Errorf("aSt[1].fVal offset = %d, want 0", pins[0].Offset)
	}
	if pins[1].Offset != 4 {
		t.Errorf("aSt[1].bFlag offset = %d, want 4", pins[1].Offset)
	}
	// Element [2]: must start at 8 (tail-padded element size = 8).
	if pins[2].Offset != 8 {
		t.Errorf("aSt[2].fVal offset = %d, want 8", pins[2].Offset)
	}
	if pins[3].Offset != 12 {
		t.Errorf("aSt[2].bFlag offset = %d, want 12", pins[3].Offset)
	}
}

// TestLayoutNestedStruct checks alignment when a struct is nested inside another.
func TestLayoutNestedStruct(t *testing.T) {
	// outer { BOOL b; inner { WORD w; DINT n; } }
	// b at 0; inner.maxAlign=4 → inner starts at 4; w at 4 (2B); n at 8 (4B)
	// inner raw end=12; inner maxAlign=4; tail pad → 12 (already multiple of 4)
	// outer ends at 12; outer maxAlign = max(1, 4) = 4; tail pad → 12
	cfg := `
outer
  in bFlag BOOL
  inner
    in wVal WORD
    in nVal DINT
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree: %v", err)
	}
	pins, err := ComputeLayout(roots)
	if err != nil {
		t.Fatalf("ComputeLayout: %v", err)
	}
	if len(pins) != 3 {
		t.Fatalf("expected 3 pins, got %d", len(pins))
	}
	if pins[0].HALPath != "outer.bFlag" || pins[0].Offset != 0 {
		t.Errorf("bFlag: path=%q offset=%d", pins[0].HALPath, pins[0].Offset)
	}
	// inner.maxAlign=4 → inner starts at alignUp(1,4)=4
	if pins[1].HALPath != "outer.inner.wVal" || pins[1].Offset != 4 {
		t.Errorf("wVal: path=%q offset=%d, want outer.inner.wVal at 4", pins[1].HALPath, pins[1].Offset)
	}
	// nVal: DINT align=4; after WORD (2B) at offset 4 → end 6; alignUp(6,4)=8
	if pins[2].HALPath != "outer.inner.nVal" || pins[2].Offset != 8 {
		t.Errorf("nVal: path=%q offset=%d, want outer.inner.nVal at 8", pins[2].HALPath, pins[2].Offset)
	}
}

// TestLayoutPadOccupiesSpace checks that a pad entry takes up space and shifts
// subsequent fields.
func TestLayoutPadOccupiesSpace(t *testing.T) {
	cfg := `
stMsg
  pad _r BYTE
  in eType INT
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree: %v", err)
	}
	pins, err := ComputeLayout(roots)
	if err != nil {
		t.Fatalf("ComputeLayout: %v", err)
	}
	// 2 pins: pad + eType
	if len(pins) != 2 {
		t.Fatalf("expected 2 pins, got %d", len(pins))
	}
	if pins[0].Dir != DirPad || pins[0].Offset != 0 || pins[0].Size != 1 {
		t.Errorf("pad: dir=%q offset=%d size=%d", pins[0].Dir, pins[0].Offset, pins[0].Size)
	}
	// INT align=2; after BYTE (1B) at 0 → end 1; alignUp(1,2)=2
	if pins[1].Dir != DirIn || pins[1].Offset != 2 || pins[1].Size != 2 {
		t.Errorf("eType: dir=%q offset=%d size=%d, want in at 2 size 2", pins[1].Dir, pins[1].Offset, pins[1].Size)
	}
}

// TestLayoutInoutPin checks that an "inout" leaf is included as a LayoutPin
// with DirInOut.
func TestLayoutInoutPin(t *testing.T) {
	cfg := `
stBlock
  inout fSetpoint REAL
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree: %v", err)
	}
	pins, err := ComputeLayout(roots)
	if err != nil {
		t.Fatalf("ComputeLayout: %v", err)
	}
	if len(pins) != 1 {
		t.Fatalf("expected 1 pin, got %d", len(pins))
	}
	if pins[0].Dir != DirInOut {
		t.Errorf("Dir = %q, want %q", pins[0].Dir, DirInOut)
	}
	if pins[0].HALPath != "stBlock.fSetpoint" {
		t.Errorf("HALPath = %q", pins[0].HALPath)
	}
	if pins[0].ADSName != "stBlock.fSetpoint" {
		t.Errorf("ADSName = %q", pins[0].ADSName)
	}
}

// TestLayoutArrayOfStructs verifies correct element spacing for an array where
// the element struct needs tail padding.
func TestLayoutArrayOfStructs(t *testing.T) {
	// struct { REAL f; BOOL b; } → size 5, maxAlign 4, alignedSize 8
	// aSt[1..3] → offsets: [0,8,16]
	cfg := `
@struct ST_AST 00000000-0000-0000-0000-000000000000
  in fPower REAL
  out bManu BOOL

root
  struct aSt[1..3] ST_AST
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	pins, err := ComputeLayout(roots, aliases)
	if err != nil {
		t.Fatalf("ComputeLayout: %v", err)
	}
	if len(pins) != 6 {
		t.Fatalf("expected 6 pins, got %d", len(pins))
	}
	expected := []struct {
		path   string
		offset uint32
	}{
		{"root.aSt.1.fPower", 0},
		{"root.aSt.1.bManu", 4},
		{"root.aSt.2.fPower", 8},
		{"root.aSt.2.bManu", 12},
		{"root.aSt.3.fPower", 16},
		{"root.aSt.3.bManu", 20},
	}
	for i, e := range expected {
		if pins[i].HALPath != e.path {
			t.Errorf("pins[%d].HALPath = %q, want %q", i, pins[i].HALPath, e.path)
		}
		if pins[i].Offset != e.offset {
			t.Errorf("pins[%d].Offset = %d, want %d", i, pins[i].Offset, e.offset)
		}
	}
}

// TestLayoutGalvHmiSelectedOffsets parses the clean galv-hmi.conf and checks
// the computed offsets for a selected set of leaf pins in the stData section.
func TestLayoutGalvHmiSelectedOffsets(t *testing.T) {
	f, err := os.Open("configs/galv-hmi.conf")
	if err != nil {
		t.Skipf("galv-hmi.conf not found: %v", err)
	}
	defer f.Close()

	aliases, roots, err := ParseTreeWithAliases(f)
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	pins, err := ComputeLayout(roots, aliases)
	if err != nil {
		t.Fatalf("ComputeLayout: %v", err)
	}

	// Build a lookup map for easy offset checking.
	byADS := make(map[string]uint32)
	for _, p := range pins {
		byADS[p.ADSName] = p.Offset
	}

	// Expected absolute byte offsets for the stData section.
	// These are computed from natural (C-struct) alignment rules.
	// aPools[1] starts at offset 8 (after DT=4, 2×BOOL, 2-byte gap to align-4).
	expected := []struct {
		name   string
		offset uint32
	}{
		{"DISPLAY_DATA.stData.dtCurrentTime", 0},
		{"DISPLAY_DATA.stData.bGlobalErr", 4},
		{"DISPLAY_DATA.stData.bAckErr", 5},
		// aPools[1] starts at 8 (alignUp(6, 4) = 8)
		{"DISPLAY_DATA.stData.aPools[1].sPoolName", 8},
		{"DISPLAY_DATA.stData.aPools[1].nFormulaId", 40},   // 8+32
		{"DISPLAY_DATA.stData.aPools[1].sFormulaName", 44}, // 40+4
		{"DISPLAY_DATA.stData.aPools[1].eState", 76},       // 44+32
		// eState ends at 78; REAL needs align-4: alignUp(78,4)=80
		{"DISPLAY_DATA.stData.aPools[1].fTemp", 80},
		{"DISPLAY_DATA.stData.aPools[1].fTempSetpoint", 136}, // 8+128
		{"DISPLAY_DATA.stData.aPools[1].bPumpOnIdle", 140},   // 8+132
		// TIME needs align-4: alignUp(133,4)=136 → tMixerTimeManual at 144
		{"DISPLAY_DATA.stData.aPools[1].tMixerTimeManual", 144},
		// stMsg: maxAlign=2; starts at alignUp(151,2)=152 → abs 8+152=160
		{"DISPLAY_DATA.stData.aPools[1].stMsg.eType", 160},
		// aMixers: maxAlign=4; starts at alignUp(158,4)=160 → abs 8+160=168
		{"DISPLAY_DATA.stData.aPools[1].aMixers[1].fPower", 168},
		{"DISPLAY_DATA.stData.aPools[1].aMixers[1].bManu", 172},
		// aMixers element size = 8 (tail-padded); Mixer[2] at 168+8=176
		{"DISPLAY_DATA.stData.aPools[1].aMixers[2].fPower", 176},
		// stErrors section — all BOOLs, maxAlign=1, no alignment padding anywhere.
		{"DISPLAY_DATA.stErrors.stGlobalErrors.bEmergStop", 200},
		{"DISPLAY_DATA.stErrors.stGlobalErrors.bTempErr", 203},
		{"DISPLAY_DATA.stErrors.aPoolErrors[1].bHeaterTempWarn", 204},
		{"DISPLAY_DATA.stErrors.aPoolErrors[1].bPsVoltErr", 217},
		{"DISPLAY_DATA.stErrors.aPoolErrors[1].aMixerErrors[1].bDriveWarn", 218},
		{"DISPLAY_DATA.stErrors.aPoolErrors[1].aMixerErrors[1].bVeloErr", 222},
		{"DISPLAY_DATA.stErrors.aPoolErrors[1].aMixerErrors[2].bDriveWarn", 223},
		{"DISPLAY_DATA.stErrors.aPoolErrors[1].aMixerErrors[4].bVeloErr", 237},
	}
	for _, e := range expected {
		got, ok := byADS[e.name]
		if !ok {
			t.Errorf("pin %q not found in layout", e.name)
			continue
		}
		if got != e.offset {
			t.Errorf("pin %q: offset = %d, want %d", e.name, got, e.offset)
		}
	}
}

// TestLayoutIdempotencyGalvHmiPadding parses galv-hmi-padding.conf (which has
// explicit manual pad entries) and checks that auto-alignment produces the same
// offsets for non-pad pins as the clean galv-hmi.conf layout.
// This verifies that correct manual padding is idempotent under auto-alignment.
func TestLayoutIdempotencyGalvHmiPadding(t *testing.T) {
	fPad, err := os.Open("configs/galv-hmi-padding.conf")
	if err != nil {
		t.Skipf("galv-hmi-padding.conf not found: %v", err)
	}
	defer fPad.Close()

	roots, err := ParseTree(fPad)
	if err != nil {
		t.Fatalf("ParseTree(padding): %v", err)
	}
	pins, err := ComputeLayout(roots)
	if err != nil {
		t.Fatalf("ComputeLayout(padding): %v", err)
	}

	byADS := make(map[string]uint32)
	for _, p := range pins {
		if p.Dir != DirPad {
			byADS[p.ADSName] = p.Offset
		}
	}

	// These offsets must match those from the clean config (see TestLayoutGalvHmiSelectedOffsets).
	expected := []struct {
		name   string
		offset uint32
	}{
		{"DISPLAY_DATA.stData.dtCurrentTime", 0},
		{"DISPLAY_DATA.stData.bGlobalErr", 4},
		{"DISPLAY_DATA.stData.bAckErr", 5},
		{"DISPLAY_DATA.stData.aPools[1].sPoolName", 8},
		{"DISPLAY_DATA.stData.aPools[1].nFormulaId", 40},
		{"DISPLAY_DATA.stData.aPools[1].sFormulaName", 44},
		{"DISPLAY_DATA.stData.aPools[1].eState", 76},
		{"DISPLAY_DATA.stData.aPools[1].fTemp", 80},
		{"DISPLAY_DATA.stData.aPools[1].fTempSetpoint", 136},
		{"DISPLAY_DATA.stData.aPools[1].bPumpOnIdle", 140},
		{"DISPLAY_DATA.stData.aPools[1].tMixerTimeManual", 144},
		{"DISPLAY_DATA.stData.aPools[1].stMsg.eType", 160},
		{"DISPLAY_DATA.stData.aPools[1].aMixers[1].fPower", 168},
		{"DISPLAY_DATA.stData.aPools[1].aMixers[1].bManu", 172},
		{"DISPLAY_DATA.stData.aPools[1].aMixers[2].fPower", 176},
		// stErrors section — all BOOLs, maxAlign=1, no alignment padding anywhere.
		{"DISPLAY_DATA.stErrors.stGlobalErrors.bEmergStop", 200},
		{"DISPLAY_DATA.stErrors.stGlobalErrors.bTempErr", 203},
		{"DISPLAY_DATA.stErrors.aPoolErrors[1].bHeaterTempWarn", 204},
		{"DISPLAY_DATA.stErrors.aPoolErrors[1].bPsVoltErr", 217},
		{"DISPLAY_DATA.stErrors.aPoolErrors[1].aMixerErrors[1].bDriveWarn", 218},
		{"DISPLAY_DATA.stErrors.aPoolErrors[1].aMixerErrors[1].bVeloErr", 222},
		{"DISPLAY_DATA.stErrors.aPoolErrors[1].aMixerErrors[2].bDriveWarn", 223},
		{"DISPLAY_DATA.stErrors.aPoolErrors[1].aMixerErrors[4].bVeloErr", 237},
	}
	for _, e := range expected {
		got, ok := byADS[e.name]
		if !ok {
			t.Errorf("pin %q not found in padding layout", e.name)
			continue
		}
		if got != e.offset {
			t.Errorf("pin %q: padding config offset = %d, want %d (must match clean config)", e.name, got, e.offset)
		}
	}
}

// ---------------------------------------------------------------------------
// @type alias resolution tests
// ---------------------------------------------------------------------------

// TestTypeSizeAliasResolution verifies that ComputeLayout resolves @type
// aliases to their base types for size/alignment computation.
func TestTypeSizeAliasResolution(t *testing.T) {
	aliases := TypeAliasMap{
		"EN_DISP_MSGTYPE": TypeAlias{BaseType: "WORD"},
	}
	cfg := `
stBlock
  in eType EN_DISP_MSGTYPE
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree: %v", err)
	}
	pins, err := ComputeLayout(roots, aliases)
	if err != nil {
		t.Fatalf("ComputeLayout with alias: %v", err)
	}
	if len(pins) != 1 {
		t.Fatalf("expected 1 pin, got %d", len(pins))
	}
	// WORD: size=2, align=2.
	if pins[0].Size != 2 {
		t.Errorf("pin size = %d, want 2 (WORD)", pins[0].Size)
	}
	if pins[0].Align != 2 {
		t.Errorf("pin align = %d, want 2 (WORD)", pins[0].Align)
	}
	// TypeName should be the alias name, not the base type.
	if pins[0].TypeName != "EN_DISP_MSGTYPE" {
		t.Errorf("pin TypeName = %q, want EN_DISP_MSGTYPE", pins[0].TypeName)
	}
}

// TestComputeLayoutUnknownTypeWithoutAlias verifies that an unknown type name
// without an alias map produces an error.
func TestComputeLayoutUnknownTypeWithoutAlias(t *testing.T) {
	cfg := `
stBlock
  in eType UNKNOWN_TYPE
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree: %v", err)
	}
	if _, err := ComputeLayout(roots); err == nil {
		t.Error("expected error for unknown type without alias map, got nil")
	}
}

// TestComputeLayoutAliasCorrectOffset verifies that alias types produce the
// same offsets as their base types.
func TestComputeLayoutAliasCorrectOffset(t *testing.T) {
	aliases := TypeAliasMap{
		"MY_WORD_ALIAS": TypeAlias{BaseType: "WORD"},
	}
	// struct { BOOL b; MY_WORD_ALIAS w; }
	// BOOL at 0 (size 1, align 1); WORD at alignUp(1,2)=2 (size 2, align 2)
	cfg := `
stFoo
  in bFlag BOOL
  in eVal MY_WORD_ALIAS
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree: %v", err)
	}
	pins, err := ComputeLayout(roots, aliases)
	if err != nil {
		t.Fatalf("ComputeLayout: %v", err)
	}
	if len(pins) != 2 {
		t.Fatalf("expected 2 pins, got %d", len(pins))
	}
	if pins[0].Offset != 0 || pins[0].Size != 1 {
		t.Errorf("bFlag: offset=%d size=%d, want 0/1", pins[0].Offset, pins[0].Size)
	}
	if pins[1].Offset != 2 || pins[1].Size != 2 {
		t.Errorf("eVal: offset=%d size=%d, want 2/2", pins[1].Offset, pins[1].Size)
	}
}

// TestLayoutLeafArray verifies the layout of scalar leaf arrays (inline arrays
// with a direction keyword: "in/out/inout/pad name[start..end] TYPE").
func TestLayoutLeafArray(t *testing.T) {
	t.Run("BoolArrayOffsets", func(t *testing.T) {
		cfg := "stRoot\n  in aFlags[1..4] BOOL\n"
		roots, err := ParseTree(strings.NewReader(cfg))
		if err != nil {
			t.Fatalf("ParseTree: %v", err)
		}
		pins, err := ComputeLayout(roots)
		if err != nil {
			t.Fatalf("ComputeLayout: %v", err)
		}
		if len(pins) != 4 {
			t.Fatalf("expected 4 pins, got %d", len(pins))
		}
		for i, p := range pins {
			wantOffset := uint32(i)
			if p.Offset != wantOffset {
				t.Errorf("aFlags[%d] offset = %d, want %d", i+1, p.Offset, wantOffset)
			}
			if p.Size != 1 {
				t.Errorf("aFlags[%d] size = %d, want 1", i+1, p.Size)
			}
			if p.Align != 1 {
				t.Errorf("aFlags[%d] align = %d, want 1", i+1, p.Align)
			}
		}
	})

	t.Run("DwordArrayOffsets", func(t *testing.T) {
		cfg := "stRoot\n  in aVals[1..3] DWORD\n"
		roots, err := ParseTree(strings.NewReader(cfg))
		if err != nil {
			t.Fatalf("ParseTree: %v", err)
		}
		pins, err := ComputeLayout(roots)
		if err != nil {
			t.Fatalf("ComputeLayout: %v", err)
		}
		if len(pins) != 3 {
			t.Fatalf("expected 3 pins, got %d", len(pins))
		}
		for i, p := range pins {
			wantOffset := uint32(i) * 4
			if p.Offset != wantOffset {
				t.Errorf("aVals[%d] offset = %d, want %d", i+1, p.Offset, wantOffset)
			}
			if p.Size != 4 {
				t.Errorf("aVals[%d] size = %d, want 4", i+1, p.Size)
			}
		}
	})

	t.Run("HALPathDotNotation", func(t *testing.T) {
		cfg := "stRoot\n  in aFlags[1..4] BOOL\n"
		roots, err := ParseTree(strings.NewReader(cfg))
		if err != nil {
			t.Fatalf("ParseTree: %v", err)
		}
		pins, err := ComputeLayout(roots)
		if err != nil {
			t.Fatalf("ComputeLayout: %v", err)
		}
		for i := 1; i <= 4; i++ {
			want := fmt.Sprintf("stRoot.aFlags.%d", i)
			if pins[i-1].HALPath != want {
				t.Errorf("pins[%d].HALPath = %q, want %q", i-1, pins[i-1].HALPath, want)
			}
		}
	})

	t.Run("ADSNameBracketNotation", func(t *testing.T) {
		cfg := "stRoot\n  in aFlags[1..4] BOOL\n"
		roots, err := ParseTree(strings.NewReader(cfg))
		if err != nil {
			t.Fatalf("ParseTree: %v", err)
		}
		pins, err := ComputeLayout(roots)
		if err != nil {
			t.Fatalf("ComputeLayout: %v", err)
		}
		for i := 1; i <= 4; i++ {
			want := fmt.Sprintf("stRoot.aFlags[%d]", i)
			if pins[i-1].ADSName != want {
				t.Errorf("pins[%d].ADSName = %q, want %q", i-1, pins[i-1].ADSName, want)
			}
		}
	})

	t.Run("AlignmentAfterPrecedingField", func(t *testing.T) {
		// BOOL at 0, then DWORD array aligned to 4.
		cfg := "stRoot\n  in bPrev BOOL\n  in aVals[1..2] DWORD\n"
		roots, err := ParseTree(strings.NewReader(cfg))
		if err != nil {
			t.Fatalf("ParseTree: %v", err)
		}
		pins, err := ComputeLayout(roots)
		if err != nil {
			t.Fatalf("ComputeLayout: %v", err)
		}
		// 3 pins: bPrev, aVals[1], aVals[2]
		if len(pins) != 3 {
			t.Fatalf("expected 3 pins, got %d", len(pins))
		}
		if pins[0].Offset != 0 {
			t.Errorf("bPrev offset = %d, want 0", pins[0].Offset)
		}
		// DWORD align=4; alignUp(1,4)=4
		if pins[1].Offset != 4 {
			t.Errorf("aVals[1] offset = %d, want 4", pins[1].Offset)
		}
		if pins[2].Offset != 8 {
			t.Errorf("aVals[2] offset = %d, want 8", pins[2].Offset)
		}
	})

	t.Run("StructArrayOffsets", func(t *testing.T) {
		cfg := `
@struct ST_TEST 00000000-0000-0000-0000-000000000001
  in bFlag BOOL
  in nVal DWORD

stRoot
  struct aPools[1..2] ST_TEST
`
		_, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
		if err != nil {
			t.Fatalf("ParseTreeWithAliases: %v", err)
		}
		pins, err := ComputeLayout(roots)
		if err != nil {
			t.Fatalf("ComputeLayout: %v", err)
		}
		// 2 elements × 2 fields = 4 pins.
		// Element: BOOL at 0 (1B), DWORD at 4 (4B), tail pad to 8. elemSz = 8.
		if len(pins) != 4 {
			t.Fatalf("expected 4 pins, got %d", len(pins))
		}
		expected := []struct {
			path   string
			offset uint32
		}{
			{"stRoot.aPools.1.bFlag", 0},
			{"stRoot.aPools.1.nVal", 4},
			{"stRoot.aPools.2.bFlag", 8},
			{"stRoot.aPools.2.nVal", 12},
		}
		for i, e := range expected {
			if pins[i].HALPath != e.path {
				t.Errorf("pins[%d].HALPath = %q, want %q", i, pins[i].HALPath, e.path)
			}
			if pins[i].Offset != e.offset {
				t.Errorf("pins[%d].Offset = %d, want %d", i, pins[i].Offset, e.offset)
			}
		}
	})
}
