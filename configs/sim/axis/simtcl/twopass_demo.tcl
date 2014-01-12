# twopass_demo: demo using tcl configuration file for hal

# (derived from core_sim.hal,axis_manualtoolchange.hal,simlated_home.hal)
#    test use of both
#        "loadrt module names=" format  (ddt,or2,scale)
#    and
#        "loadrt module count=" format  (hypot,comp,flipflop)
#    test alias command
#    test loadrt usage in pass1 (second pass)
#    all addf calls are organized in one place
#    convention herein: signal names (net) begin with a capital letter

proc axis_manualtoolchange {} {
  loadusr -W hal_manualtoolchange

  unlinkp tchange     ;# in case already linked
  unlinkp tchanged    ;# in case already linked

  net Tool-change      hal_manualtoolchange.change  tchange
  net Tool-changed     hal_manualtoolchange.changed tchanged
  net Tool-prep-number hal_manualtoolchange.number  tprep_no
} ;# axis_manualtoolchange

proc simulated_home {axes} {
  foreach A $axes {
    loadrt comp names=${A}_comp
    net ${A}_homeswpos => ${A}_comp.in0
    switch $A {
      X {sets ${A}_homeswpos 1.0}
      Y {sets ${A}_homeswpos 0.5}
      Z {sets ${A}_homeswpos 2.0}
    }
    net  ${A}_pos => ${A}_comp.in1
    setp ${A}_comp.hyst .02
    net  ${A}_homesw <= ${A}_comp.out
  }

  net Y_homesw => yhome_sw_in

  loadrt or2  names=or2a
  net X_homesw => or2a.in0
  net Z_homesw => or2a.in1
  net XZ_homesw or2a.out => xhome_sw_in zhome_sw_in
} ;# simulated_home

proc make_ddts {axes} {
  loadrt hypot names=hyp_xy,hyp_xyz

  foreach A $axes {
    set al [string tolower $A]
    loadrt ddt names=${A}_vel,${A}_accel
    net ${A}_pos ${al}pos_cmd => ${al}pos_fb ${A}_vel.in

    net ${A}_vel ${A}_vel.out => ${A}_accel.in
    net ${A}_acc <= ${A}_accel.out
    switch $A {
      X {net ${A}_vel => hyp_xy.in0}
      Y {net ${A}_vel => hyp_xy.in1}
      Z {net ${A}_vel => hyp_xyz.in0
         net XY_vel   => hyp_xy.out => hyp_xyz.in1
         net XYZ_vel  <= hyp_xyz.out
        }
    }
  }
} ;# make_ddts

proc core_sim {} {
  loadrt trivkins

  loadrt $::EMCMOT(EMCMOT) \
         base_period_nsec=$::EMCMOT(BASE_PERIOD) \
         servo_period_nsec=$::EMCMOT(SERVO_PERIOD) \
         num_joints=$::TRAJ(AXES)

  alias pin iocontrol.0.tool-change      tchange
  alias pin iocontrol.0.tool-changed     tchanged
  alias pin iocontrol.0.tool-prepare     tprepare
  alias pin iocontrol.0.tool-prepared    tprepared
  alias pin iocontrol.0.user-enable-out  enable_out
  alias pin iocontrol.0.emc-enable-in    enable_in
  alias pin iocontrol.0.tool-prep-number tprep_no

  alias pin axis.0.motor-pos-cmd  xpos_cmd
  alias pin axis.0.motor-pos-fb   xpos_fb
  alias pin axis.1.motor-pos-cmd  ypos_cmd
  alias pin axis.1.motor-pos-fb   ypos_fb
  alias pin axis.2.motor-pos-cmd  zpos_cmd
  alias pin axis.2.motor-pos-fb   zpos_fb

  alias pin axis.0.home-sw-in xhome_sw_in
  alias pin axis.1.home-sw-in yhome_sw_in
  alias pin axis.2.home-sw-in zhome_sw_in

  net Estop-loop enable_out => enable_in

  net Tool-prep-loop tprepare tprepared
  net Tool-change-loop tchange tchanged
} ;# core_sim

# note: ini items ::SECTION(ITEM) are a list for each
#       line item (even one) so use eval set $::SECTION(ITEM)
eval set axes $::TRAJ(COORDINATES)
core_sim
make_ddts $axes
axis_manualtoolchange
simulated_home $axes

# addf calls are done in second pass, arrange sequence here:
foreach A $axes {
  addf ${A}_comp servo-thread
  addf ${A}_vel servo-thread
  addf ${A}_accel servo-thread
}
addf or2a servo-thread
addf hyp_xy  servo-thread
addf hyp_xyz servo-thread
addf motion-command-handler servo-thread
addf motion-controller servo-thread

#-----------------------------------------------------------------------
# below: tests
# test: use of loadrt count=...
loadrt not count=2
loadrt not count=3

# test: loading on second pass for components that don't exist should work
#       not sure why you would want to though
if [::tp::passnumber] {
  loadrt scale    names=scalea,scaleb
  loadrt flipflop count=1
}

# test: ::tp::no_puts
if [::tp::passnumber] {
  puts "[file tail $::argv0]:\
        The tcl \"puts\" command is enabled here in pass[::tp::passnumber]"
} else {
  ::tp::no_puts
    # this should not print:
    puts "[file tail $::argv0]:\
        The tcl \"puts\" command is disabled here in pass[::tp::passnumber]"
  ::tp::restore_puts
}
