/********************************************************************
* Description: ruckig_wrapper.c
*   Ruckig trajectory planning library wrapper implementation
*
*   This file provides a C wrapper around Ruckig C++ library
*   for S-curve trajectory planning in LinuxCNC.
*
* License: GPL Version 2
* System: Linux
* Author: 杨阳
* Contact: mika-net@outlook.com
*
* Copyright (c) 2024 All rights reserved.
********************************************************************/

#include "ruckig_wrapper.h"
#include "rtapi.h"
#include "rtapi_math.h"
#include <cstdlib>  // for free()
#include <limits>   // for std::numeric_limits
#include <cmath>    // for fabs

// LinuxCNC precision constants (consistent with tp_types.h)
#ifndef TP_POS_EPSILON
#define TP_POS_EPSILON 1e-12
#endif
#ifndef TP_VEL_EPSILON
#define TP_VEL_EPSILON 1e-8
#endif

// Ruckig C++ header files
#include <ruckig/ruckig.hpp>
using namespace ruckig;

// Ruckig planner struct (defined outside extern "C" because it contains C++ types)
struct RuckigPlannerImpl {
    Ruckig<1> otg;              // 1-DOF Ruckig planner
    InputParameter<1> input;    // input parameters
    Trajectory<1> trajectory;   // trajectory (new API uses Trajectory instead of OutputParameter)
    double cycle_time;          // cycle time
    bool planned;               // whether planning has been done
    double start_time;          // trajectory start time
    double target_pos;          // target position (used for precision correction)
    double target_vel;          // target velocity (used for precision correction)
    int use_position_control;   // whether to use position control mode (1=position control, 0=velocity control)
    double last_actual_acc;     // previous actual acceleration (used for jerk smoothing calculation)
    bool is_first_cycle;        // whether this is the first cycle after replanning
    bool enable_logging;        // whether to enable log output (1=enabled, 0=disabled)
};

