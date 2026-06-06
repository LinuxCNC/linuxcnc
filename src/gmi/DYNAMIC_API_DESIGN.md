# Dynamic API Design

This document describes the dynamic inter-module communication system for LinuxCNC,
intended to replace NML with a modern, type-safe approach.

## Current Status (May 2026)

| Step | Status | Tests |
|------|--------|-------|
| 0: Core Module Migration | ✅ Complete | — |
| 1: apiserver Package | ✅ Complete | 37 |
| 2: `--server-go` | ✅ Complete | 3 |
| 3: `--server-c` + cgo | ✅ Complete | 5 |
| 4: Client Generation | ✅ Complete | 14 |
| 4.5: halcmd REST Tool | ✅ Complete | — |
| 5: Python Client | ✅ Complete | 3 |
| 5.1: Manualtoolchange REST | ✅ Complete | — |
| 5.2: AXIS UI Watch Channel | ✅ Complete | 2 |
| 5.3: PyVCP REST/WebSocket | ✅ Complete | — |
| 5.4: INI REST Migration | ✅ Complete | 6 |
| 5.5: NML Gateway (stat/cmd/error) | ✅ Complete | — |
| 5.6: TypeScript Client Generation | ✅ Complete | — |
| 5.7: Web App Infrastructure | ✅ Complete | — |
| 5.8: Halscope (gomod + Vue Web UI) | ✅ Complete | — |
| 5.9: Halshow (Vue Web UI, uses halcmd API) | ✅ Complete | — |
| 5.10: Emccalib (gomod + Vue Web UI) | ✅ Complete | — |
| 6: Polish | ❌ Not Started | — |
| 7: Remove Go Plugins | ✅ Complete | — |

**Total: 81 tests passing**

**Inter-module call patterns tested:**
- cmod→cmod ✅ (directtest)
- gomod→cmod ✅ (directtest)  
- gomod→gomod ✅ (gomodtest)
- cmod→gomod ✅ (cmodtogomod)

## Overview

The system enables modules (cmod/gomod) to:
1. Register API implementations (expose functionality)
2. Lookup and call APIs (consume functionality)
3. Optionally expose APIs via REST for external clients

All module code works with native structs (C or Go). JSON/REST handling is
centralized in the gomc-server binary and invisible to modules.

## Architecture

```
                     External REST Client
                            │
                            │ JSON/HTTP (localhost:port)
                            ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                         GOMC-SERVER (Go)                                 │
│                                                                          │
│  ┌────────────────────────────────────────────────────────────────────┐  │
│  │                      HTTP Server Layer                             │  │
│  │  - Binds to localhost only (no auth initially)                     │  │
│  │  - JSON ↔ Go struct marshaling (ONLY external boundary)            │  │
│  │  - Started after HAL setup, stopped after UI shutdown              │  │
│  └────────────────────────────────────────────────────────────────────┘  │
│                                    │                                     │
│                                    │ Go struct                           │
│                                    ▼                                     │
│  ┌────────────────────────────────────────────────────────────────────┐  │
│  │                      Dispatcher / Registry                         │  │
│  │  - Stores APIMeta (from IDL) per instance                          │  │
│  │  - Holds opaque Callbacks pointer (Go interface or C struct ptr)   │  │
│  │  - HTTP server matches path → funcIndex → Funcs[i].Dispatch       │  │
│  │  - Generated dispatch wrappers handle all marshaling               │  │
│  └────────────────────────────────────────────────────────────────────┘  │
│              │                                      │                    │
│              │ Go struct                            │ C struct (cgo)     │
│              ▼                                      ▼                    │
│  ┌─────────────────────┐                ┌─────────────────────┐          │
│  │       gomod         │◀──────────────▶│       cmod          │          │
│  │  (Go struct API)    │   Go↔C conv    │  (C struct API)     │          │
│  │  Always non-RT      │                │  Can be RT-safe     │          │
│  └─────────────────────┘                └─────────────────────┘          │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

## Key Design Principles

### 1. Modules Know Nothing About REST/JSON

Modules implement pure struct-based APIs:
- **cmod**: C structs, C function callbacks
- **gomod**: Go structs, Go function callbacks

All protocol handling (HTTP, JSON) is centralized in the gomc-server binary.

### 2. JSON Only at External Boundary

| Call Path | Data Serialization |
|-----------|-------------------|
| External REST → any module | JSON → DispatchFunc → callback (via dispatch table) |
| gomod → gomod | Go struct (direct interface call) |
| gomod → cmod | Go struct → C struct (via generated client) |
| cmod → cmod | C struct (direct function pointer call, RT-safe) |
| cmod → gomod | C struct → Go struct (via cgo export) |

### 3. Single Port, Path-Based Routing

All REST services share one HTTP server:
```
http://localhost:8080/api/v1/{instance}/{path}

Examples:
  GET  /api/v1/hal/pins
  GET  /api/v1/hal/pin/axis.0.pos-cmd
  POST /api/v1/halcmd/signal
```

### 4. RT-Safe Marking

- `@rt_safe "true"` in IDL means the callback CAN be called from RT context
- All callbacks can be called from non-RT context (REST, gomod)
- gomod is always non-RT (Go has garbage collection)
- cmod→gomod calls are always non-RT

## IDL Definition (.gmi files)

Interface definitions live in `src/gmi/idl/`. Example:

```
@api hal
@version 1
@prefix "hal"
@rest_export false  // Not exposed via REST

const MAX_PINS = 256

enum PinDir {
    HAL_IN = 16
    HAL_OUT = 32
    HAL_IO = 48
}

type PinInfo {
    name: string
    dir: PinDir
    type: PinType
    value: f64
}

@rt_safe "true"
func pin_read(name: string) -> PinInfo
```

### IDL Language Features (Implemented)

| Feature | Syntax | Example |
|---------|--------|---------|
| Constants | `const NAME = N` | `const MAX_JOINTS = 16` |
| Enums | `enum Name { A = 1, B = 2 }` | `enum PinDir { IN = 16 }` |
| Types | `type Name { field: T }` | `type PinInfo { name: string }` |
| Primitives | `bool`, `i8`-`i64`, `u8`-`u64`, `f32`, `f64`, `string` | |
| Fixed arrays | `[N]T` or `[CONST]T` | `[16]f64`, `[MAX_JOINTS]f64` |
| Slices | `[]T` | `[]PinInfo` |
| Nullable | `T?` | `string?`, `PinInfo?` |
| By-ref params | `byref name: T` | `byref joints: [16]f64` |
| Out params | `name: T out` | `result: i32 out` |
| Functions | `func name(params) -> ReturnType` | `func forward(...) -> i32` |

**Parameter qualifiers:**
- `byref` — True in/out: passed as `*T` in Go, pointer in C. Caller provides value, callee may modify.
- `out` — Output only: excluded from Go method signature, appears as additional return values.
  The generated trampoline writes back to the C out-pointer after the Go call returns.
  For void functions with out-params, Go returns values directly (no error in tuple).
  For non-void functions with out-params, the function return comes first, then out-params, then error.

**Directives:**
- `@api name` — API name (required)
- `@version N` — API version (required)
- `@prefix "str"` — C/REST prefix
- `@rest_export true/false` — Enable REST exposure
- `@rt_safe "true"` — Mark function as RT-safe
- `@publish true` — Mark function as a publish producer (C → ring → Go drain)
- `@publish_ring_size N` — Ring buffer slot count (default 64)
- `@watch true` — Mark function as a WebSocket watch endpoint
- `@watch_source "func_name"` — Link watch to a publish function (drain feeds watch)
- `@watch_default_rate 200ms` — Default push interval for WS subscribers

## Code Generation (gmicompile)

### Generated C Server Code (`--server-c`)

```c
// hal_api.h - Generated by gmicompile

typedef struct {
    const char *name;
    hal_pin_dir_t dir;
    hal_pin_type_t type;
    double value;
} hal_pin_info_t;

typedef int (*hal_pin_read_fn)(const char *name, hal_pin_info_t *out);

typedef struct {
    hal_pin_read_fn pin_read;
    // ... other callbacks
} hal_callbacks_t;

// Register this API implementation with the gomc-server.
// Internally generates Go dispatch wrappers (via cgo) embedded in FuncMeta.
int hal_api_register(const char *instance_name, const hal_callbacks_t *callbacks);
```

### Generated Go Dispatch Wrappers (internal, from `--server-c`)

These are generated alongside the C header and compiled into the gomc-server binary.
They bridge the gap between the uniform `DispatchFunc` signature and C callbacks:

```go
// hal_dispatch.go - Generated by gmicompile (not hand-written)

func halDispatchPinRead(cb unsafe.Pointer, req []byte) ([]byte, error) {
    // JSON → Go
    var params struct { Name string `json:"name"` }
    json.Unmarshal(req, &params)
    // Go → C
    cname := C.CString(params.Name)
    defer C.free(unsafe.Pointer(cname))
    var out C.hal_pin_info_t
    rc := C.call_pin_read((*C.hal_callbacks_t)(cb), cname, &out)
    if rc != 0 { return nil, syscall.Errno(-rc) }
    // C → JSON
    return json.Marshal(pinInfoFromC(&out))
}

var halMeta = &APIMeta{
    Name: "hal", Version: 1, RESTExport: true, Prefix: "hal",
    Funcs: []FuncMeta{
        {Name: "pin_read", Method: "GET", Path: "/pin/{name}", Dispatch: halDispatchPinRead},
        // ...
    },
}
```

### Generated Go Server Code (`--server-go`)

```go
// hal_api.go - Generated by gmicompile

type PinInfo struct {
    Name  string  `json:"name"`
    Dir   PinDir  `json:"dir"`
    Type  PinType `json:"type"`
    Value float64 `json:"value"`
}

type HalCallbacks interface {
    PinRead(name string) (*PinInfo, error)
    // ... other callbacks
}

// Register populates FuncMeta.Dispatch for each function and calls
// the generic registry.Register().
func RegisterHalAPI(instance string, impl HalCallbacks) error
```

When used as a C→Go bridge (e.g. canon callbacks), `--server-go` also generates:
- `//export` trampolines callable from C (CGO function pointers)
- `BuildCallbacks()` returning a C struct of function pointers
- `XxxCommands(impl)` returning `[]CommandMeta` for WebSocket exposure
- Void functions generate trampolines with no return value
- `out` parameters become additional Go return values (excluded from params)

### Generated C Client Code (`--client-c`)

For cmod calling other APIs (lookup at startup, direct calls at runtime):

```c
// halcmd_client.h - for cmod to call halcmd API

// Lookup API at module init (fails if not found or version mismatch).
// Returns opaque pointer — cast to halcmd_callbacks_t* for direct calls.
halcmd_callbacks_t *halcmd_api_get(const char *instance, int required_version);

// Direct function calls via callbacks struct (no dispatch, no overhead):
// halcmd_callbacks_t *api = halcmd_api_get("halcmd", 1);
// int rc = api->list_pins("*", &result, &len);
```

### Generated C REST Client Code (`--client-c` with `@rest_export true`)

For external (non-launcher) C programs calling APIs over REST.
Each client instance owns a persistent CURL handle for connection pooling
(TCP keep-alive, TLS session reuse). Not thread-safe — create one per thread:

```c
// halcmd_rest_client.h - for standalone C programs

typedef struct halcmd_rest_client halcmd_rest_client_t;

// Create client — owns CURL handle, reuses connections across calls.
// For multi-threaded use, create one client per thread.
halcmd_rest_client_t *halcmd_rest_connect(const char *base_url);
void halcmd_rest_disconnect(halcmd_rest_client_t *client);

int halcmd_rest_list_pins(halcmd_rest_client_t *client, const char *pattern,
                          halcmd_pin_info_t **out, size_t *out_len);
```

### Generated Go Client Code (`--client-go`)

For gomod calling other APIs (lookup at startup):

```go
// halcmd_client.go - for gomod to call halcmd API

// Lookup at module init - returns typed interface wrapping the callbacks.
// Returns error if not found or version mismatch.
func GetHalcmdAPI(instance string, requiredVersion int) (HalcmdCallbacks, error)

// Direct calls via returned interface (no dispatch table overhead):
// api, _ := GetHalcmdAPI("halcmd", 1)
// pins, err := api.ListPins("*")
```

## Call Flow Examples

### External REST → cmod (or gomod — identical path)

```
1. HTTP request: GET /api/v1/hal/pin/axis.0.pos-cmd
2. HTTP server looks up "hal" in registry → RegisteredAPI
3. HTTP server matches (GET, "/pin/{name}") → funcIndex
4. HTTP server calls api.Meta.Funcs[funcIndex].Dispatch(api.Callbacks, body)
5. Generated dispatch wrapper (Go):
   a. Unmarshals JSON request body → Go values
   b. For cmod: converts Go values → C values, calls C callback via cgo
      For gomod: calls Go interface method directly
   c. Converts result → JSON response bytes
6. HTTP server writes JSON response
```

Note: The HTTP server is completely generic. It does not know whether the
API is backed by a cmod or gomod — both produce the same `DispatchFunc` table.

### gomod → cmod (internal, at runtime)

```
Prerequisites (done at module init):
  - gomod called GetHalAPI("hal", 1)
  - Returned HalCallbacks wraps resolved C callback pointers

At runtime:
1. gomod calls: api.PinRead("axis.0.pos-cmd")
2. Generated client converts Go values → C values
3. Direct C callback call via cgo (no dispatch table, no lookup!)
4. C callback executes, fills result struct, returns errno
5. Generated client converts C result → Go values
6. Return Go struct to caller
```

### cmod → gomod (internal, non-RT only, at runtime)

```
Prerequisites (done at module init):
  - cmod called halcmd_api_get("halcmd", 1)
  - Returned opaque pointer is cast to halcmd_callbacks_t*

At runtime:
1. cmod calls: api->list_pins("*", &result, &len)
2. Generated C stub enters Go via cgo export
3. Direct Go interface call (no dispatch table, no registry lookup!)
4. Go callback executes, returns Go values + error
5. Generated stub converts Go result → C struct
6. Return to C caller with errno
```

### cmod → cmod (internal, RT-safe possible, at runtime)

```
Prerequisites (done at module init):
  - cmod called hal_api_get("hal", 1)
  - Returned opaque pointer is cast to hal_callbacks_t*

At runtime (can be from RT context if callback is RT-safe):
1. cmod calls: api->pin_read("axis.0.pos-cmd", &info)
2. Direct C function pointer call (no cgo, no dispatch table!)
3. C callback executes, fills result struct, returns errno
4. Return to caller
```

## Registry Design

### Core Types

```go
// DispatchFunc is the uniform signature for all generated dispatch wrappers.
// Both cmod and gomod generate functions with this signature.
// The HTTP server calls these — it never touches callbacks directly.
type DispatchFunc func(callbacks unsafe.Pointer, req []byte) ([]byte, error)

// FuncMeta holds static metadata + dispatch for one API function (generated).
// Routing info and dispatch wrapper live together — no parallel arrays.
type FuncMeta struct {
    Name     string       // "pin_read"
    Method   string       // "GET", "POST", etc. (empty if not REST-exported)
    Path     string       // "/pin/{name}" (empty if not REST-exported)
    RTSafe   bool
    Dispatch DispatchFunc // generated wrapper (nil if not REST-exported)
}

// APIMeta holds static metadata for an entire API (generated, read-only).
type APIMeta struct {
    Name       string     // "hal"
    Version    int
    RESTExport bool
    Prefix     string     // REST path prefix
    Funcs      []FuncMeta // routing + dispatch in one place
}

// RegisteredAPI is one registered API instance in the registry.
type RegisteredAPI struct {
    APIName   string         // "tp" — API name from registration
    Version   int            // API version from registration
    Meta      *APIMeta       // optional — REST routing/dispatch (nil for pure C-to-C)
    Instance  string         // "default" — unique instance name within an API
    Callbacks unsafe.Pointer // opaque — *tp_callbacks_t (cmod) or Go interface
}
```

### Language-Agnostic Dispatch

Both cmod and gomod generate Go-side dispatch functions with identical `DispatchFunc`
signatures. Since cgo already requires Go wrapper functions to call C function
pointers, the REST path always goes through generated Go code — no special handling
for cmod vs gomod:

```go
// Generated for cmod (--server-c):
func halDispatchPinRead(cb unsafe.Pointer, req []byte) ([]byte, error) {
    name := unmarshalString(req)            // JSON → Go
    cname := C.CString(name)               // Go → C
    defer C.free(unsafe.Pointer(cname))
    var out C.hal_pin_info_t
    rc := C.hal_pin_read_call(             // cgo call to C callback
        (*C.hal_callbacks_t)(cb).pin_read, cname, &out)
    if rc != 0 { return nil, syscall.Errno(-rc) }
    return marshalPinInfo(&out), nil        // C → Go → JSON
}

// Generated for gomod (--server-go):
func halDispatchPinRead(cb unsafe.Pointer, req []byte) ([]byte, error) {
    name := unmarshalString(req)            // JSON → Go
    impl := (*goHalCallbacks)(cb)           // type assertion
    result, err := impl.PinRead(name)       // direct Go call
    if err != nil { return nil, err }
    return marshalPinInfo(result), nil      // Go → JSON
}

// Both populate Dispatch in the same FuncMeta slice:
var halMeta = &APIMeta{
    Funcs: []FuncMeta{
        {Name: "init", Dispatch: halDispatchInit},
        {Name: "pin_read", Method: "GET", Path: "/pin/{name}", Dispatch: halDispatchPinRead},
        // ...
    },
}
```

### API Registration (at module load)

