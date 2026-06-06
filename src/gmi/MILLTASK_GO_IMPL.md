# Milltask Go Rewrite — Implementation Plan

## Current Status (2026-05-25)

Integration test: **61 pass, 0 fail, 4 xfail** (configs/sim/test/tasktest.ini)

### Done
- ✅ Task struct + dependency interfaces (motctl, emcio, motstat clients)
- ✅ INI config loading (loadConfig → motctl calls for traj/joint/axis/spindle)
- ✅ HAL pins (inihal component — runtime INI parameter override)
- ✅ Module registration (`gomc.RegisterModule("milltask", factory)`)
- ✅ Lifecycle fix: API lookups in Start() (not factory/New)
- ✅ Integration: loads via `load milltask` in lib/hallib/linuxcnc.hal
- ✅ Launcher cleanup: no more hasTask special handling
- ✅ emccmd API (27 command handlers) — registered for C callers (halui) and WS
- ✅ emcstat API (GetStat → StatFull) — registered + WebSocket watch with delta push
- ✅ State machine (estop/estop_reset/off/on transitions)
- ✅ Mode switching (manual/auto/mdi) with guards
- ✅ All 27 emccmd handlers implemented (commands.go)
- ✅ Interpreter integration (CInterp wrapping librs274.so via C shim)
- ✅ Canon callbacks in Go (straight_traverse, straight_feed, arc_feed, dwell, spindle, coolant, tool-length, offsets)
- ✅ Sequencer goroutine (executes QueuedCmd from interpQueue)
- ✅ Readahead with backpressure (waitSequencerDrain on EXECUTE_FINISH)
- ✅ Pause/resume with channel signaling
- ✅ Single step (sequencer-level pause after each motion command, waitMotionDone)
- ✅ Program run (goroutine reads interpreter lines, enqueues canon commands)
- ✅ MDI execution (single command — synch, execute, interpDoneCmd)
- ✅ MDI queue (buffer multiple commands, dequeue in mdiDoneCmd.PostWait)
- ✅ Continuous jog + jog stop
- ✅ Homing / unhoming (delegates to motctl)
- ✅ Spindle on/off/increase/decrease
- ✅ Coolant flood/mist on/off
- ✅ Overrides: feed, spindle, rapid, max velocity
- ✅ Teleop enable/disable, override limits
- ✅ Optional stop, block delete flags
- ✅ Position logger (poslog.go — ring buffer + WS push)
- ✅ Tools REST API (tooldata shim + GET/PUT/DELETE endpoints)
- ✅ CGO bridge error propagation (all exports return -1 on error)
- ✅ ProgramOpen works in any state/mode (matches C milltask)
- ✅ interpState race fix: sequencer abort path no longer overwrites InterpReading
- ✅ Tool change cycle (M6: ToolPrepare → waitIO → ToolLoad → waitIO → interp synch)
- ✅ M-code handler worker (M100-199 with abort support)
- ✅ Load tool table (delegates to io.ToolLoadTable)
- ✅ NO_FORCE_HOMING (skip homing check before MDI/AUTO when INI flag set)
- ✅ External offset applied check (block AUTO RUN if external offsets active)
- ✅ Operator error ring (errors from sequencer/interp pushed to emcerror drain)
- ✅ RCS return code convention (rcsDone=1 returned for halui compatibility)
- ✅ Poll interval aligned to 10ms (100Hz, matching C milltask cycle time)
- ✅ Stat: ToolInSpindle + PocketPrepped populated from IO controller
- ✅ Program file cleared on ESTOP (fixes run_requires_file test)
- ✅ Default program loading at startup ([DISPLAY]OPEN_FILE loaded by milltask, not UI)

### XFAILs (known issues, not regressions)
1. **jog/incremental** — wrong distance (units/scale bug in motctl or motion)
2. **homing/unhome** — homed flag not clearing in motstat after unhome
3. ~~**program/step**~~ — FIXED (implemented sequencer-level step mode)
4. ~~**program/run_requires_file**~~ — FIXED (clear programOpen on ESTOP)
5. **spindle/forward+reverse** — spindle enabled flag not reflected in motstat
6. ~~**misc/load_tool_table**~~ — FIXED (delegates to io.ToolLoadTable)

### Pending Work — Priority Order

