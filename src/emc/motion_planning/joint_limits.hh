/********************************************************************
 * Description: joint_limits.hh
 *   Joint limit calculation for userspace kinematics trajectory planning
 *
 * Uses the Jacobian to compute maximum world-space velocity and
 * acceleration that respects all joint limits.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/
#ifndef JOINT_LIMITS_HH
#define JOINT_LIMITS_HH

// emcpos.h includes posemath.h which has C++ function overloads
#include "emcpos.h"

extern "C" {
#include "kinematics_userspace/kinematics_user.h"
}

namespace motion_planning {

/**
 * Joint limit configuration
 * Mirrors emcmot_joint_t limits from motion.h
 */
struct JointLimitConfig {
    double max_pos_limit;   // Upper soft limit on joint position
    double min_pos_limit;   // Lower soft limit on joint position
    double vel_limit;       // Maximum joint velocity
    double acc_limit;       // Maximum joint acceleration
    double jerk_limit;      // Maximum joint jerk (for S-curve planning)

    JointLimitConfig() :
        max_pos_limit(1e9),
        min_pos_limit(-1e9),
        vel_limit(1e9),
        acc_limit(1e9),
        jerk_limit(1e9) {}
};

/**
 * Result of joint limit calculation
 */
struct JointLimitResult {
    double max_world_vel;       // Max world velocity respecting joint vel limits
    double max_world_acc;       // Max world accel respecting joint acc limits
    double max_world_jerk;      // Max world jerk (for S-curve planning)
    bool position_ok;           // True if joint positions are within soft limits
    int limiting_joint_vel;     // Joint index that limits velocity (-1 if none)
    int limiting_joint_acc;     // Joint index that limits acceleration
    int limiting_joint_jerk;    // Joint index that limits jerk
    double condition_number;    // Jacobian condition number (singularity indicator)

    JointLimitResult() :
        max_world_vel(1e9),
        max_world_acc(1e9),
        max_world_jerk(1e9),
        position_ok(true),
        limiting_joint_vel(-1),
        limiting_joint_acc(-1),
        limiting_joint_jerk(-1),
        condition_number(1.0) {}
};

/**
 * Joint limit calculator class
 *
 * Computes maximum world-space velocity/acceleration that respects
 * all joint limits, given the Jacobian at a pose.
 *
 * The relationship is:
 *   joint_vel = J × world_vel
 *   |joint_vel[j]| ≤ joint_limit[j].vel_limit for all j
 *
 * To find max world velocity, we solve:
 *   max_world_vel = min over all joints j of:
 *     joint_limit[j].vel_limit / |J[j] · direction|
 *
 * For a general direction, we use a conservative estimate:
 *   max_world_vel = min over all joints j of:
 *     joint_limit[j].vel_limit / max(|J[j][:]|)
 */
class JointLimitCalculator {
public:
    JointLimitCalculator();
    ~JointLimitCalculator();

    /**
     * Initialize with number of joints
     *
     * @param num_joints  Number of joints
     * @return true on success
     */
    bool init(int num_joints);

    /**
     * Set limits for a joint
     *
     * @param joint   Joint index (0 to num_joints-1)
     * @param limits  Limit configuration for this joint
     * @return true on success
     */
    bool setJointLimits(int joint, const JointLimitConfig& limits);

    /**
     * Update limits for all joints at once
     *
     * This is used to refresh limits from shared memory (motion status structure),
     * which reflects any runtime changes via HAL pins (ini.N.max_limit, etc.)
     *
     * @param vel_limits   Array of velocity limits [num_joints]
     * @param acc_limits   Array of acceleration limits [num_joints]
     * @param min_pos      Array of min position limits [num_joints]
     * @param max_pos      Array of max position limits [num_joints]
     * @param jerk_limits  Array of jerk limits [num_joints] (can be NULL)
     * @return true on success
     */
    bool updateAllLimits(const double* vel_limits,
                         const double* acc_limits,
                         const double* min_pos,
                         const double* max_pos,
                         const double* jerk_limits = nullptr);

    /**
     * Get limits for a joint
     */
    const JointLimitConfig& getJointLimits(int joint) const;

    /**
     * Get velocity limit for a specific joint
     */
    double getJointVelLimit(int joint) const;

    /**
     * Get acceleration limit for a specific joint
     */
    double getJointAccLimit(int joint) const;

    /**
     * Get jerk limit for a specific joint
     */
    double getJointJerkLimit(int joint) const;

    /**
     * Compute world-space limits at a pose given the Jacobian
     *
     * Uses conservative direction-independent bound (max |J[j][:]|).
     *
     * @param J             Jacobian matrix [joint][axis]
     * @param joint_pos     Current joint positions (for position limit check)
     * @param result        Output limit result
     * @param singularity_threshold  Condition number threshold for singularity
     * @return true on success
     */
    bool compute(const double J[9][9],
                 const double joint_pos[9],
                 JointLimitResult& result,
                 double singularity_threshold = 100.0);

    /**
     * Compute world-space limits for a specific path tangent direction
     *
     * Uses the actual path tangent to compute tight bounds. The tangent
     * is in world-axis units per unit of the Ruckig path parameter (which
     * may be XYZ arc length). Rotary components can be >> 1.0 when
     * rotary axes move much more than linear axes per unit path.
     *
     * The bound for each joint is:
     *   limit[j] / sum(|J[j][a]| * |tangent[a]|)
     *
     * @param J             Jacobian matrix [joint][axis]
     * @param joint_pos     Current joint positions (for position limit check)
     * @param tangent       Path tangent: d(world_axis)/d(path_param) [9]
     * @param result        Output limit result
     * @param singularity_threshold  Condition number threshold for singularity
     * @return true on success
     */
    bool computeForTangent(const double J[9][9],
                           const double joint_pos[9],
                           const double tangent[9],
                           JointLimitResult& result,
                           double singularity_threshold = 100.0);

    /**
     * Check if joint positions are within soft limits
     *
     * @param joint_pos  Array of joint positions
     * @return true if all joints within limits
     */
    bool checkPositionLimits(const double joint_pos[9]);

    /**
     * Get the number of joints
     */
    int getNumJoints() const { return num_joints_; }

private:
    /**
     * Compute maximum world velocity from joint velocity limits and Jacobian
     *
     * Uses conservative estimate: max over all directions
     */
    double computeMaxVelocity(const double J[9][9], int& limiting_joint);

    /**
     * Compute maximum world acceleration from joint accel limits and Jacobian
     */
    double computeMaxAcceleration(const double J[9][9], int& limiting_joint);

    /**
     * Compute maximum world jerk from joint jerk limits and Jacobian
     */
    double computeMaxJerk(const double J[9][9], int& limiting_joint);

    /**
     * Tangent-aware versions: use sum(|J[j][a]| * |tangent[a]|) instead of max(|J[j][a]|)
     */
    double computeMaxVelocityForTangent(const double J[9][9], const double tangent[9], int& limiting_joint);
    double computeMaxAccelerationForTangent(const double J[9][9], const double tangent[9], int& limiting_joint);
    double computeMaxJerkForTangent(const double J[9][9], const double tangent[9], int& limiting_joint);

    /**
     * Compute Jacobian condition number (simplified)
     */
    double computeConditionNumber(const double J[9][9]);

    int num_joints_;
    JointLimitConfig limits_[KINEMATICS_USER_MAX_JOINTS];
    bool initialized_;
};

} // namespace motion_planning

#endif // JOINT_LIMITS_HH
