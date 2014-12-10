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
# The moveoff component may be initalized with settings from the ini file
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
#      [OFFSET]
#      EPSILON =
#      WAYPOINT_SAMPLE_SECS =
#      WAYPOINT_THRESHOLD =
#
#   3) Include ini file entries for the per-axis settings
#      [AXIS_n]
#      OFFSET_MAX_LIMIT =
#      OFFSET_MIN_LIMIT =
#      OFFSET_MAX_VELOCITY =
#      OFFSET_MAX_ACCELERATION =
#
#   5) If settings are not found in the ini file, the moveoff
#      component defaults will be used.
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
#   The moveoff_gui will provide a display and and control for
#   enabling offsetting if the pin mv.move-enable is connected
#   when moveoff_gui is started.
#
#   If the mv.move-enable pin is not connected when moveoff_gui
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#-----------------------------------------------------------------------

proc do_hal {args} {
  if {[info exists ::noexecute] && $::noexecute} {
    puts "((do_hal))$args"
  } else {
    if {$::HU(verbose)} {puts do_hal:$args}
    eval hal $args
  }
} ;# do_hal

proc find_thread_names {} {
  # set ::HU(motion,threadname) ==> name of thread with motion-command-handler
  # and populate ::HU for all thread items
  set ans [hal show thread]
  set lines [split $ans \n]
  set header_len 2
  set lines [lreplace $lines 0 [expr $header_len -1]]
  set lines [lreplace $lines end end]
  set remainder ""
  set ct 0
  foreach line $lines {
    catch {unset f1 f2 f3 f4}
    scan [lindex $lines $ct] \
                "%s %s %s %s" \
                 f1 f2 f3 f4
    if ![info exists f3] {
      set index $f1; set compname $f2
      set ::HU($threadname,$compname)  $index
      set ::HU($threadname,index,last) $index
      if {"$compname" == "motion-command-handler"} {
        set ::HU(motion-command-handler,threadname) $threadname
        set ::HU(motion-command-handler,index)      $index
      }
      if {"$compname" == "motion-controller"} {
        set ::HU(motion-controller,threadname) $threadname
        set ::HU(motion-controller,index)      $index
      }
    } else {
      set period $f1; set fp $f2; set threadname $f3
      lappend ::HU(threadnames) $threadname
      set ::HU($threadname,fp) $fp
      set ::HU($threadname,period) $period
    }
    incr ct
  }
  if [info exists ::HU(motion-controller,threadname)] {
    if [info exists ::HU(motion-command-handler,threadname)] {
      if {   "$::HU(motion-controller,threadname)" \
          != "$::HU(motion-command-handler,threadname)" \
         } {
        return -code error "find_thread_names: mot funcs on separate threads"
        if {  $::HU(motion-command-handler,index) \
            > $::HU(motion-controller,index) \
           } {
          return -code error "find_thread_names: mot funcs OUT-OF-SEQUENCE"
        }
      }
      set ::HU(motion,threadname) $::HU(motion-controller,threadname)
      return $::HU(motion,threadname)
    } else {
      return -code error "find_thread_names: motion-controller not found"
    }
  } else {
    return -code error "find_thread_names: motion-command-handler not found"
  }
} ;# find_thread_names

proc is_connected {result_name pinname } {
  upvar $result_name ay
  # use hal directly here for show
  # elsewhere use do_hal to support testing with ::noexecute
  set ans [hal show pin $pinname]
  set lines [split $ans \n]
  set sig ""
  set sep ""
  set ct [scan [lindex $lines 2] "%s %s %s %s %s %s %s" \
                                 owner type dir val name sep sig]
  if {$ct <0} {
    return 0
  }
  set ay(owner)     $owner
  set ay(type)      $type
  set ay(direction) $dir
  set ay(value)     $val
  set ay(separator) $sep
  set ay(signame)   $sig
  return 1
} ;# is_connected

proc get_netlist {inpins_name outpin_name signame} {
  upvar $inpins_name inpins
  upvar $outpin_name outpin
  # use hal directly here for show
  # elsewhere use do_hal to support testing with ::noexecute
  set ans [hal show sig $signame]
  set lines [split $ans \n]
  set header_len 3
  set lines [lreplace $lines 0 [expr $header_len -1]]
  set lines [lreplace $lines end end]
  set ct 0
  foreach line $lines {
    scan [lindex $lines $ct] "%s %s" dir pinname
    switch $dir {
      "==>" {lappend inpins $pinname}
      "<==" {lappend outpin $pinname}
      default {return -code error "get_netlist: unrecognized <$line>"}
    }
    incr ct
  }
  if {[llength $inpins] == 0} { set inpins {} }
  if {[llength $outpin] == 0} { set outpin {} }
} ;# get_netlist

