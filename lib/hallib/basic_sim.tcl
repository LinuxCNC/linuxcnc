# Notes:
#  1) ::env() is a global associative array of environmental variables
#     as exported by the main LinuxCNC script (linuxcnc)
#  2) Settings from the ini file are available as global associative
#     arrays named: ::SECTION(varname)
#     example: ::EMCMOT(SERVO_PERIOD)
#  3) procs are from sim_lib.tcl

#begin-----------------------------------------------------------------
source [file join $::env(HALLIB_DIR) sim_lib.tcl]

set axes [get_traj_coordinates]
set number_of_joints $::KINS(JOINTS)

set base_period 0 ;# 0 means no thread
if [info exists ::EMCMOT(BASE_PERIOD)] {
  set base_period $::EMCMOT(BASE_PERIOD)
}

core_sim $axes \
         $number_of_joints \
         $::EMCMOT(SERVO_PERIOD) \
         $base_period

make_ddts $number_of_joints
simulated_home $number_of_joints
use_hal_manualtoolchange
sim_spindle
