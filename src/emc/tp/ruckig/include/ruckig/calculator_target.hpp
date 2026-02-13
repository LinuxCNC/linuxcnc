#pragma once

#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <ruckig/optional.hpp>
#include <tuple>
#include <type_traits>

#include <ruckig/block.hpp>
#include <ruckig/brake.hpp>
#include <ruckig/error.hpp>
#include <ruckig/input_parameter.hpp>
#include <ruckig/profile.hpp>
#include <ruckig/position.hpp>
#include <ruckig/trajectory.hpp>
#include <ruckig/velocity.hpp>


namespace ruckig {

//! Calculation class for a state-to-state trajectory.
template<size_t DOFs, template<class, size_t> class CustomVector = StandardVector>
class TargetCalculator {
private:
    template<class T> using Vector = CustomVector<T, DOFs>;
    template<class T> using StandardVectorIntervals = StandardSizeVector<T, DOFs, 3*DOFs+1>;

    constexpr static double eps {std::numeric_limits<double>::epsilon()};
    constexpr static bool return_error_at_maximal_duration {true};

    Vector<double> new_phase_control, pd; // For phase synchronization
    StandardVectorIntervals<double> possible_t_syncs;
    StandardVectorIntervals<size_t> idx;

    StandardVector<Block, DOFs> blocks;
    StandardVector<double, DOFs> inp_min_velocity, inp_min_acceleration;

    StandardVector<ControlInterface, DOFs> inp_per_dof_control_interface;
    StandardVector<Synchronization, DOFs> inp_per_dof_synchronization;

    //! Is the trajectory (in principle) phase synchronizable?
    bool is_input_collinear(const InputParameter<DOFs, CustomVector>& inp, Profile::Direction limiting_direction, size_t limiting_dof) {
        // Check that vectors pd, v0, a0, vf, af are collinear
        for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
            pd[dof] = inp.target_position[dof] - inp.current_position[dof];
        }

        const Vector<double>* scale_vector = nullptr;
        Optional<size_t> scale_dof; // Need to find a scale DOF because limiting DOF might not be phase synchronized
        for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
            if (inp_per_dof_synchronization[dof] != Synchronization::Phase) {
                continue;
            }

            if (inp_per_dof_control_interface[dof] == ControlInterface::Position && std::abs(pd[dof]) > eps) {
                scale_vector = &pd;
                scale_dof = dof;
                break;

            } else if (std::abs(inp.current_velocity[dof]) > eps) {
                scale_vector = &inp.current_velocity;
                scale_dof = dof;
                break;

            } else if (std::abs(inp.current_acceleration[dof]) > eps) {
                scale_vector = &inp.current_acceleration;
                scale_dof = dof;
                break;

            } else if (std::abs(inp.target_velocity[dof]) > eps) {
                scale_vector = &inp.target_velocity;
                scale_dof = dof;
                break;

            } else if (std::abs(inp.target_acceleration[dof]) > eps) {
                scale_vector = &inp.target_acceleration;
                scale_dof = dof;
                break;
            }
        }

        if (!scale_dof) {
            return false; // Zero everywhere is in theory collinear, but that trivial case is better handled elsewhere
        }

        const double scale = scale_vector->operator[](*scale_dof);
        const double pd_scale = pd[*scale_dof] / scale;
        const double v0_scale = inp.current_velocity[*scale_dof] / scale;
        const double vf_scale = inp.target_velocity[*scale_dof] / scale;
        const double a0_scale = inp.current_acceleration[*scale_dof] / scale;
        const double af_scale = inp.target_acceleration[*scale_dof] / scale;

        const double scale_limiting = scale_vector->operator[](limiting_dof);
        double control_limiting = (limiting_direction == Profile::Direction::UP) ? inp.max_jerk[limiting_dof] : -inp.max_jerk[limiting_dof];
        if (std::isinf(inp.max_jerk[limiting_dof])) {
            control_limiting = (limiting_direction == Profile::Direction::UP) ? inp.max_acceleration[limiting_dof] : inp_min_acceleration[limiting_dof];
        }

