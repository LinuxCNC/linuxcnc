// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package parser

import (
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/internal/gmicompile/ast"
)

func TestParseSimpleAPI(t *testing.T) {
	src := `@api test
@version 1
@prefix test
@rest_export true

enum Status {
    OK = 0
    ERROR = 1
}

type Result {
    success: bool
    message: string?
}

@method "GET"
@path "/status"
@rt_safe "true"
func get_status() -> Status
`
	api, errors := Parse("test.gmi", src)
	if len(errors) > 0 {
		t.Fatalf("Parse errors: %v", errors)
	}

	if api.Name != "test" {
		t.Errorf("Name = %q, want %q", api.Name, "test")
	}
	if api.Version != 1 {
		t.Errorf("Version = %d, want 1", api.Version)
	}
	if api.Prefix != "test" {
		t.Errorf("Prefix = %q, want %q", api.Prefix, "test")
	}
	if !api.RestExport {
		t.Error("RestExport = false, want true")
	}

	// Enum
	if len(api.Enums) != 1 {
		t.Fatalf("len(Enums) = %d, want 1", len(api.Enums))
	}
	if api.Enums[0].Name != "Status" {
		t.Errorf("Enum[0].Name = %q, want %q", api.Enums[0].Name, "Status")
	}
	if len(api.Enums[0].Values) != 2 {
		t.Fatalf("len(Enum[0].Values) = %d, want 2", len(api.Enums[0].Values))
	}

	// Type
	if len(api.Types) != 1 {
		t.Fatalf("len(Types) = %d, want 1", len(api.Types))
	}
	if api.Types[0].Name != "Result" {
		t.Errorf("Type[0].Name = %q, want %q", api.Types[0].Name, "Result")
	}
	if len(api.Types[0].Fields) != 2 {
		t.Fatalf("len(Type[0].Fields) = %d, want 2", len(api.Types[0].Fields))
	}
	if api.Types[0].Fields[1].Type.Nullable != true {
		t.Error("Type[0].Fields[1].Nullable = false, want true")
	}

	// Func
	if len(api.Funcs) != 1 {
		t.Fatalf("len(Funcs) = %d, want 1", len(api.Funcs))
	}
	fn := api.Funcs[0]
	if fn.Name != "get_status" {
		t.Errorf("Func[0].Name = %q, want %q", fn.Name, "get_status")
	}
	if fn.Method != "GET" {
		t.Errorf("Func[0].Method = %q, want %q", fn.Method, "GET")
	}
	if fn.Path != "/status" {
		t.Errorf("Func[0].Path = %q, want %q", fn.Path, "/status")
	}
	if fn.RTSafe != true {
		t.Error("Func[0].RTSafe = false, want true")
	}
}

func TestParseSliceTypes(t *testing.T) {
	src := `@api test
@version 1

type Item {
    name: string
}

type List {
    items: []Item
}

@method "GET"
@path "/items"
@rt_safe "false"
func get_items() -> []Item
`
	api, errors := Parse("test.gmi", src)
	if len(errors) > 0 {
		t.Fatalf("Parse errors: %v", errors)
	}

	if len(api.Types) != 2 {
		t.Fatalf("len(Types) = %d, want 2", len(api.Types))
	}

	list := api.Types[1]
	if list.Fields[0].Type.Kind != ast.TypeSlice {
		t.Errorf("Field[0].Type.Kind = %v, want TypeSlice", list.Fields[0].Type.Kind)
	}

	fn := api.Funcs[0]
	if fn.Return.Kind != ast.TypeSlice {
		t.Errorf("Return.Kind = %v, want TypeSlice", fn.Return.Kind)
	}
}

func TestParseArrayTypes(t *testing.T) {
	src := `@api test
@version 1

type Position {
    coords: [3]f64
}
`
	api, errors := Parse("test.gmi", src)
	if len(errors) > 0 {
		t.Fatalf("Parse errors: %v", errors)
	}

	if len(api.Types) != 1 {
		t.Fatalf("len(Types) = %d, want 1", len(api.Types))
	}

	field := api.Types[0].Fields[0]
	if field.Type.Kind != ast.TypeArray {
		t.Errorf("Field.Type.Kind = %v, want TypeArray", field.Type.Kind)
	}
	if field.Type.ArrayLen != 3 {
		t.Errorf("Field.Type.ArrayLen = %d, want 3", field.Type.ArrayLen)
	}
}

