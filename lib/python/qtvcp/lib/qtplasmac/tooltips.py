'''
tooltips.py

Copyright (C) 2021 - 2024 Phillip A Carter
Copyright (C) 2021 - 2024 Gregory D Carl

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

from PyQt5.QtCore import QCoreApplication

_translate = QCoreApplication.translate


#########################################################################################################################
# TOOL TIP FUNCTIONS #
#########################################################################################################################
def tool_tips_changed(P, W):
    if W.chk_tool_tips.isChecked():
        set_tool_tips(P, W)
    else:
        clear_tool_tips(W)


def clear_tool_tips(W):
    main_preview_widgets = [
        'material_selector', 'velocity_show', 'view_p', 'view_z', 'view_clear',
        'pan_right', 'pan_left', 'pan_up', 'pan_down', 'zoom_in', 'zoom_out']

    main_machine_widgets = [
        'estop', 'power', 'run', 'pause', 'abort', 'feed_slider', 'rapid_slider',
        'jog_slider', 'feed_label', 'rapid_label', 'jogs_label']

    main_button_widgets = [
        'file_edit', 'mdi_show']

    main_arc_widgets = [
        'arc_voltage', 'led_arc_ok', 'height_reset', 'height_lower', 'height_ovr_label',
        'height_raise']

    main_control_widgets = [
        'led_torch_on', 'led_corner_lock', 'led_void_lock']

    main_sensor_widgets = [
        'mesh_enable', 'ignore_arc_ok', 'pmx485_label', 'torch_enable',
        'cornerlock_enable', 'voidlock_enable', 'use_auto_volts',
        'ohmic_probe_enable', 'pmx485_enable']

    main_thc_widgets = [
        'led_float_switch', 'led_ohmic_probe', 'led_breakaway_switch', 'thc_auto',
        'thc_enable', 'led_thc_up', 'led_thc_enabled', 'led_thc_down', 'led_thc_active']

    main_jog_widgets = [
        'jogincrements', 'jog_slow', 'jog_z_minus', 'jog_z_plus', 'jog_x_minus',
        'jog_x_plus', 'jog_a_plus', 'jog_a_minus', 'jog_y_plus', 'jog_y_minus',
        'jog_b_minus', 'jog_b_plus', 'jog_c_minus', 'jog_c_plus']

    main_cut_rec_widgets = [
        'cut_rec_cancel', 'cut_rec_rev', 'cut_rec_speed', 'cut_rec_fwd', 'cut_rec_feed',
        'cut_rec_se', 'cut_rec_s', 'cut_rec_nw', 'cut_rec_e', 'cut_rec_sw', 'cut_rec_w',
        'cut_rec_n', 'cut_rec_ne', 'cut_rec_move_label']

    main_gcode_widgets = [
        'file_open', 'file_reload']

    main_dro_widgets = [
        'dro_a', 'dro_b', 'dro_x', 'dro_y', 'dro_z', 'home_a', 'home_b', 'home_x', 'home_y',
        'home_z', 'home_all', 'touch_a', 'touch_b', 'touch_x', 'touch_y', 'touch_z',
        'touch_xy', 'wcs_button', 'camera', 'laser']

    main_file_widgets = [
        'file_select', 'file_next', 'file_prev', 'file_cancel']

    main_camera_widgets = [
        'cam_dia_plus', 'cam_dia_minus', 'cam_zoom_plus', 'cam_zoom_minus', 'cam_goto',
        'cam_mark']

    conversational_widgets = [
        'conv_line', 'conv_circle', 'conv_ellipse', 'conv_triangle', 'conv_rectangle',
        'conv_polygon', 'conv_bolt', 'conv_slot', 'conv_star', 'conv_gusset', 'conv_sector',
        'conv_block', 'conv_new', 'conv_save', 'conv_settings', 'conv_send']

    parameters_configuration_widgets = [
        'arc_fail_delay', 'arc_max_starts', 'arc_restart_delay', 'arc_voltage_scale',
        'arc_voltage_offset', 'height_per_volt', 'arc_ok_high', 'arc_ok_low',
        'thc_delay', 'thc_threshold', 'pid_p_gain', 'pid_i_gain', 'pid_d_gain',
        'cornerlock_threshold', 'voidlock_slope', 'float_switch_travel',
        'probe_feed_rate', 'probe_start_height', 'ohmic_probe_offset',
        'ohmic_max_attempts', 'skip_ihs_distance', 'offset_feed_rate', 'safe_height',
        'scribe_arm_delay', 'scribe_on_delay', 'spotting_threshold', 'spotting_time',
        'setup_feed_rate', 'save_plasma', 'reload_plasma']

    parameters_material_widgets = [
        'kerf_width', 'pierce_height', 'pierce_delay', 'cut_height', 'cut_feed_rate',
        'cut_amps', 'cut_volts', 'puddle_jump_height', 'puddle_jump_delay',
        'pause_at_end', 'gas_pressure', 'cut_mode', 'save_material', 'reload_material',
        'new_material', 'delete_material']

    parameters_gui_widgets = [
        'color_foregrnd', 'color_foregalt', 'color_led', 'color_backgrnd',
        'color_backgalt', 'color_frams', 'color_estop', 'color_disabled',
        'color_preview', 'chk_soft_keyboard', 'chk_keyboard_shortcuts',
        'chk_overlay', 'opt_stp', 'chk_run_from_line', 'chk_override_limits',
        'chk_tool_tips', 'opt_blk', 'grid_size', 'cone_size', 'chk_exit_warning']

    parameters_utilities_widgets = [
        'actionbutton_halshow', 'actionbutton_halscope', 'actionbutton_halmeter',
        'actionbutton_calibration', 'actionbutton_lcnc_status', 'backup']

    statistics_widgets = [
        'pierce_reset', 'cut_length_reset', 'cut_time_reset', 'torch_time_reset',
        'run_time_reset', 'rapid_time_reset', 'probe_time_reset', 'all_reset',
        'pierce_count', 'cut_length', 'cut_time', 'torch_time', 'run_time',
        'rapid_time', 'probe_time', 'pierce_count_t', 'cut_length_t', 'cut_time_t',
        'torch_time_t', 'run_time_t', 'rapid_time_t', 'probe_time_t',
        'pmx_arc_time_t']

    W.statusbar.setToolTip('')
    for widget in main_preview_widgets:
        W[widget].setToolTip('')
    for widget in main_machine_widgets:
        W[widget].setToolTip('')
    for widget in main_button_widgets:
        W[widget].setToolTip('')
    for b in range(1, 21):
        W[f'button_{b}'].setToolTip(_translate('ToolTips', ''))
    for widget in main_arc_widgets:
        W[widget].setToolTip('')
    for widget in main_control_widgets:
        W[widget].setToolTip('')
    for widget in main_sensor_widgets:
        W[widget].setToolTip('')
    for widget in main_thc_widgets:
        W[widget].setToolTip('')
    for widget in main_jog_widgets:
        W[widget].setToolTip('')
    for widget in main_cut_rec_widgets:
        W[widget].setToolTip('')
    for widget in main_gcode_widgets:
        W[widget].setToolTip('')
    for widget in main_dro_widgets:
        W[widget].setToolTip('')
    for widget in main_file_widgets:
        W[widget].setToolTip('')
    for widget in main_camera_widgets:
        W[widget].setToolTip('')
    for widget in conversational_widgets:
        W[widget].setToolTip('')
    for widget in parameters_configuration_widgets:
        W[widget].setToolTip('')
    for widget in parameters_material_widgets:
        W[widget].setToolTip('')
    for widget in parameters_gui_widgets:
        W[widget].setToolTip('')
    for widget in parameters_utilities_widgets:
        W[widget].setToolTip('')
    for widget in statistics_widgets:
        W[widget].setToolTip('')


def set_tool_tips(P, W):
    # main widgets
    W.statusbar.setToolTip(_translate('ToolTips', 'Shows active G- and M-Codes'))

    # main_preview_widgets
    text0 = _translate('ToolTips', 'Shows the currently loaded material')
    text1 = _translate('ToolTips', 'Click to change material')
    W.material_selector.setToolTip(_translate('ToolTips', f'{text0}\n{text1}'))
    W.velocity_show.setToolTip(_translate('ToolTips', 'Shows the current velocity in units per minute'))
    W.view_p.setToolTip(_translate('ToolTips', 'Changes the view to perspective view'))
    W.view_z.setToolTip(_translate('ToolTips', 'Show the view from the top'))
    W.view_clear.setToolTip(_translate('ToolTips', 'Clears motion segments from preview'))
    W.pan_right.setToolTip(_translate('ToolTips', 'Pans the view right'))
    W.pan_left.setToolTip(_translate('ToolTips', 'Pans the view left'))
    W.pan_up.setToolTip(_translate('ToolTips', 'Pans the view up'))
    W.pan_down.setToolTip(_translate('ToolTips', 'Pans the view down'))
    W.zoom_in.setToolTip(_translate('ToolTips', 'Zooms in'))
    W.zoom_out.setToolTip(_translate('ToolTips', 'Zooms out'))

    # main_machine_widgets
    W.estop.setToolTip(_translate('ToolTips', 'Shows the Estop status'))
    W.power.setToolTip(_translate('ToolTips', 'Switches the GUI on or off\n\nA long press displays the GUI shutdown dialog'))
    W.run.setToolTip(_translate('ToolTips', 'Runs the currently loaded G-Code program'))
    W.pause.setToolTip(_translate('ToolTips', 'Pauses or resume the currently running G-Code program'))
    W.abort.setToolTip(_translate('ToolTips', 'Stops the currently running process'))
    W.feed_slider.setToolTip(_translate('ToolTips', 'Sets the feed override percentage'))
    W.rapid_slider.setToolTip(_translate('ToolTips', 'Sets the rapid override percentage'))
    W.jog_slider.setToolTip(_translate('ToolTips', 'Sets the jog feed rate'))
    W.feed_label.setToolTip(_translate('ToolTips', 'Click to reset the feed slider to 100%'))
    W.rapid_label.setToolTip(_translate('ToolTips', 'Click to reset the rapid slider to 100%'))
    W.jogs_label.setToolTip(_translate('ToolTips', 'Click to reset the jog slider to the default velocity'))

    # main_button_widgets
    W.file_edit.setToolTip(_translate('ToolTips', 'Toggles the use of the G-Code file editor'))
    W.mdi_show.setToolTip(_translate('ToolTips', 'Toggles the use of Manual Data Input mode'))
    text0 = _translate('ToolTips', 'User button')
    text1 = _translate('ToolTips', 'configured in the SETTINGS tab')
    for b in range(1, 21):
        W[f'button_{b}'].setToolTip(_translate('ToolTips', f'{text0} #{b} {text1}'))

    # main_arc_widgets
    W.arc_voltage.setToolTip(_translate('ToolTips', 'Shows the current arc voltage'))
    W.led_arc_ok.setToolTip(_translate('ToolTips', 'Shows the Arc OK signal status'))
    W.height_reset.setToolTip(_translate('ToolTips', 'Click to reset the THC override voltage to zero'))
    W.height_lower.setToolTip(_translate('ToolTips', 'Lowers the target voltage for THC override'))
    W.height_ovr_label.setToolTip(_translate('ToolTips', 'Shows the THC target voltage override amount'))
    W.height_raise.setToolTip(_translate('ToolTips', 'Raises the target voltage for THC override'))

    # main_control_widgets
    W.led_torch_on.setToolTip(_translate('ToolTips', 'Shows the status of the torch on signal'))
    W.led_corner_lock.setToolTip(_translate('ToolTips', 'Shows the velocity anti-dive status'))
    W.led_void_lock.setToolTip(_translate('ToolTips', 'Shows the void anti-dive status'))
    W.mesh_enable.setToolTip(_translate('ToolTips', 'Toggles the use of mesh mode cutting'))
    W.ignore_arc_ok.setToolTip(_translate('ToolTips', 'Toggles the need for an Arc OK signal before XY motion begins'))
    W.pmx485_label.setToolTip(_translate('ToolTips', 'Shows the status of PMX485 communications'))
    W.torch_enable.setToolTip(_translate('ToolTips', 'Toggles the use of the torch'))
    W.cornerlock_enable.setToolTip(_translate('ToolTips', 'Toggles the use of velocity anti-dive'))
    W.voidlock_enable.setToolTip(_translate('ToolTips', 'Toggles the use of void anti-dive'))
    W.use_auto_volts.setToolTip(_translate('ToolTips', 'Toggles the use of automatic voltage for THC'))
    W.ohmic_probe_enable.setToolTip(_translate('ToolTips', 'Toggles the use of ohmic probing'))
    W.pmx485_enable.setToolTip(_translate('ToolTips', 'Toggles the use of Powermax communications'))

    # main_sensor_widgets
    W.led_float_switch.setToolTip(_translate('ToolTips', 'Shows the float switch status'))
    W.led_ohmic_probe.setToolTip(_translate('ToolTips', 'Shows the ohmic probe status'))
    W.led_breakaway_switch.setToolTip(_translate('ToolTips', 'Shows the breakaway switch status'))

    # main_thc_widgets
    W.thc_auto.setToolTip(_translate('ToolTips', 'Toggles the type of automatic THC activation'))
    W.thc_enable.setToolTip(_translate('ToolTips', 'Toggles the use of torch height control'))
    W.led_thc_up.setToolTip(_translate('ToolTips', 'Shows the THC is moving the Z axis up'))
    W.led_thc_enabled.setToolTip(_translate('ToolTips', 'Shows the THC status'))
    W.led_thc_down.setToolTip(_translate('ToolTips', 'Shows the THC is moving the Z axis down'))
    W.led_thc_active.setToolTip(_translate('ToolTips', 'Shows the THC is currently in control of the Z axis'))

    # main_jog_widgets():
    W.jogincrements.setToolTip(_translate('ToolTips', 'Sets the jog increment'))
    W.jog_slow.setToolTip(_translate('ToolTips', 'Toggles the jog feed rate by a factor of 10'))
    W.jog_a_minus.setToolTip(_translate('ToolTips', 'Jogs the A axis counter negative'))
    W.jog_a_plus.setToolTip(_translate('ToolTips', 'Jogs the A axis positive'))
    W.jog_b_minus.setToolTip(_translate('ToolTips', 'Jogs the B axis negative'))
    W.jog_b_plus.setToolTip(_translate('ToolTips', 'Jogs the B axis positive'))
    W.jog_c_minus.setToolTip(_translate('ToolTips', 'Jogs the C axis negative'))
    W.jog_c_plus.setToolTip(_translate('ToolTips', 'Jogs the C axis positive'))
    W.jog_x_minus.setToolTip(_translate('ToolTips', 'Jogs the X axis negative'))
    W.jog_x_plus.setToolTip(_translate('ToolTips', 'Jogs the X axis positive'))
    W.jog_y_minus.setToolTip(_translate('ToolTips', 'Jogs the Y axis negative'))
    W.jog_y_plus.setToolTip(_translate('ToolTips', 'Jogs the Y axis positive'))
    W.jog_z_minus.setToolTip(_translate('ToolTips', 'Jogs the Z axis negative'))
    W.jog_z_plus.setToolTip(_translate('ToolTips', 'Jogs the Z axis positive'))

    # main_cut_rec_widgets
    W.cut_rec_cancel.setToolTip(_translate('ToolTips', 'Cancels cut recovery'))
    W.cut_rec_fwd.setToolTip(_translate('ToolTips', 'Moves the torch forward along current motion segment'))
    W.cut_rec_rev.setToolTip(_translate('ToolTips', 'Moves the torch reverse along current motion segment'))
    text0 = _translate('ToolTips', 'Changes the feed rate for paused motion')
    text1 = _translate('ToolTips', 'Slider position indicates percentage of feed rate for current material')
    W.cut_rec_speed.setToolTip(_translate('ToolTips', f'{text0}\n{text1}'))
    W.cut_rec_feed.setToolTip(_translate('ToolTips', 'Shows the feed rate for paused motion'))
    W.cut_rec_move_label.setToolTip(_translate('ToolTips', 'Shows the distance the torch will move'))
    text0 = _translate('ToolTips', 'Moves the torch in the')
    text1 = _translate('ToolTips', 'direction')
    W.cut_rec_e.setToolTip(f'{text0} X+ {text1}')
    W.cut_rec_n.setToolTip(f'{text0} Y+ {text1}')
    W.cut_rec_ne.setToolTip(f'{text0} X+ Y+ {text1}')
    W.cut_rec_nw.setToolTip(f'{text0} X- Y+ {text1}')
    W.cut_rec_s.setToolTip(f'{text0} Y- {text1}')
    W.cut_rec_se.setToolTip(f'{text0} X+ Y- {text1}')
    W.cut_rec_sw.setToolTip(f'{text0} X- Y- {text1}')
    W.cut_rec_w.setToolTip(f'{text0} X- {text1}')

    # main gcode widgets
    W.file_open.setToolTip(_translate('ToolTips', 'Opens the file selector'))
    W.file_reload.setToolTip(_translate('ToolTips', 'Reloads the current G-Code file'))

    # main dro widgets
    text0 = _translate('ToolTips', 'Shows the current')
    text1 = _translate('ToolTips', 'axis position')
    W.dro_a.setToolTip(f'{text0} A {text1}')
    W.dro_b.setToolTip(f'{text0} B {text1}')
    W.dro_c.setToolTip(f'{text0} C {text1}')
    W.dro_x.setToolTip(f'{text0} X {text1}')
    W.dro_y.setToolTip(f'{text0} Y {text1}')
    W.dro_z.setToolTip(f'{text0} Z {text1}')
    text0 = _translate('ToolTips', 'Homes the')
    text1 = _translate('ToolTips', 'axis')
    W.home_a.setToolTip(f'{text0} A {text1}')
    W.home_b.setToolTip(f'{text0} B {text1}')
    W.home_c.setToolTip(f'{text0} C {text1}')
    W.home_x.setToolTip(f'{text0} X {text1}')
    W.home_y.setToolTip(f'{text0} Y {text1}')
    W.home_z.setToolTip(f'{text0} Z {text1}')
    W.home_all.setToolTip(_translate('ToolTips', 'Homes all axes'))
    text0 = _translate('ToolTips', 'Touches off the')
    text1 = _translate('ToolTips', 'axis')
    W.touch_a.setToolTip(f'{text0} A {text1}')
    W.touch_b.setToolTip(f'{text0} B {text1}')
    W.touch_c.setToolTip(f'{text0} C {text1}')
    W.touch_x.setToolTip(f'{text0} X {text1}')
    W.touch_y.setToolTip(f'{text0} Y {text1}')
    W.touch_z.setToolTip(f'{text0} Z {text1}')
    text1 = _translate('ToolTips', 'axes to zero')
    W.touch_xy.setToolTip(f'{text0} X & Y {text1}')
    W.wcs_button.setToolTip(_translate('ToolTips', 'Selects the active work coordinate system'))
    W.camera.setToolTip(_translate('ToolTips', 'Use camera view to set origin/rotation'))
    W.laser.setToolTip(_translate('ToolTips', 'Use laser to set origin/rotation\nLong press to begin a dry run'))

    # main file widgets
    W.file_select.setToolTip(_translate('ToolTips', 'Opens the selected G-Code file'))
    W.file_next.setToolTip(_translate('ToolTips', 'Moves the file selector down'))
    W.file_prev.setToolTip(_translate('ToolTips', 'Moves the file selector up'))
    W.file_cancel.setToolTip(_translate('ToolTips', 'Closes the file selector'))

    # main camera widgets
    W.cam_dia_plus.setToolTip(_translate('ToolTips', 'Increases the diameter of the overlay circle'))
    W.cam_dia_minus.setToolTip(_translate('ToolTips', 'Decreases the diameter of the overlay circle'))
    W.cam_zoom_plus.setToolTip(_translate('ToolTips', 'Zooms in'))
    W.cam_zoom_minus.setToolTip(_translate('ToolTips', 'Zooms out'))
    W.cam_goto.setToolTip(_translate('ToolTips', 'Moves the torch to the origin (X0 Y0)'))
    W.cam_mark.setToolTip(_translate('ToolTips', 'Starts the origin marking process'))

    # conversational widgets
    W.conv_line.setToolTip(_translate('ToolTips', 'Create a line or arc'))
    W.conv_circle.setToolTip(_translate('ToolTips', 'Create a circle'))
    W.conv_ellipse.setToolTip(_translate('ToolTips', 'Create an ellipse'))
    W.conv_triangle.setToolTip(_translate('ToolTips', 'Create a triangle'))
    W.conv_rectangle.setToolTip(_translate('ToolTips', 'Create a rectangle'))
    W.conv_polygon.setToolTip(_translate('ToolTips', 'Create a polygon'))
    W.conv_bolt.setToolTip(_translate('ToolTips', 'Create a circular bolt pattern'))
    W.conv_slot.setToolTip(_translate('ToolTips', 'Create a slot'))
    W.conv_star.setToolTip(_translate('ToolTips', 'Create a star'))
    W.conv_gusset.setToolTip(_translate('ToolTips', 'Create a gusset'))
    W.conv_sector.setToolTip(_translate('ToolTips', 'Create a sector'))
    W.conv_block.setToolTip(_translate('ToolTips', 'Rotate, Scale, and Array the current G-Code program'))
    W.conv_new.setToolTip(_translate('ToolTips', 'Starts a new file'))
    W.conv_save.setToolTip(_translate('ToolTips', 'Saves the current file'))
    W.conv_settings.setToolTip(_translate('ToolTips', 'Opens the conversational settings panel'))
    W.conv_send.setToolTip(_translate('ToolTips', 'Loads the current file for cutting'))

    # parameters configuration widgets
    W.arc_fail_delay.setToolTip(_translate('ToolTips', 'Time for torch on with no Arc OK before failing (seconds)'))
    W.arc_max_starts.setToolTip(_translate('ToolTips', 'Number of arc start attempts before pausing'))
    W.arc_restart_delay.setToolTip(_translate('ToolTips', 'Delay between arc failure and next arc start attempt (seconds)'))
    W.arc_voltage_scale.setToolTip(_translate('ToolTips', 'Sets the input scale to display correct arc voltage'))
    W.arc_voltage_offset.setToolTip(_translate('ToolTips', 'Sets the offset to display zero volts at zero input'))
    W.height_per_volt.setToolTip(_translate('ToolTips', 'Distance the torch moves per volt (affects manual override only)'))
    W.arc_ok_high.setToolTip(_translate('ToolTips', 'Voltage value below which Arc OK is valid'))
    W.arc_ok_low.setToolTip(_translate('ToolTips', 'Voltage value above which Arc OK is valid'))
    W.thc_delay.setToolTip(_translate('ToolTips', 'Delay between Arc OK and THC activation (seconds)'))
    W.thc_sample_counts.setToolTip(_translate('ToolTips', 'Number of consecutive arc voltage readings within the sample threshold required to activate THC'))
    W.thc_sample_threshold.setToolTip(_translate('ToolTips', 'Maximum voltage deviation allowed for THC Sample Counts'))
    W.thc_threshold.setToolTip(_translate('ToolTips', 'Deviation from target voltage before THC attempts correction'))
    W.pid_p_gain.setToolTip(_translate('ToolTips', 'Proportional gain for the THC PID loop'))
    W.cornerlock_threshold.setToolTip(_translate('ToolTips', 'Percentage of cut feed rate reduction before THC locks'))
    W.voidlock_slope.setToolTip(_translate('ToolTips', 'Voltage change required to activate void anti-dive in volts per second'))
    W.pid_i_gain.setToolTip(_translate('ToolTips', 'Integral gain for the THC PID loop'))
    W.pid_d_gain.setToolTip(_translate('ToolTips', 'Derivative gain for the THC PID loop'))
    W.float_switch_travel.setToolTip(_translate('ToolTips', 'Distance of float travel before switch activation'))
    W.probe_feed_rate.setToolTip(_translate('ToolTips', 'Feed rate for probe move after move to Probe Height'))
    W.probe_start_height.setToolTip(_translate('ToolTips', 'Height above Z minimum limit that Probe Speed begins'))
    W.ohmic_probe_offset.setToolTip(_translate('ToolTips', 'Distance above material Z should move after an ohmic probe'))
    W.ohmic_max_attempts.setToolTip(_translate('ToolTips', 'Number of retry attempts before defaulting to float switch'))
    W.skip_ihs_distance.setToolTip(_translate('ToolTips', 'Initial Height Sense distance, refer to user guide for detailed explanation'))
    W.offset_feed_rate.setToolTip(_translate('ToolTips', f'Feed rate for offset probe moves in the X and Y axes, maximum = {int(P.offsetFeedRate)}'))
    W.safe_height.setToolTip(_translate('ToolTips', 'Height above material the torch will retract to before rapid moves'))
    W.scribe_arm_delay.setToolTip(_translate('ToolTips', 'Delay from scribe command to activation of scribe (seconds)'))
    W.scribe_on_delay.setToolTip(_translate('ToolTips', 'Delay from scribe activation to XY motion (seconds)'))
    text0 = _translate('ToolTips', 'Arc voltage threshold to start the spotting timer')
    text1 = _translate('ToolTips', 'Setting to 0V will start the Time On counter upon torch activation')
    W.spotting_threshold.setToolTip(_translate('ToolTips', f'{text0}\n{text1}'))
    W.spotting_time.setToolTip(_translate('ToolTips', 'Length of time torch is on after spotting threshold is met (milliseconds)'))
    W.setup_feed_rate.setToolTip(_translate('ToolTips', f'Z axis velocity for setup moves (Probe Height, Pierce Height, Cut Height), maximum = {int(P.thcFeedRate)}'))
    W.save_plasma.setToolTip(_translate('ToolTips', 'Saves the configuration changes'))
    W.reload_plasma.setToolTip(_translate('ToolTips', 'Discards the configuration changes and reloads the configuration'))

    # parameters material widgets
    text0 = _translate('ToolTips', 'Shows the currently loaded material')
    text1 = _translate('ToolTips', 'Click to change material')
    W.materials_box.setToolTip(_translate('ToolTips', f'{text0}\n{text1}'))
    W.kerf_width.setToolTip(_translate('ToolTips', 'Width of the material removed by the plasma arc'))
    W.pierce_height.setToolTip(_translate('ToolTips', 'Distance above the material the arc start will occur'))
    W.pierce_delay.setToolTip(_translate('ToolTips', 'Delay at Pierce Height after Arc OK before cut movements (seconds)'))
    W.cut_height.setToolTip(_translate('ToolTips', 'Distance above the material cut movements will occur at'))
    W.cut_feed_rate.setToolTip(_translate('ToolTips', 'Speed the torch will travel during cut movements'))
    text0 = _translate('ToolTips', 'Cutting current measured in Amps')
    text1 = _translate('ToolTips', 'Indicator only, unless using Powermax communications')
    W.cut_amps.setToolTip(_translate('ToolTips', f'{text0}\n{text1}'))
    W.cut_volts.setToolTip(_translate('ToolTips', 'Voltage target used for torch height adjustment if not using auto volts'))
    W.puddle_jump_height.setToolTip(_translate('ToolTips', 'Height transition between Pierce Height and Cut Height (percentage of Pierce Height)'))
    W.puddle_jump_delay.setToolTip(_translate('ToolTips', 'Length of time at Puddle Height before moving to Cut Height (seconds)'))
    W.pause_at_end.setToolTip(_translate('ToolTips', 'Delay before turning torch off at end of cut (seconds)'))
    text0 = _translate('ToolTips', 'Sets torch gas pressure')
    text1 = _translate('ToolTips', '0 = Use Powermax automatic pressure mode')
    W.gas_pressure.setToolTip(_translate('ToolTips', f'{text0}\n{text1}'))
    text0 = _translate('ToolTips', 'Powermax cut modes')
    text1 = _translate('ToolTips', '1 = Normal')
    text2 = _translate('ToolTips', '2 = CPA (Constant Pilot Arc)')
    text3 = _translate('ToolTips', '3 = Gouge/Mark')
    W.cut_mode.setToolTip(_translate('ToolTips', f'{text0}:\n{text1}\n{text2}\n{text3}'))
    W.save_material.setToolTip(_translate('ToolTips', 'Saves current material set'))
    W.reload_material.setToolTip(_translate('ToolTips', 'Discards any changes and reloads the material set'))
    W.new_material.setToolTip(_translate('ToolTips', 'Creates a new material'))
    W.delete_material.setToolTip(_translate('ToolTips', 'Deletes a material'))

    # parameters gui widgets
    W.color_foregrnd.setToolTip(_translate('ToolTips', 'Shows and changes the color of the foreground'))
    W.color_foregalt.setToolTip(_translate('ToolTips', 'Shows and changes the color of highlights'))
    W.color_led.setToolTip(_translate('ToolTips', 'Shows and changes the color of the LEDs'))
    W.color_backgrnd.setToolTip(_translate('ToolTips', 'Shows and changes the color of the background'))
    W.color_backgalt.setToolTip(_translate('ToolTips', 'Shows and changes the color of the alternate background'))
    W.color_frams.setToolTip(_translate('ToolTips', 'Shows and changes the color of the frames'))
    W.color_estop.setToolTip(_translate('ToolTips', 'Shows and changes the color of the estop button/indicator'))
    W.color_disabled.setToolTip(_translate('ToolTips', 'Shows and changes the color of disabled items'))
    W.color_preview.setToolTip(_translate('ToolTips', 'Shows and changes the color of the preview screen background'))
    W.chk_soft_keyboard.setToolTip(_translate('ToolTips', 'Toggles the use of the soft keyboard'))
    W.chk_keyboard_shortcuts.setToolTip(_translate('ToolTips', 'Toggles the use of keyboard shortcuts'))
    W.chk_overlay.setToolTip(_translate('ToolTips', 'Toggles the display of material property visibility on the preview screen'))
    W.opt_stp.setToolTip(_translate('ToolTips', 'Toggles pausing G-Code execution at M1 optional stops'))
    W.chk_run_from_line.setToolTip(_translate('ToolTips', 'Toggles the use of run from line'))
    W.chk_override_limits.setToolTip(_translate('ToolTips', 'Toggles the override of limit switches'))
    W.chk_tool_tips.setToolTip(_translate('ToolTips', 'Toggles the display of tooltips'))
    W.chk_exit_warning.setToolTip(_translate('ToolTips', 'Toggles always display an exit warning'))
    W.opt_blk.setToolTip(_translate('ToolTips', 'Toggles the execution of G-Code lines starting with "/"'))
    W.grid_size.setToolTip(_translate('ToolTips', 'Changes the size of the grid in the preview screen'))
    W.cone_size.setToolTip(_translate('ToolTips', 'Changes the size of the cone in the preview screen'))

    # parameters utilities widgets
    W.actionbutton_halshow.setToolTip(_translate('ToolTips', 'Loads the HalShow application'))
    W.actionbutton_halscope.setToolTip(_translate('ToolTips', 'Loads the HalScope application'))
    W.actionbutton_halmeter.setToolTip(_translate('ToolTips', 'Loads the HalMeter application'))
    W.actionbutton_calibration.setToolTip(_translate('ToolTips', 'Loads the LinuxCNC Calibration application'))
    W.actionbutton_lcnc_status.setToolTip(_translate('ToolTips', 'Loads the LinuxCNC Status application'))
    W.backup.setToolTip(_translate('ToolTips', 'Creates a complete backup of the current machine configuration'))

    # statisticswidgets
    W.pierce_reset.setToolTip(_translate('ToolTips', 'Resets the pierce counter'))
    W.cut_length_reset.setToolTip(_translate('ToolTips', 'Resets the cut length counter'))
    W.cut_time_reset.setToolTip(_translate('ToolTips', 'Resets the cut time counter'))
    W.torch_time_reset.setToolTip(_translate('ToolTips', 'Resets the torch on time counter'))
    W.run_time_reset.setToolTip(_translate('ToolTips', 'Resets the program run time counter'))
    W.rapid_time_reset.setToolTip(_translate('ToolTips', 'Resets the rapid time counter'))
    W.probe_time_reset.setToolTip(_translate('ToolTips', 'Resets the probe time counter'))
    W.all_reset.setToolTip(_translate('ToolTips', 'Resets all counters'))
    W.pierce_count.setToolTip(_translate('ToolTips', 'Shows the pierce count for the last job'))
    W.cut_length.setToolTip(_translate('ToolTips', 'Shows the cut length for the last job'))
    W.cut_time.setToolTip(_translate('ToolTips', 'Shows the cut time for the last job'))
    W.torch_time.setToolTip(_translate('ToolTips', 'Shows the torch in time for the last job'))
    W.run_time.setToolTip(_translate('ToolTips', 'Shows the program run time for the last job'))
    W.rapid_time.setToolTip(_translate('ToolTips', 'Shows the rapid time for the last job'))
    W.probe_time.setToolTip(_translate('ToolTips', 'Shows the probe time for the last job'))
    W.pierce_count_t.setToolTip(_translate('ToolTips', 'Shows the total pierce count since the last reset'))
    W.cut_length_t.setToolTip(_translate('ToolTips', 'Shows the total cut length since the last reset'))
    W.cut_time_t.setToolTip(_translate('ToolTips', 'Shows the total cut time since the last reset'))
    W.torch_time_t.setToolTip(_translate('ToolTips', 'Shows the total torch on time since the last reset'))
    W.run_time_t.setToolTip(_translate('ToolTips', 'Shows the total program run time since the last reset'))
    W.rapid_time_t.setToolTip(_translate('ToolTips', 'Shows the total rapid time since the last reset'))
    W.probe_time_t.setToolTip(_translate('ToolTips', 'Shows the total probe time since the last reset'))
    W.pmx_arc_time_t.setToolTip(_translate('ToolTips', 'Shows the total arc on time for the Powermax (hh:mm:ss)'))
