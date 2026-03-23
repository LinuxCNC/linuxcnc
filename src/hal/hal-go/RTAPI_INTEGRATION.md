# Phase 4: rtapi_app Integration — Implementation Guide

## 1. Overview

### 1.1 Goal

Replace the standalone `rtapi_app` binary and Unix socket IPC with direct in-process
function calls. After this change:

- The Go launcher **is** the RT process — RT hardening, module loading, and thread
  execution all happen in the same process space.
- `loadrt` commands call C functions directly instead of spawning `rtapi_app load`.
- The Unix socket server/client code is deleted.
- `halcmd` (standalone binary) continues to work via the same direct calls.

### 1.2 Non-Goals

- No changes to the kernel/RTAI path (this is USPACE-only).
- No changes to `loadusr` — userspace components still run as separate processes.
- No changes to HAL file syntax or halcmd command semantics.

### 1.3 Architecture Before

```
┌─────────────────┐     socket      ┌─────────────────────┐
│  halcmd/halrun  │ ──────────────► │     rtapi_app       │
│  (hal client)   │                 │  (RT process)       │
└─────────────────┘                 │  - loads modules    │
                                    │  - runs threads     │
┌─────────────────┐     socket      │  - dlopen/dlclose   │
│   Go launcher   │ ──────────────► │                     │
│                 │                 └─────────────────────┘
└─────────────────┘
```

### 1.4 Architecture After

```
┌───────────────────────────────────────────────────────┐
│                    Go Launcher                         │
│  ┌─────────────────────────────────────────────────┐  │
│  │            RT Environment (in-process)           │  │
│  │  - RT hardening at startup                       │  │
│  │  - module loading via dlopen()                   │  │
│  │  - thread execution via pthreads                 │  │
│  │  - direct API calls (no socket)                  │  │
│  └─────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────┘
```

---

## 2. File Changes Summary

| File | Action | Description |
|------|--------|-------------|
| `src/rtapi/uspace_rtapi_app.c` | **REFACTOR** | Extract library functions; delete socket code |
| `src/rtapi/rtapi_uspace.h` | **NEW** | Public C API header for RT environment |
| `src/rtapi/Submakefile` | **MODIFY** | Build `librtapi_uspace.so` instead of `rtapi_app` binary |
| `src/hal/utils/halcmd_commands.cc` | **MODIFY** | Replace `rtapi_app` subprocess with direct calls |
| `src/hal/hal-go/cgo.go` | **MODIFY** | Add shims for `Init()`, `CreateThread()` |
| `src/hal/hal-go/command.go` | **MODIFY** | Add `Init()`, `CreateThread()` functions |
| `src/launcher/launcher/launcher.go` | **MODIFY** | Call `hal.Init()`, use `hal.CreateThread()` |
| `src/launcher/launcher/cleanup.go` | **MODIFY** | Remove rtapi_app stop logic |
| `src/launcher/realtime/` | **DELETE** | No longer needed (rtMgr manages external process) |
| `scripts/realtime.in` | **MODIFY** | Remove rtapi_app references |

---

## 3. API Design: `rtapi_uspace.h`