func TestParseNegativeEnumValue(t *testing.T) {
	src := `@api test
@version 1

enum Type {
    UNKNOWN = -1
    DEFAULT = 0
}
`
	api, errors := Parse("test.gmi", src)
	if len(errors) > 0 {
		t.Fatalf("Parse errors: %v", errors)
	}

	if api.Enums[0].Values[0].Value != -1 {
		t.Errorf("Values[0].Value = %d, want -1", api.Enums[0].Values[0].Value)
	}
}

func TestParseConst(t *testing.T) {
	src := `@api test
@version 1

const MAX_JOINTS = 16

type Joints {
    values: [MAX_JOINTS]f64
}
`
	api, errors := Parse("test.gmi", src)
	if len(errors) > 0 {
		t.Fatalf("Parse errors: %v", errors)
	}

	if len(api.Consts) != 1 {
		t.Fatalf("len(Consts) = %d, want 1", len(api.Consts))
	}
	if api.Consts[0].Name != "MAX_JOINTS" {
		t.Errorf("Consts[0].Name = %q, want %q", api.Consts[0].Name, "MAX_JOINTS")
	}
	if api.Consts[0].Value != 16 {
		t.Errorf("Consts[0].Value = %d, want 16", api.Consts[0].Value)
	}

	// Array should resolve named size
	field := api.Types[0].Fields[0]
	if field.Type.Kind != ast.TypeArray {
		t.Fatalf("Field.Type.Kind = %v, want TypeArray", field.Type.Kind)
	}
	if field.Type.ArrayLen != 16 {
		t.Errorf("Field.Type.ArrayLen = %d, want 16", field.Type.ArrayLen)
	}
	if field.Type.ArrayLenName != "MAX_JOINTS" {
		t.Errorf("Field.Type.ArrayLenName = %q, want %q", field.Type.ArrayLenName, "MAX_JOINTS")
	}
}

func TestParseByRef(t *testing.T) {
	src := `@api test
@version 1

type Pose {
    x: f64
    y: f64
}

func forward(joints: []f64, world: Pose byref, flags: u64 byref) -> i32
`
	api, errors := Parse("test.gmi", src)
	if len(errors) > 0 {
		t.Fatalf("Parse errors: %v", errors)
	}

	fn := api.Funcs[0]
	if len(fn.Params) != 3 {
		t.Fatalf("len(Params) = %d, want 3", len(fn.Params))
	}

	// joints: []f64 — no byref
	if fn.Params[0].ByRef {
		t.Error("Params[0].ByRef = true, want false")
	}
	// world: Pose byref
	if !fn.Params[1].ByRef {
		t.Error("Params[1].ByRef = false, want true")
	}
	if fn.Params[1].Type.Kind != ast.TypeNamed {
		t.Errorf("Params[1].Type.Kind = %v, want TypeNamed", fn.Params[1].Type.Kind)
	}
	// flags: u64 byref
	if !fn.Params[2].ByRef {
		t.Error("Params[2].ByRef = false, want true")
	}
	if fn.Params[2].Type.Kind != ast.TypePrimitive {
		t.Errorf("Params[2].Type.Kind = %v, want TypePrimitive", fn.Params[2].Type.Kind)
	}
}

func TestParsePtrIsPrimitive(t *testing.T) {
	src := `@api test
@version 1

func ok(handle: ptr) -> i32
`
	api, errors := Parse("test.gmi", src)
	if len(errors) > 0 {
		t.Fatalf("Parse errors: %v", errors)
	}

	// "ptr" is a primitive type — maps to void* in C
	p := api.Funcs[0].Params[0]
	if p.Type.Kind != ast.TypePrimitive {
		t.Errorf("ptr should be TypePrimitive, got Kind=%v", p.Type.Kind)
	}
	if p.Type.Name != "ptr" {
		t.Errorf("Name = %q, want %q", p.Type.Name, "ptr")
	}
}

