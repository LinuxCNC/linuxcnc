# hallib procs:
source [file join $::env(HALLIB_DIR) hal_procs_lib.tcl]
#
# hookup_moveoff.tcl -- HALFILE to:
#   1) disconnect initial pos-cmd and pos-fb pin connections
#   2) loadrt the moveoff component with name=mv
#   3) addf the moveoff component functions in required sequence
#   4) reconnect the pos-cmd and pos-fb pins to use the component
#
# Support for demo type ini files where the pos-cmd  and pos-fb pins are
# 'shortcircuit' connected together is included.
#
# The moveoff component may be initialized with settings from the ini file
#
# Usage:
#   1) Specify this file in the ini file as [HAL]HALFILE
#      Its position must follow halfiles that connect the pos-cmd and
#      pos-fb pins.
#      [HAL]
#      ...
#      HALFILE = hookup_moveoff.tcl
#      ...
#
#   2) Include ini file entries for moveoff component settings:
#      [MOVEOFF]
#      EPSILON =
#      WAYPOINT_SAMPLE_SECS =
#      WAYPOINT_THRESHOLD =
#      BACKTRACK_ENABLE =
#
#      If these settings are not found in the ini file, the moveoff
#      component defaults will be used.
#
#   3) Include ini file entries for the per-joint settings
#      [MOVEOFF_n]
#      MAX_VELOCITY =
#      MAX_ACCELERATION =
#      MAX_LIMIT =
#      MIN_LIMIT =
#
#      If settings are not found in the ini file, the items 
#      [JOINT_n]
#      MAX_VELOCITY =
#      MAX_ACCELERATION =
#      MAX_LIMIT =
#      MIN_LIMIT =
#      are used.  If these are not defined, the moveoff component
#      defaults are used.
#
#   To use the (optional) demonstration gui named moveoff_gui,
#   include an ini entry:
#
#   [APPLICATIONS]
#   APP = moveoff_gui option1 option2 ...
#
#   For available options, Use:
#      $ moveoff_gui --help
#
#   The moveoff_gui will provide a display and control for
#   enabling offsetting if the pin mv.move-enable is not connected
#   when moveoff_gui is started.
#
#   If the mv.move-enable pin is connected when moveoff_gui
#   is started, then it will provide a display but no control.
#   This mode supports hal connections for a jog wheel or other
#   methods of controlling the offset input pins (mv.offset-M)

#-----------------------------------------------------------------------
# Copyright: 2014
# Authors:   Dewey Garrett <dgarrett@panix.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#-----------------------------------------------------------------------

proc do_hal {args} {
  if {[info exists ::noexecute] && $::noexecute} {
    puts "((do_hal))$args"
  } else {
    if {$::HU(verbose)} {puts do_hal:$args}
    eval hal $args
  }
} ;# do_hal

proc setup_pinnames {} {
  # Note: works for standard names but a custom proc is needed
  #       here if the Hal alias command is used on the names

  # Identify JOINT_n stanzas
  for {set a 0} {$a < 9} {incr a} {
     if [info exists ::JOINT_[set a](TYPE)] {
        lappend ::HU(axes) $a
        set ::HU(highest_joint_num) $a
     }
  }
  foreach a $::HU(axes) {
    set ::HU($a,pos,pinname) joint.${a}.motor-pos-cmd
    set ::HU($a,fb,pinname)  joint.${a}.motor-pos-fb
  }
} ;# setup_pinnames

proc install_moveoff {} {
  # note expected name for moveoff_gui is $::m
  set pnumber [expr 1 + $::HU(highest_joint_num)]
  do_hal loadrt moveoff personality=$pnumber names=$::m

  set mot_thread $::HU(motion-controller,threadname)

  if [info exists ::HU($mot_thread,motion-controller)] {
    set write_index [expr 1 + $::HU($mot_thread,motion-controller)]
  } else {
    set write_index [expr 1 + $::HU($mot_thread,index,last)]
  }
  do_hal addf $::m.write-outputs $mot_thread $write_index

  if [info exists ::HU($mot_thread,motion-command-handler)] {
    if {1 == $::HU($mot_thread,motion-command-handler) } {
      set read_index 1
    } else {
      set read_index [expr -1 + $::HU($mot_thread,motion-command-handler)]
    }
  } else {
    set read_index 1
  }
  do_hal addf $::m.read-inputs $mot_thread $read_index
} ;# install_moveoff

