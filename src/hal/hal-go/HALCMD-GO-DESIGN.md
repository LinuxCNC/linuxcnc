# HALCMD-GO-DESIGN: Replacing C halcmd with a Go Implementation

## Overview

This document describes the plan to replace the C `halcmd` binary with a native Go
implementation inside the `hal-go` package. The goal is a **Go library** providing
individual callable command functions (for the launcher, REST API, and other Go callers)
plus a **Go file interpreter** that can replace `halcmd -f <file>` subprocess calls.

The `halcmd` binary will **not** be deleted — it remains useful as a standalone debugging
tool, for `halrun`, and for any use outside the Go launcher. We are replacing its use as
a *subprocess* inside the Go launcher and halfile executor.

---

## 1. Architecture Layers

The implementation is split into three layers:

### Layer 1 — C Shim (`cgo.go`)

Thin C helper functions written inside the cgo preamble (the `/* ... */` block before
`import "C"`). Each shim:

- Calls `liblinuxcnchal` public API functions (`hal_signal_new`, `hal_link`,
  `hal_add_funct_to_thread`, etc.), **or**
- Accesses `hal_data` shared memory directly using `hal_priv.h` internals
  (`halpr_find_pin_by_name`, `halpr_find_sig_by_name`, `SHMPTR`, mutex lock/unlock), **or**
- Forks and execs an external process (`rtapi_app load`, arbitrary user processes).

Already implemented: `hal_init`, `hal_exit`, `hal_ready`, pin creation, port I/O,
`hal_start_threads`, `hal_stop_threads`, `hal_shim_list_comps`, `hal_shim_unload_all`.

### Layer 2 — Go Command Functions (`command.go`)

Public Go functions, one per halcmd command (or logical operation). Each function:

- Accepts typed Go arguments (strings, enums, numeric values).
- Calls one or more Layer 1 shim functions via `C.hal_shim_*()`.
- Returns structured Go data (not formatted strings) or a typed `error`.
- Is the REST API entry point — return types must be JSON-serializable.

Already implemented: `StartThreads()`, `StopThreads()`, `ListComponents()`, `UnloadAll()`.

To be added: `Net()`, `LoadRT()`, `LoadUSR()`, `AddF()`, `DelF()`, `NewSig()`, `DelSig()`,
`SetP()`, `GetP()`, `SetS()`, `GetS()`, `PType()`, `SType()`, `LinkPS()`, `LinkSP()`,
`UnlinkP()`, `Alias()`, `UnAlias()`, `Show()`, `List()`, `Save()`, `WaitUSR()`, etc.

### Layer 3 — Go Parser and Executor (`token.go`, `parser.go`, `executor.go`, new files)

A typed-token, parse-then-execute pipeline. It:

- Parses each HAL file through a three-tier pipeline: `SingleFileParser` (template +
  INI/ENV substitution + source recursion + token classification) → `LineParser`
  (per-command typed token) → `MultiFileParser` (loops over files and merges results).
- Produces strongly-typed token structs with all parameters already validated (strings
  → enums, `count=5` → int, arrow tokens stripped from `net` pin lists, etc.).
- Classifies tokens into `loadrt`, `loadusr`, and `halcmd` lists inside
  `SingleFileParser`, then executes them in order (merged `loadrt` first via
  `TwopassCollector`, then `loadusr`, then `halcmd`).
- Handles `source` (recursive file inclusion) at parse time inside `SingleFileParser`.
- Tracks filename and line number in every `Token.Location` for error messages.
- The caller provides `INILookup`, `*HalTemplateData`, and `PathResolver` — the parser
  has no knowledge of specific INI parsers, path resolution, or template construction.

Replaces the `halcmd -f` subprocess call in `src/launcher/halfile/halfile.go`.

### Layer Dependencies

```
┌──────────────────────────────────────────────┐
│  REST API / Launcher / Other Go callers       │
│  Provides: INILookup, *HalTemplateData,       │
│            PathResolver                       │
└──────────────────────────┬───────────────────┘
                           │  calls
┌──────────────────────────▼───────────────────┐
│  Layer 3: Go Parser + Executor                │
│  (token.go, parser.go, executor.go)           │
│  MultiFileParser → SingleFileParser →         │
│  LineParser → classify → execute              │
└──────────────────────────┬───────────────────┘
                           │  calls
┌──────────────────────────▼───────────────────┐
│  Layer 2: Go Command Functions  (command.go)  │
│  Net(), LoadRT(), SetP(), Show(), ...         │
└──────────────────────────┬───────────────────┘
                           │  calls via cgo
┌──────────────────────────▼───────────────────┐
│  Layer 1: C Shim  (cgo.go)                    │
│  hal_shim_net(), hal_shim_setp(), ...         │
└──────────────────────────┬───────────────────┘
                           │  links to
┌──────────────────────────▼───────────────────┐
│  liblinuxcnchal  +  hal_priv.h  +  rtapi_app  │
└──────────────────────────────────────────────┘
```

---

## 2. C Shim Functions Needed

For each halcmd command, the table below identifies what the C shim must do and which
C API it uses. Shims are grouped by complexity.

### 2a. Simple Wrappers — one public C API call

| halcmd command | C shim name | C API called |
|---|---|---|
| `newsig <name> <type>` | `hal_shim_newsig` | `hal_signal_new(name, type)` |
| `delsig <name>` | `hal_shim_delsig` | `hal_signal_delete(name)` |
| `linkps <pin> <sig>` | `hal_shim_linkps` | `hal_link(pin, sig)` |
| `linksp <sig> <pin>` | `hal_shim_linksp` | `hal_link(pin, sig)` (arg order swap) |
| `unlinkp <pin>` | `hal_shim_unlinkp` | `hal_unlink(pin)` |
| `addf <funct> <thread>` | `hal_shim_addf` | `hal_add_funct_to_thread(funct, thread, pos)` |
| `delf <funct> <thread>` | `hal_shim_delf` | `hal_del_funct_from_thread(funct, thread)` |
| `start` | `hal_shim_start_threads` | `hal_start_threads()` — **already exists** |
| `stop` | `hal_shim_stop_threads` | `hal_stop_threads()` — **already exists** |
| `lock` | `hal_shim_lock` | `hal_set_lock(HAL_LOCK_ALL)` |
| `unlock` | `hal_shim_unlock` | `hal_set_lock(HAL_LOCK_NONE)` |

### 2b. Multi-call — validation + multiple API calls

| halcmd command | C shim name | What it does |
|---|---|---|
| `net <sig> <pin...>` | `hal_shim_net` | Find or create signal; for each pin call `hal_link(pin, sig)`. Checks pin direction compatibility. Needs `halpr_find_sig_by_name`, `halpr_find_pin_by_name`, then `hal_signal_new` + `hal_link`. |
| `linkpp <pin1> <pin2>` | `hal_shim_linkpp` | (deprecated) Find pin1's signal (or create one), link pin2 to it. Needs `halpr_find_pin_by_name`. |
| `alias <pin\|param> <name> <alias>` | `hal_shim_alias` | `hal_pin_alias` or `hal_param_alias` depending on first arg. |
| `unalias <pin\|param> <name>` | `hal_shim_unalias` | `hal_pin_alias(name, NULL)` or `hal_param_alias(name, NULL)` |
| `waitusr <comp>` | `hal_shim_waitusr` | Poll HAL shmem (via `halpr_find_comp_by_name`) until component disappears or timeout. |

