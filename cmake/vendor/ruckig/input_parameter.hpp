#pragma once

#include <array>
#include <iomanip>
#include <optional>
#include <sstream>
#include <type_traits>
#include <vector>

#include "utils.hpp"

namespace ruckig {

//! Result type of Ruckig's update function
enum Result {
    Working = 0, ///< The trajectory is calculated normally
    Finished = 1, ///< Trajectory has reached its final position
    Error = -1, ///< Unclassified error
    ErrorInvalidInput = -100, ///< Error in the input parameter
    ErrorTrajectoryDuration = -101, ///< The trajectory duration exceeds its numerical limits
    ErrorPositionalLimits = -102, ///< The trajectory exceeds the given positional limits (only in Ruckig Pro)
    // ErrorNoPhaseSynchronization = -103, ///< The trajectory cannot be phase synchronized
    ErrorExecutionTimeCalculation = -110, ///< Error during the extremel time calculation (Step 1)
    ErrorSynchronizationCalculation = -111, ///< Error during the synchronization calculation (Step 2)
};


enum class ControlInterface {
    Position, ///< Position-control: Full control over the entire kinematic state (Default)
    Velocity, ///< Velocity-control: Ignores the current position, target position, and velocity limits
};

enum class Synchronization {
    Time, ///< Always synchronize the DoFs to reach the target at the same time (Default)
    TimeIfNecessary, ///< Synchronize only when necessary (e.g. for non-zero target velocity or acceleration)
    Phase, ///< Phase synchronize the DoFs when possible, else fallback to "Time" strategy
    // PhaseOnly, ///< Always phase synchronize the DoFs (even when this is not time-optimal), else returns "ErrorNoPhaseSynchronization"
    None, ///< Calculate every DoF independently
};

enum class DurationDiscretization {
    Continuous, ///< Every trajectory synchronization duration is allowed (Default)
    Discrete, ///< The trajectory synchronization duration must be a multiple of the control cycle
};


//! Input type of Ruckig
template<size_t DOFs>
class InputParameter {
    template<class T> using Vector = typename std::conditional<DOFs >= 1, std::array<T, DOFs>, std::vector<T>>::type;

    void initialize() {
        std::fill(current_velocity.begin(), current_velocity.end(), 0.0);
        std::fill(current_acceleration.begin(), current_acceleration.end(), 0.0);
        std::fill(target_velocity.begin(), target_velocity.end(), 0.0);
        std::fill(target_acceleration.begin(), target_acceleration.end(), 0.0);
        std::fill(enabled.begin(), enabled.end(), true);
    }

    void resize(size_t dofs) {
        current_position.resize(dofs);
        current_velocity.resize(dofs);
        current_acceleration.resize(dofs);
        target_position.resize(dofs);
        target_velocity.resize(dofs);
        target_acceleration.resize(dofs);
        max_velocity.resize(dofs);
        max_acceleration.resize(dofs);
        max_jerk.resize(dofs);
        enabled.resize(dofs);
    }

#if defined WITH_ONLINE_CLIENT
    void reserve(size_t max_number_of_waypoints) {
        intermediate_positions.reserve(max_number_of_waypoints);
    }
#endif

public:
    size_t degrees_of_freedom;

    ControlInterface control_interface {ControlInterface::Position};
    Synchronization synchronization {Synchronization::Time};
    DurationDiscretization duration_discretization {DurationDiscretization::Continuous};

    // Current state
    Vector<double> current_position, current_velocity, current_acceleration;

    // Target state
    Vector<double> target_position, target_velocity, target_acceleration;

    // Kinematic constraints
    Vector<double> max_velocity, max_acceleration, max_jerk;
    std::optional<Vector<double>> min_velocity, min_acceleration;

    //! Intermediate waypoints (only in Ruckig Pro)
    std::vector<Vector<double>> intermediate_positions;

    // Kinematic constraints for intermediate sections (between waypoints) (only in Ruckig Pro)
    std::optional<std::vector<Vector<double>>> per_section_max_velocity, per_section_max_acceleration, per_section_max_jerk;
    std::optional<std::vector<Vector<double>>> per_section_min_velocity, per_section_min_acceleration;

    // Positional constraints (only in Ruckig Pro)
    std::optional<Vector<double>> max_position, min_position;
    // std::optional<double> maximum_distance_to_linear_interpolation;

