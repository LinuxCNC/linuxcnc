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

/* ─── G-code endpoints: flower-another-line2.ngc spike region ─── */
/* Spike at j0_jrk=204k, X≈62.85 Z≈-16.28 (between g-code lines 93-94) */
/* Start position: X=67.007, Z=-16.643 (line 87) */
static double gcode[][2] = {
    {66.474,-16.513},  /* 88 */
    {65.942,-16.461},  /* 89 */
    {65.409,-16.470},  /* 90 */
    {64.743,-16.562},  /* 91 */
    {64.343,-16.550},  /* 92 */
    {63.544,-16.436},  /* 93 — spike between here */
    {62.212,-16.150},  /* 94 — and here (204k j0 jerk) */
    {60.879,-15.828},  /* 95 */
    {60.347,-15.641},  /* 96 */
    {59.414,-15.180},  /* 97 */
    {59.014,-15.038},  /* 98 */
    {58.482,-14.922},  /* 99 */
    {57.949,-14.895},  /* 100 */
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
    tp->currentPos.tran.x = 67.007; tp->currentPos.tran.y = 0.001; tp->currentPos.tran.z = -16.643;
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

    /* Print ALL segments */
    for (int i = 0; i < qlen; i++) {
        TC_STRUCT *t = tcqItem(&tp->queue, i);
        if (!t) continue;
        EmcPose ps, pe;
        tc_get_pos(t, 0, &ps); tc_get_pos(t, t->target, &pe);
        const char *tn = t->motion_type == TC_BEZIER ? "BEZR" : "LINE";
        const char *tc_str = t->term_cond == TC_TERM_COND_TANGENT ? "TAN" :
                             t->term_cond == TC_TERM_COND_STOP ? "STP" :
                             t->term_cond == TC_TERM_COND_EXACT ? "EXA" : "PAR";
        printf("  [%3d] %s id=%d tgt=%.4f v0=%.2f vf=%.2f kink=%.2f fvel=%.2f maxv=%.1f jrk=%.0f %s valid=%d opt=%d\n",
               i, tn, t->id, t->target,
               t->shared_9d.profile.valid ? t->shared_9d.profile.v[0] : -1.0,
               t->shared_9d.profile.valid ? t->shared_9d.profile.v[RUCKIG_PROFILE_PHASES] : -1.0,
               t->kink_vel, t->finalvel, t->maxvel, t->maxjerk, tc_str,
               t->shared_9d.profile.valid,
               (int)t->shared_9d.optimization_state);
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
    printf("\nPhase 2: RT consumption at 1kHz (with split cycles)...\n");
    double dt = 0.001;
    struct Sample { double x, z; int seg_id, seg_type; double vel; int split; };
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

    for (int sample = 0; sample < 200000; sample++) {
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

        int spike_region = trace[i].z < -15.5 && trace[i].z > -17.0 && trace[i].x < 65 && trace[i].x > 59;
        if (viol || spike_region) {
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
