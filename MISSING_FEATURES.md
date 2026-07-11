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
| 2 | `JOINT_LOAD_COMP` | ~~**Leadscrew compensation silently absent.**~~ **FIXED.** `loadJoint` now reads `[JOINT_n]COMP_FILE`/`COMP_FILE_TYPE`, parses the `nominal forward reverse` triplets (`loadJointComp`), applies the type-0 position→diff conversion (matching C++ usrmotLoadComp), and pushes each to the already-implemented `motctl.SetJointComp`. Added `SetJointComp` to the `MotionConfig` interface. Covered by `config_comp_test.go`. | fixed | `config.go` `loadJoint`/`loadJointComp` |

## Tier 2 — user-visible (fix)

| # | Command | Problem | Verified | Pointer |
|---|---|---|---|---|
| 3 | `TASK_PLAN_FORWARD` | ~~**Silent no-op.**~~ **FIXED.** Added the `AutoForward=5` constant and `case AutoForward: return t.motion.Forward()` in `autoCommand` (mirrors `AutoReverse`; the `preflightAuto` fall-through already covers it). Covered by `auto_forward_test.go`. | fixed | `commands.go` `autoCommand` |
| 4 | canon `MESSAGE` / `(MSG,…)` | ~~**G-code operator messages never reach the UI.**~~ **FIXED.** `Message()` now enqueues a `DisplayMsgCmd` that publishes to the operator-display channel (`operatorDisplay` → `ErrorPublisher.OperatorDisplay`, `ErrorKind_OPERATOR_DISPLAY`) when the sequencer reaches it — in program order, not read-ahead. Covered by `message_display_test.go`. `(LOG,…)`/logfile still no-op (minor). | fixed | `canon.go` `Message`, `messages.go` `operatorDisplay` |
| 5 | `TRAJ_SET_OFFSET` (tool offset) | ~~**`ToolOffset` reported as zero.**~~ **FIXED.** `stat.go` now sources `ToolOffset` from the canon snapshot (`cs.toolOffset`, the value `toAbsolute` applies) instead of the always-zero motion echo — matching C++ which reports `task.toolOffset` from SET_OFFSET (emctaskmain.cc:1889), and consistent with G5x/G92. `TestGetStat_Positions` now uses a canon offset distinct from the motion mock to prove the source. | fixed | `stat.go` positions block |
| 6 | `TRAJ_SET_FO/FH/SO_ENABLE` | ~~**PARTIAL — no GUI-immediate command.**~~ **FIXED.** Added REST commands `set_fo_enable`/`set_fh_enable`/`set_so_enable` (emccmd.gmi) → `api_provider` → Task `SetFeedOverrideEnable`/`SetFeedHoldEnable`/`SetSpindleOverrideEnable` → `motion.FeedScaleEnable`/`FeedHoldEnable`/`SpindleScaleEnable`. A GUI can now toggle the three overrides without an MDI M-code; the G-code path (M48–M53) is unchanged. Covered by `override_enable_test.go`. | fixed | `emccmd.gmi`, `commands.go`, `api_provider.go` |

## Tier 3 — minor / edge

