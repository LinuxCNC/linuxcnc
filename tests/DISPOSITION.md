# Runtests disposition ledger (gomc)

One central, honest record of every test in `tests/` that does **not** run-and-pass
green against gomc, and every `expected` oracle re-baselined from classic-C to gomc output.
Purpose: a parity reviewer can see *what* diverged from LinuxCNC 2.9 and *why* without git
archaeology.

**Governing rule (user, 2026-07-12):** everything that exists must be tested; the only genuine
feature removals are (1) **TCL support for HAL** and (2) **Python support in the interpreter**.
Everything else exists (the mechanism may have changed) → default **port** (adjust method) or
**xfail** (adjusted method hits a real gomc gap). Reserve **skip** for the two removals only.
Verify a replacement exists before calling anything removed. See `memory/runtests-only-two-removals`.

First run (2026-07-12, full suite): **216 run, 167 pass, 0 fail, 49 xfail, 37 skip** (+1 XPASS: lathe).

---

## 1. Re-baselined `expected` (oracle: classic-C → gomc)

### 1a. Benign — format-only (gomc `halcmd`/loader output shape; no behavioral divergence)

| test | delta |
|---|---|
| alias.0 | one pin-name per line (was space-separated); dealias entry preserved |
| hal-backslash | new `halcmd show sig` column layout (drops "(linked to)" col) |
| loadrt.1 | pin/comp lines split one-per-line |
| loadrt.2 | line-split + gomc single-instance naming `streamer.0.*`→`streamer.*` |
| mb2hal/mb2hal.1b · mb2hal.2b | new `halcmd show pin` layout (drops owner col) |
| modparam.0 | space-separated line split one-per-line |
| pyvcp | new `halcmd show pin` layout; rows re-sorted by Dir then name |
| twopass · twopass-personality | drop 3 `twopass:invoked/found` announce lines + blanks; line-split (no TWOPASS in gomc) |
| save.0 | `halcmd save` re-baseline: `# component X (loaded by cmod)` headers; gomc naming; hex→decimal (round-trip verified, e252c17ed5) |

### 1b. Semantic — canon-call / motion-stream divergence (parity note in `../PRODUCTION_READINESS.md`)

| test | delta | parity flag |
|---|---|---|
| interp/m98m99/12…/expected | +3 `ON_RESET()` (1 after SET_FEED_REFERENCE, 2 after PROGRAM_END) | ⚠ rs274 extra ON_RESET |
| interp/m98m99/12…/expected.motion-logger | real-motmod re-baseline: drop preamble, add FS/FH/SS_ENABLE + per-move SET_VEL/ACC/TERM_COND | ⚠ motion-logger stream diffs |
| motion-logger/basic/expected.g0 | prepend COORD, drop trailing SET_SPINDLESYNC, id renumber | ⚠ motion-logger stream diffs |
| motion-logger/basic/expected.g1 | prepend COORD, add per-move SET_VEL/ACC/TERM_COND, drop SET_SPINDLESYNC | ⚠ motion-logger stream diffs |
| motion-logger/basic/expected.s | prepend COORD, drop leading zero-speed SPINDLE_ON + trailing SET_SPINDLESYNC | ⚠ motion-logger stream diffs |
| motion-logger/basic/expected.builtin-startup (A) | gomc-native, replaces deleted `.in`; real-motmod startup dump (adds FS/FH/SS_ENABLE, FEED_SCALE, RAPID_SCALE) | ⚠ motion-logger stream diffs |
| motion-logger/mountaindew/expected.motion-logger | real-motmod re-baseline: drop preamble, add FS/FH/SS_ENABLE + FEED_SCALE + per-move SET_VEL/ACC/TERM_COND | ⚠ motion-logger stream diffs |

### 1c. Deleted `expected` (test removed — cross-ref §4)

