/** @section blend6d 6D blend functions from blendmath6.h */
#include "blendmath6.h"
#include "tp_debug.h"
#include "tp_types.h"
#include "tc.h"
#include "rtapi_math.h"

int findIntersectionAngle6(Vector6 const * const u1,
        Vector6 const * const u2,
        double * const theta)
{
    double dot;
    VecVecDot(u1, u2, &dot);

    if (dot > 1.0 || dot < -1.0) {
        tp_debug_print("dot product %.16g outside domain of acos!\n",
                dot);
        sat_inplace(&dot,1.0);
    }

    *theta = acos(-dot)/2.0;
    return TP_ERR_OK;
}


/** Only need normal (not binormal) */
int blendCalculateNormals6(BlendGeom6 * const geom)
{
    VecVecSub(&geom->u2, &geom->u1, &geom->normal);
    return VecUnitEq(&geom->normal);
}


int blendFindPoints6(BlendPoints6 * const points, BlendGeom6 const * const geom,
        BlendParameters const * const param)
{
    // Find center of blend arc along normal vector
    double center_dist = param->R_plan / sin(param->theta);
    tp_debug_print("center_dist = %f\n", center_dist);

    VecScalMult(&geom->normal, center_dist, &points->arc_center);
    VecVecAddEq(&points->arc_center, &geom->P);
    tp_debug_print("arc_center:\n");
    VecPrint(&points->arc_center);

    // Start point is d_plan away from intersection P in the
    // negative direction of u1
    VecScalMult(&geom->u1, -param->d_plan, &points->arc_start);
    VecVecAddEq(&points->arc_start, &geom->P);
    tp_debug_print("arc_start:\n");
    VecPrint(&points->arc_start);

    // End point is d_plan aw.ax[0] from intersection P in the
    // positive direction of u1
    VecScalMult(&geom->u2, param->d_plan, &points->arc_end);
    VecVecAddEq(&points->arc_end, &geom->P);
    tp_debug_print("arc_end:\n");
    VecPrint(&points->arc_end);

    //For line case, just copy over d_plan since it's the same
    points->trim1 = param->d_plan;
    points->trim2 = param->d_plan;

    return TP_ERR_OK;
    return 0;
}


int blendGeom6Init(BlendGeom6 * const geom,
        TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc)
{
    if (!geom || !prev_tc || !tc) {
        return TP_ERR_FAIL;
    }
    geom->v_max1 = prev_tc->maxvel;
    geom->v_max2 = tc->maxvel;

    tcGetEndTangentUnitVector(prev_tc, &geom->u1);
    tcGetStartTangentUnitVector(tc, &geom->u2);

    // Manually extract the end of line 1 for XYZ and UVW
    // FIXME redundant
    CartToVec(&prev_tc->coords.line.xyz.end, &prev_tc->coords.line.uvw.end, &geom->P);

    // Find angle between tangent vectors
    int res_angle = findIntersectionAngle6(&geom->u1,
            &geom->u2,
            &geom->theta);

    // Test for intersection angle errors
    if(PM_PI / 2.0 - geom->theta < TP_ANGLE_EPSILON) {
        tp_debug_print("Intersection angle too close to pi/2, can't compute normal\n");
        return TP_ERR_TOLERANCE;
    }

    if(geom->theta < TP_ANGLE_EPSILON) {
        tp_debug_print("Intersection angle too small for arc fit\n");
        return TP_ERR_TOLERANCE;
    }

    blendCalculateNormals6(geom);

    //TODO
    return res_angle;
    /*return res_u1 |*/
        /*res_u2 |*/
        /*res_intersect |*/
        /*res_angle;*/
}


int blendParamKinematics6(BlendGeom6 * const geom,
        BlendParameters * const param,
        TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc,
        Vector6 const * const acc_bound,
        Vector6 const * const vel_bound,
        double maxFeedScale)
{
    // KLUDGE: common operations, but not exactly kinematics
    param->phi = (PM_PI - param->theta * 2.0);

    double nominal_tolerance;
    tcFindBlendTolerance(prev_tc, tc, &param->tolerance, &nominal_tolerance);

    // First, do the geometry heavy-lifting to find the scale factors for each
    // limit, depending on the plane connecting each line.
    Vector6 u, v;
    findOrthonormalBasis(&geom->u1, &geom->u2, &u, &v);

    Vector6 scales;
    int res_scales = calcBoundScales(&u, &v, &scales);
    
    res_scales |= calcMinBound(&scales, acc_bound, &param->a_max);

    // Store max normal acceleration
    param->a_n_max = param->a_max * BLEND_ACC_RATIO_NORMAL;
    tp_debug_print("a_max = %f, a_n_max = %f\n", param->a_max,
            param->a_n_max);

    // Find common velocity and acceleration
    param->v_req = fmax(prev_tc->reqvel, tc->reqvel);
    param->v_goal = param->v_req * maxFeedScale;

    // Calculate the maximum planar velocity
    double v_planar_max=0;
    res_scales |= calcMinBound(&scales, vel_bound, &v_planar_max);
    tp_debug_print("v_planar_max = %f\n", v_planar_max);

    // Clip the angle at a reasonable value (less than 90 deg), to prevent div by zero
    double phi_effective = fmin(param->phi, PM_PI * 0.49);

    // Copy over maximum velocities, clipping velocity to place altitude within base
    double v_max1 = fmin(prev_tc->maxvel, tc->maxvel / cos(phi_effective));
    double v_max2 = fmin(tc->maxvel, prev_tc->maxvel / cos(phi_effective));

    tp_debug_print("v_max1 = %f, v_max2 = %f\n", v_max1, v_max2);

    // Get "altitude"
    double v_area = v_max1 * v_max2 / 2.0 * sin(param->phi);
    tp_debug_print("phi = %f\n", param->phi);
    tp_debug_print("v_area = %f\n", v_area);

    // Get "base" of triangle
    Vector6 tmp1, tmp2, diff;
    VecScalMult(&geom->u1, v_max1, &tmp1);
    VecScalMult(&geom->u2, v_max2, &tmp2);
    VecVecSub(&tmp2, &tmp1, &diff);
    double base;
    VecMag(&diff, &base);
    tp_debug_print("v_base = %f\n", base);

    double v_max_alt = 2.0 * v_area / base;

    tp_debug_print("v_max_alt = %f\n", v_max_alt);
    double v_max = fmax(v_max_alt, v_planar_max);

    tp_debug_print("v_max = %f\n", v_max);
    param->v_goal = fmin(param->v_goal, v_max);

    tp_debug_print("vr1 = %f, vr2 = %f\n", prev_tc->reqvel, tc->reqvel);
    tp_debug_print("v_goal = %f, max scale = %f\n", param->v_goal, maxFeedScale);

    return res_scales;
}


