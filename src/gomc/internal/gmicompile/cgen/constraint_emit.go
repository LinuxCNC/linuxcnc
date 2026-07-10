// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// constraintEmitter generates Go validation of IDL @constraints for the REST
// dispatch wrappers. The checks run after a request is unmarshaled and before
// the callback is invoked, returning an *apiserver.ValidationError (rendered as
// HTTP 400) on the first violation — fail-fast; the generated clients do
// collect-all for UX.
//
// It is generator-agnostic: it returns code as strings so any generator (the
// cgo dispatch file today) can splice it in.
package cgen

import (
	"fmt"
	"sort"
	"strconv"
	"strings"

	"github.com/sittner/linuxcnc/src/gomc/internal/gmicompile/ast"
)

type constraintEmitter struct {
	api      *ast.API
	regexVar map[string]string // @regex pattern -> package-scope var name
}

func newConstraintEmitter(api *ast.API) *constraintEmitter {
	e := &constraintEmitter{api: api, regexVar: map[string]string{}}
	e.collectRegexVars()
	return e
}

// --- Regex vars (package scope, compiled once) ---

func (e *constraintEmitter) collectRegexVars() {
	e.walkValidation(func(_ ast.TypeRef, cs []ast.Constraint) {
		for _, c := range cs {
			if c.Kind == ast.ConstraintRegex {
				if _, ok := e.regexVar[c.Str]; !ok {
					e.regexVar[c.Str] = fmt.Sprintf("%sRe%d", e.api.Name, len(e.regexVar))
				}
			}
		}
	})
}

// regexVarDecls returns the package-scope compiled-pattern var block (empty if
// no @regex constraints are used).
func (e *constraintEmitter) regexVarDecls() string {
	if len(e.regexVar) == 0 {
		return ""
	}
	pats := make([]string, 0, len(e.regexVar))
	for p := range e.regexVar {
		pats = append(pats, p)
	}
	sort.Slice(pats, func(i, j int) bool { return e.regexVar[pats[i]] < e.regexVar[pats[j]] })

	var b strings.Builder
	b.WriteString("// --- Compiled validation patterns ---\n\n")
	for _, p := range pats {
		fmt.Fprintf(&b, "var %s = apiserver.MustRegex(%s)\n", e.regexVar[p], strconv.Quote(p))
	}
	b.WriteString("\n")
	return b.String()
}

// usesFmt reports whether the generated validation references fmt (enum checks
// and element-indexed paths do), so the caller can arrange the import.
func (e *constraintEmitter) usesFmt() bool {
	uses := false
	e.walkValidation(func(t ast.TypeRef, cs []ast.Constraint) {
		if _, ok := e.enumFor(t); ok && !hasConstraint(cs, ast.ConstraintEnumOpen) {
			uses = true
		}
		if t.Kind == ast.TypeSlice || t.Kind == ast.TypeArray {
			uses = true // element paths use fmt.Sprintf
		}
	})
	return uses
}

// walkValidation visits every (type, constraints) pair reachable from REST input
// parameters, recursing through struct fields and slice/array elements. The seen
// set breaks cycles in recursive struct types.
func (e *constraintEmitter) walkValidation(visit func(ast.TypeRef, []ast.Constraint)) {
	seen := map[string]bool{}
	var walk func(t ast.TypeRef, cs []ast.Constraint)
	walk = func(t ast.TypeRef, cs []ast.Constraint) {
		visit(t, cs)
		switch t.Kind {
		case ast.TypeNamed:
			if st, ok := e.structFor(t); ok && !seen[st.Name] {
				seen[st.Name] = true
				for _, f := range st.Fields {
					walk(f.Type, f.Constraints)
				}
			}
		case ast.TypeSlice, ast.TypeArray:
			walk(*t.Elem, nil)
		}
	}
	for _, fn := range e.api.Funcs {
		if fn.Method == "" {
			continue // not REST-exported
		}
		for _, p := range fn.Params {
			if p.IsOut || p.IsPtr {
				continue
			}
			walk(p.Type, p.Constraints)
		}
	}
}

// --- Per-dispatch validation ---

// validation returns the validation statements for one function's input params
// (empty if there is nothing to check). The generated code assumes the request
// has been unmarshaled into a local `params` struct.
func (e *constraintEmitter) validation(fn ast.Func) string {
	var b strings.Builder
	for _, p := range fn.Params {
		if p.IsOut || p.IsPtr {
			continue // not a marshaled input
		}
		expr := "params." + toPascalCase(p.Name)
		b.WriteString(e.buildChecks(expr, staticPath(p.Name), p.Type, p.Constraints, map[string]bool{}))
	}
	code := b.String()
	if code == "" {
		return ""
	}
	return "\n\t// --- validation (generated from @constraints) ---\n" + code +
		"\t// --- end validation ---\n"
}

