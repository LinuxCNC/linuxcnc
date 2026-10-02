/********************************************************************
 * Description: segment_cap.hh
 *   The velocity and acceleration a segment can be run at without
 *   asking any joint for more than its own limits, read off the
 *   kinematics module's Jacobian along the segment.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2026 All rights reserved.
 ********************************************************************/
#ifndef SEGMENT_CAP_HH
#define SEGMENT_CAP_HH

#include <emcpos.h>
#include <kinematics_user.h>

namespace motion_planning {

/* The joint limits the cap is read against, in the units motion commands
   the joints in: [JOINT_n] MAX_VELOCITY and MAX_ACCELERATION. */
struct SegmentCapLimits {
    int joints;
    double vel[KINEMATICS_USER_MAX_JOINTS];
    double acc[KINEMATICS_USER_MAX_JOINTS];
};

/* No joint binds. */
#define SEGMENT_CAP_NONE 1e9

/* The most a segment was sampled at. */
#define SEGMENT_CAP_MAX_SAMPLES 129

/* What the joints allow along the segment: the fastest the path parameter
   may advance, and the hardest it may accelerate, per second. */
struct SegmentCap {
    double vel;
    double acc;
    int vel_joint;      /* the joint setting each, -1 for none */
    int acc_joint;
    double vel_at;      /* the fraction of the segment the velocity cap is read at */
    int samples;        /* points the segment was sampled at */
    int unanswered;     /* of them, the ones the module could not invert or differentiate */
};

/* The pose at a fraction of the segment, 0 the start and 1 the end, in
   the units the module takes, which are motion's.  A line interpolates,
   an arc walks its circle. */
typedef void (*SegmentPoseFn)(double fraction, EmcPose *pose, void *arg);

/* Sample the segment at `samples` points, the ends among them, and read
   at each the rate every joint moves at per unit of path parameter: the
   module's Jacobian along the local tangent.  Between neighbouring samples
   read how that rate changes with the parameter, and from three in a row
   how much it can bulge between two of them, which is added to the rate so
   that a peak falling between samples is covered.  A joint's velocity
   limit then caps the path velocity, and its acceleration limit caps the
   path acceleration once the share the changing rate takes at the capped
   velocity (what a carriage on a circle feels as centripetal) is set
   aside: at most half the joint's budget goes to it, and the velocity is
   lowered where it would take more.

   length is the segment's extent in the path parameter, in the units the
   caps come back in.  joints seeds the inverse at the start and comes back
   holding the joints at the end, so the next segment starts on the same
   solution branch.  Returns 0, or -1 where no sample could be evaluated,
   the caps then NONE. */
int segmentCap(KinematicsUserContext *ctx, const SegmentCapLimits *lim,
               double length, int samples, SegmentPoseFn pose_at, void *arg,
               double *joints, SegmentCap *out);

} // namespace motion_planning

#endif // SEGMENT_CAP_HH
