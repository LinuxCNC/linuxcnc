#!/usr/bin/wish
#
# Copyright: 2009-2010
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

# Supports tool offsets along all axes (x y z a b c u v w)
# Older tool table formats are not supported
# No distinction is made between mill or lathe tools
# Text on a line following a semicolon (;) is treated as comment
# Comment-only lines are preserved

package require BWidget ;# for ScrolledWindow, ScrollableFrame

namespace eval ::tooledit {
  namespace export tooledit ;# public interface
}

proc ::tooledit::init {} {
  set ::te(fmt,int)   %d
  set ::te(fmt,real)  %g
  set ::te(fmt,angle) %f
  set ::te(msg,last)  ""
  set ::te(pollms)    2000

  set ::te(types)        {unified}
  set ::te(unified,width)     0 ;# initial width as reqd
  set ::te(unified,height)  100 ;# initial height limit here
  set ::te(unified,hincr)    10 ;# height increment to bump scrollable size
  set ::te(unified,header)  {tool poc x y z a b c u v w \
                          diam front back orien comment}

  # include values for each *,header item:
  set ::te(tool,width)     5; set ::te(tool,tag)     T
  set ::te(poc,width)      5; set ::te(poc,tag)      P
  set ::te(x,width)        7; set ::te(x,tag)        X
  set ::te(y,width)        7; set ::te(y,tag)        Y
  set ::te(z,width)        7; set ::te(z,tag)        Z
  set ::te(a,width)        7; set ::te(a,tag)        A
  set ::te(b,width)        7; set ::te(b,tag)        B
  set ::te(c,width)        7; set ::te(c,tag)        C
  set ::te(u,width)        7; set ::te(u,tag)        U
  set ::te(v,width)        7; set ::te(v,tag)        V
  set ::te(w,width)        7; set ::te(w,tag)        W
  set ::te(diam,width)     7; set ::te(diam,tag)     D
  set ::te(front,width)    7; set ::te(front,tag)    I
  set ::te(back,width)     7; set ::te(back,tag)     J
  set ::te(orien,width)    5; set ::te(orien,tag)    Q
  set ::te(comment,width)  0; set ::te(comment,tag) \;
  # note: width 0 expands with text in entry widget
  #       when using Bwidget scrollable frame
} ;# init

proc ::tooledit::validangle {v} {
  if {[string trim $v] == ""} {return 1} ;# allow null value
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
  if {[string trim $v] == ""} {return 1} ;# allow null value
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
   if {"$new" == ""} {return 1}
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
  set lno 1
  while {1} {
    gets $fd newline
    incr lno
    if [eof $fd] break
    foreach item {t p x y z a b c u v w d i j q comment} {
      set u($item) ""
    }
    set newline [string tolower $newline]
    set i1 [string first \; $newline]
    if {$i1 >= 0} {
      set u(comment) [string range $newline [expr $i1 +1] end]
      set u(comment) [string trim $u(comment)]
      set newline    [string range $newline 0 [expr -1 + $i1]]
      set newline    [string trim $newline]
    }

    if {"$newline" == ""} {
      lappend ::te(global,comments) $u(comment)
      continue
    }

    set bogus 0
    foreach tagvalue [split [string trim $newline]] {
      set tagvalue [string trim $tagvalue]
      if {"$tagvalue" == ""} continue
      set tag   [string range $tagvalue 0 0   ]
      set value [string range $tagvalue 1 end ]
      if ![isnumber $value] {
        puts stderr "Skipping linenumber $lno: not a number for tag=$tag <$value>"
        incr bct; set bogus 1
      }
      switch $tag {
        t - p - q {  if ![isinteger $value] {
                   puts stderr "Skipping linenumber $lno: expected integer $tag $value"
                   incr bct; set bogus 1
                 }
              }
      }
      # catch errors since format is already checked
      # (line will not be displayed)
      # this allows all errors on a line to be flagged in one pass
      switch $tag {
        t - p - q   {catch {set u($tag) [format "$::te(fmt,int)" $value]}}
        x - y - z -
        a - b - c -
        u - v - w -
        d           {catch {set u($tag) [format "$::te(fmt,real)"  $value]}}
        i - j       {catch {set u($tag) [format "$::te(fmt,angle)" $value]}}
        default     {puts stderr "At linenumber $lno: unknown tag <$tag>"
                     incr bct; set bogus 1
                    }
      }
    }
    if $bogus continue
    makeline unified u
  } ;# while
  close $fd
  if {$bct >0} {
    # schedule message after message widget is created
    after 0 {::tooledit::message bogus}
  }
  return 1 ;# ok
} ;# readfile

