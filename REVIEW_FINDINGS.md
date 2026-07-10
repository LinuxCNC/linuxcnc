# Pre-merge review findings — branch `verify-milltask`

Review scope: `git diff gomc...HEAD` (81 commits, ~7,900 insertions across 123 files),
reviewed 2026-07-10 against the C++ 2.9 reference in `~/source/linuxcnc-2.9`.
Method: 7 independent finder passes (line-by-line, removed-behavior, cross-file,
reuse, simplification, efficiency, altitude), then an adversarial verification pass
per finding. **Every finding below was CONFIRMED with a traced code path** —
line numbers were re-checked during verification.

Policy for this branch: fix **all** findings, including style — this project is too
big to add noise with every fix instead of reducing it.

Status: `[ ]` open · `[x]` fixed · strike through + note if a finding is rejected
with justification.

---

## A. Correctness — merge blockers

### [x] C1 — MDI interpreter errors never abort queued motion
**Where:** `src/gomc/internal/task/commands.go:1035-1043` (executeMDI error paths),
`commands.go:963-970` (finishMDI o-word continuation error path)
**Problem:** `faultProgram()` (motion.Abort + sequencer restart to discard queued
readahead) is called only from `runProgram` (1117/1134/1160). The MDI error paths
just set state and return — canon-enqueued commands stay in the sequencer queue and
in-flight motion continues. Reachable via multi-block MDI o-word subs (added by
commit 44fe211d9c): an error at block N leaves blocks 1..N-1's motion executing.
C++ 2.9 clears `interp_list` and runs `emcTaskAbort` + `emcIoAbort` on MDI
INTERP_ERROR too (`emctaskmain.cc` ~2268-2277).
**Fix:** extract a `faultProgram` variant that is safe outside `runProgram` and call
it from *every* `interp.Execute`/`ExecuteString` error path (AUTO and both MDI
paths). The two MDI error paths must also stop diverging from each other
(continuation sets ExecError, executeMDI leaves execState untouched).

### [x] C2 — Bare S-word enables a stopped spindle and releases its brake
**Where:** `src/gomc/internal/task/canon.go:691-698` (SetSpindleSpeed)
**Problem:** `SetSpindleSpeed` unconditionally enqueues
`spindleCommand(spindle, s.spindleDir[spindle], ...)` with no `dir==0` guard →
`SpindleOnCmd{Speed:0}` → `h_spindle_on` hardcodes `state=1`
(`motctl_handlers.c:363-372`) → `command.c` sets `state=1, direction=+1, brake=0`
→ `control.c:2268-2274` drives `spindle.N.on` TRUE and releases `spindle.N.brake`.
MDI `S500` with the spindle stopped enables the drive at zero speed with the brake
off. 2.9's `emcSpindleSpeed` sends `state=0` after M5/startup, and the state-guarded
branch leaves direction/brake untouched.
**Fix:** when `spindleDir[spindle] == 0`, store the speed only (no motion command),
matching 2.9 semantics; M3/M4 then applies it.
**Implemented note:** verified against 2.9 — `SET_SPINDLE_SPEED` routes through
`emcSpindleSpeed`, which (unlike `emcSpindleOn`) does NOT force `state=1`, so for a
stopped spindle it appends a *status-only* `SPINDLE_ON s=0 speed=0` (drive stays
off, brake untouched). gomc's `spindle_on` GMI hardcodes `state=1`
(`h_spindle_on`) and cannot express "set speed, stay off", so the canon drops the
command entirely when `dir==0`. Machine behavior is identical to 2.9; the only
divergence is the absent leading status-only `SPINDLE_ON` (documented in
`TestBlendIntegration_Spindle_ViaInterpreter` / `TestCanon_SpindleOnOff`). The
manual `spindle.ngc` parity replay therefore drops that one line by design.

### [x] C3 — ESTOP command while already estopped/off permanently kills the sequencer
**Where:** `src/gomc/internal/task/commands.go:294-296` (SetState fires
`signalAbort()` unconditionally for StateEstop), `commands.go:324-329`
(`!wasOn` branch never restarts), `sequencer.go:189-195` (loop exits permanently on
`seqAbort` close)
**Problem:** ESTOP received while the machine is already estopped/off closes
`seqAbort` and kills the sequencer goroutine, but only the `wasOn` branch runs
`machineShutdown` (the sole `StartSequencer` on this path). After estop-reset +
machine-on, every canon `EnqueueCmd` fails with "sequencer aborted" and is only
*logged* (`canon.go:1159-1161`) — MDI silently produces no motion, and
`mdiDoneCmd`'s unchecked enqueue error (`commands.go:1048`) wedges `interpState` at
InterpReading. Only AutoRun/AutoStep/Abort rescue it by accident.
**Fix:** restart the sequencer on every estop entry regardless of `wasOn` (or don't
signalAbort when already down). Additionally: check the enqueue error at
`commands.go:1048`, and make canon enqueue failures surface as operator errors, not
log lines.

