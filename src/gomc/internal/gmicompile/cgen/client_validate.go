// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Client-side collect-all validation of IDL @constraints, generated into the
// TypeScript and Python clients. Unlike the server (fail-fast, authoritative),
// clients gather *all* violations and raise them together so a UI can highlight
// every bad field before a request is sent.
//
// @regex is intentionally NOT checked client-side: regex flavors differ across
// JS/Python/RE2, so the pattern is authoritative on the Go server only (matching
// the design's enforcement model). Everything else — bounds, lengths, presence,
// enum membership — mirrors the server, including struct/slice recursion.
package cgen

import (
	"strconv"
	"strings"

	"github.com/sittner/linuxcnc/src/gomc/internal/gmicompile/ast"
)

// clientLang abstracts the per-language snippets. All block emitters take and
// return tab-indented code (one leading tab per nesting level); the caller
// reindents the whole block to the method-body depth.
type clientLang interface {
	open() string                              // declare the error accumulator
	closeThrow() string                        // raise if any errors collected
	check(cond, msgExpr string) string         // if <cond> then record msgExpr
	guard(expr, body string) string            // if <expr> is present { body }
	loop(coll, iter, body string) string       // for each element (index iter) of coll
	lenExpr(expr string, t ast.TypeRef) string // character/element count
	absent(expr string) string                 // test that a nullable value is missing
	enumMiss(expr string, vals []int) string   // test value is not a valid enum member
	idxPathExpr(baseExpr, iter string) string  // path string expression for coll[iter]
}

// clientValidation returns the full validation block for one client method
// (empty if the function has no constrained inputs). The block declares an
// accumulator, records every violation, and raises them together.
func clientValidation(lang clientLang, fn ast.Func, api *ast.API) string {
	var b strings.Builder
	for _, p := range fn.Params {
		if p.IsOut || p.IsPtr {
			continue
		}
		b.WriteString(clientChecks(lang, api, clientArgName(lang, p.Name), staticPath(p.Name), p.Type, p.Constraints, map[string]bool{}, 0))
	}
	checks := b.String()
	if checks == "" {
		return ""
	}
	return lang.open() + checks + lang.closeThrow()
}

// clientChecks emits collect-all client validation for one value, via the shared
// walk (walkConstraints) with the language-parameterized clientTarget. The
// recursion structure is shared with the Go server validator (D9).
func clientChecks(lang clientLang, api *ast.API, expr string, path valPath, t ast.TypeRef, cs []ast.Constraint, stack map[string]bool, depth int) string {
	return walkConstraints(clientTarget{lang: lang, api: api}, api, expr, path, t, cs, stack, depth)
}

func clientArgName(lang clientLang, name string) string {
	if _, ok := lang.(tsLang); ok {
		return toCamelCaseTS(name)
	}
	return name // Python keeps snake_case
}

func intsJoin(vals []int, sep string) string {
	parts := make([]string, len(vals))
	for i, v := range vals {
		parts[i] = strconv.Itoa(v)
	}
	return strings.Join(parts, sep)
}

// spaceIndent converts tab-indented generated code to space indentation and
// prefixes every non-empty line with baseSpaces spaces — Python cannot mix tabs
// and spaces, so validation spliced into a space-indented method must use spaces.
func spaceIndent(s string, tabWidth, baseSpaces int) string {
	if s == "" {
		return s
	}
	base := strings.Repeat(" ", baseSpaces)
	lines := strings.Split(strings.TrimSuffix(s, "\n"), "\n")
	for i, l := range lines {
		if l == "" {
			continue
		}
		n := 0
		for n < len(l) && l[n] == '\t' {
			n++
		}
		lines[i] = base + strings.Repeat(" ", n*tabWidth) + l[n:]
	}
	return strings.Join(lines, "\n") + "\n"
}

// --- TypeScript ---

type tsLang struct{}

func (tsLang) open() string { return "\tconst _verrs: string[] = [];\n" }
func (tsLang) closeThrow() string {
	return "\tif (_verrs.length > 0) throw new ValidationError(_verrs);\n"
}

func (tsLang) check(cond, msg string) string {
	return "\tif (" + cond + ") {\n\t\t_verrs.push(" + msg + ");\n\t}\n"
}

func (tsLang) guard(expr, body string) string {
	return "\tif (" + expr + " !== undefined && " + expr + " !== null) {\n" + reindent(body, 1) + "\t}\n"
}

func (tsLang) loop(coll, iter, body string) string {
	return "\tfor (let " + iter + " = 0; " + iter + " < " + coll + ".length; " + iter + "++) {\n" + reindent(body, 1) + "\t}\n"
}

func (tsLang) lenExpr(expr string, t ast.TypeRef) string {
	if isStringType(t) {
		return "[..." + expr + "].length" // count code points, matching server RuneLen
	}
	return expr + ".length"
}

func (tsLang) absent(expr string) string {
	return "(" + expr + " === undefined || " + expr + " === null)"
}

func (tsLang) enumMiss(expr string, vals []int) string {
	return "![" + intsJoin(vals, ", ") + "].includes(" + expr + ")"
}

func (tsLang) idxPathExpr(baseExpr, iter string) string {
	return baseExpr + ` + "[" + ` + iter + ` + "]"`
}

// --- Python ---

type pyLang struct{}

func (pyLang) open() string       { return "\t_verrs = []\n" }
func (pyLang) closeThrow() string { return "\tif _verrs:\n\t\traise ValidationError(_verrs)\n" }

func (pyLang) check(cond, msg string) string {
	return "\tif " + cond + ":\n\t\t_verrs.append(" + msg + ")\n"
}

func (pyLang) guard(expr, body string) string {
	return "\tif " + expr + " is not None:\n" + reindent(body, 1)
}

func (pyLang) loop(coll, iter, body string) string {
	return "\tfor " + iter + " in range(len(" + coll + ")):\n" + reindent(body, 1)
}

func (pyLang) lenExpr(expr string, t ast.TypeRef) string {
	return "len(" + expr + ")"
}

func (pyLang) absent(expr string) string {
	return expr + " is None"
}

func (pyLang) enumMiss(expr string, vals []int) string {
	if len(vals) == 1 {
		return expr + " not in (" + intsJoin(vals, ", ") + ",)"
	}
	return expr + " not in (" + intsJoin(vals, ", ") + ")"
}

func (pyLang) idxPathExpr(baseExpr, iter string) string {
	return baseExpr + ` + "[" + str(` + iter + `) + "]"`
}