func TestParseConstAndByRefCombined(t *testing.T) {
	src := `@api kins
@version 1

const MAX_JOINTS = 16

type Pose {
    x: f64
    y: f64
    z: f64
}

@rt_safe "true"
func forward(joints: [MAX_JOINTS]f64, world: Pose byref, fflags: u64, iflags: u64 byref) -> i32
`
	api, errors := Parse("test.gmi", src)
	if len(errors) > 0 {
		t.Fatalf("Parse errors: %v", errors)
	}

	fn := api.Funcs[0]
	if !fn.RTSafe {
		t.Error("RTSafe = false, want true")
	}

	// joints: [MAX_JOINTS]f64 — array with named size, no byref
	p0 := fn.Params[0]
	if p0.Type.Kind != ast.TypeArray {
		t.Fatalf("Params[0].Type.Kind = %v, want TypeArray", p0.Type.Kind)
	}
	if p0.Type.ArrayLen != 16 {
		t.Errorf("Params[0].Type.ArrayLen = %d, want 16", p0.Type.ArrayLen)
	}
	if p0.Type.ArrayLenName != "MAX_JOINTS" {
		t.Errorf("Params[0].Type.ArrayLenName = %q, want %q", p0.Type.ArrayLenName, "MAX_JOINTS")
	}
	if p0.ByRef {
		t.Error("Params[0].ByRef = true, want false")
	}

	// world: Pose byref
	if !fn.Params[1].ByRef {
		t.Error("Params[1].ByRef = false, want true")
	}

	// fflags: u64 — value
	if fn.Params[2].ByRef {
		t.Error("Params[2].ByRef = true, want false")
	}

	// iflags: u64 byref
	if !fn.Params[3].ByRef {
		t.Error("Params[3].ByRef = false, want true")
	}
}

func TestParseCallback(t *testing.T) {
	src := `@api mcode_handler
@version 1

type McodeCall {
    abort_fd: i32
    mcode: i32
    p_number: f64
    q_number: f64
}

callback handler(call: McodeCall, user_data: ptr) -> i32

func register_handler(mcode: i32, fn: handler, user_data: ptr) -> i32
`
	api, errors := Parse("test.gmi", src)
	if len(errors) > 0 {
		t.Fatalf("Parse errors: %v", errors)
	}

	// Callback declaration
	if len(api.Callbacks) != 1 {
		t.Fatalf("len(Callbacks) = %d, want 1", len(api.Callbacks))
	}
	cb := api.Callbacks[0]
	if cb.Name != "handler" {
		t.Errorf("Callback.Name = %q, want %q", cb.Name, "handler")
	}
	if len(cb.Params) != 2 {
		t.Fatalf("len(Callback.Params) = %d, want 2", len(cb.Params))
	}
	// call: McodeCall — should be TypeNamed (it's a struct)
	if cb.Params[0].Type.Kind != ast.TypeNamed {
		t.Errorf("cb.Params[0].Type.Kind = %v, want TypeNamed", cb.Params[0].Type.Kind)
	}
	// user_data: ptr — should be TypePrimitive
	if cb.Params[1].Type.Kind != ast.TypePrimitive || cb.Params[1].Type.Name != "ptr" {
		t.Errorf("cb.Params[1].Type = %v, want TypePrimitive ptr", cb.Params[1].Type)
	}
	// Return type
	if cb.Return == nil || cb.Return.Name != "i32" {
		t.Errorf("cb.Return = %v, want i32", cb.Return)
	}

	// Function using callback type
	fn := api.Funcs[0]
	if fn.Params[1].Type.Kind != ast.TypeCallback {
		t.Errorf("fn.Params[1].Type.Kind = %v, want TypeCallback", fn.Params[1].Type.Kind)
	}
	if fn.Params[1].Type.Name != "handler" {
		t.Errorf("fn.Params[1].Type.Name = %q, want %q", fn.Params[1].Type.Name, "handler")
	}
}

func TestParseImport(t *testing.T) {
	src := `@api interp_ext
@version 1
@rest_export false
@import interp_ctx

callback oword_fn(ctx: interp_ctx ptr, name: string) -> i32

func register_oword(name: string, fn: oword_fn, user: ptr) -> i32
`
	api, errors := Parse("test.gmi", src)
	if len(errors) > 0 {
		t.Fatalf("Parse errors: %v", errors)
	}

	// Import
	if len(api.Imports) != 1 {
		t.Fatalf("len(Imports) = %d, want 1", len(api.Imports))
	}
	if api.Imports[0].Name != "interp_ctx" {
		t.Errorf("Import.Name = %q, want %q", api.Imports[0].Name, "interp_ctx")
	}

	// Callback using imported type
	cb := api.Callbacks[0]
	if cb.Params[0].Type.Kind != ast.TypeImport {
		t.Errorf("cb.Params[0].Type.Kind = %v, want TypeImport", cb.Params[0].Type.Kind)
	}
	if cb.Params[0].Type.Name != "interp_ctx" {
		t.Errorf("cb.Params[0].Type.Name = %q, want %q", cb.Params[0].Type.Name, "interp_ctx")
	}
	if !cb.Params[0].IsPtr {
		t.Error("cb.Params[0].IsPtr = false, want true")
	}
}

