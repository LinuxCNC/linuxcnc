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
void rtapi_print_msg(msg_level_t level, const char *fmt, ...) { (void)level;(void)fmt; }
void rtapi_print(const char *fmt, ...) { (void)fmt; }
int ruckigProfileSample(ruckig_profile_t const *profile, double t,
                        double *pos, double *vel, double *acc, double *jerk);
void tpFlushCompressor_9D(TP_STRUCT *tp);
int userspace_kins_init(const char*, int, const char*);
int userspace_kins_set_joint_limits(int, double, double, double, double);
}

/* ─── G-code endpoints: spike region only (lines 100-110) ─── */
/* Start position: X=42.363, Z=-13.300 (line 101) */
static double gcode[][2] = {
    {41.830,-13.862},  /* 102 */
    {41.563,-14.206},  /* 103 */
    {41.297,-14.482},  /* 104 */
    {41.031,-14.851},  /* 105 */
    {40.897,-15.094},  /* 106 */
    {40.498,-16.063},  /* 107 */
    {40.365,-16.581},  /* 108 — spike nearby */
    {40.231,-18.000},  /* 109 — 84.6° kink with 110 */
    {0.001, -18.000},  /* 110 — long X return */
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
    g_config.trajCycleTime = 0.001;
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
    tp->magic = TP_MAGIC; tp->queue_ready = 1; tp->queueSize = 200;
    tcqCreate(&tp->queue, tp->queueSize); tcqInit(&tp->queue);
    tp->cycleTime = 0.001; tp->vMax = 500; tp->ini_maxvel = 250;
    tp->ini_maxjerk = 50000; tp->vLimit = 500; tp->aMax = 5000;
    tp->termCond = TC_TERM_COND_PARABOLIC; tp->tolerance = 0;
    tp->currentPos.tran.x = 42.363; tp->currentPos.tran.y = 0.001; tp->currentPos.tran.z = -13.300;
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

    /* Mark first segment active for optimizer convergence */
    TC_STRUCT *first = tcqItem(&tp->queue, 0);
    if (first) { first->active = 1; first->currentvel = 0.0; }

    /* ═══ PHASE 1b: Converge optimizer ═══ */
    printf("Phase 1b: converging optimizer...\n");
    for (int pass = 0; pass < 500; pass++)
        tpOptimizePlannedMotions_9D(tp, 200);

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

    /* Print segments near spike */
    for (int i = 0; i < qlen; i++) {
        TC_STRUCT *t = tcqItem(&tp->queue, i);
        if (!t) continue;
        EmcPose ps, pe;
        tc_get_pos(t, 0, &ps); tc_get_pos(t, t->target, &pe);
        if ((ps.tran.z < -15 && ps.tran.z > -18.5 && ps.tran.x < 45 && ps.tran.x > 38) ||
            t->motion_type == TC_BEZIER) {
            const char *tn = t->motion_type == TC_BEZIER ? "BEZR" : "LINE";
            printf("  [%3d] %s id=%d tgt=%.4f v0=%.2f vf=%.2f kink=%.2f jrk=%.0f X=%.1f→%.1f Z=%.1f→%.1f valid=%d\n",
                   i, tn, t->id, t->target,
                   t->shared_9d.profile.valid ? t->shared_9d.profile.v[0] : -1.0,
                   t->shared_9d.profile.valid ? t->shared_9d.profile.v[RUCKIG_PROFILE_PHASES] : -1.0,
                   t->kink_vel, t->maxjerk, ps.tran.x, pe.tran.x, ps.tran.z, pe.tran.z,
                   t->shared_9d.profile.valid);
        }
    }

    /* ═══ PHASE 2: RT consumption with progressive optimization ═══ */
    printf("\nPhase 2: RT consumption at 1kHz...\n");
    double dt = 0.001;
    struct Sample { double x, z; int seg_id, seg_type; };
    std::vector<Sample> trace;
    trace.reserve(200000);

    TC_STRUCT *tc = tcqItem(&tp->queue, 0);
    if (!tc || !tc->shared_9d.profile.valid) {
        printf("ERROR: first segment invalid\n"); return 1;
    }
    tc->active = 1; tc->currentvel = tc->shared_9d.profile.v[0];
    tc->elapsed_time = 0; tc->position_base = 0;
    tc->last_profile_generation = tc->shared_9d.profile.generation;

    int opt_counter = 0;

    for (int sample = 0; sample < 200000; sample++) {
        /* Run optimizer periodically (every 10 RT cycles ≈ 10ms) */
        if (++opt_counter >= 10) {
            opt_counter = 0;
            tpOptimizePlannedMotions_9D(tp, 200);
        }

        if (!tc->shared_9d.profile.valid) {
            /* Run optimizer urgently until profile appears */
            for (int w = 0; w < 500; w++) {
                tpOptimizePlannedMotions_9D(tp, 200);
                if (tc->shared_9d.profile.valid) break;
            }
            if (!tc->shared_9d.profile.valid) {
                printf("RT: seg %d type=%d no profile after 500 opt passes at sample %d\n",
                       tc->id, tc->motion_type, sample);
                break;
            }
        }

        /* Stopwatch reset */
        int pg = __atomic_load_n(&tc->shared_9d.profile.generation, __ATOMIC_ACQUIRE);
        if (pg != tc->last_profile_generation) {
            tc->position_base = tc->progress;
            tc->elapsed_time = 0;
            tc->last_profile_generation = pg;
        }

        double st = std::min(tc->elapsed_time, tc->shared_9d.profile.duration);
        double pos, vel, acc, jrk;
        ruckigProfileSample(&tc->shared_9d.profile, st, &pos, &vel, &acc, &jrk);
        double tp2 = std::clamp(tc->position_base + pos, 0.0, tc->target);
        tc->progress = tp2; tc->currentvel = vel; tc->currentacc = acc;
        tc->elapsed_time += dt;

        EmcPose pose; tc_get_pos(tc, tp2, &pose);
        trace.push_back({pose.tran.x, pose.tran.z, tc->id, tc->motion_type});

        /* Segment completion */
        if (tc->elapsed_time >= tc->shared_9d.profile.duration + dt*0.5 &&
            tp2 >= tc->target - 1e-6) {
            double dur = tc->shared_9d.profile.duration;
            double cv; { double p2,a2,j2; ruckigProfileSample(&tc->shared_9d.profile, dur, &p2,&cv,&a2,&j2); }
            tc->progress = tc->target; tc->active = 0;
            tcqPop(&tp->queue);
            TC_STRUCT *next = tcqItem(&tp->queue, 0);
            if (!next) break;
            tc = next;
            tc->active = 1; tc->currentvel = cv; tc->currentacc = 0;
            tc->elapsed_time = 0; tc->position_base = 0;
            tc->last_profile_generation = tc->shared_9d.profile.generation;
        }
    }
    printf("Collected %zu samples\n", trace.size());

    /* ═══ PHASE 3: Jerk analysis ═══ */
    double j_limit = 50000;
    double max_jx=0, max_jz=0; int mjx_i=0, mjz_i=0, violations=0;

    printf("\nJerk violations & spike region:\n");
    for (size_t i = 3; i < trace.size(); i++) {
        double jx = (trace[i].x - 3*trace[i-1].x + 3*trace[i-2].x - trace[i-3].x)/(dt*dt*dt);
        double jz = (trace[i].z - 3*trace[i-1].z + 3*trace[i-2].z - trace[i-3].z)/(dt*dt*dt);
        if (fabs(jx)>fabs(max_jx)) { max_jx=jx; mjx_i=i; }
        if (fabs(jz)>fabs(max_jz)) { max_jz=jz; mjz_i=i; }
        int viol = fabs(jx) > j_limit*1.1 || fabs(jz) > j_limit*1.1;
        if (viol) violations++;
        int spike_region = trace[i].z < -16.5 && trace[i].z > -17.5 && trace[i].x < 42 && trace[i].x > 38;
        if (viol || spike_region) {
            double vz = (trace[i].z - trace[i-1].z)/dt;
            const char *tn = trace[i].seg_type == TC_BEZIER ? "BEZR" : "LINE";
            printf("  %5zu %9.4f %9.4f  vz=%7.2f  jx=%10.0f jz=%10.0f  %s(%d)%s\n",
                   i, trace[i].x, trace[i].z, vz, jx, jz, tn, trace[i].seg_id, viol?" ***":"");
        }
    }

    printf("\n=== SUMMARY ===\n");
    printf("Samples: %zu  Beziers: %d  Valid: %d/%d\n", trace.size(), bezr_n, valid_n, qlen);
    printf("Max jx: %.0f at %d (%.2fx)\n", max_jx, mjx_i, fabs(max_jx)/j_limit);
    printf("Max jz: %.0f at %d (%.2fx)\n", max_jz, mjz_i, fabs(max_jz)/j_limit);
    printf("Violations: %d\n", violations);
    if (trace.size()>1) printf("Path: (%.3f,%.3f)→(%.3f,%.3f)\n",
        trace[0].x, trace[0].z, trace.back().x, trace.back().z);
    free(tp);
    return violations > 0 ? 1 : 0;
}
