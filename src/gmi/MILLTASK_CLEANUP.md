# Milltask Cleanup: Remove Python, Enable Multi-Instance Interpreter

## Goal

Remove all Python/Boost.Python/CPython dependencies from milltask and the
RS274NGC interpreter. Replace external process execution (fork/exec for M1xx)
with cmod/gomod registration. Make the interpreter multi-instance capable by
eliminating global state and introducing a canon callback table.

## Motivation

- CPython GIL creates deadlocks when milltask and pyproxy share the runtime
- Boost.Python bindings deeply couple the interpreter to a single process context
- fork() for M100-M199 won't work without shared memory
- Multi-instance interpreter is required for concurrent G-code preview/execution
- Python extension features (remap, oword, task override) are used by <5% of configs

## Current State

### NML Removal — Done

All NML/libnml dependencies have been removed from the build:

**libnml.so:** Removed entirely. Build target deleted from `libnml/Submakefile`.
The Submakefile is now empty — only `linklist.cc` survives, compiled directly
by milltask.

**liblinuxcnc.a:** Reduced to 3 objects: `emcglb.o`, `emcpose.o`, `tool_shim.o`.
Removed objects:
- `emc.cc` (deleted — 2700 lines of CMS update methods)
- `emcargs.cc` (deleted — NML arg parsing)
- `emcops.cc` (deleted — stat constructors inlined into `emc_nml.hh`)
- `modal_state.cc` (moved exclusively to librs274)
- `canon_position.cc`, `interpl.cc`, `emc_symbol_lookup.cc` (moved to `emc/task/`)

**Dead NML clients removed from build:**
- `tcl/linuxcnc.so` (Tcl extension: `emcsh.cc`, `shcom.cc`) — dead NML
  channel client, was loaded by AXIS via `linuxcnc.tcl`. Load line commented
  out in `linuxcnc.tcl` / `linuxcnc.tcl.in`.
- `bin/linuxcnclcd` (`emclcd.cc`, `shcom.cc`, `sockets.c`) — dead NML LCD client.

**All remaining binaries are libnml-free:**
`io.so`, `iov2.so`, `milltask.so`, `halui.so`, `rs274`, `emcmodule.so`,
`motion-logger`, `gomc-server`.

**Circular include fix:** Created `emc/tooldata/tooldata_fwd.hh` to break the
`emc_nml.hh` ↔ `tooldata.hh` circular dependency (needed for inlined
`EMC_TOOL_STAT` constructor/operator=).

**Lightweight shims replacing libnml functions:**
- `emc/task/rcs_shim.cc` — `rcs_print` → `vfprintf(stderr)`,
  `etime` → `gettimeofday`, `esleep` → `usleep`, `RCS_TIMER` for milltask
- `emc/usr_intf/sockets.c` — `rcs_print_error` → `fprintf(stderr)` macro
- `emc/usr_intf/emcsh.cc` (removed) — had inline `-ini` parsing replacing
  `emcGetArgs`, `etime` → `gettimeofday`, `esleep` → `usleep`

**StateTag → state_tag_t:** `EMC_TRAJ_CMD_MSG.tag` and `EMC_TRAJ_STAT.tag`
changed from C++ `StateTag` (uses `std::bitset`) to POD `state_tag_t` in
shared memory structs. `StateTag` remains only in librs274/interpreter.

### Python Extension Points in Milltask

| Feature | Where | What it does |
|---------|-------|-------------|
| PythonPlugin singleton | `rs274ngc_pre.cc` Interp ctor | Always initialized, even without `[PYTHON]` |
| Boost.Python modules | `interpmodule.cc`, `taskmodule.cc`, `emccanon.cc` | Expose ~100+ internals to Python |
| Remap prolog/body/epilog | `interp_o_word.cc`, `interp_python.cc` | Custom G/M code handlers via Python |
| Python O-word subs | `interp_o_word.cc` | O\<name\> call dispatches to Python |
| Python Task override | `taskclass.cc`, `taskmodule.cc` | Override tool change, coolant, etc. |
| Inline `(py,...)` | `interp_convert.cc` | Execute Python from G-code comments |
| PLUGIN\_CALL | `emccanon.cc`, `taskclass.cc` | Queue Python for task context (unused) |
| IO\_PLUGIN\_CALL | `emccanon.cc`, `taskclass.cc` | Queue Python for IO context (unused) |

### External Process Execution

| Mechanism | Where | How |
|-----------|-------|-----|
| M100-M199 | `emctask.cc`, `emctaskmain_gomc.cc` | `fork()+execvp()`, async `waitpid` polling |
| EMC\_SYSTEM\_CMD | `emctaskmain_gomc.cc` | NML message → same fork/exec (only for M1xx) |
| POSTTASK\_HALFILE | `taskclass.cc` | `vfork()+execlp("halcmd")`, synchronous |

### Multi-Instance Blockers (~35 globals)

| Category | Variables | Fix |
|----------|-----------|-----|
| Canon state | `static CanonConfig_t canon`, `_tag`, `quat`, `chained_points`, `probefile`, `logfile` | Move into canon context struct |
| Canon API | ~110 free functions accessing globals | Replace with callback table |
| Command queue | `interp_list` (global NML queue) | Per-instance via canon context |
| Machine status | `emcStatus` pointer | Per-instance via canon context |
| Interpreter | `pinterp`, `_is` (setup pointer) | Already per-instance in `_setup` |
| Python plugin | `python_plugin` singleton | Remove entirely |
| NURBS state | `nurbs_order`, `nurbs_control_points` | Move into `_setup` |
| Error buffer | `savedError` | Move into `_setup` |
| M-code dirs | `user_defined_fmt[]`, `user_defined_function_dirindex[]` | Replace with registration |
| Tool table | `the_table` (global) | Per-instance via canon context |

## Architecture

### Implementation via GMI

All inter-module APIs (`interp_canon_t`, `interp_ext_t`, `task_ext_t`) are
defined as GMI interfaces. The GMI code generator produces:
- C struct definitions (callback tables)
- Go wrapper types with idiomatic method signatures
- Marshalling code for cgo boundary crossing
- Version-tagged structs for ABI compatibility

This ensures consistent interface evolution and eliminates hand-written
cgo boilerplate.

### Two Separate Extension APIs

The interpreter and task have different lifecycles and scopes. Separate APIs
enforce that the interpreter can be used standalone (preview, simulation)
without any task dependency.

