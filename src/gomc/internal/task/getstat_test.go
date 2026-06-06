package task

import (
	"log/slog"
	"os"
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/emcstat"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/motstat"
)

// richMockStatus returns a MotionStatus with realistic data for testing GetStat.
type richMockStatus struct {
	status motstat.MotionStatus
}

func (m *richMockStatus) GetStatus() (motstat.MotionStatus, error) { return m.status, nil }
func (m *richMockStatus) GetPosCmd() (motstat.Pose, error)         { return m.status.CartePosCmd, nil }
func (m *richMockStatus) GetPosFb() (motstat.Pose, error)          { return m.status.CartePosFb, nil }
func (m *richMockStatus) GetInpos() (int32, error)                 { return m.status.Inpos, nil }
func (m *richMockStatus) GetExecId() (int32, error)                { return m.status.Id, nil }
func (m *richMockStatus) GetQueueDepth() (int32, error)            { return m.status.QueueDepth, nil }
func (m *richMockStatus) GetCommandNumEcho() (int32, error)        { return m.status.CommandNumEcho, nil }
func (m *richMockStatus) GetCommandStatus() (int32, error)         { return m.status.CommandStatus, nil }

func newRichTestTask() (*Task, *richMockStatus) {
	ms := &richMockStatus{
		status: motstat.MotionStatus{
			Enabled:      1,
			Inpos:        1,
			Paused:       0,
			FeedScale:    1.0,
			RapidScale:   0.5,
			LimitVel:     100.0,
			CurrentVel:   42.5,
			DistanceToGo: 12.3,
			Id:           7,
			MotionType:   2,
			Dtg:          motstat.Pose{X: 1.0, Y: 2.0, Z: 3.0},
			CartePosCmd:  motstat.Pose{X: 10, Y: 20, Z: 30, A: 1, B: 2, C: 3},
			CartePosFb:   motstat.Pose{X: 10.1, Y: 20.1, Z: 30.1},
			ToolOffset:   motstat.Pose{X: 0, Y: 0, Z: 50.0},
			KinType:      1, // IDENTITY
			Joints: [16]motstat.JointStatus{
				{Homed: 1, Enabled: 1, PosFb: 10.1, VelCmd: 5.0, MinPosLimit: -100, MaxPosLimit: 100},
				{Homed: 1, Enabled: 1, PosFb: 20.1, VelCmd: 3.0, MinPosLimit: -200, MaxPosLimit: 200},
				{Homed: 0, Enabled: 1, PosFb: 30.1, VelCmd: 1.0, MinPosLimit: -50, MaxPosLimit: 50, OnPosLimit: 1},
			},
			Spindles: [8]motstat.SpindleStatus{
				{Speed: 1000, Direction: 1, Brake: 0, Homed: 1, Scale: 1.0},
			},
			Axes: [9]motstat.AxisStatus{
				{MinPosLimit: -100, MaxPosLimit: 100, VelLimit: 50},
				{MinPosLimit: -200, MaxPosLimit: 200, VelLimit: 50},
				{MinPosLimit: -50, MaxPosLimit: 50, VelLimit: 25},
			},
			Probe: motstat.ProbeStatus{
				Pos: motstat.Pose{X: 5, Y: 6, Z: 7},
			},
		},
	}

	mot := &mockMotion{}
	io := &mockIO{}
	logger := slog.New(slog.NewTextHandler(os.Stderr, &slog.HandlerOptions{Level: slog.LevelError}))
	t := NewTask(mot, io, ms, logger)

	// Configure task state as if it started up.
	t.numJoints = 3
	t.axisMask = 7 // XYZ
	t.linearUnits = 1.0
	t.numSpindles = 1

	return t, ms
}

func TestGetStat_TaskState(t *testing.T) {
	task, _ := newRichTestTask()
	task.SetState(int32(StateEstopReset))
	task.SetState(int32(StateOn))
	task.SetMode(int32(ModeMDI))

	stat := task.BuildStat()

	if stat.Task.State != emcstat.TaskState_ON {
		t.Errorf("Task.State = %d, want ON(%d)", stat.Task.State, emcstat.TaskState_ON)
	}
	if stat.Task.Mode != emcstat.TaskMode_MDI {
		t.Errorf("Task.Mode = %d, want MDI(%d)", stat.Task.Mode, emcstat.TaskMode_MDI)
	}
	if stat.Task.InterpState != emcstat.InterpState_IDLE {
		t.Errorf("Task.InterpState = %d, want IDLE", stat.Task.InterpState)
	}
}

func TestGetStat_MotionFields(t *testing.T) {
	task, _ := newRichTestTask()
	bringUp(task)

	stat := task.BuildStat()

	if !stat.Motion.Enabled {
		t.Error("Motion.Enabled should be true")
	}
	if stat.Motion.Feedrate != 1.0 {
		t.Errorf("Motion.Feedrate = %f, want 1.0", stat.Motion.Feedrate)
	}
	if stat.Motion.Rapidrate != 0.5 {
		t.Errorf("Motion.Rapidrate = %f, want 0.5", stat.Motion.Rapidrate)
	}
	if stat.Motion.CurrentVel != 42.5 {
		t.Errorf("Motion.CurrentVel = %f, want 42.5", stat.Motion.CurrentVel)
	}
	if stat.Motion.DistanceToGo != 12.3 {
		t.Errorf("Motion.DistanceToGo = %f, want 12.3", stat.Motion.DistanceToGo)
	}
	if stat.Motion.Dtg.X != 1.0 || stat.Motion.Dtg.Y != 2.0 || stat.Motion.Dtg.Z != 3.0 {
		t.Errorf("Motion.Dtg = %+v, want {1,2,3}", stat.Motion.Dtg)
	}
	if stat.Motion.MotionId != 7 {
		t.Errorf("Motion.MotionId = %d, want 7", stat.Motion.MotionId)
	}
	if stat.Motion.MotionType != 2 {
		t.Errorf("Motion.MotionType = %d, want 2", stat.Motion.MotionType)
	}
}

