# do not change the contents of this file as it will be overwiten by updates
# make custom changes in <machinename>_connections.hal or postgui.hal

#INI
net plasmac:axis-max-limit          ini.z.max_limit                     =>  plasmac.axis-z-max-limit
net plasmac:axis-min-limit          ini.z.min_limit                     =>  plasmac.axis-z-min-limit

#ARC PARAMETERS
net plasmac:arc-fail-delay          qtplasmac.arc_fail_delay-f          =>  plasmac.arc-fail-delay
net plasmac:arc-max-starts          qtplasmac.arc_max_starts-s          =>  plasmac.arc-max-starts
net plasmac:restart-delay           qtplasmac.arc_restart_delay-f       =>  plasmac.restart-delay
net plasmac:arc-voltage-scale       qtplasmac.arc_voltage_scale-f       =>  plasmac.arc-voltage-scale
net plasmac:arc-voltage-offset      qtplasmac.arc_voltage_offset-f      =>  plasmac.arc-voltage-offset
net plasmac:height-per-volt         qtplasmac.height_per_volt-f         =>  plasmac.height-per-volt
net plasmac:arc-ok-high             qtplasmac.arc_ok_high-f             =>  plasmac.arc-ok-high
net plasmac:arc-ok-low              qtplasmac.arc_ok_low-f              =>  plasmac.arc-ok-low

#THC PARAMETERS
net plasmac:thc-delay               qtplasmac.thc_delay-f               =>  plasmac.thc-delay
net plasmac:thc-threshold           qtplasmac.thc_threshold-f           =>  plasmac.thc-threshold
net plasmac:pid-p-gain              qtplasmac.pid_p_gain-f              =>  plasmac.pid-p-gain
net plasmac:cornerlock-threshold    qtplasmac.cornerlock_threshold-f    =>  plasmac.cornerlock-threshold
net plasmac:kerfcross-override      qtplasmac.kerfcross_override-f      =>  plasmac.kerfcross-override
net plasmac:pid-i-gain              qtplasmac.pid_i_gain-f              =>  plasmac.pid-i-gain
net plasmac:pid-d-gain              qtplasmac.pid_d_gain-f              =>  plasmac.pid-d-gain

#PROBE PARAMETERS
net plasmac:float-switch-travel     qtplasmac.float_switch_travel-f     =>  plasmac.float-switch-travel
net plasmac:probe-feed-rate         qtplasmac.probe_feed_rate-f         =>  plasmac.probe-feed-rate
net plasmac:probe-start-height      qtplasmac.probe_start_height-f      =>  plasmac.probe-start-height
net plasmac:ohmic-probe-offset      qtplasmac.ohmic_probe_offset-f      =>  plasmac.ohmic-probe-offset
net plasmac:ohmic-max-attempts      qtplasmac.ohmic_max_attempts-s      =>  plasmac.ohmic-max-attempts
net plasmac:skip-ihs-distance       qtplasmac.skip_ihs_distance-f       =>  plasmac.skip-ihs-distance

#SAFETY PARAMETERS
net plasmac:safe-height             qtplasmac.safe_height-f             =>  plasmac.safe-height

#SCRIBE PARAMETERS
net plasmac:scribe-arm-delay        qtplasmac.scribe_arm_delay-f        =>  plasmac.scribe-arm-delay
net plasmac:scribe-on-delay         qtplasmac.scribe_on_delay-f         =>  plasmac.scribe-on-delay

#SPOTTING PARAMETERS
net plasmac:spotting-threshold      qtplasmac.spotting_threshold-f      =>  plasmac.spotting-threshold
net plasmac:spotting-time           qtplasmac.spotting_time-f           =>  plasmac.spotting-time

#MOTION PARAMETERS
net plasmac:setup-feed-rate         qtplasmac.setup_feed_rate-f         =>  plasmac.setup-feed-rate

#MATERIAL PARAMETERS
net plasmac:cut-feed-rate           qtplasmac.cut_feed_rate-f           =>  plasmac.cut-feed-rate
net plasmac:cut-height              qtplasmac.cut_height-f              =>  plasmac.cut-height
net plasmac:cut-volts               qtplasmac.cut_volts-f               =>  plasmac.cut-volts
net plasmac:pause-at-end            qtplasmac.pause_at_end-f            =>  plasmac.pause-at-end
net plasmac:pierce-delay            qtplasmac.pierce_delay-f            =>  plasmac.pierce-delay
net plasmac:pierce-height           qtplasmac.pierce_height-f           =>  plasmac.pierce-height
net plasmac:puddle-jump-delay       qtplasmac.puddle_jump_delay-f       =>  plasmac.puddle-jump-delay
net plasmac:puddle-jump-height      qtplasmac.puddle_jump_height-f      =>  plasmac.puddle-jump-height

#MONITOR
net plasmac:arc_ok_out              plasmac.arc-ok-out                  =>  qtplasmac.led_arc_ok
net plasmac:arc_voltage_out         plasmac.arc-voltage-out             =>  qtplasmac.arc_voltage
net plasmac:breakaway-switch-out                                        =>  qtplasmac.led_breakaway_switch
net plasmac:cornerlock-is-locked    plasmac.cornerlock-is-locked        =>  qtplasmac.led_corner_lock
net plasmac:float-switch-out                                            =>  qtplasmac.led_float_switch
net plasmac:kerfcross-is-locked     plasmac.kerfcross-is-locked         =>  qtplasmac.led_kerf_lock
net plasmac:move-up                 plasmac.led-up                      =>  qtplasmac.led_thc_up
net plasmac:move-down               plasmac.led-down                    =>  qtplasmac.led_thc_down
net plasmac:ohmic-probe-out                                             =>  qtplasmac.led_ohmic_probe
net plasmac:thc-active              plasmac.thc-active                  =>  qtplasmac.led_thc_active
net plasmac:thc-enabled             plasmac.thc-enabled                 =>  qtplasmac.led_thc_enabled
net plasmac:torch-on                                                    =>  qtplasmac.led_torch_on

#CONTROL
net plasmac:cornerlock-enable       qtplasmac.cornerlock_enable         =>  plasmac.cornerlock-enable
net plasmac:kerfcross-enable        qtplasmac.kerfcross_enable          =>  plasmac.kerfcross-enable
net plasmac:mesh-enable             qtplasmac.mesh_enable               =>  plasmac.mesh-enable
net plasmac:ignore-arc-ok-1         qtplasmac.ignore_arc_ok             =>  plasmac.ignore-arc-ok-1
net plasmac:ohmic-probe-enable      qtplasmac.ohmic_probe_enable        =>  plasmac.ohmic-probe-enable
net plasmac:thc-enable              qtplasmac.thc_enable                =>  plasmac.thc-enable
net plasmac:use-auto-volts          qtplasmac.use_auto_volts            =>  plasmac.use-auto-volts
net plasmac:torch-enable            qtplasmac.torch_enable              =>  plasmac.torch-enable

#OFFSETS
net plasmac:x-offset-current                                            =>  qtplasmac.x_offset
net plasmac:y-offset-current                                            =>  qtplasmac.y_offset
