#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <limits>
#include <math.h>
#include <numeric>
#include <optional>
#include <tuple>

#include <ruckig/calculator.hpp>
#include <ruckig/error.hpp>
#include <ruckig/input_parameter.hpp>
#include <ruckig/output_parameter.hpp>
#include <ruckig/trajectory.hpp>


namespace ruckig {

//! Main interface for the Ruckig algorithm
template<size_t DOFs = 0, template<class, size_t> class CustomVector = StandardVector, bool throw_error = false>
class Ruckig {
    //! Current input, only for comparison for recalculation
    InputParameter<DOFs, CustomVector> current_input;

    //! Flag that indicates if the current_input was properly initialized
    bool current_input_initialized {false};

public:
    //! Calculator for new trajectories
    Calculator<DOFs, CustomVector> calculator;

    //! Max number of intermediate waypoints
    const size_t max_number_of_waypoints;

    //! Degrees of freedom
    const size_t degrees_of_freedom;

    //! Time step between updates (cycle time) in [s]
    double delta_time {0.0};

    template<size_t D = DOFs, typename std::enable_if<(D >= 1), int>::type = 0>
    explicit Ruckig():
        max_number_of_waypoints(0),
        degrees_of_freedom(DOFs),
        delta_time(-1.0)
    {
    }

    template<size_t D = DOFs, typename std::enable_if<(D >= 1), int>::type = 0>
    explicit Ruckig(double delta_time):
        max_number_of_waypoints(0),
        degrees_of_freedom(DOFs),
        delta_time(delta_time)
    {
    }

#if defined WITH_CLOUD_CLIENT
    template<size_t D = DOFs, typename std::enable_if<(D >= 1), int>::type = 0>
    explicit Ruckig(double delta_time, size_t max_number_of_waypoints):
        current_input(InputParameter<DOFs, CustomVector>(max_number_of_waypoints)),
        calculator(Calculator<DOFs, CustomVector>(max_number_of_waypoints)),
        max_number_of_waypoints(max_number_of_waypoints),
        degrees_of_freedom(DOFs),
        delta_time(delta_time)
    {
    }
#endif

    template<size_t D = DOFs, typename std::enable_if<(D == 0), int>::type = 0>
    explicit Ruckig(size_t dofs):
        current_input(InputParameter<DOFs, CustomVector>(dofs)),
        calculator(Calculator<DOFs, CustomVector>(dofs)),
        max_number_of_waypoints(0),
        degrees_of_freedom(dofs),
        delta_time(-1.0)
    {
    }

    template<size_t D = DOFs, typename std::enable_if<(D == 0), int>::type = 0>
    explicit Ruckig(size_t dofs, double delta_time):
        current_input(InputParameter<DOFs, CustomVector>(dofs)),
        calculator(Calculator<DOFs, CustomVector>(dofs)),
        max_number_of_waypoints(0),
        degrees_of_freedom(dofs),
        delta_time(delta_time)
    {
    }

#if defined WITH_CLOUD_CLIENT
    template<size_t D = DOFs, typename std::enable_if<(D == 0), int>::type = 0>
    explicit Ruckig(size_t dofs, double delta_time, size_t max_number_of_waypoints):
        current_input(InputParameter<DOFs, CustomVector>(dofs, max_number_of_waypoints)),
        calculator(Calculator<DOFs, CustomVector>(dofs, max_number_of_waypoints)),
        max_number_of_waypoints(max_number_of_waypoints),
        degrees_of_freedom(dofs),
        delta_time(delta_time)
    {
    }
#endif

    //! Reset the instance (e.g. to force a new calculation in the next update)
    void reset() {
        current_input_initialized = false;
    }

    //! Filter intermediate positions based on a threshold distance for each DoF
    template<class T> using Vector = CustomVector<T, DOFs>;
    std::vector<Vector<double>> filter_intermediate_positions(const InputParameter<DOFs, CustomVector>& input, const Vector<double>& threshold_distance) const {
        if (input.intermediate_positions.empty()) {
            return input.intermediate_positions;
        }

        const size_t n_waypoints = input.intermediate_positions.size();
        std::vector<bool> is_active;
        is_active.resize(n_waypoints);
        for (size_t i = 0; i < n_waypoints; ++i) {
            is_active[i] = true;
        }

        size_t start = 0;
        size_t end = start + 2;
        for (; end < n_waypoints + 2; ++end) {
            const auto pos_start = (start == 0) ? input.current_position : input.intermediate_positions[start - 1];
            const auto pos_end = (end == n_waypoints + 1) ? input.target_position : input.intermediate_positions[end - 1];

            // Check for all intermediate positions
            bool are_all_below {true};
            for (size_t current = start + 1; current < end; ++current) {
                const auto pos_current = input.intermediate_positions[current - 1];

                // Is there a point t on the line that holds the threshold?
                double t_start_max = 0.0;
                double t_end_min = 1.0;
                for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
                    const double h0 = (pos_current[dof] - pos_start[dof]) / (pos_end[dof] - pos_start[dof]);
                    const double t_start = h0 - threshold_distance[dof] / std::abs(pos_end[dof] - pos_start[dof]);
                    const double t_end = h0 + threshold_distance[dof] / std::abs(pos_end[dof] - pos_start[dof]);

                    t_start_max = std::max(t_start, t_start_max);
                    t_end_min = std::min(t_end, t_end_min);

                    if (t_start_max > t_end_min) {
                        are_all_below = false;
                        break;
                    }
                }
                if (!are_all_below) {
                    break;
                }
            }

            is_active[end - 2] = !are_all_below;
            if (!are_all_below) {
                start = end - 1;
            }
        }

