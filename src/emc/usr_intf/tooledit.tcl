#!/usr/bin/wish
#
# Copyright: 2009
# Author:    Dewey Garrett <dgarrett@panix.com>
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

#
# As standalone tool table editor
# Usage:
#        tooleditor.tcl filename
#
# This file can also be sourced and included in tcl scripts
# The namespace ::tooledit exports a single function:
#     ::tooledit::tooledit
# A single global array ::te() stores private data

# lathe and mill tools can be combined in same tool table file but
# a pathological mill tool entry having a comment field with 4
# leading numbers will be interpreted as a lathe tool entry

# ref: user  manual,version 2.3, mar 6 2009, section 7.4
#  The file consists of any number of header lines, followed by one blank line,
#  followed by any number of lines of data. The header lines are ignored by the
#  interpreter.

# but the current emc implementation does not seem to require a blank line:

# ref: src/emc/iotask/ioControl.cc  loadToolTable()
#  366   /* invalid line. skip it silently */
#  367   continue;

namespace eval ::tooledit {
  namespace export tooledit ;# public interface
}

proc ::tooledit::validangle {v} {
  if {$v <= 360 && $v >= -360}  {return 1}
  return 0
} ;# validangle

proc ::tooledit::isnegative {v} {
  if {$v < 0} {return 1}
  if {[string first - $v] >=0} {return 1} ;# this gets -0
  return 0
} ;# ispositive

proc ::tooledit::isinteger {v} {
  if ![isnumber $v]            {return 0}
  if {[string first . $v] >=0} {return 0}
  if {[string first e [string tolower $v]] >= 0} {return 0}
  return 1
} ;# isinteger

proc ::tooledit::isnumber {v} {
  if [catch {format %f $v}] {
    return 0
  } else {
    return 1
  }
} ;# isnumber

proc ::tooledit::qid {} {
   # generate unique id
   if { ![info exists ::te(qid)]} {
      set ::te(qid) 0
   }
   incr ::te(qid)
   return q$::te(qid)
} ;# qid

proc ::tooledit::ventry {f validatenumber tvar \
                        {twidth 12} {expand 0} {justify left} {fill x}} {
  if {$validatenumber} {
    set e [entry $f.[qid] \
          -width $twidth -relief sunken -justify $justify \
          -textvariable    $tvar \
          -validate        all \
          -validatecommand [list ::tooledit::validateNumber $tvar %W %s %P] \
          -invalidcommand  [list ::tooledit::invalidNumber  $tvar %W] \
         ]
     pack $e -side left -expand $expand -fill $fill
  } else {
    set e [entry $f.[qid] \
          -width $twidth -relief sunken -justify $justify\
          -textvariable $tvar \
         ]
    pack $e -side left -expand $expand -fill $fill
  }
  return $e
} ;# ventry

proc ::tooledit::validateNumber {varname widget current new} {
   if ![info exists $varname] {return 1}
   if {"$new" == "NEW"} {
     return 1 ;# 1==>ok dont flag items tagged "NEW"
   }
   if {"$current" == "NEW"} {
     $widget configure -selectbackground $::te(restore,selectbackground)
     $widget configure -selectforeground $::te(restore,selectforeground)
   }
   if [catch  {format %f $new} ] {
     $widget configure -fg red
     message verror
     return 1 ;# problem but return ok (just change color)
   } else {
     if {"$current" != "$new"} {message modified}
     $widget configure -fg black
     return 1 ;# 1==>ok
   }
} ;# validateNumber

proc ::tooledit::invalidNumber {varname widget} {
  tk_dialog .problem \
            Problem \
            "$varname must be a number" \
            {} \
            0 \
            ok
  $widget configure -validate all ;# restore validation
} ;# invalidNumber