### 2c. Shmem Access — need `hal_priv.h` for pin/param/signal value access

These commands read or write values stored in HAL shared memory at offsets accessed via
`SHMPTR`. They require the HAL mutex and `hal_priv.h` internals.

| halcmd command | C shim name | Internals needed |
|---|---|---|
| `setp <name> <value>` | `hal_shim_setp` | `halpr_find_pin_by_name` or `halpr_find_param_by_name`; write to `SHMPTR(pin->data_ptr)` or `SHMPTR(param->data_ptr)` |
| `getp <name>` | `hal_shim_getp` | Same lookup; read value and return as string/union |
| `sets <sig> <value>` | `hal_shim_sets` | `halpr_find_sig_by_name`; write to `SHMPTR(sig->data_ptr)` |
| `gets <sig>` | `hal_shim_gets` | `halpr_find_sig_by_name`; read value |
| `ptype <name>` | `hal_shim_ptype` | `halpr_find_pin_by_name` or `halpr_find_param_by_name`; return type enum |
| `stype <sig>` | `hal_shim_stype` | `halpr_find_sig_by_name`; return type enum |
| `show <type> [patterns]` | `hal_shim_show_*` | Walk shmem linked lists via `hal_data->comp_list_ptr`, `hal_data->pin_list_ptr`, etc. Return structured data. |
| `list <type> [patterns]` | `hal_shim_list_*` | Same shmem walk, return name array. `halListComponents` already exists. |
| `save [type] [file]` | `hal_shim_save` | Walk shmem; serialize to HAL file format strings. |
| `status` | `hal_shim_status` | Read `hal_data->shmem_avail`, lock state, etc. |
| `debug <level>` | `hal_shim_debug` | Write to `rtapi_msg_level` via `rtapi_set_msg_level()` |

### 2d. Process Management — fork/exec external programs

| halcmd command | C shim name | What it does |
|---|---|---|
| `loadrt <mod> [args...]` | `hal_shim_loadrt` | On USPACE: exec `rtapi_app load <mod> [args]` and wait; equivalent to `hal_systemv`. On RTAI: `insmod`. |
| `loadusr [opts] <prog> [args...]` | `hal_shim_loadusr` | `fork`+`exec` the user-space program. Handles `-W` (wait for HAL component), `-Wn <name>` (wait for named component), `-w` (wait for process exit), `-iN` (stdin from /dev/null). |
| `unloadrt <mod>` | `hal_shim_unloadrt` | exec `rtapi_app unload <mod>` |
| `unloadusr <comp>` | `hal_shim_unloadusr` | Send `SIGTERM` to the process owning the component (via `kill()`). `hal_shim_unload_all` already does this. |
| `unload <comp>` | `hal_shim_unload` | Dispatch to `unloadrt` or `unloadusr` based on component type flag. |

**Note on `loadrt` on USPACE**: halcmd's `do_loadrt_cmd` already wraps `loadrt` as
`loadusr -Wn <mod> rtapi_app load <mod>` on USPACE. The Go shim should do the same.
The key difference from `loadusr` is that it uses `rtapi_app` as the actual process.

---

## 3. Go Command Function Signatures

All functions live in `command.go` (package `hal`). Return types are designed to be
JSON-serializable for REST API use.