int blendInit6FromLineLine(BlendGeom6 * const geom, BlendParameters * const param,
        TC_STRUCT const * const prev_tc,
        TC_STRUCT const * const tc,
        Vector6 const * const acc_bound,
        Vector6 const * const vel_bound,
        double maxFeedScale)
{
    if (tc->motion_type != TC_LINEAR || prev_tc->motion_type != TC_LINEAR) {
        return TP_ERR_FAIL;
    }

    int res_init = blendGeom6Init(geom, prev_tc, tc);
    if (res_init != TP_ERR_OK) {
        return res_init;
    }

    param->theta = geom->theta;

    tp_debug_print("theta = %f\n", param->theta);

    param->phi = (PM_PI - param->theta * 2.0);

    //TODO print details
    
    //Nominal length restriction prevents gobbling too much of parabolic blends
    param->L1 = fmin(prev_tc->target, prev_tc->nominal_length * BLEND_DIST_FRACTION);
    param->L2 = tc->target * BLEND_DIST_FRACTION;
    tp_debug_print("prev. nominal length = %f, next nominal_length = %f\n",
            prev_tc->nominal_length, tc->nominal_length);
    tp_debug_print("L1 = %f, L2 = %f\n", param->L1, param->L2);

    // Setup common parameters
    int res_kin = blendParamKinematics6(geom,
            param,
            prev_tc,
            tc,
            acc_bound,
            vel_bound,
            maxFeedScale);

    return res_kin;
}


int arcFromBlendPoints6(SphericalArc * const arc, BlendPoints6 const * const points,
        BlendGeom6 const * const geom, BlendParameters const * const param)
{
    arc->uTan = geom->u1;
    arc->line_length = param->line_length;

    return arcInitFromPoints(arc, &points->arc_start,
            &points->arc_end, &points->arc_center);
}


int findOrthonormalBasis(Vector6 const * const a,
        Vector6 const * const b,
        Vector6 * const u,
        Vector6 * const v)
{
    //TODO error check
    // Find component of a in direction of b
    // NOTE assumes both a and b are already unit
    double dot;
    VecVecDot(a, b, &dot);

    //Build the component in the direction of b
    Vector6 temp;
    VecScalMult(b, dot, &temp);

    *u = *b;
    // Subtract component of a in direction of b
    VecVecSub(a, &temp, v);
    VecUnitEq(v);

    tp_debug_print("Orthonormal u\n");
    VecPrint(u);

    tp_debug_print("Orthonormal v\n");
    VecPrint(v);

    return 0;
}


/**
 * Compute the squares of the axis bound scales.
 * TODO better explanation
 */
int calcBoundScales(Vector6 const * const u,
        Vector6 const * const v,
        Vector6 * scales)
{
    if (!u || !v || !scales) {
        return -1;
    }

    int i;
    for (i = 0; i < 6; ++i) {
        scales->ax[i] = pmSq(u->ax[i]) + pmSq(v->ax[i]);
        tp_debug_print("scale %d: %f\n", i, scales->ax[i]);
    }
    return 0;
}


int calcMinBound(Vector6 const * scales,
        Vector6 const * bounds,
        double * min_bound)
{
    if (!scales || !bounds || !min_bound) {
        return -1;
    }
    //TODO check inputs
    //Square of max accel bound
    // KLUDGE use "big" number in place of INFINITY since it doesn't seem to
    // build in RTAI
    double m2 = 1e300;

    int i;
    for (i = 0; i < 6; ++i) {
        // Crude numerical cutoff to prevent scales being too low and causing division errors
        double s = fmax(scales->ax[i], TP_POS_EPSILON);
        double b = bounds->ax[i];
        if (s > 0.0 && b > 0.0) {
            //Have to square b here since the scale is also squared
            m2 = fmin(m2, pmSq(b) / s);
        }
    }
    // One square root at the end to get the actual max accel
    *min_bound = pmSqrt(m2);
    return 0;

}

