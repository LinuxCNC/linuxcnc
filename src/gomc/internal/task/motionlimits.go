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
// so no single axis exceeds its own maximum. It also returns tmax (the limiting
// per-axis time), used by the arc velocity computation. Returns 0 for a move to
// nowhere (the caller substitutes the programmed feed rate, as the C++ canon
// does).
func blendLimit(d [9]float64, max [9]float64, cartesian, angular bool) (limit, tmax float64) {
	tmax = 0.0
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
		return 0, 0
	}
	return dtot / tmax, tmax
}

// straightLimits returns the per-axis-blended maximum velocity and acceleration
// for a straight move from `from` to `to` (absolute mm / deg), plus the move
// classification. A zero velocity/acceleration means "move to nowhere" — the
// caller falls back to the programmed feed rate, matching the C++ canon.
func (t *Task) straightLimits(from, to Pose) (velMax, accMax float64, cartesian, angular bool) {
	d, cartesian, angular := t.moveDeltas(from, to)
	velMax, _ = blendLimit(d, t.axisMaxVel, cartesian, angular)
	accMax, _ = blendLimit(d, t.axisMaxAcc, cartesian, angular)
	return
}

// arcLimits computes the commanded velocity, per-axis + centripetal-limited
// max velocity (ini_maxvel), and acceleration for a circular/helical arc from
// `from` to `to` about `center` (all absolute mm) in the given plane, porting
// the C++ ARC_FEED velocity computation. `rotation` is the interpreter's raw
// rotation count (sign = direction, |n|-1 = extra full turns); `linearFeed` is
// the programmed feed rate (mm/s). The velocity is bounded by (a) the planar
// axes' own limits, (b) the centripetal limit sqrt(a_norm*r), and (c) the time
// the helical/auxiliary axes need for their straight displacement.
func (t *Task) arcLimits(from, to Pose, center Cartesian, plane, rotation int32, linearFeed float64) (vel, iniMaxVel, acc float64) {
	// Planar axis pair (i1,i2) and normal axis, right-handed (normal = i1×i2),
	// matching the C++ circshift-based plane selection.
	var i1, i2, naxis int
	switch plane {
	case CanonPlaneYZ:
		i1, i2, naxis = 1, 2, 0
	case CanonPlaneXZ:
		i1, i2, naxis = 2, 0, 1
	default: // XY
		i1, i2, naxis = 0, 1, 2
	}
	fromA := [3]float64{from.X, from.Y, from.Z}
	toA := [3]float64{to.X, to.Y, to.Z}
	cen := [3]float64{center.X, center.Y, center.Z}

	// Planar projections of start/end relative to the center → radii + angles.
	pS1, pS2 := fromA[i1]-cen[i1], fromA[i2]-cen[i2]
	pE1, pE2 := toA[i1]-cen[i1], toA[i2]-cen[i2]
	thetaStart := math.Atan2(pS2, pS1)
	thetaEnd := math.Atan2(pE2, pE1)
	startRadius := math.Hypot(pS1, pS2)
	endRadius := math.Hypot(pE1, pE2)

	const minArcAngle = 1e-12
	if rotation < 0 { // clockwise
		if thetaEnd+minArcAngle >= thetaStart {
			thetaEnd -= 2 * math.Pi
		}
	} else {
		if thetaEnd-minArcAngle <= thetaStart {
			thetaEnd += 2 * math.Pi
		}
	}
	fullTurns := 0
	if rotation > 1 {
		fullTurns = int(rotation) - 1
	} else if rotation < -1 {
		fullTurns = int(rotation) + 1
	}
	fullAngle := (thetaEnd - thetaStart) + 2*math.Pi*float64(fullTurns)

	// Spiral (radius change over the arc) and effective radius.
	spiral := endRadius - startRadius
	minRadius := math.Min(startRadius, endRadius)
	dr := 0.0
	if fullAngle != 0 {
		dr = spiral / math.Abs(fullAngle)
	}
	effectiveRadius := math.Sqrt(dr*dr + minRadius*minRadius)

	// Planar velocity/accel bounds and the centripetal cap.
	vMaxAxes := math.Min(t.axisMaxVel[i1], t.axisMaxVel[i2])
	aMaxAxes := math.Min(t.axisMaxAcc[i1], t.axisMaxAcc[i2])
	aMaxNormal := aMaxAxes * math.Sqrt(3) / 2
	vMaxPlanar := math.Min(math.Sqrt(aMaxNormal*effectiveRadius), vMaxAxes)

	// Time the helical/auxiliary axes need (straight displacement to the end).
	d, cart, ang := t.moveDeltas(from, to)
	_, tMaxMotionVel := blendLimit(d, t.axisMaxVel, cart, ang)
	_, tMaxMotionAcc := blendLimit(d, t.axisMaxAcc, cart, ang)

	// Arc / spiral lengths and total 3D path length (incl. the normal axis).
	circularLength := minRadius * math.Abs(fullAngle)
	spiralLength := math.Hypot(circularLength, spiral)
	axisLen := toA[naxis] - fromA[naxis]
	totalLength := math.Hypot(spiralLength, axisLen)

	vMax := linearFeed
	if vMaxPlanar > 0 {
		tMax := math.Max(tMaxMotionVel, spiralLength/vMaxPlanar)
		if tMax > 0 {
			vMax = totalLength / tMax
		}
	}
	aMax := t.maxAcceleration
	if aMaxAxes > 0 {
		ttMax := math.Max(tMaxMotionAcc, spiralLength/aMaxAxes)
		if ttMax > 0 {
			aMax = totalLength / ttMax
		}
	}

	vel = linearFeed
	if vMax < vel {
		vel = vMax
	}
	return vel, vMax, aMax
}
