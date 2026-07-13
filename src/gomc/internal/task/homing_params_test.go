// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"testing"

	"github.com/sittner/linuxcnc/src/gomc/pkg/inifile"
)

// noopMotionConfig is a do-nothing MotionConfig for exercising loadJoint.
type noopMotionConfig struct{}

func (noopMotionConfig) SetVel(float64) error                                 { return nil }
func (noopMotionConfig) SetVelLimit(float64) error                            { return nil }
func (noopMotionConfig) SetAcc(float64) error                                 { return nil }
func (noopMotionConfig) SetMaxFeedOverride(float64) error                     { return nil }
func (noopMotionConfig) SetWorldHome(Pose) error                              { return nil }
func (noopMotionConfig) SetProbeErrInhibit(int32, int32) error                { return nil }
func (noopMotionConfig) JointActivate(int32) error                            { return nil }
func (noopMotionConfig) SetJointPositionLimits(int32, float64, float64) error { return nil }
func (noopMotionConfig) SetJointBacklash(int32, float64) error                { return nil }
func (noopMotionConfig) SetJointMaxFerror(int32, float64) error               { return nil }
func (noopMotionConfig) SetJointMinFerror(int32, float64) error               { return nil }
func (noopMotionConfig) SetJointVelLimit(int32, float64) error                { return nil }
func (noopMotionConfig) SetJointAccLimit(int32, float64) error                { return nil }
func (noopMotionConfig) SetJointJerkLimit(int32, float64) error               { return nil }
func (noopMotionConfig) SetJointHomingParams(int32, float64, float64, float64, float64, float64, int32, int32, int32) error {
	return nil
}
func (noopMotionConfig) SetJointComp(int32, float64, float64, float64) error { return nil }
func (noopMotionConfig) SetAxisPositionLimits(int32, float64, float64) error { return nil }
func (noopMotionConfig) SetAxisVelLimit(int32, float64, float64) error       { return nil }
func (noopMotionConfig) SetAxisAccLimit(int32, float64, float64) error       { return nil }
func (noopMotionConfig) SetAxisLockingJoint(int32, int32) error              { return nil }
func (noopMotionConfig) SetSpindleParams(int32, float64, float64, float64, float64, float64, int32, float64) error {
	return nil
}

// loadJoint must cache the INI-fixed homing params so a later runtime HAL
// home/offset/seq change (inihal) can re-push them instead of zeroing them.
func TestLoadJoint_CachesHomingParams(t *testing.T) {
	ini, err := inifile.ParseString(`[JOINT_0]
MIN_LIMIT=-10
MAX_LIMIT=10
MAX_VELOCITY=5
MAX_ACCELERATION=50
HOME=1.0
HOME_OFFSET=0.5
HOME_SEARCH_VEL=2.0
HOME_LATCH_VEL=0.5
HOME_FINAL_VEL=3.0
HOME_SEQUENCE=2
HOME_IGNORE_LIMITS=1
VOLATILE_HOME=1
`)
	if err != nil {
		t.Fatalf("parse ini: %v", err)
	}

	task := &Task{}
	if err := loadJoint(ini, task, 0, noopMotionConfig{}); err != nil {
		t.Fatalf("loadJoint: %v", err)
	}

	hp := task.jointHoming[0]
	if hp.finalVel != 3.0 || hp.searchVel != 2.0 || hp.latchVel != 0.5 {
		t.Errorf("cached vels = %+v, want finalVel=3 searchVel=2 latchVel=0.5", hp)
	}
	if hp.flags&1 == 0 { // HOME_IGNORE_LIMITS = 1
		t.Errorf("cached flags = %d, want HOME_IGNORE_LIMITS bit set", hp.flags)
	}
	if hp.volatileHome != 1 {
		t.Errorf("cached volatileHome = %d, want 1", hp.volatileHome)
	}
}
