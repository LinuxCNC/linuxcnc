#pragma once

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <tuple>
#include <type_traits>

#include "block.hpp"
#include "brake.hpp"
#include "input_parameter.hpp"
#include "profile.hpp"
#include "position.hpp"
#include "trajectory.hpp"
#include "velocity.hpp"

namespace ruckig {

//! Calculation class for a state-to-state trajectory.
template<size_t DOFs>
class TargetCalculator {
private:
    template<class T> using Vector = typename std::conditional<DOFs >= 1, std::array<T, DOFs>, std::vector<T>>::type;
    template<class T> using VectorIntervals = typename std::conditional<DOFs >= 1, std::array<T, 3*DOFs+1>, std::vector<T>>::type;

    constexpr static double eps {std::numeric_limits<double>::epsilon()};

    Vector<double> new_max_jerk; // For phase synchronization
    Vector<double> pd;
    VectorIntervals<double> possible_t_syncs;
    VectorIntervals<int> idx;

    Vector<Block> blocks;
    Vector<double> inp_min_velocity, inp_min_acceleration;

    Vector<ControlInterface> inp_per_dof_control_interface;
    Vector<Synchronization> inp_per_dof_synchronization;

    //! Is the trajectory (in principle) phase synchronizable?
    bool is_input_collinear(const InputParameter<DOFs>& inp, const Vector<double>& jMax, Profile::Direction limiting_direction, size_t limiting_dof, Vector<double>& new_max_jerk) {
        // Get scaling factor of first DoF
        bool pd_found_nonzero {false};
        double v0_scale, a0_scale, vf_scale, af_scale;
        for (size_t dof = 0; dof < pd.size(); ++dof) {
            if (inp_per_dof_synchronization[dof] != Synchronization::Phase) {
                continue;
            }

            pd[dof] = inp.target_position[dof] - inp.current_position[dof];

            if (!pd_found_nonzero && std::abs(pd[dof]) > eps) {
                v0_scale = inp.current_velocity[dof] / pd[dof];
                a0_scale = inp.current_acceleration[dof] / pd[dof];
                vf_scale = inp.target_velocity[dof] / pd[dof];
                af_scale = inp.target_acceleration[dof] / pd[dof];
                pd_found_nonzero = true;
            }
        }

        if (!pd_found_nonzero) { // position difference is zero everywhere...
            return false;
        }

        const double max_jerk_limiting = (limiting_direction == Profile::Direction::UP) ? jMax[limiting_dof] : -jMax[limiting_dof];
        constexpr double eps_colinear {10 * eps};
        
        for (size_t dof = 0; dof < pd.size(); ++dof) {
            if (dof == limiting_dof || inp_per_dof_synchronization[dof] != Synchronization::Phase) {
                continue;
            }

            // Are the vectors collinear?
            if (
                std::abs(inp.current_velocity[dof] - v0_scale * pd[dof]) > eps_colinear
                || std::abs(inp.current_acceleration[dof] - a0_scale * pd[dof]) > eps_colinear
                || std::abs(inp.target_velocity[dof] - vf_scale * pd[dof]) > eps_colinear
                || std::abs(inp.target_acceleration[dof] - af_scale * pd[dof]) > eps_colinear
            ) {
                return false;
            }

            const double scale = pd[dof] / pd[limiting_dof];
            new_max_jerk[dof] = scale * max_jerk_limiting;
        }

        return true;
    }

    bool synchronize(std::optional<double> t_min, double& t_sync, int& limiting_dof, Vector<Profile>& profiles, bool discrete_duration, double delta_time) {
        if (degrees_of_freedom == 1 && !t_min && !discrete_duration) {
            limiting_dof = 0;
            t_sync = blocks[0].t_min;
            profiles[0] = blocks[0].p_min;
            return true;
        }

        // Possible t_syncs are the start times of the intervals and optional t_min
        bool any_interval {false};
        for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
            // Ignore DoFs without synchronization here
            if (inp_per_dof_synchronization[dof] == Synchronization::None) {
                possible_t_syncs[dof] = 0.0;
                possible_t_syncs[degrees_of_freedom + dof] = std::numeric_limits<double>::infinity();
                possible_t_syncs[2 * degrees_of_freedom + dof] = std::numeric_limits<double>::infinity();
                continue;
            }

            possible_t_syncs[dof] = blocks[dof].t_min;
            possible_t_syncs[degrees_of_freedom + dof] = blocks[dof].a ? blocks[dof].a->right : std::numeric_limits<double>::infinity();
            possible_t_syncs[2 * degrees_of_freedom + dof] = blocks[dof].b ? blocks[dof].b->right : std::numeric_limits<double>::infinity();
            any_interval |= blocks[dof].a || blocks[dof].b;
        }
        possible_t_syncs[3 * degrees_of_freedom] = t_min.value_or(std::numeric_limits<double>::infinity());
        any_interval |= t_min.has_value();