        for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
            if (inp_per_dof_synchronization[dof] != Synchronization::Phase) {
                continue;
            }

            const double current_scale = scale_vector->operator[](dof);
            if (
                (inp_per_dof_control_interface[dof] == ControlInterface::Position && std::abs(pd[dof] - pd_scale * current_scale) > eps)
                || std::abs(inp.current_velocity[dof] - v0_scale * current_scale) > eps
                || std::abs(inp.current_acceleration[dof] - a0_scale * current_scale) > eps
                || std::abs(inp.target_velocity[dof] - vf_scale * current_scale) > eps
                || std::abs(inp.target_acceleration[dof] - af_scale * current_scale) > eps
            ) {
                return false;
            }

            new_phase_control[dof] = control_limiting * current_scale / scale_limiting;
        }

        return true;
    }

    bool synchronize(Optional<double> t_min, double& t_sync, Optional<size_t>& limiting_dof, Vector<Profile>& profiles, bool discrete_duration, double delta_time) {
        // Check for (degrees_of_freedom == 1 && !t_min && !discrete_duration) is now outside

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
            for (auto& possible_t_sync: possible_t_syncs) {
                if (std::isinf(possible_t_sync)) {
                    continue;
                }

                const double remainder = std::fmod(possible_t_sync, delta_time); // in [0, delta_time)
                if (remainder > eps) {
                    possible_t_sync += delta_time - remainder;
                }
            }
        }

        // Test them in sorted order
        auto idx_end = any_interval ? idx.end() : idx.begin() + degrees_of_freedom;
        std::iota(idx.begin(), idx_end, 0);
        std::sort(idx.begin(), idx_end, [&](size_t i, size_t j) { return possible_t_syncs[i] < possible_t_syncs[j]; });

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
            if (is_blocked || possible_t_sync < t_min.value_or(0.0) || std::isinf(possible_t_sync)) {
                continue;
            }

            t_sync = possible_t_sync;
            if (*i == 3*degrees_of_freedom) { // Optional t_min
                limiting_dof = nullopt;
                return true;
            }

            const auto div = std::div(static_cast<long>(*i), static_cast<long>(degrees_of_freedom));
            limiting_dof = div.rem;
            switch (div.quot) {
                case 0: {
                    profiles[limiting_dof.value()] = blocks[limiting_dof.value()].p_min;
                } break;
                case 1: {
                    profiles[limiting_dof.value()] = blocks[limiting_dof.value()].a->profile;
                } break;
                case 2: {
                    profiles[limiting_dof.value()] = blocks[limiting_dof.value()].b->profile;
                } break;
            }
            return true;
        }

        return false;
    }

