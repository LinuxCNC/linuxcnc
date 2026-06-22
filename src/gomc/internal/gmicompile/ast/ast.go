// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package ast defines the AST types for GMI (GOMC Interface Definition) files.
//
// A GMI file describes an API with types, enums, and functions that can be
// used for inter-module communication in LinuxCNC. The AST is designed to
// support code generation for:
//   - C server callbacks and types
//   - C REST client (cJSON/libcurl)
//   - Go server handlers and HTTP routing
//   - Go REST client
//   - Python REST client
package ast

import "fmt"

// ---------------------------------------------------------------------------
// Source positions
// ---------------------------------------------------------------------------

// Pos tracks a location in the source for error reporting.
type Pos struct {
	File string
	Line int
	Col  int
}

func (p Pos) String() string {
	if p.File == "" {
		return fmt.Sprintf("%d:%d", p.Line, p.Col)
	}
	return fmt.Sprintf("%s:%d:%d", p.File, p.Line, p.Col)
}

// ---------------------------------------------------------------------------
// API — the top-level compilation unit
// ---------------------------------------------------------------------------

// API represents a complete GMI interface definition.
type API struct {
	Name       string // API name from @api directive
	Version    int    // Version from @version directive
	Prefix     string // REST path prefix from @prefix directive
	RestExport bool   // Whether to expose via REST from @rest_export directive
	Authors    []string // Authors from @author directives
	License    string   // License from @license directive
	Pos        Pos      // Position of @api directive

	Consts        []Const
	Enums         []Enum
	Types         []Type
	Callbacks     []Callback
	Imports       []Import
	Funcs         []Func
	StreamServers []StreamServer
}

// ---------------------------------------------------------------------------
// Import — imported API reference
// ---------------------------------------------------------------------------

// Import represents a reference to another GMI API (@import directive).
// The imported API's callbacks struct type becomes available as a type name.
type Import struct {
	Name string // API name (e.g. "interp_ctx")
	Pos  Pos
}

// ---------------------------------------------------------------------------
// Const — named integer constant
// ---------------------------------------------------------------------------

// Const represents a named integer constant (e.g. const MAX_JOINTS = 16).
type Const struct {
	Name  string
	Value int
	Pos   Pos
}

// ---------------------------------------------------------------------------
// Enum — enumeration type
// ---------------------------------------------------------------------------

// Enum represents an enumeration definition.
type Enum struct {
	Name   string
	Pos    Pos
	Values []EnumValue
}

// EnumValue represents a single enum variant.
type EnumValue struct {
	Name  string
	Value int
	Pos   Pos
}

// ---------------------------------------------------------------------------
// Type — struct type
// ---------------------------------------------------------------------------

// Type represents a struct/record type definition.
type Type struct {
	Name   string
	Pos    Pos
	Fields []Field
}

// Field represents a single field in a type.
type Field struct {
	Name string
	Type TypeRef
	Pos  Pos
}

// ---------------------------------------------------------------------------
// TypeRef — type reference (primitive, named, array, slice)
// ---------------------------------------------------------------------------

// TypeKind distinguishes different kinds of type references.
type TypeKind int

const (
	TypePrimitive TypeKind = iota // bool, i32, u32, i64, u64, f64, ptr, string
	TypeNamed                     // user-defined type or enum
	TypeArray                     // [N]T fixed-size array (N can be const name or integer)
	TypeSlice                     // []T dynamic slice
	TypeCallback                  // callback type reference (named callback declaration)
	TypeImport                    // imported API type reference (@import)
)

// TypeRef represents a reference to a type.
type TypeRef struct {
	Kind         TypeKind
	Name         string   // for Primitive: "bool", "i32", etc.; for Named: type name
	Elem         *TypeRef // for Array/Slice: element type
	ArrayLen     int      // for Array: resolved integer length
	ArrayLenName string   // for Array: const name if used (e.g. "MAX_JOINTS")
	Nullable     bool     // T? syntax
}

