#ifndef TP_PRIV_H
#define TP_PRIV_H

#include "tp_enums.h"
#include "tc_types.h"
#include "tp_types.h"
#include "stdbool.h"
#include "error_util.h"

int tpComputeBlendVelocity(
        TC_STRUCT const *tc,
        TC_STRUCT const *nexttc,
        double v_target_this,
        double v_target_next,
        double *v_blend_this,
        double *v_blend_next,
        double *v_blend_net);

double estimateParabolicBlendPerformance(
        TC_STRUCT const *tc,
        TC_STRUCT const *nexttc);

int tpCheckEndCondition(TP_STRUCT const * const tp,
                        TC_STRUCT * const tc,
                        TC_STRUCT const * const nexttc);

int tpUpdateCycle(TP_STRUCT * const tp,
        TC_STRUCT * const tc,
        TC_STRUCT const * const nexttc,
        UpdateCycleMode cycle_mode);

double tpGetCurrentVel(TP_STRUCT const * const tp, PmVector const * const v_current, int * pure_angular);
int tpRunOptimization(TP_STRUCT * const tp);

int tpAddSegmentToQueue(TP_STRUCT * const tp, TC_STRUCT * const tc);

int tpIsWaitingOnSpindle(TP_STRUCT const * const tp);

int tpSetupSegmentBlend(TP_STRUCT * const tp, TC_STRUCT * const prev_tc, TC_STRUCT * const tc);

tp_err_t tpCreateBiarcBlend(TP_STRUCT * const tp, TC_STRUCT * const prev_tc, TC_STRUCT * const this_tc);

int tcRotaryMotionCheck(TC_STRUCT const * const tc);

double tpGetTangentKinkRatio(void);

double tpGetRealAbsFeedScale(TP_STRUCT const * const tp,
                      TC_STRUCT const * const tc);

double tpGetRealAbsTargetVel(TP_STRUCT const * const tp,
                          TC_STRUCT const * const tc);

double getMaxBlendFeedScale(TC_STRUCT const * prev_tc, TC_STRUCT const * tc);

double tpGetRealMaxTargetVel(TP_STRUCT const * const tp, TC_STRUCT const * const tc);

double tpGetRealFinalVel(TP_STRUCT const * const tp,
                         TC_STRUCT const * const tc, TC_STRUCT const * const nexttc);

void setSpindleOrigin(spindle_origin_t *origin, double position);

double tpCalculateTriangleVel(TC_STRUCT const *tc);

double calculateOptimizationInitialVel(const TC_STRUCT * const prev_tc, const TC_STRUCT * const tc);

bool isArcBlendFaster(TC_STRUCT const * const prev_tc, double expected_v_max);

tc_blend_type_t tpChooseBestBlend(TC_STRUCT * const prev_tc,
                                  TC_STRUCT * const tc,
                                  double arc_blend_maxvel);

tp_err_t tpCreateLineLineBlend(TP_STRUCT * const tp,
                               TC_STRUCT * const prev_tc,
                               TC_STRUCT * const tc,
                               TC_STRUCT * const blend_tc);

int handlePrevTermCondition(TC_STRUCT *prev_tc, TC_STRUCT *tc);
int handleModeChange(TC_STRUCT *prev_tc, TC_STRUCT *tc);
int tpSetupSyncedIO(TP_STRUCT * const tp, TC_STRUCT * const tc);

tp_err_t tpFinalizeAndEnqueue(TP_STRUCT * const tp, TC_STRUCT * const tc, const PmVector * nominal_goal);

int tpComputeOptimalVelocity(TC_STRUCT * const tc,
    TC_STRUCT * const prev1_tc);

TCIntersectType tpSetupTangent(
    TP_STRUCT const * const tp,
    TC_STRUCT * const prev_tc,
    TC_STRUCT * const tc);

int tcUpdateDistFromAccel(TC_STRUCT * const tc, double acc, double vel_desired, int reverse_run);

void tpDebugCycleInfo(TP_STRUCT const * const tp,
                      TC_STRUCT const * const tc,
                      TC_STRUCT const * const nexttc,
                      double acc,
                      int accel_mode,
                      UpdateCycleMode cycle);

int tpCalculateRampAccel(TP_STRUCT const * const tp,
                         TC_STRUCT * const tc,
                         TC_STRUCT const * const nexttc,
                         double * const acc,
                         double * const vel_desired);

double estimate_rigidtap_decel_distance(double vel, double uu_per_rev);

void tpUpdateRigidTapState(
    TP_STRUCT * const tp,
    TC_STRUCT * const tc,
    TC_STRUCT * const nexttc);

int tpUpdateMovementStatus(TP_STRUCT * const tp,
                           TC_STRUCT const * const tc,
                           TC_STRUCT const * const nexttc);

void tpHandleEmptyQueue(TP_STRUCT * const tp);

void tpSetRotaryUnlock(IndexRotaryAxis axis, int unlock);

int tpGetRotaryIsUnlocked(IndexRotaryAxis axis);

int tpCompleteSegment(TP_STRUCT * const tp,
                      TC_STRUCT * const tc);

tp_err_t tpHandleAbort(TP_STRUCT * const tp,
                       TC_STRUCT * const tc,
                       TC_STRUCT * const nexttc);

tp_err_t tpCheckAtSpeed(TP_STRUCT * const tp, TC_STRUCT * const tc);

tp_err_t tpActivateSegment(TP_STRUCT * const tp, TC_STRUCT * const tc);

void tpSyncVelocityMode(TC_STRUCT * const tc, TC_STRUCT * const nexttc);

void checkPositionSyncError(TP_STRUCT const *tp, TC_STRUCT const *tc);

void clearPositionSyncErrors();
void clearPosTrackingStatus();

double findSpindleDisplacement(
        double new_pos,
        spindle_origin_t origin
    );

double findSpindleVelocity(
        double spindle_velocity,
        spindle_origin_t origin
    );

bool spindleReversed(spindle_origin_t origin, double prev_pos, double current_pos);

void cmdReverseSpindle(double reversal_scale);

void reportTPAxisError(TP_STRUCT const *tp, unsigned failed_axes, const char *msg_prefix);


void tpSyncPositionMode(TP_STRUCT * const tp,
                        TC_STRUCT * const tc,
                        TC_STRUCT * const nexttc );

int tpDoParabolicBlending(TP_STRUCT * const tp,
                          TC_STRUCT * const tc,
                          TC_STRUCT * const nexttc);

int tpUpdateInitialStatus(TP_STRUCT const * const tp);

int tpUpdateCycle(TP_STRUCT * const tp,
        TC_STRUCT * const tc,
        TC_STRUCT const * const nexttc,
        UpdateCycleMode cycle_mode);

int tcSetSplitCycle(TC_STRUCT * const tc,
                    double split_time,
                    double v_f);

int tpHandleRegularCycle(TP_STRUCT * const tp,
                         TC_STRUCT * const tc,
                         TC_STRUCT * const nexttc);

int tpHandleSplitCycle(TP_STRUCT * const tp,
                       TC_STRUCT * const tc,
                       TC_STRUCT * const nexttc);

int tpSteppingCheck(TP_STRUCT * const tp,
                    TC_STRUCT * const tc,
                    TC_STRUCT * const nexttc);

int tpInitBlendArcFromAdjacent(TP_STRUCT const * const tp,
                           TC_STRUCT const * const prev_tc,
                           TC_STRUCT * const blend_tc,
                           double vel,
                           double ini_maxvel,
                           double acc,
                           tc_motion_type_t motion_type);

int find_max_element(double arr[], int sz);

#endif // TP_PRIV_H