// buildChecks returns the validation statements for one value. For nullable
// (pointer) values it emits @notnull first, then guards the remaining checks
// under a non-nil test operating on the pointee.
func (e *constraintEmitter) buildChecks(expr string, path valPath, t ast.TypeRef, cs []ast.Constraint, stack map[string]bool) string {
	if e.goIsPointer(t) {
		var b strings.Builder
		if hasConstraint(cs, ast.ConstraintNotNull) {
			fmt.Fprintf(&b, "\tif %s == nil {\n\t\treturn nil, apiserver.NewValidationError(%s, \"notnull\", %s)\n\t}\n",
				expr, path.expr, path.msg(" must not be null"))
		}
		inner := t
		inner.Nullable = false
		body := e.buildValue("(*"+expr+")", path, inner, dropConstraint(cs, ast.ConstraintNotNull), stack)
		if body != "" {
			fmt.Fprintf(&b, "\tif %s != nil {\n%s\t}\n", expr, indentLines(body))
		}
		return b.String()
	}
	return e.buildValue(expr, path, t, cs, stack)
}

// buildValue emits scalar checks, automatic enum membership, and recursion into
// struct fields / slice-array elements for a non-pointer value.
func (e *constraintEmitter) buildValue(expr string, path valPath, t ast.TypeRef, cs []ast.Constraint, stack map[string]bool) string {
	var b strings.Builder

	for _, c := range cs {
		switch c.Kind {
		case ast.ConstraintMin:
			e.emitFail(&b, fmt.Sprintf("%s < %s", expr, c.Num), path, "min", path.msg(" must be >= "+c.Num))
		case ast.ConstraintMax:
			e.emitFail(&b, fmt.Sprintf("%s > %s", expr, c.Num), path, "max", path.msg(" must be <= "+c.Num))
		case ast.ConstraintMinLen:
			e.emitFail(&b, fmt.Sprintf("%s < %s", lenExpr(expr, t), c.Num), path, "minlen",
				path.msg(fmt.Sprintf(" must have at least %s %s", c.Num, lenUnit(t))))
		case ast.ConstraintMaxLen:
			e.emitFail(&b, fmt.Sprintf("%s > %s", lenExpr(expr, t), c.Num), path, "maxlen",
				path.msg(fmt.Sprintf(" must have at most %s %s", c.Num, lenUnit(t))))
		case ast.ConstraintNotEmpty:
			e.emitFail(&b, fmt.Sprintf("%s == 0", lenExpr(expr, t)), path, "notempty", path.msg(" must not be empty"))
		case ast.ConstraintRegex:
			fmt.Fprintf(&b, "\tif verr := apiserver.ValidateRegex(%s, %s, %s); verr != nil {\n\t\treturn nil, verr\n\t}\n",
				path.expr, e.regexVar[c.Str], expr)
		}
	}

	// Automatic enum membership (opt out with @enum_open).
	if en, ok := e.enumFor(t); ok && !hasConstraint(cs, ast.ConstraintEnumOpen) {
		fmt.Fprintf(&b, "\tswitch %s {\n\tcase %s:\n\tdefault:\n\t\treturn nil, apiserver.NewValidationError(%s, \"enum\", fmt.Sprintf(\"%%s: invalid enum value %%d\", %s, int32(%s)))\n\t}\n",
			expr, e.enumCaseList(en), path.expr, path.expr, expr)
	}

	// Recurse into struct fields.
	if st, ok := e.structFor(t); ok && !stack[st.Name] {
		stack[st.Name] = true
		for _, f := range st.Fields {
			b.WriteString(e.buildChecks(expr+"."+toPascalCase(f.Name), path.field(f.Name), f.Type, f.Constraints, stack))
		}
		delete(stack, st.Name)
	}

	// Recurse into slice/array elements (validate nested struct fields / enums).
	if t.Kind == ast.TypeSlice || t.Kind == ast.TypeArray {
		elem := *t.Elem
		inner := e.buildChecks(expr+"[i]", path.elem("i"), elem, nil, stack)
		if inner != "" {
			fmt.Fprintf(&b, "\tfor i := range %s {\n%s\t}\n", expr, indentLines(inner))
		}
	}

	return b.String()
}

