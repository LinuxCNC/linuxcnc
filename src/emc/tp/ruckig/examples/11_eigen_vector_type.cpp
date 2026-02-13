#include <iostream>

// Include Eigen before Ruckig
#include <Eigen/Core> // Version 3.4 or later

#include <ruckig/ruckig.hpp>


using namespace ruckig;

int main() {
    // Create instances: the Ruckig OTG as well as input and output parameters
    Ruckig<3, EigenVector> otg(0.01);  // control cycle
    InputParameter<3, EigenVector> input;
    OutputParameter<3, EigenVector> output;

    Eigen::Vector3d start_position;
    start_position << 0.0, 0.0, 0.5; // Eigen 3.4 also supports construction via an initializer list...

    Eigen::Vector3d position_diff;
    position_diff << 5.0, -2.0, -4.0;

    // Set input parameters
    input.current_position = start_position;
    input.current_velocity = {0.0, -2.2, -0.5};
    input.current_acceleration = {0.0, 2.5, -0.5};

    input.target_position = start_position + position_diff;
    input.target_velocity = {0.0, -0.5, -2.0};
    input.target_acceleration = {0.0, 0.0, 0.5};

    input.max_velocity = {3.0, 1.0, 3.0};
    input.max_acceleration = {3.0, 2.0, 1.0};
    input.max_jerk = {4.0, 3.0, 2.0};

    // Generate the trajectory within the control loop
    std::cout << "t | position" << std::endl;
    while (otg.update(input, output) == Result::Working) {
        std::cout << output.time << " | " << join(output.new_position) << std::endl;

        output.pass_to_input(input);
    }

    std::cout << "Trajectory duration: " << output.trajectory.get_duration() << " [s]." << std::endl;
}
