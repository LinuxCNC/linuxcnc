// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
// Package task implements the milltask gomod — the CNC task controller
// that coordinates motion, I/O, and the G-code interpreter.
//
// All state lives in the Task struct (no globals), making the module
// inherently multi-instance capable.
package task

import (
	"fmt"
	"log/slog"
	"sync"
	"time"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/emcerror"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/motctl"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/motstat"
)

// TaskState represents the machine state (estop, on, etc.)
type TaskState int32

const (
	StateEstop      TaskState = 1
	StateEstopReset TaskState = 2
	StateOff        TaskState = 3
	StateOn         TaskState = 4
)

func (s TaskState) String() string {
	switch s {
	case StateEstop:
		return "ESTOP"
	case StateEstopReset:
		return "ESTOP_RESET"
	case StateOff:
		return "OFF"
	case StateOn:
		return "ON"
	default:
		return fmt.Sprintf("TaskState(%d)", int(s))
	}
}

// TaskMode represents the current operating mode.
type TaskMode int32

const (
	ModeManual TaskMode = 1
	ModeMDI    TaskMode = 3
	ModeAuto   TaskMode = 2
)

func (m TaskMode) String() string {
	switch m {
	case ModeManual:
		return "MANUAL"
	case ModeMDI:
		return "MDI"
	case ModeAuto:
		return "AUTO"
	default:
		return fmt.Sprintf("TaskMode(%d)", int(m))
	}
}

// InterpState represents the interpreter execution state.
type InterpState int32

const (
	InterpIdle    InterpState = 1
	InterpReading InterpState = 2
	InterpPaused  InterpState = 3
	InterpWaiting InterpState = 4
)

// ExecState represents the task execution state.
// Values must match the emcstat GMI enum (emcstat.gmi ExecState).
type ExecState int32

const (
	ExecError                     ExecState = 1
	ExecDone                      ExecState = 2
	ExecWaitingForMotion          ExecState = 3
	ExecWaitingForMotionQueue     ExecState = 4
	ExecWaitingForIO              ExecState = 5
	ExecWaitingForMotionAndIO     ExecState = 7
	ExecWaitingForDelay           ExecState = 8
	ExecWaitingForSystemCmd       ExecState = 9
	ExecWaitingForSpindleOriented ExecState = 10
)

// jogTimeout is how long a continuous jog stays active without being refreshed.
// Clients must re-send the jog command within this interval to keep it alive.
const jogTimeout = 2 * time.Second

// activeJog tracks a single active continuous jog for the watchdog.
type activeJog struct {
	active   bool
	isTeleop int32
	fromHAL  bool // HAL-pin-driven jogs are self-managing, skip watchdog
	lastSeen time.Time
}

