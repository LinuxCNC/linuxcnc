# Field Validation Design

This document describes declarative input validation for GMI interfaces: field-
and parameter-level constraints (`@min`, `@max`, `@minlen`, `@maxlen`,
`@notempty`, `@notnull`, `@regex`) declared once in the `.gmi` IDL and enforced
by generated code.

## Status (July 2026)

| Slice | Scope | Status |
|-------|-------|--------|
| 1 | AST + scanner + parser + `check.Validate` | ✅ Complete |
| 2 | Runtime `validate.go` + `writeDispatchError` branch | ✅ Complete |
| 3 | REST dispatch emit (`constraintEmitter`, regex vars, enum auto) | ✅ Complete |
| 4 | WebSocket command-handler validation | ✅ Complete |
| 5 | Client-side collect-all (TypeScript + Python) | ✅ Complete |

The feature is complete. Both server-side JSON input paths validate every input
(fail-fast, authoritative) before invoking the callback, and both generated
clients pre-validate (collect-all) before sending:

- **REST dispatch** — the cgo `_cgo.go` file (emitted by `GenerateDispatchC`).
- **WebSocket command handlers** — `TooltableCommands`-style handlers (emitted by
  `GenerateServerGoExtra` into `_bridge.go`) that some callers register directly
  (e.g. `task/watches.go` → `EmccmdCommands(m)`). This file declares its own
  compiled `@regex` vars with a distinct `CmdRe` prefix (vs `_cgo.go`'s `Re`), so
  each generated file in the shared package is self-contained rather than
  referencing another file's symbols. Compiling a pattern twice is free —
  `MustRegex` runs once at package init.
- **TypeScript & Python clients** — a shared `clientValidation` emitter (with a
  per-language adapter) gathers *all* violations and raises a `ValidationError`
  before the request is sent, so a UI can highlight every bad field. Clients
  skip `@regex` (server-authoritative, avoids JS/Python/RE2 flavor mismatch);
  everything else mirrors the server, including struct/slice recursion and enum
  membership.

## Motivation

Input validation on the REST boundary is currently ad hoc: each callback that
cares re-checks its own arguments, and most don't. A bad value (out-of-range
tool number, empty string, unknown enum) reaches the callback — or the C module
behind it — before anything rejects it.

GMI already owns the compiler and fans one IDL definition out to five targets
(C server, Go server, C/Python/TypeScript clients). Constraints belong in the
IDL for the same reason types do: **declare once, enforce everywhere.** This is
the approach taken by protobuf-validate, OpenAPI/JSON-Schema, and similar IDLs.

## Scope

Covered — stateless, per-field constraints:

| Constraint            | Applies to               | Meaning                     |
|-----------------------|--------------------------|-----------------------------|
| `@min(n)` / `@max(n)` | `i*`, `u*`, `f*`         | numeric bound (inclusive)   |
| `@minlen(n)` / `@maxlen(n)` | `string`, `[]T`, `[N]T` | length bound (see note) |
| `@notempty`           | `string`, `[]T`, `[N]T`  | length > 0                  |
| `@notnull`            | `T?` (nullable) only     | value must be present       |
| `@regex("…")`         | `string`                 | full-match pattern          |
| *(automatic)*         | any `enum` type          | value must be a declared variant |

**Not covered** (stays in the callback): cross-field rules
(`frontangle < backangle`), uniqueness, existence ("tool must already exist"),
and any check needing state. Declarative validation handles the ~80 % of dumb
input errors; nobody should assume the IDL now guarantees *semantic* validity.

## IDL syntax

Constraints are inline annotations following a field or parameter type. On a
parameter they come *after* the `byref`/`out`/`ptr` mode keyword — the order is
`name: type [mode] [@constraints…]`:

```
type ToolEntry {
    toolno:      i32    @min(1) @max(99999)
    diameter:    f64    @min(0)
    orientation: i32    @min(0) @max(8)
    comment:     string @maxlen(255) @regex("^[\\x20-\\x7e]*$")
}

@method PUT
@path /{toolno}
func put_tool(toolno: i32 @min(1), entry: ToolEntry) -> PutToolResult
```

Enum-typed fields need no annotation — membership is checked automatically.

## Enforcement model

Validation runs in two places, generated from the same constraints, each doing
what it is good at:

- **Server (Go) — authoritative, fail-fast.** Validates every input field,
  stops at the **first** violation, returns a structured HTTP 400. The server
  never trusts that a client pre-validated; fail-fast is correct because the
  server's job is to *reject*, and rejection needs only one reason.
- **Generated clients (TypeScript, Python) — UX, collect-all.** Run the same
  constraints before sending and report *all* bad fields at once, so a web app
  can highlight every error with zero round-trips.

