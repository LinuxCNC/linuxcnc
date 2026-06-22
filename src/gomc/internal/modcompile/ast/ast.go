// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package ast defines the intermediate representation shared between
// modcompile frontends (.comp, future .st) and backends (C codegen).
//
// The top-level unit is Package, which maps to a single .so output file.
// A Package always contains exactly one Component (the HAL-facing interface)
// and may additionally contain user-defined types, function blocks, and
// pure functions (used by ST modules).
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
// Package — the compilation unit (one .so)
// ---------------------------------------------------------------------------

// Package is the top-level compilation unit produced by a frontend.
type Package struct {
	Component Component

	// Future ST support: user-defined types, sub-modules, helpers.
	Types     []TypeDef
	Blocks    []FunctionBlock
	Functions []PureFunction
}

// ---------------------------------------------------------------------------
// Component — the HAL-facing interface
// ---------------------------------------------------------------------------

// Component describes a HAL component: its pins, parameters, exported
// functions, per-instance variables, options, and (for .comp) verbatim
// user C code.
type Component struct {
	Name string
	Pos  Pos // position of the "component" keyword

	// Documentation fields.
	Summary     string // Short one-line description from component declaration
	Description string // Full description from "description" block
	License     string
	Author      string
	SeeAlso     string
	Notes       string
	Examples    string

	// Declarations.
	Pins      []Pin
	Params    []Param
	Functions []Function
	Variables []Variable
	Options   map[string]string
	Modparams []Modparam
	Includes  []string

	// GMI API bindings.
	// GMIProvide lists API names this component provides (server side).
	// GMIConsume lists APIs this component consumes (client side).
	GMIProvide []string
	GMIConsume []GMIConsumeEntry

	// VerbatimC holds the raw C code from after the ";;" separator
	// in .comp files.  Empty for ST modules.
	VerbatimC string
}

// GMIConsumeEntry describes a consumed API with an optional default provider instance.
type GMIConsumeEntry struct {
	API  string // API name (e.g. "mot")
	From string // Default provider instance name (e.g. "motmod"); empty means use API name
}

// ---------------------------------------------------------------------------
// HAL types and directions
// ---------------------------------------------------------------------------

// HALType represents a HAL data type.
type HALType int

const (
	HALBit HALType = iota
	HALFloat
	HALS32
	HALU32
	HALPort
)

func (t HALType) String() string {
	switch t {
	case HALBit:
		return "bit"
	case HALFloat:
		return "float"
	case HALS32:
		return "s32"
	case HALU32:
		return "u32"
	case HALPort:
		return "port"
	default:
		return "unknown"
	}
}

// CType returns the C typedef name used in generated code.
func (t HALType) CType() string {
	switch t {
	case HALBit:
		return "gomc_hal_bit_t"
	case HALFloat:
		return "gomc_hal_float_t"
	case HALS32:
		return "gomc_hal_s32_t"
	case HALU32:
		return "gomc_hal_u32_t"
	case HALPort:
		return "gomc_hal_port_t"
	default:
		return "void"
	}
}

// PinDir represents a pin direction.
type PinDir int

const (
	PinIn PinDir = iota
	PinOut
	PinIO
)

func (d PinDir) String() string {
	switch d {
	case PinIn:
		return "in"
	case PinOut:
		return "out"
	case PinIO:
		return "io"
	default:
		return "unknown"
	}
}

// ParamDir represents a parameter direction.
type ParamDir int

const (
	ParamR ParamDir = iota
	ParamRW
)

func (d ParamDir) String() string {
	switch d {
	case ParamR:
		return "r"
	case ParamRW:
		return "rw"
	default:
		return "unknown"
	}
}

// ---------------------------------------------------------------------------
// Pin, Param, Function, Variable, Modparam
// ---------------------------------------------------------------------------

// Pin represents a HAL pin declaration.
type Pin struct {
	Pos  Pos
	Name string // HAL-style name (may contain #, -, .)
	Type HALType
	Dir  PinDir

	// Array support. ArraySize > 0 means this is an array pin.
	// ArrayPersonality, if non-empty, is a C expression giving the
	// actual count (ArraySize is then the maximum).
	ArraySize        int
	ArrayPersonality string

	// Default is the default value expression (empty if none).
	Default string

	// Personality is the "if <expr>" condition (empty if unconditional).
	Personality string

	Doc string
}

// Param represents a HAL parameter declaration.
type Param struct {
	Pos  Pos
	Name string
	Type HALType
	Dir  ParamDir

	ArraySize        int
	ArrayPersonality string
	Default          string
	Personality      string
	Doc              string
}

// Function represents an exported HAL function.
type Function struct {
	Pos  Pos
	Name string
	FP   bool // true if function uses floating point (default true)
	Doc  string
}

// Variable represents a per-instance C variable.
type Variable struct {
	Pos     Pos
	CType   string // C type name (e.g. "double", "unsigned", "hal_bit_t")
	Name    string // variable name (may include leading * for pointers)
	Array   int    // element count; 0 if not an array
	Default string // initial value expression (empty if none)
}

// Modparam represents a module parameter (becomes argv parsing in cmod).
type Modparam struct {
	Pos     Pos
	Type    string // "int" or "dummy"
	Name    string
	Default string
	Doc     string
}

// ---------------------------------------------------------------------------
// Future ST support — placeholder types
// ---------------------------------------------------------------------------

// TypeDefKind classifies user-defined types.
type TypeDefKind int

const (
	TypeStruct TypeDefKind = iota
	TypeEnum
	TypeAlias
	TypeArray
)

// TypeDef represents a user-defined type (ST).
type TypeDef struct {
	Pos  Pos
	Name string
	Kind TypeDefKind
}

// FunctionBlock represents a reusable sub-module (ST FUNCTION_BLOCK).
type FunctionBlock struct {
	Pos  Pos
	Name string
}

// PureFunction represents a stateless helper function (ST FUNCTION).
type PureFunction struct {
	Pos  Pos
	Name string
}
