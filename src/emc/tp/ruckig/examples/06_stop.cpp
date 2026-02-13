#include <iostream>

#include <ruckig/ruckig.hpp>


using namespace ruckig;

int main() {
    // Create instances: the ruckig otg as well as input and output parameters
    Ruckig<3> otg(0.01);
    InputParameter<3> input;
    OutputParameter<3> output;

    // Set input parameters
    input.current_position = {0.0, 0.0, 0.5};
    input.current_velocity = {0.0, -2.2, -0.5};
    input.current_acceleration = {0.0, 2.5, -0.5};

    input.target_position = {5.0, -2.0, -3.5};
    input.target_velocity = {0.0, -0.5, -2.0};
    input.target_acceleration = {0.0, 0.0, 0.5};

    input.max_velocity = {3.0, 1.0, 3.0};
    input.max_acceleration = {3.0, 2.0, 1.0};
    input.max_jerk = {4.0, 3.0, 2.0};

    // Generate the trajectory within the control loop
    std::cout << "t | position" << std::endl;
    bool on_stop_trajectory = false;
    while (otg.update(input, output) == Result::Working) {
        std::cout << output.time << " | " << join(output.new_position) << std::endl;

        // Activate stop trajectory after 1s
        if (output.time >= 1.0 && !on_stop_trajectory) {
            std::cout << "Stop immediately." << std::endl;
            on_stop_trajectory = true;

            // Synchronization is disabled so that each DoF stops as fast as possible independently
            input.control_interface = ControlInterface::Velocity;
            input.synchronization = Synchronization::None;
            input.target_velocity = {0.0, 0.0, 0.0};
            input.target_acceleration = {0.0, 0.0, 0.0};
            input.max_jerk = {12.0, 10.0, 8.0};
        }

        output.pass_to_input(input);
    }

    std::cout << "Stop trajectory duration: " << output.trajectory.get_duration() << " [s]." << std::endl;
}