halcompile/command_line_flags (f3cd5a61c8), halcompile/personalities_mod/{4count_2pers,4names_2pers} (6bc8f606ff),
halcompile/userspace-count-names/*.expected ×6 (f3cd5a61c8), motion-logger/basic/expected.builtin-startup.in +
expected.reset (047c4962a5), motion-logger/startup-gcode-abort/expected.motion-logger.in (3e3fe9c93c).

---

## 2. Skips

### 2a. Correct skip — Python interpreter removed

interp/compile (`Python.h`), interp/plug/{absolute,filename,relative} (`canterp`),
interp/{pymove,python/error,python-self,value-returned}, m70-m73/m73-flood-mist-restore.0,
remap/fail/{body-py,canon_error,epilog,prolog}, remap/{introspect,oword-pycall,predefined-named-params,remap-reentry,spindle,variable-injection}.

### 2b. Correct skip — TCL-for-HAL removed

tclsh-extensions, tcllibpath-separator.

### 2c. RECLASSIFIED to must-test (was skip; the capability still exists)

| test | capability | adjusted method | class / status |
|---|---|---|---|
| hal-link-unlink | HAL link/unlink value preservation | resident server + `tristate_float` pins + halcmd | ✅ **PASS** — ported; both hal_lib invariants verified green |
| rtapi_printf.0 | custom `rtapi_vsnprintf` %f formatter | ⏭ **skip** (precise) — custom kernel-safe `rtapi_vsnprintf` removed (gomc uspace-only → libc); no meaningful re-expression. rtapi_print/rtapi_print_msg remain but formatting is libc's now. |
| build/header-sanity | headers compile standalone | ✅ **PASS** — found + **fixed** 2 header-packaging bugs: removed internal `axis.h` from public SRCHEADERS; installed `iniparse.h` (dep of the public `inifile.h`). All 61 headers now compile standalone. |
| build/ui | external program links the control API | ✅ **PASS** — re-expressed as a minimal gmi C client compiled/linked against `libgmi` (+`-lcurl -lcjson`, which libgmi fails to declare — noted in PRODUCTION_READINESS). |
| overrun | runtests overrun-*retry* workaround | ⛔ **DROPPED** — it tested `run_without_overruns` (re-run a `test.hal` up to 10× if it prints `overrun`), a flakiness-masking retry that was dormant in gomc (nothing emits `overrun`) and is a workaround, not behavior. Removed `run_without_overruns` from `runtests.in` (a `.hal` now runs once); deleted the test. Tests must be deterministic, not retried. |
| halmodule.0 | pin type/range coercion | ✅ **PASS** — ported: haljson creates s32/u32/float pins, gmi client POSTs values over REST (`haljson.writePin` range-coerces, matching the classic oracle line-for-line). Found+fixed a real gomc bug: haljson nil-INI deref under `-f` (mirrors the pyvcp fix). Binding-object introspection (is_pin/getitem) dropped — that's the removed userspace-Python-binding API. |
| pyhal | Python HAL binding (scalar + PORT) | ✅ **PASS** — scalar s32/u32/float/bit signal-propagation via haljson pins + net links + gmi-client REST. **PORT** read/write/peek omitted → deferred+documented (HAL_PORT exists in hal_lib core but not exposed via haljson/REST). |
| halmodule.1 · hal-stream | HAL *stream* API | Flavor B — belongs with the streamer/sampler batch (item 7); capability = halstreamer/halsampler + test-io cmod | (item 7) |
| tooledit | tool-table float fidelity | ✅ **PASS** — classic drove the Tk tooledit to round-trip a `.tbl`; gomc has no Tk tooledit and no `.tbl` writer, so import the 21-tool `.tbl` via a minimal `persist_sqlite`+`tooltable` server and assert every offset/diameter/pocket/comment survives the import→sqlite→REST round-trip exactly. (INI needs `[EMC]VERSION` or gomc treats it as a convert-me config.) |
| mb2hal/mb2hal.1a · mb2hal.2a | mb2hal cmod (loads/creates pins) | runnable — INI-DEBUG dump routes to server log not stdout | xfail |

### 2d. FLAGGED — mechanism removed in a *prior* session, not one of the two official removals (need ruling)

| test | mechanism | capability now | proposed |
|---|---|---|---|
| linuxcncrsh · linuxcncrsh-tcp | `linuxcncrsh` telnet protocol | REST API; rsh-driver tests already ported to gmi | skip (dead transport conformance) |
| uspace/spawnv-root | userspace `rtapi_spawnv` | no `loadusr`/userspace helpers | skip |
| halrun-getopt-reset | `halrun` getopt-reset across `loadusr` | `gomc-server -f`; no `loadusr` | skip (removed CLI semantics) |
| module-loading/{encoder,encoder_ratio,pid,siggen,sim_encoder}/num_chan=0 | count-based zero-instance load | explicit instance names | skip (concept removed) |
| mdi-while-queuebuster-waitflag | MDI "queue-buster" + waitflag | MDI queue exists | re-examine → likely port/xfail |

---

## 3. Xfails (49)

### 3a. Legit — runnable, fail on a documented gomc bug (`../PRODUCTION_READINESS.md`)

| bug | tests |
|---|---|
| G43 Hn tool-length offset | rs274ngc-startup, tlo |
| RANDOM_TOOLCHANGER startup tool detection | io-startup/random/{no-tool-in-P0,tool-in-P0}, t0/{random-without-t0,random-with-t0}, tool-info/{random-no-startup-tool,random-with-startup-tool} |
| tool-tracking (M6 #5400, M61 Q) | t0/nonrandom, tool-info/non-random, toolchanger/m61, toolchanger/reload-tool/{non-random,random}, toolchanger/toolno-pocket-differ/{nonrandom,random} |
| abort doesn't restore modal state | abort/g64 |
| g5x active CS inconsistent on abort | statbuffer-g5x-abort |
| M67/M62 sync-I/O + blended motion | single-step |
| RS274NGC_STARTUP_CODE never executed | motion-logger/startup-gcode-abort |
| ON_ABORT_COMMAND not wired + gmi.Stat queue depth | abort/{on_abort_command,stop-button}-crazy-move |
| jog/teleop + joint-mode + limit status | hard-limits, halui/jogging |
| gmi.Stat client field gaps | startup-state, mdi-queue-length |
| rtapi_shmem_delete not exported to cmods | rtapi-shmem |
| stepgen array module-param instance count | modparam.0 |
| streaming one-row-per-cycle multiplicity | mux, multiclick — expected to flip green via the file-driven test-io cmod |
| jog overshoot from WS-lagged gmi.Stat | lathe (intermittent; XPASS this run) |
| (other) | interp/oword-mdi-sub-update |
| module-loading array-count (9/17 names, num_chan=9/17) | module-loading/{encoder,sim_encoder}/{9-names,num_chan=9}, module-loading/{pid,siggen}/{17-names,num_chan=17}, module-loading/encoder_ratio/{9-names,num_chan=9} |

### 3b. Reclassified out of xfail (→ §2d)

module-loading/*/num_chan=0 → skip (concept removed); mdi-while-queuebuster-waitflag → re-examine.

