# GENERIC HAL TCL FILE FOR PLASMAC SIM CONFIGS WITHOUT STEPGENS

# ---SET CONSTANTS---
set numJoints $::KINS(JOINTS)
set zAxis [string first "z" [string tolower $::TRAJ(COORDINATES)]]

# ---LOAD COMPONENTS---
loadrt $::KINS(KINEMATICS)
loadrt $::EMCMOT(EMCMOT) servo_period_nsec=$::EMCMOT(SERVO_PERIOD) num_joints=$numJoints num_spindles=$::TRAJ(SPINDLES)
loadrt  plasmac
for {set jNum 0} {$jNum < $numJoints} {incr jNum} {
    loadrt pid names=sim-joint.${jNum}.pid
    loadrt mux2 names=sim-joint.${jNum}.mux
    loadrt ddt names=sim-joint.${jNum}.vel
    loadrt sim_home_switch names=sim-joint.${jNum}.home-switch
}

# ---LINK TO THREADS---
addf motion-command-handler servo-thread
addf motion-controller servo-thread
addf plasmac servo-thread
for {set jNum 0} {$jNum < $numJoints} {incr jNum} {
    addf sim-joint.${jNum}.pid.do-pid-calcs servo-thread
    addf sim-joint.${jNum}.mux              servo-thread
    addf sim-joint.${jNum}.vel              servo-thread
}
for {set jNum 0} {$jNum < $numJoints} {incr jNum} {
    addf sim-joint.${jNum}.home-switch servo-thread
}

# ---SET UNCONNECTED INPUT PINS---
for {set jNum 0} {$jNum < $numJoints} {incr jNum} {
    setp sim-joint.${jNum}.pid.Pgain 0
    setp sim-joint.${jNum}.pid.Dgain 0
    setp sim-joint.${jNum}.pid.Igain 0
    setp sim-joint.${jNum}.pid.FF0   1.0
    setp sim-joint.${jNum}.pid.FF1   0
    setp sim-joint.${jNum}.pid.FF2   0
}

# ---JOINT CONNECTIONS---
for {set jNum 0} {$jNum < $numJoints} {incr jNum} {
    net sim:joint${jNum}.enable  joint.${jNum}.amp-enable-out          => sim-joint.${jNum}.pid.enable
    net sim:joint${jNum}.homesw  sim-joint.${jNum}.home-switch.home-sw => joint.${jNum}.home-sw-in
    net sim:joint${jNum}.on-pos  sim-joint.${jNum}.pid.output          => sim-joint.${jNum}.mux.in1
    net sim:joint${jNum}.pos-cmd joint.${jNum}.motor-pos-cmd           => sim-joint.${jNum}.pid.command
    net sim:joint${jNum}.pos-fb  sim-joint.${jNum}.mux.out             => sim-joint.${jNum}.mux.in0 sim-joint.${jNum}.home-switch.cur-pos sim-joint.${jNum}.vel.in joint.${jNum}.motor-pos-fb
}
net sim:enabled motion.motion-enabled
for {set jNum 0} {$jNum < $numJoints} {incr jNum} {
    net sim:enabled sim-joint.${jNum}.mux.sel
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
