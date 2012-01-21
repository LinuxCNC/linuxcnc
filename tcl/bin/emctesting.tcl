#!/bin/sh
# the next line restarts using emcsh \
exec $LINUXCNC_EMCSH "$0" "$@"

source [file join [file dirname [info script]] .. linuxcnc.tcl]

###############################################################
# Description:  emctesting.tcl
#               EMC performance testing program
#
#  Derived from a work by Fred Proctor & Will Shackleford
#  License: GPL Version 2
#
#  Copyright (c) 2005-2009 All rights reserved.
###############################################################
# emctesting.tcl
# EMC performance testing program
# Needs emcsh to run-- this is sourced by tkemc, but it can be run
# standalone. Make sure directory containing emcsh is in your path,
# or edit the exec line above with the path to emcsh.

eval emc_init $argv

# check if any emc commands exist, and quit if not
if {! [string length [info commands emc_plat]]} {
    error "emctesting needs to run from \"emcsh\""
}

if {! [info exists activeAxis]} {
    set activeAxis 0
}

proc popdownTesting {w} {
    destroy $w
    if {$w == "."} {
        exit
    }
}

proc popupTesting {{w .testingwindow}} {
    global axisentry outvolts ampenable
    global activeAxis

    # trap "." as window name, and erase it, so that widgets
    # don't begin with two dots. We'll refer to the passed-in
    # name "w" here; in code following this setup we'll use the
    # modified name "lw"
    if {$w == "."} {
        set lw ""
        wm title $w "EMC Testing"
    } else {
        set lw $w
        if {[winfo exists $w]} {
            wm deiconify $w
            raise $w
            focus $w
            return
        }
        toplevel $w
        wm title $w "EMC Testing"
    }

    # use "lw" as name of top level from now on

    # force the selected axis to be the active axis
    set axisentry $activeAxis
    # force the button to reflect actual state
    set ampenable [emc_axis_enable $activeAxis]
    # force the volts to 0, a valid entry
    set outvolts 0.0

    frame $lw.axis
    label $lw.axis.lab -text "Axis:" -width 20 -anchor w
    entry $lw.axis.ent -width 10 -textvariable axisentry
    pack $lw.axis -side top -fill x
    pack $lw.axis.lab -side left
    pack $lw.axis.ent -side left

    frame $lw.outvolts
    label $lw.outvolts.lab -text "Output Voltage:" -width 20 -anchor w
    entry $lw.outvolts.ent -width 10 -textvariable outvolts
    pack $lw.outvolts -side top -fill x
    pack $lw.outvolts.lab -side left
    pack $lw.outvolts.ent -side left

    frame $lw.amp
    checkbutton $lw.amp.chk -text "Amp Enable" -variable ampenable -relief flat
    pack $lw.amp -side top -fill x
    pack $lw.amp.chk -side left
    bind $lw.amp.chk <ButtonRelease-1> "emc_axis_enable \$axisentry \$ampenable"

    frame $lw.buttons
    button $lw.buttons.set -text Set -command "emc_axis_set_output \$axisentry \$outvolts"
    button $lw.buttons.done -default active -text Done -command "popdownTesting $w"

    pack $lw.buttons -side bottom -fill x -pady 2m
    pack $lw.buttons.set -side left -expand 1
    pack $lw.buttons.done -side left -expand 1
    bind $w <Return> "popdownTesting $w"
}

# if we're not running inside tkemc, then pop us up in root window
if {! [info exists tkemc]} {
    popupTesting .
}
