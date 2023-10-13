#pragma once

#include <chrono>
#include <memory>

#include <ReflexxesAPI.h>
#include <RMLPositionFlags.h>
#include <RMLPositionInputParameters.h>
#include <RMLPositionOutputParameters.h>

#include <ruckig/input_parameter.hpp>
#include <ruckig/output_parameter.hpp>


namespace ruckig {

//! Wrapper around Reflexxes as an alternative OTG algorithm (for comparison)
template<size_t DOFs>
class Reflexxes {
    InputParameter<DOFs> current_input;
    std::shared_ptr<ReflexxesAPI> rml;
    std::shared_ptr<RMLPositionInputParameters> input_parameters;
    std::shared_ptr<RMLPositionOutputParameters> output_parameters;
    std::shared_ptr<RMLVelocityInputParameters> input_vel_parameters;
    std::shared_ptr<RMLVelocityOutputParameters> output_vel_parameters;

    int result_value {0};
    RMLPositionFlags flags;
    RMLVelocityFlags vel_flags;

public:
    size_t degrees_of_freedom;

    double delta_time;

    explicit Reflexxes() { }

    template <size_t D = DOFs, typename std::enable_if<D >= 1, int>::type = 0>
    explicit Reflexxes(double delta_time): delta_time(delta_time), degrees_of_freedom(DOFs) {
        rml = std::make_shared<ReflexxesAPI>(DOFs, delta_time);
        input_parameters = std::make_shared<RMLPositionInputParameters>(DOFs);
        output_parameters = std::make_shared<RMLPositionOutputParameters>(DOFs);
        input_vel_parameters = std::make_shared<RMLVelocityInputParameters>(DOFs);
        output_vel_parameters = std::make_shared<RMLVelocityOutputParameters>(DOFs);

        flags.SynchronizationBehavior = RMLPositionFlags::PHASE_SYNCHRONIZATION_IF_POSSIBLE;
        vel_flags.SynchronizationBehavior = RMLVelocityFlags::PHASE_SYNCHRONIZATION_IF_POSSIBLE;
    }

    template <size_t D = DOFs, typename std::enable_if<D == 0, int>::type = 0>
    explicit Reflexxes(size_t dofs, double delta_time): delta_time(delta_time), degrees_of_freedom(dofs), current_input(InputParameter<0>(dofs)) {
        rml = std::make_shared<ReflexxesAPI>(dofs, delta_time);
        input_parameters = std::make_shared<RMLPositionInputParameters>(dofs);
        output_parameters = std::make_shared<RMLPositionOutputParameters>(dofs);
        input_vel_parameters = std::make_shared<RMLVelocityInputParameters>(dofs);
        output_vel_parameters = std::make_shared<RMLVelocityOutputParameters>(dofs);

        flags.SynchronizationBehavior = RMLPositionFlags::PHASE_SYNCHRONIZATION_IF_POSSIBLE;
        vel_flags.SynchronizationBehavior = RMLVelocityFlags::PHASE_SYNCHRONIZATION_IF_POSSIBLE;
    }