```
┌─────────────────────────────────────────────────────────┐
│ gomc-server                                             │
│                                                         │
│  ┌──────────────────────┐   ┌────────────────────────┐  │
│  │ interp_canon_t       │   │ task_ext_t             │  │
│  │ (canon callback tbl) │   │ (task method overrides)│  │
│  │                      │   │                        │  │
│  │ Per-instance:        │   │ Singleton per milltask:│  │
│  │ - motion commands    │   │ - tool_prepare         │  │
│  │ - state queries      │   │ - tool_change          │  │
│  │ - IO control         │   │ - coolant_mist/flood   │  │
│  │ - interp_list ref    │   │ - lube                 │  │
│  │ - emcStatus ref      │   │ - estop                │  │
│  └──────┬───────────────┘   │ - mcode_handler[100]   │  │
│         │                   └────────────────────────┘  │
│         ▼                                               │
│  ┌──────────────────────┐                               │
│  │ interp_ext_t         │                               │
│  │ (extension callbacks)│                               │
│  │                      │                               │
│  │ Per-instance:        │                               │
│  │ - remap prolog/epilog│                               │
│  │ - remap body         │                               │
│  │ - oword handler      │                               │
│  └──────────────────────┘                               │
│                                                         │
│  Registration lives in librs274.so (per Interp instance)│
│  milltask exposes GMI pass-through (gomc_interp_ext_t)  │
│  so external cmods/gomods can register handlers         │
└─────────────────────────────────────────────────────────┘
```

### interp\_canon\_t — Canon Callback Table

Replaces the ~110 free functions in `canon.hh`. Each Interp instance receives
its own `interp_canon_t` at construction. The table carries a `void *ctx` that
the implementation uses to reach its private state (CanonConfig, interp\_list,
emcStatus, etc.).

```c
typedef struct interp_canon {
    void *ctx;

    // --- Motion ---
    void (*straight_traverse)(void *ctx, int lineno,
                               double x, double y, double z,
                               double a, double b, double c,
                               double u, double v, double w);
    void (*straight_feed)(void *ctx, int lineno,
                           double x, double y, double z,
                           double a, double b, double c,
                           double u, double v, double w);
    void (*arc_feed)(void *ctx, int lineno,
                      double first_end, double second_end,
                      double first_axis, double second_axis,
                      int rotation, double axis_end_point,
                      double a, double b, double c,
                      double u, double v, double w);
    void (*rigid_tap)(void *ctx, int lineno,
                       double x, double y, double z, double scale);
    void (*straight_probe)(void *ctx, int lineno,
                            double x, double y, double z,
                            double a, double b, double c,
                            double u, double v, double w,
                            unsigned char probe_type);
    void (*stop)(void *ctx);
    void (*dwell)(void *ctx, double seconds);
    void (*finish)(void *ctx);

    // --- Feed/motion control ---
    void (*set_feed_rate)(void *ctx, double rate);
    void (*set_feed_mode)(void *ctx, int spindle, int mode);
    void (*set_motion_control_mode)(void *ctx, int mode, double tolerance);
    void (*set_naivecam_tolerance)(void *ctx, double tolerance);
    void (*set_traverse_rate)(void *ctx, double rate);

    // --- Coordinate system ---
    void (*set_g5x_offset)(void *ctx, int origin,
                            double x, double y, double z,
                            double a, double b, double c,
                            double u, double v, double w);
    void (*set_g92_offset)(void *ctx,
                            double x, double y, double z,
                            double a, double b, double c,
                            double u, double v, double w);
    void (*set_xy_rotation)(void *ctx, double t);
    void (*use_length_units)(void *ctx, int units);
    void (*select_plane)(void *ctx, int plane);

    // --- Cutter compensation ---
    void (*set_cutter_radius_compensation)(void *ctx, double radius);
    void (*start_cutter_radius_compensation)(void *ctx, int direction);
    void (*stop_cutter_radius_compensation)(void *ctx);

    // --- Speed-feed sync ---
    void (*start_speed_feed_synch)(void *ctx, int spindle,
                                    double feed_per_rev, int vel_mode);
    void (*stop_speed_feed_synch)(void *ctx);

    // --- Spindle ---
    void (*set_spindle_mode)(void *ctx, int spindle, double mode);
    void (*set_spindle_speed)(void *ctx, int spindle, double rpm);
    void (*start_spindle_cw)(void *ctx, int spindle, int wait);
    void (*start_spindle_ccw)(void *ctx, int spindle, int wait);
    void (*stop_spindle)(void *ctx, int spindle);
    void (*orient_spindle)(void *ctx, int spindle,
                            double orientation, int mode);
    void (*wait_spindle_orient_complete)(void *ctx, int spindle,
                                         double timeout);

    // --- Tool ---
    void (*select_tool)(void *ctx, int tool);
    void (*start_change)(void *ctx);
    void (*change_tool)(void *ctx, int slot);
    void (*change_tool_number)(void *ctx, int number);
    void (*reload_tooldata)(void *ctx);
    void (*set_tool_table_entry)(void *ctx, int pocket, int toolno,
                                  double ox, double oy, double oz,
                                  double oa, double ob, double oc,
                                  double ou, double ov, double ow,
                                  double diameter,
                                  double frontangle, double backangle,
                                  int orientation);
    void (*use_tool_length_offset)(void *ctx,
                                    double x, double y, double z,
                                    double a, double b, double c,
                                    double u, double v, double w);

    // --- Coolant ---
    void (*flood_on)(void *ctx);
    void (*flood_off)(void *ctx);
    void (*mist_on)(void *ctx);
    void (*mist_off)(void *ctx);

    // --- Overrides ---
    void (*enable_feed_override)(void *ctx);
    void (*disable_feed_override)(void *ctx);
    void (*enable_speed_override)(void *ctx, int spindle);
    void (*disable_speed_override)(void *ctx, int spindle);
    void (*enable_feed_hold)(void *ctx);
    void (*disable_feed_hold)(void *ctx);
    void (*enable_adaptive_feed)(void *ctx);
    void (*disable_adaptive_feed)(void *ctx);

    // --- IO digital/analog ---
    void (*set_motion_output_bit)(void *ctx, int index);
    void (*clear_motion_output_bit)(void *ctx, int index);
    void (*set_aux_output_bit)(void *ctx, int index);
    void (*clear_aux_output_bit)(void *ctx, int index);
    void (*set_motion_output_value)(void *ctx, int index, double value);
    void (*set_aux_output_value)(void *ctx, int index, double value);
    int  (*wait_input)(void *ctx, int index, int input_type,
                       int wait_type, double timeout);

    // --- Clamping ---
    void (*clamp_axis)(void *ctx, int axis);
    void (*unclamp_axis)(void *ctx, int axis);
    int  (*lock_rotary)(void *ctx, int lineno, int joint);
    int  (*unlock_rotary)(void *ctx, int lineno, int joint);

    // --- Program flow ---
    void (*program_stop)(void *ctx);
    void (*optional_program_stop)(void *ctx);
    void (*program_end)(void *ctx);
    void (*pallet_shuttle)(void *ctx);

    // --- Messages/logging ---
    void (*comment)(void *ctx, const char *s);
    void (*message)(void *ctx, const char *s);
    void (*log_msg)(void *ctx, const char *s);
    void (*logopen)(void *ctx, const char *s);
    void (*logappend)(void *ctx, const char *s);
    void (*logclose)(void *ctx);
    void (*canon_error)(void *ctx, const char *msg);

    // --- Probe ---
    void (*turn_probe_on)(void *ctx);
    void (*turn_probe_off)(void *ctx);

    // --- Block delete / optional stop ---
    void (*set_block_delete)(void *ctx, int enabled);
    int  (*get_block_delete)(void *ctx);
    void (*set_optional_program_stop)(void *ctx, int enabled);
    int  (*get_optional_program_stop)(void *ctx);

    // --- State tag ---
    void (*update_tag)(void *ctx, int fields[16]);

    // --- Parameter file ---
    void (*set_parameter_file_name)(void *ctx, const char *name);
    void (*on_reset)(void *ctx);
    void (*canon_update_end_point)(void *ctx,
                                    double x, double y, double z,
                                    double a, double b, double c,
                                    double u, double v, double w);

    // --- Getters (interpreter reads machine state) ---
    double (*get_external_feed_rate)(void *ctx);
    double (*get_external_traverse_rate)(void *ctx);
    int    (*get_external_length_unit_type)(void *ctx);
    double (*get_external_length_units)(void *ctx);
    double (*get_external_angle_units)(void *ctx);
    int    (*get_external_motion_control_mode)(void *ctx);
    double (*get_external_motion_control_tolerance)(void *ctx);
    double (*get_external_motion_control_naivecam_tolerance)(void *ctx);
    int    (*get_external_flood)(void *ctx);
    int    (*get_external_mist)(void *ctx);
    void   (*get_external_position)(void *ctx, double *pos);  // 9-element array
    void   (*get_external_probe_position)(void *ctx, double *pos);
    double (*get_external_probe_value)(void *ctx);
    int    (*get_external_probe_tripped_value)(void *ctx);
    double (*get_external_speed)(void *ctx, int spindle);
    int    (*get_external_spindle)(void *ctx, int spindle);
    void   (*get_external_tool_length_offset)(void *ctx, double *off);
    int    (*get_external_tool_slot)(void *ctx);
    int    (*get_external_selected_tool_slot)(void *ctx);
    int    (*get_external_tool_table)(void *ctx, int pocket,
                                      int *toolno, double *offset,
                                      double *diameter,
                                      double *frontangle,
                                      double *backangle,
                                      int *orientation);
    int    (*get_external_digital_input)(void *ctx, int index, int def);
    double (*get_external_analog_input)(void *ctx, int index, double def);
    int    (*get_external_queue_empty)(void *ctx);
    int    (*get_external_axis_mask)(void *ctx);
    int    (*get_external_feed_override_enable)(void *ctx);
    int    (*get_external_spindle_override_enable)(void *ctx, int spindle);
    int    (*get_external_adaptive_feed_enable)(void *ctx);
    int    (*get_external_feed_hold_enable)(void *ctx);
    int    (*get_external_plane)(void *ctx);
    void   (*get_external_parameter_file_name)(void *ctx,
                                                char *buf, int max);
    int    (*get_external_tc_fault)(void *ctx);
    int    (*get_external_tc_reason)(void *ctx);
    int    (*get_external_offset_applied)(void *ctx);
    void   (*get_external_offsets)(void *ctx, double *offsets);

} interp_canon_t;
```