| # | Command | Problem | Pointer |
|---|---|---|---|
| 7 | `TRAJ_CLEAR_PROBE_TRIPPED_FLAG` | ~~no caller~~ **FIXED.** The audit under-rated this: C++ `TURN_PROBE_ON` appends `CLEAR_PROBE_TRIPPED_FLAG` to the interp_list, but gomc's `TurnProbeOn` was an empty stub — the probe-tripped flag was never cleared at probe start. `TurnProbeOn` now enqueues a `ClearProbeFlagsCmd` (→ `motion.ClearProbeFlags`) in program order before the STRAIGHT_PROBE move, matching C++. Added `ClearProbeFlags` to the `MotionController` interface (motctl client already implements it). `probe_clear_test.go`. | fixed | `canon.go` `TurnProbeOn` |
| 8 | `TOOL_UNLOAD` | ~~unwired~~ **FIXED.** Added REST `tool_unload` → `Task.ToolUnload` → `io.ToolUnload` (reject-while-busy, interp synch after). `tier3_batch_test.go`. | fixed |
| 9 | `SET_DEBUG` | ~~motion-only~~ **FIXED.** `SetDebug` now forwards to both `motion.SetDebug` and `io.SetDebug` and records the level to `stat.debug`. `tier3_batch_test.go`. | fixed |
| 10 | `AUX_INPUT_WAIT` (M66) | **NOT A GAP (verified).** The interp (`interp_convert.cc:3286`) rejects analog + non-immediate wait (`NCE_ANALOG_INPUT_WITH_WAIT_NOT_IMMEDIATE`) and always calls `wait_input(e, ANALOG, 0, 0)` — immediate. gomc wraps that same librs274ngc, so `WaitInputCmd`'s analog rise/fall/high/low branch is unreachable dead code (analog always hits the `WaitType==0` early return). Every valid M66 case (digital any-mode, analog immediate) matches C++. The audit's "semantics differ" was a misread of the dead branch, which has now been removed (the poll loop is digital-only, guarded, with the invariant documented). | `canon.go` `WaitInputCmd` |
| 11 | `JOINT_ENABLE`/`DISABLE` | **NOT A GAP (verified).** The motion handler `EMCMOT_JOINT_ENABLE_AMPLIFIER`/`DISABLE` is a no-op — byte-identical to LinuxCNC 2.9's, which is also a no-op. Amp-enable is driven automatically by the servo loop (`control.c: amp_enable = GET_JOINT_ENABLE_FLAG`), following machine-enable + joint-active. The per-joint command is vestigial in upstream; exposing it via GMI would wire a do-nothing command. Correctly omitted. | `command.c:1462` (stub) |
| 12 | `JOINT_SET_HOMING_PARAMS` | ~~**BUG:** runtime HAL home/offset/seq change zeroed the other homing params~~ **FIXED.** `loadJoint` caches the INI-fixed params (`jointHomingParams` on the Task); inihal copies them in `initPins` and re-pushes them intact instead of `0`. `homing_params_test.go`. | fixed |
| 13 | `TASK_PLAN_EXECUTE` multi-level MDI | ~~**CONFIRMED BUG (real, unlike #10/#11).**~~ **FIXED.** `Interp::_execute` (rs274ngc_pre.cc:290) runs a `while(MDImode && call_level)` loop for an MDI o-word sub; when a block inside the sub is a queue-buster (probe, M66, dwell, tool change) it returns `INTERP_EXECUTE_FINISH` **mid-sub** and expects the caller to drain the queue and call `execute()` again. gomc's `executeMDI` called `ExecuteString` once and routed `EXECUTE_FINISH` into its `default` (done) case — the rest of the sub after the queue-buster never ran (broke MDI probing/measurement macros like `o<probe_z> call`). `finishMDI` now, after the drain and re-synch, continues while `interp.CallLevel() > 0`: it re-points the active canon and re-runs `interp.Execute()`, drains any newly-queued motion via another `mdiDoneCmd`, and re-enters — continuing (call level still up) or completing (call level 0) — mirroring C++ re-issuing `emcTaskPlanExecute(0)` and the AUTO path. A plain MDI line / single top-level queue-buster has call level 0 and is unaffected. Mutation-proven test (`mdi_subcall_test.go`) with a scriptable two-queue-buster sub. | `commands.go` `finishMDI` |

---

## Suggested order if fixing

All of Tier 1 and Tier 2 are now done: ~~#1 orient timeout~~, ~~#3 `PLAN_FORWARD`~~,
~~#4 `(MSG,…)` → operator-display~~, ~~#5 ToolOffset~~ (sourced from canon),
~~#2 `LOAD_COMP`~~ (INI key + file parse + `set_joint_comp` wiring), and
~~#6 FO/FH/SO GUI-immediate commands~~. Tier 3 is likewise resolved:
~~#7 probe-clear~~, ~~#8 tool-unload~~, ~~#9 SET_DEBUG~~, ~~#12 homing-params~~,
and ~~#13 MDI multi-level o-word continuation~~ fixed; #10/#11 verified
not-a-gap. **No open items remain.**
</content>