```go
// Register is called by generated code during module init.
// Only apiName, version, instance, and callbacks are required — all supplied
// by the C module at runtime. If an APIMeta with matching name+version was
// registered (e.g. via a generated Go package init()), it is automatically
// attached for REST dispatch.
func Register(apiName string, version int, instance string, callbacks unsafe.Pointer) error {
    key := registryKey(apiName, instance)
    if registry.Has(key) {
        return syscall.EEXIST
    }
    // Attach REST metadata if available (optional — nil is fine for C-to-C)
    meta := GetMeta(apiName, version)
    registry.Put(key, &RegisteredAPI{
        APIName:   apiName,
        Version:   version,
        Meta:      meta,
        Instance:  instance,
        Callbacks: callbacks,
    })
    return nil
}
```

### API Lookup (at module init, not runtime)

```go
// GetAPI returns the callbacks pointer for direct inter-module calls.
// For gomod clients — called during module init.
func GetAPI(apiName, instance string, requiredVersion int) (unsafe.Pointer, error) {
    key := registryKey(apiName, instance)
    api := registry.Get(key)
    if api == nil {
        return nil, syscall.ENOENT
    }
    if api.Version != requiredVersion {
        return nil, syscall.EINVAL
    }
    // Return opaque callbacks pointer — client casts to concrete type
    return api.Callbacks, nil
}

// For cmod clients — called during module init via gomc_api_t callback table:
// void* get_api(void *ctx, const char *api_name, int version, const char *instance);
// Returns NULL on not found or version mismatch.
// The cmod casts the result to kins_callbacks_t* and calls members directly.
```

### Registry Event Hooks

The registry supports event-driven callbacks for deferred initialization:

```go
// OnRegister subscribes to new API registrations. The callback fires
// immediately for already-registered APIs, then for each future Register().
func (r *Registry) OnRegister(fn func(*RegisteredAPI))

// OnDefaultRegistryReady queues a callback that fires when SetDefaultRegistry()
// is called. Used by generated package init() functions that need the registry
// (which is created later during launcher startup).
func OnDefaultRegistryReady(fn func(*Registry))
```

These hooks enable the auto-drain pattern: generated code subscribes in `init()`,
and when the C module eventually registers its publish ring, the Go drain starts
automatically without any launcher knowledge.

### Publish Ring Pattern (C → Go)

For high-frequency C → Go data streams (e.g., operator messages), the system uses
lock-free SPSC (single-producer, single-consumer) ring buffers:

```
┌─────────────┐     ring buffer      ┌─────────────────────┐     WebSocket
│   C module  │ ──── (lock-free) ────▶│  Go drain goroutine │ ──────────────▶ clients
│  (milltask) │     64 slots SPSC     │  (auto-started)     │   (via watch)
└─────────────┘                       └─────────────────────┘
```

**IDL declaration:**

```
@publish true
@publish_ring_size 64
func publish_error(kind: ErrorKind, text: string)

@watch true
@watch_source "publish_error"
@watch_default_rate 200ms
func get_errors() -> []ErrorMessage
```

**Generated artifacts (by `gmicompile --server-c`):**

1. **`<api>_pub.h`** — C producer API:
   - `ring_init(api, instance)` — allocates ring, registers with API registry
   - `ring_write(ring, ...)` — lock-free slot write (returns -1 if full)

2. **`<api>_pub.go`** — Go drain type:
   - `NewPublishErrorDrain(callbacks)` — creates drain from callbacks pointer
   - `drain.Start()` / `drain.Stop()` — goroutine lifecycle
   - `drain.WatchFunc()` — returns function compatible with WS watch

3. **`<api>_drain_hook.go`** — Auto-start glue (via `init()`):
   - Subscribes to `OnDefaultRegistryReady` → `OnRegister`
   - When the matching API name appears, starts drain + registers WS watch
   - No launcher code required — fully self-contained

**Lifecycle:**
1. Package `init()` registers callback via `OnDefaultRegistryReady`
2. Launcher calls `SetDefaultRegistry()` → pending callbacks fire
3. C module loads, calls `ring_init()` → `Register()` in registry
4. `OnRegister` fires → drain starts → WS watch registered
5. Clients subscribe to watch endpoint, receive messages at configured rate

### REST Dispatch (HTTP server)

The HTTP server is completely generic — no per-API code:

```go
func (s *Server) handleAPIRequest(w http.ResponseWriter, r *http.Request) {
    // 1. Extract instance + path from URL
    instance, path := parseURL(r.URL.Path)

    // 2. Find registered API
    api := registry.Get(instance)
    if api == nil || !api.Meta.RESTExport {
        http.NotFound(w, r)
        return
    }

    // 3. Match path against FuncMeta to get funcIndex
    funcIndex := matchFunc(api.Meta.Funcs, r.Method, path)
    if funcIndex < 0 {
        http.NotFound(w, r)
        return
    }

    // 4. Dispatch — uniform call, no cmod/gomod awareness
    body, _ := io.ReadAll(r.Body)
    resp, err := api.Meta.Funcs[funcIndex].Dispatch(api.Callbacks, body)

    // 5. Write response
    if err != nil {
        writeError(w, err)
        return
    }
    w.Header().Set("Content-Type", "application/json")
    w.Write(resp)
}
```

### Direct Inter-Module Calls (no dispatch table)

For cmod→cmod and gomod→gomod, the dispatch table is NOT used.
Clients call through the callbacks struct directly — zero overhead:

```c
// cmod→cmod: direct C function pointer call (RT-safe)
hal_callbacks_t *api = (hal_callbacks_t *)gmi_get_api("hal", 1);
hal_pin_info_t info;
int rc = api->pin_read("axis.0.pos-cmd", &info);
```

```go
// gomod→gomod: direct Go interface call
api, _ := GetHalAPI("hal", 1)  // returns HalCallbacks interface
info, err := api.PinRead("axis.0.pos-cmd")
```

The dispatch table only exists for REST and cross-language calls.

## Struct Conversion (Go ↔ C)

Options:

1. **Generated converters**: gmicompile generates conversion functions
2. **Reflection-based**: Generic converter using struct tags
3. **Hybrid**: Generated for performance-critical paths, reflection for rest

Recommended: **Generated converters** for type safety and performance.

```go
// Generated by gmicompile
func pinInfoGoToC(src *PinInfo, dst *C.hal_pin_info_t) {
    dst.name = C.CString(src.Name)
    dst.dir = C.hal_pin_dir_t(src.Dir)
    dst.type_ = C.hal_pin_type_t(src.Type)
    dst.value = C.double(src.Value)
}

func pinInfoCToGo(src *C.hal_pin_info_t) *PinInfo {
    return &PinInfo{
        Name:  C.GoString(src.name),
        Dir:   PinDir(src.dir),
        Type:  PinType(src.type_),
        Value: float64(src.value),
    }
}
```

## Lifecycle Integration

### Startup Sequence

```
1. Parse config, load modules (existing)
2. Execute HAL file setup (existing)
3. All modules register their APIs
4. Start HTTP server (new)
5. Start UI (existing)
```

### Shutdown Sequence

```
1. UI exits
2. Stop HTTP server (new) 
3. Unload modules (existing)
4. Cleanup (existing)
```

## Security Considerations

### Current (Phase 1)
- Bind to localhost only
- No authentication
- Suitable for single-machine use

### Future (Phase 2+)
- Optional TLS
- Token-based authentication
- Permission system per API/function
- Optional remote access

## File Structure

```
src/gmi/
├── idl/                    # Interface definitions (.gmi files)
│   ├── hal.gmi             # HAL component API (not compilable, uses opaque ptrs)
│   ├── halcmd.gmi          # HAL command API (@rest_export true)
│   ├── home.gmi            # Homing API (19 callbacks)
│   ├── kins.gmi            # Kinematics API (5 callbacks)
│   ├── mot.gmi             # Motion reverse-callbacks (83 callbacks)
│   ├── tp.gmi              # Trajectory planner API (29 callbacks)
│   ├── emcstat.gmi         # Machine status (watchable, @rest_export true)
│   ├── emccmd.gmi          # Machine commands (@rest_export true)
│   ├── emcerror.gmi        # Error messages (watchable, @rest_export true)
│   └── README.md
├── python/                 # Hand-written Python client modules
│   ├── __init__.py         # gmi package: rest_url(), ws_url(), IniFile, helpers
│   ├── stat.py             # gmi.Stat (WS watch, drop-in for linuxcnc.stat())
│   ├── command.py          # gmi.Command (WS commands, drop-in for linuxcnc.command())
│   ├── error.py            # gmi.ErrorChannel (WS watch, drop-in for linuxcnc.error_channel())
│   ├── constants.py        # Flat constants (MODE_MANUAL=1, etc.)
│   ├── positionlogger.py   # gmi.PositionLogger (WS-based, vertex9/OpenGL)
│   ├── pyvcp_compat.py     # PyVCPCompat (WS-based hal.component drop-in)
│   └── tools.py            # Tool table REST client
├── lib/                    # C runtime library (libgmi) for REST clients
│   ├── gmi.h               # Main include
│   ├── gmi_http.c/h        # HTTP client (libcurl wrapper)
│   ├── gmi_json.c/h        # JSON utilities (cJSON wrapper)
│   ├── gmi_error.c/h       # Error codes (GMI_ERR_*)
│   ├── gmi_types.c/h       # Type utilities
│   └── Submakefile
├── DYNAMIC_API_DESIGN.md   # This document
└── README.md

src/gomc/                   # Go module: github.com/sittner/linuxcnc/src/gomc
├── go.mod.in               # Tracked base go.mod (copied to go.mod on fresh build)
├── go.mod                   # Runtime go.mod (gitignored, managed by modcompile)
├── go.sum                   # Runtime go.sum (gitignored, managed by go toolchain)
├── packages.conf.in         # Tracked base package registry
├── packages.conf            # Runtime package registry (gitignored, managed by modcompile)
├── .gitignore               # Ignores runtime files: go.mod, go.sum, packages.conf, imports_generated.go
├── Submakefile              # Build rules for all Go targets
├── cmd/
│   ├── gomc-server/         # Server binary (compiled-in architecture)
│   │   ├── main.go
│   │   └── imports_generated.go  # Generated blank imports (gitignored)
│   ├── modcompile/          # Unified tool: .comp + .gmi + gomod management
│   │   └── main.go
│   ├── ads-xml-gen/         # ADS XML generator
│   │   └── main.go
│   └── halcmd/              # Go REST-based halcmd replacement (Step 4.5)
│       └── main.go
├── generated/               # Generated code (gitignored)
│   └── gmi/
│       ├── halcmd/          # halcmd_client.go (Go REST client)
│       ├── home/            # home_api.h, home_cgo.go
│       ├── kins/            # kins_api.h, kins_cgo.go
│       ├── mot/             # mot_api.h, mot_cgo.go
│       ├── manualtoolchange/ # manualtoolchange_api.h, manualtoolchange_cgo.go
│       ├── tp/              # tp_api.h, tp_cgo.go
│       ├── emcerror/        # emcerror_pub.h, emcerror_pub.go, emcerror_drain_hook.go
│       ├── emcstat/         # emcstat_api.h, emcstat_cgo.go
│       └── emccmd/          # emccmd_api.h, emccmd_cgo.go
├── external/                # Installed external Go packages (gitignored)
│   └── <name>/              # Copied source + .origin marker file
├── internal/
│   ├── ads/                 # ADS server (moved from external plugin, Phase 2)
│   ├── adsbridge/           # ADS bridge layer
│   ├── adsconfig/           # ADS configuration
│   ├── adsmodule/           # init() registers "ads-server" with gomc registry
│   ├── apiserver/           # REST server (Step 1)
│   │   ├── types.go         # DispatchFunc, FuncMeta, APIMeta, RegisteredAPI
│   │   ├── registry.go      # Register(), GetAPI(), OnRegister(), OnDefaultRegistryReady()
│   │   ├── server.go        # HTTP handler, path matching, _registry introspection
│   │   ├── *_test.go        # 37 tests
│   │   └── directtest/      # cmod direct-call simulation tests
│   ├── config/              # Compile-time config (paths injected via -ldflags)
│   │   └── paths.go         # EMC2GomcDir, EMC2BinDir, etc.
│   ├── gomc/                # Server lifecycle
│   │   ├── launcher.go      # Main server struct + startup
│   │   ├── rest_server.go   # REST API server start/stop ([GMC]REST_ADDR)
│   │   └── cleanup.go       # Shutdown sequence
│   ├── emcgateway/           # DELETED — replaced by Go milltask (internal/task)
│   ├── halrest/             # Server-side REST handler for halcmd API (Step 4.5)
│   │   └── halrest.go       # Dispatches REST calls to internal/halcmd
│   ├── halscope/            # Halscope gomod with embedded C RT (Step 5.8)
│   │   ├── module.go        # gomod: lifecycle, REST/WS dispatch, state persistence
│   │   ├── halscope_rt.h    # C RT data structures, state machine enums
│   │   ├── halscope_rt.c    # C RT sampling engine (runs in HAL thread via cgo)
│   │   └── testrt/          # Standalone C unit tests (mock HAL headers)
│   ├── inirest/             # Server-side REST handler for INI file access (Step 5.4)
│   │   ├── inirest.go       # POST /query dispatch, reads from launcher's parsed INI
│   │   └── inirest_test.go  # 6 tests (single, missing, empty, findall, bulk)
│   └── gmicompile/          # Code generator (parses .gmi → C/Go)
│       ├── ast/             # AST types
│       ├── parser/          # IDL parser (8 tests)
│       └── cgen/            # Code generators
│           ├── server.go            # --server-c: C header generation
│           ├── dispatch_c.go        # --server-c: Go cgo dispatch wrappers
│           ├── publish_c.go         # --server-c: C ring producer + Go drain
│           ├── publish_drain_hook.go # --server-c: Go auto-drain init() hook
│           ├── server_go.go         # --server-go: Go server generation
│           ├── client.go            # --client-c: C REST client generation
│           ├── client_go.go         # --client-go: Go REST client generation
│           └── client_py.go         # --client-python: Python REST client generation
├── pkg/
│   ├── cmodule/             # C module headers (gomc_*.h)
│   ├── gomc/                # Public registration interface for external packages
│   │   └── gomc.go          # RegisterModule(), RegisterMeta(), GetFactory(), HasModule()
│   ├── inifile/             # INI file parser
│   └── hal/                 # Go HAL bindings
│       ├── examples/        # passthrough example
│       └── tests/           # str-sender, str-receiver
└── pkgreg/                  # Package registry (packages.conf reader/writer)
    └── registry.go          # Registry type, GenerateImports()
```

## Implementation Plan

### Step 0: Core Module Migration (COMPLETE)

Migrate the three core motion modules (kins, tp, homing) from legacy RTAPI loadable
modules to self-contained cmods using the GMI dynamic API.

**Deliverables:**
- [x] IDL definitions: `mot.gmi` (83 callbacks), `home.gmi` (19), `tp.gmi` (29), `kins.gmi` (5)
- [x] Code generator (`gmicompile`): `--server-c` producing C headers + Go CGO wrappers
- [x] Codegen: functions return values directly (not via out-pointer)
- [x] Codegen: enums passed by value (not pointer)
- [x] Kinematics: 17 kins modules ported to cmod (in `emc/kinematics/`)
- [x] Trajectory planner: `tp.c` is self-contained cmod (owns `TP_STRUCT`, registers tp API)
- [x] Homing: `homing.c` is self-contained cmod (registers home API)
- [x] Motion controller (`motmod`): consumes tp + home APIs via direct pointer calls
- [x] Bridge layer removed — `control.c`/`command.c` call `motmod_tp_api->*` directly
- [x] All wrapper layers eliminated (tpmod.c, homemod.c deleted)
- [x] Build system: cmod rules in Makefile/Submakefiles, rtlib rules removed
- [x] Generated code properly gitignored (`src/gomc/.gitignore`)
- [x] Kins round-trip Go tests: forward→inverse→compare (trivkins, pumakins)
- [x] RPY convention test: verifies j1 rotation maps to yaw (C), not roll (A)

**Migration findings:**

- **Self-contained cmod pattern**: Each module implements `New()` entry point,
  returns a `cmod_t` with Init/Start/Destroy. Init calls `*_api_get()` to
  resolve dependencies. This replaces the old RTAPI module_init + EXPORT_SYMBOL pattern.

- **Wrapper elimination**: Initial migration used intermediate wrapper files
  (tpmod.c, homemod.c) that forwarded calls to the original implementation.
  These were eliminated by merging directly — making original functions `static`
  and appending `gmi_*` callback wrappers + cmod lifecycle to the same file.

- **Forward declarations needed**: After making functions static, some are called
  before their definition. Forward declarations are required (added at top of file).

- **Internal self-calls**: When public wrapper functions are removed, internal code
  that previously called those wrappers must be updated to call `base_*` functions
  directly (e.g., `get_allhomed()` → `base_get_allhomed()` inside homing.c).

- **Kinematics are trivial cmods**: Each is a single .c file with `New()` that
  registers kins callbacks. No complex lifecycle. The `switchkins_cmod.h` header
  provides common infrastructure for switchable kins modules.

- **Posemath convention standardized**: Modules that inline rotation helpers must
  use the same convention as `posemath.h`. Storage: `R.AB` = column A, row B
  (matching `PmRotationMatrix` where `m->x.y` = column x, row y). RPY:
  `R = Rz(yaw) * Ry(pitch) * Rx(roll)`, where roll=A (about X), pitch=B (about Y),
  yaw=C (about Z). Two modules (`pumakins.c`, `pentakins.c`) had roll/yaw swapped
  in their inline helpers — fixed to match legacy posemath behavior.

- **No separate directories needed**: Source lives in standard locations
  (`emc/kinematics/`, `emc/tp/`, `emc/motion/`). Only the IDL definitions and
  runtime library need a dedicated `gmi/` directory.

### Step 1: apiserver Package (COMPLETE)

Foundation for everything else. Fully testable in isolation.

