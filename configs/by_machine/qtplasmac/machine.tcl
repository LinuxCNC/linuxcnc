
# COMPONENTS
loadrt $::KINS(KINEMATICS)
loadrt $::EMCMOT(EMCMOT) servo_period_nsec=$::EMCMOT(SERVO_PERIOD) num_joints=$::KINS(JOINTS) num_spindles=$::TRAJ(SPINDLES)
if {$::KINS(JOINTS) == 3} {
    loadrt pid names=sim:0_pid,sim:1_pid,sim:2_pid,sim:3_pid
    loadrt mux2 names=sim:0_mux,sim:1_mux,sim:2_mux,sim:3_mux
    loadrt ddt names=sim:0_vel,sim:0_accel,sim:1_vel,sim:1_accel,sim:2_vel,sim:2_accel,sim:3_vel,sim:3_accel
    loadrt hypot names=sim:hyp_xy,sim:hyp_xyz
    loadrt sim_home_switch names=sim:0_switch,sim:1_switch,sim:2_switch,sim:3_switch
} elseif {$::KINS(JOINTS) == 4} {
    loadrt pid names=sim:0_pid,sim:1_pid,sim:2_pid,sim:3_pid,sim:4_pid
    loadrt mux2 names=sim:0_mux,sim:1_mux,sim:2_mux,sim:3_mux,sim:4_mux
    loadrt ddt names=sim:0_vel,sim:0_accel,sim:1_vel,sim:1_accel,sim:2_vel,sim:2_accel,sim:3_vel,sim:3_accel,sim:4_vel,sim:4_accel
    loadrt hypot names=sim:hyp_xy,sim:hyp_xyz 
    loadrt sim_home_switch names=sim:0_switch,sim:1_switch,sim:2_switch,sim:3_switch,sim:4_switch
} elseif {$::KINS(JOINTS) == 5} {
    loadrt pid names=sim:0_pid,sim:1_pid,sim:2_pid,sim:3_pid,sim:4_pid,sim:5_pid
    loadrt mux2 names=sim:0_mux,sim:1_mux,sim:2_mux,sim:3_mux,sim:4_mux,sim:5_mux
    loadrt ddt names=sim:0_vel,sim:0_accel,sim:1_vel,sim:1_accel,sim:2_vel,sim:2_accel,sim:3_vel,sim:3_accel,sim:4_vel,sim:4_accel,sim:5_vel,sim:5_accel
    loadrt hypot names=sim:hyp_xy,sim:hyp_xyz 
    loadrt sim_home_switch names=sim:0_switch,sim:1_switch,sim:2_switch,sim:3_switch,sim:4_switch,sim:5_switch
}

# NETS
for {set jnum 0} {$jnum < $::KINS(JOINTS)} {incr jnum} {
    net sim:j${jnum}-acc sim:${jnum}_accel.out
    net sim:j${jnum}-enable joint.${jnum}.amp-enable-out => sim:${jnum}_pid.enable
    net sim:j${jnum}-homesw sim:${jnum}_switch.home-sw => joint.${jnum}.home-sw-in
    net sim:j${jnum}-on-pos sim:${jnum}_pid.output => sim:${jnum}_mux.in1
    net sim:j${jnum}-pos-cmd joint.${jnum}.motor-pos-cmd => sim:${jnum}_pid.command
    net sim:j${jnum}-pos-fb sim:${jnum}_mux.out => sim:${jnum}_mux.in0 sim:${jnum}_switch.cur-pos sim:${jnum}_vel.in joint.${jnum}.motor-pos-fb
    net sim:j${jnum}-vel sim:${jnum}_vel.out => sim:${jnum}_accel.in
}
net sim:xy-vel sim:hyp_xy.out
net sim:xyz-vel sim:hyp_xyz.out
net sim:enable motion.motion-enabled
for {set jnum 0} {$jnum < $::KINS(JOINTS)} {incr jnum} {
    net sim:enable sim:${jnum}_mux.sel
}
foreach {x y z } {0 0 0 } {}
for {set jnum 0} {$jnum < $::KINS(JOINTS)} {incr jnum} {
    if {[string index $::TRAJ(COORDINATES) $jnum] == "X"} {
        if {$x == 0} {
            incr x
            net sim:j${jnum}-vel => sim:hyp_xy.in0 sim:hyp_xyz.in0
        }
    } elseif {[string index $::TRAJ(COORDINATES) $jnum] == "Y"} {
        if {$y == 0} {
            incr y
            net sim:j${jnum}-vel => sim:hyp_xy.in1 sim:hyp_xyz.in1
        }
    } elseif {[string index $::TRAJ(COORDINATES) $jnum] == "Z"} {
        if {$z == 0} {
            incr z
            net sim:j${jnum}-vel => sim:hyp_xyz.in2
        }
    }
}

## REALTIME THREAD/FUNCTION LINKS
addf motion-command-handler servo-thread
addf motion-controller servo-thread
for {set jnum 0} {$jnum < $::KINS(JOINTS)} {incr jnum} {
    addf sim:${jnum}_pid.do-pid-calcs servo-thread
    addf sim:${jnum}_mux servo-thread
    addf sim:${jnum}_vel servo-thread
    addf sim:${jnum}_accel servo-thread
}
for {set jnum 0} {$jnum < $::KINS(JOINTS)} {incr jnum} {
    addf sim:${jnum}_switch servo-thread
}

## SETP COMMANDS FOR UNCONNECTED INPUT PINS
for {set jnum 0} {$jnum < $::KINS(JOINTS)} {incr jnum} {
    setp sim:${jnum}_pid.Pgain 0
    setp sim:${jnum}_pid.Dgain 0
    setp sim:${jnum}_pid.Igain 0
    setp sim:${jnum}_pid.FF0 1.0
    setp sim:${jnum}_pid.FF1 0
    setp sim:${jnum}_pid.FF2 0
}

# QTPLASMAC TOOLCHANGE PASSTHROUGH
net tool:change iocontrol.0.tool-change  => iocontrol.0.tool-changed
net tool:prep   iocontrol.0.tool-prepare => iocontrol.0.tool-prepared

# QTPLASMAC ESTOP HANDLING
loadrt or2 names=estop_or
loadrt not names=estop_not
addf estop_or servo-thread
addf estop_not servo-thread
net sim:estop-raw estop_or.out estop_not.in
net sim:estop-out estop_not.out iocontrol.0.emc-enable-in
if {[info exists ::QTPLASMAC(ESTOP_TYPE)]} {
    if {$::QTPLASMAC(ESTOP_TYPE) == 2} {
        loadrt not names=estop_not_1
        addf estop_not_1 servo-thread
        net sim:estop-1-raw iocontrol.0.user-enable-out => estop_not_1.in
        net sim:estop-1-in estop_not_1.out => estop_or.in1
    }
}
