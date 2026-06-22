# GOMC — Golang Machine Controller

> **Working title.** GOMC is an effort to push the high-value but aged concepts of
> EMC/LinuxCNC to the forefront of modern industrial automation — without carrying
> the burden of decades-old compatibility constraints.

## Motivation

LinuxCNC has proven for over 30 years that open-source CNC control is not only
viable but often superior to proprietary alternatives. Its core ideas — a
hardware abstraction layer (HAL), real-time motion control, pluggable kinematics,
and an interpreted G-code engine — remain sound and relevant.

What has aged is the surrounding infrastructure: NML message passing from the
1990s, Tcl/Tk user interfaces, a monolithic build system tied to kernel modules,
and an architecture that resists incremental modernization. Every attempt to
improve one layer pulls in the weight of all others.

GOMC takes a different path: **preserve the proven control concepts, rewrite the
plumbing.**

## Technical Approach

### Single-Process, Mixed Go/C Architecture

GOMC runs as a single process with a unified address space:

- **Go runtime** handles non-real-time tasks: REST API, G-code interpretation,
  trajectory planning, configuration, MQTT, persistence
- **C pthreads with SCHED_FIFO** handle hard real-time: servo loops, HAL cyclic
  components, EtherCAT communication
- **Direct memory sharing** between Go and C — no IPC serialization,
  sub-microsecond latency between domains, lock-free ring buffers for
  streaming data

This eliminates the complexity of LinuxCNC's multi-process NML architecture while
maintaining strict RT guarantees. If the servo loop crashes, the entire process
dies and an external watchdog triggers E-stop — clean, predictable failure
instead of ambiguous partial failures.

### Component Model (cmod + gomod)

Two component types serve different needs:

**cmod** — C shared libraries (`.so`), loaded at runtime. Can combine RT and
non-RT functions in a single component:

| Execution | Use Case | Scheduling |
|-----------|----------|-----------|
| **Cyclic** | PID, filtering, interpolation | Fixed period, SCHED_FIFO |
| **Triggered** | Homing, tool change, probing | Start signal, busy/result |
| **Threaded** | Device I/O, protocol handlers | Own thread, non-RT |

**gomod** — Go modules for complex non-RT tasks (MQTT bridge, REST endpoints,
database persistence, protocol gateways). Full access to Go's ecosystem,
goroutines, and standard library.

Both types share the same HAL signal namespace and are managed by the gomc
server process.

### EtherCAT Fieldbus

Native EtherCAT support via IGH EtherLab Master with:
- YAML-based device configuration
- Automatic PDO/SDO mapping to HAL signals
- CiA 402 drive profile state machine
- FSoE (Fail Safe over EtherCAT) support

Currently requires the `uspace` branch from
https://github.com/sittner/ethercat.

### Web-Based Tooling

Operator and engineering interfaces are moving to browser-based implementations
(Vue.js + TypeScript), served directly by the Go process. No X11 dependency, no
Tcl/Tk, no Python GUI stack required. The REST/WebSocket/GMI architecture gives
full freedom of choice for UI technology — the GMI compiler can generate client
bindings for Python, TypeScript, and Go.

**Already migrated:**
- HAL signal scope (oscilloscope-style waveform viewer)
- HAL signal browser
- Tool table editor
- ClassicLadder logic viewer
- Machine calibration

**Machine UI:**
- Modified Axis UI (communicating via REST/WebSocket) — available now
- Other existing LinuxCNC UIs will be migrated
- New purpose-built web UIs to follow

### Generic Machine Interface (GMI)

A typed, versioned API layer between the control engine and user-facing tools.
Defined via IDL, with generated client bindings for Go, Python, and TypeScript.
Replaces LinuxCNC's untyped NML stat/command channels, moving from asynchronous
message passing to synchronous function calls with clear request/response
semantics.

## What This Is Not

- **Not a fork that tries to stay compatible.** INI files, NML tools, and the
  old Python/Tcl interfaces are deliberately not carried forward.