// MotionController is the interface to motmod (motctl GMI API).
// Methods match the motctl.gmi function names.
type MotionController interface {
	// Motion queue
	SetLine(pos Pose, vel, iniMaxvel, acc float64, motionType int32, id int32, feedMmPerMin float64, indexerJnum int32) error
	SetCircle(pos Pose, center, normal Cartesian, turn int32, vel, iniMaxvel, acc float64, motionType int32, id int32, feedMmPerMin float64) error
	Probe(pos Pose, vel, iniMaxvel, acc float64, motionType int32, probeType uint8, id int32, feedMmPerMin float64) error
	RigidTap(pos Pose, vel, iniMaxvel, acc float64, scale float64, id int32, feedMmPerMin float64) error

	// Motion control
	Abort() error
	Pause() error
	Resume() error
	Step(id int32) error
	Reverse() error
	Forward() error
	SetFree() error
	SetCoord() error
	SetTeleop() error
	Enable() error
	Disable() error

	// Jogging
	JogCont(num int32, vel float64, isTeleop int32) error
	JogIncr(num int32, vel, incr float64, isTeleop int32) error
	JogAbs(num int32, vel, pos float64, isTeleop int32) error
	JogAbort(num int32, isTeleop int32) error

	// Spindle
	SpindleOn(spindle int32, speed float64, css_factor float64, css_max float64, wait int32) error
	SpindleOff(spindle int32) error
	SpindleOrient(spindle int32, orientation float64, mode int32) error
	SpindleIncrease(spindle int32) error
	SpindleDecrease(spindle int32) error
	SpindleBrakeEngage(spindle int32) error
	SpindleBrakeRelease(spindle int32) error
	SetSpindleScale(spindle int32, scale float64) error

	// Overrides
	SetFeedScale(scale float64) error
	SetRapidScale(scale float64) error
	SetMaxFeedOverride(max float64) error
	FeedScaleEnable(enable int32) error
	SpindleScaleEnable(spindle int32, enable int32) error
	AdaptiveFeedEnable(enable int32) error
	FeedHoldEnable(enable int32) error

	// Limits and homing
	OverrideLimits(joint int32) error
	JointHome(joint int32) error
	JointUnhome(joint int32) error

	// Parameters
	SetVel(vel float64) error
	SetVelLimit(vel float64) error
	SetAcc(acc float64) error
	SetTermCond(cond int32, tolerance float64) error
	SetOffset(offset Pose) error
	SetDebug(level int32) error

	// I/O
	SetDout(index, value int32) error
	SetDoutSynched(index, startValue, endValue int32) error
	SetAout(index int32, value float64) error
	SetAoutSynched(index int32, startValue, endValue float64) error

	// Spindle sync
	SetSpindlesync(sync float64, motionType int32) error
}

// IOController is the interface to iocontrol (emcio GMI API).
type IOController interface {
	CoolantFloodOn() error
	CoolantFloodOff() error
	CoolantMistOn() error
	CoolantMistOff() error
	LubeOn() error
	LubeOff() error
	ToolPrepare(tool int32) error
	ToolLoad() error
	ToolUnload() error
	ToolStartChange() error
	ToolSetNumber(tool int32) error
	ToolSetOffset(pocket, toolno int32, x, y, z, a, b, c, u, v, w, diameter, frontangle, backangle float64, orientation int32) error
	ToolLoadTable(file string) error
	EstopOn() error
	EstopOff() error
	IoAbort(reason int32) error
	SetDebug(debug int32) error
	GetCmdStatus() (int32, error) // 1=DONE, 2=EXEC, 3=ERROR
	GetToolInSpindle() (int32, error)
	GetPocketPrepped() (int32, error)
}

// IO CmdStatus values.
const (
	IOStatusDone  int32 = 1
	IOStatusExec  int32 = 2
	IOStatusError int32 = 3
)

// MotionStatusReader provides read access to motion state (motstat GMI API).
type MotionStatusReader interface {
	GetStatus() (motstat.MotionStatus, error)
	GetPosCmd() (motstat.Pose, error)
	GetPosFb() (motstat.Pose, error)
	GetInpos() (int32, error)
	GetExecId() (int32, error)
	GetQueueDepth() (int32, error)
	GetCommandNumEcho() (int32, error)
	GetCommandStatus() (int32, error)
}

// ErrorPublisher publishes operator error/text/display messages to UI clients.
type ErrorPublisher interface {
	OperatorError(text string)
	OperatorText(text string)
	OperatorDisplay(text string)
}

// Pose represents a 9-axis position.
// Type alias for the generated motctl.Pose.
type Pose = motctl.Pose

// Cartesian represents a 3D vector.
// Type alias for the generated motctl.Cartesian.
type Cartesian = motctl.Cartesian