**Deliverables:**
- [x] `types.go` — `DispatchFunc`, `FuncMeta`, `APIMeta`, `RegisteredAPI`
- [x] `registry.go` — `Register()`, `GetAPI()`, thread-safe instance map
- [x] `server.go` — generic HTTP handler, path matching, JSON error responses
- [x] `ws_handler.go` — WebSocket watch/command infrastructure, `CommandsFromAPI()` helper

`CommandsFromAPI(api)` wraps all `DispatchFunc` entries of a registered API
as WebSocket `CommandMeta` handlers, enabling simple one-liner command
registration without duplicating dispatch logic.

**Tests:** 37 passing
- [x] Unit: registry Register/GetAPI, duplicate rejection, version mismatch (9 tests)
- [x] Unit: path matching (static, parameterized, wildcard, method filtering) (5 tests)
- [x] Unit: dispatch with mock `DispatchFunc` (success, error, not found) (11 tests)
- [x] Integration: `httptest.Server` → register fake API → REST roundtrip → verify JSON (8 tests)
- [x] Direct call tests: cmod→cmod lookup and call simulation (4 tests)

**No dependencies on:** gmicompile, cgo, generated code. Hand-written mock APIs only.

### Step 2: gmicompile `--server-go` (COMPLETE)

Generate Go code that plugs into the apiserver from Step 1.

**Deliverables:**
- [x] Generate `APIMeta` literal with `FuncMeta` entries (including `Dispatch`)
- [x] Generate Go callbacks interface (e.g., `HalCallbacks`)
- [x] Generate Go dispatch wrappers (`DispatchFunc` per function)
- [x] Generate `Register*API()` wrapper calling `apiserver.Register()`
- [x] Generate Go struct types from IDL `type` declarations
- [x] Generate `//export` trampolines + `BuildCallbacks()` for C→Go bridge
- [x] Generate `XxxCommands(impl)` for WS command exposure (typed, no dispatch table)
- [x] Void functions: no error return in Go interface, trampoline returns void
- [x] `out` parameters: excluded from Go method params, appear as return values
- [x] `TypeArray` support: fixed arrays passed as pointer in C, `unsafe.Slice` in Go
- [x] `[]Struct` (slice of named type): C array + length, converted via unsafe.Slice loop

**Tests:** 3 passing
- [x] Unit: golden-file comparison of generated .go output vs expected
- [x] Unit: keyword escape handling (Go reserved words)
- [x] Unit: non-REST API generation (no dispatch wrappers)

**Canon bridge (largest --server-go user):**

The canon callback table (`gmi/idl/canon.gmi`, 143 functions) is generated
via `--server-go`, producing `//export` C trampolines that the C milltask
calls directly. This replaced a hand-written 1200-line bridge. Features
exercised: void functions (most canon callbacks), `out` params (e.g.
`GET_EXTERNAL_POSITION`), `TypeArray` for `[9]f64` joint arrays, `byref`
for mutable array parameters.

**String memory ownership convention:**

Generated `--server-go` bridges for C→Go direction follow these rules:
- `const char *` fields in C structs = borrowed pointers into C internal
  state. `C.GoString()` copies the data safely. No free needed.
- `C.CString()` allocations in Go→C direction = caller-owns. The generated
  `_retAllocs` list is intentionally not freed (C caller owns returned data).
- For `--server-c` (Go→C dispatch): string fields marshaled via
  `C.CString()` are freed after the C callback returns.

### Step 3: gmicompile `--server-c` + cgo Bridge (COMPLETE)

Generate C callbacks struct + Go dispatch wrappers that cross the cgo boundary.

**Deliverables:**
- [x] Generate C header (callbacks struct, register function, types)
- [x] Generate Go dispatch wrappers (cgo: Go → C callback calls)
- [x] Generate Go↔C struct converters (both directions)
- [x] Generate cgo-exported `Register()` callable from C
- [x] REST dispatch wrappers (JSON → Go → C → errno → JSON)

**Tests:** 5 passing
- [x] Unit: golden-file comparison of generated .h and .go output
- [x] Unit: cgo keyword field escaping (`type` → `_type`)
- [x] Unit: void return functions, primitive return functions

**Generated files:** `kins_api.h` + `kins_cgo.go`, `tp_api.h` + `tp_cgo.go`,
`home_api.h` + `home_cgo.go`, `mot_api.h` + `mot_cgo.go` (in `generated/gmi/`)

### Step 4: Client Generation (COMPLETE)

Enable inter-module calls (direct) and external REST clients.

**Deliverables:**
- [x] `--client-go` — Go REST client for external programs (halcmd replacement)
- [x] `--client-c` internal — C header with `<api>_api_get()` for cmod→cmod/gomod (direct callback)
- [x] `--client-c` REST — C REST client using libgmi (for external programs)

**Tests:** All four calling patterns tested (14 tests total)
- [x] cmod→cmod: `directtest/` — C callbacks registered, Go looks up, calls via cgo (4 tests)
- [x] gomod→cmod: `directtest/` — same mechanism, Go code calling C function pointers
- [x] gomod→gomod: `gomodtest/` — pure Go interface registration and lookup (5 tests)
- [x] cmod→gomod: `cmodtogomod/` — C code calling `//export` Go functions (5 tests)

**Runtime library (libgmi):** Complete in `src/gmi/lib/`
- `gmi.h` — main include
- `gmi_http.c/h` — HTTP client (libcurl wrapper)
- `gmi_json.c/h` — JSON utilities (cJSON wrapper)
- `gmi_error.c/h` — error codes (GMI_ERR_*)
- `gmi_types.c/h` — type utilities

### Step 4.5: halcmd REST Tool (COMPLETE)

Replace the legacy C halcmd/halrmt with a new Go-based halcmd using the REST API.

**Motivation:**
- Validates `--client-go` in real-world usage
- Removes ~8k lines of old C halcmd code
- halrmt becomes redundant (REST is inherently remote-capable)
- Consistent architecture: all external tools use REST

**Deliverables:**
- [x] New `cmd/halcmd/` in launcher — Go CLI using generated halcmd client
- [x] Environment variable `GMC_REST_URL` (default: `http://localhost:5080/`)
- [x] Full command compatibility (show, list, getp, setp, gets, sets, newsig, delsig, net, loadrt, etc.)
- [x] Disable old halcmd/halrmt in build system (`BUILD_GOLANG=yes` guard)

**Commands mapped to REST:**
| halcmd command | REST API call |
|----------------|---------------|
| `show pin [pattern]` | GET /pins?pattern= |
| `show sig [pattern]` | GET /signals?pattern= |
| `show param [pattern]` | GET /params?pattern= |
| `show comp [pattern]` | GET /components?pattern= |
| `show funct [pattern]` | GET /functions?pattern= |
| `show thread [pattern]` | GET /threads?pattern= |
| `status` | GET /status |
| `getp <pin>` | GET /pin/{name} |
| `gets <signal>` | GET /signal/{name} |
| `setp <pin> <value>` | PUT /pin/{name} |
| `sets <signal> <value>` | PUT /signal/{name} |
| `newsig <name> <type>` | POST /signal |
| `delsig <name>` | DELETE /signal/{name} |
| `net <signal> <pins>` | POST /net |
| `linksp <signal> <pin>` | POST /link |
| `linkpp <pin1> <pin2>` | POST /linkpp |
| `unlinkp <pin>` | DELETE /link/{pin} |
| `loadrt <module> [args]` | POST /loadrt |
| `unloadrt <module>` | DELETE /loadrt/{module} |
| `loadusr [-W] <cmd>` | POST /loadusr |
| `unloadusr <name>` | DELETE /loadusr/{name} |
| `waitusr <name>` | POST /waitusr/{name} |
| `newthread <n> <period>` | POST /thread |
| `delthread <name>` | DELETE /thread/{name} |
| `addf <func> <thread>` | POST /thread/{thread}/function |
| `delf <func> <thread>` | DELETE /thread/{thread}/function/{func} |
| `start` | POST /start |
| `stop` | POST /stop |
| `alias pin <n> <a>` | POST /pin/{n}/alias |
| `unalias pin <n>` | DELETE /pin/{n}/alias |
| `lock [level]` | POST /lock |
| `unlock [level]` | POST /unlock |
| `debug <level>` | PUT /debug |
| `save [type]` | GET /save |

### Step 5: Python Client Generation (COMPLETE)

REST client for Python UIs (axis, gmoccapy, etc.).

**Deliverables:**
- [x] `--client-python` — generate Python REST client module using `urllib` (stdlib only, no external deps)
- [x] Generate `@dataclass` classes from IDL `type` declarations (with `from_dict()`/`to_dict()`)
- [x] Generate `IntEnum` subclasses from IDL `enum` declarations
- [x] Generate `<Api>Client` class with typed methods, path/query param handling, JSON body
- [x] `APIError` exception class for HTTP error responses
- [x] Wired into gmicompile CLI (`--client-python` mode with `@rest_export` validation)
- [x] Generated halcmd Python client (621 lines, valid Python syntax)

**Implementation:** `internal/gmicompile/cgen/client_py.go`

**Tests:** 3 passing (`client_py_test.go`)
- [x] Unit: full API generation (types, enums, constants, client class, methods)
- [x] Unit: multiple path parameter substitution
- [x] Unit: primitive return types and void methods

### Step 5.1: Manualtoolchange REST Migration (COMPLETE)

First real consumer of the GMI pipeline: replace `hal_manualtoolchange.py`
(HAL userspace component in Python that directly accesses HAL pins) with a
cmod + REST API architecture.

**Architecture:**
- `manualtoolchange.comp` — cmod (C, RT-capable) handling HAL pins + iocontrol handshake
- `manualtoolchange.gmi` — IDL defining REST API (GET /state, POST /confirm)
- Generated dispatch (`manualtoolchange_cgo.go`) — compiled into gomc-server
- Generated Python client (`manualtoolchange_client.py`) — used by UI
- `manualtoolchange_ui.py` — Tkinter UI, polls REST, replaces old `hal_manualtoolchange.py`

**Completed:**
- [x] `gmi/idl/manualtoolchange.gmi` — IDL with `@rest_export true`, two endpoints
- [x] `hal/components/manualtoolchange.comp` — cmod with `gmi_provide manualtoolchange`,
      HAL pins (change, number, change_button, changed), thread function,
      GMI callbacks (`gmi_manualtoolchange_get_state`, `gmi_manualtoolchange_confirm`)
- [x] Generated `manualtoolchange_api.h` + `manualtoolchange_cgo.go` in
      `gomc/generated/gmi/manualtoolchange/`
- [x] Generated `lib/python/gmi/manualtoolchange_client.py` — Python REST client
- [x] `manualtoolchange_ui.py` (133 lines) — Tkinter UI using generated REST client
- [x] `gmi/codegen/Submakefile` — build rules for API header, cgo dispatch, Python client
- [x] `hal/components/Submakefile` — cmod build rule with GMI header dependency
- [x] `bin/manualtoolchange_ui` — installed UI script
- [x] Generated cgo uses `internal/apiserver` (correct import path)
- [x] `packages.conf` entry present — cgo dispatch compiled into gomc-server
- [x] `lib/python/gmi/__init__.py` — hand-written source in `src/gmi/python/`, copied at build time
- [x] All HAL configs migrated from `hal_manualtoolchange` to `manualtoolchange` cmod
- [x] `sim_lib.tcl` `use_hal_manualtoolchange` proc updated (3-line cmod form)
- [x] `stepconf/build_HAL.py` and `pncconf/build_HAL.py` updated
- [x] `qtvcp/widgets/dialog_widget.py` updated to use `manualtoolchange` prefix

**Notes:**
- The proc/option name `use_hal_manualtoolchange` / `-no_use_hal_manualtoolchange`
  is kept for backward compatibility with user INI files. Only the implementation
  changed (loads cmod instead of `loadusr`).
- `hal_manualtoolchange.py` (old Python component) is kept but deprecated — to be
  removed in a future cleanup pass.

### Step 5.2: AXIS UI Watch Channel (COMPLETE)

Migrate AXIS GUI's HAL pins to a WebSocket-based watch channel, eliminating the
UI's dependency on HAL shared memory. This is a preparation step for removing
all shared memory access from UI processes.

**Motivation:**
- Mid-term goal: UI processes must not access HAL shared memory
- AXIS currently exports ~25 HAL pins (jog, status, notifications, sliders)
- REST polling is adequate for slow state (notifications, errors) but insufficient
  for jog buttons (~10ms) and position updates (~50ms)
- NML status polling (current mechanism for machine position) will also need
  replacement — the watch channel infrastructure serves both needs

**Architecture: GMI Watch Channel (WebSocket)**

A single persistent WebSocket connection between UI and gomc-server:

```
axis.py (Tk)                    gomc-server
    │                               │
    ├─ REST (1s) ──────────────────►│  tool change, notifications
    │                               │
    └─ WebSocket (persistent) ◄────►│  position/status push (50ms)
         jog commands (immediate) ──►│  jog start/stop, abort
         slider values ────────────►│  feed override, spindle override
```

**Server→Client (push):** Server calls `@watch`-annotated functions at the
subscribed rate and pushes results as JSON over the WebSocket.

**Client→Server (commands):** Jog start/stop, abort, slider values sent as
JSON command messages over the same connection. Minimal framing overhead
(~2 bytes WebSocket header vs ~200 bytes HTTP per request).

**Why WebSocket:**
- Bidirectional on one persistent TCP connection
- ~1-5ms LAN latency (limited only by TCP + Go scheduling)
- Native Python support (`websockets` / `asyncio`)
- Tk integration: run in thread, post events to mainloop
- Works over network / reverse proxies → remote UI for free
- SSE is push-only (still need REST for commands); gRPC is heavy for Python/Tk;
  long-poll has per-request overhead defeating 50ms updates

**GMI IDL Extensions:**

`@watch` is a function-level annotation. Watchable functions can return any
GMI type (structs, enums, arrays, nested structs). The framework serializes
whatever the function returns.

```gmi
@api axis

type Position {
    x: f64
    y: f64
    z: f64
    a: f64
    b: f64
    c: f64
}

type JogState {
    active_axis: string
    increment: f64
    disabled: bool
}

type MachineStatus {
    position: Position
    jog: JogState
    is_running: bool
    has_error: bool
    has_notifications: bool
}

type Notification {
    level: NotifyLevel
    message: string
}

enum NotifyLevel {
    Info = 0
    Error = 1
}

enum NotifyClearMask {
    All = 0
    Info = 1
    Error = 2
}

# Push at 50ms — client subscribes, server calls at requested rate
@watch true
@watch_default_rate 50ms
func get_status() -> MachineStatus

# Slower status, separate subscription
@watch true
@watch_default_rate 1000ms
func get_notifications() -> []Notification

# Commands — not watchable, dispatched immediately over same WebSocket
func jog_start(axis: string, speed: f64) -> bool
func jog_stop(axis: string) -> bool
func set_jog_increment(value: f64) -> bool
func clear_notifications(which: NotifyClearMask) -> bool
func abort() -> bool
```

**Generated Code (gmicompile):**

| Flag | Output | Purpose |
|------|--------|---------|
| `--server-ws` | Go WebSocket handler | Subscribe/dispatch loop, per-client goroutine |
| `--client-python-ws` | Python async client | Typed callbacks, same dataclasses as REST client |

Server-side handler:
- Accepts subscription messages: `{"subscribe": "get_status", "rate_ms": 50}`
- Runs a goroutine per client per subscription that calls the GMI function
  at the requested rate and pushes the serialized result
- Receives command messages: `{"call": "jog_start", "args": {"axis": "x", "speed": 100.0}}`
- Optional delta optimization: only send fields that changed since last push

Python client:
```python
client = AxisWatchClient("ws://localhost:5080/api/v1/watch")
client.subscribe_get_status(rate_ms=50, callback=on_status_update)
client.subscribe_get_notifications(rate_ms=1000, callback=on_notifications)
# Commands go through the same connection
client.jog_start(axis="x", speed=100.0)
```

**Pin Migration Map (axis.py → axis.gmi):**

| Old HAL Pin | Direction | New GMI Mechanism |
|-------------|-----------|-------------------|
| `is-running` | OUT | `get_status().is_running` (watch @50ms) |
| `error` | OUT | `get_status().has_error` (watch @50ms) |
| `has-notifications` | OUT | `get_status().has_notifications` (watch @50ms) |
| `abort` | OUT | `abort()` command |
| `jog.{x..w}` | OUT | `get_status().jog.active_axis` (watch @50ms) |
| `jog.increment` | OUT | `get_status().jog.increment` (watch @50ms) |
| `jog.{x..w}-plus/minus` | IN | `jog_start()`/`jog_stop()` commands |
| `jog.disable` | IN | `get_status().jog.disabled` (watch @50ms) |
| `notifications-clear*` | IN | `clear_notifications()` command |
| `resume-inhibit` | IN | Part of status or separate command |
| `sliders.scale*` | IN | Slider commands (future) |

**Implementation Plan:**

- [x] GMI parser: `@watch`, `@watch_default_rate` annotations on functions
- [x] gmicompile `--server-ws`: Go WebSocket subscribe/push handler
- [x] gmicompile `--client-python-ws`: Python async watch client
- [x] gomc-server: WebSocket endpoint at `/api/v1/watch`
- [x] `axisui.gmi`: IDL with jog/slider/notification watch + set commands
- [x] axis.py: replace HAL `comp` with WebSocket client (WSCompat + AxisuiWatchThread)
- [x] axisui.comp: cmod with HAL pins and GMI callbacks
- [x] HAL config: `axisui.hal` + INI entries for sim configs
- [x] WatchFactory registry: auto-registration via init() + packages.conf
- [x] `modcompile add-gmi`: auto-registration during codegen
- [x] Python `gmi` package: `rest_url()`/`ws_url()` central URL helpers
- [x] Debian packaging: install rules for `gmi/` Python package and `cmod/*.so`
- [x] `loadusr` PID tracking: proper SIGTERM on shutdown for non-HAL child processes

