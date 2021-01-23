# BASIC PLASMAC SIM
loadrt $::KINS(KINEMATICS)
loadrt $::EMCMOT(EMCMOT) base_period_nsec=50000 servo_period_nsec=1000000 num_joints=$::KINS(JOINTS) num_spindles=$::TRAJ(SPINDLES)
addf motion-command-handler servo-thread
addf motion-controller servo-thread

# ESTOP
loadrt or2 names=estop_or
loadrt not names=estop_not
addf estop_or servo-thread
addf estop_not servo-thread
net sim:estop-raw estop_or.out estop_not.in
net sim:estop-out estop_not.out iocontrol.0.emc-enable-in
if {$::QTPLASMAC(ESTOP_TYPE) == 2} {
    loadrt not names=estop_not_1
    addf estop_not_1 servo-thread
    net sim:estop-1-raw iocontrol.0.user-enable-out => estop_not_1.in
    net sim:estop-1-in estop_not_1.out => estop_or.in1
}

# JOINT FEEDBACK
for {set jnum 0} {$jnum < $::KINS(JOINTS)} {incr jnum} {
    if {[hal list pin joint.${jnum}.motor-pos-cmd] == {}} {
        continue
    }
    net sim:joint-${jnum}-pos joint.${jnum}.motor-pos-cmd => joint.${jnum}.motor-pos-fb
}

# TOOL PASSTHROUGH
net sim:tool-change iocontrol.0.tool-change     => iocontrol.0.tool-changed
net sim:tool-prep   iocontrol.0.tool-prepare    => iocontrol.0.tool-prepared
