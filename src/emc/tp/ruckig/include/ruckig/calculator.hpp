#pragma once

#include <ruckig/calculator_target.hpp>
#ifdef WITH_CLOUD_CLIENT
#include <ruckig/calculator_cloud.hpp>
#endif
#include <ruckig/input_parameter.hpp>
#include <ruckig/trajectory.hpp>


namespace ruckig {

//! Internal interface for the main calculator and its hyperparameters
template<size_t DOFs, template<class, size_t> class CustomVector = StandardVector>
class Calculator {
    inline bool use_waypoints_trajectory(const InputParameter<DOFs, CustomVector>& input) {
        return !input.intermediate_positions.empty() && input.control_interface == ControlInterface::Position;
    }

public:
    //! Calculator for state-to-state trajectories
    TargetCalculator<DOFs, CustomVector> target_calculator;

#if defined WITH_CLOUD_CLIENT
    //! Calculator for trajectories with intermediate waypoints
    WaypointsCalculator<DOFs, CustomVector> waypoints_calculator;
#endif

    template<size_t D = DOFs, typename std::enable_if<(D >= 1), int>::type = 0>
    explicit Calculator() { }

#if defined WITH_CLOUD_CLIENT
    template<size_t D = DOFs, typename std::enable_if<(D >= 1), int>::type = 0>
    explicit Calculator(size_t max_waypoints):
        waypoints_calculator(WaypointsCalculator<DOFs, CustomVector>(max_waypoints))
        { }

    template<size_t D = DOFs, typename std::enable_if<(D == 0), int>::type = 0>
    explicit Calculator(size_t dofs):
        target_calculator(TargetCalculator<DOFs, CustomVector>(dofs)),
        waypoints_calculator(WaypointsCalculator<DOFs, CustomVector>(dofs))
        { }

    template<size_t D = DOFs, typename std::enable_if<(D == 0), int>::type = 0>
    explicit Calculator(size_t dofs, size_t max_waypoints):
        target_calculator(TargetCalculator<DOFs, CustomVector>(dofs)),
        waypoints_calculator(WaypointsCalculator<DOFs, CustomVector>(dofs, max_waypoints))
        { }
#else
    template<size_t D = DOFs, typename std::enable_if<(D == 0), int>::type = 0>
    explicit Calculator(size_t dofs): target_calculator(TargetCalculator<DOFs, CustomVector>(dofs)) { }
#endif

    //! Calculate the time-optimal waypoint-based trajectory
    template<bool throw_error>
    Result calculate(const InputParameter<DOFs, CustomVector>& input, Trajectory<DOFs, CustomVector>& trajectory, double delta_time, bool& was_interrupted) {
        Result result;
#if defined WITH_CLOUD_CLIENT
        if (use_waypoints_trajectory(input)) {
            result = waypoints_calculator.template calculate<throw_error>(input, trajectory, delta_time, was_interrupted);
        } else {
            result = target_calculator.template calculate<throw_error>(input, trajectory, delta_time, was_interrupted);
        }
#else
        result = target_calculator.template calculate<throw_error>(input, trajectory, delta_time, was_interrupted);
#endif

        return result;
    }

    //! Continue the trajectory calculation
    template<bool throw_error>
    Result continue_calculation(const InputParameter<DOFs, CustomVector>& input, Trajectory<DOFs, CustomVector>& trajectory, double delta_time, bool& was_interrupted) {
        Result result;
#if defined WITH_CLOUD_CLIENT
        if (use_waypoints_trajectory(input)) {
            result = waypoints_calculator.template continue_calculation<throw_error>(input, trajectory, delta_time, was_interrupted);
        } else {
            result = target_calculator.template continue_calculation<throw_error>(input, trajectory, delta_time, was_interrupted);
        }
#else
        result = target_calculator.template continue_calculation<throw_error>(input, trajectory, delta_time, was_interrupted);
#endif

        return result;
    }
};

} // namespace ruckig
