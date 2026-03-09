package main

import (
	"strings"
	"testing"
)

func TestParseTreeSimple(t *testing.T) {
	cfg := `
stDISPLAY_DATA
  in bErrRest bool
  out nState dint
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree error: %v", err)
	}
	if len(roots) != 1 {
		t.Fatalf("expected 1 root, got %d", len(roots))
	}
	root := roots[0]
	if root.Name != "stDISPLAY_DATA" {
		t.Errorf("root.Name = %q, want stDISPLAY_DATA", root.Name)
	}
	if root.Dir != "" {
		t.Errorf("root.Dir = %q, want empty (container)", root.Dir)
	}
	if len(root.Children) != 2 {
		t.Fatalf("expected 2 children, got %d", len(root.Children))
	}

	c0 := root.Children[0]
	if c0.Dir != DirIn {
		t.Errorf("children[0].Dir = %q, want %q", c0.Dir, DirIn)
	}
	if c0.Name != "bErrRest" {
		t.Errorf("children[0].Name = %q", c0.Name)
	}
	if c0.TypeName != "BOOL" {
		t.Errorf("children[0].TypeName = %q, want BOOL", c0.TypeName)
	}

	c1 := root.Children[1]
	if c1.Dir != DirOut {
		t.Errorf("children[1].Dir = %q, want %q", c1.Dir, DirOut)
	}
	if c1.Name != "nState" {
		t.Errorf("children[1].Name = %q", c1.Name)
	}
	if c1.TypeName != "DINT" {
		t.Errorf("children[1].TypeName = %q, want DINT", c1.TypeName)
	}
}

func TestParseTreeArray(t *testing.T) {
	cfg := `
stROOT
  stPOOL[1..3]
    in bReady bool
    out nCount dint
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree error: %v", err)
	}
	if len(roots) != 1 {
		t.Fatalf("expected 1 root, got %d", len(roots))
	}
	root := roots[0]
	if len(root.Children) != 1 {
		t.Fatalf("expected 1 child (the array), got %d", len(root.Children))
	}

	arr := root.Children[0]
	if arr.Name != "stPOOL" {
		t.Errorf("array.Name = %q, want stPOOL", arr.Name)
	}
	if arr.ArrayStart != 1 {
		t.Errorf("ArrayStart = %d, want 1", arr.ArrayStart)
	}
	if arr.ArrayEnd != 3 {
		t.Errorf("ArrayEnd = %d, want 3", arr.ArrayEnd)
	}
	if len(arr.Children) != 2 {
		t.Fatalf("expected 2 template children, got %d", len(arr.Children))
	}
	if arr.Children[0].Name != "bReady" {
		t.Errorf("arr.Children[0].Name = %q", arr.Children[0].Name)
	}
}

func TestParseTreeStringType(t *testing.T) {
	cfg := `
stBlock
  in sName string(32)
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree error: %v", err)
	}
	if len(roots) != 1 || len(roots[0].Children) != 1 {
		t.Fatalf("unexpected structure")
	}
	c := roots[0].Children[0]
	if c.TypeName != "STRING(32)" {
		t.Errorf("TypeName = %q, want STRING(32)", c.TypeName)
	}
}

func TestParseTreeComments(t *testing.T) {
	cfg := `
# This is a comment
stBlock
  # nested comment
  in bFlag bool
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree error: %v", err)
	}
	if len(roots) != 1 || len(roots[0].Children) != 1 {
		t.Fatalf("expected 1 root with 1 child, got %d root(s)", len(roots))
	}
}

