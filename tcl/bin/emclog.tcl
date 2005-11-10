#!/bin/sh
# we need to find the tcl dir, it was exported from emc.run \
export EMC2_TCL_DIR
# the next line restarts using iosh \
exec $EMC2_EMCSH "$0" "$@"

# emclog.tcl
# EMC data logger
# Needs emcsh to run-- this is sourced by tkemc, but it can be run
# standalone. Make sure directory containing emcsh is in your path,
# or edit the exec line above with the path to emcsh.

# check if any emc commands exist, and quit if not
if {! [string length [info commands emc_plat]]} {
    error "emclog needs to run from \"emcsh\""
}

# set cycle time for updating, using tkemc's displayCycleTime if
# present or our own if not
if {! [info exists displayCycleTime]} {
    set displayCycleTime 100
}

if {! [info exists activeAxis]} {
    set activeAxis 0
}

# check for existence of 'gnuplot'; if it's there, we can plot,
# otherwise we'll dim the plot button
set noPlotting [catch {exec echo exit | gnuplot}]

proc updateLog {} {
    global isopen islogging numpoints displayCycleTime

    if {[emc_log_isopen]} {
        set isopen "Open"
    } else {
        set isopen "Closed"
    }

    if {[emc_log_islogging]} {
        set islogging "Logging"
    } else {
        set islogging "Not Logging"
    }

    set numpoints [emc_log_numpoints]

    after $displayCycleTime updateLog
}

set triggertype "manual"
set triggervar "volt"
set triggerthreshold 0.1
set use_mktemp 1
set tempfile "/tmp/emc.plot.sh.XXXXXX"


proc startLog {} {
    global fileentry typeentry sizeentry skipentry whichentry
    global triggertype triggervar triggerthreshold
    global use_mktemp tempfile

    set tempfile $fileentry
    if { $use_mktemp == 1 } { 
	set tempfile [ exec mktemp $fileentry.XXXXXX ]
    }

    # send the open cmd
    emc_log_open $tempfile $typeentry $sizeentry $skipentry $triggertype $triggervar $triggerthreshold  $whichentry
    # now wait until it's done
    emc_wait done
    # now start the log
    emc_log_start
}

proc stopLog {} {
    emc_log_stop
}

proc saveLog {} {
    emc_log_close
}

