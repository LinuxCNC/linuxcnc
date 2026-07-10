// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package cgen

import (
	"bytes"
	"go/parser"
	"go/token"
	"testing"

	gmiparser "github.com/sittner/linuxcnc/src/gomc/internal/gmicompile/parser"
)

// genDispatch parses src and generates the production cgo dispatch file (the
// REST server path), asserting the result is syntactically valid Go.
func genDispatch(t *testing.T, src string) string {
	t.Helper()
	api, perrs := gmiparser.Parse("test.gmi", src)
	if len(perrs) > 0 {
		t.Fatalf("parse errors: %v", perrs)
	}
	var buf bytes.Buffer
	if err := GenerateDispatchC(&buf, api, "testpkg", api.Name+"_api.h"); err != nil {
		t.Fatalf("GenerateDispatchC: %v", err)
	}
	out := buf.String()
	// Parse as Go (cgo files are valid Go syntax); confirms the emit is well-formed.
	if _, err := parser.ParseFile(token.NewFileSet(), "gen.go", out, parser.SkipObjectResolution); err != nil {
		t.Fatalf("generated code is not valid Go: %v\n---\n%s", err, out)
	}
	return out
}

func TestGenerateValidationPutTool(t *testing.T) {
	src := `@api tooltable
@version 1
@license "GPL Version 2"
@rest_export true

enum Mode {
    A = 0
    B = 2
}

type ToolEntry {
    toolno:      i32    @min(1) @max(99999)
    diameter:    f64    @min(0)
    orientation: i32    @min(0) @max(8)
    comment:     string @maxlen(255) @regex("^[a-z]+$")
    mode:        Mode
}

@method PUT
@path /{toolno}
func put_tool(toolno: i32 @min(1), entry: ToolEntry) -> i32
`
	out := genDispatch(t, src)

	// Compiled regex var, package scope (single set — no per-transport prefix).
	assertContains(t, out, `var tooltable0 = apiserver.MustRegex("^[a-z]+$")`)

	// A single standalone validator; the dispatch and WS handler both call it.
	assertContains(t, out, "func validateTooltablePutTool(toolno int32, entry ToolEntry) *apiserver.ValidationError {")
	assertContains(t, out, "if verr := validateTooltablePutTool(params.Toolno, params.Entry); verr != nil {")

	// Body uses the arg names (not params.X).
	assertContains(t, out, "if toolno < 1 {")
	assertContains(t, out, `apiserver.NewValidationError("toolno", "min", "toolno must be >= 1")`)

	// Nested struct field, dotted path.
	assertContains(t, out, "if entry.Toolno < 1 {")
	assertContains(t, out, `apiserver.NewValidationError("entry.toolno", "max", "entry.toolno must be <= 99999")`)
	assertContains(t, out, "if entry.Diameter < 0 {")
	assertContains(t, out, "if entry.Orientation > 8 {")

	// String length uses RuneLen.
	assertContains(t, out, "if apiserver.RuneLen(entry.Comment) > 255 {")

	// Regex references the compiled var.
	assertContains(t, out, `apiserver.ValidateRegex("entry.comment", tooltable0, entry.Comment)`)

	// Automatic enum membership (non-contiguous 0,2), constant-name cases.
	assertContains(t, out, "switch entry.Mode {")
	assertContains(t, out, "case Mode_A, Mode_B:")
	assertContains(t, out, `apiserver.NewValidationError("entry.mode", "enum", fmt.Sprintf("%s: invalid enum value %d", "entry.mode", int32(entry.Mode)))`)
}