**Future (out of scope for 5.2):**
- NML status channel replacement (same watch infrastructure, different GMI API)
- Multiple simultaneous UI clients (watch supports this by design)
- Remote UI over network (WebSocket works through reverse proxies)

### Step 5.3: PyVCP REST/WebSocket Migration (COMPLETE)

Migrated PyVCP from direct HAL shared memory access to REST + WebSocket,
eliminating the UI's dependency on HAL shared memory. PyVCP panels are now
pure display/input frontends communicating through the gomc-server.

**Architecture:**

A Go module (pyvcpmodule) inside gomc-server owns the HAL component.
The Python frontend is a pure display client:

```
pyvcp (Tk frontend)              gomc-server
    │                               │
    ├─ GET /panel/{name} ─────────►│  Fetch panel info + pin defs
    │                               │
    ├─ WebSocket (persistent) ◄────►│  Pin value push (100ms)
    │   set_pin commands ──────────►│  Write output pin values
    │                               │
    │                          pyvcpmodule (gomod)
    │                               │
    │                          HAL component (real pins)
    │                          via pkg/hal/ Go bindings
```

**Server-Side: pyvcpmodule**

```
src/gomc/internal/pyvcpmodule/
├── module.go        # init() → RegisterModule("pyvcp", factory)
│                    # REST dispatch (get_panel), WS watch (watch_pins, set_pin)
│                    # Panel registry, WatchRegistry for subscriptions
└── panel.go         # XML parsing for 20 widget types → HAL pin creation
                     # autoName counters match Python pyvcp_widgets.py naming
```

Startup flow:
1. gomc-server loads pyvcpmodule via `[HAL]GOMOD = pyvcp` INI directive
2. Module reads XML path from module config → parses XML → extracts pin definitions
3. Creates real HAL component with required pins via `pkg/hal/` Go bindings
4. Registers REST + WebSocket endpoints as API instance "pyvcp"
5. `comp.Ready()` — pins visible to halcmd, connectable in HAL files

**REST Endpoints (registered on apiserver as instance "pyvcp"):**

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/api/v1/pyvcp/panel/{name}` | Fetch panel info (name, XML, pin defs) |

**WebSocket Commands (via `/api/v1/watch`):**

| Command | Purpose |
|---------|---------|
| `watch_pins` | Subscribe to pin value push at 100ms |
| `set_pin` | Write a pin value (name + string-encoded value) |

**Client-Side: PyVCPCompat + pyvcp_compat.py**

Hand-written (not generated) drop-in replacement for `hal.component`:

```
src/gmi/python/pyvcp_compat.py → lib/python/gmi/pyvcp_compat.py (build copy)
```

- `PyVCPCompat` class: `__getitem__`/`__setitem__` interface, backed by WebSocket
- `_WatchThread`: asyncio WebSocket client in background thread
- `_flush_loop()`: batches outgoing `set_pin` writes
- Fetches panel info via REST on init, subscribes to watch_pins
- Widget code (`pyvcp_widgets.py`) requires zero changes

**AXIS Integration:**

`vcpparse.py` has a new `create_vcp_rest()` function that:
1. Fetches panel info from REST (`GET /api/v1/pyvcp/panel/{name}`)
2. Creates `PyVCPCompat` instead of `hal.component`
3. Builds widget tree from XML (existing code path)

AXIS calls `vcpparse.create_vcp_rest(f, compname="pyvcp")` — the return
value is unused since `PyVCPCompat` is self-contained (WebSocket thread).

**HAL Module Elimination from axis.py:**

With PyVCP migrated, the `hal` Python module is no longer imported by axis.py.
All HAL queries now go through the gomc REST API via the `gmi` package:

| Old (hal module, needs shared memory) | New (gmi REST) |
|---------------------------------------|----------------|
| `hal.component("axisui-display")` | Removed (axisui cmod owns pins) |
| `hal.component_exists(name)` | `gmi.component_exists(name)` |
| `hal.pin_has_writer(name)` | `gmi.pin_has_writer(name)` |

**gmi Python Package (`src/gmi/python/__init__.py`):**

Now a hand-written source file (previously an empty `@touch` build artifact).
Contains central helpers used by axis.py and other UI code:

- `rest_url()` / `ws_url()` — URL helpers from `GMC_REST_URL` env var
- `component_exists(name)` — `GET /api/v1/halcmd/components?pattern={name}`
- `pin_has_writer(name)` — `GET /api/v1/halcmd/pins?pattern={name}`, checks `has_writer` field

**halcmd REST Enhancement:**

Added `has_writer` field to the pins endpoint to support `pin_has_writer()`:

- `hal_shim_pin_info_t` C struct: new `int has_writer` field
- `hal_shim_show_pins`: sets `has_writer = (sig->writers > 0)` when pin is linked
- `PinInfo` Go struct: new `HasWriter bool` field (JSON: `"has_writer"`)
- Exposed via `GET /api/v1/halcmd/pins?pattern={name}` response

**Build System:**

- `gmi/codegen/Submakefile`: dedicated copy rule for `__init__.py` (source → build)
- Other GMI Python targets depend on `$(GMI_INIT_DST)` instead of `@touch`
- `pyvcp_compat.py` copy rule (same pattern)
- `packages.conf.in`: pyvcpmodule compiled into gomc-server

**INI Configuration:**

```ini
[HAL]
GOMOD = pyvcp xml=pyvcp_demo1.xml
HALFILE = pyvcp_rest.hal
HALFILE = custom.hal    # was POSTGUI_HALFILE — moved since gomod creates pins before GUI

[DISPLAY]
PYVCP = pyvcp_demo1.xml
```

No `POSTGUI_HALFILE` needed — the Go module creates HAL pins during module
init (before AXIS starts), so `custom.hal` can `net` pins as a regular HALFILE.

**Completed:**
- [x] `internal/pyvcpmodule/module.go` — gomod: REST dispatch, WebSocket watch, panel registry
- [x] `internal/pyvcpmodule/panel.go` — XML parser for 20 widget types, HAL pin creation
- [x] `src/gmi/python/pyvcp_compat.py` — PyVCPCompat + WatchThread (WebSocket client)
- [x] `src/gmi/python/__init__.py` — package init with `rest_url()`, `ws_url()`,
      `component_exists()`, `pin_has_writer()` helpers
- [x] `gmi/codegen/Submakefile` — build rules for `__init__.py` + `pyvcp_compat.py` copy
- [x] `vcpparse.py` — `create_vcp_rest()` function for REST/WS panel creation
- [x] `axis.py` — uses `gmi.component_exists()`, `gmi.pin_has_writer()`, no `import hal`
- [x] `halcmd/cgo.go` — `has_writer` in `hal_shim_pin_info_t` + `PinInfo`
- [x] `halcmd/halcmd.go` — `HasWriter bool` field on `PinInfo`
- [x] `packages.conf.in` — pyvcpmodule entry
- [x] `configs/sim/pyvcp_demo/pyvcp_rest.ini` — sim config (no POSTGUI_HALFILE)
- [x] Pin naming: dial/spinbox/scale always use `autoName()` counters (matches Python)

**Notes:**
- Widget code (`pyvcp_widgets.py`) required zero changes
- No GMI IDL file — pyvcpmodule uses hand-written REST/WS dispatch (not gmicompile-generated)
- The 100ms WebSocket push rate matches existing PyVCP polling interval
- Panel XML is parsed server-side; widgets are built client-side from the same XML
- HAL pins are real HAL pins, fully visible to halcmd and connectable in HAL files
- `axis.py` no longer imports the `hal` Python module at all

### Step 5.4: INI File REST Migration (COMPLETE)

Replace direct INI file parsing in axis.py (`linuxcnc.ini()`) with REST
queries to gomc-server, eliminating the `liblinuxcnc` C extension dependency
for INI access.

**Motivation:**
- axis.py currently uses `linuxcnc.ini(sys.argv[2])` to parse the INI file
  directly from disk via the C `liblinuxcnc` extension
- With HAL removed (Step 5.3), INI is the next `liblinuxcnc` dependency to eliminate
- Remaining `liblinuxcnc` deps (`linuxcnc.stat()`, `linuxcnc.command()`,
  `linuxcnc.error_channel()`) depend on NML and will be removed after the
  server-internal NML→GMI migration

**Scope:**
- 82 `inifile.find()` / `inifile.findall()` calls in axis.py across
  sections: DISPLAY, TRAJ, EMC, RS274NGC, EMCIO, KINS, FILTER, TASK,
  JOINT_N, AXIS_X/A/B/C, GMC
- Read-only — axis.py never writes to INI

**Architecture:**

gomc-server already has a full INI parser (`pkg/inifile/`) and the parsed
INI is available in the launcher. A new REST module exposes it:

```
axis.py                          gomc-server
    │                               │
    ├─ POST /api/v1/ini/query ─────►│  Bulk INI lookup
    │  [{section, key}, ...]        │  returns all values in one response
    │◄──────────────────────────────┤
    │  [{value: "..."}, ...]        │
    │                               │
    │                          internal/inirest/
    │                               │
    │                          pkg/inifile/ (already parsed)
```

**REST Endpoint:**

```
POST /api/v1/ini/query
Content-Type: application/json

[
  {"section": "DISPLAY", "key": "GEOMETRY"},
  {"section": "DISPLAY", "key": "MAX_FEED_OVERRIDE"},
  {"section": "FILTER", "key": "PROGRAM_EXTENSION", "all": true}
]

→ 200 OK
[
  {"value": "XYZABCUVW"},
  {"value": "1.5"},
  {"values": [".nc", ".ngc"]}
]
```

- `all: true` → uses `findall()` semantics, returns `values` array
- Missing keys return `{}` (no `value` field — `omitempty` on the pointer)
- Empty-value keys return `{"value": ""}` (key exists but value is empty)
- Single round-trip for all ~82 lookups at startup (~1-2ms local)

**Implementation Plan:**

1. **Go: `internal/inirest/`** — new REST module, registers as "ini" instance on
   apiserver, single `POST /query` dispatch function, reads from launcher's
   parsed `inifile.INI` (no re-parsing). Registered in `launcher.go` right
   after INI parsing, before `startAPIServer()`.
2. **Python: `gmi.IniFile` class** — lazy per-call REST with local cache.
   Each `.find()` / `.findall()` call issues a single-item POST on first
   access, caches the result, and returns from cache on subsequent calls.
   Separate caches for find (single value) and findall (value list).
3. **axis.py** — replace `inifile = linuxcnc.ini(sys.argv[2])` with
   `inifile = gmi.IniFile()`, all 82 `.find()`/`.findall()` calls work unchanged
4. **Remove `import linuxcnc` for INI** — but keep it for `stat()`, `command()`,
   `error_channel()` until NML migration (future step)

**Python Client (implemented):**

```python
class IniFile:
    def __init__(self):
        self._cache = {}      # (section, key) -> str or None
        self._cache_all = {}  # (section, key) -> list[str]

    def find(self, section, key):
        """Return first value for section/key, or None if not found."""
        cache_key = (section, key)
        if cache_key in self._cache:
            return self._cache[cache_key]
        result = self._query([{"section": section, "key": key}])
        if result and len(result) == 1:
            val = result[0].get("value")
            self._cache[cache_key] = val
            return val
        self._cache[cache_key] = None
        return None

    def findall(self, section, key):
        """Return all values for section/key as a list."""
        cache_key = (section, key)
        if cache_key in self._cache_all:
            return self._cache_all[cache_key]
        result = self._query([{"section": section, "key": key, "all": True}])
        if result and len(result) == 1:
            vals = result[0].get("values", [])
            self._cache_all[cache_key] = vals
            return vals
        self._cache_all[cache_key] = []
        return []

    def _query(self, items):
        """Issue a bulk query to the INI REST endpoint."""
        url = rest_url() + "/api/v1/ini/query"
        data = json.dumps(items).encode("utf-8")
        req = urllib.request.Request(url, data=data,
            headers={"Content-Type": "application/json"}, method="POST")
        with urllib.request.urlopen(req, timeout=5) as resp:
            return json.loads(resp.read())
```

The lazy approach was chosen over bulk prefetch for simplicity — each call
is a single round-trip (~0.1ms localhost), cached after first access.
The `_query()` method accepts a list for future bulk optimization if needed.

**Deliverables:**
- [x] `internal/inirest/inirest.go` — Go REST module with `POST /query`
- [x] `src/gmi/python/__init__.py` — `IniFile` class with bulk fetch + `.find()`/`.findall()`
- [x] `axis.py` — replace `linuxcnc.ini()` with `gmi.IniFile()`, remove INI-related `import`
- [x] Tests for inirest endpoint

**Notes:**
- No GMI IDL file — inirest uses hand-written REST dispatch (same pattern as
  halrest and pyvcpmodule), not gmicompile-generated code
- Other UIs (touchy, gmoccapy, gscreen) also use `linuxcnc.ini()` — the REST
  endpoint benefits them all, but migration is per-UI
- The endpoint is read-only by design (INI is parsed once at startup)
- The `IniFile` class matches `linuxcnc.ini` return types exactly:
  `find()` returns `str | None`, `findall()` returns `list[str]`
- The Go dispatch distinguishes "key not found" (empty JSON object) from
  "key exists with empty value" (`{"value": ""}`) by checking `GetSection()`
  entries when `Get()` returns empty string
- Instance name is `"ini"` (no numeric suffix — instance identity is the name itself,
  consistent with halcmd, pyvcp, and all other singleton APIs)

### Step 5.5: NML Gateway — stat/command/error via GMI (COMPLETE → DELETED)

> **Note (2026):** The `internal/emcgateway/` gomod has been deleted. Its
> functionality is now handled directly by the Go milltask (`internal/task/`),
> which implements the same stat/command/error APIs natively without NML.
> The section below is retained for historical context on the design decisions
> that shaped the current Python client wrappers (`gmi.Stat`, `gmi.Command`,
> `gmi.ErrorChannel`).

Replace the remaining `liblinuxcnc` dependency in axis.py by exposing
`linuxcnc.stat()`, `linuxcnc.command()`, and `linuxcnc.error_channel()`
through GMI REST/WebSocket endpoints. A gateway module translates between
GMI and the existing NML C API, allowing axis.py to drop `import linuxcnc`
entirely without rewriting the task controller internals.

**Motivation:**
- After Steps 5.3 (HAL) and 5.4 (INI), the only `liblinuxcnc` dependency
  left in axis.py is the NML interface: `stat()`, `command()`, `error_channel()`
- These account for ~155 `linuxcnc.*` references (90 constants, 40 stat,
  30 command, 3 error)
- Rewriting the task controller's internal NML usage is months of work;
  a gateway decouples the UI migration from the internal migration
- Once axis.py is on GMI, the internal NML→GMI migration can proceed
  incrementally without UI-visible changes

**Architecture: Gateway Pattern**

```
axis.py (pure REST/WS)       gomc-server
    │                               │
    ├─ WS: watch stat ◄───────────►│  stat push @50ms (position, mode, state)
    │                               │
    ├─ WS/REST: commands ──────────►│  state(), auto(), jog(), mdi(), spindle()
    │                               │
    ├─ WS: watch errors ◄─────────►│  error/info message push
    │                               │
    │                          task module (internal/task)
    │                               │
    │                          Canon API (via generated --server-go bridge)
    │                               │  stat: direct struct → JSON
    │                               │  cmd:  GMI call → NML message → task
    │                               │  err:  NML error poll → watch push
    │                               │
    │                          [future: replace with direct cmod API]
```

The gateway is an **anti-corruption layer**: axis.py codes against the clean
GMI interface. When NML is later replaced internally, only the gateway
implementation changes — axis.py is unaffected.

**Why gomod (not cmod):**
- The NML stat struct is large (~50 fields, nested structs, arrays)
- Go's JSON marshaling handles this naturally
- cgo can call the existing NML C functions (`emcStatusBuffer`, `emcCommandBuffer`)
- Watch channel infrastructure (50ms push) is already proven in Go (axisui, pyvcp)
- A cmod would need manual C→JSON serialization for the stat struct

**Scope — what axis.py uses from `linuxcnc`:**

| Category | Count | `linuxcnc.*` Usage | GMI Replacement |
|----------|-------|--------------------|-----------------|
| Constants | ~90 | `MODE_MANUAL`, `STATE_ON`, `INTERP_IDLE`, `JOG_STOP`, etc. | Pure-Python `gmi.constants` module |
| stat() | ~40 | `s.task_state`, `s.motion_mode`, `s.joints`, `s.position`, etc. | Watch channel: `gmi.Stat` (WS @50ms) |
| command() | ~30 | `c.state()`, `c.auto()`, `c.jog()`, `c.mdi()`, `c.spindle()`, etc. | REST/WS commands: `gmi.Command` |
| error_channel() | ~3 | `e.poll()` → `(kind, text)` | Watch channel: errors pushed via WS |
| positionlogger | ~2 | `linuxcnc.positionlogger(stat, ...)` | New impl using `gmi.Stat` watch data |
| nmlfile | ~2 | `linuxcnc.nmlfile` path | Removed (gateway handles NML internally) |

**GMI IDL Definitions:**

Three IDL files covering the NML interface. Constants are defined as enums
in the API that semantically owns them. The Python client generator produces
`IntEnum` classes, so `linuxcnc.MODE_MANUAL` becomes `TaskMode.MANUAL`.

```gmi
# gmi/idl/emcstat.gmi — Machine status (read-only, watchable)
@api emcstat
@version 1
@prefix "emcstat"
@rest_export true

const MAX_JOINTS = 16
const MAX_AXIS = 9

