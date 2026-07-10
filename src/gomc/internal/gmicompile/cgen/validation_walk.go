// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package cgen

import (
	"fmt"
	"strings"

	"github.com/sittner/linuxcnc/src/gomc/internal/gmicompile/ast"
)

// validationTarget abstracts the emission differences between the fail-fast Go
// server validator and the collect-all Python/TS client validators, so a single
// walk (walkConstraints) generates both. The recursion structure — scalar
// checks, enum membership, struct-field and slice/array recursion, per-depth
// index naming, nullable handling — lives in one place; only the leaf emission
// and the language conventions (field access, deref, path construction) differ.
type validationTarget interface {
	// check emits a conditional failure for cond, describing the violation with
	// path/kind and the message suffix.
	check(cond string, path valPath, kind, msgSuffix string) string
	// lenExpr returns the character/element-count expression for a string or
	// collection value.
	lenExpr(expr string, t ast.TypeRef) string
	// notEmptyCond returns the "is empty" boolean for @notempty.
	notEmptyCond(expr string, t ast.TypeRef) string
	// enumCheck emits the automatic enum-membership check.
	enumCheck(expr string, path valPath, en *ast.Enum) string
	// regexCheck emits the @regex check ("" on targets that skip it — clients).
	regexCheck(expr string, path valPath, pattern string) string
	// fieldExpr / elemExpr return the child value expression for a struct field
	// or a slice/array element (index idx).
	fieldExpr(parent, fieldName string) string
	elemExpr(parent, idx string) string
	// elemPath returns the runtime error path for element idx.
	elemPath(parentPath valPath, idx string) valPath
	// loop wraps a per-element body in a range loop (index idx over coll).
	loop(idx, coll, body string) string
	// isNullable reports whether t is nil/undefined-checkable on this target.
	isNullable(t ast.TypeRef) bool
	// notNullCheck emits the @notnull presence check.
	notNullCheck(expr string, path valPath) string
	// guardedExpr adapts expr for use inside the presence guard (the server
	// dereferences the pointer; clients use the value as-is).
	guardedExpr(expr string) string
	// nullableWrap wraps body so it runs only when expr is present.
	nullableWrap(expr, body string) string
}

// walkConstraints emits validation for one value, handling nullable presence.
// depth names the range index per loop-nesting level (i0, i1, …) so nested
// constrained collections do not shadow one another; struct/pointer recursion
// keeps the same depth, slice/array element recursion increments it.
func walkConstraints(tgt validationTarget, api *ast.API, expr string, path valPath, t ast.TypeRef, cs []ast.Constraint, stack map[string]bool, depth int) string {
	if tgt.isNullable(t) {
		var b strings.Builder
		if hasConstraint(cs, ast.ConstraintNotNull) {
			b.WriteString(tgt.notNullCheck(expr, path))
		}
		inner := t
		inner.Nullable = false
		body := walkValue(tgt, api, tgt.guardedExpr(expr), path, inner, dropConstraint(cs, ast.ConstraintNotNull), stack, depth)
		if body != "" {
			b.WriteString(tgt.nullableWrap(expr, body))
		}
		return b.String()
	}
	return walkValue(tgt, api, expr, path, t, cs, stack, depth)
}

// walkValue emits scalar checks, automatic enum membership, and recursion into
// struct fields / slice-array elements for a non-nullable value.
func walkValue(tgt validationTarget, api *ast.API, expr string, path valPath, t ast.TypeRef, cs []ast.Constraint, stack map[string]bool, depth int) string {
	var b strings.Builder

	for _, c := range cs {
		switch c.Kind {
		case ast.ConstraintMin:
			b.WriteString(tgt.check(expr+" < "+c.Num, path, "min", " must be >= "+c.Num))
		case ast.ConstraintMax:
			b.WriteString(tgt.check(expr+" > "+c.Num, path, "max", " must be <= "+c.Num))
		case ast.ConstraintMinLen:
			b.WriteString(tgt.check(tgt.lenExpr(expr, t)+" < "+c.Num, path, "minlen",
				fmt.Sprintf(" must have at least %s %s", c.Num, lenUnit(t))))
		case ast.ConstraintMaxLen:
			b.WriteString(tgt.check(tgt.lenExpr(expr, t)+" > "+c.Num, path, "maxlen",
				fmt.Sprintf(" must have at most %s %s", c.Num, lenUnit(t))))
		case ast.ConstraintNotEmpty:
			b.WriteString(tgt.check(tgt.notEmptyCond(expr, t), path, "notempty", " must not be empty"))
		case ast.ConstraintRegex:
			b.WriteString(tgt.regexCheck(expr, path, c.Str))
		}
	}

	// Automatic enum membership (opt out with @enum_open).
	if en, ok := api.EnumByName(t); ok && !hasConstraint(cs, ast.ConstraintEnumOpen) {
		b.WriteString(tgt.enumCheck(expr, path, en))
	}

	// Recurse into struct fields.
	if st, ok := api.StructByName(t); ok && !stack[st.Name] {
		stack[st.Name] = true
		for _, f := range st.Fields {
			b.WriteString(walkConstraints(tgt, api, tgt.fieldExpr(expr, f.Name), path.field(f.Name), f.Type, f.Constraints, stack, depth))
		}
		delete(stack, st.Name)
	}

	// Recurse into slice/array elements with a per-depth index.
	if t.Kind == ast.TypeSlice || t.Kind == ast.TypeArray {
		idx := fmt.Sprintf("i%d", depth)
		elem := *t.Elem
		inner := walkConstraints(tgt, api, tgt.elemExpr(expr, idx), tgt.elemPath(path, idx), elem, nil, stack, depth+1)
		if inner != "" {
			b.WriteString(tgt.loop(idx, expr, inner))
		}
	}

	return b.String()
}