func TestParseTreeArrayFollowedBySibling(t *testing.T) {
	cfg := `
stROOT
  stPOOL[1..2]
    in bReady bool
  in bGlobal bool
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree error: %v", err)
	}
	if len(roots) != 1 {
		t.Fatalf("expected 1 root, got %d", len(roots))
	}
	root := roots[0]
	// Should have the array and the sibling leaf as children.
	if len(root.Children) != 2 {
		t.Fatalf("expected 2 children (array + sibling), got %d: %+v", len(root.Children), root.Children)
	}
	arr := root.Children[0]
	if arr.ArrayStart != 1 || arr.ArrayEnd != 2 {
		t.Errorf("array range = [%d..%d], want [1..2]", arr.ArrayStart, arr.ArrayEnd)
	}
	sib := root.Children[1]
	if sib.Name != "bGlobal" || sib.Dir != DirIn {
		t.Errorf("sibling = %+v, want {bGlobal in}", sib)
	}
}

func TestParseTreePad(t *testing.T) {
	cfg := `
stMsg
  pad _reserved1 BYTE
  in eType INT
  pad _align WORD
  out fValue REAL
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree error: %v", err)
	}
	if len(roots) != 1 {
		t.Fatalf("expected 1 root, got %d", len(roots))
	}
	children := roots[0].Children
	if len(children) != 4 {
		t.Fatalf("expected 4 children, got %d", len(children))
	}

	if children[0].Dir != DirPad || children[0].TypeName != "BYTE" {
		t.Errorf("children[0]: Dir=%q TypeName=%q, want pad BYTE", children[0].Dir, children[0].TypeName)
	}
	if children[1].Dir != DirIn || children[1].TypeName != "INT" {
		t.Errorf("children[1]: Dir=%q TypeName=%q, want in INT", children[1].Dir, children[1].TypeName)
	}
	if children[2].Dir != DirPad || children[2].TypeName != "WORD" {
		t.Errorf("children[2]: Dir=%q TypeName=%q, want pad WORD", children[2].Dir, children[2].TypeName)
	}
	if children[3].Dir != DirOut || children[3].TypeName != "REAL" {
		t.Errorf("children[3]: Dir=%q TypeName=%q, want out REAL", children[3].Dir, children[3].TypeName)
	}
}

func TestParseTreeInout(t *testing.T) {
	cfg := `
stBlock
  inout fSetpoint REAL
  in bEnable BOOL
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree error: %v", err)
	}
	if len(roots) != 1 || len(roots[0].Children) != 2 {
		t.Fatalf("unexpected tree structure")
	}
	c0 := roots[0].Children[0]
	if c0.Dir != DirInOut {
		t.Errorf("children[0].Dir = %q, want %q", c0.Dir, DirInOut)
	}
	if c0.Name != "fSetpoint" {
		t.Errorf("children[0].Name = %q, want fSetpoint", c0.Name)
	}
	if c0.TypeName != "REAL" {
		t.Errorf("children[0].TypeName = %q, want REAL", c0.TypeName)
	}
}

func TestParseTreePadInArray(t *testing.T) {
	cfg := `
stRoot
  stItems[1..2]
    pad _hdr BYTE
    in bReady BOOL
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree error: %v", err)
	}
	if len(roots) != 1 {
		t.Fatalf("expected 1 root, got %d", len(roots))
	}
	arr := roots[0].Children[0]
	if arr.Name != "stItems" || arr.ArrayStart != 1 || arr.ArrayEnd != 2 {
		t.Errorf("array node = %+v", arr)
	}
	if len(arr.Children) != 2 {
		t.Fatalf("expected 2 template children, got %d", len(arr.Children))
	}
	if arr.Children[0].Dir != DirPad {
		t.Errorf("template[0].Dir = %q, want pad", arr.Children[0].Dir)
	}
}

