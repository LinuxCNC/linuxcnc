/********************************************************************
 * Description: userspace_kinematics.cc
 *   Userspace kinematics integration implementation
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#include "userspace_kinematics.hh"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <vector>

// Include inihal for access to current joint limits (reflects HAL pin changes)
// The old_inihal_data structure is updated by inihal.cc when HAL pins change
// (ini.N.max_velocity, ini.N.max_limit, etc.)
#include "inihal.hh"
extern value_inihal_data old_inihal_data;

namespace motion_planning {

// Global userspace kinematics planner instance
UserspaceKinematicsPlanner g_userspace_kins_planner;

UserspaceKinematicsPlanner::UserspaceKinematicsPlanner()
    : initialized_(false),
      enabled_(false),
      kins_ctx_(nullptr) {
}

UserspaceKinematicsPlanner::~UserspaceKinematicsPlanner() {
    if (kins_ctx_) {
        kinematicsUserFree(kins_ctx_);
        kins_ctx_ = nullptr;
    }
}

bool UserspaceKinematicsPlanner::init(const UserspaceKinematicsConfig& config) {
    config_ = config;

    if (!config.enabled) {
        enabled_ = false;
        initialized_ = true;
        return true;
    }

    // Initialize userspace kinematics
    kins_ctx_ = kinematicsUserInit(config.kinematics_type.c_str(),
                                   config.num_joints,
                                   config.coordinates.c_str());
    if (!kins_ctx_) {
        // Kinematics type not supported
        enabled_ = false;
        initialized_ = false;
        return false;
    }

    // Initialize path sampler
    PathSamplerConfig sampler_config;
    sampler_config.min_samples = 10;
    sampler_config.max_samples = 100;
    sampler_config.sample_distance = 0.1;
    sampler_config.compute_jacobian = !kinematicsUserIsIdentity(kins_ctx_);

    if (!path_sampler_.init(kins_ctx_, sampler_config)) {
        enabled_ = false;
        initialized_ = false;
        return false;
    }

    // Initialize Jacobian calculator
    if (!jacobian_calc_.init(kins_ctx_)) {
        enabled_ = false;
        initialized_ = false;
        return false;
    }

    // Initialize joint limit calculator
    if (!limit_calc_.init(config.num_joints)) {
        enabled_ = false;
        initialized_ = false;
        return false;
    }

    enabled_ = true;
    initialized_ = true;
    return true;
}

bool UserspaceKinematicsPlanner::setJointLimits(int joint,
                                   double vel_limit,
                                   double acc_limit,
                                   double min_pos,
                                   double max_pos) {
    if (!initialized_) {
        return false;
    }

    JointLimitConfig limits;
    limits.vel_limit = vel_limit;
    limits.acc_limit = acc_limit;
    limits.min_pos_limit = min_pos;
    limits.max_pos_limit = max_pos;
    limits.jerk_limit = 1e9;  // Jerk limits deferred

    return limit_calc_.setJointLimits(joint, limits);
}

bool UserspaceKinematicsPlanner::isIdentity() const {
    if (!kins_ctx_) {
        return true;
    }
    return kinematicsUserIsIdentity(kins_ctx_) != 0;
}

bool UserspaceKinematicsPlanner::computeJointSpaceSegment(const EmcPose& start,
                                             const EmcPose& end,
                                             TC_STRUCT* tc) {
    if (!enabled_ || !tc) {
        return false;
    }

    // Refresh joint limits from inihal data before computing
    // This ensures we use current limits, including any runtime changes
    // via HAL pins (ini.N.max_limit, ini.N.max_velocity, etc.)
    // The old_inihal_data structure is updated by inihal.cc when HAL pins change.
    {
        double vel_limits[KINEMATICS_USER_MAX_JOINTS];
        double acc_limits[KINEMATICS_USER_MAX_JOINTS];
        double min_pos[KINEMATICS_USER_MAX_JOINTS];
        double max_pos[KINEMATICS_USER_MAX_JOINTS];
        double jerk_limits[KINEMATICS_USER_MAX_JOINTS];

        for (int j = 0; j < config_.num_joints && j < KINEMATICS_USER_MAX_JOINTS; j++) {
            vel_limits[j] = old_inihal_data.joint_max_velocity[j];
            acc_limits[j] = old_inihal_data.joint_max_acceleration[j];
            min_pos[j] = old_inihal_data.joint_min_limit[j];
            max_pos[j] = old_inihal_data.joint_max_limit[j];
            jerk_limits[j] = old_inihal_data.joint_jerk[j];
        }

        limit_calc_.updateAllLimits(vel_limits, acc_limits, min_pos, max_pos, jerk_limits);
    }

    // Sample the path
    std::vector<PathSample> samples;
    int num_samples = path_sampler_.sampleLine(start, end, samples);
    if (num_samples < 2) {
        return false;
    }

    // Compute joint positions at start and end
    double joints_start[KINEMATICS_USER_MAX_JOINTS];
    double joints_end[KINEMATICS_USER_MAX_JOINTS];

    if (kinematicsUserInverse(kins_ctx_, &start, joints_start) != 0) {
        return false;
    }
    if (kinematicsUserInverse(kins_ctx_, &end, joints_end) != 0) {
        return false;
    }

    // Compute velocity and acceleration limits
    // First compute trivkins-style limits (direct joint limits from moving joints)
    // Then for non-trivkins, also compute Jacobian-based limits
    // Use the more restrictive of the two, but with trivkins limits as a floor

    // Step 1: Compute trivkins-style limits (works for all kinematics)
    // Only apply limits from moving joints to prevent stationary
    // rotary axes from limiting linear moves
    double trivkins_vel_limit = 1e9;
    double trivkins_acc_limit = 1e9;

    bool joint_moving[KINEMATICS_USER_MAX_JOINTS] = {false};
    for (int j = 0; j < config_.num_joints; j++) {
        double delta = fabs(joints_end[j] - joints_start[j]);
        joint_moving[j] = (delta > 1e-9);
    }

    for (int j = 0; j < config_.num_joints; j++) {
        if (joint_moving[j]) {
            if (limit_calc_.getJointVelLimit(j) < trivkins_vel_limit) {
                trivkins_vel_limit = limit_calc_.getJointVelLimit(j);
            }
            if (limit_calc_.getJointAccLimit(j) < trivkins_acc_limit) {
                trivkins_acc_limit = limit_calc_.getJointAccLimit(j);
            }
        }
    }

    // Fallback if no joints moving
    if (trivkins_vel_limit > 1e8) trivkins_vel_limit = tc->maxvel;
    if (trivkins_acc_limit > 1e8) trivkins_acc_limit = tc->maxaccel;

    // Step 2: For non-trivkins, compute Jacobian-based limits
    double jacobian_vel_limit = 1e9;
    double jacobian_acc_limit = 1e9;
    double max_condition_number = 1.0;

    if (!isIdentity()) {
        // Non-trivkins path: use Jacobian to compute world-space limits
        // Sample the path and find the most restrictive limits
        for (const auto& sample : samples) {
            // Compute Jacobian at this sample point
            double J[9][9];
            if (!jacobian_calc_.compute(sample.world_pos, J)) {
                // Jacobian computation failed - skip this sample
                continue;
            }

            // Compute world-space limits from Jacobian and joint limits
            JointLimitResult result;
            if (!limit_calc_.compute(J, sample.joints, result,
                                     config_.singularity_threshold)) {
                continue;
            }

            // Track minimum limits across all samples
            if (result.max_world_vel < jacobian_vel_limit) {
                jacobian_vel_limit = result.max_world_vel;
            }
            if (result.max_world_acc < jacobian_acc_limit) {
                jacobian_acc_limit = result.max_world_acc;
            }

            // Track worst-case condition number (for diagnostics)
            if (result.condition_number > max_condition_number) {
                max_condition_number = result.condition_number;
            }
        }
    }

    // Step 3: Combine limits
    // For trivkins: use trivkins limits directly
    // For non-trivkins: use Jacobian limits but floor at trivkins limits
    // This ensures we never go slower than the old working code
    double min_vel_limit;
    double min_acc_limit;

    if (isIdentity()) {
        min_vel_limit = trivkins_vel_limit;
        min_acc_limit = trivkins_acc_limit;
    } else {
        // Use Jacobian limits if valid, but floor at trivkins limits
        // TODO: Once Jacobian is validated, this floor can be removed
        // and we can use Jacobian limits to properly slow down near singularities
        min_vel_limit = trivkins_vel_limit;  // Use trivkins for now
        min_acc_limit = trivkins_acc_limit;

        // Log if Jacobian would have been more restrictive (for debugging)
        (void)jacobian_vel_limit;  // Suppress unused warning
        (void)jacobian_acc_limit;
        (void)max_condition_number;
    }

    // Populate JointSpaceSegment
    JointSpaceSegment* js = &tc->joint_space;
    js->num_joints = config_.num_joints;

    for (int j = 0; j < config_.num_joints; j++) {
        js->start[j] = joints_start[j];
        js->end[j] = joints_end[j];
    }
    for (int j = config_.num_joints; j < JOINT_SPACE_MAX_JOINTS; j++) {
        js->start[j] = 0.0;
        js->end[j] = 0.0;
    }

    js->vel_limit_start = min_vel_limit;
    js->vel_limit_end = min_vel_limit;
    js->acc_limit = min_acc_limit;
    js->valid = 1;

    // Update TC velocity limits based on joint limits
    // The backward pass will use these as additional constraints
    if (min_vel_limit < tc->maxvel) {
        tc->maxvel = min_vel_limit;
    }
    if (min_acc_limit < tc->maxaccel) {
        tc->maxaccel = min_acc_limit;
    }

    // Ensure reqvel doesn't exceed maxvel
    if (tc->reqvel > tc->maxvel) {
        tc->reqvel = tc->maxvel;
    }

    return true;
}

bool UserspaceKinematicsPlanner::evaluateJointPositions(const TC_STRUCT* tc,
                                           double progress,
                                           double* joints) {
    if (!tc || !joints) {
        return false;
    }

    const JointSpaceSegment* js = &tc->joint_space;
    if (!js->valid) {
        return false;
    }

    // Clamp progress to [0, 1]
    if (progress < 0.0) progress = 0.0;
    if (progress > 1.0) progress = 1.0;

    // Linear interpolation
    for (int j = 0; j < js->num_joints; j++) {
        joints[j] = js->start[j] + progress * (js->end[j] - js->start[j]);
    }

    // Zero unused joints
    for (int j = js->num_joints; j < KINEMATICS_USER_MAX_JOINTS; j++) {
        joints[j] = 0.0;
    }

    return true;
}

// C interface implementations

extern "C" int userspace_kins_init(const char* kins_type,
                                   int num_joints,
                                   const char* coordinates) {
    UserspaceKinematicsConfig config;
    config.enabled = true;
    config.kinematics_type = kins_type ? kins_type : "trivkins";
    config.num_joints = num_joints;
    config.coordinates = coordinates ? coordinates : "";
    config.singularity_threshold = 100.0;

    if (!g_userspace_kins_planner.init(config)) {
        return -1;
    }
    return 0;
}

extern "C" int userspace_kins_set_joint_limits(int joint,
                                               double vel_limit,
                                               double acc_limit,
                                               double min_pos,
                                               double max_pos) {
    if (!g_userspace_kins_planner.setJointLimits(joint, vel_limit, acc_limit, min_pos, max_pos)) {
        return -1;
    }
    return 0;
}

extern "C" int userspace_kins_is_enabled(void) {
    return g_userspace_kins_planner.isEnabled() ? 1 : 0;
}

extern "C" int userspace_kins_compute_joint_segment(const EmcPose* start,
                                                    const EmcPose* end,
                                                    TC_STRUCT* tc) {
    if (!start || !end || !tc) {
        return -1;
    }
    if (!g_userspace_kins_planner.computeJointSpaceSegment(*start, *end, tc)) {
        return -1;
    }
    return 0;
}

extern "C" int userspace_kins_evaluate_joints(const TC_STRUCT* tc,
                                              double progress,
                                              double* joints) {
    if (!g_userspace_kins_planner.evaluateJointPositions(tc, progress, joints)) {
        return -1;
    }
    return 0;
}

} // namespace motion_planning