Enum parameters (`CANON_UNITS`, `CANON_PLANE`, etc.) are passed as `int` in the
C table to avoid header coupling. Constants defined in a shared header.

Complex struct parameters (`EmcPose`, `StateTag`) are flattened to primitive
arrays. `EmcPose` → 9 doubles. `StateTag` → `int fields[16]`.

`NURBS_FEED` and the NURBS math helpers are interpreter-internal (they compute
line segments and call `straight_feed`). They do NOT go in the canon table.

`PLUGIN_CALL` and `IO_PLUGIN_CALL` are removed (dead feature).

`USER_DEFINED_FUNCTION_ADD` is removed (replaced by task\_ext registration).

### interp\_ext\_t — Interpreter Extension API

Registered by cmod/gomod via the milltask GMI pass-through. The interpreter
dispatches to these instead of Python. The registration map and dispatch logic
live in **librs274.so** (per Interp instance). milltask exposes the registration
functions as a GMI interface (`gomc_interp_ext_t`) so external cmods/gomods
can call them.

```c
// Words parsed from a G-code block, passed to extension callbacks
typedef struct {
    unsigned int flags;     // bitmask: which words are present
    double x, y, z, a, b, c, u, v, w;
    double i, j, k;
    double p, q, r, l;
    double e, f, s, d, h;
    int    t_number;
    int    line_number;
} interp_block_words_t;

// Interpreter context passed to extension callbacks
typedef struct {
    void *interp;   // opaque interpreter handle

    // Read/write interpreter parameters (#1, #<named>, etc.)
    double (*get_param)(void *interp, const char *name);
    int    (*set_param)(void *interp, const char *name, double val);

    // Tool table queries
    int (*find_tool_pocket)(void *interp, int tool_number);

    // Error reporting
    void (*set_error)(void *interp, const char *msg);

    // Canon access (same interface the interpreter uses)
    const struct CanonInterface *canon;

    // Resume phase counter (0 = first call, 1+ = after INTERP_EXECUTE_FINISH)
    int phase;

    // Per-registration user data (passed back from register call)
    void *user;
} interp_ext_ctx_t;

// Return values (match existing INTERP_OK/ERROR/EXECUTE_FINISH)
#define INTERP_EXT_OK              0
#define INTERP_EXT_ERROR          -1
#define INTERP_EXT_EXECUTE_FINISH  3  // pause, flush motion, call again

// --- Handler callback typedefs ---

// O-word sub: O<name> call [#1] [#2] ...
typedef int (*interp_oword_fn)(interp_ext_ctx_t *ctx,
                                const char *name,
                                const double *args, int n_args,
                                double *retval);

// Remap prolog: validate args, set params before body
typedef int (*interp_remap_prolog_fn)(interp_ext_ctx_t *ctx,
                                       const interp_block_words_t *words);

// Remap body: the handler itself (alternative to NGC sub)
typedef int (*interp_remap_body_fn)(interp_ext_ctx_t *ctx,
                                     const interp_block_words_t *words);

// Remap epilog: commit results after NGC body returns
typedef int (*interp_remap_epilog_fn)(interp_ext_ctx_t *ctx,
                                       double return_value,
                                       int value_returned);
```