```c
// rtapi_uspace.h - Userspace RTAPI library interface
//
// Replaces the standalone rtapi_app binary. The RT environment
// runs in-process with the caller (Go launcher or halcmd).

#ifndef RTAPI_USPACE_H
#define RTAPI_USPACE_H

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// RT Environment Lifecycle
// ============================================================

// Initialize the RT environment (memory locking, RT hardening).
// Must be called once before any other rtapi/HAL functions.
// Automatically loads hal_lib.so.
// Returns 0 on success, negative errno on failure.
int rtapi_uspace_init(void);

// Check if RT environment is initialized.
int rtapi_uspace_is_initialized(void);

// Cleanup RT environment. Called at shutdown.
// Unloads all modules, stops threads, releases resources.
void rtapi_uspace_cleanup(void);

// ============================================================
// Module Management
// ============================================================

// Load a realtime component module (.so file).
// module_name: component name (e.g., "trivkins", "motmod")
// argc/argv: module parameters in "key=value" format
// Returns 0 on success, negative errno on failure.
int rtapi_uspace_module_load(const char *module_name, int argc, const char **argv);

// Unload a realtime component module.
// Returns 0 on success, negative errno on failure.
int rtapi_uspace_module_unload(const char *module_name);

// Create new instance of an instantiable component.
// comp_type: component type name
// inst_name: instance name to create
// arg: optional argument string (may be NULL)
// Returns 0 on success, negative errno on failure.
int rtapi_uspace_newinst(const char *comp_type, const char *inst_name, const char *arg);

// ============================================================
// Thread Management
// ============================================================

// Start the RT thread execution loop.
// This function blocks until rtapi_uspace_threads_stop() is called
// or all modules are unloaded.
// Should be called from a dedicated thread.
// Returns 0 on normal exit, negative errno on error.
int rtapi_uspace_threads_run(void);

// Signal all RT threads to stop.
// Safe to call from any thread (e.g., signal handler).
void rtapi_uspace_threads_stop(void);

// Check if threads are currently running.
int rtapi_uspace_threads_running(void);

// ============================================================
// Information
// ============================================================

// Get the flavor name (e.g., "posix", "rt-preempt").
const char *rtapi_uspace_flavor_name(void);

// Check if running with realtime capabilities.
int rtapi_uspace_is_realtime(void);

// Get loaded module count (excluding hal_lib).
int rtapi_uspace_module_count(void);

#ifdef __cplusplus
}
#endif

#endif // RTAPI_USPACE_H
```

---

## 4. Code Migration from `uspace_rtapi_app.c`

### 4.1 Functions to Keep (refactor into library)

| Function | New Name | Changes |
|----------|----------|---------|
| `harden_rt()` | `rtapi_uspace_init()` (internal) | Remove `with_root_*` wrappers (caller handles privileges) |
| `configure_memory()` | `configure_memory()` (static) | No changes |
| `initialize_app()` | `rtapi_uspace_init()` (part of) | Merge with harden_rt |
| `do_load_cmd()` | `rtapi_uspace_module_load()` | Remove socket response code |
| `do_unload_cmd()` | `rtapi_uspace_module_unload()` | Remove socket response code |
| `do_newinst_cmd()` | `rtapi_uspace_newinst()` | Remove socket response code |
| `do_comp_args()` | `do_comp_args()` (static) | No changes |
| `do_one_item()` | `do_one_item()` (static) | No changes |
| `find_module()` | `find_module()` (static) | No changes |
| `add_module()` | `add_module()` (static) | No changes |
| `remove_module()` | `remove_module()` (static) | No changes |
| `sim_rtapi_run_threads()` / `run_threads()` | `rtapi_uspace_threads_run()` | Remove socket fd parameter |
| `task_new()` | `task_new()` (static) | No changes |
| `task_start()` | `task_start()` (static) | No changes |
| `task_wrapper()` | `task_wrapper()` (static) | No changes |
| `signal_handler()` | `signal_handler()` (static) | Minor changes for library use |
| `queue_function()` | `queue_function()` (static) | No changes |
| `msg_queue_push()` | `msg_queue_push()` (static) | No changes |
| `msg_queue_consume_all()` | `msg_queue_consume_all()` (static) | No changes |
| `rtapi_*` exported functions | Keep all | These are the RTAPI implementation |

### 4.2 Functions to DELETE (socket-related)

| Function | Reason |
|----------|--------|
| `main()` | Replaced by library init |
| `rtapi_become_master()` | Socket master/slave logic |
| `master()` | Socket server loop |
| `slave()` | Socket client |
| `callback()` | Socket command handler |
| `handle_command()` | Socket command dispatcher |
| `read_number()` | Socket protocol |
| `read_string()` | Socket protocol |
| `read_strings()` | Socket protocol |
| `write_number()` | Socket protocol |
| `write_string()` | Socket protocol |
| `write_strings()` | Socket protocol |
| `get_fifo_path()` | Socket path management |
| `get_fifo_path_internal()` | Socket path management |
| `get_fifo_path_buf()` | Socket path management |

### 4.3 Global Variables

