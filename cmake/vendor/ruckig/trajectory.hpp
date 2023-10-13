#pragma once

#include <array>
#include <tuple>
#include <vector>

#include "input_parameter.hpp"
#include "profile.hpp"

namespace ruckig {

template <size_t> class Reflexxes;
template <size_t> class TargetCalculator;
template <size_t> class WaypointsCalculator;


//! Interface for the generated trajectory.
template<size_t DOFs>
class Trajectory {
#if defined WITH_ONLINE_CLIENT
    template<class T> using Container = std::vector<T>;
#else
    template<class T> using Container = std::array<T, 1>;
#endif

    template<class T> using Vector = typename std::conditional<DOFs >= 1, std::array<T, DOFs>, std::vector<T>>::type;

    friend class Reflexxes<DOFs>;
    friend class TargetCalculator<DOFs>;
    friend class WaypointsCalculator<DOFs>;

    Container<Vector<Profile>> profiles;

    double duration {0.0};
    Container<double> cumulative_times;

    Vector<double> independent_min_durations;
    Vector<PositionExtrema> position_extrema;

    size_t continue_calculation_counter {0};

#if defined WITH_ONLINE_CLIENT
    void resize(size_t max_number_of_waypoints) {
        profiles.resize(max_number_of_waypoints + 1);
        cumulative_times.resize(max_number_of_waypoints + 1);

        if constexpr (DOFs == 0) {
            for (auto& p: profiles) {
                p.resize(degrees_of_freedom);
            }
        }
    }
#endif

public:
    size_t degrees_of_freedom;

    template <size_t D = DOFs, typename std::enable_if<D >= 1, int>::type = 0>
    Trajectory(): degrees_of_freedom(DOFs) {
#if defined WITH_ONLINE_CLIENT
        resize(0);
#endif
    }

    template <size_t D = DOFs, typename std::enable_if<D == 0, int>::type = 0>
    Trajectory(size_t dofs): degrees_of_freedom(dofs) {
#if defined WITH_ONLINE_CLIENT
        resize(0);
#endif

        profiles[0].resize(dofs);
        independent_min_durations.resize(dofs);
        position_extrema.resize(dofs);
    }

#if defined WITH_ONLINE_CLIENT
    template <size_t D = DOFs, typename std::enable_if<D >= 1, int>::type = 0>
    Trajectory(size_t max_number_of_waypoints): degrees_of_freedom(DOFs) {
        resize(max_number_of_waypoints);
    }

    template <size_t D = DOFs, typename std::enable_if<D == 0, int>::type = 0>
    Trajectory(size_t dofs, size_t max_number_of_waypoints): degrees_of_freedom(dofs) {
        resize(max_number_of_waypoints);

        independent_min_durations.resize(dofs);
        position_extrema.resize(dofs);
    }
#endif

    //! Get the underlying profiles of the trajectory
    Container<Vector<Profile>> get_profiles() const {
        return profiles;
    }

    //! Get the kinematic state at a given time

    //! The Python wrapper takes `time` as an argument, and returns `new_position`, `new_velocity`, and `new_acceleration` instead.
    void at_time(double time, Vector<double>& new_position, Vector<double>& new_velocity, Vector<double>& new_acceleration, size_t& new_section) const {
        if constexpr (DOFs == 0) {
            if (degrees_of_freedom != new_position.size() || degrees_of_freedom != new_velocity.size() || degrees_of_freedom != new_acceleration.size()) {
                throw std::runtime_error("[ruckig] mismatch in degrees of freedom (vector size).");
            }
        }

        if (time >= duration) {
            // Keep constant acceleration
            new_section = profiles.size();
            auto& profiles_dof = profiles.back();
            for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
                const double t_pre = (profiles.size() > 1) ? cumulative_times[cumulative_times.size() - 2] : profiles_dof[dof].brake.duration;
                const double t_diff = time - (t_pre + profiles_dof[dof].t_sum.back());
                std::tie(new_position[dof], new_velocity[dof], new_acceleration[dof]) = integrate(t_diff, profiles_dof[dof].p.back(), profiles_dof[dof].v.back(), profiles_dof[dof].a.back(), 0);
            }
            return;
        }

        const auto new_section_ptr = std::upper_bound(cumulative_times.begin(), cumulative_times.end(), time);
        new_section = std::distance(cumulative_times.begin(), new_section_ptr);
        double t_diff = time;
        if (new_section > 0) {
            t_diff -= cumulative_times[new_section - 1];
        }

        for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
            const Profile& p = profiles[new_section][dof];
            double t_diff_dof = t_diff;