// Helper macro: conditionally output log based on planner's logging setting
#define RUCKIG_LOG_IF_ENABLED(planner, level, fmt, ...) \
    do { \
        if (planner) { \
            struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner; \
            if (impl->enable_logging) { \
                rtapi_print_msg(level, fmt, ##__VA_ARGS__); \
            } \
        } else { \
            rtapi_print_msg(level, fmt, ##__VA_ARGS__); \
        } \
    } while (0)

// Wrap all functions with extern "C" so they can be called from C code
extern "C" {

RuckigPlanner ruckig_create(double cycle_time) {
    if (cycle_time <= 0.0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_create: invalid cycle_time %f\n", cycle_time);
        return NULL;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)malloc(sizeof(struct RuckigPlannerImpl));
    if (!impl) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_create: memory allocation failed\n");
        return NULL;
    }

    impl->cycle_time = cycle_time;
    impl->planned = 0;  // use 0/1 instead of false/true (C compatibility)
    impl->start_time = 0.0;
    impl->target_pos = 0.0;
    impl->target_vel = 0.0;
    impl->use_position_control = 0;
    impl->last_actual_acc = 0.0;
    impl->is_first_cycle = 0;
    impl->enable_logging = 1;  // enable log output by default

    // Initialize Ruckig planner (using placement new because Ruckig contains const members)
    new(&impl->otg) Ruckig<1>(cycle_time);
    new(&impl->input) InputParameter<1>();
    new(&impl->trajectory) Trajectory<1>();

    return (RuckigPlanner)impl;
}

void ruckig_destroy(RuckigPlanner planner) {
    if (planner) {
        struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;
        // Call destructors
        impl->otg.~Ruckig();
        impl->input.~InputParameter();
        impl->trajectory.~Trajectory();
        free(planner);  // use standard free instead of rtapi_free
    }
}

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
                         double max_jerk) {
    if (!planner) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    // Parameter validation
    if (max_vel <= 0.0 || max_acc <= 0.0 || max_jerk <= 0.0) {
        RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_position: invalid limits (v=%f, a=%f, j=%f)\n",
                        max_vel, max_acc, max_jerk);
        return -1;
    }

    // Set input parameters (position control mode)
    impl->input.control_interface = ControlInterface::Position;  // explicitly set to position control mode
    impl->input.synchronization = Synchronization::Time;  // use time synchronization (default)

    impl->input.current_position = {current_pos};
    impl->input.current_velocity = {current_vel};
    impl->input.current_acceleration = {current_acc};
    impl->input.target_position = {target_pos};
    impl->input.target_velocity = {target_vel};
    impl->input.target_acceleration = {target_acc};
    impl->input.max_velocity = {max_vel};
    impl->input.min_velocity = std::array<double, 1>{min_vel};  // minimum velocity limit (specified by caller, set to 0 for unidirectional motion)
    impl->input.max_acceleration = {max_acc};
    impl->input.max_jerk = {max_jerk};

    // Key fix: if a previous plan exists, back up the trajectory object to restore on planning failure
    // This ensures the previous trajectory is not corrupted and can continue to be used
    bool had_previous_plan = (impl->planned != 0);
    Trajectory<1> trajectory_backup;
    double backup_target_pos = 0.0;
    double backup_target_vel = 0.0;
    int backup_use_position_control = 0;
    double backup_last_actual_acc = 0.0;

    if (had_previous_plan) {
        // Back up trajectory object and related state
        trajectory_backup = impl->trajectory;  // back up using copy constructor
        backup_target_pos = impl->target_pos;
        backup_target_vel = impl->target_vel;
        backup_use_position_control = impl->use_position_control;
        backup_last_actual_acc = impl->last_actual_acc;
    }

    // Execute planning (new API uses Trajectory)
    Result result = impl->otg.calculate(impl->input, impl->trajectory);

    // Handle various result states
    if (result == Result::Working) {
        // Normal success: trajectory calculation succeeded, continue execution
        // Continue to subsequent processing
    } else if (result == Result::Finished) {
        // Already finished: current position already equals target position (distance is 0 or very small)
        // Trajectory duration may be 0 or very small, needs special handling
        double duration = impl->trajectory.get_duration();
        if (duration < 0.001) {
            // Distance is 0, trajectory already finished, but state still needs to be set
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_INFO, "ruckig_plan_position: already at target position (duration=%f)\n", duration);
        } else {
            // Duration is very short but non-zero, handle normally
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_INFO, "ruckig_plan_position: trajectory finished (duration=%f)\n", duration);
        }
        // Continue to subsequent processing, treat as success
    } else {
        // Planning failed: restore previous trajectory (if it exists)
        if (had_previous_plan) {
            impl->trajectory = trajectory_backup;  // restore backed-up trajectory
            impl->target_pos = backup_target_pos;
            impl->target_vel = backup_target_vel;
            impl->use_position_control = backup_use_position_control;
            impl->last_actual_acc = backup_last_actual_acc;
            // Note: do not reset impl->planned, keep it as 1 to continue using previous trajectory
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_INFO, "ruckig_plan_position: planning failed, restored previous trajectory\n");
        }

        // Output error information
        if (result == Result::ErrorInvalidInput) {
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_position: invalid input parameters\n");
        } else if (result == Result::ErrorTrajectoryDuration) {
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_position: trajectory duration exceeds numerical limits\n");
        } else if (result == Result::ErrorPositionalLimits) {
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_position: positional limits exceeded\n");
        } else if (result == Result::ErrorZeroLimits) {
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_position: zero limits conflict\n");
        } else if (result == Result::ErrorExecutionTimeCalculation) {
            return -2;
            //RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_position: error during execution time calculation\n");
        } else if (result == Result::ErrorSynchronizationCalculation) {
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_position: synchronization calculation error\n");
        } else if (result == Result::Error) {
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_position: general error\n");
        } else {
            // Unknown error state
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_position: unknown error result %d\n", (int)result);
        }
        return -1;
    }

    // Key fix: when replanning, last_actual_acc should preserve the actual acceleration from the
    // last cycle of the previous trajectory, not the acceleration at time 0 of the new trajectory.
    // This ensures jerk continuity and avoids abrupt changes.
    // Check if this is the first plan (planned == 0 before planning)
    bool was_planned = (impl->planned != 0);
    if (!was_planned) {
        // First plan, use the passed-in current_acc
        impl->last_actual_acc = current_acc;
    }
    // If replanning (was_planned == true), keep last_actual_acc unchanged
    // (it is already the actual acceleration from the last cycle of the previous trajectory, updated in ruckig_at_time)

    impl->planned = 1;  // use 1 instead of true (C compatibility)
    impl->start_time = 0.0;  // reset start time
    impl->target_pos = target_pos;  // save target position (used for precision correction)
    impl->target_vel = target_vel;  // save target velocity (used for precision correction)
    impl->use_position_control = 1;  // mark as position control mode
    impl->is_first_cycle = 1;  // mark as first cycle after replanning (jerk calculation needs special handling)
    return 0;
}