// Task is the central controller state. One instance per machine.
//
// Locking model (three locks, strict order cmdMu > seqLifeMu > mu):
//
//   - cmdMu serializes mutating commands for their FULL duration, including
//     I/O and waits. All emccmd entry points are blocking calls; cmdMu is what
//     makes their guard->act sequences atomic against each other (the Go
//     equivalent of the C milltask's single NML command queue). Worker
//     goroutines (sequencer, runProgram producer, mcode worker) never take
//     cmdMu. Abort-class operations (Abort, external/UI estop, pause, resume,
//     jog-stop) fire their signals WITHOUT cmdMu first — signals are what
//     unblock a command stuck in EnqueueCmd backpressure, so they must never
//     queue behind one — and only the state cleanup takes cmdMu.
//   - seqLifeMu serializes sequencer generation changes (StartSequencer /
//     StopSequencer), so concurrent restart requests (e.g. a monitor fault
//     teardown racing a user abort) cannot spawn two sequencer loops.
//   - mu is the short-lived state lock. It is never held across blocking I/O
//     or channel waits; several internal helpers release and re-acquire it
//     around external calls (safe because cmdMu holds command-level atomicity).
//   - msgMu (leaf, below mu) guards only the operator message list, so
//     operatorError is callable from any locking context, including under mu.
//
// Guard evaluation is two-phase: every mutating command runs its full guard
// set as a preflight BEFORE acquiring cmdMu (immediate rejection instead of
// queueing behind a long command) and again inside the serialized body (the
// authoritative check — state may change while waiting for the lock).
//
// Interpreter ownership: the C++ interpreter is not thread-safe. It may be
// touched only (a) under cmdMu with the interpreter idle (checked via
// programBusy), or (b) by the runProgram producer goroutine, whose lifetime
// teardown paths synchronize on runDone before touching the interpreter.
// BuildStat never calls the interpreter; it reads caches published by
// updateActiveCodes.
type Task struct {
	mu sync.Mutex

	// cmdMu serializes mutating command entry points end-to-end (see the
	// locking model above). Always acquired before mu, never while holding mu.
	cmdMu sync.Mutex

	// seqLifeMu serializes StartSequencer/StopSequencer generation changes.
	seqLifeMu sync.Mutex

	// Current state
	state       TaskState
	mode        TaskMode
	interpState InterpState
	execState   ExecState

	// Transactional mode restore: when ensureMode switches mode for a
	// command (e.g. MDI from manual), the previous mode is saved here.
	// After the command completes (MDI queue drained), mode is restored.
	modeBeforeTx TaskMode
	modeTx       bool // true if ensureMode performed a transient switch

	// Configuration
	numJoints       int
	numSpindles     int
	axisMask        int32
	linearUnits     float64
	angularUnits    float64
	maxVelocity     float64
	maxAcceleration float64
	jointMaxVel     [16]float64 // per-joint max velocity for jog clamping
	axisMaxVel      [9]float64  // per-axis max velocity for jog clamping + canon blend
	axisMaxAcc      [9]float64  // per-axis max acceleration for canon vel/acc blend
	startupCode     string
	debug           int32 // EMC_SET_DEBUG level, echoed to stat.debug

	// Flags
	optionalStop  bool
	blockDelete   bool
	floodOn       bool
	mistOn        bool
	lubeOn        bool
	noForceHoming bool // [TRAJ]NO_FORCE_HOMING — skip homing check before MDI/AUTO
	stepping      bool // single-step mode: auto-pause after each interpreter line
	interpActive  bool // true while runProgram goroutine is executing
	// runDone is closed by runProgram when it exits, so teardown paths can wait
	// for the producer to stop touching the interpreter before Close/Reset.
	runDone  chan struct{}
	dwellEnd time.Time // wall-clock end of the current G4 dwell (for delayLeft)

	// Jog selection (shared across clients)
	jogAxis      int32   // selected jog axis (0=X .. 8=W, -1=none)
	jogIncrement float64 // current jog increment (0 = continuous)
	jogSpeed     float64 // linear jog speed (units/sec, from UI slider)
	ajogSpeed    float64 // angular jog speed (deg/sec, from UI slider)

	// Jog watchdog: active continuous jogs must be refreshed within jogTimeout.
	activeJogs [maxJoints]activeJog // indexed by axis_or_joint number

	// Line tracking (for stat reporting)
	readLine    int32 // line the interpreter has read up to
	currentLine int32 // line currently being executed by sequencer

	// Motion segment side table: maps serial segment id → {file, lineno}
	// Written by canon at enqueue time; read by BuildStat for halui.program-line.
	motionMap map[int32]motionInfo

	// Interpreter active codes (updated after each execute). These are the
	// ONLY view of interpreter state stat consumers may use — BuildStat must
	// not call into the (non-thread-safe) interpreter while the producer
	// goroutine may be executing it.
	activeGcodes   []int32
	activeMcodes   []int32
	activeSettings []float64
	callLevel      int32 // cached interp.CallLevel(), updated with the codes

	// canonSnap is a value snapshot of *canon.state, republished by
	// updateActiveCodes after every interpreter execute. Canon mutates its
	// state lock-free on the producer goroutine; stat/halui readers must use
	// this snapshot, never t.canon.state.
	canonSnap CanonState

	// Dependencies (injected, mockable for tests)
	motion MotionController
	io     IOController
	ioStat IOStatusReader // optional; used to verify estop state from HAL
	status MotionStatusReader
	interp Interpreter
	errors ErrorPublisher
	logger *slog.Logger

	// Canon state (interpreter callback context)
	canon *Canon

	// Program state
	programFile string
	programOpen bool
	previewSeq  int32 // increments on changes that invalidate preview

	// Sequencer
	interpQueue chan QueuedCmd
	seqDone     chan struct{} // closed when sequencer goroutine exits
	seqAbort    chan struct{} // close to abort sequencer
	// seqInflight is true while the sequencer is processing a dequeued command
	// (from dequeue until its wait/post-wait completes). waitSequencerDrain
	// needs it: an empty queue with ExecDone is also observable in the instant
	// between dequeue and the command's first setExecState.
	seqInflight bool

	// MDI queue — commands queued while interpreter is busy
	mdiQueue     []string
	maxMDIQueued int

	// Sequencer-level pause/step control
	seqPauseCh  chan struct{} // closed to request sequencer pause
	seqResumeCh chan struct{} // closed to wake sequencer from pause

	// M-code handler (M100-M199)
	mcode *mcodeHandler

	// Cached motion status (fallback if read ever fails)
	lastMotionStatus motstat.MotionStatus
	hasMotionStatus  bool

	// Current message list (independent of emcerror /errors drain queue).
	// Guarded by msgMu, NOT mu — a leaf lock, so operatorError is callable
	// from any locking context (guard rejects emit messages under mu).
	msgMu         sync.Mutex
	messageList   []TaskMessage
	nextMessageID uint64

	// Status display fields (echoed to stat, mirrors C++ EMC_TASK_STAT).
	taskCommand  string // last/executing MDI command string ("" when idle)
	inputTimeout int32  // M66 wait: 0=none/cleared, 1=timed out, 2=waiting
	heartbeat    int32  // monotonic liveness counter, bumped each BuildStat
}

