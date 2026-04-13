/**
 * test_tp_jerk_sim.cc — Full TP2 pipeline simulation
 *
 * Phase 1: Feed all G-code + optimize (single thread)
 * Phase 2: RT-style consumption at 1kHz (single thread)
 * Phase 3: Jerk analysis via backward-difference
 *
 * Build (from unit_tests/tp/):
 *   g++ -std=c++20 -O2 -DULAPI \
 *       -I../../include -I../../src/emc/tp -I../../src/emc/motion_planning \
 *       -I../../src/emc/motion -I../../src/emc/nml_intf -I../../src/emc/kinematics \
 *       -I../../src/emc/ini \
 *       test_tp_jerk_sim.cc \
 *       -L../../lib -lmotion_planning_9d -lposemath -Wl,-rpath,../../lib \
 *       -lm -lpthread -o test_tp_jerk_sim
 */

#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <vector>

#include "posemath.h"
#include "emcpos.h"
#include "motion.h"
#include "inihal.hh"
#include "kinematics_params.h"
#include "motion_planning_9d.hh"

/* ─── Global stubs ─── */
static emcmot_status_t g_status;
static emcmot_config_t g_config;
static emcmot_internal_t g_internal;
emcmot_status_t *emcmotStatus = &g_status;
emcmot_config_t *emcmotConfig = &g_config;
emcmot_internal_t *emcmotInternal = &g_internal;
struct emcmot_struct_t;
emcmot_struct_t *emcmotStruct = nullptr;
value_inihal_data old_inihal_data;
static kinematics_params_t g_kins_params;

extern "C" {
int hal_struct_attach(const char *n, void **p) {
    if (n && strstr(n, ".params")) { *p = &g_kins_params; return 0; }
    return -1;
}
void rtapi_print_msg(msg_level_t level, const char *fmt, ...) {
    if (level <= 4) { /* RTAPI_MSG_INFO=3, RTAPI_MSG_DBG=4 */
        va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
    }
}
void rtapi_print(const char *fmt, ...) { (void)fmt; }
int ruckigProfileSample(ruckig_profile_t const *profile, double t,
                        double *pos, double *vel, double *acc, double *jerk);
void tpFlushCompressor_9D(TP_STRUCT *tp);
int userspace_kins_init(const char*, int, const char*);
int userspace_kins_set_joint_limits(int, double, double, double, double);
}

