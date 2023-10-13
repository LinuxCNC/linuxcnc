#pragma once

#include "calculator_target.hpp"
#ifdef WITH_ONLINE_CLIENT
#include <ruckig/calculator_online.hpp>
#endif
#include "input_parameter.hpp"
#include "trajectory.hpp"

namespace ruckig {

//! Calculation interface.
template<size_t DOFs>
class Calculator {
    TargetCalculator<DOFs> target_calculator;
#if defined WITH_ONLINE_CLIENT
    WaypointsCalculator<DOFs> waypoints_calculator;
#endif

    inline bool use_waypoints_trajectory(const InputParameter<DOFs>& input) {
        return !input.intermediate_positions.empty() && input.control_interface == ControlInterface::Position;
    }

public:
    template <size_t D = DOFs, typename std::enable_if<D >= 1, int>::type = 0>
    explicit Calculator() { }

#if defined WITH_ONLINE_CLIENT
    template <size_t D = DOFs, typename std::enable_if<D >= 1, int>::type = 0>
    explicit Calculator(size_t max_waypoints): waypoints_calculator(WaypointsCalculator<DOFs>(max_waypoints)) { }

    template <size_t D = DOFs, typename std::enable_if<D == 0, int>::type = 0>
    explicit Calculator(size_t dofs): target_calculator(TargetCalculator<DOFs>(dofs)), waypoints_calculator(WaypointsCalculator<DOFs>(dofs)) { }

    template <size_t D = DOFs, typename std::enable_if<D == 0, int>::type = 0>
    explicit Calculator(size_t dofs, size_t max_waypoints): target_calculator(TargetCalculator<DOFs>(dofs)), waypoints_calculator(WaypointsCalculator<DOFs>(dofs, max_waypoints)) { }
#else
    template <size_t D = DOFs, typename std::enable_if<D == 0, int>::type = 0>
    explicit Calculator(size_t dofs): target_calculator(TargetCalculator<DOFs>(dofs)) { }
#endif

    //! Calculate the time-optimal waypoint-based trajectory
    template<bool throw_error, bool return_error_at_maximal_duration>
    Result calculate(const InputParameter<DOFs>& input, Trajectory<DOFs>& trajectory, double delta_time, bool& was_interrupted) {
        Result result;
#if defined WITH_ONLINE_CLIENT
        if (use_waypoints_trajectory(input)) {
            result = waypoints_calculator.template calculate<throw_error, return_error_at_maximal_duration>(input, trajectory, delta_time, was_interrupted);
        } else {
            result = target_calculator.template calculate<throw_error, return_error_at_maximal_duration>(input, trajectory, delta_time, was_interrupted);
        }
#else
        result = target_calculator.template calculate<throw_error, return_error_at_maximal_duration>(input, trajectory, delta_time, was_interrupted);
#endif

        return result;
    }

    //! Continue the trajectory calculation
    template<bool throw_error, bool return_error_at_maximal_duration>
    Result continue_calculation(const InputParameter<DOFs>& input, Trajectory<DOFs>& trajectory, double delta_time, bool& was_interrupted) {
        Result result;
#if defined WITH_ONLINE_CLIENT
        if (use_waypoints_trajectory(input)) {
            result = waypoints_calculator.template continue_calculation<throw_error, return_error_at_maximal_duration>(input, trajectory, delta_time, was_interrupted);
        } else {
            result = target_calculator.template continue_calculation<throw_error, return_error_at_maximal_duration>(input, trajectory, delta_time, was_interrupted);
        }
#else
        result = target_calculator.template continue_calculation<throw_error, return_error_at_maximal_duration>(input, trajectory, delta_time, was_interrupted);
#endif

        return result;
    }
};

} // namespace ruckig