    Result update(const InputParameter<DOFs>& input, OutputParameter<DOFs>& output) {
        if (input != current_input) {
            current_input = input;

            for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
                if (input.target_acceleration[dof] != 0.0) {
                    return Result::ErrorInvalidInput;
                }
            }

            switch (input.control_interface) {
            case ControlInterface::Position: {
                if (input.minimum_duration) {
                    input_parameters->SetMinimumSynchronizationTime(input.minimum_duration.value());
                }

                for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
                    input_parameters->SetSelectionVectorElement(input.enabled[dof], dof); // Because of vector<bool> specialization
                }
                input_parameters->SetCurrentPositionVector(input.current_position.data());
                input_parameters->SetCurrentVelocityVector(input.current_velocity.data());
                input_parameters->SetCurrentAccelerationVector(input.current_acceleration.data());
                input_parameters->SetTargetPositionVector(input.target_position.data());
                input_parameters->SetTargetVelocityVector(input.target_velocity.data());
                input_parameters->SetMaxVelocityVector(input.max_velocity.data());
                input_parameters->SetMaxAccelerationVector(input.max_acceleration.data());
                input_parameters->SetMaxJerkVector(input.max_jerk.data());
            } break;
            case ControlInterface::Velocity: {
                if (input.minimum_duration) {
                    input_vel_parameters->SetMinimumSynchronizationTime(input.minimum_duration.value());
                }

                for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
                    input_vel_parameters->SetSelectionVectorElement(input.enabled[dof], dof); // Because of vector<bool> specialization
                }
                input_vel_parameters->SetCurrentPositionVector(input.current_position.data());
                input_vel_parameters->SetCurrentVelocityVector(input.current_velocity.data());
                input_vel_parameters->SetCurrentAccelerationVector(input.current_acceleration.data());
                input_vel_parameters->SetTargetVelocityVector(input.target_velocity.data());
                input_vel_parameters->SetMaxAccelerationVector(input.max_acceleration.data());
                input_vel_parameters->SetMaxJerkVector(input.max_jerk.data());
            } break;
            }
        }

        auto start = std::chrono::high_resolution_clock::now();

        switch (input.control_interface) {
        case ControlInterface::Position: {
            result_value = rml->RMLPosition(*input_parameters, output_parameters.get(), flags);

            for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
                output.new_position[dof] = output_parameters->NewPositionVector->VecData[dof];
                output.new_velocity[dof] = output_parameters->NewVelocityVector->VecData[dof];
                output.new_acceleration[dof] = output_parameters->NewAccelerationVector->VecData[dof];
            }
            output.trajectory.duration = output_parameters->GetSynchronizationTime();
            output.new_calculation = output_parameters->WasACompleteComputationPerformedDuringTheLastCycle();
        } break;
        case ControlInterface::Velocity: {
            result_value = rml->RMLVelocity(*input_vel_parameters, output_vel_parameters.get(), vel_flags);

            for (size_t dof = 0; dof < degrees_of_freedom; ++dof) {
                output.new_position[dof] = output_vel_parameters->NewPositionVector->VecData[dof];
                output.new_velocity[dof] = output_vel_parameters->NewVelocityVector->VecData[dof];
                output.new_acceleration[dof] = output_vel_parameters->NewAccelerationVector->VecData[dof];
            }
            output.trajectory.duration = output_vel_parameters->GetSynchronizationTime();
            output.new_calculation = output_vel_parameters->WasACompleteComputationPerformedDuringTheLastCycle();
        } break;
        }

        if (output.new_calculation) {
            output.time = 0.0;
        }

        auto stop = std::chrono::high_resolution_clock::now();
        output.calculation_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start).count() / 1000.0;
        output.time += delta_time;

        if (result_value == ReflexxesAPI::RML_FINAL_STATE_REACHED) {
            return Result::Finished;
        } else if (result_value < 0) {
            return Result::Error;
        }
        return Result::Working;
    }

    void at_time(double time, OutputParameter<DOFs>& output) {
        switch (current_input.control_interface) {
        case ControlInterface::Position: {
            rml->RMLPositionAtAGivenSampleTime(time, output_parameters.get());

            for (size_t dof = 0; dof < degrees_of_freedom; dof += 1) {
                output.new_position[dof] = output_parameters->NewPositionVector->VecData[dof];
                output.new_velocity[dof] = output_parameters->NewVelocityVector->VecData[dof];
                output.new_acceleration[dof] = output_parameters->NewAccelerationVector->VecData[dof];
            }
        } break;
        case ControlInterface::Velocity: {
            rml->RMLVelocityAtAGivenSampleTime(time, output_vel_parameters.get());

            for (size_t dof = 0; dof < degrees_of_freedom; dof += 1) {
                output.new_position[dof] = output_vel_parameters->NewPositionVector->VecData[dof];
                output.new_velocity[dof] = output_vel_parameters->NewVelocityVector->VecData[dof];
                output.new_acceleration[dof] = output_vel_parameters->NewAccelerationVector->VecData[dof];
            }
        } break;
        }
    }
};

} // namespace ruckig
