'''
interlock_rules.py

Copyright (C) 2020-2026 Gregory D Carl

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

INTERLOCK_RULES = {
# MAIN tab
    'touch_xy': {
        'all': {
            'all_homed': True,
            'estop_cleared': True,
            'interp_idle': True,
            'laser_button_state': True,
            'manual_cut_active': False,
            'machine_on': True,
            'offsets_active': False,
            'ohmic_test': False,
            'previewstack_is_camera': False,
            'previewstack_is_edit': False,
            'previewstack_is_open': False,
            'previewstack_is_user_manual': False,
            'probe_test': False,
            'single_cut_dialog': False
        }
    },
    'wcs_button': {
        'all': {
            'all_homed': True,
            'estop_cleared': True,
            'interp_idle': True,
            'manual_cut_active': False,
            'machine_on': True,
            'offsets_active': False,
            'ohmic_test': False,
            'previewstack_is_edit': False,
            'previewstack_is_open': False,
            'previewstack_is_user_manual': False,
            'probe_test': False,
            'single_cut_dialog': False
        }
    },
    'camera': {
        'all': {
            'all_homed': True,
            'estop_cleared': True,
            'interp_idle': True,
            'laser_button_state': True,
            'manual_cut_active': False,
            'machine_on': True,
            'offsets_active': False,
            'ohmic_test': False,
            'probe_test': False,
            'previewstack_is_edit': False,
            'previewstack_is_offsets': False,
            'previewstack_is_open': False,
            'previewstack_is_user_manual': False,
            'single_cut_dialog': False
        }
    },
    'laser': {
        'all': {
            'all_homed': True,
            'consumable_changing': False,
            'estop_cleared': True,
            'manual_cut_active': False,
            'machine_on': True,
            'offsets_active': False,
            'ohmic_test': False,
            'previewstack_is_preview': True,
            'probe_test': False,
            'single_cut_dialog': False
        },
        'any': {
            'interp_idle': True,
            'interp_paused': True
        }
    },
    'mdi_show': {
        'all': {
            'all_homed': True,
            'estop_cleared': True,
            'interp_idle': True,
            'manual_cut_active': False,
            'machine_on': True,
            'ohmic_test': False,
            'previewstack_is_open': False,
            'single_cut_dialog': False
        }
    },
    'height_lower': {
        'all': {
            'all_homed': True,
            'estop_cleared': True,
            'manual_cut_active': False,
            'machine_on': True,
            'ohmic_test': False,
            'previewstack_is_edit': False,
            'previewstack_is_offsets': False,
            'previewstack_is_open': False,
            'previewstack_is_user_manual': False,
            'single_cut_dialog': False
        },
        'any': {'interp_idle': True,
               'interp_running': True
        }
    },
    'height_raise': {
        'all': {
            'all_homed': True,
            'estop_cleared': True,
            'manual_cut_active': False,
            'machine_on': True,
            'ohmic_test': False,
            'previewstack_is_edit': False,
            'previewstack_is_offsets': False,
            'previewstack_is_open': False,
            'previewstack_is_user_manual': False,
            'single_cut_dialog': False
        },
        'any': {'interp_idle': True,
               'interp_running': True
        }
    },
    'power': {
        'all': {
            'estop_cleared': True
        }
    },
    'run': {
        'all': {
            'all_homed': True,
            'consumable_changing': False,
            'file_bounds_error': False,
            'file_opened': True,
            'framing': False,
            'interp_idle': True,
            'machine_on': True,
            'plasmac_idle': True,
            'previewstack_is_preview': True,
            'probe_bounds_error': False,
            'probe_test': False,
            'offsets_active': False,
            'ohmic_test': False,
            'torch_pulse': False,
            'single_cut_dialog': False
        },
        'any': {
            'interp_running': False
        }
    },
    'pause_resume': {
        'all': {
            'consumable_changing': False,
            'torch_pulse': False
        },
        'any': {
            'interp_paused': True,
            'interp_running': True
        }
    },
    'abort': {
        'all': {
        },
        'any': {
            'framing': True,
            'interp_paused': True,
            'interp_running': True,
            'manual_cut_active': True,
            'probe_test': True,
            'torch_pulse': True
        }
    },
    'jog_frame': {
        'all': {
            'estop_cleared': True,
            'machine_on': True,
            'gcodestack_is_mdi': False,
            'interp_idle': True,
            'previewstack_is_edit': False,
            'previewstack_is_offsets': False,
            'previewstack_is_open': False,
            'previewstack_is_user_manual': False,
            'probe_test': False,
            'torch_pulse': False
        }
    },
    'jog_slider': {
        'all': {
            'estop_cleared': True,
            'machine_on': True,
            'interp_idle': True,
            'probe_test': False,
            'torch_pulse': False,
            'manual_cut_active': False
        }
    },
    'jogs_label': {
        'all': {
            'estop_cleared': True,
            'machine_on': True,
            'interp_idle': True,
            'probe_test': False,
            'torch_pulse': False,
            'manual_cut_active': False
        }
    },
    'jog_slow': {
        'all': {
            'estop_cleared': True,
            'machine_on': True,
            'interp_idle': True,
            'probe_test': False,
            'torch_pulse': False,
            'manual_cut_active': False
        }
    },
    'jogincrements': {
        'all': {
            'estop_cleared': True,
            'machine_on': True,
            'interp_idle': True,
            'probe_test': False,
            'torch_pulse': False,
            'manual_cut_active': False
        }
    },
    'material_label': {
        'all': {
            'interp_idle': True,
            'probe_test': False,
            'torch_pulse': False,
            'manual_cut_active': False
        }
    },
    'material_selector': {
        'all': {
            'interp_idle': True,
            'probe_test': False,
            'torch_pulse': False,
            'manual_cut_active': False
        }
    },
    'feed_label': {
        'all': {
            'estop_cleared': True
        }
    },
    'rapid_label': {
        'all': {
            'estop_cleared': True
        }
    },
    'cut_rec_cancel': {
        'all': {
            'consumable_changing': False,
            'interp_paused': True,
            'offsets_active': True
        },
        'any': {
        }
    },
# SETTINGS tab
    'ub_save': {
        'all': {
            'interp_running': False,
            'probe_test': False,
            'torch_pulse': False,
            'manual_cut_active': False
        }
    },
    'ub_reload': {
        'all': {
            'interp_running': False,
            'probe_test': False,
            'torch_pulse': False,
            'manual_cut_active': False
        }
    },
# user button templates
    'toggle-laser_template': {
        'all': {
            'all_homed': True,
            'estop_cleared': True,
            'interp_idle': True,
            'manual_cut_active': False,
            'machine_on': True,
            'ohmic_test': False,
            'previewstack_is_edit': False,
            'previewstack_is_offsets': False,
            'previewstack_is_open': False,
            'previewstack_is_user_manual': False,
            'probe_test': False,
            'single_cut_dialog': False,
            'torch_pulse': False
        }
    },
    'change-consumables_template': {
        'all': {
            'all_homed': True,
            'estop_cleared': True,
            'manual_cut_active': False,
            'machine_on': True,
            'offsets_active': False,
            'ohmic_test': False,
            'single_cut_dialog': False,
            'interp_paused': True
        },
        'override': {
            'consumable_change': True
        }
    },
    'probe-test_template': {
        'all': {
            'all_homed': True,
            'estop_cleared': True,
            'interp_idle': True,
            'manual_cut_active': False,
            'machine_on': True,
            'offsets_active': False,
            'ohmic_test': False,
            'previewstack_is_edit': False,
            'previewstack_is_offsets': False,
            'previewstack_is_open': False,
            'previewstack_is_user_manual': False,
            'single_cut_dialog': False
        },
        'override': {
            'probe_test': True
        }
    },
    'torch-pulse_template': {
        'all': {
            'consumable_changing': False,
            'estop_cleared': True,
            'manual_cut_active': False,
            'machine_on': True,
            'ohmic_test': False,
            'previewstack_is_edit': False,
            'previewstack_is_offsets': False,
            'previewstack_is_open': False,
            'previewstack_is_user_manual': False,
            'probe_test': False,
            'torch_enable': True,
            'single_cut_dialog': False
        },
        'any': {
            'interp_idle': True,
            'interp_paused': True
        }
    },
    'ohmic-test_template': {
        'all': {
            'consumable_changing': False,
            'estop_cleared': True,
            'manual_cut_active': False,
            'machine_on': True,
            'ohmic_probe_enable': True,
            'previewstack_is_edit': False,
            'previewstack_is_offsets': False,
            'previewstack_is_open': False,
            'previewstack_is_user_manual': False,
            'single_cut_dialog': False
        },
        'any': {
            'interp_idle': True,
            'interp_paused': True
        }
    },
    'framing_template': {
        'all': {
            'all_homed': True,
            'estop_cleared': True,
            'file_bounds_error': False,
            'gcode_loaded': True,
            'interp_idle': True,
            'manual_cut_active': False,
            'machine_on': True,
            'ohmic_test': False,
            'previewstack_is_edit': False,
            'previewstack_is_offsets': False,
            'previewstack_is_open': False,
            'previewstack_is_user_manual': False,
            'probe_bounds_error': False,
            'rfl_dialog': False,
            'single_cut_dialog': False
        }
    },
    'cut-type_template': {
        'all': {
            'estop_cleared': True,
            'interp_idle': True,
            'manual_cut_active': False,
            'machine_on': True,
            'ohmic_test': False,
            'previewstack_is_edit': False,
            'previewstack_is_offsets': False,
            'previewstack_is_open': False,
            'previewstack_is_user_manual': False,
            'single_cut_dialog': False
        }
    },
    'single-cut_template': {
        'all': {
            'all_homed': True,
            'estop_cleared': True,
            'file_bounds_error': False,
            'interp_idle': True,
            'manual_cut_active': False,
            'machine_on': True,
            'offsets_active': False,
            'ohmic_test': False,
            'previewstack_is_preview': True,
            'probe_test': False,
            'probe_bounds_error': False,
            'single_cut_dialog': False
        }
    },
    'manual-cut_template': {
        'all': {
            'all_homed': True,
            'estop_cleared': True,
            'interp_idle': True,
            'machine_on': True,
            'offsets_active': False,
            'ohmic_test': False,
            'previewstack_is_preview': True,
            'probe_test': False,
            'single_cut_dialog': False
        },
        'override': {
            'manual_cut_active': True
        }
    },
    'load_template': {
        'all': {
            'estop_cleared': True,
            'interp_idle': True,
            'manual_cut_active': False,
            'machine_on': True,
            'ohmic_test': False,
            'previewstack_is_edit': False,
            'previewstack_is_offsets': False,
            'previewstack_is_open': False,
            'previewstack_is_user_manual': False,
            'single_cut_dialog': False
        }
    },
    'toggle-halpin_template': {
        'all': {
            'machine_on': True
        }
    },
    'pulse-halpin_template': {
        'all': {
            'machine_on': True
        }
    },
    'offsets-view_template': {
        'all': {
            'interp_idle': True,
            'manual_cut_active': False,
            'single_cut_dialog': False
        }
    },
    'latest-file_template': {
        'all': {
            'interp_idle': True,
            'manual_cut_active': False,
            'single_cut_dialog': False
        }
    },
    'user-manual_template': {
        'all': {
            'interp_idle': True,
            'manual_cut_active': False,
            'single_cut_dialog': False
        }
    },
    'toggle-joint_template': {
        'all': {
            'all_homed': True,
            'estop_cleared': True,
            'interp_idle': True,
            'manual_cut_active': False,
            'machine_on': True,
            'ohmic_test': False,
            'previewstack_is_edit': False,
            'previewstack_is_offsets': False,
            'previewstack_is_open': False,
            'previewstack_is_user_manual': False,
            'single_cut_dialog': False
        }
    },
    'dual-code_template': {
        'all': {
            'all_homed': True,
            'estop_cleared': True,
            'interp_idle': True,
            'manual_cut_active': False,
            'machine_on': True,
            'ohmic_test': False,
            'previewstack_is_edit': False,
            'previewstack_is_offsets': False,
            'previewstack_is_open': False,
            'previewstack_is_user_manual': False,
            'probe_test': False,
            'single_cut_dialog': False,
            'torch_pulse': False
        }
    },
    'always-on_template': {
        'all': {
        }
    },
    'main_tab': {
        'all': {
            # main tab is always active
        }
    },
    'conversational_tab': {
        'all': {
            'interp_idle': True,
            'manual_cut_active': False,
            'offsets_active': False,
            'probe_test': False
        }
    },
    'parameters_tab': {
        'all': {
            'manual_cut_active': False,
            'offsets_active': False,
            'probe_test': False,
        },
        'any': {
            'interp_idle': True,
            'interp_paused': True
        }
    },
    'settings_tab': {
        'all': {
            'manual_cut_active': False,
            'offsets_active': False,
            'probe_test': False,
        },
        'any': {
            'interp_idle': True,
            'interp_paused': True
        }
    },
    'statistics_tab': {
        'all': {
            'manual_cut_active': False,
            'offsets_active': False,
            'probe_test': False,
        },
        'any': {
            'interp_idle': True,
            'interp_paused': True
        }
    },
}

# previous "idleList"
INTERLOCK_RULES.update({
    f'{item}': {'all': {
                    'interp_idle': True,
                    'manual_cut_active': False,
                    'ohmic_test': False,
                    'single_cut_dialog': False
                        }}
    for item in ['file_clear', 'file_open', 'file_reload', 'file_edit']})

# previous "idleOnList"
INTERLOCK_RULES.update({
    f'{item}': {'all': {
                    'estop_cleared': True,
                    'interp_idle': True,
                    'manual_cut_active': False,
                    'machine_on': True,
                    'offsets_active': False,
                    'ohmic_test': False,
                    'previewstack_is_edit': False,
                    'previewstack_is_offsets': False,
                    'previewstack_is_open': False,
                    'previewstack_is_user_manual': False,
                    'probe_test': False,
                    'single_cut_dialog': False
                        }}
    for item in ['home_x', 'home_y', 'home_z', 'home_a', 'home_b', 'home_c', 'home_all']})

# previous "idleHomedList"
INTERLOCK_RULES.update({
    f'{item}': {'all': {
                    'all_homed': True,
                    'estop_cleared': True,
                    'interp_idle': True,
                    'manual_cut_active': False,
                    'machine_on': True,
                    'ohmic_test': False,
                    'previewstack_is_edit': False,
                    'previewstack_is_open': False,
                    'previewstack_is_user_manual': False,
                    'probe_test': False,
                    'single_cut_dialog': False
                        }}
    for item in ['touch_x', 'touch_y', 'touch_z', 'touch_a', 'touch_b', 'touch_c', 'set_offsets']})

# jog buttons a,b,c,x,y - z gets special handling due to manual cut
INTERLOCK_RULES.update({
    f'{item}': {'all': {
                    'all_homed': True,
                    'interp_idle': True,
                    'machine_on': True,
                    'offsets_active': False
                        }}
    for item in ['jog_a_plus', 'jog_a_minus', 'jog_b_plus', 'jog_b_minus','jog_c_plus', 'jog_c_minus','jog_x_plus', 'jog_x_minus','jog_y_plus', 'jog_y_minus']})
INTERLOCK_RULES.update({
    f'{item}': {'all': {
                    'all_homed': True,
                    'interp_idle': True,
                    'machine_on': True,
                    'manual_cut_active': False,
                    'offsets_active': False
                        }}
    for item in ['jog_z_plus', 'jog_z_minus']})

# cut recovery controls
INTERLOCK_RULES.update({
    f'cut_rec_{item}': {'all': {
                        'consumable_changing': False,
                        'interp_paused': True
                        }}
    for item in ['fwd', 'rev', 'speed', 'n', 'ne', 'e', 'se', 's', 'sw', 'w', 'nw', 'feed', 'move_label']})

# PARAMETERS tab common
INTERLOCK_RULES.update({
    f'{item}': {'all': {
                    'interp_idle': True,
                    'probe_test': False,
                    'torch_pulse': False,
                    'manual_cut_active': False
                        }}
    for item in ['material_box', 'save_material', 'delete_material', 'new_material', 'reload_material', 'cut_amps', 'cut_feed_rate', 'cut_height', 'cut_mode', 'cut_mode_label', 'cut_volts', 'gas_pressure', 'material_thickness', 'kerf_width', 'pause_at_end', 'pierce_delay', 'pierce_height', 'puddle_jump_delay', 'puddle_jump_height']})

# SETTINGS tab user button fields
INTERLOCK_RULES.update({
    f'{item}_{n}': {'all': {
                        'interp_running': False,
                        'probe_test': False,
                        'torch_pulse': False,
                        'manual_cut_active': False
                            }}
    for item in ('ub_name', 'ub_code')
    for n in range(1, 21)})