/********************************************************************
 * Description: joint_limits.cc
 *   Joint limit calculation implementation for userspace kinematics trajectory planning
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#include "joint_limits.hh"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace motion_planning {

JointLimitCalculator::JointLimitCalculator()
    : num_joints_(0),
      initialized_(false) {
}

JointLimitCalculator::~JointLimitCalculator() {
}

bool JointLimitCalculator::init(int num_joints) {
    if (num_joints < 1 || num_joints > KINEMATICS_USER_MAX_JOINTS) {
        return false;
    }

    num_joints_ = num_joints;

    // Initialize with default (very permissive) limits
    for (int i = 0; i < KINEMATICS_USER_MAX_JOINTS; i++) {
        limits_[i] = JointLimitConfig();
    }

    initialized_ = true;
    return true;
}

bool JointLimitCalculator::setJointLimits(int joint, const JointLimitConfig& limits) {
    if (joint < 0 || joint >= num_joints_) {
        return false;
    }
    limits_[joint] = limits;
    return true;
}

const JointLimitConfig& JointLimitCalculator::getJointLimits(int joint) const {
    static JointLimitConfig default_limits;
    if (joint < 0 || joint >= num_joints_) {
        return default_limits;
    }
    return limits_[joint];
}

double JointLimitCalculator::getJointVelLimit(int joint) const {
    if (joint < 0 || joint >= num_joints_) return 1e9;
    return limits_[joint].vel_limit;
}

double JointLimitCalculator::getJointAccLimit(int joint) const {
    if (joint < 0 || joint >= num_joints_) return 1e9;
    return limits_[joint].acc_limit;
}

double JointLimitCalculator::getJointJerkLimit(int joint) const {
    if (joint < 0 || joint >= num_joints_) return 1e9;
    return limits_[joint].jerk_limit;
}

bool JointLimitCalculator::updateAllLimits(const double* vel_limits,
                                           const double* acc_limits,
                                           const double* min_pos,
                                           const double* max_pos,
                                           const double* jerk_limits) {
    if (!initialized_) {
        return false;
    }

    // Update limits from arrays
    // This is used to refresh limits from shared memory (motion status),
    // which reflects any runtime changes via HAL pins (ini.N.max_limit, etc.)
    for (int j = 0; j < num_joints_; j++) {
        if (vel_limits) limits_[j].vel_limit = vel_limits[j];
        if (acc_limits) limits_[j].acc_limit = acc_limits[j];
        if (min_pos) limits_[j].min_pos_limit = min_pos[j];
        if (max_pos) limits_[j].max_pos_limit = max_pos[j];
        if (jerk_limits) limits_[j].jerk_limit = jerk_limits[j];
    }

    return true;
}

bool JointLimitCalculator::checkPositionLimits(const double joint_pos[9]) {
    for (int j = 0; j < num_joints_; j++) {
        if (joint_pos[j] > limits_[j].max_pos_limit ||
            joint_pos[j] < limits_[j].min_pos_limit) {
            return false;
        }
    }
    return true;
}

double JointLimitCalculator::computeMaxVelocity(const double J[9][9], int& limiting_joint) {
    // Conservative estimate: assume worst-case direction
    // For each joint j, find the maximum Jacobian element magnitude
    // max_world_vel = min over j of: vel_limit[j] / max(|J[j][:]|)

    double max_world_vel = 1e18;
    limiting_joint = -1;

    for (int j = 0; j < num_joints_; j++) {
        // Find maximum absolute value in this row of J
        double max_abs_J = 0.0;
        for (int a = 0; a < AXIS_COUNT; a++) {
            double abs_J = std::fabs(J[j][a]);
            if (abs_J > max_abs_J) {
                max_abs_J = abs_J;
            }
        }

        if (max_abs_J > 1e-15) {
            // This joint contributes to motion
            double vel_limit_world = limits_[j].vel_limit / max_abs_J;
            if (vel_limit_world < max_world_vel) {
                max_world_vel = vel_limit_world;
                limiting_joint = j;
            }
        }
    }

    // Apply sanity bounds
    if (max_world_vel > 1e9) max_world_vel = 1e9;
    if (max_world_vel < 1e-9) max_world_vel = 1e-9;

    return max_world_vel;
}

double JointLimitCalculator::computeMaxAcceleration(const double J[9][9], int& limiting_joint) {
    // Same approach as velocity
    double max_world_acc = 1e18;
    limiting_joint = -1;

    for (int j = 0; j < num_joints_; j++) {
        double max_abs_J = 0.0;
        for (int a = 0; a < AXIS_COUNT; a++) {
            double abs_J = std::fabs(J[j][a]);
            if (abs_J > max_abs_J) {
                max_abs_J = abs_J;
            }
        }

        if (max_abs_J > 1e-15) {
            double acc_limit_world = limits_[j].acc_limit / max_abs_J;
            if (acc_limit_world < max_world_acc) {
                max_world_acc = acc_limit_world;
                limiting_joint = j;
            }
        }
    }

    if (max_world_acc > 1e9) max_world_acc = 1e9;
    if (max_world_acc < 1e-9) max_world_acc = 1e-9;

    return max_world_acc;
}

double JointLimitCalculator::computeMaxJerk(const double J[9][9], int& limiting_joint) {
    // Same approach as velocity and acceleration
    double max_world_jerk = 1e18;
    limiting_joint = -1;

    for (int j = 0; j < num_joints_; j++) {
        double max_abs_J = 0.0;
        for (int a = 0; a < AXIS_COUNT; a++) {
            double abs_J = std::fabs(J[j][a]);
            if (abs_J > max_abs_J) {
                max_abs_J = abs_J;
            }
        }

        if (max_abs_J > 1e-15) {
            double jerk_limit_world = limits_[j].jerk_limit / max_abs_J;
            if (jerk_limit_world < max_world_jerk) {
                max_world_jerk = jerk_limit_world;
                limiting_joint = j;
            }
        }
    }

    if (max_world_jerk > 1e9) max_world_jerk = 1e9;
    if (max_world_jerk < 1e-9) max_world_jerk = 1e-9;

    return max_world_jerk;
}

double JointLimitCalculator::computeMaxVelocityForTangent(const double J[9][9], const double tangent[9], int& limiting_joint) {
    double max_world_vel = 1e18;
    limiting_joint = -1;

    for (int j = 0; j < num_joints_; j++) {
        // Compute sum(|J[j][a]| * |tangent[a]|) — the actual amplification
        // for this joint along the given path direction
        double amplification = 0.0;
        for (int a = 0; a < AXIS_COUNT; a++) {
            amplification += std::fabs(J[j][a]) * std::fabs(tangent[a]);
        }

        if (amplification > 1e-15) {
            double vel_limit_world = limits_[j].vel_limit / amplification;
            if (vel_limit_world < max_world_vel) {
                max_world_vel = vel_limit_world;
                limiting_joint = j;
            }
        }
    }

    if (max_world_vel > 1e9) max_world_vel = 1e9;
    if (max_world_vel < 1e-9) max_world_vel = 1e-9;
    return max_world_vel;
}

double JointLimitCalculator::computeMaxAccelerationForTangent(const double J[9][9], const double tangent[9], int& limiting_joint) {
    double max_world_acc = 1e18;
    limiting_joint = -1;

    for (int j = 0; j < num_joints_; j++) {
        double amplification = 0.0;
        for (int a = 0; a < AXIS_COUNT; a++) {
            amplification += std::fabs(J[j][a]) * std::fabs(tangent[a]);
        }

        if (amplification > 1e-15) {
            double acc_limit_world = limits_[j].acc_limit / amplification;
            if (acc_limit_world < max_world_acc) {
                max_world_acc = acc_limit_world;
                limiting_joint = j;
            }
        }
    }

    if (max_world_acc > 1e9) max_world_acc = 1e9;
    if (max_world_acc < 1e-9) max_world_acc = 1e-9;
    return max_world_acc;
}

double JointLimitCalculator::computeMaxJerkForTangent(const double J[9][9], const double tangent[9], int& limiting_joint) {
    double max_world_jerk = 1e18;
    limiting_joint = -1;

    for (int j = 0; j < num_joints_; j++) {
        double amplification = 0.0;
        for (int a = 0; a < AXIS_COUNT; a++) {
            amplification += std::fabs(J[j][a]) * std::fabs(tangent[a]);
        }

        if (amplification > 1e-15) {
            double jerk_limit_world = limits_[j].jerk_limit / amplification;
            if (jerk_limit_world < max_world_jerk) {
                max_world_jerk = jerk_limit_world;
                limiting_joint = j;
            }
        }
    }

    if (max_world_jerk > 1e9) max_world_jerk = 1e9;
    if (max_world_jerk < 1e-9) max_world_jerk = 1e-9;
    return max_world_jerk;
}

bool JointLimitCalculator::computeForTangent(const double J[9][9],
                                              const double joint_pos[9],
                                              const double tangent[9],
                                              JointLimitResult& result,
                                              double singularity_threshold) {
    if (!initialized_) {
        return false;
    }

    result.position_ok = checkPositionLimits(joint_pos);
    result.condition_number = computeConditionNumber(J);

    result.max_world_vel = computeMaxVelocityForTangent(J, tangent, result.limiting_joint_vel);
    result.max_world_acc = computeMaxAccelerationForTangent(J, tangent, result.limiting_joint_acc);
    result.max_world_jerk = computeMaxJerkForTangent(J, tangent, result.limiting_joint_jerk);

    if (result.condition_number > singularity_threshold) {
        double slowdown_factor = singularity_threshold / result.condition_number;
        result.max_world_vel *= slowdown_factor;
        result.max_world_acc *= slowdown_factor;
        result.max_world_jerk *= slowdown_factor;
    }

    return true;
}

double JointLimitCalculator::computeConditionNumber(const double J[9][9]) {
    // Simplified condition number: ratio of max to min row norms
    double max_row_norm = 0.0;
    double min_row_norm = 1e18;

    for (int j = 0; j < num_joints_; j++) {
        double row_norm = 0.0;
        for (int a = 0; a < AXIS_COUNT; a++) {
            row_norm += J[j][a] * J[j][a];
        }
        row_norm = std::sqrt(row_norm);

        if (row_norm > max_row_norm) max_row_norm = row_norm;
        if (row_norm > 1e-15 && row_norm < min_row_norm) min_row_norm = row_norm;
    }

    if (min_row_norm < 1e-15) {
        return 1e18; // Near-singular
    }

    return max_row_norm / min_row_norm;
}

bool JointLimitCalculator::compute(const double J[9][9],
                                   const double joint_pos[9],
                                   JointLimitResult& result,
                                   double singularity_threshold) {
    if (!initialized_) {
        return false;
    }

    // Check position limits
    result.position_ok = checkPositionLimits(joint_pos);

    // Compute condition number
    result.condition_number = computeConditionNumber(J);

    // Compute max velocity
    result.max_world_vel = computeMaxVelocity(J, result.limiting_joint_vel);

    // Compute max acceleration
    result.max_world_acc = computeMaxAcceleration(J, result.limiting_joint_acc);

    // Compute max jerk
    result.max_world_jerk = computeMaxJerk(J, result.limiting_joint_jerk);

    // Apply singularity slowdown
    // If condition number exceeds threshold, reduce limits proportionally
    if (result.condition_number > singularity_threshold) {
        double slowdown_factor = singularity_threshold / result.condition_number;
        result.max_world_vel *= slowdown_factor;
        result.max_world_acc *= slowdown_factor;
        result.max_world_jerk *= slowdown_factor;
    }

    return true;
}

} // namespace motion_planning