func TestParseFieldConstraints(t *testing.T) {
	src := `@api test
@version 1

type ToolEntry {
    toolno:   i32    @min(1) @max(99999)
    diameter: f64    @min(0.5)
    comment:  string @maxlen(255) @regex("^[a-z]+$")
    label:    string @notempty
    note:     string?
}

@method "PUT"
@path "/{toolno}"
func put_tool(toolno: i32 @min(1), entry: ToolEntry) -> i32
`
	api, errors := Parse("test.gmi", src)
	if len(errors) > 0 {
		t.Fatalf("Parse errors: %v", errors)
	}

	fields := api.Types[0].Fields
	if len(fields) != 5 {
		t.Fatalf("len(Fields) = %d, want 5", len(fields))
	}

	// toolno: i32 @min(1) @max(99999)
	if got := fields[0].Constraints; len(got) != 2 {
		t.Fatalf("toolno constraints = %d, want 2", len(got))
	}
	if fields[0].Constraints[0].Kind != ast.ConstraintMin || fields[0].Constraints[0].Num != "1" {
		t.Errorf("toolno[0] = %+v, want min(1)", fields[0].Constraints[0])
	}
	if fields[0].Constraints[1].Kind != ast.ConstraintMax || fields[0].Constraints[1].Num != "99999" {
		t.Errorf("toolno[1] = %+v, want max(99999)", fields[0].Constraints[1])
	}

	// diameter: f64 @min(0.5) — exercises the FLOAT token
	if got := fields[1].Constraints; len(got) != 1 || got[0].Kind != ast.ConstraintMin || got[0].Num != "0.5" {
		t.Errorf("diameter constraints = %+v, want [min(0.5)]", got)
	}

	// comment: string @maxlen(255) @regex("^[a-z]+$")
	cc := fields[2].Constraints
	if len(cc) != 2 {
		t.Fatalf("comment constraints = %d, want 2", len(cc))
	}
	if cc[0].Kind != ast.ConstraintMaxLen || cc[0].Num != "255" {
		t.Errorf("comment[0] = %+v, want maxlen(255)", cc[0])
	}
	if cc[1].Kind != ast.ConstraintRegex || cc[1].Str != "^[a-z]+$" {
		t.Errorf("comment[1] = %+v, want regex(^[a-z]+$)", cc[1])
	}

	// label: string @notempty
	if got := fields[3].Constraints; len(got) != 1 || got[0].Kind != ast.ConstraintNotEmpty {
		t.Errorf("label constraints = %+v, want [notempty]", got)
	}

	// note: string? — no constraints
	if got := fields[4].Constraints; len(got) != 0 {
		t.Errorf("note constraints = %+v, want none", got)
	}

	// Param constraint alongside a param mode-free type.
	fn := api.Funcs[0]
	if got := fn.Params[0].Constraints; len(got) != 1 || got[0].Kind != ast.ConstraintMin || got[0].Num != "1" {
		t.Errorf("put_tool param toolno constraints = %+v, want [min(1)]", got)
	}
	if got := fn.Params[1].Constraints; len(got) != 0 {
		t.Errorf("put_tool param entry constraints = %+v, want none", got)
	}
}

func TestParseConstraintByRefOrder(t *testing.T) {
	// Constraints follow the byref/out/ptr mode keyword.
	src := `@api test
@version 1

func f(x: i32 byref @min(0), y: i32 @max(10)) -> i32
`
	api, errors := Parse("test.gmi", src)
	if len(errors) > 0 {
		t.Fatalf("Parse errors: %v", errors)
	}
	fn := api.Funcs[0]
	if !fn.Params[0].ByRef {
		t.Error("Params[0].ByRef = false, want true")
	}
	if got := fn.Params[0].Constraints; len(got) != 1 || got[0].Kind != ast.ConstraintMin {
		t.Errorf("Params[0].Constraints = %+v, want [min]", got)
	}
	if got := fn.Params[1].Constraints; len(got) != 1 || got[0].Kind != ast.ConstraintMax {
		t.Errorf("Params[1].Constraints = %+v, want [max]", got)
	}
}
