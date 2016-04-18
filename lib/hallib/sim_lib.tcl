# sim_lib.tcl: haltcl procs for sim configurations

#----------------------------------------------------------------------
# Notes (Joints-Axes):
#   1) if  ::KINS(KINEMATICS) exists:
#        loadrt the kins using any included parameters
#        example: for inifile item:
#           [KINS]KINEMATICS = trivkins coordinates=XZ kinstype=BOTH
#        use:
#           loadrt trivkins coordinates=xz kinstype=BOTH
#      else:
#           loadrt trivkins
#
#   2) NB: If $::KINS(KINEMATICS) specifies coordinates=, the
#          coordinates must agree with ::TRAJ(COORDINATES)
#
#   3) if known kins (trivkins) and xyz, make hypotenuse velocity
#      pins for xy,xyz
#----------------------------------------------------------------------

proc indices_for_trivkins {axes} {
  # ref: src/emc/kinematics/trivkins.c
  if {"$axes" == ""} {set axes {x y z a b c u v w}}
  set i 0
  foreach a [string tolower $axes] {
     # assign to consecutive joints:
     set ::SIM_LIB(jointidx,$a) $i
     incr i
  }
} ;# indices_for_trivkins

proc get_traj_coordinates {} {
  # initraj.cc: coordinates may be with or without spaces X Z or XZ
  # convert either form to list like {x z}
  set coordinates [lindex $::TRAJ(COORDINATES) 0]
  set coordinates [string map {" " "" "\t" ""} $coordinates]
  set coordinates [split $coordinates ""]
  return [string tolower $coordinates]
} ;# get_traj_coordinates

proc check_ini_items {} {
  foreach {section item} {KINS KINEMATICS
                          KINS JOINTS
                          TRAJ COORDINATES
                         } {
    if ![info exists ::${section}($item)] {
      return -code error "Missing inifile item: \[$section\]$item"
    }
  }

  if {   [info exists ::DISPLAY(LATHE)]
      && [lsearch $::KINS(KINEMATICS) trivkins] >= 0
     } {
     # reject historical lathe config using default trivkins coordinates (all)
     if {[lsearch $::KINS(KINEMATICS) =] < 0} {
       set msg "trivkins lathe config must specify coordinates= "
       set msg "$msg\n(typically use \[KINS]KINEMATICS trivkins coordinates=XZ)"
       return -code error "$msg"
     }
  }

  set kins [split [lindex $::KINS(KINEMATICS) 0]]
  if {[string first trivkins $kins] >= 0} {
    foreach item $kins {
      if {[string first coordinates= $item] < 0} continue
        set     tcoords [lindex [split $item =] end]
        set len_tcoords [string len $tcoords]
        if {$len_tcoords != $::KINS(JOINTS)} {
          set m "\ncheck_ini_items: WARNING:\n"
          set m "$m  trivkins coordinates=$tcoords specifies $len_tcoords joints\n"
          set m "$m  but \[KINS\]JOINTS is $::KINS(JOINTS)\n"
          puts stderr $m
        }
    } ;# foreach
  }
  return
} ;# check_ini_items

proc setup_kins {axes} {
  if ![info exists ::KINS(KINEMATICS)] {
    puts stderr "setup_kins: NO \[KINS\]KINEMATICS, trying default trivkins"
    loadrt trivkins
    return
  }
  set ::KINS(KINEMATICS) [lindex $::KINS(KINEMATICS) end]
  set cmd "loadrt $::KINS(KINEMATICS)" ;# may include parms

  set kins [lindex $::KINS(KINEMATICS) 0] ;# just the kins

  puts stderr "setup_kins: cmd=$cmd"
  if [catch {eval $cmd} msg] {
    puts stderr "msg=$msg"
    # if fail, try without coordinates parameters
    $cmd
  }

  # set up axis indices for known kins
  switch $kins {
    trivkins   {indices_for_trivkins $axes}
    default    {
      puts stderr "setup_kins: unknown \[KINS\]KINEMATICS=<$::KINS(KINEMATICS)>"
    }
  }
} ;# setup_kins

