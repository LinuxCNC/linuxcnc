// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import (
	"math"
	"testing"
)

// Per-axis limits used by the parity harness config (tests/milltask-parity):
// X: 40/600, Y: 25/400, Z: 8/120 — deliberately non-uniform so the blend is
// visible. Expected values below were hand-derived from the C++ canon
// getStraightVelocity/getStraightAcceleration and confirmed byte-for-byte
// against the old milltask via the differential oracle.
func newBlendTestTask() *Task {
	t := &Task{axisMask: 0b111} // X|Y|Z
	t.axisMaxVel = [9]float64{40, 25, 8}
	t.axisMaxAcc = [9]float64{600, 400, 120}
	return t
}

func p(x, y, z float64) Pose { return Pose{X: x, Y: y, Z: z} }

func TestStraightLimits_PerAxisBlend(t *testing.T) {
	task := newBlendTestTask()
	const eps = 1e-6
	cases := []struct {
		name             string
		from, to         Pose
		wantVel, wantAcc float64
	}{
		{"pure-Z (slow axis limits)", p(0, 0, 5), p(0, 0, 0), 8, 120},
		{"pure-X (fast axis)", p(0, 0, 0), p(20, 0, 0), 40, 600},
		{"pure-Y", p(20, 0, 0), p(20, 20, 0), 25, 400},
		{"diagonal XY (sqrt2 blend)", p(20, 20, 0), p(0, 0, 0), 35.35533906, 565.6854249},
		{"full XYZ (Z-dominated)", p(0, 0, 0), p(30, 15, -10), 28, 420},
	}
	for _, c := range cases {
		velMax, accMax, cart, ang := task.straightLimits(c.from, c.to)
		if math.Abs(velMax-c.wantVel) > eps {
			t.Errorf("%s: velMax=%.6f want %.6f", c.name, velMax, c.wantVel)
		}
		if math.Abs(accMax-c.wantAcc) > eps {
			t.Errorf("%s: accMax=%.6f want %.6f", c.name, accMax, c.wantAcc)
		}
		if !cart || ang {
			t.Errorf("%s: classification cart=%v ang=%v, want cart=true ang=false", c.name, cart, ang)
		}
	}
}

// A move to nowhere yields zero (caller falls back to the programmed feed).
func TestStraightLimits_MoveToNowhere(t *testing.T) {
	task := newBlendTestTask()
	velMax, accMax, cart, ang := task.straightLimits(p(1, 2, 3), p(1, 2, 3))
	if velMax != 0 || accMax != 0 || cart || ang {
		t.Errorf("move to nowhere: got vel=%v acc=%v cart=%v ang=%v, want all zero/false", velMax, accMax, cart, ang)
	}
}

// Arc velocity: per-axis planar limit, centripetal cap sqrt(a_norm*r), and the
// helical/aux-axis traversal time. Expected values hand-derived from the C++
// ARC_FEED and confirmed byte-for-byte against the old milltask.
func TestArcLimits(t *testing.T) {
	task := newBlendTestTask() // X:40/600 Y:25/400 Z:8/120
	const feed = 800.0 / 60.0  // F800 -> 13.333 mm/s
	const eps = 1e-3
	cases := []struct {
		name                          string
		from, to                      Pose
		center                        Cartesian
		rotation                      int32
		wantVel, wantIniMaxVel, wantAcc float64
	}{
		{"semicircle r=10 (planar-axis limited)",
			p(0, 0, 0), p(20, 0, 0), Cartesian{X: 10}, -1, 13.3333, 25, 400},
		{"small arc r=1 (centripetal limited)",
			p(0, 0, 0), p(2, 0, 0), Cartesian{X: 1}, -1, 13.3333, 18.6121, 400},
		{"helical arc r=1 + Z-5 (helical time limited)",
			p(2, 0, 0), p(0, 0, -5), Cartesian{X: 1, Z: -5}, -1, 9.4480, 9.4480, 141.72},
	}
	for _, c := range cases {
		vel, iniMaxVel, acc := task.arcLimits(c.from, c.to, c.center, CanonPlaneXY, c.rotation, feed)
		if math.Abs(vel-c.wantVel) > eps {
			t.Errorf("%s: vel=%.4f want %.4f", c.name, vel, c.wantVel)
		}
		if math.Abs(iniMaxVel-c.wantIniMaxVel) > eps {
			t.Errorf("%s: ini_maxvel=%.4f want %.4f", c.name, iniMaxVel, c.wantIniMaxVel)
		}
		if math.Abs(acc-c.wantAcc) > 1e-2 {
			t.Errorf("%s: acc=%.4f want %.4f", c.name, acc, c.wantAcc)
		}
	}
}

// A pure-angular move blends against the angular-axis limits and is classified
// angular (so the caller clamps to angularFeedRate, not linearFeedRate).
func TestStraightLimits_PureAngular(t *testing.T) {
	task := &Task{axisMask: 0b111 | (1 << 3)} // X|Y|Z|A
	task.axisMaxVel = [9]float64{40, 25, 8, 30}
	task.axisMaxAcc = [9]float64{600, 400, 120, 500}
	velMax, accMax, cart, ang := task.straightLimits(Pose{}, Pose{A: 90})
	if cart || !ang {
		t.Fatalf("classification cart=%v ang=%v, want cart=false ang=true", cart, ang)
	}
	// da=90, tmax=90/30=3, dtot=90 => vel=30; acc: tmax=90/500, dtot=90 => 500.
	if math.Abs(velMax-30) > 1e-6 || math.Abs(accMax-500) > 1e-6 {
		t.Errorf("angular: vel=%.6f acc=%.6f, want 30 / 500", velMax, accMax)
	}
}