### [x] C4 — RigidTap velocity is clamped by the F word; tap cannot follow the spindle
**Where:** `src/gomc/internal/task/canon.go:595` (RigidTap uses `feedLimits()`),
`canon.go:1188` (`c.Vel` passed as both vel and ini_maxvel)
**Problem:** `feedLimits()` clamps vel to `s.linearFeedRate` (canon.go:522-523).
C++ RIGID_TAP passes unclamped `getStraightVelocity` as both vel and ini_maxvel
(`emccanon.cc` ~1107-1122) because tap velocity is dictated by spindle sync, not F.
In tp.c the pos-sync phase caps `target_vel` at `tc->maxvel` (tp.c:248-251, ~3062),
so with F50 active a G33.1 needing 1000 mm/min Z is capped at 50 — broken
threads/tap breakage.
**Fix:** RigidTap must use the per-axis straight-move max velocity (unclamped by
feed) for both vel and ini_maxvel.

### [x] C5 — M66 wait types "rise"/"fall" implemented as levels, not edges
**Where:** `src/gomc/internal/task/canon.go:1374-1378` (WaitInputCmd)
**Problem:** `case 1, 3: satisfied = high` / `case 2, 4: satisfied = !high`
collapses rise→high and fall→low. C++ 2.9 implements true edges: WAIT_MODE_RISE
first requires observing the input low, then high (`emctaskmain.cc:2759-2779`).
With the sensor still high from the previous cycle, `M66 P0 L1` completes on the
first poll tick and the handshake is skipped.
**Fix:** implement the two-phase edge state machine (observe opposite level first)
for wait types 1 and 2.

### [x] C6 — M66 timeout contract broken: #5399 never gets −1
**Where:** `src/gomc/internal/task/canon_getters.go:290-298` (digital),
`canon_getters.go:301` (analog has the same omission)
**Problem:** `t.inputTimeout` is set by WaitInputCmd (canon.go:1336) and exported to
stat, but no getter consumes it: `GET_EXTERNAL_DIGITAL_INPUT` returns the raw pin
value regardless. C++ returns −1 when `task.input_timeout` is set
(`emccanon.cc` ~3086), which the interp stores into #5399 — so the documented
`o100 if [#5399 LT 0]` timeout check never fires and programs take the success path
after a failed handshake.
**Fix:** return −1 from the digital (and analog) input getters when
`t.inputTimeout` is set, and clear the flag at the same points 2.9 does.

### [x] C7 — G96 CSS in inch mode is 25.4× too slow
**Where:** `src/gomc/internal/task/canon.go:663-665` (spindleCommand CSS factor)
**Problem:** the inch branch uses bare `k = 12/(2π)` while C++ computes
`12/(2π)·speed·TO_EXT_LEN(25.4)` (`emccanon.cc:1930-1931`). gomc canon/motion work
in mm and motion computes RPM as `css_factor / offset_mm` (control.c:2230-2234), so
inch-mode css_factor is 25.4× too small → G20 G96 spindle runs ~25.4× slower than
the commanded surface speed. Masked by the mm-only parity corpus and the mm-only
blend test.
**Fix:** multiply the inch-branch constant by 25.4. Add an inch-mode CSS case to
the tests (see also the corpus note in T1/S-section).

### [x] C8 — finishMDI has no ownership token: stale-completion race + ExecError clobber
**Where:** `src/gomc/internal/task/commands.go:917` (finishMDI), `:934` (canSynch
guards), `:980-987` (unconditional InterpIdle/ExecDone commit),
`sequencer.go:1032-1034` (PostWait spawns detached goroutine)
**Problem:** `mdiDoneCmd.PostWait` spawns `go t.finishMDI()` with no generation
token; the guards (`interpState != InterpIdle`, `!interpActive`) cannot distinguish
"my MDI finished" from "a newer MDI is running" or "a fault teardown ran".
Consequences (both traced): (a) MDI-1 completes → finishMDI-A pending; Abort; MDI-2
starts → stale finishMDI-A Synchs the interp against a position MDI-2 hasn't
reached, commits InterpIdle/ExecDone mid-move, dequeues queued MDIs early;
(b) after a monitor fault teardown commits `execState=ExecError`, a pending
finishMDI overwrites it with ExecDone — clean DONE in the UI right after a machine
fault (no `execState==ExecError` check anywhere in finishMDI).
**Fix:** introduce an MDI generation counter captured when the MDI is issued and
carried by `mdiDoneCmd`; finishMDI validates it under `cmdMu` and exits if stale.
Never downgrade ExecError outside the estop-reset/on recovery path.

