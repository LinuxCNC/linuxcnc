/*
 * ruckig_compat.hpp - C++ compatibility shim for cruckig
 *
 * Provides a minimal ruckig:: namespace API on top of the cruckig C library,
 * so that C++ code written for the original Ruckig library can use cruckig
 * with minimal changes.
 *
 * Only supports 1-DOF (template parameter N=1), which is all that
 * motion_planning_9d.cc needs.
 */
#ifndef CRUCKIG_RUCKIG_COMPAT_HPP
#define CRUCKIG_RUCKIG_COMPAT_HPP

#include <array>
#include <vector>
#include <cstring>

extern "C" {
#include "cruckig.h"
}

namespace ruckig {

enum class Result {
    Working  = CRuckigWorking,
    Finished = CRuckigFinished,
    Error    = CRuckigError,
    ErrorInvalidInput = CRuckigErrorInvalidInput,
};

enum class ControlInterface {
    Position = CRuckigPosition,
    Velocity = CRuckigVelocity,
};

/* 1-DOF InputParameter compatible with ruckig::InputParameter<1> */
template <size_t N>
struct InputParameter {
    static_assert(N == 1, "cruckig compat shim only supports 1 DOF");

    std::array<double, 1> current_position = {0};
    std::array<double, 1> current_velocity = {0};
    std::array<double, 1> current_acceleration = {0};
    std::array<double, 1> target_position = {0};
    std::array<double, 1> target_velocity = {0};
    std::array<double, 1> target_acceleration = {0};
    std::array<double, 1> max_velocity = {0};
    std::array<double, 1> max_acceleration = {0};
    std::array<double, 1> max_jerk = {0};
    ControlInterface control_interface = ControlInterface::Position;
};

/* Wrapper around CRuckigProfile providing the same member access as C++ Ruckig */
struct ProfileView {
    double t[7];
    double j[7];

    struct BrakeView {
        double duration;
        double t[2];
        double j[2];
    } brake;
};

/* 1-DOF Trajectory compatible with ruckig::Trajectory<1> */
template <size_t N>
class Trajectory {
    static_assert(N == 1, "cruckig compat shim only supports 1 DOF");

    CRuckigTrajectory *traj_ = nullptr;
    bool owns_ = false;
    ProfileView cached_profile_;
    bool profile_cached_ = false;

public:
    Trajectory() : traj_(cruckig_trajectory_create(1)), owns_(true) {}
    ~Trajectory() { if (owns_ && traj_) cruckig_trajectory_destroy(traj_); }

    Trajectory(const Trajectory &) = delete;
    Trajectory &operator=(const Trajectory &) = delete;
    Trajectory(Trajectory &&o) noexcept : traj_(o.traj_), owns_(o.owns_), profile_cached_(false) {
        o.traj_ = nullptr; o.owns_ = false;
    }
    Trajectory &operator=(Trajectory &&o) noexcept {
        if (this != &o) {
            if (owns_ && traj_) cruckig_trajectory_destroy(traj_);
            traj_ = o.traj_; owns_ = o.owns_; profile_cached_ = false;
            o.traj_ = nullptr; o.owns_ = false;
        }
        return *this;
    }

    CRuckigTrajectory *c_ptr() { return traj_; }
    const CRuckigTrajectory *c_ptr() const { return traj_; }

    double get_duration() const {
        return traj_ ? cruckig_trajectory_get_duration(traj_) : 0.0;
    }

    void at_time(double t, std::array<double, 1> &pos,
                 std::array<double, 1> &vel,
                 std::array<double, 1> &acc) const {
        if (traj_) {
            cruckig_trajectory_at_time_simple(traj_, t, &pos[0], &vel[0], &acc[0]);
        }
    }

    /* Provide access to profiles in the same structure as C++ Ruckig:
     * profiles[section][dof] → CRuckigProfile
     * For 1-DOF single-section: profiles[0][0] */
    using ProfileList = std::vector<std::vector<ProfileView>>;

    ProfileList get_profiles() const {
        ProfileList result;
        if (!traj_ || !traj_->profiles) return result;

        for (size_t s = 0; s < traj_->num_sections; s++) {
            std::vector<ProfileView> section;
            const CRuckigProfile &cp = traj_->profiles[s * traj_->degrees_of_freedom];
            ProfileView pv;
            for (int i = 0; i < 7; i++) { pv.t[i] = cp.t[i]; pv.j[i] = cp.j[i]; }
            pv.brake.duration = cp.brake.duration;
            pv.brake.t[0] = cp.brake.t[0]; pv.brake.t[1] = cp.brake.t[1];
            pv.brake.j[0] = cp.brake.j[0]; pv.brake.j[1] = cp.brake.j[1];
            section.push_back(pv);
            result.push_back(std::move(section));
        }
        return result;
    }
};

/* 1-DOF Ruckig planner compatible with ruckig::Ruckig<1> */
template <size_t N>
class Ruckig {
    static_assert(N == 1, "cruckig compat shim only supports 1 DOF");

    CRuckig *otg_ = nullptr;

public:
    explicit Ruckig(double delta_time) : otg_(cruckig_create(1, delta_time)) {}
    ~Ruckig() { if (otg_) cruckig_destroy(otg_); }

    Ruckig(const Ruckig &) = delete;
    Ruckig &operator=(const Ruckig &) = delete;

    Result calculate(const InputParameter<1> &input, Trajectory<1> &traj) {
        /* Fill cruckig input from the C++ wrapper */
        CRuckigInputParameter *cinp = cruckig_input_create(1);
        if (!cinp) return Result::Error;

        cinp->current_position[0]     = input.current_position[0];
        cinp->current_velocity[0]     = input.current_velocity[0];
        cinp->current_acceleration[0] = input.current_acceleration[0];
        cinp->target_position[0]      = input.target_position[0];
        cinp->target_velocity[0]      = input.target_velocity[0];
        cinp->target_acceleration[0]  = input.target_acceleration[0];
        cinp->max_velocity[0]         = input.max_velocity[0];
        cinp->max_acceleration[0]     = input.max_acceleration[0];
        cinp->max_jerk[0]             = input.max_jerk[0];
        // min_velocity and min_acceleration are NULL in cruckig (optional).
        // cruckig uses -max when NULL, which matches ruckig's default.
        cinp->control_interface       = static_cast<CRuckigControlInterface>(input.control_interface);

        CRuckigResult cresult = cruckig_calculate(otg_, cinp, traj.c_ptr());
        cruckig_input_destroy(cinp);

        return static_cast<Result>(cresult);
    }
};

} // namespace ruckig

#endif /* CRUCKIG_RUCKIG_COMPAT_HPP */
