#!/usr/bin/wish
#
# Copyright: 2009-2012
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
#        tooleditor.tcl [column_1 ... column_n] filename
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

#-----------------------------------------------------------------------
# Internationalization

# use the tcl-package named Emc to set up I18n support
if [catch {package require Linuxcnc} msg] {
  # if user is trying to use as standalone in an unconfigured (non-Emc)
  # environment, just continue without internationalization
  puts stderr "Internationalization not available: <$msg>"
}
# use a command or proc named "_" for ::msgcat::mc
# when embedded in axis, a command named "_" is predefined,
# since "_" is not defined for standalone usage, make a proc named "_"
if {"" == [info command "_"]} {
  package require msgcat
  proc _ {s} {return [::msgcat::mc $s]}
}
#-----------------------------------------------------------------------


proc ::tooledit::init { {columns ""} } {
  if [file readable ~/.tooleditrc] {
    if [catch {source ~/.tooleditrc} msg] {
      puts stderr "[_ "Problem reading ~/.tooleditrc"]:"\n$msg"
    }
    if [info exists geometry] {
      set ::te(top,restore,geometry) $geometry
    }
  }
  if {"$columns" == ""} {
    set columns $::te(allcolumns)
  } else {
    set columns [string tolower $columns]
    set ::te(user_specified_columns) $columns
  }

  # disallow duplicate columns (order according to first occurrence)
  set checked_columns {}
  foreach cname $columns {
    if {[lsearch $checked_columns $cname] >= 0} {
       puts stderr [format [_ "Note: Ignoring duplicate column name: %s"] $cname]
       continue ;# duplicate col name
    }
    lappend checked_columns $cname
  }

  # include only allowed column names:
  foreach cname $checked_columns {
    if {[lsearch $::te(allcolumns) $cname] >= 0} {
      lappend ::te(columns) $cname
    } else {
      puts stderr [format [_ "Unknown column: %s"] $cname]
    }
  }

  set ::te(filemod) 0
  set ::te(fmt,int)   %d
  set ::te(fmt,real)  %g
  set ::te(fmt,angle) %f
  set ::te(msg,last)  ""
  set ::te(pollms)    2000

  set ::te(initial,width)     0 ;# initial width as reqd
  set ::te(initial,height)  110 ;# initial height limit here
  set ::te(hincr)             1 ;# height increment to bump scrollable size

  set ::te(autocolumns) {tool poc}
  set ::te(header) [concat $::te(autocolumns) $::te(columns) comment]

  foreach item $::te(header) { set ::te(type,$item) real ;# default }
  set ::te(type,tool)    integer
  set ::te(type,poc)     integer
  set ::te(type,orien)   integer
  set ::te(type,comment) ascii

  # include values for each (header) item:
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
  set ::te(orien,width)    6; set ::te(orien,tag)    Q
  set ::te(comment,width)  20; set ::te(comment,tag) \;
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
          -bg white \
          -validate        all \
          -validatecommand [list ::tooledit::validateNumber $tvar %W %s %P] \
          -invalidcommand  [list ::tooledit::invalidNumber  $tvar %W] \
         ]
     pack $e -side left -expand $expand -fill $fill
  } else {
    set e [entry $f.[qid] \
          -width $twidth -relief sunken -justify $justify\
          -textvariable $tvar \
          -bg white \
          -validate all \
          -validatecommand [list ::tooledit::validateOther $tvar %W %s %P] \
         ]
    pack $e -side left -expand $expand -fill $fill
  }
  return $e
} ;# ventry

proc ::tooledit::validateOther {varname widget current new} {
   if {"$current" != $new} {
     incr ::te(filemod)
   }
   return 1 ;# 1==>ok
} ;# validateOther

proc ::tooledit::validateNumber {varname widget current new} {
   if ![info exists $varname] {return 1}
   if {"$new" == ""} {return 1}
   if {"$new" == "[_ "NEW"]"} {
     return 1 ;# 1==>ok dont flag items tagged "NEW"
   }
   if {"$current" == "[_ "NEW"]"} {
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
     incr ::te(filemod)
     return 1 ;# 1==>ok
   }
} ;# validateNumber

