// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package check performs semantic validation of a parsed GMI API, focused on
// the inline @constraints declared on fields and parameters. It runs after
// parsing and before code generation so that mistyped or contradictory
// constraints fail the build instead of emitting dead or broken checks.
package check

import (
	"fmt"
	"regexp"
	"strconv"
	"strings"

	"github.com/sittner/linuxcnc/src/gomc/internal/gmicompile/ast"
)

// Validate returns every constraint error found in the API. It does not stop at
// the first — a compiler should report all mistakes in one run.
func Validate(api *ast.API) []error {
	c := &checker{api: api}
	for _, t := range api.Types {
		for _, f := range t.Fields {
			site := fmt.Sprintf("%s: type %s.%s", f.Pos, t.Name, f.Name)
			c.checkSite(site, f.Type, f.Constraints)
		}
	}
	for _, fn := range api.Funcs {
		for _, p := range fn.Params {
			site := fmt.Sprintf("%s: func %s param %s", p.Pos, fn.Name, p.Name)
			if len(p.Constraints) > 0 {
				// Constraints only make sense on marshaled inputs.
				if p.IsOut {
					c.errf(site, "constraints on an out (output) parameter are meaningless")
				} else if p.IsPtr {
					c.errf(site, "constraints on a ptr (opaque, unmarshaled) parameter are not supported")
				}
			}
			c.checkSite(site, p.Type, p.Constraints)
		}
	}
	return c.errs
}

type checker struct {
	api  *ast.API
	errs []error
}

func (c *checker) errf(site, format string, a ...interface{}) {
	c.errs = append(c.errs, fmt.Errorf("%s: %s", site, fmt.Sprintf(format, a...)))
}

// checkSite validates all constraints attached to one field or parameter.
func (c *checker) checkSite(site string, t ast.TypeRef, cs []ast.Constraint) {
	seen := map[ast.ConstraintKind]bool{}
	var minRaw, maxRaw, minLenRaw, maxLenRaw string

	for _, con := range cs {
		name := ast.ConstraintName(con.Kind)
		if seen[con.Kind] {
			c.errf(site, "duplicate constraint @%s", name)
			continue
		}
		seen[con.Kind] = true

		switch con.Kind {
		case ast.ConstraintMin, ast.ConstraintMax:
			if !isNumeric(t) {
				c.errf(site, "@%s applies to numeric types, not %s", name, t.String())
				break
			}
			if !numericFits(t, con.Num) {
				c.errf(site, "@%s value %s out of range for %s", name, con.Num, t.String())
			}
			if isUnsigned(t) && strings.HasPrefix(con.Num, "-") {
				c.errf(site, "@%s(%s) is negative on unsigned type %s", name, con.Num, t.String())
			}
			if con.Kind == ast.ConstraintMin {
				minRaw = con.Num
			} else {
				maxRaw = con.Num
			}

		case ast.ConstraintMinLen, ast.ConstraintMaxLen:
			if !isCollection(t) {
				c.errf(site, "@%s applies to string/slice/array, not %s", name, t.String())
				break
			}
			n, err := strconv.Atoi(con.Num)
			if err != nil || n < 0 {
				c.errf(site, "@%s must be a non-negative integer, got %s", name, con.Num)
				break
			}
			if t.Kind == ast.TypeArray && n > t.ArrayLen {
				c.errf(site, "@%s(%s) exceeds fixed array length %d", name, con.Num, t.ArrayLen)
			}
			if con.Kind == ast.ConstraintMinLen {
				minLenRaw = con.Num
			} else {
				maxLenRaw = con.Num
			}

		case ast.ConstraintNotEmpty:
			if !isCollection(t) {
				c.errf(site, "@notempty applies to string/slice/array, not %s", t.String())
			}

		case ast.ConstraintRegex:
			if !isString(t) {
				c.errf(site, "@regex applies to string, not %s", t.String())
			}
			// Compiled with Go's regexp precisely because @regex is Go-server-only.
			if _, err := regexp.Compile(con.Str); err != nil {
				c.errf(site, "@regex pattern does not compile: %v", err)
			}

		case ast.ConstraintNotNull:
			if !t.Nullable {
				c.errf(site, "@notnull is redundant on non-nullable type %s", t.String())
			} else if isString(t) {
				c.errf(site, "@notnull on a nullable string is not expressible "+
					"(string? maps to a non-pointer Go string); use @notempty")
			}

		case ast.ConstraintEnumOpen:
			if _, ok := enumFor(c.api, t); !ok {
				c.errf(site, "@enum_open applies to enum types, not %s", t.String())
			}
		}
	}

	// Cross-constraint sanity.
	if minRaw != "" && maxRaw != "" {
		if lo, hi, ok := asFloatPair(minRaw, maxRaw); ok && lo > hi {
			c.errf(site, "@min(%s) > @max(%s)", minRaw, maxRaw)
		}
	}
	if minLenRaw != "" && maxLenRaw != "" {
		if lo, hi, ok := asFloatPair(minLenRaw, maxLenRaw); ok && lo > hi {
			c.errf(site, "@minlen(%s) > @maxlen(%s)", minLenRaw, maxLenRaw)
		}
	}

	// @min/@max on an enum duplicates the automatic membership check.
	if _, isEnum := enumFor(c.api, t); isEnum && (seen[ast.ConstraintMin] || seen[ast.ConstraintMax]) {
		c.errf(site, "@min/@max on enum %s is redundant with automatic enum validation", t.Name)
	}
}