int ruckig_plan_velocity(RuckigPlanner planner,
                         double current_vel,
                         double current_acc,
                         double target_vel,
                         double target_acc,
                         double min_vel,
                         double max_acc,
                         double max_jerk) {
    if (!planner) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    // Parameter validation
    if (max_acc <= 0.0 || max_jerk <= 0.0) {
        RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_velocity: invalid limits (a=%f, j=%f)\n",
                        max_acc, max_jerk);
        return -1;
    }

    // Set input parameters (velocity control mode)
    impl->input.control_interface = ControlInterface::Velocity;
    impl->input.synchronization = Synchronization::None;  // independent control, no synchronization needed

    // Velocity control mode: position is ignored, only velocity matters
    impl->input.current_position = {0.0};  // position is ignored in velocity control mode
    impl->input.current_velocity = {current_vel};
    impl->input.current_acceleration = {current_acc};
    impl->input.target_position = {0.0};  // position is ignored in velocity control mode
    impl->input.target_velocity = {target_vel};
    impl->input.target_acceleration = {target_acc};
    // Velocity control mode: max_velocity is not needed (will be ignored), only acceleration and jerk limits
    impl->input.max_velocity = {std::numeric_limits<double>::infinity()};
    impl->input.min_velocity = std::array<double, 1>{min_vel};  // minimum velocity limit (specified by caller, set to 0 for unidirectional motion)
    impl->input.max_acceleration = {max_acc};
    impl->input.max_jerk = {max_jerk};

    // Key fix: if a previous plan exists, back up the trajectory object to restore on planning failure
    // This ensures the previous trajectory is not corrupted and can continue to be used
    bool had_previous_plan = (impl->planned != 0);
    Trajectory<1> trajectory_backup;
    double backup_target_pos = 0.0;
    double backup_target_vel = 0.0;
    int backup_use_position_control = 0;
    double backup_last_actual_acc = 0.0;

    if (had_previous_plan) {
        // Back up trajectory object and related state
        trajectory_backup = impl->trajectory;  // back up using copy constructor
        backup_target_pos = impl->target_pos;
        backup_target_vel = impl->target_vel;
        backup_use_position_control = impl->use_position_control;
        backup_last_actual_acc = impl->last_actual_acc;
    }

    // Execute planning (new API uses Trajectory)
    Result result = impl->otg.calculate(impl->input, impl->trajectory);

    // Handle various result states
    if (result == Result::Working) {
        // Normal success: trajectory calculation succeeded, continue execution
        // Continue to subsequent processing
    } else if (result == Result::Finished) {
        // Already finished: current velocity already equals target velocity (difference is 0 or very small)
        // Trajectory duration may be 0 or very small, needs special handling
        double duration = impl->trajectory.get_duration();
        if (duration < 0.001) {
            // Velocity already matched, trajectory finished, but state still needs to be set
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_INFO, "ruckig_plan_velocity: already at target velocity (duration=%f)\n", duration);
        } else {
            // Duration is very short but non-zero, handle normally
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_INFO, "ruckig_plan_velocity: trajectory finished (duration=%f)\n", duration);
        }
        // Continue to subsequent processing, treat as success
    } else {
        // Planning failed: restore previous trajectory (if it exists)
        if (had_previous_plan) {
            impl->trajectory = trajectory_backup;  // restore backed-up trajectory
            impl->target_pos = backup_target_pos;
            impl->target_vel = backup_target_vel;
            impl->use_position_control = backup_use_position_control;
            impl->last_actual_acc = backup_last_actual_acc;
            // Note: do not reset impl->planned, keep it as 1 to continue using previous trajectory
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_INFO, "ruckig_plan_velocity: planning failed, restored previous trajectory\n");
        }

        // Output error information
        if (result == Result::ErrorInvalidInput) {
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_velocity: invalid input parameters\n");
        } else if (result == Result::ErrorTrajectoryDuration) {
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_velocity: trajectory duration exceeds numerical limits\n");
        } else if (result == Result::ErrorPositionalLimits) {
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_velocity: positional limits exceeded\n");
        } else if (result == Result::ErrorZeroLimits) {
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_velocity: zero limits conflict\n");
        } else if (result == Result::ErrorExecutionTimeCalculation) {
            return -2;
            //RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_velocity: error during execution time calculation\n");
        } else if (result == Result::ErrorSynchronizationCalculation) {
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_velocity: synchronization calculation error\n");
        } else if (result == Result::Error) {
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_velocity: general error\n");
        } else {
            // Unknown error state
            RUCKIG_LOG_IF_ENABLED(planner, RTAPI_MSG_ERR, "ruckig_plan_velocity: unknown error result %d\n", (int)result);
        }
        return -1;
    }

    // Key fix: when replanning, last_actual_acc should preserve the actual acceleration from the
    // last cycle of the previous trajectory, not the acceleration at time 0 of the new trajectory.
    // This ensures jerk continuity and avoids abrupt changes.
    // Check if this is the first plan (planned == 0 before planning)
    bool was_planned = (impl->planned != 0);
    if (!was_planned) {
        // First plan, use the passed-in current_acc
        impl->last_actual_acc = current_acc;
    }
    // If replanning (was_planned == true), keep last_actual_acc unchanged
    // (it is already the actual acceleration from the last cycle of the previous trajectory, updated in ruckig_at_time)

    impl->planned = 1;  // use 1 instead of true (C compatibility)
    impl->start_time = 0.0;  // reset start time
    impl->target_pos = 0.0;  // position is meaningless in velocity control mode
    impl->target_vel = target_vel;  // save target velocity (used for precision correction at trajectory end)
    impl->use_position_control = 0;  // mark as velocity control mode
    impl->is_first_cycle = 1;  // mark as first cycle after replanning (jerk calculation needs special handling)
    return 0;
}

