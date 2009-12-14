#!/usr/bin/tclsh
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

# convert tooltables for emc 2.4 and earlier to use tlo_all_axes

# Usage: ./toolconvert filename
#        (original file is saved as filename.orig)

namespace eval ::tconvert {
  namespace export tconvert ;# public interface
}

proc ::tconvert::readfile {filename} {
  # return codes:
  #              error  msg
  #              nofile 0
  #              ok     1
  if {[file exists $filename] && ![file readable $filename]} {
    set ::tc(msg) "filename: <$filename> not readable"
    return 0
  }
  if [file exists $filename] {
    if ![file writable $filename] {
      set ::tc(msg) "filename: <$filename> not writable"
      return 0
    }
  }
  if [catch {set fd [open $filename r]} msg] {
    set ::tc(msg) $msg
    return 0
  }

  set ::tc(types)    {mill lathe}
  set ::tc(bogus,ct) 0
  set ::tc(mill,ct)  0
  set ::tc(lathe,ct) 0

  set bct 0
  while {1} {
    gets $fd newline
    if [eof $fd] break
    if {"$newline" == ""} continue

    catch {unset m}; set m(4) ""
    set mct [scan $newline "%d %d %f %f %\[^\n\]" m(0) m(1) m(2) m(3) m(4)]

    catch {unset l}; set l(8) ""
    set lct [scan $newline "%d %d %f %f %f %f %f %d %\[^\n\]" \
            l(0) l(1) l(2) l(3) l(4) l(5) l(6) l(7) l(8)]

    if { $lct == 8 || $lct == 9} {
      makeline lathe l
      incr ::tc(lathe,ct)
    } elseif {$mct == 4 || $mct == 5} {
      makeline mill m
      incr ::tc(mill,ct)
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
      incr ::tc(bogus,ct)
    }
    puts stderr ""
    # schedule message after message widget is created
    after 0 {::tconvert::message bogus}
  }
  if {0 == [expr $::tc(mill,ct) + $::tc(lathe,ct)]} {
    puts stderr "No valid lines in $filename"
    return 0 ;# ng
  }
  return 1 ;# ok
} ;# readfile

proc ::tconvert::fixcomment {comment} {
  set c [string trim $comment]
  if {[string first # $c] == 0} {
     set c [string range $c 1 end]
  }
  return [string trim $c]
} ;# fixcomment

proc ::tconvert::makeline {type ay_name} {
  upvar $ay_name ay
  switch $type {
    mill  {
      if ![info exists ::tc(mill,i)] {set ::tc(mill,i) 0}
      set i $::tc(mill,i)
      set ::tc(mill,$i,t) $ay(0)  ;# t:tool
      set ::tc(mill,$i,p) $ay(1)  ;# p:pocket
      set ::tc(mill,$i,z) $ay(2)  ;# z:zoffset
      set ::tc(mill,$i,d) $ay(3)  ;# d:diameter
      set ::tc(mill,$i,comment)   [fixcomment $ay(4)]
      lappend ::tc(mill,items) $i
      incr ::tc(mill,i)
    }
    lathe {
      if ![info exists ::tc(lathe,i)] {set ::tc(lathe,i) 0}
      set i $::tc(lathe,i)
      set ::tc(lathe,$i,t) $ay(0)  ;# t:tool
      set ::tc(lathe,$i,p) $ay(1)  ;# p:pocket
      set ::tc(lathe,$i,z) $ay(2)  ;# z:zoffset
      set ::tc(lathe,$i,x) $ay(3)  ;# x:xoffset
      set ::tc(lathe,$i,d) $ay(4)  ;# d:diameter
      set ::tc(lathe,$i,i) $ay(5)  ;# i:frontangle
      set ::tc(lathe,$i,j) $ay(6)  ;# i:backangle
      set ::tc(lathe,$i,q) $ay(7)  ;# q:orient
      set ::tc(lathe,$i,comment)   [fixcomment $ay(8)]
      lappend ::tc(lathe,items) $i
      incr ::tc(lathe,i)
    }
    default {return -code error "makeline:unknown type <$type>"}
  }
} ;# makeline

proc ::tconvert::writefile {filename} {
  if [catch {set fd [open $filename w]} msg] {
    puts "writefile: $msg"
    exit 1
  }

  foreach type $::tc(types) {
    if [info exists ::tc($type,items)] {
      foreach i $::tc($type,items) {
        switch $type {
          mill  {
            puts -nonewline $fd "T$::tc(mill,$i,t) "
            puts -nonewline $fd "P$::tc(mill,$i,p) "
            puts -nonewline $fd "Z$::tc(mill,$i,z) "
            puts -nonewline $fd "D$::tc(mill,$i,d) "
            puts -nonewline $fd ";$::tc(mill,$i,comment)"
            puts            $fd ""
          }
          lathe {
            puts -nonewline $fd "T$::tc(lathe,$i,t) "
            puts -nonewline $fd "P$::tc(lathe,$i,p) "
            puts -nonewline $fd "X$::tc(lathe,$i,x) "
            puts -nonewline $fd "Z$::tc(lathe,$i,z) "
            puts -nonewline $fd "D$::tc(lathe,$i,d) "
            puts -nonewline $fd "I$::tc(lathe,$i,i) "
            puts -nonewline $fd "J$::tc(lathe,$i,j) "
            puts -nonewline $fd "Q$::tc(lathe,$i,q) "
            puts -nonewline $fd ";$::tc(lathe,$i,comment)"
            puts            $fd ""
          }
        }
      }
    }
  }
  close $fd
  return ""
} ;# writefile

proc ::tconvert::report {} {
  if [info exists ::tc(msg)] {puts stderr $::tc(msg)}
  foreach item {bogus mill lathe} {
    if {[info exists ::tc($item,ct)] && 0 != $::tc($item,ct)} {
      puts stderr "     $item lines=$::tc($item,ct)"
    }
  }
  return ""
} ;# report

#-----------------------------------------------------------------------
# begin
if {[lindex $argv 0] == ""} {
  puts stderr "Convert a tooltable file for tlo_all_axes"
  puts stderr "Usage: $::argv0 filename"
  puts stderr "       (original file is saved as filename.orig)"
  exit 0
}
set filename [lindex $argv 0]

if {![::tconvert::readfile $filename]} {
  ::tconvert::report
  puts stderr "$filename NOT converted"
  exit 1
}

if [catch {file rename $filename $filename.orig} msg] {
  puts Error:$msg
  exit 1
}

::tconvert::writefile $filename
puts stderr "$filename converted:"
::tconvert::report

exit 0
