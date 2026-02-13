/********************************************************************
 * Description: userspace_kinematics.hh
 *   Userspace kinematics integration for trajectory planning
 *
 * This module integrates the userspace kinematics components:
 * - Path sampling
 * - Jacobian calculation
 * - Joint limit enforcement
 * - Joint-space segment generation
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/
#ifndef USERSPACE_KINEMATICS_HH
#define USERSPACE_KINEMATICS_HH

#include <string>
#include "path_sampler.hh"
#include "jacobian.hh"
#include "joint_limits.hh"

// tc_types.h and emcpos.h include posemath.h which has C++ overloads
#include "tc_types.h"
#include "emcpos.h"

namespace motion_planning {

/**
 * Userspace kinematics planner configuration
 */
struct UserspaceKinematicsConfig {
    bool enabled;                   // True if userspace kinematics path is active
    std::string kinematics_type;    // Kinematics type string (e.g., "trivkins")
    int num_joints;                 // Number of joints
    std::string coordinates;        // Coordinate mapping (e.g., "XYZABC")
    double singularity_threshold;   // Condition number threshold for slowdown

    UserspaceKinematicsConfig() :
        enabled(false),
        kinematics_type("trivkins"),
        num_joints(3),
        coordinates("XYZ"),
        singularity_threshold(100.0) {}
};

/**
 * Userspace kinematics planner class
 *
 * Integrates path sampling, Jacobian calculation, and joint limits
 * to compute joint-space segments from world-space moves.
 */
class UserspaceKinematicsPlanner {
public:
    UserspaceKinematicsPlanner();
    ~UserspaceKinematicsPlanner();

    /**
     * Initialize the userspace kinematics planner
     *
     * @param config  Configuration including kinematics type and joints
     * @return true on success, false if kinematics not supported
     */
    bool init(const UserspaceKinematicsConfig& config);

    /**
     * Set joint limits from motion controller configuration
     *
     * @param joint      Joint index
     * @param vel_limit  Maximum velocity
     * @param acc_limit  Maximum acceleration
     * @param min_pos    Minimum position (soft limit)
     * @param max_pos    Maximum position (soft limit)
     * @return true on success
     */
    bool setJointLimits(int joint,
                        double vel_limit,
                        double acc_limit,
                        double min_pos,
                        double max_pos);

    /**
     * Compute joint-space segment for a linear move
     *
     * Samples the path, computes Jacobian at each sample, determines
     * limiting velocity/acceleration from joint limits, and populates
     * the JointSpaceSegment in the TC_STRUCT.
     *
     * @param start  Start position in world coordinates
     * @param end    End position in world coordinates
     * @param tc     TC_STRUCT to populate with joint_space data
     * @return true on success
     */
    bool computeJointSpaceSegment(const EmcPose& start,
                                  const EmcPose& end,
                                  TC_STRUCT* tc);

    /**
     * Evaluate joint positions at a given progress through segment
     *
     * Uses linear interpolation:
     *   joint[j] = start[j] + progress * (end[j] - start[j])
     *
     * @param tc        TC_STRUCT with joint_space data
     * @param progress  Progress through segment [0, 1]
     * @param joints    Output array of joint positions
     * @return true on success
     */
    bool evaluateJointPositions(const TC_STRUCT* tc,
                                double progress,
                                double* joints);

    /**
     * Check if userspace kinematics planner is enabled and initialized
     */
    bool isEnabled() const { return enabled_ && initialized_; }

    /**
     * Check if using identity kinematics
     */
    bool isIdentity() const;

    /**
     * Get the current configuration
     */
    const UserspaceKinematicsConfig& getConfig() const { return config_; }

    /**
     * Get per-joint jerk limit (for direction-dependent kink velocity computation)
     */
    double getJointJerkLimit(int joint) const { return limit_calc_.getJointJerkLimit(joint); }

    /**
     * Get per-joint acceleration limit
     */
    double getJointAccLimit(int joint) const { return limit_calc_.getJointAccLimit(joint); }

    /**
     * Get number of joints
     */
    int getNumJoints() const { return limit_calc_.getNumJoints(); }

    /**
     * Compute Jacobian at a world pose
     *
     * @param pose  World-space pose
     * @param J     Output 9x9 Jacobian matrix
     * @return true on success
     */
    bool computeJacobian(const EmcPose& pose, double J[9][9]) const;

private:
    UserspaceKinematicsConfig config_;
    bool initialized_;
    bool enabled_;

    KinematicsUserContext* kins_ctx_;
    PathSampler path_sampler_;
    JacobianCalculator jacobian_calc_;
    JointLimitCalculator limit_calc_;
};

/**
 * Global userspace kinematics planner instance
 *
 * This is automatically initialized when PLANNER_TYPE=2 in the INI file.
 */
extern UserspaceKinematicsPlanner g_userspace_kins_planner;

/**
 * Initialize userspace kinematics planner from INI configuration
 *
 * Called during motion controller initialization if userspace kinematics is enabled.
 *
 * @param kins_type    Kinematics type string
 * @param num_joints   Number of joints
 * @param coordinates  Coordinate mapping string
 * @return 0 on success, -1 on failure
 */
extern "C" int userspace_kins_init(const char* kins_type,
                                   int num_joints,
                                   const char* coordinates);

/**
 * Set joint limits for userspace kinematics planner
 *
 * Called after userspace_kins_init for each joint.
 */
extern "C" int userspace_kins_set_joint_limits(int joint,
                                               double vel_limit,
                                               double acc_limit,
                                               double min_pos,
                                               double max_pos);

/**
 * Check if userspace kinematics planner is enabled
 */
extern "C" int userspace_kins_is_enabled(void);

/**
 * Compute joint-space segment (C interface)
 */
extern "C" int userspace_kins_compute_joint_segment(const EmcPose* start,
                                                    const EmcPose* end,
                                                    TC_STRUCT* tc);

/**
 * Evaluate joint positions at progress (C interface)
 */
extern "C" int userspace_kins_evaluate_joints(const TC_STRUCT* tc,
                                              double progress,
                                              double* joints);

} // namespace motion_planning

#endif // USERSPACE_KINEMATICS_HH