// --- type predicates ---
//
// These read t.Kind/t.Name directly; the Nullable flag is orthogonal, so an
// i32? is still numeric.

func isNumeric(t ast.TypeRef) bool {
	if t.Kind != ast.TypePrimitive {
		return false
	}
	switch t.Name {
	case ast.PrimI8, ast.PrimU8, ast.PrimI16, ast.PrimU16, ast.PrimI32, ast.PrimU32,
		ast.PrimI64, ast.PrimU64, ast.PrimF32, ast.PrimF64:
		return true
	}
	return false
}

func isUnsigned(t ast.TypeRef) bool {
	if t.Kind != ast.TypePrimitive {
		return false
	}
	switch t.Name {
	case ast.PrimU8, ast.PrimU16, ast.PrimU32, ast.PrimU64:
		return true
	}
	return false
}

func isString(t ast.TypeRef) bool {
	return t.Kind == ast.TypePrimitive && t.Name == ast.PrimString
}

func isCollection(t ast.TypeRef) bool {
	return isString(t) || t.Kind == ast.TypeSlice || t.Kind == ast.TypeArray
}

// numericFits reports whether raw parses within the field's width/signedness,
// catching e.g. @min(0.5) on i32 or @max(9999999999) on i32.
func numericFits(t ast.TypeRef, raw string) bool {
	switch t.Name {
	case ast.PrimF32:
		_, err := strconv.ParseFloat(raw, 32)
		return err == nil
	case ast.PrimF64:
		_, err := strconv.ParseFloat(raw, 64)
		return err == nil
	case ast.PrimI8, ast.PrimI16, ast.PrimI32, ast.PrimI64:
		_, err := strconv.ParseInt(raw, 10, intBits(t.Name))
		return err == nil
	case ast.PrimU8, ast.PrimU16, ast.PrimU32, ast.PrimU64:
		_, err := strconv.ParseUint(raw, 10, intBits(t.Name))
		return err == nil
	}
	return false
}

func intBits(name string) int {
	switch name {
	case ast.PrimI8, ast.PrimU8:
		return 8
	case ast.PrimI16, ast.PrimU16:
		return 16
	case ast.PrimI32, ast.PrimU32:
		return 32
	default:
		return 64
	}
}

// asFloatPair parses two numeric literals for ordering comparison.
func asFloatPair(a, b string) (float64, float64, bool) {
	x, err1 := strconv.ParseFloat(a, 64)
	y, err2 := strconv.ParseFloat(b, 64)
	if err1 != nil || err2 != nil {
		return 0, 0, false
	}
	return x, y, true
}

// enumFor returns the enum a named type refers to, if any.
func enumFor(api *ast.API, t ast.TypeRef) (*ast.Enum, bool) {
	if t.Kind != ast.TypeNamed {
		return nil, false
	}
	for i := range api.Enums {
		if api.Enums[i].Name == t.Name {
			return &api.Enums[i], true
		}
	}
	return nil, false
}