#### Registration — Two Layers

**Layer 1: librs274.so (C++ methods + C-linkage wrappers)**

The Interp class holds a per-instance `std::unordered_map<std::string, handler_entry>`
and provides registration methods. C-linkage wrappers are exported for use by
milltask (which links librs274.so):

```c
// C-linkage API exported from librs274.so
// (InterpBase* is the opaque handle milltask already has)
int interp_ext_register_oword(void *interp, const char *name,
                               interp_oword_fn fn, void *user);
int interp_ext_register_remap_prolog(void *interp, const char *name,
                                      interp_remap_prolog_fn fn, void *user);
int interp_ext_register_remap_body(void *interp, const char *name,
                                    interp_remap_body_fn fn, void *user);
int interp_ext_register_remap_epilog(void *interp, const char *name,
                                      interp_remap_epilog_fn fn, void *user);
```

**Layer 2: milltask GMI pass-through (gomc_interp_ext_t)**

milltask exposes these as a GMI interface. The `ctx` captures the Interp
pointer; each function simply forwards to the librs274 C API:

```c
typedef struct gomc_interp_ext {
    void *ctx;

    int (*register_oword)(void *ctx, const char *name,
                           interp_oword_fn fn, void *user);
    int (*register_remap_prolog)(void *ctx, const char *name,
                                  interp_remap_prolog_fn fn, void *user);
    int (*register_remap_body)(void *ctx, const char *name,
                                interp_remap_body_fn fn, void *user);
    int (*register_remap_epilog)(void *ctx, const char *name,
                                  interp_remap_epilog_fn fn, void *user);
} gomc_interp_ext_t;
```

The `void *user` is stored alongside the function pointer and passed back
through `interp_ext_ctx_t.user` so the cmod/gomod can maintain its own state.

### task\_ext\_t — Task Extension API

Replaces Python Task method overrides and M100-M199 fork/exec.

Task method overrides (tool change, coolant, etc.) are **synchronous** — they
run inline in the task loop and return immediately. These are fast operations
that set HAL pins or send NML commands.

M100-M199 handlers run on a **separate thread** with an `abort_fd` for clean
cancellation. This keeps the task loop responsive for status updates, abort
processing, and estop handling while the M-code executes.

```c
typedef struct {
    void *task;     // opaque task handle

    // Access to emcStatus fields needed by task callbacks
    // (specific accessor functions TBD based on actual usage)
} task_ext_ctx_t;

// Task method override callbacks (synchronous, run in task loop)
typedef int (*task_tool_prepare_fn)(task_ext_ctx_t *ctx,
                                     int tool, int pocket);
typedef int (*task_tool_change_fn)(task_ext_ctx_t *ctx, int pocket);
typedef int (*task_coolant_fn)(task_ext_ctx_t *ctx, int on);
typedef int (*task_lube_fn)(task_ext_ctx_t *ctx, int on);
typedef int (*task_estop_fn)(task_ext_ctx_t *ctx, int on);
typedef int (*task_io_init_fn)(task_ext_ctx_t *ctx);
typedef int (*task_io_halt_fn)(task_ext_ctx_t *ctx);

// M100-M199 handler context (passed to handler on its own thread)
typedef struct {
    int    abort_fd;    // eventfd, becomes readable on abort/estop
    int    mcode;       // the M-code number (100-199)
    double p;           // P argument from G-code
    double q;           // Q argument from G-code
    double result;      // handler writes result here (read by task)
    void  *user;        // per-registration user data
} task_mcode_ctx_t;

// M-code handler — runs on its own thread, must poll abort_fd.
// Returns 0 = success, -1 = error, -2 = aborted.
typedef int (*task_mcode_fn)(task_mcode_ctx_t *ctx);

typedef struct gomc_task_ext {
    void *ctx;

    int (*register_tool_prepare)(void *ctx, task_tool_prepare_fn fn,
                                  void *user);
    int (*register_tool_change)(void *ctx, task_tool_change_fn fn,
                                 void *user);
    int (*register_coolant_mist)(void *ctx, task_coolant_fn fn,
                                  void *user);
    int (*register_coolant_flood)(void *ctx, task_coolant_fn fn,
                                   void *user);
    int (*register_lube)(void *ctx, task_lube_fn fn, void *user);
    int (*register_estop)(void *ctx, task_estop_fn fn, void *user);
    int (*register_io_init)(void *ctx, task_io_init_fn fn, void *user);
    int (*register_io_halt)(void *ctx, task_io_halt_fn fn, void *user);

    // Register handler for specific M-code (100-199)
    int (*register_mcode)(void *ctx, int mcode, task_mcode_fn fn,
                           void *user);
} gomc_task_ext_t;
```

#### M-code Execution Flow

```
1. Interpreter reads M1xx → queues internal mcode command on interp_list
2. Task loop dequeues command, looks up registered handler for mcode N
3. If no handler registered → error "M1xx: no handler registered"
4. Task creates eventfd (abort_fd), populates task_mcode_ctx_t
5. Task spawns thread calling handler(ctx)
6. Task enters WAITING_FOR_MCODE_HANDLER state
7. Each task loop cycle:
   a. Process NML commands (including abort/estop)
   b. Update and publish status
   c. Check handler thread (pthread_tryjoin_np / non-blocking)
   d. If abort requested: write to abort_fd, wait with timeout, force-cancel
8. Handler finishes → task reads ctx->result, sets execState = DONE
```

