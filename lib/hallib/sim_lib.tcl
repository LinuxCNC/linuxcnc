# sim_lib.tcl: haltcl procs for sim configurations

#----------------------------------------------------------------------
# all globals are in one associative array ::SIM_LIB()
set letters {x y z a b c u v w}
foreach a {x y z a b c u v w} {
  set ::SIM_LIB($a,idx) [lsearch $letters $a]
}
#----------------------------------------------------------------------
proc core_sim {axes
               number_of_axes
               servo_period
               {base_period 0}
               {emcmot motmod}
              } {
  # adapted as haltcl proc from core_sim.hal
  # note: with default emcmot==motmot,
  #       thread will not be added for (default) base_pariod == 9
  loadrt trivkins
  set lcmd "loadrt $emcmot"
  set lcmd "$lcmd base_period_nsec=$base_period"
  set lcmd "$lcmd servo_period_nsec=$servo_period"
  set lcmd "$lcmd num_joints=$number_of_axes"

  eval $lcmd
  addf motion-command-handler servo-thread
  addf motion-controller      servo-thread

  set pid_names ""
  set mux_names ""
  foreach a $axes {
    set pid_names "${pid_names},${a}_pid"
    set mux_names "${mux_names},${a}_mux"
  }
  set pid_names [string trimleft $pid_names ,]
  set mux_names [string trimleft $mux_names ,]
  loadrt pid  names=$pid_names
  loadrt mux2 names=$mux_names

  # pid components
  # The pid comp is used as a pass-thru device (FF0=1,all other gains=0)
  # to emulate the connectivity of a servo system
  # (e.g., no short-circuit connection of motor-pos-cmd to motor-pos-fb)
  foreach cname [split $pid_names ,] {
    addf ${cname}.do-pid-calcs servo-thread
    # FF0 == 1 for pass-thru, all others 0
    setp ${cname}.FF0 1.0
    foreach pin {Pgain Dgain Igain FF1 FF2} { setp ${cname}.$pin 0 }
  }

  # mux components
  # The mux comp is used as a sample-hold to simulate a machine
  # with encoders that measure output position when power
  # is not applied to the motors or controllers
  foreach cname [split $mux_names ,] {
    addf $cname servo-thread
  }

  # signal connections:
  net estop:loop <= iocontrol.0.user-enable-out
  net estop:loop => iocontrol.0.emc-enable-in

  net tool:prep-loop <= iocontrol.0.tool-prepare
  net tool:prep-loop => iocontrol.0.tool-prepared

  net tool:change-loop <= iocontrol.0.tool-change
  net tool:change-loop => iocontrol.0.tool-changed

  net sample:enable <= motion.motion-enabled
  foreach a $axes {
    set idx $::SIM_LIB($a,idx)
    net sample:enable => ${a}_mux.sel

    net ${a}:enable  <= axis.$idx.amp-enable-out
    net ${a}:enable  => ${a}_pid.enable

    net ${a}:pos-cmd <= axis.$idx.motor-pos-cmd
    net ${a}:pos-cmd => ${a}_pid.command

    net ${a}:on-pos  <= ${a}_pid.output
    net ${a}:on-pos  => ${a}_mux.in1 ;# pass thru when motion-enabled

    net ${a}:pos-fb  <= ${a}_mux.out
    net ${a}:pos-fb  => ${a}_mux.in0 ;# hold position when !motion-enabled
    net ${a}:pos-fb  => axis.$idx.motor-pos-fb
  }
} ;# core_sim

