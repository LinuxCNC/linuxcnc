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
@struct ST_POOL 00000000-0000-0000-0000-000000000001
  in bReady BOOL
  out nCount DINT

stROOT
  struct stPOOL[1..3] ST_POOL
`
	_, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases error: %v", err)
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
	if arr.TypeName != "ST_POOL" {
		t.Errorf("array.TypeName = %q, want ST_POOL", arr.TypeName)
	}
	if len(arr.Children) != 2 {
		t.Fatalf("expected 2 template children (from ST_POOL), got %d", len(arr.Children))
	}
	if arr.Children[0].Name != "bReady" {
		t.Errorf("arr.Children[0].Name = %q, want bReady", arr.Children[0].Name)
	}
}

// TestParseTreeInlineLeafArray verifies that "in name[start..end] TYPE" creates
// a leaf array node (ArrayStart/ArrayEnd set, no Children) rather than a container.
func TestParseTreeInlineLeafArray(t *testing.T) {
	cfg := `
stROOT
  in aFlags[1..4] BOOL
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
		t.Fatalf("expected 1 child (leaf array), got %d", len(root.Children))
	}

	arr := root.Children[0]
	if arr.Name != "aFlags" {
		t.Errorf("array.Name = %q, want aFlags", arr.Name)
	}
	if arr.Dir != DirIn {
		t.Errorf("array.Dir = %q, want %q", arr.Dir, DirIn)
	}
	if arr.TypeName != "BOOL" {
		t.Errorf("array.TypeName = %q, want BOOL", arr.TypeName)
	}
	if arr.ArrayStart != 1 {
		t.Errorf("ArrayStart = %d, want 1", arr.ArrayStart)
	}
	if arr.ArrayEnd != 4 {
		t.Errorf("ArrayEnd = %d, want 4", arr.ArrayEnd)
	}
	if arr.Children != nil {
		t.Errorf("leaf array must have nil Children, got %v", arr.Children)
	}
}

// TestParseTreeInlineLeafArrayDirections verifies all direction keywords with
// the inline array syntax.
func TestParseTreeInlineLeafArrayDirections(t *testing.T) {
	cfg := `
stROOT
  in aIn[1..2] BOOL
  out aOut[1..2] DWORD
  inout aInOut[1..2] REAL
  pad aPad[1..2] BYTE
`
	roots, err := ParseTree(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTree error: %v", err)
	}
	children := roots[0].Children
	if len(children) != 4 {
		t.Fatalf("expected 4 children, got %d", len(children))
	}
	cases := []struct {
		dir      PinDir
		name     string
		typeName string
	}{
		{DirIn, "aIn", "BOOL"},
		{DirOut, "aOut", "DWORD"},
		{DirInOut, "aInOut", "REAL"},
		{DirPad, "aPad", "BYTE"},
	}
	for i, tc := range cases {
		c := children[i]
		if c.Dir != tc.dir {
			t.Errorf("children[%d].Dir = %q, want %q", i, c.Dir, tc.dir)
		}
		if c.Name != tc.name {
			t.Errorf("children[%d].Name = %q, want %q", i, c.Name, tc.name)
		}
		if c.TypeName != tc.typeName {
			t.Errorf("children[%d].TypeName = %q, want %q", i, c.TypeName, tc.typeName)
		}
		if c.ArrayStart != 1 || c.ArrayEnd != 2 {
			t.Errorf("children[%d] range = [%d..%d], want [1..2]", i, c.ArrayStart, c.ArrayEnd)
		}
		if c.Children != nil {
			t.Errorf("children[%d]: leaf array must have nil Children", i)
		}
	}
}

// TestParseTreeInlineStructArrayInvalidRange verifies that an invalid array
// range in inline struct array syntax returns an error.
func TestParseTreeInlineStructArrayInvalidRange(t *testing.T) {
	cfg := `
@struct ST_FOO 00000000-0000-0000-0000-000000000001
  in bFlag BOOL

stRoot
  struct aFoo[1..0] ST_FOO
`
	_, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for invalid array range [1..0], got nil")
	}
}

