// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"fmt"

	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/emccmd"
	"github.com/sittner/linuxcnc/src/gomc/generated/gmi/emcstat"
)

// Compile-time interface checks.
var _ emccmd.EmccmdCallbacks = (*milltaskModule)(nil)
var _ emcstat.EmcstatCallbacks = (*milltaskModule)(nil)

// errNotReady is returned by command handlers before Start() completes.
var errNotReady = fmt.Errorf("milltask: not ready")

// RCS status codes returned to C callers (halui, etc.)
// The C milltask returns RCS_DONE(1) on success; match that for compatibility.
const (
	rcsDone  int32 = 1 // RCS_DONE: command completed
	rcsExec  int32 = 2 // RCS_EXEC: command accepted, executing
	rcsError int32 = 3 // RCS_ERROR: command rejected
)

func (m *milltaskModule) ready() error {
	if m.task == nil || m.stopped.Load() {
		return errNotReady
	}
	return nil
}

// --- EmccmdCallbacks implementation ---

func (m *milltaskModule) SetState(state int32) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetState(state)
}

func (m *milltaskModule) SetMode(mode int32) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetMode(mode)
}

func (m *milltaskModule) AutoCmd(cmd emccmd.AutoCmd, line int32) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.AutoCommand(int32(cmd), line)
}

func (m *milltaskModule) Mdi(command string) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.MDI(command)
}

func (m *milltaskModule) Jog(jogType emccmd.JogType, jjogmode bool, axisOrJoint int32, velocity float64, distance float64) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.Jog(int32(jogType), jjogmode, axisOrJoint, velocity, distance)
}

func (m *milltaskModule) JogStop(jjogmode bool, axisOrJoint int32) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.JogStop(jjogmode, axisOrJoint)
}

func (m *milltaskModule) Spindle(cmd emccmd.SpindleCmd, speed float64, spindleNum int32, wait int32) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.Spindle(int32(cmd), speed, spindleNum, wait)
}

func (m *milltaskModule) Home(joint int32) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.Home(joint)
}

func (m *milltaskModule) Unhome(joint int32) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.Unhome(joint)
}

func (m *milltaskModule) OverrideLimits(joint int32) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.OverrideLimits(joint)
}

func (m *milltaskModule) TeleopEnable(enable bool) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.TeleopEnable(enable)
}

func (m *milltaskModule) SetFeedOverride(rate float64) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetFeedOverride(rate)
}

func (m *milltaskModule) SetSpindleOverride(rate float64, spindleNum int32) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetSpindleOverride(rate, spindleNum)
}

func (m *milltaskModule) SetRapidOverride(rate float64) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetRapidOverride(rate)
}

func (m *milltaskModule) SetMaxVelocity(velocity float64) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetMaxVelocity(velocity)
}

func (m *milltaskModule) SetFoEnable(enable bool) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetFeedOverrideEnable(enable)
}

func (m *milltaskModule) SetFhEnable(enable bool) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetFeedHoldEnable(enable)
}

func (m *milltaskModule) SetSoEnable(enable bool, spindleNum int32) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetSpindleOverrideEnable(enable, spindleNum)
}

func (m *milltaskModule) Flood(on bool) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.Flood(on)
}

func (m *milltaskModule) Mist(on bool) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.Mist(on)
}

func (m *milltaskModule) Brake(on bool, spindleNum int32) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.Brake(on, spindleNum)
}

func (m *milltaskModule) Lube(on bool) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.Lube(on)
}

func (m *milltaskModule) Abort() (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.Abort()
}

func (m *milltaskModule) TaskPlanSynch() (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.TaskPlanSynch()
}

func (m *milltaskModule) SetOptionalStop(on bool) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetOptionalStop(on)
}

func (m *milltaskModule) SetBlockDelete(on bool) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetBlockDelete(on)
}

func (m *milltaskModule) LoadToolTable(file string) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.LoadToolTable(file)
}

func (m *milltaskModule) ProgramOpen(file string) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.ProgramOpen(file)
}

func (m *milltaskModule) WaitComplete(timeout float64) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	if timeout <= 0 {
		timeout = 5.0
	}
	if err := m.task.WaitComplete(timeout); err != nil {
		return rcsError, err
	}
	return rcsDone, nil
}

func (m *milltaskModule) SetDebug(debug int32) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetDebug(debug)
}

func (m *milltaskModule) SetJogAxis(axis int32) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetJogAxis(axis)
}

func (m *milltaskModule) SetJogIncrement(increment float64) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetJogIncrement(increment)
}

func (m *milltaskModule) SetJogSpeed(speed float64) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetJogSpeed(speed)
}

func (m *milltaskModule) SetAjogSpeed(speed float64) (int32, error) {
	if err := m.ready(); err != nil {
		return rcsError, err
	}
	return rcsDone, m.task.SetAjogSpeed(speed)
}

// --- EmcstatCallbacks implementation ---

func (m *milltaskModule) GetStat() (*emcstat.StatFull, error) {
	t := m.task
	if t == nil {
		return &emcstat.StatFull{
			Task: emcstat.StatTaskInfo{
				State:       emcstat.TaskState_ESTOP,
				Mode:        emcstat.TaskMode_MANUAL,
				InterpState: emcstat.InterpState_IDLE,
				ExecState:   emcstat.ExecState_DONE,
			},
		}, nil
	}
	stat := t.BuildStat()
	return stat, nil
}
