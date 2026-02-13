# Only with Ruckig Pro

from math import sin, cos

from ruckig import Trackig, TargetState, InputParameter, OutputParameter


# Create the target state signal
def model_ramp(t, ramp_vel=0.5, ramp_pos=1.0):
    target = TargetState(1)
    on_ramp = t < ramp_pos / abs(ramp_vel)
    target.position = [t * ramp_vel] if on_ramp else [ramp_pos]
    target.velocity = [ramp_vel] if on_ramp else [0.0]
    target.acceleration = [0.0]
    return target


def model_constant_acceleration(t, ramp_acc=0.05):
    target = TargetState(1)
    target.position = [t * t * ramp_acc]
    target.velocity = [t * ramp_acc]
    target.acceleration = [ramp_acc]
    return target


def model_sinus(t, ramp_vel=0.4):
    target = TargetState(1)
    target.position = [sin(ramp_vel * t)]
    target.velocity = [ramp_vel * cos(ramp_vel * t)]
    target.acceleration = [-ramp_vel * ramp_vel * sin(ramp_vel * t)]
    return target


if __name__ == '__main__':
    # Create instances: the Trackig OTG as well as input and output parameters
    inp = InputParameter(1)
    out = OutputParameter(inp.degrees_of_freedom)
    otg = Trackig(inp.degrees_of_freedom, 0.01)

    # Set input parameters
    inp.current_position = [0.0]
    inp.current_velocity = [0.0]
    inp.current_acceleration = [0.0]

    inp.max_velocity = [0.8]
    inp.max_acceleration = [2.0]
    inp.max_jerk = [5.0]

    # Optional minimum and maximum position
    inp.min_position = [-2.5]
    inp.max_position = [2.5]

    # otg.reactiveness = 1.0 # default value, should be in [0, 1]

    print('target | follow')

    # Generate the trajectory following the target state
    steps, target_list, follow_list = [], [], []
    for t in range(500):
        target_state = model_ramp(otg.delta_time * t)

        steps.append(t)
        res = otg.update(target_state, inp, out)

        out.pass_to_input(inp)

        print('\t'.join([f'{p:0.3f}' for p in target_state.position] + [f'{p:0.3f}' for p in out.new_position]) + f' in {out.calculation_duration:0.2f} [µs]')

        target_list.append([target_state.position, target_state.velocity, target_state.acceleration])
        follow_list.append([out.new_position, out.new_velocity, out.new_acceleration])

    # Plot the trajectory
    # from pathlib import Path
    # project_path = Path(__file__).parent.parent.absolute()

    # import numpy as np
    # import matplotlib.pyplot as plt

    # follow_list = np.array(follow_list)
    # target_list = np.array(target_list)

    # plt.ylabel(f'DoF 1')
    # plt.plot(steps, follow_list[:, 0], label='Follow Position')
    # plt.plot(steps, follow_list[:, 1], label='Follow Velocity', linestyle='dotted')
    # plt.plot(steps, follow_list[:, 2], label='Follow Acceleration', linestyle='dotted')
    # plt.plot(steps, target_list[:, 0], color='r', label='Target Position')
    # plt.grid(True)
    # plt.legend()

    # plt.savefig(project_path / 'examples' / '13_trajectory.pdf')
