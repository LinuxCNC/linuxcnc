# This example shows the usage of intermediate waypoints. It will only work with Ruckig Pro or enabled cloud API (e.g. default when installed by pip / PyPI).

from copy import copy

from ruckig import InputParameter, OutputParameter, Result, Ruckig


if __name__ == '__main__':
    # Create instances: the Ruckig OTG as well as input and output parameters
    otg = Ruckig(3, 0.01, 10)  # DoFs, control cycle rate, maximum number of intermediate waypoints for memory allocation
    inp = InputParameter(3)  # DoFs
    out = OutputParameter(3, 10)  # DoFs, maximum number of intermediate waypoints for memory allocation

    inp.current_position = [0.8, 0, 0.5]
    inp.current_velocity = [0, 0, 0]
    inp.current_acceleration = [0, 0, 0]

    inp.intermediate_positions = [
        [1.4, -1.6, 1.0],
        [-0.6, -0.5, 0.4],
        [-0.4, -0.35, 0.0],
        [-0.2, 0.35, -0.1],
        [0.2, 0.5, -0.1],
        [0.8, 1.8, -0.1],
    ]

    inp.target_position = [0.5, 1.2, 0]
    inp.target_velocity = [0, 0, 0]
    inp.target_acceleration = [0, 0, 0]

    inp.max_velocity = [3, 2, 2]
    inp.max_acceleration = [6, 4, 4]
    inp.max_jerk = [16, 10, 20]

    # Define a minimum duration per section of the trajectory (number waypoints + 1)
    inp.per_section_minimum_duration = [0, 2.0, 0.0, 1.0, 0.0, 2.0, 0]

    print('\t'.join(['t'] + [str(i) for i in range(otg.degrees_of_freedom)]))

    # Generate the trajectory within the control loop
    first_output, out_list = None, []
    res = Result.Working
    while res == Result.Working:
        res = otg.update(inp, out)

        print('\t'.join([f'{out.time:0.3f}'] + [f'{p:0.3f}' for p in out.new_position]))
        out_list.append(copy(out))

        out.pass_to_input(inp)

        if not first_output:
            first_output = copy(out)

    print(f'Calculation duration: {first_output.calculation_duration:0.1f} [µs]')
    print(f'Trajectory duration: {first_output.trajectory.duration:0.4f} [s]')

    # Plot the trajectory
    # from pathlib import Path
    # from plotter import Plotter

    # project_path = Path(__file__).parent.parent.absolute()
    # Plotter.plot_trajectory(project_path / 'examples' / '08_trajectory.pdf', otg, inp, out_list, plot_jerk=False)
