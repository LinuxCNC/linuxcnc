# `motion-logger` for gomc — interceptor design

Status: **Step 1 DONE.** The interceptor cmod is built (`cmod/motion-logger.so`)
and validated end-to-end. Of the 6 tests: 3 green via runtests
(motion-logger/basic, motion-logger/mountaindew, interp/m98m99/12), 3 xfail on
independent gomc/gmi feature gaps (NOT interceptor issues):
- motion-logger/startup-gcode-abort → `RS274NGC_STARTUP_CODE` never executed.
- abort/on_abort_command-crazy-move → `ON_ABORT_COMMAND` never wired + gmi queue gap.
- abort/stop-button-crazy-move → gmi.Stat lacks motion queue depth.
(the two abort tests turned out not to use motion-logger at all — real core_sim +
position check; their old xfail reason was stale.) Also fixed a real gomc bug found
here: main-program M99 now loops in task (`SetLoopOnMainM99`). Steps 2-3 below
(milltask-parity rewire, then delete the RT parity trace) remain, under human review.
See PRODUCTION_READINESS.md for the gap entries.

This supersedes the earlier "replacement cmod that fakes status" idea — the
interceptor is simpler and more correct.

## Concept

`motion-logger` is a thin **interceptor / proxy** cmod that sits between milltask
and the *real* motmod:

    milltask  --motctl-->  [motion-logger]  --motctl-->  motmod (real motion)
    milltask  <--motstat-- [motion-logger]  <--motstat-- motmod

It **registers the `motctl` and `motstat` GMI APIs under its own instance name**
(so milltask, configured with `[EMCMOT]EMCMOT=motion-logger`, looks it up and gets
the interceptor), and it **looks up the real motmod by instance name** (a load arg
like `mot_instance=motmod`) and forwards every call, logging the `motctl` command
stream on the way through. motmod does the real trajectory planning + provides
real status; the interceptor **just logs + forwards** — no status faking.

This is idiomatic gomc: motmod already looks up other modules by instance name
(`motion.c:598-599`: `kins_instance=` / `tp_instance=`); the registry is
instance-name keyed (`internal/apiserver/registry.go`: `Get`/`GetAll`/`Instances`).

## Why this design (vs a status-faking replacement)
- No `motstat` completion-faking to tune (the real motmod supplies real
  inpos/DONE/position) — removes the main iteration sink.
- Real motion behavior → correct for timing-dependent tests (abort mid-motion).
- Thin/mechanical: each provider callback is `log(args); forward(args)`.
- One clean seam serves BOTH the `motion-logger/*` tests AND `milltask-parity`,
  with zero test hook in production motmod.

## Implementation surface (CONFIRMED feasible — the design's key unknown is resolved)

The GMI mechanism supports the interceptor exactly:
- **Register providers under own name:** `motctl_api_register(env->api, name, cb)` /
  `motstat_api_register(env->api, name, cb)` — do this in `New()`
  (motmod: `motion.c:759-781`).
- **Get a client to a named instance:** `const motctl_callbacks_t *real =
  motctl_api_get(env->api, mot_inst_name)` — do this in `Init()` (real motmod's
  providers exist by then). This is exactly how motmod reaches kins/tp
  (`motion.c:906/913`), plus `env->api->record_consumer(...)`.
- **cmod entry:** `New(const cmod_env_t *env, const char *name, int argc,
  const char **argv, cmod_t **out)` + `Destroy` (`motion.c:668-672`); store
  `env->api` in the ctx for the `Init()` lookups; parse `mot_instance=` from argv.
- **ctx struct:** `{ FILE *log; const gomc_api_t *api; char mot_inst_name[];
  const motctl_callbacks_t *real_motctl; const motstat_callbacks_t *real_motstat; }`.
- **motctl:** ~70 callbacks. `motctl_callbacks_t` shares ONE `void *ctx`, so each
  needs its own thin wrapper (can't reuse motmod's fn pointers — the ctx differs):
  `log_print(<classic fmt>); return real->fn(real->ctx, args…)`. Log maps from the
  DECODED motctl args (not raw `emcmot_command_t`) — field by field.
- **motstat:** 10 getters (`get_status/pos_cmd/pos_fb/exec_id/queue_depth/inpos/
  command_num_echo/command_status/synch_di/analog_input`) — pure forwards, no log.
- **Build:** mirror `src/emc/motion/Submakefile`'s motmod cmod rule (smaller — needs
  motctl/motstat generated headers, no TP/kins/GMI-kins).