#### M-code Handler Example (cmod)

```c
// Wait for a pneumatic clamp sensor, respecting abort
int clamp_mcode(task_mcode_ctx_t *ctx) {
    // Activate clamp via HAL pin
    hal_pin_set_bit("clamp.activate", 1);

    // Poll sensor, checking abort_fd
    struct pollfd pfd = { .fd = ctx->abort_fd, .events = POLLIN };
    while (!hal_pin_get_bit("clamp.clamped")) {
        if (poll(&pfd, 1, 100) > 0) {   // 100ms poll timeout
            hal_pin_set_bit("clamp.activate", 0);  // cleanup
            return -2;  // aborted
        }
    }
    ctx->result = 0;
    return 0;
}
```

On the Go/gomod side, the abort_fd maps to a `context.Context` cancellation
or a channel read, providing idiomatic Go abort handling.

### POSTTASK\_HALFILE

Handled by gomc-server launcher calling internal halcmd directly. No API needed.

### INI Configuration Changes

```ini
# BEFORE (Python remap):
[RS274NGC]
REMAP = T   prolog=prepare_prolog ngc=prepare epilog=prepare_epilog
REMAP = M6  modalgroup=6 prolog=change_prolog ngc=change epilog=change_epilog

[PYTHON]
TOPLEVEL = python/toplevel.py
PATH_PREPEND = python

# AFTER (cmod/gomod remap):
[RS274NGC]
REMAP = T   prolog=prepare_prolog ngc=prepare epilog=prepare_epilog
REMAP = M6  modalgroup=6 prolog=change_prolog ngc=change epilog=change_epilog
# prolog/epilog names resolve to registered cmod/gomod handlers
# ngc= still works (NGC subroutine files, no Python needed)
# py= removed

# [PYTHON] section no longer needed
```

The `REMAP` syntax stays the same. The `prolog=`/`epilog=` names now resolve
to cmod/gomod-registered handlers instead of Python functions. The `py=` keyword
is removed. `ngc=` continues to work for NGC subroutine bodies.

## Implementation Phases

### Phase 1: Canon Callback Table

**Goal:** Replace ~110 global canon functions with `interp_canon_t`.
No behavior change — pure refactoring.

1. Define `interp_canon_t` struct in new header `src/emc/nml_intf/interp_canon.h`
2. Implement `emccanon_make_table()` in `emccanon.cc` — creates an
   `interp_canon_t` whose callbacks call the existing global implementations.
   The `ctx` holds pointers to `canon`, `interp_list`, `emcStatus`, etc.
3. Add `interp_canon_t *canon` parameter to `Interp::init()`. Store as member.
4. Mechanically replace all `STRAIGHT_FEED(...)` calls in `rs274ngc/*.cc` with
   `canon->straight_feed(canon->ctx, ...)`. This is ~250 call sites, but each
   is a trivial search-and-replace.
5. Similarly replace all `GET_EXTERNAL_*()` calls.
6. Move `static CanonConfig_t canon` and related statics into a struct allocated
   per-instance by `emccanon_make_table()`.
7. Remove the free-function declarations from `canon.hh`.
8. SAI (`saicanon.cc`) gets its own table implementation — it already has
   its own canon function bodies. gcodemodule.cc is replaced entirely by
   server-side ngcpreview (see Phase 5).

**Validation:** All existing configs work unchanged. Two Interp instances can
coexist (tested with preview + execution).

### Phase 2: Remove Python from Interpreter

**Goal:** Strip all Python/Boost.Python from rs274ngc.

1. Remove `PythonPlugin::instantiate()` from `Interp::Interp()`.
2. Remove `python_plugin` references from `interp_python.cc`.
3. In `pycall()` dispatch: if handler name is registered via `interp_ext_t`,
   call the registered C function. If not registered, return error
   "handler not found" (instead of calling Python).
4. Remove `interpmodule.cc` (the Boost.Python binding module).
5. Remove `from interpreter import this` setup in constructor.
6. Remove `PYUSABLE` checks throughout — replace with ext registration checks.
7. Remove `python_plugin.hh` / `python_plugin.cc` from milltask link.
8. Remove `boost_python` and `python3-embed` from milltask build deps.

