#  Nf Screen designer for Tk toolkit
#  Copyright (C) 2004 Jeff Epler <jepler@unpythonic.net>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

namespace eval ::sb {}

proc ::sb::arrow_size_vertical {w} {
    set xx [expr [::sb::_$w cget -bd]+1]
    for {set i [expr [::sb::_$w cget -bd]+1]} {1} {incr i} {
        set part [::sb::_$w identify $xx $i]
        if {$part != "arrow1"} { return $i }
    }
}
proc ::sb::arrow_size_horizontal {w} {
    set yy [expr [::sb::_$w cget -bd]+1]
    for {set i [expr [::sb::_$w cget -bd]+1]} {1} {incr i} {
        set part [::sb::_$w identify $i $yy]
        if {$part != "arrow1"} { return $i }
    }
}

image create bitmap ::sb::uparrow -file $imagedir/uparrow.xbm
image create bitmap ::sb::downarrow -file $imagedir/downarrow.xbm
image create bitmap ::sb::leftarrow -file $imagedir/leftarrow.xbm
image create bitmap ::sb::rightarrow -file $imagedir/rightarrow.xbm

bind ::sb::nicer <Configure> { ::sb::nicer_config %W }
proc ::sb::nicer_sb {w} {
    bindtags $w [concat ::sb::nicer [bindtags $w]]
    ::sb::_$w configure -bd 0 -highlightt 0
    ::sb::nicer_config $w
}

proc ::sb::nicer_config {w} {
    set o [::sb::_$w cget -orient]
    if {![winfo exists ${w}._arrow1]} {
        button ${w}._arrow1
    } 
    if {![winfo exists ${w}._arrow2]} {
        button ${w}._arrow2
    } 
    
    if {[string match "v*" $o]} {
        set sz [::sb::arrow_size_vertical $w]
        set width [winfo width $w]
        ${w}._arrow1 config -image ::sb::uparrow   -takefocus 0 -highlightt 0 \
            -anchor center
        ${w}._arrow2 config -image ::sb::downarrow -takefocus 0 -highlightt 0 \
            -anchor center
        place ${w}._arrow1 -x 0 -y 0 \
            -width $width -height $sz
        place ${w}._arrow2 -x 0 -y [expr [winfo height $w]-$sz] \
            -width $width -height $sz
    } else {
        set sz [::sb::arrow_size_horizontal $w]
        set height [winfo height $w]
        ${w}._arrow1 config -image ::sb::leftarrow -takefocus 0 -highlightt 0 \
            -anchor center
        ${w}._arrow2 config -image ::sb::rightarrow -takefocus 0 -highlightt 0 \
            -anchor center
        place ${w}._arrow1 -x 0 -y 0 \
            -width $sz -height $height
        place ${w}._arrow2 -x [expr [winfo width $w]-$sz] -y 0 \
            -width $sz -height $height
    }
    foreach b {
        <1> <B1-Leave> <ButtonRelease-1> <B1-Enter> <Leave> <Enter>
    } {
        bind ${w}._arrow1 $b [subst -nocommands {
            event generate $w $b \
                -x [expr %x+[winfo x %W]] \
                -y [expr %y+[winfo y %W]]
        }]
        bind ${w}._arrow2 $b [subst -nocommands {
            event generate $w $b \
                -x [expr %x+[winfo x %W]] \
                -y [expr %y+[winfo y %W]]
        }]
    }
}

proc ::sb::install {} {
    rename ::scrollbar ::sb::real_scrollbar
    proc ::scrollbar { path args } {
        eval [concat ::sb::real_scrollbar $path $args]
        rename $path ::sb::_$path
        proc $path { args } [subst -nocommands {
            set ret [eval [concat ::sb::_$path \$args]]
            ::sb::nicer_config $path
            set ret
        }]
        ::sb::nicer_sb $path
        set path
    }
}
