from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


class Plotter:
    @staticmethod
    def plot_trajectory(filename, otg, inp, out_list, show=False, plot_acceleration=True, plot_jerk=True, time_offsets=None, title=None):
        taxis = np.array(list(map(lambda x: x.time, out_list)))
        if time_offsets:
            taxis += np.array(time_offsets)
        qaxis = np.array(list(map(lambda x: x.new_position, out_list)))
        dqaxis = np.array(list(map(lambda x: x.new_velocity, out_list)))
        ddqaxis = np.array(list(map(lambda x: x.new_acceleration, out_list)))
        dddqaxis = np.diff(ddqaxis, axis=0, prepend=ddqaxis[0, 0]) / otg.delta_time
        dddqaxis[0, :] = 0.0
        dddqaxis[-1, :] = 0.0

        plt.figure(figsize=(8.0, 2.0 + 3.0 * inp.degrees_of_freedom), dpi=120)
        plt.subplot(inp.degrees_of_freedom, 1, 1)

        if title:
            plt.title(title)

        for dof in range(inp.degrees_of_freedom):
            global_max = np.max([qaxis[:, dof], dqaxis[:, dof], ddqaxis[:, dof]])
            global_min = np.min([qaxis[:, dof], dqaxis[:, dof], ddqaxis[:, dof]])

            if plot_jerk:
                global_max = max(global_max, np.max(dddqaxis[:, dof]))
                global_min = min(global_min, np.min(dddqaxis[:, dof]))

            plt.subplot(inp.degrees_of_freedom, 1, dof + 1)
            plt.ylabel(f'DoF {dof + 1}')
            plt.plot(taxis, qaxis[:, dof], label=f'Position {dof + 1}')
            plt.plot(taxis, dqaxis[:, dof], label=f'Velocity {dof + 1}')
            if plot_acceleration:
                plt.plot(taxis, ddqaxis[:, dof], label=f'Acceleration {dof + 1}')
            if plot_jerk:
                plt.plot(taxis, dddqaxis[:, dof], label=f'Jerk {dof + 1}')

            # Plot sections
            if hasattr(out_list[-1], 'trajectory'):
                linewidth = 1.0 if len(out_list[-1].trajectory.intermediate_durations) < 20 else 0.25
                for t in out_list[-1].trajectory.intermediate_durations:
                    plt.axvline(x=t, color='black', linestyle='--', linewidth=linewidth)

            # Plot limit lines
            if inp.min_position and inp.min_position[dof] > 1.4 * global_min:
                plt.axhline(y=inp.min_position[dof], color='grey', linestyle='--', linewidth=1.1)

            if inp.max_position and inp.max_position[dof] < 1.4 * global_max:
                plt.axhline(y=inp.max_position[dof], color='grey', linestyle='--', linewidth=1.1)

            if inp.max_velocity[dof] < 1.4 * global_max:
                plt.axhline(y=inp.max_velocity[dof], color='orange', linestyle='--', linewidth=1.1)

            min_velocity = inp.min_velocity[dof] if inp.min_velocity else -inp.max_velocity[dof]
            if min_velocity > 1.4 * global_min:
                plt.axhline(y=min_velocity, color='orange', linestyle='--', linewidth=1.1)

            if plot_acceleration and inp.max_acceleration[dof] < 1.4 * global_max:
                plt.axhline(y=inp.max_acceleration[dof], color='g', linestyle='--', linewidth=1.1)

            min_acceleration = inp.min_acceleration[dof] if inp.min_acceleration else -inp.max_acceleration[dof]
            if plot_acceleration and min_acceleration > 1.4 * global_min:
                plt.axhline(y=min_acceleration, color='g', linestyle='--', linewidth=1.1)

            if plot_jerk and inp.max_jerk[dof] < 1.4 * global_max:
                plt.axhline(y=inp.max_jerk[dof], color='red', linestyle='--', linewidth=1.1)

            if plot_jerk and -inp.max_jerk[dof] > 1.4 * global_min:
                plt.axhline(y=-inp.max_jerk[dof], color='red', linestyle='--', linewidth=1.1)

            plt.legend()
            plt.grid(True)

        plt.xlabel('t')
        plt.savefig(Path(__file__).parent.parent / 'build' / filename)

        if show:
            plt.show()