**Validation:** Configs without `[PYTHON]`/`REMAP` work unchanged.
Configs with `REMAP ngc=` work (NGC subroutine bodies don't need Python).
Configs with `REMAP py=` fail with clear error (expected).

### Phase 3: Interpreter Extension API

**Goal:** Implement extension handler registration so cmod/gomod can
register remap prologs/epilogs and oword handlers.

#### Architecture

The registration API lives in **librs274.so** as part of the Interp class.
This is natural because:
- The Interp owns the dispatch (it replaces pycall)
- The registry is per-instance (multi-instance ready)
- Non-milltask consumers (SAI) get an empty registry — handlers
  are never registered there, dispatch returns error, which is correct
  (preview doesn't need prolog/epilog side effects)

**milltask.so** (the only consumer that creates an Interp AND loads cmods)
exposes a GMI pass-through interface (`gomc_interp_ext_t`) so that external
cmods/gomods can register handlers. This is a thin wrapper: milltask captures
the Interp pointer and forwards registration calls to librs274.

```
┌───────────────────────────────────────────────────────────┐
│ librs274.so                                               │
│                                                           │
│  Interp class                                             │
│    ├─ ext_registry (name → handler_entry map)             │
│    ├─ register_oword(name, fn, user)                      │
│    ├─ register_remap_prolog(name, fn, user)               │
│    ├─ register_remap_body(name, fn, user)                 │
│    ├─ register_remap_epilog(name, fn, user)               │
│    └─ dispatch: pycall() stub → registry lookup → call    │
└────────────────────────────────────────────▲──────────────┘
                                             │ links
┌────────────────────────────────────────────┴──────────────┐
│ milltask.so (cmod in gomc-server)                          │
│                                                           │
│  Owns the Interp instance                                 │
│  Exposes GMI pass-through: gomc_interp_ext_t              │
│    register_oword(ctx, name, fn, user) →                  │
│        interp->register_oword(name, fn, user)             │
└────────────────────────────────────────────▲──────────────┘
                                             │ GMI (cmod_env_t)
┌────────────────────────────────────────────┴──────────────┐
│ External cmod/gomod (e.g. stdglue.so)                      │
│                                                           │
│  Calls env->interp_ext->register_remap_prolog(ctx, ...)   │
└───────────────────────────────────────────────────────────┘
```

#### Implementation Steps

1. Define `interp_ext.h` header in `src/emc/rs274ngc/`:
   - Handler callback typedefs (`interp_oword_fn`, `interp_remap_prolog_fn`, etc.)
   - `interp_ext_ctx_t` struct (context passed to handlers at call time)
   - Return value constants (`INTERP_EXT_OK`, `INTERP_EXT_ERROR`,
     `INTERP_EXT_EXECUTE_FINISH`)
2. Add registration methods to Interp class (C++ side, per-instance map).
3. Add C-linkage wrapper functions exported from librs274.so for use by
   milltask (takes `InterpBase*` + args, casts and calls C++ methods).
4. Wire dispatch: replace the `pycall()` error stub with registry lookup.
   If handler found → populate `interp_ext_ctx_t` → call handler.
   If not found → return error "handler 'X' not registered".
5. Implement `interp_ext_ctx_t` population:
   - `get_param` / `set_param` → read/write interpreter parameters
   - `find_tool_pocket` → tool table query
   - `set_error` → set interpreter error message
   - `canon` → pointer to the instance's canon interface
   - `phase` counter for EXECUTE\_FINISH yield/resume
6. Define `gomc_interp_ext.gmi` — the GMI interface file for the
   pass-through registration API exposed by milltask.
7. Implement the pass-through in milltask (thin: capture Interp*, forward).
8. Port `stdglue.py` (prepare\_prolog, change\_prolog, change\_epilog) to a
   reference `stdglue` cmod that registers via the GMI API.

**Validation:** Remap configs using stdglue functions work with the cmod.
Custom remap prologs/epilogs can be written as cmods/gomods.

### Phase 4: Task Extension API + M-code Registration

**Goal:** Replace Python task overrides and M100-M199 fork/exec.

1. Define `task_ext_t` / `gomc_task_ext_t` header.
2. Replace `TaskWrap` (Boost.Python) with C callback dispatch in
   `taskclass.cc` — each Task virtual method checks for registered handler,
   falls through to default C++ if none.
3. Replace `emcSystemCmd()` fork/exec with threaded mcode handler dispatch.
4. Remove `EMC_SYSTEM_CMD` NML message type.
5. Replace `WAITING_FOR_SYSTEM_CMD` with `WAITING_FOR_MCODE_HANDLER` task
   exec state — polls handler thread completion each cycle.
6. Implement abort path: on abort/estop, write to handler's `abort_fd`,
   wait with timeout, then force-cancel thread.
6. Remove `user_defined_fmt[]` / `user_defined_function_dirindex[]` from
   `emctask.cc`.
7. Remove `taskmodule.cc` (Boost.Python bindings).
8. POSTTASK\_HALFILE: call halcmd internally from gomc-server launcher.
9. Remove PLUGIN\_CALL / IO\_PLUGIN\_CALL entirely.
10. Port iocontrol-v2 Python task class to a cmod (if anyone uses it).

**Validation:** M1xx codes work via registered gomods.
Default tool change (iocontrol-based) works without Python.

### Phase 5: Multi-Instance Interpreter + Server-Side Preview — Done

**Goal:** Enable multiple concurrent Interp instances. Primary driver:
server-side G-code preview that runs concurrently with execution.

1. Move remaining file-scoped statics into `_setup`:
   - `nurbs_order`, `nurbs_control_points` → `_setup`
   - `savedError` → `_setup`
2. Each Interp instance gets its own `interp_canon_t` at construction.
3. Each `interp_canon_t` instance has its own CanonConfig, interp\_list,
   and status reference.
4. Tool table access goes through canon getters (already done in Phase 1).
5. ~~Implement **preview canon**~~ — **Done.** `ngcpreview` gomod in
   `src/gomc/internal/ngcpreview/module.go` implements a recording canon
   (C callbacks via `canon_callbacks_t`) that stores segments (traverse,
   feed, arc+center+rotation, probe), dwells, and tool changes. Returns
   JSON via REST endpoint `POST /api/v1/ngcpreview/file`.
6. Preview Interp instance runs with `interp_ext = NULL`. Extensions are
   skipped entirely — remapped codes that have `ngc=` subs still execute
   (normal NGC sub call, correct geometry), but prolog/epilog/body
   extension callbacks are not invoked. This is correct because preview
   doesn't need side effects, only geometry.
7. ~~gomc-server exposes preview via REST endpoint~~ — **Done.** Client
   sends `{filename, initcodes, unitcode}`, server creates preview Interp
   + preview canon, runs interpretation, returns JSON geometry.
8. ~~Python client module~~ — **Done.** `lib/python/gcode.py` is a pure
   Python REST client that replaces the old `gcodemodule.cc` C extension.
   Provides `parse()`, `strerror()`, `MIN_ERROR`, `arc_to_segments()`,
   `calc_extents()`. All existing `import gcode` consumers (glcanon,
   gremlin, qt5_graphics, qtvcp, axis) work unchanged.
9. Test: create two Interp instances — one for execution, one for preview.
   They must not interfere. Preview must produce identical geometry to
   execution canon for the same program (modulo side-effect-only codes).

**Completed items:**
- `src/gomc/internal/ngcpreview/module.go` — ngcpreview gomod with preview
  canon (C callbacks via `canon_callbacks_t`). Features:
  - Segment storage: traverse, feed, arc (center+rotation), probe
  - Metric→inches conversion for all linear quantities (positions,
    feedrate, tool offset, g5x/g92 offsets, arc centers)
  - Per-axis `get_external_position_*` returning program-unit values
    (separate `prog_pos[9]` for interpreter feedback vs `pos[9]` in inches)
  - XY rotation capture (`set_xy_rotation` callback → PreviewResult field)
  - Plane tracking (`select_plane` callback → PreviewResult field)
  - G5x/G92 offset capture (passed to Python canon for rotate\_and\_translate)
  - NaN/Inf sanitization, initcodes split-by-newline execution
- `src/gmi/idl/ngcpreview.gmi` — IDL with PreviewResult containing segments,
  dwells, tool\_changes, g5x\_index, g5x\_offset, g92\_offset, xy\_rotation, plane
- `lib/python/gcode.py` — REST client replacing gcodemodule.cc. Sets g5x/g92
  offsets, rotation\_cos/sin, and plane on the Python canon before replaying
  segments. Includes pure-Python `arc_to_segments()` and `calc_extents()`.
- `src/emc/rs274ngc/gcodemodule.cc` — **deleted** (source removed)
- `lib/python/gcode.so` — **deleted** (build target removed from Submakefile)
- All `import gcode` consumers work via the new Python module without changes
- File-scope statics eliminated from interpreter:
  - `current_phase`, `current_user` → `_setup.ext_phase`, `_setup.ext_user`
  - `endpoint[2]`, `endpoint_valid` → `_setup.qc_endpoint[2]`, `_setup.qc_endpoint_valid`
  - `qc()` function-local static vector → `_setup.qc_queue` (opaque pointer)
  - `nurbs_order`, `nurbs_control_points`, `savedError` — already in `_setup`
  - `qc_reset()`, `qc_scale()`, `set_endpoint()` — signatures updated to take `setup_pointer`

**Remaining:**
- None — Phase 5 complete. Concurrent multi-instance validated by
  ngcpreview creating a fresh Interp per request while milltask runs its own.

### Phase 6: Cleanup — Done

1. ~~Remove `#include <Python.h>` and `#include <boost/python.hpp>` throughout.~~
   Removed by deleting all dead source files below. Python/Boost remain only
   in active UI components (emcmodule.so, delta kins, HAL components).
2. ~~Remove `python3-embed` from `packages.conf`.~~ N/A — gomc packages.conf
   never had it. System-level Python/Boost deps stay for UI components.
3. ~~Remove `libboost_python` from link flags.~~ Already removed from
   librs274 and milltask Submakefiles in Phase 2. Remains only for
   kinematics Python wrappers and HAL components.
4. ~~Remove `src/emc/pythonplugin/` directory.~~ Done — deleted entirely,
   removed from Makefile Submakefile includes, removed `libpyplugin.so.0`.
5. ~~Remove `emccanon.cc` Boost.Python canon bindings (`canonmodule.cc`).~~
   Done — `canonmodule.cc` deleted.
6. Documentation: this file.

**Deleted files:**
- `src/emc/pythonplugin/` (entire directory: python_plugin.cc, .hh, testpp.cc, Submakefile)
- `src/emc/task/taskmodule.cc`
- `src/emc/rs274ngc/canonmodule.cc`
- `src/emc/rs274ngc/interpmodule.cc`
- `src/emc/rs274ngc/pyblock.cc`
- `src/emc/rs274ngc/pyarrays.cc`
- `src/emc/rs274ngc/pyinterp1.cc`
- `src/emc/rs274ngc/pyemctypes.cc`
- `src/emc/rs274ngc/pyparamclass.cc`
- `src/emc/rs274ngc/interp_python.hh`
- `src/emc/rs274ngc/boost_pyenum_macros.hh`
- `src/emc/rs274ngc/paramclass.hh`
- `src/emc/rs274ngc/array1.hh`
- `src/emc/rs274ngc/interp_array_types.hh`
- `src/emc/rs274ngc/gcodemodule.cc` (was untracked; old reference copy)
- `lib/libpyplugin.so.0` (stale build artifact)

**Build modified:**
- `src/Makefile` — removed `emc/pythonplugin` from Submakefile include list

## Files Modified/Removed

### Phase 1 — Done

**New:**
- `src/gmi/idl/canon.gmi` — GMI interface definition for canon callback table
- `src/gomc/generated/gmi/canon/canon_api.h` — generated `canon_callbacks_t` struct
- `src/emc/rs274ngc/canon_interface.hh` — C++ wrapper class hiding ctx plumbing, type conversions
- `src/emc/task/emccanon_table.cc` — milltask canon table implementation (populates `canon_callbacks_t`)
- `src/emc/task/emccanon_table.hh` — `emccanon_get_callbacks()` declaration

**Modified:**
- `src/emc/rs274ngc/interp_base.hh` — added `set_canon_callbacks()` pure virtual, `canon_callbacks_t` forward decl
- `src/emc/rs274ngc/rs274ngc_interp.hh` — added `set_canon_callbacks()` override, CanonInterface member
- `src/emc/rs274ngc/rs274ngc_pre.cc` — `set_canon_callbacks()` implementation
- `src/emc/sai/saicanon.cc` — own `saicanon_table` for standalone interpreter
- `src/emc/task/emctask.cc` — calls `set_canon_callbacks(emccanon_get_callbacks())`

### Phase 2 — Done

**Removed from librs274.so build** (files still exist for Phase 4 milltask):
- `src/emc/rs274ngc/interpmodule.cc` — removed from LIBRS274SRCS
- `src/emc/rs274ngc/canonmodule.cc` — removed from LIBRS274SRCS
- `src/emc/rs274ngc/pyparamclass.cc` — removed from LIBRS274SRCS
- `src/emc/rs274ngc/pyemctypes.cc` — removed from LIBRS274SRCS
- `src/emc/rs274ngc/pyinterp1.cc` — removed from LIBRS274SRCS
- `src/emc/rs274ngc/pyblock.cc` — removed from LIBRS274SRCS
- `src/emc/rs274ngc/pyarrays.cc` — removed from LIBRS274SRCS

**Rewritten (stubs):**
- `src/emc/rs274ngc/interp_python.cc` — error stubs for pycall/py\_execute/etc.

**Modified:**
- `src/emc/rs274ngc/interp_internal.hh` — removed PYUSABLE, python\_plugin extern, pythis, hollowed pycontext
- `src/emc/rs274ngc/rs274ngc_pre.cc` — removed Python init/exit/configure
- `src/emc/rs274ngc/interp_o_word.cc` — CT\_PYTHON\_OWORD\_SUB → error, removed pystuff access
- `src/emc/rs274ngc/interp_remap.cc` — python= → error, prolog/epilog accepted, removed pydict
- `src/emc/rs274ngc/interp_namedparams.cc` — PA\_PYTHON → error
- `src/emc/rs274ngc/interp_setup.cc` — removed pythis init/destructor
- `src/emc/rs274ngc/Submakefile` — removed py\*.cc, Boost/Python link deps, gcodemodule build target
- `src/emc/task/taskclass.cc` — removed PyInit\_interpreter/emccanon from builtin\_modules
- `src/emc/task/emctask.cc` — removed python\_plugin.hh include

### Phase 3 — Done

**New:**
- `src/emc/rs274ngc/interp_ext.h` — thin wrapper with _API_CGO guards around generated interp_ext_api.h + C-linkage declarations
- `src/emc/rs274ngc/interp_ext.cc` — InterpExtRegistry (per-instance map), ctx accessor implementations, C-linkage exports
- `src/emc/task/stdglue.c` — reference remap handler cmod (plain C), replaces Python stdglue.py
- `configs/sim/axis/remap/stdglue-cmod/` — test config for stdglue.so

**Modified:**
- `src/emc/rs274ngc/interp_python.cc` — rewritten: pycall() dispatches to ext registry, maps return codes
- `src/emc/rs274ngc/interp_o_word.cc` — CT_PYTHON_OWORD_SUB dispatches via ext registry; handler_returned() returns pycall status
- `src/emc/rs274ngc/interp_internal.hh` — added `int last_status` to pycontext (handler_returned pass-through)
- `src/emc/rs274ngc/rs274ngc_interp.hh` — added ext_registry pointer + ext dispatch methods
- `src/emc/rs274ngc/rs274ngc_pre.cc` — ext_registry init in ctor, destroy in dtor
- `src/emc/rs274ngc/Submakefile` — added interp_ext.cc to LIBRS274SRCS
- `src/emc/task/emctaskmain_gomc.cc` — milltask New() calls emcTaskPlanCreate() + registers interp_ext_api
- `src/emc/task/emctask.cc` — split emcTaskPlanInit() into emcTaskPlanCreate() (Interp construction) + emcTaskPlanInit() (init + startup gcode)
- `src/emc/task/task.hh` — added emcTaskPlanCreate() declaration
- `src/emc/task/Submakefile` — added stdglue.so build rules

**Design decisions:**
- interp_ext_api is now GMI-generated (was hand-written, migrated in Phase 4b)
- interp_ext_ctx_t has ~35 accessor function pointers for block words, setup state, and canon calls
- milltask registers API in New() (not Start()) — Interp creation split out so no implicit ordering dependencies
- stdglue.so looks up API in Start() — clean lifecycle: register in New(), lookup in Start()
- ctx_set_param() calls add_named_param() before store_named_param() (matching Python ParamClass.setitem behavior)

### Phase 4a: M-code Handler Registration — Done

**New:**
- `src/gmi/idl/mcode_handler.gmi` — GMI interface for M-code handler registration
- `src/gomc/generated/gmi/mcode_handler/mcode_handler_api.h` — generated mcode_handler_callbacks_t
- `src/emc/task/test_mcode_handler.c` — test harness for mcode handler cmod

**Modified:**
- `src/emc/task/emctaskmain_gomc.cc` — registers mcode_handler_api, dispatches M1xx via registered handlers

### Phase 4b: GMI Migration of interp_ext + interp_ctx — Done

**New GMI IDL features:**
- `ptr` primitive type (void* in C)
- `callback name(params) -> rettype` declaration (function pointer typedefs)
- `@import api_name` directive (cross-API header dependency)

**New:**
- `src/gmi/idl/interp_ctx.gmi` — ~30 accessor functions for interpreter context
- `src/gmi/idl/interp_ext.gmi` — @import interp_ctx, 3 callback types, 3 registration functions

**Deleted:**
- `src/emc/task/mcode_handler_api.h` — replaced by generated mcode_handler_api.h
- `src/emc/rs274ngc/interp_ext_api.h` — replaced by generated interp_ext_api.h

**Modified:**
- `src/emc/rs274ngc/interp_ext.h` — thin wrapper with _API_CGO guards around generated header
- `src/emc/rs274ngc/interp_ext.cc` — renamed types (interp_ext_oword_fn_cb, etc.), ctx→get_phase()/get_user() accessors
- `src/emc/rs274ngc/rs274ngc_interp.hh` — updated callback type names
- `src/emc/task/stdglue.c` — renamed types, ctx->get_phase(ctx->ctx), ctx->ctx
- `src/emc/task/emctaskmain_gomc.cc` — renamed types
- `src/emc/rs274ngc/Submakefile` — -I paths for generated headers (librs274)
- `src/emc/task/Submakefile` — -Igomc/pkg/cmodule for gomc_api.h
- `src/emc/sai/Submakefile` — -I paths for generated headers
- `src/gmi/codegen/Submakefile` — codegen rules for mcode_handler, interp_ctx, interp_ext

### Phase 4 remaining — Done

**Dead files deleted (were already excluded from build, now removed from tree):**
- `src/emc/pythonplugin/python_plugin.cc` — deleted (Phase 6)
- `src/emc/pythonplugin/python_plugin.hh` — deleted (Phase 6)
- `src/emc/task/taskmodule.cc` — deleted (Phase 6)

**Already deleted:**
- `src/emc/rs274ngc/gcodemodule.cc` — replaced by `lib/python/gcode.py` + server-side ngcpreview

All functional items complete:
- M100-M199 threaded handler dispatch with abort_fd — done (emctaskmain_gomc.cc)
- fork/exec replaced — no fork/execvp/waitpid remain
- WAITING_FOR_MCODE_HANDLER state — implemented
- Task extension API (tool_prepare, coolant, etc.) — not needed as separate API; standard HAL/iocontrol path suffices

## Resolved Questions

1. ~~**stdglue scope**~~: **Resolved.** stdglue is a replaceable cmod.
   The `interp_ext_ctx_t` accessor API exposes what conceptually makes
   sense for any remap handler — not just what today's stdglue.py uses.
   Design for the general case: tool state, pocket selection, coordinate
   systems, motion mode, etc. Accessors are read-only so there's no risk
   in exposing more than currently needed. A future replacement handler
   shouldn't be limited by a too-narrow API.

2. ~~**G-code preview multi-instance**~~: **Resolved.** Preview runs
   server-side with a recording canon that emits JSON geometry. Extensions
   (`interp_ext`) are NULL for preview — NGC sub bodies still execute for
   correct geometry, but extension callbacks are skipped. Client is a
   pure renderer receiving geometry via REST/WS. See Phase 5.

3. ~~**NGC sub bodies with remap**~~: **Not a question.** NGC sub bodies
   (`ngc=prepare`) work today without Python and continue unchanged.
   The prolog sets named params (#\<tool\>, #\<pocket\>), the NGC sub
   reads them. This path never touches Python and needs no migration.

4. ~~**Inline `(ext, ...)` vs O-word**~~: **Resolved.** Drop inline
   extension support entirely. O-word registration covers the same
   functionality. No `(ext, ...)` comment syntax.