// emitFail writes `if <cond> { return nil, NewValidationError(path, kind, msg) }`.
func (e *constraintEmitter) emitFail(b *strings.Builder, cond string, path valPath, kind, msg string) {
	fmt.Fprintf(b, "\tif %s {\n\t\treturn nil, apiserver.NewValidationError(%s, %s, %s)\n\t}\n",
		cond, path.expr, strconv.Quote(kind), msg)
}

// --- Type lookups and helpers ---

func (e *constraintEmitter) enumFor(t ast.TypeRef) (*ast.Enum, bool) {
	if t.Kind != ast.TypeNamed {
		return nil, false
	}
	for i := range e.api.Enums {
		if e.api.Enums[i].Name == t.Name {
			return &e.api.Enums[i], true
		}
	}
	return nil, false
}

func (e *constraintEmitter) structFor(t ast.TypeRef) (*ast.Type, bool) {
	if t.Kind != ast.TypeNamed {
		return nil, false
	}
	for i := range e.api.Types {
		if e.api.Types[i].Name == t.Name {
			return &e.api.Types[i], true
		}
	}
	return nil, false
}

// enumCaseList returns the Go constant names for the enum's distinct values
// (deduped by value so the switch has no duplicate cases).
func (e *constraintEmitter) enumCaseList(en *ast.Enum) string {
	goName := toPascalCase(en.Name)
	seen := map[int]bool{}
	var names []string
	for _, v := range en.Values {
		if seen[v.Value] {
			continue
		}
		seen[v.Value] = true
		names = append(names, goName+"_"+v.Name)
	}
	return strings.Join(names, ", ")
}

// goIsPointer reports whether t maps to a Go pointer (nil-checkable) — mirrors
// goTypeForDispatch: nullable named types and nullable non-string primitives.
func (e *constraintEmitter) goIsPointer(t ast.TypeRef) bool {
	if !t.Nullable {
		return false
	}
	if t.Kind == ast.TypeNamed {
		return true
	}
	return t.Kind == ast.TypePrimitive && t.Name != ast.PrimString
}

func lenExpr(expr string, t ast.TypeRef) string {
	if isStringType(t) {
		return "apiserver.RuneLen(" + expr + ")"
	}
	return "len(" + expr + ")"
}

func isStringType(t ast.TypeRef) bool {
	return t.Kind == ast.TypePrimitive && t.Name == ast.PrimString
}

func lenUnit(t ast.TypeRef) string {
	if isStringType(t) {
		return "chars"
	}
	return "items"
}

func hasConstraint(cs []ast.Constraint, k ast.ConstraintKind) bool {
	for _, c := range cs {
		if c.Kind == k {
			return true
		}
	}
	return false
}

func dropConstraint(cs []ast.Constraint, k ast.ConstraintKind) []ast.Constraint {
	var out []ast.Constraint
	for _, c := range cs {
		if c.Kind != k {
			out = append(out, c)
		}
	}
	return out
}

func indentLines(s string) string { return reindent(s, 1) }

// reindent prefixes every non-empty line with extraTabs tabs. Used to splice the
// validation block (emitted at one-tab depth) into a more deeply nested scope,
// e.g. a WebSocket command handler closure.
func reindent(s string, extraTabs int) string {
	if s == "" || extraTabs <= 0 {
		return s
	}
	prefix := strings.Repeat("\t", extraTabs)
	lines := strings.Split(strings.TrimSuffix(s, "\n"), "\n")
	for i, l := range lines {
		if l != "" {
			lines[i] = prefix + l
		}
	}
	return strings.Join(lines, "\n") + "\n"
}

// --- valPath: a JSON field path carried as a Go string-valued expression ---
//
// Static paths fold into a single string literal so common-case messages stay
// clean ("entry.diameter must be >= 0"); once an element index enters the path
// it becomes a runtime fmt.Sprintf expression.

type valPath struct {
	expr string // Go expression yielding the path string
	val  string // constant value (valid when lit)
	lit  bool   // expr is a pure string literal (foldable)
}

func staticPath(s string) valPath {
	return valPath{expr: strconv.Quote(s), val: s, lit: true}
}

func (p valPath) field(name string) valPath {
	if p.lit {
		return staticPath(p.val + "." + name)
	}
	return valPath{expr: p.expr + " + " + strconv.Quote("."+name)}
}

func (p valPath) elem(iVar string) valPath {
	return valPath{expr: fmt.Sprintf("fmt.Sprintf(\"%%s[%%d]\", %s, %s)", p.expr, iVar)}
}

// msg returns a Go expression for the path string with suffix appended.
func (p valPath) msg(suffix string) string {
	if p.lit {
		return strconv.Quote(p.val + suffix)
	}
	return p.expr + " + " + strconv.Quote(suffix)
}