int ruckig_at_time(RuckigPlanner planner,
                   double time,
                   double *pos,
                   double *vel,
                   double *acc,
                   double *jerk) {
    if (!planner || !pos || !vel || !acc || !jerk) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    if (!impl->planned) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_at_time: trajectory not planned\n");
        return -1;
    }

    // Use Ruckig to compute state at specified time (new API uses Trajectory directly)
    double duration = impl->trajectory.get_duration();

    // Special handling: if time exceeds total duration (last cycle), use total duration to get final state
    // This ensures the correct final state is returned at trajectory end, instead of an error
    double query_time = time;
    if (time < 0.0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_at_time: time %f is negative\n", time);
        return -1;
    }
    if (time > duration) {
        // Time out of range, use total duration to get final state
        query_time = duration;
    }

    // Get state at specified time (if out of range, use total duration to get final state)
    std::array<double, 1> new_position, new_velocity, new_acceleration;
    impl->trajectory.at_time(query_time, new_position, new_velocity, new_acceleration);

    *pos = new_position[0];
    *vel = new_velocity[0];
    *acc = new_acceleration[0];

    // Precision correction: ensure position and velocity exactly match target values (meet LinuxCNC precision requirements)
    // Ruckig's default precision is 1e-8, but LinuxCNC requires position precision of 1e-12 (TP_POS_EPSILON)
    // When approaching trajectory endpoint, force values to target (if error is within acceptable range)
    // Correction strategy: start correction early, use looser thresholds, avoid getting stuck in 1e-6 to 1e-7 precision range
    if (impl->use_position_control) {
        // Position control mode: check position and velocity precision
        // Start correction early: begin within the last 10% of trajectory time or last 10 cycles (whichever is larger)
        const double TIME_THRESHOLD = fmax(duration * 0.1, impl->cycle_time * 10.0);
        const double POS_ERROR_THRESHOLD = 1e-6;  // position error threshold: 1e-6 (allow correction to 1e-12)
        const double VEL_ERROR_THRESHOLD = 1e-7;  // velocity error threshold: 1e-7 (allow correction to 1e-8)

        if (time >= duration - TIME_THRESHOLD || time >= duration) {
            // Check position error
            double pos_error = fabs(*pos - impl->target_pos);
            if (pos_error < POS_ERROR_THRESHOLD && pos_error > TP_POS_EPSILON) {
                // If error is between 1e-12 and 1e-6, force correction to target value
                *pos = impl->target_pos;
            } else if (pos_error <= TP_POS_EPSILON) {
                // If error is already very small, also ensure exact match
                *pos = impl->target_pos;
            }

            // Check velocity error
            // Key fix: when velocity is near 0, do not correct velocity too early to avoid breaking S-curve smoothness
            // Only perform velocity correction when trajectory is truly near the end (time >= duration)
            // This avoids acceleration and jerk being forced to 0 when velocity is near 0
            double vel_error = fabs(*vel - impl->target_vel);
            if (time >= duration) {
                // Trajectory has ended, ensure velocity exactly matches target value
                *vel = impl->target_vel;
            } else if (vel_error < VEL_ERROR_THRESHOLD && vel_error > TP_VEL_EPSILON) {
                // During trajectory execution, if velocity error is between 1e-8 and 1e-7, do not correct
                // Let Ruckig's S-curve complete naturally, avoid breaking smoothness
                // Only correct at trajectory end
            } else if (vel_error <= TP_VEL_EPSILON) {
                // If error is already very small, also do not correct, let S-curve complete naturally
            }
        }
    } else {
        // Velocity control mode: Ruckig smoothly transitions from current velocity and acceleration to target velocity and acceleration along an S-curve
        // Note: in velocity control mode, precision correction should not be applied mid-trajectory as it would break S-curve smoothness
        // Only perform precision correction when trajectory truly ends (time >= duration) to ensure final state exactly matches target values
        if (time >= duration) {
            // Trajectory has ended, ensure velocity and acceleration exactly match target values
            // This satisfies LinuxCNC precision requirements without breaking the S-curve
            *vel = impl->target_vel;
            *acc = impl->input.target_acceleration[0];  // ensure acceleration also matches target value
        }
        // During trajectory execution, fully trust Ruckig's S-curve planning, do not perform any correction
    }

    // Calculate jerk (from acceleration rate of change)
    // Note: Ruckig does not directly provide jerk, it must be computed from acceleration changes
    // An approximation method is used here
    // Key fix: do not force acceleration to 0 just because time exceeds total duration
    // Must check if actual position has reached target, avoid misjudging as complete when velocity is near 0 but distance remains
    if (time > duration) {
        // Time exceeds total duration, but need to check if actual position has reached target
        // For position control mode, check position error; for velocity control mode, check velocity error
        if (impl->use_position_control) {
            double pos_error = fabs(*pos - impl->target_pos);
            double vel_error = fabs(*vel - impl->target_vel);
            double acc_threshold = 1e-6;  // acceleration threshold
            bool acc_near_zero = (fabs(*acc) < acc_threshold);
            // Only consider truly complete when position, velocity, and acceleration are all near target
            if (pos_error < TP_POS_EPSILON * 100.0 && vel_error < TP_VEL_EPSILON * 10.0 && acc_near_zero) {
                // Truly reached final state, both jerk and acceleration should be 0
                *jerk = 0.0;
                *acc = 0.0;
            }
            // If position or velocity has not yet approached target, continue using Ruckig's returned acceleration
            // This ensures the S-curve completes smoothly, avoiding abrupt changes
        } else {
            // Velocity control mode: check velocity error and acceleration
            double vel_error = fabs(*vel - impl->target_vel);
            double acc_threshold = 1e-6;  // acceleration threshold
            bool acc_near_zero = (fabs(*acc) < acc_threshold);
            // Only consider truly complete when both velocity and acceleration are near target
            if (vel_error < TP_VEL_EPSILON * 10.0 && acc_near_zero) {
                // Velocity is near target and acceleration is also near 0, both jerk and acceleration should be 0
                *jerk = 0.0;
                *acc = 0.0;
            }
            // If velocity has not yet approached target, continue using Ruckig's returned acceleration
        }
    } else if (query_time > impl->cycle_time) {
        std::array<double, 1> prev_acceleration;
        double prev_time = query_time - impl->cycle_time;
        if (prev_time < 0.0) prev_time = 0.0;
        impl->trajectory.at_time(prev_time, new_position, new_velocity, prev_acceleration);
        *jerk = (new_acceleration[0] - prev_acceleration[0]) / impl->cycle_time;
    } else {
        // For the first cycle after replanning, special handling is needed to ensure jerk continuity
        // Key: during mode switching, must use the previous cycle's actual acceleration, not the initial acceleration at planning time
        // Because the initial acceleration at planning time may differ slightly from the actual acceleration during execution
        if (impl->is_first_cycle) {
            // First cycle: use last_actual_acc as baseline (set to Ruckig's actual acceleration at time 0 during planning)
            // This ensures jerk continuity and avoids abrupt changes
            // last_actual_acc has already been set to Ruckig's actual acceleration at time 0 during planning, so use it directly
            double base_acc = impl->last_actual_acc;

            // Calculate jerk: rate of change from base_acc to new_acceleration[0]
            *jerk = (new_acceleration[0] - base_acc) / impl->cycle_time;

            impl->is_first_cycle = 0;  // clear first cycle flag
        } else {
            // Normal case: use the initial acceleration from planning time
            *jerk = (new_acceleration[0] - impl->input.current_acceleration[0]) / query_time;
        }
    }

    // Save current acceleration for jerk calculation in next cycle
    impl->last_actual_acc = *acc;

    return 0;
}

