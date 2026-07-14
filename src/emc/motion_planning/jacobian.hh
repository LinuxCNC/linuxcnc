/********************************************************************
 * Description: jacobian.hh
 *   Jacobian calculation for userspace kinematics trajectory planning
 *
 * Computes the Jacobian matrix relating world velocities to joint
 * velocities. For trivkins this is the identity matrix.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/
#ifndef JACOBIAN_HH
#define JACOBIAN_HH

// emcpos.h includes posemath.h which has C++ function overloads
// so we can't use extern "C" around it
#include "emcpos.h"

extern "C" {
#include "kinematics_userspace/kinematics_user.h"
}

namespace motion_planning {

/**
 * Jacobian calculator class
 *
 * Computes the Jacobian matrix J where:
 *   joint_velocities = J × world_velocities
 *
 * For trivkins, J is the identity matrix (with appropriate axis mapping).
 * For non-trivial kinematics, J is computed via numerical differentiation.
 */
class JacobianCalculator {
public:
    JacobianCalculator();
    ~JacobianCalculator();

    /**
     * Initialize with kinematics context
     *
     * @param kins_ctx  Userspace kinematics context
     * @return true on success
     */
    bool init(KinematicsUserContext* kins_ctx);

    /**
     * Compute Jacobian at a given pose
     *
     * The Jacobian J[joint][axis] relates:
     *   d(joint[j])/dt = sum over axis a of J[j][a] * d(axis[a])/dt
     *
     * @param pose      World pose at which to compute Jacobian
     * @param J         Output 9×9 Jacobian matrix [joint][axis]
     * @return true on success, false on failure
     */
    bool compute(const EmcPose& pose, double J[9][9]);

    /**
     * Compute condition number of Jacobian
     *
     * The condition number indicates how close to a singularity the pose is.
     * High condition number = near singularity.
     *
     * For trivkins, always returns 1.0 (no singularities).
     *
     * @param J  Jacobian matrix
     * @return Condition number (≥ 1.0), or -1.0 on error
     */
    double conditionNumber(const double J[9][9]);

    /**
     * Check if current kinematics is identity (trivkins)
     */
    bool isIdentity() const { return is_identity_; }

private:
    /**
     * Compute Jacobian for trivkins (identity with axis mapping)
     */
    void computeTrivkins(double J[9][9]);

    /**
     * Compute Jacobian via numerical differentiation
     * Uses central differences: J[j][a] = (f(x+h) - f(x-h)) / (2h)
     */
    bool computeNumerical(const EmcPose& pose, double J[9][9]);

    KinematicsUserContext* kins_ctx_;
    bool is_identity_;
    int num_joints_;

    // Perturbation size for numerical differentiation (mm or degrees)
    // Must be large enough for kinematics to produce stable results
    // but small enough for accurate derivatives
    static constexpr double DELTA_LINEAR = 0.1;    // 0.1 mm
    static constexpr double DELTA_ROTARY = 0.1;    // 0.1 degrees
};

} // namespace motion_planning

#endif // JACOBIAN_HH