proc setup_pinnames {} {
  # Identify AXIS_n stanzas
  for {set a 0} {$a < 9} {incr a} {
     if [info exists ::AXIS_[set a](TYPE)] {
        lappend ::HU(axes) $a
     }
  }
  foreach a $::HU(axes) {
    set ::HU($a,pos,pinname) axis.${a}.motor-pos-cmd
    set ::HU($a,fb,pinname)  axis.${a}.motor-pos-fb
  }
} ;# setup_pinnames

proc install_moveoff {} {
  # note expected name for moveoff_gui is $::m
  do_hal loadrt moveoff personality=[llength $::HU(axes)] names=$::m

  set mot_thread $::HU(motion,threadname)

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
  if [is_connected tmp $pinname] {
    if {$tmp(signame) != ""} {
      get_netlist inpins outpin $tmp(signame)
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
  if [is_connected tmp $pinname] {
    if {$tmp(signame) != ""} {
      get_netlist inpins outpin     $tmp(signame)
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
  foreach in_pinname $::HU($a,fb,inputs) {
    if {"$in_pinname" == "$::HU($a,pos,pinname)"} continue
    do_hal unlinkp            $in_pinname
    do_hal net hu:minus-$a => $in_pinname
  }
} ;# new_connect_plus_offset

proc new_connect_fb_to_moveoff {a} {
  if $::HU($a,shortcircuit) return
  do_hal delsig $::HU($a,fb,signame)
  do_hal net hu:fb-$a => $::m.fb-$a
  do_hal net hu:fb-$a <= $::HU($a,fb,output)
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
  # note: first look for item preceded with 'OFFSET_'
  foreach {item ininame} {max,velocity     MAX_VELOCITY \
                          max,acceleration MAX_ACCELERATION \
                          max,limit        MAX_LIMIT \
                          min,limit        MIN_LIMIT \
                         } {
    if [info exists ::AXIS_[set a](OFFSET_$ininame)] {
      set ::HU($a,$item) [set ::AXIS_[set a](OFFSET_$ininame)]
    }
  }
  # setp only if ini entry found:
  if [info exists ::HU($a,max,velocity)] {
    do_hal setp $::m.offset-vel-$a    $::HU($a,max,velocity)
  }
  if [info exists ::HU($a,max,acceleration)] {
    do_hal setp $::m.offset-accel-$a  $::HU($a,max,acceleration)
  }
  if [info exists ::HU($a,max,limit)] {
    do_hal setp $::m.offset-max-$a    $::HU($a,max,limit)
  }
  if [info exists ::HU($a,min,limit)] {
    do_hal setp $::m.offset-min-$a    $::HU($a,min,limit)
  }
} ;# set_moveoff_inputs

proc set_moveoff_limits {} {
  foreach {item ininame} {epsilon               EPSILON \
                          waypoint,sample,secs  WAYPOINT_SAMPLE_SECS \
                          waypoint,threshold    WAYPOINT_THRESHOLD \
                         } {
    if {[info exists ::OFFSET($ininame)]} {
      set ::HU($item) [set ::OFFSET($ininame)]
    }
  }
  # setp only if ini entry found:
  if [info exists ::HU(epsilon)] {
    do_hal setp $::m.epsilon              $::HU(epsilon)
  }
  if [info exists ::HU(waypoint,sample,secs)] {
    do_hal setp $::m.waypoint-sample-secs $::HU(waypoint,sample,secs)
  }
  if [info exists ::HU(waypoint,threshold)] {
    do_hal setp $::m.waypoint-threshold   $::HU(waypoint,threshold)
  }
} ;# set_moveoff_limits

proc set_moveoff_controls {} {
  # offset control connections:
  do_hal net hu:ison         halui.machine.is-on     => $::m.is-on

  do_hal net hu:applyoffsets halui.program.is-paused => $::m.apply-offsets
  # Note: The above connection is for offset during program pause.
  #       Other modes are possible but not implemented herein.
  #       Subsequent halfiles may delete this signal and reconnect
  #       for other purposes
} ;# set_moveoff_controls

# begin-----------------------------------------------------------------
package require Hal

set ::m mv ;# moveoff component name
            #(must agree with the (optionaL) gui moveoff_gui)

# debugging items
set ::HU(verbose) 0
set ::noexecute   0

if [catch {
  set ::HU(cmd) find_thread_names; eval $::HU(cmd)
  set ::HU(cmd) setup_pinnames;    eval $::HU(cmd)
  set ::HU(cmd) install_moveoff;   eval $::HU(cmd)
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
  set ::HU(cmd) "set_moveoff_limits"; eval $::HU(cmd)
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

