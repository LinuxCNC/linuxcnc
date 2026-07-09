# milltask — Goroutine / Shared-State Concurrency Analysis

**Status:** analysis complete, decision pending · **Date:** 2026-07-09 · **Scope:** `src/gomc/internal/task`

This note summarizes an investigation into the concurrency model of the Go
`milltask` coordinator: what the goroutine structure is, where the real race
risk lives, and whether it should be reduced to a single-goroutine state machine
or hardened in place. It is the design record behind that decision.

---

## 1. Context

The C++ `milltask` is **single-threaded**: one loop reads a command, dispatches
it, steps the interpreter one line, updates status, checks IO/motion, repeats.
The Go port kept the RT-in-C / coordinator-in-Go / typed-IDL-transport split
(all sound) but **fanned that single loop out across several goroutines that
share a `Task` struct behind a mutex.** That sharing is where every concurrency
bug we have found lives.

Two such bugs were already found and fixed during the test audit:

- **`ExecError` clobber** — `checkMotionErrors` set `execState = ExecError`
  *before* `AbortSequencer`, and the exiting old `sequencerLoop` set `ExecDone`,
  clobbering it. A write-write race on `execState` between the monitor and the
  sequencer. (fixed: latch `ExecError` *after* the sequencer join.)
- **Partial-state estop** — handlers set `state = StateEstop` first, then did
  abort/spindle-off/operator-error later, so a reader could observe
  `state=Estop` with the machine not yet aborted. (surfaced via a test flake.)

These two are representative of the **two distinct bug classes** below.

---

## 2. Current goroutine model

Six goroutine sources touch `Task`:

| Goroutine | Lifetime | Why it exists | Writes state machine? |
|---|---|---|---|
| **command handlers** (GMI/REST) | per request | multi-client ingress | **yes** — SetState/SetMode/Jog/MDI under `t.mu` |
| **monitor** | persistent | 10 ms poll of IO/motion | **yes** — estop/error teardown |
| **sequencer** (`sequencerLoop`) | persistent | blocks on TP queue space / motion | **yes** — sets `execState`/`interpState` on exit/fault |
| **runProgram** | per RUN/step | blocks on interp Read/Execute | **yes** — via completion + `faultProgram` |
| **mcode worker** | persistent | blocks on user M100–199 | indirectly |
| **poslog** | persistent | position sampling | mostly read-only |

They fall into **two camps**, and only one is the problem:

- **Blocking pipeline workers** (sequencer draining, runProgram's interp loop,
  mcode exec) — legitimately goroutines because they block on C calls. Fine.