    //! Is the DoF considered for calculation?
    Vector<bool> enabled;

    //! Per-DoF control_interface (overwrites global control_interface)
    std::optional<Vector<ControlInterface>> per_dof_control_interface;

    //! Per-DoF synchronization (overwrites global synchronization)
    std::optional<Vector<Synchronization>> per_dof_synchronization;

    //! Optional minimum trajectory duration
    std::optional<double> minimum_duration;

    //! Optional duration [µs] after which the trajectory calculation is (softly) interrupted (only in Ruckig Pro)
    std::optional<double> interrupt_calculation_duration;

    template <size_t D = DOFs, typename std::enable_if<D >= 1, int>::type = 0>
    InputParameter(): degrees_of_freedom(DOFs) {
        initialize();
    }

    template <size_t D = DOFs, typename std::enable_if<D == 0, int>::type = 0>
    InputParameter(size_t dofs): degrees_of_freedom(dofs) {
        resize(dofs);
        initialize();
    }

#if defined WITH_ONLINE_CLIENT
    template <size_t D = DOFs, typename std::enable_if<D >= 1, int>::type = 0>
    InputParameter(size_t max_number_of_waypoints): degrees_of_freedom(DOFs) {
        reserve(max_number_of_waypoints);
        initialize();
    }

    template <size_t D = DOFs, typename std::enable_if<D == 0, int>::type = 0>
    InputParameter(size_t dofs, size_t max_number_of_waypoints): degrees_of_freedom(dofs) {
        reserve(max_number_of_waypoints);
        resize(dofs);
        initialize();
    }
#endif

    bool operator!=(const InputParameter<DOFs>& rhs) const {
        return (
            current_position != rhs.current_position
            || current_velocity != rhs.current_velocity
            || current_acceleration != rhs.current_acceleration
            || target_position != rhs.target_position
            || target_velocity != rhs.target_velocity
            || target_acceleration != rhs.target_acceleration
            || max_velocity != rhs.max_velocity
            || max_acceleration != rhs.max_acceleration
            || max_jerk != rhs.max_jerk
            || intermediate_positions != rhs.intermediate_positions
            || per_section_max_velocity != rhs.per_section_max_velocity
            || per_section_max_acceleration != rhs.per_section_max_acceleration
            || per_section_max_jerk != rhs.per_section_max_jerk
            || per_section_min_velocity != rhs.per_section_min_velocity
            || per_section_min_acceleration != rhs.per_section_min_acceleration
            || max_position != rhs.max_position
            || min_position != rhs.min_position
            || enabled != rhs.enabled
            || minimum_duration != rhs.minimum_duration
            || min_velocity != rhs.min_velocity
            || min_acceleration != rhs.min_acceleration
            || control_interface != rhs.control_interface
            || synchronization != rhs.synchronization
            || duration_discretization != rhs.duration_discretization
            || per_dof_control_interface != rhs.per_dof_control_interface
            || per_dof_synchronization != rhs.per_dof_synchronization
        );
    }

    std::string to_string() const {
        std::stringstream ss;
        ss << "\ninp.current_position = [" << join(current_position) << "]\n";
        ss << "inp.current_velocity = [" << join(current_velocity) << "]\n";
        ss << "inp.current_acceleration = [" << join(current_acceleration) << "]\n";
        ss << "inp.target_position = [" << join(target_position) << "]\n";
        ss << "inp.target_velocity = [" << join(target_velocity) << "]\n";
        ss << "inp.target_acceleration = [" << join(target_acceleration) << "]\n";
        ss << "inp.max_velocity = [" << join(max_velocity) << "]\n";
        ss << "inp.max_acceleration = [" << join(max_acceleration) << "]\n";
        ss << "inp.max_jerk = [" << join(max_jerk) << "]\n";
        if (min_velocity) {
            ss << "inp.min_velocity = [" << join(min_velocity.value()) << "]\n";
        }
        if (min_acceleration) {
            ss << "inp.min_acceleration = [" << join(min_acceleration.value()) << "]\n";
        }
        if (!intermediate_positions.empty()) {
            ss << "inp.intermediate_positions = [\n";
            for (auto p: intermediate_positions) {
                ss << "    [" << join(p) << "],\n";
            }
            ss << "]\n";
        }
        return ss.str();
    }
};

} // namespace ruckig