proc ::tooledit::invalidNumber {varname widget} {
  tk_dialog .problem \
            Problem \
            "[format [_ "%s must be a number"] $varname]" \
            {} \
            0 \
            ok
  $widget configure -validate all ;# restore validation
} ;# invalidNumber

proc ::tooledit::readfile {filename} {
  if {[file exists $filename] && ![file readable $filename]} {
    lappend msg [format [_ "filename: <%s> not readable"] $filename]
  }
  if [file exists $filename] {
    if ![file writable $filename] {
      lappend msg "[format [_ "filename: <%s> not writable"] $filename]"
    }
  } else {
    set new 1
    if ![file writable [file dirname $filename]] {
      lappend msg "[format [_ "directory: <%s> not writable"] $filename]"
    }
  }

  if [info exists msg] {return -code error $msg}
  if [info exists new] {
    makeline new
    incr ::te(filemod)
    message newfile
    return
  }

  set fd [open $filename r]

  set bct 0
  set lno 0
  while {1} {
    gets $fd newline
    incr lno ;# starts at 1
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
        puts stderr [format [_ "Skipping linenumber %d for tag %s, value <%s> is not a number"] \
                    $lno $tag $value]
        incr bct; set bogus 1
      }
      switch $tag {
        t - p - q {  if ![isinteger $value] {
                   puts stderr [format [_ "Skipping linenumber %d for tag %s, expected integer not <%s>"] \
                               $lno $tag $value]
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
        default     {puts stderr [format [_ "At linenumber %d, Unknown tag <%s>"] \
                                  $lno $tag]
                     incr bct; set bogus 1
                    }
      }
    }
    if $bogus continue
    makeline u
    ::tooledit::repack
  } ;# while
  close $fd
  if {$bct >0} {
    # schedule message after message widget is created
    after 0 {::tooledit::message bogus}
  }
  message opened
  ::tooledit::column_sort $::te(entry,header,tool) tool 1
  set ::te(filemod) 0
  return
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
      && "$::te(msg,last)" != "isort"
      && "$::te(msg,last)" != "dsort"
     } {
    ::tooledit::toolvalidate silent ;# to clear errors
  }
  if [info exists ::te(load,button)] {
    if ![sendaxis ping] {
      # axis disappeared
      pack forget $::te(load,button)
      unset ::te(load,button)
    } else {
      if [sendaxis check_for_reload] {
        $::te(load,button) configure -state normal
      } else {
        $::te(load,button) configure -state disabled
      }
    }
  }
  set ::te(afterid) [after $::te(pollms) ::tooledit::watch]
} ;# watch

