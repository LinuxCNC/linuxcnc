# GENERIC HAL TCL FILE FOR PLASMAC SIM CONFIGS WITH STEPGENS

# ---SET CONSTANTS---
set numJoints $::KINS(JOINTS)
set zAxis [string first "z" [string tolower $::TRAJ(COORDINATES)]]

# ---LOAD COMPONENTS---
loadrt $::KINS(KINEMATICS)
loadrt $::EMCMOT(EMCMOT) base_period_nsec=$::EMCMOT(BASE_PERIOD) servo_period_nsec=$::EMCMOT(SERVO_PERIOD) num_joints=$numJoints num_spindles=$::TRAJ(SPINDLES)
loadrt  plasmac
set stepType "0"
for {set jNum 0} {$jNum < $numJoints} {incr jNum} {
    if {$jNum > 0} {
        append stepType "," "0"
    }
    loadrt sim_home_switch names=sim-joint.${jNum}.home-switch
}
loadrt stepgen step_type=$stepType

# ---LINK TO THREADS---
addf stepgen.make-pulses      base-thread
addf stepgen.capture-position servo-thread
addf motion-command-handler   servo-thread
addf motion-controller        servo-thread
addf plasmac                  servo-thread
addf stepgen.update-freq      servo-thread
for {set jNum 0} {$jNum < $numJoints} {incr jNum} {
    addf sim-joint.${jNum}.home-switch servo-thread
}

# ---JOINT CONNECTIONS---
for {set jNum 0} {$jNum < $numJoints} {incr jNum} {
    setp stepgen.${jNum}.maxaccel       [set ::JOINT_[set jNum](STEPGEN_MAXACCEL)]
    setp stepgen.${jNum}.position-scale [set ::JOINT_[set jNum](STEP_SCALE)]
    setp stepgen.${jNum}.steplen        [set ::JOINT_[set jNum](STEPLEN)]
    setp stepgen.${jNum}.stepspace      [set ::JOINT_[set jNum](STEPSPACE)]
    setp stepgen.${jNum}.dirsetup       [set ::JOINT_[set jNum](DIRSETUP)]
    setp stepgen.${jNum}.dirhold        [set ::JOINT_[set jNum](DIRHOLD)]
    net sim:joint${jNum}.enable  joint.${jNum}.amp-enable-out          => stepgen.${jNum}.enable
    net sim:joint${jNum}.pos-cmd joint.${jNum}.motor-pos-cmd           => stepgen.${jNum}.position-cmd
    net sim:joint${jNum}.pos-fb  stepgen.${jNum}.position-fb           => joint.${jNum}.motor-pos-fb sim-joint.${jNum}.home-switch.cur-pos
    net sim:joint${jNum}.homesw  sim-joint.${jNum}.home-switch.home-sw => joint.${jNum}.home-sw-in
}

# ---INPUT DEBOUNCE---
loadrt dbounce names=db_breakaway,db_float,db_ohmic,db_arc-ok
addf db_float     servo-thread
addf db_ohmic     servo-thread
addf db_breakaway servo-thread
addf db_arc-ok    servo-thread

# ---Z JOINT CONNECTION---
net plasmac:axis-position joint.${zAxis}.pos-fb => plasmac.axis-z-position

# ---TOOL CHANGE PASSTHROUGH
net sim:tool-number                                    <= iocontrol.0.tool-prep-number
net sim:tool-change-loopback  iocontrol.0.tool-change  => iocontrol.0.tool-changed
net sim:tool-prepare-loopback iocontrol.0.tool-prepare => iocontrol.0.tool-prepared

# ---ESTOP COMPONENTS FOR QTPLASMAC---
loadrt or2 names=estop_or
loadrt not names=estop_not,estop_not_1
addf estop_or    servo-thread
addf estop_not   servo-thread
addf estop_not_1 servo-thread
