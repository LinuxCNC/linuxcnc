# GOMC Production Readiness

Goal: prototype machines can be delivered to customers. Assumptions:

- Short access paths exist to debug/fix issues in the field (observability + deployment story required).
- All hard functional safety (protection of human life/health) is handled by **external certified hardware**.
  The software must not be silently load-bearing for any safety function — see [Safety boundary](#safety-boundary).

This document tracks the per-submodule verification pipeline. Pattern proven with milltask:
AI review vs. LinuxCNC 2.9 → findings doc → fix PRs → tests → sign-off
(see `MILLTASK_REVIEW_FINDINGS.md`).

---

## Immediate next steps

1. **Runtests against gomc** — get the existing `tests/` runtests harness (79 tests, the largest
   behavioral oracle we have) running against gomc sim configs. Every test it passes is a
   regression test we don't have to write. Track pass/fail/not-applicable per test.
2. **CI gates** — extend `.github/workflows/ci.yml` (currently builds RIP + C-side runtests only;
   **no `go test` at all today**) with a gomc job:
   - `go build ./...` + `go test -race ./...` in `src/gomc`
   - `go vet` + `golangci-lint` (incl. `staticcheck`, `unused`) — baseline first, then ratchet
   - gomc runtests subset from step 1
3. **Parity check for failing/fault paths** — the capture corpus only yielded 3 trustworthy
   oracles (lines, arcs, spindle — see `tests/milltask-parity/`). Aborts, estop, dwell-drain,
   feed-per-rev, tool-change and the sync/m66/dio programs have **no usable C oracle**.
   These need written-spec tests verified against the 2.9 C source
   (reference tree: `~/source/linuxcnc-2.9` old code), not capture conversion.

**Bug FIXED this pass (production-relevant): shutdown deadlock with ≥2 HAL threads.** Any config with `BASE_PERIOD>0` (base + servo thread — most stepper configs) hung forever on shutdown: `task_wait()` re-acquired `thread_lock` on the cooperative-exit path, so the first HAL task to be deleted exited holding it and the next task's `pthread_join` blocked. Fixed in `src/gomc/internal/hallib/uspace_rtapi_lib.c` (leave `thread_lock` released on cooperative exit). This was also the root cause of the runtests full-instance flakiness (hung shutdown → leaked gomc-server → shared-REST-port collision → stalled suite). Verified: lathe/abort-g64 now shut down ~0s; 0 leaked servers.

**Runtests progress (branch `reenable-runtests`):** Category C (standalone interp)
and the HAL `test.hal` bucket are re-enabled and green — interp 71 pass / 9
Python-skip / 1 xfail; HAL 30 pass / 0 fail / 10 skip. Infra added:
`gomc-server -f` one-shot + `-f --serve` resident HAL modes, `scripts/halrun`
shim, `tests/hal-stream-driver.sh`. Remaining: ~15 `halrun`-in-`test.sh`, the
halcompile/build tests, and the ~46 full-instance (Category D) tests (need the
Python NML→`src/gmi/python` REST port).

### Component gaps surfaced by runtests re-enablement

- **G43 Hn tool-length offset wrong — PRODUCTION-RELEVANT.** A tool-table `Z0.1234` yields `tool_offset[2]=3.1344` (wrong); `G43.1` (explicit offset) is correct; `G43 H2` for a nonexistent tool gives 0. Tool-table→G43 offset lookup is broken. (tests/rs274ngc-startup, tests/tlo)
- **RANDOM_TOOLCHANGER startup tool detection wrong — PRODUCTION-RELEVANT.** With `RANDOM_TOOLCHANGER=1`, `iocontrol.tool-number` reads 0 at startup regardless of the tool in pocket 0 (expected the loaded tool number, or -1). (tests/io-startup/random/*)
- **jog/teleop + joint-mode + limit status.** After `teleop_enable(0)` (or `mode(MANUAL)` + `setp halui.mode.joint 1`), the controller stays in TELEOP: `halui.mode.is-joint` stays FALSE and a joint jog errors "Mode is TELEOP, cannot jog joint". Also `status.limit[0]` reads -1 on a tripped min hard limit where classic expects a bitmask (1). (tests/hard-limits, tests/halui/jogging)
- **tool-tracking cluster — PRODUCTION-RELEVANT.** (a) `M6` does not update the interp `#<_current_tool>` (`#5400` also stale) — after `T1 M6`, `tool_in_spindle` and (post-synch) `#<_current_pocket>` are correct, but `#<_current_tool>`/`#5400` stay 0. (b) `M61 Q<n>` sets nothing — `tool_in_spindle` and `#5400` stay 0. Breaks tool-change/tool-info introspection. (tests/tool-info/*, tests/toolchanger/m61, tests/toolchanger/*, tests/t0/*)
- **user M-codes (M1xx / `[RS274NGC]USER_M_PATH`) unsupported.** `mcode_handler: no handler for M100`. Breaks any config/test using custom M-codes. (tool tests introspecting via M100)
- **abort does not restore interp modal state — PRODUCTION-RELEVANT.** After aborting a running program, gomc leaves the modal G-code state as the program left it instead of restoring the pre-program state: motion mode reads `G64` + blend tolerances `P1/Q2` where `G61` is expected. (tests/abort/g64)
- **g5x active coordinate system inconsistent on abort.** After aborting a program running in G55, status reports gcode `G55` but g5x active index 3 / offset (G56). Active-CS index/offset desync. (tests/statbuffer-g5x-abort)
- **`motion-logger` not ported to gomc.** The motmod-replacement command-logger is still NML-based (no cmod/bin). Should become a motctl-provider cmod that logs each motion command + fakes instant completion (also a useful debug tool). Blocks tests/motion-logger/*, tests/interp/m98m99/12, tests/abort/*crazy-move.
- **`gmi.Stat` field gaps** (client, not controller): missing `cycle_time`, `max_acceleration`, `max_velocity`, `program_units`, `queued_mdi_commands`, `tool_from_pocket`; joint position is `joint_actual_position` (not `joint_position`). Some full-instance drivers simplified their status-waits around these. (tests/startup-state, tests/mdi-queue-length)
- **CORRECTION (was wrongly reported):** `(DEBUG,msg)` / OPERATOR_DISPLAY messages DO reach the `gmi` ErrorChannel as `(13, msg)` — they work with a poll-loop + settle. (tests/interp/oword-mdi-sub-update xfails for other reasons.)
- **milltask synchronized-I/O bug (M67/M62 + blended motion) — PRODUCTION-RELEVANT.** Synchronized digital/analog output (M62/M63/M67) is not applied when the M-code is followed by multiple continuously-blended moves in AUTO. Verified: M67/M68 work via MDI, in AUTO with a single move, and in AUTO loops of single moves; they FAIL with an M67 followed by ≥2 blended moves (output pin stays 0). The trajectory planner is byte-identical to upstream 2.9 (syncdio attach `tpSetupSyncedIO` + apply `tpToggleDIOs`), and 2.9 works — so the bug is in gomc milltask's (Go) canon-command streaming: `SetAoutSyncCmd`→`set_aout`→`tp->syncdio` ordering vs the move commands→`add_line`→`tpSetupSyncedIO` during read-ahead (single `tp->syncdio` slot mis-attached/overwritten). Affects any real config using M62–M68 with continuous motion (laser/plasma/spindle-sync). Blocks tests/single-step (xfail). Fix target: `src/gomc/internal/task` canon queue/sequencer sync-I/O path.


Real gomc behavior gaps found while converting HAL tests (tests skipped with
reasons; these are component bugs, not test problems):

- **`conv_float_u32` missing** — comp absent entirely (no cmod, not in registry). (limit3/constraints)
- **`logic` ignores `personality=`** — only the `.time` pin is created, not the configured and/or/in-NN pins. (loadrt.1)
- **`stepgen` module-param instance count** — `load stepgen <stepgen.0> step_type="2,2,2"` creates 1 instance, not 3; array module-param count doesn't drive instance count. (modparam.0)
- **`mux_generic` single-instance only** — rejects the classic multi-instance comma config (`mux-gen.NN`); errors `invalid character ',' in config string`. (mux, multiclick)
- **mb2hal debug output routing** — mb2hal INI-DEBUG dump goes to the server log, not a capturable stdout stream. (mb2hal.1a/2a)
- **one-shot `list`/`show` render nothing to stdout** — the `-f` executor's halparse path doesn't emit list/show output (worked around via resident server + `halcmd`).
- **INTENDED gomc model change (not a gap):** there is no `singleton` concept and no rt/userspace separation — a single cmod can provide both realtime and userspace behavior. So `option singleton`, `option rtapi_app no` (+ custom `rtapi_app_main`), and userspace `--install` (.c→`bin/`) have no direct modcompile equivalent BY DESIGN. Tests built on those concepts (rtapi-shmem, module-loading/rtapi-app-main-fails, halcompile/userspace-count-names) must be re-evaluated against the single-cmod model, not treated as blocked.
- **`conv_*` comp family** — FIXED: was unbuilt (generator existed but `CMOD_COMPS` wildcard missed the ungenerated files); wired into the build, all 11 now in `cmod/`. Enables `limit3/constraints`.
- **`modcompile` gaps vs `halcompile`**:
  1. **relative include path** — FIXED: modcompile now adds the source file's own directory to `-I`. (halcompile/relative-header enabled)
  2. **name-match enforcement** — FIXED: modcompile now rejects `component <name>;` != filename (normalizing `-`→`_`). (halcompile/names enabled)
  3. **personalities non-functional** — no `--personalities` flag, AND comps ignore `personality=` at load (only `.time` pin created). `modcompile --personalities=2` exits 0 (silently ignores unknown flag — modcompile likely should reject unknown flags). (halcompile/personalities_mod; ties to the `logic` personality gap above)
- **gomc HAL lock model differs** — `all|tune|none`, not the classic 4-level `LOAD/CONFIG/PARAMS/RUN`; `status`/lock rendering absent. (halrun-lock unfixable as-is)
- **No two-pass HAL loading (TWOPASS)** in gomc. (twopass, twopass-personality)
- **`halcmd getp` prints a verbose line** (`s32 OUT name = val`), not a bare value — output-parsing tests must `awk '{print $NF}'`.
- **hostmot2 sim / hm2 test comp** path not validated on gomc. (hm2-idrom)

---

## Review tiers

Manual review of everything is not realistic (~60k LOC of Go alone). Risk-based split:

- **Tier 1 — human review mandatory.** State machines, abort/error paths, concurrency
  ownership, and everything on the [hotspot list](#tier-1-hotspots). ~8k LOC total.
- **Tier 2 — AI review with adversarial verification.** Independent AI passes attempt to
  refute each finding; a human only adjudicates findings that survive (CONFIRMED).
  One findings doc per submodule (`<MODULE>_REVIEW_FINDINGS.md`).
- **Tier 3 — mechanical checks only.** Lint, `-race`, deadcode, and spot checks.
  Applies to generated code (`generated/gmi/*`), thin CLI wrappers, test scaffolding.

Review checklist applied in Tiers 1+2 (from the project kickoff list):

- quick fix taken instead of clean implementation
- unused code (largely automated via `staticcheck`/`unused`)
- redundant code that should be refactored
- magic numbers where enum/const should be used — is generated code (gmi) available for it?
- hand-written code where generated (gmi) code should be used
- compatibility macros or shims — none allowed
- thread-local porting hacks (no real thread/multi-instance safety)
- mix of concerns
- workarounds for missing gmi/architecture features (file as gmi feature requests instead)
- functional differences (regressions) vs. LinuxCNC 2.9
- TODO/FIXME/HACK markers (currently 18 in non-generated Go code)
- polling that should be event-driven
- artificial timeout handling
- logic and error handling validation

Known transferable risk classes from the milltask review — check explicitly in every module:

1. **Goroutine ownership** — who starts it, who stops it, shutdown ordering. 2.9 had no
   goroutines, so parity checks cannot catch this. Requires `-race` + an ownership writeup.
2. **2.9 edge parity** — error/abort paths diverge more easily than happy paths.
3. **Codegen duplication** — a bug in `gmicompile` replicates into all 39 generated packages;
   review the generator once, thoroughly, instead of its output 39 times.
4. **Fixed-but-untested** — every review fix needs a test that would have caught it.

---

## Submodule matrix

Stages: **L**int clean · **R**eview done (tier per row) · **F**indings fixed ·
**U**nit tests adequate · **RC** race clean · **FP** fault paths tested · **S**ign-off

LOC = non-test Go lines / test lines (2026-07-11 snapshot).

### Phase 0 — done

| Module | LOC | Tier | L | R | F | U | RC | FP | S |
|---|---|---|---|---|---|---|---|---|---|
| internal/task (milltask) | 12445/4839 | 1 | ☐ | ✅ | ✅ | ✅ | ✅ | ◐ | ◐ |

Milltask review closed and merged. Remaining: fault-path parity tests from
[Immediate next steps](#immediate-next-steps) §3, then sign-off.

### Phase 1 — foundation (bugs here multiply into everything else)

| Module | LOC | Tier | L | R | F | U | RC | FP | S |
|---|---|---|---|---|---|---|---|---|---|
| pkg/hal | 1174/54 | 1 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/gmicompile | 10755/2141 | 1 (emission logic) / 2 (rest) | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| generated/gmi/* boundary | n/a | 3 (spot-check vs IDL) | ☐ | ☐ | ☐ | — | ☐ | — | ☐ |
| internal/realtime | 80/43 | 1 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/gmi | 376/262 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| pkg/gomc, pkg/cmodule | 94/0 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |

### Phase 2 — field I/O (drives real iron; highest risk per untested line)

| Module | LOC | Tier | L | R | F | U | RC | FP | S |
|---|---|---|---|---|---|---|---|---|---|
| cmd/ethercat | 3867/0 | 1 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/ads | 1763/1700 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/adsbridge | 498/47 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/adsconfig | 1473/2988 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/adsmodule | 163/0 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |

### Phase 3 — supervision & startup (first thing a field tech touches)

| Module | LOC | Tier | L | R | F | U | RC | FP | S |
|---|---|---|---|---|---|---|---|---|---|
| internal/launcher | 2599/237 | 1 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/daemon | 157/0 | 1 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| cmd/gomc-server | 266/0 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/config | 86/37 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/pkgreg | 353/0 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| pkg/inifile | 606/966 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |

### Phase 4 — HAL tooling

| Module | LOC | Tier | L | R | F | U | RC | FP | S |
|---|---|---|---|---|---|---|---|---|---|
| internal/halcmd + cmd/halcmd | 3540+1932/364 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/halparse | 1769/2330 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/halfile | 343/400 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/haljson | 876/151 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/modcompile + cmd | 2909+1636/393 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/hallib | 30/0 | 3 | ☐ | ☐ | ☐ | — | ☐ | — | ☐ |

### Phase 5 — services & auxiliaries

| Module | LOC | Tier | L | R | F | U | RC | FP | S |
|---|---|---|---|---|---|---|---|---|---|
| internal/apiserver | 2174/2446 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/halrest | 659/0 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/inirest | 87/171 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/mqttbridge | 791/0 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/halscope | 939/0 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| cmd/halsampler, cmd/halstreamer | 146+174/0 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/persist_sqlite | 323/0 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/tooltable | 338/0 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/emccalib, internal/calibreg | 313+46/53 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |

### Phase 6 — UI-adjacent

| Module | LOC | Tier | L | R | F | U | RC | FP | S |
|---|---|---|---|---|---|---|---|---|---|
| internal/ngcpreview | 1302/0 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |
| internal/pyvcpmodule | 749/0 | 2 | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ | ☐ |

### Deferred / frozen

| Module | Reason |
|---|---|
| internal/classicladder | **Mid-migration/reimplementation** — review after it settles; only lint + `-race` in CI until then |
| internal/tasktest | Test scaffolding (Tier 3) |
| cmod/* (motion, tp, homing, components) | Inherited 2.9 C code — algorithm risk low; the **binding boundary** is covered in Phase 1 |
| panelui, tracking-test, linuxcnclcd, motion logger cmod host, classicladder UI, all UIs except axis, qtvcp/gladevcp | Not (fully) ported — tracked in `MISSING_FEATURES.md` |

---

## Tier 1 hotspots

Human review mandatory, in this order:

1. **pkg/hal** — the binding layer every realtime interaction crosses; 54 test lines.
   Focus: pin/signal lifecycle, type conversions, thread interaction, error propagation.
2. **gmicompile emission logic** (`internal/gmicompile/cgen`) — one wrong emission pattern
   replicates into 39 generated packages. Review generator + diff a sample of generated
   output against the IDL by hand. The parser/AST side is Tier 2.
3. **cmd/ethercat** — commands real drives, zero tests. Focus: state machine
   (INIT/PREOP/SAFEOP/OP transitions), error/timeout handling, watchdog behavior,
   behavior on slave loss/rejoin.
4. **internal/launcher + internal/daemon** — process supervision, startup/shutdown ordering,
   restart-after-crash. Focus: goroutine ownership, orphan handling, partial-startup failure.
5. **State machines & abort paths across modules** — wherever a Tier 2 AI review flags a
   state machine or abort/estop path, that section gets human eyes regardless of module tier.
6. **internal/realtime** — small, but sits on the RT boundary; verify no GC-managed
   allocation in cyclic paths.

---

## Cross-cutting work items

Not per-module; each needs an owner and a done-definition.

- [ ] **Safety boundary document** — list exactly which functions the external certified
  hardware covers (estop chain, limits, spindle stop, interlocks) and assert per module that
  the software is not load-bearing for any of them. <a name="safety-boundary"></a>
- [ ] **Concurrency policy** — per-module goroutine ownership writeup; `-race` gate in CI.
- [ ] **Panic/robustness policy** — recover-and-log vs. die-and-restart, decided once,
  applied everywhere; watchdog/supervision behavior documented.
- [ ] **Observability** ("short access path"): consistent structured logging levels across
  modules; crash reports persisted on machine; one-command diagnostic bundle
  (logs + halscope capture + config + version).
- [ ] **Config compatibility corpus** — parse every shipped config in `configs/` through the
  new stack as a test; existing customer 2.9 INI/HAL files must load identically.
- [ ] **Deployment/rollback** — version stamp visible in UI/logs, defined update procedure,
  rollback path for field prototypes.
- [ ] **RT/latency validation** — latency-histogram soak test on target hardware; verify Go GC
  cannot stall any cyclic path.
- [ ] **Fuzzing** — `go fuzz` targets for halparse, inifile, gmicompile parser.

## Test environment

- **Simulation configs** — gomc sim config set sufficient to run the runtests subset and
  fault-injection tests (abort mid-motion, estop during tool change, component crash/restart,
  GMI peer loss).
- **Automated integration tests** — runtests in CI (see Immediate next steps) + gomc-specific
  integration tests for what runtests cannot express (restart, supervision, GMI-level behavior).
- **Real-machine test plan** — only what simulation cannot cover: latency/jitter on target
  hardware, EtherCAT with real slaves, homing on physical switches, spindle/VFD behavior,
  diagnostic-bundle pull. Written checklist, executed per prototype before delivery.

## Status log

| Date | Event |
|---|---|
| 2026-07-09 | milltask review closed, merged (PR #248) |
| 2026-07-11 | This document created |