```go
// --- Signal commands ---

// NewSig creates a new HAL signal with the given name and type.
func NewSig(name string, halType Type) error

// DelSig removes a HAL signal.
func DelSig(name string) error

// SetS sets the value of an unlinked signal.
func SetS(name string, value string) error

// GetS returns the current value of a signal as a string.
func GetS(name string) (string, error)

// SType returns the HAL type of a signal.
func SType(name string) (Type, error)

// --- Pin/param value commands ---
// SetP sets the value of a pin or parameter by name.
func SetP(name string, value string) error

// GetP returns the current value of a pin or parameter as a string.
func GetP(name string) (string, error)

// PType returns the HAL type of a pin or parameter.
func PType(name string) (Type, error)

// --- Link / net commands ---

// Net connects a signal to one or more pins. If the signal does not exist it
// is created with a type inferred from the first pin's type. Arrow tokens
// ("=>", "<=", "<=>") in pins are silently removed before processing.
func Net(signame string, pins ...string) error

// LinkPS links a pin to a signal (pin-first argument order).
func LinkPS(pin, sig string) error

// LinkSP links a signal to a pin (signal-first argument order).
func LinkSP(sig, pin string) error

// LinkPP links two pins together by finding or creating a signal that connects them.
// This command is deprecated in halcmd; prefer Net() for new HAL files.
// It is retained here for backward compatibility with HAL files that use it.
func LinkPP(pin1, pin2 string) error

// UnlinkP removes a pin from its signal.
func UnlinkP(pin string) error

// --- Alias commands ---

// Alias creates an alias for a pin or parameter.
// kind must be "pin" or "param".
func Alias(kind, name, alias string) error

// UnAlias removes an alias from a pin or parameter.
// kind must be "pin" or "param".
func UnAlias(kind, name string) error

// --- Thread function commands ---

// AddF adds a realtime function to a thread.
// pos controls insertion order: 0 (or any positive value) appends to the end
// of the thread's function list. -1 inserts before the first existing function.
// Other negative values are not defined by halcmd and should not be used.
func AddF(funct, thread string, pos int) error

// DelF removes a realtime function from a thread.
func DelF(funct, thread string) error

// --- RT component management ---

// LoadRT loads a realtime component (module) with optional parameters.
// On USPACE this calls rtapi_app load.
func LoadRT(mod string, args ...string) error

// UnloadRT unloads a realtime component.
func UnloadRT(mod string) error

// --- User-space component management ---

// LoadUSROptions controls loadusr subprocess behaviour.
type LoadUSROptions struct {
    WaitReady     bool   // -W: wait until HAL component appears
    WaitName      string // -Wn: wait for component with this name
    WaitExit      bool   // -w: wait for process exit
    NoStdin       bool   // -i: replace stdin with /dev/null
    TimeoutSecs   int    // -T: timeout in seconds (0 = default)
}

// LoadUSR starts a user-space program. opts may be nil for defaults.
func LoadUSR(opts *LoadUSROptions, prog string, args ...string) error

// UnloadUSR sends SIGTERM to the component owner process.
func UnloadUSR(comp string) error

// Unload unloads a component regardless of type (realtime or user-space).
// It detects whether comp is a realtime module or a user-space component and
// dispatches to UnloadRT or UnloadUSR accordingly.
func Unload(comp string) error

// WaitUSR waits until a user-space component disappears from HAL.
func WaitUSR(comp string) error

// --- Lock/unlock ---

// Lock sets the HAL lock level ("tune", "load", or "all").
func Lock(level string) error

// Unlock sets the HAL lock level ("tune", "load", or "none").
func Unlock(level string) error

// --- Debug ---

// SetDebug sets the RTAPI message verbosity level (0–5).
func SetDebug(level int) error

// --- Structured query types (for show / list / save) ---

// PinInfo holds all attributes of a HAL pin.
type PinInfo struct {
    Name      string  `json:"name"`
    Type      string  `json:"type"`
    Direction string  `json:"direction"`
    Value     string  `json:"value"`
    Signal    string  `json:"signal,omitempty"`
    Owner     string  `json:"owner"`
}

// ParamInfo holds all attributes of a HAL parameter.
type ParamInfo struct {
    Name      string `json:"name"`
    Type      string `json:"type"`
    Direction string `json:"direction"`
    Value     string `json:"value"`
    Owner     string `json:"owner"`
}

// SigInfo holds all attributes of a HAL signal.
type SigInfo struct {
    Name      string   `json:"name"`
    Type      string   `json:"type"`
    Value     string   `json:"value"`
    Drivers   []string `json:"drivers,omitempty"`
    Readers   []string `json:"readers,omitempty"`
}

// FunctInfo holds all attributes of a HAL realtime function.
type FunctInfo struct {
    Name    string `json:"name"`
    Owner   string `json:"owner"`
    Thread  string `json:"thread,omitempty"`
    Runtime int64  `json:"runtime_ns,omitempty"`
    MaxTime int64  `json:"maxtime_ns,omitempty"`
}

// ThreadInfo holds all attributes of a HAL thread.
type ThreadInfo struct {
    Name     string   `json:"name"`
    Period   int64    `json:"period_ns"`
    Functs   []string `json:"functs"`
    Running  bool     `json:"running"`
}

// CompInfo holds all attributes of a HAL component.
type CompInfo struct {
    Name   string `json:"name"`
    ID     int    `json:"id"`
    Type   string `json:"type"`
    State  string `json:"state"`
}

// ShowResult aggregates what "show" returns.
type ShowResult struct {
    Comps   []CompInfo   `json:"comps,omitempty"`
    Pins    []PinInfo    `json:"pins,omitempty"`
    Params  []ParamInfo  `json:"params,omitempty"`
    Signals []SigInfo    `json:"signals,omitempty"`
    Functs  []FunctInfo  `json:"functs,omitempty"`
    Threads []ThreadInfo `json:"threads,omitempty"`
}

// Show returns structured information about HAL objects matching the given type
// and optional name patterns. type can be "all", "comp", "pin", "param",
// "sig", "funct", "thread".
func Show(halType string, patterns ...string) (*ShowResult, error)

// List returns names of HAL objects matching type and patterns.
// Equivalent to "halcmd list <type> [patterns]".
func List(halType string, patterns ...string) ([]string, error)

// Save serializes current HAL state as halcmd commands.
// halType can be "comp", "sig", "link", "net", "param", "thread", or "all".
// If filename is non-empty the output is written to that file; otherwise
// the lines are returned as a slice.
func Save(halType string, filename string) ([]string, error)

// Status returns a summary of HAL shared memory usage and lock state.
type StatusInfo struct {
    ShmemFree   int    `json:"shmem_free_bytes"`
    LockLevel   string `json:"lock_level"`
}
func Status() (*StatusInfo, error)
```

### Error types

```go
// HalError wraps a HAL C error code with an operation name and message.
// (extends the existing errors.go Error type)
type HalError struct {
    Code    int
    Op      string
    Message string
}
func (e *HalError) Error() string
```

---

## 4. Parser and Executor Design

The interpreter (new files `token.go`, `parser.go`, `executor.go`, package `hal`)
replaces `halcmd -f` using a **typed-token, parse-then-execute** architecture. Instead
of a monolithic line-by-line interpreter with runtime string dispatch, the pipeline
is split into three distinct tiers:

1. **Parse and classify** each input file into strongly-typed token lists (inside
   `SingleFileParser`).
2. **Merge** results across files (inside `MultiFileParser`).
3. **Execute** buckets in order: merged `loadrt` → `loadusr` → `halcmd`.

Flow control (`if`/`elif`/`else`/`endif`, `while`/`endwhile`) is **not** handled by
the interpreter — it is entirely replaced by Go `text/template` conditionals and loops
that run inside `SingleFileParser` before any token is produced (see Section 6a).

---

### 4a. Three-Tier Parser

#### Caller-provided interfaces and data-flow diagram

The caller (e.g. the launcher) constructs the callback objects and passes them to
`MultiFileParser`. `SingleFileParser` calls back through these interfaces and never
imports the launcher's INI parser, path resolver, or template-data builder directly.

```
┌─────────────────────────────────────────────────────────┐
│  Caller (Launcher)                                      │
│  Provides: INILookup, *HalTemplateData, PathResolver    │
│  Calls: MultiFileParser.Parse(halfiles)                 │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│  MultiFileParser                                        │
│  For each file: call SingleFileParser, merge results    │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│  SingleFileParser                                       │
│  1. Read file                                           │
│  2. RenderHalTemplate (if {{)                           │
│  3. For each line: INI/ENV sub → tokenize → LineParser  │
│  4. Classify token → LoadRT / LoadUSR / HALCmd          │
│  5. source → recursive SingleFileParser (depth+1)       │
│  Returns: ParseResult{LoadRT, LoadUSR, HALCmd}          │
└─────────────────┬───────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────┐
│  LineParser (parseLine function)                        │
│  tokens[] → per-command parse → Token{Data: *XxxToken}  │
└─────────────────────────────────────────────────────────┘
```

```go
// PathResolver resolves source file paths. The caller provides an implementation
// backed by whatever path resolution logic they have (e.g. the launcher's
// resolve.go with LIB: prefix and HALLIB_PATH support).
type PathResolver interface {
    Resolve(path string) (string, error)
}

// INILookup provides access to INI file values. The caller provides an
// implementation backed by whatever INI parser they already have.
type INILookup interface {
    Get(section, key string) (string, error)
    GetAll() map[string]map[string]string  // needed for HalTemplateData
}
```

#### Tier 1: LineParser

LineParser takes a single pre-processed line (after quote/escape/comment handling and
variable substitution) and produces one typed token. It:

