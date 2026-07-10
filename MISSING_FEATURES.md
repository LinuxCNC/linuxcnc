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
| 7 | `TRAJ_CLEAR_PROBE_TRIPPED_FLAG` | `motctl.clear_probe_flags` declared, no caller / no command path. Low impact (motion clears on probe start). | `motctl.gmi:296` |
| 8 | `TOOL_UNLOAD` | ~~unwired~~ **FIXED.** Added REST `tool_unload` → `Task.ToolUnload` → `io.ToolUnload` (reject-while-busy, interp synch after). `tier3_batch_test.go`. | fixed |
| 9 | `SET_DEBUG` | ~~motion-only~~ **FIXED.** `SetDebug` now forwards to both `motion.SetDebug` and `io.SetDebug` and records the level to `stat.debug`. `tier3_batch_test.go`. | fixed |
| 10 | `AUX_INPUT_WAIT` (M66) | Analog-input wait semantics differ (polls analog as boolean for edge/level; C++ restricts non-immediate waits to digital). Edge case. | `canon.go:~1349` |
| 11 | `JOINT_ENABLE`/`DISABLE` | Only whole-machine amp enable/disable; no per-joint command. Rarely used. | — |
| 12 | `JOINT_SET_HOMING_PARAMS` | **BUG (not just missing):** a runtime HAL change to home/offset/sequence re-pushes `SetJointHomingParams` with `0` for `home_final_vel`/`search_vel`/`latch_vel`/`flags`/`volatile_home`, wiping the INI-configured values — homing would then use zero velocities. Needs the full per-joint homing params cached at INI load so inihal can re-push them unchanged. Not a quick wire; deferred. | `inihal.go:302-312` |
| 13 | `TASK_PLAN_EXECUTE` multi-level MDI | MDI o-word sub-calls that yield `INTERP_EXECUTE_FINISH` may not be driven to completion the way a running program's loop handles it. **Needs a test to confirm.** | `commands.go` `executeMDI` |

---

## Suggested order if fixing

All of Tier 1 and Tier 2 are now done: ~~#1 orient timeout~~, ~~#3 `PLAN_FORWARD`~~,
~~#4 `(MSG,…)` → operator-display~~, ~~#5 ToolOffset~~ (sourced from canon),
~~#2 `LOAD_COMP`~~ (INI key + file parse + `set_joint_comp` wiring), and
~~#6 FO/FH/SO GUI-immediate commands~~. Remaining: Tier 3 minor/edge items —
**#13** (MDI multi-level o-word) should get a test regardless.
</content>
