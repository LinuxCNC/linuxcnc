# halcheck.tcl: a halfile to report:
#  1) functions with no addf   (usually an error)
#  2) signals   with no inputs (not an error)
#  3) signals   with no output (not an error)
#
# Usage in ini file (must be the last HALFILE):
# [HAL]
# ...
# HALFILE = LIB:halcheck.tcl
#
# Note: connections in a POSTGUI_HALFILE are not checked
#

# config items:
set ::popup 1 ;# default: enable popup (in addition to prints)
set ::bigfont {Helvetica 12 bold} ;# {Family size weight}

#----------------------------------------------------------------------
set ::prog halcheck

# test for ::argv items on halfile line (requires 2.7)
if [info exists ::argv] {
  if {[lsearch $::argv nopopup] >= 0} {
    set ::popup 0 ;# disable popup
  }
}

#----------------------------------------------------------------------
proc gui_message { type items } {
  if { "$items" == "" } return
  # twopass processing uses built-ins only (no Tk)
  if {$::popup && [catch {package require Tk} msg]} {
    set ::popup 0
  }
  if { !$::popup } return
  if { ![winfo exists .b] } {
    wm title . "HAL checks"
    pack [button .b \
         -text Dismiss \
         -command {destroy .} \
         ] -side bottom
  }
  set items [string map {" " \n} $items]
  switch $type {
    no_addfs  {
      pack [label .noa1 \
           -text "Functions with no addf: " \
           -font $::bigfont \
           -relief ridge -bd 3 \
           ] -side top -fill x -expand 1
      pack [label .noa2 \
           -text "$items" \
           -justify left \
           -anchor w \
           -relief sunken -bd 3 \
           ] -side top -fill x -expand 1
    }
    no_inputs {
      pack [label .noi1 \
           -text "Signals with no inputs: " \
           -font $::bigfont \
           -relief ridge -bd 3 \
           ] -side top -fill x -expand 1
      pack [label .noi2 \
           -text "$items" \
           -justify left \
           -anchor w \
           -relief sunken -bd 3 \
           ] -side top -fill x -expand 1
    }
    no_output {
      pack [label .noo1 \
           -text "Signals with no output: " \
           -font $::bigfont \
           -relief ridge -bd 3 \
           ] -side top -fill x -expand 1
      pack [label .noo2 \
           -text "$items" \
           -justify left \
           -anchor w \
           -relief sunken -bd 3 \
           ] -side top -fill x -expand 1
    }
  }
} ;# gui_message

proc check_function_usage {} {
  set ans [hal show function]
  set lines [split $ans \n]
  set header_len 2
  set lines [lreplace $lines 0 [expr $header_len -1]]
  set lines [lreplace $lines end end]
  set ct 0
  set no_addfs ""
  foreach line $lines {
    catch {unset f1 f2 f3 f4}
    scan [lindex $lines $ct] \
                "%s %s %s %s %s %s" \
                 owner codeaddr arg fp users name
    if {"$users" == 0} {
      puts "$::prog: Functions with no addf: $name"
      lappend no_addfs $name
    }
    incr ct
  }
  gui_message no_addfs $no_addfs
} ;# check_function_usage

proc check_signal_usage {} {
  set ans [hal show signal]
  set lines [split $ans \n]
  set header_len 2
  set lines [lreplace $lines 0 [expr $header_len -1]]
  set lines [lreplace $lines end end]
  set ct 0
  foreach line $lines {
    set howmany [scan $line "%s%s%s" fld1 fld2 fld3]
    if {$howmany == 3} {
      set signame $fld3
      set ::SIG($signame,type)   $fld1
      set ::SIG($signame,value)  $fld2
      set ::SIG($signame,inputs) ""
      set ::SIG($signame,output) ""
      set ::SIG($signame,ios)    ""
      continue
    }
    switch $fld1 {
      "==>" {lappend ::SIG($signame,inputs) $fld2}
      "<==" {lappend ::SIG($signame,output) $fld2}
      "<=>" {lappend ::SIG($signame,ios)    $fld2}
      default {return -code error "check-signal_usage: unrecognized <$line>"}
    }
    incr ct
  }
  set no_inputs ""
  foreach iname [array names ::SIG *,inputs] {
    set signame [lindex [split $iname ,] 0]
    if {$::SIG($iname) == ""} {
      set msg "$::prog: Signal: $signame <$::SIG($signame,value)> NO inputs"
      if {$::SIG($signame,ios) != ""} {
        set msg "$msg (i/o:$::SIG($signame,ios))"
      } else {
        lappend no_inputs $signame
      }
      puts "$msg"
    }
  }
  set no_output ""
  foreach oname [array names ::SIG *,output] {
    set signame [lindex [split $oname ,] 0]
    if {$::SIG($oname) == ""} {
      set msg "$::prog: Signal: $signame <$::SIG($signame,value)> NO output"
      if {$::SIG($signame,ios) != ""} {
        set msg "$msg (i/o:$::SIG($signame,ios))"
      } else {
        lappend no_output $signame
      }
      puts "$msg"
    }
  }
  foreach iname [array names ::SIG *,ios] {
    if { $::SIG($iname) == ""} {unset ::SIG($iname)}
  }
  gui_message no_inputs $no_inputs
  gui_message no_output $no_output
} ;# check_signal_usage

#----------------------------------------------------------------------
# begin
check_function_usage
check_signal_usage
