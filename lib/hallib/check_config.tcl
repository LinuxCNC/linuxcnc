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
  puts -nonewline "\n$::progname:\n"
  catch {puts ($::kins(module) kinematics) WARNING:"}
  foreach m $msg {puts "  $m"}
  puts ""
} ;# warnings

proc err_exit msg {
  puts -nonewline "\n$::progname:\n"
  catch {puts ($::kins(module) kinematics) ERROR:"}
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

proc consistent_coords_for_trivkins {trivcoords} {
  set m {" " "" "\t" ""} ;# mapping to remove whitespace
  set trivcoords [string map $m $trivcoords]
  if {"$trivcoords" == "XYZABCUVW"} {
     return ;# unspecified trivkins coordinates
             # allows use of any coordinates
  }
  set trajcoords ""
  if [info exists ::TRAJ(COORDINATES)] {
    set trajcoords [string map $m [lindex $::TRAJ(COORDINATES) 0]]
  }
  if {[string toupper "$trivcoords"] != [string toupper "$trajcoords"]} {
    lappend ::wmsg "INCONSISTENT coordinates specifications:
               trivkins coordinates=$trivcoords
               \[TRAJ\]COORDINATES=$trajcoords"
  }
} ;# consistent_coords_for_trivkins

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

proc assert_ini_datatypes {} {
  # Datatype assertions, run right after parse_ini, before any comparison.
  # numeric: must be a real number (covers integers too: string is double)
  set numeric {
    MIN_LIMIT MAX_LIMIT MAX_VELOCITY MAX_ACCELERATION MAX_JERK
    FERROR MIN_FERROR BACKLASH
    P I D FF0 FF1 FF2 BIAS DEADBAND MAX_OUTPUT MAX_ERROR
    INPUT_SCALE OUTPUT_SCALE SCALE
    HOME HOME_OFFSET HOME_SEARCH_VEL HOME_LATCH_VEL HOME_FINAL_VEL
    HOME_SEQUENCE STEPGEN_MAXVEL STEPGEN_MAXACCEL
  }
  # enum: must match a fixed set (case-insensitive, mirroring LinuxCNC's
  # caseless maps mapJointType / convertBool)
  set bool {TRUE YES 1 ON FALSE NO 0 OFF}
  array set enum [list \
    TYPE                        {LINEAR ANGULAR} \
    HOME_USE_INDEX              $bool \
    HOME_IGNORE_LIMITS          $bool \
    HOME_IS_SHARED              $bool \
    HOME_INDEX_NO_ENCODER_RESET $bool \
    VOLATILE_HOME               $bool \
    LOCKING_INDEXER             $bool \
  ]
  set emsg ""
  foreach g [info globals] {
    if {![string match JOINT_* $g] && ![string match AXIS_* $g]} continue
    foreach item [array names ::$g] {
      set val [set ::${g}($item)]
      # LinuxCNC reads the first occurrence (IniFile::Find defaults to num=1),
      # so a duplicate is not fatal. Validate the first occurrence; if there is
      # more than one, warn and collapse the array to it so the downstream limit
      # comparison uses the same value LinuxCNC would.
      set use [lindex $val 0]
      if {[llength $val] > 1} {
        lappend ::wmsg "Multiple values for \[$g\]$item: <$val> (first value is used)"
        set ::${g}($item) $use
      }
      if {[lsearch -exact $numeric $item] >= 0} {
        if {![string is double -strict $use]} {
          set m "\[$g\]$item must be numeric, got: <$use>"
          if {[string match {*[#;]*} $use]} {
            append m "\n    Inline comments are not allowed: '#' and ';' start a comment"
            append m "\n    only at the start of a line, not after a value. Move the comment"
            append m "\n    to its own line. See the LinuxCNC manual, \"The INI File\" ->"
            append m "\n    \"Comments\": https://linuxcnc.org/docs/stable/html/config/ini-config.html#_comments"
          }
          lappend emsg $m
        }
      } elseif {[info exists enum($item)]} {
        if {[lsearch -nocase $enum($item) $use] < 0} {
          lappend emsg "\[$g\]$item must be one of {$enum($item)}, got: <$use>"
        }
      }
    }
  }
  if {"$emsg" != ""} {
    if {[info exists ::wmsg]} {warnings $::wmsg}
    err_exit $emsg
  }
} ;# assert_ini_datatypes

proc check_extrajoints {} {
  if ![info exists ::EMCMOT(EMCMOT)] return
  if {[string first motmod $::EMCMOT(EMCMOT)] <= 0} return
  set mot [split [lindex $::EMCMOT(EMCMOT) 0]]
  foreach item $mot {
    set pair [split $item =]
    set parm [lindex $pair 0]
    set val  [lindex $pair 1]
    if {"$parm" == "num_extrajoints"} {
      set ::num_extrajoints $val
    }
  }
  if [info exists ::num_extrajoints] {
     lappend ::wmsg [format "Extra joints specified=%d\n \
\[KINS\]JOINTS=%d must accommodate kinematic joints *plus* extra joints " \
                     $::num_extrajoints $::KINS(JOINTS)]
  }
} ;#check_extrajoints

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

assert_ini_datatypes

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
check_extrajoints

#parray ::kins
set emsg [validate_identity_kins_limits]
consistent_coords_for_trivkins $::kins(coordinates)
if [info exists ::wmsg] {warnings $::wmsg}

if {"$emsg" == ""} {exit 0}
err_exit $emsg