// --- Go server target (fail-fast, authoritative) ---

type serverTarget struct{ e *constraintEmitter }

func (s serverTarget) check(cond string, path valPath, kind, msgSuffix string) string {
	var b strings.Builder
	s.e.emitFail(&b, cond, path, kind, path.msg(msgSuffix))
	return b.String()
}
func (serverTarget) lenExpr(expr string, t ast.TypeRef) string { return lenExpr(expr, t) }
func (serverTarget) notEmptyCond(expr string, t ast.TypeRef) string {
	return lenExpr(expr, t) + " == 0"
}
func (s serverTarget) enumCheck(expr string, path valPath, en *ast.Enum) string {
	return fmt.Sprintf("\tswitch %s {\n\tcase %s:\n\tdefault:\n\t\treturn apiserver.NewValidationError(%s, \"enum\", fmt.Sprintf(\"%%s: invalid enum value %%d\", %s, int32(%s)))\n\t}\n",
		expr, s.e.enumCaseList(en), path.expr, path.expr, expr)
}
func (s serverTarget) regexCheck(expr string, path valPath, pattern string) string {
	return fmt.Sprintf("\tif verr := apiserver.ValidateRegex(%s, %s, %s); verr != nil {\n\t\treturn verr\n\t}\n",
		path.expr, s.e.regexVar[pattern], expr)
}
func (serverTarget) fieldExpr(parent, fieldName string) string {
	return parent + "." + toPascalCase(fieldName)
}
func (serverTarget) elemExpr(parent, idx string) string              { return parent + "[" + idx + "]" }
func (serverTarget) elemPath(parentPath valPath, idx string) valPath { return parentPath.elem(idx) }
func (serverTarget) loop(idx, coll, body string) string {
	return fmt.Sprintf("\tfor %s := range %s {\n%s\t}\n", idx, coll, indentLines(body))
}
func (s serverTarget) isNullable(t ast.TypeRef) bool { return s.e.goIsPointer(t) }
func (serverTarget) notNullCheck(expr string, path valPath) string {
	return fmt.Sprintf("\tif %s == nil {\n\t\treturn apiserver.NewValidationError(%s, \"notnull\", %s)\n\t}\n",
		expr, path.expr, path.msg(" must not be null"))
}
func (serverTarget) guardedExpr(expr string) string { return "(*" + expr + ")" }
func (serverTarget) nullableWrap(expr, body string) string {
	return fmt.Sprintf("\tif %s != nil {\n%s\t}\n", expr, indentLines(body))
}

// --- Python/TS client target (collect-all) ---

type clientTarget struct {
	lang clientLang
	api  *ast.API
}

func (c clientTarget) check(cond string, path valPath, kind, msgSuffix string) string {
	return c.lang.check(cond, path.msg(msgSuffix))
}
func (c clientTarget) lenExpr(expr string, t ast.TypeRef) string { return c.lang.lenExpr(expr, t) }
func (c clientTarget) notEmptyCond(expr string, t ast.TypeRef) string {
	return c.lang.lenExpr(expr, t) + " < 1"
}
func (c clientTarget) enumCheck(expr string, path valPath, en *ast.Enum) string {
	var vals []int
	for _, v := range en.DistinctMembers() {
		vals = append(vals, v.Value)
	}
	return c.lang.check(c.lang.enumMiss(expr, vals), path.msg(": invalid enum value"))
}
func (clientTarget) regexCheck(string, valPath, string) string { return "" } // server-only
func (clientTarget) fieldExpr(parent, fieldName string) string { return parent + "." + fieldName }
func (clientTarget) elemExpr(parent, idx string) string        { return parent + "[" + idx + "]" }
func (c clientTarget) elemPath(parentPath valPath, idx string) valPath {
	return valPath{expr: c.lang.idxPathExpr(parentPath.expr, idx)}
}
func (c clientTarget) loop(idx, coll, body string) string { return c.lang.loop(coll, idx, body) }
func (clientTarget) isNullable(t ast.TypeRef) bool        { return t.Nullable }
func (c clientTarget) notNullCheck(expr string, path valPath) string {
	return c.lang.check(c.lang.absent(expr), path.msg(" must not be null"))
}
func (clientTarget) guardedExpr(expr string) string          { return expr }
func (c clientTarget) nullableWrap(expr, body string) string { return c.lang.guard(expr, body) }
