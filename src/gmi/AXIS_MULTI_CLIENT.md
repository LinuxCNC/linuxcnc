# Axis Multi-Client Architecture

## Goal

Centralize all UI state in the server-side companion (currently `axis_ui` cmod)
so that multiple axis client instances can run simultaneously without conflicts.

## Current Status (May 2026)

| Feature | Status | Commit |
|---------|--------|--------|
| REST routes for cmod APIs | ✅ Done | c57b08d3b8 |
| File sync across instances | ✅ Done | 7bc84a70b2 |
| Default program loading in milltask | ✅ Done | d959ec9af0 |
| Jog speed slider sync | ✅ Done | 853e62427f |
| Progress.done() crash fix | ✅ Done | 2ae38c2100 |
| axisui cmod removed | ✅ Done | c63af91cdd |
| Mode management (remove ensure_mode) | ❌ Pending | — |
| Remove c.mode() calls from axis | ❌ Pending | — |

## Original Problems (mostly solved)

- ~~Each axis client holds its own state (loaded file, slider values, jog increment, mode)~~
  → File syncs from stat.file; jog speed syncs via stat + slider update
- ~~Starting a second client re-issues mode switches and file opens, conflicting with running programs~~
  → Default file loaded by milltask at startup; clients read stat.file
- HAL pins are "last writer wins" — no defined owner in multi-client setups
  → axisui cmod removed; HAL pins owned by halui
- ~~Startup during program execution causes rejected NML commands and slow window appearance~~
  → Client syncs existing state from server, no mode switches on startup

## Architecture

```
┌──────────────────────┐     ┌─────────────────────────────────┐
│  axis client (thin)  │     │  axis_ui server component       │
│                      │◄───►│  (cmod or gomod in gomc-server) │
│  - renders UI        │ WS  │                                 │
│  - sends user intent │     │  Owns:                          │
│  - syncs display     │     │  - loaded_file                  │
│  - zero NML calls    │     │  - jog_increment / jog_axis     │
│                      │     │  - feed_override                │
└──────────────────────┘     │  - spindle_override             │
                             │  - max_velocity                 │
┌──────────────────────┐     │  - coordinate_type              │
│  axis client #2      │◄───►│  - HAL pins                     │
└──────────────────────┘     │  - file open logic              │
                             │  - mode management              │
                             │                                 │
                             │  Lives in same process as       │
                             │  task/motion (gomc-server)      │
                             │  → no IPC, direct function calls│
                             └─────────────────────────────────┘
```

## Key Principles

1. **Server owns state** — clients are stateless views that push user intent
2. **State sync via watch** — all clients subscribe and get pushed updates when state changes (another client changes jog increment → all update)
3. **Command serialization** — server decides mode switches, queues if interpreter busy
4. **File management centralized** — `open_file(path)` is an API call; server handles mode switch + plan_synch + program_open atomically
5. **HAL pins reflect server state** — not "last client to set wins"

## Migration Order (incremental)

1. **Sliders** (feed override, spindle override, max velocity, jog speed) ✅
   - Server owns values via emcstat watch
   - Client syncs from stat polling with blackout timers to prevent feedback loops
   - Jog speed uses logarithmic slider mapping (setval/val2vel)

2. **Jog settings** (axis, increment) — partially done
   - Jog speed: ✅ syncs via stat.jog_speed / stat.ajog_speed
   - Jog axis/increment: still client-local (low priority — rarely conflicts)

3. **File management** (open, reload, close) ✅
   - milltask loads `[DISPLAY]OPEN_FILE` at startup (no UI involvement)
   - Clients detect `stat.file != loaded_file` in update() loop → sync
   - File open via emccmd API → all clients get notified via stat watch

4. **Mode management** — pending
   - Next step: remove `ensure_mode()` from axis.py
   - Remove direct `c.mode()` calls
   - Server (milltask) handles mode switches internally for commands that need them

## Implementation Options

The server-side component can be implemented as either:

- **cmod** (current `axis_ui`): C module loaded by gomc-server, exposes HAL pins directly
- **gomod**: Pure Go module within gomc-server — may be preferable if the state logic becomes complex enough that Go's ergonomics help, and HAL pin access can be done via the Go HAL bindings

Choose based on whether HAL pin manipulation or state logic dominates the complexity.

## What This Solves

| Problem | Solution |
|---------|----------|
| Startup conflicts during execution | Server knows state; client just syncs |
| Mode switch races between clients | Single command issuer |
| Slider/pin inconsistency | Server is authority |
| Duplicate file opens | Server tracks loaded file; new client reads it |
| NML replacement path | Server uses GMI internally; NML eliminated |

## Notes

- gomc-server unifies cmod + task + motion in one process — "moving logic to server" means no IPC overhead
- NML will be replaced by GMI calls (one of the goals of the gomc project)
- The axisui `.gmi` IDL already defines part of this interface — expand it incrementally
- Existing axis client code can be thinned step by step (remove ensure_mode, remove direct c.mode() calls)
- axisui cmod has been removed — axis talks directly to emccmd/emcstat APIs
- Multi-client sync uses blackout timers (1s) to prevent feedback loops when local changes propagate
