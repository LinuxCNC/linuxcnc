# Porting `motion-logger` to a gomc cmod

Status: **de-risked, not yet implemented.** This is a focused implementation task
(byte-exact log format + status-completion tuning need build/test iteration; it
is NOT a one-shot job). Everything needed is below.

## Goal / architecture decision

Implement `motion-logger` as a standalone cmod that stands in for `motmod`, AND
use it to **replace the in-tree parity-trace instrumentation** so the RT motion
controller stays clean-by-design (no test hooks in production RT code).

Today there is a `motcmd_trace()` hook inside `src/emc/motion/command.c` (a
`#ifdef MILLTASK_PARITY_TRACE` block, no-op in production, called at ~`command.c:485`)
that logs the `emcmot_command_t` stream for `tests/milltask-parity`. That is
test instrumentation embedded in the production RT command path. A `motion-logger`
cmod logs the **same** command stream from the receiving (motctl-provider) side,
so it can serve BOTH the `motion-logger/*` tests AND the `milltask-parity`
comparison — letting us delete the embedded trace entirely. This matches upstream
LinuxCNC (motion-logger is a separate motmod replacement, not a hook in motmod).

Re-enables 6 runtests (currently xfail "motion-logger not ported"):
`tests/motion-logger/{basic,mountaindew,startup-gcode-abort}`,
`tests/interp/m98m99/12-M99-endless-main-program`,
`tests/abort/{on_abort_command-crazy-move,stop-button-crazy-move}`.
Each compares per-program logs **byte-for-byte** (all-or-nothing).

## Execution sequence (do NOT remove the trace first)

1. **Implement the `motion-logger` cmod** (design below) and get the 6
   `motion-logger/*` tests passing byte-for-byte.
2. **Rewire `tests/milltask-parity`** to run milltask + the `motion-logger` cmod
   (`[EMCMOT]EMCMOT=motion-logger`) instead of building motmod with
   `-DMILLTASK_PARITY_TRACE`. Confirm it reproduces the parity comparison; if the
   log format differs from what the corpus/oracle expects, align formats (and
   re-capture the C-side oracle with classic `motion-logger` on the 2.9 tree if
   needed — using the same tool on both trees is the point).
3. **Only after step 2 passes**, remove `motcmd_trace()` + the
   `#ifdef MILLTASK_PARITY_TRACE` block + its call site from `src/emc/motion/command.c`.
   motmod is then free of test instrumentation.

**Coverage guard:** before step 3, verify the cmod's log contains every field /
decode point the parity comparison actually checks. If something is missing,
EXTEND the cmod — do not re-add the RT hook.

## Design
A cmod that **replaces motmod**: it registers the **motctl** provider (to log the
motion command stream) and the **motstat** provider (to fake instant completion),
with no TP / kinematics / HAL threads — just log + status.

Mirror the structure of `src/emc/motion/motion.c` `New`/`Init`/`Destroy` and its
registration block (~`motion.c:759-781`: `motctl_api_register` /
`motstat_api_register`), stripped down.

## motctl side (logging)
Implement each `motctl_callbacks_t` fn (`motctl_set_line_fn`, `motctl_set_circle_fn`,
`set_aout`/`set_dout` (+`_synched`), `set_traj_*`, abort/enable, spindle, …) to
`log_print` in the **exact classic format** from `motion-logger.c`, e.g.
`SET_LINE x=%.6g, y=%.6g, …, id=%d, motion_type=%d, vel=%.6g, ini_maxvel=%.6g, acc=%.6g, turn=%d\n`.
The provider callbacks carry all needed args (`h_set_line(pos, vel, ini_maxvel,
acc, motion_type, id, …)` — see `src/emc/motion/motctl_handlers.c`).
**30+ command types must match byte-for-byte**, incl. the large startup sequence
(`SET_NUM_JOINTS`, `SET_VEL`, `SET_ACC`, `SETUP_ARC_BLENDS`, `SET_WORLD_HOME`, and
per-joint `SET_JOINT_BACKLASH` / `POSITION_LIMITS`×2 / `MAX_FERROR` / `MIN_FERROR`
/ `HOMING_PARAMS` / `VEL_LIMIT` / `ACC_LIMIT` / `JOINT_ACTIVATE` for all joints —
see `tests/motion-logger/basic/expected.builtin-startup`).

## motstat side (the subtle part — needs tuning)
`motstat_callbacks_t` (`get_inpos`, `get_command_num_echo`, `get_command_status`,
`get_pos_cmd`/`get_pos_fb`, `get_exec_id`, `get_queue_depth`, `get_status`) must
fake instant completion so milltask's interp advances through the whole program:
`inpos=1`, echo the command number, `pos_fb = ` last commanded pos, `queue_depth=0`,
status `DONE`. This is the part that needs iteration.

## Logfile + selection
- Logfile path is arg-driven (`out.motion-logger`), format `%.6g`.
- Config `mock-motion.hal`: classic `loadusr -W motion-logger out.motion-logger`
  becomes gomc `load motion-logger …` + `[EMCMOT]EMCMOT=motion-logger`.
- Convert the 6 tests to the gomc full-instance model (config `LIB:linuxcnc.hal` +
  `PARAMETER_FILE`; `test.sh` drives a resident gomc-server; driver `import gmi`);
  see `tests/single-step` / `tests/lathe` as the pattern.

## Build
Add `cmod/motion-logger.so` mirroring `src/emc/motion/Submakefile`'s motmod cmod
rule (simpler — no TP/kins/GMI-kins includes).

## Notes
- A gomc-native new `.c` is fine (the classic `motion-logger.c` is NML-coupled).
- All-or-nothing per test: partial format matches don't pass. Budget iteration for
  the byte-exact startup sequence + the motstat completion tuning.