// TestParseTreeInlineLeafArrayInvalidRange verifies that an invalid range in
// inline leaf array syntax returns an error.
func TestParseTreeInlineLeafArrayInvalidRange(t *testing.T) {
	cfg := `
stRoot
  in aBad[5..3] BOOL
`
	_, err := ParseTree(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for invalid array range [5..3], got nil")
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
// @type directive removal tests and @struct directive tests
// ---------------------------------------------------------------------------

// TestParseTypeRejected verifies that a @type directive produces an error,
// since @type has been superseded by @enum (for enum types) and @struct (for
// struct types).
func TestParseTypeRejected(t *testing.T) {
	cfg := "@type EN_DISP_MSGTYPE WORD 96656ea5-0db7-49b0-86ec-56cef26b56d0\nstBlock\n  in x BOOL\n"
	_, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for @type directive, got nil")
	}
}

// TestParseTypeAliasInvalidGUID verifies that a malformed GUID in a directive
// returns a parse error (tests @type removal too).
func TestParseTypeAliasInvalidGUID(t *testing.T) {
	cfg := "@type BAD WORD not-a-valid-guid\nstBlock\n  in x BOOL\n"
	_, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error, got nil")
	}
}

// TestParseTypeAliasMissingArgs verifies that an @type line returns a parse error.
func TestParseTypeAliasMissingArgs(t *testing.T) {
	cfg := "@type JUST_TWO_ARGS WORD\nstBlock\n  in x BOOL\n"
	_, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for @type, got nil")
	}
}

// TestParseStructBasic verifies that a @struct directive is parsed into the
// TypeAliasMap with the correct GUID and member children.
func TestParseStructBasic(t *testing.T) {
	cfg := `
@struct ST_DISP_MSG 702ba601-5f18-413a-95f1-5fe16503843e
  in eType WORD
  in bEnableOk BOOL
  out bOk BOOL

stBlock
  struct stMsg ST_DISP_MSG
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	// Check alias registry.
	if len(aliases) != 1 {
		t.Fatalf("expected 1 alias, got %d", len(aliases))
	}
	alias, ok := aliases["ST_DISP_MSG"]
	if !ok {
		t.Fatal("alias ST_DISP_MSG not found")
	}
	if alias.BaseType != "" {
		t.Errorf("BaseType = %q, want empty for @struct", alias.BaseType)
	}
	if alias.StructDef == nil {
		t.Fatal("StructDef is nil, want non-nil for @struct")
	}
	if len(alias.StructDef) != 3 {
		t.Fatalf("len(StructDef) = %d, want 3", len(alias.StructDef))
	}

	// Check GUID (Microsoft GUID encoding):
	// 702ba601-5f18-413a-95f1-5fe16503843e
	// Data1=0x702ba601 LE: 01 a6 2b 70
	// Data2=0x5f18 LE: 18 5f
	// Data3=0x413a LE: 3a 41
	// Data4: 95 f1 5f e1 65 03 84 3e
	want := [16]byte{0x01, 0xa6, 0x2b, 0x70, 0x18, 0x5f, 0x3a, 0x41,
		0x95, 0xf1, 0x5f, 0xe1, 0x65, 0x03, 0x84, 0x3e}
	if alias.GUID != want {
		t.Errorf("GUID = %x, want %x", alias.GUID, want)
	}

	// Check struct fields.
	if alias.StructDef[0].Name != "eType" || alias.StructDef[0].Dir != DirIn {
		t.Errorf("StructDef[0] = %+v, want {eType in}", alias.StructDef[0])
	}
	if alias.StructDef[1].Name != "bEnableOk" || alias.StructDef[1].Dir != DirIn {
		t.Errorf("StructDef[1] = %+v, want {bEnableOk in}", alias.StructDef[1])
	}
	if alias.StructDef[2].Name != "bOk" || alias.StructDef[2].Dir != DirOut {
		t.Errorf("StructDef[2] = %+v, want {bOk out}", alias.StructDef[2])
	}

	// Check tree: struct stMsg ST_DISP_MSG should produce a container node.
	if len(roots) != 1 || len(roots[0].Children) != 1 {
		t.Fatalf("unexpected tree structure: roots=%d, children=%d", len(roots), len(roots[0].Children))
	}
	stMsg := roots[0].Children[0]
	if stMsg.Name != "stMsg" {
		t.Errorf("container Name = %q, want stMsg", stMsg.Name)
	}
	if stMsg.TypeName != "ST_DISP_MSG" {
		t.Errorf("container TypeName = %q, want ST_DISP_MSG", stMsg.TypeName)
	}
	if stMsg.Dir != "" {
		t.Errorf("container Dir = %q, want empty", stMsg.Dir)
	}
	if len(stMsg.Children) != 3 {
		t.Fatalf("container children count = %d, want 3", len(stMsg.Children))
	}
	if stMsg.Children[0].Name != "eType" {
		t.Errorf("cloned child[0].Name = %q, want eType", stMsg.Children[0].Name)
	}
}

// TestParseStructMultiple verifies that multiple @struct directives are all
// collected into the alias map.
func TestParseStructMultiple(t *testing.T) {
	cfg := `
@struct ST_A 00000000-0000-0000-0000-000000000001
  in bFlag BOOL

@struct ST_B 00000000-0000-0000-0000-000000000002
  in nVal DWORD

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
	if _, ok := aliases["ST_A"]; !ok {
		t.Error("alias ST_A not found")
	}
	if _, ok := aliases["ST_B"]; !ok {
		t.Error("alias ST_B not found")
	}
}

