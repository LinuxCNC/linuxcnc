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

bool JacobianCalculator::compute(const EmcPose& pose, double J[9][9]) {
    if (!kins_ctx_) {
        return false;
    }
    std::memset(J, 0, sizeof(double) * 9 * 9);
    return kinematicsUserJacobian(kins_ctx_, &pose, J) == 0;
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
