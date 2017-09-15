# ngcgui_ttt.tcl: a LinuxCNC [DISPLAY]TKAPP for using truetype-tracer and ngcgui

#-----------------------------------------------------------------------
# Copyright: 2010-2012
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
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#-----------------------------------------------------------------------

# $ truetype-tracer -?
# v3: [-?] [-u] [-s subdist] [-f font.ttf] 'Text to trace'
# v4: [-?] [-s subdiv] [-u] [-c scale] [-l linescale] [-f /some/file.ttf] 'The Text'

# ttt.c -l 0 or if speciied, 24 min
# ttt.c -s --> dsteps default: 200

#note: to use quotes in text, express as \" seems to work

namespace eval ::ttt {namespace export ttt ;# public interface}

proc ::ttt::trimsuffix {s {sfx .ttf} } {
  set idx [string last $sfx $s]
  if {$idx <0} {return $s}
  return [string range $s 0 [expr -1 + $idx]]
} ;# trimsuffix

proc ::ttt::setfont {} {
  if {"$::ttt(font)" == ""} {
    set idir "/usr/share/fonts/truetype/freefont"
  } else {
    set idir [file dirname $::ttt(font)]
  }
  set filename [tk_getOpenFile \
           -title "[_ "Set Font"]"\
           -defaultextension .ttf \
           -initialfile "" \
           -initialdir  "$idir" \
           ]
  set ::ttt(font) $filename
} ;# setfont

proc ::ttt::checkttt {} {
  # 101127 ttt.c doesn't hava a --version option and returns 99 for -?
  #future: set ans -1
  #future: if [catch {set ans [eval exec -ignorestderr $::ttt(exe) -v]} msg] {
  #future:   puts check:<$msg>
  #future: }
  #future: test ans here

  # hack follows:
  catch {set ans1 [eval exec  $::ttt(exe) -? 2>/tmp/ttt.q]}
  catch {set ans2 [eval exec grep -c subdiv /tmp/ttt.q]}
  catch {file delete /tmp/ttt.q}
  if {[info exists ans2] && $ans2 == 1} {
    set ::ttt(msg) "::ttt::embedinit [_ "found truetype-tracer v4 -OK"]"
    return 1
  } else {
    puts stderr "::ttt::embedinit:[_ "Note truetype-tracer v4 is required"]"
    set ::ttt(msg) "[_ "Note: truetype-tracer v4 is required"]"
    return 0
  }
} ;# checkttt

proc ::ttt::embedinit {} {
  package require Ngcgui
  if [info exists ::ttt(instance)] return ;# for testing convenience
  set ::ttt(this) ngcgui_ttt.tcl
  if {[info procs ::ngcgui::parent] == ""} {
    return -code error "\n\n[_ "ngcgui_app.tcl must be loaded before"] $::ttt(this)"
  }

  # alternate behaviors
  set ::ttt(multiplepages)      1 ;# 1:most flexible
  set ::ttt(use_program_prefix) 0 ;# 0:no clutter

  set ::ttt(info) \
     "[_ "Create a subroutine file from truetype-tracer (V4 reqd)"]"
  set ::ttt(instance)    0
  set ::ttt(color,ok)    green4
  set ::ttt(color,black) black
  set ::ttt(color,error) red
  set ::ttt(datefmt)     "%d %b %Y"

  lappend ::ttt(embed,options)  noiframe ;# no image and user needs controls
  lappend ::ttt(embed,options)  nonew    ;#
  lappend ::ttt(embed,options)  nopathcheck ;# SUBROUTINE_PATH not checked
 
  set ::ttt(exe) [inifindall DISPLAY TTT]
  if {[llength $::ttt(exe)] >1} {
    return -code error "[_ "problem with"] \[DISPLAY\]TTT <$::ttt(exe)>"
  }
  if {"$::ttt(exe)" == ""} {
    set ::ttt(exe) [file join /usr/local/bin truetype-tracer]
    puts stderr "::ttt::embedinit:[_ "No entry for"] \[DISPLAY\]TTT, trying $::ttt(exe)"
  }
  if ![checkttt] {
    puts stderr "::ttt::embedinit:[_ "wrong version of truetype-tracer"]"
    return
  }

  set preamble [inifindall DISPLAY TTT_PREAMBLE]
  if {([llength $preamble] >1)} {
    return -code error "[_ "problem with"] \[DISPLAY\]TTT_PREAMBLE <$preamble>"
  }
  set ::ttt(preamble) $preamble

  if $::ttt(use_program_prefix) {
    set ::ttt(dir) [inifindall DISPLAY PROGRAM_PREFIX]
    if ![file writable $::ttt(dir)] {
      set d $::ttt(dir)
      set ::ttt(dir) /tmp
      puts stderr \
       "::ttt::embedinit: <$d> [_ "not writable, using"] $::ttt(dir) [_ "and setting expandsub"]"
      lappend ::ttt(embed,options) expandsub ;# needed if no PROGRAM_PREFIX
    }
  } else {
    set ::ttt(dir) /tmp
    lappend ::ttt(embed,options) expandsub ;# using /tmp so need to expand
  }

  set ::ttt(filebase) ttt
  set ::ttt(text) truetype-tracer
  if [info exists ::env(USERNAME)] {
    set ::ttt(text) $::env(USERNAME)
  }

  # hardcoded in ttt.c:
  set ::ttt(font) "/usr/share/fonts/truetype/freefont/FreeSerifBoldItalic.ttf"
  set ::ttt(default,scale) 0.0003 ;# hardcoded in ttt.c:

  set ::ttt(pagename) ttt
  set ::ttt(topf) [::ngcgui::getngcgui_frame $::ttt(pagename)]

  if {[info procs ::ngcgui::tabmanage] == "::ngcgui::tabmanage"} {
    ::ngcgui::tabmanage $::ttt(pagename) $::ttt(topf) \
                        "truetype-tracer-ngcgui"\
                        ::ttt(info)
  }
  set w [frame $::ttt(topf).t]
  pack $w -fill both -expand 1 -anchor nw -side top

  set lw 8 ;# labelwidth
  set ew 6 ;# entrywidth
  set rw 8 ;# radiobutton width

  # f1 Text
  set f [frame $w.f1 -bd 1 -relief ridge]
  pack $f -fill both -side top -anchor nw -expand 0
  pack [label $f.l -text "[_ "Text"]" \
       -anchor e -relief ridge \
       -width $lw \
       ] -side left -fill none -expand 0
  pack [entry $f.e -textvariable ::ttt(text)\
        -justify left -width 40  -relief sunken] -side left -fill x -expand 1

  # f2 linescale
  set f [frame $w.f2 -bd 1 -relief ridge]
  pack $f -side top -anchor nw -fill x -expand 0
  pack [label $f.l -text "[_ "Linescale"]" -width $lw  \
       -anchor e -relief ridge \
       ] -side left -fill none -expand 0
  pack [entry $f.e -textvariable ::ttt(linescale) \
        -justify right -width $ew\
       ] -side left -fill none -expand 0
  set dummy "[_ "none"]" ;# for i18n
  foreach value {none 25 50 100} {
    pack [radiobutton $f.ls$value \
           -width $rw \
           -text  "[_ "$value"]" \
           -value "[_ "$value"]" \
           -variable ::ttt(linescale)\
           -relief sunken\
         ] -side left -fill none -expand 0
  }
  set ::ttt(linescale) "none"

  # f3 subdiv
  set f [frame $w.f3 -bd 1 -relief ridge]
  pack $f -side top -anchor nw -fill x -expand 0
  pack [label $f.l -text "[_ "Subdiv"]" \
       -width $lw  \
       -anchor e -relief ridge \
       ] -side left -fill none -expand 0
  pack [entry $f.e -textvariable ::ttt(subdiv) \
        -justify right -width $ew\
       ] -side left -fill none -expand 0
  set dummy "[_ "default"]" ;# for i18n
  foreach value {default 100 200 400} {
    pack [radiobutton $f.ls$value \
           -width $rw \
           -text  "[_ "$value"]" \
           -value "[_ "$value"]" \
           -variable ::ttt(subdiv)\
           -relief sunken\
         ] -side left -fill none -expand 0
  }
  set ::ttt(subdiv) "default"

  # f4 scale
  set f [frame $w.f4 -bd 1 -relief ridge]
  pack $f -side top -anchor nw -fill x -expand 0
  pack [label $f.l -text "[_ "Scale"]" \
       -width $lw \
       -anchor e -relief ridge \
       ] -side left -fill none -expand 0
  pack [entry $f.e -textvariable ::ttt(scale) \
        -justify right -width $ew\
       ] -side left -fill none -expand 0
  set svalues "default"
  set ct 0
  foreach m {.5 2 5} {
    lappend svalues [format %.4g [expr $::ttt(default,scale) * $m]]
  }
  foreach value $svalues {
    incr ct
    pack [radiobutton $f.x$ct \
           -width $rw \
           -text $value -value "$value" -variable ::ttt(scale)\
           -relief sunken\
         ] -side left -fill none -expand 0
  }
  set ::ttt(scale) default

  # f5 mode
  set f [frame $w.f5 -bd 1 -relief ridge]
  pack $f -side top -anchor nw -fill x -expand 0
  pack [label $f.m -text "[_ "Mode"]" \
       -width $lw \
       -anchor e -relief ridge \
       ] -side left -fill none -expand 0
  pack [entry $f.e -textvariable ::ttt(test) \
        -justify right -width $ew\
        -state readonly \
       ] -side left -fill none -expand 0
  set dummy "[_ "normal"]"   ;# i18n
  set dummy "[_ "date"]"     ;# i18n
  set dummy "[_ "fontname"]" ;# i18n
  foreach value {normal date AaBb123 fontname} {
  pack [radiobutton $f.x$value \
           -width $rw \
           -text $value -value "$value" -variable ::ttt(test)\
           -relief sunken\
       ] -side left -fill none -expand 0
  }
  set ::ttt(test) "normal"

  # f6 check buttons
  set f [frame $w.f6 -bd 1 -relief ridge]
  pack $f -side top -anchor nw -fill x -expand 0
  pack [label $f.m -text "[_ "Switches"]" \
       -width $lw \
       -anchor e -relief ridge \
       ] -side left -fill none -expand 0

  pack $f -side top -anchor nw -fill x -expand 0

  set ::ttt(unicode) 0
  pack [checkbutton $f.unicode -text "[_ "Unicode"]" \
       -var ::ttt(unicode)\
       ] -side left -fill none -expand 0

  set ::ttt(allowrotation) 0
  pack [checkbutton $f.rotation -text "[_ "Allow Rotation"]" \
                    -var ::ttt(allowrotation)\
       ] -side left -fill none -expand 0


  # f7 font
  set f [frame $w.f7 -bd 1 -relief ridge]
  pack $f -fill both -side top -anchor nw -expand 0
  pack [button $f.b -text "[_ "Font"]" \
                     -width $lw  -padx 0 -pady 0\
                     -command ::ttt::setfont \
                     ] -side left -fill none -expand 0
  pack [entry $f.e -textvariable ::ttt(font)\
        -justify left -width 40  -relief sunken\
       ] -side left -fill x -expand 1

  # f8 Make
  set f [frame $w.f8 -relief ridge]
  pack $f -fill both -anchor nw -expand 0
  pack [button $f.b \
       -text "[_ "Make ngcgui-compatible subfile and new tab page"]" \
       -padx 0 -pady 0 \
       -command ::ttt::runttt\
       ] -side top -fill x -expand 1

  # f9 msg
  set f [frame $w.f9 -bd 1 -relief ridge]
  pack $f -fill both -side top -anchor nw -expand 0
  set l [entry $f.msg -relief flat \
          -state readonly \
          -justify left -textvariable ::ttt(msg)]
  pack $l -anchor nw -fill x -expand 0
  set ::ttt(msg,widget) $l
  $::ttt(msg,widget) configure -fg $::ttt(color,ok)

  ::ttt::bindings setup
} ;# init

proc ::ttt::bindings {mode} {
  switch $mode {
    setup {
     ::ngcgui::bind_for_axis $::ttt(topf)
     bindtags $::ttt(topf) $::ttt(topf)
     ::ngcgui::entry_mend $::ttt(topf)
     bind $::ttt(topf) <Enter> [list ::ttt::bindings enter]
     bind $::ttt(topf) <Leave> [list ::ttt::bindings leave]
     set ::ttt(restore,focus) [focus -lastfor $::ttt(topf)]
    }
    enter {
      set ::ttt(restore,focus) [focus -lastfor $::ttt(topf)]
      focus $::ttt(topf)
    }
    leave {
     focus -force $::ttt(restore,focus)
    }
  }
} ;# bindings

proc ::ttt::runttt {} {
  set sname   $::ttt(filebase)

  # if you make a separate tab for each MakeTTT,
  # then need different subname and filename
  if $::ttt(multiplepages) {
    set sname $::ttt(filebase)-$::ttt(instance)
  }
  set ofile [file join $::ttt(dir) $sname.ngc]

  # switch -nocase not avail tcl8.4
  switch [string tolower $::ttt(test)] {
    date     {set thetext [clock format [clock seconds] \
                          -format $::ttt(datefmt)]
             }
    normal   {set thetext "$::ttt(text)"}
    fontname {set thetext [trimsuffix [file tail $::ttt(font)]]}
    default  {set thetext "$::ttt(test)"}
  }
  if {[string trim $thetext] == ""} {
    set ::ttt(msg) "[_ "Null text"]"
    $::ttt(msg,widget) configure -fg $::ttt(color,error)
    return
  }
  set ::ttt(msg)  ""

  # tricky to get pipeline tokens correct:

  if {"$::ttt(linescale)" == "none"} {
    set lopt ""; set lval ""
  } else {
    set lopt "-l"; set lval $::ttt(linescale)
  }
  if {"$::ttt(subdiv)" == "default"} {
    set sopt ""; set sval ""
  } else {
    set sopt "-s"; set sval $::ttt(subdiv)
  }
  if {"$::ttt(scale)" == "default"} {
    set copt ""; set cval ""
    set ::ttt(scale) $::ttt(default,scale) ;# hack to agree with ttt.c
  } else {
    set copt "-c"; set cval "$::ttt(scale)"
  }
  switch $::ttt(unicode) {
          1 {set uopt "-u"}
    default {set uopt ""}
  }

  if {$::ttt(font) == ""} {
    set fopt ""; set fval ""
    set ::ttt(msg) "[_ "Using truetype-tracer default font"]"
    $::ttt(msg,widget) configure -fg $::ttt(color,black)
  } else {
    if ![file exists $::ttt(font)] {
      set ::ttt(msg) "[_ "no such file"] : $::ttt(font)"
      $::ttt(msg,widget) configure -fg $::ttt(color,error)
      return
    }
    if ![file readable $::ttt(font)] {
      set ::ttt(msg) "[_ "file not readable"] :$::ttt(font)"
      $::ttt(msg,widget) configure -fg $::ttt(color,error)
      return
    }
    set fopt "-f"; set fval "$::ttt(font)"
  }
  set eline "$::ttt(exe) >>$ofile \
             $lopt $lval \
             $sopt $sval \
             $copt $cval \
             $uopt \
             $fopt $fval \
             \"$thetext\" \
             | grep -Evi m02\|m2\|= \
             "
  #puts eline=$eline
  set showopt "$lopt $lval $sopt $sval $copt $cval $uopt"
  #omitted for space: $fopt $fval

#  set pno 6; set gno 59;
  set gno(1) G54
  set gno(2) G55
  set gno(3) G56
  set gno(4) G57
  set gno(5) G58
  set gno(6) G59
  set gno(7) G59.1
  set gno(8) G59.2
  set gno(9) G59.3
  set   fd [open $ofile w] ;# truncate
  set txt \
"(cmd:  [file tail $::ttt(exe)] $showopt)
(info: TEXT: $thetext)
($::ttt(this) created this file using truetype-tracer)
(      exe = $::ttt(exe))
(     font = $::ttt(font))
(linescale = $::ttt(linescale))
(    scale = $::ttt(scale))
(   subdiv = $::ttt(subdiv))
o<$sname> sub
#<zsafe>   = #1 (=0.1 Safe Height)
#<zcut>    = #2 (=0.01 Depth of Cut)
#<xyscale> = #3 (=$::ttt(scale) XY Scale)
#<feed>    = #4 (=10.0 Feed)
#<xoff>    = #5 (=0.0 X offset)
#<yoff>    = #6 (=0.0 Y offset)"
  puts $fd $txt
  if $:::ttt(allowrotation) {
    set txt \
"#<system>  = #7 (=1 coordsystem to rotate 1==g54)
#<rotate>  = #8 (=0 rotate angle)
o<ifb> if \[#<system> LT 0.0\]
         (debug, Illegal system #<system>)
         m2
o<ifb> endif
o<ifc> if \[#<system> GT 9.0\]
         (debug, Illegal system #<system>)
         m2
o<ifc> endif
"
    foreach s {1 2 3 4 5 6 7 8 9} {
      set txt "$txt
o<if$s> if \[#<system> EQ $s\]
o<ifr$s>  if \[#<rotate> NE 0.0\]
            g10l2p$s r#<rotate> ;system $s $gno($s) rotate
            (debug,system $s $gno($s) rotated #<rotate>)
o<ifr$s>  endif
          $gno($s)
o<if$s> endif
"
    }
    puts $fd $txt
  }
  close $fd

  if [catch {eval exec >>$ofile $eline} msg] {
    set ::ttt(msg) "$msg"
    $::ttt(msg,widget) configure -fg $::ttt(color,error)
    return
  }

  set fd [open $ofile a] ;# append
  if $::ttt(allowrotation) {
    set txt ""
    foreach s {1 2 3 4 5 6 7 8 9} {
      set txt "$txt
o<ifr$s> if \[#<system> EQ $s\]
o<ifu$s>   if \[#<rotate> NE 0.0\]
             g10l2p$s r0 ;reset $s rotation
            (debug,$gno($s) UNrotated)
o<ifu$s>   endif
o<ifr$s> endif
"
    }
    set txt "$txt
o<ifx$s> if \[#<system> NE 1\]
           (debug, m2 restores g54)
o<ifx$s> endif
"
    puts $fd $txt
  }
  puts $fd "o<$sname> endsub"
  close $fd

  if !$::ttt(multiplepages) {
    if [info exists ::ttt(hdl)] {
      # user may have already deleted it:
      if [::ngcgui::pageexists $:::ttt(hdl)] {
        ::ngcgui::deletepage $::ttt(pagename)
      }
      unset ::ttt(hdl)
    }
  }

  set ::ttt(msg) "[_ "Creating new tab page"]"
  $::ttt(msg,widget) configure -fg $::ttt(color,black)
  update
  after 200
  # NB: using option expandsub lets the ofile be located anywhere
  set ::ttt(hdl) [::ngcgui::embed_in_axis_tab \
                    [::ngcgui::getngcgui_frame ttt-$::ttt(instance)] \
                    subfile=$ofile \
                    preamble=$::ttt(preamble) \
                    options=$::ttt(embed,options) \
                 ]
  ::ngcgui::raiselastpage
  incr ::ttt(instance)
} ;# runttt

package provide Ngcguittt 1.0
::ttt::embedinit
