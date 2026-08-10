/********************************************************************
 * Description: kinslimits.cc
 *   Diagnostic tool: print the Jacobian and the world-space velocity,
 *   acceleration and jerk caps that a given kinematics module imposes
 *   on a straight move between two poses.
 *
 *   The tool attaches to a running HAL instance, loads the kinematics
 *   module through the non-RT interface, samples the move, and reports
 *   the most restrictive cap found along it.  The sampling loop here is
 *   the same one the trajectory planner uses to cap a segment.
 *
 *   Example (in a terminal with a running config, or under halrun):
 *
 *     halrun -I
 *     halcmd: loadrt 5axiskins coordinates=XYZBCW
 *     halcmd: setp 5axiskins.pivot-length 100
 *     halcmd: loadusr -w kinslimits --module 5axiskins --joints 6 \
 *               --coords XYZBCW --start 0,0,0,0,0,0,0,0,0 \
 *               --end 100,0,0,0,90,0,0,0,0 \
 *               --vel 100,100,100,30,30,30 --acc 500,500,500,200,200,200
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

#include <hal.h>
#include "jacobian.hh"
#include "joint_limits.hh"

using namespace motion_planning;

static const char *AXIS_NAME[9] = {"X","Y","Z","A","B","C","U","V","W"};

static std::vector<double> parse_list(const char *s)
{
    std::vector<double> out;
    const char *p = s;
    while (*p) {
        char *endp = nullptr;
        double v = strtod(p, &endp);
        if (endp == p) break;
        out.push_back(v);
        p = endp;
        while (*p == ',' || *p == ' ') p++;
    }
    return out;
}

static void list_to_pose(const std::vector<double>& v, EmcPose *p)
{
    double a[9] = {0,0,0,0,0,0,0,0,0};
    for (size_t i = 0; i < v.size() && i < 9; i++) a[i] = v[i];
    p->tran.x = a[0]; p->tran.y = a[1]; p->tran.z = a[2];
    p->a = a[3]; p->b = a[4]; p->c = a[5];
    p->u = a[6]; p->v = a[7]; p->w = a[8];
}

static double pose_axis(const EmcPose& p, int ax)
{
    switch (ax) {
        case 0: return p.tran.x; case 1: return p.tran.y; case 2: return p.tran.z;
        case 3: return p.a; case 4: return p.b; case 5: return p.c;
        case 6: return p.u; case 7: return p.v; default: return p.w;
    }
}

static void set_pose_axis(EmcPose *p, int ax, double val)
{
    switch (ax) {
        case 0: p->tran.x = val; break; case 1: p->tran.y = val; break;
        case 2: p->tran.z = val; break; case 3: p->a = val; break;
        case 4: p->b = val; break; case 5: p->c = val; break;
        case 6: p->u = val; break; case 7: p->v = val; break;
        default: p->w = val; break;
    }
}

static void usage(const char *argv0)
{
    fprintf(stderr,
        "usage: %s --module NAME --joints N --coords LETTERS\n"
        "          --start x,y,z,a,b,c,u,v,w --end x,y,z,a,b,c,u,v,w\n"
        "          --vel v0,v1,... --acc a0,a1,... [--jerk j0,j1,...]\n"
        "          [--samples N] [--singularity COND]\n"
        "\n"
        "Prints the Jacobian and the world-space caps the joint limits imply\n"
        "for a straight move from --start to --end.  Requires a running HAL\n"
        "instance with the kinematics module loaded.\n", argv0);
}

int main(int argc, char **argv)
{
    const char *module = nullptr;
    const char *coords = nullptr;
    int num_joints = 0;
    int samples = 11;
    double singularity = 100.0;
    std::vector<double> start_v, end_v, vel_v, acc_v, jerk_v;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *next = (i + 1 < argc) ? argv[i + 1] : nullptr;
        if (!strcmp(a, "--module") && next)          { module = next; i++; }
        else if (!strcmp(a, "--coords") && next)     { coords = next; i++; }
        else if (!strcmp(a, "--joints") && next)     { num_joints = atoi(next); i++; }
        else if (!strcmp(a, "--samples") && next)    { samples = atoi(next); i++; }
        else if (!strcmp(a, "--singularity") && next){ singularity = atof(next); i++; }
        else if (!strcmp(a, "--start") && next)      { start_v = parse_list(next); i++; }
        else if (!strcmp(a, "--end") && next)        { end_v = parse_list(next); i++; }
        else if (!strcmp(a, "--vel") && next)        { vel_v = parse_list(next); i++; }
        else if (!strcmp(a, "--acc") && next)        { acc_v = parse_list(next); i++; }
        else if (!strcmp(a, "--jerk") && next)       { jerk_v = parse_list(next); i++; }
        else { usage(argv[0]); return 1; }
    }

    if (!module || !coords || num_joints < 1 ||
        start_v.empty() || end_v.empty() || vel_v.empty() || acc_v.empty()) {
        usage(argv[0]);
        return 1;
    }
    if ((int)vel_v.size() < num_joints || (int)acc_v.size() < num_joints) {
        fprintf(stderr, "kinslimits: --vel and --acc need %d entries\n", num_joints);
        return 1;
    }
    if (samples < 2) samples = 2;

    int comp_id = hal_init("kinslimits");
    if (comp_id < 0) {
        fprintf(stderr, "kinslimits: hal_init failed (is HAL running?)\n");
        return 1;
    }

    KinematicsUserContext *ctx = kinematicsUserInit(module, num_joints, coords,
                                                    comp_id, "kinslimits");
    if (!ctx) {
        fprintf(stderr, "kinslimits: kinematicsUserInit failed for '%s'\n", module);
        hal_exit(comp_id);
        return 1;
    }
    if (kinematicsUserIsRtOnly(ctx)) {
        fprintf(stderr, "kinslimits: '%s' is RT-only, no non-RT interface\n", module);
        kinematicsUserFree(ctx);
        hal_exit(comp_id);
        return 1;
    }

    JacobianCalculator jac;
    JointLimitCalculator lim;
    if (!jac.init(ctx) || !lim.init(num_joints)) {
        fprintf(stderr, "kinslimits: calculator init failed\n");
        kinematicsUserFree(ctx);
        hal_exit(comp_id);
        return 1;
    }

    std::vector<double> minpos(num_joints, -1e9), maxpos(num_joints, 1e9);
    if ((int)jerk_v.size() < num_joints) jerk_v.assign(num_joints, 1e9);
    lim.updateAllLimits(vel_v.data(), acc_v.data(),
                        minpos.data(), maxpos.data(), jerk_v.data());

    EmcPose start, end;
    list_to_pose(start_v, &start);
    list_to_pose(end_v, &end);

    /* Path parameter: XYZ arc length, falling back to the largest rotary
       delta for a pure rotary move, matching what the planner uses. */
    double dx = end.tran.x - start.tran.x;
    double dy = end.tran.y - start.tran.y;
    double dz = end.tran.z - start.tran.z;
    double target = sqrt(dx*dx + dy*dy + dz*dz);
    if (target < 1e-12) {
        for (int ax = 3; ax < 9; ax++) {
            double d = fabs(pose_axis(end, ax) - pose_axis(start, ax));
            if (d > target) target = d;
        }
    }
    if (target < 1e-12) {
        fprintf(stderr, "kinslimits: start and end are the same pose\n");
        kinematicsUserFree(ctx);
        hal_exit(comp_id);
        return 1;
    }

    /* tangent[a] = d(world axis a) / d(path parameter) */
    double tangent[9];
    for (int ax = 0; ax < 9; ax++) {
        tangent[ax] = (pose_axis(end, ax) - pose_axis(start, ax)) / target;
    }

    printf("module        : %s (%s, %d joints)%s\n", module, coords, num_joints,
           kinematicsUserIsIdentity(ctx) ? "  [identity]" : "");
    printf("path length   : %.6f (tangent units per path unit)\n", target);
    printf("tangent       :");
    for (int ax = 0; ax < 9; ax++) {
        if (fabs(tangent[ax]) > 1e-12) printf("  %s=%.4f", AXIS_NAME[ax], tangent[ax]);
    }
    printf("\n\n");

    double min_vel = 1e9, min_acc = 1e9, min_jerk = 1e9, max_cond = 1.0;
    int at_vel = -1, at_acc = -1, at_jerk = -1;
    double min_vel_s = 0.0;

    for (int i = 0; i < samples; i++) {
        double frac = (double)i / (double)(samples - 1);
        EmcPose p;
        for (int ax = 0; ax < 9; ax++) {
            set_pose_axis(&p, ax,
                pose_axis(start, ax) + frac * (pose_axis(end, ax) - pose_axis(start, ax)));
        }

        double joints[KINEMATICS_USER_MAX_JOINTS] = {0};
        if (kinematicsUserInverse(ctx, &p, joints) != 0) {
            printf("sample %2d: inverse kinematics failed\n", i);
            continue;
        }

        double J[9][9];
        if (!jac.compute(p, J)) {
            printf("sample %2d: Jacobian failed\n", i);
            continue;
        }

        double jpad[9] = {0};
        for (int j = 0; j < num_joints && j < 9; j++) jpad[j] = joints[j];

        JointLimitResult r;
        if (!lim.computeForTangent(J, jpad, tangent, r, singularity)) {
            printf("sample %2d: limit calculation failed\n", i);
            continue;
        }

        printf("s=%.3f  vel<=%10.3f (j%d)  acc<=%10.1f (j%d)  jerk<=%12.1f (j%d)  cond=%.2f\n",
               frac, r.max_world_vel, r.limiting_joint_vel,
               r.max_world_acc, r.limiting_joint_acc,
               r.max_world_jerk, r.limiting_joint_jerk, r.condition_number);

        if (r.max_world_vel  < min_vel)  { min_vel = r.max_world_vel;  at_vel = r.limiting_joint_vel; min_vel_s = frac; }
        if (r.max_world_acc  < min_acc)  { min_acc = r.max_world_acc;  at_acc = r.limiting_joint_acc; }
        if (r.max_world_jerk < min_jerk) { min_jerk = r.max_world_jerk; at_jerk = r.limiting_joint_jerk; }
        if (r.condition_number > max_cond) max_cond = r.condition_number;

        if (i == 0) {
            printf("        Jacobian at start (rows = joints, cols = XYZABCUVW):\n");
            for (int j = 0; j < num_joints && j < 9; j++) {
                printf("        j%d:", j);
                for (int ax = 0; ax < 9; ax++) printf(" %8.4f", J[j][ax]);
                printf("\n");
            }
        }
    }

    printf("\nsegment cap   : vel %.3f (joint %d at s=%.3f), acc %.1f (joint %d), jerk %.1f (joint %d)\n",
           min_vel, at_vel, min_vel_s, min_acc, at_acc, min_jerk, at_jerk);
    printf("worst cond    : %.3f\n", max_cond);

    kinematicsUserFree(ctx);
    hal_exit(comp_id);
    return 0;
}
