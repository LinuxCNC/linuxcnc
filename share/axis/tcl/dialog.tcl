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

# dialog.tcl --
#
# This file defines the procedure nf_dialog, which creates a dialog
# box containing an image, a message, and one or more buttons.
#
# RCS: @(#) $Id$
#
# Copyright (c) 1992-1993 The Regents of the University of California.
# Copyright (c) 1994-1997 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

proc patient_grab w {
    set ret [catch { grab $w } res]
    if {!$ret} { return }
    set sei $::errorInfo
    if {$res == "grab failed: another application has grab"
        || $res == "grab failed: window not viewable"} {
        after 100
        after idle patient_grab $w
    } else {
        error $ret $savedInfo
    }
}

#
# nf_dialog:
#
# This procedure displays a dialog box, waits for a button in the dialog
# to be invoked, then returns the index of the selected button.  If the
# dialog somehow gets destroyed, -1 is returned.
#
# Arguments:
# w -		Window to use for dialog top-level.
#
#               If it is a list, then it is of the form {w args}
#               where args (different from the 'args' below) specify
#               extra keyword arguments:
#		    -ext ...: show ... in a scrolling text area below the main
#		              text
# title -	Title to display in dialog's decorative frame.
# text -	Message to display in dialog.
# image -	Image to display in dialog (empty string means none).
# default -	Index of button that is to display the default ring
#		(-1 means none).
# args -	One or more strings to display in buttons across the
#		bottom of the dialog box.

proc nf_dialog_default {t n i} {
    for {set j 0} {$j < $n} {incr j} {
	if {$i == $j} {
	    $t.button$j configure -default active
	} else {
	    $t.button$j configure -default normal
	}
    }
}

proc nf_dialog {w title text image default args} {
    global tkPriv tcl_platform

    set pargs [lrange $w 1 end]
    set w [lindex $w 0]

    set ext {}
    foreach {k v} $pargs {
	switch -- $k {
	-ext { set ext $v }
	default { error "nf_dialog: unexpected positional argument $k $v" }
	}
    }

    if {[llength $default] != 1} {
        set accel $default
        set default [lsearch $accel -2]
    } else {
        set accel {}
    }

    # 1. Create the top-level window and divide it into top
    # and bottom parts.

    catch {destroy $w}
    toplevel $w -class Dialog
    wm title $w $title
    wm iconname $w Dialog
    wm protocol $w WM_DELETE_WINDOW { }
    wm resiz $w 0 0

    # The following command means that the dialog won't be posted if
    # [winfo parent $w] is iconified, but it's really needed;  otherwise
    # the dialog can become obscured by other windows in the application,
    # even though its grab keeps the rest of the application from being used.

    wm transient $w [winfo toplevel [winfo parent $w]]
    if {![string compare $tcl_platform(platform) "macintosh"]} {
	unsupported1 style $w dBoxProc
    }

    frame $w.bot
    frame $w.top 
    if {[llength $args] == 1} {
	pack $w.bot -side bottom -fill both
    } else {
	pack $w.bot -side bottom -fill none -anchor e -expand 1
    }
    pack $w.top -side top -fill both -expand 1

    # 2. Fill the top part with image and message (use the option
    # database for -wraplength and -font so that they can be
    # overridden by the caller).

    option add *Dialog.msg.wrapLength 3i widgetDefault
    if {![string compare $tcl_platform(platform) "macintosh"]} {
	option add *Dialog.msg.font system widgetDefault
    } else {
	option add *Dialog.msg.font {Times 12} widgetDefault
    }

    label $w.msg -justify left -text $text
    pack $w.msg -in $w.top -side right -expand 1 -fill both -padx 3m -pady 3m
    if {[string compare $image ""]} {
      if {![string compare $tcl_platform(platform) "macintosh"] && ![string compare $image "error"]} {
	    set image "stop"
	}
	label $w.image -image [load_image std_$image]
	pack $w.image -in $w.top -side left -padx 3m -pady 3m
    }

    if {$ext != {}} {
	frame $w.ext
	text $w.ext.t -yscrollcommand [list $w.ext.s set] -wrap word
	scrollbar $w.ext.s -command [list $w.ext.t yview] -orient v
	pack $w.ext.t -side left -fill both -expand 1
	pack $w.ext.s -side left -fill y
	$w.ext.t insert end $ext
	$w.ext.t configure -state disabled
	pack $w.ext -side top
    }

    # 3. Create a row of buttons at the bottom of the dialog.

    set i 0
    set l [llength $args]
    foreach but $args {
	button $w.button$i -text $but -command "set tkPriv(button) $i" \
            -width 10 -height 1 -padx 0 -pady .25 

        set u [lindex $accel $i]

	bind $w.button$i <FocusIn> [list nf_dialog_default $w $l $i]

        if {$u == -3} {
            bind $w <Escape> "$w.button$i flash; set tkPriv(button) $i"
        }
	bind $w.button$i <Return> {%W flash; %W invoke}

        if {$u >= 0} {
            set c [string index $but $u]
            bind $w "[string tolower $c]" \
                    "$w.button$i flash; set tkPriv(button) $i"
            $w.button$i configure -underline $u
        }

	grid $w.button$i -in $w.bot -column $i -row 0 -sticky ew -padx 3 -pady 3
	grid columnconfigure $w.bot $i

        set f [$w.button$i cget -font]
        set bwidth [expr 9 * [font measure $f "0"]]
        set twidth [font measure $f $but]
        if {$twidth > $bwidth} {
            $w.button$i configure -width 0 -padx .25m
        }

	incr i
    }

    # 4. Create a <Destroy> binding for the window that sets the
    # button variable to -1;  this is needed in case something happens
    # that destroys the window, such as its parent window being destroyed.

    bind $w <Destroy> {set tkPriv(button) -1}

    # 5. Withdraw the window, then update all the geometry information
    # so we know how big it wants to be, then center the window in the
    # display and de-iconify it.

    wm withdraw $w
    update idletasks

    set parent [winfo parent $w]
    if {[winfo viewable $parent]} { 
        set x [expr {[winfo rootx $parent]+([winfo reqwidth $parent]-[winfo reqwidth $w])/2}]
        set y [expr {[winfo rooty $parent]+([winfo reqheight $parent]-[winfo reqheight $w])/2}]
    } else {
        set x [expr {([winfo screenwidth $w]-[winfo reqwidth $w])/2}]
        set y [expr {([winfo screenheight $w]-[winfo reqheight $w])/2}]
    }
    wm geom $w +$x+$y
    wm deiconify $w

    # 6. Set a grab and claim the focus too.

    set oldFocus [focus]
    set oldGrab [grab current $w]
    if {[string compare $oldGrab ""]} {
	set grabStatus [grab status $oldGrab]
    }
    patient_grab $w
    if {$default >= 0} {
	focus $w.button$default
    } else {
	focus $w
    }

    # 7. Wait for the user to respond, then restore the focus and
    # return the index of the selected button.  Restore the focus
    # before deleting the window, since otherwise the window manager
    # may take the focus away so we can't redirect it.  Finally,
    # restore any grab that was in effect.

    tkwait variable tkPriv(button)
    catch {focus $oldFocus}
    catch {
	# It's possible that the window has already been destroyed,
	# hence this "catch".  Delete the Destroy handler so that
	# tkPriv(button) doesn't get reset by it.

	bind $w <Destroy> {}
	destroy $w
    }
    if {[string compare $oldGrab ""]} {
      if {[string compare $grabStatus "global"]} {
	    grab $oldGrab
      } else {
          grab -global $oldGrab
	}
    }
    return $tkPriv(button)
}

# vim:sw=4:sts=4:
