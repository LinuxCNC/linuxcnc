<div align="center">
  <h1 align="center">Ruckig</h1>
  <h3 align="center">
    Instantaneous Motion Generation for Robots and Machines.
  </h3>
</div>
<p align="center">
  <a href="https://github.com/pantor/ruckig/actions">
    <img src="https://github.com/pantor/ruckig/workflows/CI/badge.svg" alt="CI">
  </a>
  <a href="https://github.com/pantor/ruckig/issues">
    <img src="https://img.shields.io/github/issues/pantor/ruckig.svg" alt="Issues">
  </a>
  <a href="https://github.com/pantor/ruckig/releases">
    <img src="https://img.shields.io/github/v/release/pantor/ruckig.svg?include_prereleases&sort=semver" alt="Releases">
  </a>
  <a href="https://github.com/pantor/ruckig/blob/main/LICENSE">
    <img src="https://img.shields.io/badge/license-MIT-green.svg" alt="MIT">
  </a>
</p>

### <span style="color:yellow">**⚠ 这里为了适配我的老软件，降级支持 C++11，同时开启一些警告检查。** </span>
我的仓库：[链接](https://github.com/mika4128/ruckig.git)

Ruckig generates trajectories on-the-fly, allowing robots and machines to react instantaneously to sensor input. Ruckig calculates a trajectory to a *target* waypoint (with position, velocity, and acceleration) starting from *any* initial state limited by velocity, acceleration, and jerk constraints. Besides the target state, Ruckig allows to define intermediate positions for waypoint following. For state-to-state motions, Ruckig guarantees a time-optimal solution. With intermediate waypoints, Ruckig calculates the path and its time parametrization jointly, resulting in significantly faster trajectories compared to traditional methods.

More information can be found at [ruckig.com](https://ruckig.com) and in the corresponding paper [Jerk-limited Real-time Trajectory Generation with Arbitrary Target States](https://arxiv.org/abs/2105.04830), accepted for the *Robotics: Science and Systems (RSS), 2021* conference.


## Installation

Ruckig has no dependencies (except for testing). To build Ruckig using CMake, just run

```bash
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

To install Ruckig in a system-wide directory, you can either use `(sudo) make install` or install it as debian package using cpack by running

```bash
cpack
sudo dpkg -i ruckig*.deb
```

An example of using Ruckig in your CMake project is given by `examples/CMakeLists.txt`. However, you can also include Ruckig as a directory within your project and call `add_subdirectory(ruckig)` in your parent `CMakeLists.txt`.

Ruckig is also available as a Python module, in particular for development or debugging purposes. The Ruckig *Community Version* can be installed from [PyPI](https://pypi.org/project/ruckig/) via
```bash
pip install ruckig
```
When using CMake, the Python module can be built using the `BUILD_PYTHON_MODULE` flag. If you're only interested in the Python module (and not in the C++ library), you can build and install Ruckig via `pip install .`.


## Tutorial

Furthermore, we will explain the basics to get started with online generated trajectories within your application. There is also a [collection of examples](https://docs.ruckig.com/pages.html) that guide you through the most important features of Ruckig. A time-optimal trajectory for a single degree of freedom is shown in the figure below. We also added plots of the resulting trajectories for all examples. Let's get started!

![Trajectory Profile](https://github.com/pantor/ruckig/raw/main/doc/example_profile.png?raw=true)


### Waypoint-based Trajectory Generation

Ruckig provides three main interface classes: the *Ruckig*, the *InputParameter*, and the *OutputParameter* class.

First, you'll need to create a Ruckig instance with the number of DoFs as a template parameter, and the control cycle (e.g. in seconds) in the constructor.

```.cpp
Ruckig<6> ruckig {0.001}; // Number DoFs; control cycle in [s]
```

The input type has 3 blocks of data: the *current* state, the *target* state and the corresponding kinematic *limits*.

```.cpp
InputParameter<6> input; // Number DoFs
input.current_position = {0.2, ...};
input.current_velocity = {0.1, ...};
input.current_acceleration = {0.1, ...};
input.target_position = {0.5, ...};
input.target_velocity = {-0.1, ...};
input.target_acceleration = {0.2, ...};
input.max_velocity = {0.4, ...};
input.max_acceleration = {1.0, ...};
input.max_jerk = {4.0, ...};

OutputParameter<6> output; // Number DoFs
```

If you only want to have a acceleration-constrained trajectory, you can also omit the `max_jerk` as well as the `current` and `target_acceleration` value. Given all input and output resources, we can iterate over the trajectory at each discrete time step. For most applications, this loop must run within a real-time thread and controls the actual hardware.

```.cpp
while (ruckig.update(input, output) == Result::Working) {
  // Make use of the new state here!
  // e.g. robot->setJointPositions(output.new_position);

  output.pass_to_input(input); // Don't forget this!
}
```

Within the control loop, you need to update the *current state* of the input parameter according to the calculated trajectory. Therefore, the `pass_to_input` method copies the new kinematic state of the output to the current kinematic state of the input parameter. If (in the next step) the current state is not the expected, pre-calculated trajectory, Ruckig will calculate a new trajectory based on the novel input. When the trajectory has reached the target state, the `update` function will return `Result::Finished`.


### Intermediate Waypoints

The Ruckig Community Version includes built-in support for intermediate waypoints, using our cloud API for remote calculation. Of course, the Ruckig Pro version is fully local. To allocate the necessary memory for a variable number of waypoints beforehand, we need to pass the maximum number of waypoints to Ruckig via
```.cpp
Ruckig<6> ruckig {0.001, 8};
InputParameter<6> input {8};
OutputParameter<6> output {8};
```
The `InputParameter` class takes the number of waypoints as an optional input, however usually you will fill in the values (and therefore reserve its memory) yourself. Then you're ready to set intermediate via points by
```.cpp
input.intermediate_positions = {
  {0.2, ...},
  {0.8, ...},
};
```
As soon as at least one intermediate positions is given, the Ruckig Community Version switches to the mentioned (of course, non real-time capable) cloud API. If you require real-time calculation on your own hardware, please contact us for the *Ruckig Pro Version*.

When using *intermediate positions*, both the underlying motion planning problem as well as its calculation changes significantly. In particular, there are some fundamental limitations for jerk-limited online trajectory generation regarding the usage of waypoints. Please find more information about these limitations [here](https://docs.ruckig.com/md_pages_2__intermediate__waypoints.html), and in general we recommend to use
```.cpp
input.intermediate_positions = ruckig.filter_intermediate_positions(input.intermediate_positions, {0.1, ...});
```
to filter waypoints according to a (high) threshold distance. Setting *interrupt_calculation_duration* makes sure to be real-time capable by refining the solution in the next control invocation. Note that this is a soft interruption of the calculation. Currently, no minimum or discrete durations are supported when using intermediate positions.


### Input Parameter

To go into more detail, the *InputParameter* type has following members:

```.cpp
using Vector = std::array<double, DOFs>; // By default

Vector current_position;
Vector current_velocity; // Initialized to zero
Vector current_acceleration; // Initialized to zero

std::vector<Vector> intermediate_positions; // (only in Pro Version)

Vector target_position;
Vector target_velocity; // Initialized to zero
Vector target_acceleration; // Initialized to zero

Vector max_velocity;
Vector max_acceleration;
Vector max_jerk; // Initialized to infinity

std::optional<Vector> min_velocity; // If not given, the negative maximum velocity will be used.
std::optional<Vector> min_acceleration; // If not given, the negative maximum acceleration will be used.

std::optional<Vector> min_position; // (only in Pro Version)
std::optional<Vector> max_position; // (only in Pro Version)

std::array<bool, DOFs> enabled; // Initialized to true
std::optional<double> minimum_duration;
std::optional<double> interrupt_calculation_duration; // [µs], (only in Pro Version)

ControlInterface control_interface; // The default position interface controls the full kinematic state.
Synchronization synchronization; // Synchronization behavior of multiple DoFs
DurationDiscretization duration_discretization; // Whether the duration should be a discrete multiple of the control cycle (off by default)

std::optional<Vector<ControlInterface>> per_dof_control_interface; // Sets the control interface for each DoF individually, overwrites global control_interface
std::optional<Vector<Synchronization>> per_dof_synchronization; // Sets the synchronization for each DoF individually, overwrites global synchronization
```

On top of the current state, target state, and constraints, Ruckig allows for a few more advanced settings:
- A *minimum* velocity and acceleration can be specified - these should be a negative number. If they are not given, the negative maximum velocity or acceleration will be used (similar to the jerk limit). For example, this might be useful in human robot collaboration settings with a different velocity limit towards a human. Or, when switching between different moving coordinate frames like picking from a conveyer belt.
- You can overwrite the global kinematic limits to specify limits for each section between two waypoints separately by using e.g. `per_section_max_velocity`.
- If a DoF is not *enabled*, it will be ignored in the calculation. Ruckig will output a trajectory with constant acceleration for those DoFs.
- A *minimum duration* can be optionally given. Note that Ruckig can not guarantee an exact, but only a minimum duration of the trajectory.
- The control interface (position or velocity control) can be switched easily. For example, a stop trajectory or visual servoing can be easily implemented with the velocity interface.
- Different synchronization behaviors (i.a. phase, time, or no synchonization) are implemented. Phase synchronization results in straight-line motions.
- The trajectory duration might be constrained to a multiple of the control cycle. This way, the *exact* state can be reached at a control loop execution.

We refer to the [API documentation](https://docs.ruckig.com/namespaceruckig.html) of the enumerations within the `ruckig` namespace for all available options.


### Input Validation

To check that Ruckig is able to generate a trajectory before the actual calculation step,
```.cpp
ruckig.validate_input(input, check_current_state_within_limits=false, check_target_state_within_limits=true);
// returns true or throws
```
throws an error with a detailed reason if an input is not valid. You can also set the default template parameter to false via `ruckig.validate_input<false>(...)` to just return a boolean true or false. The two boolean arguments check that the current or target state are within the limits. The check includes a typical catch of jerk-limited trajectory generation: When the current state is at maximal velocity, any positive acceleration will inevitable lead to a velocity violation *at a future timestep*. In general, this condition is fulfilled when
```
Abs(acceleration) <= Sqrt(2 * max_jerk * (max_velocity - Abs(velocity))).
```
If both arguments are set to true, the calculated trajectory is guaranteed to be *within the kinematic limits throughout* its duration. Also, note that there are range constraints of the input due to numerical reasons, see below for more details.


### Result Type

The `update` function of the Ruckig class returns a Result type that indicates the current state of the algorithm. This can either be **working**, **finished** if the trajectory has finished, or an **error** type if something went wrong during calculation. The result type can be compared as a standard integer.

State                           | Error Code
------------------------------- | ----------
Working                         | 0
Finished                        | 1
Error                           | -1
ErrorInvalidInput               | -100
ErrorTrajectoryDuration         | -101
ErrorPositionalLimits           | -102
ErrorExecutionTimeCalculation   | -110
ErrorSynchronizationCalculation | -111


### Output Parameter

The output class includes the new kinematic state and the overall trajectory.

```.cpp
Vector new_position;
Vector new_velocity;
Vector new_acceleration;

Trajectory trajectory; // The current trajectory
double time; // The current, auto-incremented time. Reset to 0 at a new calculation.

size_t new_section; // Index of the section between two (possibly filtered) intermediate positions.
bool did_section_change; // Was a new section reached in the last cycle?

bool new_calculation; // Whether a new calculation was performed in the last cycle
bool was_calculation_interrupted; // Was the trajectory calculation interrupted? (only in Pro Version)
double calculation_duration; // Duration of the calculation in the last cycle [µs]
```
Moreover, the **trajectory** class has a range of useful parameters and methods.

```.cpp
double duration; // Duration of the trajectory
std::array<double, DOFs> independent_min_durations; // Time-optimal profile for each independent DoF

<...> at_time(double time); // Get the kinematic state of the trajectory at a given time
<...> get_position_extrema(); // Returns information about the position extrema and their times
```
Again, we refer to the [API documentation](https://docs.ruckig.com) for the exact signatures.


### Offline Calculation

Ruckig also supports an offline approach for calculating a trajectory:
```.cpp
result = ruckig.calculate(input, trajectory);
```
When only using this method, the `Ruckig` constructor does not need a control cycle (`delta_time`) as an argument. However if given, Ruckig supports stepping through the trajectory with
```.cpp
while (ruckig.update(trajectory, output) == Result::Working) {
  // Make use of the new state here!
  // e.g. robot->setJointPositions(output.new_position);
}
```
starting from the current `output.time` (currently Ruckig Pro only).


### Tracking Interface

When following an arbitrary signal with position, velocity, acceleration, and jerk-limitation, the straight forward way would be to pass the current state to Ruckig's target state. However, as the resulting trajectory will take time to catch up, this approach will always lag behind the signal. The tracking interface solves this problem by predicting ahead (e.g. with constant acceleration by default) and is therefore able to follow signals very closely in a time-optimal way. This might be very helpful for (general) tracking, robot servoing, or trajectory post-processing applications.

To use the tracking interface, construct
```.cpp
Trackig<1> trackig {0.01};  // control cycle
```
and set the current state as well as the kinematic constraints via
```.cpp
input.current_position = {0.0};
input.current_velocity = {0.0};
input.current_acceleration = {0.0};
input.max_velocity = {0.8};
input.max_acceleration = {2.0};
input.max_jerk = {5.0};
```
Then, we can track a signal in an online manner within the real-time control loop
```.cpp
for (double t = 0; t < 10.0; t += trackig.delta_time) {
  auto target_state = signal(t); // signal returns position, velocity, and acceleration
  auto res = trackig.update(target_state, input, output);
  // Make use of the smooth target motion here (e.g. output.new_position)

  output.pass_to_input(input);
}
```
Please find a complete example [here](https://github.com/pantor/ruckig/blob/main/examples/13_tracking.cpp). This functionality can also be used in an offline manner, e.g. when the entire signal is known beforehand. Here, we call the
```.cpp
smooth_trajectory = trackig.calculate_trajectory(target_states, input);
```
method with the trajectory given as a `std::vector` of target states. The Tracking interface is available in the Ruckig Pro version.


### Dynamic Number of Degrees of Freedom

So far, we have told Ruckig the number of DoFs as a template parameter. If you don't know the number of DoFs at compile-time, you can set the template parameter to `ruckig::DynamicDOFs` and pass the DoFs to the constructor:

```.cpp
Ruckig<DynamicDOFs> ruckig {6, 0.001};
InputParameter<DynamicDOFs> input {6};
OutputParameter<DynamicDOFs> output {6};
```

This switches the default Vector from the `std::array` to the dynamic `std::vector` type. However, we recommend to keep the template parameter when possible: First, it has a performance benefit of a few percent. Second, it is convenient for real-time programming due to its easier handling of memory allocations. When using dynamic degrees of freedom, make sure to allocate the memory of all vectors beforehand.


### Custom Vector Types

Ruckig supports custom vector types to make interfacing with your code even easier and more flexible. Most importantly, you can switch to [Eigen Vectors](https://eigen.tuxfamily.org) simply by including Eigen (3.4 or later) before Ruckig
```.cpp
#include <Eigen/Core> // Version 3.4 or later
#include <ruckig/ruckig.hpp>
```
and then call the constructors with the `ruckig::EigenVector` parameter.
```.cpp
Ruckig<6, EigenVector> ruckig {0.001};
InputParameter<6, EigenVector> input;
OutputParameter<6, EigenVector> output;
```
Now every in- and output of Ruckig's API (such as `current_position`, `new_position` or `max_jerk`) are Eigen types! To define completely custom vector types, you can pass a C++ [template template parameter](https://en.cppreference.com/w/cpp/language/template_parameters) to the constructor. This template template parameter needs to fulfill a range of template arguments and methods:
```.cpp
template<class Type, size_t DOFs>
struct MinimalVector {
  Type operator[](size_t i) const; // Array [] getter
  Type& operator[](size_t i); // Array [] setter
  size_t size() const; // Current size
  bool operator==(const MinimalVector<T, DOFs>& rhs) const; // Equal comparison operator

  // Only required in combination with DynamicDOFs, e.g. to allocate memory
  void resize(size_t size);
};
```
Note that `DynamicDOFs` corresponds to `DOFs = 0`. We've included a range of examples for using Ruckig with [(10) Eigen](https://github.com/pantor/ruckig/blob/main/examples/10_eigen_vector_type.cpp), [(11) custom vector types](https://github.com/pantor/ruckig/blob/main/examples/11_custom_vector_type.cpp), and [(12) custom types with a dynamic number of DoFs](https://github.com/pantor/ruckig/blob/main/examples/12_custom_vector_type_dynamic_dofs.cpp).


## Tests and Numerical Stability

The current test suite validates over 5.000.000.000 random trajectories as well as many additional edge cases. The numerical exactness is tested for the final position and final velocity to be within `1e-8`, for the final acceleration to be within `1e-10`, and for the velocity, acceleration and jerk limit to be within of a numerical error of `1e-12`. These are absolute values - we suggest to scale your input so that these correspond to your required precision of the system. For example, for most real-world systems we suggest to use input values in `[m]` (instead of e.g. `[mm]`), as `1e-8m` is sufficient precise for practical trajectory generation. Furthermore, all kinematic limits should be below `1e9`. The maximal supported trajectory duration is `7e3`. Note that Ruckig will also output values outside of this range, there is however no guarantee for correctness.

The Ruckig Pro version has additional tools to increase the numerical range and improve reliability. For example, the`position_scale` and `time_scale` parameter of the `Calculator` class change the internal representation of the input parameters.
```.cpp
Ruckig<1> ruckig;  // Works also for Trackig

ruckig.calculator.position_scale = 1e2;  // Scales all positions in the input parameters
ruckig.calculator.time_scale = 1e3;  // Scale all times in the input parameters
```
This way, you can easily achieve the requirements above even for very high jerk limits or very long trajectories. Note that the scale parameters don't effect the resulting trajectory - they are for internal calculation only.


## Benchmark

We find that Ruckig is more than twice as fast as Reflexxes Type IV for state-to-state motions and well-suited for control cycles as low as 250 microseconds. The Ruckig *Community Version* is in general a more powerful and open-source alternative to the [Reflexxes Type IV](http://reflexxes.ws/) library. In fact, Ruckig is the first Type V trajectory generator for arbitrary target states and even supports directional velocity and acceleration limits, while also being faster on top.

![Benchmark](https://github.com/pantor/ruckig/raw/main/doc/benchmark.png?raw=true)

For trajectories with intermediate waypoints, we compare Ruckig to [Toppra](https://github.com/hungpham2511/toppra), a state-of-the-art library for robotic motion planning. Ruckig is able to improve the trajectory duration on average by around 10%, as the path planning and time parametrization are calculated jointly. Moreover, Ruckig is real-time capable and supports jerk-constraints.

![Benchmark](https://github.com/pantor/ruckig/raw/main/doc/ruckig_toppra_example.png?raw=true)



## Development

Ruckig is written in C++20. It is continuously tested on `ubuntu-latest`, `macos-latest`, and `windows-latest` against following versions:

- Doctest v2.4 (only for testing)
- Nanobind v2.7 (only for Python wrapper)

A C++17, C++11, and C++03 version of Ruckig is also available - please contact us if you're interested.


## Used By

Ruckig is used by over hundred research labs, companies, and open-source projects worldwide, including:
- [MoveIt 2](https://moveit.ros.org) for trajectory generation.
- [CoppeliaSim](https://www.coppeliarobotics.com/) starting from version 4.3.
- [Fuzzy Logic Robotics](https://flr.io)
- [Gestalt Robotics](https://www.gestalt-robotics.com)
- [Struckig](https://github.com/stefanbesler/struckig), a port of Ruckig to Structered Text (ST - IEC61131-3) for usage on PLCs.
- [Scanlab](https://www.scanlab.de/de) for controlling lasers.
- [Frankx](https://github.com/pantor/frankx) for controlling the Franka Emika robot arm.
- [Wiredworks](https://wiredworks.com) made a simple Kivy [GUI application](https://github.com/wiredworks/ruckig-showcase)
- and many others!


## Citation

```
@article{berscheid2021jerk,
  title={Jerk-limited Real-time Trajectory Generation with Arbitrary Target States},
  author={Berscheid, Lars and Kr{\"o}ger, Torsten},
  journal={Robotics: Science and Systems XVII},
  year={2021}
}
```