proc ::tooledit::tooledit {filename {columns ""} } {
  package require Tk
  if {[package vcompare $::tcl_version 8.5] >= 0} {
    # tcl8.5 lsort -indices available
    set ::te(enable_column_sorting) 1
  } else {
    set prog [file tail $::argv0]
    puts stderr [format [_ "%s: Column sorting not available with tcl_version==%s"] \
                         $prog $::tcl_version]
  }
  ::tooledit::init $columns
  set ::te(filename) $filename

  # allow for translated names for header columns:
  foreach h $::te(header) {
    switch -exact $h {
      tool    {set ::te($h,show) [_ "tool"]}
      poc     {set ::te($h,show) [_ "poc"]}
      diam    {set ::te($h,show) [_ "diam"]}
      front   {set ::te($h,show) [_ "front"]}
      back    {set ::te($h,show) [_ "back"]}
      orien   {set ::te($h,show) [_ "orien"]}
      comment {set ::te($h,show) [_ "comment"]}
      default {set ::te($h,show) $h}
    }
    set ::te($h,show) [string toupper $::te($h,show)]
  }

  set ::te(top) [toplevel .tooledit]
  wm withdraw $::te(top); update
  wm resizable $::te(top) 1 1
  wm protocol $::te(top) WM_DELETE_WINDOW ::tooledit::bye
  wm title $::te(top) "tooledit: [file tail $::te(filename)]"
  if [info exists ::te(tooledit,geometry)] {
    wm geometry $::te(top) $::te(tooledit,geometry)
  }

  # note: never pack ::te(scroll,frame), handled by ScrolledWindow
  set ::te(scroll,window)  [ScrolledWindow  $::te(top).scrolled \
             -scrollbar vertical -auto none]
  set ::te(scroll,frame) [ScrollableFrame $::te(top).scrolled.sff \
             -height $::te(initial,height) -width $::te(initial,width) \
             -constrainedwidth 1]
  $::te(scroll,window) setwidget $::te(scroll,frame) ;# associates scrollbars
  set ::te(main,frame) [$::te(scroll,frame) getframe] ;# this is parent

  set ::te(lasti) 0

  # header frame -------------------------------------------------
  set f [frame $::te(top).header]
  set ::te(header,frame) $f
  pack $f -side top -expand 1 -fill x -anchor n
  pack [label $f.b -text [_ "Del"] -width 3] -side left -expand 0

  foreach h $::te(header) {
    set e 0;set j center
    if {"$h" == "comment"} {set e 1;set j left}
    set ey [entry $f.$::te(lasti)$h -justify $j -textvariable ::te($h,show) \
         -state disabled -relief groove \
         -disabledforeground black \
         -width $::te($h,width)]
    pack $ey -side left -fill x -expand $e
    set ::te(entry,header,$h) $ey
    $ey configure -cursor arrow
    bind $ey <ButtonRelease-1> "::tooledit::column_sort $ey $h"
  }

  readfile $::te(filename)
  if [file exists $::te(filename)] {watch start}

  pack $::te(scroll,window) -side top -fill x -expand 0 -anchor nw

  # button frame -------------------------------------------------
  set bf [frame $::te(top).[qid]]
  pack $bf -side top -expand 0 -fill both -anchor nw

  pack [button $bf.[qid] -text "[_ "Quit"]" \
       -command ::tooledit::bye] \
       -side right -fill x -expand 1

  if {[sendaxis ping] && [sendaxis tool_table_filename]} {
    set ::te(load,button) [button $bf.[qid] -text "[_ "ReLoadTable"]" \
         -state disabled \
         -command [list ::tooledit::sendaxis reload_tool_table]]
    pack $::te(load,button) -side right -fill x -expand 1
  }
  pack [button $bf.[qid] -text "[_ "SaveFile"]" \
       -command [list ::tooledit::writefile $::te(filename)]] \
       -side right -fill x -expand 1
# pack [button $bf.[qid] -text "[_ "Check Entries"]" \
#      -command [list ::tooledit::toolvalidate]] -side right -fill x -expand 1
  pack [button $bf.[qid] -text "[_ "ReRead"]" \
       -command  ::tooledit::toolreread] -side right -fill x -expand 1
  pack [button $bf.[qid] -text "[_ "AddTool"]" \
       -command {::tooledit::makeline new}] -side right -fill x -expand 1

  set   bb [button $bf.[qid] -text "[_ "Delete"]"\
           -command {::tooledit::deleteline}]
  pack $bb -side right -fill x -expand 1
  set ::te(deletebutton) $bb
  checkdelete


  # message frame -------------------------------------------------
  set mf [frame $::te(top).[qid]]
  pack $mf -side top -expand 0 -fill x

  set msg [label $mf.msg -anchor w]
  set ::te(msg,widget) $msg
  pack $msg -side top -expand 0 -fill x -anchor w

  update ;# wait for display before binding Configure events
  if [info exists ::te(top,restore,geometry)] {
    wm geometry $::te(top) $::te(top,restore,geometry)
    unset ::te(top,restore,geometry)
  }
  set ::te(top,geometry) [wm geometry $::te(top)]
  set ::te(top,height)   [winfo height $::te(top)]
  # set min width so top cannot be disappeared inadvertently
  # set min height to initial
  wm minsize $::te(top) 100 $::te(top,height)
  bind $::te(top) <Configure> {::tooledit::configure %W %w %h}
  wm deiconify $::te(top)
} ;# tooledit

