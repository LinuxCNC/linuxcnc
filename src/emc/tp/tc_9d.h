/********************************************************************
* Description: tc_9d.h
*   9D trajectory component helper functions
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
#ifndef TC_9D_H
#define TC_9D_H

#include "posemath.h"
#include "tc_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compute length of a 9D line segment
 *
 * Returns the Euclidean distance considering all 9 axes:
 * XYZ + ABC + UVW
 *
 * @param line9 9D line structure
 * @return Total length
 */
double pmLine9Length(PmLine9 const * const line9);

/**
 * @brief Compute length of a 9D circle/arc segment
 *
 * For circular motion, this includes the arc length in the XYZ plane
 * plus linear motion in ABC and UVW.
 *
 * @param circle9 9D circle structure
 * @return Total length
 */
double pmCircle9Length(PmCircle9 const * const circle9);

/**
 * @brief Get tangential maximum acceleration for 9D segment
 *
 * Computes the maximum acceleration in the direction of motion,
 * considering all 9 axis limits and the geometry.
 *
 * @param tc Trajectory component
 * @return Maximum tangential acceleration
 */
double tcGetTangentialMaxAccel_9D(TC_STRUCT const * const tc);

/**
 * @brief Get maximum velocity considering 9D axis limits
 *
 * Applies velocity limits for all 9 axes based on motion direction.
 *
 * @param tc Trajectory component
 * @param max_feed_scale Feed override scale factor
 * @return Maximum velocity
 */
double tcGetMaxVel_9D(TC_STRUCT const * const tc, double max_feed_scale);

#ifdef __cplusplus
}
#endif

#endif // TC_9D_H