#### Tier 1: All Done ✅
All items previously in Tier 1 are now implemented.

#### Tier 2: Stat accuracy (UI shows wrong values)
| # | Item | Description | Status |
|---|------|-------------|--------|
| 8 | ~~Line tracking~~ | Set currentLine/readLine/motionLine from interp + sequencer | ✅ Done |
| 9 | ~~Active G/M codes~~ | Read from interpreter after each line, publish in stat | ✅ Done |
| 10 | Spindle state | Read spindle direction/enabled from motstat properly | xfail (motion-side) |
| 11 | Unhome flag | Ensure motstat reflects unhome (may be motion-side bug) | xfail (motion-side) |

#### Tier 3: Edge cases / advanced
| # | Item | Description | Status |
|---|------|-------------|--------|
| 13 | Incremental jog fix | Debug units/scale in jog_incr path | xfail (motctl-side) |
| 14 | ~~Task plan synch~~ | Sync interpreter position with motion actual | ✅ Done |
| 15 | ~~Wait complete~~ | Poll execState until ExecDone or timeout | ✅ Done |
| 16 | ~~Readahead exec states~~ | WAITING_FOR_IO, WAITING_FOR_DELAY, SPINDLE_ORIENT | ✅ Done |
| 17 | ~~Probe result~~ | Publish probed_position in stat from motstat | ✅ Done |
| 18 | ~~Operator error ring~~ | Push errors to emcerror ring (not just slog) | ✅ Done |
| 19 | ~~Feed hold / adaptive feed~~ | Controlled by interpreter via canon (M52/M53) | ✅ N/A (no UI cmd needed) |
| 20 | ~~Program end rewind~~ | Reset interpreter to line 0 on M2/M30 | ✅ Done (interpDoneCmd + re-open on run) |

### Milestone targets
- **Tier 1 complete** ✅ → usable for basic machining with Axis UI
- **Tier 1+2 complete** ✅ → UI shows correct state, suitable for daily use
- **All tiers** → 3 remaining xfails are motion-module bugs, not task-level; can delete cmod/milltask.so

## Overview

Rewrite milltask from C++ cmod (~3,580 lines in emctaskmain_gomc.cc) to a Go
gomod. The C version routes commands through NML message structs and three
large switch statements. The Go design eliminates NML — GMI methods on the
Task struct are the direct command handlers.

The Go milltask is already the default (loaded via `load milltask` in
linuxcnc.hal). The old C milltask.so exists only as fallback reference.

## Architecture

```
UI (Go/REST/WebSocket)
  │
  ▼
emccmd GMI method → Task.Jog() / Task.Home() / ...
  │
  ├─ guard check: requireState(On), requireMode(Manual), ...
  ├─ execute: t.motctl.JogCont(...)
  └─ return error or nil
```

For interpreter-generated commands (AUTO/MDI):

```
Interpreter (C++ librs274.so via thin C shim)
  │  calls canon callbacks
  ▼
Canon (Go) → pushes QueuedCmd to interpQueue
  │
  ▼
Sequencer goroutine:
  for cmd := range t.interpQueue {
      cmd.Execute(t)
      t.waitFor(cmd.WaitKind())
  }
```

## Key Design Decisions

### 1. No NML message types

The ~80 NML structs used as internal command tokens are replaced by:
- **UI commands**: direct method calls on `Task` struct (27 emccmd GMI methods)
- **Interpreter commands**: typed `QueuedCmd` interface values (~20 concrete types)

### 2. No big switch statements

Current `emcTaskPlan()` (720 lines, state×mode×command filter) becomes guard
methods called at the top of each Task method:

```go
func (t *Task) Jog(...) error {
    if err := t.requireState(StateOn); err != nil { return err }
    if err := t.requireMode(ModeManual); err != nil { return err }
    return t.motctl.JogCont(...)
}
```

Current `emcTaskIssueCommand()` (770 lines, type→function dispatch) is
eliminated — each GMI method IS the dispatch.

### 3. Canon callbacks implemented in Go

The `canon_callbacks_t` vtable (from canon.gmi) already supports Go
implementations. The interpreter calls canon through C function pointers;
these point into Go via cgo exports. No C++ canon code needed.

### 4. INI via inifile.IniFile