proc ::tooledit::configure {W w h} {
  if {"$W" != "$::te(top)"} return
  if {"$W" == $::te(top) && $::te(top,geometry) != [wm geometry $::te(top)]} {
    set ::te(top,geometry) [wm geometry $::te(top)]
    set deltah [expr $h - $::te(top,height)]
    set fsize [$::te(scroll,frame) cget -height]
    if {[expr abs($deltah)] > $::te(hincr)} {
      $::te(scroll,frame) configure -height [expr $fsize + $deltah]
      set ::te(top,height) $h
    }
  }
} ;# configure

proc ::tooledit::message {mtype} {
  if ![info exists ::te(msg,widget)] return
  set w $::te(msg,widget)
  set dt [clock format [clock seconds]]
  switch $mtype {
    opened   {$w conf -text "$dt: [format [_ "Opened %s"] $::te(filename)]"  -fg darkblue}
    newfile  {$w conf -text "$dt: [format [_ "Created %s"] $::te(filename)]" -fg darkblue}
    write    {$w conf -text "$dt: [_ "File updated"]"                -fg green4}
    modified {$w conf -text "$dt: [_ "File modified"]"               -fg darkred}
    checke   {$w conf -text "$dt: [_ "File check errors"]"           -fg red}
    checkok  {$w conf -text "$dt: [_ "File checked"]"                -fg darkgreen}
    delete   {$w conf -text "$dt: [_ "File items deleted"]"          -fg cyan4}
    bogus    {$w conf -text "$dt: [_ "Bogus lines in file ignored"]" -fg darkorange}
    verror   {$w conf -text "$dt: [_ "File errors -- Check Entries"]"   -fg red}
    changed  {$w conf -text "$dt: [_ "Warning: File changed by another process"]" -fg red}
    filegone {$w conf -text "$dt: [_ "Warning: File deleted by another process"]" -fg red}
    newtool  {$w conf -text "$dt: [_ "Added Tool"]" -fg green4
                 update idletasks
                 $::te(scroll,frame) yview moveto 1.0
             }
    isort    {$w conf -text "$dt: [format [_ "Sorted by %s, increasing"] $::te(lastsort)]" -fg darkgreen}
    dsort    {$w conf -text "$dt: [format [_ "Sorted by %s, decreasing"] $::te(lastsort)]" -fg darkgreen}
  }
  set ::te(msg,last) $mtype
} ;# message

proc ::tooledit::deleteline {} {
  set dct 0
  catch {unset dlines}
  foreach item [array names ::te "parm,*,deleteme"] {
    if {$::te($item) == 1} {
      set i1 [expr  1 + [string first , $item]]
      set i2 [expr -1 + [string last  , $item]]
      lappend dlines [string range $item $i1 $i2]
    }
  }
  if ![info exists dlines] continue
  foreach i $dlines {
    destroy $::te(entry,$i,frame); unset ::te(entry,$i,frame)
    incr dct

    if [info exists ::te(items)] {
      set idx [lsearch $::te(items) $i]
      if {$idx >= 0} {
        set ::te(items) [lreplace $::te(items) $idx $idx]
      }
      if {[string length $::te(items)] == 0} {
        unset ::te(items)
      }
    }

    foreach name [array names ::te parm,$i,*] {
      unset ::te($name)
    }
  }
  checkdelete
  if {$dct >0}  { message delete}
  incr ::te(filemod)
} ;# deleteline