| Variable | Action | Notes |
|----------|--------|-------|
| `modules[]` | Keep | Protected by `modules_lock` mutex |
| `modules_lock` | Keep | pthread mutex |
| `instance_count` | Keep | Track loaded modules |
| `force_exit` | Keep | Shutdown signal |
| `msg_queue[]` | Keep | Message queue for RT threads |
| `msg_head`, `msg_tail` | Keep | Atomic queue pointers |
| `queue_thread` | Keep | Message consumer thread |
| `main_thread` | Keep → rename `init_thread` | Thread that called init |
| `app_policy` | Keep | SCHED_FIFO or SCHED_OTHER |
| `app_period` | Keep | Base period |
| `do_thread_lock` | Keep | Non-RT fallback flag |
| `task_key` | Keep | pthread TLS key |
| `thread_lock` | Keep | Non-RT mutex |
| `task_array[]` | Keep | Task tracking |
| `euid`, `ruid` | Keep | UID management |
| `with_root_level` | Keep | Privilege escalation counter |

### 4.4 Initialization Sequence

**Current (`rtapi_app` binary):**
```
main()
  → setreuid() / setresuid()
  → rtapi_become_master()
      → bind socket
      → master()
          → pthread_create(queue_function)
          → do_load_cmd("hal_lib")
          → sim_rtapi_run_threads(socket_fd, callback)
```

**New (library):**
```
rtapi_uspace_init()
  → check if already initialized
  → setreuid() / setresuid() if running as root
  → harden_rt() / configure_memory()
  → pthread_create(queue_function)
  → rtapi_uspace_module_load("hal_lib", 0, NULL)
  → return (caller decides when to run threads)
```

---

## 5. Changes to `halcmd_commands.cc`

### 5.1 `do_loadrt_cmd()` (lines 1099-1206)

**Before:**
```cpp
#if defined(RTAPI_USPACE)
    argv[m++] = "-Wn";
    argv[m++] = mod_name;
    argv[m++] = EMC2_BIN_DIR "/rtapi_app";
    argv[m++] = "load";
    argv[m++] = mod_name;
    // ... args ...
    retval = do_loadusr_cmd(argv);  // spawns rtapi_app
#endif
```

**After:**
```cpp
#if defined(RTAPI_USPACE)
    #include "rtapi_uspace.h"
    
    // Ensure RT environment is initialized
    if (!rtapi_uspace_is_initialized()) {
        retval = rtapi_uspace_init();
        if (retval < 0) {
            halcmd_error("rtapi init failed: %d\n", retval);
            return -1;
        }
    }
    
    // Build argv for module parameters
    const char *mod_argv[MAX_TOK];
    int mod_argc = 0;
    while (args[n] && args[n][0] != '\0') {
        mod_argv[mod_argc++] = args[n++];
    }
    
    // Direct call - no socket, no subprocess
    retval = rtapi_uspace_module_load(mod_name, mod_argc, mod_argv);
#endif
```

### 5.2 `unloadrt_comp()` (lines 1366-1392)

**Before:**
```cpp
#if defined(RTAPI_USPACE)
    argv[0] = EMC2_BIN_DIR "/rtapi_app";
    argv[1] = "unload";
    argv[2] = mod_name;
    argv[3] = NULL;
    retval = hal_systemv(argv);  // spawns rtapi_app
#endif
```

**After:**
```cpp
#if defined(RTAPI_USPACE)
    retval = rtapi_uspace_module_unload(mod_name);
#endif
```

### 5.3 `do_newinst_cmd()` (currently commented out, lines 519-624)

If re-enabled:
```cpp
#if defined(RTAPI_USPACE)
    retval = rtapi_uspace_newinst(comp_name, inst_name, NULL);
#endif
```

---

## 6. Changes to hal-go

### 6.1 New C Shims in `cgo.go`

```c
// In the cgo preamble of cgo.go:

#include "rtapi_uspace.h"

// hal_shim_init initializes the RT environment.
// Returns 0 on success, negative errno on failure.
static int hal_shim_rt_init(void) {
    return rtapi_uspace_init();
}

// hal_shim_rt_is_initialized checks if RT environment is ready.
static int hal_shim_rt_is_initialized(void) {
    return rtapi_uspace_is_initialized();
}

// hal_shim_rt_cleanup shuts down the RT environment.
static void hal_shim_rt_cleanup(void) {
    rtapi_uspace_cleanup();
}

// hal_shim_create_thread creates a HAL thread.
// This wraps hal_create_thread from hal.h.
static int hal_shim_create_thread(const char *name, long period_nsec, int uses_fp) {
    return hal_create_thread(name, period_nsec, uses_fp);
}

// hal_shim_threads_run starts the RT thread loop (blocks until stop).
static int hal_shim_threads_run(void) {
    return rtapi_uspace_threads_run();
}

// hal_shim_threads_stop signals threads to stop.
static void hal_shim_threads_stop(void) {
    rtapi_uspace_threads_stop();
}

// hal_shim_threads_running checks if thread loop is active.
static int hal_shim_threads_running(void) {
    return rtapi_uspace_threads_running();
}
```

