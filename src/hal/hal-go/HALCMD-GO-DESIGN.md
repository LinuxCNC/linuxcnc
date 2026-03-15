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

### Layer 3 — Go Interpreter (`interpreter.go`, new file)

A line-oriented parser and file executor. It:

- Tokenizes each line (quotes, backslash escapes, comment stripping, line continuation).
- Substitutes `[SECTION]VAR` from INI and `$ENV` from environment.
- Dispatches token[0] as a command name to Layer 2 functions.
- Handles flow control: `if`/`elif`/`else`/`endif`, `while`/`endwhile`.
- Handles `source` (recursive file inclusion).
- Strips `=>`, `<=`, `<=>` arrow tokens from `net` argument lists.
- Tracks filename and line number for error messages.

Replaces the `halcmd -f` subprocess call in `src/launcher/halfile/halfile.go`.

### Layer Dependencies

```
┌──────────────────────────────────────────────┐
│  REST API / Launcher / Other Go callers       │
└──────────────────────────┬───────────────────┘
                           │  calls
┌──────────────────────────▼───────────────────┐
│  Layer 3: Go Interpreter  (interpreter.go)    │
│  tokenize → substitute → dispatch             │
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

## 4. Interpreter Design

The interpreter (new file `interpreter.go`, package `hal`) replaces `halcmd -f`.

### 4a. Tokenizer

The tokenizer processes a single input line into a slice of tokens. Rules (matching
halcmd's C tokenizer exactly):

1. Strip trailing `\r` (DOS line ending).
2. Strip comments: `#` not inside a string is a line comment; everything from `#` to
   EOL is discarded. A `\#` sequence is an escaped `#` that becomes a literal `#`.
3. Split on whitespace. Tokens are separated by one or more spaces or tabs.
4. **Single-quoted strings** `'...'`: no escape processing inside; the quotes are removed.
5. **Double-quoted strings** `"..."`: backslash escapes `\\`, `\"` processed; the quotes
   are removed.
6. **Backslash continuation**: a `\` as the last non-whitespace character on the line
   means the next line is a continuation (lines are joined before tokenizing).
7. The resulting token slice includes the command name as token[0].

```go
// tokenizeLine splits a (possibly multi-line-continued) line into tokens.
// It handles single/double quotes and backslash escapes.
// Returns (tokens, error).
func tokenizeLine(line string) ([]string, error)
```

### 4b. Variable Substitution

After tokenization (or before, as a pre-pass on the raw line — same as halcmd):

1. **INI substitution** — `[SECTION]VAR` is replaced with the value from the loaded INI
   file. The halfile executor (`src/launcher/halfile/substitute.go`) already implements
   `substituteLine()`. The interpreter should either reuse that function or incorporate
   the same logic.
2. **Environment substitution** — `$VARNAME` is replaced with `os.Getenv("VARNAME")`.
   Unset variables expand to empty string (same as halcmd).

Both substitutions operate on the raw line *before* tokenization, so they can span
inside quoted strings. This matches halcmd's `replace_vars()` order.

```go
// substituteLine expands [SECTION]VAR and $ENV references in a raw line.
// ini may be nil if no INI file is loaded.
func substituteLine(line string, ini IniReader) string
```

### 4c. Command Dispatch

After tokenizing and substituting, the interpreter looks up `tokens[0]` in a command
table and validates the argument count before calling the Layer 2 Go function.

```go
// cmdEntry describes a single halcmd command in the dispatch table.
type cmdEntry struct {
    name    string
    minArgs int       // minimum additional args after command name
    maxArgs int       // -1 = unlimited
    fn      func(args []string) error
}

