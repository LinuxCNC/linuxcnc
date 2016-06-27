# util_lib.tcl -- utilities

set ::util_lib_quiet 0
# ::tp is the namespace for [HAL]TWOPASS processing
# quiet irrelevant messages in pass0
if {   [namespace exists ::tp] \
    && ([::tp::passnumber] == 0) } { set ::util_lib_quiet 1 }

proc show_context {} {
  if { $::util_lib_quiet } { return }
  set pname show_context
  puts "$pname: argv0=$::argv0"
  puts "$pname:  argv=$::argv"
  foreach arg $::argv {
    puts "$pname:   arg=$arg"
  }
  puts "$pname: INI_FILE_NAME=$::env(INI_FILE_NAME)"
} ;# show_context

proc show_ini {} {
  if { $::util_lib_quiet } { return }
  set vars [uplevel #0 {info vars}]
  set exclude_list {TP auto_index tcl_platform env}
  foreach v $vars {
    if { [lsearch $exclude_list $v] >= 0 } {continue}
    set vg ::$v
    if [array exists $vg] {
      parray $vg
    }
  }
} ;# show_ini_info

proc show_env {} {
  parray ::env
} ;# show_env

proc joint_number_for_axis {axis_letter} {
  # For JOINTS_AXES:
  # Apply rule for known kins with KINEMATICS_IDENTITY
  set kinematics [lindex $::env(KINS_KINEMATICS) 0]
  set coordinates $::env(TRAJ_COORDINATES)
  if {[ string first " " $coordinates] < 0} {
    set coordinates [split $coordinates ""]
  }
  set coordinates [string toupper $coordinates]
  set axis_letter [string toupper $axis_letter]

  # rules for known kinematics types
  switch $kinematics {
    trivkins {set joint_number [lsearch $coordinates $axis_letter]}
    default     {return -code error \
      "joint_number_for_axis: <$axis_letter> unavailable for kinematics:\
<$::env(KINS_KINEMATICS)>"
    }
  }
  #puts stderr "kins=$kinematics coords=$coordinates a=$axis_letter j=$joint_number"
  if { $joint_number < 0} {
    return -code error \
    "joint_number_for_axis <$axis_letter> not in $::env(TRAJ_COORDINATES)"
  }
  return $joint_number
} ;# joint_number_for_axis
