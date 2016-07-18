# check_config.tcl
# Usage: tclsh path_to_this_file inifilename
# if a check fails, print message and exit 1
#
# checks:
#   1) mandatory items per ::mandatory_items list
#   2) if trivkins, check consistency of [JOINT_] and [AXIS_]
#      MIN_LIMIT and MAX_LIMIT
#----------------------------------------------------------------------
set ::mandatory_items {KINS KINEMATICS
                       KINS JOINTS
                      }
#----------------------------------------------------------------------
proc err_exit msg {
  puts "\n$::progname: ERROR"
  if [info exists ::kins(module)] {
    puts "  (Using identity kinematics: $::kins(module))"
  }
  foreach m $msg {puts "  $m"}
  puts ""
  exit 1
} ;# err_exit

proc check_mandatory_items {} {
  foreach {section item} $::mandatory_items {
    if ![info exists ::[set section]($item) ] {
      set section [string trim $section :]
      lappend emsg "Missing \[$section\]$item="
    }
  }
  if [info exists emsg] {
    err_exit $emsg
  }
} ;# check_mandatory_items

proc uniq {list_name} {  ;# make list unique
  foreach item $list_name { set tmp($item) "" }
  return [array names tmp]
} ;# uniq

proc joints_for_trivkins {coords} {
  # ref: src/emc/kinematics/trivkins.c
  # order of coord letters determines assigned joint number
  if {"$coords" == ""} {set coords {X Y Z A B C U V W}}
  set i 0
  foreach a [string toupper $coords] {
     # assign to consecutive joints:
     lappend ::kins(jointidx,$a) $i
     incr i
  }
} ;# joints_for_trivkins

proc validate_identity_kins_limits {} {
  set emsg ""
  foreach c [uniq $::kins(coordinates)] {
    if ![info exists ::AXIS_[set c](MIN_LIMIT)] {
      set ::AXIS_[set c](MIN_LIMIT) -1e99 ;# ref: src/emc/ini/iniaxis.cc
    }
    if ![info exists ::AXIS_[set c](MAX_LIMIT)] {
      set ::AXIS_[set c](MAX_LIMIT)  1e99 ;# ref: src/emc/ini/iniaxis.cc
    }
    foreach j $::kins(jointidx,$c) {
      if [info exists  ::JOINT_[set j](MIN_LIMIT)] {
         set jlim [set ::JOINT_[set j](MIN_LIMIT)]
         set clim [set  ::AXIS_[set c](MIN_LIMIT)]
         if {$jlim > $clim} {
           lappend emsg "\[JOINT_$j\]MIN_LIMIT > \[AXIS_$c\]MIN_LIMIT ($jlim > $clim)"
         }
      }
      if [info exists  ::JOINT_[set j](MAX_LIMIT)] {
         set jlim [set ::JOINT_[set j](MAX_LIMIT)]
         set clim [set  ::AXIS_[set c](MAX_LIMIT)]
         if {$jlim < $clim} {
           lappend emsg "\[JOINT_$j\]MAX_LIMIT < \[AXIS_$c\]MAX_LIMIT ($jlim < $clim)"
         }
      }
    }
  }
  return $emsg
} ;# validate_identity_kins_limits
#----------------------------------------------------------------------
# begin
package require Linuxcnc ;# parse_ini

set ::progname [file rootname [file tail [info script]]]
set inifile [lindex $::argv 0]
if ![file exists $inifile]   {
  err_exit [list "No such file <$inifile>"]
}
if ![file readable $inifile] {
   err_exit [list "File not readable <$inifile>"]
}
parse_ini $inifile

check_mandatory_items

set kins_kinematics [lindex $::KINS(KINEMATICS) end]
set ::kins(module)  [lindex $kins_kinematics 0]

# parameters specified as parm=value --> ::kins(parm)=value
foreach item $kins_kinematics {
  if {[string first = $item] >= 0} {
    set l [split $item =]
    set ::kins([lindex $l 0]) [lindex $l 1]
  } else {
    if {"$item" != $::kins(module)} {
      puts "Unknown \[KINS\]KINEMATICS item: <$item>"
    }
  }
}
# coordinates --> ::kins(coordinates) (all if not specified)
if [info exists ::kins(coordinates)] {
  set ::kins(coordinates) [split $::kins(coordinates) ""]
  set ::kins(coordinates) [string toupper $::kins(coordinates)]
} else {
  set ::kins(coordinates) {X Y Z A B C U V W}
}

# list of joints for each coord --> ::kins(jointidx,coordinateletter) 
switch $::kins(module) {
  trivkins {joints_for_trivkins $::kins(coordinates)}
  default  {
    puts "$::progname: Unknown \[KINS\]KINEMATICS=$::KINS(KINEMATICS)"
    exit 0
  }
}

#parray ::kins
set emsg [validate_identity_kins_limits]

if {"$emsg" == ""} {exit 0}
err_exit $emsg
