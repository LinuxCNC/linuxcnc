// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package cgen

import (
	"bytes"
	"strings"
	"testing"

	gmiparser "github.com/sittner/linuxcnc/src/gomc/internal/gmicompile/parser"
)

const clientValidationIDL = `@api tooltable
@version 1
@license "GPL Version 2"
@rest_export true

enum Mode {
    A = 0
    B = 2
}

type ToolEntry {
    toolno:  i32    @min(1) @max(99999)
    comment: string @maxlen(255) @regex("^[a-z]+$")
    mode:    Mode
    tags:    []string @maxlen(8)
    note:    string? @maxlen(40)
}

@method PUT
@path /{toolno}
func put_tool(toolno: i32 @min(1), entry: ToolEntry) -> i32
`

func TestClientValidationTS(t *testing.T) {
	api, perrs := gmiparser.Parse("test.gmi", clientValidationIDL)
	if len(perrs) > 0 {
		t.Fatalf("parse errors: %v", perrs)
	}
	var buf bytes.Buffer
	if err := GenerateClientTS(&buf, api); err != nil {
		t.Fatalf("GenerateClientTS: %v", err)
	}
	out := buf.String()

	assertContains(t, out, "export class ValidationError extends Error {")
	assertContains(t, out, "const _verrs: string[] = [];")
	assertContains(t, out, `if (toolno < 1) {`)
	assertContains(t, out, `_verrs.push("entry.toolno must be <= 99999");`)
	assertContains(t, out, `if ([...entry.comment].length > 255) {`) // code-point length
	assertContains(t, out, `if (![0, 2].includes(entry.mode)) {`)    // enum membership
	assertContains(t, out, `if (entry.tags.length > 8) {`)           // slice length ("items")
	assertContains(t, out, `if (entry.note !== undefined && entry.note !== null) {`) // nullable guard
	assertContains(t, out, "if (_verrs.length > 0) throw new ValidationError(_verrs);")

	// @regex is server-only — must NOT appear in the client.
	if strings.Contains(out, "^[a-z]+$") {
		t.Error("client must not emit @regex checks (server-only)")
	}
}

func TestClientValidationPython(t *testing.T) {
	api, perrs := gmiparser.Parse("test.gmi", clientValidationIDL)
	if len(perrs) > 0 {
		t.Fatalf("parse errors: %v", perrs)
	}
	var buf bytes.Buffer
	if err := GenerateClientPython(&buf, api); err != nil {
		t.Fatalf("GenerateClientPython: %v", err)
	}
	out := buf.String()

	assertContains(t, out, "class ValidationError(Exception):")
	assertContains(t, out, "_verrs = []")
	assertContains(t, out, `if toolno < 1:`)
	assertContains(t, out, `_verrs.append("entry.toolno must be <= 99999")`)
	assertContains(t, out, `if len(entry.comment) > 255:`)
	assertContains(t, out, `if entry.mode not in (0, 2):`) // enum membership
	assertContains(t, out, `if len(entry.tags) > 8:`)      // slice length ("items")
	assertContains(t, out, `if entry.note is not None:`)   // nullable guard
	assertContains(t, out, "raise ValidationError(_verrs)")

	if strings.Contains(out, "^[a-z]+$") {
		t.Error("client must not emit @regex checks (server-only)")
	}
}

// A single-value enum uses a trailing-comma tuple in Python.
func TestClientValidationPythonSingleEnum(t *testing.T) {
	src := `@api demo
@version 1
@license "GPL Version 2"
@rest_export true

enum Solo { ONLY = 0 }

@method POST
@path /x
func f(s: Solo) -> i32
`
	api, perrs := gmiparser.Parse("test.gmi", src)
	if len(perrs) > 0 {
		t.Fatalf("parse errors: %v", perrs)
	}
	var buf bytes.Buffer
	if err := GenerateClientPython(&buf, api); err != nil {
		t.Fatalf("GenerateClientPython: %v", err)
	}
	assertContains(t, buf.String(), "not in (0,)")
}