proc disconnect_pos_from_motion {a} {
  set pinname $::HU($a,pos,pinname)
  set inpins {}; set outpin {}
  if [connection_info tmp $pinname] {
    if {$tmp(signame) != ""} {
      get_netlist inpins outpin iopins $tmp(signame)
      set ::HU($a,pos,signame)  $tmp(signame)
      set ::HU($a,pos,inputs)   $inpins
      set ::HU($a,pos,output)   $outpin
    }
  } else {
    return -code error "<$pinname> not connected as expected"
  }
  do_hal unlinkp $pinname
} ;# disconnect_pos_from_motion

proc disconnect_fb_to_motion {a} {
  set pinname $::HU($a,fb,pinname)
  set inpins {}; set outpin {}
  if [connection_info tmp $pinname] {
    if {$tmp(signame) != ""} {
      get_netlist inpins outpin iopins $tmp(signame)
      set ::HU($a,fb,signame)       $tmp(signame)
      set ::HU($a,fb,inputs)        $inpins
      set ::HU($a,fb,output) $outpin
    }
  } else {
    return -code error "<$pinname> not connected as expected"
  }
  do_hal unlinkp $pinname
} ;# disconnect_fb_to_motion

proc check_for_short_circuit {a} {
  if {$::HU($a,fb,signame) == $::HU($a,pos,signame)} {
    set ::HU($a,shortcircuit) 1
  } else {
    set ::HU($a,shortcircuit) 0
  }
} ;# check_for_short_circuit

proc new_connect_pos_to_moveoff {a} {
  do_hal delsig $::HU($a,pos,signame)
  do_hal net hu:pos-$a <= $::HU($a,pos,pinname)
  do_hal net hu:pos-$a => $::m.pos-$a
} ;# new_connect_pos_to_moveoff

proc new_connect_plus_offset {a} {
  if $::HU($a,shortcircuit) return
  do_hal net hu:plus-$a <= $::m.pos-plusoffset-$a
  foreach in_pinname $::HU($a,pos,inputs) {
    if {"$in_pinname" == "$::HU($a,fb,pinname)"} continue
    do_hal unlinkp           $in_pinname
    do_hal net hu:plus-$a => $in_pinname
  }
} ;# new_connect_plus_offset

proc new_connect_minus_offset {a} {
  do_hal net hu:minus-$a <= $::m.fb-minusoffset-$a
  do_hal net hu:minus-$a => $::HU($a,fb,pinname)
} ;# new_connect_plus_offset

proc new_connect_fb_to_moveoff {a} {
  if $::HU($a,shortcircuit) return
  do_hal delsig $::HU($a,fb,signame)
  do_hal net hu:fb-$a => $::m.fb-$a
  do_hal net hu:fb-$a <= $::HU($a,fb,output)
  foreach in_pinname $::HU($a,fb,inputs) {
  if {"$in_pinname" == "$::HU($a,fb,pinname)"} continue
    do_hal unlinkp         $in_pinname
    do_hal net hu:fb-$a => $in_pinname
  }
} ;# new_connect_fb_to_moveoff

proc reconnect_short_circuit {a} {
  do_hal net hu:shortcircuit-$a <= $::m.pos-plusoffset-$a
  do_hal net hu:shortcircuit-$a => $::m.fb-$a
  foreach in_pinname $::HU($a,pos,inputs) {
    if {"$in_pinname" == "$::HU($a,fb,pinname)"} {continue}
    do_hal unlinkp                   $in_pinname
    do_hal net hu:shortcircuit-$a => $in_pinname
  }
} ;# reconnect_short_circuit