- Splits the line into tokens on whitespace (handling single/double quotes and
  backslash escapes using the same rules as halcmd's C tokenizer).
- Identifies the command from `tokens[0]`.
- Dispatches to a per-command parse function that validates and converts all arguments
  into fully-typed fields (strings → enums, `count=5` → int, arrow tokens stripped
  from `net` pin lists, etc.).
- Returns a `Token` (with a fully-populated `Data` field) or a `*ParseError`.

Errors are caught at parse time, before any HAL state is modified.

```go
// tokenizeLine splits a single line into raw string tokens.
// It handles single/double quotes and backslash escapes.
// Returns (tokens, error).
func tokenizeLine(line string) ([]string, error)

// parseLine converts a raw token slice into a typed Token.
// loc is used to populate the returned Token.Location and any ParseError.
func parseLine(tokens []string, loc SourceLoc) (Token, *ParseError)
```

#### Tier 2: SingleFileParser

`SingleFileParser` takes a **file path**, reads the file, and returns a `*ParseResult`
with tokens already classified into the three execution buckets. It:

1. Reads the file from disk.
2. Runs the content through `RenderHalTemplate` (from `template.go`) so that
   `{{...}}` directives are expanded before any line is parsed.  Template expansion
   is per-file, so `source`'d files get their own independent template context.
3. Handles `[SECTION]KEY` INI substitution (via `ini.Get(section, key)`) and `$ENV`
   environment substitution on each raw line.
4. Handles line continuation (`\` at end of line) and strips comments (`#`).
5. Feeds each resulting line to `LineParser`, then immediately classifies the token:
   - `*LoadRTToken` → `result.LoadRT`
   - `*LoadUSRToken` with `WaitReady || WaitName != ""` → `result.LoadUSR`
   - Everything else → `result.HALCmd`
6. When it encounters a `source <file>` command it calls `resolver.Resolve(path)` to
   resolve the path, then **recursively calls itself** on the resolved file (steps
   1–6 above) and merges the returned `ParseResult` into its own.  A depth counter
   (maximum 20) prevents infinite recursion.

`source` is resolved at parse time — it never appears as a token.

```go
// SingleFileParser parses a single HAL file (and any source'd files recursively)
// into a ParseResult with tokens classified into three execution buckets.
type SingleFileParser struct {
    ini          INILookup
    templateData *HalTemplateData
    resolver     PathResolver
    depth        int  // current recursion depth; rejects > 20
}

// Parse reads path, renders templates, performs substitutions, feeds each line to
// LineParser, classifies each token, and recurses for source commands.
func (sp *SingleFileParser) Parse(path string) (*ParseResult, error)
```

#### Tier 3: MultiFileParser

`MultiFileParser` takes an array of file paths (the `[HAL]HALFILE` entries), creates a
`SingleFileParser` for each one, and merges the returned `ParseResult` objects into a
single `ParseResult`:

- **`LoadRT`** — every `loadrt` token (collected from all files).
- **`LoadUSR`** — `loadusr` tokens with `-W` or `-Wn` flags set (collected from all
  files).
- **`HALCmd`** — everything else.

`MultiFileParser` is a trivial collector; all classification logic lives in
`SingleFileParser`.

```go
// MultiFileParser parses a set of HAL files and merges the ParseResult from each
// SingleFileParser into a single result.
type MultiFileParser struct {
    ini          INILookup
    templateData *HalTemplateData
    resolver     PathResolver
}

// Parse processes each file path in order and returns a merged ParseResult.
func (mp *MultiFileParser) Parse(paths []string) (*ParseResult, error) {
    merged := &ParseResult{}
    for _, path := range paths {
        sp := &SingleFileParser{
            ini:          mp.ini,
            templateData: mp.templateData,
            resolver:     mp.resolver,
        }
        result, err := sp.Parse(path)
        if err != nil {
            return nil, err
        }
        merged.LoadRT  = append(merged.LoadRT,  result.LoadRT...)
        merged.LoadUSR = append(merged.LoadUSR, result.LoadUSR...)
        merged.HALCmd  = append(merged.HALCmd,  result.HALCmd...)
    }
    return merged, nil
}
```

---

### 4b. Typed Tokens — Sealed Interface Pattern

Every token carries a `SourceLoc` (for error reporting) and a `TokenData` value
whose **concrete type** is the discriminator — no separate enum field is needed.

```go
// TokenData is implemented by all per-command token structs.
// The unexported marker method seals the interface to this package.
type TokenData interface {
    tokenData()
}

// Token is the parsed representation of one HAL command line.
type Token struct {
    Location SourceLoc
    Data     TokenData  // exactly one concrete type
}

// SourceLoc records the file and line number where a token originated.
type SourceLoc struct {
    File string
    Line int
}
```

---

### 4c. Enums for Categorical Values

Every field that has a fixed set of valid values uses an enum, not a string.
String-to-enum conversion happens inside the per-command parse functions; an invalid
string causes a `*ParseError` before any token is added to the list.

```go
// AliasKind distinguishes pin aliases from param aliases.
type AliasKind int
const (
    AliasPin   AliasKind = iota
    AliasParam
)

// LockLevel maps to HAL's HAL_LOCK_* constants.
type LockLevel int
const (
    LockNone   LockLevel = 0
    LockLoad   LockLevel = 1
    LockConfig LockLevel = 2
    LockTune   LockLevel = 3
    LockParams LockLevel = 4
    LockRun    LockLevel = 8
    LockAll    LockLevel = 255
)

// HalObjType enumerates the object classes accepted by list/show.
type HalObjType int
const (
    ObjPin    HalObjType = iota
    ObjSig
    ObjParam
    ObjFunct
    ObjThread
    ObjComp
    ObjAll
)

// SaveType enumerates the save sub-commands.
type SaveType int
const (
    SaveComp   SaveType = iota
    SaveSig
    SaveLink
    SaveNet
    SaveParam
    SaveThread
    SaveAll
)

// PinType is already defined in types.go (TypeBit, TypeFloat, TypeS32, TypeU32).
```

---

### 4d. Per-Command Token Structs

One struct per halcmd command, each implementing `TokenData`.

```go
type LoadRTToken struct {
    Comp   string
    Count  int               // parsed from count=N; 0 if absent
    Names  []string          // parsed from names=a,b,c; nil if absent
    Params map[string]string // remaining key=value args (module-specific)
}
func (*LoadRTToken) tokenData() {}

type LoadUSRToken struct {
    WaitReady bool     // -W flag
    WaitName  string   // -Wn <name> flag
    WaitExit  bool     // -w flag
    NoStdin   bool     // -i flag
    Timeout   int      // -T <secs> flag; 0 = default
    Prog      string
    Args      []string
}
func (*LoadUSRToken) tokenData() {}

type NetToken struct {
    Signal string
    Pins   []string  // arrow tokens (=>, <=, <=>) already stripped at parse time
}
func (*NetToken) tokenData() {}

type SetPToken struct {
    Name  string
    Value string  // stays string — C shim interprets based on runtime pin type
}
func (*SetPToken) tokenData() {}

type SetSToken struct {
    Name  string
    Value string
}
func (*SetSToken) tokenData() {}

type GetPToken   struct{ Name string }
func (*GetPToken) tokenData() {}

type GetSToken   struct{ Name string }
func (*GetSToken) tokenData() {}

type AddFToken struct {
    Funct  string
    Thread string
    Pos    int  // parsed from optional 3rd arg; default -1 (append)
}
func (*AddFToken) tokenData() {}

type DelFToken struct {
    Funct  string
    Thread string
}
func (*DelFToken) tokenData() {}

type NewSigToken struct {
    Name    string
    SigType PinType  // parsed from "bit"/"float"/"s32"/"u32" at parse time
}
func (*NewSigToken) tokenData() {}

type DelSigToken struct{ Name string }
func (*DelSigToken) tokenData() {}

type LinkPSToken struct{ Pin, Sig string }
func (*LinkPSToken) tokenData() {}

type LinkSPToken struct{ Sig, Pin string }
func (*LinkSPToken) tokenData() {}

type UnlinkPToken struct{ Pin string }
func (*UnlinkPToken) tokenData() {}

type AliasToken struct {
    Kind  AliasKind  // AliasPin or AliasParam
    Name  string
    Alias string
}
func (*AliasToken) tokenData() {}

type UnAliasToken struct {
    Kind AliasKind
    Name string
}
func (*UnAliasToken) tokenData() {}

type StartToken  struct{}
func (*StartToken) tokenData() {}

type StopToken   struct{}
func (*StopToken) tokenData() {}

type LockToken struct {
    Level LockLevel  // parsed from "none"/"load"/"config"/"tune"/"params"/"run"/"all"
}
func (*LockToken) tokenData() {}

type UnlockToken struct {
    Level LockLevel
}
func (*UnlockToken) tokenData() {}

type UnloadRTToken  struct{ Comp string }
func (*UnloadRTToken) tokenData() {}

type UnloadUSRToken struct{ Comp string }
func (*UnloadUSRToken) tokenData() {}

type UnloadToken    struct{ Comp string }
func (*UnloadToken) tokenData() {}

type WaitUSRToken   struct{ Comp string }
func (*WaitUSRToken) tokenData() {}

type ListToken struct {
    ObjType  HalObjType
    Patterns []string
}
func (*ListToken) tokenData() {}

type ShowToken struct {
    ObjType  HalObjType
    Patterns []string
}
func (*ShowToken) tokenData() {}

type SaveToken struct {
    SaveType SaveType
    File     string  // empty string means stdout
}
func (*SaveToken) tokenData() {}

type StatusToken struct{}
func (*StatusToken) tokenData() {}

type DebugToken struct {
    Level int  // already parsed integer
}
func (*DebugToken) tokenData() {}

type PTypeToken struct{ Name string }
func (*PTypeToken) tokenData() {}

type STypeToken struct{ Name string }
func (*STypeToken) tokenData() {}

// EchoToken and UnEchoToken are kept as no-op tokens for backward compatibility.
type EchoToken   struct{}
func (*EchoToken) tokenData() {}

type UnEchoToken struct{}
func (*UnEchoToken) tokenData() {}

type PrintToken struct {
    Message string  // free-form text, already joined at parse time
}
func (*PrintToken) tokenData() {}
```

Full list of token structs (one per halcmd command): `LoadRTToken`, `LoadUSRToken`,
`NetToken`, `SetPToken`, `SetSToken`, `GetPToken`, `GetSToken`, `AddFToken`,
`DelFToken`, `NewSigToken`, `DelSigToken`, `LinkPSToken`, `LinkSPToken`,
`UnlinkPToken`, `AliasToken`, `UnAliasToken`, `StartToken`, `StopToken`, `LockToken`,
`UnlockToken`, `UnloadRTToken`, `UnloadUSRToken`, `UnloadToken`, `WaitUSRToken`,
`ListToken`, `ShowToken`, `SaveToken`, `StatusToken`, `DebugToken`, `PTypeToken`,
`STypeToken`, `EchoToken`, `UnEchoToken`, `PrintToken`.

`source` is **not** in this list — it is not a token at all.  It is resolved at parse
time by `SingleFileParser` recursion and never passed through to the token lists.

---

### 4e. Fields That Must Stay Strings

Not every field can be converted to a richer type at parse time:

| Field | Reason |
|---|---|
| Pin / signal / component names | User-defined, unbounded set |
| `SetPToken.Value` / `SetSToken.Value` | C shim parses based on runtime pin type |
| `LoadRTToken.Params` values | Module-specific, opaque to the parser |
| `PrintToken.Message` | Free-form text |
| `LoadUSRToken.Prog` / `.Args` | Arbitrary paths and arguments |

---

### 4f. ParseResult and Execution

`ParseResult` holds the three classified token lists produced by `MultiFileParser` (via
`SingleFileParser` per file).

```go
// ParseResult holds the three execution buckets produced by MultiFileParser.
type ParseResult struct {
    LoadRT  []Token  // merged via TwopassCollector, then executed
    LoadUSR []Token  // executed in order after loadrt
    HALCmd  []Token  // executed in order after loadusr
}

// Execute runs all tokens in the correct order:
//   1. Merge LoadRT via TwopassCollector, then execute.
//   2. Execute LoadUSR in order.
//   3. Execute HALCmd in order.
func (r *ParseResult) Execute() error
```

Execution dispatches via a type switch — each case is a one-line call to the
existing `command.go` API:

```go
func executeToken(tok Token) error {
    switch d := tok.Data.(type) {
    case *LoadRTToken:
        return hal.LoadRT(d.Comp, loadRTArgs(d)...)
    case *LoadUSRToken:
        return hal.LoadUSR(loadUSROpts(d), d.Prog, d.Args...)
    case *NetToken:
        return hal.Net(d.Signal, d.Pins...)
    case *SetPToken:
        return hal.SetP(d.Name, d.Value)
    case *SetSToken:
        return hal.SetS(d.Name, d.Value)
    case *AddFToken:
        return hal.AddF(d.Funct, d.Thread, d.Pos)
    case *NewSigToken:
        return hal.NewSig(d.Name, d.SigType)
    case *LockToken:
        return hal.SetLock(int(d.Level))
    case *UnlockToken:
        return hal.SetLock(int(d.Level))
    case *EchoToken, *UnEchoToken:
        return nil  // no-op for backward compatibility
    // ... one case per token type ...
    default:
        return fmt.Errorf("%s:%d: unknown token type %T",
            tok.Location.File, tok.Location.Line, tok.Data)
    }
}
```

`LockToken.Level` and `UnlockToken.Level` are `LockLevel` enum values (Section 4c).
`hal.SetLock(int(d.Level))` passes the enum's integer value directly to the C shim
(`hal_set_lock`), with no intermediate string conversion.  The existing `Lock()` /
`Unlock()` string-accepting functions in `command.go` are kept for callers that use
them from the REST API or interactive mode, but `executeToken` bypasses them.

---

### 4g. Error Reporting

```go
// ParseError is returned by LineParser when a line cannot be parsed.
// It always carries the SourceLoc of the offending line.
type ParseError struct {
    Loc SourceLoc
    Msg string
}
func (e *ParseError) Error() string {
    return fmt.Sprintf("%s:%d: %s", e.Loc.File, e.Loc.Line, e.Msg)
}
```

Every `Token` also carries its `SourceLoc`, so execution errors can report the
original file and line number even after the three lists have been reordered for
twopass merging.

---

## 5. Migration Plan / Phases

### Phase 1 — Complete C Shims and Go Command Functions

**Goal**: Every `do_*_cmd()` function in `halcmd_commands.cc` has a corresponding Go
function in `command.go` backed by a C shim in `cgo.go`.

Work items:
- Add `hal_shim_newsig`, `hal_shim_delsig`, `hal_shim_net`, `hal_shim_linkps`,
  `hal_shim_linksp`, `hal_shim_unlinkp` to `cgo.go`.
- Add `hal_shim_addf`, `hal_shim_delf` to `cgo.go`.
- Add `hal_shim_setp`, `hal_shim_getp`, `hal_shim_sets`, `hal_shim_gets`,
  `hal_shim_ptype`, `hal_shim_stype` (shmem access) to `cgo.go`.
- Add `hal_shim_show_*`, `hal_shim_list_*`, `hal_shim_save` (shmem walks) to `cgo.go`.
- Add `hal_shim_loadrt`, `hal_shim_loadusr`, `hal_shim_unloadrt`, `hal_shim_unloadusr`,
  `hal_shim_waitusr` (process management) to `cgo.go`.
- Add `hal_shim_alias`, `hal_shim_unalias`, `hal_shim_lock`, `hal_shim_unlock`,
  `hal_shim_debug` to `cgo.go`.
- Add all corresponding exported Go functions to `command.go`.
- Add unit tests for function signatures in `command_test.go` (following the existing
  `TestStartThreadsSignature` pattern).

**Deliverable**: Any Go code can call individual HAL commands without spawning halcmd.

### Phase 2 — Build the Typed-Token Parser and Executor

**Goal**: Implement the three-tier parse-then-execute pipeline described in Section 4.
New HAL files are parsed into typed token lists and executed without any line-by-line
interpreter state machine or runtime string dispatch.

Work items:
- Implement `token.go` — all token data structs (`LoadRTToken`, `NetToken`, etc.),
  enums (`AliasKind`, `LockLevel`, `HalObjType`, `SaveType`), the `TokenData` sealed
  interface, `Token`, `SourceLoc`, `ParseError`, `INILookup`, and `PathResolver`.
- Implement `parser.go` — `tokenizeLine()` (quote/escape/continuation), per-command
  parse functions (string → enum conversion, typed field population), `LineParser`,
  `SingleFileParser` (template rendering + INI/`$ENV` substitution + token
  classification + source recursion with depth limit of 20 via `resolver.Resolve()`),
  and `MultiFileParser` (trivial multi-file loop that merges `ParseResult` objects).
- Implement `executor.go` — `ParseResult.Execute()` with a type-switch dispatch to
  the `command.go` API; `loadrt` merging via the existing `TwopassCollector`;
  `LockToken` / `UnlockToken` executed via `hal.SetLock(int(d.Level))`.
- Implement `parser_test.go` — table-driven tests for `tokenizeLine` edge cases,
  per-command parse functions (including enum validation and invalid-input rejection),
  `SingleFileParser` (template expansion, `source` recursion, token classification),
  and `MultiFileParser` (correct merging of `loadrt`/`loadusr`/other tokens across
  files).  All tests must be pure Go with **no CGO dependency** (mock or stub the
  `command.go` layer).
- Update `twopass.go` — add an adapter so `TwopassCollector` can accept `LoadRTToken`
  structs directly in addition to the existing string-slice interface, or expose the
  merge logic as a function that `ParseResult.Execute()` can call.

**Deliverable**: `new(MultiFileParser).Parse(halFiles).Execute()` processes a set of
`[HAL]HALFILE` entries identically to the old `halcmd -f` pipeline, with all parsing
errors caught before any HAL state is modified.

### Phase 3 — Replace `halcmd -f` in the Halfile Executor ✅ COMPLETE

**Goal**: `src/launcher/halfile/halfile.go` stops forking `halcmd`.

**Status**: Completed. TCL support has been cancelled entirely.

Work items completed:
- Replaced `runHalcmdFile(path)` with `hal.NewMultiFileParser(ini, resolver).Parse(paths).Execute()`.
- Removed `substitute.go` — INI/`$ENV` substitution is now handled by `SingleFileParser`.
- Replaced `runHalcmd(cmd)` / `RunHalcmdArgs(args)` with `SingleFileParser.ParseContent()` +
  `ParseResult.Execute()` for `[HAL]HALCMD` lines.
- Removed `runHaltcl()`, `executeTwopass()`, `findTwopassTcl()` — **TCL support cancelled**.
  `.tcl` HALFILE entries now return a hard error.
- Removed `halcmdPath` from `halfile.Executor` struct and `New()` signature.
- Removed `twopass.go` from `src/launcher/halfile/` (TCL twopass delegation deleted).
- Updated halfile tests to use the Go path.

**Deliverable**: No `halcmd` subprocess is spawned during normal machine startup.

### Phase 4 — Remove halcmd Binary Dependency from the Launcher ✅ MOSTLY COMPLETE

**Goal**: The Go launcher never calls halcmd as a subprocess.

Work items completed:
- Replaced `exec.Command(halcmdPath, "loadrt", "threads", ...)` in `loadThreads()` with
  `hal.LoadRT("threads", ...)`.
- Replaced `exec.Command(halcmdPath, "loadusr", "-Wn", "iocontrol", ...)` in
  `startIOControl()` with `hal.LoadUSR(&hal.LoadUSROptions{WaitReady: true, WaitName: "iocontrol"}, ...)`.
- Replaced `exec.Command(halcmdPath, "loadusr", "-Wn", "halui", ...)` in `startHalUI()`
  with `hal.LoadUSR(...)`.
- Replaced both `exec.Command(halcmdPath, "loadrt", tpMod)` and `exec.Command(halcmdPath,
  "loadrt", homeMod)` in `preloadMotionModules()` with `hal.LoadRT(tpMod)` /
  `hal.LoadRT(homeMod)`.
- Replaced `exec.Command(halcmdPath, "loadusr", "-Wn", "inihal", emctask, ...)` in
  `startTask()` with `hal.LoadUSR(&hal.LoadUSROptions{WaitReady: true, WaitName: "inihal"}, ...)`.
  Updated `stopTask()` to use `hal.UnloadUSR("inihal")` + `hal.WaitUSR("inihal")`.
- Replaced `exec.Command(halcmdPath, "list", "retain")` in `loadRetain()` with
  `hal.List("retain")`; replaced `halExec.RunHalcmdArgs(["loadrt", "retain"])` with
  `hal.LoadRT("retain")`; replaced `halExec.RunHalcmdArgs(["addf", ...])` with
  `hal.AddF(...)`; replaced `halExec.RunHalcmdArgs(["loadusr", ...])` with
  `hal.LoadUSR(...)`.

Remaining item:
- `[HAL]SHUTDOWN` script in cleanup.go still uses `exec.Command(halcmdPath, "-f", shutdown)`.
  This will be replaced when the `[HAL]SHUTDOWN` script execution is migrated to the Go
  interpreter in a future phase.

**Deliverable**: A machine can start and stop without the `halcmd` binary present (except
for `[HAL]SHUTDOWN` script execution).

### Phase 5 — REST API Using Go Command Functions Directly

**Goal**: The REST API (or equivalent Go service) calls Layer 2 functions directly
rather than shelling out to halcmd.

Work items:
- Expose `Show()`, `List()`, `SetP()`, `GetP()`, `Net()`, `LoadRT()`, `LoadUSR()` etc.
  as HTTP handlers (JSON in/out).
- The structured return types (`PinInfo`, `SigInfo`, `ShowResult`, etc.) serialize to
  JSON without any formatting adapter.
- Authentication and authorization are out of scope for this document.

**Deliverable**: A REST-capable HAL endpoint with no halcmd dependency.

---

## 6. TCL Support — CANCELLED

**TCL support has been completely removed.** The following applies:

- `.tcl` HALFILE entries cause a hard error: `SingleFileParser.Parse` returns an error
  for any file ending in `.tcl`, and `halfile.Executor.ExecuteAll()` propagates this as a
  fatal error.
- `runHaltcl()`, `executeTwopass()`, `findTwopassTcl()` have been deleted from
  `src/launcher/halfile/`.
- `twopass.go` in `src/launcher/halfile/` has been deleted.
- `LINUXCNC_TCL_DIR` environment variable is no longer set by the launcher.
- TCL display dispatching (`tklinuxcnc`, `mini`) has been removed from `startDisplay()`.
  These display programs can still be launched as regular executables via the `default`
  case (they must be on PATH or specified with an absolute path in the INI).
- The Go `text/template` engine (see Section 6a) provides flow control and
  parameterization for HAL files, replacing what TCL was used for in `.tcl` HALFILE
  entries.

---

## 6a. Go Template Engine

The Go pipeline supports `.hal` files that use Go `text/template` syntax for
parameterized configuration. This replaces the flow-control that was previously
provided by TCL (`.tcl`) HAL files. This is **not** a general-purpose scripting
language — it is a parameterized configuration template engine.

The template is rendered **inside `SingleFileParser`**, before `LineParser` sees any
line.  The per-file pipeline is:

```
.hal file (raw text with {{...}} directives)
  → text/template.Execute() with INI data context   [SingleFileParser step 2]
  → rendered plain HAL commands
  → LineParser produces typed tokens                [SingleFileParser step 5]
```

### Template data context

The following data context is provided to every template:

```go
type HalTemplateData struct {
    INI    map[string]map[string]string  // INI[SECTION][KEY] → value
    Axes   []string                      // from [TRAJ]COORDINATES, split into letters
    Joints int                           // from [KINS]JOINTS
    Env    map[string]string             // environment variables
}
```

Templates are expanded **once** before any `loadrt` is executed. All template
conditionals must therefore be resolvable from INI data alone — no HAL runtime
probing is possible or needed.

### Built-in template functions

| Category    | Functions |
|-------------|-----------|
| String      | `lower`, `upper`, `replace`, `contains`, `split`, `join`, `printf`, `trim` |
| Math        | `add`, `sub`, `mul`, `div`, `neg` |
| Iteration   | `seq` (start, end → []int), `count` (n → []int) |
| INI access  | `ini` (section, key → string) |
| Environment | `env` (name → string) |
| Conversion  | `atoi`, `atof`, `itoa` |
| Range checks | `hasJoint` (n → bool), `hasAxis` (letter → bool) |

#### `hasJoint(n int) bool`

Returns `true` if joint `n` is within the configured range: `0 <= n < .Joints`
(from `[KINS]JOINTS`). Use this instead of runtime `pinExists` checks on
`joint.N.*` pins.

#### `hasAxis(letter string) bool`

Returns `true` if the given axis letter (case-insensitive) appears in `.Axes`
(from `[TRAJ]COORDINATES`). Use this instead of runtime `pinExists` checks on
axis pins.

### Example: Go template for parameterized HAL configuration

Before (TCL `extrajoints.tcl`, uses runtime `getp` probing — **no longer supported**):

```tcl
for {set j 0} {$j <= 15} {incr j} {
    if [catch {getp joint.${j}.posthome-cmd} msg] { continue }
    loadrt limit3 names=j${j}.limit3
    addf  j${j}.limit3 servo-thread
    ...
}
```

Go template equivalent (INI-derived range check, no runtime probing):

```
{{- range $j := seq 0 16}}
{{- if hasJoint $j}}
loadrt limit3 names=j{{$j}}.limit3
addf j{{$j}}.limit3 servo-thread
setp j{{$j}}.limit3.min  {{ini (printf "JOINT_%d" $j) "MIN_LIMIT"}}
...
{{- end}}
{{- end}}
```

The template expands **once** to produce pure halcmd text. `SingleFileParser` then
classifies the resulting tokens into `loadrt`, `loadusr`, and `halcmd` lists, and
`MultiFileParser` merges the results across files for ordered execution.

### Detection

If a `.hal` file contains `{{` anywhere, it is rendered through `text/template` first.
Otherwise it is passed directly to LineParser.

### Implementation

Implemented in `template.go` (`RenderHalTemplate`, `NewHalTemplateData`,
`halTemplateFuncs`). Tests in `template_test.go`. Both files are pure Go with no CGO
dependencies.

---

## 6b. Native Twopass Support

Twopass is now **inherent in the three-tier architecture** — no separate two-pass
bookkeeping is needed.

### How twopass falls out of the architecture

`SingleFileParser` classifies every token into one of three lists during its parse
pass, and `MultiFileParser` merges results across all `[HAL]HALFILE` entries:

- **`LoadRT`** — all `loadrt` tokens, regardless of which file they came from.
- **`LoadUSR`** — `loadusr` tokens with `-W` or `-Wn` flags (wait for component).
- **`HALCmd`** — everything else.

`ParseResult.Execute()` then:

1. Passes `LoadRT` tokens through the existing `TwopassCollector` to merge duplicates
   (combining `count=` and `names=` parameters across files), then executes the merged
   set.
2. Executes `LoadUSR` tokens in order (after all RT components are loaded).
3. Executes `HALCmd` tokens in order (after all components have started).

This replaces the two-pass algorithm described below (which is no longer needed as a
separate concept) while preserving identical execution semantics.

### Merging logic for `loadrt` (unchanged, handled by TwopassCollector)

| Parameter   | Merge strategy |
|-------------|---------------|
| `count=N`   | Take the maximum |
| `names=a,b` | Concatenate unique names |
| `num_chan=N` | Take the maximum |
| Other       | First-wins |

### Implementation

The `TwopassCollector` in `twopass.go` is updated to accept `LoadRTToken` structs
directly (or via an adapter) in addition to the existing string-slice interface.
`ParseResult.Execute()` in `executor.go` drives the merge + execution sequence.

This replaces the ~600 lines of TCL in `twopass.tcl` with a small amount of Go that
is a natural consequence of the three-list classification in `SingleFileParser` and the
merge loop in `MultiFileParser`.

---

## 7. Compatibility Notes

### Behavioral Compatibility

The Go parser and executor **must** produce identical HAL state to halcmd for all
supported commands. This means:

- `net` must create the signal with the same type inference rules.
- `setp` must accept the same value formats (including scientific notation for floats,
  `TRUE`/`FALSE`/`true`/`false`/`1`/`0` for bits).
- `addf` position argument (`-1` = append to end) must be handled identically.
- Arrow tokens (`=>`, `<=`, `<=>`) in `net` arguments are stripped at parse time inside
  `LineParser` (in `NetToken.Pins`) before `Net()` is ever called.
- `source` path resolution is delegated to the `PathResolver` interface provided by the
  caller.  The launcher's implementation in `src/launcher/halfile/resolve.go` handles
  the `LIB:` prefix and `HALLIB_PATH` environment variable.  `SingleFileParser` calls
  `resolver.Resolve(path)` before recursing and has no knowledge of the resolution
  rules itself.

### Error Message Format

Error messages **do not** need to be byte-identical to halcmd's output. They should be
descriptive and include the filename and line number. The `ParseError` type (Section 4g)
serves this purpose for parse-time errors; execution errors wrap the underlying
`command.go` error with the originating `Token.Location`.

### `halcmd` Binary Preservation

The `halcmd` binary is not deleted or deprecated. It continues to be:

- Built by the LinuxCNC build system.
- Available for manual use (`halcmd show all`, `halrun`, interactive `halcmd` session).
- Used by third-party scripts and integrations that invoke it directly.
- The reference implementation against which the Go parser/executor is tested.

### `show`/`list`/`save` Output Format

The C halcmd formats `show`, `list`, and `save` output as human-readable text printed
to stdout. The Go command functions (`Show()`, `List()`, `Save()`) return **structured
data** (slices of `*Info` structs or `[]string`). When `executeToken` processes a
`ShowToken` or `ListToken` at startup (i.e. output is a terminal), it formats the data
identically to halcmd's text output. The REST API uses the structured form directly.
Scripts that currently parse `halcmd show` output are unaffected as long as they use
the `halcmd` binary; scripts migrated to use the REST API should use the JSON form.

---

## Appendix A: halcmd Command Reference Table

Complete mapping of halcmd commands to their halcmd_commands.cc implementations,
proposed Go function names, and shim categories.

| halcmd command | C function | Go function | Shim category |
|---|---|---|---|
| `addf <funct> <thread> [pos]` | `do_addf_cmd` | `AddF(funct, thread, pos)` | Simple wrapper |
| `alias pin\|param <name> <alias>` | `do_alias_cmd` | `Alias(kind, name, alias)` | Simple wrapper |
| `delf <funct> <thread>` | `do_delf_cmd` | `DelF(funct, thread)` | Simple wrapper |
| `delsig <sig>` | `do_delsig_cmd` | `DelSig(name)` | Simple wrapper |
| `getp <name>` | `do_getp_cmd` | `GetP(name)` | Shmem access |
| `gets <sig>` | `do_gets_cmd` | `GetS(name)` | Shmem access |
| `linkpp <pin1> <pin2>` | `do_linkpp_cmd` | `LinkPP(pin1, pin2)` | Multi-call |
| `linkps <pin> <sig>` | `do_linkps_cmd` | `LinkPS(pin, sig)` | Simple wrapper |
| `linksp <sig> <pin>` | `do_linksp_cmd` | `LinkSP(sig, pin)` | Simple wrapper |
| `list <type> [patterns]` | `do_list_cmd` | `List(halType, patterns...)` | Shmem access |
| `loadrt <mod> [args]` | `do_loadrt_cmd` | `LoadRT(mod, args...)` | Process mgmt |
| `loadusr [opts] <prog> [args]` | `do_loadusr_cmd` | `LoadUSR(opts, prog, args...)` | Process mgmt |
| `lock [level]` | `do_lock_cmd` | `Lock(level)` | Simple wrapper |
| `net <sig> <pins...>` | `do_net_cmd` | `Net(sig, pins...)` | Multi-call |
| `newsig <name> <type>` | `do_newsig_cmd` | `NewSig(name, halType)` | Simple wrapper |
| `ptype <name>` | `do_ptype_cmd` | `PType(name)` | Shmem access |
| `save [type] [file]` | `do_save_cmd` | `Save(halType, file)` | Shmem access |
| `setp <name> <value>` | `do_setp_cmd` | `SetP(name, value)` | Shmem access |
| `sets <sig> <value>` | `do_sets_cmd` | `SetS(name, value)` | Shmem access |
| `show [type] [patterns]` | `do_show_cmd` | `Show(halType, patterns...)` | Shmem access |
| `source <file>` | `do_source_cmd` | resolved by SingleFileParser | n/a |
| `start` | `do_start_cmd` | `StartThreads()` | Simple wrapper ✅ |
| `status [type]` | `do_status_cmd` | `Status()` | Shmem access |
| `stop` | `do_stop_cmd` | `StopThreads()` | Simple wrapper ✅ |
| `stype <sig>` | `do_stype_cmd` | `SType(name)` | Shmem access |
| `unalias pin\|param <name>` | `do_unalias_cmd` | `UnAlias(kind, name)` | Simple wrapper |
| `unlink <pin>` | `do_unlinkp_cmd` | `UnlinkP(pin)` | Simple wrapper |
| `unlinkp <pin>` | `do_unlinkp_cmd` | `UnlinkP(pin)` | Simple wrapper |
| `unload <comp>` | `do_unload_cmd` | `Unload(comp)` | Process mgmt |
| `unloadrt <mod>` | `do_unloadrt_cmd` | `UnloadRT(mod)` | Process mgmt |
| `unloadusr <comp>` | `do_unloadusr_cmd` | `UnloadUSR(comp)` | Process mgmt |
| `unlock [level]` | `do_unlock_cmd` | `Unlock(level)` | Simple wrapper |
| `waitusr <comp>` | `do_waitusr_cmd` | `WaitUSR(comp)` | Multi-call |
| `debug <level>` | `do_set_debug_cmd` | `SetDebug(level)` | Simple wrapper |
| `print <msg>` | `do_print_cmd` | `PrintToken` (no-op at startup) | n/a |
| `echo` / `unecho` | `do_echo_cmd` | `EchoToken`/`UnEchoToken` (no-op) | n/a |
| `list comp` | (print_comp_names) | `ListComponents()` | Shmem access ✅ |
| `unload all` | (unload all comps) | `UnloadAll(exceptID)` | Process mgmt ✅ |

✅ = already implemented.
