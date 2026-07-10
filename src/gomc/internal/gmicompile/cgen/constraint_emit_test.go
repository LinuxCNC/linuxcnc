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

	// Compiled regex var, package scope.
	assertContains(t, out, `var tooltableRe0 = apiserver.MustRegex("^[a-z]+$")`)

	// Validation section marker.
	assertContains(t, out, "// --- validation (generated from @constraints) ---")

	// Param constraint.
	assertContains(t, out, "if params.Toolno < 1 {")
	assertContains(t, out, `apiserver.NewValidationError("toolno", "min", "toolno must be >= 1")`)

	// Nested struct field, dotted path.
	assertContains(t, out, "if params.Entry.Toolno < 1 {")
	assertContains(t, out, `apiserver.NewValidationError("entry.toolno", "max", "entry.toolno must be <= 99999")`)
	assertContains(t, out, "if params.Entry.Diameter < 0 {")
	assertContains(t, out, "if params.Entry.Orientation > 8 {")

	// String length uses RuneLen.
	assertContains(t, out, "if apiserver.RuneLen(params.Entry.Comment) > 255 {")

	// Regex references the compiled var.
	assertContains(t, out, `apiserver.ValidateRegex("entry.comment", tooltableRe0, params.Entry.Comment)`)

	// Automatic enum membership (non-contiguous 0,2), constant-name cases.
	assertContains(t, out, "switch params.Entry.Mode {")
	assertContains(t, out, "case Mode_A, Mode_B:")
	assertContains(t, out, `apiserver.NewValidationError("entry.mode", "enum", fmt.Sprintf("%s: invalid enum value %d", "entry.mode", int32(params.Entry.Mode)))`)
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

	// Element recursion with a runtime index path.
	assertContains(t, out, "for i := range params.Pts {")
	assertContains(t, out, "if params.Pts[i].X < 0 {")
	assertContains(t, out, `fmt.Sprintf("%s[%d]", "pts", i)`)

	// Per-element enum still auto-checked.
	assertContains(t, out, "switch params.Pts[i].K {")

	// @enum_open param: no membership switch emitted for mode.
	if bytes.Contains([]byte(out), []byte("switch params.Mode {")) {
		t.Error("@enum_open param should not emit an enum switch")
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

	// Validation present inside the command handler, reindented to closure depth.
	assertContains(t, out, "// --- validation (generated from @constraints) ---")
	assertContains(t, out, "\t\t\tif params.Toolno < 1 {")
	assertContains(t, out, "\t\t\tif params.Entry.Toolno < 1 {")
	// Regex var is referenced (declared separately in _cgo.go), not re-declared here.
	assertContains(t, out, "apiserver.ValidateRegex(\"entry.comment\", tooltableRe0, params.Entry.Comment)")
	if bytes.Contains([]byte(out), []byte("tooltableRe0 = apiserver.MustRegex")) {
		t.Error("regex var must NOT be re-declared in the bridge file (lives in _cgo.go)")
	}
}