proc core_sim {axes
               number_of_joints
               servo_period
               {base_period 0}
               {emcmot motmod}
              } {
  # adapted as haltcl proc from core_sim.hal
  # note: with default emcmot==motmot,
  #       thread will not be added for (default) base_pariod == 0

  setup_kins $axes

  loadrt $emcmot \
         base_period_nsec=$base_period \
         servo_period_nsec=$servo_period \
         num_joints=$number_of_joints

  addf motion-command-handler servo-thread
  addf motion-controller      servo-thread

  set pid_names ""
  set mux_names ""
  for {set jno 0} {$jno < $number_of_joints} {incr jno} {
    set pid_names "${pid_names},J${jno}_pid"
    set mux_names "${mux_names},J${jno}_mux"
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

  for {set jno 0} {$jno < $number_of_joints} {incr jno} {
    net sample:enable => J${jno}_mux.sel

    net J${jno}:enable  <= joint.$jno.amp-enable-out
    net J${jno}:enable  => J${jno}_pid.enable

    net J${jno}:pos-cmd <= joint.$jno.motor-pos-cmd
    net J${jno}:pos-cmd => J${jno}_pid.command

    net J${jno}:on-pos  <= J${jno}_pid.output
    net J${jno}:on-pos  => J${jno}_mux.in1 ;# pass thru when motion-enabled

    net J${jno}:pos-fb  <= J${jno}_mux.out
    net J${jno}:pos-fb  => J${jno}_mux.in0 ;# hold position when !motion-enabled
    net J${jno}:pos-fb  => joint.$jno.motor-pos-fb
  }
} ;# core_sim

proc make_ddts {number_of_joints} {
  # make vel,accel ddts and signals for all joints
  # if xyz, make hypotenuse xy,xyz vels

  set ddt_limit 16 ;# limited by ddt component
  set ddt_names ""
  set ddt_ct 0
  for {set jno 0} {$jno < $number_of_joints} {incr jno} {
    incr ddt_ct 2
    if {$ddt_ct > $ddt_limit} {
      puts stderr "make_ddts: number of ddts limited to $ddt_limit"
      continue
    }
    set ddt_names "${ddt_names},J${jno}_vel,J${jno}_accel"
  }
  set ddt_names [string trimleft $ddt_names ,]
  loadrt ddt names=$ddt_names
  foreach cname [split $ddt_names ,] {
    addf $cname servo-thread
  }

  # joint vel,accel signal connections:
  set ddt_ct 0
  for {set jno 0} {$jno < $number_of_joints} {incr jno} {
    incr ddt_ct 2
    if {$ddt_ct > $ddt_limit} { continue }
    net J${jno}:pos-fb   => J${jno}_vel.in ;# net presumed to exist
    net J${jno}:vel      <= J${jno}_vel.out
    net J${jno}:vel      => J${jno}_accel.in
    net J${jno}:acc      <= J${jno}_accel.out
  }

  set has_xyz 1
  foreach letter {x y z} {
    if ![info exists ::SIM_LIB(jointidx,$letter)] {
      set has_xyz 0
      break
    }
  }
  if $has_xyz {
    loadrt hypot names=hyp_xy,hyp_xyz ;# vector velocities
    addf hyp_xy  servo-thread
    addf hyp_xyz servo-thread
    net J$::SIM_LIB(jointidx,x):vel <= J$::SIM_LIB(jointidx,x)_vel.out
    net J$::SIM_LIB(jointidx,x):vel => hyp_xy.in0
    net J$::SIM_LIB(jointidx,x):vel => hyp_xyz.in0

    net J$::SIM_LIB(jointidx,y):vel <= J$::SIM_LIB(jointidx,y)_vel.out
    net J$::SIM_LIB(jointidx,y):vel => hyp_xy.in1
    net J$::SIM_LIB(jointidx,y):vel => hyp_xyz.in1

    net J$::SIM_LIB(jointidx,z):vel <= J$::SIM_LIB(jointidx,z)_vel.out
    net J$::SIM_LIB(jointidx,z):vel => hyp_xyz.in2

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

proc simulated_home {number_of_joints} {
  # uses sim_home_switch component
  set switch_names ""
  for {set jno 0} {$jno < $number_of_joints} {incr jno} {
    set switch_names "${switch_names},J${jno}_switch"
  }
  set switch_names [string trimleft $switch_names ,]
  loadrt sim_home_switch names=$switch_names
  foreach cname [split $switch_names ,] {
    addf $cname servo-thread
  }

  for {set jno 0} {$jno < $number_of_joints} {incr jno} {
    # add pin to pre-existing signal:
    net J${jno}:pos-fb => J${jno}_switch.cur-pos

    net J${jno}:homesw <= J${jno}_switch.home-sw
    net J${jno}:homesw => joint.$jno.home-sw-in

    #setp J${jno}_switch.hysteresis ;# using component default
    #setp J${jno}_switch.home-pos   ;# using component default
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