### [x] C9 — A failed dequeued MDI strands the mdiQueue and causes out-of-order execution
**Where:** `src/gomc/internal/task/commands.go:995-998` (log-and-return on error),
`:1013` (taskCommand set, never cleared on error)
**Problem:** executeMDI's error paths enqueue no mdiDoneCmd, so no further dequeue
is ever scheduled: remaining `mdiQueue` entries are stranded and `stat.task.command`
stays showing the failed command. A later MDI passes the `interpState==InterpIdle`
check and runs immediately — *ahead* of the stranded entries, which then execute
after it via that MDI's finishMDI dequeue. Out-of-order motion.
**Fix:** on dequeued-MDI failure, either flush the remaining queue (with an operator
message) or continue the dequeue chain; clear/update taskCommand on the error path.
2.9 flushes (`mdi_execute_abort`), which is the safer parity choice.

### [x] C10 — Pause/resume with no program running wedges the task
**Where:** `src/gomc/internal/task/commands.go:576-592` (autoSignal AutoPause, no
interp-state guard), `:599-603` (AutoResume sets InterpReading unconditionally when
paused), `:735-757` (autoCommand path, identical)
**Problem:** with mode AUTO and the interpreter idle, halui pause (unguarded pin
edge, `halui.go:1156-1163`) sets InterpPaused; resume then sets InterpReading. No
runProgram producer exists, so nothing ever sets InterpIdle again — `programBusy()`
stays true and every AutoRun/MDI/ProgramOpen is rejected until Abort/E-stop. C++
saves and restores the pre-pause state via `interpResumeState`
(`emctaskmain.cc:2314-2347`).
**Fix:** gate AutoPause on a program/MDI actually being in a pausable state, and
make AutoResume restore the saved pre-pause interp state instead of assuming
Reading. Do this once in shared helpers (see D4) so both dispatch paths get it.

## B. Correctness — secondary

### [x] C11 — External-estop teardown diverges from commanded-estop teardown
**Recheck 2026-07-10 (after 684a3fbd0e): PARTIAL.** The monitor path is fixed
(stopSignals turns lube off, finishShutdown syncs the canon endpoint before
`interp.Synch`; asserted by monitor_test.go:267-274). But the `abortLocked`
omission named below is still open: `commands.go:1851` calls `interp.Synch()`
with no preceding `canon.syncEndPointFromMachine` (masked only by the re-syncs
at the next executeMDI/AutoRun). See R6.
**RESOLVED (second pass):** R6 fixed — `abortLocked` now calls
`canon.syncEndPointFromMachine()` before `interp.Synch()`.
**Where:** `src/gomc/internal/task/monitor.go:210-252` (checkEstop),
`commands.go:1739-1743` (abortLocked shares one omission)
**Problem:** checkEstop hand-rolls the shutdown sequence and has already diverged
from `machineShutdown`: (a) no `io.LubeOff` and `lubeOn` never cleared — lube stays
on after an external estop (2.9 turns it off, `emctaskmain.cc:3430-3432`);
(b) no `canon.syncEndPointFromMachine` before `interp.Synch` — interp synched to the
stale read-ahead endpoint (mostly masked by re-syncs in executeMDI/AutoRun, but
abortLocked has the same hole).
**Fix:** structural — see D2 (make the monitor call `machineShutdown`); verify lube
and endpoint behavior in a test.

### [x] C12 — IDL `@min(0)` on spindle_num rejects the −1 all-spindles broadcast this branch implements
**Where:** `src/gmi/idl/emccmd.gmi` (`spindle()`, `set_spindle_override()`);
generated check in `generated/gmi/emccmd/emccmd_cgo.go:371-372`
**Problem:** the same branch adds −1 = all-spindles to motion `command.c`
(matching 2.9 `command.c:1653`), and the task layer passes spindleNum straight
through — only the new API validation blocks it. External REST/WS clients (and any
2.9-ported UI) lose the broadcast; internal halui loops per-spindle so it survives.
Related altitude problem: spindle/joint ranges are validated at three depths with
hardcoded literals (`@max(7)`/`@max(15)` restating EMCMOT_MAX_* array bounds) while
the layer that knows the configured `numSpindles`/`numJoints` — the task — checks
nothing, so validity depends on entry path.
**Fix (decided): the −1 all-spindles broadcast is a required feature.** Change the
IDL to `@min(-1)` for the broadcast-capable commands (`spindle()`,
`set_spindle_override()`), add an authoritative task-layer check against the
configured `numSpindles`/`numJoints` (covers all transports incl. halui), and
derive the IDL bound literals from the shared EMCMOT_MAX_* constants instead of
hand-typing them. Consider simplifying halui's per-spindle loop to use the
broadcast once the path is validated end-to-end.