proc plotLog {} {
    global fileentry typeentry tempfile

    # we'll use gnuplot with the -persist option, which leaves the
    # plot window up after gnuplot exits. Backgrounding the exec 
    # with the & lets control return immediately and keeps the GUI
    # from locking up while the plot window is up.
    # The "using" string will hold the specific values to be plotted,
    # which differ in each log type

    if {! [ catch { exec test -x $tempfile  } ] } {
	catch { exec $tempfile > $tempfile.stdout 2> $tempfile.stderr }
	return
    }
    # compose the plot string
    set s {plot \"$tempfile\"}

    # FIXME This is legacy code. (Now in usrmotintf.c)

    # now compose the 'using' strings so that number of plots, labels match log
    if {$typeentry == "axis_pos"} {
	set u {using 1:2 title \"cmd pos\", \"\" using 1:3 title \"act pos\"}
    } elseif {$typeentry == "axis_vel"} {
	set u {using 1:2 title \"cmd vel\", \"\" using 1:3 title \"act vel\"}
    } elseif {$typeentry == "axes_inpos"} {
	set u {using 1:2 title \"axis 0\", \"\" using 1:3 title \"axis 1\", \"\" using 1:4 title \"axis 2\"}
    } elseif {$typeentry == "axes_outpos"} {
	set u {using 1:2 title \"axis 0\", \"\" using 1:3 title \"axis 1\", \"\" using 1:4 title \"axis 2\"}
    } elseif {$typeentry == "axes_ferror"} {
	set u {using 1:2 title \"axis 0\", \"\" using 1:3 title \"axis 1\", \"\" using 1:4 title \"axis 2\"}
    } elseif {$typeentry == "traj_pos"} {
	set u {using 1:2 title \"X\", \"\" using 1:3 title \"Y\", \"\" using 1:4 title \"Z\"}
    } elseif {$typeentry == "traj_vel"} {
	set u {using 1:2 title \"Vx\", \"\" using 1:3 title \"Vy\", \"\" using 1:4 title \"Vz\"}
    } elseif {$typeentry == "traj_acc"} {
	set u {using 1:2 title \"Ax\", \"\" using 1:3 title \"Ay\", \"\" using 1:4 title \"Az\"}
    } elseif {$typeentry == "pos_voltage"} {
	set u {using 1:2 title \"pos\", \"\" using 1:3 title \"volts\"}
   }

    # compose the gnuplot string
    set p {gnuplot -persist}

    # now do the plot
    eval exec echo $s $u | $p &
}

proc popdownLog {w} {
    after cancel updateLog
    destroy $w
    if {$w == "."} {
        exit
    }
}

proc popupLog {{w .logwindow}} {
    global fileentry typeentry sizeentry skipentry whichentry
    global triggertype triggervar triggerthreshold
    
    global typelabel
    global isopen islogging numpoints
    global activeAxis
    global noPlotting

    # trap "." as window name, and erase it, so that widgets
    # don't begin with two dots. We'll refer to the passed-in
    # name "w" here; in code following this setup we'll use the
    # modified name "lw"
    if {$w == "."} {
        set lw ""
        wm title $w "EMC Logging"
    } else {
        set lw $w
        if {[winfo exists $w]} {
            wm deiconify $w
            raise $w
            focus $w
            return
        }
        toplevel $w
        wm title $w "EMC Logging"
    }

    # use "lw" as name of top level from now on

    set fileentry "/tmp/emc.plot.sh"
    set typeentry "axis_pos"
    set sizeentry 1000
    set skipentry 0
    set whichentry $activeAxis
    set triggertype "manual"
    set triggervar "volt"
    set triggerthreshold 0.1

    set f [frame $lw.type]
    pack $f -side top -fill x
    label $f.lbl -text "Type:" -anchor w
    set typelabel "Axis Position"
    menubutton $f.mb -textvariable typelabel -direction below -menu $f.mb.m -relief raised -width 20
    menu $f.mb.m -tearoff 0
    $f.mb.m add command -label "Axis Position" -command {set typelabel "Axis Position" ; set typeentry "axis_pos"}
    $f.mb.m add command -label "All Input Positions" -command {set typelabel "All Input Positions" ; set typeentry "axes_inpos"}
    $f.mb.m add command -label "All Output Positions" -command {set typelabel "All Output Positions" ; set typeentry "axes_outpos"}
    $f.mb.m add command -label "Axis Velocity" -command {set typelabel "Axis Velocity" ; set typeentry "axis_vel"}
    $f.mb.m add command -label "All Following Errors" -command {set typelabel "All Following Errors" ; set typeentry "axes_ferror"}
    $f.mb.m add command -label "Trajectory Points" -command {set typelabel "Trajectory Points" ; set typeentry "traj_pos"}
    $f.mb.m add command -label "Trajectory Vels" -command {set typelabel "Trajectory Vels" ; set typeentry "traj_vel"}
    $f.mb.m add command -label "Trajectory Accs" -command {set typelabel "Trajectory Accs" ; set typeentry "traj_acc"}
    $f.mb.m add command -label "Pos and Voltage" -command {set typelabel "Pos and Voltage" ; set typeentry "pos_voltage"}
    pack $f.lbl -side left
    pack $f.mb -side right

    set f [frame $lw.file]
    pack $f -side top -fill x
    label $f.lbl -text "File:" -anchor w
    entry $f.ent -textvariable fileentry
    pack $f.lbl -side left
    pack $f.ent -side right

    set f [frame $lw.size]
    pack $f -side top -fill x
    label $f.lbl -text "Size:" -anchor w
    entry $f.ent -textvariable sizeentry
    pack $f.lbl -side left
    pack $f.ent -side right

    set f [frame $lw.skip]
    pack $f -side top -fill x
    label $f.lbl -text "Skip:" -anchor w
    entry $f.ent -textvariable skipentry
    pack $f.lbl -side left
    pack $f.ent -side right

    set f [frame $lw.which]
    pack $f -side top -fill x
    label $f.lbl -text "Which:" -anchor w
    entry $f.ent -textvariable whichentry
    pack $f.lbl -side left
    pack $f.ent -side right


    set f [frame $lw.triggertype]
    pack $f -side top -fill x
    label $f.lbl -text "Trigger Type:" -anchor w
    set triggertypelabel "Manual"
    menubutton $f.mb -textvariable triggertypelabel -direction below -menu $f.mb.m -relief raised -width 20
    menu $f.mb.m -tearoff 0
    $f.mb.m add command -label "Manual" -command {set triggertypelabel "Manual" ; set triggertype "manual"}
    $f.mb.m add command -label "Delta" -command {set triggertypelabel "Delta" ; set triggertype "delta"}
    $f.mb.m add command -label "Over" -command {set triggertypelabel "Over" ; set triggertype "over"}
    $f.mb.m add command -label "Under" -command {set triggertypelabel "Under" ; set triggertype "under"}
    pack $f.lbl -side left
    pack $f.mb -side right


    set f [frame $lw.triggervar]
    pack $f -side top -fill x
    label $f.lbl -text "Trigger Var:" -anchor w
    set triggervarlabel "Position"
    menubutton $f.mb -textvariable triggervarlabel -direction below -menu $f.mb.m -relief raised -width 20
    menu $f.mb.m -tearoff 0
    $f.mb.m add command -label "Position" -command {set triggervarlabel "Position" ; set triggervar "pos"}
    $f.mb.m add command -label "Volt" -command {set triggervarlabel "Volt" ; set triggervar "volt"}
    $f.mb.m add command -label "Following Error" -command {set triggervarlabel "Following Error" ; set triggervar "ferror"}
    $f.mb.m add command -label "Velocity" -command {set triggervarlabel "Velocity" ; set triggervar "vel"}
    pack $f.lbl -side left
    pack $f.mb -side right

    set f [frame $lw.triggerthreshold]
    pack $f -side top -fill x
    label $f.lbl -text "Trigger Threshold:" -anchor w
    entry $f.ent -textvariable triggerthreshold
    pack $f.lbl -side left
    pack $f.ent -side right

    set f [frame $lw.stat]
    pack $f -side top -fill x -pady 2m
    label $f.isopen -width 10 -textvariable isopen -relief sunken
    label $f.islogging -width 10 -textvariable islogging -relief sunken
    label $f.numpoints -width 10 -textvariable numpoints -relief sunken
    pack $f.isopen $f.islogging $f.numpoints -side left -expand 1

    set f [frame $lw.cmdbuttons]
    pack $f -side top
    button $f.start -text "Start" -command startLog
    button $f.stop -text "Stop" -command stopLog
    button $f.save -text "Save" -command saveLog
    button $f.plot -text "Plot" -command plotLog
    if {$noPlotting} {
	$f.plot config -state disabled
    }
    pack $f.start $f.stop $f.save $f.plot -side left -expand 1

    # note that the commands for these buttons use the "w" widget name,
    # so that if "." was passed, it's not stripped off

    set f [frame $lw.buttons]
    pack $f -side bottom -fill x -pady 2m
    button $f.done -text "Done" -default active -command "popdownLog $w"
    pack $f.done -side left -expand 1
    bind $w <Return> "popdownLog $w"

    updateLog
}

# if we're not running inside tkemc, then pop us up in root window
if {! [info exists tkemc]} {
    popupLog .
}