---

## 4. Vanished dirs (12) — deleted on gomc

### 4a. Deleted as "removed-by-design" — REVERSED under the rule; must re-express + test

Deleted (f3cd5a61c8 / 6bc8f606ff) as rt-userspace-split casualties, but the capability (modcompile;
module-load-failure handling) still exists — only userspace/rtapi_app *packaging* changed. Restore
against the single-cmod model; **xfail** where modcompile lacks the feature (grep confirms no
personalities/count/extralib surface in `internal/modcompile`).

| test | capability | class |
|---|---|---|
| halcompile/command_line_flags | modcompile CLI flags | port/xfail |
| halcompile/extralib | comp links an extra lib | port/xfail (modcompile gap) |
| halcompile/relative-header-user | relative `#include` (non-user variant already FIXED) | port |
| halcompile/userspace | compile a "userspace" comp → single cmod | port/re-express |
| halcompile/userspace-count-names | count/names instancing → explicit-name model | port/re-express |
| halcompile/personalities_mod | personality-gated pins (separate loads work; `--personalities` array-cycling is a modcompile gap) | xfail |
| module-loading/rtapi-app-main-fails | module-load-failure handling | port/re-express |

### 4b. To restore or disposition

| test | action |
|---|---|
| threads.0 · threads.1 | **port** — core multi-thread HAL scheduling (fast/slow period ratio, `threadtest` counter → sampler, 3500-sample capture) |
| mdi-queue/simple-queue-buster · oword-queue-buster | disposition pending — does the MDI queue-buster mechanism survive? port or skip |
| mqtt | disposition pending — port to `internal/mqttbridge` if the mechanism survives |

---

## 5. Orphaned (no `test.sh` — invisible to the runner)

| test | action |
|---|---|
| trajectory-planner/circular-arcs | **port** — profiling harness (`profile-run.sh`, no `test.sh`); port the TP circular-arc verification to gomc/REST |