- **State-machine writers** (command handlers, monitor, *and the pipeline
  workers' status callbacks*) — **this is the race surface.** "Simplify the
  goroutines" is not about count; it is about **who may write the state.**

---

## 3. Shared-state inventory

The coordinator carries ~35 state-machine fields; ~19 are written by more than
one goroutine. But most of that is not genuinely contended:

- **Temporally single-writer** (only one executor runs at a time — AUTO *xor*
  MDI): `activeGcodes/Mcodes/Settings`, `readLine`, `currentLine`, `motionMap`,
  `previewSeq`, `inputTimeout`. Statically shared, never concurrently written.
- **Benign-convergent** (monitor clears to a safe value on teardown):
  `floodOn`, `mistOn`, `stepping`.
- **Genuinely contested** — concurrent writers whose *ordering* is the bug:
  **`execState`, `interpState`, `state`, `taskCommand`** + the `seq*` channel
  lifecycle. Each of `execState`/`interpState` is written by **four** goroutines
  (CMD, SEQ, RUN, MON).

**The contested core is ~4 fields, not 19.** Every "latch after StartSequencer"
and "don't overwrite InterpPaused" ordering hack in the code is defending this
cluster.

---

## 4. The two bug classes

| Class | What it is | Fixed by *commit-then-drain*? |
|---|---|---|
| **Partial-state visibility** | a reader sees a transition half-applied (state set, side-effects/other fields not yet) | **Yes** |
| **Write-write ordering** | two goroutines both legitimately write one field; which wins is unspecified | **No** — needs single *writer* |

*Commit-then-drain* = commit the entire observable state change in one `t.mu`
hold (no I/O in between), then release the lock and do the slow side-effects
(they must not write state-machine fields back). It makes each transition
internally atomic → closes the first class. It does **nothing** for the second,
because both racers are already atomic; the hazard is their ordering. The
`ExecError` fix was a hand-placed cross-goroutine happens-before, not a
commit-then-drain fix.

**Per-transition analysis result:** every command/monitor transition is CLEAN or
restructurable-to-CLEAN under commit-then-drain, with **one** exception:

- **`executeMDI` is the only NEEDS-RESULT path** — it sets `InterpReading`,
  releases the lock, runs `interp.Synch()` + `interp.ExecuteString()`
  synchronously, then picks `InterpIdle`/`ExecWaitingForMotion` **from the return
  code**. Its final state genuinely depends on an in-flight result, so
  commit-then-drain cannot pre-commit it.

---

## 5. The blocking-surface analysis (the decisive part)

`executeMDI` is NEEDS-RESULT only because it runs the interpreter *synchronously
in a goroutine*. But the interpreter is **line-granular and read-ahead**: each
`Read`/`Execute` handles one line, queues canon calls, and returns — it never
blocks on motion. So interp execution can be *incrementalized* (one line per
tick, exactly like C++). Under that model MDI stops being synchronous and
unifies with AUTO into "step the interp"; the lone NEEDS-RESULT path
**dissolves**.

Enumerating every long-running job and asking "must it block a thread, or can it
poll per tick?":

| Job | Incrementalizable? |
|---|---|
| Interp `Read`/`Execute` per line (AUTO **and** MDI) | **yes** — queues canon, returns |
| Sequencer `waitForQueueSpace` / `waitForMotion` | **yes** — poll depth/in-position |
| `DwellCmd` (G4) | **yes** — check clock per tick |
| Jog end-wait (`waitMotionTeleop/Free`) | **yes** — already a poll |
| Tool prepare/change | **yes** — poll IO status (as C++ does) |
| `waitRunProgramDone` | **vanishes** — artifact of runProgram being a goroutine |
| **User M-codes M100–199** | **no** — shells out to an external process; genuinely unbounded |

**Result: subtract interpreter execution and essentially one job truly must
block — user M100–199 codes.** Everything else polls or is a structural artifact
that disappears. The blocking surface that made a single loop look unjustified is
nearly empty.

---

## 6. Options

- **A — Single-loop (C++ model).** One coordinator goroutine owns *all* state;
  interp stepped incrementally; one delegated worker for M-codes (+ completion
  poll); REST commands funnel through the loop's inbox. Eliminates the entire
  race class by construction — no shared mutable state, no state mutex, no
  ordering hacks. Cost: a real restructure of safety-critical code; adds a
  "waiting-for-mcode" state.
- **C-scoped — single-writer contested core.** Keep the pipeline; make just
  `execState`/`interpState`/`state`/`taskCommand` single-writer by having SEQ/RUN/
  MON *signal* transitions to one owner instead of writing directly. Kills the
  write-write class. Cost: rerouting four fields written from five goroutines
  through event plumbing while preserving precedence — fiddly and itself
  error-prone.
- **B — commit-then-drain discipline only.** Cheapest. Closes the
  partial-visibility class. Does **not** close the write-write class — the
  demonstrated bug type recurs on any new teardown path.

The NML worry (that a single owner re-adds a command queue) does **not** apply:
the serialization queue already exists as `sync.Mutex`'s wait queue; a single
loop just makes it explicit. NML's weight was the *transport* (shared-mem CMS) —
REST keeps that gone regardless.

---

## 7. Assessment & recommendation

- The **architecture is close to correct.** The RT/coordinator/IDL split is
  right; the current design is essentially the C++ single-loop with the loop body
  sprayed across five goroutines.
- **Commit-then-drain is necessary but not sufficient** — it must be applied to
  the partial-visibility spots regardless of which end-state is chosen.
- The **contested core is small (~4 fields)**, and its only real defect is
  having four uncoordinated writers.
- **A full single-loop is more feasible than it first appears** — the blocking
  surface reduces to user M-codes — and it eliminates the race *class* rather
  than defusing each instance.

**Recommended phasing (do Phase 1 regardless):**

1. **Phase 1 — commit-then-drain the known partial-visibility spots** (see
   Appendix). Cheap, contained, testable; closes real torn-read windows now.
2. **Phase 2 — decide A vs C-scoped for the contested core.** Current lean: if
   investing here at all, **A (single-loop)** is likely the better target than
   the C-scoped patch, because the blocking work that made A look expensive is
   one external-process case, and "delete the sharing" is arguably less
   error-prone than "carefully reroute five writers." Gated by rewrite appetite
   and mitigated by the (now growing) state-machine test suite.
3. **Not recommended:** stopping at B alone — it leaves the write-write class
   live.

**Open decision:** A (single-loop) vs C-scoped (single-writer core) for Phase 2.

---

## Appendix — Phase 1 targets (residual hazards found by the map)

Partial-state / ordering hazards remaining beyond the two already fixed:

1. **`checkEstop` (asserted branch) splits state across two locks.**
   `state=StateEstop` is committed early; `interpState`/`mdiQueue`/`taskCommand`/
   `stepping` only after the fast aborts — a reader can see `state=Estop` with
   `interpState=Reading`. Sibling paths commit these together; hoist them into
   the first lock hold.
2. **Error-path interp/exec skew.** `checkMotionEnabled`/`checkMotionErrors`/
   `faultProgram` commit `interpState=Idle` early but `execState=ExecError` only
   after `StartSequencer` — transiently reads as a clean stop. Inherent to the
   `ExecError`-fix ordering; only fully closed by single-writer (Phase 2).
3. **The post-join `execState` invariant is fragile.** `sequencerLoop` is a
   free-running writer of `execState=ExecDone`; every teardown must sequence its
   terminal `ExecError` write *after* the `StartSequencer` join, with no
   structural guard. This is the strongest argument for Phase 2. A minimal
   structural fix: **`sequencerLoop` must not write `execState` on its
   abort-exit** — let the aborter own the terminal state.
4. **`faultProgram` two-lock write + concurrent-teardown.** Uses separate
   `setInterpState`/`setExecState` (brief inconsistency) and can run concurrently
   with a monitor teardown — both call `StartSequencer` and write `ExecError`
   (benign values, but a double teardown / double join).