The division matters: enumerating all failures is a UX nicety and belongs to the
client; the server stays simple. A non-browser caller (`curl`, another service)
that skips client validation still gets fully rejected — just one reason per
round-trip, which is cheap and deterministic.

`@regex` is enforced on the **Go server only.** Regex flavors differ (Go RE2 vs
POSIX vs JS); rather than reconcile them, patterns are authoritative server-side
and the other targets skip `@regex`. The compiler validates patterns against
Go's `regexp` — the one engine that will ever run them.

## Implementation

Five pieces across the compiler and runtime.

### 1. AST + scanner + parser

`ast.Constraint` (kind + raw literal args) added to `Field` and `Param`:

```go
type ConstraintKind int
const (
    ConstraintMin ConstraintKind = iota
    ConstraintMax
    ConstraintMinLen
    ConstraintMaxLen
    ConstraintNotEmpty
    ConstraintNotNull
    ConstraintRegex
)

type Constraint struct {
    Kind ConstraintKind
    Num  string // raw numeric literal for Min/Max/MinLen/MaxLen ("0", "0.5")
    Str  string // pattern for Regex
    Pos  Pos
}
```

Numeric bounds are stored as the **raw literal string**, not a parsed number —
avoids float-precision drift and lets each target emit the literal verbatim.

Scanner: add a `FLOAT` token and a fractional tail to `scanNumber` (today it
returns `INT` and stops at `.`, so `@min(0.5)` would not tokenize). Existing
`INT` uses — array sizes, const/enum values — are unaffected.

Parser: a `parseConstraints()` helper reads zero or more `@name(args)` after a
type, wired into the field loop in `parseType` and the param loop in `parseFunc`
(after the `byref`/`out`/`ptr` block). No ambiguity: inside a type body the token
after a constraint-less field is the next field's `IDENT` or `RBRACE`, never `@`.

### 2. Compile-time check pass (`check.Validate(api)`)

Run in `modcompile` between a successful parse and codegen. There is no semantic
pass today; this adds one. It reports **all** errors in a run (a compiler should
not stop at the first) and fails the build if any exist. Rules:

| Rule                    | Rejected example              |
|-------------------------|-------------------------------|
| Kind vs type            | `name: string @min(0)`        |
| Literal fits the type   | `x: i32 @max(9999999999)`, `x: i32 @min(0.5)` |
| Sign                    | `x: u32 @min(-1)`             |
| Ordering                | `x: i32 @min(10) @max(1)`     |
| Redundant `@notnull`    | `tag: string @notnull`        |
| Unsatisfiable length    | `xs: [4]f64 @minlen(9)`       |
| Bad regex               | `s: string @regex("(")`       |
| Constraint on `out`/`ptr` param | `func f(x: i32 out @min(0))` |
| Duplicate constraint    | `x: i32 @min(0) @min(1)`      |
| `@min`/`@max` on enum   | `m: TaskMode @max(3)` (redundant with auto-check) |

Compiling regex here with Go's `regexp` is correct, not a shortcut: regex is
Go-server-only, so validating against RE2 *is* validating against the only engine
that runs it.

### 3. Runtime (`apiserver/validate.go`)

Keeps the fiddly bits — regex, rune counting, error shape — out of generated
code, and makes regex structurally Go-only (these helpers exist only here):

```go
type ValidationError struct {
    Field      string `json:"field"`      // dotted path: "entry.diameter"
    Constraint string `json:"constraint"` // "min","maxlen","regex","enum",...
    Message    string `json:"message"`
}
func (e *ValidationError) Error() string { return e.Message }
func NewValidationError(field, constraint, msg string) *ValidationError { … }

func RuneLen(s string) int                                  // @minlen/@maxlen count characters
func MustRegex(p string) *regexp.Regexp                     // compiled once at package init
func ValidateRegex(field string, re *regexp.Regexp, v string) *ValidationError
```

`writeDispatchError` gains one branch, before its errno switch, that renders a
`*ValidationError` as a 400 with `{error, code, field, constraint}`. The existing
errno contract is untouched.

### 4. Server emit (`constraintEmitter` in `cgen/constraint_emit.go`)

The emit lives in a generator-agnostic `constraintEmitter` (returns code as
strings) wired into `GenerateDispatchC` — the generator behind the cgo `_cgo.go`
dispatch file, which is the production REST path (`--server-meta`). *Not*
`server_go.go`'s `GenerateServerGo`, which is a separate test-only generator.

The dispatch wrapper already unmarshals `req` into a flat `params` struct then
converts and calls the C callback. Validation slots in **between** the unmarshal
and the conversion, so path/query params (merged into the same body by the
runtime) are covered for free. A package-scope pass emits one compiled regex var
per distinct pattern; struct-typed params recurse (dotted paths), slice/array
params recurse per element (runtime-indexed paths), and enum-typed values get an
automatic membership `switch`.