### 6.2 New Go Functions in `command.go`

```go
// Init initializes the RT environment.
// Must be called once before loading any RT modules.
// This sets up memory locking, RT scheduling, and loads hal_lib.
func Init() error {
    ret := C.hal_shim_rt_init()
    if ret < 0 {
        return &Error{Code: int(ret), Op: "rtapi_init"}
    }
    return nil
}

// IsInitialized returns true if the RT environment is ready.
func IsInitialized() bool {
    return C.hal_shim_rt_is_initialized() != 0
}

// Cleanup shuts down the RT environment.
// Unloads all modules and releases resources.
func Cleanup() {
    C.hal_shim_rt_cleanup()
}

// CreateThread creates a new HAL thread.
// name: thread name (e.g., "servo-thread")
// periodNs: thread period in nanoseconds
// usesFP: true if thread functions use floating point
func CreateThread(name string, periodNs int64, usesFP bool) error {
    cName := C.CString(name)
    defer C.free(unsafe.Pointer(cName))
    
    fp := C.int(0)
    if usesFP {
        fp = 1
    }
    
    ret := C.hal_shim_create_thread(cName, C.long(periodNs), fp)
    if ret < 0 {
        return &Error{Code: int(ret), Op: "hal_create_thread", Name: name}
    }
    return nil
}

// RunThreads starts the RT thread execution loop.
// This blocks until StopThreads() is called.
// Should be called from a dedicated goroutine.
func RunThreads() error {
    // Lock this goroutine to its OS thread for RT
    runtime.LockOSThread()
    defer runtime.UnlockOSThread()
    
    ret := C.hal_shim_threads_run()
    if ret < 0 {
        return &Error{Code: int(ret), Op: "rtapi_threads_run"}
    }
    return nil
}

// StopThreads signals all RT threads to stop.
// Safe to call from any goroutine.
func StopThreads() {
    C.hal_shim_threads_stop()
}

// ThreadsRunning returns true if the RT thread loop is active.
func ThreadsRunning() bool {
    return C.hal_shim_threads_running() != 0
}
```

---

## 7. Changes to Launcher

### 7.1 `launcher.go` — Startup

**Current approach:**
```go
// In startRealtimeEnvironment():
l.rtMgr = realtime.New(l.logger)
if err := l.rtMgr.Start(); err != nil {
    return err
}
```

**New approach:**
```go
func (l *Launcher) startRealtimeEnvironment() error {
    l.logger.Info("initializing RT environment")
    
    // Initialize RT (hardening, memory lock, load hal_lib)
    if err := hal.Init(); err != nil {
        return fmt.Errorf("RT init: %w", err)
    }
    
    l.logger.Info("RT environment ready", 
        "flavor", hal.FlavorName(),
        "realtime", hal.IsRealtime())
    
    return nil
}
```

### 7.2 `launcher.go` — Thread Creation

**Current approach (via halfile):**
```go
// HAL file contains: loadrt threads name1=servo-thread period1=1000000
halExec.Execute(halfile)
```

**New approach:**
```go
func (l *Launcher) loadThreads() error {
    period := l.ini.GetInt64("EMCMOT", "SERVO_PERIOD", 1000000)
    
    // Direct thread creation - no threads.so needed
    l.logger.Info("creating servo thread", "period_ns", period)
    if err := hal.CreateThread("servo-thread", period, true); err != nil {
        return fmt.Errorf("create servo-thread: %w", err)
    }
    
    return nil
}
```

**Note:** The `loadrt threads` command in HAL files should still work via
`rtapi_uspace_module_load("threads", ...)`. The direct `hal.CreateThread()` is
an optimization for the launcher — it avoids parsing the `threads` module's
parameter syntax.

### 7.3 `launcher.go` — Thread Loop Start

