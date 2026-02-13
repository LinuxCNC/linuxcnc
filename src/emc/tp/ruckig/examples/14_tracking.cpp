// Only with Ruckig Pro

#include <cmath>
#include <iostream>

#include <ruckig/trackig.hpp>


using namespace ruckig;

// Create the target state signal
TargetState<1> model_ramp(double t, double ramp_vel=0.5, double ramp_pos=1.0) {
    TargetState<1> target;
    const bool on_ramp = t < ramp_pos / std::abs(ramp_vel);
    target.position[0] = on_ramp ? t * ramp_vel : ramp_pos;
    target.velocity[0] = on_ramp ? ramp_vel : 0.0;
    target.acceleration[0] = 0.0;
    return target;
}

TargetState<1> model_constant_acceleration(double t, double ramp_acc=0.05) {
    TargetState<1> target;
    target.position[0] = t * t * ramp_acc;
    target.velocity[0] = t * ramp_acc;
    target.acceleration[0] = ramp_acc;
    return target;
}

TargetState<1> model_sinus(double t, double ramp_vel=0.4) {
    TargetState<1> target;
    target.position[0] = std::sin(ramp_vel * t);
    target.velocity[0] = ramp_vel * std::cos(ramp_vel * t);
    target.acceleration[0] = -ramp_vel * ramp_vel * std::sin(ramp_vel * t);
    return target;
}


int main() {
    // Create instances: the Trackig OTG as well as input and output parameters
    Trackig<1> otg(0.01);  // control cycle
    InputParameter<1> input;
    OutputParameter<1> output;

    // Set input parameters
    input.current_position = {0.0};
    input.current_velocity = {0.0};
    input.current_acceleration = {0.0};

    input.max_velocity = {0.8};
    input.max_acceleration = {2.0};
    input.max_jerk = {5.0};

    // Optional minimum and maximum position
    input.min_position = {-2.5};
    input.max_position = {2.5};

    otg.reactiveness = 1.0; // default value, should be in [0, 1]

    // Generate the trajectory following the target state
    std::cout << "target | follow" << std::endl;
    for (size_t t = 0; t < 500; t += 1) {
        const TargetState<1> target_state = model_ramp(otg.delta_time * t);
        const Result res = otg.update(target_state, input, output);
        std::cout << join(target_state.position) << " " << join(output.new_position) << std::endl;

        output.pass_to_input(input);
    }
}