proc ::tooledit::readfile {filename} {
  # return codes:
  #              error  msg
  #              nofile 0
  #              ok     1
  if {[file exists $filename] && ![file readable $filename]} {
    set msg "filename: <$filename> not readable"
  }
  if [file exists $filename] {
    if ![file writable $filename] {
      set msg "filename: <$filename> not writable"
    }
  } else {
    set new 1
    if ![file writable [file dirname $filename]] {
      set msg "filename: <$filename> directory not writable"
    }
  }

  if [info exists msg] {return $msg}
  if [info exists new] {return    0}

  set fd [open $filename r]

  set bct 0
  while {1} {
    gets $fd newline
    if [eof $fd] break
    if {"$newline" == ""} {
      # a mandatory blank line denotes end of header
      set endheader 1
      continue
    }

    catch {unset m}; set m(4) ""
    set mct [scan $newline "%d %d %f %f %\[^\n\]" m(0) m(1) m(2) m(3) m(4)]

    catch {unset l}; set l(8) ""
    set lct [scan $newline "%d %d %f %f %f %f %f %d %\[^\n\]" \
            l(0) l(1) l(2) l(3) l(4) l(5) l(6) l(7) l(8)]

    if { $lct == 8 || $lct == 9} {
      makeline lathe l
    } elseif {$mct == 4 || $mct == 5} {
      makeline mill m
    } else {
      # newline was not parsed as mill or lathe item
      # try to notify for bogus items:
      set nl [string tolower $newline]
      if {    [string trim $newline] == "" \
          ||  [string first poc $nl] >= 0 \
          ||  [string first "file includes both" $nl] >= 0 \
         } {
        # ignore header line
      } else {
        set bl($bct) "$newline"
        incr bct
      }
      continue
    }
  } ;# while
  close $fd
  if [info exists bl] {
    puts stderr "bogus lines ignored:"
    for {set b 0} {$b < $bct} {incr b} {
      puts stderr "$bl($b)"
    }
    puts stderr ""
    # schedule message after message widget is created
    after 0 {::tooledit::message bogus}
  }
  return 1 ;# ok
} ;# readfile