// motionInfo is the state tag milltask keeps for each motion segment, keyed by
// the serial id that motion echoes back. Motion itself is interpreter-agnostic
// (it only carries the id); milltask maps id → tag to report the source line
// and the active G/M codes of the segment actually executing (not readahead).
type motionInfo struct {
	File     string
	LineNo   int32
	Gcodes   []int32   // active G-codes when the segment was queued (nil = untagged)
	Mcodes   []int32   // active M-codes
	Settings []float64 // active settings (feed, speed, …)
}

// NewTask creates a new Task with dependencies injected.
func NewTask(motion MotionController, io IOController, status MotionStatusReader, logger *slog.Logger) *Task {
	t := &Task{
		state:          StateEstop,
		mode:           ModeManual,
		interpState:    InterpIdle,
		execState:      ExecDone,
		motion:         motion,
		io:             io,
		status:         status,
		logger:         logger,
		activeSettings: make([]float64, 5), // ACTIVE_SETTINGS
		activeGcodes:   make([]int32, 17),  // ACTIVE_G_CODES
		activeMcodes:   make([]int32, 10),  // ACTIVE_M_CODES
		maxMDIQueued:   10,
		mcode:          newMcodeHandler(),
		motionMap:      make(map[int32]motionInfo),
	}
	t.canon = NewCanon(t)
	t.canonSnap = *t.canon.state
	return t
}