func TestParseTreeNestedStructs(t *testing.T) {
	cfg := `
outer
  inner
    in bFlag BOOL
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree error: %v", err)
	}
	if len(roots) != 1 {
		t.Fatalf("expected 1 root, got %d", len(roots))
	}
	outer := roots[0]
	if outer.Name != "outer" || len(outer.Children) != 1 {
		t.Fatalf("outer = %+v", outer)
	}
	inner := outer.Children[0]
	if inner.Name != "inner" || len(inner.Children) != 1 {
		t.Fatalf("inner = %+v", inner)
	}
	leaf := inner.Children[0]
	if leaf.Dir != DirIn || leaf.Name != "bFlag" {
		t.Errorf("leaf = %+v", leaf)
	}
}

func TestParseTreeInvalidArray(t *testing.T) {
	cfg := `
stBad[1..0]
  in bFlag bool
`
	_, err := ParseTree(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for invalid array range, got nil")
	}
}

func TestParseConfigTabIndent(t *testing.T) {
	// Tab indentation: 1 tab = depth 1, 2 tabs = depth 2.
	cfg := "stDISPLAY_DATA\n\tin bErrRest bool\n\tout nState dint\n"
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree error: %v", err)
	}
	if len(roots) != 1 {
		t.Fatalf("expected 1 root, got %d", len(roots))
	}
	root := roots[0]
	if root.Name != "stDISPLAY_DATA" {
		t.Errorf("root.Name = %q, want stDISPLAY_DATA", root.Name)
	}
	if len(root.Children) != 2 {
		t.Fatalf("expected 2 children, got %d", len(root.Children))
	}
	if root.Children[0].Dir != DirIn || root.Children[0].Name != "bErrRest" {
		t.Errorf("children[0] = %+v", root.Children[0])
	}
	if root.Children[1].Dir != DirOut || root.Children[1].Name != "nState" {
		t.Errorf("children[1] = %+v", root.Children[1])
	}
}

func TestParseConfigTabIndentNested(t *testing.T) {
	// Two levels of tab indentation.
	cfg := "stROOT\n\tstPOOL[1..3]\n\t\tin bReady bool\n\t\tout nCount dint\n"
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree error: %v", err)
	}
	if len(roots) != 1 {
		t.Fatalf("expected 1 root, got %d", len(roots))
	}
	arr := roots[0].Children[0]
	if arr.Name != "stPOOL" || arr.ArrayStart != 1 || arr.ArrayEnd != 3 {
		t.Errorf("array = %+v", arr)
	}
	if len(arr.Children) != 2 {
		t.Fatalf("expected 2 template children, got %d", len(arr.Children))
	}
}

func TestParseConfigFourSpaceIndent(t *testing.T) {
	cfg := `
stDISPLAY_DATA
    in bErrRest bool
    out nState dint
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree error: %v", err)
	}
	if len(roots) != 1 {
		t.Fatalf("expected 1 root, got %d", len(roots))
	}
	if len(roots[0].Children) != 2 {
		t.Fatalf("expected 2 children, got %d", len(roots[0].Children))
	}
	if roots[0].Children[0].Dir != DirIn || roots[0].Children[0].Name != "bErrRest" {
		t.Errorf("children[0] = %+v", roots[0].Children[0])
	}
}

func TestParseConfigFourSpaceIndentNested(t *testing.T) {
	cfg := `
stROOT
    stPOOL[1..2]
        in bReady bool
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree error: %v", err)
	}
	if len(roots) != 1 || len(roots[0].Children) != 1 {
		t.Fatalf("unexpected structure")
	}
	arr := roots[0].Children[0]
	if arr.Name != "stPOOL" || arr.ArrayStart != 1 || arr.ArrayEnd != 2 {
		t.Errorf("array = %+v", arr)
	}
	if len(arr.Children) != 1 || arr.Children[0].Name != "bReady" {
		t.Errorf("template children = %+v", arr.Children)
	}
}

func TestParseConfigSingleSpaceIndent(t *testing.T) {
	cfg := `
stBlock
 in bFlag bool
 out nVal dint
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree error: %v", err)
	}
	if len(roots) != 1 || len(roots[0].Children) != 2 {
		t.Fatalf("unexpected structure: %+v", roots)
	}
	if roots[0].Children[0].Dir != DirIn || roots[0].Children[0].Name != "bFlag" {
		t.Errorf("children[0] = %+v", roots[0].Children[0])
	}
}

func TestParseConfigMixedIndentError(t *testing.T) {
	// First indented line uses spaces; second uses tabs — should error.
	cfg := "stBlock\n  in bFlag bool\n\tout nVal dint\n"
	_, err := ParseTree(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for mixed indentation, got nil")
	}
}

func TestParseConfigInconsistentIndentError(t *testing.T) {
	// First indented line uses 2 spaces; a later line uses 3 spaces — should error.
	cfg := `
stBlock
  in bFlag bool
   out nVal dint
`
	_, err := ParseTree(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for inconsistent indentation, got nil")
	}
}

// ---------------------------------------------------------------------------
// @type directive tests
// ---------------------------------------------------------------------------