proc ::tooledit::watch {args} {
  catch {after cancel $::te(afterid)}
  if ![file exists $::te(filename)] {
    ::tooledit::message filegone
    set ::te(afterid) [after $::te(pollms) ::tooledit::watch]
    return
  }
  set mtime [file mtime $::te(filename)]
  switch $args {
    start {
      set ::te(mtime) $mtime
      set ::te(md5sum) [eval exec md5sum $::te(filename)]
    }
    stop  {return}
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

  # try to clear error display in case user clears error before check
  #   skip if newtool since error is annoying
  #   skip if filegone|changed|bogus since important
  if {   "$::te(msg,last)" != "newtool"
      && "$::te(msg,last)" != "filegone"
      && "$::te(msg,last)" != "changed"
      && "$::te(msg,last)" != "bogus"
     } {
    ::tooledit::toolvalidate silent ;# to clear errors
  }
  set ::te(afterid) [after $::te(pollms) ::tooledit::watch]
} ;# watch

proc ::tooledit::tooledit {filename} {
  package require Tk
  init
  set ::te(filename) $filename

  foreach h $::te(unified,header)  {set ::te($h) [string toupper $h]}

  set ::te(top) [toplevel .tooledit]
  wm resizable $::te(top) 1 1
  wm protocol $::te(top) WM_DELETE_WINDOW ::tooledit::bye
  wm title $::te(top) "tooledit: [file tail $::te(filename)]"
  if [info exists ::te(tooledit,geometry)] {
    wm geometry $::te(top) $::te(tooledit,geometry)
  }

  # frame for headers & entries
  foreach type $::te(types) {
    set ::te($type,hframe) [frame $::te(top).[qid]]
    pack $::te($type,hframe) -side top -expand 0 -fill x -anchor nw



    # note: dont pack ::te($type,sframe), handled by ScrolledWindow
    set ::te($type,sw)  [ScrolledWindow  $::te(top).$type -auto both]
    set ::te($type,sframe) [ScrollableFrame $::te(top).$type.sff \
             -height $::te($type,height) -width $::te($type,width)]
    $::te($type,sw) setwidget $::te($type,sframe) ;# associates scrollbars
    set ::te($type,frame) [$::te($type,sframe) getframe] ;# this is parent
  }

  set readstatus [readfile $filename]
  switch $readstatus {
    0 {
      # no existing file, create empty entries:
      makeline unified new
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
  pack $f -side top -expand 0 -fill both -anchor nw

  set   bb [button $f.[qid] -text "Delete items"\
           -command {::tooledit::deleteline}]
  pack $bb -side left -fill y -expand 0
  set ::te(deletebutton) $bb
  checkdelete

  pack [button $f.[qid] -text "Add Tool" \
       -command {::tooledit::makeline unified new}] -side left -fill x -expand 1
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

  set f [frame $::te(top).[qid]]
  pack $f -side top -expand 0 -fill x

  set msg [label $f.msg -anchor w]
  set ::te(msg,widget) $msg
  pack $msg -side top -expand 0 -fill x -anchor w

  set f [frame $::te(top).dummy -height 1] ;# unused widget for expansion
  pack $f -side top -expand 0 -fill x

  update ;# wait for display before binding Configure events
  set geo [wm geometry $::te(top)]
  set ::te(top,geometry) $geo
  set ::te(top,height) [string range $geo [expr  1 + [string first x $geo]] \
                                        [expr -1 + [string first + $geo]] ]
  # set min width so it can be disappeared inadvertently
  # set min height to initial
  wm minsize $::te(top) 100 $::te(top,height)

  bind $::te(top) <Configure> {::tooledit::configure %W %w %h}
} ;# tooledit

proc ::tooledit::configure {W w h} {
  if {"$W" == $::te(top) && $::te(top,geometry) != [wm geometry $::te(top)]} {
    set ::te(top,geometry) [wm geometry $::te(top)]
    set deltah [expr $h - $::te(top,height)]
    set fsize [$::te(unified,sframe) cget -height]
    if {[expr abs($deltah)] > $::te(unified,hincr)} {
      $::te(unified,sframe) configure -height [expr $fsize + $deltah]
      set ::te(top,height) $h
    }
  }
} ;# configure

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
    verror   {$w conf -text "$dt: File errors -- Check Entries"   -fg red}
    changed  {$w conf -text "$dt: Warning: File changed by another process" -fg red}
    filegone {$w conf -text "$dt: Warning: File deleted by another process" -fg red}
    newtool  {$w conf -text "$dt: Added Tool" -fg green4
                 update idletasks
                 $::te(unified,sframe) yview moveto 1.0
             }
  }
  set ::te(msg,last) $type
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
  if {"$ay_name" == "new"} {
    set new 1
    set date "Added [clock format [clock seconds] -format %Y%m%d]"

    switch $type {
      unified  {
        foreach item {t p x y z a b c u v w d i j q} {
          set ay($item) ""
        }
        set ay(p) NEW
        set ay(t) NEW
        set ay(comment) "$date"
        after 0 {::tooledit::message newtool}
      }
    }
  } else {
    upvar $ay_name ay
  }
  switch $type {
    unified  {
      if ![info exists ::te(unified,i)] {set ::te(unified,i) 0}
      set i $::te(unified,i)
      if ![info exists ::te(unified,header,frame)] {
        set f [frame $::te($type,hframe).${i}unified,header]
        set ::te(unified,header,frame) $f
        pack $f -side top -expand 1 -fill x -anchor n
        pack [label $f.b -text Del -width 3] -side left -expand 0
        foreach h $::te(unified,header) {
          set e 0;set j center
          if {"$h" == "comment"} {set e 1;set j left}
          pack [entry $f.$i$h -justify $j -textvariable ::te($h) \
               -state disabled -relief groove \
               -disabledforeground black \
               -width $::te($h,width)] -side left -fill x -expand $e
        }
      }
      set f [frame $::te($type,frame).data$i]
      set ::te(unified,$i,frame) $f
      pack $f -side top -expand 1 -fill x -anchor n
      lappend ::te(unified,items) $i
      set ::te(unified,$i,tool)      $ay(t)
      set ::te(unified,$i,poc)       $ay(p)
      set ::te(unified,$i,x)         $ay(x)
      set ::te(unified,$i,y)         $ay(y)
      set ::te(unified,$i,z)         $ay(z)
      set ::te(unified,$i,a)         $ay(a)
      set ::te(unified,$i,b)         $ay(b)
      set ::te(unified,$i,c)         $ay(c)
      set ::te(unified,$i,u)         $ay(u)
      set ::te(unified,$i,v)         $ay(v)
      set ::te(unified,$i,w)         $ay(w)
      set ::te(unified,$i,diam)      $ay(d)
      set ::te(unified,$i,front)     $ay(i)
      set ::te(unified,$i,back)      $ay(j)
      set ::te(unified,$i,orien)     $ay(q)
      set ::te(unified,$i,comment)   [string trim $ay(comment)]
      pack [checkbutton $f.b -variable ::te(unified,$i,deleteme)\
           -command "::tooledit::checkdelete"] -side left -expand 0
      foreach h $::te(unified,header) {
        set e 0;set j right;set v 1
        if {"$h" == "comment"} {set e 1; set j left;set v 0}
        set ve [ventry $f $v ::te(unified,$i,$h) $::te($h,width) $e $j]
        if {[info exists new] && "$h" == "tool"} {set vefocus $ve}
        entrybindings unified $ve $h $i
      }
      incr ::te(unified,i)
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
  pack $::te($type,sw) -side top -fill x -expand 0 -anchor nw
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

  set filename $::te(filename)
  destroy $::te(top)
  unset ::te
  set ::te(tooledit,geometry) $loc
  after 0 ::tooledit::tooledit $filename
} ;# toolreread

proc ::tooledit::writefile {filename} {
  if [toolvalidate] return ;# failed validation
  if [file exists $filename] {
    set backup $filename.bak
    file rename -force $filename $backup
  }

  set fd [open $filename w]

  if [info exists ::te(global,comments)] {
    foreach c $::te(global,comments) {
      puts $fd ";$c"
    }
  }

  foreach type $::te(types) {
    if [info exists ::te($type,items)] {
      foreach i $::te($type,items) {
        foreach h $::te($type,header) {
          set j ""
          set w $::te($h,width)
          # correct entries with leading zeros
          if {[string first 0 [string trim $::te($type,$i,$h)]] == 0} {
             set ::te($type,$i,$h) [format %g $::te($type,$i,$h)]
          }
          set value [string trim $::te($type,$i,$h)]
          if {"$value" != ""} {
            puts -nonewline $fd "$::te($h,tag)$value "
          }
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

proc ::tooledit::toolvalidate {args} {
  set msg ""
  set silent 0
  if {"$args" == "silent"} {set silent 1}

  foreach type $::te(types) {
    if [info exists ::te($type,items)] {
      foreach i $::te($type,items) {
        foreach h $::te($type,header) {
          if {"$h" == "comment"} continue
          if ![isnumber $::te($type,$i,$h)] {
            set nextmsg "Tool $::te($type,$i,tool): $h value=$::te($type,$i,$h) is not a number"
            if {[lsearch $msg $nextmsg] >= 0} continue
            lappend msg $nextmsg
          }

          switch -glob $h {
            tool* - poc* {
              if {![isinteger $::te($type,$i,$h)] || [isnegative $::te($type,$i,$h)]} {
                 lappend msg "Tool $::te($type,$i,tool): $h must be nonnegative integer"
              }
            }
            orien* {
              if {   "$::te($type,$i,$h)" != "" \
                  && [lsearch {0 1 2 3 4 5 6 7 8 9} $::te($type,$i,$h)] < 0} {
                lappend msg "Tool $::te($type,$i,tool): Orientation must be 0..9 integer"
              }
            }
            front* - back* {
              if {![validangle $::te($type,$i,$h)] } {
                lappend msg "Tool $::te($type,$i,tool): $h must be between -360 and 360"
              }
            }
          }
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
        set nextmsg "Pocket $p specified multiple times"
        if {[lsearch $msg $nextmsg] >= 0} continue
        lappend msg $nextmsg
      } else {
        lappend pocs $p
      }
    }
  }
  # check for multiple uses of a single tool
  foreach type $::te(types) {
    if ![info exists ::te($type,items)] continue
    set tools ""
    foreach i $::te($type,items) {
      set t $::te($type,$i,tool)

      if {[lsearch $tools $t] >= 0} {
        set nextmsg "Tool $t specified multiple times"
        if {[lsearch $msg $nextmsg] >= 0} continue
        lappend msg $nextmsg
      } else {
        lappend tools $t
      }
    }
  }

  if {"$msg" != ""} {
    if {!$silent} {showerr $msg}
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
  catch {after cancel $::te(afterid)}
  destroy $::te(top)      ;# for embedded usage
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

