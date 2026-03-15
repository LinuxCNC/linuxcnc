# Launcher HAL-Go Migration — Prepare Phase

This document tracks the migration from halcmd subprocesses to direct hal-go API calls in the launcher.

## Background

The launcher initializes a HAL component (`hal.NewComponent("launcher")`) after realtime start — exactly
as every halcmd subprocess calls `hal_init("halcmd")` before doing any HAL work. This enables direct
API calls to `hal_start_threads()`, `hal_stop_threads()`, etc. without spawning subprocesses.

## Migration Status

| # | Item | API | Status |
|---|------|-----|--------|
| 1 | Load HAL files (`halcmd -f <file>`) | `halfile.Executor.ExecuteAll()` | ✅ Done (Go re-implementation) |
| 2 | Start HAL threads (`halcmd start`) | `hal.StartThreads()` | ✅ Done |
| 3 | Stop HAL threads (`halcmd stop`) | `hal.StopThreads()` | ✅ Done |
| 4 | Unload all HAL components (`halcmd unload all`) | `hal.UnloadAll(0)` | ✅ Done |
| 5 | List HAL components (`halcmd list comp`) | `hal.ListComponents()` | ✅ Done |
| 6 | Load realtime modules (`halcmd loadrt`) | `hal.LoadRT()` | 🔲 Pending |
| 7 | Load userspace components (`halcmd loadusr`) | `hal.LoadUSR()` | 🔲 Pending |

## Notes

- The launcher component (`hal.NewComponent("launcher")`) is initialized right after `rtMgr.Start()`,
  before any HAL API calls are made. This ensures `hal_init()` has been called, which is required
  by all HAL C API functions.
- `hal.Exit()` is called as the very last operation in cleanup, after all other HAL operations
  are complete (after realtime stop, NML cleanup, lock release).
- The HAL SHUTDOWN script (step 3 in cleanup) still uses a halcmd subprocess because it executes
  arbitrary user-defined HAL commands from a file.
