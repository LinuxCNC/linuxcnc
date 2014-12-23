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

