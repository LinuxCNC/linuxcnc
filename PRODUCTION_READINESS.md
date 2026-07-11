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

**Runtests progress (branch `reenable-runtests`):** Category C (standalone interp)
and the HAL `test.hal` bucket are re-enabled and green — interp 71 pass / 9
Python-skip / 1 xfail; HAL 30 pass / 0 fail / 10 skip. Infra added:
`gomc-server -f` one-shot + `-f --serve` resident HAL modes, `scripts/halrun`
shim, `tests/hal-stream-driver.sh`. Remaining: ~15 `halrun`-in-`test.sh`, the
halcompile/build tests, and the ~46 full-instance (Category D) tests (need the
Python NML→`src/gmi/python` REST port).

### Component gaps surfaced by runtests re-enablement

Real gomc behavior gaps found while converting HAL tests (tests skipped with
reasons; these are component bugs, not test problems):

- **`conv_float_u32` missing** — comp absent entirely (no cmod, not in registry). (limit3/constraints)
- **`logic` ignores `personality=`** — only the `.time` pin is created, not the configured and/or/in-NN pins. (loadrt.1)
- **`stepgen` module-param instance count** — `load stepgen <stepgen.0> step_type="2,2,2"` creates 1 instance, not 3; array module-param count doesn't drive instance count. (modparam.0)
- **`mux_generic` single-instance only** — rejects the classic multi-instance comma config (`mux-gen.NN`); errors `invalid character ',' in config string`. (mux, multiclick)
- **mb2hal debug output routing** — mb2hal INI-DEBUG dump goes to the server log, not a capturable stdout stream. (mb2hal.1a/2a)
- **one-shot `list`/`show` render nothing to stdout** — the `-f` executor's halparse path doesn't emit list/show output (worked around via resident server + `halcmd`).
- **INTENDED gomc model change (not a gap):** there is no `singleton` concept and no rt/userspace separation — a single cmod can provide both realtime and userspace behavior. So `option singleton`, `option rtapi_app no` (+ custom `rtapi_app_main`), and userspace `--install` (.c→`bin/`) have no direct modcompile equivalent BY DESIGN. Tests built on those concepts (rtapi-shmem, module-loading/rtapi-app-main-fails, halcompile/userspace-count-names) must be re-evaluated against the single-cmod model, not treated as blocked.
- **`modcompile` genuine gaps vs `halcompile`** (confirmed, worth fixing):
  1. **relative include path** — modcompile compiles the generated `.c` in a temp dir and does NOT add the `.comp`'s source directory to the C include path, so a relative `#include "local.h"` in a comp fails. (halcompile/relative-header)
  2. **no name-match enforcement** — modcompile does NOT check that `component <name>;` matches the `.comp` filename; it accepts a mismatched name and emits `.c`. (halcompile/names)
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
