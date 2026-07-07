// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
// License: GPL Version 2
package task

import "math"

// cartFuzz is the minimum per-axis displacement that counts as motion, matching
// CART_FUZZ (posemath.h) used by the C++ canon's applyMinDisplacement.
const cartFuzz = 1.0e-8

// Axis index groups: linear axes X,Y,Z,U,V,W and angular axes A,B,C.
var linearAxes = [...]int{0, 1, 2, 6, 7, 8}
var angularAxes = [...]int{3, 4, 5}

// moveDeltas returns the per-axis absolute displacement (mm / deg) from `from`
// to `to`, zeroing inactive axes and sub-fuzz motion — the Go port of the C++
// applyMinDisplacement step. It also reports whether the move has any linear
// (cartesian) and/or angular component, matching canon.cartesian_move /
// canon.angular_move.
func (t *Task) moveDeltas(from, to Pose) (d [9]float64, cartesian, angular bool) {
	d = [9]float64{
		math.Abs(to.X - from.X), math.Abs(to.Y - from.Y), math.Abs(to.Z - from.Z),
		math.Abs(to.A - from.A), math.Abs(to.B - from.B), math.Abs(to.C - from.C),
		math.Abs(to.U - from.U), math.Abs(to.V - from.V), math.Abs(to.W - from.W),
	}
	for i := 0; i < 9; i++ {
		if t.axisMask&(1<<uint(i)) == 0 || d[i] < cartFuzz {
			d[i] = 0
		}
	}
	cartesian = d[0] > 0 || d[1] > 0 || d[2] > 0 || d[6] > 0 || d[7] > 0 || d[8] > 0
	angular = d[3] > 0 || d[4] > 0 || d[5] > 0
	return
}

// blendLimit computes the coordinated limit (velocity or acceleration) for a
// move with per-axis displacements `d` and per-axis maxima `max`, following the
// C++ getStraightVelocity/getStraightAcceleration logic:
//
//	t[i] = d[i]/max[i];  tmax = max over participating axes;
//	dtot = |xyz| (or |uvw| if no xyz, or |abc| for a pure angular move);
//	limit = dtot / tmax
//
// so no single axis exceeds its own maximum. Returns 0 for a move to nowhere
// (the caller substitutes the programmed feed rate, as the C++ canon does).
func blendLimit(d [9]float64, max [9]float64, cartesian, angular bool) float64 {
	tmax := 0.0
	tAxis := func(i int) {
		if d[i] > 0 && max[i] > 0 {
			if ti := d[i] / max[i]; ti > tmax {
				tmax = ti
			}
		}
	}
	var dtot float64
	xyz := math.Sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2])
	uvw := math.Sqrt(d[6]*d[6] + d[7]*d[7] + d[8]*d[8])

	switch {
	case cartesian && !angular:
		for _, i := range linearAxes {
			tAxis(i)
		}
		if d[0] > 0 || d[1] > 0 || d[2] > 0 {
			dtot = xyz
		} else {
			dtot = uvw
		}
	case !cartesian && angular:
		for _, i := range angularAxes {
			tAxis(i)
		}
		dtot = math.Sqrt(d[3]*d[3] + d[4]*d[4] + d[5]*d[5])
	case cartesian && angular:
		// NIST IR6556 2.1.2.5(A): coordinate like a linear move, letting the
		// angular axes take the same time as the linear ones.
		for _, i := range linearAxes {
			tAxis(i)
		}
		for _, i := range angularAxes {
			tAxis(i)
		}
		if d[0] > 0 || d[1] > 0 || d[2] > 0 {
			dtot = xyz
		} else {
			dtot = uvw
		}
	}
	if tmax <= 0 {
		return 0
	}
	return dtot / tmax
}

// straightLimits returns the per-axis-blended maximum velocity and acceleration
// for a straight move from `from` to `to` (absolute mm / deg), plus the move
// classification. A zero velocity/acceleration means "move to nowhere" — the
// caller falls back to the programmed feed rate, matching the C++ canon.
func (t *Task) straightLimits(from, to Pose) (velMax, accMax float64, cartesian, angular bool) {
	d, cartesian, angular := t.moveDeltas(from, to)
	velMax = blendLimit(d, t.axisMaxVel, cartesian, angular)
	accMax = blendLimit(d, t.axisMaxAcc, cartesian, angular)
	return
}