# Task mode: linuxcnc.MODE_*
enum TaskMode {
    MANUAL = 1
    AUTO = 2
    MDI = 3
}

# Task state: linuxcnc.STATE_*
enum TaskState {
    ESTOP = 1
    ESTOP_RESET = 2
    OFF = 3
    ON = 4
}

# Interpreter state: linuxcnc.INTERP_*
enum InterpState {
    IDLE = 1
    READING = 2
    PAUSED = 3
    WAITING = 4
}

# Exec state: linuxcnc.TASK_EXEC_*
enum ExecState {
    ERROR = 1
    DONE = 2
    WAITING_FOR_MOTION = 3
    WAITING_FOR_MOTION_QUEUE = 4
    WAITING_FOR_IO = 5
    WAITING_FOR_MOTION_AND_IO = 7
    WAITING_FOR_DELAY = 8
    WAITING_FOR_MCODE_HANDLER = 9
    WAITING_FOR_SPINDLE_ORIENTED = 10
}

# Trajectory mode: linuxcnc.TRAJ_MODE_*
enum TrajMode {
    FREE = 1
    COORD = 2
    TELEOP = 3
}

# Kinematics type: linuxcnc.KINEMATICS_*
enum KinematicsType {
    IDENTITY = 1
    FORWARD_ONLY = 2
    INVERSE_ONLY = 3
    BOTH = 4
}

type Position {
    x: f64; y: f64; z: f64
    a: f64; b: f64; c: f64
    u: f64; v: f64; w: f64
}

type JointInfo {
    homed: bool
    homing: bool
    enabled: bool
    fault: bool
    min_soft_limit: f64
    max_soft_limit: f64
    min_hard_limit: bool
    max_hard_limit: bool
    override_limits: bool
    velocity: f64
    input: f64           # joint_actual_position
    output: f64          # joint_position (commanded)
    limit: i32           # bitmask: 1=minHard,2=maxHard,4=minSoft,8=maxSoft
}

type SpindleInfo {
    speed: f64
    direction: i32
    brake: bool
    enabled: bool
    override: f64
    override_enabled: bool
    homed: bool
    orient_state: i32
    orient_fault: i32
}

type AxisInfo {
    velocity: f64
    min_position_limit: f64
    max_position_limit: f64
}

type StatTaskInfo {
    mode: TaskMode
    state: TaskState
    interp_state: InterpState
    exec_state: ExecState
    file: string
    command: string
    line: i32
    motion_line: i32
    current_line: i32
    read_line: i32
    queued_mdi_commands: i32
    optional_stop: bool
    block_delete: bool
    task_paused: bool
    g5x_index: i32
}

type StatMotionInfo {
    mode: TrajMode
    enabled: bool
    in_position: bool
    paused: bool
    feedrate: f64
    rapidrate: f64
    max_velocity: f64
    velocity: f64
    distance_to_go: f64
    dtg: Position
    current_vel: f64
    motion_id: i32
    motion_line: i32
}

type StatFull {
    task: StatTaskInfo
    motion: StatMotionInfo
    position: Position
    actual_position: Position
    joint_actual_position: [MAX_JOINTS]f64
    probed_position: Position
    g5x_offset: Position
    g92_offset: Position
    tool_offset: Position
    rotation_xy: f64
    joints: []JointInfo
    spindle: []SpindleInfo
    axis: []AxisInfo
    active_gcodes: []i32
    active_mcodes: []i32
    active_settings: []f64
    kinematics_type: KinematicsType
    joints_count: i32
    num_extrajoints: i32
    axis_mask: i32
    flood: bool
    mist: bool
    tool_in_spindle: i32
    pocket_prepped: i32
    linear_units: f64
    homed: [MAX_JOINTS]bool
    limit: [MAX_JOINTS]i32
    state: i32           # RCS state for connectivity check
}

@watch true
@watch_default_rate 50ms
func get_stat() -> StatFull
```

```gmi
# gmi/idl/emccmd.gmi — Machine commands (write-only)
@api emccmd
@version 1
@prefix "emccmd"
@rest_export true

# Auto command sub-type: linuxcnc.AUTO_*
enum AutoCmd {
    RUN = 0
    PAUSE = 1
    RESUME = 2
    STEP = 3
    REVERSE = 4
    FORWARD = 5
}

# Jog type: linuxcnc.JOG_*
enum JogType {
    STOP = 0
    CONTINUOUS = 1
    INCREMENT = 2
}

# Spindle command: linuxcnc.SPINDLE_*
enum SpindleCmd {
    OFF = 0
    FORWARD = 1
    REVERSE = -1
    INCREASE = 10
    DECREASE = 11
    CONSTANT = 12
}

func set_state(state: i32) -> i32
func set_mode(mode: i32) -> i32
func auto_cmd(cmd: AutoCmd, line: i32) -> i32
func mdi(command: string) -> i32
func jog(type: JogType, jjogmode: bool, axis_or_joint: i32, velocity: f64, distance: f64) -> i32
func jog_stop(jjogmode: bool, axis_or_joint: i32) -> i32
func spindle(cmd: SpindleCmd, speed: f64, spindle: i32, wait: i32) -> i32
func home(joint: i32) -> i32
func unhome(joint: i32) -> i32
func override_limits() -> i32
func teleop_enable(enable: bool) -> i32
func set_feed_override(rate: f64) -> i32
func set_spindle_override(rate: f64, spindle: i32) -> i32
func set_rapid_override(rate: f64) -> i32
func set_max_velocity(velocity: f64) -> i32
func flood(on: bool) -> i32
func mist(on: bool) -> i32
func brake(on: bool, spindle: i32) -> i32
func abort() -> i32
func task_plan_synch() -> i32
func set_optional_stop(on: bool) -> i32
func set_block_delete(on: bool) -> i32
func load_tool_table() -> i32
func program_open(file: string) -> i32
func wait_complete(timeout: f64) -> i32
```

```gmi
# gmi/idl/emcerror.gmi — Error/info messages (watchable)
@api emcerror
@version 1
@prefix "emcerror"
@rest_export true

# Error kind: linuxcnc.NML_ERROR, OPERATOR_ERROR, etc.
enum ErrorKind {
    NML_ERROR = 1
    NML_TEXT = 2
    NML_DISPLAY = 3
    OPERATOR_ERROR = 11
    OPERATOR_TEXT = 12
    OPERATOR_DISPLAY = 13
}

type ErrorMessage {
    kind: ErrorKind
    text: string
}

@watch true
@watch_default_rate 200ms
func get_errors() -> []ErrorMessage
```

**Implementation Plan:**

1. ✅ **IDL files** — `emcstat.gmi`, `emccmd.gmi`, `emcerror.gmi` written with
   all types, enums, and functions. (gmicompile code generation pending)

2. ✅ **C++ NML shim: `emc/nml_intf/nml_shim.cc` + `nml_shim.h`** — `extern "C"`
   wrapper around the NML C++ API. Covers stat polling, 25 commands, error
   polling, init/shutdown lifecycle. Not yet compiled into gomc-server.

3. ✅ **Gateway gomod: `internal/emcgateway/`** — Hand-written (not yet
   IDL-generated). 4 files: `module.go`, `types.go`, `convert.go`,
   `commands.go`. Registers REST meta + watch factories for emcstat/emccmd/
   emcerror. Uses cgo to call the NML shim. Not yet in packages.conf.

4. ✅ **Python wrapper classes** — Hand-written with sensible defaults for
   all fields when watch data isn't available yet:
   - `gmi.Stat` — WS watch @50ms, `__getattr__` maps flat names to nested JSON
   - `gmi.Command` — REST POST to emccmd endpoints
   - `gmi.ErrorChannel` — WS watch @200ms, deque-based message queue
   - `gmi.constants` — flat constants (`MODE_MANUAL=1`, etc.) for `import *`
   - Submakefile rules copy all 4 files to `lib/python/gmi/`

5. ✅ **axis.py migration** — ~134 `linuxcnc.*` references replaced:
   - `s = gmi.Stat()`, `c = gmi.Command()`, `e = gmi.ErrorChannel()`
   - All `linuxcnc.CONSTANT` → bare `CONSTANT` (via `from gmi.constants import *`)
   - `except linuxcnc.error` → `except Exception`
   - `linuxcnc.nmlfile` handling removed
   - Kept: `linuxcnc.version`, `linuxcnc.positionlogger` (deferred to Step 6)

6. ✅ **Build system** — Remaining work:
   - Compile `nml_shim.cc` and link into gomc-server (CGO_LDFLAGS) ✅
   - Task module registers APIs directly at startup (no separate gomod) ✅
   - Canon bridge generated via `--server-go` from `canon.gmi` (143 functions) ✅

7. 🔲 **Tests** — Gateway endpoint tests (deferred to Step 6)

**NML Connection Lifecycle:**

The gateway connects to NML as part of gomc-server startup (same process
that runs the task controller). No external NML socket — it's in-process
shared memory access, same as the current `linuxcnc.stat()` but from Go
instead of Python.

**Design Decisions (April 2026):**

1. **C++ NML bridge**: C shim layer (`nml_shim.cc` + `nml_shim.h`) with
   `extern "C"` wrappers around the NML C++ API. Compiled as a separate
   object and linked into gomc-server. cgo calls the C shim, not C++ directly.

2. **IDL-generated code**: All three APIs (emcstat, emccmd, emcerror) are
   defined as `.gmi` IDL files and processed by gmicompile. This generates:
   - Go server dispatch code (`--server-go` / `--server-ws`)
   - Python REST/WS clients (`--client-python` / `--client-python-ws`)
   - Typed enum constants as Python `IntEnum` classes
   This aligns with the future internal NML→GMI migration: the same IDL
   definitions will later describe the direct cmod API.

3. **Constants via GMI enums**: All `linuxcnc.*` constants (MODE_MANUAL,
   STATE_ON, JOG_STOP, etc.) are defined as IDL enums in the appropriate
   `.gmi` file. The Python client generator produces `IntEnum` classes.
   axis.py imports typed enum members instead of bare integers.

4. **Full StatFull struct**: Expose the complete stat structure, not just
   what axis.py reads. Other UIs (touchy, gmoccapy, gscreen) will migrate
   later and need the same endpoints.

5. **Delta compression implemented**: Per-connection delta encoding in
   `pushLoop` (ws_handler.go). First message = full snapshot, subsequent
   messages contain only changed top-level JSON keys. Python `stat.py`
   uses `self._data.update(data)` to merge deltas. Reduces WS bandwidth
   to near-zero when machine is idle.

6. **positionlogger complete**: Server-side `poslog.go` samples NML stat
   at 100Hz, client-side `positionlogger.py` handles vertex9/colinearity/
   OpenGL rendering. `_glhelpers.so` C extension extracted for GL geometry.
   `import linuxcnc` fully removed from axis.py.

7. **NML semaphore EINTR fix**: `libnml/os_intf/_sem.c` had missing EINTR
   retry loops on `semtimedop`/`semop` calls. Go runtime's SIGURG for
   goroutine preemption interrupted the syscalls, causing spurious
   "Can't take semaphore" errors. Fixed with standard POSIX retry pattern.

8. **pprof profiling**: Permanent `/debug/pprof/` endpoints added to the
   API server for runtime CPU/memory profiling. Zero overhead when idle.

9. **Task module auto-registration**: The task module registers its APIs
   (stat/command/error/canon) at startup, eliminating the need for separate
   gateway modules. All ~80 axis sim configs work without modification.

10. **Tool table REST API**: `GET/PUT /api/v1/tools/` endpoints backed by
    `tooldata_mmap` via a C shim (`tool_shim.h/cc`). Python `gmi.tools`
    module and `tooledit_widget.py` adapted. Returns entries in mmap index
    order (not re-indexed by pocket number).

11. **NML process name kept as `xemc`**: The gateway connects to NML as
    process `xemc` (standard NML role name for display clients). Not
    renamed to `gomc` since NML is being replaced — not worth the churn
    of modifying `linuxcnc.nml` + all custom configs.

**Deliverables:**
- [x] `gmi/idl/emcstat.gmi` — stat types, enums (TaskMode, TaskState, etc.), watch function
- [x] `gmi/idl/emccmd.gmi` — command enums (AutoCmd, JogType, SpindleCmd), command functions
- [x] `gmi/idl/emcerror.gmi` — error types/enums, error watch function
- [x] `emc/nml_intf/nml_shim.cc` + `nml_shim.h` — C shim wrapping NML C++ API with `extern "C"`
- [x] `internal/emcgateway/` — gomod implementing callbacks via cgo→NML shim (DELETED — replaced by internal/task)
- [x] `src/gmi/python/stat.py` — `Stat` class (watch-based, drop-in for `linuxcnc.stat()`)
- [x] `src/gmi/python/command.py` — `Command` class (REST/WS, drop-in for `linuxcnc.command()`)
- [x] `src/gmi/python/error.py` — `ErrorChannel` class (watch-based, drop-in for `linuxcnc.error_channel()`)
- [x] `src/gmi/python/constants.py` — flat constants for backward compat (`from gmi.constants import *`)
- [x] `gmi/codegen/Submakefile` — copy rules for Python wrapper files
- [x] Build system: `nml_shim.cc` in liblinuxcnc.a, `packages.conf` entry, gomc-server deps
- [x] axis.py — stat/command/error/positionlogger/constants all use `gmi.*`;
      `import linuxcnc` fully removed. GL helpers extracted to standalone
      `_glhelpers.so` C extension (no NML/linuxcnc deps).
- [x] `src/gmi/python/positionlogger.py` — `PositionLogger` class (WS-based, drop-in
      for `linuxcnc.positionlogger`); includes client-side vertex9, colinearity reduction,
      and OpenGL rendering via ctypes interleaved arrays
- [x] `internal/emcgateway/poslog.go` — server-side position sampler goroutine;
      polls NML stat at configurable rate, dedup on position+motion_type,
      buffers raw 9-axis points for WS delivery via `get_positions` watch
- [x] `nml_shim.h/cc` — added `motion_type` field to `nml_stat_t`
- [x] `gmi/__init__.py` — `gmi.version` (from `LINUXCNCVERSION` env var),
      `gmi.positionlogger()` factory function
- [ ] Generate Go dispatch from IDL files (gmicompile runs — deferred, hand-written gateway sufficient)
- [x] End-to-end: axis.py starts and runs with all sim configs via task module APIs

**Position Logger Architecture:**

The position logger replaces `linuxcnc.positionlogger` with a split
server/client architecture:

- **Server** (`poslog.go`): Goroutine samples NML stat at configurable rate
  (default 100Hz). Extracts `position - toolOffset` (raw 9-axis) and
  `motion_type` (0-5). Dedup: skips points where both are unchanged.
  Points accumulate in a ring buffer (10K max, drops oldest 10% on overflow).
  Delivered via the `get_positions` WatchFuncMeta at 200ms rate.

- **Client** (`positionlogger.py`): Subscribes to `get_positions` watch.
  Processes each chunk: applies `vertex9()` geometry transformation (ported
  from C), colinearity reduction, motion_type→color mapping, and stores in
  a ctypes `_LoggerPoint` array matching the C struct layout. OpenGL rendering
  uses `glVertexPointer`/`glDrawArrays` on the interleaved vertex+color array.

- **Protocol**: Standard WatchAPI — client subscribes, server pushes updates.
  Logger start/stop/clear via WS command messages (`start_logger`,
  `stop_logger`, `clear_logger`) dispatched on the `emcstat` API.

- **Key decision**: `vertex9()` runs client-side because it depends on
  `rotation_offsets` state (`gui_respect_offsets`, `gui_rot_offsets`) that
  is set by the Python GUI. Server sends raw 9-axis positions.

- **Tool table**: Implemented as REST API backed by `tooldata_mmap` via
  C shim (`tool_shim.h/cc`). `GET /api/v1/tools/` returns all entries in
  mmap index order. `PUT /api/v1/tools/reload` triggers tool table reload.
  Python `stat.tool_table` property fetches from REST. `tooledit_widget.py`
  adapted to use `gmi` REST instead of direct mmap access.

### Step 5.6: TypeScript Client Generation (`--client-ts`)

Add TypeScript client code generation to gmicompile, enabling Vue web UIs
to consume any GMI API. This replaces the Dart client generator (removed)
and is a prerequisite for Step 5.8 (Halscope Web UI) and all future
web-based UI tools (halmeter, halshow, EtherCAT configurator, etc.).

**Rationale for TypeScript over Dart/Flutter:**

Flutter's Linux desktop embedding has fundamental shutdown lifecycle issues
(multi-threaded engine vs. single-threaded GTK GObject dispose cascade)
that cannot be cleanly resolved. The web UI approach eliminates native
dependencies entirely:
- gomc-server already provides REST + WebSocket APIs
- Static files served from `share/gomc/webapp/<app>/`
- Works as local desktop tool (browser window) and remote monitoring
- Single TypeScript client generator replaces both `--client-dart` and
  `--client-dart-ws` (TypeScript handles both REST and WebSocket natively)

**Generated Output (per `.gmi` file):**

For an API `halscope` with `@rest_export true`, `--client-ts` generates
a single TypeScript module with:

1. **Constants** — exported `const`:
   ```typescript
   export const MAX_CHANNELS = 16;
   export const MAX_SAMPLES = 65536;
   ```

2. **Enums** — TypeScript `enum` with integer values:
   ```typescript
   export enum ScopeState {
     Idle = 0, Init = 1, PreTrig = 2, TrigWait = 3,
     PostTrig = 4, Done = 5, Reset = 6,
   }
   ```

3. **Typed interfaces** — for all struct types:
   ```typescript
   export interface ScopeStatus {
     state: ScopeState;
     samples: number;
     rec_len: number;
     pre_trig: number;
     sample_len: number;
   }
   ```

4. **REST client class** — typed methods per API function:
   ```typescript
   export class HalscopeClient {
     constructor(private baseUrl: string) {}

     async configure(config: CaptureConfig): Promise<number> { ... }
     async setChannel(ch: ChannelConfig): Promise<number> { ... }
     async getStatus(): Promise<ScopeStatus> { ... }
   }
   ```

5. **WebSocket client class** — subscribe/unsubscribe with typed callbacks,
   binary frame support for `@binary true` watch functions:
   ```typescript
   export class HalscopeWsClient {
     constructor(private wsUrl: string) {}

     watchState(onData: (status: ScopeStatus) => void): void { ... }
     watchSamples(onData: (data: ArrayBuffer) => void): void { ... }
     call(func: string, args?: Record<string, unknown>): Promise<unknown> { ... }
     dispose(): void { ... }
   }
   ```

**IDL → TypeScript naming conventions:**
- Type names: `PascalCase` (matches IDL)
- Field names: `snake_case` (matching server-side JSON keys)
- Enum values: `PascalCase` (from IDL `UPPER_CASE`)
- Constants: `UPPER_CASE` (from IDL)

**Codec details:**
- `string` → `string`, `i32`/`i64` → `number`, `f64` → `number`,
  `bool` → `boolean`, `[]T` → `T[]`, `[N]T` → `T[]`,
  `T?` → `T | null`
- Binary watch payloads: raw `ArrayBuffer`, client interprets via
  `DataView` (documented per-API)

**Implementation Plan:**

1. [x] **TS codegen in gmicompile** — `--client-ts` flag, generates
       enums, interfaces, REST client, WS client in a single `.ts` file
2. [x] **Test with existing APIs** — Generate TS clients for `halcmd`
       and `emcstat`, validate against running gomc-server
3. [x] **Output convention** — `src/webapp/<app>/src/generated/<api>.ts`

**Deliverables:**
- [x] `--client-ts` in gmicompile (enums, interfaces, REST client, WS client)
- [x] Generated TS clients for existing APIs as validation
- [x] Tests

### Step 5.7: Web App Infrastructure

Add static file serving to gomc-server so Vue web apps can be served
alongside the REST/WebSocket API on the same port.

**URL Scheme:**

```
http://localhost:5080/                    → app index (lists available apps)
http://localhost:5080/app/halscope/       → share/gomc/webapp/halscope/index.html
http://localhost:5080/app/halscope/assets/→ static files
http://localhost:5080/api/v1/...          → REST API (unchanged)
http://localhost:5080/api/v1/watch        → WebSocket (unchanged)
```

**Directory Layout:**

Build outputs (static files) are installed under `share/gomc/webapp/`:
```
share/gomc/webapp/
├── halscope/
│   ├── index.html
│   └── assets/
│       ├── index-xxxxx.js
│       └── index-xxxxx.css
├── ethercat-config/
│   └── ...
└── hal-viewer/
    └── ...
