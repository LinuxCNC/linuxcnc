/********************************************************************
 * Description: jacobian.hh
 *   Jacobian calculation for userspace kinematics trajectory planning
 *
 * Computes the Jacobian matrix relating world velocities to joint
 * velocities, from the module's own closed form through the non-RT
 * kinematics loader.
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
#include <emcpos.h>

extern "C" {
#include <kinematics_user.h>
}

namespace motion_planning {

/**
 * Jacobian calculator class
 *
 * Computes the Jacobian matrix J where:
 *   joint_velocities = J × world_velocities
 *
 * The module answers: a closed form where it has one, its inverse
 * differenced where it does not.  See kinematicsUserJacobian().
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
    KinematicsUserContext* kins_ctx_;
    bool is_identity_;
    int num_joints_;
};

} // namespace motion_planning

#endif // JACOBIAN_HH
