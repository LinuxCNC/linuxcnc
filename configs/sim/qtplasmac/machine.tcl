# GENERIC HAL FILE FOR QTPLASMAC SIM CONFIGS

# ---SET CONSTANTS---
set numJoints $::KINS(JOINTS)
set z-axis [string first "z" [string tolower $::TRAJ(COORDINATES)]]

# ---COMPONENTS---
loadrt $::KINS(KINEMATICS)
loadrt $::EMCMOT(EMCMOT) servo_period_nsec=$::EMCMOT(SERVO_PERIOD) num_joints=$numJoints num_spindles=$::TRAJ(SPINDLES)
loadrt  plasmac
for {set jnum 0} {$jnum < $numJoints} {incr jnum} {
    loadrt pid names=sim:${jnum}_pid
    loadrt mux2 names=sim:${jnum}_mux
    loadrt ddt names=sim:${jnum}_vel,sim:${jnum}_accel
    loadrt sim_home_switch names=sim:${jnum}_switch
}
loadrt hypot names=sim:hyp_xy,sim:hyp_xyz

# ---THREAD LINKS---
addf motion-command-handler servo-thread
addf motion-controller servo-thread
# don't add the plasmac component if it already exists
if {[hal list pin plasmac.mode] == {}} {
    addf plasmac servo-thread
}
for {set jnum 0} {$jnum < $numJoints} {incr jnum} {
    addf sim:${jnum}_pid.do-pid-calcs servo-thread
    addf sim:${jnum}_mux servo-thread
    addf sim:${jnum}_vel servo-thread
    addf sim:${jnum}_accel servo-thread
}
for {set jnum 0} {$jnum < $numJoints} {incr jnum} {
    addf sim:${jnum}_switch servo-thread
}

# ---SETP COMMANDS FOR UNCONNECTED INPUT PINS---
for {set jnum 0} {$jnum < $numJoints} {incr jnum} {
    setp sim:${jnum}_pid.Pgain 0
    setp sim:${jnum}_pid.Dgain 0
    setp sim:${jnum}_pid.Igain 0
    setp sim:${jnum}_pid.FF0 1.0
    setp sim:${jnum}_pid.FF1 0
    setp sim:${jnum}_pid.FF2 0
}

# ---MACHINE NET CONNECTIONS---
for {set jnum 0} {$jnum < $numJoints} {incr jnum} {
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
for {set jnum 0} {$jnum < $numJoints} {incr jnum} {
    net sim:enable sim:${jnum}_mux.sel
}
foreach {x y z } {0 0 0 } {}
for {set jnum 0} {$jnum < $numJoints} {incr jnum} {
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

# ---PLASMA INPUT DEBOUNCE---
loadrt dbounce names=db_breakaway,db_float,db_ohmic,db_arc-ok
addf db_float     servo-thread
addf db_ohmic     servo-thread
addf db_breakaway servo-thread
addf db_arc-ok    servo-thread

# ---Z AXIS FEEDBACK---
net plasmac:axis-position        joint.${z-axis}.pos-fb      =>  plasmac.axis-z-position

# ---POWERMAX RS485 COMPONENT---
#loadusr -Wn pmx485 pmx485 /dev/ttyUSB0

# ---TOOL CHANGE PASSTHROUGH
net tool-number <= iocontrol.0.tool-prep-number
net tool-change-loopback iocontrol.0.tool-change => iocontrol.0.tool-changed
net tool-prepare-loopback iocontrol.0.tool-prepare => iocontrol.0.tool-prepared

# ---QTPLASMAC ESTOP HANDLING---
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