```

Source lives under `src/webapp/<app>/`:
```
src/webapp/halscope/
├── src/
│   ├── App.vue
│   ├── main.ts
│   ├── generated/          ← output of --client-ts
│   │   └── halscope.ts
│   └── components/
│       └── Waveform.vue
├── index.html
├── package.json
├── tsconfig.json
├── vite.config.ts
└── svelte.config.js        ← (not used, Vue project)
```

**Implementation in gomc-server:**

1. Add `EMC2WebAppDir` to `config/paths.go` (compile-time ldflags)
2. In `apiserver.NewServer()`, register `http.FileServer` for each app
   directory found under the webapp root
3. SPA fallback: serve `index.html` for any path under `/app/<name>/`
   that doesn't match a real file
4. Root `/` serves an index page listing available apps

**RIP vs Installed paths:**
- RIP: `share/gomc/webapp/` (relative to `EMC2_HOME`)
- Installed: `$(datadir)/gomc/webapp/`

**Implementation Plan:**

1. [x] Add `EMC2WebAppDir` config variable + ldflags
2. [x] Static file handler in `apiserver` with SPA fallback
3. [x] Root index handler listing discovered apps
4. [x] Wire into Makefile: `npm run build` → copy `dist/` to `share/gomc/webapp/<name>/`

**Deliverables:**
- [x] Static file serving in gomc-server
- [x] Webapp directory convention documented
- [x] Build system integration (Vite build + install)

### Step 5.8: Halscope — gomod with Embedded C RT + Vue Web UI (COMPLETE)

Replaced the old shared-memory GTK3 halscope with a unified gomod that embeds
the RT sampling engine via cgo and serves a Vue 3 web UI.

**Previous Architecture (removed):**
- `scope_rt.c` — standalone RT module, shared memory ring buffer
- `scope.c` + `scope_*.c` — GTK3 GUI mapping same shared memory
- Tight coupling via `SHMPTR` offsets, fragile state machine

**New Architecture:**

```
  Vue halscope (browser/gmcui)          other clients (future)
       │                                        │
       └──────── WebSocket + REST ──────────────┘
                          │
                    gomc-server
                          │
                    halscope gomod (internal/halscope/)
                          │
                    ├── Go: module lifecycle, REST/WS dispatch, state persistence
                    └── C (via cgo): RT sampling engine (halscope_rt.h/halscope_rt.c)
                          │
                    halscope.sample (RT function, added to HAL thread)
```

**Key Design Decisions:**

1. **gomod with embedded C RT** — not a separate cmod. The RT sampling code
   (`halscope_rt.c`) is compiled via cgo into the gomc-server binary. The Go
   module (`module.go`) owns the lifecycle, REST/WS API, and state persistence.
   The C code handles only the hot loop: sample capture and trigger detection.

2. **No GMI IDL** — hand-written REST/WS dispatch (same pattern as pyvcpmodule,
   inirest). The API is specific to halscope and unlikely to be consumed by
   other modules via direct calls.

3. **uPlot** — lightweight (~35KB) charting library for waveform rendering.
   Chosen over raw Canvas for built-in zoom/pan, axis labeling, and series
   management. Data is in "divisions" space (-5 to +5 vertical).

4. **gmcui native container** — GTK3+WebKit2 wrapper (~150 LOC) providing a
   native window for the web UI. Detected via `basename(argv[0])` symlink
   (e.g., `halscope` → `gmcui`). DevTools enabled.

5. **State persistence** — Scope configuration (thread, channels, trigger,
   continuous mode) is saved to a JSON file on every config change and on
   `Stop()`. Path configurable via `[HAL]SCOPE_STATE_STORAGE` in the INI file.
   State is restored in `Start()` (after all HAL pins exist).

6. **Client-side capture save/load** — CSV export with `#` comment headers
   (sample period, trigger info), semicolon separator, pin names with `[TYPE]`
   annotations. Load reconstructs waveforms without server involvement.

**Server-Side Implementation:**

```
src/gomc/internal/halscope/
├── module.go          # gomod: init(), New(), Start(), Stop(), REST/WS dispatch
│                      # State save/load, watch loop (100ms status push)
│                      # Dispatches: configure, set_channel, clear_channel,
│                      # set_trigger, arm, force_trigger, reset, get_status,
│                      # set_continuous, set_sample_period_mult
├── halscope_rt.h      # C header: RT data structures, state machine enums
│                      # scope_data_t union (bit/u32/s32/float), channel config,
│                      # trigger config, sample buffer management
├── halscope_rt.c      # C RT code: halscope_sample() function (runs in HAL thread)
│                      # State machine: IDLE→PRE_TRIG→TRIG_WAIT→POST_TRIG→DONE
│                      # Per-channel sampling, trigger detection (rising/falling edge)
│                      # Supports bit, u32, s32, float HAL types
└── testrt/            # C unit tests for RT code (standalone, no Go)
    ├── test_halscope_rt.c
    └── testmock/hal.h # Mock HAL headers for testing
```

**State Machine (C RT code):**

```
IDLE → (arm) → PRE_TRIG → (pre-trig samples done) → TRIG_WAIT
     → (trigger condition met OR force) → POST_TRIG
     → (post-trig samples done) → DONE → (reset/re-arm) → IDLE
```

Continuous mode: DONE → automatic re-arm → PRE_TRIG (no manual reset needed).

**REST Endpoints (instance "halscope"):**

| Method | Path | Purpose |
|--------|------|---------|
| POST | `/configure` | Set thread, max channels, sample period mult |
| POST | `/set_channel` | Assign HAL pin/sig/param to channel slot |
| POST | `/clear_channel` | Remove channel assignment |
| POST | `/set_trigger` | Configure trigger (channel, level, edge, auto) |
| POST | `/arm` | Start capture |
| POST | `/force_trigger` | Force immediate trigger |
| POST | `/reset` | Return to IDLE |
| POST | `/set_continuous` | Enable/disable continuous mode |
| POST | `/set_sample_period_mult` | Change sample decimation |
| GET | `/status` | Current state, channels, trigger config |

**WebSocket (via `/api/v1/watch`):**

- `watch halscope/status` — 100ms push of state + sample data (binary samples
  included when capture is complete)
- Client subscribes once; receives JSON status + base64-encoded sample buffer

**Client-Side Implementation:**

```
src/webapp/halscope/
├── index.html
├── package.json           # Vue 3, uPlot, vite
├── vite.config.ts
├── tsconfig*.json
└── src/
    ├── main.ts
    ├── App.vue
    ├── stores/
    │   └── scope.ts       # Pinia store: WS connection, chart data, save/load
    ├── components/
    │   ├── ScopeChart.vue         # uPlot wrapper, data transformation
    │   ├── ScopeToolbar.vue       # Arm/Reset/Run Mode/Save/Load buttons
    │   ├── ChannelSetup.vue       # Channel config panel (pin selection)
    │   ├── TriggerControls.vue    # Trigger config (source, level, edge)
    │   ├── HorizontalControls.vue # Zoom + position sliders
    │   └── VerticalControls.vue   # Per-channel gain + offset sliders
    └── generated/
        ├── halscope_client.ts     # Generated REST client (from --client-ts)
        └── halscope_watch_client.ts # Generated WS watch client
```

**Native Container (gmcui):**

```
src/emc/usr_intf/gmcui/
├── gmcui.c        # GTK3+WebKit2, profile table, symlink detection
└── Submakefile    # Conditional on BUILD_WEBKIT2GTK=yes
```

Profile table maps symlink names to webapp paths:
```c
{ "halscope", "/app/halscope/", "HAL Oscilloscope", 1280, 800 }
```

Build produces `bin/gmcui` + `bin/halscope` symlink. CLI supports
`--url`, `--title`, `--width`, `--height` for custom use.

**State Persistence Format (version 1):**

```json
{
  "version": 1,
  "config": {
    "threadName": "servo-thread",
    "maxChannels": 4,
    "samplePeriodMult": 1,
    "continuous": true
  },
  "channels": [
    {"slot": 1, "pinName": "joint.0.motor-pos-cmd"}
  ],
  "trigger": {
    "channel": 1,
    "level": 0.0,
    "edge": 1,
    "autoTrig": true
  }
}
```

**Build Dependencies:**

| Dependency | Build-time | Runtime | configure.ac check |
|------------|-----------|---------|-------------------|
| Node.js + npm | Yes (Vite build) | No | `HAVE_NODEJS` |
| webkit2gtk-4.1 | Yes (gmcui) | Yes | `HAVE_WEBKIT2GTK` (pkg-config) |

Both are optional — gomc-server and the web UI work in a browser without them.

**Completed:**
- [x] RT sampling engine (`halscope_rt.h` / `halscope_rt.c`) with C unit tests
- [x] Go module (`internal/halscope/module.go`) — full REST/WS API
- [x] State persistence (JSON, INI-configured path, save on change + Stop)
- [x] Vue 3 web app with uPlot charting, channel/trigger/horizontal/vertical controls
- [x] Generated TypeScript clients (REST + WS watch)
- [x] Mouse wheel on all sliders, hover tooltip with cursor dot, drag rubberband
- [x] CSV capture save/load (client-side, with legacy format support)
- [x] gmcui native WebKit container with halscope symlink
- [x] Old halscope removed: `scope.c`, `scope_*.c`, `scope_rt.c`, `scope_shm.h` (git rm)
- [x] `scope_rt` RTMODULE removed from Makefile
- [x] All `loadrt scope_rt` / `loadusr halscope` references cleaned from configs
- [x] `configure.ac`: Node.js/npm + webkit2gtk-4.1 checks (replaces Flutter deps)
- [x] Debian packaging: `libwebkit2gtk-4.1-dev` (build), `libwebkit2gtk-4.1-0` (runtime), `nodejs`, `npm`
- [x] `.gitignore`: `configs/**/halscope_state.json`

**Notes:**
- No Flutter code was ever merged — Flutter evaluation was abandoned in favor of
  Vue 3 + native WebKit container (simpler build, smaller footprint, web-native)
- The RT code is tested standalone via a C test harness with mock HAL headers,
  independently of the Go module
- Continuous mode auto-rearms after DONE, providing oscilloscope-like live view
- Trigger supports rising/falling edge detection on any channel type (bit/s32/u32/float)
- Sample period multiplier allows decimation (sample every Nth thread invocation)
- The gomod is loaded via `load halscope` in HAL config files

### Step 5.9: Halshow — Vue Web UI (COMPLETE)

Replaced the old Tcl halshow (`tcl/bin/halshow.tcl`) with a Vue 3 web UI that
uses the existing halcmd REST API. No new backend IDL — reuses the halcmd API
endpoints already served by `internal/halrest/`.

**Previous Architecture (removed):**
- `tcl/bin/halshow.tcl` (1357 lines) — Tcl/Tk GUI with BWidget tree
- Direct `halcmd` subprocess calls for all queries
- `bin/halmeter` — standalone GTK meter (C, `meter.c` + `miscgtk.c`)

**New Architecture:**

```
  Vue halshow (browser/gmcui)
       │
       └──── REST (halcmd API at /api/v1/halcmd/...)
                    │
              gomc-server
                    │
              internal/halrest/ (existing halcmd REST handler)
```

**Features:**

1. **Tree browser** — hierarchical pin/signal/param/component tree with
   expand/collapse. Click node name = show overview (non-leaf) or detail (leaf).
   Double-click leaf = add to watch.

2. **Watch panel** — live-updating value table with explicit "Set" button per row.
   Set dialog with TRUE/FALSE toggle for bit types. Set button hidden for: OUT
   pins, linked pins, RO params, signals with writer pins.

3. **Node overview** — clicking a non-leaf tree node shows all child pins in a
   table (Name, Value, Type, Dir, Signal). "+W" button per pin, "+ Watch All"
   button. Shows "✓" indicators for already-watched items.

4. **halcmd console** — terminal-like command panel with history display. Supports
   show/getp/gets/setp/sets/net/linkps/unlinkp/newsig/delsig/loadrt/unloadrt/
   start/stop/status/help. Color-coded output (green) and errors (red).

5. **Detail panel** — full pin/param/signal info with "Watch" button (shows
   "✓ Watched" when already in watch list).

**Client-Side Implementation:**

```
src/webapp/halshow/
├── index.html
├── package.json           # Vue 3, vite
├── vite.config.ts
├── tsconfig*.json
└── src/
    ├── main.ts
    ├── App.vue
    ├── stores/
    │   └── halshow.ts     # Pinia store: tree, watch, cmd history, node overview
    ├── components/
    │   ├── TreePanel.vue          # Tree browser container
    │   ├── TreeNodeItem.vue       # Recursive tree node (arrow toggle, name click)
    │   ├── DetailPanel.vue        # Leaf node detail view
    │   ├── NodeOverview.vue       # Non-leaf node pin table
    │   ├── WatchPanel.vue         # Live watch with Set dialog
    │   └── HalcmdPanel.vue        # Command console with history
    └── generated/
        └── halcmd_client.ts       # Generated TypeScript REST client
```

**Backend Changes:**

- `internal/halrest/halrest_impl.go` — `GetPin()`, `GetParam()`, `GetSignal()`
  enhanced to return full metadata (direction, signal, owner, linked status)
  using `halcmd.Show()` parsing instead of bare `GetP()`/`PType()`
- `internal/halrest/halrest.go` — `watchItems()` expanded to return pins +
  signals + params (previously only pins). Signals with writer pins marked
  `linked: true` to suppress Set button in frontend.

**gmcui Integration:**

Profile entry in `gmcui.c`:
```c
{ "halshow", "/app/halshow/", "HAL Configuration", 1024, 768 }
```

`bin/halshow` symlink → `gmcui` → opens halshow webapp.

**AXIS Menu Integration:**

```tcl
.menu.machine add command -command {exec halshow &}
```

**Removed Files:**
- `tcl/bin/halshow.tcl` — old Tcl GUI
- `tcl/halshow_icon.png` — old icon
- `src/hal/utils/meter.c` — halmeter source
- `src/hal/utils/miscgtk.c` + `miscgtk.h` — GTK helpers (only used by halmeter)
- `bin/halmeter` — built binary
- `docs/man/man1/halmeter.1` — man page
- `docs/src/hal/images/halmeter-*.png` — documentation images

**Completed:**
- [x] Vue 3 web app with tree browser, watch, detail, overview, halcmd console
- [x] Generated TypeScript halcmd client (reuses existing halcmd.gmi)
- [x] Backend enhanced: full pin/param/signal metadata in REST responses
- [x] Watch panel: Set dialog, bit toggle, canSet() logic (hides for OUT/linked/RO)
- [x] Node overview: child pin table, +W per pin, +Watch All
- [x] halcmd console: parse+execute, history, color-coded output
- [x] gmcui native container with halshow symlink
- [x] AXIS menu integration (`exec halshow &`)
- [x] Old halshow.tcl, halmeter, and associated files removed
- [x] Build system cleaned (Submakefiles, debian packaging)

### Step 5.10: Emccalib — Live Calibration Tuning (COMPLETE)

