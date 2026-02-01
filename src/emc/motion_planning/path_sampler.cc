/********************************************************************
 * Description: path_sampler.cc
 *   Path sampling implementation for userspace kinematics trajectory planning
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/

#include "path_sampler.hh"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace motion_planning {

PathSampler::PathSampler()
    : kins_ctx_(nullptr),
      initialized_(false) {
}

PathSampler::~PathSampler() {
    // Note: kins_ctx_ is owned externally, don't free it
}

bool PathSampler::init(KinematicsUserContext* kins_ctx,
                       const PathSamplerConfig& config) {
    if (!kins_ctx) {
        return false;
    }

    kins_ctx_ = kins_ctx;
    config_ = config;

    // For trivkins, don't need to compute Jacobian
    if (kinematicsUserIsIdentity(kins_ctx_)) {
        config_.compute_jacobian = false;
    }

    initialized_ = true;
    return true;
}

void PathSampler::interpolatePose(const EmcPose& start,
                                  const EmcPose& end,
                                  double t,
                                  EmcPose& out) {
    // Linear interpolation for all 9 axes
    out.tran.x = start.tran.x + t * (end.tran.x - start.tran.x);
    out.tran.y = start.tran.y + t * (end.tran.y - start.tran.y);
    out.tran.z = start.tran.z + t * (end.tran.z - start.tran.z);
    out.a = start.a + t * (end.a - start.a);
    out.b = start.b + t * (end.b - start.b);
    out.c = start.c + t * (end.c - start.c);
    out.u = start.u + t * (end.u - start.u);
    out.v = start.v + t * (end.v - start.v);
    out.w = start.w + t * (end.w - start.w);
}

double PathSampler::poseDistance(const EmcPose& p1, const EmcPose& p2) {
    // Euclidean distance in 9D space
    // Weight rotary axes differently (convert degrees to equivalent linear)
    const double rotary_scale = 1.0; // mm per degree (adjustable)

    double dx = p2.tran.x - p1.tran.x;
    double dy = p2.tran.y - p1.tran.y;
    double dz = p2.tran.z - p1.tran.z;
    double da = (p2.a - p1.a) * rotary_scale;
    double db = (p2.b - p1.b) * rotary_scale;
    double dc = (p2.c - p1.c) * rotary_scale;
    double du = p2.u - p1.u;
    double dv = p2.v - p1.v;
    double dw = p2.w - p1.w;

    return std::sqrt(dx*dx + dy*dy + dz*dz +
                     da*da + db*db + dc*dc +
                     du*du + dv*dv + dw*dw);
}

bool PathSampler::computeJoints(const EmcPose& world_pos, PathSample& sample) {
    if (!kins_ctx_) {
        return false;
    }

    // Copy world position
    sample.world_pos = world_pos;

    // Compute inverse kinematics
    if (kinematicsUserInverse(kins_ctx_, &world_pos, sample.joints) != 0) {
        sample.valid = false;
        return false;
    }

    // Initialize limits to high values (will be set by joint limit calculator)
    sample.max_vel = 1e9;
    sample.max_acc = 1e9;

    // Zero Jacobian for now (computed by JacobianCalculator if needed)
    std::memset(sample.jacobian, 0, sizeof(sample.jacobian));
    sample.condition_number = 1.0; // Identity for trivkins

    sample.valid = true;
    return true;
}

int PathSampler::sampleLine(const EmcPose& start,
                            const EmcPose& end,
                            std::vector<PathSample>& samples) {
    if (!initialized_) {
        return -1;
    }

    samples.clear();

    // Compute path length
    double path_length = poseDistance(start, end);

    // Determine number of samples
    int num_samples;
    if (path_length < 1e-9) {
        // Zero-length move: just sample endpoints
        num_samples = 2;
    } else {
        // Calculate based on sample_distance
        num_samples = static_cast<int>(std::ceil(path_length / config_.sample_distance)) + 1;
    }

    // Clamp to min/max
    num_samples = std::max(num_samples, config_.min_samples);
    num_samples = std::min(num_samples, config_.max_samples);

    // Reserve space
    samples.reserve(num_samples);

    // Sample the path
    for (int i = 0; i < num_samples; i++) {
        double t = (num_samples > 1) ?
                   static_cast<double>(i) / (num_samples - 1) :
                   0.0;

        PathSample sample;
        EmcPose pose;
        interpolatePose(start, end, t, pose);

        if (!computeJoints(pose, sample)) {
            // Kinematics failed at this point
            sample.valid = false;
        }

        sample.distance_from_start = t * path_length;
        samples.push_back(sample);
    }

    return static_cast<int>(samples.size());
}

int PathSampler::sampleCircle(const EmcPose& start,
                              const EmcPose& end,
                              const EmcPose& center,
                              int plane,
                              int direction,
                              std::vector<PathSample>& samples) {
    // Phase 4: Implement proper arc sampling
    // For now, tessellate as a chord (straight line)
    // This is a placeholder that will be replaced with proper arc handling

    (void)center;  // Unused for now
    (void)plane;
    (void)direction;

    // Just sample as a line for now
    return sampleLine(start, end, samples);
}

} // namespace motion_planning