// commandTable is sorted alphabetically for binary search (matching halcmd).
var commandTable = []cmdEntry{
    {"addf",     2, 3,  dispatchAddF},
    {"alias",    3, 3,  dispatchAlias},
    {"delf",     1, 2,  dispatchDelF},
    {"delsig",   1, 1,  dispatchDelSig},
    {"getp",     1, 1,  dispatchGetP},
    {"gets",     1, 1,  dispatchGetS},
    {"linkpp",   2, 2,  dispatchLinkPP},
    {"linkps",   2, 2,  dispatchLinkPS},
    {"linksp",   2, 2,  dispatchLinkSP},
    {"list",     1, -1, dispatchList},
    {"loadrt",   1, -1, dispatchLoadRT},
    {"loadusr",  1, -1, dispatchLoadUSR},
    {"lock",     0, 1,  dispatchLock},
    {"net",      2, -1, dispatchNet},
    {"newsig",   2, 2,  dispatchNewSig},
    {"ptype",    1, 1,  dispatchPType},
    {"save",     0, 2,  dispatchSave},
    {"setp",     2, 2,  dispatchSetP},
    {"sets",     2, 2,  dispatchSetS},
    {"show",     0, -1, dispatchShow},
    {"source",   1, 1,  dispatchSource},
    {"start",    0, 0,  dispatchStart},
    {"status",   0, 1,  dispatchStatus},
    {"stop",     0, 0,  dispatchStop},
    {"stype",    1, 1,  dispatchSType},
    {"unalias",  2, 2,  dispatchUnAlias},
    {"unlink",   1, 1,  dispatchUnlinkP},   // "unlinkp" alias
    {"unlinkp",  1, 1,  dispatchUnlinkP},
    {"unload",   1, 1,  dispatchUnload},
    {"unloadrt", 1, 1,  dispatchUnloadRT},
    {"unloadusr",1, 1,  dispatchUnloadUSR},
    {"unlock",   0, 1,  dispatchUnlock},
    {"waitusr",  1, 1,  dispatchWaitUSR},
    // interpreter-only commands (no Layer 2 function):
    {"echo",     0, 0,  dispatchEcho},
    {"print",    0, -1, dispatchPrint},
    {"unecho",   0, 0,  dispatchUnecho},
    // flow control is handled before dispatch:
    // "if", "elif", "else", "endif", "while", "endwhile"
}
```

### 4d. Arrow Removal for `net`

The `net` command ignores `=>`, `<=`, and `<=>` tokens in the pin list. These are
stripped before calling `Net()`:

```go
// removeArrows filters arrow tokens from a net argument list.
func removeArrows(pins []string) []string {
    out := pins[:0]
    for _, p := range pins {
        if p != "=>" && p != "<=" && p != "<=>" {
            out = append(out, p)
        }
    }
    return out
}
```

### 4e. Flow Control

The interpreter maintains a **condition stack** — a slice of `condState` values. Each
`if` or `while` pushes a new state onto the stack; `endif`/`endwhile` pops it.

```go
type condKind int
const (
    condIf    condKind = iota
    condWhile
)

type condState struct {
    kind      condKind
    active    bool   // true if we are currently executing inside this block
    seenElse  bool   // true after "else" (for if/elif)
    startLine int    // line number where "while" started (for looping)
    startFile string // filename for "while" (supports source'd files)
}

// Interpreter holds the interpreter state across a file execution.
type Interpreter struct {
    ini        IniReader        // INI file for variable substitution
    condStack  []condState      // flow control stack
    output     io.Writer        // for "print" and show/list output (defaults to os.Stdout)
    errWriter  io.Writer        // for error messages (defaults to os.Stderr)
}

// NewInterpreter creates an Interpreter with the given INI reader.
func NewInterpreter(ini IniReader) *Interpreter

// ExecuteFile parses and executes a HAL file.
func (interp *Interpreter) ExecuteFile(path string) error

// ExecuteLine parses and executes a single line.
// filename and lineNo are used for error messages.
func (interp *Interpreter) ExecuteLine(line, filename string, lineNo int) error
```

Flow control logic overview:

```
For each line:
  1. Check condStack.active — if false and not a control keyword, skip the line.
  2. If token[0] == "if":     evaluate condition; push condIf state
     If token[0] == "elif":   check seenElse; pop/re-evaluate condition
     If token[0] == "else":   flip active if top-of-stack is condIf and !seenElse
     If token[0] == "endif":  pop condIf state
     If token[0] == "while":  evaluate condition; if false skip to "endwhile"
     If token[0] == "endwhile": re-evaluate while condition; loop or pop
  3. Otherwise: dispatch to command table.