### [x] C13 — Constraint codegen reuses loop variable `i` at every nesting level (latent)
**Where:** `src/gomc/internal/gmicompile/cgen/constraint_emit.go:207-210`,
`client_validate.go:106-114`
**Problem:** nested constrained collections emit
`for i := range x { for i := range x[i] { ... x[i][i] ... } }` — compiles fine in
Go and silently validates the diagonal (wrong elements, possible OOB); Python/TS
clients have the same shadowing. Not triggered by any current .gmi (classicladder's
`Element` carries no constraints, so only one loop level is emitted today), but
silently wrong the moment a nested constraint appears.
**Fix:** per-depth index variables (`i0`, `i1`, …) in all three emitters; add a
doubly-nested fixture to the codegen tests.

## C. Test coverage

### [x] T1 — PlasmaC launch-guard positive path left untested
**Where:** `src/gomc/internal/launcher/launcher_test.go` (TestCheckPlasmaC_PlasmaC
deleted; only the negative-path test remains), `prelaunch.go:83-111`
**Problem:** the deleted test was the only coverage of `[PLASMAC]MODE →
ErrPlasmaC`. The stated reason (it exec'd the real `qtplasmac-plasmac2qt` GUI tool)
justifies not running the real tool, not dropping coverage — `checkPlasmaC` uses a
bare-name `exec.Command`, i.e. PATH lookup.
**Fix:** restore the positive-path test with a PATH stub (`t.Setenv("PATH", dir)`
containing a fake `qtplasmac-plasmac2qt`).

## D. Design / duplication refactors

### [x] D1 — Nine preflight* helpers hand-mirror their command bodies' guard chains
**Recheck 2026-07-10 (after 684a3fbd0e): PARTIAL.** The compound chains are
correctly unified with nothing dropped (autoRunGuardLocked/autoStepGuardLocked,
requireHomedForMDILocked, spindleOwnedByProgram — old and new conditions and
operator strings verified identical). Still duplicated: preflightSetMode vs
SetMode (:52-59 vs :560-563), preflightSetState vs setState's StateOn check
(:37-48 vs :505-513), preflightNotBusy's programBusy+message re-inlined in four
bodies (ProgramOpen :618/:631, Unhome :1578/:1592, LoadToolTable :1925/:1937,
ToolUnload :1958/:1968), and the MDI queue-full check (:164-167 vs :912-917).
The "keep them in sync" comment (:26-27) still stands. See R7.
**RESOLVED (second pass):** R7 done — shared `*Locked` guard primitives
(`canSwitchModeAutoLocked`, `canPowerOnLocked`, `rejectIfBusyLocked`/
`rejectIfBusy`, `mdiQueueFullLocked`) now back both the preflights and the
bodies (SetMode, setState, ProgramOpen/Unhome/LoadToolTable/ToolUnload, MDI);
the "keep them in sync" comment is replaced by "both call the SAME shared
guard primitives, so the two copies cannot drift".
**Where:** `src/gomc/internal/task/commands.go:17-27` (the "keep them in sync"
comment), helpers at :30, :37, :52, :64, :77, :122, :144, :152, :169;
`preflightAuto:86-102` duplicates `autoCommand:673-696` verbatim incl. operator
strings
**Problem:** every guard change must be made twice; drift is invisible (both copies
compile). Verified feasible to unify: all guard primitives read state under `t.mu`
only, and the one asymmetry (ensureMode vs canSwitchMode) is already factored.
**Fix:** one shared guard func per command, called from both the pre-cmdMu preflight
and the serialized body (e.g. a `t.serializedCommand(guard, body)` wrapper).

### [x] D2 — The machine-shutdown sequence is hand-rolled four times
**Where:** `commands.go:256` (machineShutdown — the extracted helper),
`monitor.go:210-252` (checkEstop), `monitor.go:312-333` (checkMotionEnabled),
`monitor.go:422-444` (checkMotionErrors)
**Problem:** the branch itself demonstrates the failure mode — waitRunProgramDone
and the post-join terminal commit had to be patched into each copy, and checkEstop
has already drifted (C11). Every future teardown-ordering fix must find every copy.
**Fix:** parameterize `machineShutdown` (io-abort reason, terminal execState,
whether cleanup runs under cmdMu) and have all three monitor handlers call it,
keeping only their detection/latch logic inline. Closes C11 structurally.

### [x] D3 — "Aborter owns the terminal state" enforced by convention at six sites
**Where:** `commands.go:280-287` (machineShutdown), `:1063-1070` (faultProgram),
`:1747-1757` (abortLocked), `monitor.go:246-252` (checkEstop), `:323-334`
(checkMotionEnabled), `:436-444` (checkMotionErrors)
**Problem:** each site hand-writes StartSequencer-join-then-commit with its own
ordering comment; the next teardown path will get the ordering wrong and
reintroduce the ExecError-clobber race that d600b0e448/152c9ae9d3 fixed.
**Fix:** a single `restartSequencer(terminalInterp, terminalExec)` helper (or
StartSequencer taking the terminal state) that performs join + commit atomically;
all six sites call it.

### [x] D4 — autoSignal and autoCommand duplicate the pause/resume/step bodies
**Where:** `commands.go:574` (autoSignal) vs `:661` (autoCommand); word-for-word
identical 4-line resume-while-running comment at :595-598 and :749-752
**Problem:** pause/resume semantics live in two places; a fix applied to one path
(e.g. C10's resume-state restore) silently misses the other — the serialized
fallback path is the one tests least exercise.
**Fix:** `doPause()/doResume()/doStep()` helpers holding the single copy; autoSignal
calls them lock-free, autoCommand after its mode-switch preamble. Do together with
C10.

### [x] D5 — The spindle "program owns the spindle" guard is written five times
**Where:** `commands.go:160-161` (preflightSpindle), `:1403-1404`, `:1408-1409`,
`:1413-1414` (Forward/Reverse/Off case arms), `:1653-1654` (Brake body)
**Problem:** changing what "program owns the spindle" means requires touching five
sites; missing one gives Forward and Off different acceptance rules.
**Fix:** hoist one shared guard (the shape preflightSpindle already has) before the
switch; Brake calls the same guard.

### [x] D6 — The idempotent channel-close idiom is copy-pasted 14 times
**Where:** `sequencer.go:71, 107, 132, 286, 310, 335, 396`;
`commands.go:198, 588, 609, 651, 742, 764, 843`
**Problem:** each new abort/wake path re-derives the check-then-close-under-mu
subtlety; getting it wrong is a double-close panic — the exact bug StartSequencer
already had to fix on this branch.
**Fix:** one `t.closeOnce(ch chan struct{})` helper locking t.mu around the
select/close, plus a `closeOnceLocked` variant for call sites already holding mu.

### [x] D7 — Motion-drain barriers are sprinkled op-by-op instead of declared per command
**Where:** `canon.go:628-635` (comment listing self-barriering ops), `:636`
(syncBefore), 11 call sites (:604, :624, :641, :680, :687, :697, :708, :730-733);
inconsistent second idiom: StopSpindleTurning (:701-703), Stop/Finish/StartChange/
Message (:646, :650, :723, :811) enqueue `waitForMotionSingleton` directly — the
comment even claims M5 "barriers itself"
**Problem:** every future canon op that must act "at a point" silently
blends/overlaps with prior motion unless the author remembers syncBefore; the
exception list in the comment rots (it is already wrong about M5). C++ centralizes
this: `emcTaskCheckPreconditions` maps command type → wait condition in the
executor.
**Fix:** add a precondition method to the `QueuedCmd` interface (mirroring the
existing `Wait()`); the sequencer drains motion before executing any command that
declares it. Canon then just enqueues. Enables the coalescing in E1 for free.

### [x] D8 — Validation block generated twice per API (REST + WS) with a regex-prefix scheme
**Where:** `constraint_emit.go:32` (prefix 'Re' vs 'CmdRe'), `dispatch_c.go:45/:52/
:747`, `server_go.go:483-529` (reindent splice), `constraint_emit.go:318-330`
(reindent)
**Problem:** doubled generated code, two runtime-compiled copies of every @regex, a
prefix-collision invariant that needs its own policing, and reindent() splicing at
closure depth; every validation change doubles the generated diff downstream.
**Fix:** emit one package-level `validate<Fn>(params) *apiserver.ValidationError`
per function in a single generated file; both the REST dispatch and the WS command
handler call it. Prefix scheme, duplicate regex vars, and reindent all disappear.

### [x] D9 — Client-side validation walker mirrors the server-side walker
**Where:** `client_validate.go:55-117` (clientChecks/clientValue) vs
`constraint_emit.go:146-214` (buildChecks/buildValue) — identical constraint-kind
switch, identical nullable handling, identical recursion, near-byte-identical
message strings (enum message differs; @regex divergence is deliberate and
documented)
**Problem:** a new @constraint kind or reworded message must be edited in both
walkers or clients accept what the server 400s / show different text.
**Fix:** one shared constraint-walk parameterized by emission target
(Go-server/Python/TS) and mode (fail-fast vs collect-all); keep the documented
@regex exception explicit in the shared walk.

### [x] D10 — Named-type lookups implemented three times
**Where:** `client_validate.go:121-131` (clientStruct), `:133-152`
(clientEnumValues, incl. a second copy of enumCaseList's dedup),
`constraint_emit.go:224-246` (enumFor/structFor), `check/check.go:231-241` (enumFor)
**Problem:** three linear scans over api.Types/api.Enums drift independently if the
AST gains aliasing or name rules.
**Fix:** `StructByName`/`EnumByName` (and a shared deduped enum-values helper) on
the `ast` package; all three consumers call them.

## E. Efficiency

### [x] E1 — Every motion-drain barrier pays a ≥50-60 ms floor, even on an empty queue
**Where:** `sequencer.go:507-526` (waitMotionDone: unconditional 5×10 ms settle-skip
before the first status check), `canon.go:636` (syncBefore call sites — consecutive
barriers not coalesced)
**Problem:** `S1200 M3 M8` emits three back-to-back barriers ≈180 ms of pure idle;
programs with per-line speed changes accumulate seconds-to-minutes. The C++ task
re-checks preconditions every cycle with no settle skip (empty queue clears in ~1
cycle).
**Fix:** fast-path waitMotionDone (check inpos && queueDepth==0 immediately; apply
the settle skip only when motion was actually dispatched since the last drain), and
coalesce consecutive `waitForMotionSingleton` entries at enqueue (or via D7's
precondition mechanism).

### [x] E2 — BuildStat does triple lookups and throwaway allocations per status cycle
**Where:** `stat.go:153/:160` (lookupMotionLine ×2), `:166` (lookupMotionInfo),
`:173` (pruneMotionMap — a fourth t.mu acquisition), `:79-81` vs `:167-169`
(slices allocated then overwritten whenever the executing segment is tagged — the
common running case)
**Fix:** one locked `lookupMotionInfo(ms.Id)` reused for line + tag; alias the
immutable tag slices (isolated at tag time) or populate the active-codes fields
once from whichever source applies.

### [x] E3 — M66 wait copies the full MotionStatus every 10 ms to read one bit
**Where:** `canon.go:1365` (WaitInputCmd.Execute polls `t.status.GetStatus()`)
**Problem:** a near-timeout `M66 P0 L3 Q30` performs ~3000 full-struct snapshot
copies (joints, spindles, SynchDi[64], AnalogInput[64], poses) to test one element.
**Fix:** narrow accessor (`GetSynchDi(index)` / `GetAnalogInput(index)`) following
the existing GetInpos/GetQueueDepth pattern (task.go:213-215).

### [x] E4 — setSeqInflight costs two mutex round-trips per dequeued command
**Where:** `sequencer.go:209` (true), `:358` (false), helper at `:364-368`;
consumer `waitSequencerDrain` (commands.go:1230)
**Problem:** two extra contended t.mu acquisitions per queued command (thousands per
program), competing with BuildStat/monitor/halui readers. No cross-field invariant
is maintained atomically by the writers (verified).
**Fix:** `atomic.Bool`.

### [x] E5 — finishMDI's o-word continuation pays a full drain round-trip even when nothing was queued
**Where:** `commands.go:975` (unconditional mdiDoneCmd re-enqueue per continuation)
**Problem:** an MDI o-word sub with N queue-busters completes in no less than
N×(goroutine spawn + cmdMu round-trip + E1's ≥50 ms floor) even if no motion was
queued.
**Fix:** after the continuation `Execute()`, compare the canon serial before/after
(the `startSerial`/`tagMotionRange` pattern runProgram already uses,
commands.go:1130/1140); loop finishMDI directly when nothing was enqueued.

### [x] E6 — Every (MSG,)/(DEBUG,) is logged twice, once misleadingly at read-ahead
**Where:** `canon.go:815-821` (Canon.Message logs "MSG: "+s inline, then enqueues
DisplayMsgCmd), `messages.go:45` (operatorDisplay logs again at execute time)
**Problem:** double log per message; the read-ahead line fires ahead of program
order — the very thing the queued DisplayMsgCmd was added to fix.
**Fix:** drop the inline read-ahead log; keep the single execute-time log.

## F. Test & script style

### [x] S1 — waitIdle hand-rolls a poll loop next to the generic waitForCond helper
**Where:** `concurrency_test.go:120` vs `monitor_test.go:190` (same package)
**Fix:** implement waitIdle on top of waitForCond (single polling idiom, single
place to tune timeouts/flakiness).

### [x] S2 — Blend test fixture (axis limits) defined twice
**Where:** `blend_integration_test.go:80` (newBlendCanonTask) vs
`motionlimits_test.go:15-20` (newBlendTestTask): identical axisMask 0b111,
MaxVel {40,25,8}, MaxAcc {600,400,120} — which also mirror
`tests/milltask-parity/parity3.ini`
**Fix:** share one fixture (newBlendCanonTask builds on newBlendTestTask or both use
an exported constant), so a parity-config retune can't leave one suite validating
stale limits. The hand-derived expected values depend on these numbers.

### [x] S3 — moves.sh duplicates normalize.sh's float-rounding pipeline
**Where:** `tests/milltask-parity/moves.sh:8` and `normalize.sh:14` — byte-identical
perl `sprintf("%.4f")` pipeline; %.4f is the parity tolerance knob
**Fix:** one rounding stage (moves.sh pipes through normalize.sh's rounding, or a
shared sourced helper), so the tolerance lives in one place.

### [x] S4 — Nine near-identical link_test.go files
**Where:** `internal/{adsbridge,classicladder,halcmd,halfile,haljson,halparse,
launcher}/link_test.go`, `pkg/hal/link_test.go`, `internal/task/hallink_test.go` —
each repeating the same explanation + blank import of `internal/hallib`
**Fix:** move the explanation once into a tiny documented package (e.g.
`internal/hallib/hallibtest` with a doc.go); each per-package file becomes a single
blank-import line (Go still requires one file per test binary).

---

## G. Recheck residuals — 2026-07-10, after fix commit 684a3fbd0e

Recheck method: full test suite incl. `-race` passes; 6 fix-verification agents
re-traced every finding against the current code and the 2.9 reference (generated
GMI outputs re-generated from HEAD emitters and byte-compared); 1 finder swept the
fix diff for new bugs. Result: 32 of 34 findings fully fixed, 2 partial (C11, D1
above), plus the following residuals — mostly small gaps at the edges of the new
mechanisms, found and cross-confirmed by independent agents.

**Second-pass resolution 2026-07-10:** all residuals below fixed (build + full
`-race` suite green; only the pre-existing, unrelated `halscopetest` build
failure remains). Summary:
- **R1** ProbeCmd added to the motionDispatched set (settle-skip invariant).
- **R2** new `seqFaultExit()` flushes mdiQueue+echo and bumps mdiGen at every
  sequencer error exit; monitor latch paths also bump mdiGen.
- **R3** the fragile `lastEnqueued` barrier-coalescing removed entirely — D7's
  preconditions + E1's waitMotionDone fast-path already cover it.
- **R4** M66 deadline is now checked regardless of a read-error (no `continue`
  that skips it).
- **R5** canon enqueue drop is log-only during an in-progress abort
  (errSeqAborted); operator error kept only for the unexpected not-running case.
- **R6** abortLocked syncs the canon endpoint before Synch (closes C11).
- **R7** simple preflight guards unified (see D1 above).
- **R8** C1/C8/C9 regression tests added to `review_regression_test.go`.
- **R9** stale `syncBefore()` comment fixed; vestigial `prefix` param + doc
  removed; `walkValidation` aligned with `allValidationFuncs` (walks every
  dispatched function). The "accepted for now" items (const hand-sync, motstat
  −1 sentinel, DisplayMsgCmd non-barrier) are left as documented.

### [x] R1 — ProbeCmd missing from the motionDispatched set (stale-inpos race for G38)
**Where:** `src/gomc/internal/task/sequencer.go:246` (marks only
LinearMoveCmd/CircularMoveCmd/RigidTapCmd), `canon.go:1261-1262` (ProbeCmd also
dispatches a TP segment)
**Problem:** E1's settle-skip is gated on `motionDispatched`; ProbeCmd never sets
it, so its `WaitMotion` drain can take the immediate fast path on stale
`inpos==1/queueDepth==0` before the servo cycle registers the probe move — probe
declared complete before it runs; in MDI the following Synch reads stale probe
parameters (#5061-#5070). The pre-fix unconditional 5-tick settle covered this.
One agent argues motctl's commandNumEcho ack makes queueDepth>=1 visible before
Probe() returns; even so the invariant "any TP-dispatching command sets
motionDispatched" is violated.
**Fix:** add `*ProbeCmd` to the switch at sequencer.go:246 (one line).

### [x] R2 — ExecError latch paths outside faultMDI don't flush mdiQueue (stale MDI replay)
**Where:** `src/gomc/internal/task/sequencer.go:298-345` (sequencer error exits),
`monitor.go` checkMotionErrors/checkMotionEnabled latch paths;
`commands.go:1032-1038` (finishMDI's ExecError early-return skips the dequeue)
**Problem:** only faultMDI/abortLocked/setState flush `mdiQueue`. Two confirmed
trigger paths strand queued MDIs: (a) a sequencer error exit while MDIs are
queued; (b) a monitor fault latch (ferror etc.) — finishMDI returns on the
ExecError guard, machine stays ON, queue untouched. In both cases the next
operator MDI runs immediately (nothing gates MDI on execState) and its finishMDI
then dequeues and executes the stale pre-fault entries — e.g. a forgotten
`M3 S2000` starting the spindle long after the operator abandoned it.
**Fix:** flush mdiQueue + clear taskCommand (or bump mdiGen) at every point that
latches ExecError — the sequencer error exits and the monitor latch paths —
mirroring faultMDI.

### [x] R3 — Barrier-coalescing state survives aborts and is blind to non-sequencer motion
**Where:** `src/gomc/internal/task/canon.go:1195-1198` (lastEnqueued), reset only
at the seek boundary (canon.go:307)
**Problem:** `lastEnqueued` is set before EnqueueCmd's error check and never reset
on abort/sequencer restart, and canon cannot see jog/homing motion between two
barriers. Confirmed scenario: program aborted right after M2/Stop enqueued a
barrier (flushed unexecuted) → operator jogs → MDI `M6`: StartChange's barrier
matches stale lastEnqueued and is dropped — ToolChangeCmd (no Precondition)
executes while the axis is still moving from the jog.
**Fix:** reset lastEnqueued in restartSequencer/StartSequencer (and on enqueue
error). Optionally coalesce by sequencer generation instead of object identity.

### [x] R4 — M66 poll: read errors postpone/disable the timeout
**Where:** `src/gomc/internal/task/canon.go:1446` (`continue` on GetSynchDi error
skips the deadline check at the bottom of the tick branch)
**Problem:** persistent read errors keep jumping back to the select without ever
evaluating `time.Now().After(deadline)` — the M66 Q-timeout never fires and the
sequencer blocks until abort/estop. Pre-fix code checked the deadline every tick.
**Fix:** check the deadline before (or regardless of) the read-error `continue`.

### [x] R5 — Operator-error burst on normal abort from canon enqueue failures
**Where:** `src/gomc/internal/task/canon.go:1200-1208` (C3's operator-error on
dropped commands)
**Problem:** a user Abort mid-block leaves the producer firing the rest of the
current block's canon callbacks into the closed queue — each now pops a
"Motion command dropped (sequencer stopped)" operator error for an expected,
user-initiated stop. Alarming UI noise the old log-only path avoided (C3 only
needed the message for the wedged-sequencer case).
**Fix:** suppress the operator error when the drop is caused by an in-progress
commanded abort (e.g. only emit if the sequencer is *supposed* to be running —
check abort-in-progress state), keep it for unexpected drops.

### [x] R6 — C11 residual: abortLocked still Synchs against the stale canon endpoint
**Where:** `src/gomc/internal/task/commands.go:1851`
**Fix:** call `canon.syncEndPointFromMachine` before `interp.Synch()` in
abortLocked, as finishShutdown now does.

### [x] R7 — D1 residual: simple preflights still duplicated
**Where:** `commands.go` — preflightSetMode/SetMode, preflightSetState/setState,
preflightNotBusy re-inlined in ProgramOpen/Unhome/LoadToolTable/ToolUnload,
MDI queue-full check ×2; the "keep them in sync" comment at :26-27
**Fix:** finish the D1 unification for the simple guards; delete the comment when
nothing is left to keep in sync.

### [x] R8 — Fixed-but-untested: C1, C8, C9 (C3/C4/C5 covered as of 6226a019c4)
**Problem:** the code fixes are in place but no test exercises: MDI interp error →
faultMDI aborts motion + flushes queue (C1); mdiGen staleness token (C8);
failed dequeued MDI flush / no out-of-order replay (C9). These are exactly the
regression-prone paths.
**Fix:** add the three remaining tests. (C2/C6/C7/C10 already had non-vacuous
tests.)
**Recheck 2026-07-10:** commit 6226a019c4 added
`review_regression_test.go` covering C3, C4, C5 — verified non-vacuous by
mutation: re-applying the RigidTap feed clamp, reverting M66 edges to levels,
and skipping restartSequencer in the estop-while-down branch each make the
corresponding test fail with its designed message; all pass (incl. `-race`) on
the unmutated tree.

### [x] R9 — Cosmetic/latent leftovers (batch)
- `sequencer.go:762` comment still references the deleted `syncBefore()`.
- `constraint_emit.go:24,28-31`: vestigial `prefix` parameter and stale doc
  comment describing the removed 'Re'/'CmdRe' scheme.
- Latent codegen gap (pre-existing pattern): collectRegexVars walks only
  REST-exported functions while allValidationFuncs emits validators for all
  dispatched functions — a @regex on a non-REST function would emit invalid Go
  (`ValidateRegex(path, , expr)`). Not triggered today; align the two walks.
- `emccmd.gmi` MAX_SPINDLE_INDEX/MAX_JOINT_INDEX consts are hand-synced with
  motion.h via comment, not mechanically derived (accepted for now).
- motstat `-1` error sentinel conflates with data on the analog getter and with
  a "high" digital reading (`di=-1 → high=true`); unreachable today behind three
  layers of guards, but contract-fragile — consider a separate ok/err channel if
  the GMI plumbing ever grows one. DisplayMsgCmd not barriering (2.9 barriers
  EMC_OPERATOR_DISPLAY) is a pre-existing divergence, listed for completeness.

---

## Suggested fix order

1. **C1, C2, C3** — machine-visible safety behavior (motion after error, spindle
   enable, silent no-motion).
2. **C4-C7** — machining-correctness parity (rigid tap, M66 semantics ×2, inch CSS).
   Extend the parity corpus / unit tests where the mm-only corpus masked these.
3. **C8-C10 together with D3/D4** — the finishMDI/pause-resume family shares one
   root cause (lifecycle ownership); fixing via the D-refactors avoids patching the
   same bug in multiple copies.
4. **D1, D2, D5, D6, D7** — task-package dedup (D2 closes C11; D7 enables E1's
   coalescing).
5. **C12, C13, D8-D10** — gmi/gmicompile contract + codegen cleanup.
6. **E1-E6, T1, S1-S4** — efficiency, coverage, and style.
