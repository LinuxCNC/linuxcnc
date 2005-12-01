#!/bin/sh
# we need to find the tcl dir, it was exported from emc.run \
export EMC2_TCL_DIR
# the next line restarts using emcsh \
exec $EMC2_EMCSH "$0" "$@"

# emccalib.tcl
# EMC parameter setting application
# Needs emcsh to run-- this is sourced by tkemc, but it can be run
# standalone. Make sure directory containing emcsh is in your path,
# or edit the exec line above with the path to emcsh.

# check if any emc commands exist, and quit if not
if {! [string length [info commands emc_plat]]} {
    error "emccalib needs to run from \"emcsh\""
}

if {! [info exists activeAxis]} {
    set activeAxis 0
}

proc popdownCalibration {w} {
    destroy $w
    if {$w == "."} {
        exit
    }
}

proc popupCalibration {{w .calibwindow}} {
    global axisentry pgain igain dgain ff0 ff1 ff2
    global backlash bias maxerror deadband
    global activeAxis

    # trap "." as window name, and erase it, so that widgets
    # don't begin with two dots. We'll refer to the passed-in
    # name "w" here; in code following this setup we'll use the
    # modified name "lw"
    if {$w == "."} {
        set lw ""
        wm title $w "EMC Calibration"
    } else {
        set lw $w
        if {[winfo exists $w]} {
            wm deiconify $w
            raise $w
            focus $w
            return
        }
        toplevel $w
        wm title $w "EMC Calibration"
    }

    # use "lw" as name of top level from now on

    # force the selected axis to be the active axis
    set axisentry $activeAxis

    # get the parameters from the controller
    set pgain [emc_axis_gains $activeAxis p]
    set igain [emc_axis_gains $activeAxis i]
    set dgain [emc_axis_gains $activeAxis d]
    set ff0 [emc_axis_gains $activeAxis ff0]
    set ff1 [emc_axis_gains $activeAxis ff1]
    set ff2 [emc_axis_gains $activeAxis ff2]
    set backlash [emc_axis_backlash $activeAxis]
    set bias [emc_axis_gains $activeAxis bias]
    set maxerror [emc_axis_gains $activeAxis maxerror]
    set deadband [emc_axis_gains $activeAxis deadband]

    label $lw.title -text "Axis Calibration" -width 30 -anchor center
    pack $lw.title -side top -fill x

    frame $lw.axis
    label $lw.axis.lab -text "Axis:" -anchor w
    entry $lw.axis.ent -width 20 -textvariable axisentry
    pack $lw.axis -side top -fill x
    pack $lw.axis.lab -side left
    pack $lw.axis.ent -side right

    frame $lw.p
    label $lw.p.lab -text "P Gain:" -anchor w
    entry $lw.p.ent -width 20 -textvariable pgain
    pack $lw.p -side top -fill x
    pack $lw.p.lab -side left
    pack $lw.p.ent -side right

    frame $lw.i
    label $lw.i.lab -text "I Gain:" -anchor w
    entry $lw.i.ent -width 20 -textvariable igain
    pack $lw.i -side top -fill x
    pack $lw.i.lab -side left
    pack $lw.i.ent -side right

    frame $lw.d
    label $lw.d.lab -text "D Gain:" -anchor w
    entry $lw.d.ent -width 20 -textvariable dgain
    pack $lw.d -side top -fill x
    pack $lw.d.lab -side left
    pack $lw.d.ent -side right

    frame $lw.ff0
    label $lw.ff0.lab -text "Pos FF Gain:" -anchor w
    entry $lw.ff0.ent -width 20 -textvariable ff0
    pack $lw.ff0 -side top -fill x
    pack $lw.ff0.lab -side left
    pack $lw.ff0.ent -side right

    frame $lw.ff1
    label $lw.ff1.lab -text "Vel FF Gain:" -anchor w
    entry $lw.ff1.ent -width 20 -textvariable ff1
    pack $lw.ff1 -side top -fill x
    pack $lw.ff1.lab -side left
    pack $lw.ff1.ent -side right

    frame $lw.ff2
    label $lw.ff2.lab -text "Acc FF Gain:" -anchor w
    entry $lw.ff2.ent -width 20 -textvariable ff2
    pack $lw.ff2 -side top -fill x
    pack $lw.ff2.lab -side left
    pack $lw.ff2.ent -side right

    frame $lw.backlash
    label $lw.backlash.lab -text "Backlash:" -anchor w
    entry $lw.backlash.ent -width 20 -textvariable backlash
    pack $lw.backlash -side top -fill x
    pack $lw.backlash.lab -side left
    pack $lw.backlash.ent -side right

    frame $lw.bias
    label $lw.bias.lab -text "Bias:" -anchor w
    entry $lw.bias.ent -width 20 -textvariable bias
    pack $lw.bias -side top -fill x
    pack $lw.bias.lab -side left
    pack $lw.bias.ent -side right

    frame $lw.maxerror
    label $lw.maxerror.lab -text "Max Cum Error:" -anchor w
    entry $lw.maxerror.ent -width 20 -textvariable maxerror
    pack $lw.maxerror -side top -fill x
    pack $lw.maxerror.lab -side left
    pack $lw.maxerror.ent -side right

    frame $lw.deadband
    label $lw.deadband.lab -text "Deadband:" -anchor w
    entry $lw.deadband.ent -width 20 -textvariable deadband
    pack $lw.deadband -side top -fill x
    pack $lw.deadband.lab -side left
    pack $lw.deadband.ent -side right

    frame $lw.buttons
    button $lw.buttons.ok -text OK -default active -command "emc_axis_gains \$activeAxis all \$pgain \$igain \$dgain \$ff0 \$ff1 \$ff2 \$bias \$maxerror \$deadband; emc_axis_backlash \$activeAxis \$backlash; popdownCalibration $w"
    button $lw.buttons.cancel -text Cancel -command "popdownCalibration $w"

    pack $lw.buttons -side bottom -fill x -pady 2m
    pack $lw.buttons.ok $lw.buttons.cancel -side left -expand 1
    bind $w <Return> "emc_axis_gains \$activeAxis all \$pgain \$igain \$dgain \$ff0 \$ff1 \$ff2 \$bias \$maxerror \$deadband; emc_axis_backlash \$activeAxis \$backlash; popdownCalibration $w"

}

# if we're not running inside tkemc, then pop us up in root window
if {! [info exists tkemc]} {
    popupCalibration .
}
