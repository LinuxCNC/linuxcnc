#!/usr/bin/python3

class Connections():
    def __init__(self, parent, widget):
        self.w = widget
        self.parent = parent
        # buttons
        self.w.jog_xy.joy_btn_pressed.connect( self.parent.jog_xy_pressed)
        self.w.jog_xy.joy_btn_released.connect(self.parent.jog_xy_released)
        self.w.jog_az.joy_btn_pressed.connect(self.parent.jog_az_pressed)
        self.w.jog_az.joy_btn_released.connect(self.parent.jog_az_released)
        self.w.pgm_control.joy_btn_pressed.connect(self.parent.pgm_button_pressed)
        self.w.btn_save_log.clicked.connect(self.parent.btn_save_log_clicked)
        self.w.btn_clear_status.clicked.connect(self.parent.btn_clear_status_clicked)
        self.w.btn_home_all.clicked.connect(self.parent.btn_home_all_clicked)
        self.w.btn_ref_laser.clicked.connect(self.parent.btn_ref_laser_clicked)
        self.w.btn_ref_camera.clicked.connect(self.parent.btn_ref_camera_clicked)
        self.w.btn_goto_zero.clicked.connect(self.parent.btn_goto_location_clicked)
        self.w.btn_goto_zero_a.clicked.connect(self.parent.btn_goto_location_clicked)
        self.w.btn_goto_sensor.clicked.connect(self.parent.btn_goto_location_clicked)
        self.w.btn_go_home.clicked.connect(self.parent.btn_goto_location_clicked)
        self.w.btn_tool_sensor.clicked.connect(self.parent.btn_touchoff_pressed)
        self.w.btn_touchplate.clicked.connect(self.parent.btn_touchoff_pressed)
        self.w.btn_dimensions.clicked.connect(lambda state: self.parent.btn_dimensions_clicked(state))
        self.w.btn_alpha_mode.clicked.connect(lambda state: self.w.gcodegraphics.set_alpha_mode(state))
        self.w.btn_run_from_line.clicked.connect(self.parent.btn_run_from_line_clicked)
        self.w.btn_pause_spindle.clicked.connect(self.parent.btn_pause_spindle_clicked)
        self.w.btn_enable_comp.clicked.connect(self.parent.btn_enable_comp_clicked)
        self.w.btn_mdi_clear.pressed.connect(lambda: self.w.mdihistory.MDILine.clear())
        self.w.btn_mdi_enter.pressed.connect(self.parent.mdi_enter_pressed)
        self.w.btn_show_macros.clicked.connect(lambda state: self.parent.btn_show_macros_clicked(state))
        # tool table buttons
        self.w.btn_add_tool.pressed.connect(self.parent.btn_add_tool_pressed)
        self.w.btn_delete_tool.pressed.connect(self.parent.btn_delete_tool_pressed)
        self.w.btn_load_tool.pressed.connect(self.parent.btn_load_tool_pressed)
        self.w.btn_unload_tool.pressed.connect(self.parent.btn_unload_tool_pressed)
        self.w.btn_tool_db.clicked.connect(lambda state: self.parent.btn_tool_db_clicked(state))
        self.w.btn_db_help.pressed.connect(self.parent.show_db_help_page)
        # checkboxes
        self.w.chk_inhibit_selection.stateChanged.connect(lambda state: self.w.gcodegraphics.set_inhibit_selection(state))
        self.w.chk_use_mpg.stateChanged.connect(lambda state: self.parent.use_mpg_changed(state))
        self.w.chk_override_limits.stateChanged.connect(lambda state: self.parent.override_limits_changed(state))
        self.w.chk_use_camera.stateChanged.connect(lambda state: self.parent.use_camera_changed(state))
        self.w.chk_use_mdi_keyboard.stateChanged.connect(lambda state: self.parent.use_mdi_keyboard_changed(state))
        self.w.chk_edit_gcode.stateChanged.connect(lambda state: self.parent.edit_gcode_changed(state))
        self.w.chk_use_joypad.stateChanged.connect(lambda state: self.parent.use_joypad_changed(state))
        # adjustment bars
        self.w.adj_feed_ovr.valueChanged.connect(lambda value: self.parent.adj_feed_ovr_changed(value))
        self.w.adj_spindle_ovr.valueChanged.connect(lambda value: self.parent.adj_spindle_ovr_changed(value))
        self.w.adj_linear_jog.valueChanged.connect(lambda value: self.parent.adj_linear_changed(value))
        self.w.adj_angular_jog.valueChanged.connect(lambda value: self.parent.adj_angular_changed(value))
        # sliders
        self.w.cam_diameter.valueChanged.connect(lambda value: self.parent.cam_dia_changed(value))
        self.w.cam_rotate.valueChanged.connect(lambda value: self.parent.cam_rot_changed(value))
        # comboboxes
        self.w.cmb_gcode_history.activated.connect(self.parent.cmb_gcode_history_clicked)
        self.w.cmb_stylesheet.activated.connect(lambda index: self.parent.cmb_stylesheet_clicked(index))
        self.w.cmb_mdi_texts.activated.connect(self.parent.mdi_select_text)
        self.w.cmb_icon_select.activated.connect(self.parent.tool_db.icon_select_activated)
        # lineEdits
        self.w.lineEdit_probe_x.editingFinished.connect(self.parent.probe_offset_edited)
        self.w.lineEdit_probe_y.editingFinished.connect(self.parent.probe_offset_edited)
        # misc
        self.w.gcode_viewer.percentDone.connect(lambda percent: self.parent.percent_done_changed(percent))
        self.w.gcodegraphics.percentLoaded.connect(lambda percent: self.parent.percent_loaded_changed(percent))
        
