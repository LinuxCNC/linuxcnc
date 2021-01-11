# basic plasmac sim
loadrt $::KINS(KINEMATICS)
loadrt $::EMCMOT(EMCMOT) base_period_nsec=50000 servo_period_nsec=1000000 num_joints=$::KINS(JOINTS) num_spindles=$::TRAJ(SPINDLES)

addf motion-command-handler servo-thread
addf motion-controller servo-thread

net sim:estop:loop  iocontrol.0.user-enable-out => iocontrol.0.emc-enable-in

for {set jnum 0} {$jnum < $::KINS(JOINTS)} {incr jnum} {
    if {[hal list pin joint.${jnum}.motor-pos-cmd] == {}} {
        continue
    }
    net sim:joint-${jnum}-pos joint.${jnum}.motor-pos-cmd => joint.${jnum}.motor-pos-fb
}

net sim:tool-change iocontrol.0.tool-change     => iocontrol.0.tool-changed
net sim:tool-prep   iocontrol.0.tool-prepare    => iocontrol.0.tool-prepared
