/********************************************************************
* Description: ruckig_wrapper.h
*   Ruckig trajectory planning library wrapper for LinuxCNC
*
*   This wrapper provides a C interface to Ruckig C++ library
*   for S-curve trajectory planning.
*
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2024 All rights reserved.
********************************************************************/
#ifndef RUCKIG_WRAPPER_H
#define RUCKIG_WRAPPER_H

#include "rtapi.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Ruckig trajectory planner handle (opaque pointer)
 */
typedef void* RuckigPlanner;

/**
 * Create a Ruckig trajectory planner.
 *
 * @param cycle_time  cycle time in seconds
 * @return planner handle, or NULL on failure
 */
RuckigPlanner ruckig_create(double cycle_time);

/**
 * Destroy a Ruckig trajectory planner.
 *
 * @param planner  planner handle
 */
void ruckig_destroy(RuckigPlanner planner);

/**
 * Plan an S-curve trajectory in position control mode.
 *
 * Given the initial and target states, plan a complete S-curve trajectory.
 *
 * @param planner      planner handle
 * @param current_pos  current position
 * @param current_vel  current velocity
 * @param current_acc  current acceleration
 * @param target_pos   target position
 * @param target_vel   target velocity (usually 0)
 * @param target_acc   target acceleration (usually 0)
 * @param min_vel      minimum velocity limit (set to 0 for unidirectional motion)
 * @param max_vel      maximum velocity limit
 * @param max_acc      maximum acceleration limit
 * @param max_jerk     maximum jerk limit
 * @return 0 on success, -1 on failure (insufficient distance or invalid params)
 */
int ruckig_plan_position(RuckigPlanner planner,
                         double current_pos,
                         double current_vel,
                         double current_acc,
                         double target_pos,
                         double target_vel,
                         double target_acc,
                         double min_vel,
                         double max_vel,
                         double max_acc,
                         double max_jerk);

/**
 * Plan an S-curve trajectory in velocity control mode (for stop/pause).
 *
 * Uses velocity control mode, ignoring target position.
 * Suitable for stop or pause scenarios where deceleration may span segments.
 *
 * @param planner      planner handle
 * @param current_vel  current velocity
 * @param current_acc  current acceleration
 * @param target_vel   target velocity (0 for stop)
 * @param target_acc   target acceleration (usually 0)
 * @param min_vel      minimum velocity limit (set to 0 for unidirectional motion)
 * @param max_acc      maximum acceleration limit
 * @param max_jerk     maximum jerk limit
 * @return 0 on success, -1 on failure (invalid params)
 */
int ruckig_plan_velocity(RuckigPlanner planner,
                         double current_vel,
                         double current_acc,
                         double target_vel,
                         double target_acc,
                         double min_vel,
                         double max_acc,
                         double max_jerk);

/**
 * Get the motion state at a specified time.
 *
 * @param planner  planner handle
 * @param time     time in seconds (from trajectory start)
 * @param pos      [out] position
 * @param vel      [out] velocity
 * @param acc      [out] acceleration
 * @param jerk     [out] jerk
 * @return 0 on success, -1 on failure (time out of range)
 */
int ruckig_at_time(RuckigPlanner planner,
                   double time,
                   double *pos,
                   double *vel,
                   double *acc,
                   double *jerk);

/**
 * Get the motion state at the next cycle.
 *
 * Computes the state at (current_time + cycle_time).
 *
 * @param planner       planner handle
 * @param current_time  current time in seconds (from trajectory start)
 * @param cycle_time    cycle time in seconds
 * @param pos           [out] position
 * @param vel           [out] velocity
 * @param acc           [out] acceleration
 * @param jerk          [out] jerk
 * @return 0 on success, -1 on failure (time out of range or not planned)
 */
int ruckig_next_cycle(RuckigPlanner planner,
                      double current_time,
                      double cycle_time,
                      double *pos,
                      double *vel,
                      double *acc,
                      double *jerk);

/**
 * Get total trajectory duration.
 *
 * @param planner  planner handle
 * @return total time in seconds, or -1.0 on failure
 */
double ruckig_get_duration(RuckigPlanner planner);

/**
 * Check if the trajectory has completed.
 *
 * @param planner       planner handle
 * @param current_time  current time in seconds
 * @return 1 if finished, 0 if not, -1 on error
 */
int ruckig_is_finished(RuckigPlanner planner, double current_time);

/**
 * Reset the planner state.
 *
 * Clears previous planning results, preparing for new planning.
 *
 * @param planner  planner handle
 */
void ruckig_reset(RuckigPlanner planner);

/**
 * Enable or disable log output.
 *
 * Controls whether the planner outputs error and warning messages.
 * For velocity planning scenarios (e.g. sp_scurve.c), logging can be
 * disabled to avoid unnecessary warnings.
 *
 * @param planner  planner handle
 * @param enable   1=enable logging, 0=disable logging
 */
void ruckig_set_logging(RuckigPlanner planner, int enable);

/**
 * Get the deceleration phase durations (T1 and T2) from the Ruckig profile.
 *
 * T1: time for acceleration to change from 0 to -amax (jerk phase)
 * T2: time at constant -amax acceleration (constant accel phase)
 *
 * @param planner  planner handle (must have completed planning)
 * @param t1       [out] T1 time (jerk phase), NULL if not needed
 * @param t2       [out] T2 time (constant accel phase), NULL if not needed
 * @return 0 on success, -1 on failure (not planned or cannot retrieve)
 */
int ruckig_get_decelerate_phases(RuckigPlanner planner, double *t1, double *t2);

/**
 * Get the peak velocity of the trajectory.
 *
 * @param planner   planner handle (must have completed planning)
 * @param peak_vel  [out] peak velocity
 * @return 0 on success, -1 on failure (not planned or cannot retrieve)
 */
int ruckig_get_peak_velocity(RuckigPlanner planner, double *peak_vel);

/**
 * Get the start velocity of the trajectory.
 *
 * @param planner    planner handle (must have completed planning)
 * @param start_vel  [out] start velocity
 * @return 0 on success, -1 on failure (not planned or cannot retrieve)
 */
int ruckig_get_start_velocity(RuckigPlanner planner, double *start_vel);

/**
 * Get the time at which the trajectory first reaches a given position.
 *
 * @param planner     planner handle (must have completed planning)
 * @param position    target position
 * @param time_after  start query time (optional, default 0.0)
 * @param time        [out] time at which position is reached
 * @return 0 on success, -1 on failure (not planned, position unreachable)
 */
int ruckig_get_time_at_position(RuckigPlanner planner, double position, double time_after, double *time);

#ifdef __cplusplus
}
#endif

#endif /* RUCKIG_WRAPPER_H */