/* ─── G-code endpoints: flower-another-line2.ngc (full program) ─── */
/* Start position: X=0.001, Y=0.001, Z=-18.000 (after G1Z-18 at line 5) */
/* First G1 move is to X=149.866 (line 6), then back through all segments */
static double gcode[][2] = {
    {149.866, -18.000},  /*  6 */
    {127.086, -18.000},  /*  7 */
    {126.953, -17.756},  /*  8 */
    {126.687, -14.090},  /*  9 */
    {126.553, -13.508},  /* 10 */
    {126.287, -13.026},  /* 11 */
    {126.154, -12.845},  /* 12 */
    {126.021, -12.700},  /* 13 */
    {125.754, -12.492},  /* 14 */
    {125.355, -12.256},  /* 15 */
    {124.822, -12.014},  /* 16 */
    {124.422, -11.879},  /* 17 */
    {123.889, -11.778},  /* 18 */
    {123.223, -11.706},  /* 19 */
    {122.557, -11.684},  /* 20 */
    {121.758, -11.712},  /* 21 */
    {120.825, -11.807},  /* 22 */
    {119.760, -11.989},  /* 23 */
    {118.427, -12.273},  /* 24 */
    {117.095, -12.602},  /* 25 */
    {114.564, -13.251},  /* 26 */
    {112.699, -13.702},  /* 27 */
    {110.701, -14.178},  /* 28 */
    {109.103, -14.599},  /* 29 */
    {108.170, -14.892},  /* 30 */
    {107.371, -15.185},  /* 31 */
    {106.838, -15.424},  /* 32 */
    {106.172, -15.772},  /* 33 */
    {105.639, -16.131},  /* 34 */
    {105.239, -16.480},  /* 35 */
    {104.840, -16.950},  /* 36 */
    {104.707, -17.130},  /* 37 */
    {104.440, -17.067},  /* 38 */
    {103.508, -16.750},  /* 39 */
    {102.575, -16.362},  /* 40 */
    {101.776, -15.963},  /* 41 */
    {100.710, -15.338},  /* 42 */
    {100.177, -15.108},  /* 43 */
    { 99.644, -14.969},  /* 44 */
    { 99.112, -14.916},  /* 45 */
    { 98.446, -14.932},  /* 46 */
    { 98.046, -14.995},  /* 47 */
    { 97.513, -15.153},  /* 48 */
    { 97.113, -15.323},  /* 49 */
    { 96.714, -15.553},  /* 50 */
    { 95.648, -16.313},  /* 51 */
    { 94.982, -16.751},  /* 52 */
    { 94.582, -16.128},  /* 53 */
    { 94.316, -15.818},  /* 54 */
    { 94.049, -15.580},  /* 55 */
    { 93.650, -15.312},  /* 56 */
    { 93.250, -15.125},  /* 57 */
    { 92.717, -14.971},  /* 58 */
    { 92.318, -14.907},  /* 59 */
    { 91.785, -14.898},  /* 60 */
    { 91.385, -14.943},  /* 61 */
    { 90.852, -15.077},  /* 62 */
    { 90.453, -15.241},  /* 63 */
    { 89.787, -15.585},  /* 64 */
    { 89.254, -15.786},  /* 65 */
    { 87.522, -16.211},  /* 66 */
    { 86.456, -16.437},  /* 67 */
    { 85.657, -16.549},  /* 68 */
    { 85.257, -16.561},  /* 69 */
    { 84.725, -16.481},  /* 70 */
    { 84.192, -16.455},  /* 71 */
    { 83.659, -16.497},  /* 72 */
    { 82.993, -16.640},  /* 73 */
    { 82.860, -16.648},  /* 74 */
    { 81.527, -16.376},  /* 75 */
    { 80.195, -16.160},  /* 76 */
    { 78.730, -15.979},  /* 77 */
    { 77.398, -15.873},  /* 78 */
    { 75.932, -15.806},  /* 79 */
    { 74.467, -15.797},  /* 80 */
    { 73.268, -15.832},  /* 81 */
    { 71.803, -15.933},  /* 82 */
    { 70.604, -16.055},  /* 83 */
    { 69.139, -16.261},  /* 84 */
    { 67.673, -16.531},  /* 85 */
    { 67.140, -16.648},  /* 86 */
    { 67.007, -16.643},  /* 87 */
    { 66.474, -16.513},  /* 88 */
    { 65.942, -16.461},  /* 89 */
    { 65.409, -16.470},  /* 90 */
    { 64.743, -16.562},  /* 91 */
    { 64.343, -16.550},  /* 92 */
    { 63.544, -16.436},  /* 93 */
    { 62.212, -16.150},  /* 94 */
    { 60.879, -15.828},  /* 95 */
    { 60.347, -15.641},  /* 96 */
    { 59.414, -15.180},  /* 97 */
    { 59.014, -15.038},  /* 98 */
    { 58.482, -14.922},  /* 99 */
    { 57.949, -14.895},  /*100 */
    { 57.416, -14.943},  /*101 */
    { 56.883, -15.088},  /*102 */
    { 56.483, -15.252},  /*103 */
    { 56.084, -15.490},  /*104 */
    { 55.684, -15.828},  /*105 */
    { 55.418, -16.138},  /*106 */
    { 55.151, -16.514},  /*107 */
    { 55.018, -16.746},  /*108 */
    { 54.885, -16.716},  /*109 */
    { 53.952, -16.091},  /*110 */
    { 53.153, -15.518},  /*111 */
    { 52.620, -15.229},  /*112 */
    { 52.087, -15.045},  /*113 */
    { 51.554, -14.939},  /*114 */
    { 51.022, -14.912},  /*115 */
    { 50.356, -14.947},  /*116 */
    { 49.956, -15.032},  /*117 */
    { 49.423, -15.223},  /*118 */
    { 48.890, -15.522},  /*119 */
    { 48.224, -15.914},  /*120 */
    { 47.425, -16.317},  /*121 */
    { 46.492, -16.708},  /*122 */
    { 45.560, -17.031},  /*123 */
    { 45.293, -17.086},  /*124 */
    { 45.160, -16.905},  /*125 */
    { 44.761, -16.464},  /*126 */
    { 44.361, -16.117},  /*127 */
    { 43.828, -15.757},  /*128 */
    { 43.029, -15.347},  /*129 */
    { 42.363, -15.075},  /*130 */
    { 41.297, -14.710},  /*131 */
    { 39.965, -14.335},  /*132 */
    { 38.500, -13.968},  /*133 */
    { 36.501, -13.489},  /*134 */
    { 34.636, -13.024},  /*135 */
    { 32.372, -12.432},  /*136 */
    { 30.906, -12.088},  /*137 */
    { 29.708, -11.853},  /*138 */
    { 29.041, -11.751},  /*139 */
    { 28.109, -11.665},  /*140 */
    { 27.310, -11.639},  /*141 */
    { 26.644, -11.674},  /*142 */
    { 25.711, -11.803},  /*143 */
    { 25.311, -11.910},  /*144 */
    { 24.912, -12.066},  /*145 */
    { 24.246, -12.422},  /*146 */
    { 23.979, -12.631},  /*147 */
    { 23.846, -12.775},  /*148 */
    { 23.713, -12.956},  /*149 */
    { 23.580, -13.208},  /*150 */
    { 23.447, -13.598},  /*151 */
    { 23.313, -14.236},  /*152 */
    { 23.047, -17.803},  /*153 */
    { 22.914, -18.000},  /*154 */
    {  0.001, -18.000},  /*155 */
};
static const int N_GCODE = sizeof(gcode)/sizeof(gcode[0]);