            // Brake pre-trajectory
            if (new_section == 0 && p.brake.duration > 0) {
                if (t_diff_dof < p.brake.duration) {
                    const size_t index = (t_diff_dof < p.brake.t[0]) ? 0 : 1;
                    if (index > 0) {
                        t_diff_dof -= p.brake.t[index - 1];
                    }

                    std::tie(new_position[dof], new_velocity[dof], new_acceleration[dof]) = integrate(t_diff_dof, p.brake.p[index], p.brake.v[index], p.brake.a[index], p.brake.j[index]);
                    continue;
                } else {
                    t_diff_dof -= p.brake.duration;
                }
            }

            // Accel post-trajectory
            // if (new_section == profiles.size() - 1 && p.accel.duration > 0) {
            //     if (t_diff_dof > p.t_sum.back()) {
            //         const size_t index = (t_diff_dof < p.accel.t[1]) ? 1 : 0;
            //         if (index > 0) {
            //             t_diff_dof -= p.accel.t[index - 1];
            //         }

            //         t_diff_dof -= p.t_sum.back();

            //         if (t_diff_dof < p.accel.t[1]) {
            //             std::tie(new_position[dof], new_velocity[dof], new_acceleration[dof]) = integrate(t_diff_dof, p.p.back(), p.v.back(), p.a.back(), p.accel.j[1]);
            //             continue;
            //         }

            //         t_diff_dof -= p.accel.t[1];

            //         const size_t index = 1;
            //         std::tie(new_position[dof], new_velocity[dof], new_acceleration[dof]) = integrate(t_diff_dof, p.accel.p[index], p.accel.v[index], p.accel.a[index], p.accel.j[index]);
            //         continue;
            //     }
            // }

            // Non-time synchronization
            if (t_diff_dof >= p.t_sum.back()) {
                // Keep constant acceleration
                std::tie(new_position[dof], new_velocity[dof], new_acceleration[dof]) = integrate(t_diff_dof - p.t_sum.back(), p.p.back(), p.v.back(), p.a.back(), 0);
                continue;
            }

            const auto index_dof_ptr = std::upper_bound(p.t_sum.begin(), p.t_sum.end(), t_diff_dof);
            const size_t index_dof = std::distance(p.t_sum.begin(), index_dof_ptr);

            if (index_dof > 0) {
                t_diff_dof -= p.t_sum[index_dof - 1];
            }

            std::tie(new_position[dof], new_velocity[dof], new_acceleration[dof]) = integrate(t_diff_dof, p.p[index_dof], p.v[index_dof], p.a[index_dof], p.j[index_dof]);
        }
    }

    //! Get the kinematic state and current section at a given time
    void at_time(double time, Vector<double>& new_position, Vector<double>& new_velocity, Vector<double>& new_acceleration) const {
        size_t new_section;
        at_time(time, new_position, new_velocity, new_acceleration, new_section);
    }

    //! Get the duration of the (synchronized) trajectory
    double get_duration() const {
        return duration;
    }

    //! Get the durations when the intermediate waypoints are reached
    Container<double> get_intermediate_durations() const {
        return cumulative_times;
    }

    //! Get the minimum duration of each independent DoF
    Vector<double> get_independent_min_durations() const {
        return independent_min_durations;
    }

    //! Get the min/max values of the position for each DoF
    Vector<PositionExtrema> get_position_extrema() {
        for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
            position_extrema[dof] = profiles[0][dof].get_position_extrema();
        }

        for (size_t i = 1; i < profiles.size(); ++i) {
            for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
                auto section_position_extrema = profiles[i][dof].get_position_extrema();
                if (section_position_extrema.max > position_extrema[dof].max) {
                    position_extrema[dof].max = section_position_extrema.max;
                    position_extrema[dof].t_max = section_position_extrema.t_max;
                }
                if (section_position_extrema.min < position_extrema[dof].min) {
                    position_extrema[dof].min = section_position_extrema.min;
                    position_extrema[dof].t_min = section_position_extrema.t_min;
                }
            }
        }

        return position_extrema;
    }

    //! Get the time that this trajectory passes a specific position of a given DoF the first time

    //! If the position is passed, this method returns true, otherwise false
    //! The Python wrapper takes `dof` and `position` as arguments and returns `time` (or `None`) instead
    bool get_first_time_at_position(size_t dof, double position, double& time) const {
        if (dof >= degrees_of_freedom) {
            return false;
        }

        double v, a;
        for (auto& p: profiles) {
            if (p[dof].get_first_state_at_position(position, time, v, a)) {
                return true;
            }
        }
        return false;
    }
};

} // namespace ruckig