proc ::tooledit::makeline {ay_name} {
  if {"$ay_name" == "new"} {
    set new 1
    set date "[_ "Added"] [clock format [clock seconds] -format %Y%m%d]"

    foreach item {t p x y z a b c u v w d i j q} {
      set ay($item) ""
    }
    set ay(p) [_ "NEW"] ;# support translation of special entry item value
    set ay(t) [_ "NEW"] ;# support translation of special entry item value
    set ay(comment) "$date"
    after 0 {::tooledit::message newtool}
  } else {
    upvar $ay_name ay
  }

  set i $::te(lasti)
  set f [frame $::te(main,frame).[qid]]
  set ::te(entry,$i,frame) $f
  if {"$ay_name" == "new"} {
    pack $f -side top -expand 1 -fill x -anchor n
  } else {
    # caller must pack (use ::tooledit::repack)
  }
  lappend ::te(items) $i
  set ::te(parm,$i,tool)      $ay(t)
  set ::te(parm,$i,poc)       $ay(p)
  set ::te(parm,$i,x)         $ay(x)
  set ::te(parm,$i,y)         $ay(y)
  set ::te(parm,$i,z)         $ay(z)
  set ::te(parm,$i,a)         $ay(a)
  set ::te(parm,$i,b)         $ay(b)
  set ::te(parm,$i,c)         $ay(c)
  set ::te(parm,$i,u)         $ay(u)
  set ::te(parm,$i,v)         $ay(v)
  set ::te(parm,$i,w)         $ay(w)
  set ::te(parm,$i,diam)      $ay(d)
  set ::te(parm,$i,front)     $ay(i)
  set ::te(parm,$i,back)      $ay(j)
  set ::te(parm,$i,orien)     $ay(q)
  set ::te(parm,$i,comment)   [string trim $ay(comment)]
  pack [checkbutton $f.b -variable ::te(parm,$i,deleteme)\
       -command "::tooledit::checkdelete"] -side left -expand 0
  foreach h $::te(header) {
    set e 0;set j right;set v 1
    if {"$h" == "comment"} {set e 1; set j left;set v 0}
    set ve [ventry $f $v ::te(parm,$i,$h) $::te($h,width) $e $j]
    if {[info exists new] && "$h" == "tool"} {set vefocus $ve}
    entrybindings $ve $h $i
  }
  incr ::te(lasti)
  if [info exists vefocus] {
    set ::te(restore,selectbackground) [$vefocus cget -selectbackground]
    set ::te(restore,selectforeground) [$vefocus cget -selectforeground]
    $vefocus configure -selectbackground white
    $vefocus configure -selectforeground red
    $vefocus selection to end
    focus $vefocus
  }
} ;# makeline

proc ::tooledit::entrybindings {e h i} {
  $e conf -takefocus 1
  set ::te($i,$h,entry) $e
  bind $e <Key-Up>    "::tooledit::bindactions $h $i %K"
  bind $e <Key-Down>  "::tooledit::bindactions $h $i %K"
  bind $e <Key-Left>  "::tooledit::bindactions $h $i %K"
  bind $e <Key-Right> "::tooledit::bindactions $h $i %K"
} ;# entrybindings

proc ::tooledit::bindactions {h i key args} {
  set nexth $h;set nexti $i;
  switch $key {
    Up {
      set nexti [expr $i -1]
      if {$nexti <0} {
        set nexti [expr $::te(lasti) -0]
        after 0 [list ::tooledit::bindactions $h $nexti $key]
        return
      }
    }
    Down {
      set nexti [expr $i + 1]
      if {$nexti >= $::te(lasti)} {
        set nexti -1
        after 0 [list ::tooledit::bindactions $h $nexti $key]
        return
      }
    }
    Right {
      if {"$h" == "nosuch"} {
        set nextidx 0
      } else {
        set idx [lsearch $::te(header) $h]
        set nextidx [expr $idx + 1]
        if {$nextidx >= [llength $::te(header)]} {
          after 0 [list ::tooledit::bindactions nosuch $nexti $key]
          return
        }
      }
      set nexth [lindex $::te(header)  $nextidx]
    }
    Left {
      if {"$h" == "nosuch"} {
        set nextidx [expr [llength $::te(header)] -1]
      } else {
        set idx [lsearch $::te(header) $h]
        set nextidx [expr $idx + -1]
        if {$nextidx < 0} {
          after 0 [list ::tooledit::bindactions nosuch $nexti $key]
          return
        }
      }
      set nexth [lindex $::te(header)  $nextidx]
    }
  }
  if [info exists ::te($nexti,$nexth,entry)] {
    $::te($nexti,$nexth,entry) selection to end
    focus $::te($nexti,$nexth,entry)
  } else {
    # frame has been deleted
    switch $key {
      Up - Down {
        set nexti [expr $nexti + 0]
        after 0 [list ::tooledit::bindactions $h $nexti $key]
        return
      }
    }
  }
} ;# bindactions

