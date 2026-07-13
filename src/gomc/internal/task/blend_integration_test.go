// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

// Integration tests for per-axis velocity/acceleration blending, driving REAL
// canon calls through the real canon -> sequencer -> motion boundary and
// asserting WRITTEN rules on the emitted motion commands (not a golden diff).
//
// The expected vel/ini_maxvel/acc are hand-derived from the per-axis config
// limits (X:40/600, Y:25/400, Z:8/120) and each move's geometry, and were
// confirmed against the instrumented C milltask capture
// (tests/milltask-parity/logs/old/lines.log, arcs.log) as the ORACLE — but what
// the test asserts is the derived RULE with its reasoning, so it documents
// intent and cannot be silently re-blessed.
//
// The _Lines/_Arcs tests drive the canon directly (the gomc-specific
// blend/transform/emit path); _Lines_ViaInterpreter drives the same program
// through the REAL rs274ngc interpreter (file-based param IO on a throwaway
// temp copy of the fixture .var — so the committed fixture stays read-only),
// proving interp -> canon -> blend -> motion end to end.

import (
	"log/slog"
	"math"
	"os"
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

// recordingMotion records the full arguments of every queued move command.
type recMove struct {
	kind                string // "line" | "circle"
	pos                 Pose
	vel, iniMaxvel, acc float64
	motionType          int32
	feed                float64   // FeedMmPerMin
	center, normal      Cartesian // circle only
	turn                int32     // circle only
}

// spindleRec records the full arguments of a SpindleOn/SpindleOff command.
// css args map to motion.SpindleOn(spindle, speed, css_factor, css_max, wait);
// the C trace prints css_max as "css_offset".
type spindleRec struct {
	on                          bool // true=SpindleOn, false=SpindleOff
	spindle                     int32
	speed, cssFactor, cssOffset float64
	wait                        int32
}

type recordingMotion struct {
	mockMotion
	moves       []recMove
	spindleCmds []spindleRec
}

func (m *recordingMotion) SetLine(pos Pose, vel, iniMaxvel, acc float64, mt int32, id int32, feedUpm float64, ij int32) error {
	m.moves = append(m.moves, recMove{kind: "line", pos: pos, vel: vel, iniMaxvel: iniMaxvel, acc: acc, motionType: mt, feed: feedUpm})
	return nil
}

func (m *recordingMotion) SetCircle(pos Pose, center, normal Cartesian, turn int32, vel, iniMaxvel, acc float64, mt int32, id int32, feedUpm float64) error {
	m.moves = append(m.moves, recMove{kind: "circle", pos: pos, vel: vel, iniMaxvel: iniMaxvel, acc: acc, motionType: mt, feed: feedUpm, center: center, normal: normal, turn: turn})
	return nil
}

func (m *recordingMotion) SpindleOn(spindle int32, speed, cssFactor, cssMax float64, wait int32) error {
	m.spindleCmds = append(m.spindleCmds, spindleRec{true, spindle, speed, cssFactor, cssMax, wait})
	return nil
}

func (m *recordingMotion) SpindleOff(spindle int32) error {
	m.spindleCmds = append(m.spindleCmds, spindleRec{on: false, spindle: spindle})
	return nil
}

// newBlendCanonTask builds a real Task+Canon with the parity3 non-uniform axis
// limits and a recording motion sink, with the sequencer running.
func newBlendCanonTask(t *testing.T) (*Task, *recordingMotion) {
	t.Helper()
	mot := &recordingMotion{}
	st := &testStatus{}
	st.inPosition.Store(true)
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	task := NewTask(mot, &mockIO{}, st, logger)
	applyBlendLimits(task) // shared axis mask + per-axis vel/acc limits (S2)
	task.maxAcceleration = 600
	task.numJoints = 3
	task.canon.UseLengthUnits(2) // CANON_UNITS_MM
	task.StartSequencer()
	return task, mot
}

// collect drains the sequencer and returns the recorded moves.
func collect(t *testing.T, task *Task, mot *recordingMotion) []recMove {
	t.Helper()
	task.DrainQueue()
	task.StopSequencer()
	return mot.moves
}

const blendEps = 1e-3

func sf(c *Canon, x, y, z float64) { c.StraightFeed(0, x, y, z, 0, 0, 0, 0, 0, 0) }
func st(c *Canon, x, y, z float64) { c.StraightTraverse(0, x, y, z, 0, 0, 0, 0, 0, 0) }

func checkMove(t *testing.T, got recMove, kind string, x, y, z, vel, ini, acc float64, why string) {
	t.Helper()
	if got.kind != kind {
		t.Errorf("%s: kind=%s want %s", why, got.kind, kind)
	}
	if math.Abs(got.pos.X-x) > blendEps || math.Abs(got.pos.Y-y) > blendEps || math.Abs(got.pos.Z-z) > blendEps {
		t.Errorf("%s: pos=[%.3f,%.3f,%.3f] want [%.3f,%.3f,%.3f]", why, got.pos.X, got.pos.Y, got.pos.Z, x, y, z)
	}
	if math.Abs(got.vel-vel) > blendEps {
		t.Errorf("%s: vel=%.4f want %.4f", why, got.vel, vel)
	}
	if math.Abs(got.iniMaxvel-ini) > blendEps {
		t.Errorf("%s: ini_maxvel=%.4f want %.4f", why, got.iniMaxvel, ini)
	}
	if math.Abs(got.acc-acc) > blendEps {
		t.Errorf("%s: acc=%.4f want %.4f", why, got.acc, acc)
	}
}

// TestBlendIntegration_Lines replays lines.ngc's moves through the canon and
// asserts each emitted SET_LINE. vel = min(programmed feed, per-axis blend);
// ini_maxvel/acc = the per-axis blend for the move's direction.
func TestBlendIntegration_Lines(t *testing.T) {
	task, mot := newBlendCanonTask(t)
	c := task.canon

	st(c, 0, 0, 5)     // G0 X0 Y0 Z5  — Z-only traverse
	c.SetFeedRate(600) // F600 = 10 mm/s
	sf(c, 0, 0, 0)     // G1 Z0
	sf(c, 20, 0, 0)    // G1 X20
	sf(c, 20, 20, 0)   // G1 Y20
	c.SetFeedRate(1200)
	sf(c, 0, 0, 0) // G1 X0 Y0  (diagonal)
	c.SetFeedRate(1500)
	sf(c, 30, 15, -10) // G1 X30 Y15 Z-10
	st(c, 30, 15, 5)   // G0 Z5

	m := collect(t, task, mot)
	if len(m) != 7 {
		t.Fatalf("expected 7 moves, got %d: %+v", len(m), m)
	}
	// pos, vel, ini_maxvel, acc — with the per-axis reasoning.
	checkMove(t, m[0], "line", 0, 0, 5, 8, 8, 120, "Z-only traverse: capped at Z max (8/120)")
	checkMove(t, m[1], "line", 0, 0, 0, 8, 8, 120, "Z-only feed: feed 10 capped by Z blend 8")
	checkMove(t, m[2], "line", 20, 0, 0, 10, 40, 600, "X-only feed: feed 10 < X blend 40")
	checkMove(t, m[3], "line", 20, 20, 0, 10, 25, 400, "Y-only feed: feed 10 < Y blend 25")
	// XY diagonal: t=[20/40,20/25]=[.5,.8], tmax=.8, dtot=√800=28.284, blend=28.284/.8=35.355
	checkMove(t, m[4], "line", 0, 0, 0, 20, 35.35534, 565.68542, "XY diagonal: feed 20 < blend 35.355")
	// XYZ: t=[30/40,15/25,10/8]=[.75,.6,1.25], tmax=1.25(Z), dtot=√1225=35, blend=35/1.25=28
	checkMove(t, m[5], "line", 30, 15, -10, 25, 28, 420, "XYZ (Z-dominated): feed 25 < blend 28")
	checkMove(t, m[6], "line", 30, 15, 5, 8, 8, 120, "Z-only traverse: Z max 8")
}

// TestBlendIntegration_Arcs replays arcs.ngc through the canon and asserts arc
// centripetal limiting: ini_maxvel is capped by v = sqrt(a_normal * r_eff), so a
// small-radius arc is slowed below the per-axis blend, and a helical arc lower
// still. Values verified against logs/old/arcs.log.
func TestBlendIntegration_Arcs(t *testing.T) {
	task, mot := newBlendCanonTask(t)
	c := task.canon
	// ArcFeed args: (lineno, xEnd, yEnd, centerX, centerY, rotation, zEnd, a..w).
	// rotation: -1 = CW (G2), +1 = CCW (G3). Center is absolute (interp resolves I/J).
	st(c, 0, 0, 5)                                      // G0 Z5
	c.SetFeedRate(400)                                  // F400 = 6.667 mm/s
	sf(c, 0, 0, 0)                                      // G1 Z0
	c.SetFeedRate(800)                                  // F800 = 13.333 mm/s
	c.ArcFeed(0, 20, 0, 10, 0, -1, 0, 0, 0, 0, 0, 0, 0) // G2 X20 Y0 I10 J0  (r=10 CW)
	c.ArcFeed(0, 0, 0, 10, 0, 1, 0, 0, 0, 0, 0, 0, 0)   // G3 X0 Y0 I-10 J0  (r=10 CCW)
	c.ArcFeed(0, 2, 0, 1, 0, -1, 0, 0, 0, 0, 0, 0, 0)   // G2 X2 Y0 I1 J0    (r=1 CW)
	c.ArcFeed(0, 0, 0, 1, 0, -1, -5, 0, 0, 0, 0, 0, 0)  // G2 X0 Y0 Z-5 I-1  (helical)
	st(c, 0, 0, 5)                                      // G0 Z5

	m := collect(t, task, mot)
	if len(m) != 7 {
		t.Fatalf("expected 7 moves, got %d: %+v", len(m), m)
	}
	checkMove(t, m[0], "line", 0, 0, 5, 8, 8, 120, "Z-only traverse")
	checkMove(t, m[1], "line", 0, 0, 0, 6.6666667, 8, 120, "Z-only feed: F400=6.667 < Z blend 8")
	checkMove(t, m[2], "circle", 20, 0, 0, 13.3333333, 25, 400, "r=10 arc: feed 13.333 < arc blend 25")
	checkMove(t, m[3], "circle", 0, 0, 0, 13.3333333, 25, 400, "r=10 arc back")
	checkMove(t, m[4], "circle", 2, 0, 0, 13.3333333, 18.6120972, 400, "r=1 arc: centripetal caps ini_maxvel at 18.612")
	checkMove(t, m[5], "circle", 0, 0, -5, 9.4480785, 9.4480785, 141.721177, "helical r=1: Z coupling caps vel/acc lower still")
	checkMove(t, m[6], "line", 0, 0, 5, 8, 8, 120, "Z-only traverse")
}

// --- Full-stack integration: real rs274ngc interpreter -> canon -> motion ---

// runNGCViaInterp runs an NGC program through the REAL interpreter (with the
// in-memory param IO) and returns the recorded moves. Hermetic: INI is parsed
// from a string, the program is written to a temp dir.
func runNGCViaInterp(t *testing.T, program string) []recMove {
	return runNGCViaInterpRec(t, program).moves
}

func runNGCViaInterpRec(t *testing.T, program string) *recordingMotion {
	t.Helper()
	mot := &recordingMotion{}
	st := &testStatus{}
	st.inPosition.Store(true)
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	task := NewTask(mot, &mockIO{}, st, logger)
	applyBlendLimits(task) // shared axis mask + per-axis vel/acc limits (S2)
	task.maxAcceleration = 600
	task.numJoints = 3
	task.linearUnits = 1.0 // mm native; feeds GET_EXTERNAL_LENGTH_UNITS (else 0/0=NaN)

	dir := t.TempDir()
	prog := dir + "/prog.ngc"
	if err := os.WriteFile(prog, []byte(program), 0o644); err != nil {
		t.Fatalf("write program: %v", err)
	}
	// Copy the read-only fixture params to a throwaway temp file so the
	// interpreter's save() writes there, never touching the committed fixture.
	varPath := dir + "/params.var"
	varData, err := os.ReadFile("testdata/blend.var")
	if err != nil {
		t.Fatalf("read fixture var: %v", err)
	}
	if err := os.WriteFile(varPath, varData, 0o644); err != nil {
		t.Fatalf("write temp var: %v", err)
	}
	ini, err := inifile.ParseString("[EMC]\nMACHINE=blendtest\n[RS274NGC]\nPARAMETER_FILE=" + varPath + "\n[TRAJ]\nCOORDINATES=X Y Z\nLINEAR_UNITS=mm\n[EMCIO]\n")
	if err != nil {
		t.Fatalf("parse ini: %v", err)
	}

	interp, err := NewCInterp()
	if err != nil {
		t.Fatalf("NewCInterp: %v", err)
	}
	defer interp.Destroy()
	ct := newCanonCallbackTable(task.canon)
	defer ct.release()
	interp.SetCanonCallbacks(ct.ptr())
	accHandle, err := interp.IniLoadAccessor(ini)
	if err != nil {
		t.Fatalf("IniLoadAccessor: %v", err)
	}
	defer FreeIniAccessor(accHandle)
	paramIO := newInterpParamIOFile(varPath)
	defer paramIO.destroy()
	paramIO.install(interp)
	interp.SetTaskMode(1)
	if err := interp.Init(); err != nil {
		t.Fatalf("interp.Init: %v [%q]", err, interp.ErrorText(InterpError))
	}
	task.SetInterpreter(interp)
	interp.RegisterAllMcodeSlots()
	if err := interp.Synch(); err != nil {
		t.Fatalf("interp.Synch: %v", err)
	}

	task.StartSequencer()
	setActiveCanon(task.canon)
	defer clearActiveCanon()
	if err := interp.Open(prog); err != nil {
		t.Fatalf("interp.Open: %v", err)
	}
	for {
		rc, err := interp.Read()
		if err != nil {
			t.Fatalf("interp.Read: %v", err)
		}
		if rc == InterpEndfile || rc == InterpExit {
			break
		}
		rc, err = interp.Execute()
		if err != nil {
			t.Fatalf("interp.Execute: %v [%q]", err, interp.ErrorText(InterpError))
		}
		if rc == InterpEndfile || rc == InterpExit {
			break
		}
	}
	task.DrainQueue()
	task.StopSequencer()
	return mot
}

const linesNGC = `G21 G90 G94 G17
G0 X0 Y0 Z5
G1 Z0 F600
G1 X20 F600
G1 Y20 F600
G1 X0 Y0 F1200
G1 X30 Y15 Z-10 F1500
G0 Z5
M2
`

// Same assertions as TestBlendIntegration_Lines, but driven through the REAL
// interpreter — proving interp -> canon -> blend -> motion end to end.
func TestBlendIntegration_Lines_ViaInterpreter(t *testing.T) {
	m := runNGCViaInterp(t, linesNGC)
	if len(m) != 7 {
		t.Fatalf("expected 7 moves, got %d: %+v", len(m), m)
	}
	checkMove(t, m[0], "line", 0, 0, 5, 8, 8, 120, "Z-only traverse")
	checkMove(t, m[1], "line", 0, 0, 0, 8, 8, 120, "Z-only feed capped by Z blend")
	checkMove(t, m[2], "line", 20, 0, 0, 10, 40, 600, "X-only feed 10 < X blend 40")
	checkMove(t, m[3], "line", 20, 20, 0, 10, 25, 400, "Y-only feed 10 < Y blend 25")
	checkMove(t, m[4], "line", 0, 0, 0, 20, 35.35534, 565.68542, "XY diagonal")
	checkMove(t, m[5], "line", 30, 15, -10, 25, 28, 420, "XYZ Z-dominated")
	checkMove(t, m[6], "line", 30, 15, 5, 8, 8, 120, "Z-only traverse")
}

// spindleNGC drives the spindle command stream captured in logs/old/spindle.log:
// S-before-M3 (dir=0 -> speed 0), M3, S re-tune while running, G96 CSS (requires
// an S word on the same line), G97 back to RPM, M5.
const spindleNGC = `G21 G90 G94 G17
S1000
M3
G0 Z5
G1 Z0 F300
S2000
G1 X10 F600
G96 D3000 S200
G1 X20 F600
G97 S1500
M5
M2
`

func checkSpindleOn(t *testing.T, got spindleRec, speed, cssFactor, cssOffset float64, wait int32, why string) {
	t.Helper()
	if !got.on {
		t.Errorf("%s: expected SpindleOn, got SpindleOff", why)
		return
	}
	if math.Abs(got.speed-speed) > blendEps {
		t.Errorf("%s: speed=%.4f want %.4f", why, got.speed, speed)
	}
	if math.Abs(got.cssFactor-cssFactor) > blendEps {
		t.Errorf("%s: css_factor=%.4f want %.4f", why, got.cssFactor, cssFactor)
	}
	if math.Abs(got.cssOffset-cssOffset) > blendEps {
		t.Errorf("%s: css_offset=%.4f want %.4f", why, got.cssOffset, cssOffset)
	}
	if got.wait != wait {
		t.Errorf("%s: wait=%d want %d", why, got.wait, wait)
	}
}

// TestBlendIntegration_Spindle_ViaInterpreter drives the spindle program through
// the REAL interpreter and asserts the emitted spindle command stream.
//
// Deliberate divergence from the C milltask oracle (logs/old/spindle.log): the C
// SET_SPINDLE_SPEED always appends a SPINDLE_ON message, and for a *stopped*
// spindle (dir=0) it carries state=0 (emcSpindleSpeed does not force state), so
// the machine is not enabled — it is a status-only no-op. gomc's spindle-on GMI
// call hardcodes state=1 (motctl_handlers.c h_spindle_on), so it cannot express
// "set speed, stay off". Rather than enable the drive at zero speed and release
// its brake, the canon drops the command entirely when dir=0 (see
// Canon.SetSpindleSpeed). Machine behavior is identical to 2.9; only the leading
// status-only SPINDLE_ON (s=0 speed=0) is absent. The remaining stream matches
// the oracle:
//   - M3: dir=+1, emit at 1000 with wait=1.
//   - S2000 while running: re-emit at 2000, carrying the stale wait=1.
//   - G96 D3000 S200 (CSS): Speed = the D-word RPM cap (3000); css_factor =
//     (1000/2pi)*S surface speed (mm), i.e. the same k the canon uses.
//   - G97 S1500: back to RPM mode, css_factor=0.
//   - M5 then M2: two SPINDLE_OFF (explicit stop + program-end teardown).
func TestBlendIntegration_Spindle_ViaInterpreter(t *testing.T) {
	mot := runNGCViaInterpRec(t, spindleNGC)
	sp := mot.spindleCmds
	if len(sp) != 6 {
		t.Fatalf("expected 6 spindle commands, got %d: %+v", len(sp), sp)
	}
	cssFactor200 := 1000.0 / (2 * math.Pi) * 200 // mm CSS: k * surface speed(200)
	checkSpindleOn(t, sp[0], 1000, 0, 0, 1, "M3: dir=+1 -> speed 1000, wait=1")
	checkSpindleOn(t, sp[1], 2000, 0, 0, 1, "S2000 while running: re-emit, wait carried")
	checkSpindleOn(t, sp[2], 3000, cssFactor200, 0, 1, "G96 D3000 S200: speed=cap, css_factor=(1000/2pi)*200")
	checkSpindleOn(t, sp[3], 1500, 0, 0, 1, "G97 S1500: back to RPM, css_factor=0")
	if sp[4].on || sp[5].on {
		t.Errorf("expected two SPINDLE_OFF at end (M5, M2), got sp[4]=%+v sp[5]=%+v", sp[4], sp[5])
	}

	// The interleaved moves use the same per-axis blend as the lines test.
	if len(mot.moves) != 4 {
		t.Fatalf("expected 4 moves, got %d: %+v", len(mot.moves), mot.moves)
	}
	checkMove(t, mot.moves[0], "line", 0, 0, 5, 8, 8, 120, "G0 Z5")
	checkMove(t, mot.moves[1], "line", 0, 0, 0, 5, 8, 120, "G1 Z0 F300: feed 5 < Z blend 8")
	checkMove(t, mot.moves[2], "line", 10, 0, 0, 10, 40, 600, "G1 X10 F600")
	checkMove(t, mot.moves[3], "line", 20, 0, 0, 10, 40, 600, "G1 X20 F600")
}

// arcsNGC is the arc program whose emitted motion the C milltask captured in
// logs/old/arcs.log. Arc centers use the default incremental (G91.1) I/J: each
// is relative to the arc's start point. Radii were verified start==end.
const arcsNGC = `G21 G90 G94 G17
G0 X0 Y0 Z5
G1 Z0 F400
G2 X20 Y0 I10 J0 F800
G3 X0 Y0 I-10 J0
G2 X2 Y0 I1 J0
G2 X0 Y0 Z-5 I-1 J0
G0 Z5
M2
`

// Same arc centripetal-limiting assertions as TestBlendIntegration_Arcs, but
// driven through the REAL interpreter. The expected vel/ini_maxvel/acc match the
// C milltask oracle exactly (logs/old/arcs.log SET_CIRCLE/SET_LINE block):
//
//	SET_LINE   [0,0,5]  vel=8          ini=8          acc=120
//	SET_LINE   [0,0,0]  vel=6.66666667 ini=8          acc=120
//	SET_CIRCLE [20,0,0] vel=13.3333333 ini=25         acc=400
//	SET_CIRCLE [0,0,0]  vel=13.3333333 ini=25         acc=400
//	SET_CIRCLE [2,0,0]  vel=13.3333333 ini=18.6120972 acc=400
//	SET_CIRCLE [0,0,-5] vel=9.4480785  ini=9.4480785  acc=141.721177
//	SET_LINE   [0,0,5]  vel=8          ini=8          acc=120
func TestBlendIntegration_Arcs_ViaInterpreter(t *testing.T) {
	m := runNGCViaInterp(t, arcsNGC)
	if len(m) != 7 {
		t.Fatalf("expected 7 moves, got %d: %+v", len(m), m)
	}
	checkMove(t, m[0], "line", 0, 0, 5, 8, 8, 120, "Z-only traverse")
	checkMove(t, m[1], "line", 0, 0, 0, 6.6666667, 8, 120, "Z-only feed: F400=6.667 < Z blend 8")
	checkMove(t, m[2], "circle", 20, 0, 0, 13.3333333, 25, 400, "r=10 arc: feed 13.333 < arc blend 25")
	checkMove(t, m[3], "circle", 0, 0, 0, 13.3333333, 25, 400, "r=10 arc back")
	checkMove(t, m[4], "circle", 2, 0, 0, 13.3333333, 18.6120972, 400, "r=1 arc: centripetal caps ini_maxvel at 18.612")
	checkMove(t, m[5], "circle", 0, 0, -5, 9.4480785, 9.4480785, 141.721177, "helical r=1: Z coupling caps vel/acc lower still")
	checkMove(t, m[6], "line", 0, 0, 5, 8, 8, 120, "Z-only traverse")
}