proc set_moveoff_inputs {a} {
  foreach {pin ininame} { offset-vel   MAX_VELOCITY \
                          offset-accel MAX_ACCELERATION \
                          offset-max   MAX_LIMIT \
                          offset-min   MIN_LIMIT \
                         } {
    if [info exists ::MOVEOFF_[set a]($ininame)] {
      set ::HU($a,$pin) [set ::MOVEOFF_[set a]($ininame)]
      # lindex is used in case there are duplicate entries
      set ::HU($a,$pin) [lindex $::HU($a,$pin) end]
    } elseif { [info exists ::JOINT_[set a]($ininame)] } {
      set ::HU($a,$pin) [set ::JOINT_[set a]($ininame)]
      # lindex is used in case there are duplicate entries
      set ::HU($a,$pin) [lindex $::HU($a,$pin) end]
      puts "hookup_moveoff.tcl:use \[JOINT_$a\]$ininame=$::HU($a,$pin)"
    }
    if [info exists ::HU($a,$pin)] {
      do_hal setp $::m.$pin-$a    $::HU($a,$pin)
    }
  }
} ;# set_moveoff_inputs

proc set_moveoff_parms {} {
  foreach {pin ininame} { epsilon               EPSILON \
                          waypoint-sample-secs  WAYPOINT_SAMPLE_SECS \
                          waypoint-threshold    WAYPOINT_THRESHOLD \
                          backtrack-enable      BACKTRACK_ENABLE \
                         } {
    if {[info exists ::MOVEOFF($ininame)]} {
      # lindex is used in case there are duplicate entries
      set ::HU($pin) [lindex $::MOVEOFF($ininame) end]
      do_hal setp $::m.$pin $::HU($pin)
    }
  }
} ;# set_moveoff_parms

proc set_moveoff_controls {} {
  return
  # provision for future offset control connections
} ;# set_moveoff_controls

# begin-----------------------------------------------------------------

set ::m mv ;# moveoff component name
            #(must agree with the (optionaL) gui moveoff_gui)

# debugging items
set ::HU(verbose) 0
set ::noexecute   0

# Provision for twopass compatibility
# (::tp is the namespace for [HAL]TWOPASS processing)
if [namespace exists ::tp] {
  set passno [::tp::passnumber]
  if {$passno == 0} {
    # With twopass processing, the initial pass0 only collects
    # loadrt and loadusr commands with no execution.
    # So  the checks etc herein cannot work until pass1
    # Note that this file only uses loadrt once for the moveoff
    # component so it manages its loadrt exclusively
    puts "hookup_moveoff.tcl: twopass active, pass $passno skipped"
    return
  } else {
    puts "hookup_moveoff.tcl: twopass active, pass $passno active"
  }
}

if [catch {
  set ::HU(cmd) "thread_info ::HU"; eval $::HU(cmd)
  set ::HU(cmd) setup_pinnames;     eval $::HU(cmd)
  set ::HU(cmd) install_moveoff;    eval $::HU(cmd)
  foreach a $::HU(axes) {
    set ::HU(cmd) "disconnect_pos_from_motion $a"; eval $::HU(cmd)
    set ::HU(cmd) "disconnect_fb_to_motion    $a"; eval $::HU(cmd)
    set ::HU(cmd) "check_for_short_circuit    $a"; eval $::HU(cmd)
    set ::HU(cmd) "new_connect_pos_to_moveoff $a"; eval $::HU(cmd)
    set ::HU(cmd) "new_connect_plus_offset    $a"; eval $::HU(cmd)
    set ::HU(cmd) "new_connect_minus_offset   $a"; eval $::HU(cmd)
    set ::HU(cmd) "new_connect_fb_to_moveoff  $a"; eval $::HU(cmd)
    if $::HU($a,shortcircuit) {
      set ::HU(cmd) "reconnect_short_circuit $a "; eval $::HU(cmd)
    }
    set ::HU(cmd) "set_moveoff_inputs  $a"; eval $::HU(cmd)
  }
  set ::HU(cmd) "set_moveoff_parms"; eval $::HU(cmd)
  set ::HU(cmd) "set_moveoff_controls"; eval $::HU(cmd)
  if $::HU(verbose) {parray ::HU}
} msg ] {
  puts "\n\n$::argv0 $::argv"
  parray ::HU
  catch {puts filename=$filename}
  puts "\n failing cmd=$::HU(cmd)"
  puts msg=<$msg>
  return -code error
}