proc ::tooledit::checkdelete {} {
  set ct 0
  foreach name [array names ::te parm,*,deleteme] {
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
  set ::te(tooledit,geometry) [wm geometry $::te(top)]

  for {set i 0} {$i < $::te(lasti)} {incr i} {
    catch {
      destroy $::te(entry,$i,frame)
      unset ::te(entry,$i,frame)
    } ;# it may already be gone
  }
  set ::te(lasti) 0
  # can be missing for some prior file open errors
  catch {unset ::te(items)}

  readfile $::te(filename)
  if [file exists $::te(filename)] {watch start}
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

  # write to all populated header items (to preserve values if not displayed)
  set allheader [concat $::te(autocolumns) $::te(allcolumns) comment]

  foreach i $::te(items) {
    foreach h $allheader {
      set j ""
      set w $::te($h,width)
      # correct entries with leading zeros
      if {$h != "comment" &&
		[string first 0 [string trim $::te(parm,$i,$h)]] == 0} {
         set ::te(parm,$i,$h) [format %g $::te(parm,$i,$h)]
      }
      set value [string trim $::te(parm,$i,$h)]
      if {"$value" != ""} {
        puts -nonewline $fd "$::te($h,tag)$value "
      }
    }
    puts $fd "" ;# new line
  }
  watch stop
  close $fd
  watch start
  message write
  set ::te(filemod) 0
} ;# writefile

proc ::tooledit::toolvalidate {args} {
  set msg ""
  set silent 0
  if {"$args" == "silent"} {set silent 1}

  if [info exists ::te(items)] {
    foreach i $::te(items) {
      foreach h $::te(header) {
        if {"$h" == "comment"} continue
        if ![isnumber $::te(parm,$i,$h)] {
          set nextmsg [format [_ "Tool %s, Column %s, parameter %s is not a number"] \
                       $::te(parm,$i,tool) $h $::te(parm,$i,$h)]
          if {[lsearch $msg $nextmsg] >= 0} continue
          lappend msg $nextmsg
        }

        switch -glob $h {
          tool* - poc* {
            if {![isinteger $::te(parm,$i,$h)] || [isnegative $::te(parm,$i,$h)]} {
              lappend msg [format [_ "Tool %s, parameter %s must be nonnegative integer"] \
                          $::te(parm,$i,tool) $h]

            }
          }
          orien* {
            if {   "$::te(parm,$i,$h)" != "" \
                && [lsearch {0 1 2 3 4 5 6 7 8 9} $::te(parm,$i,$h)] < 0} {
              lappend msg [format [_ "Tool %s: <Orientation> must be 0..9 integer"] \
                          $::te(parm,$i,tool)]

            }
          }
          front* - back* {
            if {![validangle $::te(parm,$i,$h)] } {
              lappend msg [format [_ "Tool %s: <%s> must be between -360 and 360"] \
                          $::te(parm,$i,tool) $h]
            }
          }
        }
      }
    }
  }

  # check for multiple uses of a single pocket
  if [info exists ::te(items)] {
    set pocs ""
    foreach i $::te(items) {
      set p $::te(parm,$i,poc)

      if {[lsearch $pocs $p] >= 0} {
        set nextmsg [format [_ "Pocket <%s> specified multiple times"]  $p]
        if {[lsearch $msg $nextmsg] >= 0} continue
        lappend msg $nextmsg
      } else {
        lappend pocs $p
      }
    }
  }
  # check for multiple uses of a single tool
  if [info exists ::te(items)] {
    set tools ""
    foreach i $::te(items) {
      set t $::te(parm,$i,tool)

      if {[lsearch $tools $t] >= 0} {
        set nextmsg [format [_ "Tool <%s> specified multiple times"] $t]
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
  if $::te(filemod) {
    set ans [tk_dialog .filemod \
              "[_ "File Modified"]" \
              "[_ "Save Modifications to File?"]" \
              {} \
              0 \
              Yes No]
    if {$ans == 0} {
      ::tooledit::writefile $::te(filename)
    }
  }

  catch {after cancel $::te(afterid)}

  if ![file exists ~/.tooleditrc] {
    # first time use presumed, instruct for configuring columns
    set used ""
    foreach item $::te(items) {
      foreach col $::te(allcolumns) {
        if {"$::te(parm,$item,$col)" != ""} {lappend used $col}
      }
    }
    # make used list unique:
    foreach item $used { set tmp($item) "" }
    set used [array names tmp]

    if {   ![info exists ::te(user_specified_columns)] \
        && ("$used" != "") \
        && ([llength $used] < [llength $::te(allcolumns)]) } {
      set prog [file tail $::argv0]
      set msg "[format [_ "Only these columns are currently used:\n\n %s"] $used] \
           \n\n[_ "Limit display to these columns by specifying"]:\
             \n \[DISPLAY\]TOOL_EDITOR = $prog $used\
           \n\n[_ "Format for ini file is"]:\
             \n  \[DISPLAY\]TOOL_EDITOR = $prog col_1 col_2 ... col_n\
           \n\n[_ "For standalone use, invoke as"]:\
           \n\n  $prog col_1 col_2 ... col_n [_ "tool_table_filename"]
              "
      catch {destroy .msg}
      toplevel .msg
      set txt [text .msg.txt -width 80 -height 14]
      $txt insert end $msg
      pack $txt -side top -fill x -expand 0
      pack [button .msg.b -text OK -command {destroy .msg}] -side top
    }
    update
  }
  while 1 {
    if ![winfo exists .msg] break
    after 1000
    update
  }

  if [winfo exists $::te(top)] {
    set fd [open ~/.tooleditrc w]
    set time [clock format [clock seconds] -format %Y%m%d.%H.%M.%S]
    puts $fd   "# $time [format [_ "Created by %s"] [file normalize $::argv0]]"
    puts $fd "\n# [_ "Saved geometry (updated on program termination)"]:"
    puts $fd "set geometry [wm geometry $::te(top)]"
    close $fd
    destroy $::te(top)    ;# for embedded usage
  }
  set ::tooledit::finis 1 ;# for standalone usage
} ;# bye

proc ::tooledit::sendaxis {cmd} {
  # return 1==>ok
  switch $cmd {
    ping {
      # must ping to see if axis is running and get its pwd
      if ![catch {set ::te(axis,pwd) [send axis pwd]} msg] {return 1 ;#ok}
    }
    tool_table_filename {
      set prog [file tail $::argv0]
      # check that tooledit opened with same filename as axis
      if [catch {set f [send axis inifindall EMCIO TOOL_TABLE]} msg] {
        return -code error "::tooledit::sendaxis tool_table_filename <$msg>"
      }
      if {[llength $f] > 1} {
        set f [lindex $f 0] ;# use first item specified for compatibility
        puts stderr [format [_ "%s: Axis inifile specifies multiple inifile items for: \[EMCIO\]TOOL_TABLE"] $prog]
        puts stderr [format [_ "%s: Using: %s"] $prog $f]
      }
      if {[file pathtype $f] == "relative"} {
        set f [file join $::te(axis,pwd) $f]
      }
      set ::te(axis,filename) [file normalize $f]
      if {"$::te(axis,filename)" == [file normalize $::te(filename)]} {
        return 1 ;# ok
      } else {
        puts stderr [format [_ "%s: Warning: Axis is running but the tool table file <%s>\
                               \ndiffers from the standalone startup file <%s>"]\
                     $prog $::te(axis,filename) $::te(filename)]
      }
    }
    check_for_reload {
      # use same test as axis for disabling:
      if [send axis {expr "$::task_state"   == "$::STATE_ON"\
                       && "$::interp_state" == "$::INTERP_IDLE"}] {
        return 1 ;# ok
      }
    }
    reload_tool_table {
      if ![sendaxis check_for_reload] {
        showerr [list "[_ "Must be On and Idle to reload tool table"]"]
        return 0 ;# fail
      }
      ::tooledit::writefile $::te(filename)
      if [catch {send axis reload_tool_table} msg] {
        return -code error "::tooledit::sendaxis reload_tool_table <$msg>"
      }
      return 1 ;# ok
    }
    default {return -code error "::tooledit::sendaxis: unknown cmd <$cmd>"}
  }

  return 0 ;# fail
} ;# sendaxis

proc ::tooledit::repack { {entryname tool} {mode increasing} } {
  if ![info exist ::te(enable_column_sorting)] {
    foreach name [array names ::te entry*frame] {
     pack $::te($name) -side top -expand 1 -fill x -anchor n
    }
    return
  }

  set type $::te(type,$entryname)

  foreach item $::te(items) {
    set value $::te(parm,$item,$entryname)
    if {    ( ("$type"  == "real") || ("$type"  == "integer") )\
         && ( ("$value" == ""    ) || ("$value" == "[_ "NEW"]") ) } {
      set value 0
    }
    lappend parms   $value
    lappend parms_i $item
  }

  foreach i $::te(items) {
    pack forget $::te(entry,$i,frame)
  }

  set indices [lsort -$type -$mode -indices $parms]
  foreach idx $indices {
    set i [lindex $parms_i $idx]
    pack $::te(entry,$i,frame) -side top -expand 1 -fill x -anchor n
  }
} ;# repack

proc ::tooledit::column_sort {e parm {initialize 0} } {
  if {$initialize || ![info exists ::te(columnsortorder)]} {
    set ::te(columnsortorder) increasing
    catch {$::te(lastsort,entry) configure -disabledforeground black}
  } else {
    if [info exists ::te(lastsort,entry)] {
      $::te(lastsort,entry) configure -disabledforeground black
      if {"$::te(lastsort,entry)" != "$e"} {
        set ::te(columnsortorder) decreasing
      }
    }
    if {"$::te(columnsortorder)" == "increasing"} {
      set ::te(columnsortorder) decreasing
    } else {
      set ::te(columnsortorder) increasing
    }
  }
  ::tooledit::repack $parm $::te(columnsortorder)

  if ![info exists ::te(enable_column_sorting)] return

  set ::te(lastsort,entry) $e
  set ::te(lastsort) [string toupper $parm]
  switch $::te(columnsortorder) {
    increasing {
      $e configure -disabledforeground blue
      ::tooledit::message isort
    }
    decreasing {
      $e configure -disabledforeground violetred
      ::tooledit::message dsort
    }
  }
} ;# column_sort

#------------------------------------------------------------------------
set ::te(allcolumns)  {x y z a b c u v w diam front back orien}
proc standalone_tooledit {args} {
  # configure for standalone usage:
  set ::te(standalone) 1
  wm withdraw .
  if {[lindex $::argv 0] == ""} {
    set prog [file tail $::argv0]
    puts stderr "\n[_ "Usage"]:"
    puts stderr "       $prog [_ "filename"]"
    puts stderr "       $prog \[column_1 ... column_n\] [_ "filename"]"
    puts stderr "\n[format [_ "Allowed column_ names are: %s"] $::te(allcolumns)]"
    exit 1
  }
  # start, unless already started (convenient for debug sourcing):
  if ![info exists ::te(top)] {
    # expect ::argv == [colname colname ...] filename
    set columns "" ;# default use all columns
    set argct [llength $::argv]
    switch $argct] {
          0 {#notreached
             puts stderr "$::argv0: [_ "Missing filename"]";exit 1}
          1 {set filename $::argv}
    default {set filename [lindex $::argv end]
             set  columns [lreplace $::argv end end]
            }
    }
    ::tooledit::tooledit $filename $columns
    tkwait variable ::tooledit::finis
    exit 0
  }
} ;# standalone_tooledit

if {[info script] == $::argv0} standalone_tooledit