        if (discrete_duration) {
            for (size_t i = 0; i < possible_t_syncs.size(); ++i) {
                possible_t_syncs[i] = std::ceil(possible_t_syncs[i] / delta_time) * delta_time;
            }
        }

        // Test them in sorted order
        auto idx_end = any_interval ? idx.end() : idx.begin() + degrees_of_freedom;
        std::iota(idx.begin(), idx_end, 0);
        std::sort(idx.begin(), idx_end, [&possible_t_syncs=possible_t_syncs](size_t i, size_t j) { return possible_t_syncs[i] < possible_t_syncs[j]; });

        // Start at last tmin (or worse)
        for (auto i = idx.begin() + degrees_of_freedom - 1; i != idx_end; ++i) {
            const double possible_t_sync = possible_t_syncs[*i];            
            bool is_blocked {false};
            for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
                if (inp_per_dof_synchronization[dof] == Synchronization::None) {
                    continue; // inner dof loop
                }
                if (blocks[dof].is_blocked(possible_t_sync)) {
                    is_blocked = true;
                    break; // inner dof loop
                }
            }
            if (is_blocked || possible_t_sync < t_min.value_or(0.0)) {
                continue;
            }

            t_sync = possible_t_sync;
            if (*i == 3*degrees_of_freedom) { // Optional t_min
                limiting_dof = -1;
                return true;
            }

            const auto div = std::div(*i, degrees_of_freedom);
            limiting_dof = div.rem;
            switch (div.quot) {
                case 0: {
                    profiles[limiting_dof] = blocks[limiting_dof].p_min;
                } break;
                case 1: {
                    profiles[limiting_dof] = blocks[limiting_dof].a->profile;
                } break;
                case 2: {
                    profiles[limiting_dof] = blocks[limiting_dof].b->profile;
                } break;
            }
            return true;
        }

        return false;
    }

