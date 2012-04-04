#!/bin/sh
# the next line restarts using emcsh \
exec $LINUXCNC_EMCSH "$0" "$@"

####################################################################
# Description:  Set_Coordinates.tcl
#               This file, ' Set_Coordinates.tcl', is a script that lets
#               the user chose between different coordinate systems
#
# AuthorS: Fred Proctor, Ray Henry
# License: GPL Version 2
#    
# Copyright (c) 2005-2009 All rights reserved.
####################################################################
# Graphical display of coordinate system offsets
# Writes revised offsets to xxx.var and reloads global variables
####################################################################

# Load the linuxcnc.tcl file, which defines variables for various useful paths
source [file join [file dirname [info script]] .. linuxcnc.tcl]
eval emc_init $argv

wm title . [msgcat::mc "LinuxCNC Set Coordinate"]

set top [frame .frame -borderwidth 2 -relief raised]
label $top.l1 -text [msgcat::mc "Coordinate System Control Window"]
pack $top.l1 -side top

set paramfilename [emc_ini "PARAMETER_FILE" "RS274NGC"]
if {[string length $paramfilename] == 0} {
    set paramfilename "emc.var"
}

# Reads the number of axi from ini
set numaxis [emc_ini "AXES" "TRAJ"]

# Reads axi names from ini
set nameaxis1 [emc_ini "COORDINATES" "TRAJ"]

set nameaxis "X Y Z A B C U V W"

# put the parm file into an invisible widget
set vartext [text $top.vartext]
$vartext config -state normal
$vartext delete 1.0 end
if {[catch {open $paramfilename} programin]} {
    return
} else {
    $vartext insert end [read $programin]
    catch {close $programin}
}

# Build the coordinates radio buttons.
set sel [frame $top.selectors -borderwidth 2 -relief groove]
radiobutton $sel.r540 -text G54 -variable coordsys -value 5221 -anchor w \
    -command findVarNumbers
radiobutton $sel.r550 -text G55 -variable coordsys -value 5241 -anchor w \
    -command findVarNumbers
radiobutton $sel.r560 -text G56 -variable coordsys -value 5261 -anchor w \
    -command findVarNumbers
radiobutton $sel.r570 -text G57 -variable coordsys -value 5281 -anchor w \
    -command findVarNumbers
radiobutton $sel.r580 -text G58 -variable coordsys -value 5301 -anchor w \
    -command findVarNumbers
radiobutton $sel.r590 -text G59 -variable coordsys -value 5321 -anchor w \
    -command findVarNumbers
radiobutton $sel.r591 -text G59.1 -variable coordsys -value 5341 -anchor w \
    -command findVarNumbers
radiobutton $sel.r592 -text G59.2 -variable coordsys -value 5361 -anchor w \
    -command findVarNumbers
radiobutton $sel.r593 -text G59.3 -variable coordsys -value 5381 -anchor w \
    -command findVarNumbers
pack  $sel.r540 $sel.r550 $sel.r560 $sel.r570 $sel.r580 \
    $sel.r590 $sel.r591 $sel.r592 $sel.r593 -side top -fill x
pack $sel -side left -padx 10 -pady 10

# Build the variable numbers and value entry widgets.
set right [frame $top.col]
set axis [frame $right.coord]
label $axis.name -text [msgcat::mc "Axis "]
label $axis.varnum -text [msgcat::mc "Var # "]
label $axis.varval -text [msgcat::mc "Offset Value "]
label $axis.forceval -text [msgcat::mc "What to Teach"]
grid $axis.name $axis.varnum $axis.varval $axis.forceval -sticky news
for {set i 0} {$i < $numaxis} {incr i} {
  if { [lsearch $nameaxis1 [lindex $nameaxis $i]] != -1 } {
    label  $axis.l$i -text [lindex $nameaxis $i ]  -anchor e
    label $axis.l1$i -textvariable "num$i"  -anchor e
    entry $axis.e$i -textvariable val$i -takefocus 1
    entry $axis.fv$i -textvariable forceval$i
    button $axis.b$i -text [msgcat::mc "Teach"] -command "getLocation $i"
    grid $axis.l$i $axis.l1$i $axis.e$i $axis.fv$i $axis.b$i -sticky news
    bind $axis.e$i <Down>  {shiftFocus 1}
    bind $axis.e$i <Up> {shiftFocus -1}
 }
}
focus $axis.e0