**Expected-log strategy (important):** the classic `expected.*` were captured at the
raw-command level of *classic* motion-logger; the interceptor logs gomc's decoded
motctl sequence — close but not guaranteed identical. So **RE-CAPTURE `expected`
from the gomc run for all 6 tests** (they become gomc self-regression tests;
`milltask-parity` remains the cross-tree C-vs-gomc check), then inspect each
capture is sensible. abort tests additionally differ by real-motion timing.

## Implementation

**Structure:** a cmod (like motmod but far smaller — no TP/kins/HAL threads). Its
`New`/`Init`/`Start`/`Destroy` mirror `src/emc/motion/motion.c`, but in Init it:
1. Registers `motctl` + `motstat` providers under its instance name
   (see `motctl_api_register` / `motstat_api_register` at `motion.c:759-781`).
2. Creates GMI **clients** to the real motmod's `motctl`/`motstat` (by
   `mot_instance=` name) — the generated client types
   (`generated/gmi/motctl/*Client`, `generated/gmi/motstat/*Client`).

**motctl provider callbacks (milltask -> interceptor -> motmod):** for each
`motctl_callbacks_t` fn (`set_line`, `set_circle`, `set_aout`/`set_dout`(+`_synched`),
`set_traj_*`, abort/enable, spindle, …): `log_print(<classic format>)` then call
the corresponding real-motmod client method. Log FORMAT must match classic
`src/emc/motion-logger/motion-logger.c` byte-for-byte, e.g.
`SET_LINE x=%.6g, y=%.6g, …, id=%d, motion_type=%d, vel=%.6g, ini_maxvel=%.6g, acc=%.6g, turn=%d\n`
(the provider callbacks carry all the args — see `src/emc/motion/motctl_handlers.c`).
Include the startup sequence tests expect (`SET_NUM_JOINTS`, `SET_VEL`, `SET_ACC`,
`SETUP_ARC_BLENDS`, `SET_WORLD_HOME`, per-joint `SET_JOINT_BACKLASH` /
`POSITION_LIMITS`×2 / `MAX_FERROR` / `MIN_FERROR` / `HOMING_PARAMS` / `VEL_LIMIT` /
`ACC_LIMIT` / `JOINT_ACTIVATE` — see `tests/motion-logger/basic/expected.builtin-startup`).

**motstat provider callbacks (milltask reads -> interceptor -> motmod):** forward
each to the real motmod's motstat client (no faking). If any field must be
massaged, do it minimally.

**Logfile:** arg-driven (`out.motion-logger`), format `%.6g`.

**Build:** add `cmod/motion-logger.so` mirroring `src/emc/motion/Submakefile`'s
motmod cmod rule (much smaller — no TP/kins/GMI-kins includes; but it DOES need
the motctl/motstat generated headers/clients).

## Test conversion
The 6 tests currently xfail "motion-logger not ported":
`tests/motion-logger/{basic,mountaindew,startup-gcode-abort}`,
`tests/interp/m98m99/12-M99-endless-main-program`,
`tests/abort/{on_abort_command-crazy-move,stop-button-crazy-move}`.

Convert configs to load **both** the real motmod (as `mot_instance`) and the
interceptor (as `[EMCMOT]EMCMOT=motion-logger`, `mot_instance=motmod`), on a real
sim kinematics config (they now need real motion), in the gomc full-instance model
(config `LIB:linuxcnc.hal` + `PARAMETER_FILE`; `test.sh` drives a resident
gomc-server; driver `import gmi`). See `tests/single-step` / `tests/lathe`.

- **Deterministic programs** (basic, mountaindew, g0/g1/reset/s, m98m99/12): the
  command stream is downstream-independent → `expected.*` should still match
  byte-for-byte. Log to a file the checkresult diffs.
- **abort/*crazy-move (timing-dependent):** with REAL motion the abort lands at a
  real-motion point that likely differs from the fake-motion capture → **re-capture
  `expected` from the gomc run** (this is now testing real abort behavior). Do NOT
  fake `expected`; capture it and sanity-check it's sensible.

Remove each xfail as it passes.

## Sequence (later steps under human review — coverage guard)
1. Implement the interceptor cmod + get the 6 tests passing. (This spec's scope.)
2. Rewire `tests/milltask-parity` to run milltask + interceptor + real motmod
   instead of `-DMILLTASK_PARITY_TRACE`; confirm it reproduces the parity data
   (re-capture the C-side oracle with classic motion-logger on the 2.9 tree if the
   format needs aligning).
3. Only after (2), delete `motcmd_trace()` + the `#ifdef MILLTASK_PARITY_TRACE`
   block + its call site from `src/emc/motion/command.c` — motmod then carries no
   test instrumentation. **Verify the interceptor log has everything the parity
   comparison checks BEFORE removing the hook; extend the interceptor if not.**