// TestParseTypeAliasBasic verifies that a single @type directive is parsed
// into the TypeAliasMap with the correct BaseType and GUID.
func TestParseTypeAliasBasic(t *testing.T) {
	cfg := `
@type EN_DISP_MSGTYPE WORD 96656ea5-0db7-49b0-86ec-56cef26b56d0

stBlock
  in eType EN_DISP_MSGTYPE
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	// Check alias registry.
	if len(aliases) != 1 {
		t.Fatalf("expected 1 alias, got %d", len(aliases))
	}
	alias, ok := aliases["EN_DISP_MSGTYPE"]
	if !ok {
		t.Fatal("alias EN_DISP_MSGTYPE not found")
	}
	if alias.BaseType != "WORD" {
		t.Errorf("BaseType = %q, want WORD", alias.BaseType)
	}

	// Check GUID (Microsoft GUID encoding):
	// 96656ea5-0db7-49b0-86ec-56cef26b56d0
	// Data1=0x96656ea5 LE: a5 6e 65 96
	// Data2=0x0db7 LE: b7 0d
	// Data3=0x49b0 LE: b0 49
	// Data4: 86 ec 56 ce f2 6b 56 d0
	want := [16]byte{0xa5, 0x6e, 0x65, 0x96, 0xb7, 0x0d, 0xb0, 0x49,
		0x86, 0xec, 0x56, 0xce, 0xf2, 0x6b, 0x56, 0xd0}
	if alias.GUID != want {
		t.Errorf("GUID = %x, want %x", alias.GUID, want)
	}

	// Check that the node preserves the alias name.
	if len(roots) != 1 || len(roots[0].Children) != 1 {
		t.Fatalf("unexpected tree structure")
	}
	leaf := roots[0].Children[0]
	if leaf.TypeName != "EN_DISP_MSGTYPE" {
		t.Errorf("leaf TypeName = %q, want EN_DISP_MSGTYPE", leaf.TypeName)
	}
}

// TestParseTypeAliasMultiple verifies that multiple @type directives are all
// collected into the alias map.
func TestParseTypeAliasMultiple(t *testing.T) {
	cfg := `
@type EN_DISP_MSGTYPE WORD 96656ea5-0db7-49b0-86ec-56cef26b56d0
@type EN_DISP_POOL_STATE WORD 4bb8098e-6846-4a59-915d-71a3e3d369c0

stBlock
  in x BOOL
`
	aliases, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	if len(aliases) != 2 {
		t.Fatalf("expected 2 aliases, got %d", len(aliases))
	}
	if _, ok := aliases["EN_DISP_MSGTYPE"]; !ok {
		t.Error("alias EN_DISP_MSGTYPE not found")
	}
	if _, ok := aliases["EN_DISP_POOL_STATE"]; !ok {
		t.Error("alias EN_DISP_POOL_STATE not found")
	}
}

// TestParseTypeAliasCaseNormalization verifies that @type alias names are
// normalised to upper case.
func TestParseTypeAliasCaseNormalization(t *testing.T) {
	cfg := `
@type MyAlias DINT 00000000-0000-0000-0000-000000000000
stBlock
  in x BOOL
`
	aliases, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	if _, ok := aliases["MYALIAS"]; !ok {
		t.Error("expected alias to be normalised to MYALIAS")
	}
}

// TestParseTypeAliasInvalidGUID verifies that a malformed GUID in an @type
// directive returns a parse error.
func TestParseTypeAliasInvalidGUID(t *testing.T) {
	cfg := "@type BAD WORD not-a-valid-guid\nstBlock\n  in x BOOL\n"
	_, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for invalid GUID, got nil")
	}
}

// TestParseTypeAliasMissingArgs verifies that an @type line with wrong number
// of arguments returns a parse error.
func TestParseTypeAliasMissingArgs(t *testing.T) {
	cfg := "@type JUST_TWO_ARGS WORD\nstBlock\n  in x BOOL\n"
	_, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for @type with missing GUID arg, got nil")
	}
}

// TestParseTreeBackwardCompat verifies that ParseTree (without aliases) still
// works correctly and does not return an error for configs with @type directives.
func TestParseTreeBackwardCompat(t *testing.T) {
	cfg := `
@type EN_DISP_MSGTYPE WORD 96656ea5-0db7-49b0-86ec-56cef26b56d0
stBlock
  in eType EN_DISP_MSGTYPE
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree with @type directive: %v", err)
	}
	if len(roots) != 1 || len(roots[0].Children) != 1 {
		t.Fatalf("unexpected tree structure")
	}
	// The node should have the alias name (not the base type).
	leaf := roots[0].Children[0]
	if leaf.TypeName != "EN_DISP_MSGTYPE" {
		t.Errorf("leaf TypeName = %q, want EN_DISP_MSGTYPE", leaf.TypeName)
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