Generated output for `put_tool` (verified to compile against the real package):

```go
var tooltableRe0 = apiserver.MustRegex("^[a-z]+$")

func tooltableDispatchPutTool(callbacks unsafe.Pointer, req []byte) ([]byte, error) {
    cb := (*C.tooltable_callbacks_t)(callbacks)
    // ... _freeList setup ...
    var params struct {
        Toolno int32     `json:"toolno"`
        Entry  ToolEntry `json:"entry"`
    }
    if len(req) > 0 {
        if err := json.Unmarshal(req, &params); err != nil {
            return nil, syscall.EINVAL
        }
    }

    // --- validation (generated from @constraints) ---
    if params.Toolno < 1 {
        return nil, apiserver.NewValidationError("toolno", "min", "toolno must be >= 1")
    }
    if params.Entry.Toolno < 1 {
        return nil, apiserver.NewValidationError("entry.toolno", "min", "entry.toolno must be >= 1")
    }
    if params.Entry.Toolno > 99999 {
        return nil, apiserver.NewValidationError("entry.toolno", "max", "entry.toolno must be <= 99999")
    }
    if params.Entry.Diameter < 0 {
        return nil, apiserver.NewValidationError("entry.diameter", "min", "entry.diameter must be >= 0")
    }
    if params.Entry.Orientation < 0 {
        return nil, apiserver.NewValidationError("entry.orientation", "min", "entry.orientation must be >= 0")
    }
    if params.Entry.Orientation > 8 {
        return nil, apiserver.NewValidationError("entry.orientation", "max", "entry.orientation must be <= 8")
    }
    if apiserver.RuneLen(params.Entry.Comment) > 255 {
        return nil, apiserver.NewValidationError("entry.comment", "maxlen", "entry.comment must have at most 255 chars")
    }
    if verr := apiserver.ValidateRegex("entry.comment", tooltableRe0, params.Entry.Comment); verr != nil {
        return nil, verr
    }
    // enum-typed fields also get an automatic membership switch here
    // --- end validation ---

    // ... convert params → C, call the C wrapper, marshal the result ...
}
```

The emitter recurses: a struct-typed param validates its fields (dotted path),
a slice/array validates its elements, a nullable field checks `@notnull` then
guards remaining checks under a non-nil test.

### 5. Enum auto-validation

Zero annotation. Enum names resolve to `TypeNamed`, so the generator looks a
field/param type up in `api.Enums` and emits a `switch` over the declared integer
values (a switch, not a range check — enum values can be sparse, e.g. `emcstat`).
Anything else returns a `"enum"` `ValidationError`.

Opt-out `@enum_open` for forward-compatible fields where an unknown value from a
newer peer should pass through rather than 400. Default is closed.

## Design decisions

- **Raw-literal storage** for numeric bounds — no compiler-side float precision
  loss; targets emit the literal directly.
- **Length counts characters** (`RuneLen`), not bytes, for strings; `len()` for
  slices/arrays.
- **Fail-fast server, collect-all clients** — see Enforcement model.
- **Regex is Go-server-only** — avoids cross-engine flavor mismatch.
- **Redundant annotations are hard errors** (`@notnull` on non-nullable,
  `@min` on enum). In a generated system, "this annotation does nothing" is worth
  stopping the build for.
- **Error shape stays non-exclusive** — the flat `{field, constraint, message}`
  is right for fail-fast, and can gain a `violations: []` array later without
  restructuring, should a consumer ever need all errors at once.
- **Validation runs only in the userspace gomc-server, never in RT.** All checks
  live in the Go REST/WS dispatch (`_cgo.go` / `_bridge.go`), which handles the
  external JSON boundary for both cmod and gomod APIs; the generated C header
  (`_api.h`) contains no validation. A real RT caller reaches a cmod C→C
  directly, bypassing the dispatch entirely — so `@regex` (allocation,
  non-deterministic timing) never executes in RT context. **Guard for the
  future:** if C-side enforcement is ever emitted into a module's own callbacks,
  it must exclude `@regex` — and realistically all non-trivial validation — for
  `@rt_safe` functions, since RT forbids allocation and unbounded work.

## Build order (as shipped)

Independently testable, independently valuable slices:

1. AST + scanner + parser + `check.Validate` + a parser round-trip test.
2. Runtime `validate.go` + the `writeDispatchError` branch (no codegen).
3. REST dispatch emit (`constraintEmitter`) wired into `GenerateDispatchC`, with
   a test asserting the `put_tool` output above and an end-to-end compile.
4. WebSocket command-handler validation (`GenerateServerGoExtra`), referencing
   the regex vars declared in `_cgo.go`.
5. Client-side collect-all (`clientValidation` + TS/Python adapters), verified by
   `tsc --strict`, `py_compile`, and functional collect-all runs.