// registerMotion records the G-code location for a motion segment serial id.
// Called by Canon when a motion segment is enqueued.
func (t *Task) registerMotion(id int32, file string, lineno int32) {
	t.mu.Lock()
	t.motionMap[id] = motionInfo{File: file, LineNo: lineno}
	t.mu.Unlock()
}

// lookupMotionLine returns the G-code line number for a motion segment id.
// Returns 0 if not found.
func (t *Task) lookupMotionLine(id int32) int32 {
	t.mu.Lock()
	info, ok := t.motionMap[id]
	t.mu.Unlock()
	if !ok {
		return 0
	}
	return info.LineNo
}

// lookupMotionInfo returns the full state tag for a motion segment id.
func (t *Task) lookupMotionInfo(id int32) (motionInfo, bool) {
	t.mu.Lock()
	info, ok := t.motionMap[id]
	t.mu.Unlock()
	return info, ok
}

// tagMotionRange attaches the active-code state tag to every motion segment
// whose serial id is in [startID, endID) — the segments queued while the
// interpreter executed one source line. Codes are captured after that line's
// execute (their modal state), completing the id → state_tag mapping.
func (t *Task) tagMotionRange(startID, endID int32, gcodes, mcodes []int32, settings []float64) {
	if endID <= startID {
		return
	}
	t.mu.Lock()
	for id := startID; id < endID; id++ {
		if info, ok := t.motionMap[id]; ok {
			info.Gcodes, info.Mcodes, info.Settings = gcodes, mcodes, settings
			t.motionMap[id] = info
		}
	}
	t.mu.Unlock()
}

// pruneMotionMap removes entries with id less than execId to bound map size.
// Called periodically during status updates.
func (t *Task) pruneMotionMap(execId int32) {
	t.mu.Lock()
	for id := range t.motionMap {
		if id < execId {
			delete(t.motionMap, id)
		}
	}
	t.mu.Unlock()
}

// SetInterpreter sets the interpreter dependency. Must be called before
// running G-code programs. The interpreter should already have its canon
// callbacks wired via SetCanonCallbacks.
func (t *Task) SetInterpreter(interp Interpreter) {
	t.interp = interp
}

// SetErrorPublisher sets the error publisher for operator messages.
func (t *Task) SetErrorPublisher(ep ErrorPublisher) {
	t.errors = ep
}

// SetIOStatusReader sets the IO status reader for estop state verification.
func (t *Task) SetIOStatusReader(r IOStatusReader) {
	t.ioStat = r
}

// operatorError sends an operator error message to connected UIs.
func (t *Task) operatorError(text string) {
	t.appendMessage(emcerror.ErrorKind_OPERATOR_ERROR, text)
	if t.errors != nil {
		t.errors.OperatorError(text)
	}
	t.logger.Warn("operator error", "msg", text)
}

// updateActiveCodes fetches the interpreter's active G/M codes and settings
// and stores them in the task state for stat reporting. It also republishes
// the canon state snapshot and call level — this is the single point where
// interpreter/canon state becomes visible to stat consumers, so it must be
// called by whoever owns the interpreter after anything that may have changed
// that state (execute, reset).
func (t *Task) updateActiveCodes(interp Interpreter) (gc, mc []int32, st []float64) {
	gc = interp.ActiveGCodes()
	mc = interp.ActiveMCodes()
	st = interp.ActiveSettings()
	cl := int32(interp.CallLevel())
	t.mu.Lock()
	t.activeGcodes = gc
	t.activeMcodes = mc
	t.activeSettings = st
	t.callLevel = cl
	t.canonSnap = *t.canon.state
	t.mu.Unlock()
	return gc, mc, st
}