Replace the old Tcl emccalib (`tcl/bin/emccalib.tcl`) with a gomod + Vue 3 web UI
that discovers tunable HAL parameters from INI/HAL file references, allows live
tuning via HAL setp, and saves changes back to the correct INI file(s).

**Previous Architecture (to be removed):**
- `tcl/bin/emccalib.tcl` — Tcl/Tk GUI with BWidget NoteBook tabs
- Direct `hal getp`/`hal setp` subprocess calls
- Text-widget-based INI file manipulation for save-back
- `emc_ini` calls for INI variable substitution in HAL pin names

**New Architecture:**

```
  Vue emccalib (browser/gmcui)
       │
       └──── REST (/api/v1/emccalib/...)
                    │
              gomc-server
                    │
              internal/emccalib/ (gomod)
                    │
              ┌─────┴─────┐
         pkg/inifile    internal HAL API
       (with provenance)  (pin read/write)
                    │
              setp interceptor
        (records INI→pin mappings during HAL load)
```

**Design Decisions:**

1. **Self-contained gomod** — emccalib owns write-back. The inirest module stays
   read-only. Write requests are validated against the discovered tunable list —
   only discovered `{section, key}` pairs are writable.

2. **Lifecycle-gated security** — if the emccalib gomod isn't loaded, no INI write
   endpoints exist. Loaded via HAL `load` command like other gomods.

3. **Interceptor-based discovery** — instead of scanning raw HAL files (which would
   miss Tcl conditionals, loops, sourced files, and template expansions), the setp
   handler in halcmd/halrest records INI→pin mappings at execution time. When a
   `setp` command resolves a `[SECTION]KEY` INI reference during HAL file loading,
   the mapping is recorded:
   ```
   {pin: "pid.0.Pgain", section: "JOINT_0", key: "P", ini_value: 150.0}
   ```
   The emccalib gomod queries this registry on load — no file scanning needed.
   This is correct regardless of how the HAL files were generated or processed.

4. **INI provenance tracking** — `pkg/inifile` enhanced with `SourceFile` and
   `SourceLine` per `Entry`, so write-back targets the correct file when
   `#INCLUDE` directives are used.

5. **Build-time enable** — `./configure --enable-emccalib` (default: yes when Go
   available), filtered via `@GOMOD:EMCCALIB@` in `packages.conf.in`.

6. **No WebSocket/watch needed** — tunable values are PID gains, velocities,
   accelerations — they only change when the user explicitly clicks Test. The UI
   fetches tunables via plain REST GET on load, and re-fetches after each set_pin
   call. No real-time push required.

**IDL Definition (`gmi/idl/emccalib.gmi`):**

```gmi
@api emccalib
@version 1
@prefix "emccalib"
@rest_export true

type TunableItem {
    section: string
    key: string
    hal_pin: string
    value: f64
    ini_value: f64
}

type TunableSection {
    name: string
    suffix: string
    items: []TunableItem
}

func get_tunables() -> []TunableSection

func set_pin(section: string, key: string, value: f64) -> bool

func save_ini() -> bool

func revert(section: string, key: string) -> bool
```

**Backend (`internal/emccalib/module.go`):**

- `init()` registers `"emccalib"` with `gomc.RegisterModule()`
- On load: queries the setp interceptor registry for all recorded INI→pin
  mappings, groups by section/suffix, enriches with INI provenance (source file
  + line from `pkg/inifile`)
- `get_tunables()` — returns discovered sections with current HAL pin values
  (plain REST GET, re-fetched on demand by the UI)
- `set_pin()` — validates `{section, key}` against discovered list, calls
  internal HAL setp API to apply value immediately
- `save_ini()` — writes all changed values back to their respective source
  files (respecting `#INCLUDE` provenance), creates `.bak` backup first
- `revert()` — restores original INI value to the HAL pin

**Setp Interceptor (in halcmd/halrest INI substitution path):**

When `setp` resolves a `[SECTION]KEY` INI reference during HAL file execution,
record the mapping in an in-memory registry:
```go
type IniPinMapping struct {
    Pin      string  // "pid.0.Pgain"
    Section  string  // "JOINT_0"
    Key      string  // "P"
    IniValue float64 // value at load time
}
```
This registry accumulates during the entire HAL loading phase. The emccalib
gomod reads it when initialized — guaranteed to capture all tunables regardless
of Tcl/template processing.

**Convenience HAL file (`lib/hallib/emccalib.hal`):**

```hal
load emccalib
```

Users add `HALFILE = emccalib.hal` to their `[HAL]` section to enable.
The file is found via `HALLIB_PATH` resolution.

**INI Parser Enhancement (`pkg/inifile`):**

- Add `SourceFile string` and `SourceLine int` fields to `Entry` struct
- Populated during parse, including through `#INCLUDE` chains
- Enables write-back to target the correct file

**Vue Web App (`src/webapp/emccalib/`):**

- Tabbed UI: sections as tabs (JOINT_0..N, AXIS_X..W, SPINDLE_0..N, etc.)
- Per-item: INI name, current HAL value, entry field for new value
- Buttons: Test (apply to HAL), Revert (restore original), Save (write INI)
- Fetches tunables via REST GET; re-fetches after Test/Revert to show updated values
- Generated TypeScript client from `emccalib.gmi`

**Deliverables:**
- [x] `pkg/inifile` — provenance tracking (`SourceFile`, `SourceLine` per entry)
- [x] Setp interceptor (`internal/calibreg/`) — record INI→pin mappings during HAL load
- [x] `gmi/idl/emccalib.gmi` — IDL definition
- [x] Generated dispatch: `gomc/generated/gmi/emccalibapi/` (server-go output)
- [x] Generated TypeScript client: `src/webapp/emccalib/src/generated/`
- [x] `internal/emccalib/module.go` — gomod implementation
- [x] `lib/hallib/emccalib.hal` — convenience HAL file
- [x] `src/webapp/emccalib/` — Vue 3 web app
- [x] Build integration: `configure.ac` (`--enable-emccalib`), `packages.conf.in`,
      `Submakefile` rules
- [x] gmcui profile entry + `bin/emccalib` symlink
- [x] Sim config: `configs/sim/emccalib/` (3-axis servo sim with PID tunables)
- [x] UI integration: axis, gmoccapy, gscreen, qtvcp, tklinuxcnc menus updated
- [x] Old Tcl source removed (`tcl/bin/emccalib.tcl`)

### Step 6: Polish (NOT STARTED)
- [ ] Error handling standardization
- [ ] Logging/tracing
- [ ] Performance optimization
- [ ] Documentation
- [x] Launcher REST server reads listen URL from INI file (`[GMC]REST_ADDR`, default `localhost:5080`)

## Step 7: Remove Go Plugins — Compile-In Architecture (COMPLETE)

### Motivation

Go's `plugin.Open` mechanism has fundamental problems:
- **Version fragility**: Plugin must be built with exact same Go version and
  dependency versions as the host binary. Any mismatch → runtime panic.
- **No unload**: `plugin.Open` has no `Close`. Memory grows, can't hot-swap.
- **Runtime duplication**: Each plugin embeds parts of the Go runtime (~5MB baseline).
- **Size overhead**: A trivial plugin like manualtoolchange is 5.9MB (vs ~60KB as cmod).

The solution: **eliminate Go plugins entirely**. All Go packages (internal like
ads-server, generated GMI dispatch, external like galv-formula) are compiled
directly into the server binary. Adding a package = rebuild the server.

This is the same pattern used by Caddy, Traefik, and other Go-based extensible
servers. It trades dynamic loading for build-time composition — which is the
natural Go approach.

### Terminology Changes (Implemented)

| Old | New | Reason |
|-----|-----|--------|
| `linuxcnc-launcher` | `gomc-server` | Reflects role: server process, not UI launcher |
| `src/launcher/` | `src/gomc/` | Directory matches binary name |
| gomod (.so plugin) | gomod (compiled-in Go package) | Same name, but compiled in, not dynamic |
| `pkg/gomodule/` | `pkg/gomc/` | Registration interface, no plugin machinery |
| `gomodules.go` | registry lookup | No more `plugin.Open`, `loadGoPlugin` |
| `gomod/*.so` | (nothing) | No more plugin .so files |
| `gmicompile` (standalone) | `modcompile gmi` | Merged into unified tool |
| `LAUNCHER_*` vars | `GOMC_*` vars | Consistent naming in Submakefile |
| `EMC2LauncherDir` | `EMC2GomcDir` | Config field matches directory |

### What Was Removed

- `pkg/gomodule/gomodule.go` — Module interface, Factory type
- `internal/gomc/gomodules.go` — `loadGoPlugin`, `resolveGoModulePath`, etc.
- `gomod/` directory — no more plugin .so outputs
- `gomod/` top-level directory — stale build artifacts
- `share/gomodule/` — stale share directory
- `hal/proto/ads-server/` — moved to `internal/ads/`
- `EMC2_GOMOD_DIR` — no more gomod path in config
- `-buildmode=plugin` build rules in Submakefile
- `go.work` files in individual plugin dirs
- `cmd/gmicompile/` — merged into `cmd/modcompile/`

### New Architecture

#### Server Source Layout (Implemented)

The gomc module serves as both development tree and installable build directory.
For RIP, `EMC2_GOMC_DIR` points to the source tree directly. For installed
systems, the source is copied to a share directory.

Runtime files (`go.mod`, `go.sum`, `packages.conf`, `imports_generated.go`) are
gitignored and auto-generated from tracked `.in` base files on fresh checkouts.
This keeps the git tree clean after `add-gomod`/`rm-gomod` operations.

```
src/gomc/                               # = EMC2_GOMC_DIR (RIP)
├── go.mod.in                           # Tracked base (no external deps)
├── go.mod                              # Runtime (gitignored, copied from .in or managed by modcompile)
├── go.sum                              # Runtime (gitignored)
├── packages.conf.in                    # Tracked base (core gmi + internal adsmodule)
├── packages.conf                       # Runtime (gitignored, managed by modcompile)
├── cmd/
│   ├── gomc-server/
│   │   ├── main.go
│   │   └── imports_generated.go        # Generated blank imports (gitignored)
│   ├── modcompile/                     # Unified tool
│   │   └── main.go
│   ├── ads-xml-gen/
│   │   └── main.go
│   └── halcmd/
│       └── main.go
├── generated/
│   └── gmi/                            # Generated dispatch packages
│       ├── kins/                       # kins_api.h + kins_cgo.go
│       ├── tp/
│       ├── home/
│       ├── mot/
│       ├── manualtoolchange/           # REST dispatch for manualtoolchange cmod
│       └── halcmd/                     # halcmd Go REST client
├── external/                           # Installed external Go packages (gitignored)
│   └── <name>/                         # Copied source + .origin marker, go.mod stripped
├── internal/
│   ├── ads/                            # ads-server (moved in-tree, Phase 2)
│   ├── adsmodule/                      # init() registers "ads-server" with gomc
│   ├── apiserver/                      # REST server + registry
│   ├── config/                         # Compile-time paths (injected via -ldflags -X)
│   ├── gomc/                           # Server lifecycle
│   └── gmicompile/                     # .gmi codegen (parser + C/Go/Python generators)
├── pkg/
│   ├── cmodule/                        # C module headers (gomc_*.h)
│   ├── gomc/                           # Public registration: RegisterModule(), RegisterMeta()
│   ├── inifile/                        # INI file parser
│   └── hal/                            # Go HAL bindings
└── pkgreg/                             # Package registry reader/writer
    └── registry.go
```

#### Package Registry (`packages.conf`)

A simple text file that `modcompile` reads and writes. Tracks all Go packages
compiled into the server — both GMI dispatch packages and full Go modules.

Two copies exist:
- **`packages.conf.in`** — tracked in git, contains the base set (core gmi + internal modules)
- **`packages.conf`** — gitignored runtime copy, modified by `add-gomod`/`rm-gomod`

On fresh checkout, the Makefile copies `.in` → runtime if missing. `modcompile`
also calls `ensureRuntimeFiles()` before any registry operation.

```ini
# packages.conf — managed by modcompile. DO NOT EDIT MANUALLY.
#
# Format: TYPE IMPORT_PATH
#
# TYPE:
#   gmi     — generated GMI dispatch package (compiled into gomc-server)
#   gomod   — Go module compiled into gomc-server
#
# IMPORT_PATH — relative path within the gomc module for blank import

# Core GMI dispatch (generated, part of this module)
gmi generated/gmi/kins
gmi generated/gmi/tp
gmi generated/gmi/home
gmi generated/gmi/mot
gmi generated/gmi/manualtoolchange
gmi generated/gmi/halcmd

# Go modules (in-tree and installed external)
gomod internal/adsmodule
```

#### Generated Files

`modcompile` regenerates `imports_generated.go` from `packages.conf`:

**`imports_generated.go`** — blank imports that pull packages into the binary:

```go
// Code generated by modcompile. DO NOT EDIT.
package main

import (
    // GMI dispatch packages
    _ "github.com/sittner/linuxcnc/src/gomc/generated/gmi/kins"
    _ "github.com/sittner/linuxcnc/src/gomc/generated/gmi/tp"
    _ "github.com/sittner/linuxcnc/src/gomc/generated/gmi/home"
    _ "github.com/sittner/linuxcnc/src/gomc/generated/gmi/mot"
    _ "github.com/sittner/linuxcnc/src/gomc/generated/gmi/manualtoolchange"
    _ "github.com/sittner/linuxcnc/src/gomc/generated/gmi/halcmd"

    // Go modules
    _ "github.com/sittner/linuxcnc/src/gomc/internal/adsmodule"
)
```

External packages are copied into `external/<name>/` (with their `go.mod`
stripped) and referenced as relative paths in the module. Third-party
dependencies from external packages are merged into the main `go.mod` via
`go get`.

### Unified `modcompile` Tool (Implemented)

`modcompile` is the single entry point for all module operations:

```
modcompile [global-flags] <command> [command-flags] [args...]

Build environment queries:
  --cflags        Print C compiler flags for cmod builds
  --ldflags       Print linker flags for cmod builds
  --cmod-dir      Print cmod installation directory
  --include-dir   Print cmod include directory
  --go            Print Go binary path
  --print-make-inc  Print Makefile include snippet (GOMC_DIR variable)

.comp compiler:
  --parse FILE         Parse .comp and dump AST (JSON)
  --preprocess FILE    Generate C source from .comp
  --document FILE      Generate documentation from .comp
  --view-doc FILE      Generate and display documentation
  --compile FILE       Compile .comp to cmod .so
  --install FILE       Compile and install .comp to EMC2_CMOD_DIR
  --uninstall NAME     Remove an installed cmod

GMI code generator:
  gmi [flags] FILE     Generate dispatch code from .gmi file
    --server-c         Generate C header + Go cgo dispatch
    --client-c         Generate C REST client
    --client-go        Generate Go REST client
    --client-python    Generate Python REST client

Package registry:
  list                 List registered packages from packages.conf
  rebuild              Regenerate imports + rebuild gomc-server
  regenerate-imports   Regenerate imports_generated.go only (for Makefile use)
  add-gomod [-f] DIR   Copy external Go package and rebuild
  rm-gomod NAME        Remove external Go package and rebuild

Examples:
  # Compile and install a HAL component:
  modcompile --install mycomponent.comp

  # Generate GMI dispatch from IDL:
  modcompile gmi --server-c src/gmi/idl/manualtoolchange.gmi

  # Add external Go package + rebuild server:
  modcompile add-gomod ~/source/galv-mqtt-receiver/galv-formula

  # Remove a Go package:
  modcompile rm-gomod galv-formula

  # Rebuild after manual edits:
  modcompile rebuild
```

#### `modcompile gmi` Workflow

```
1. Parse the .gmi file
2. Generate C header + Go cgo dispatch to generated/gmi/<api>/
3. If @rest_export true: also generate Python client to lib/python/gmi/
4. (Submakefile handles adding gmi entry to packages.conf and regenerating imports)
```

#### `modcompile add-gomod` Workflow (Implemented)

```
1. Validate: target directory has go.mod
2. Name = directory basename; destination = external/<name>/
3. dirMirror: pure Go directory copy (replaces rsync), excludes go.mod
4. Write .origin marker file (records source path for reinstall detection)
5. mergeGoDeps: parse external go.mod via "go mod edit -json",
   filter out local replaces and self-references,
   run "go get <dep>@<version>" for remaining third-party deps
6. Add "gomod external/<name>" to packages.conf
7. Regenerate imports_generated.go
8. Rebuild gomc-server binary
```

Key implementation details:
- External packages' `go.mod` is **not** copied — the package becomes a
  sub-directory of the gomc module, not a separate module
- Third-party dependencies are merged into the main `go.mod` via `go get`
- Same-source reinstall is auto-detected via `.origin` file (no `--force` needed)
- `dirMirror()` is a pure Go replacement for `rsync --delete`, avoiding
  the external tool dependency

#### `modcompile rm-gomod` Workflow (Implemented)

```
1. Delete external/<name>/ directory
2. Remove "gomod external/<name>" from packages.conf
3. Regenerate imports_generated.go
4. Run "go mod tidy" to clean up orphaned dependencies
5. Rebuild gomc-server binary
```

#### `modcompile rebuild` Workflow

```
1. Read packages.conf
2. Regenerate imports_generated.go
3. cd EMC2_GOMC_DIR && go build -ldflags "..." -o $BIN/gomc-server ./cmd/gomc-server/
```

#### `modcompile regenerate-imports` Workflow

```
1. Read packages.conf
2. Regenerate imports_generated.go only (no build)
```

This subcommand exists specifically for Makefile use to avoid a race condition:
with parallel builds (`make -j8`), the `imports_generated.go` rule must not
trigger a `go build` because GMI codegen targets may not have finished yet.
The actual build is handled by the `../bin/gomc-server` Makefile target which
has proper GMI dependencies.

