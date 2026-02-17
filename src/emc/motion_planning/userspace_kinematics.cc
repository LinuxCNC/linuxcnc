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

extern "C" {
#include "blendmath.h"  // For pmCircleEffectiveMinRadius
}

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

    // Sample the path (line or circle based on motion type)
    std::vector<PathSample> samples;
    int num_samples;
    if (tc->motion_type == TC_CIRCULAR) {
        num_samples = path_sampler_.sampleCircle(start, end, tc->coords.circle, samples);
    } else if (tc->motion_type == TC_BEZIER) {
        num_samples = path_sampler_.sampleBezier(tc->coords.bezier, samples);
    } else {
        num_samples = path_sampler_.sampleLine(start, end, samples);
    }
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
    double trivkins_jerk_limit = 1e9;

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
            if (limit_calc_.getJointJerkLimit(j) < trivkins_jerk_limit) {
                trivkins_jerk_limit = limit_calc_.getJointJerkLimit(j);
            }
        }
    }

    // Fallback if no joints moving
    if (trivkins_vel_limit > 1e8) trivkins_vel_limit = tc->maxvel;
    if (trivkins_acc_limit > 1e8) trivkins_acc_limit = tc->maxaccel;
    if (trivkins_jerk_limit > 1e8) trivkins_jerk_limit = tc->maxjerk;

    // Step 2: For non-trivkins, compute Jacobian-based limits
    double jacobian_vel_limit = 1e9;
    double jacobian_acc_limit = 1e9;
    double jacobian_jerk_limit = 1e9;
    double max_condition_number = 1.0;

    if (!isIdentity()) {
        // Precompute chord tangent: exact for lines, fallback for circles
        // tangent[a] = d(world_axis[a]) / d(path_param)
        // For rotary-dominated moves, rotary components can be >> 1.0
        double chord_tangent[9] = {0};
        if (tc->target > 1e-12) {
            chord_tangent[AXIS_X] = (end.tran.x - start.tran.x) / tc->target;
            chord_tangent[AXIS_Y] = (end.tran.y - start.tran.y) / tc->target;
            chord_tangent[AXIS_Z] = (end.tran.z - start.tran.z) / tc->target;
            chord_tangent[AXIS_A] = (end.a - start.a) / tc->target;
            chord_tangent[AXIS_B] = (end.b - start.b) / tc->target;
            chord_tangent[AXIS_C] = (end.c - start.c) / tc->target;
            chord_tangent[AXIS_U] = (end.u - start.u) / tc->target;
            chord_tangent[AXIS_V] = (end.v - start.v) / tc->target;
            chord_tangent[AXIS_W] = (end.w - start.w) / tc->target;
        }

        bool is_circular = (tc->motion_type == TC_CIRCULAR);

        // Non-trivkins path: use Jacobian to compute world-space limits
        // Sample the path and find the most restrictive limits
        for (size_t i = 0; i < samples.size(); i++) {
            const auto& sample = samples[i];

            // Compute Jacobian at this sample point
            double J[9][9];
            if (!jacobian_calc_.compute(sample.world_pos, J)) {
                continue;
            }

            // Compute path tangent at this sample point
            // Lines: constant chord tangent (exact)
            // Circles: finite differences on sample positions (XYZ tangent rotates)
            double tangent[9];
            if (is_circular && samples.size() >= 3) {
                size_t i_prev = (i > 0) ? i - 1 : 0;
                size_t i_next = (i < samples.size() - 1) ? i + 1 : samples.size() - 1;
                double ds = samples[i_next].distance_from_start
                          - samples[i_prev].distance_from_start;
                if (ds > 1e-15) {
                    const EmcPose& pp = samples[i_prev].world_pos;
                    const EmcPose& pn = samples[i_next].world_pos;
                    tangent[AXIS_X] = (pn.tran.x - pp.tran.x) / ds;
                    tangent[AXIS_Y] = (pn.tran.y - pp.tran.y) / ds;
                    tangent[AXIS_Z] = (pn.tran.z - pp.tran.z) / ds;
                    tangent[AXIS_A] = (pn.a - pp.a) / ds;
                    tangent[AXIS_B] = (pn.b - pp.b) / ds;
                    tangent[AXIS_C] = (pn.c - pp.c) / ds;
                    tangent[AXIS_U] = (pn.u - pp.u) / ds;
                    tangent[AXIS_V] = (pn.v - pp.v) / ds;
                    tangent[AXIS_W] = (pn.w - pp.w) / ds;
                } else {
                    std::memcpy(tangent, chord_tangent, sizeof(tangent));
                }
            } else {
                std::memcpy(tangent, chord_tangent, sizeof(tangent));
            }

            // Compute world-space limits using path tangent at this sample
            JointLimitResult result;
            if (!limit_calc_.computeForTangent(J, sample.joints, tangent,
                                               result,
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
            if (result.max_world_jerk < jacobian_jerk_limit) {
                jacobian_jerk_limit = result.max_world_jerk;
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
    double min_jerk_limit;

    if (isIdentity()) {
        min_vel_limit = trivkins_vel_limit;
        min_acc_limit = trivkins_acc_limit;
        min_jerk_limit = trivkins_jerk_limit;
    } else {
        // Use the more restrictive of trivkins and Jacobian limits
        // Jacobian accounts for kinematic coupling (e.g. 5axiskins pivot)
        // that amplifies Cartesian motion into joint-space motion
        min_vel_limit = std::min(trivkins_vel_limit, jacobian_vel_limit);
        min_acc_limit = std::min(trivkins_acc_limit, jacobian_acc_limit);
        min_jerk_limit = std::min(trivkins_jerk_limit, jacobian_jerk_limit);
    }

    // Step 4: Apply centripetal velocity limit for circular motion
    // v_max = sqrt(a * r) to keep centripetal acceleration within limits
    if (tc->motion_type == TC_CIRCULAR) {
        double radius = pmCircleEffectiveMinRadius(&tc->coords.circle.xyz);
        if (radius > 1e-9) {
            double v_centripetal = std::sqrt(min_acc_limit * radius);
            if (v_centripetal < min_vel_limit) {
                min_vel_limit = v_centripetal;
            }
        }
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

    // Update TC limits based on joint limits
    // The backward pass will use these as additional constraints
    if (min_vel_limit < tc->maxvel) {
        tc->maxvel = min_vel_limit;
    }
    if (min_acc_limit < tc->maxaccel) {
        tc->maxaccel = min_acc_limit;
    }
    if (min_jerk_limit < tc->maxjerk || tc->maxjerk <= 0) {
        tc->maxjerk = min_jerk_limit;
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

bool UserspaceKinematicsPlanner::computeJacobian(const EmcPose& pose,
                                                  double J[9][9]) const {
    if (!enabled_ || !initialized_) return false;
    // jacobian_calc_ is mutable-safe (no state change in compute)
    return const_cast<JacobianCalculator&>(jacobian_calc_).compute(pose, J);
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