int ruckig_next_cycle(RuckigPlanner planner,
                      double current_time,
                      double cycle_time,
                      double *pos,
                      double *vel,
                      double *acc,
                      double *jerk) {
    if (!planner) {
        return -1;
    }

    double next_time = current_time + cycle_time;
    return ruckig_at_time(planner, next_time, pos, vel, acc, jerk);
}

double ruckig_get_duration(RuckigPlanner planner) {
    if (!planner) {
        return -1.0;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    if (!impl->planned) {
        return -1.0;
    }

    return impl->trajectory.get_duration();
}

int ruckig_is_finished(RuckigPlanner planner, double current_time) {
    if (!planner) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    if (!impl->planned) {
        return -1;
    }

    double duration = ruckig_get_duration(planner);
    if (duration < 0.0) {
        return -1;
    }

    return (current_time >= duration) ? 1 : 0;
}

void ruckig_reset(RuckigPlanner planner) {
    if (!planner) {
        return;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    // Reset all state fields
    impl->planned = 0;  // use 0 instead of false (C compatibility)
    impl->start_time = 0.0;
    impl->target_pos = 0.0;
    impl->target_vel = 0.0;
    impl->use_position_control = 0;
    impl->last_actual_acc = 0.0;
    impl->is_first_cycle = 0;
    // Note: do not reset enable_logging, preserve user setting

    // Re-initialize C++ objects (using placement new)
    // First call destructors to clean up old objects
    impl->otg.~Ruckig();
    impl->input.~InputParameter();
    impl->trajectory.~Trajectory();

    // Reconstruct objects (using placement new)
    new(&impl->otg) Ruckig<1>(impl->cycle_time);
    new(&impl->input) InputParameter<1>();
    new(&impl->trajectory) Trajectory<1>();
}

void ruckig_set_logging(RuckigPlanner planner, int enable) {
    if (!planner) {
        return;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;
    impl->enable_logging = (enable != 0) ? 1 : 0;
}

int ruckig_get_decelerate_phases(RuckigPlanner planner, double *t1, double *t2) {
    if (!planner) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    if (!impl->planned) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_get_decelerate_phases: trajectory not planned\n");
        return -1;
    }

    // Get Profile (for single DOF, there is only one Profile)
    auto profiles = impl->trajectory.get_profiles();
    if (profiles.empty() || profiles[0].empty()) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_get_decelerate_phases: no profile available\n");
        return -1;
    }

    // Get the first (and only) Profile
    const Profile& profile = profiles[0][0];

    // For a complete S-curve in position control mode, Profile has 7 phases:
    // t[0]-t[2]: acceleration phase (may be 0 if initial velocity is already high)
    // t[3]: constant velocity phase (may be 0)
    // t[4]-t[6]: deceleration phase
    //    t[4]: T1 - time for acceleration to change from 0 to -amax (jerk phase)
    //    t[5]: T2 - time for acceleration to remain at -amax (constant acceleration phase)
    //    t[6]: T3 - time for acceleration to return from -amax to 0 (jerk phase, usually equals T1)
    //
    // For a trajectory decelerating from velocity v to 0, we care about the deceleration phase:
    // T1 = t[4] (acceleration change phase)
    // T2 = t[5] (constant acceleration phase)
    //
    // Note: if t[4] is 0, it may be a triangular curve (no constant acceleration phase),
    // in which case T1 and T2 need to be inferred from other phases, but normally t[4] should be > 0

    if (t1 != NULL) {
        *t1 = (profile.t[4] > 0.0) ? profile.t[4] : 0.0;
    }

    if (t2 != NULL) {
        *t2 = (profile.t[5] > 0.0) ? profile.t[5] : 0.0;
    }

    return 0;
}

int ruckig_get_peak_velocity(RuckigPlanner planner, double *peak_vel) {
    if (!planner || !peak_vel) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    if (!impl->planned) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_get_peak_velocity: trajectory not planned\n");
        return -1;
    }

    // Get Profile (for single DOF, there is only one Profile)
    auto profiles = impl->trajectory.get_profiles();
    if (profiles.empty() || profiles[0].empty()) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_get_peak_velocity: no profile available\n");
        return -1;
    }

    // Get the first (and only) Profile
    const Profile& profile = profiles[0][0];

    // Profile has v[0] to v[7] array, storing velocity values at each phase
    // For a rest-to-rest trajectory:
    // - v[0] = 0 (start velocity)
    // - v[1], v[2], v[3] are velocities during acceleration phase
    // - v[3] or v[4] may be peak velocity (if there is a constant velocity phase, v[3] = v[4])
    // - v[4], v[5], v[6] are velocities during deceleration phase
    // - v[7] = 0 (final velocity)
    //
    // Peak velocity should be the maximum of v[0] through v[7]

    double max_vel = 0.0;
    for (size_t i = 0; i < 8; i++) {
        if (profile.v[i] > max_vel) {
            max_vel = profile.v[i];
        }
    }

    *peak_vel = max_vel;
    return 0;
}