static void tc_get_pos(TC_STRUCT *tc, double progress, EmcPose *out) {
    memset(out, 0, sizeof(*out));
    if (tc->motion_type == TC_LINEAR) {
        PmCartesian *s = &tc->coords.line.xyz.start;
        PmCartesian *e = &tc->coords.line.xyz.end;
        double f = (tc->target > 1e-12) ? std::clamp(progress/tc->target, 0.0, 1.0) : 0.0;
        out->tran.x = s->x + f*(e->x - s->x);
        out->tran.y = s->y + f*(e->y - s->y);
        out->tran.z = s->z + f*(e->z - s->z);
    } else if (tc->motion_type == TC_BEZIER) {
        bezier9Point(&tc->coords.bezier, tc->coords.bezier.s_start + progress, out);
    }
}

int main() {
    printf("=== TP2 Jerk Simulator ===\n");

    memset(&g_status, 0, sizeof(g_status));
    g_status.planner_type = 2;
    g_status.feed_scale = 1.0;
    g_status.rapid_scale = 1.0;
    g_status.net_feed_scale = 1.0;
    memset(&g_config, 0, sizeof(g_config));
    g_config.trajCycleTime = 0.00025;
    memset(&g_internal, 0, sizeof(g_internal));

    setHandoffConfig(100, 50, 100, 200, 500, 50, 0.001, 50000);
    initPredictiveHandoff();

    /* Joint limits in inihal */
    memset(&old_inihal_data, 0, sizeof(old_inihal_data));
    for (int j=0;j<3;j++) { old_inihal_data.joint_max_velocity[j]=250; old_inihal_data.joint_max_acceleration[j]=5000; old_inihal_data.joint_jerk[j]=50000; old_inihal_data.joint_min_limit[j]=-1000; old_inihal_data.joint_max_limit[j]=1000; }
    for (int j=3;j<5;j++) { old_inihal_data.joint_max_velocity[j]=360; old_inihal_data.joint_max_acceleration[j]=3600; old_inihal_data.joint_jerk[j]=36000; old_inihal_data.joint_min_limit[j]=-1000; old_inihal_data.joint_max_limit[j]=1000; }
    old_inihal_data.joint_max_velocity[5]=250; old_inihal_data.joint_max_acceleration[5]=5000; old_inihal_data.joint_jerk[5]=50000; old_inihal_data.joint_min_limit[5]=-1000; old_inihal_data.joint_max_limit[5]=1000;

    /* Kinematics params */
    memset(&g_kins_params, 0, sizeof(g_kins_params));
    strncpy(g_kins_params.module_name, "trivkins", 31);
    strncpy(g_kins_params.coordinates, "XYZBCW", 15);
    g_kins_params.num_joints = 6;
    for (int j=0;j<6;j++) g_kins_params.joint_to_axis[j]=j;
    for (int a=0;a<9;a++) g_kins_params.axis_to_joint[a]=-1;
    g_kins_params.axis_to_joint[0]=0; g_kins_params.axis_to_joint[1]=1; g_kins_params.axis_to_joint[2]=2;
    g_kins_params.axis_to_joint[3]=3; g_kins_params.axis_to_joint[4]=4; g_kins_params.axis_to_joint[8]=5;
    g_kins_params.valid=1; g_kins_params.is_identity=1; g_kins_params.head=1; g_kins_params.tail=1;

    userspace_kins_init("trivkins", 6, "XYZBCW");
    for (int j=0;j<3;j++) userspace_kins_set_joint_limits(j, 250, 5000, -1000, 1000);
    for (int j=3;j<5;j++) userspace_kins_set_joint_limits(j, 360, 3600, -1000, 1000);
    userspace_kins_set_joint_limits(5, 250, 5000, -1000, 1000);

    /* TP on heap */
    TP_STRUCT *tp = (TP_STRUCT *)calloc(1, sizeof(TP_STRUCT));
    tp->magic = TP_MAGIC; tp->queue_ready = 1; tp->queueSize = 500;
    tcqCreate(&tp->queue, tp->queueSize); tcqInit(&tp->queue);
    tp->cycleTime = 0.00025; tp->vMax = 500; tp->ini_maxvel = 250;
    tp->ini_maxjerk = 50000; tp->vLimit = 500; tp->aMax = 5000;
    tp->termCond = TC_TERM_COND_PARABOLIC; tp->tolerance = 0;
    tp->currentPos.tran.x = 0.001; tp->currentPos.tran.y = 0.001; tp->currentPos.tran.z = -18.000;
    tp->goalPos = tp->currentPos;

    /* ═══ PHASE 1: Feed all segments ═══ */
    printf("Phase 1: feeding %d segments...\n", N_GCODE);
    state_tag_t tag; memset(&tag, 0, sizeof(tag));
    for (int i = 0; i < N_GCODE; i++) {
        EmcPose end; memset(&end, 0, sizeof(end));
        end.tran.x = gcode[i][0]; end.tran.y = 0.001; end.tran.z = gcode[i][1];
        tag.fields[0] = i+1;
        tpAddLine_9D(tp, end, 1, 800.0, 250.0, 5000.0, 1, tag);
        tpOptimizePlannedMotions_9D(tp, 200);
    }
    tpFlushCompressor_9D(tp);
    tpOptimizePlannedMotions_9D(tp, 200);

    /* ═══ PHASE 1b: Seed velocities and converge optimizer ═══ */
    /* Blend creation sets v_plan=0 (kink_vel carries the actual limit).
     * The optimizer's backward pass needs non-zero finalvel to propagate
     * velocities.  Seed all TANGENT segments with kink_vel as initial
     * finalvel, then run the optimizer to converge profiles. */
    printf("Phase 1b: seeding velocities + converging optimizer...\n");
    {
        int qlen_conv = tcqLen(&tp->queue);

        /* Seed: set finalvel = kink_vel for all TANGENT segments */
        for (int i = 0; i < qlen_conv; i++) {
            TC_STRUCT *t = tcqItem(&tp->queue, i);
            if (!t) continue;
            if (t->term_cond == TC_TERM_COND_TANGENT && t->kink_vel > 0) {
                t->finalvel = t->kink_vel;
                __atomic_store(&t->shared_9d.final_vel, &t->kink_vel, __ATOMIC_RELEASE);
                __atomic_store(&t->shared_9d.final_vel_limit, &t->kink_vel, __ATOMIC_RELEASE);
            }
        }

        /* Activate first segment */
        TC_STRUCT *first = tcqItem(&tp->queue, 0);
        if (first) { first->active = 1; first->currentvel = 0.0; }

        /* Run optimizer until all profiles converge (or timeout) */
        for (int pass = 0; pass < 5000; pass++) {
            tpOptimizePlannedMotions_9D(tp, 200);
            if (pass % 500 == 499) {
                int valid = 0;
                for (int i = 0; i < qlen_conv; i++) {
                    TC_STRUCT *t = tcqItem(&tp->queue, i);
                    if (t && t->shared_9d.profile.valid) valid++;
                }
                if (valid == qlen_conv) break;
            }
        }
    }

    /* Queue summary */
    int qlen = tcqLen(&tp->queue);
    int valid_n = 0, bezr_n = 0;
    for (int i = 0; i < qlen; i++) {
        TC_STRUCT *t = tcqItem(&tp->queue, i);
        if (!t) continue;
        if (t->motion_type == TC_BEZIER) bezr_n++;
        if (t->shared_9d.profile.valid) valid_n++;
    }
    printf("Queue: %d segments, %d bezier, %d valid profiles\n", qlen, bezr_n, valid_n);

    /* Print segments with curvature info for beziers */
    for (int i = 0; i < qlen; i++) {
        TC_STRUCT *t = tcqItem(&tp->queue, i);
        if (!t) continue;
        EmcPose ps, pe;
        tc_get_pos(t, 0, &ps); tc_get_pos(t, t->target, &pe);
        const char *tn = t->motion_type == TC_BEZIER ? "BEZR" : "LINE";
        const char *tc_str = t->term_cond == TC_TERM_COND_TANGENT ? "TAN" :
                             t->term_cond == TC_TERM_COND_STOP ? "STP" :
                             t->term_cond == TC_TERM_COND_EXACT ? "EXA" : "PAR";
        if (t->motion_type == TC_BEZIER) {
            double kappa = t->coords.bezier.max_kappa;
            double dkds = t->coords.bezier.max_dkappa_ds;
            double rmin = t->coords.bezier.min_radius;
            double v_acc_full = sqrt(BLEND9_ACC_RATIO_NORMAL * t->maxaccel * rmin);
            double v_jrk_full = (dkds > 1e-12) ? cbrt(BLEND9_ACC_RATIO_NORMAL * 50000.0 / dkds) : 999;
            double v_jrk_tang = (dkds > 1e-12) ? cbrt(BLEND9_ACC_RATIO_NORMAL * t->maxjerk / dkds) : 999;
            printf("  [%3d] %s id=%d tgt=%.4f kink=%.2f maxv=%.1f mjrk=%.0f v_acc=%.1f v_jrk_full=%.1f v_jrk_tang=%.1f κ=%.2f dκ/ds=%.1f %s X=%.2f..%.2f Z=%.3f..%.3f\n",
                   i, tn, t->id, t->target,
                   t->kink_vel, t->maxvel, t->maxjerk, v_acc_full, v_jrk_full, v_jrk_tang,
                   kappa, dkds, tc_str,
                   ps.tran.x, pe.tran.x, ps.tran.z, pe.tran.z);
        } else {
            printf("  [%3d] %s id=%d tgt=%.4f v0=%.2f vf=%.2f kink=%.2f fvel=%.2f maxv=%.1f %s\n",
                   i, tn, t->id, t->target,
                   t->shared_9d.profile.valid ? t->shared_9d.profile.v[0] : -1.0,
                   t->shared_9d.profile.valid ? t->shared_9d.profile.v[RUCKIG_PROFILE_PHASES] : -1.0,
                   t->kink_vel, t->finalvel, t->maxvel, tc_str);
        }
    }

    /* ═══ PHASE 2: RT consumption with split-cycle handling ═══ */
    /*
     * Flow per cycle:
     *   1. Advance elapsed_time += dt
     *   2. Chain-split: if elapsed > duration, transition to next segment(s),
     *      accumulating displacement from each completed segment + remainder
     *   3. Sample current segment, accumulate displacement
     *   4. Record running position
     *
     * Displacement-based accumulation ensures split cycles that cross
     * segment boundaries don't produce zero-displacement artifacts.
     */
    printf("\nPhase 2: RT consumption at 4kHz / 250us (with split cycles)...\n");
    double dt = 0.00025;
    struct Sample { double x, z; int seg_id, seg_type; double vel; int split; };
    std::vector<Sample> trace;
    trace.reserve(800000);

    TC_STRUCT *tc = tcqItem(&tp->queue, 0);
    if (!tc || !tc->shared_9d.profile.valid) {
        printf("ERROR: first segment invalid\n"); return 1;
    }
    tc->active = 1; tc->currentvel = tc->shared_9d.profile.v[0];
    tc->elapsed_time = 0; tc->position_base = 0;
    tc->last_profile_generation = tc->shared_9d.profile.generation;

    int opt_counter = 0;
    double run_x = tp->currentPos.tran.x;
    double run_z = tp->currentPos.tran.z;

    auto ensure_valid_profile = [&](TC_STRUCT *seg) -> bool {
        if (__atomic_load_n(&seg->shared_9d.profile.valid, __ATOMIC_ACQUIRE))
            return true;
        for (int w = 0; w < 500; w++) {
            tpOptimizePlannedMotions_9D(tp, 200);
            if (__atomic_load_n(&seg->shared_9d.profile.valid, __ATOMIC_ACQUIRE))
                return true;
        }
        return false;
    };

    for (int sample = 0; sample < 800000; sample++) {
        if (++opt_counter >= 10) {
            opt_counter = 0;
            tpOptimizePlannedMotions_9D(tp, 200);
        }

        if (!ensure_valid_profile(tc)) {
            printf("RT: seg %d type=%d no profile at sample %d\n",
                   tc->id, tc->motion_type, sample);
            break;
        }

        /* Stopwatch reset */
        int pg = __atomic_load_n(&tc->shared_9d.profile.generation, __ATOMIC_ACQUIRE);
        if (pg != tc->last_profile_generation) {
            tc->position_base = tc->progress;
            tc->elapsed_time = 0;
            tc->last_profile_generation = pg;
        }

        /* Step 1: advance time */
        tc->elapsed_time += dt;
        double dx = 0, dz = 0;

        /* Step 2: chain-split if segment completed */
        double dur = tc->shared_9d.profile.duration;
        int chain_depth = 0;
        while (chain_depth < 10 &&
               tc->progress >= tc->target - 1e-6 &&
               tc->elapsed_time > dur + 1e-9 &&
               tc->term_cond == TC_TERM_COND_TANGENT)
        {
            /* Exit velocity at profile end */
            double cp, cv, ca, cj;
            ruckigProfileSample(&tc->shared_9d.profile, dur, &cp, &cv, &ca, &cj);

            /* Displacement: current progress → segment end */
            EmcPose p_cur, p_end;
            tc_get_pos(tc, tc->progress, &p_cur);
            tc_get_pos(tc, tc->target, &p_end);
            dx += p_end.tran.x - p_cur.tran.x;
            dz += p_end.tran.z - p_cur.tran.z;

            double remain = tc->elapsed_time - dur;
            if (remain < 0) remain = 0;
            if (remain > dt) remain = dt;

            /* Transition to next segment */
            tc->progress = tc->target;
            tc->active = 0;
            tcqPop(&tp->queue);
            TC_STRUCT *next = tcqItem(&tp->queue, 0);
            if (!next) { tc = nullptr; break; }
            tc = next;
            tc->active = 1;
            tc->position_base = 0;
            tc->progress = 0;
            tc->currentvel = cv;
            tc->currentacc = ca;

            if (!ensure_valid_profile(tc)) {
                tc->elapsed_time = remain;
                break;
            }
            tc->last_profile_generation = tc->shared_9d.profile.generation;
            tc->elapsed_time = remain;

            /* Velocity continuity check: compare exit vel with next profile's v0 */
            double next_v0 = tc->shared_9d.profile.v[0];
            double v_mismatch = fabs(cv - next_v0);
            if (v_mismatch > 0.01) {
                const char *tn = tc->motion_type == TC_BEZIER ? "BEZR" : "LINE";
                EmcPose split_pos; tc_get_pos(tc, 0, &split_pos);
                printf("V_MISMATCH: exit=%.3f next_v0=%.3f delta=%.3f → %s(id=%d) "
                       "at X=%.2f Z=%.2f sample=%d\n",
                       cv, next_v0, cv - next_v0, tn, tc->id,
                       split_pos.tran.x, split_pos.tran.z, sample);
            }
            dur = tc->shared_9d.profile.duration;
            chain_depth++;
        }
        if (!tc) break;

        /* Step 3: sample current segment at elapsed_time */
        dur = tc->shared_9d.profile.duration;
        double st = std::min(tc->elapsed_time, dur);
        double pos, vel, acc, jrk;
        ruckigProfileSample(&tc->shared_9d.profile, st, &pos, &vel, &acc, &jrk);

        EmcPose p_before; tc_get_pos(tc, tc->progress, &p_before);
        double tp2 = std::clamp(tc->position_base + pos, 0.0, tc->target);
        tc->progress = tp2;
        tc->currentvel = vel;
        tc->currentacc = acc;
        EmcPose p_after; tc_get_pos(tc, tc->progress, &p_after);
        dx += p_after.tran.x - p_before.tran.x;
        dz += p_after.tran.z - p_before.tran.z;

        /* Step 4: record */
        run_x += dx;
        run_z += dz;
        trace.push_back({run_x, run_z, tc->id, tc->motion_type, tc->currentvel, chain_depth > 0 ? 1 : 0});

        /* STOP/EXACT completion (decel to 0, no split) */
        if (tc->progress >= tc->target - 1e-6 &&
            tc->elapsed_time >= dur &&
            tc->term_cond != TC_TERM_COND_TANGENT)
        {
            tc->progress = tc->target;
            tc->active = 0;
            tcqPop(&tp->queue);
            TC_STRUCT *next = tcqItem(&tp->queue, 0);
            if (!next) break;
            tc = next;
            tc->active = 1; tc->currentvel = 0; tc->currentacc = 0;
            tc->elapsed_time = 0; tc->position_base = 0;
            if (tc->shared_9d.profile.valid)
                tc->last_profile_generation = tc->shared_9d.profile.generation;
        }
    }
    printf("Collected %zu samples\n", trace.size());

    /* ═══ PHASE 3: Jerk analysis ═══ */
    double j_limit = 50000;
    double max_jx=0, max_jz=0; int mjx_i=0, mjz_i=0, violations=0;

    /* Detect jerk violations.  A backward-difference window that straddles
     * a segment boundary has partial-cycle displacement that inflates the
     * apparent jerk.  Detect this by checking if segment ID or type changes
     * within the 4-sample window.  These "junction" violations are inherent
     * to the split-cycle mechanism and don't represent real kinematic issues. */
    int junc_violations = 0;
    int real_violations = 0;
    double max_real_jx=0, max_real_jz=0;

    printf("\nJerk analysis (>1.1x limit):\n");
    for (size_t i = 3; i < trace.size(); i++) {
        double jx = (trace[i].x - 3*trace[i-1].x + 3*trace[i-2].x - trace[i-3].x)/(dt*dt*dt);
        double jz = (trace[i].z - 3*trace[i-1].z + 3*trace[i-2].z - trace[i-3].z)/(dt*dt*dt);
        if (fabs(jx)>fabs(max_jx)) { max_jx=jx; mjx_i=i; }
        if (fabs(jz)>fabs(max_jz)) { max_jz=jz; mjz_i=i; }
        int viol = fabs(jx) > j_limit*1.1 || fabs(jz) > j_limit*1.1;
        if (viol) violations++;

        /* Check if segment identity changes within an extended 6-sample
         * window (i-4..i+1).  The partial-cycle displacement occurs one
         * sample BEFORE the chain fires, so we need to look ahead. */
        int near_junc = 0;
        for (int w = -1; w <= 1 && !near_junc; w++) {
            /* Check 4-sample window [i+w-3 .. i+w] for seg changes */
            for (int k = 0; k < 3 && !near_junc; k++) {
                int a = (int)i + w - 3 + k;
                int b = a + 1;
                if (a < 0 || b < 0 || a >= (int)trace.size() || b >= (int)trace.size()) continue;
                if (trace[a].seg_id != trace[b].seg_id || trace[a].seg_type != trace[b].seg_type)
                    near_junc = 1;
            }
        }
        if (viol && near_junc) junc_violations++;
        if (viol && !near_junc) {
            real_violations++;
            if (fabs(jx)>fabs(max_real_jx)) max_real_jx=jx;
            if (fabs(jz)>fabs(max_real_jz)) max_real_jz=jz;
        }

        if (viol) {
            double vz = (trace[i].z - trace[i-1].z)/dt;
            const char *tn = trace[i].seg_type == TC_BEZIER ? "BEZR" : "LINE";
            const char *tag = near_junc ? " [junc]" : "";
            printf("  %5zu %9.4f %9.4f  vz=%7.2f vel=%7.2f  jx=%10.0f jz=%10.0f  %s(%d)%s%s\n",
                   i, trace[i].x, trace[i].z, vz, trace[i].vel, jx, jz, tn, trace[i].seg_id,
                   viol?" ***":"", tag);
        }
    }

    printf("\n=== SUMMARY ===\n");
    printf("Samples: %zu  Beziers: %d  Valid: %d/%d\n", trace.size(), bezr_n, valid_n, qlen);
    printf("Max jx: %.0f at %d (%.2fx)\n", max_jx, mjx_i, fabs(max_jx)/j_limit);
    printf("Max jz: %.0f at %d (%.2fx)\n", max_jz, mjz_i, fabs(max_jz)/j_limit);
    printf("Violations: %d total (%d junction artifacts, %d real)\n",
           violations, junc_violations, real_violations);
    if (real_violations > 0) {
        printf("Max real jx: %.0f (%.2fx)\n", max_real_jx, fabs(max_real_jx)/j_limit);
        printf("Max real jz: %.0f (%.2fx)\n", max_real_jz, fabs(max_real_jz)/j_limit);
    }
    if (trace.size()>1) printf("Path: (%.3f,%.3f)→(%.3f,%.3f)\n",
        trace[0].x, trace[0].z, trace.back().x, trace.back().z);
    free(tp);
    return violations > 0 ? 1 : 0;
}
