# milltask — Feature-Parity Gaps vs C++ 2.9

**Status:** register (living) · **Date:** 2026-07-09 · **Scope:** `src/gomc` milltask + canon + motctl

Derived from a parity audit of all **95** `EMC_*` command types dispatched in the
C++ `emctaskmain.cc`, matched against the gomc surfaces (Task methods, canon
callbacks, the `motctl` motion interface, the IO controller) and verified against
the source. Most commands are implemented at the expected layer; this file
records what is **not** — as gaps to fix, or as intentional divergences.

Legend: **MISSING** = no equivalent anywhere · **PARTIAL** = present but a
sub-behavior is absent · gaps are grouped by operational impact.

---

## Intentional divergences (NOT gaps — do not "fix")

- **User M-codes M100–199 (`EMC_SYSTEM_CMD`) — no shell-script fork/exec.**
  C++ forks an external `M1xx` script per call. gomc deliberately dropped this:
  spawning an external process per M-code is inefficient and a security risk.
  M-code handlers must instead be compiled **cmod/gomod** modules that register
  via the `mcode_handler` GMI. Consequence: stock configs that ship shell-script
  M-codes will not run them (handler faults with "no handler for M1xx"). This is
  by design; migrating such configs means porting the script to a cmod/gomod.
- **Python task/IO plugins (`EMC_EXEC_PLUGIN_CALL`, `EMC_IO_PLUGIN_CALL`) — absent.**
  No Python plugin infrastructure in gomc. Affects only Python-remap/plugin
  configs.
- **`TASK_INIT`, `PLAN_INIT`, `PLAN_CLOSE`, `PLAN_END`** — handled at startup /
  folded into `abortLocked` / no-ops in C++ too. No runtime command needed.
- **`SPINDLE_CONSTANT`** — a no-op in C++ (`return 0`); matched.
- **Runtime NML setters for joint backlash/ferror/limits** — gomc sets these via
  INI + HAL pins rather than runtime NML commands. Functionally equivalent for
  normal operation (GUIs almost never send these live).
- **Multi-spindle index** — verified fully threaded end-to-end (not hardcoded to
  0). No gap; recorded because it was a suspected risk.

---

## Tier 1 — operational impact (fix)

| # | Command | Problem | Verified | Pointer |
|---|---|---|---|---|
| 1 | `SPINDLE_WAIT_ORIENT_COMPLETE` | ~~**Timeout ignored → infinite hang.**~~ **FIXED.** `waitForCompletion` now passes the command through; `waitSpindleOriented(timeout)` enforces a deadline and faults with `ExecError` once the M19 timeout elapses (matches C++). A non-positive timeout keeps the wait-indefinitely behavior (abort/fault still end it). Covered by `orient_timeout_test.go`. | fixed | `sequencer.go` `waitSpindleOriented` |
| 2 | `JOINT_LOAD_COMP` | **Leadscrew compensation silently absent.** No `COMP_FILE` INI key; `motctl.set_joint_comp` (`motctl.gmi:253`) has no caller. Machines relying on screw-error mapping get zero compensation. | yes (grep: no `COMP_FILE`, no `SetJointComp` caller) | `config.go` (no key), `motctl.gmi:253` (unused) |

## Tier 2 — user-visible (fix)

| # | Command | Problem | Verified | Pointer |
|---|---|---|---|---|
| 3 | `TASK_PLAN_FORWARD` | ~~**Silent no-op.**~~ **FIXED.** Added the `AutoForward=5` constant and `case AutoForward: return t.motion.Forward()` in `autoCommand` (mirrors `AutoReverse`; the `preflightAuto` fall-through already covers it). Covered by `auto_forward_test.go`. | fixed | `commands.go` `autoCommand` |
| 4 | canon `MESSAGE` / `(MSG,…)` | ~~**G-code operator messages never reach the UI.**~~ **FIXED.** `Message()` now enqueues a `DisplayMsgCmd` that publishes to the operator-display channel (`operatorDisplay` → `ErrorPublisher.OperatorDisplay`, `ErrorKind_OPERATOR_DISPLAY`) when the sequencer reaches it — in program order, not read-ahead. Covered by `message_display_test.go`. `(LOG,…)`/logfile still no-op (minor). | fixed | `canon.go` `Message`, `messages.go` `operatorDisplay` |
| 5 | `TRAJ_SET_OFFSET` (tool offset) | **`ToolOffset` reported as zero.** Offset is folded into canon coordinate math (moves are correct), but `motion.SetOffset` is never called, and `stat.go` reads `ToolOffset` from motion status → UIs show stale/zero tool offset. (Same item getstat audit flagged.) | yes (grep: `motion.SetOffset` never called) | `stat.go:182`; `SetOffset` uncalled |
| 6 | `TRAJ_SET_FO/FH/SO_ENABLE` | **PARTIAL — no GUI-immediate command.** The three override-enable toggles are forced ON at machine-on. Capability *is* reachable via G-code (M48/M49/M50/M51/M53 → `EnableFeedOverride`/`FeedHoldEnableCmd`), but there is no immediate command for a GUI to toggle them without an MDI M-code. | yes (downgraded from MISSING) | `commands.go:254-257` (machine-on only); G-code path `canon.go:1262/1275` |

## Tier 3 — minor / edge

| # | Command | Problem | Pointer |
|---|---|---|---|
| 7 | `TRAJ_CLEAR_PROBE_TRIPPED_FLAG` | `motctl.clear_probe_flags` declared, no caller / no command path. Low impact (motion clears on probe start). | `motctl.gmi:296` |
| 8 | `TOOL_UNLOAD` | `IOController.ToolUnload()` exists but nothing calls it; a UI tool-unload has no path. | `task.go:186` |
| 9 | `SET_DEBUG` | Forwarded only to `motion.SetDebug`; C++ also sets IO debug + `status.debug`. `io.SetDebug` never invoked. | `commands.go:1609` |
| 10 | `AUX_INPUT_WAIT` (M66) | Analog-input wait semantics differ (polls analog as boolean for edge/level; C++ restricts non-immediate waits to digital). Edge case. | `canon.go:~1349` |
| 11 | `JOINT_ENABLE`/`DISABLE` | Only whole-machine amp enable/disable; no per-joint command. Rarely used. | — |
| 12 | `JOINT_SET_HOMING_PARAMS` | Runtime HAL push zeroes `home_final_vel`/`search_vel`/`latch_vel`/`flags`/`volatile_home`. Homing params are normally INI-fixed. | `inihal.go:302-312` |
| 13 | `TASK_PLAN_EXECUTE` multi-level MDI | MDI o-word sub-calls that yield `INTERP_EXECUTE_FINISH` may not be driven to completion the way a running program's loop handles it. **Needs a test to confirm.** | `commands.go` `executeMDI` |

---

## Suggested order if fixing

Cheap + high value first: ~~#1 orient timeout~~ (**done**), ~~#3 `PLAN_FORWARD`~~
(**done**), ~~#4 `(MSG,…)` → operator-display~~ (**done**), **#5 ToolOffset**
(send `SetOffset` to motion or source it from canon). **#2 `LOAD_COMP`** is the
largest (INI key + file parse + `set_joint_comp` wiring) and only matters for
comp-file machines. Tier 3 as encountered. **#13** should get a test regardless.
</content>
