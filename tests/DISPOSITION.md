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

### 2a. Correct skip — Python interpreter removed (all CONFIRMED, #3)

interp/compile (`Python.h`), interp/plug/{absolute,filename,relative} (`canterp`),
interp/{pymove,python/error,python-self},
remap/fail/{body-py,canon_error}, remap/{predefined-named-params,remap-reentry}.
(remap/spindle, remap/fail/{prolog,epilog}, remap/oword-pycall, remap/introspect, remap/variable-injection, m70-m73/m73-flood-mist-restore.0 moved out — re-expressed; see §2e.)

**#3 (2026-07-13): each verified against the exact removed mechanism, not blanket "python gone"; per-test reason lives in each `skip` file. Summary:**

| test | exact removed mechanism it needs | why not re-expressible |
|---|---|---|
| interp/compile | external C++ program embedding the interp: `#include <Python.h>` + `$PYTHON_LIBS`, Python types in the public interp API (`struct inttab`) | Python.h/$PYTHON_LIBS not in the build; rs274ngc.hh no longer carries Python types. (Linking an external client is covered by build/ui via libgmi.) |
| interp/plug/{absolute,filename,relative} | `rs274 -p canterp.so` — the Python "canonical interpreter" plugin, resolved by absolute / bare-filename / relative path | canterp.so (a Python C-extension) isn't built; no non-Python interpreter plugin exists to exercise `-p`. |
| interp/python/error | `python3 -mcanon` + `import gcode` — drives the interp through the Python `gcode` binding to catch an arc error | the Python `gcode`/`emc` binding is removed (preview = Go ngcpreview/REST). Arc-radius-mismatch detection itself stays covered by standalone-interp arc tests. |
| interp/python-self | Python `self.param1 = x` on the interp object + `interpreter.this` alias, persisting across `;py,`/o-word calls | interp_ext handlers are stateless C callbacks; no interpreter-bound Python `self`/`this`. |
| interp/pymove | `emccanon.STRAIGHT_FEED/STRAIGHT_TRAVERSE` — direct canon **motion** emission from a handler | interp_ctx exposes only canon_enqueue_set_spindle_speed/_feed_rate + tool calls, no motion emit; motion from a handler is via an `ngc=` body. |
| remap/fail/body-py | `REMAP=M400 py=interp_error` — a pure Python remap **body** returning INTERP_ERROR | gomc rejects `py=`/`python=` at parse. Handler-fails-conveys-error is covered by remap/fail/{prolog,epilog}; only the py= body form is gone. |
| remap/fail/canon_error | prolog calls `emccanon.CANON_ERROR("…%s…")` (literal-%s safety of the canon error path) | no `canon_error` accessor on interp_ctx (C handlers use set_error, a different path). Prolog-fail path covered by remap/fail/prolog. |
| remap/predefined-named-params | Python-registered **predefined named params** (`_pi`, `_py_motion_mode`, read-only #<_name>) | interp_ext has no register-named-param; interp_ctx get/set only touch existing params. |
| remap/remap-reentry | Python **generator** handler bodies doing `self.execute("G0 …")` + repeated `yield INTERP_EXECUTE_FINISH` | interp_ctx has no execute-string accessor; interp_ext's single EXECUTE_FINISH can't reproduce the multi-yield coroutine + self.execute pattern; py= rejected at parse anyway. |

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
| hal-stream | stream cfg validation (Python `hal.stream(...,"xx")` must reject) | ✅ **PASS** — re-expressed as the `filestream` cmod refusing `sample_cfg=xx` (`hal_stream_parse_cfg` rejects any non-f/b/u/s type); the load fails. |
| halmodule.1 | stream ring overrun/underrun/sampleno (Python `hal.stream`) | ✅ **PASS** — re-expressed via `filestream` HAL pins: replay 9 bfsu samples through a depth-10 ring clocked 12 ticks → round-trips the 9, `sample-num`=12, `underruns`=3 (empty clocks), `overruns`=0. The Python write-raises-on-full / read-returns-None API is the removed binding; the ring counters are the gomc equivalent. |
| tooledit | tool-table float fidelity | ✅ **PASS** — classic drove the Tk tooledit to round-trip a `.tbl`; gomc has no Tk tooledit and no `.tbl` writer, so import the 21-tool `.tbl` via a minimal `persist_sqlite`+`tooltable` server and assert every offset/diameter/pocket/comment survives the import→sqlite→REST round-trip exactly. (INI needs `[EMC]VERSION` or gomc treats it as a convert-me config.) |
| mb2hal/mb2hal.1a · mb2hal.2a | mb2hal cmod (loads/creates pins) | runnable — INI-DEBUG dump routes to server log not stdout | xfail |

**Item 7 (HAL streaming) — DONE.** The WS sampler/streamer decision was kept for live/GUI use (panelui/qtvcp/gladevcp later) but a new **`filestream`** cmod (`src/hal/components/filestream.c`, file-backed replay+capture, deterministic one-line-per-thread-cycle, byte-identical to halsampler) now backs the tests. The 26 streaming tests migrated off the WS driver to `filestream` + `tests/filestream-driver.sh` (`fs_run`), expected files unchanged; `tests/ws-stream` is the new dedicated WS-path coverage; `hal-stream`/`halmodule.1` re-enabled (above); `multiclick` **xfail→pass** (filestream's one-per-cycle pacing fixed its timing). Only `mux` stays xfail — a documented benign classic-streamer-FIFO-overflow artifact in its golden (100 real rows match; 5 held-last rows differ). The 16 resident-server-only tests still use `hal-stream-driver.sh`'s `hal_start_server`.

### 2d. Ruled (user, 2026-07-12)

| test | mechanism | disposition |
|---|---|---|
| linuxcncrsh | telnet remote-shell + bulk-MDI g-code → canon output | ✅ **PASS** — migrated to REST: the rsh command stream (hello/enable/mode/estop/machine/mdi + 201 M100 MDI calls) is translated to gmi by `rsh2gmi.py`; M100 captured by `mcode_coord_log format=raw`. Output matches the classic `expected-gcode-output` exactly. |
| linuxcncrsh-tcp | same test, forced onto NML-over-TCP (`tcp.nml`) | ⛔ **REMOVED** — NML gone; REST has one transport, nothing distinct left |
| uspace/spawnv-root | userspace `rtapi_spawnv` (build+spawn a `.c` userspace binary as root) | ⛔ **REMOVED** — no userspace binaries / `rtapi_spawnv` |
| halrun-getopt-reset | `halrun` getopt-reset across repeated `loadusr` | ⛔ **REMOVED** — no `loadusr`; `halrun` is a shim |
| module-loading/{encoder,encoder_ratio,pid,siggen,sim_encoder}/num_chan=0 | `num_chan=0` = load with the *default* channel count (1 instance) | ⛔ **REMOVED** — explicit-names-only; the 1-instance case is already covered by the `count=1` test |

**Pending (Python discussion):** `mdi-while-queuebuster-waitflag` — its `M400` queue-buster is a **Python remap** (§2a-blocked). Either skip (Python-blocked) or re-express the MDI-vs-queuebuster race with a non-Python queue-buster.

### 2e. Re-expressed against the C interp_ext / mcode_handler mechanism (#2, was §2a Python skip)

The gomc replacement for embedded-Python interpreter extensions is the C interp_ext API
(register_oword / register_remap_prolog / register_remap_epilog) plus mcode_handler — all
now wired + tested (tests/interp-ext, tests/mcode-handler). Python remap/O-word tests whose
*capability* still exists are being re-expressed against it rather than skipped.

| test | classic mechanism | re-expression | status |
|---|---|---|---|
| interp/value-returned | Python O-word sub returning a value | NGC-only (endsub/return observable via `g0 x#<_value>` canon moves) | ✅ **PASS** |
| remap/spindle | `M500 py=m500` reads `self.speed[]`/`self.active_spindle` | `REMAP=M500 prolog=m500_prolog` C cmod (`test_spindle_remap.so`) reads per-spindle speed via interp_ctx `get_speed()`; full-instance MDI run, checkresult greps the prolog's logged speeds ([0,0,0]→[1000,0,0]→[1000,2000,0]) | ✅ **PASS** |
| remap/fail/prolog | Python prolog returns INTERP_ERROR; must abort + convey error, NGC body not run | `REMAP=M400 prolog=failingprolog` C cmod (`test_remap_fail.so`) `set_error()`+INTERP_EXT_ERROR; checkresult confirms prolog failed, error text conveyed, body (`o<mark_body>`) NOT run | ✅ **PASS** (found+fixed the pycall message-clobber bug below) |
| remap/fail/epilog | Python epilog returns INTERP_ERROR after the NGC body ran | `REMAP=M400 ngc=mustbecalled epilog=failingepilog` (same cmod); checkresult confirms body ran, epilog failed, error conveyed | ✅ **PASS** |
| remap/oword-pycall | Python O-word subs (o<square>, o<multiply>) w/ fixed+variable args and #<_value> return | C interp_ext O-words (cmod `test_oword_math.so`, register_oword); MDI feeds a prior call's #<_value> back as an arg to prove the return round-tripped. checkresult greps args+result: square(5)=25, multiply(25,2)=50, multiply(5,6,7)=210 | ✅ **PASS** |
| remap/introspect | Python O-word reads args + live interp state (feed/speed/named/INI/global params) | C interp_ext O-word (cmod `test_introspect.so`) via interp_ctx get_feed_rate/get_speed/get_param; checkresult greps args [1,2,3,3.14159], feed=200, rpm=3000, global=47.11, ini=3.14159. Python-binding-only bits (block param arrays, sub_context iteration, params.locals()/globals(), self.remaps) dropped — removed embedded-Python API | ✅ **PASS** |
| remap/variable-injection | Python prolog injects a var, NGC bumps it, epilog retrieves it; per-remap scoping | C interp_ext prolog/epilog (cmod `test_var_inject.so`) via interp_ctx set_param/get_param; M405/406/407 run singly + all-in-one-block. checkresult confirms each prolog injected #<fooNNN>=42, NGC bumped to 43, epilog retrieved 43, and no abort (sibling-remap vars not visible — local scoping intact) | ✅ **PASS** |
| m70-m73/m73-flood-mist-restore.0 | M73 auto-restore of M7/M8; verified with `;py,assert this.params[...]` | NGC-only (standalone rs274, like sibling m73autorestore.0): drop the py-asserts, surface restored state via `(debug, _mist=#<_mist> _flood=#<_flood>)`; MIST_ON/FLOOD_ON reappear in the canon trace after the sub returns | ✅ **PASS** |

**gomc bug fixed here (interp error conveyance):** a C interp_ext prolog/epilog/O-word handler that called `ctx->set_error()` and
returned INTERP_EXT_ERROR had its saved message clobbered with a generic "pycall(...) failed" / "handler not registered".
Root cause: gomc's `Interp::pycall` returned the handler's mapped status directly, tripping the caller's
`CHKS(status==INTERP_ERROR,...)` (and the O-word caller's not-registered ERS) *before* `handler_returned()` could convey it.
Classic Python left pycall's own status INTERP_OK and surfaced the handler's return via `handler_returned`. Fixed: pycall now
detects genuine not-registered via `ext_has_*` (clear error), and otherwise returns INTERP_OK with the handler's status in
`last_status`; the O-word caller conveys it through `handler_returned` (interp_python.cc, interp_o_word.cc).

**#2 COMPLETE for the re-expressible set.** Remaining §2a Python skips are genuine removals (no C interp_ext / interp_ctx
equivalent): remap/predefined-named-params (Python-computed predefined named params), remap/remap-reentry (python
`yield INTERP_EXECUTE_FINISH` generator body), interp/pymove (direct `emccanon` motion emission from a handler),
remap/fail/{body-py,canon_error}, interp/{compile,python-self,python/error}, interp/plug/* (canterp).

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

### 3b. Reclassified out of xfail (→ §2d, ruled)

module-loading/*/num_chan=0 → **removed** (default-channel-count concept gone, covered by `count=1`);
mdi-while-queuebuster-waitflag → pending Python discussion.

---

## 4. Vanished dirs (12) — deleted on gomc

### 4a. Deleted — INTENTIONALLY removed (user ruling): rt/userspace-split model is gone

Deleted (f3cd5a61c8 / 6bc8f606ff). These test the classic `halcompile` **rt/userspace split** —
compiling a component as a separate userspace program, `rtapi_app` main, `RTAPI_MP_ARRAY_INT`
personality arrays, `--personalities` count-cycling, userspace `count=`/`names=` instancing. gomc has
**no** such split (one cmod does both realtime and userspace), so the features these address are gone
or handled differently in the single-cmod model. **Correctly removed — do not restore.**

| test | addressed a feature that is now… |
|---|---|
| halcompile/command_line_flags | halcompile CLI-flag surface of the removed model |
| halcompile/extralib | userspace-comp extra-lib linking — no rt/userspace split |
| halcompile/relative-header-user | the *userspace* variant of relative-header (the RT/single-cmod variant is covered + green) |
| halcompile/userspace | "compile a userspace comp" — no separate userspace build |
| halcompile/userspace-count-names | userspace `count=`/`names=` instancing — replaced by explicit instance names |
| halcompile/personalities_mod | personality arrays + `--personalities` cycling — no equivalent by design |

### 4b. To restore or disposition

| test | action |
|---|---|
| threads.0 · threads.1 | **port** — core multi-thread HAL scheduling (fast/slow period ratio, `threadtest` counter → sampler, 3500-sample capture) |
| module-loading/rtapi-app-main-fails | ✅ **PASS** — ported to the cmod/`load` model: a comp fails its init via a failing `EXTRA_SETUP` (`-ERANGE`), and `load` correctly fails (`factory returned error code`). Classic used `option rtapi_app no` + custom `rtapi_app_main`. Note: gomc flattens the errno to `-1` (documented gap). |
| mdi-queue/simple-queue-buster · oword-queue-buster | disposition pending — does the MDI queue-buster mechanism survive? port or skip |
| mqtt | disposition pending — port to `internal/mqttbridge` if the mechanism survives |

---

## 5. Orphaned (no `test.sh` — invisible to the runner)

| test | action |
|---|---|
| trajectory-planner/circular-arcs | **port** — profiling harness (`profile-run.sh`, no `test.sh`); port the TP circular-arc verification to gomc/REST |