func TestGenerateValidationSliceElemsAndEnumOpen(t *testing.T) {
	src := `@api demo
@version 1
@license "GPL Version 2"
@rest_export true

enum Kind {
    A = 0
    B = 1
}

type Point {
    x: f64 @min(0)
    k: Kind
}

@method POST
@path /pts
func set_points(pts: []Point, mode: Kind @enum_open) -> i32
`
	out := genDispatch(t, src)

	// Element recursion with a runtime index path (per-depth index i0). The
	// standalone validator body uses the arg name `pts`.
	assertContains(t, out, "for i0 := range pts {")
	assertContains(t, out, "if pts[i0].X < 0 {")
	assertContains(t, out, `fmt.Sprintf("%s[%d]", "pts", i0)`)

	// Per-element enum still auto-checked.
	assertContains(t, out, "switch pts[i0].K {")

	// @enum_open param: no membership switch emitted for mode.
	if bytes.Contains([]byte(out), []byte("switch mode {")) {
		t.Error("@enum_open param should not emit an enum switch")
	}
}

// TestGenerateValidationNestedSlices is the C13 regression: a slice-of-slices
// must emit distinct per-depth range indices (i0, i1) instead of shadowing a
// single `i`, which silently validated the diagonal (and could index OOB).
func TestGenerateValidationNestedSlices(t *testing.T) {
	src := `@api demo
@version 1
@license "GPL Version 2"
@rest_export true

type Row {
    cells: [][]Cell
}

type Cell {
    v: f64 @min(0)
}

@method POST
@path /grid
func set_grid(rows: []Row) -> i32
`
	out := genDispatch(t, src)

	// Distinct index per nesting level — no reused `i`. Body uses arg name `rows`.
	assertContains(t, out, "for i0 := range rows {")
	assertContains(t, out, "for i1 := range rows[i0].Cells {")
	assertContains(t, out, "for i2 := range rows[i0].Cells[i1] {")
	// The innermost constraint indexes all three levels, not a diagonal.
	assertContains(t, out, "if rows[i0].Cells[i1][i2].V < 0 {")
	if bytes.Contains([]byte(out), []byte("for i := range")) {
		t.Error("nested slices must not reuse a single loop index `i` (C13 shadowing)")
	}
}

func TestGenerateNoConstraintsNoValidation(t *testing.T) {
	src := `@api plain
@version 1
@license "GPL Version 2"
@rest_export true

@method GET
@path /x
func get_x(n: i32) -> i32
`
	out := genDispatch(t, src)
	if bytes.Contains([]byte(out), []byte("--- validation")) {
		t.Error("no constraints should emit no validation section")
	}
}

// TestGenerateWSCommandValidation checks the WebSocket command handlers (emitted
// into _bridge.go by GenerateServerGoExtra) carry the same validation, reindented
// into the handler closure and referencing the regex var declared in _cgo.go.
func TestGenerateWSCommandValidation(t *testing.T) {
	src := `@api tooltable
@version 1
@license "GPL Version 2"
@rest_export true

type ToolEntry {
    toolno:  i32    @min(1)
    comment: string @regex("^[a-z]+$")
}

@method PUT
@path /{toolno}
func put_tool(toolno: i32 @min(1), entry: ToolEntry) -> i32
`
	api, perrs := gmiparser.Parse("test.gmi", src)
	if len(perrs) > 0 {
		t.Fatalf("parse errors: %v", perrs)
	}
	var buf bytes.Buffer
	if err := GenerateServerGoExtra(&buf, api, "testpkg"); err != nil {
		t.Fatalf("GenerateServerGoExtra: %v", err)
	}
	out := buf.String()

	// The WS command handler calls the shared validate<Api><Fn> (defined once in
	// _cgo.go, same package), not an inlined-and-reindented copy of the checks or
	// a duplicate compiled regex var (D8).
	assertContains(t, out, "if verr := validateTooltablePutTool(params.Toolno, params.Entry); verr != nil {")
	if bytes.Contains([]byte(out), []byte("apiserver.MustRegex")) {
		t.Error("WS bridge must not declare its own compiled regex vars (D8: shared validator)")
	}
	if bytes.Contains([]byte(out), []byte("if params.Toolno < 1")) {
		t.Error("WS bridge must not inline the validation checks (D8: shared validator)")
	}
}