After all modules are loaded and `addf` commands have been executed:

```go
func (l *Launcher) startRTThreadLoop() {
    // Start thread loop in background goroutine
    l.rtThreadsDone = make(chan struct{})
    go func() {
        defer close(l.rtThreadsDone)
        if err := hal.RunThreads(); err != nil {
            l.logger.Error("RT thread loop error", "error", err)
        }
    }()
}
```

### 7.4 `cleanup.go` — Shutdown

**Current approach:**
```go
// Step 7 — Stop realtime environment.
if l.rtMgr != nil {
    if err := l.rtMgr.Stop(); err != nil {
        l.logger.Error("realtime stop failed", "error", err)
    }
}
```

**New approach:**
```go
// Step 7 — Stop RT thread loop.
l.logger.Debug("stopping RT thread loop")
hal.StopThreads()

// Wait for thread loop goroutine to exit
if l.rtThreadsDone != nil {
    select {
    case <-l.rtThreadsDone:
    case <-time.After(5 * time.Second):
        l.logger.Warn("timeout waiting for RT threads to stop")
    }
}

// Step 7b — Cleanup RT environment (after all HAL operations)
// This is called later, after hal_exit()
```

### 7.5 Delete `src/launcher/realtime/`

The entire `realtime/` package can be deleted:
- `realtime.go` — managed external rtapi_app process
- `realtime_test.go` — tests for the above
- `ipc_cleanup.go` — IPC cleanup utilities

These are no longer needed because there is no external process to manage.

---

## 8. Build System Changes

### 8.1 `src/rtapi/Submakefile`

**Current:**
```makefile
$(OBJDIR)/bin/rtapi_app: $(RTAPI_APP_SRCS)
    $(CXX) ... -o $@ $^
```

**New:**
```makefile
# Build shared library instead of binary
$(OBJDIR)/lib/librtapi_uspace.so: $(RTAPI_USPACE_SRCS)
    $(CC) -shared -fPIC ... -o $@ $^

# Install header
install: $(DESTDIR)$(includedir)/rtapi_uspace.h

# No longer build rtapi_app binary
# RTAPI_APP_SRCS is removed
```

### 8.2 `src/hal/Submakefile`

Add link dependency:
```makefile
HALCMD_LDFLAGS += -lrtapi_uspace
```

### 8.3 `src/hal/hal-go/`

Update cgo directives:
```go
// #cgo LDFLAGS: -L${SRCDIR}/../../lib -llinuxcnchal -lrtapi_uspace -ldl -lpthread -lrt
```

---

## 9. Testing Strategy

### 9.1 Unit Tests

1. **`rtapi_uspace_init()` / `rtapi_uspace_cleanup()`**
   - Call init twice (second should be no-op or return already-initialized)
   - Call cleanup without init (should be safe)
   - Verify hal_lib is loaded after init

2. **`rtapi_uspace_module_load()` / `rtapi_uspace_module_unload()`**
   - Load a simple module (e.g., `siggen`)
   - Verify component appears in HAL
   - Unload and verify component disappears
   - Load same module twice (should fail)
   - Unload non-existent module (should fail gracefully)

3. **`rtapi_uspace_threads_run()` / `rtapi_uspace_threads_stop()`**
   - Start threads with no threads created (should handle gracefully)
   - Start/stop cycle
   - Call stop from different thread

### 9.2 Integration Tests

1. **Basic startup sequence:**
   ```bash
   linuxcnc-launcher configs/sim/axis.ini
   # Should start without errors
   # halcmd show comp should list components
   # halcmd show thread should list servo-thread
   ```

2. **Module load via halcmd:**
   ```bash
   halcmd loadrt siggen
   halcmd show comp siggen
   halcmd unloadrt siggen
   ```

3. **Full simulation run:**
   - Start axis sim
   - Jog axes
   - Run G-code
   - Clean shutdown

### 9.3 Edge Cases

1. **Signal handling:**
   - SIGTERM during module load
   - SIGINT during thread execution
   - SIGSEGV in RT thread (should dump core, not hang)

2. **Resource cleanup:**
   - Verify no shared memory leaks after shutdown
   - Verify all threads exit

3. **Error recovery:**
   - Module load failure (missing .so)
   - Module load failure (bad parameters)
   - Thread creation failure (invalid period)

