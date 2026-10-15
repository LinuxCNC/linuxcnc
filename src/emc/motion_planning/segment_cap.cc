/********************************************************************
 * Description: segment_cap.cc
 *   The velocity and acceleration cap a segment gets from the joint
 *   limits through the kinematics module's Jacobian, and the first
 *   place along it a joint would leave its travel.
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

/* at each sample: the joints, the rate of every joint per unit of path
   parameter, the path parameter, and whether the module answered there */
static double q[SEGMENT_CAP_MAX_SAMPLES][KINEMATICS_USER_MAX_JOINTS];
static double g[SEGMENT_CAP_MAX_SAMPLES][KINEMATICS_USER_MAX_JOINTS];
static double s[SEGMENT_CAP_MAX_SAMPLES];
static bool ok[SEGMENT_CAP_MAX_SAMPLES];

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

/* The joints at fraction f of a segment sampled at K points, the inverse
   seeded from what joints holds, and the rate of each there per unit of
   path parameter: the module's Jacobian along the tangent, taken by a
   central difference of the geometry, exact for a line and a small
   fraction of a sample step for an arc.  False where the module cannot
   answer. */
static bool sample_at(KinematicsUserContext *ctx, double length, int K,
                      SegmentPoseFn pose_at, void *arg, int njoints,
                      double f, double *joints, double *rate)
{
    double df = 1.0 / (8.0 * (K - 1));
    double fp = f + df > 1.0 ? 1.0 : f + df;
    double fm = f - df < 0.0 ? 0.0 : f - df;
    EmcPose p, pp, pm;
    double t[AXIS_COUNT];
    double J[KINEMATICS_USER_MAX_JOINTS][AXIS_COUNT];
    int j, a;

    pose_at(f, &p, arg);
    pose_at(fp, &pp, arg);
    pose_at(fm, &pm, arg);
    for (a = 0; a < AXIS_COUNT; a++) {
        t[a] = (pose_axis(pp, a) - pose_axis(pm, a)) / ((fp - fm) * length);
    }
    if (kinematicsUserInverse(ctx, &p, joints) != 0
        || kinematicsUserJacobianAt(ctx, joints, &p, J) != 0) {
        return false;
    }
    for (j = 0; j < njoints; j++) {
        rate[j] = 0.0;
        for (a = 0; a < AXIS_COUNT; a++) { rate[j] += J[j][a] * t[a]; }
    }
    return true;
}

//----------------------------------------------------------------------
// The travel.  Every joint is followed from sample to sample against its
// MIN_LIMIT and MAX_LIMIT, the samples themselves and, between two, the
// cubic through their positions and rates, which is where the joint goes
// between them as long as the samples are close enough to see its turns.
// The cubic only says where to look: a joint is reported past a limit
// where the inverse puts it there, never on the cubic's word.
//----------------------------------------------------------------------

/* a hair past a limit is the inverse rounding, not travel */
static const double TRAVEL_HAIR = 1e-6;
/* the share of its bulge past both ends the cubic may be out by at its
   peak; a peak that close to the limit is read through the inverse */
static const double TRAVEL_DOUBT = 0.05;
/* how deep a stretch is split at its peaks, and how many points beyond
   the samples a segment may be read at in all */
enum { TRAVEL_DEPTH = 6, TRAVEL_READS = 32 };

enum { WATCH_MAX = 1, WATCH_MIN = 2 };
enum { TRAVEL_CLEAR, TRAVEL_CROSSED, TRAVEL_UNSURE };

/* a point the joints are known at */
struct TravelPoint {
    double s;
    double q[KINEMATICS_USER_MAX_JOINTS];
    double g[KINEMATICS_USER_MAX_JOINTS];
};

static void travel_load(TravelPoint *p, int k, int njoints)
{
    p->s = s[k];
    for (int j = 0; j < njoints; j++) {
        p->q[j] = q[k][j];
        p->g[j] = g[k][j];
    }
}

/* whether some joint moves between two points further than its rates at
   them account for: the inverse has changed branch, or the joint swings
   through more between the two than either shows */
static bool travel_jumped(const TravelPoint &a, const TravelPoint &b, int njoints)
{
    double ds = b.s - a.s;

    for (int j = 0; j < njoints; j++) {
        double moved = b.q[j] - a.q[j];
        double trapezoid = 0.5 * (a.g[j] + b.g[j]) * ds;
        double rate = fmax(fabs(a.g[j]), fabs(b.g[j])) * ds;
        if (fabs(moved - trapezoid) > rate + TRAVEL_HAIR) { return true; }
    }
    return false;
}