```

Conditions are evaluated by comparing two halcmd-style expressions. Supported
operators (matching halcmd): `=`, `!=`, `<`, `>`, `<=`, `>=`. Operands are strings;
numeric comparison is used when both sides are valid numbers.

### 4f. `source` — Recursive File Inclusion

```go
// dispatchSource handles the "source" command.
// It calls ExecuteFile recursively with a new line counter.
func (interp *Interpreter) dispatchSource(args []string) error {
    return interp.ExecuteFile(args[0])
}
```

File inclusion nesting depth should be capped (e.g. 20 levels) to prevent infinite loops.

### 4g. `print`, `echo`, and `unecho`

These three commands are handled entirely within the interpreter and have no
corresponding Layer 2 Go function.

**`print <message>`** writes the joined remaining tokens to the interpreter's output
writer (defaulting to `os.Stdout`), followed by a newline. It is always executed
regardless of `echo` state and regardless of flow-control active/inactive state (i.e.
a `print` inside an inactive `if` block is still skipped; the `echo` setting only
affects command echoing, not `print` itself).

**`echo`** sets an interpreter flag `echoMode = true`. When echo mode is active, every
command line is printed to the output writer before it is executed (matching halcmd's
`-e` flag behaviour). The command table entry for `echo` sets this flag directly instead
of calling any Layer 2 function.

**`unecho`** sets `echoMode = false`, disabling line echoing.

```go
// echo state is stored on the Interpreter struct:
type Interpreter struct {
    // ... existing fields ...
    echoMode   bool   // true when "echo" command has been issued
}

// dispatchEcho enables command echoing.
func (interp *Interpreter) dispatchEcho(_ []string) error {
    interp.echoMode = true
    return nil
}

// dispatchUnecho disables command echoing.
func (interp *Interpreter) dispatchUnecho(_ []string) error {
    interp.echoMode = false
    return nil
}

