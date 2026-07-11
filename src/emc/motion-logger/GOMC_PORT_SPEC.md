# Porting `motion-logger` to a gomc cmod

Status: **de-risked, not yet implemented.** This is a focused implementation task
(byte-exact log format + status-completion tuning need build/test iteration; it
is NOT a one-shot job). Everything needed is below.

Re-enables 6 runtests (currently xfail "motion-logger not ported"):
`tests/motion-logger/{basic,mountaindew,startup-gcode-abort}`,
`tests/interp/m98m99/12-M99-endless-main-program`,
`tests/abort/{on_abort_command-crazy-move,stop-button-crazy-move}`.
Each compares per-program logs **byte-for-byte** (all-or-nothing).

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