- **Not a competing project.** GOMC builds directly on LinuxCNC's proven
  motion control, trajectory planning, and G-code interpretation. We acknowledge
  and respect the decades of engineering in that codebase.
- **Not starting from scratch.** The rs274ngc interpreter, trajectory planner,
  kinematics modules, and core motion controller are inherited and evolved.

## Target Audience

- **LinuxCNC contributors** frustrated by architectural constraints that prevent
  meaningful modernization
- **Machine builders** looking for an open, flexible alternative to proprietary
  PLCs without vendor lock-in
- **Automation engineers** who need modern APIs, web interfaces, and EtherCAT
  support without the overhead of enterprise licensing

## Building

```bash
git clone -b gomc https://github.com/sittner/linuxcnc.git linuxcnc
cd linuxcnc
git submodule update --init

# Install build dependencies (Debian/Ubuntu)
cd debian && ./configure && cd ..
sudo apt-get build-dep .

# Build
cd src
./autogen.sh
./configure
make -j$(nproc)
```

The build process is essentially the same as classic LinuxCNC (autoconf + make).
The EtherCAT master is built automatically from a git submodule.
A future goal is migration to CMake.

## Running the Simulator

**Terminal 1 — start the server:**

```bash
cd linuxcnc
./scripts/linuxcnc configs/sim/axis/axis_mm.ini
```

**Terminal 2 — start a UI client (as many as you like):**

```bash
cd linuxcnc
. scripts/rip-environment
axis
```

See [README_LINUXCNC.md](README_LINUXCNC.md) for full build options and
additional configuration.

## Multi-Instance Demo

A single gomc-server process can host multiple independent CNC instances.
Multiple UI clients can connect to the same or different instances
simultaneously — state updates are synchronized in real time.

**Terminal 1 — start the server with multi-instance config:**

```bash
cd linuxcnc
./scripts/linuxcnc configs/sim/axis/multiinst/multiinst.ini
```

**Terminal 2 — Axis client connected to mill1:**

```bash
cd linuxcnc
. scripts/rip-environment
GMC_INSTANCE=mill1 axis
```

**Terminal 3 — second Axis client, also connected to mill1:**

```bash
cd linuxcnc
. scripts/rip-environment
GMC_INSTANCE=mill1 axis
```

**Terminal 4 — Axis client connected to mill2 (independent instance):**

```bash
cd linuxcnc
. scripts/rip-environment
GMC_INSTANCE=mill2 axis
```

**Things to try:**

1. On client 1 (mill1): E-stop off → Machine on → Home all axes.
   Observe client 2 (also mill1) mirrors the state changes in real time.
2. On client 1: File → Open → `nc_files/3dtest.ngc`.
   Observe the backplot preview appears on client 2 as well.
3. On client 1: Run the program.
   Observe client 2 shows the live execution progress.
4. On client 3 (mill2): Independently home, load `nc_files/axis.ngc`, and run.
   Mill2 operates completely independently from mill1.

## License

This project contains code under multiple open source licenses:

- **GPL-2.0** — inherited LinuxCNC code (motion, HAL, interpreter, drivers, etc.)
  and new `src/gomc` components by Sascha Ittner. See [COPYING](COPYING).
- **LGPL-2.0** — HAL library (`hal_lib.c`, `gomc_hal.h`), originally by
  John Kasunich. See [COPYING.more](COPYING.more).
- **LGPL-2.1** — RTAPI interface (`gomc_rtapi.h`) originally by John Kasunich
  and Paul Corner; GMI library headers (`src/gmi/lib/`) by Sascha Ittner.
- **LGPL-2.1+** — Classic Ladder RT engine, originally by Marc Le Douarain.

See individual source file headers for per-file license and copyright details.

## Status

Active development. Not yet suitable for production use.

The scope of this architectural migration — touching hundreds of files across
real-time control, build system, HAL drivers, and UI — would not have been
feasible for a small team without massive AI-assisted development (GitHub
Copilot). This enabled rapid prototyping and refactoring at a scale that would
otherwise require years of manual effort.