func TestGetStat_Positions(t *testing.T) {
	task, _ := newRichTestTask()
	bringUp(task)

	stat := task.BuildStat()

	// Position = commanded cartesian
	if stat.Position.X != 10 || stat.Position.Y != 20 || stat.Position.Z != 30 {
		t.Errorf("Position = %+v, want {10,20,30,...}", stat.Position)
	}
	// ActualPosition = feedback cartesian
	if stat.ActualPosition.X != 10.1 || stat.ActualPosition.Y != 20.1 || stat.ActualPosition.Z != 30.1 {
		t.Errorf("ActualPosition = %+v, want {10.1,20.1,30.1,...}", stat.ActualPosition)
	}
	// ToolOffset
	if stat.ToolOffset.Z != 50.0 {
		t.Errorf("ToolOffset.Z = %f, want 50", stat.ToolOffset.Z)
	}
	// ProbedPosition
	if stat.ProbedPosition.X != 5 || stat.ProbedPosition.Y != 6 || stat.ProbedPosition.Z != 7 {
		t.Errorf("ProbedPosition = %+v, want {5,6,7,...}", stat.ProbedPosition)
	}
}

func TestGetStat_Joints(t *testing.T) {
	task, _ := newRichTestTask()
	bringUp(task)

	stat := task.BuildStat()

	if stat.JointsCount != 3 {
		t.Fatalf("JointsCount = %d, want 3", stat.JointsCount)
	}
	if len(stat.Joints) != 3 {
		t.Fatalf("len(Joints) = %d, want 3", len(stat.Joints))
	}

	j0 := stat.Joints[0]
	if !j0.Homed {
		t.Error("Joint[0].Homed should be true")
	}
	if !j0.Enabled {
		t.Error("Joint[0].Enabled should be true")
	}
	if j0.MinSoftLimit != -100 {
		t.Errorf("Joint[0].MinSoftLimit = %f, want -100", j0.MinSoftLimit)
	}
	if j0.MaxSoftLimit != 100 {
		t.Errorf("Joint[0].MaxSoftLimit = %f, want 100", j0.MaxSoftLimit)
	}
	if j0.Velocity != 5.0 {
		t.Errorf("Joint[0].Velocity = %f, want 5.0", j0.Velocity)
	}
	if j0.Input != 10.1 {
		t.Errorf("Joint[0].Input = %f, want 10.1 (pos_fb)", j0.Input)
	}

	// Joint 2 is on positive limit
	j2 := stat.Joints[2]
	if j2.Homed {
		t.Error("Joint[2].Homed should be false")
	}
	if !j2.MaxHardLimit {
		t.Error("Joint[2].MaxHardLimit should be true (on_pos_limit=1)")
	}

	// Homed array
	if !stat.Homed[0] || !stat.Homed[1] || stat.Homed[2] {
		t.Errorf("Homed = %v, want [true,true,false,...]", stat.Homed[:3])
	}
}

func TestGetStat_Axes(t *testing.T) {
	task, _ := newRichTestTask()
	bringUp(task)

	stat := task.BuildStat()

	if stat.AxisMask != 7 {
		t.Errorf("AxisMask = %d, want 7", stat.AxisMask)
	}
	if len(stat.Axis) != 3 {
		t.Fatalf("len(Axis) = %d, want 3", len(stat.Axis))
	}

	ax0 := stat.Axis[0]
	if ax0.MinPositionLimit != -100 {
		t.Errorf("Axis[0].MinPositionLimit = %f, want -100", ax0.MinPositionLimit)
	}
	if ax0.MaxPositionLimit != 100 {
		t.Errorf("Axis[0].MaxPositionLimit = %f, want 100", ax0.MaxPositionLimit)
	}
}

func TestGetStat_Spindle(t *testing.T) {
	task, _ := newRichTestTask()
	bringUp(task)

	stat := task.BuildStat()

	if len(stat.Spindle) != 1 {
		t.Fatalf("len(Spindle) = %d, want 1", len(stat.Spindle))
	}
	sp := stat.Spindle[0]
	if sp.Speed != 1000 {
		t.Errorf("Spindle[0].Speed = %f, want 1000", sp.Speed)
	}
	if sp.Direction != 1 {
		t.Errorf("Spindle[0].Direction = %d, want 1", sp.Direction)
	}
	if !sp.Homed {
		t.Error("Spindle[0].Homed should be true")
	}
}

func TestGetStat_ScalarFields(t *testing.T) {
	task, _ := newRichTestTask()
	bringUp(task)

	stat := task.BuildStat()

	if stat.KinematicsType != emcstat.KinematicsType_IDENTITY {
		t.Errorf("KinematicsType = %d, want IDENTITY(1)", stat.KinematicsType)
	}
	if stat.LinearUnits != 1.0 {
		t.Errorf("LinearUnits = %f, want 1.0", stat.LinearUnits)
	}
}

func TestGetStat_NilTask(t *testing.T) {
	// milltaskModule with nil task returns safe defaults.
	m := &milltaskModule{}
	stat, err := m.GetStat()
	if err != nil {
		t.Fatal(err)
	}
	if stat.Task.State != emcstat.TaskState_ESTOP {
		t.Errorf("nil task: State = %d, want ESTOP", stat.Task.State)
	}
}
