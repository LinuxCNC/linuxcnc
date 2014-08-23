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

# Each label, button, checkbutton, radiobutton, and menubutton can have
# an underlined accelerator key.
#
# To activate an accelerator key K, Alt+K is pressed.
#
# If only one item has this accelerator, then the associated widget
# is invoked.  If the associated "widget" is a group of radio buttons,
# the next option is selected.  For buttons, the associated widget is
# the button itself.  For labels, the associated widget is intended
# to be the same as the widget selected by "label_button" (though the
# definition of label_hotkey is different)
#
# If more than one item has this accelerator, then focus is merely moved
# to the item.


proc make_accel { label underline } {
    if {$underline == -1} {
        regsub -all {([\\_])} $label {\\\1} label
        return $label
    }
    set a [string range $label 0 [expr $underline-1]]
    set b [string range $label $underline end]
    regsub -all {([\\_])} $a {\\\1} a
    regsub -all {([\\_])} $b {\\\1} b
    return "${a}_${b}"
}

proc decode_accel { label } {
    set index -1
    if {[regexp {^([^\\_]|\\.)*_} $label match]} {
        set index [expr [string length $match]-1]
        set a [string range $label 0 [expr $index-1]]
        set b [string range $label [expr $index+1] end]
        regsub -all {\\(.)} $a {\1} a    
        regsub -all {\\(.)} $b {\1} b    
        set index [string length $a]
        set label ${a}$b
        return [list $index $label]
    }
    regsub -all {\\(.)} $label {\1} label    
    return [list -1 $label]
}

proc clear_accel_map {} {
    global _accel _last_accel
    catch { unset _accel }
    set _last_accel {}
}

proc label_hotkey {w {activate 1}} {
    set x [tk_focusNext $w]
    set y [tk_focusNext $x]

    set px [winfo parent $x]
    set py [winfo parent $y]

    if {$px == $py} {
        # Next two choices are siblings
        # must be a radiobutton
        set idx -1
        set spx [pack slaves $px]
        foreach k $spx {
            set v [$k cget -variable]
            upvar \#0 $v val
            if {[$k cget -value] == $val} {
                set idx [lsearch $spx $k]
            }
        }
        if {$activate} {
            set idx [expr ($idx+1) % [llength $spx]]
        } elseif {$idx == -1} {
                set idx 0
        }
        set w [lindex $spx $idx]
    } else {
        set w $x
    }
    if {$activate} {
        switch [winfo class $w] {
            Button -
            Checkbutton -
            Radiobutton -
            Menubutton {
                $w invoke
            }
            Entry {
                $w select 0 end
            }
            Combobox {
                $w open
            }
            default {}
        }
    }
    tkTabToWindow $w
}

proc see_accel {w k} {
    global _accel _last_accel _last_widget
    set w [winfo toplevel $w]
    if {![info exists _accel($k,$w)]} { return 0 }
    set accel $_accel($k,$w)
    if {$k == $_last_accel} {
        set idx [expr ([lsearch $accel $_last_widget]+1) % [llength $accel]]
    } else {
        set idx 0
    }
    set w [lindex $accel $idx]
    set _last_accel $k
    set _last_widget $w
    set k [winfo class $w]
    switch $k {
        Button -
        Menubutton -
        Checkbutton -
        Radiobutton {
            if {[llength $accel] == 1} {
                $w invoke
            } else {
                focus $w
            }
        }
        Label {
            label_hotkey $w [expr [llength $accel] == 1]
        }
    }
    return 1
}

bind all <Alt-Key> [concat {if {[see_accel %W %A]} break;} [bind all <Alt-Key>]]

proc setup_menu_accel { path item label } {
    set li [decode_accel $label]
    set index [lindex $li 0]
    set label [lindex $li 1]
    $path entryconfigure $item -underline $index -label $label
}

proc setup_widget_accel { path label } {
    set li [decode_accel $label]
    set index [lindex $li 0]
    set label [lindex $li 1]
    $path configure -underline $index -text $label
    if {$index != -1} {
        global _accel
        set char [string tolower [string index $label $index]]
        set w [winfo toplevel $path]
        lappend _accel($char,$w) $path
    }
}

proc extract_text { label } {
    set li [decode_accel $label]
    lindex $li 1
}

proc sbind { tag seq body } {
    catch { bind $tag $seq $body }
}

sbind all <Key-ISO_Left_Tab> { tkTabToWindow [tk_focusPrev %W] }

if {[info procs ::tk::TabToWindow] != {}} {
    proc tkTabToWindow w [info body ::tk::TabToWindow]
}

clear_accel_map

# vim:sw=4:sts=4:
