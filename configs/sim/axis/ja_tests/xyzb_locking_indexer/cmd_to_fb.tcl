# accomodate arbitrary no. of consecutive joints (9 max)
# with short-circuit cmd_to_fb connections

eval loadrt [lindex $::KINS(KINEMATICS) end]

eval loadrt [lindex $::EMCMOT(EMCMOT) 0] \
     servo_period_nsec=$::EMCMOT(SERVO_PERIOD) num_joints=$::KINS(JOINTS)

addf motion-command-handler servo-thread
addf motion-controller servo-thread

if [catch {
   net J0 joint.0.motor-pos-cmd => joint.0.motor-pos-fb
   net J1 joint.1.motor-pos-cmd => joint.1.motor-pos-fb
   net J2 joint.2.motor-pos-cmd => joint.2.motor-pos-fb
   net J3 joint.3.motor-pos-cmd => joint.3.motor-pos-fb
   net J4 joint.4.motor-pos-cmd => joint.4.motor-pos-fb
   net J5 joint.5.motor-pos-cmd => joint.5.motor-pos-fb
   net J6 joint.6.motor-pos-cmd => joint.6.motor-pos-fb
   net J7 joint.7.motor-pos-cmd => joint.7.motor-pos-fb
   net J8 joint.8.motor-pos-cmd => joint.8.motor-pos-fb
   } msg] {
  #puts msg=$msg
}

net estop-loop       iocontrol.0.user-enable-out iocontrol.0.emc-enable-in

net tool-prep-loop   iocontrol.0.tool-prepare iocontrol.0.tool-prepared
net tool-change-loop iocontrol.0.tool-change  iocontrol.0.tool-changed