func (t TypeRef) String() string {
	base := ""
	switch t.Kind {
	case TypePrimitive, TypeNamed, TypeCallback, TypeImport:
		base = t.Name
	case TypeArray:
		if t.ArrayLenName != "" {
			base = fmt.Sprintf("[%s]%s", t.ArrayLenName, t.Elem.String())
		} else {
			base = fmt.Sprintf("[%d]%s", t.ArrayLen, t.Elem.String())
		}
	case TypeSlice:
		base = fmt.Sprintf("[]%s", t.Elem.String())
	}
	if t.Nullable {
		return base + "?"
	}
	return base
}

// IsPrimitive returns true if this is a primitive type.
func (t TypeRef) IsPrimitive() bool {
	return t.Kind == TypePrimitive
}

// Primitive type names.
const (
	PrimBool   = "bool"
	PrimI8     = "i8"
	PrimU8     = "u8"
	PrimI16    = "i16"
	PrimU16    = "u16"
	PrimI32    = "i32"
	PrimU32    = "u32"
	PrimI64    = "i64"
	PrimU64    = "u64"
	PrimF32    = "f32"
	PrimF64    = "f64"
	PrimString = "string"
	PrimPtr    = "ptr"
)

// Primitives is the set of valid primitive type names.
var Primitives = map[string]bool{
	PrimBool:   true,
	PrimI8:     true,
	PrimU8:     true,
	PrimI16:    true,
	PrimU16:    true,
	PrimI32:    true,
	PrimU32:    true,
	PrimI64:    true,
	PrimU64:    true,
	PrimF32:    true,
	PrimF64:    true,
	PrimString: true,
	PrimPtr:    true,
}

// ---------------------------------------------------------------------------
// Callback — named function-pointer type
// ---------------------------------------------------------------------------

// Callback represents a named function-pointer type declaration.
// In C this generates a typedef: typedef rettype (*api_name_cb)(...);
// Used when a function parameter needs to pass a function pointer.
type Callback struct {
	Name   string
	Pos    Pos
	Params []Param
	Return *TypeRef // nil if no return type
}

// ---------------------------------------------------------------------------
// Func — function definition
// ---------------------------------------------------------------------------

// Func represents a function/endpoint definition.
type Func struct {
	Name   string
	Pos    Pos
	Params []Param
	Return *TypeRef // nil if no return type

	// Metadata from annotations.
	Method           string // GET, POST, PUT, DELETE (empty if not REST)
	Path             string // REST endpoint path
	RTSafe           bool   // true if callable from RT context
	Doc              string // documentation string
	Watch            bool   // true if this function supports WebSocket watch subscriptions
	WatchDefaultRate string // default push rate (e.g. "50ms", "1s")
	WatchFactory     bool   // true if watch uses per-connection factory (params sent as subscribe args)
	Publish          bool   // true if this is a publish (event producer) function
	PublishRingSize  int    // ring buffer slot count (default 64)
	WatchSource      string // name of @publish function that feeds this watch
	ReturnsValue     bool   // true if i32 return is a value, not an error code (@returns_value)
}

// Param represents a function parameter.
type Param struct {
	Name  string
	Type  TypeRef
	ByRef bool // passed as mutable pointer (byref keyword) — in/out
	IsOut bool // output-only parameter (out keyword) — caller receives value
	IsPtr bool // passed as opaque typed pointer (ptr keyword) — no marshaling
	Pos   Pos
}

// ---------------------------------------------------------------------------
// StreamServer — bidirectional streaming endpoint
// ---------------------------------------------------------------------------

// StreamServer represents a per-connection streaming interface.
// The generated bridge spawns one goroutine per WebSocket connection.
// The cmod implements the callback functions (new_conn, closed_conn,
// poll_transmit / data_received).
type StreamServer struct {
	Name  string // stream server name (e.g. "hal_sampler")
	Pos   Pos
	Funcs []StreamFunc // callback functions in this stream server
}

// StreamFunc represents a single function in a stream_server block.
type StreamFunc struct {
	Name   string
	Pos    Pos
	Params []Param
	Return *TypeRef // nil if void
}