/* whether a followed joint is past its travel at a point, and if so the
   first one, into out */
static bool travel_past(const TravelPoint &p, const SegmentCapLimits *lim,
                        const int *watch, int njoints, double length, SegmentCap *out)
{
    for (int j = 0; j < njoints; j++) {
        int side = 0;
        if ((watch[j] & WATCH_MAX) && p.q[j] > lim->max[j] + TRAVEL_HAIR) { side = 1; }
        if ((watch[j] & WATCH_MIN) && p.q[j] < lim->min[j] - TRAVEL_HAIR) { side = -1; }
        if (side) {
            out->travel_joint = j;
            out->travel_side = side;
            out->travel_at = p.s / length;
            out->travel_pos = p.q[j];
            return true;
        }
    }
    return false;
}

/* The peak toward one side of the cubic through two points, the joint at
   qa and qb, its rates there scaled to the stretch ma and mb: where it is,
   u inside (0, 1), its value, and how far it bulges past both ends.  False
   where the cubic has no peak inside the stretch on that side. */
static bool travel_peak(double qa, double ma, double qb, double mb, int side,
                        double *u, double *peak, double *bulge)
{
    /* p(t) = qa + ma t + c2 t^2 + c3 t^3, p'(t) = 3 c3 t^2 + 2 c2 t + ma */
    double c2 = 3.0 * (qb - qa) - 2.0 * ma - mb;
    double c3 = 2.0 * (qa - qb) + ma + mb;
    double roots[2];
    int i, n = 0;
    bool found = false;

    if (c3 == 0.0) {
        if (c2 != 0.0) { roots[n++] = -ma / (2.0 * c2); }
    } else {
        double disc = c2 * c2 - 3.0 * c3 * ma;
        if (disc >= 0.0) {
            /* the pair that keeps its digits */
            double r = c2 >= 0.0 ? -(c2 + sqrt(disc)) : -(c2 - sqrt(disc));
            roots[n++] = r / (3.0 * c3);
            if (r != 0.0) { roots[n++] = ma / r; }
        }
    }
    for (i = 0; i < n; i++) {
        double t = roots[i], v;
        if (!(t > 0.0 && t < 1.0)) { continue; }
        v = qa + t * (ma + t * (c2 + t * c3));
        if (!found || side * (v - *peak) > 0.0) {
            *u = t;
            *peak = v;
            found = true;
        }
    }
    if (!found) { return false; }
    *bulge = side * (*peak - (side > 0 ? fmax(qa, qb) : fmin(qa, qb)));
    return *bulge > 0.0;
}

/* What lies between two points: where the cubic between them peaks past a
   limit, or close enough that it may be wrong, the inverse is read at the
   peak; a joint past a limit there is CROSSED, else each half is looked at
   the same way.  UNSURE where the module cannot answer at a peak or a
   joint jumps to it. */
static int travel_between(KinematicsUserContext *ctx, const SegmentCapLimits *lim,
                          double length, int K, SegmentPoseFn pose_at, void *arg,
                          int njoints, const int *watch,
                          const TravelPoint &a, const TravelPoint &b,
                          int *reads, SegmentCap *out)
{
    struct Stretch {
        TravelPoint a, b;
        int depth;
    };
    /* split depth first, the half nearer the start on top */
    static Stretch stack[TRAVEL_DEPTH + 2];
    static TravelPoint m;
    int n = 0, j, side;

    stack[n].a = a;
    stack[n].b = b;
    stack[n++].depth = 0;
    while (n > 0) {
        const Stretch st = stack[--n];
        double ds = st.b.s - st.a.s;
        double worst = 0.0, at = -1.0;

        for (j = 0; j < njoints; j++) {
            for (side = 1; side >= -1; side -= 2) {
                double limit, u, peak, bulge, past;
                if (!(watch[j] & (side > 0 ? WATCH_MAX : WATCH_MIN))) { continue; }
                if (!travel_peak(st.a.q[j], st.a.g[j] * ds, st.b.q[j], st.b.g[j] * ds,
                                 side, &u, &peak, &bulge)) {
                    continue;
                }
                /* how far past the limit the true peak can be */
                limit = side > 0 ? lim->max[j] : lim->min[j];
                past = side * (peak - limit) + TRAVEL_DOUBT * bulge - TRAVEL_HAIR;
                if (past > worst) {
                    worst = past;
                    at = u;
                }
            }
        }
        if (at < 0.0) { continue; }
        /* out of reads: what is left is let through */
        if (*reads <= 0) { return TRAVEL_CLEAR; }
        (*reads)--;
        m.s = st.a.s + at * ds;
        for (j = 0; j < njoints; j++) { m.q[j] = st.a.q[j]; }
        if (!sample_at(ctx, length, K, pose_at, arg, njoints, m.s / length, m.q, m.g)
            || travel_jumped(st.a, m, njoints) || travel_jumped(m, st.b, njoints)) {
            return TRAVEL_UNSURE;
        }
        if (travel_past(m, lim, watch, njoints, length, out)) { return TRAVEL_CROSSED; }
        if (st.depth < TRAVEL_DEPTH) {
            stack[n].a = m;
            stack[n].b = st.b;
            stack[n++].depth = st.depth + 1;
            stack[n].a = st.a;
            stack[n].b = m;
            stack[n++].depth = st.depth + 1;
        }
    }
    return TRAVEL_CLEAR;
}

