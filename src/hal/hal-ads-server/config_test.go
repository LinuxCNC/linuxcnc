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
