#!/bin/sh
# we need to find the tcl dir, it was exported from emc.run \
export EMC2_TCL_DIR
# the next line restarts using iosh \
exec $EMC2_IOSH "$0" "$@"


set TCLBIN tcl/bin
set TCLSCRIPTS tcl/scripts

if {[info exists env(EMC2_TCL_DIR)]} {
    set TCLBIN $env(EMC2_TCL_DIR)
    set TCLSCRIPTS $env(EMC2_TCL_DIR)
    set TCLBIN $TCLBIN/bin
    set TCLSCRIPTS $TCLSCRIPTS/scripts
}

# TclTk script for tkemc gui

# Notes
# This script presents a canvas with i/o condition displays
# And allows changing of pin values. (use at your own risk)
# You can use this script to trash most anything.

wm title . "IO Set"

# create the main window top frame
set top [frame .ioset]
pack $top -padx 4 -pady 4

message $top.message1 -text "Shows and Sets Hex I/O buffers." \
   -justify center -aspect 300
frame $top.sep  -relief groove -bg black -height 2
pack $top.message1 $top.sep -side top -fill x

set showaddress 378
set tempshowaddress $showaddress
set setaddress 378
set tempsetaddress $setaddress
set value 0

set a [frame $top.addresses]
label $a.l1 -text "Show Address"
label $a.l2 -text "Set Address"
label $a.l3 -text "Value To Set"
entry $a.entry1 -textvariable tempshowaddress -width 7 \
    -justify center -bd 1
button $a.submitshow -bd 1 -text "Enter" \
    -command {set showaddress $tempshowaddress}
entry $a.entry2 -textvariable tempsetaddress -width 7 \
    -justify center -bd 1
button $a.submitset -bd 1 -text "Enter" \
    -command {setAddress}
label $a.entry3 -textvariable value -width 7 \
    -justify center -bd 1 -relief sunken
button $a.submitvalue -bd 1 \
    -font {Helvetica -12 bold} -text "Set" -command {setValue}
grid $a.l1 -sticky ew
grid configure $a.l1 -columnspan 2
grid $a.entry1 $a.submitshow -sticky ew
grid $a.l2 -sticky ew
grid configure $a.l2 -columnspan 2
grid $a.entry2 $a.submitset -sticky ew
grid $a.l3 -sticky ew
grid configure $a.l3 -columnspan 2
grid $a.entry3 $a.submitvalue -sticky ew
pack $a -side top -fill x

set b [frame $top.box -bd 2 -relief groove]
set c [canvas $b.canvas]
$c configure -height 170 -width 50
set d [frame $b.addresses]
set e [frame $b.checkframe]
label $d.l1 -textvariable showaddress -justify left
label $d.l2 -textvariable setaddress -justify center

for {set i 0} {$i < 8} {incr i 1} {
    $c create oval 5 [expr $i * 22 + 6] 15 [expr $i * 22 + 16] -fill green
    checkbutton $e.ckb$i -text $i -variable setnum$i -command {computeValue}
    pack $e.ckb$i -side top -fill y
}
pack $d.l1 -side left -fill x
pack $d.l2 -side right -fill x -expand yes
pack $d -side top -fill x
pack $c $e -side left -fill y
button $top.button1 -bg darkgray\
    -font {Helvetica -12 bold} -text CANCEL -command {destroy .}
pack $b $top.button1 -side top -fill x

proc computeValue {} {
    global value
    set value 0
    for {set j 0} {$j < 8} {incr j} {
        global setnum$j
        switch $j {
            0 {set temp 1}
            1 {set temp 2}
            2 {set temp 4}
            3 {set temp 8}
            4 {set temp 16}
            5 {set temp 32}
            6 {set temp 64}
            7 {set temp 128}
        }
    set value [expr $value + [expr [set setnum$j] * $temp]]
    }
}

proc showAddress {} {
    global tempshowaddress showaddress value c
    if {$showaddress < 0} {
        set tempshowaddress "error"
        set showaddress 0
    } else {
    set value1 0
        set value1 [inb 0x$showaddress]
        for {set i 1} {$i <= 8} {incr i 1} {
            if {$value1 % 2} {
                $c itemconfigure $i -fill red
            } else {
                $c itemconfigure $i -fill green
            }
        set value1 [expr $value1 / 2]
        }
    }
}

proc setAddress {} {
    global tempsetaddress setaddress
    if {$setaddress < 0} {
        set setaddress "error"
        set setaddress 0
    } else {
    set setaddress $tempsetaddress
    }
}

proc setValue {} {
    global setaddress value
    outb 0x$setaddress $value
}

# 'ioUpdate' updates the showAddress display repeatedly

proc ioUpdate {} {
    showAddress
    # schedule this again
    after 100 ioUpdate
}

# start ioUpdate
ioUpdate