proc ::tooledit::watch {args} {
  catch {after cancel $::te(afterid)}
  if ![file exists $::te(filename)] {
    ::tooledit::message filegone
    return
  }
  set mtime [file mtime $::te(filename)]
  switch $args {
    start {
      set ::te(mtime) $mtime
      set ::te(md5sum) [eval exec md5sum $::te(filename)]
    }
    stop  {return; # no reschedule}
    default {
      if {$mtime > $::te(mtime)} {
        set ::te(mtime) $mtime
        set md5sum $::te(md5sum)
        set ::te(md5sum) [eval exec md5sum $::te(filename)]
        # no message if file contents unchanged
        if {"$md5sum" != "$::te(md5sum)"} {
          ::tooledit::message changed
        }
      }
    }
  }
  set ::te(afterid) [after 2000 ::tooledit::watch]
} ;# watch

proc ::tooledit::tooledit {filename} {
  set ::te(filename) $filename
  set ::te(types)        {mill lathe}
  set ::te(mill,header)  {poc fms tlo diameter comment}
  set ::te(lathe,header) {poc fms zoffset xoffset diameter \
                         frontangle backangle orient comment}
  set ::te(poc,width)         4
  set ::te(fms,width)         4
  set ::te(tlo,width)        10
  set ::te(diameter,width)   10
  set ::te(comment,width)    20
  set ::te(zoffset,width)    10
  set ::te(xoffset,width)    10
  set ::te(frontangle,width) 12
  set ::te(backangle,width)  12
  set ::te(orient,width)      7
  foreach h $::te(mill,header)  {set ::te($h) [string toupper $h]}
  foreach h $::te(lathe,header) {set ::te($h) [string toupper $h]}

  set ::te(top) [toplevel .tooledit]
  wm protocol $::te(top) WM_DELETE_WINDOW ::tooledit::bye
  wm title $::te(top) "tooledit: [file tail $::te(filename)]"
  if [info exists ::priv(tooledit,geometry)] {
    wm geometry $::te(top) $::priv(tooledit,geometry)
  }

  # frame for headers & entries
  foreach type $::te(types) {
    set f [frame $::te(top).$type] ;# pack deferred until used
    set ::te($type,frame) $f
  }

  # message frame (pack from bottom)
  set f [frame $::te(top).[qid]]
  pack $f -side bottom -expand 1 -fill x

  set msg [label $f.[qid] -anchor w]
  set ::te(msg,widget) $msg
  pack $msg -side top -expand 1 -fill x -anchor w

  set readstatus [readfile $filename]
  switch $readstatus {
    0 {
      # no existing file, create empty entries:
      makeline mill  new
      makeline lathe new
      message newfile
    }
    1 {message opened}
    default {
      if [info exists ::te(standalone)] {
        puts stderr "$readstatus"
        exit
      } else {
        return -code error $readstatus
      }
    }
  }
  if [file exists $::te(filename)] {watch start}

  # button frame (pack from bottom)
  set f [frame $::te(top).[qid]]
  pack $f -side bottom -expand 1 -fill x

  set   bb [button $f.[qid] -text "Delete items"\
           -command {::tooledit::deleteline}]
  pack $bb -side left -fill none -expand 0
  set ::te(deletebutton) $bb
  checkdelete

  pack [button $f.am -text "Add Mill Tool" \
       -command {::tooledit::makeline mill new}] -side left -fill x -expand 1
  pack [button $f.[qid] -text "Add Lathe Tool" \
       -command {::tooledit::makeline lathe new}] -side left -fill x -expand 1
  pack [button $f.[qid] -text "Reload File" \
       -command  ::tooledit::toolreread] -side left -fill x -expand 1
  pack [button $f.[qid] -text "Check Entries" \
       -command [list ::tooledit::toolvalidate]] -side left -fill x -expand 1
  pack [button $f.[qid] -text "Write Tool Table" \
       -command [list ::tooledit::writefile $::te(filename)]] \
       -side left -fill x -expand 1
  pack [button $f.[qid] -text Dismiss \
       -command ::tooledit::bye] \
       -side left -fill x -expand 1

} ;# tooledit

proc ::tooledit::message {type} {
  if ![info exists ::te(msg,widget)] return
  set w $::te(msg,widget)
  set dt [clock format [clock seconds]]
  switch $type {
    opened   {$w conf -text "$dt: Opened $::te(filename)"      -fg darkblue}
    newfile  {$w conf -text "$dt: Created $::te(filename)"     -fg darkblue}
    write    {$w conf -text "$dt: File updated"                -fg green4}
    modified {$w conf -text "$dt: File modified"               -fg darkred}
    checke   {$w conf -text "$dt: File check errors"           -fg red}
    checkok  {$w conf -text "$dt: File checked"                -fg darkgreen}
    delete   {$w conf -text "$dt: File items deleted"          -fg cyan4}
    bogus    {$w conf -text "$dt: Bogus lines in file ignored" -fg darkorange}
    verror   {$w conf -text "$dt: File errors"                 -fg red}
    changed  {$w conf -text "$dt: Warning: File changed by another process" -fg red}
    filegone {$w conf -text "$dt: Warning: File deleted by another process" -fg red}
  }
} ;# message

proc ::tooledit::deleteline {} {
  set dct 0
  foreach type $::te(types) {
    catch {unset dlines}
    foreach item [array names ::te "$type,*,deleteme"] {
      if {$::te($item) == 1} {
        set i1 [expr  1 + [string first , $item]]
        set i2 [expr -1 + [string last  , $item]]
        lappend dlines [string range $item $i1 $i2]
      }
    }
    if ![info exists dlines] continue
    foreach i $dlines {
      destroy $::te($type,$i,frame); unset ::te($type,$i,frame)
      incr dct

      if [info exists ::te($type,items)] {
        set idx [lsearch $::te($type,items) $i]
        if {$idx >= 0} {
          set ::te($type,items) [lreplace $::te($type,items) $idx $idx]
        }
        if {[string length $::te($type,items)] == 0} {
          unset ::te($type,items)
        }
      }

      foreach name [array names ::te $type,$i,*] {
        unset ::te($name)
      }
    }
  }
  checkdelete
  if {$dct >0}  { message delete}
} ;# deleteline

proc ::tooledit::makeline {type ay_name} {
  # ay(0)...ay(8) ==> entry line items
  if {"$ay_name" == "new"} {
    set ay(0) NEW
    set date "# Added [clock format [clock seconds] -format %Y%m%d]"
    switch $type {
      mill  {set ay(1) 0;set ay(2) 0;set ay(3) 0;set ay(4) $date}
      lathe {set ay(1) 0;set ay(2) 0;set ay(3) 0;set ay(4) 0
             set ay(5) 0;set ay(6) 0;set ay(7) 0;set ay(8) $date
      }
    }
  } else {
    upvar $ay_name ay
  }

  switch $type {
    lathe {
      if ![info exists ::te(lathe,i)] {set ::te(lathe,i) 0}
      set i $::te(lathe,i)
      pack $::te($type,frame) -side top -fill both -expand 1
      if ![info exists ::te(lathe,header,frame)] {
        set f [frame $::te($type,frame).${i}lathe,header]
        set ::te(lathe,header,frame) $f
        pack $f -side top -expand 0 -fill x
        pack [label $f.b -text Del -width 3] -side left -expand 0
        foreach h $::te(lathe,header) {
          set e 0;set j right
          if {"$h" == "comment"} {set e 1;set j left}
          pack [entry $f.$i$h -justify $j -textvariable ::te($h) \
               -state disabled -relief groove \
               -disabledforeground black \
               -width $::te($h,width)] -side left -fill x -expand $e
        }
      }
      set f [frame $::te($type,frame).data$i]
      set ::te(lathe,$i,frame) $f
      pack $f -side top -expand 0 -fill x
      lappend ::te(lathe,items) $i
      set ::te(lathe,$i,poc)        $ay(0)
      set ::te(lathe,$i,fms)        $ay(1)
      set ::te(lathe,$i,zoffset)    $ay(2)
      set ::te(lathe,$i,xoffset)    $ay(3)
      set ::te(lathe,$i,diameter)   $ay(4)
      set ::te(lathe,$i,frontangle) $ay(5)
      set ::te(lathe,$i,backangle)  $ay(6)
      set ::te(lathe,$i,orient)     $ay(7)
      set ::te(lathe,$i,comment)    [string trim $ay(8)]
      pack [checkbutton $f.b -variable ::te(lathe,$i,deleteme)\
           -command "::tooledit::checkdelete"] -side left -expand 0
      foreach h $::te(lathe,header) {
        set e 0;set j right;set v 1
        if {"$h" == "comment"} {set e 1; set j left;set v 0}
        set ve [ventry $f $v ::te(lathe,$i,$h) $::te($h,width) $e $j]
        if {"$ay(0)" == "NEW" && "$h" == "poc"} {set vefocus $ve}
        entrybindings lathe $ve $h $i
      }
      incr ::te(lathe,i)
    }
    mill {
      if ![info exists ::te(mill,i)] {set ::te(mill,i) 0}
      set i $::te(mill,i)
      pack $::te($type,frame) -side top -fill both -expand 1
      if ![info exists ::te(mill,header,frame)] {
        set f [frame $::te($type,frame).${i}mill,header]
        set ::te(mill,header,frame) $f
        pack $f -side top -expand 0 -fill x
        pack [label $f.b -text Del -width 3] -side left -expand 0
        foreach h $::te(mill,header) {
          set e 0;set j right
          if {"$h" == "comment"} {set e 1;set j left}
          pack [entry $f.$i$h -justify $j -textvariable ::te($h) \
               -state disabled -relief groove \
               -disabledforeground black \
               -width $::te($h,width)] -side left -fill x -expand $e
        }
      }
      set f [frame $::te($type,frame).data$i]
      set ::te(mill,$i,frame) $f
      pack $f -side top -expand 0 -fill x
      lappend ::te(mill,items) $i
      set ::te(mill,$i,poc)      $ay(0)
      set ::te(mill,$i,fms)      $ay(1)
      set ::te(mill,$i,tlo)      $ay(2)
      set ::te(mill,$i,diameter) $ay(3)
      set ::te(mill,$i,comment)  [string trim $ay(4)]
      pack [checkbutton $f.b -variable ::te(mill,$i,deleteme) \
           -command "::tooledit::checkdelete"] -side left -expand 0
      foreach h $::te(mill,header) {
        set e 0;set j right;set v 1
        if {"$h" == "comment"} {set e 1;set j left;set v 0}
        set ve [ventry $f $v ::te(mill,$i,$h) $::te($h,width) $e $j]
        if {"$ay(0)" == "NEW" && "$h" == "poc"} {set vefocus $ve}
        entrybindings mill $ve $h $i
      }
      incr ::te(mill,i)
    }
    default {return -code error "::tooledit::makeline:<unknown type <$type>"}
  }
  if [info exists vefocus] {
    set ::te(restore,selectbackground) [$vefocus cget -selectbackground]
    set ::te(restore,selectforeground) [$vefocus cget -selectforeground]
    $vefocus configure -selectbackground white
    $vefocus configure -selectforeground red
    $vefocus selection to end
    focus $vefocus
  }
} ;# makeline

proc ::tooledit::entrybindings {type e h i} {
  $e conf -takefocus 1
  set ::te($type,$i,$h,entry) $e
  bind $e <Key-Up>    "::tooledit::bindactions $type $h $i %K"
  bind $e <Key-Down>  "::tooledit::bindactions $type $h $i %K"
  bind $e <Key-Left>  "::tooledit::bindactions $type $h $i %K"
  bind $e <Key-Right> "::tooledit::bindactions $type $h $i %K"
} ;# entrybindings

proc ::tooledit::bindactions {type h i key args} {
  set nexth $h;set nexti $i;
  switch $key {
    Up {
      set nexti [expr $i -1]
      if {$nexti <0} {
        set nexti [expr $::te($type,i) -0]
        after 0 [list ::tooledit::bindactions $type $h $nexti $key]
        return
      }
    }
    Down {
      set nexti [expr $i + 1]
      if {$nexti >= $::te($type,i)} {
        set nexti -1
        after 0 [list ::tooledit::bindactions $type $h $nexti $key]
        return
      }
    }
    Right {
      if {"$h" == "nosuch"} {
        set nextidx 0
      } else {
        set idx [lsearch $::te($type,header) $h]
        set nextidx [expr $idx + 1]
        if {$nextidx >= [llength $::te($type,header)]} {
          after 0 [list ::tooledit::bindactions $type nosuch $nexti $key]
          return
        }
      }
      set nexth [lindex $::te($type,header)  $nextidx]
    }
    Left {
      if {"$h" == "nosuch"} {
        set nextidx [expr [llength $::te($type,header)] -1]
      } else {
        set idx [lsearch $::te($type,header) $h]
        set nextidx [expr $idx + -1]
        if {$nextidx < 0} {
          after 0 [list ::tooledit::bindactions $type nosuch $nexti $key]
          return
        }
      }
      set nexth [lindex $::te($type,header)  $nextidx]
    }
  }
  if [info exists ::te($type,$nexti,$nexth,entry)] {
    $::te($type,$nexti,$nexth,entry) selection to end
    focus $::te($type,$nexti,$nexth,entry)
  } else {
    # frame has been deleted
    switch $key {
      Up - Down {
        set nexti [expr $nexti + 0]
        after 0 [list ::tooledit::bindactions $type $h $nexti $key]
        return
      }
    }
  }
} ;# bindactions

proc ::tooledit::checkdelete {} {
  set ct 0
  foreach name [array names ::te *,deleteme] {
    if {$::te($name) == 1} {incr ct}
  }
  if {$ct > 0} {
    $::te(deletebutton) conf -fg red   -state normal
  } else {
    $::te(deletebutton) conf -fg black -state disabled
  }
  focus $::te(deletebutton)
} ;# checkdelete

proc ::tooledit::toolreread {} {
  set geo [wm geometry $::te(top)]
  set loc [string range $geo [string first + $geo] end]
  set ::priv(tooledit,geometry) $loc

  set filename $::te(filename)
  destroy $::te(top)
  unset ::te
  after 0 ::tooledit::tooledit $filename
} ;# toolreread

proc ::tooledit::writefile {filename} {
  if [toolvalidate] return ;# failed validation
  if [file exists $filename] {
    set backup $filename.bak
    file rename -force $filename $backup
  }

  set fd [open $filename w]

  # header lines
  if {[info exists ::te(mill,items)] && [info exists ::te(lathe,items)]} {
    puts $fd "File includes both mill(4-5 columns) and lathe(8-9 columns) entries"
  }
  if [info exists ::te(mill,items)] {
    foreach h $::te(mill,header) {
      set j ""
      if {"$h" == "comment"} {set j -}
      puts -nonewline $fd [format " %$j$::te($h,width)s" [string toupper $h]]
    }
    puts $fd "" ;# new line
  }
  if [info exists ::te(lathe,items)] {
    foreach h $::te(lathe,header) {
      set j ""
      if {"$h" == "comment"} {set j -}
      puts -nonewline $fd [format " %$j$::te($h,width)s" [string toupper $h]]
    }
    puts $fd "" ;# new line
  }
  puts $fd "" ;# blank line ends header data (per user manual 7.4)

  foreach type $::te(types) {
    if [info exists ::te($type,items)] {
      foreach i $::te($type,items) {
        foreach h $::te($type,header) {
          set j ""
          set w $::te($h,width)
          if {"$h" == "comment"} {
            set j -;set w ""
            set ::te($type,$i,$h) [fixcomment $::te($type,$i,$h)]
          }
          puts -nonewline $fd [format " %$j${w}s" $::te($type,$i,$h)]
        }
        puts $fd "" ;# new line
      }
    }
  }
  watch stop
  close $fd
  watch start
  message write
} ;# writefile

proc ::tooledit::fixcomment {c} {
  # add leading "# " to nonnull strings
  set c [string trim $c]
  if {"$c" == ""} {return $c}
  if {[string first "#" $c] == 0} {return $c}
  set c "# $c"
  return $c
} ;# fixcomment

proc ::tooledit::toolvalidate {} {
  set msg ""

  # common (lathe or mill) checks
  foreach type $::te(types) {
    if [info exists ::te($type,items)] {
      foreach i $::te($type,items) {
        foreach h $::te($type,header) {
          if {"$h" == "comment"} continue
          if ![isnumber $::te($type,$i,$h)] {
            set nextmsg "Pocket $::te($type,$i,poc): $h value=$::te($type,$i,$h) is not a number"
            if {[lsearch $msg $nextmsg] >= 0} continue
            lappend msg $nextmsg
          }
          if {   ("$h" == "poc" || "$h" == "fms")
              && (![isinteger $::te($type,$i,$h)] || [isnegative $::te($type,$i,$h)]) } {
             lappend msg "Pocket $::te($type,$i,poc): $h must be nonnegative integer"
          }
        }
      }
    }
  }

  # lathe-specific checks
  if [info exists ::te(lathe,items)] {
    foreach i $::te(lathe,items) {
      foreach h $::te(lathe,header) {
        if {"$h" == "comment"} continue
        if {   "$h" == "orient"
             && [lsearch {0 1 2 3 4 5 6 7 8 9} $::te(lathe,$i,$h)] < 0} {
          lappend msg "Pocket $::te(lathe,$i,poc): Orientation must be 0..9 integer"
        }
        if {   ("$h" == "frontangle" || "$h" == "backangle")
            && ![validangle $::te(lathe,$i,$h)] } {
           lappend msg "Pocket $::te(lathe,$i,poc): $h must be between  -360 and 360"
        }
      }
    }
  }

  # check for multiple uses of a single pocket
  foreach type $::te(types) {
    if ![info exists ::te($type,items)] continue
    set pocs ""
    foreach i $::te($type,items) {
      set p $::te($type,$i,poc)

      if {[lsearch $pocs $p] >= 0} {
        set nextmsg "$type pocket $p specified multiple times"
        if {[lsearch $msg $nextmsg] >= 0} continue
        lappend msg $nextmsg
      } else {
        lappend pocs $p
      }
    }
  }

  if {"$msg" != ""} {
    showerr $msg
    message checke
    return 1 ;#fail
  }
  message checkok
  return 0
} ;# toolvalidate

proc ::tooledit::showerr {msg} {
  set w .showerr
  catch {destroy $w}
  set w [toplevel $w]
  set l [label $w.l -justify left]
  set text ""
  set msg [lsort $msg]
  foreach item $msg {set text "$text\n$item"}
  $l configure -text $text
  pack $l -side top
  set b [button $w.b -text Dismiss -command "destroy $w"]
  pack $b -side top
  focus $b
  wm withdraw $w
  wm title    $w Error
  update idletasks
  set x [expr [winfo screenwidth $w]/2 \
            - [winfo reqwidth $w]/2  - [winfo vrootx [winfo parent $w]]]
  set y [expr [winfo screenheight $w]/2 \
            - [winfo reqheight $w]/2  - [winfo vrooty [winfo parent $w]]]
  wm geom $w +$x+$y
  wm deiconify $w
} ;# showerr

proc ::tooledit::bye {} {
  destroy $::te(top)       ;# for embedded usage
  set ::tooledit::finis 1 ;# for standalone usage
} ;# bye

#------------------------------------------------------------------------
if {[info script] == $argv0} {
  # configure for standalone usage:
  set ::te(standalone) 1
  wm withdraw .
  if {[lindex $argv 0] == ""} {
    puts stderr "Usage: $::argv0 filename"
    exit 1
  }
  ::tooledit::tooledit [lindex $argv 0] ;# arg: filename
  tkwait variable ::tooledit::finis
  exit 0
}

