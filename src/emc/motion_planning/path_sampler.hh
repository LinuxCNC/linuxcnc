/********************************************************************
 * Description: path_sampler.hh
 *   Path sampling for userspace kinematics trajectory planning
 *
 * Samples paths in world coordinates for kinematic analysis.
 * Each sample point is used to compute Jacobian and joint limits.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 ********************************************************************/
#ifndef PATH_SAMPLER_HH
#define PATH_SAMPLER_HH

#include <vector>

// emcpos.h includes posemath.h which has C++ function overloads
#include "emcpos.h"

extern "C" {
#include "kinematics_userspace/kinematics_user.h"
#include "tc_types.h"  // For PmCircle9
}

namespace motion_planning {

/**
 * A single sample point along a path
 */
struct PathSample {
    EmcPose world_pos;          // World coordinates at this sample
    double joints[KINEMATICS_USER_MAX_JOINTS];  // Joint positions
    double max_vel;             // Max velocity at this point (from joint limits)
    double max_acc;             // Max acceleration at this point
    double distance_from_start; // Distance along path from segment start
    double jacobian[9][9];      // Jacobian at this point (for non-trivkins)
    double condition_number;    // Jacobian condition number (singularity detection)
    bool valid;                 // True if sample is valid (kin solved, limits ok)
};

/**
 * Configuration for path sampling
 */
struct PathSamplerConfig {
    int min_samples;            // Minimum samples per segment (default: 10)
    int max_samples;            // Maximum samples per segment (default: 1000)
    double sample_distance;     // Target distance between samples (mm, default: 0.1)
    bool compute_jacobian;      // Whether to compute Jacobian (false for trivkins)

    PathSamplerConfig() :
        min_samples(10),
        max_samples(1000),
        sample_distance(0.1),
        compute_jacobian(false) {}
};

/**
 * Path sampler class
 *
 * Samples paths in world coordinates and computes joint positions.
 * Currently supports linear moves with trivkins.
 */
class PathSampler {
public:
    PathSampler();
    ~PathSampler();

    /**
     * Initialize with kinematics context
     *
     * @param kins_ctx  Userspace kinematics context
     * @param config    Sampler configuration
     * @return true on success
     */
    bool init(KinematicsUserContext* kins_ctx,
              const PathSamplerConfig& config = PathSamplerConfig());

    /**
     * Sample a linear path (G1 move)
     *
     * @param start     Start position in world coordinates
     * @param end       End position in world coordinates
     * @param samples   Output vector of samples (cleared first)
     * @return Number of samples generated, or -1 on error
     */
    int sampleLine(const EmcPose& start,
                   const EmcPose& end,
                   std::vector<PathSample>& samples);

    /**
     * Sample a circular arc (G2/G3)
     *
     * @param start     Start position in world coordinates
     * @param end       End position in world coordinates
     * @param circle    PmCircle9 arc geometry (initialized by pmCircle9Init)
     * @param samples   Output vector of samples (cleared first)
     * @return Number of samples generated, or -1 on error
     */
    int sampleCircle(const EmcPose& start,
                     const EmcPose& end,
                     const PmCircle9& circle,
                     std::vector<PathSample>& samples);

    /**
     * Get the current configuration
     */
    const PathSamplerConfig& getConfig() const { return config_; }

    /**
     * Set configuration
     */
    void setConfig(const PathSamplerConfig& config) { config_ = config; }

private:
    /**
     * Compute joint positions for a world pose
     *
     * @param world_pos World coordinates
     * @param sample    Sample to fill in
     * @return true on success
     */
    bool computeJoints(const EmcPose& world_pos, PathSample& sample);

    /**
     * Linear interpolation between two poses
     *
     * @param start  Start pose
     * @param end    End pose
     * @param t      Interpolation parameter [0, 1]
     * @param out    Output pose
     */
    static void interpolatePose(const EmcPose& start,
                                const EmcPose& end,
                                double t,
                                EmcPose& out);

    /**
     * Compute distance between two poses
     */
    static double poseDistance(const EmcPose& p1, const EmcPose& p2);

    KinematicsUserContext* kins_ctx_;
    PathSamplerConfig config_;
    bool initialized_;
};

} // namespace motion_planning

#endif // PATH_SAMPLER_HH
