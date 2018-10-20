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
# Ref: src/emc/nml_intf/emccfg.h,src/emc/ini/inijoint.cc,src/emc/iniaxis.cc:
set ::DEFAULT_AXIS_MAX_VELOCITY      1.0
set ::DEFAULT_AXIS_MAX_ACCELERATION  1.0
set ::DEFAULT_JOINT_MAX_VELOCITY     1.0
set ::DEFAULT_JOINT_MAX_ACCELERATION 1.0

# Ref: src/emc/ini/iniaxis.cc:
set ::DEFAULT_AXIS_MIN_LIMIT -1e99
set ::DEFAULT_AXIS_MAX_LIMIT +1e99
#----------------------------------------------------------------------
proc warnings msg {
  puts "\n$::progname: ($::kins(module) kinematics) WARNING:"
  foreach m $msg {puts "  $m"}
  puts ""
} ;# warnings

proc err_exit msg {
  puts "\n$::progname: ($::kins(module) kinematics) ERROR:"
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
  for {set j 0} {$j < $::KINS(JOINTS)} {incr j} {
    if {![info exists ::JOINT_[set j](MAX_VELOCITY)] } {
      lappend ::wmsg "Unspecified \[JOINT_$j\]MAX_VELOCITY,     default used: $::DEFAULT_JOINT_MAX_VELOCITY"
    }
    if {![info exists ::JOINT_[set j](MAX_ACCELERATION)] } {
      lappend ::wmsg "Unspecified \[JOINT_$j\]MAX_ACCELERATION, default used: $::DEFAULT_JOINT_MAX_ACCELERATION"
    }
  }
  foreach c [uniq $::kins(coordinates)] {
    # array test avoids superfluous messages when coordinates= not specified
    if [array exists ::AXIS_[set c]] {
      if {![info exists ::AXIS_[set c](MAX_VELOCITY)] } {
        lappend ::wmsg "Unspecified \[AXIS_$c\]MAX_VELOCITY,     default used: $::DEFAULT_AXIS_MAX_VELOCITY"
      }
      if {![info exists ::AXIS_[set c](MAX_ACCELERATION)] } {
        lappend ::wmsg "Unspecified \[AXIS_$c\]MAX_ACCELERATION, default used: $::DEFAULT_AXIS_MAX_ACCELERATION"
      }
    }

    set missing_axis_min_limit 0
    set missing_axis_max_limit 0
    if ![info exists ::AXIS_[set c](MIN_LIMIT)] {
      set ::AXIS_[set c](MIN_LIMIT) $::DEFAULT_AXIS_MIN_LIMIT
      set missing_axis_min_limit 1
    }
    if ![info exists ::AXIS_[set c](MAX_LIMIT)] {
      set ::AXIS_[set c](MAX_LIMIT) $::DEFAULT_AXIS_MAX_LIMIT
      set missing_axis_max_limit 1
    }
    foreach j $::kins(jointidx,$c) {
      if [info exists  ::JOINT_[set j](MIN_LIMIT)] {
         set jlim [set ::JOINT_[set j](MIN_LIMIT)]
         set clim [set  ::AXIS_[set c](MIN_LIMIT)]
         if {$jlim > $clim} {
           if $missing_axis_min_limit {
             lappend ::wmsg "Unspecified \[AXIS_$c\]MIN_LIMIT,        default used: $::DEFAULT_AXIS_MIN_LIMIT"
           }
           lappend emsg "\[JOINT_$j\]MIN_LIMIT > \[AXIS_$c\]MIN_LIMIT ($jlim > $clim)"
         }
      }
      if [info exists  ::JOINT_[set j](MAX_LIMIT)] {
         set jlim [set ::JOINT_[set j](MAX_LIMIT)]
         set clim [set  ::AXIS_[set c](MAX_LIMIT)]
         if {$jlim < $clim} {
           if $missing_axis_max_limit {
             lappend ::wmsg "Unspecified \[AXIS_$c\]MAX_LIMIT,        default used: $::DEFAULT_AXIS_MAX_LIMIT"
           }
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
    puts "$::progname: Unchecked: \[KINS\]KINEMATICS=$::KINS(KINEMATICS)"
    exit 0
  }
}

#parray ::kins
set emsg [validate_identity_kins_limits]
if [info exists ::wmsg] {warnings $::wmsg}

if {"$emsg" == ""} {exit 0}
err_exit $emsg
