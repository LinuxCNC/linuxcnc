#include <array>
#include <string>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>

#include <ruckig/ruckig.hpp>

#ifdef WITH_REFLEXXES
    #include <ruckig/reflexxes_comparison.hpp>
#endif


namespace py = pybind11;
using namespace pybind11::literals; // to bring in the `_a` literal
using namespace ruckig;


PYBIND11_MODULE(ruckig, m) {
    m.doc() = "Instantaneous Motion Generation for Robots and Machines. Real-time and time-optimal trajectory calculation \
given a target waypoint with position, velocity, and acceleration, starting from any initial state \
limited by velocity, acceleration, and jerk constraints.";

    py::enum_<ControlInterface>(m, "ControlInterface")
        .value("Position", ControlInterface::Position)
        .value("Velocity", ControlInterface::Velocity)
        .export_values();

    py::enum_<Synchronization>(m, "Synchronization")
        .value("Phase", Synchronization::Phase)
        .value("Time", Synchronization::Time)
        .value("TimeIfNecessary", Synchronization::TimeIfNecessary)
        .value("No", Synchronization::None)
        .export_values();

    py::enum_<DurationDiscretization>(m, "DurationDiscretization")
        .value("Continuous", DurationDiscretization::Continuous)
        .value("Discrete", DurationDiscretization::Discrete)
        .export_values();

    py::enum_<Result>(m, "Result", py::arithmetic())
        .value("Working", Result::Working)
        .value("Finished", Result::Finished)
        .value("Error", Result::Error)
        .value("ErrorInvalidInput", Result::ErrorInvalidInput)
        .value("ErrorPositionalLimits", Result::ErrorPositionalLimits)
        .value("ErrorExecutionTimeCalculation", Result::ErrorExecutionTimeCalculation)
        .value("ErrorSynchronizationCalculation", Result::ErrorSynchronizationCalculation)
        .export_values();

    py::class_<PositionExtrema>(m, "PositionExtrema")
        .def_readonly("min", &PositionExtrema::min)
        .def_readonly("max", &PositionExtrema::max)
        .def_readonly("t_min", &PositionExtrema::t_min)
        .def_readonly("t_max", &PositionExtrema::t_max)
        .def("__repr__", [](const PositionExtrema& ext) {
            return "[" + std::to_string(ext.min) + ", " + std::to_string(ext.max) + "]";
        });

    py::class_<Trajectory<DynamicDOFs>>(m, "Trajectory")
        .def(py::init<size_t>(), "dofs"_a)
#if defined WITH_ONLINE_CLIENT
        .def(py::init<size_t, size_t>(), "dofs"_a, "max_number_of_waypoints"_a)
#endif
        .def_readonly("degrees_of_freedom", &Trajectory<DynamicDOFs>::degrees_of_freedom)
#ifdef WITH_EXPOSE_INTERNAL
        .def_property_readonly("profiles", &Trajectory<DynamicDOFs>::get_profiles)
#endif
        .def_property_readonly("duration", &Trajectory<DynamicDOFs>::get_duration)
        .def_property_readonly("intermediate_durations", &Trajectory<DynamicDOFs>::get_intermediate_durations)
        .def_property_readonly("independent_min_durations", &Trajectory<DynamicDOFs>::get_independent_min_durations)
        .def_property_readonly("position_extrema", &Trajectory<DynamicDOFs>::get_position_extrema)
        .def("at_time", [](const Trajectory<DynamicDOFs>& traj, double time, bool return_section=false) {
            std::vector<double> new_position(traj.degrees_of_freedom), new_velocity(traj.degrees_of_freedom), new_acceleration(traj.degrees_of_freedom);
            size_t new_section;
            traj.at_time(time, new_position, new_velocity, new_acceleration, new_section);
            if (return_section) {
                return py::make_tuple(new_position, new_velocity, new_acceleration, new_section);
            }
            return py::make_tuple(new_position, new_velocity, new_acceleration);
        }, "time"_a, "return_section"_a=false)
        .def("get_first_time_at_position", [](const Trajectory<DynamicDOFs>& traj, size_t dof, double position) -> py::object {
            double time;
            if (traj.get_first_time_at_position(dof, position, time)) {
                return py::cast(time);
            }
            return py::none();
        }, "dof"_a, "position"_a);

    py::class_<InputParameter<DynamicDOFs>>(m, "InputParameter")
        .def(py::init<size_t>(), "dofs"_a)
#if defined WITH_ONLINE_CLIENT
        .def(py::init<size_t, size_t>(), "dofs"_a, "max_number_of_waypoints"_a)
#endif
        .def_readonly("degrees_of_freedom", &InputParameter<DynamicDOFs>::degrees_of_freedom)
        .def_readwrite("current_position", &InputParameter<DynamicDOFs>::current_position)
        .def_readwrite("current_velocity", &InputParameter<DynamicDOFs>::current_velocity)
        .def_readwrite("current_acceleration", &InputParameter<DynamicDOFs>::current_acceleration)
        .def_readwrite("target_position", &InputParameter<DynamicDOFs>::target_position)
        .def_readwrite("target_velocity", &InputParameter<DynamicDOFs>::target_velocity)
        .def_readwrite("target_acceleration", &InputParameter<DynamicDOFs>::target_acceleration)
        .def_readwrite("max_velocity", &InputParameter<DynamicDOFs>::max_velocity)
        .def_readwrite("max_acceleration", &InputParameter<DynamicDOFs>::max_acceleration)
        .def_readwrite("max_jerk", &InputParameter<DynamicDOFs>::max_jerk)
        .def_readwrite("min_velocity", &InputParameter<DynamicDOFs>::min_velocity)
        .def_readwrite("min_acceleration", &InputParameter<DynamicDOFs>::min_acceleration)
        .def_readwrite("intermediate_positions", &InputParameter<DynamicDOFs>::intermediate_positions)
        .def_readwrite("per_section_max_velocity", &InputParameter<DynamicDOFs>::per_section_max_velocity)
        .def_readwrite("per_section_max_acceleration", &InputParameter<DynamicDOFs>::per_section_max_acceleration)
        .def_readwrite("per_section_max_jerk", &InputParameter<DynamicDOFs>::per_section_max_jerk)
        .def_readwrite("per_section_min_velocity", &InputParameter<DynamicDOFs>::per_section_min_velocity)
        .def_readwrite("per_section_min_acceleration", &InputParameter<DynamicDOFs>::per_section_min_acceleration)
        .def_readwrite("max_position", &InputParameter<DynamicDOFs>::max_position)
        .def_readwrite("min_position", &InputParameter<DynamicDOFs>::min_position)
        .def_readwrite("enabled", &InputParameter<DynamicDOFs>::enabled)
        .def_readwrite("control_interface", &InputParameter<DynamicDOFs>::control_interface)
        .def_readwrite("synchronization", &InputParameter<DynamicDOFs>::synchronization)
        .def_readwrite("duration_discretization", &InputParameter<DynamicDOFs>::duration_discretization)
        .def_readwrite("per_dof_control_interface", &InputParameter<DynamicDOFs>::per_dof_control_interface)
        .def_readwrite("per_dof_synchronization", &InputParameter<DynamicDOFs>::per_dof_synchronization)
        .def_readwrite("minimum_duration", &InputParameter<DynamicDOFs>::minimum_duration)
        .def_readwrite("interrupt_calculation_duration", &InputParameter<DynamicDOFs>::interrupt_calculation_duration)
        .def(py::self != py::self)
        .def("__repr__", &InputParameter<DynamicDOFs>::to_string);

    py::class_<OutputParameter<DynamicDOFs>>(m, "OutputParameter")
        .def(py::init<size_t>(), "dofs"_a)
#if defined WITH_ONLINE_CLIENT
        .def(py::init<size_t, size_t>(), "dofs"_a, "max_number_of_waypoints"_a)
#endif
        .def_readonly("degrees_of_freedom", &OutputParameter<DynamicDOFs>::degrees_of_freedom)
        .def_readonly("new_position", &OutputParameter<DynamicDOFs>::new_position)
        .def_readonly("new_velocity", &OutputParameter<DynamicDOFs>::new_velocity)
        .def_readonly("new_acceleration", &OutputParameter<DynamicDOFs>::new_acceleration)
        .def_readonly("new_section", &OutputParameter<DynamicDOFs>::new_section)
        .def_readonly("did_section_change", &OutputParameter<DynamicDOFs>::did_section_change)
        .def_readonly("trajectory", &OutputParameter<DynamicDOFs>::trajectory)
        .def_readonly("time", &OutputParameter<DynamicDOFs>::time)
        .def_readonly("new_calculation", &OutputParameter<DynamicDOFs>::new_calculation)
        .def_readonly("was_calculation_interrupted", &OutputParameter<DynamicDOFs>::was_calculation_interrupted)
        .def_readonly("calculation_duration", &OutputParameter<DynamicDOFs>::calculation_duration)
        .def("pass_to_input", &OutputParameter<DynamicDOFs>::pass_to_input, "input"_a)
        .def("__repr__", &OutputParameter<DynamicDOFs>::to_string)
        .def("__copy__",  [](const OutputParameter<DynamicDOFs> &self) {
            return OutputParameter<DynamicDOFs>(self);
        });

    py::class_<Ruckig<DynamicDOFs, true>>(m, "Ruckig")
        .def(py::init<size_t>(), "dofs"_a)
        .def(py::init<size_t, double>(), "dofs"_a, "delta_time"_a)
#if defined WITH_ONLINE_CLIENT
        .def(py::init<size_t, double, size_t>(), "dofs"_a, "delta_time"_a, "max_number_of_waypoints"_a=0)
        .def("filter_intermediate_positions", &Ruckig<DynamicDOFs, true>::filter_intermediate_positions, "input"_a, "threshold_distance"_a)
#endif
        .def_readonly("delta_time", &Ruckig<DynamicDOFs, true>::delta_time)
        .def_readonly("degrees_of_freedom", &Ruckig<DynamicDOFs, true>::degrees_of_freedom)
        .def("validate_input", &Ruckig<DynamicDOFs, true>::validate_input, "input"_a, "check_current_state_within_limits"_a=false, "check_target_state_within_limits"_a=true)
        .def("calculate", static_cast<Result (Ruckig<DynamicDOFs, true>::*)(const InputParameter<DynamicDOFs>&, Trajectory<DynamicDOFs>&)>(&Ruckig<DynamicDOFs, true>::calculate), "input"_a, "trajectory"_a)
        .def("calculate", static_cast<Result (Ruckig<DynamicDOFs, true>::*)(const InputParameter<DynamicDOFs>&, Trajectory<DynamicDOFs>&, bool&)>(&Ruckig<DynamicDOFs, true>::calculate), "input"_a, "trajectory"_a, "was_interrupted"_a)
        .def("update", &Ruckig<DynamicDOFs, true>::update, "input"_a, "output"_a);

#ifdef WITH_EXPOSE_INTERNAL
    py::class_<BrakeProfile>(m, "BrakeProfile")
        .def_readonly("duration", &BrakeProfile::duration)
        .def_readonly("t", &BrakeProfile::t)
        .def_readonly("j", &BrakeProfile::j)
        .def_readonly("a", &BrakeProfile::a)
        .def_readonly("v", &BrakeProfile::v)
        .def_readonly("p", &BrakeProfile::p);

    py::class_<Profile>(m, "Profile")
        .def_readonly("limits", &Profile::limits)
        .def_readonly("direction", &Profile::direction)
        .def_readonly("jerk_signs", &Profile::jerk_signs)
        .def_readonly("t", &Profile::t)
        .def_readonly("t_sum", &Profile::t_sum)
        .def_readonly("j", &Profile::j)
        .def_readonly("a", &Profile::a)
        .def_readonly("v", &Profile::v)
        .def_readonly("p", &Profile::p)
        .def_readonly("pf", &Profile::pf)
        .def_readonly("vf", &Profile::vf)
        .def_readonly("af", &Profile::af)
        .def_readonly("brake", &Profile::brake)
        .def_readonly("accel", &Profile::accel)
        .def("__repr__", &Profile::to_string);
#endif

#ifdef WITH_REFLEXXES
    py::class_<Reflexxes<DynamicDOFs>>(m, "Reflexxes")
        .def(py::init<size_t, double>(), "dofs"_a, "delta_time"_a)
        .def_readonly("degrees_of_freedom", &Reflexxes<DynamicDOFs>::degrees_of_freedom)
        .def_readonly("delta_time", &Reflexxes<DynamicDOFs>::delta_time)
        .def("update", &Reflexxes<DynamicDOFs>::update);
#endif
}
