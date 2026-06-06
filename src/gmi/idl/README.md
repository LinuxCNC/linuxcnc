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