---

## 10. Migration / Compatibility Notes

### 10.1 What Breaks

1. **Direct rtapi_app invocation:**
   ```bash
   # This no longer works:
   rtapi_app load siggen
   ```
   Use `halcmd loadrt siggen` instead.

2. **RTAPI_FIFO_PATH environment variable:**
   No longer relevant — there is no socket.

3. **Multiple halcmd instances coordinating via socket:**
   Still works because they all use the same HAL shared memory.
   The socket was only for rtapi_app communication.

### 10.2 What Stays Compatible

1. **All HAL file syntax** — unchanged
2. **All halcmd commands** — unchanged
3. **All HAL API functions** — unchanged
4. **Component .so files** — unchanged
5. **INI file configuration** — unchanged

### 10.3 Debugging

Without `rtapi_app` as a separate process:

- **GDB:** Attach to the launcher process directly
- **Core dumps:** Generated by the launcher process
- **Logs:** All RT messages go to the launcher's stderr

---

## 11. Implementation Order

### Phase 4a: Create `rtapi_uspace.h` and stub implementation
- [ ] Write header file with API
- [ ] Create stub `.c` file that compiles but returns errors
- [ ] Update Submakefile to build library

### Phase 4b: Migrate `rtapi_uspace_init()` / `rtapi_uspace_cleanup()`
- [ ] Extract `harden_rt()`, `configure_memory()`, `initialize_app()`
- [ ] Implement init/cleanup lifecycle
- [ ] Add hal_lib auto-load
- [ ] Test standalone

### Phase 4c: Migrate `rtapi_uspace_module_load()` / `rtapi_uspace_module_unload()`
- [ ] Extract `do_load_cmd()`, `do_unload_cmd()`, `do_comp_args()`
- [ ] Remove socket response code
- [ ] Test with `halcmd_commands.cc` changes

### Phase 4d: Migrate `rtapi_uspace_threads_run()` / `rtapi_uspace_threads_stop()`
- [ ] Extract `run_threads()` / `sim_rtapi_run_threads()`
- [ ] Remove socket fd parameter and callback
- [ ] Add `rtapi_uspace_threads_stop()` for clean shutdown
- [ ] Test thread start/stop cycle

### Phase 4e: Update `halcmd_commands.cc`
- [ ] Modify `do_loadrt_cmd()` to use direct calls
- [ ] Modify `unloadrt_comp()` to use direct calls
- [ ] Test with existing halcmd binary

### Phase 4f: Update hal-go and launcher
- [ ] Add C shims to `cgo.go`
- [ ] Add Go functions to `command.go`
- [ ] Update launcher to use `hal.Init()` and `hal.CreateThread()`
- [ ] Remove `src/launcher/realtime/` package
- [ ] Update cleanup sequence

### Phase 4g: Delete socket code and rtapi_app binary
- [ ] Remove socket functions from `uspace_rtapi_app.c`
- [ ] Remove rtapi_app binary target from Makefile
- [ ] Update `scripts/realtime.in`
- [ ] Final testing

### Phase 4h: Documentation and cleanup
- [ ] Update man pages
- [ ] Update developer documentation
- [ ] Remove obsolete comments referencing rtapi_app socket

---

## 12. Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| RT timing regression | Medium | High | Benchmark before/after; keep old code path initially |
| Signal handling issues | Medium | Medium | Extensive testing with SIGTERM/SIGINT |
| Go runtime interference with RT | Low | High | Use `runtime.LockOSThread()` for RT thread |
| dlopen/dlclose issues | Low | Medium | Existing code already handles this |
| Shared memory corruption | Low | High | No changes to HAL data structures |

---

## 13. Appendix: Socket Protocol (for reference)

The deleted socket protocol used length-prefixed strings:

```
Client → Server:
  <count> <len1> <arg1> <len2> <arg2> ...
  
Server → Client:
  <result>

Where:
  count  = number of arguments (decimal, space-terminated)
  lenN   = length of argN (decimal, space-terminated)
  argN   = raw bytes (no terminator)
  result = integer return code (decimal, space-terminated)
```

Commands sent over socket:
- `load <module> [args...]`
- `unload <module>`
- `newinst <type> <name> [arg]`
- `exit`

This protocol is entirely eliminated by Phase 4.
