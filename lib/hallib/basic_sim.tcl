# Notes:
#  1) ::env() is a global associative array of environmental variables
#     as exported by the main LinuxCNC script (linuxcnc)
#  2) Settings from the ini file are available as global associative
#     arrays named: ::SECTION(varname)
#     example: ::EMCMOT(SERVO_PERIOD)
#  3) procs are from sim_lib.tcl

#begin-----------------------------------------------------------------
source [file join $::env(HALLIB_DIR) sim_lib.tcl]
set axes [eval set axes $::TRAJ(COORDINATES)] ;# eval to handle list {}
set axes [string tolower $axes] ;# expect lowercase throughout
set number_of_axes $::TRAJ(AXES)

# core_sim using defaults for base_period and emcmot
core_sim $axes \
         $number_of_axes \
         $::EMCMOT(SERVO_PERIOD)

make_ddts $axes
simulated_home $axes
use_hal_manualtoolchange
sim_spindle