        std::vector<Vector<double>> filtered_positions;
        filtered_positions.reserve(n_waypoints);
        for (size_t i = 0; i < n_waypoints; ++i) {
            if (is_active[i]) {
                filtered_positions.push_back(input.intermediate_positions[i]);
            }
        }

        return filtered_positions;
    }

    //! Validate the input as well as the Ruckig instance for trajectory calculation
    template<bool throw_validation_error = true>
    bool validate_input(const InputParameter<DOFs, CustomVector>& input, bool check_current_state_within_limits = false, bool check_target_state_within_limits = true) const {
        if (!input.template validate<throw_validation_error>(check_current_state_within_limits, check_target_state_within_limits)) {
            return false;
        }

        if (!input.intermediate_positions.empty() && input.control_interface == ControlInterface::Position) {
            if (input.intermediate_positions.size() > max_number_of_waypoints) {
                if (throw_validation_error) {
                    throw RuckigError("The number of intermediate positions " + std::to_string(input.intermediate_positions.size()) + " exceeds the maximum number of waypoints " + std::to_string(max_number_of_waypoints) + ".");
                }
                return false;
            }
        }

        if (delta_time <= 0.0 && input.duration_discretization != DurationDiscretization::Continuous) {
            if (throw_validation_error) {
                throw RuckigError("delta time (control rate) parameter " + std::to_string(delta_time) + " should be larger than zero.");
            }
            return false;
        }

        return true;
    }

    //! Calculate a new trajectory for the given input
    Result calculate(const InputParameter<DOFs, CustomVector>& input, Trajectory<DOFs, CustomVector>& trajectory) {
        bool was_interrupted {false};
        return calculate(input, trajectory, was_interrupted);
    }

    //! Calculate a new trajectory for the given input and check for interruption
    Result calculate(const InputParameter<DOFs, CustomVector>& input, Trajectory<DOFs, CustomVector>& trajectory, bool& was_interrupted) {
        if (!validate_input<throw_error>(input, false, true)) {
            return Result::ErrorInvalidInput;
        }

        return calculator.template calculate<throw_error>(input, trajectory, delta_time, was_interrupted);
    }

    //! Get the next output state (with step delta_time) along the calculated trajectory for the given input
    Result update(const InputParameter<DOFs, CustomVector>& input, OutputParameter<DOFs, CustomVector>& output) {
        const auto start = std::chrono::steady_clock::now();

        if (DOFs == 0 && throw_error) {
            if (degrees_of_freedom != input.degrees_of_freedom || degrees_of_freedom != output.degrees_of_freedom) {
                throw RuckigError("mismatch in degrees of freedom (vector size).");
            }
        }

        output.new_calculation = false;

        Result result {Result::Working};
        if (!current_input_initialized || input != current_input) {
            result = calculate(input, output.trajectory, output.was_calculation_interrupted);
            if (result != Result::Working && result != Result::ErrorPositionalLimits) {
                return result;
            }

            current_input = input;
            current_input_initialized = true;
            output.time = 0.0;
            output.new_calculation = true;
        }

        const size_t old_section = output.new_section;
        output.time += delta_time;
        output.trajectory.at_time(output.time, output.new_position, output.new_velocity, output.new_acceleration, output.new_jerk, output.new_section);
        output.did_section_change = (output.new_section > old_section);  // Report only forward section changes

        const auto stop = std::chrono::steady_clock::now();
        output.calculation_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count() / 1000.0;

        output.pass_to_input(current_input);

        if (output.time > output.trajectory.get_duration()) {
            return Result::Finished;
        }

        return result;
    }
};


template<size_t DOFs, template<class, size_t> class CustomVector = StandardVector>
using RuckigThrow = Ruckig<DOFs, CustomVector, true>;


} // namespace ruckig
