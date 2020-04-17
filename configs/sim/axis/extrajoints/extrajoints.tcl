# extrajoints.tcl (requires [HAL]TWOPASS
# create limit3 and signals for all extra joints

set script [file tail [info script]]
set min_joint_num  0 ;# first possible extra joint
set max_joint_num 15 ;# last  possible extra joint

for {set j $min_joint_num} {$j <= $max_joint_num} {incr j} {
  if [catch {getp joint.${j}.posthome-cmd} msg] {
     #puts "$::argv0: skip $j"
     continue
  } else {
     if [::tp::passnumber] {
       puts "$script: Create limit3 for extra joint $j"
     }
  }
  catch {
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
}
