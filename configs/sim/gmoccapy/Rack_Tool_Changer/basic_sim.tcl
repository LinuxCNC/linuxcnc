# basic_sim.tcl
#
# Provide common hal components and connections for a simulated machine.
# By default, the script makes and connects ddts, simulated_home,
# spindle, and hal_manualtoolchange components.
#
# Options are available to 
#   a) disable specific hal components and connections
#   b) save a halfile equivalent to the hal commands executed by
#      this script (and any prior executed hal commands)
#
# Coordinate letters and number_of_joints are determined from the usual
# ini # file settings.
#
# Ini file usage:
#                 [HAL]HALFILE = basic_sim.tcl [Options]
# Options:
#                 -no_make_ddts
#                 -no_simulated_home
#                 -no_use_hal_manualtoolchange
#                 -no_sim_spindle

#----------------------------------------------------------------------
# Notes:
#  1) ::env() is a global associative array of environmental variables
#     as exported by the main LinuxCNC script (linuxcnc)
#  2) Settings from the ini file are available as global associative
#     arrays named: ::SECTION(varname)
#     example: ::EMCMOT(SERVO_PERIOD)
#  3) procs are from sim_lib.tcl

#begin-----------------------------------------------------------------
source [file join $::env(HALLIB_DIR) sim_lib.tcl]
set save_options {}

if [catch {check_ini_items} msg] {
  puts "\nbasic_sim.tcl ERROR: $msg\n"
  exit 1
}
set axes [get_traj_coordinates]
set number_of_joints $::KINS(JOINTS)

set base_period 0 ;# 0 means no thread
if [info exists ::EMCMOT(BASE_PERIOD)] {
  set base_period $::EMCMOT(BASE_PERIOD)
}

core_sim $axes \
         $number_of_joints \
         $::EMCMOT(SERVO_PERIOD) \
         $base_period \
         $::EMCMOT(EMCMOT)

if {[lsearch -nocase $::argv -no_make_ddts] < 0} {
  make_ddts $number_of_joints
}
if {[lsearch -nocase $::argv -no_simulated_home] < 0} {
  simulated_home $number_of_joints
}
if {[lsearch -nocase $::argv -no_use_hal_manualtoolchange] < 0} {
  use_hal_manualtoolchange
  lappend save_options use_hal_manualtoolchange
}
if {[lsearch -nocase $::argv -no_sim_spindle] < 0} {
  sim_spindle
}

# make a halfile (inifilename_cmds.hal) with equivalent hal commands
set savefilename \
    ./[file rootname [file tail $::env(INI_FILE_NAME)]]_cmds.hal
save_hal_cmds $savefilename $save_options
