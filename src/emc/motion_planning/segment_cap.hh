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
#include <posemath.h>
#include <kinematics_user.h>

namespace motion_planning {

/* The joint limits the cap is read against, in the units motion commands
   the joints in: [JOINT_n] MAX_VELOCITY and MAX_ACCELERATION, and the
   travel between MIN_LIMIT and MAX_LIMIT, -HUGE_VAL and HUGE_VAL for a
   joint whose travel is not to be followed. */
struct SegmentCapLimits {
    int joints;
    double vel[KINEMATICS_USER_MAX_JOINTS];
    double acc[KINEMATICS_USER_MAX_JOINTS];
    double min[KINEMATICS_USER_MAX_JOINTS];
    double max[KINEMATICS_USER_MAX_JOINTS];
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
    int travel_joint;   /* the first joint found leaving its travel along the segment, -1 for none */
    int travel_side;    /* 1 past its MAX_LIMIT, -1 past its MIN_LIMIT */
    double travel_at;   /* the fraction of the segment it is found at */
    double travel_pos;  /* where the joint is there */
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

   The joints are followed against their travel along the same samples: a
   sample past a limit, or, between two samples, the peak of the cubic
   through their positions and rates, read again through the inverse at
   the peak and found past the limit there, is reported, the first one
   along the segment.  Where the cubic puts a peak near the limit without
   the inverse confirming it, the stretch is split at the peak and each
   half looked at the same way, a bounded number of times.  Only a point
   the inverse itself puts past a limit is reported, and not on a side of
   the travel the joint starts the segment past, nor beyond a sample the
   module cannot invert or a joint moving between two points further than
   its rates allow, as where the inverse changes branch: past those the
   segment is not known well enough to be refused.

   length is the segment's extent in the path parameter, in the units the
   caps come back in.  joints seeds the inverse at the start and comes back
   holding the joints at the end, so the next segment starts on the same
   solution branch.  Returns 0, or -1 where no sample could be evaluated,
   the caps then NONE and no joint found leaving its travel. */
int segmentCap(KinematicsUserContext *ctx, const SegmentCapLimits *lim,
               double length, int samples, SegmentPoseFn pose_at, void *arg,
               double *joints, SegmentCap *out);

/* A line between two poses, every axis interpolated, for segmentLinePose. */
struct SegmentLine {
    EmcPose start, end;
};

void segmentLinePose(double fraction, EmcPose *pose, void *arg);

/* An arc: the circle in its own units, scale times them the units of the
   poses, which give the axes off the circle; a line, no circle at all, is
   the chord.  For segmentArcPose. */
struct SegmentArc {
    SegmentLine ends;
    PmCircle circle;
    double scale;
    bool line;
};

void segmentArcPose(double fraction, EmcPose *pose, void *arg);

/* How many points to sample a segment at: enough that a turn of the
   rotaries, a stretch of travel on a machine whose Jacobian changes with
   position, or a sweep of arc is seen every few degrees or centimetres.
   angular has bit n set for an axis n (0 X to 8 W) that turns. */
int segmentSamples(const EmcPose *start, const EmcPose *end, double sweep_deg,
                   unsigned angular);

} // namespace motion_planning

#endif // SEGMENT_CAP_HH