Go modules receive `*inifile.IniFile` directly in their Factory. No custom
INI parsing — use `ini.Get("TRAJ", "COORDINATES")` etc.

### 5. Interpreter stays C++

A thin C shim (~100 lines) wraps `InterpBase*` virtual calls:
- `interp_init()`, `interp_open(file)`, `interp_read()`
- `interp_execute(cmd)`, `interp_synch()`, `interp_close()`

The shim is the ONLY C++ code in the Go milltask.

## Package Structure

```
src/gomc/internal/task/
    task.go            // Task struct, state types, dependency interfaces
    guards.go          // requireOn(), requireMode(), requireInterpIdle(), externalOffsetApplied()
    commands.go        // 27 emccmd method implementations + runProgram/seekToLine
    sequencer.go       // interpreter queue execution loop (goroutine) + QueuedCmd types
    canon.go           // canon callback implementations (push QueuedCmd)
    canon_bridge.go    // generated cgo //export thunks for canon callbacks
    canon_bridge_init.go  // canon bridge initialization (register vtable)
    canon_bridge_manual.go // hand-written canon bridge helpers
    canon_bridge_table.c  // C vtable pointing into Go exports
    canon_getters.go   // canon getter callbacks (position, tool, probe, etc.)
    gen_canon_bridge.py   // code generator for canon_bridge.go
    interp.go          // cgo wrapper for interpreter C shim (CInterp)
    interp_iface.go    // Interpreter interface definition
    interp_shim.cc     // thin C++ shim wrapping InterpBase* calls
    interp_shim.h      // header for interp shim
    module.go          // gomc.Module lifecycle (factory, Start, Stop, Destroy)
    api_provider.go    // EmccmdCallbacks + EmcstatCallbacks implementations
    api_cbridge.go     // CGO //export functions for C callers (halui)
    config.go          // INI reading at startup (joints, axes, traj, spindles)
    inihal.go          // inihal HAL component (runtime parameter override)
    stat.go            // BuildStat() — fills StatFull from motstat + internal state
    watches.go         // WebSocket watch registration (emcstat, poslogger)
    poslog.go          // Position logger (ring buffer, WS push)
    tools.go           // Tool table REST endpoints (via tooldata shim)
    monitor.go         // Periodic health check goroutine (estop, errors, inihal)
    mcode_handler.go   // M100-199 handler worker goroutine
    mcode_bridge.go    // CGO bridge for M-code trampoline
    task_test.go       // Unit tests (guards, state transitions, commands)
    sequencer_test.go  // Unit tests (sequencer, pause/step/abort)
    canon_test.go      // Unit tests (canon offset math)
    getstat_test.go    // Unit tests (stat assembly)
    monitor_test.go    // Unit tests (monitor health checks)
```

## Dependency Interfaces (mockable for tests)

```go
type MotionController interface {
    JogCont(joint int, vel float64, jjogmode int) error
    HomeJoint(joint int) error
    TrajLinearMove(...) error
    TrajCircularMove(...) error
    TrajAbort() error
    // ... (matches motctl GMI)
}

type IOController interface {
    FloodOn() error
    FloodOff() error
    MistOn() error
    MistOff() error
    ToolPrepare(pocket, tool int) error
    ToolChange() error
    // ... (matches emcio GMI)
}

type Interpreter interface {
    Init() error
    Open(file string) error
    Read() (int, error)
    Execute(cmd string) (int, error)
    Synch() error
    Close() error
}
```

## QueuedCmd Interface

```go
type WaitType int
const (
    WaitNone WaitType = iota
    WaitMotion
    WaitIO
    WaitMotionAndIO
    WaitDelay
    WaitSpindleOriented
)

type QueuedCmd interface {
    Execute(t *Task) error
    WaitKind() WaitType
}
```

Concrete types: LinearMove, CircularMove, SpindleOn, SpindleOff,
ToolPrepare, ToolChange, Dwell, SetOffset, Probe, RigidTap, etc.

## Guard Matrix (from emcTaskPlan)

The acceptance rules extracted from the current implementation:

### Always accepted (any state, any mode)
- set_state, set_mode, abort, set_debug, set_optional_stop, set_block_delete
- set_feed_override, set_spindle_override, set_rapid_override, set_max_velocity

### Requires StateOn + ModeManual
- jog, jog_stop, home, unhome, override_limits

