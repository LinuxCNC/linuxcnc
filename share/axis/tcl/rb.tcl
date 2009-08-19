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

namespace eval ::rb {}
foreach b [bind Radiobutton] {
    bind ::rb::nicer_rb $b { after cancel ::rb::nicer_rb_update %W; after idle ::rb::nicer_rb_update %W }
    bind ::rb::nicer_rb_b $b [subst -nocommands {
        event generate [winfo parent %W] $b \
            -button %b -state %s -time %t -x %x -y %y \
            -rootx %X -rooty %Y
        }]
}

foreach b {<Enter> <Leave>} {
    bind ::rb::nicer_rb_b $b {}
}

bind ::rb::nicer_rb <Configure> { after cancel ::rb::nicer_rb_configure %W; after idle ::rb::nicer_rb_configure %W }

image create photo ::rb::radio.normal.0   -file $imagedir/rbn0.gif
image create photo ::rb::radio.disabled.0 -file $imagedir/rbd0.gif
image create photo ::rb::radio.active.0   -file $imagedir/rba0.gif
image create photo ::rb::radio.normal.1   -file $imagedir/rbn1.gif
image create photo ::rb::radio.disabled.1 -file $imagedir/rbd1.gif
image create photo ::rb::radio.active.1   -file $imagedir/rba1.gif

proc ::rb::nicer_rb_update {w args} {
    if {![winfo exists $w]} { return }
    set state [::rb::_$w cget -state]
    set var   [::rb::_$w cget -variable]
    upvar \#0 $var val
    set is_on [expr {"[::rb::_$w cget -value]" == "$val"}]
    if {$state == "disabled"} { set bstate normal } else { set bstate $state }
    $w._button configure -image ::rb::radio.$state.$is_on -state $bstate
}

proc ::rb::nicer_rb {w} {
    set v [::rb::_$w cget -variable]
    upvar \#0 $v var
    trace variable var w [list ::rb::nicer_rb_update $w]
    button $w._button -bd 0 -highlightt 0 -takefocus 0
    bind $w._button <Destroy> [list trace vdelete $v w [list ::rb::nicer_rb_update $w]]
    ::rb::nicer_rb_configure $w
}

proc ::rb::nicer_rb_configure {w} {
    if {![::rb::_$w cget -indicatoron]} { place forget $w._button; return }
    set anchor [::rb::_$w cget -anchor]
    if {[::rb::_$w cget -width] != 0 && $anchor != "w"
            && $anchor != "nw" && $anchor != "sw"} {
        ::rb::_$w configure -anchor w
    }
    set hasimage [expr {"[::rb::_$w cget -image]" != ""}]
    set loss [expr 2*[::rb::_$w cget -bd] - 2*[::rb::_$w cget -highlightt]]
    if {$hasimage} {
        set sz [winfo reqheight $w] 
    } else {
        set sz [font metrics [::rb::_$w cget -font] -linespace]
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
    set height [expr [winfo height $w]-2*[::rb::_$w cget -highlightthickness]-2]
    place $w._button -x $x -y 0 -width $sz -height $height
    if {[lsearch [bindtags $w] ::rb::nicer_rb] == -1} {
        bindtags $w [concat ::rb::nicer_rb [bindtags $w]]
        bindtags $w._button [list ::rb::nicer_rb_b [winfo toplevel $w] all]
    }
    ::rb::nicer_rb_update $w 
}

# If there is an image, then the height of the button is the
# reqheight of the widget.  Otherwise, the height of the button
# is the height of one line of text.  The width is the same
# as the height. (if there's an explicit width, we're fucked
# because 'font measure' sucks)
#
# If the anchor is center, n, or s then the left-hand side
# of the button is at ([winfo width $w]-[winfo reqwidth $w])/2
# If the anchor is w, nw, or sw, then the left-hand side of the
# button is at 0
# If the anchor is e, ne, or se, then the left-hand side of the
# button is at [winfo width $w]-[winfo reqwidth $w]

proc ::rb::install {} {
    if {$::tk_version >= "8.5"} { return }
    rename ::radiobutton ::rb::real_radiobutton
    proc ::radiobutton { path args } {
        eval [concat ::rb::real_radiobutton $path $args]
        rename $path ::rb::_$path
        proc $path { args } [subst -nocommands {
            set ret [eval [concat ::rb::_$path \$args]]
            ::rb::nicer_rb_configure $path
            set ret
        }]
        ::rb::nicer_rb $path
        set path
    }
}