public:
    size_t degrees_of_freedom;

    template <size_t D = DOFs, typename std::enable_if<D >= 1, int>::type = 0>
    explicit TargetCalculator(): degrees_of_freedom(DOFs) { }

    template <size_t D = DOFs, typename std::enable_if<D == 0, int>::type = 0>
    explicit TargetCalculator(size_t dofs): degrees_of_freedom(dofs) {
        blocks.resize(dofs);
        inp_min_velocity.resize(dofs);
        inp_min_acceleration.resize(dofs);
        inp_per_dof_control_interface.resize(dofs);
        inp_per_dof_synchronization.resize(dofs);
        new_max_jerk.resize(dofs);
        pd.resize(dofs);
        possible_t_syncs.resize(3*dofs+1);
        idx.resize(3*dofs+1);
    }

    //! Calculate the time-optimal waypoint-based trajectory
    template<bool throw_error, bool return_error_at_maximal_duration>
    Result calculate(const InputParameter<DOFs>& inp, Trajectory<DOFs>& traj, double delta_time, bool& was_interrupted) {
        was_interrupted = false;

        for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
            auto& p = traj.profiles[0][dof];
    
            if (!inp.enabled[dof]) {
                p.p.back() = inp.current_position[dof];
                p.v.back() = inp.current_velocity[dof];
                p.a.back() = inp.current_acceleration[dof];
                p.t_sum.back() = 0.0;
                blocks[dof].t_min = 0.0;
                blocks[dof].a = std::nullopt;
                blocks[dof].b = std::nullopt;
                continue;
            }

            inp_min_velocity[dof] = inp.min_velocity ? inp.min_velocity.value()[dof] : -inp.max_velocity[dof];
            inp_min_acceleration[dof] = inp.min_acceleration ? inp.min_acceleration.value()[dof] : -inp.max_acceleration[dof];
            inp_per_dof_control_interface[dof] = inp.per_dof_control_interface ? inp.per_dof_control_interface.value()[dof] : inp.control_interface;
            inp_per_dof_synchronization[dof] = inp.per_dof_synchronization ? inp.per_dof_synchronization.value()[dof] : inp.synchronization;

            // Calculate brake (if input exceeds or will exceed limits)
            switch (inp_per_dof_control_interface[dof]) {
                case ControlInterface::Position: {
                    p.brake.get_position_brake_trajectory(inp.current_velocity[dof], inp.current_acceleration[dof], inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]);
                    // p.accel.get_position_brake_trajectory(inp.target_velocity[dof], inp.target_acceleration[dof], inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]);
                    p.set_boundary(inp.current_position[dof], inp.current_velocity[dof], inp.current_acceleration[dof], inp.target_position[dof], inp.target_velocity[dof], inp.target_acceleration[dof]);
                } break;
                case ControlInterface::Velocity: {
                    p.brake.get_velocity_brake_trajectory(inp.current_acceleration[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]);
                    // p.accel.get_velocity_brake_trajectory(inp.target_acceleration[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]);
                    p.set_boundary(inp.current_position[dof], inp.current_velocity[dof], inp.current_acceleration[dof], inp.target_velocity[dof], inp.target_acceleration[dof]);
                } break;
            }

            // Finalize pre & post-trajectories
            p.brake.finalize(p.p[0], p.v[0], p.a[0]);
            // p.accel.finalize(p.pf, p.vf, p.af);

            bool found_profile;
            switch (inp_per_dof_control_interface[dof]) {
                case ControlInterface::Position: {
                    PositionStep1 step1 {p.p[0], p.v[0], p.a[0], p.pf, p.vf, p.af, inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]};
                    found_profile = step1.get_profile(p, blocks[dof]);
                } break;
                case ControlInterface::Velocity: {
                    VelocityStep1 step1 {p.v[0], p.a[0], p.vf, p.af, inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]};
                    found_profile = step1.get_profile(p, blocks[dof]);
                } break;
            }

            if (!found_profile) {
                if constexpr (throw_error) {
                    throw std::runtime_error("[ruckig] error in step 1, dof: " + std::to_string(dof) + " input: " + inp.to_string());
                }
                return Result::ErrorExecutionTimeCalculation;
            }

            traj.independent_min_durations[dof] = blocks[dof].p_min.brake.duration + blocks[dof].t_min;
            // std::cout << dof << " profile step1: " << blocks[dof].to_string() << std::endl;
        }

        int limiting_dof; // The DoF that doesn't need step 2
        const bool discrete_duration = (inp.duration_discretization == DurationDiscretization::Discrete);
        const bool found_synchronization = synchronize(inp.minimum_duration, traj.duration, limiting_dof, traj.profiles[0], discrete_duration, delta_time);
        if (!found_synchronization) {
            if constexpr (throw_error) {
                throw std::runtime_error("[ruckig] error in time synchronization: " + std::to_string(traj.duration));
            }
            return Result::ErrorSynchronizationCalculation;
        }

        // None Synchronization
        for (size_t dof = 0; dof < blocks.size(); ++dof) {
            if (inp.enabled[dof] && inp_per_dof_synchronization[dof] == Synchronization::None) {
                traj.profiles[0][dof] = blocks[dof].p_min;
                if (blocks[dof].t_min > traj.duration) {
                    traj.duration = blocks[dof].t_min;
                    limiting_dof = dof;
                }
            }
        }
        traj.cumulative_times[0] = traj.duration;

        if constexpr (return_error_at_maximal_duration) {
            if (traj.duration > 7.6e3) {
                return Result::ErrorTrajectoryDuration;
            }
        }

        if (traj.duration == 0.0) {
            // Copy all profiles for end state
            for (size_t dof = 0; dof < blocks.size(); ++dof) {
                traj.profiles[0][dof] = blocks[dof].p_min;
            }
            return Result::Working;
        }

        if (std::all_of(inp_per_dof_synchronization.begin(), inp_per_dof_synchronization.end(), [](Synchronization s){ return s == Synchronization::None; })) {
            return Result::Working;
        }

        // Phase Synchronization
        if (std::any_of(inp_per_dof_synchronization.begin(), inp_per_dof_synchronization.end(), [](Synchronization s){ return s == Synchronization::Phase; }) && std::all_of(inp_per_dof_control_interface.begin(), inp_per_dof_control_interface.end(), [](ControlInterface s){ return s == ControlInterface::Position; })) {
            const Profile& p_limiting = traj.profiles[0][limiting_dof];
            if (is_input_collinear(inp, inp.max_jerk, p_limiting.direction, limiting_dof, new_max_jerk)) {
                bool found_time_synchronization {true};
                for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
                    if (!inp.enabled[dof] || dof == limiting_dof || inp_per_dof_synchronization[dof] != Synchronization::Phase) {
                        continue;
                    }

                    Profile& p = traj.profiles[0][dof];
                    const double t_profile = traj.duration - p.brake.duration - p.accel.duration;

                    p.t = p_limiting.t; // Copy timing information from limiting DoF
                    p.jerk_signs = p_limiting.jerk_signs;

                    // Profile::Limits::NONE is a small hack, as there is no specialization for that in the check function
                    switch (p.jerk_signs) {
                        case Profile::JerkSigns::UDDU: {
                            if (!p.check_with_timing<Profile::JerkSigns::UDDU, Profile::Limits::NONE>(t_profile, new_max_jerk[dof], inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof])) {
                                found_time_synchronization = false;
                            }
                        } break;
                        case Profile::JerkSigns::UDUD: {
                            if (!p.check_with_timing<Profile::JerkSigns::UDUD, Profile::Limits::NONE>(t_profile, new_max_jerk[dof], inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof])) {
                                found_time_synchronization = false;
                            }
                        } break;
                    }

                    p.limits = p_limiting.limits; // After check method call to set correct limits
                }

                if (found_time_synchronization && std::all_of(inp_per_dof_synchronization.begin(), inp_per_dof_synchronization.end(), [](Synchronization s){ return s == Synchronization::Phase || s == Synchronization::None; })) {
                    return Result::Working;
                }
            }
        }

        // Time Synchronization
        for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
            if (!inp.enabled[dof] || dof == limiting_dof || inp_per_dof_synchronization[dof] == Synchronization::None) {
                continue;
            }

            Profile& p = traj.profiles[0][dof];
            const double t_profile = traj.duration - p.brake.duration - p.accel.duration;

            if (inp_per_dof_synchronization[dof] == Synchronization::TimeIfNecessary && std::abs(inp.target_velocity[dof]) < eps && std::abs(inp.target_acceleration[dof]) < eps) {
                p = blocks[dof].p_min;
                continue;
            }

            // Check if the final time corresponds to an extremal profile calculated in step 1
            if (std::abs(t_profile - blocks[dof].t_min) < eps) {
                p = blocks[dof].p_min;
                continue;
            } else if (blocks[dof].a && std::abs(t_profile - blocks[dof].a->right) < eps) {
                p = blocks[dof].a->profile;
                continue;
            } else if (blocks[dof].b && std::abs(t_profile - blocks[dof].b->right) < eps) {
                p = blocks[dof].b->profile;
                continue;
            }

            bool found_time_synchronization;
            switch (inp_per_dof_control_interface[dof]) {
                case ControlInterface::Position: {
                    PositionStep2 step2 {t_profile, p.p[0], p.v[0], p.a[0], p.pf, p.vf, p.af, inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]};
                    found_time_synchronization = step2.get_profile(p);
                } break;
                case ControlInterface::Velocity: {
                    VelocityStep2 step2 {t_profile, p.v[0], p.a[0], p.vf, p.af, inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]};
                    found_time_synchronization = step2.get_profile(p);
                } break;
            }
            if (!found_time_synchronization) {
                if constexpr (throw_error) {
                    throw std::runtime_error("[ruckig] error in step 2 in dof: " + std::to_string(dof) + " for t sync: " + std::to_string(traj.duration) + " input: " + inp.to_string());
                }
                return Result::ErrorSynchronizationCalculation;
            }
            // std::cout << dof << " profile step2: " << p.to_string() << std::endl;
        }

        return Result::Working;
    }

    //! Continue the trajectory calculation
    template<bool throw_error, bool return_error_at_maximal_duration>
    Result continue_calculation(const InputParameter<DOFs>&, Trajectory<DOFs>&, double, bool&) {
        return Result::Error;
    }
};

} // namespace ruckig
