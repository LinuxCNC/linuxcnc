#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <limits>
#include <memory>
#include <tuple>
#include <vector>

#include <nlohmann/json.hpp>

#include <ruckig/error.hpp>
#include <ruckig/input_parameter.hpp>
#include <ruckig/profile.hpp>
#include <ruckig/trajectory.hpp>


namespace httplib { class Client; }

namespace ruckig {

class CloudClient {
    std::shared_ptr<httplib::Client> cli;

public:
    explicit CloudClient();

    nlohmann::json post(const nlohmann::json& params, bool throw_error);
};


NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BrakeProfile, duration, t, j, a, v, p)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Profile, t, t_sum, j, a, v, p, pf, vf, af, brake, accel)


//! Calculation class for a trajectory along waypoints.
template<size_t DOFs, template<class, size_t> class CustomVector = StandardVector>
class WaypointsCalculator {
    template<class T> using Vector = CustomVector<T, DOFs>;

    // Convert the custom MinimumVector type to json, using only .size() and []
    template<class V>
    static void vector_to_json(nlohmann::json& j, const V& vector) {
        j = nlohmann::json::array();
        auto& j_vector = j.get_ref<nlohmann::json::array_t&>();

        j_vector.resize(vector.size());
        for (size_t i = 0; i < vector.size(); ++i) {
            j_vector[i] = vector[i];
        }
    }

    // Convert a double vector to json
    template<class V>
    static void double_vector_to_json(nlohmann::json& j, const std::vector<V>& vector) {
        j = nlohmann::json::array();
        auto& j_vector = j.get_ref<nlohmann::json::array_t&>();

        j_vector.resize(vector.size());
        for (size_t i = 0; i < vector.size(); ++i) {
            vector_to_json(j_vector[i], vector[i]);
        }
    }

    CloudClient client;

public:
    size_t degrees_of_freedom;

    template<size_t D = DOFs, typename std::enable_if<(D >= 1), int>::type = 0>
    explicit WaypointsCalculator(): degrees_of_freedom(DOFs) { }

    template<size_t D = DOFs, typename std::enable_if<(D >= 1), int>::type = 0>
    explicit WaypointsCalculator(size_t): degrees_of_freedom(DOFs) { }

    template<size_t D = DOFs, typename std::enable_if<(D == 0), int>::type = 0>
    explicit WaypointsCalculator(size_t dofs): degrees_of_freedom(dofs) { }

    template<size_t D = DOFs, typename std::enable_if<(D == 0), int>::type = 0>
    explicit WaypointsCalculator(size_t dofs, size_t): degrees_of_freedom(dofs) { }

    template<bool throw_error>
    Result calculate(const InputParameter<DOFs, CustomVector>& input, Trajectory<DOFs, CustomVector>& traj, double, bool& was_interrupted) {
        std::cout << "[ruckig] calculate trajectory via cloud API." << std::endl;

        nlohmann::json params;
        params["degrees_of_freedom"] = input.degrees_of_freedom;
        vector_to_json<Vector<double>>(params["current_position"], input.current_position);
        vector_to_json<Vector<double>>(params["current_velocity"], input.current_velocity);
        vector_to_json<Vector<double>>(params["current_acceleration"], input.current_acceleration);
        vector_to_json<Vector<double>>(params["target_position"], input.target_position);
        vector_to_json<Vector<double>>(params["target_velocity"], input.target_velocity);
        vector_to_json<Vector<double>>(params["target_acceleration"], input.target_acceleration);
        vector_to_json<Vector<double>>(params["max_velocity"], input.max_velocity);
        vector_to_json<Vector<double>>(params["max_acceleration"], input.max_acceleration);
        vector_to_json<Vector<double>>(params["max_jerk"], input.max_jerk);
        if (input.min_velocity) {
            vector_to_json<Vector<double>>(params["min_velocity"], input.min_velocity.value());
        }
        if (input.min_acceleration) {
            vector_to_json<Vector<double>>(params["min_acceleration"], input.min_acceleration.value());
        }
        if (!input.intermediate_positions.empty()) {
            double_vector_to_json<Vector<double>>(params["intermediate_positions"], input.intermediate_positions);
        }
        if (input.per_section_max_velocity) {
            double_vector_to_json<Vector<double>>(params["per_section_max_velocity"], input.per_section_max_velocity.value());
        }
        if (input.per_section_max_acceleration) {
            double_vector_to_json<Vector<double>>(params["per_section_max_acceleration"], input.per_section_max_acceleration.value());
        }
        if (input.per_section_max_jerk) {
            double_vector_to_json<Vector<double>>(params["per_section_max_jerk"], input.per_section_max_jerk.value());
        }
        if (input.per_section_min_velocity) {
            double_vector_to_json<Vector<double>>(params["per_section_min_velocity"], input.per_section_min_velocity.value());
        }
        if (input.per_section_min_acceleration) {
            double_vector_to_json<Vector<double>>(params["per_section_min_acceleration"], input.per_section_min_acceleration.value());
        }
        if (input.max_position) {
            vector_to_json<Vector<double>>(params["max_position"], input.max_position.value());
        }
        if (input.min_position) {
            vector_to_json<Vector<double>>(params["min_position"], input.min_position.value());
        }
        vector_to_json<Vector<bool>>(params["enabled"], input.enabled);
        params["control_interface"] = input.control_interface;
        params["synchronization"] = input.synchronization;
        params["duration_discretization"] = input.duration_discretization;
        if (input.per_dof_control_interface) {
            vector_to_json<Vector<ControlInterface>>(params["per_dof_control_interface"], input.per_dof_control_interface.value());
        }
        if (input.per_dof_synchronization) {
            vector_to_json<Vector<Synchronization>>(params["per_dof_synchronization"], input.per_dof_synchronization.value());
        }
        if (input.minimum_duration) {
            params["minimum_duration"] = input.minimum_duration.value();
        }
        if (input.per_section_minimum_duration) {
            vector_to_json<std::vector<double>>(params["per_section_minimum_duration"], input.per_section_minimum_duration.value());
        }

        const auto result = client.post(params, throw_error);

        was_interrupted = false;
        traj.degrees_of_freedom = input.degrees_of_freedom;
        traj.resize(result["profiles"].size() - 1);

        traj.continue_calculation_counter = 0;
        traj.duration = result["duration"].template get<double>();
        traj.cumulative_times = result["cumulative_times"].template get<std::vector<double>>();

        if (!result["message"].empty()) {
            std::cout << "[ruckig] " << result["message"] << std::endl;
        }

        if (result["result"] == "Result.Error") {
            return Result::Error;

        } else if (result["result"] == "Result.ErrorInvalidInput") {
            return Result::ErrorInvalidInput;
        }

        for (size_t i = 0; i < result["profiles"].size(); ++i) {
            for (size_t dof = 0; dof < traj.degrees_of_freedom; ++dof) {
                auto& p = result["profiles"][i][dof];
                traj.profiles[i][dof] = p.template get<Profile>();
            }
        }

        return Result::Working;
    }

    template<bool throw_error>
    Result continue_calculation(const InputParameter<DOFs, CustomVector>&, Trajectory<DOFs, CustomVector>&, double, bool&) {
        if (throw_error) {
            throw RuckigError("continue calculation not available in Ruckig Community Version.");
        }
        return Result::Error;
    }
};

} // namespace ruckig_pro
