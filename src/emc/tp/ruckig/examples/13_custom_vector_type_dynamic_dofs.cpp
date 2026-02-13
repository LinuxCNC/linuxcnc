#include <deque>
#include <iostream>

#include <ruckig/ruckig.hpp>


template<class T, size_t DOFs>
class MinimalDynamicDofsVector {
    std::deque<T> data;

public:
    MinimalDynamicDofsVector() { }
    MinimalDynamicDofsVector(std::initializer_list<T> a) {
        data.resize(a.size());
        std::copy_n(a.begin(), a.size(), std::begin(data));
    }

    T operator[](size_t i) const {
        return data[i];
    }

    T& operator[](size_t i) {
        return data[i];
    }

    size_t size() const {
        return data.size();
    }

    void resize(size_t size) {
        data.resize(size);
    }

    bool operator==(const MinimalDynamicDofsVector<T, DOFs>& rhs) const {
        for (size_t dof = 0; dof < data.size(); ++dof) {
            if (data[dof] != rhs[dof]) {
                return false;
            }
        }
        return true;
    }
};


using namespace ruckig;

int main() {
    // Create instances: the Ruckig OTG as well as input and output parameters
    Ruckig<DynamicDOFs, MinimalDynamicDofsVector> otg(3, 0.01);  // control cycle
    InputParameter<DynamicDOFs, MinimalDynamicDofsVector> input(3);
    OutputParameter<DynamicDOFs, MinimalDynamicDofsVector> output(3);

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

    std::cout << "Trajectory duration: " << output.trajectory.get_duration() << " [s]." << std::endl;
}
