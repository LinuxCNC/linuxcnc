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
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

namespace eval ::cb {}
foreach b [bind Checkbutton] {
    bind ::cb::nicer_cb $b { after cancel ::cb::nicer_cb_update %W; after idle ::cb::nicer_cb_update %W }
    bind ::cb::nicer_cb_b $b [subst -nocommands {
        event generate [winfo parent %W] $b \
            -button %b -state %s -time %t -x %x -y %y \
            -rootx %X -rooty %Y
        }]
}

foreach b {<Enter> <Leave>} {
    bind ::cb::nicer_cb_b $b {}
}

bind ::cb::nicer_cb <Configure> { after cancel ::cb::nicer_cb_configure %W; after idle ::cb::nicer_cb_configure %W }

image create photo ::cb::check.normal.0   -file $imagedir/cbn0.gif
image create photo ::cb::check.disabled.0 -file $imagedir/cbd0.gif
image create photo ::cb::check.active.0   -file $imagedir/cba0.gif
image create photo ::cb::check.normal.1   -file $imagedir/cbn1.gif
image create photo ::cb::check.disabled.1 -file $imagedir/cbd1.gif
image create photo ::cb::check.active.1   -file $imagedir/cba1.gif

proc ::cb::nicer_cb_update {w args} {
    if {![winfo exists $w]} { return }
    set state [::cb::_$w cget -state]
    set var   [::cb::_$w cget -variable]
    upvar \#0 $var val
    set is_on [expr {"[::cb::_$w cget -onvalue]" == "$val"}]
    if {$state == "disabled"} { set bstate normal } else { set bstate $state }
    $w._button configure -image ::cb::check.$state.$is_on -state $bstate
}

proc ::cb::nicer_cb {w} {
    set v [::cb::_$w cget -variable]
    upvar \#0 $v var
    trace variable var w [list ::cb::nicer_cb_update $w]
    button $w._button -bd 0 -highlightt 0 -takefocus 0
    bind $w._button <Destroy> [list trace vdelete $v w [list ::cb::nicer_cb_update $w]]
    ::cb::nicer_cb_configure $w
}

proc ::cb::nicer_cb_configure {w} {
    if {![::cb::_$w cget -indicatoron]} { place forget $w._button; return }
    set anchor [::cb::_$w cget -anchor]
    if {[::cb::_$w cget -width] != 0 && $anchor != "w"
            && $anchor != "nw" && $anchor != "sw"} {
        ::cb::_$w configure -anchor w
    }
    set hasimage [expr {"[::cb::_$w cget -image]" != ""}]
    set loss [expr 2*[::cb::_$w cget -bd] - 2*[::cb::_$w cget -highlightt]]
    if {$hasimage} {
        set sz [winfo reqheight $w] 
    } else {
        set sz [font metrics [::cb::_$w cget -font] -linespace]
    }
    set sz [expr $sz - $loss]
    set ofs [expr [winfo width $w] - [winfo reqwidth $w]]
    switch $anchor {
        center - n - s {
            set x [expr $ofs/2]
        }
        ne - e - se {
            set x $ofs
        } 
        nw - w - sw {
            set x 0
        }
        default {
            set x 0
        }
    }
    set height [expr [winfo height $w]-2*[::cb::_$w cget -highlightthickness]-2]
    place $w._button -x $x -y 0 -width $sz -height $height
    if {[lsearch [bindtags $w] ::cb::nicer_cb] == -1} {
        bindtags $w [concat ::cb::nicer_cb [bindtags $w]]
        bindtags $w._button [list ::cb::nicer_cb_b [winfo toplevel $w] all]
    }
    ::cb::nicer_cb_update $w 
}

proc ::cb::install {} {
    if {$::tk_version >= "8.5"} { return }
    rename ::checkbutton ::cb::real_checkbutton
    proc ::checkbutton { path args } {
        eval [concat ::cb::real_checkbutton $path $args]
        if {[winfo toplevel $path] != "."} { return $path }
        rename $path ::cb::_$path
        proc $path { args } [subst -nocommands {
            set ret [eval [concat ::cb::_$path \$args]]
            ::cb::nicer_cb_configure $path
            set ret
        }]
        ::cb::nicer_cb $path
        set path
    }
}