int ruckig_get_start_velocity(RuckigPlanner planner, double *start_vel) {
    if (!planner || !start_vel) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    if (!impl->planned) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_get_start_velocity: trajectory not planned\n");
        return -1;
    }

    // Get Profile (for single DOF, there is only one Profile)
    auto profiles = impl->trajectory.get_profiles();
    if (profiles.empty() || profiles[0].empty()) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_get_start_velocity: no profile available\n");
        return -1;
    }

    // Get the first (and only) Profile
    const Profile& profile = profiles[0][0];

    // Profile's v[0] is the start velocity
    *start_vel = profile.v[0];
    return 0;
}

int ruckig_get_time_at_position(RuckigPlanner planner, double position, double time_after, double *time) {
    if (!planner || time == NULL) {
        return -1;
    }

    struct RuckigPlannerImpl *impl = (struct RuckigPlannerImpl *)planner;

    if (!impl->planned) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ruckig_get_time_at_position: trajectory not planned\n");
        return -1;
    }

    // Use Ruckig's get_first_time_at_position method
    // For single DOF, dof = 0 (return type is ruckig::Optional<double>)
    ruckig::Optional<double> result = impl->trajectory.get_first_time_at_position(0, position, time_after);

    if (result.has_value()) {
        *time = result.value();
        return 0;
    } else {
        // Position does not exist in the trajectory
        return -1;
    }
}

}  // extern "C"
