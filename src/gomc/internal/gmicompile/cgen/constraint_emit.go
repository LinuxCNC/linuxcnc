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

// newConstraintEmitter builds an emitter. The compiled @regex vars and the
// validate<Api><Fn> functions are now emitted once (into _cgo.go) and shared by
// both the REST dispatch and the WS handler (D8), so there is no per-file prefix
// scheme any more.
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
					e.regexVar[c.Str] = fmt.Sprintf("%s%d", e.api.Name, len(e.regexVar))
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

// walkValidation visits every (type, constraints) pair reachable from an input
// parameter of any dispatched function, recursing through struct fields and
// slice/array elements. The seen set breaks cycles in recursive struct types.
// It must cover EVERY function that allValidationFuncs emits a validator for
// (including non-REST ones like `close`), so collectRegexVars/usesFmt see the
// same set — otherwise a @regex on a non-REST function would reference an
// uncollected (empty) var and emit invalid Go.
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
		for _, p := range validatedParams(fn) {
			walk(p.Type, p.Constraints)
		}
	}
}

// --- Per-dispatch validation ---

// validateFuncName is the package-level validation function generated for fn.
// Both the REST dispatch and the WS command handler call it, so the checks (and
// the compiled @regex vars) live once instead of being inlined into each.
func (e *constraintEmitter) validateFuncName(fn ast.Func) string {
	return "validate" + toPascalCase(e.api.Name) + toPascalCase(fn.Name)
}

// validatedParams returns fn's marshaled input params (skipping out/ptr params,
// which are never validated).
func validatedParams(fn ast.Func) []ast.Param {
	var out []ast.Param
	for _, p := range fn.Params {
		if p.IsOut || p.IsPtr {
			continue
		}
		out = append(out, p)
	}
	return out
}

// checksFor builds the validation body for fn using base expressions produced by
// baseExpr(param) — the arg name for the standalone function, or params.Field at
// a call site. Empty when nothing needs checking.
func (e *constraintEmitter) checksFor(fn ast.Func, baseExpr func(ast.Param) string) string {
	var b strings.Builder
	for _, p := range validatedParams(fn) {
		b.WriteString(e.buildChecks(baseExpr(p), staticPath(p.Name), p.Type, p.Constraints, map[string]bool{}, 0))
	}
	return b.String()
}

// needsValidation reports whether fn has any @constraint checks to emit.
func (e *constraintEmitter) needsValidation(fn ast.Func) bool {
	return e.checksFor(fn, func(p ast.Param) string { return goArgName(p.Name) }) != ""
}

// validationFunc emits the standalone `func validate<Api><Fn>(args...)
// *apiserver.ValidationError { … return nil }` for fn (empty if nothing to
// validate). The params are passed by value as positional args, so no shared
// request struct type is needed and the REST and WS param structs (which have
// identical Go types for validated params) can both call it.
func (e *constraintEmitter) validationFunc(fn ast.Func) string {
	body := e.checksFor(fn, func(p ast.Param) string { return goArgName(p.Name) })
	if body == "" {
		return ""
	}
	var args []string
	for _, p := range validatedParams(fn) {
		args = append(args, goArgName(p.Name)+" "+goTypeForDispatch(p.Type))
	}
	var b strings.Builder
	fmt.Fprintf(&b, "// %s validates @constraints for the %s params (REST + WS).\n",
		e.validateFuncName(fn), fn.Name)
	fmt.Fprintf(&b, "func %s(%s) *apiserver.ValidationError {\n", e.validateFuncName(fn), strings.Join(args, ", "))
	b.WriteString(body)
	b.WriteString("\treturn nil\n}\n\n")
	return b.String()
}

// allValidationFuncs emits every function's standalone validator (in declaration
// order), for the shared _cgo.go file that both transports link against. It does
// NOT filter by @method: the cgo dispatch validates every dispatched function's
// params (including non-REST ones like `close`), and validationFunc emits
// nothing for a function with no @constraints, so this matches the call sites'
// needsValidation guard exactly.
func (e *constraintEmitter) allValidationFuncs() string {
	var b strings.Builder
	for _, fn := range e.api.Funcs {
		b.WriteString(e.validationFunc(fn))
	}
	return b.String()
}

// validateCallExpr returns `validate<Api><Fn>(structVar.Field, …)` — the call a
// dispatch/handler makes, passing its unmarshaled struct's fields.
func (e *constraintEmitter) validateCallExpr(fn ast.Func, structVar string) string {
	var args []string
	for _, p := range validatedParams(fn) {
		args = append(args, structVar+"."+toPascalCase(p.Name))
	}
	return e.validateFuncName(fn) + "(" + strings.Join(args, ", ") + ")"
}

// goArgName returns a valid Go identifier for a function parameter derived from
// the IDL param name (already a valid ident for the current APIs; the shared
// keyword guard future-proofs it).
func goArgName(name string) string { return escapeGoKeyword(name) }

// buildChecks returns the Go-server validation statements for one value, via the
// shared walk (walkConstraints) with the fail-fast serverTarget. The recursion
// structure is shared with the Python/TS client validators (D9); only the leaf
// emission differs (serverTarget vs clientTarget).
func (e *constraintEmitter) buildChecks(expr string, path valPath, t ast.TypeRef, cs []ast.Constraint, stack map[string]bool, depth int) string {
	return walkConstraints(serverTarget{e}, e.api, expr, path, t, cs, stack, depth)
}

// emitFail writes `if <cond> { return NewValidationError(path, kind, msg) }`.
func (e *constraintEmitter) emitFail(b *strings.Builder, cond string, path valPath, kind, msg string) {
	fmt.Fprintf(b, "\tif %s {\n\t\treturn apiserver.NewValidationError(%s, %s, %s)\n\t}\n",
		cond, path.expr, strconv.Quote(kind), msg)
}

// --- Type lookups and helpers ---

func (e *constraintEmitter) enumFor(t ast.TypeRef) (*ast.Enum, bool) {
	return e.api.EnumByName(t)
}

func (e *constraintEmitter) structFor(t ast.TypeRef) (*ast.Type, bool) {
	return e.api.StructByName(t)
}

// enumCaseList returns the Go constant names for the enum's distinct values
// (deduped by value so the switch has no duplicate cases).
func (e *constraintEmitter) enumCaseList(en *ast.Enum) string {
	goName := toPascalCase(en.Name)
	var names []string
	for _, v := range en.DistinctMembers() {
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