# Build the control button widgets.
set buttons [frame $right.buttons]
button $buttons.b -text [msgcat::mc "Set Old"] -width 12 -command {getLast}
button $buttons.b0 -text [msgcat::mc "Set Zero"] -width 12 -command {getZero}
button $buttons.b2 -text [msgcat::mc "Close"] -width 12 -command {closeWindow}
button $buttons.b4 -text [msgcat::mc "Write"] -width 12 -command {setVarValues ; loadVarFile}
grid $buttons.b0 $buttons.b4 -sticky nsew
grid $buttons.b $buttons.b2 -sticky nsew
pack $axis -side top -fill x -expand yes
pack $buttons -side bottom -fill x -expand yes
pack $right -side left -fill both
pack $top

# Initialize values
for {set i 0} {$i < $numaxis} {incr i} {
    set val$i 0.000000
    set forceval$i 0.000000
}

proc findVarNumbers {} {
    global coordsys
    global numaxis num0 num1 num2 num3 num4 num5 num6 num7 num8
    for {set i 0} {$i < $numaxis} {incr i} {
        set num$i [expr $coordsys +$i]
    }
    findVarValues
}

proc findVarValues {} {
    global vartext
    global numaxis val0 val1 val2 val3 val4 val5 val6 val7 val8
    global num0 num1 num2 num3 num4 num5 num6 num7 num8
    global numaxis oval0 oval1 oval2 oval3 oval4 oval5 oval6 oval7 oval8
    set locate "1.0"
    for {set i 0} {$i < $numaxis} {incr i} {
        set oval$i [set val$i]
        set locate [$vartext search [set num$i] 1.0 ]
        set locate [expr int($locate)]
        set valtemp [$vartext get $locate.4 "$locate.end"]
        set val$i [string trim $valtemp]
    }
}

proc getZero {} {
    global numaxis 
    global val0 val1 val2 val3 val4 val5 val6 val7 val8
    global forceval0 forceval1 forceval2 forceval3 forceval4 forceval5 forceval6 forceval7 forceval8
    for {set i 0} {$i < $numaxis} {incr i} {
        set val$i 0.000000
        set forceval$i 0.000000
    }
}

proc getLast {} {
    global numaxis 
    global oval0 oval1 oval2 oval3 oval4 oval5 oval6 oval7 oval8
    global val0 val1 val2 val3 val4 val5 val6 val6 val7 val8
    for {set i 0} {$i < $numaxis} {incr i} {
        set val$i [set oval$i]
    }
}

proc getLocation {axnum} {
    global numaxis 
    global val0 val1 val2 val3 val4 val5 val6 val7 val8
    global forceval0 forceval1 forceval2 forceval3 forceval4 forceval5 forceval6 forceval7 forceval8
    if {$axnum == "all"} {
        for {set i 0} {$i < $numaxis} {incr i} {
            set val$i [expr [emc_abs_act_pos $i] - [set forceval$i]]
        }
    } else {
        set val$axnum [expr [emc_abs_act_pos $axnum] - [set forceval$axnum]]
    }
}

proc setVarValues {} {
    global vartext
    global numaxis val0 val1 val2 val3 val4 val5 val6 val7 val8
    global num0 num1 num2 num3 num4 num5 num6 num7 num8
    for {set i 0} {$i < $numaxis} {incr i} {
        set locate [$vartext search [set num$i] 1.0 ]
        $vartext mark set insert "$locate +5c"
        $vartext delete insert "insert lineend"
        $vartext insert insert [set val$i]
    }
}

proc shiftFocus {whichway} {
    global axis numaxis
    set window [focus]
    set last [string trim [string length $window]]
    set last [expr $last - 1]
    set windowx [string index $window $last]
    set windownext [expr $windowx + $whichway]
    if {$windownext == $numaxis} {
        set windownext 0
    } elseif {$windownext < 0} {
        set windownext [expr $numaxis -1]
    }
    focus [string range $window 0 [expr $last - 1]]$windownext
}

proc saveFile {name} {
    global vartext
    catch {file copy -force $name $name.bak}
    if {[catch {open $name w} fileout]} {
        puts stdout [msgcat::mc "can't save %s" $name]
        return
    }
    puts $fileout [$vartext get 1.0 end]
    catch {close $fileout}
}

proc loadVarFile {} {
    global top paramfilename
    saveFile $paramfilename
    emc_task_plan_init
}

# loads parameter file into interpreter and closes window
proc closeWindow {} {
    loadVarFile
    destroy .
}

proc saveFile {name} {
    global vartext
    catch {file copy -force $name $name.bak}
    if {[catch {open $name w} fileout]} {
        puts stdout [msgcat::mc "can't save %s" $name]
        return
    }
    puts $fileout [$vartext get 1.0 end]
    catch {close $fileout}
}

# set the initial coordinate system and find values.
set coordsys 5221
findVarNumbers