proc make_ddts {axes} {
  # adapted as haltcl proc from core_sim.hal
  # any number of axes letters {x y z a b c u v w}
  # note: 2d velocity output only for xy  (xy:vel)
  # note: 3d velocity output only for xyz (xyz:vel)

  set ddt_names ""
  foreach a $axes {
    set ddt_names "${ddt_names},${a}_vel,${a}_accel"
  }
  set ddt_names [string trimleft $ddt_names ,]
  loadrt ddt names=$ddt_names
  foreach cname [split $ddt_names ,] {
    addf $cname servo-thread
  }
  loadrt hypot names=hyp_xy,hyp_xyz ;# vector velocities
  addf hyp_xy  servo-thread
  addf hyp_xyz servo-thread

  # signal connections:
  foreach a $axes {
    net ${a}:pos-fb   => ${a}_vel.in ;# net presumed to exist
    net ${a}:vel      <= ${a}_vel.out
    net ${a}:vel      => ${a}_accel.in
    net ${a}:acc      <= ${a}_accel.out

    switch $a {
      x {net ${a}:vel => hyp_xy.in0
         net ${a}:vel => hyp_xyz.in0
        }
      y {net ${a}:vel => hyp_xy.in1
         net ${a}:vel => hyp_xyz.in1
        }
      z {net ${a}:vel => hyp_xyz.in2
        }
    }
    net xy:vel   => hyp_xy.out
    net xyz:vel  <= hyp_xyz.out
  }
} ;# make_ddts

proc use_hal_manualtoolchange {} {
  # adapted as haltcl proc from axis_manualtoolchange.hal
  loadusr -W hal_manualtoolchange

  # disconnect if previously connected:
  unlinkp iocontrol.0.tool-change
  unlinkp iocontrol.0.tool-changed

  net tool:change <= iocontrol.0.tool-change
  net tool:change => hal_manualtoolchange.change

  net tool:changed <= hal_manualtoolchange.changed
  net tool:changed => iocontrol.0.tool-changed

  net tool:prep-number <= hal_manualtoolchange.number
  net tool:prep-number => iocontrol.0.tool-prep-number
} ;# use_hal_manualtoolchange

proc simulated_home {axes} {
  # uses sim_home_switch component
  set switch_names ""
  foreach a $axes {
    set switch_names "${switch_names},${a}_switch"
  }
  set switch_names [string trimleft $switch_names ,]
  loadrt sim_home_switch names=$switch_names
  foreach cname [split $switch_names ,] {
    addf $cname servo-thread
  }

  foreach a $axes {
    set idx $::SIM_LIB($a,idx)
    # add pin to pre-existing signal:
    net ${a}:pos-fb => ${a}_switch.cur-pos

    net ${a}:homesw <= ${a}_switch.home-sw
    net ${a}:homesw => axis.$idx.home-sw-in

    #setp ${a}_switch.hysteresis ;# using component default
    #setp ${a}_switch.home-pos   ;# using component default
  }
} ;# simulated_home

proc sim_spindle {} {
  # adapted as haltcl proc from sim_spindle_encoder.hal
  # simulated spindle encoder (for spindle-synced moves)
  loadrt sim_spindle names=sim_spindle
  setp   sim_spindle.scale 0.01666667

  loadrt limit2  names=limit_speed
  loadrt lowpass names=spindle_mass
  loadrt near    names=near_speed

  # this limit doesnt make any sense to me:
  setp limit_speed.maxv 5000.0 ;# rpm/second

  # encoder reset control
  # hook up motion controller's sync output
  net spindle-index-enable <=> motion.spindle-index-enable
  net spindle-index-enable <=> sim_spindle.index-enable

  # report our revolution count to the motion controller
  net spindle-pos <= sim_spindle.position-fb
  net spindle-pos => motion.spindle-revs

  # simulate spindle mass
  setp spindle_mass.gain .07

  # spindle speed control
  net spindle-speed-cmd <= motion.spindle-speed-out
  net spindle-speed-cmd => limit_speed.in
  net spindle-speed-cmd => near_speed.in1

  net spindle-speed-limited <= limit_speed.out
  net spindle-speed-limited => sim_spindle.velocity-cmd
  net spindle-speed-limited => spindle_mass.in

  # for spindle velocity estimate
  net spindle-rpm-filtered <= spindle_mass.out
  net spindle-rpm-filtered => motion.spindle-speed-in
  net spindle-rpm-filtered => near_speed.in2

  # at-speed detection
  setp near_speed.scale 1.1
  setp near_speed.difference 10

  net spindle-at-speed <= near_speed.out
  net spindle-at-speed => motion.spindle-at-speed

  addf limit_speed  servo-thread
  addf spindle_mass servo-thread
  addf near_speed   servo-thread
  addf sim_spindle  servo-thread
} ;# sim_spindle