### Requires StateOn + ModeAuto
- auto_cmd(RUN) — also requires: program loaded, not external_offset_applied
- auto_cmd(PAUSE/RESUME/STEP/REVERSE/FORWARD)

### Requires StateOn + ModeMDI
- mdi

### Requires StateOn (any mode)
- spindle, flood, mist, brake, lube, teleop_enable

### Special: jog in AUTO/MDI
- jog/jog_stop accepted in AUTO IDLE and MDI IDLE (allow_while_idle_type)

### Queued (go through IO)
- load_tool_table

## Test Strategy

### Unit tests (src/gomc/internal/task/task_test.go)
- Guard matrix: state×mode×command acceptance/rejection
- State transitions: estop→on sequence, idempotency
- ProgramOpen: works in any mode/state
- Run with mock interpreter + mock motion

### Integration tests (configs/sim/test/tasktest.ini)
Python test harness that starts the full system (gomc-server + sim HAL config)
and exercises all commands via the emccmd/emcstat C API with assertion on stat changes.
The tasktest module is a Go gomod loaded after milltask in the HAL file.

Categories: state, mode, motion, jog, homing, program, mdi, spindle, coolant,
override, option, abort, misc. Currently 65 tests (61 pass, 4 xfail).

### How to run
```bash
# Unit tests
cd src/gomc && LD_LIBRARY_PATH=../../lib go test ./internal/task/

# Integration tests
./scripts/linuxcnc configs/sim/test/tasktest.ini

# Build
cd src && make ../bin/gomc-server
```

## Effort Estimate (remaining work)

| Subsystem | Status | Notes |
|-----------|--------|-------|
| Abort cleanup (spindle/coolant/IO/queue) | ✅ Done | abortLocked() handles all |
| Tool change cycle (M6) | ✅ Done | ToolPrepare→waitIO→ToolLoad→waitIO→synch |
| M-code handler worker (M100-199) | ✅ Done | mcodeHandler goroutine + abort |
| MDI queue | ✅ Done | mdiDoneCmd.PostWait dequeues next |
| Line tracking + active G/M codes | ✅ Done | readLine/currentLine + updateActiveCodes |
| NO_FORCE_HOMING + load_tool_table | ✅ Done | INI flag + io.ToolLoadTable |
| Single step | ✅ Done | sequencer-level pause after motion |
| Stat: ToolInSpindle/PocketPrepped | ✅ Done | from IO controller |
| Operator error ring push | ✅ Done | operatorError at all error paths |
| External offset check | ✅ Done | block AUTO RUN if offsets applied |
| RCS return codes | ✅ Done | rcsDone=1 for halui compat |
| Spindle state in motstat | xfail | motion-module doesn't expose state properly |
| Unhome flag in motstat | xfail | motion-module side bug |
| Incremental jog fix | xfail | units/scale issue in motctl or motion |

Remaining xfails are all at the motion-module boundary, not task-level bugs.
No additional Go code needed in the task package for full functional parity.

## What Gets Eliminated Entirely

- NML message types (~80 structs with type discriminants)
- emcTaskPlan() nested switch (720 lines)
- emcTaskIssueCommand() switch (770 lines)
- emccmd_handlers.cc (351 lines — NML struct construction)
- emccmd_slot.cc (78 lines — condvar handoff)
- interp_list linked list (204 lines)
- rcs_shim.cc (131 lines)
- linklist.cc (~200 lines)
- emc_symbol_lookup.cc (297 lines)
- backtrace.cc (65 lines)
- canon_position.cc (278 lines — becomes Go struct)
- emccanon_table.cc (607 lines — Go implements canon directly)

## References

- `src/gmi/idl/emccmd.gmi` — 27 UI command methods
- `src/gmi/idl/canon.gmi` — canon callback interface (Go binding exists)
- `src/gmi/idl/motctl.gmi` — motion controller commands
- `src/gmi/idl/motstat.gmi` — motion status readback
- `src/gmi/idl/ini.gmi` — INI query API
- `src/gomc/pkg/inifile/` — pure Go INI parser
- `src/emc/task/emctaskmain_gomc.cc` — current implementation (reference)
- `src/emc/rs274ngc/canon_interface.hh` — interpreter's canon vtable wrapper
