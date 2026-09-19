/********************************************************************
 * Description: segment_cap.cc
 *   The velocity and acceleration cap a segment gets from the joint
 *   limits through the kinematics module's Jacobian.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2026 All rights reserved.
 ********************************************************************/

#include "segment_cap.hh"
#include <cmath>

namespace motion_planning {

static double pose_axis(const EmcPose &p, int ax)
{
    switch (ax) {
    case AXIS_X: return p.tran.x;
    case AXIS_Y: return p.tran.y;
    case AXIS_Z: return p.tran.z;
    case AXIS_A: return p.a;
    case AXIS_B: return p.b;
    case AXIS_C: return p.c;
    case AXIS_U: return p.u;
    case AXIS_V: return p.v;
    default:     return p.w;
    }
}

int segmentCap(KinematicsUserContext *ctx, const SegmentCapLimits *lim,
               double length, int samples, SegmentPoseFn pose_at, void *arg,
               double *joints, SegmentCap *out)
{
    const double tiny = 1e-12;
    /* the rate of every joint per unit of path parameter at each sample,
       the most that rate changes per unit of parameter on either side of
       it, and the most it can bulge past the samples on either side */
    static double g[SEGMENT_CAP_MAX_SAMPLES][KINEMATICS_USER_MAX_JOINTS];
    static double h[SEGMENT_CAP_MAX_SAMPLES][KINEMATICS_USER_MAX_JOINTS];
    static double bulge[SEGMENT_CAP_MAX_SAMPLES][KINEMATICS_USER_MAX_JOINTS];
    static double s[SEGMENT_CAP_MAX_SAMPLES];
    static bool ok[SEGMENT_CAP_MAX_SAMPLES];
    int K = samples < 2 ? 2 : samples;
    int njoints;
    int k, j, a, prev, answered = 0;

    out->vel = SEGMENT_CAP_NONE;
    out->acc = SEGMENT_CAP_NONE;
    out->vel_joint = -1;
    out->acc_joint = -1;
    out->vel_at = 0.0;
    out->samples = 0;
    out->unanswered = 0;
    if (!ctx || !lim || !pose_at || !joints || length <= 0.0) { return -1; }
    if (K > SEGMENT_CAP_MAX_SAMPLES) { K = SEGMENT_CAP_MAX_SAMPLES; }
    njoints = lim->joints;
    if (njoints > KINEMATICS_USER_MAX_JOINTS) { njoints = KINEMATICS_USER_MAX_JOINTS; }
    out->samples = K;

    for (k = 0; k < K; k++) {
        double f = (double)k / (double)(K - 1);
        /* the tangent by a central difference of the geometry, exact for
           a line and a small fraction of a sample step for an arc */
        double df = 1.0 / (8.0 * (K - 1));
        double fp = f + df > 1.0 ? 1.0 : f + df;
        double fm = f - df < 0.0 ? 0.0 : f - df;
        EmcPose p, pp, pm;
        double t[AXIS_COUNT];
        double J[KINEMATICS_USER_MAX_JOINTS][AXIS_COUNT];

        s[k] = f * length;
        ok[k] = false;
        pose_at(f, &p, arg);
        pose_at(fp, &pp, arg);
        pose_at(fm, &pm, arg);
        for (a = 0; a < AXIS_COUNT; a++) {
            t[a] = (pose_axis(pp, a) - pose_axis(pm, a)) / ((fp - fm) * length);
        }
        /* the joints at the sample, seeded from the sample before so the
           whole segment stays on one solution branch, then the Jacobian
           on that branch */
        if (kinematicsUserInverse(ctx, &p, joints) != 0
            || kinematicsUserJacobian(ctx, &p, J) != 0) {
            out->unanswered++;
            continue;
        }
        for (j = 0; j < njoints; j++) {
            g[k][j] = 0.0;
            h[k][j] = 0.0;
            bulge[k][j] = 0.0;
            for (a = 0; a < AXIS_COUNT; a++) { g[k][j] += J[j][a] * t[a]; }
        }
        ok[k] = true;
        answered++;
    }
    if (!answered) { return -1; }

    /* how far the rate can rise between two samples: the parabola
       through three in a row bulges by an eighth of its second
       difference over its middle, and a peak between samples is
       covered by charging that to all three */
    for (k = 1; k + 1 < K; k++) {
        if (!ok[k - 1] || !ok[k] || !ok[k + 1]) { continue; }
        for (j = 0; j < njoints; j++) {
            double b = fabs(g[k + 1][j] - 2.0 * g[k][j] + g[k - 1][j]) / 8.0;
            if (b > bulge[k][j]) { bulge[k][j] = b; }
            if (b > bulge[k - 1][j]) { bulge[k - 1][j] = b; }
            if (b > bulge[k + 1][j]) { bulge[k + 1][j] = b; }
        }
    }

    /* the joint velocity limits */
    for (k = 0; k < K; k++) {
        if (!ok[k]) { continue; }
        for (j = 0; j < njoints; j++) {
            double rate = fabs(g[k][j]) + bulge[k][j];
            if (rate > tiny && lim->vel[j] / rate < out->vel) {
                out->vel = lim->vel[j] / rate;
                out->vel_joint = j;
                out->vel_at = s[k] / length;
            }
        }
    }

    /* how the rates change along the segment, read between neighbours
       and charged to both */
    for (prev = -1, k = 0; k < K; k++) {
        if (!ok[k]) { continue; }
        if (prev >= 0) {
            for (j = 0; j < njoints; j++) {
                double dh = fabs(g[k][j] - g[prev][j]) / (s[k] - s[prev]);
                if (dh > h[k][j]) { h[k][j] = dh; }
                if (dh > h[prev][j]) { h[prev][j] = dh; }
            }
        }
        prev = k;
    }

    /* the changing rate at the capped velocity takes at most half a
       joint's acceleration budget */
    for (k = 0; k < K; k++) {
        if (!ok[k]) { continue; }
        for (j = 0; j < njoints; j++) {
            if (h[k][j] > tiny) {
                double cap = sqrt(lim->acc[j] / (2.0 * h[k][j]));
                if (cap < out->vel) {
                    out->vel = cap;
                    out->vel_joint = j;
                    out->vel_at = s[k] / length;
                }
            }
        }
    }

    /* the joint acceleration limits, less what the changing rate takes */
    for (k = 0; k < K; k++) {
        if (!ok[k]) { continue; }
        for (j = 0; j < njoints; j++) {
            double rate = fabs(g[k][j]) + bulge[k][j];
            if (rate > tiny) {
                double cap = (lim->acc[j] - h[k][j] * out->vel * out->vel) / rate;
                if (cap < out->acc) {
                    out->acc = cap;
                    out->acc_joint = j;
                }
            }
        }
    }
    return 0;
} // segmentCap()

} // namespace motion_planning