### Build System Integration (Implemented)

#### Makefile (`gomc/Submakefile`)

```makefile
# Runtime files: .in → working copy (on fresh checkout)
gomc/packages.conf: gomc/packages.conf.in
    $(Q)test -f $@ || cp $< $@

gomc/go.mod: gomc/go.mod.in
    $(Q)test -f $@ || cp $< $@

# imports_generated.go uses regenerate-imports (NOT rebuild) to avoid
# race conditions with parallel builds — GMI codegen may still be running.
$(IMPORTS_GENERATED): gomc/packages.conf ../bin/modcompile
    $(Q)test -f $@ || cd gomc && $(TOP)/bin/modcompile regenerate-imports

# GOMC_SRC_BASE includes gomc/go.mod (generated) — forces copy from .in.
# filter-out excludes imports_generated.go to break circular dependency
# (modcompile depends on GOMC_SRC_BASE, imports_generated depends on modcompile).
GOMC_SRC_BASE := $(filter-out $(IMPORTS_GENERATED),...) gomc/go.mod.in gomc/go.mod

# gomc-server: depends on generated GMI files + imports + source
../bin/gomc-server: $(GOMC_SRC) $(GMI_KINS_GEN_GO) $(IMPORTS_GENERATED) \
                    gomc/packages.conf ../lib/liblinuxcnchal.so
    cd gomc && CGO_LDFLAGS="..." $(GO) build -ldflags "$(GOMC_LDFLAGS)" \
        -o $(TOP)/bin/gomc-server ./cmd/gomc-server
```

Key Makefile design decisions:
- **`gomc/go.mod` in `GOMC_SRC_BASE`**: All Go targets automatically depend on
  it, triggering the copy-from-`.in` rule on fresh checkouts
- **`filter-out` for `IMPORTS_GENERATED`**: Breaks circular dependency where
  modcompile → GOMC_SRC_BASE → imports_generated → modcompile
- **`regenerate-imports` not `rebuild`**: The Makefile rule only generates the
  imports file, not the binary. The `../bin/gomc-server` target handles the
  actual build with proper prerequisite ordering
- **`test -f` guards**: Idempotent — don't overwrite existing files

#### Environment Variables

Set by `scripts/rip-environment` (RIP) or read from installed paths:

| Variable | RIP Value | Installed Value |
|----------|-----------|-----------------|
| `EMC2_GOMC_DIR` | `$EMC2_HOME/src/gomc` | `$prefix/share/linuxcnc/gomc` |
| `EMC2_CMOD_DIR` | `$EMC2_HOME/cmod` | `$prefix/lib/linuxcnc/cmod` |

#### Submakefile Variables (GOMC_* namespace)

| Variable | Purpose |
|----------|---------|
| `GOMC_LDFLAGS_PKG` | Go package path for `-ldflags -X` injection |
| `EMC2_GOMC_DIR` | Install destination for gomc source tree |
| `GOMC_LDFLAGS` | All `-X` flags for compile-time config |
| `GOMC_SRC_BASE` | Hand-written Go sources + go.mod (no generated files) |
| `GOMC_SRC` | Full source list including generated files |
| `GOMC_PKG_FILES` | Public package files copied to `share/` for RIP |

Set by `scripts/rip-environment` (RIP) or read from installed paths:

| Variable | RIP Value | Installed Value |
|----------|-----------|-----------------|
| `EMC2_GOMC_DIR` | `$EMC2_HOME/src/gomc` | `$prefix/share/linuxcnc/gomc` |
| `EMC2_CMOD_DIR` | `$EMC2_HOME/cmod` | `$prefix/lib/linuxcnc/cmod` |

### Migration Phases (All Complete)

#### Phase 1: Rename + Remove Plugin Infrastructure ✅

- Renamed `linuxcnc-launcher` binary to `gomc-server` (scripts, Submakefile, docs)
- Renamed `src/launcher/` directory to `src/gomc/`
- Renamed all `LAUNCHER_*` Submakefile variables to `GOMC_*`
- Renamed `EMC2LauncherDir` config field to `EMC2GomcDir`
- Created `pkg/gomc/gomc.go` — registration interface (`RegisterModule`, `GetFactory`, `HasModule`)
- Replaced `plugin.Open` with `pkg/gomc` registry lookup in `gomodules.go`
- Removed `-buildmode=plugin` build rules
- Removed `EMC2_GOMOD_DIR` from config

#### Phase 2: Move ads-server In-Tree ✅

- Moved `hal/proto/ads-server/` → `internal/ads/`, `internal/adsbridge/`, `internal/adsconfig/`
- Created `internal/adsmodule/module.go` with `init()` that registers "ads-server" factory
- Removed ads-server's separate `go.mod` and `go.work`
- Blank import in `cmd/gomc-server/main.go` triggers registration

#### Phase 3: Package Registry ✅

- Created `packages.conf` format (`TYPE IMPORT_PATH`)
- Created `pkgreg/registry.go` — registry reader/writer with `GenerateImports()`
- Created `imports_generated.go` generator
- Implemented `modcompile list`, `add-gomod`, `rm-gomod`, `rebuild`, `regenerate-imports`
- Implemented `dirMirror()` — pure Go directory copy replacing rsync
- Implemented `mergeGoDeps()` — parses external go.mod, runs `go get` for third-party deps
- Implemented `goModTidy()` — cleanup helper used by both add and rm paths
- Implemented `ensureRuntimeFiles()` — copies `.in` → runtime if missing

#### Phase 4: Merge gmicompile into modcompile ✅

- Moved gmicompile's CLI logic into `modcompile gmi` subcommand
- Updated `gmi/codegen/Submakefile` to use `modcompile gmi` instead of standalone `gmicompile`
- Removed standalone `cmd/gmicompile/` directory

#### Phase 5: Installed Build Support ✅

- `modcompile` uses `EMC2GomcDir` (injected via `-ldflags`) for all paths
- `--gomc-dir` CLI flag with `--launcher-dir` backward compat alias
- `--print-make-inc` emits `GOMC_DIR` variable for external Makefiles
- Compile-time config propagated to rebuilt binaries via `-ldflags -X`

### Implementation Findings

These are lessons learned during implementation that weren't anticipated in the
original design.

#### go.work Was Not Needed

The original design called for a `go.work` file to give external packages access
to the gomc module. In practice, `go.work` was unnecessary because:
- External packages are copied into `external/<name>/` inside the module tree
- Their `go.mod` is stripped — they become regular sub-packages of the module
- Third-party dependencies are merged into the main `go.mod` via `go get`

This is simpler and avoids `go.work` complexities (e.g., it prevents `go mod tidy`
from working normally).

#### Parallel Build Race Condition

With `make -j8`, the `imports_generated.go` rule originally called
`modcompile rebuild` which triggered `go build ./cmd/gomc-server`. This raced
with GMI codegen targets — `go build` would fail because generated packages
(like `generated/gmi/tp`) didn't exist yet.

**Fix**: Split into `regenerate-imports` (file generation only, used by Makefile)
and `rebuild` (generation + build, used interactively). The Makefile's
`../bin/gomc-server` target has explicit GMI prerequisites and handles the build.

#### Circular Makefile Dependencies

`GOMC_SRC_BASE` uses `$(wildcard gomc/cmd/*/*.go)` which, after the first build,
picks up `imports_generated.go`. This creates a cycle:
`modcompile` → `GOMC_SRC_BASE` → `imports_generated.go` → `modcompile`.

**Fix**: Define `IMPORTS_GENERATED` before `GOMC_SRC_BASE` and filter it out:
```makefile
GOMC_SRC_BASE := $(filter-out $(IMPORTS_GENERATED),$(wildcard gomc/cmd/*/*.go) ...) ...
```

#### Git Dirty State from Runtime Files

`packages.conf`, `go.mod`, `go.sum`, and `imports_generated.go` are modified by
`add-gomod`/`rm-gomod` and by `go build` (`go.sum` updates). Having these tracked
meant the git tree was always dirty after external module operations.

**Fix**: `.in` base file pattern:
- `packages.conf.in` and `go.mod.in` are tracked (minimal base state)
- Runtime copies are gitignored
- Makefile rules copy `.in` → runtime if missing (`test -f $@ || cp $< $@`)
- `modcompile ensureRuntimeFiles()` does the same before registry operations
- `git rm --cached` was needed to un-track previously committed runtime files

#### External Package go.mod Stripping

External packages can't keep their own `go.mod` inside the gomc module tree —
Go would treat them as separate modules. Instead:
- `dirMirror()` copies everything except `go.mod` and `go.sum`
- Third-party dependencies are extracted from the external `go.mod` via
  `go mod edit -json` and merged with `go get <dep>@<version>`
- Local `replace` directives and self-references are filtered out

#### `test -f` Guards for Idempotency

The `.in` → runtime copy rules use `test -f $@ || cp $< $@` rather than plain
`cp`. This ensures that `packages.conf` modified by `add-gomod` is not
overwritten on subsequent `make` invocations — the copy only happens when the
file doesn't exist at all.

#### modcompile Path After `cd gomc`

Submakefile rules that `cd gomc` before running commands need `$(TOP)/bin/modcompile`
(absolute path), not `../bin/modcompile` (which resolves relative to the new CWD,
giving `src/bin/modcompile` which doesn't exist).

### Go Package Requirements for `add-gomod` (Implemented)

External Go packages must follow these conventions to be compiled into
gomc-server:

1. **`go.mod`** at package root with proper module path
2. **`init()` function** that registers the package with the server via
   `gomc.RegisterModule(name, factory)` — the factory returns a `gomc.Module`
   with Start/Stop/Cleanup lifecycle hooks
3. **No `main` package** — the package is imported, not executed
4. **Compatible dependencies** — must build with the gomc module's Go version

Example minimal gomod:

```go
package mymodule

import "github.com/sittner/linuxcnc/src/gomc/pkg/gomc"

func init() {
    gomc.RegisterModule("mymodule", func(cfg gomc.ModuleConfig) (gomc.Module, error) {
        return &myModule{cfg: cfg}, nil
    })
}

type myModule struct {
    cfg gomc.ModuleConfig
}

func (m *myModule) Start() error { /* ... */ return nil }
func (m *myModule) Stop()        { /* ... */ }
func (m *myModule) Cleanup()     { /* ... */ }
```

The `gomc-stub/` directory pattern (from `go-comp-template`) provides local
development with a stub `pkg/gomc` so the package can be developed independently
and only needs the real gomc module when compiled into gomc-server.

#### External Makefile Integration

External packages use `modcompile --print-make-inc` to get the `GOMC_DIR` variable:

```makefile
$(eval $(shell modcompile --print-make-inc))
# Now GOMC_DIR is set to the gomc source directory

install:
    cd $(GOMC_DIR) && $(GOMC_DIR)/../bin/modcompile add-gomod $(CURDIR)

uninstall:
    cd $(GOMC_DIR) && $(GOMC_DIR)/../bin/modcompile rm-gomod $(notdir $(CURDIR))
```

### Configure Support for In-Tree Gomods

In-tree Go modules (like ads-server) should be selectable at configure time:

```
./configure --enable-ads-server    # default: enabled
./configure --disable-ads-server   # exclude from build
```

Configure writes the selection to a config file. The build system reads it to
determine which in-tree gomods are added to `packages.conf` and compiled into
gomc-server. This mirrors how optional C components (e.g., `--enable-pncconf`)
work today.

### Dependency Conflicts Between External Go Packages

All compiled-in packages share one dependency tree. When two external packages
require different versions of the same dependency, Go's MVS (Minimum Version
Selection) picks the highest version. This usually works, but can break if the
higher version has breaking API changes without a module path bump.

Mitigation (implemented):
- `modcompile add-gomod` merges dependencies via `go get` and then builds.
  If the combined build fails, the error is reported and the package is still
  added (the user can fix dependencies and run `modcompile rebuild`).
- `modcompile rm-gomod` runs `go mod tidy` to clean up orphaned dependencies.

Known constraint: *"All compiled-in packages share one dependency tree.
Adding a package that requires an incompatible version of a shared
dependency will fail at build time."*

### Impact on Existing Components

| Component | Change |
|-----------|--------|
| manualtoolchange (.comp with gmi_provide) | No change — cmod still loaded via dlopen, GMI dispatch compiled into server via generated package |
| ads-server | Moved from external plugin to `internal/ads/`, compiled in |
| kins/tp/home/mot | No change — already compiled into server via generated cgo packages |
| halcmd | No change — already compiled into server |
| External user modules (.comp) | No change — still compiled to cmod .so via `modcompile install` |
| External Go packages | `modcompile add-gomod` instead of building separate .so |
| Python UIs | No change — still REST clients |

## Open Questions

1. ~~**Versioning strategy**: How to handle API version mismatches?~~ **Resolved**: Exact match required, fail at lookup
2. ~~**Hot reload**: Can APIs be re-registered while running?~~ **Resolved**: No, lookup at startup only
3. ~~**Timeout handling**: Per-call timeouts? Global?~~ **Resolved**: No function timeouts, only HTTP transport
4. ~~**Error codes**: Standardize across Go/C boundary?~~ **Resolved**: errno for inter-module callbacks, GMI_ERR_* for client library
5. ~~**apiserver visibility**: Should `apiserver` move to `pkg/` for external Go packages, or use a thin `pkg/` registration interface?~~ **Resolved**: Thin `pkg/gomc` registration interface. External packages import `pkg/gomc` for types (`APIMeta`, `FuncMeta`, `RegisterMeta()`) + lifecycle hooks (`RegisterModule()`). `internal/apiserver` stays internal.
6. ~~**Module lifecycle for gomods**: External Go packages may need Start/Stop lifecycle (like ads-server). Define a registration mechanism in `pkg/` similar to the old `gomodule.Module` but without the plugin baggage?~~ **Resolved**: Mirror the cmod lifecycle. `pkg/gomc.RegisterModule()` registers Start/Stop/Cleanup hooks. The gomc-server calls these at the same lifecycle points it calls cmod equivalents. No plugin.Open — just init()-time registry lookup.

## Design Decisions

### API Lookup at Startup (Not Runtime)

API lookup happens during module initialization, not at function call time:

```c
// In cmod init function (receives gomc_api_t* from launcher):
int my_module_init(const gomc_api_t *api) {
    // Lookup happens here - fails fast if API unavailable or version mismatch
    // Returns opaque pointer, cast to typed callbacks struct
    kins_api = kins_api_get(api, "default");  // wrapper for api->get_api()
    if (!kins_api) {
        return -ENOENT;  // Fail module load
    }
    
    // All callback pointers now resolved
    // Runtime calls are direct pointer calls - RT safe
    return 0;
}

// At runtime - direct call, no lookup, no dispatch table:
kins_pose_t pose;
int rc = kins_api->forward(joints, &pose, fflags, &iflags);
```

**Benefits:**
- Fail-fast: Version/availability issues caught at startup
- RT-safe: No allocation or lookup in call path
- Predictable: All dependencies resolved before operation

### Version Matching

Exact version match required at lookup time:

```c
// Generated wrapper in <api>_api.h calls through gomc_api_t:
static inline const kins_callbacks_t *kins_api_get(
    const gomc_api_t *api,
    const char *instance_name)
{
    return (const kins_callbacks_t *)api->get_api(
        api->ctx, "kins", 1, instance_name);
}

// Returns NULL if:
// - Instance not found
// - Version mismatch (registered != required)
```

No backward/forward compatibility - keeps things simple and safe.

### No Function Timeouts

API function calls behave like normal C/Go function calls:
- No internal timeout handling
- Caller is responsible for not blocking inappropriately
- HTTP transport layer has its own timeouts (for external REST only)

**Rationale:** 
- RT callbacks must be deterministic - no timeout machinery
- Simplifies implementation
- Matches normal function call semantics

### Error Handling: Two Domains

There are two separate error code domains:

**Inter-module callback API** (cmod↔cmod, cmod↔gomod): Use standard Linux errno
codes for consistency with C ecosystem:

```c
// Callback return values:
//   0         = success
//   -EINVAL   = invalid argument
//   -ENOENT   = not found (pin, signal, etc.)
//   -ENOMEM   = allocation failed
//   -EBUSY    = resource busy
//   -EPERM    = permission denied (future auth)
//   -ENOSYS   = function not implemented
//   -EEXIST   = already exists
//   -ERANGE   = value out of range

// Example callback signature:
typedef int (*hal_pin_read_fn)(const char *name, hal_pin_info_t *out);
// Returns 0 on success, -errno on failure
```

**In Go:**
```go
import "syscall"

func (api *HalAPI) PinRead(name string) (*PinInfo, error) {
    // ...
    if notFound {
        return nil, syscall.ENOENT
    }
    return &info, nil
}
```

**Client library (libgmi)**: Uses custom `GMI_ERR_*` codes for HTTP/JSON/curl
domain-specific errors. These do not overlap with errno:

```c
// Client library error codes (negative, libgmi-specific):
//   GMI_OK            =  0   // Success
//   GMI_ERR_ALLOC     = -1   // Memory allocation failed
//   GMI_ERR_CURL      = -2   // CURL operation failed
//   GMI_ERR_JSON      = -3   // JSON parse/encode error
//   GMI_ERR_OVERFLOW  = -4   // Buffer overflow
//   GMI_ERR_INVALID   = -5   // Invalid argument
//   GMI_ERR_NOT_FOUND = -6   // Resource not found
//   GMI_ERR_TIMEOUT   = -7   // Operation timed out
//   GMI_ERR_IO        = -8   // I/O error
//   >= 100                   // HTTP status code (returned as-is)
```

**For detailed errors** (when simple errno insufficient):
```c
typedef struct {
    const char *message;  // Human-readable error detail (optional, can be NULL)
    // ... response fields
} hal_response_t;

// Caller checks return code first, then response.message if needed
```