public:
    size_t degrees_of_freedom;

    template<size_t D = DOFs, typename std::enable_if<(D >= 1), int>::type = 0>
    explicit TargetCalculator(): degrees_of_freedom(DOFs) { }

    template<size_t D = DOFs, typename std::enable_if<(D == 0), int>::type = 0>
    explicit TargetCalculator(size_t dofs): degrees_of_freedom(dofs) {
        blocks.resize(dofs);
        inp_min_velocity.resize(dofs);
        inp_min_acceleration.resize(dofs);
        inp_per_dof_control_interface.resize(dofs);
        inp_per_dof_synchronization.resize(dofs);
        new_phase_control.resize(dofs);
        pd.resize(dofs);
        possible_t_syncs.resize(3*dofs+1);
        idx.resize(3*dofs+1);
    }

    //! Calculate the time-optimal waypoint-based trajectory
    template<bool throw_error>
    Result calculate(const InputParameter<DOFs, CustomVector>& inp, Trajectory<DOFs, CustomVector>& traj, double delta_time, bool& was_interrupted) {
        was_interrupted = false;
#if defined WITH_CLOUD_CLIENT
        traj.resize(0);
#endif

        for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
            auto& p = traj.profiles[0][dof];

            inp_min_velocity[dof] = inp.min_velocity ? inp.min_velocity.value()[dof] : -inp.max_velocity[dof];
            inp_min_acceleration[dof] = inp.min_acceleration ? inp.min_acceleration.value()[dof] : -inp.max_acceleration[dof];
            inp_per_dof_control_interface[dof] = inp.per_dof_control_interface ? inp.per_dof_control_interface.value()[dof] : inp.control_interface;
            inp_per_dof_synchronization[dof] = inp.per_dof_synchronization ? inp.per_dof_synchronization.value()[dof] : inp.synchronization;

            if (!inp.enabled[dof]) {
                p.p.back() = inp.current_position[dof];
                p.v.back() = inp.current_velocity[dof];
                p.a.back() = inp.current_acceleration[dof];
                p.t_sum.back() = 0.0;
                blocks[dof].t_min = 0.0;
                blocks[dof].a = nullopt;
                blocks[dof].b = nullopt;
                continue;
            }

            // Calculate brake (if input exceeds or will exceed limits)
            switch (inp_per_dof_control_interface[dof]) {
                case ControlInterface::Position: {
                    if (!std::isinf(inp.max_jerk[dof])) {
                        p.brake.get_position_brake_trajectory(inp.current_velocity[dof], inp.current_acceleration[dof], inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]);
                        // p.accel.get_position_brake_trajectory(inp.target_velocity[dof], inp.target_acceleration[dof], inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]);
                    } else if (!std::isinf(inp.max_acceleration[dof])) {
                        p.brake.get_second_order_position_brake_trajectory(inp.current_velocity[dof], inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof]);
                        // p.accel.get_second_order_position_brake_trajectory(inp.target_velocity[dof], inp.target_acceleration[dof], inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof]);
                    }
                    p.set_boundary(inp.current_position[dof], inp.current_velocity[dof], inp.current_acceleration[dof], inp.target_position[dof], inp.target_velocity[dof], inp.target_acceleration[dof]);
                } break;
                case ControlInterface::Velocity: {
                    if (!std::isinf(inp.max_jerk[dof])) {
                        p.brake.get_velocity_brake_trajectory(inp.current_acceleration[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]);
                        // p.accel.get_velocity_brake_trajectory(inp.target_acceleration[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]);
                    } else {
                        p.brake.get_second_order_velocity_brake_trajectory();
                        // p.accel.get_second_order_velocity_brake_trajectory();
                    }
                    p.set_boundary_for_velocity(inp.current_position[dof], inp.current_velocity[dof], inp.current_acceleration[dof], inp.target_velocity[dof], inp.target_acceleration[dof]);
                } break;
            }

            // Finalize pre & post-trajectories
            if (!std::isinf(inp.max_jerk[dof])) {
                p.brake.finalize(p.p[0], p.v[0], p.a[0]);
                // p.accel.finalize(p.pf, p.vf, p.af);
            } else if (!std::isinf(inp.max_acceleration[dof])) {
                p.brake.finalize_second_order(p.p[0], p.v[0], p.a[0]);
                // p.accel.finalize_second_order(p.pf, p.vf, p.af);
            }

            bool found_profile {false};
            switch (inp_per_dof_control_interface[dof]) {
                case ControlInterface::Position: {
                    if (!std::isinf(inp.max_jerk[dof])) {
                        PositionThirdOrderStep1 step1 {p.p[0], p.v[0], p.a[0], p.pf, p.vf, p.af, inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]};
                        found_profile = step1.get_profile(p, blocks[dof]);
                    } else if (!std::isinf(inp.max_acceleration[dof])) {
                        PositionSecondOrderStep1 step1 {p.p[0], p.v[0], p.pf, p.vf, inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof]};
                        found_profile = step1.get_profile(p, blocks[dof]);
                    } else {
                        PositionFirstOrderStep1 step1 {p.p[0], p.pf, inp.max_velocity[dof], inp_min_velocity[dof]};
                        found_profile = step1.get_profile(p, blocks[dof]);
                    }
                } break;
                case ControlInterface::Velocity: {
                    if (!std::isinf(inp.max_jerk[dof])) {
                        VelocityThirdOrderStep1 step1 {p.v[0], p.a[0], p.vf, p.af, inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]};
                        found_profile = step1.get_profile(p, blocks[dof]);
                    } else {
                        VelocitySecondOrderStep1 step1 {p.v[0], p.vf, inp.max_acceleration[dof], inp_min_acceleration[dof]};
                        found_profile = step1.get_profile(p, blocks[dof]);
                    }
                } break;
            }

            if (!found_profile) {
                const bool has_zero_limits = (inp.max_acceleration[dof] == 0.0 || inp_min_acceleration[dof] == 0.0 || inp.max_jerk[dof] == 0.0);
                if (has_zero_limits) {
                    if (throw_error) {
                        throw RuckigError("zero limits conflict in step 1, dof: " + std::to_string(dof) + " input: " + inp.to_string());
                    }
                    return Result::ErrorZeroLimits;

                } else {
                    if (throw_error) {
                        throw RuckigError("error in step 1, dof: " + std::to_string(dof) + " input: " + inp.to_string());
                    }
                    return Result::ErrorExecutionTimeCalculation;
                }
            }

            traj.independent_min_durations[dof] = blocks[dof].t_min;
            // std::cout << dof << " profile step1: " << blocks[dof].to_string() << std::endl;
        }

        const bool discrete_duration = (inp.duration_discretization == DurationDiscretization::Discrete);
        if (degrees_of_freedom == 1 && !inp.minimum_duration && !discrete_duration) {
            traj.duration = blocks[0].t_min;
            traj.profiles[0][0] = blocks[0].p_min;
            traj.cumulative_times[0] = traj.duration;
            return Result::Working;
        }

        Optional<size_t> limiting_dof; // The DoF that doesn't need step 2
        const bool found_synchronization = synchronize(inp.minimum_duration, traj.duration, limiting_dof, traj.profiles[0], discrete_duration, delta_time);
        if (!found_synchronization) {
            bool has_zero_limits = false;
            for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
                if (inp.max_acceleration[dof] == 0.0 || inp_min_acceleration[dof] == 0.0 || inp.max_jerk[dof] == 0.0) {
                    has_zero_limits = true;
                    break;
                }
            }

            if (has_zero_limits) {
                if (throw_error) {
                    throw RuckigError("zero limits conflict with other degrees of freedom in time synchronization " + std::to_string(traj.duration));
                }
                return Result::ErrorZeroLimits;

            } else {
                if (throw_error) {
                    throw RuckigError("error in time synchronization: " + std::to_string(traj.duration));
                }
                return Result::ErrorSynchronizationCalculation;
            }
        }

        // None Synchronization
        for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
            if (inp.enabled[dof] && inp_per_dof_synchronization[dof] == Synchronization::None) {
                traj.profiles[0][dof] = blocks[dof].p_min;
                if (blocks[dof].t_min > traj.duration) {
                    traj.duration = blocks[dof].t_min;
                    limiting_dof = dof;
                }
            }
        }
        traj.cumulative_times[0] = traj.duration;

        if (return_error_at_maximal_duration) {
            if (traj.duration > 7.6e3) {
                return Result::ErrorTrajectoryDuration;
            }
        }

        if (traj.duration == 0.0) {
            // Copy all profiles for end state
            for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
                traj.profiles[0][dof] = blocks[dof].p_min;
            }
            return Result::Working;
        }

        if (!discrete_duration && std::all_of(inp_per_dof_synchronization.begin(), inp_per_dof_synchronization.end(), [](Synchronization s){ return s == Synchronization::None; })) {
            return Result::Working;
        }

        // Phase Synchronization
        if (limiting_dof && std::any_of(inp_per_dof_synchronization.begin(), inp_per_dof_synchronization.end(), [](Synchronization s){ return s == Synchronization::Phase; })) {
            const Profile& p_limiting = traj.profiles[0][limiting_dof.value()];
            if (is_input_collinear(inp, p_limiting.direction, limiting_dof.value())) {
                bool found_time_synchronization {true};
                for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
                    if (!inp.enabled[dof] || dof == limiting_dof || inp_per_dof_synchronization[dof] != Synchronization::Phase) {
                        continue;
                    }

                    Profile& p = traj.profiles[0][dof];
                    const double t_profile = traj.duration - p.brake.duration - p.accel.duration;

                    p.t = p_limiting.t; // Copy timing information from limiting DoF
                    p.control_signs = p_limiting.control_signs;

                    // Profile::ReachedLimits::NONE is a small hack, as there is no specialization for that in the check function
                    switch (inp_per_dof_control_interface[dof]) {
                        case ControlInterface::Position: {
                            switch (p.control_signs) {
                                case Profile::ControlSigns::UDDU: {
                                    if (!std::isinf(inp.max_jerk[dof])) {
                                        found_time_synchronization &= p.check_with_timing<Profile::ControlSigns::UDDU, Profile::ReachedLimits::NONE>(t_profile, new_phase_control[dof], inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]);
                                    } else if (!std::isinf(inp.max_acceleration[dof])) {
                                        found_time_synchronization &= p.check_for_second_order_with_timing<Profile::ControlSigns::UDDU, Profile::ReachedLimits::NONE>(t_profile, new_phase_control[dof], -new_phase_control[dof], inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof]);
                                    } else {
                                        found_time_synchronization &= p.check_for_first_order_with_timing<Profile::ControlSigns::UDDU, Profile::ReachedLimits::NONE>(t_profile, new_phase_control[dof], inp.max_velocity[dof], inp_min_velocity[dof]);
                                    }
                                } break;
                                case Profile::ControlSigns::UDUD: {
                                    if (!std::isinf(inp.max_jerk[dof])) {
                                        found_time_synchronization &= p.check_with_timing<Profile::ControlSigns::UDUD, Profile::ReachedLimits::NONE>(t_profile, new_phase_control[dof], inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]);
                                    } else {
                                        found_time_synchronization &= p.check_for_second_order_with_timing<Profile::ControlSigns::UDUD, Profile::ReachedLimits::NONE>(t_profile, new_phase_control[dof], -new_phase_control[dof], inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof]);
                                    }
                                } break;
                            }
                        } break;
                        case ControlInterface::Velocity: {
                            switch (p.control_signs) {
                                case Profile::ControlSigns::UDDU: {
                                    if (!std::isinf(inp.max_jerk[dof])) {
                                        found_time_synchronization &= p.check_for_velocity_with_timing<Profile::ControlSigns::UDDU, Profile::ReachedLimits::NONE>(t_profile, new_phase_control[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]);
                                    } else {
                                        found_time_synchronization &= p.check_for_second_order_velocity_with_timing<Profile::ControlSigns::UDDU, Profile::ReachedLimits::NONE>(t_profile, new_phase_control[dof], inp.max_acceleration[dof], inp_min_acceleration[dof]);
                                    }
                                } break;
                                case Profile::ControlSigns::UDUD: {
                                    if (!std::isinf(inp.max_jerk[dof])) {
                                        found_time_synchronization &= p.check_for_velocity_with_timing<Profile::ControlSigns::UDUD, Profile::ReachedLimits::NONE>(t_profile, new_phase_control[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]);
                                    } else {
                                        found_time_synchronization &= p.check_for_second_order_velocity_with_timing<Profile::ControlSigns::UDUD, Profile::ReachedLimits::NONE>(t_profile, new_phase_control[dof], inp.max_acceleration[dof], inp_min_acceleration[dof]);
                                    }
                                } break;
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
            const bool skip_synchronization = (dof == limiting_dof || inp_per_dof_synchronization[dof] == Synchronization::None) && !discrete_duration;
            if (!inp.enabled[dof] || skip_synchronization) {
                continue;
            }

            Profile& p = traj.profiles[0][dof];
            const double t_profile = traj.duration - p.brake.duration - p.accel.duration;

            if (inp_per_dof_synchronization[dof] == Synchronization::TimeIfNecessary && std::abs(inp.target_velocity[dof]) < eps && std::abs(inp.target_acceleration[dof]) < eps) {
                p = blocks[dof].p_min;
                continue;
            }

            // Check if the final time corresponds to an extremal profile calculated in step 1
            // Use 2*eps because of numerical robustness in duration discretization
            if (std::abs(t_profile - blocks[dof].t_min) < 2*eps) {
                p = blocks[dof].p_min;
                continue;
            } else if (blocks[dof].a && std::abs(t_profile - blocks[dof].a->right) < 2*eps) {
                p = blocks[dof].a->profile;
                continue;
            } else if (blocks[dof].b && std::abs(t_profile - blocks[dof].b->right) < 2*eps) {
                p = blocks[dof].b->profile;
                continue;
            }

            bool found_time_synchronization {false};
            switch (inp_per_dof_control_interface[dof]) {
                case ControlInterface::Position: {
                    if (!std::isinf(inp.max_jerk[dof])) {
                        PositionThirdOrderStep2 step2 {t_profile, p.p[0], p.v[0], p.a[0], p.pf, p.vf, p.af, inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]};
                        found_time_synchronization = step2.get_profile(p);
                    } else if (!std::isinf(inp.max_acceleration[dof])) {
                        PositionSecondOrderStep2 step2 {t_profile, p.p[0], p.v[0], p.pf, p.vf, inp.max_velocity[dof], inp_min_velocity[dof], inp.max_acceleration[dof], inp_min_acceleration[dof]};
                        found_time_synchronization = step2.get_profile(p);
                    } else {
                        PositionFirstOrderStep2 step2 {t_profile, p.p[0], p.pf, inp.max_velocity[dof], inp_min_velocity[dof]};
                        found_time_synchronization = step2.get_profile(p);
                    }
                } break;
                case ControlInterface::Velocity: {
                    if (!std::isinf(inp.max_jerk[dof])) {
                        VelocityThirdOrderStep2 step2 {t_profile, p.v[0], p.a[0], p.vf, p.af, inp.max_acceleration[dof], inp_min_acceleration[dof], inp.max_jerk[dof]};
                        found_time_synchronization = step2.get_profile(p);
                    } else {
                        VelocitySecondOrderStep2 step2 {t_profile, p.v[0], p.vf, inp.max_acceleration[dof], inp_min_acceleration[dof]};
                        found_time_synchronization = step2.get_profile(p);
                    }
                } break;
            }
            if (!found_time_synchronization) {
                if (throw_error) {
                    throw RuckigError("error in step 2 in dof: " + std::to_string(dof) + " for t sync: " + std::to_string(traj.duration) + " input: " + inp.to_string());
                }
                return Result::ErrorSynchronizationCalculation;
            }
            // std::cout << dof << " profile step2: " << p.to_string() << std::endl;
        }

        return Result::Working;
    }

    //! Continue the trajectory calculation
    template<bool throw_error>
    Result continue_calculation(const InputParameter<DOFs, CustomVector>&, Trajectory<DOFs, CustomVector>&, double, bool&) {
        return Result::Error;
    }
};

} // namespace ruckig
