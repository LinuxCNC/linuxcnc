'''
tooltips.py

Copyright (C) 2021  Phillip A Carter
Copyright (C) 2021  Gregory D Carl

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

from PyQt5.QtCore import Qt, QCoreApplication

_translate = QCoreApplication.translate


#########################################################################################################################
# TOOL TIP FUNCTIONS #
#########################################################################################################################
def tool_tips_changed(W):
    if W.chk_tool_tips.isChecked():
        set_tool_tips(W)
    else:
        clear_tool_tips(W)

def clear_tool_tips(W):
    main_preview_widgets = [
    'material_selector','velocity_show','view_p','view_z','view_clear',
    'pan_right','pan_left','pan_up','pan_down','zoom_in','zoom_out']

    main_machine_widgets = [
    'estop','power','run','pause','abort','feed_slider','rapid_slider',
    'jog_slider','feed_label','rapid_label','jogs_label']

    main_button_widgets = [
    'file_edit','mdi_show']

    main_arc_widgets = [
    'arc_voltage','led_arc_ok','height_reset','height_lower','height_ovr_label',
    'height_raise']

    main_control_widgets = [
    'led_torch_on','led_corner_lock','led_kerf_lock']

    main_sensor_widgets = [
    'mesh_enable','ignore_arc_ok','pmx485_label','torch_enable',
    'cornerlock_enable','kerfcross_enable','use_auto_volts',
    'ohmic_probe_enable','pmx485_enable']

    main_thc_widgets = [
    'led_float_switch','led_ohmic_probe','led_breakaway_switch','thc_enable',
    'led_thc_up','led_thc_enabled','led_thc_down','led_thc_active']

    main_jog_widgets = [
    'jogincrements','jog_slow','jog_z_minus','jog_z_plus','jog_x_minus',
    'jog_x_plus','jog_a_plus','jog_a_minus','jog_y_plus','jog_y_minus',
    'jog_b_minus','jog_b_plus']

    main_cut_rec_widgets = [
    'cut_rec_cancel','cut_rec_rev','cut_rec_speed','cut_rec_fwd','cut_rec_feed',
    'cut_rec_se','cut_rec_s','cut_rec_nw','cut_rec_e','cut_rec_sw','cut_rec_w',
    'cut_rec_n','cut_rec_ne','cut_rec_move_label']

    main_gcode_widgets = [
    'file_open','file_reload','gcode_progress']

    main_dro_widgets = [
    'dro_a','dro_b','dro_x','dro_y','dro_z','home_a','home_b','home_x','home_y',
    'home_z','home_all','touch_a','touch_b','touch_x','touch_y','touch_z',
    'touch_xy','wcs_button','camera','laser']

    main_file_widgets = [
    'file_select','file_next','file_prev','file_cancel']

    main_camera_widgets = [
    'cam_dia_plus','cam_dia_minus','cam_zoom_plus','cam_zoom_minus','cam_goto',
    'cam_mark']

    conversational_widgets = [
    'conv_line','conv_circle','conv_ellipse','conv_triangle','conv_rectangle',
    'conv_polygon','conv_bolt','conv_slot','conv_star','conv_gusset',
    'conv_sector','conv_rotate','conv_scale','conv_array','conv_new',
    'conv_save','conv_settings','conv_send']

    parameters_configuration_widgets = [
    'arc_fail_delay','arc_max_starts','arc_restart_delay','arc_voltage_scale',
    'arc_voltage_offset','height_per_volt','arc_ok_high','arc_ok_low',
    'thc_delay','thc_threshold','pid_p_gain','cornerlock_threshold',
    'kerfcross_override','pid_i_gain','pid_d_gain','float_switch_travel',
    'probe_feed_rate','probe_start_height','ohmic_probe_offset',
    'ohmic_max_attempts','skip_ihs_distance','safe_height','scribe_arm_delay',
    'scribe_on_delay','spotting_threshold','spotting_time',
    'max_offset_velocity_in','setup_feed_rate','save_plasma','reload_plasma']

    parameters_material_widgets = [
    'kerf_width','pierce_height','pierce_delay','cut_height','cut_feed_rate',
    'cut_amps','cut_volts','puddle_jump_height','puddle_jump_delay',
    'pause_at_end','gas_pressure','cut_mode','save_material','reload_material',
    'new_material','delete_material']

    parameters_gui_widgets = [
        'color_foregrnd','color_foregalt','color_led','color_backgrnd',
        'color_backgalt','color_frams','color_estop','color_disabled',
        'color_preview','chk_soft_keyboard','chk_keyboard_shortcuts',
        'chk_overlay','opt_stp','chk_run_from_line','chk_override_limits',
        'chk_tool_tips','opt_blk','grid_size','cone_size']

    parameters_utilities_widgets = [
        'actionbutton_halshow','actionbutton_halscope','actionbutton_halmeter',
        'actionbutton_calibration','actionbutton_lcnc_status_2','backup']

    statistics_widgets = [
        'pierce_reset','cut_length_reset','cut_time_reset','torch_time_reset',
        'run_time_reset','rapid_time_reset','probe_time_reset','all_reset',
        'pierce_count','cut_length','cut_time','torch_time','run_time',
        'rapid_time','probe_time','pierce_count_t','cut_length_t','cut_time_t',
        'torch_time_t','run_time_t','rapid_time_t','probe_time_t',
        'pmx_arc_time_t']

    W.statusbar.setToolTip('')
    for widget in main_preview_widgets:
        W[widget].setToolTip('')
    for widget in main_machine_widgets:
        W[widget].setToolTip('')
    for widget in main_button_widgets:
        W[widget].setToolTip('')
    for b in range(1,21):
        W['button_{}'.format(b)].setToolTip(_translate('ToolTips', ''))
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

def set_tool_tips(W):
    # main widgets
    W.statusbar.setToolTip(_translate('ToolTips', ''))

    # main_preview_widgets
    text1 = _translate('ToolTips', 'show currently loaded material')
    text2 = _translate('ToolTips', 'click to load new material')
    W.material_selector.setToolTip(_translate('ToolTips', '{}\n{}'.format(text1, text2)))
    W.velocity_show.setToolTip(_translate('ToolTips', 'current velocity in units per minute'))
    W.view_p.setToolTip(_translate('ToolTips', 'perspective view'))
    W.view_z.setToolTip(_translate('ToolTips', 'view from top'))
    W.view_clear.setToolTip(_translate('ToolTips', 'clear motion segments from preview'))
    W.pan_right.setToolTip(_translate('ToolTips', 'pan view right'))
    W.pan_left.setToolTip(_translate('ToolTips', 'pan view left'))
    W.pan_up.setToolTip(_translate('ToolTips', 'pan view up'))
    W.pan_down.setToolTip(_translate('ToolTips', 'pan view down'))
    W.zoom_in.setToolTip(_translate('ToolTips', 'zoom in'))
    W.zoom_out.setToolTip(_translate('ToolTips', 'zoom out'))

    # main_machine_widgets
    W.estop.setToolTip(_translate('ToolTips', 'Estop status'))
    W.power.setToolTip(_translate('ToolTips', 'Switch the GUI on or off'))
    W.run.setToolTip(_translate('ToolTips', 'Run the currently loaded G-Code program'))
    W.pause.setToolTip(_translate('ToolTips', 'Pause or resume the currently running G-Code program'))
    W.abort.setToolTip(_translate('ToolTips', 'Stop the currently running G-Code program'))
    W.feed_slider.setToolTip(_translate('ToolTips', 'Set the feed override percentage'))
    W.rapid_slider.setToolTip(_translate('ToolTips', 'Set the rapid override percentage'))
    W.jog_slider.setToolTip(_translate('ToolTips', 'Set the jog feed rate'))
    W.feed_label.setToolTip(_translate('ToolTips', ''))
    W.rapid_label.setToolTip(_translate('ToolTips', ''))
    W.jogs_label.setToolTip(_translate('ToolTips', ''))

    # main_button_widgets
    W.file_edit.setToolTip(_translate('ToolTips', ''))
    W.mdi_show.setToolTip(_translate('ToolTips', ''))
    for b in range(1,21):
        W['button_{}'.format(b)].setToolTip(_translate('ToolTips', 'User button #{} configured in ini file'.format(b)))

    # main_arc_widgets
    W.arc_voltage.setToolTip(_translate('ToolTips', 'Current arc voltage'))
    W.led_arc_ok.setToolTip(_translate('ToolTips', 'Arc OK signal'))
    W.height_reset.setToolTip(_translate('ToolTips', ''))
    W.height_lower.setToolTip(_translate('ToolTips', 'Override THC height down'))
    W.height_ovr_label.setToolTip(_translate('ToolTips', ''))
    W.height_raise.setToolTip(_translate('ToolTips', 'Override THC height up'))

    # main_control_widgets
    W.led_torch_on.setToolTip(_translate('ToolTips', 'Torch on status'))
    W.led_corner_lock.setToolTip(_translate('ToolTips', 'Velocity anti dive is active'))
    W.led_kerf_lock.setToolTip(_translate('ToolTips', 'Void anti dive is active'))
    W.mesh_enable.setToolTip(_translate('ToolTips', 'Enable mesh mode cutting'))
    W.ignore_arc_ok.setToolTip(_translate('ToolTips', 'Ignore Arc OK signal'))
    W.pmx485_label.setToolTip(_translate('ToolTips', 'Status of PMX485 communications'))
    W.torch_enable.setToolTip(_translate('ToolTips', 'Enable the torch'))
    W.cornerlock_enable.setToolTip(_translate('ToolTips', 'Enable velocity anti dive'))
    W.kerfcross_enable.setToolTip(_translate('ToolTips', 'Enable void anti dive'))
    W.use_auto_volts.setToolTip(_translate('ToolTips', 'Use automatic voltage for THC'))
    W.ohmic_probe_enable.setToolTip(_translate('ToolTips', 'Enable ohmic probing'))
    W.pmx485_enable.setToolTip(_translate('ToolTips', 'Enable Powermax communications'))

    # main_sensor_widgets
    W.led_float_switch.setToolTip(_translate('ToolTips', 'Float switch status'))
    W.led_ohmic_probe.setToolTip(_translate('ToolTips', 'Ohmic probe status'))
    W.led_breakaway_switch.setToolTip(_translate('ToolTips', 'Breakaway switch status'))

    # main_thc_widgets
    W.thc_enable.setToolTip(_translate('ToolTips', ''))
    W.led_thc_up.setToolTip(_translate('ToolTips', 'THC is moving the Z axis up'))
    W.led_thc_enabled.setToolTip(_translate('ToolTips', 'THC is enabled'))
    W.led_thc_down.setToolTip(_translate('ToolTips', 'THC is moving the Z axis down'))
    W.led_thc_active.setToolTip(_translate('ToolTips', 'THC is currently active'))

    # main_jog_widgets():
    W.jogincrements.setToolTip(_translate('ToolTips', 'Set the jog increment'))
    W.jog_slow.setToolTip(_translate('ToolTips', 'Toggle jog feed rate by a factor of 10'))
    W.jog_a_minus.setToolTip(_translate('ToolTips', 'Jog the A axis counter clockwise'))
    W.jog_a_plus.setToolTip(_translate('ToolTips', 'Jog the A axis clockwise'))
    W.jog_b_minus.setToolTip(_translate('ToolTips', 'Jog the B axis counter clockwise'))
    W.jog_b_plus.setToolTip(_translate('ToolTips', 'Jog the B axis clockwise'))
    W.jog_x_minus.setToolTip(_translate('ToolTips', 'Jog the X axis left'))
    W.jog_x_plus.setToolTip(_translate('ToolTips', 'Jog the X axis right'))
    W.jog_y_minus.setToolTip(_translate('ToolTips', 'Jog the Y axis forward'))
    W.jog_y_plus.setToolTip(_translate('ToolTips', 'Jog the Y axis back'))
    W.jog_z_minus.setToolTip(_translate('ToolTips', 'Jog the Z axis down'))
    W.jog_z_plus.setToolTip(_translate('ToolTips', 'Jog the Z axis up'))

    # main_cut_rec_widgets
    W.cut_rec_cancel.setToolTip(_translate('ToolTips', 'Cancel cut recovery'))
    W.cut_rec_fwd.setToolTip(_translate('ToolTips', 'Forward move along current motion segment'))
    W.cut_rec_rev.setToolTip(_translate('ToolTips', 'Reverse move along current motion segment'))
    W.cut_rec_speed.setToolTip(_translate('ToolTips', 'Change the paused motion feed rate override percentage'))
    W.cut_rec_feed.setToolTip(_translate('ToolTips', 'Paused motion feed rate override percentage'))
    W.cut_rec_move_label.setToolTip(_translate('ToolTips', 'Distance the torch will move'))
    text0 = _translate('ToolTips', 'Move in the')
    text1 = _translate('ToolTips', 'direction')
    W.cut_rec_e.setToolTip('{} X+ {}'.format(text0, text1))
    W.cut_rec_n.setToolTip('{} Y+ {}'.format(text0, text1))
    W.cut_rec_ne.setToolTip('{} X+ Y+ {}'.format(text0, text1))
    W.cut_rec_nw.setToolTip('{} X- Y+ {}'.format(text0, text1))
    W.cut_rec_s.setToolTip('{} Y- {}'.format(text0, text1))
    W.cut_rec_se.setToolTip('{} X+ Y- {}'.format(text0, text1))
    W.cut_rec_sw.setToolTip('{} X- Y- {}'.format(text0, text1))
    W.cut_rec_w.setToolTip('{} X- {}'.format(text0, text1))

    # main gcode widgets
    W.file_open.setToolTip(_translate('ToolTips', 'Open the file selector'))
    W.file_reload.setToolTip(_translate('ToolTips', 'Reload the current G-Code file'))
    W.gcode_progress.setToolTip(_translate('ToolTips', ''))

    # main dro widgets
    text0 = _translate('ToolTips', 'Current')
    text1 = _translate('ToolTips', 'axis position')
    W.dro_a.setToolTip('{} A {}'.format(text0, text1))
    W.dro_b.setToolTip('{} B {}'.format(text0, text1))
    W.dro_x.setToolTip('{} X {}'.format(text0, text1))
    W.dro_y.setToolTip('{} Y {}'.format(text0, text1))
    W.dro_z.setToolTip('{} Z {}'.format(text0, text1))
    text0 = _translate('ToolTips', 'Home the')
    text1 = _translate('ToolTips', 'axis')
    W.home_a.setToolTip('{} A {}'.format(text0, text1))
    W.home_b.setToolTip('{} B {}'.format(text0, text1))
    W.home_x.setToolTip('{} X {}'.format(text0, text1))
    W.home_y.setToolTip('{} Y {}'.format(text0, text1))
    W.home_z.setToolTip('{} Z {}'.format(text0, text1))
    W.home_all.setToolTip(_translate('ToolTips', 'Home all axes'))
    text0 = _translate('ToolTips', 'Touch off the')
    text1 = _translate('ToolTips', 'axis')
    W.touch_a.setToolTip('{} A {}'.format(text0, text1))
    W.touch_b.setToolTip('{} B {}'.format(text0, text1))
    W.touch_x.setToolTip('{} X {}'.format(text0, text1))
    W.touch_y.setToolTip('{} Y {}'.format(text0, text1))
    W.touch_z.setToolTip('{} Z {}'.format(text0, text1))
    text1 = _translate('ToolTips', 'axes to zero')
    W.touch_xy.setToolTip('{} X & Y {}'.format(text0, text1))
    W.wcs_button.setToolTip(_translate('ToolTips', 'Select work coordinate system'))
    W.camera.setToolTip(_translate('ToolTips', ''))
    W.laser.setToolTip(_translate('ToolTips', ''))

    # main file widgets
    W.file_select.setToolTip(_translate('ToolTips', 'Open the selected G-Code file'))
    W.file_next.setToolTip(_translate('ToolTips', 'Move down'))
    W.file_prev.setToolTip(_translate('ToolTips', 'Move up'))
    W.file_cancel.setToolTip(_translate('ToolTips', 'Close the file selector'))

    # main camera widgets
    W.cam_dia_plus.setToolTip(_translate('ToolTips', 'Increase the diameter of the overlay circle'))
    W.cam_dia_minus.setToolTip(_translate('ToolTips', ''))
    W.cam_zoom_plus.setToolTip(_translate('ToolTips', ''))
    W.cam_zoom_minus.setToolTip(_translate('ToolTips', ''))
    W.cam_goto.setToolTip(_translate('ToolTips', ''))
    W.cam_mark.setToolTip(_translate('ToolTips', ''))

    # conversational widgets
    W.conv_line.setToolTip(_translate('ToolTips', 'Create a line or arc'))
    W.conv_circle.setToolTip(_translate('ToolTips', ''))
    W.conv_ellipse.setToolTip(_translate('ToolTips', ''))
    W.conv_triangle.setToolTip(_translate('ToolTips', ''))
    W.conv_rectangle.setToolTip(_translate('ToolTips', ''))
    W.conv_polygon.setToolTip(_translate('ToolTips', ''))
    W.conv_bolt.setToolTip(_translate('ToolTips', ''))
    W.conv_slot.setToolTip(_translate('ToolTips', ''))
    W.conv_star.setToolTip(_translate('ToolTips', ''))
    W.conv_gusset.setToolTip(_translate('ToolTips', ''))
    W.conv_sector.setToolTip(_translate('ToolTips', ''))
    W.conv_rotate.setToolTip(_translate('ToolTips', ''))
    W.conv_scale.setToolTip(_translate('ToolTips', ''))
    W.conv_array.setToolTip(_translate('ToolTips', ''))
    W.conv_new.setToolTip(_translate('ToolTips', ''))
    W.conv_save.setToolTip(_translate('ToolTips', ''))
    W.conv_settings.setToolTip(_translate('ToolTips', ''))
    W.conv_send.setToolTip(_translate('ToolTips', ''))

    # parameters configuration widgets
    W.arc_fail_delay.setToolTip(_translate('ToolTips', 'The arc fail delay time in seconds'))
    W.arc_max_starts.setToolTip(_translate('ToolTips', ''))
    W.arc_restart_delay.setToolTip(_translate('ToolTips', ''))
    W.arc_voltage_scale.setToolTip(_translate('ToolTips', ''))
    W.arc_voltage_offset.setToolTip(_translate('ToolTips', ''))
    W.height_per_volt.setToolTip(_translate('ToolTips', ''))
    W.arc_ok_high.setToolTip(_translate('ToolTips', ''))
    W.arc_ok_low.setToolTip(_translate('ToolTips', ''))
    W.thc_delay.setToolTip(_translate('ToolTips', ''))
    W.thc_threshold.setToolTip(_translate('ToolTips', ''))
    W.pid_p_gain.setToolTip(_translate('ToolTips', ''))
    W.cornerlock_threshold.setToolTip(_translate('ToolTips', ''))
    W.kerfcross_override.setToolTip(_translate('ToolTips', ''))
    W.pid_i_gain.setToolTip(_translate('ToolTips', ''))
    W.pid_d_gain.setToolTip(_translate('ToolTips', ''))
    W.float_switch_travel.setToolTip(_translate('ToolTips', ''))
    W.probe_feed_rate.setToolTip(_translate('ToolTips', ''))
    W.probe_start_height.setToolTip(_translate('ToolTips', ''))
    W.ohmic_probe_offset.setToolTip(_translate('ToolTips', ''))
    W.ohmic_max_attempts.setToolTip(_translate('ToolTips', ''))
    W.skip_ihs_distance.setToolTip(_translate('ToolTips', ''))
    W.safe_height.setToolTip(_translate('ToolTips', ''))
    W.scribe_arm_delay.setToolTip(_translate('ToolTips', ''))
    W.scribe_on_delay.setToolTip(_translate('ToolTips', ''))
    W.spotting_threshold.setToolTip(_translate('ToolTips', ''))
    W.spotting_time.setToolTip(_translate('ToolTips', ''))
    W.max_offset_velocity_in.setToolTip(_translate('ToolTips', ''))
    W.setup_feed_rate.setToolTip(_translate('ToolTips', ''))
    W.save_plasma.setToolTip(_translate('ToolTips', ''))
    W.reload_plasma.setToolTip(_translate('ToolTips', ''))

    # parameters material widgets
    W.kerf_width.setToolTip(_translate('ToolTips', ''))
    W.pierce_height.setToolTip(_translate('ToolTips', ''))
    W.pierce_delay.setToolTip(_translate('ToolTips', ''))
    W.cut_height.setToolTip(_translate('ToolTips', ''))
    W.cut_feed_rate.setToolTip(_translate('ToolTips', ''))
    W.cut_amps.setToolTip(_translate('ToolTips', 'cutting current in amps, indicator only, not used by PlasmaC'))
    W.cut_volts.setToolTip(_translate('ToolTips', ''))
    W.puddle_jump_height.setToolTip(_translate('ToolTips', ''))
    W.puddle_jump_delay.setToolTip(_translate('ToolTips', ''))
    W.pause_at_end.setToolTip(_translate('ToolTips', ''))
    W.gas_pressure.setToolTip(_translate('ToolTips', ''))
    W.cut_mode.setToolTip(_translate('ToolTips', ''))
    W.save_material.setToolTip(_translate('ToolTips', ''))
    W.reload_material.setToolTip(_translate('ToolTips', ''))
    W.new_material.setToolTip(_translate('ToolTips', ''))
    W.delete_material.setToolTip(_translate('ToolTips', ''))

    # parameters gui widgets
    W.color_foregrnd.setToolTip(_translate('ToolTips', 'Show and change the current foreground color'))
    W.color_foregalt.setToolTip(_translate('ToolTips', ''))
    W.color_led.setToolTip(_translate('ToolTips', ''))
    W.color_backgrnd.setToolTip(_translate('ToolTips', ''))
    W.color_backgalt.setToolTip(_translate('ToolTips', ''))
    W.color_frams.setToolTip(_translate('ToolTips', ''))
    W.color_estop.setToolTip(_translate('ToolTips', 'Show and change the current estop color'))
    W.color_disabled.setToolTip(_translate('ToolTips', ''))
    W.color_preview.setToolTip(_translate('ToolTips', ''))
    W.chk_soft_keyboard.setToolTip(_translate('ToolTips', ''))
    W.chk_keyboard_shortcuts.setToolTip(_translate('ToolTips', ''))
    W.chk_overlay.setToolTip(_translate('ToolTips', ''))
    W.opt_stp.setToolTip(_translate('ToolTips', ''))
    W.chk_run_from_line.setToolTip(_translate('ToolTips', ''))
    W.chk_override_limits.setToolTip(_translate('ToolTips', ''))
    W.chk_tool_tips.setToolTip(_translate('ToolTips', 'Toggle displaying of tooltips'))
    W.opt_blk.setToolTip(_translate('ToolTips', ''))
    W.grid_size.setToolTip(_translate('ToolTips', ''))
    W.cone_size.setToolTip(_translate('ToolTips', 'Change the size of the cone in the preview screen'))

    # parameters utilities widgets
    W.actionbutton_halshow.setToolTip(_translate('ToolTips', 'Load the HalShow application'))
    W.actionbutton_halscope.setToolTip(_translate('ToolTips', ''))
    W.actionbutton_halmeter.setToolTip(_translate('ToolTips', ''))
    W.actionbutton_calibration.setToolTip(_translate('ToolTips', ''))
    W.actionbutton_lcnc_status_2.setToolTip(_translate('ToolTips', ''))
    W.backup.setToolTip(_translate('ToolTips', ''))

    # statisticswidgets
    W.pierce_reset.setToolTip(_translate('ToolTips', 'Rest the pierce counter'))
    W.cut_length_reset.setToolTip(_translate('ToolTips', ''))
    W.cut_time_reset.setToolTip(_translate('ToolTips', ''))
    W.torch_time_reset.setToolTip(_translate('ToolTips', ''))
    W.run_time_reset.setToolTip(_translate('ToolTips', ''))
    W.rapid_time_reset.setToolTip(_translate('ToolTips', ''))
    W.probe_time_reset.setToolTip(_translate('ToolTips', ''))
    W.all_reset.setToolTip(_translate('ToolTips', ''))
    W.pierce_count.setToolTip(_translate('ToolTips', 'The pierce count for the last job'))
    W.cut_length.setToolTip(_translate('ToolTips', ''))
    W.cut_time.setToolTip(_translate('ToolTips', ''))
    W.torch_time.setToolTip(_translate('ToolTips', ''))
    W.run_time.setToolTip(_translate('ToolTips', ''))
    W.rapid_time.setToolTip(_translate('ToolTips', ''))
    W.probe_time.setToolTip(_translate('ToolTips', ''))
    W.pierce_count_t.setToolTip(_translate('ToolTips', 'The total pierce count since the last reset'))
    W.cut_length_t.setToolTip(_translate('ToolTips', ''))
    W.cut_time_t.setToolTip(_translate('ToolTips', ''))
    W.torch_time_t.setToolTip(_translate('ToolTips', ''))
    W.run_time_t.setToolTip(_translate('ToolTips', ''))
    W.rapid_time_t.setToolTip(_translate('ToolTips', ''))
    W.probe_time_t.setToolTip(_translate('ToolTips', ''))
    W.pmx_arc_time_t.setToolTip(_translate('ToolTips', ''))
