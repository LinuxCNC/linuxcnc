// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package check

import (
	"strings"
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/internal/gmicompile/parser"
)

// validate parses src and runs the constraint checker, returning the errors as
// a single joined string for substring assertions.
func validate(t *testing.T, src string) string {
	t.Helper()
	api, perrs := parser.Parse("test.gmi", src)
	if len(perrs) > 0 {
		t.Fatalf("unexpected parse errors: %v", perrs)
	}
	var msgs []string
	for _, e := range Validate(api) {
		msgs = append(msgs, e.Error())
	}
	return strings.Join(msgs, "\n")
}

func TestValidateAcceptsWellTypedConstraints(t *testing.T) {
	src := `@api test
@version 1

enum Mode {
    A = 0
    B = 2
}

type Rec {
    n:     i32     @min(1) @max(10)
    ratio: f64     @min(0.0) @max(1.0)
    name:  string  @minlen(1) @maxlen(64) @regex("^[a-z]+$")
    tags:  []i32   @notempty
    fixed: [4]f64  @minlen(1)
    mode:  Mode    @enum_open
    opt:   i32?    @notnull
}

func f(n: i32 @min(0)) -> i32
`
	if got := validate(t, src); got != "" {
		t.Fatalf("expected no errors, got:\n%s", got)
	}
}

func TestValidateRejections(t *testing.T) {
	cases := []struct {
		name string
		body string // field/func lines inside the API
		want string
	}{
		{"min on string", `type T { s: string @min(0) }`, "@min applies to numeric"},
		{"maxlen on int", `type T { n: i32 @maxlen(5) }`, "@maxlen applies to string/slice/array"},
		{"regex on int", `type T { n: i32 @regex("x") }`, "@regex applies to string"},
		{"int overflow", `type T { n: i32 @max(9999999999) }`, "out of range for i32"},
		{"float on int", `type T { n: i32 @min(0.5) }`, "out of range for i32"},
		{"negative unsigned", `type T { n: u32 @min(-1) }`, "negative on unsigned"},
		{"min gt max", `type T { n: i32 @min(10) @max(1) }`, "@min(10) > @max(1)"},
		{"redundant notnull", `type T { s: string @notnull }`, "redundant on non-nullable"},
		{"notnull on nullable string", `type T { s: string? @notnull }`, "not expressible"},
		{"unsatisfiable len", `type T { xs: [4]f64 @minlen(9) }`, "exceeds fixed array length 4"},
		{"bad regex", `type T { s: string @regex("(") }`, "does not compile"},
		{"duplicate", `type T { n: i32 @min(0) @min(1) }`, "duplicate constraint @min"},
		{"constraint on out param", `func f(x: i32 out @min(0)) -> i32`, "out (output) parameter"},
		{"enum_open on non-enum", `type T { n: i32 @enum_open }`, "@enum_open applies to enum"},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			src := "@api test\n@version 1\n\n" + tc.body + "\n"
			got := validate(t, src)
			if !strings.Contains(got, tc.want) {
				t.Errorf("errors = %q, want substring %q", got, tc.want)
			}
		})
	}
}

func TestValidateEnumMinMaxRedundant(t *testing.T) {
	src := `@api test
@version 1

enum Mode {
    A = 0
    B = 1
}

type T {
    m: Mode @max(1)
}
`
	if got := validate(t, src); !strings.Contains(got, "redundant with automatic enum validation") {
		t.Errorf("errors = %q, want enum redundancy message", got)
	}
}