// TestParseStructCaseNormalization verifies that @struct alias names are
// normalised to upper case.
func TestParseStructCaseNormalization(t *testing.T) {
	cfg := `
@struct myStruct 00000000-0000-0000-0000-000000000000
  in x BOOL
stBlock
  in y BOOL
`
	aliases, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	if _, ok := aliases["MYSTRUCT"]; !ok {
		t.Error("expected alias to be normalised to MYSTRUCT")
	}
}

// TestParseTreeBackwardCompat verifies that @type directives now return an
// error (they have been removed in favour of @enum and @struct).
func TestParseTreeBackwardCompat(t *testing.T) {
	cfg := `
@type EN_DISP_MSGTYPE WORD 96656ea5-0db7-49b0-86ec-56cef26b56d0
stBlock
  in eType EN_DISP_MSGTYPE
`
	_, err := ParseTree(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for @type directive, got nil")
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

// ---------------------------------------------------------------------------
// @enum directive tests
// ---------------------------------------------------------------------------

// TestParseEnumBasic verifies that a @enum directive with explicit values is
// parsed into the TypeAliasMap with the correct BaseType, GUID, and members.
func TestParseEnumBasic(t *testing.T) {
	cfg := `
@enum EN_DISP_MSGTYPE WORD 96656ea5-0db7-49b0-86ec-56cef26b56d0
  none 0
  precheck
  finishedOk

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
	if alias.EnumValues == nil {
		t.Fatal("EnumValues is nil, want non-nil for @enum")
	}
	if len(alias.EnumValues) != 3 {
		t.Fatalf("len(EnumValues) = %d, want 3", len(alias.EnumValues))
	}

	// "none 0" — explicit value
	if alias.EnumValues[0].Name != "none" {
		t.Errorf("EnumValues[0].Name = %q, want none", alias.EnumValues[0].Name)
	}
	if alias.EnumValues[0].Value != 0 {
		t.Errorf("EnumValues[0].Value = %d, want 0", alias.EnumValues[0].Value)
	}
	if !alias.EnumValues[0].HasExplicitValue {
		t.Error("EnumValues[0].HasExplicitValue should be true")
	}

	// "precheck" — auto-incremented to 1
	if alias.EnumValues[1].Name != "precheck" {
		t.Errorf("EnumValues[1].Name = %q, want precheck", alias.EnumValues[1].Name)
	}
	if alias.EnumValues[1].Value != 1 {
		t.Errorf("EnumValues[1].Value = %d, want 1", alias.EnumValues[1].Value)
	}
	if alias.EnumValues[1].HasExplicitValue {
		t.Error("EnumValues[1].HasExplicitValue should be false")
	}

	// "finishedOk" — auto-incremented to 2
	if alias.EnumValues[2].Value != 2 {
		t.Errorf("EnumValues[2].Value = %d, want 2", alias.EnumValues[2].Value)
	}
	if alias.EnumValues[2].HasExplicitValue {
		t.Error("EnumValues[2].HasExplicitValue should be false")
	}

	// The node must preserve the alias name.
	if len(roots) != 1 || len(roots[0].Children) != 1 {
		t.Fatalf("unexpected tree structure")
	}
	leaf := roots[0].Children[0]
	if leaf.TypeName != "EN_DISP_MSGTYPE" {
		t.Errorf("leaf TypeName = %q, want EN_DISP_MSGTYPE", leaf.TypeName)
	}
}

// TestParseEnumAutoIncrement verifies that auto-increment picks up after an
// explicit value and that multiple explicit value resets work correctly.
func TestParseEnumAutoIncrement(t *testing.T) {
	cfg := `
@enum MY_ENUM WORD 00000000-0000-0000-0000-000000000000
  a 5
  b
  c 10
  d

stBlock
  in x BOOL
`
	aliases, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	alias, ok := aliases["MY_ENUM"]
	if !ok {
		t.Fatal("alias MY_ENUM not found")
	}
	wantValues := []struct {
		name             string
		value            int
		hasExplicitValue bool
	}{
		{"a", 5, true},
		{"b", 6, false},
		{"c", 10, true},
		{"d", 11, false},
	}
	if len(alias.EnumValues) != len(wantValues) {
		t.Fatalf("len(EnumValues) = %d, want %d", len(alias.EnumValues), len(wantValues))
	}
	for i, w := range wantValues {
		ev := alias.EnumValues[i]
		if ev.Name != w.name {
			t.Errorf("EnumValues[%d].Name = %q, want %q", i, ev.Name, w.name)
		}
		if ev.Value != w.value {
			t.Errorf("EnumValues[%d].Value = %d, want %d", i, ev.Value, w.value)
		}
		if ev.HasExplicitValue != w.hasExplicitValue {
			t.Errorf("EnumValues[%d].HasExplicitValue = %v, want %v", i, ev.HasExplicitValue, w.hasExplicitValue)
		}
	}
}

// TestParseEnumNoMembers verifies that a @enum with no indented members is
// valid and produces an alias with a non-nil but empty EnumValues slice.
func TestParseEnumNoMembers(t *testing.T) {
	cfg := `
@enum EMPTY_ENUM WORD 00000000-0000-0000-0000-000000000000
stBlock
  in x BOOL
`
	aliases, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	alias, ok := aliases["EMPTY_ENUM"]
	if !ok {
		t.Fatal("alias EMPTY_ENUM not found")
	}
	if alias.EnumValues == nil {
		t.Error("EnumValues should be non-nil (empty slice) for @enum with no members")
	}
	if len(alias.EnumValues) != 0 {
		t.Errorf("expected 0 members, got %d", len(alias.EnumValues))
	}
}

// TestParseEnumAtEOF verifies that an @enum block at the end of the file
// (without a trailing non-indented line) is correctly finalized.
func TestParseEnumAtEOF(t *testing.T) {
	cfg := `
stBlock
  in x BOOL

@enum TAIL_ENUM WORD 00000000-0000-0000-0000-000000000000
  first 0
  second
`
	aliases, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	alias, ok := aliases["TAIL_ENUM"]
	if !ok {
		t.Fatal("alias TAIL_ENUM not found")
	}
	if len(alias.EnumValues) != 2 {
		t.Fatalf("expected 2 members, got %d", len(alias.EnumValues))
	}
	if alias.EnumValues[1].Value != 1 {
		t.Errorf("second member value = %d, want 1", alias.EnumValues[1].Value)
	}
}

// TestParseEnumMultiple verifies that multiple @enum directives are all
// collected and coexist with @struct in the alias map.
func TestParseEnumMultiple(t *testing.T) {
	cfg := `
@enum EN_A WORD 96656ea5-0db7-49b0-86ec-56cef26b56d0
  x 0
  y

@enum EN_B WORD 4bb8098e-6846-4a59-915d-71a3e3d369c0
  alpha 0
  beta

@struct ST_PLAIN 00000000-0000-0000-0000-000000000002
  in x BOOL

stBlock
  in x BOOL
`
	aliases, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	if len(aliases) != 3 {
		t.Fatalf("expected 3 aliases, got %d", len(aliases))
	}
	enA, ok := aliases["EN_A"]
	if !ok || len(enA.EnumValues) != 2 {
		t.Errorf("EN_A: ok=%v, members=%d", ok, len(enA.EnumValues))
	}
	enB, ok := aliases["EN_B"]
	if !ok || len(enB.EnumValues) != 2 {
		t.Errorf("EN_B: ok=%v, members=%d", ok, len(enB.EnumValues))
	}
	stPlain, ok := aliases["ST_PLAIN"]
	if !ok || stPlain.EnumValues != nil || stPlain.StructDef == nil {
		t.Errorf("ST_PLAIN: ok=%v, EnumValues should be nil, StructDef should be non-nil", ok)
	}
}

// TestParseEnumCaseNormalization verifies that @enum alias names are
// normalised to upper case.
func TestParseEnumCaseNormalization(t *testing.T) {
	cfg := `
@enum myEnum DINT 00000000-0000-0000-0000-000000000000
  val 1

stBlock
  in x BOOL
`
	aliases, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	if _, ok := aliases["MYENUM"]; !ok {
		t.Error("expected @enum name to be normalised to MYENUM")
	}
}

// TestParseEnumInvalidGUID verifies that a malformed GUID in an @enum
// directive returns a parse error.
func TestParseEnumInvalidGUID(t *testing.T) {
	cfg := "@enum BAD WORD not-a-valid-guid\nstBlock\n  in x BOOL\n"
	_, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for invalid GUID, got nil")
	}
}

// TestParseEnumMissingArgs verifies that an @enum line with wrong number of
// arguments returns a parse error.
func TestParseEnumMissingArgs(t *testing.T) {
	cfg := "@enum JUST_TWO_ARGS WORD\nstBlock\n  in x BOOL\n"
	_, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for @enum with missing GUID arg, got nil")
	}
}

// TestParseEnumInvalidMemberValue verifies that an @enum member with a
// non-integer value returns a parse error.
func TestParseEnumInvalidMemberValue(t *testing.T) {
	cfg := `
@enum BAD_VAL WORD 00000000-0000-0000-0000-000000000000
  member notAnInt

stBlock
  in x BOOL
`
	_, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for invalid enum member value, got nil")
	}
}

// TestParseEnumBackwardCompatStruct verifies that @enum and @struct directives
// coexist correctly in the same config file.
func TestParseEnumBackwardCompatStruct(t *testing.T) {
	cfg := `
@enum EN_DISP_MSGTYPE WORD 96656ea5-0db7-49b0-86ec-56cef26b56d0
  none 0
  precheck

@struct ST_MSG 00000000-0000-0000-0000-000000000000
  in eType EN_DISP_MSGTYPE
  in bOk BOOL

stBlock
  struct stMsg ST_MSG
  in x BOOL
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	if len(aliases) != 2 {
		t.Fatalf("expected 2 aliases, got %d", len(aliases))
	}
	enumAlias := aliases["EN_DISP_MSGTYPE"]
	if enumAlias.EnumValues == nil {
		t.Error("EN_DISP_MSGTYPE: EnumValues should be non-nil")
	}
	structAlias := aliases["ST_MSG"]
	if structAlias.StructDef == nil {
		t.Error("ST_MSG: StructDef should be non-nil")
	}
	if len(roots) != 1 || len(roots[0].Children) != 2 {
		t.Fatalf("unexpected tree structure")
	}
	// First child is struct stMsg → container node
	if roots[0].Children[0].TypeName != "ST_MSG" {
		t.Errorf("stMsg TypeName = %q, want ST_MSG", roots[0].Children[0].TypeName)
	}
}

// ---------------------------------------------------------------------------
// Additional @struct directive tests
// ---------------------------------------------------------------------------

// TestParseStructWithNestedContainers verifies that @struct definitions can
// include sub-containers (nested structs and arrays).
func TestParseStructWithNestedContainers(t *testing.T) {
	cfg := `
@struct ST_INNER 00000000-0000-0000-0000-000000000001
  in bFlag BOOL

@struct ST_OUTER 00000000-0000-0000-0000-000000000002
  in nVal DWORD
  struct stInner ST_INNER
  struct aItems[1..3] ST_INNER

stRoot
  struct stObj ST_OUTER
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}

	outer, ok := aliases["ST_OUTER"]
	if !ok || outer.StructDef == nil {
		t.Fatal("ST_OUTER not found or StructDef nil")
	}
	// nVal, stInner (container), aItems (array)
	if len(outer.StructDef) != 3 {
		t.Fatalf("ST_OUTER StructDef len = %d, want 3", len(outer.StructDef))
	}

	// stInner should be a struct-keyword container with TypeName ST_INNER
	innerNode := outer.StructDef[1]
	if innerNode.Name != "stInner" || innerNode.TypeName != "ST_INNER" {
		t.Errorf("StructDef[1] = {Name:%q TypeName:%q}, want {stInner ST_INNER}", innerNode.Name, innerNode.TypeName)
	}

	// aItems should be an inline struct array with TypeName ST_INNER
	arrNode := outer.StructDef[2]
	if arrNode.Name != "aItems" || arrNode.ArrayStart != 1 || arrNode.ArrayEnd != 3 {
		t.Errorf("StructDef[2] array = %+v, want aItems[1..3]", arrNode)
	}
	if arrNode.TypeName != "ST_INNER" {
		t.Errorf("aItems TypeName = %q, want ST_INNER", arrNode.TypeName)
	}

	// The tree node should have TypeName=ST_OUTER and cloned children
	if len(roots) != 1 || len(roots[0].Children) != 1 {
		t.Fatalf("unexpected tree structure")
	}
	stObj := roots[0].Children[0]
	if stObj.TypeName != "ST_OUTER" || len(stObj.Children) != 3 {
		t.Errorf("stObj: TypeName=%q children=%d, want ST_OUTER/3", stObj.TypeName, len(stObj.Children))
	}
}

// TestParseStructWithEnumRef verifies that @struct members can reference @enum
// type names.
func TestParseStructWithEnumRef(t *testing.T) {
	cfg := `
@enum EN_STATE WORD 00000000-0000-0000-0000-000000000001
  idle 0
  running

@struct ST_STATUS 00000000-0000-0000-0000-000000000002
  in eState EN_STATE
  in bReady BOOL

stRoot
  struct stStatus ST_STATUS
`
	aliases, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	st, ok := aliases["ST_STATUS"]
	if !ok || st.StructDef == nil {
		t.Fatal("ST_STATUS not found or StructDef nil")
	}
	// First member has TypeName EN_STATE (enum reference)
	if st.StructDef[0].TypeName != "EN_STATE" {
		t.Errorf("StructDef[0].TypeName = %q, want EN_STATE", st.StructDef[0].TypeName)
	}

	// In the tree, struct stStatus should have cloned children with EN_STATE ref
	stStatus := roots[0].Children[0]
	if stStatus.TypeName != "ST_STATUS" {
		t.Fatalf("stStatus TypeName = %q, want ST_STATUS", stStatus.TypeName)
	}
	if stStatus.Children[0].TypeName != "EN_STATE" {
		t.Errorf("stStatus.Children[0].TypeName = %q, want EN_STATE", stStatus.Children[0].TypeName)
	}
}

// TestParseStructMissingGUID verifies that a @struct directive with a missing
// GUID returns a parse error.
func TestParseStructMissingGUID(t *testing.T) {
	cfg := "@struct NOARGS\nstBlock\n  in x BOOL\n"
	_, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for @struct with missing GUID, got nil")
	}
}

// TestParseStructInvalidGUID verifies that a @struct with a malformed GUID
// returns a parse error.
func TestParseStructInvalidGUID(t *testing.T) {
	cfg := "@struct ST_BAD not-a-guid\nstBlock\n  in x BOOL\n"
	_, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for @struct with invalid GUID, got nil")
	}
}

// TestParseStructUndefinedRef verifies that using struct keyword with an
// undefined type name returns a parse error.
func TestParseStructUndefinedRef(t *testing.T) {
	cfg := "stBlock\n  struct stX UNDEFINED_TYPE\n"
	_, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err == nil {
		t.Error("expected error for struct referencing undefined type, got nil")
	}
}

// TestParseStructNoMembers verifies that a @struct with no indented members
// is valid and produces an alias with a non-nil but empty StructDef.
func TestParseStructNoMembers(t *testing.T) {
	cfg := `
@struct EMPTY_STRUCT 00000000-0000-0000-0000-000000000000
stBlock
  in x BOOL
`
	aliases, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	alias, ok := aliases["EMPTY_STRUCT"]
	if !ok {
		t.Fatal("alias EMPTY_STRUCT not found")
	}
	if alias.StructDef == nil {
		t.Error("StructDef should be non-nil (empty slice) for @struct with no members")
	}
	if len(alias.StructDef) != 0 {
		t.Errorf("expected 0 members, got %d", len(alias.StructDef))
	}
}

// TestParseStructAtEOF verifies that a @struct block at the end of the file
// (without a trailing non-indented line) is correctly finalized.
func TestParseStructAtEOF(t *testing.T) {
	cfg := `
stBlock
  in x BOOL

@struct TAIL_STRUCT 00000000-0000-0000-0000-000000000000
  in bFlag BOOL
  out nVal DINT
`
	aliases, _, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	alias, ok := aliases["TAIL_STRUCT"]
	if !ok {
		t.Fatal("alias TAIL_STRUCT not found")
	}
	if len(alias.StructDef) != 2 {
		t.Fatalf("expected 2 members, got %d", len(alias.StructDef))
	}
	if alias.StructDef[1].Name != "nVal" {
		t.Errorf("member[1].Name = %q, want nVal", alias.StructDef[1].Name)
	}
}

// TestParseStructCloningIsDeep verifies that two separate struct instances
// (from the same @struct definition) have independent children (deep cloned).
func TestParseStructCloningIsDeep(t *testing.T) {
	cfg := `
@struct ST_FOO 00000000-0000-0000-0000-000000000000
  in bFlag BOOL

stRoot
  struct a ST_FOO
  struct b ST_FOO
`
	_, roots, err := ParseTreeWithAliases(strings.NewReader(cfg))
	if err != nil {
		t.Fatalf("ParseTreeWithAliases: %v", err)
	}
	nodeA := roots[0].Children[0]
	nodeB := roots[0].Children[1]

	// Both containers have the same structure but different node pointers.
	if nodeA == nodeB {
		t.Error("both struct instances share the same container pointer (not deep cloned)")
	}
	if len(nodeA.Children) != 1 || len(nodeB.Children) != 1 {
		t.Fatalf("expected 1 child each, got %d and %d", len(nodeA.Children), len(nodeB.Children))
	}
	if nodeA.Children[0] == nodeB.Children[0] {
		t.Error("struct instances share a child pointer (not deep cloned)")
	}
}
