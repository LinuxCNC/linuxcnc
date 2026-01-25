/********************************************************************
* Description: tc_9d.c
*   9D trajectory component helper functions implementation
*
*   Provides 9-axis specific calculations for length, acceleration,
*   and velocity limits.
*
* Author: LinuxCNC community (ported from Tormach)
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2022-2026 All rights reserved.
*
********************************************************************/

#include "tc_9d.h"
#include "posemath.h"
#include "rtapi_math.h"

/**
 * @brief Compute length of a 9D line segment
 *
 * LinuxCNC's PmLine9 has separate xyz, abc, uvw components,
 * each of which is a PmCartLine.
 */
double pmLine9Length(PmLine9 const * const line9)
{
    if (!line9) return 0.0;

    double len_xyz = 0.0, len_abc = 0.0, len_uvw = 0.0;

    // Get length of each component
    // Each PmCartLine has .tmag field for magnitude
    len_xyz = line9->xyz.tmag;
    len_abc = line9->abc.tmag;
    len_uvw = line9->uvw.tmag;

    // Euclidean combination of all axes
    double total_length = sqrt(len_xyz * len_xyz +
                               len_abc * len_abc +
                               len_uvw * len_uvw);

    return total_length;
}

/**
 * @brief Compute length of a 9D circle/arc segment
 *
 * For circular motion:
 * - XYZ follows an arc (use fit or circle arc length)
 * - ABC and UVW are linear motions
 */
double pmCircle9Length(PmCircle9 const * const circle9)
{
    if (!circle9) return 0.0;

    // For circles, we need the arc length in XYZ plane
    // plus linear length for ABC and UVW

    double arc_length = 0.0;

    // Get arc length from circle
    // PmCircle has radius and angle members directly
    arc_length = fabs(circle9->xyz.radius * circle9->xyz.angle);

    // Get linear lengths for rotary and auxiliary axes
    double len_abc = circle9->abc.tmag;
    double len_uvw = circle9->uvw.tmag;

    // Total length is Euclidean combination
    // (arc length in XYZ + linear in ABC/UVW)
    double total_length = sqrt(arc_length * arc_length +
                               len_abc * len_abc +
                               len_uvw * len_uvw);

    return total_length;
}

/**
 * @brief Get tangential maximum acceleration for 9D segment
 *
 * This is a simplified implementation. For full 9D support,
 * should consider acceleration limits in all 9 axes and
 * compute tangential component based on motion direction.
 */
double tcGetTangentialMaxAccel_9D(TC_STRUCT const * const tc)
{
    if (!tc) return 0.0;

    // For now, return the configured max acceleration
    // TODO: Implement proper 9D acceleration limit calculation
    // considering all axis limits and motion geometry

    return tc->maxaccel;
}

/**
 * @brief Get maximum velocity considering 9D axis limits
 *
 * Applies velocity limits for all 9 axes based on motion direction.
 */
double tcGetMaxVel_9D(TC_STRUCT const * const tc, double max_feed_scale)
{
    if (!tc) return 0.0;

    // Start with requested velocity
    double vel = tc->reqvel;

    // Apply feed override
    vel *= max_feed_scale;

    // Clamp to machine velocity limit
    if (tc->maxvel > 0.0 && vel > tc->maxvel) {
        vel = tc->maxvel;
    }

    // TODO: For full 9D support, should check individual axis
    // velocity limits based on motion direction unit vector
    // and apply the most restrictive limit

    return vel;
}
