# 12extrajoints.tcl (requires [HAL]TWOPASS

set min_joint_num  4 ;# first extra joint
set max_joint_num 15 ;# last  extra joint

for {set j $min_joint_num} {$j <= $max_joint_num} {incr j} {
  # use limit3 for motion planning:
  loadrt limit3 names=j${j}.limit3
  addf j${j}.limit3 servo-thread
  
  # These constraints apply to homing managed by
  # LinuxCNC and post-homing managed by the limit3 component:
  setp j${j}.limit3.min  [set ::JOINT_[set j](MIN_LIMIT)]
  setp j${j}.limit3.max  [set ::JOINT_[set j](MAX_LIMIT)]
  setp j${j}.limit3.maxv [set ::JOINT_[set j](MAX_VELOCITY)]
  setp j${j}.limit3.maxa [set ::JOINT_[set j](MAX_ACCELERATION)]

  net  J${j}:out    <= j${j}.limit3.out
  net  J${j}:out    => joint.${j}.posthome-cmd

  net  J${j}.enable <= joint.${j}.homed
  net  J${j}.enable => j${j}.limit3.enable
}
