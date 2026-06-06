package cgen

import (
	"bytes"
	"strings"
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/internal/gmicompile/ast"
)

func TestServerHeaderCallbackAndPtr(t *testing.T) {
	api := &ast.API{
		Name:    "mcode_handler",
		Version: 1,
		Types: []ast.Type{
			{
				Name: "McodeCall",
				Fields: []ast.Field{
					{Name: "abort_fd", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "i32"}},
					{Name: "mcode", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "i32"}},
					{Name: "p_number", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "f64"}},
					{Name: "q_number", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "f64"}},
				},
			},
		},
		Callbacks: []ast.Callback{
			{
				Name: "handler",
				Params: []ast.Param{
					{Name: "call", Type: ast.TypeRef{Kind: ast.TypeNamed, Name: "McodeCall"}},
					{Name: "user_data", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "ptr"}},
				},
				Return: &ast.TypeRef{Kind: ast.TypePrimitive, Name: "i32"},
			},
		},
		Funcs: []ast.Func{
			{
				Name: "register_handler",
				Params: []ast.Param{
					{Name: "mcode", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "i32"}},
					{Name: "fn", Type: ast.TypeRef{Kind: ast.TypeCallback, Name: "handler"}},
					{Name: "user_data", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "ptr"}},
				},
				Return: &ast.TypeRef{Kind: ast.TypePrimitive, Name: "i32"},
			},
		},
	}

	var buf bytes.Buffer
	err := GenerateServerHeader(&buf, api)
	if err != nil {
		t.Fatalf("GenerateServerHeader error: %v", err)
	}
	out := buf.String()

	// Struct type
	assertContains(t, out, "typedef struct mcode_handler_mcode_call {")
	assertContains(t, out, "int32_t abort_fd;")
	assertContains(t, out, "double q_number;")

	// Callback typedef — note: no void *ctx (it's a user callback, not an API callback)
	assertContains(t, out, "typedef int32_t (*mcode_handler_handler_cb)(")
	assertContains(t, out, "const mcode_handler_mcode_call_t *call")
	assertContains(t, out, "void *user_data")

	// Func callback typedef — has void *ctx (it's in the API struct)
	assertContains(t, out, "typedef int32_t (*mcode_handler_register_handler_fn)(")

	// The fn param should use the callback type
	assertContains(t, out, "mcode_handler_handler_cb fn")

	// ptr param should be void*
	if !strings.Contains(out, "void *user_data") {
		t.Error("ptr param should generate void *user_data")
	}

	// Callbacks struct
	assertContains(t, out, "typedef struct mcode_handler_callbacks {")
	assertContains(t, out, "mcode_handler_register_handler_fn register_handler;")

	// Registration
	assertContains(t, out, "static inline int mcode_handler_api_register(")
	assertContains(t, out, "static inline const mcode_handler_callbacks_t *mcode_handler_api_get(")
}

func TestServerHeaderImport(t *testing.T) {
	api := &ast.API{
		Name:    "interp_ext",
		Version: 1,
		Imports: []ast.Import{
			{Name: "interp_ctx"},
		},
		Callbacks: []ast.Callback{
			{
				Name: "oword_fn",
				Params: []ast.Param{
					{Name: "ctx", Type: ast.TypeRef{Kind: ast.TypeImport, Name: "interp_ctx"}, IsPtr: true},
					{Name: "name", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "string"}},
				},
				Return: &ast.TypeRef{Kind: ast.TypePrimitive, Name: "i32"},
			},
		},
		Funcs: []ast.Func{
			{
				Name: "register_oword",
				Params: []ast.Param{
					{Name: "name", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "string"}},
					{Name: "fn", Type: ast.TypeRef{Kind: ast.TypeCallback, Name: "oword_fn"}},
					{Name: "user", Type: ast.TypeRef{Kind: ast.TypePrimitive, Name: "ptr"}},
				},
				Return: &ast.TypeRef{Kind: ast.TypePrimitive, Name: "i32"},
			},
		},
	}

	var buf bytes.Buffer
	err := GenerateServerHeader(&buf, api)
	if err != nil {
		t.Fatalf("GenerateServerHeader error: %v", err)
	}
	out := buf.String()

	// Import generates #include
	assertContains(t, out, `#include "interp_ctx_api.h"`)

	// Callback uses imported type as mutable pointer (IsPtr=true)
	assertContains(t, out, "typedef int32_t (*interp_ext_oword_fn_cb)(")
	assertContains(t, out, "interp_ctx_callbacks_t *ctx")

	// Function param uses callback type
	assertContains(t, out, "interp_ext_oword_fn_cb fn")

	// ptr param
	if !strings.Contains(out, "void *user") {
		t.Error("ptr param should generate void *user")
	}
}
