// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package cgen

import (
	"bytes"
	"strings"
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/internal/gmicompile/ast"
)

func TestTsEscapeReserved(t *testing.T) {
	tests := []struct {
		input string
		want  string
	}{
		{"function", "function_"},
		{"class", "class_"},
		{"return", "return_"},
		{"name", "name"},
		{"thread", "thread"},
		{"await", "await_"},
		{"async", "async_"},
	}
	for _, tt := range tests {
		got := tsEscapeReserved(tt.input)
		if got != tt.want {
			t.Errorf("tsEscapeReserved(%q) = %q, want %q", tt.input, got, tt.want)
		}
	}
}

func TestToCamelCaseTSReservedWord(t *testing.T) {
	// "function" is a single word — should become "function_"
	got := toCamelCaseTS("function")
	if got != "function_" {
		t.Errorf("toCamelCaseTS(\"function\") = %q, want \"function_\"", got)
	}

	// "class_name" → "className" (not reserved after camelCase)
	got = toCamelCaseTS("class_name")
	if got != "className" {
		t.Errorf("toCamelCaseTS(\"class_name\") = %q, want \"className\"", got)
	}
}

func TestGenerateClientTSReservedParam(t *testing.T) {
	// Ensure a function with a "function" parameter generates valid TS
	api := &ast.API{
		Name:       "testapi",
		Version:    1,
		Prefix:     "testapi",
		RestExport: true,
		Funcs: []ast.Func{
			{
				Name:   "addf",
				Method: "POST",
				Path:   "/thread/{thread}/function",
				Params: []ast.Param{
					{Name: "thread", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "string"}},
					{Name: "function", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "string"}},
				},
				Return: &ast.TypeRef{Kind: ast.TypePrimitive, Name: "i32"},
			},
		},
	}

	var buf bytes.Buffer
	if err := GenerateClientTS(&buf, api); err != nil {
		t.Fatal(err)
	}

	output := buf.String()

	// Must NOT contain "function:" as a bare parameter name
	if strings.Contains(output, "function: string") {
		t.Error("generated output contains bare 'function' as param — should be escaped")
	}

	// Must contain the escaped version
	if !strings.Contains(output, "function_: string") {
		t.Error("generated output missing 'function_' escaped param name")
	}
}
