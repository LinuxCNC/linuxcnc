#include <iostream>

#include <ruckig/ruckig.hpp>


using namespace ruckig;

int main() {
    // Create instances: the Ruckig OTG as well as input and output parameters
    size_t degrees_of_freedom = 3;

    Ruckig<DynamicDOFs> otg(degrees_of_freedom, 0.01);
    InputParameter<DynamicDOFs> input(degrees_of_freedom);
    OutputParameter<DynamicDOFs> output(degrees_of_freedom);

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
    while (otg.update(input, output) == Result::Working) {
        std::cout << output.time << " | " << join(output.new_position) << std::endl;

        output.pass_to_input(input);
    }

    std::cout << "Trajectory duration: " << output.trajectory.get_duration() << " [s]" << std::endl;
}