/* the first place along the sampled segment a joint leaves its travel */
static void travel(KinematicsUserContext *ctx, const SegmentCapLimits *lim,
                   double length, int K, SegmentPoseFn pose_at, void *arg,
                   int njoints, SegmentCap *out)
{
    static int watch[KINEMATICS_USER_MAX_JOINTS];
    static TravelPoint a, b;
    int k, j, any = 0, reads = TRAVEL_READS;

    if (!ok[0]) { return; }
    /* a side of the travel the joint starts past is not followed: the move
       may be the one bringing it back */
    for (j = 0; j < njoints; j++) {
        watch[j] = 0;
        if (std::isfinite(lim->max[j]) && q[0][j] <= lim->max[j] + TRAVEL_HAIR) { watch[j] |= WATCH_MAX; }
        if (std::isfinite(lim->min[j]) && q[0][j] >= lim->min[j] - TRAVEL_HAIR) { watch[j] |= WATCH_MIN; }
        any |= watch[j];
    }
    if (!any) { return; }
    travel_load(&a, 0, njoints);
    for (k = 1; k < K; k++) {
        if (!ok[k]) { return; }
        travel_load(&b, k, njoints);
        if (travel_jumped(a, b, njoints)) { return; }
        if (travel_between(ctx, lim, length, K, pose_at, arg, njoints, watch,
                           a, b, &reads, out) != TRAVEL_CLEAR) {
            return;
        }
        if (travel_past(b, lim, watch, njoints, length, out)) { return; }
        a = b;
    }
}

int segmentCap(KinematicsUserContext *ctx, const SegmentCapLimits *lim,
               double length, int samples, SegmentPoseFn pose_at, void *arg,
               double *joints, SegmentCap *out)
{
    const double tiny = 1e-12;
    /* the most the rate of every joint changes per unit of parameter on
       either side of a sample, and the most it can bulge past the samples
       on either side */
    static double h[SEGMENT_CAP_MAX_SAMPLES][KINEMATICS_USER_MAX_JOINTS];
    static double bulge[SEGMENT_CAP_MAX_SAMPLES][KINEMATICS_USER_MAX_JOINTS];
    int K = samples < 2 ? 2 : samples;
    int njoints;
    int k, j, prev, answered = 0;

    out->vel = SEGMENT_CAP_NONE;
    out->acc = SEGMENT_CAP_NONE;
    out->vel_joint = -1;
    out->acc_joint = -1;
    out->vel_at = 0.0;
    out->samples = 0;
    out->unanswered = 0;
    out->travel_joint = -1;
    out->travel_side = 0;
    out->travel_at = 0.0;
    out->travel_pos = 0.0;
    if (!ctx || !lim || !pose_at || !joints || length <= 0.0) { return -1; }
    if (K > SEGMENT_CAP_MAX_SAMPLES) { K = SEGMENT_CAP_MAX_SAMPLES; }
    njoints = lim->joints;
    if (njoints > KINEMATICS_USER_MAX_JOINTS) { njoints = KINEMATICS_USER_MAX_JOINTS; }
    out->samples = K;

    for (k = 0; k < K; k++) {
        double f = (double)k / (double)(K - 1);

        s[k] = f * length;
        /* the joints at the sample, seeded from the sample before so the
           whole segment stays on one solution branch, then the Jacobian
           on that branch */
        ok[k] = sample_at(ctx, length, K, pose_at, arg, njoints, f, joints, g[k]);
        if (!ok[k]) {
            out->unanswered++;
            continue;
        }
        for (j = 0; j < njoints; j++) {
            q[k][j] = joints[j];
            h[k][j] = 0.0;
            bulge[k][j] = 0.0;
        }
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

    /* and the travel, on the same samples */
    travel(ctx, lim, length, K, pose_at, arg, njoints, out);
    return 0;
} // segmentCap()

} // namespace motion_planning
