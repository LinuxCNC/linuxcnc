#!/bin/sh
# the next line restarts using emcsh \
exec $LINUXCNC_EMCSH "$0" "$@"


# Load the linuxcnc.tcl file, which defines variables for various useful paths
source [file join [file dirname [info script]] .. linuxcnc.tcl]
eval emc_init $argv

###############################################################
# Description:  emctuning.tcl
#               EMC system identification and autotuning program
#
#  Derived from a work by Fred Proctor & Will Shackleford
#  License: GPL Version 2
#
#  Copyright (c) 2005-2009 All rights reserved.
###############################################################
# emctuning.tcl
# EMC system identification and autotuning program
# Needs emcsh to run-- this is sourced by tkemc, but it can be run
# standalone. Make sure directory containing emcsh is in your path,
# or edit the exec line above with the path to emcsh.

# check if any emc commands exist, and quit if not
if {! [string length [info commands emc_plat]]} {
    error "emctuning needs to run from \"emcsh\""
}

# set cycle time for updating, using tkemc's displayCycleTime if
# present or our own if not
if {! [info exists displayCycleTime]} {
    set displayCycleTime 100
}

if {! [info exists activeAxis]} {
    set activeAxis 0
}

proc goManual {} {
    emc_machine off
    emc_estop off
    emc_mode manual
}

proc setVoltage {} {
    global stepvolts
    global activeAxis
    emc_axis_enable $activeAxis 1
    emc_axis_set_output $activeAxis $stepvolts
}

proc logIt {} {
   global stepsize
   global activeAxis
   global stepfilename
   global stepssaved
   emc_log_open $stepfilename$stepssaved pos_voltage $stepsize 0 $activeAxis
   emc_wait done
   emc_log_start
}

proc getStep {} {
    goManual
    logIt
    setVoltage
}

proc saveStep {} {
    global stepssaved
    global filestring
    global stepfilename
    append filestring "$stepfilename$stepssaved "
    set stepssaved [expr $stepssaved + 1]
    emc_log_stop
    emc_log_close
}

proc doTune  {} {
    global filestring
    global pgain
    global igain
    global dgain
    global lambda
    set string "$filestring$lambda"
    scan [exec -- /usr/local/nist/emc/system $string] %s%s%s pgain igain dgain
}

# close the tuning window
proc popdownTuning {w} {
    destroy $w
    if {$w == "."} {
        exit
    }
}

# pop up the tuning window
proc popupTuning {{w .logwindow}} {
    global activeAxis
    global ampenable
    global stepvolts
    global stepsize
    global stepssaved
    global stepfilename
    global filestring
    global lambda
    global pgain
    global igain
    global dgain

    set pgain 0
    set igain 0
    set dgain 0

    set c .tuning
    set stepssaved 0
    set filestring ""

    # trap "." as window name, and erase it, so that widgets
    # don't begin with two dots. We'll refer to the passed-in
    # name "w" here; in code following this setup we'll use the
    # modified name "lw"
    if {$w == "."} {
        set lw ""
        wm title $w "EMC Tuning"
    } else {
        set lw $w
        if {[winfo exists $w]} {
            wm deiconify $w
            raise $w
            focus $w
            return
        }
        toplevel $w
        wm title $w "EMC Tuning"
    }

    # use "lw" as name of top level from now on

    # force the button to reflect actual state
    set ampenable [emc_axis_enable $activeAxis]

    label $lw.title -text "Tuning Axis $activeAxis" -width 30 -anchor center

    frame $lw.outvolts
    label $lw.outvolts.lab -text "Step Voltage:" -anchor w
    entry $lw.outvolts.ent -width 10 -textvariable stepvolts

    frame $lw.size
    label $lw.size.lab -text "Number of Samples:" -anchor w
    entry $lw.size.ent -width 10 -textvariable stepsize

    frame $lw.name
    label $lw.name.lab -text "filename:" -anchor w
    entry $lw.name.ent -width 10 -textvariable stepfilename

    frame $lw.stepssaved
    label $lw.stepssaved.lab -text "Number of steps:" -anchor w
    label $lw.stepssaved.value -width 2 -textvariable stepssaved

    frame $lw.step
    button $lw.step.start -text "getStep" -command getStep
    button $lw.step.save -text "saveStep" -command saveStep
    button $lw.step.jogback -text "jogBack"
    bind $lw.step.jogback <ButtonPress-1> {jogNeg $activeAxis}
    bind $lw.step.jogback <ButtonRelease-1> {jogStop}
    frame $lw.lambda
    label $lw.lambda.lab -text "Lambda:" -anchor w
    entry $lw.lambda.ent -width 10 -textvariable lambda

    button $lw.dotune -text "doTune" -command doTune

    frame $lw.p
    frame $lw.i
    frame $lw.d

    label $lw.p.desc -text "P gain"
    label $lw.i.desc -text "I gain"
    label $lw.d.desc -text "D gain"

    label $lw.p.value -textvariable pgain
    label $lw.i.value -textvariable igain
    label $lw.d.value -textvariable dgain

    button $lw.done -text "OK" -default active -command "popdownTuning $w"

    pack $lw.title -side top -fill x
    pack $lw.outvolts -side top -fill x
    pack $lw.outvolts.lab -side left
    pack $lw.outvolts.ent -side right
    pack $lw.size -side top -fill x
    pack $lw.size.lab -side left
    pack $lw.size.ent -side right
    pack $lw.name -side top -fill x
    pack $lw.name.lab -side left
    pack $lw.name.ent -side right
    pack $lw.stepssaved -side top -fill x
    pack $lw.stepssaved.lab -side left
    pack $lw.stepssaved.value -side left
    pack $lw.step -side top -fill x
    pack $lw.step.start -side left
    pack $lw.step.save -side left
    pack $lw.step.jogback -side left
    pack $lw.lambda -side top -fill x
    pack $lw.lambda.lab -side left
    pack $lw.lambda.ent -side right
    pack $lw.dotune -side top
    pack $lw.p -side top -fill x
    pack $lw.p.desc -side left
    pack $lw.p.value -side left
    pack $lw.i -side top -fill x
    pack $lw.i.desc -side left
    pack $lw.i.value -side left
    pack $lw.d -side top -fill x
    pack $lw.d.desc -side left
    pack $lw.d.value -side left
    pack $lw.done -side top

    bind $w <Return> "popdownTuning $w"
}

# if we're not running inside tkemc, then pop us up in root window
if {! [info exists tkemc]} {
    popupTuning .
}
