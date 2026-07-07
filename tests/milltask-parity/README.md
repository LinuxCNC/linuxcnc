# milltask parity harness (differential motion oracle)

Goal: prove the Go milltask reaches **functional equality** with the old C++
milltask by comparing the exact stream of motion commands each one sends to
the motion controller for the same G-code — move-for-move, with all velocity,
acceleration, feed, and geometry parameters.

Each milltask runs against its OWN native motion controller, in its own tree:

- **new / Go milltask** — this repo (`~/source/linuxcnc`, branch `gomc`).
- **old / C++ milltask** — `~/source/linuxcnc-2.9` (branch `verify-milltask-lcnc`).

Both trees' motion controller has the **identical** instrument in
`src/emc/motion/command.c` — a `motcmd_trace()` in `emcmotCommandHandler_locked()`,
the exact point where each native `motmod` receives a command. It is active only
when `MOTCTL_LOG` is set, logs one deterministic line per command **by opcode
name** (robust to enum renumbering between the trees), using only
`emcmot_command_t` fields common to both. So the same G-code run under each stack
yields two directly-comparable command streams; every difference is a real
behavioural difference in that milltask's canon/task layer. Off (no env) → no-op.

## Pieces

| file | role |
|------|------|
| `parity3.ini` / `parity3.hal` | 3-axis sim config with **non-uniform** per-axis limits (Z far slower than X/Y) so the canon's per-axis vel/acc blending shows up |
| `corpus/*.ngc` | test programs targeting specific regressions (see below) |
| `drive.sh` | run ONE program headlessly under the active milltask; capture its motctl log (drives the machine via `halui.*` HAL pins over the server's HTTP API) |
| `capture.sh <tag>` | run the whole corpus under the active milltask into `logs/<tag>/` |
| `normalize.sh` / `compare.sh` | round floats and diff two logs |
| `run-parity.sh [old] [new]` | diff two captured sets program-by-program |

## Usage

```bash
# --- new / Go side (this repo) ---
cd ~/source/linuxcnc/src && make ../cmod/motmod.so     # motmod with instrument
cd ~/source/linuxcnc/tests/milltask-parity && ./capture.sh new

# --- old / C++ side (2.9 tree) ---
cd ~/source/linuxcnc-2.9/src && make                   # full build w/ instrument
cd ~/source/linuxcnc-2.9/tests/milltask-parity && ./capture.sh old
cp logs/old/*.log ~/source/linuxcnc/tests/milltask-parity/logs/old/

# --- compare (in this repo) ---
cd ~/source/linuxcnc/tests/milltask-parity && ./run-parity.sh old new
```

`run-parity.sh` shows the **moves-only** diff (SET_LINE/SET_CIRCLE/PROBE/
RIGID_TAP/SPINDLE_*/SET_SPINDLESYNC) per program — the cleanest signal. For the
full semantic diff (per-move `SET_VEL`/`SET_ACC`/`SET_TERM_COND`, state machine,
override-enables) use `./compare.sh logs/old/<p>.log logs/new/<p>.log`.
A saved run is in `PARITY_RESULTS.txt`.

The two configs (`parity3.ini` here, and the 2.9 tree's classic `parity3.ini`)
carry the SAME axis limits; only the driver differs (this side drives via
`halui.*` HAL pins over the HTTP API; the 2.9 side via a Python NML `driver.py`
run as its `[DISPLAY]`). The driver is irrelevant to the comparison — only the
motion the milltask emits is logged.

### What the first run found (see PARITY_RESULTS.txt)

Against the real old C++ milltask, on the non-uniform-limit machine:

- **H1** every move: old blends per-axis (`ini_maxvel=8/25/35.35…`, `acc=120/400/565…`);
  new emits flat traj-global `ini_maxvel=40 acc=600`. Go traverses the Z axis at
  **5×** its velocity/accel limit.
- **H2** arcs: old caps small-radius/helical arcs (`ini_maxvel=18.61`, `9.45`); new `40`.
- **H4** CSS: old emits `SPINDLE_ON … css_factor=31830.99`; in new that command is absent.
- **H6** mid-run S: old re-emits `SPINDLE_ON speed=2000/1500`; new omits them.
- New never emits `SET_SPINDLESYNC` (old brackets each program with it).

## What the corpus targets

- `lines.ngc`  — per-axis velocity/acceleration blending (**H1**). On the
  non-uniform config a pure-Z feed should be capped at the Z-axis limit
  (8 mm/s, 120 mm/s²); the Go canon currently sends the traj-global
  `vel`/`ini_maxvel=40`, `acc=600` for every move.
- `arcs.ngc`   — arc centripetal velocity limiting (**H2**) + arc geometry.
- `spindle.ngc`— mid-program S change while running (**H6**) and G96 CSS
  (**H4**): watch `SPINDLE_ON ... css_factor`.
- `misc.ngc`   — dwell placement (**M5**), G95 feed-per-rev (**H5**), and the
  tool-change-position move (**M7**, appears as a `SET_LINE` if implemented).

(Regression IDs refer to the milltask review.)

## Notes

- Only the `motctl` (motion) boundary is instrumented. Tool-prepare/load and
  coolant go through the `emcio` GMI server and are not logged here; add the
  same `getenv("MOTCTL_LOG")` trace to `iocontrol` if IO parity is needed.
- The instrument is **off** unless `MOTCTL_LOG` is set — no runtime cost or
  behaviour change in normal operation.
- `logs/<tag>/<prog>.log.srvout` holds each run's server stdout/stderr for
  debugging; it is ignored by the comparison.