// dispatchPrint writes its arguments joined by spaces to interp.output.
func (interp *Interpreter) dispatchPrint(args []string) error {
    fmt.Fprintln(interp.output, strings.Join(args, " "))
    return nil
}
```

### 4h. Error Reporting

Each error returned by the interpreter includes the filename and line number:

```go
type InterpError struct {
    File    string
    Line    int
    Command string
    Cause   error
}
func (e *InterpError) Error() string {
    return fmt.Sprintf("%s:%d: %s: %v", e.File, e.Line, e.Command, e.Cause)
}
func (e *InterpError) Unwrap() error { return e.Cause }
```

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

### Phase 2 — Build the Go Interpreter

**Goal**: `interpreter.go` provides a complete halcmd-compatible line interpreter.

Work items:
- Implement `tokenizeLine()` with full quote/escape/continuation support.
- Implement `substituteLine()` (may reuse/adapt `src/launcher/halfile/substitute.go`).
- Implement `commandTable` and dispatch loop.
- Implement `removeArrows()` for `net`.
- Implement flow control (`if`/`elif`/`else`/`endif`, `while`/`endwhile`).
- Implement `source` recursion with depth limit.
- Implement `InterpError` with file/line context.
- Write table-driven tests covering tokenization edge cases, substitution, flow control,
  and command dispatch (using mock command functions to avoid needing a live HAL).
- Test against the existing HAL file corpus in `src/configs/` (integration tests that
  run the Go interpreter and the C halcmd on the same file and compare results).

**Deliverable**: `hal.NewInterpreter(ini).ExecuteFile("foo.hal")` works identically to
`halcmd -i foo.ini -f foo.hal`.

### Phase 3 — Replace `halcmd -f` in the Halfile Executor

**Goal**: `src/launcher/halfile/halfile.go` stops forking `halcmd`.

Work items:
- Replace `runHalcmdFile(path)` with a call to `hal.NewInterpreter(ini).ExecuteFile(path)`.
- The existing `substituteLine()` in `substitute.go` can be removed once the interpreter
  takes over INI substitution, or retained as a shared utility.
- `runHalcmd(cmd)` (single command execution) can be replaced with
  `hal.NewInterpreter(ini).ExecuteLine(cmd, "<halcmd>", 0)`.
- `RunHalcmdArgs(args)` is used by the launcher for `[HAL]HALCMD` lines; replace with
  the interpreter's `ExecuteLine`.
- Keep `runHaltcl(path, args)` unchanged (TCL files still go to `haltcl` subprocess —
  see Section 6).
- Update the halfile tests to use the Go path.

**Deliverable**: No `halcmd` subprocess is spawned during normal machine startup.

### Phase 4 — Remove halcmd Binary Dependency from the Launcher

**Goal**: The Go launcher never calls halcmd as a subprocess.

Work items:
- Audit all remaining halcmd subprocess calls in the launcher package:
  - `[HAL]SHUTDOWN` script: if it is a `.hal` file, use the Go interpreter; if it is a
    shell script, continue using `exec.Command`.
  - `halrun` is a separate tool and is out of scope.
- Verify that the `halcmd` binary is no longer listed as a required runtime dependency
  of the launcher binary (update packaging metadata if needed).
- The `halcmd` binary continues to be built and installed as a standalone tool.

**Deliverable**: A machine can start and stop without the `halcmd` binary present.

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

## 6. TCL / Twopass Considerations

### haltcl

HAL files with a `.tcl` extension are executed by `haltcl` (a TCL interpreter with HAL
bindings), not by halcmd. The halfile executor's `runHaltcl()` dispatches to `haltcl`
as a subprocess and this must remain unchanged. The Go interpreter does **not** attempt
to interpret TCL syntax.

Detection: if a HAL file path ends with `.tcl`, `ExecuteFile` returns an error
recommending the caller use `haltcl`; the halfile executor handles this routing before
calling `ExecuteFile`.

### TWOPASS

When `[HAL]TWOPASS` is set in the INI file, `twopass.tcl` is invoked by
`src/launcher/halfile/twopass.go`. Twopass performs two-pass analysis of HAL component
instantiation and cannot be replicated in Go without rewriting significant TCL logic.

`twopass.go` continues to delegate to `twopass.tcl` as a subprocess. The Go interpreter
is not involved in the twopass path.

---

## 7. Compatibility Notes

### Behavioral Compatibility

The Go interpreter **must** produce identical HAL state to halcmd for all supported
commands. This means:

- `net` must create the signal with the same type inference rules.
- `setp` must accept the same value formats (including scientific notation for floats,
  `TRUE`/`FALSE`/`true`/`false`/`1`/`0` for bits).
- `addf` position argument (`-1` = append to end) must be handled identically.
- Arrow tokens (`=>`, `<=`, `<=>`) in `net` arguments must be silently ignored.
- `source` must follow the same path resolution rules as halcmd (`LIB:` prefix,
  `HALLIB_PATH` environment variable — see `src/launcher/halfile/resolve.go`).

### Error Message Format

Error messages **do not** need to be byte-identical to halcmd's output. They should be
descriptive and include the filename and line number. The `InterpError` type serves this
purpose.

### `halcmd` Binary Preservation

The `halcmd` binary is not deleted or deprecated. It continues to be:

- Built by the LinuxCNC build system.
- Available for manual use (`halcmd show all`, `halrun`, interactive `halcmd` session).
- Used by third-party scripts and integrations that invoke it directly.
- The reference implementation against which the Go interpreter is tested.

### `show`/`list`/`save` Output Format

The C halcmd formats `show`, `list`, and `save` output as human-readable text printed
to stdout. The Go command functions (`Show()`, `List()`, `Save()`) return **structured
data** (slices of `*Info` structs or `[]string`). When the Go interpreter executes a
`show` or `list` command interactively (i.e. output is a terminal), it should format
the data identically to halcmd's text output. The REST API uses the structured form
directly. Scripts that currently parse `halcmd show` output are unaffected as long as
they use the `halcmd` binary; scripts migrated to use the REST API should use the JSON
form.

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
| `source <file>` | `do_source_cmd` | handled by interpreter | n/a |
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
| `print <msg>` | `do_print_cmd` | handled by interpreter | n/a |
| `echo` / `unecho` | `do_echo_cmd` | interpreter state | n/a |
| `list comp` | (print_comp_names) | `ListComponents()` | Shmem access ✅ |
| `unload all` | (unload all comps) | `UnloadAll(exceptID)` | Process mgmt ✅ |

✅ = already implemented.
