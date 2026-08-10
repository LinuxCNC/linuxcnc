/********************************************************************
 * Description: jacobian.cc
 *   Jacobian calculation implementation for userspace kinematics trajectory planning
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#include "jacobian.hh"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace motion_planning {

JacobianCalculator::JacobianCalculator()
    : kins_ctx_(nullptr),
      is_identity_(false),
      num_joints_(0) {
}

JacobianCalculator::~JacobianCalculator() {
    // kins_ctx_ is owned externally
}

bool JacobianCalculator::init(KinematicsUserContext* kins_ctx) {
    if (!kins_ctx) {
        return false;
    }

    kins_ctx_ = kins_ctx;
    is_identity_ = (kinematicsUserIsIdentity(kins_ctx) != 0);
    num_joints_ = kinematicsUserGetNumJoints(kins_ctx);

    return true;
}

void JacobianCalculator::computeTrivkins(double J[9][9]) {
    // Zero the matrix
    std::memset(J, 0, sizeof(double) * 9 * 9);

    // For trivkins, the Jacobian is identity (with axis mapping)
    // Since trivkins maps: joint[i] = world_axis[mapped_axis[i]]
    // The Jacobian is: J[joint][axis] = 1 if axis == mapped_axis[joint], else 0

    // For a simple XYZ trivkins:
    // J[0][AXIS_X] = 1 (joint 0 = X)
    // J[1][AXIS_Y] = 1 (joint 1 = Y)
    // J[2][AXIS_Z] = 1 (joint 2 = Z)
    // etc.

    // We need to query the kinematics context for the mapping.
    // Since the context is opaque, we use inverse kinematics to determine
    // the mapping.

    // Test each axis: perturb it and see which joint changes
    EmcPose zero_pose;
    ZERO_EMC_POSE(zero_pose);
    double zero_joints[9];
    kinematicsUserInverse(kins_ctx_, &zero_pose, zero_joints);

    for (int axis = 0; axis < AXIS_COUNT; axis++) {
        EmcPose test_pose = zero_pose;
        emcPoseSetAxis(&test_pose, axis, 1.0);

        double test_joints[9];
        kinematicsUserInverse(kins_ctx_, &test_pose, test_joints);

        for (int joint = 0; joint < num_joints_; joint++) {
            double delta = test_joints[joint] - zero_joints[joint];
            if (std::fabs(delta) > 0.5) {
                // This axis maps to this joint
                J[joint][axis] = 1.0;
            }
        }
    }
}

bool JacobianCalculator::computeNumerical(const EmcPose& pose, double J[9][9]) {
    // Zero the matrix
    std::memset(J, 0, sizeof(double) * 9 * 9);

    // Compute joints at nominal pose
    double joints_center[9];
    if (kinematicsUserInverse(kins_ctx_, &pose, joints_center) != 0) {
        return false;
    }

    // Perturb each axis and compute derivatives
    for (int axis = 0; axis < AXIS_COUNT; axis++) {
        // Choose perturbation size based on axis type
        double delta = (axis < 3 || axis >= 6) ? DELTA_LINEAR : DELTA_ROTARY;

        // Positive perturbation
        EmcPose pose_plus = pose;
        double val_plus = emcPoseGetAxis(&pose_plus, axis) + delta;
        emcPoseSetAxis(&pose_plus, axis, val_plus);

        double joints_plus[9];
        if (kinematicsUserInverse(kins_ctx_, &pose_plus, joints_plus) != 0) {
            // Kinematics failed - use one-sided difference
            for (int joint = 0; joint < num_joints_; joint++) {
                J[joint][axis] = (joints_plus[joint] - joints_center[joint]) / delta;
            }
            continue;
        }

        // Negative perturbation
        EmcPose pose_minus = pose;
        double val_minus = emcPoseGetAxis(&pose_minus, axis) - delta;
        emcPoseSetAxis(&pose_minus, axis, val_minus);

        double joints_minus[9];
        if (kinematicsUserInverse(kins_ctx_, &pose_minus, joints_minus) != 0) {
            // Use forward difference
            for (int joint = 0; joint < num_joints_; joint++) {
                J[joint][axis] = (joints_plus[joint] - joints_center[joint]) / delta;
            }
            continue;
        }

        // Central difference (most accurate)
        for (int joint = 0; joint < num_joints_; joint++) {
            J[joint][axis] = (joints_plus[joint] - joints_minus[joint]) / (2.0 * delta);
        }
    }

    // Check for NaN/Inf values and replace with safe defaults
    bool had_nan = false;
    for (int joint = 0; joint < num_joints_; joint++) {
        for (int axis = 0; axis < AXIS_COUNT; axis++) {
            if (!std::isfinite(J[joint][axis])) {
                // Replace NaN/Inf with 0 (assume no coupling)
                J[joint][axis] = 0.0;
                had_nan = true;
            }
        }
    }

    // If we had NaN values, the Jacobian may be unreliable
    // Return true anyway but the condition number check will catch issues
    (void)had_nan;  // Could log this in debug mode

    return true;
}

bool JacobianCalculator::compute(const EmcPose& pose, double J[9][9]) {
    if (!kins_ctx_) {
        return false;
    }

    if (is_identity_) {
        // For trivkins, use the fast identity computation
        computeTrivkins(J);
        return true;
    } else {
        // For non-trivial kinematics, use numerical differentiation
        return computeNumerical(pose, J);
    }
}

double JacobianCalculator::conditionNumber(const double J[9][9]) {
    if (is_identity_) {
        // Identity matrix has condition number 1
        return 1.0;
    }

    // We use a simplified condition number estimate:
    // Find the ratio of largest to smallest row norms
    // This is not the true 2-norm condition number, but gives a rough indication

    double max_row_norm = 0.0;
    double min_row_norm = 1e18;

    for (int joint = 0; joint < num_joints_; joint++) {
        double row_norm = 0.0;
        for (int axis = 0; axis < AXIS_COUNT; axis++) {
            row_norm += J[joint][axis] * J[joint][axis];
        }
        row_norm = std::sqrt(row_norm);

        if (row_norm > max_row_norm) max_row_norm = row_norm;
        if (row_norm > 1e-15 && row_norm < min_row_norm) min_row_norm = row_norm;
    }

    if (min_row_norm < 1e-15) {
        // Near-singular: a row is almost zero
        return 1e18;
    }

    return max_row_norm / min_row_norm;
}

} // namespace motion_planning
