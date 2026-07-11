# GOMC Interface Definition (GMI)

Machine-readable API definitions for LinuxCNC's inter-module communication.

## Purpose

GMI files define:
- Data types (structs, enums)
- Function signatures with request/response types
- API-level metadata (versioning, REST exposure, RT safety)

## Codegen Targets

From each `.gmi` file, generate:
- **C**: Server callbacks + types (`*_api.h`)
- **C**: Client library using cJSON/libcurl (`*_client.h`, `*_client.c`)
- **Go**: Server handlers + HTTP routing
- **Python**: Client library using requests

## File Format

```
@api <name>
@version <n>
@prefix "<path>"
@rest_export true|false

enum Name {
    VALUE = n
    ...
}

type Name {
    field: type
    ...
}

@method "GET|POST|PUT|DELETE"
@path "/endpoint"
@rt_safe "true|false"
@doc "description"
func name(params) -> ReturnType
```

Function annotations (`@method`, `@path`, `@rt_safe`, `@doc`) precede the `func`
declaration. Values must be quoted strings. Functions do not use braces.

## Field & Parameter Constraints

Fields (in `type` blocks) and parameters may carry inline validation
constraints, written after the type — on a parameter, after any
`byref`/`out`/`ptr` mode keyword (`name: type [mode] [@constraints…]`):

```
type ToolEntry {
    pocketno: i32    @min(0) @max(1000)
    comment:  string @maxlen(255)
}

func put_tool(toolno: i32 @min(1), entry: ToolEntry) -> PutToolResult
```

| Constraint            | Applies to               | Meaning                       |
|-----------------------|--------------------------|-------------------------------|
| `@min(n)` / `@max(n)` | `i*`, `u*`, `f*`         | numeric bound (inclusive)     |
| `@minlen(n)` / `@maxlen(n)` | `string`, `[]T`, `[N]T` | length bound (chars / elems) |
| `@notempty`           | `string`, `[]T`, `[N]T`  | length > 0                    |
| `@notnull`            | nullable `T?` (not `string?`) | value must be present    |
| `@regex("…")`         | `string`                 | full-match pattern (server only) |

Enum-typed fields/params are validated **automatically** (value must be a
declared variant) — no annotation needed; opt out with `@enum_open`.

Enforcement: the server rejects the first violation with an HTTP 400
`{error, code, field, constraint}`; the generated TypeScript/Python clients
collect **all** violations and raise before sending. `@regex` runs on the Go
server only (avoids JS/Python/RE2 flavor mismatch). Mistyped or contradictory
constraints (e.g. `@maxlen` on an `i32`, `@min > @max`) fail the build.

See `../FIELD_VALIDATION_DESIGN.md` for the full design.

## Type System

| GMI       | C              | Go        | Python         |
|-----------|----------------|-----------|----------------|
| bool      | bool           | bool      | bool           |
| i32       | int32_t        | int32     | int            |
| u32       | uint32_t       | uint32    | int            |
| i64       | int64_t        | int64     | int            |
| u64       | uint64_t       | uint64    | int            |
| f64       | double         | float64   | float          |
| string    | const char*    | string    | str            |
| T?        | T* (nullable)  | *T        | Optional[T]    |
| [T; N]    | T[N]           | [N]T      | list[T]        |
| []T       | T*, size_t len | []T       | list[T]        |
| ptr       | void*          | unsafe.Pointer | N/A       |

## Files

- `common.gmi` — shared types (Position, etc.)
- `hal.gmi` — HAL component API
- `halcmd.gmi` — HAL commands (launcher integration)
- `motion.gmi` — motion control
- `task.gmi` — task/program control
- `status.gmi` — read-only status queries
