from ruckig import InputParameter, Ruckig, Trajectory, Result


if __name__ == '__main__':
    inp = InputParameter(3)

    inp.current_position = [0.0, 0.0, 0.5]
    inp.current_velocity = [0.0, -2.2, -0.5]
    inp.current_acceleration = [0.0, 2.5, -0.5]

    inp.target_position = [5.0, -2.0, -3.5]
    inp.target_velocity = [0.0, -0.5, -2.0]
    inp.target_acceleration = [0.0, 0.0, 0.5]

    inp.max_velocity = [3.0, 1.0, 3.0]
    inp.max_acceleration = [3.0, 2.0, 1.0]
    inp.max_jerk = [4.0, 3.0, 2.0]

    # Set different constraints for negative direction
    inp.min_velocity = [-1.0, -0.5, -3.0]
    inp.min_acceleration = [-2.0, -1.0, -2.0]

    # We don't need to pass the control rate (cycle time) when using only offline features
    otg = Ruckig(3)
    trajectory = Trajectory(3)

    # Calculate the trajectory in an offline manner
    result = otg.calculate(inp, trajectory)
    if result == Result.ErrorInvalidInput:
        raise Exception('Invalid input!')

    print(f'Trajectory duration: {trajectory.duration:0.4f} [s]')

    new_time = 1.0

    # Then, we can calculate the kinematic state at a given time
    new_position, new_velocity, new_acceleration = trajectory.at_time(new_time)

    print(f'Position at time {new_time:0.4f} [s]: {new_position}')

    # Get some info about the position extrema of the trajectory
    print(f'Position extremas are {trajectory.position_extrema}')
