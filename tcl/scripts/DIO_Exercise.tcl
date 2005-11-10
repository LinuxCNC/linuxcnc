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
# And allows setting and reading of one 8255 like chip
# You can use this script to trash most anything.

wm title . "DIO Set"

# create the main window top frame
set top [frame .dioset]
pack $top -padx 15 -pady 15

message $top.message1 -text "Shows and Sets 8255 Devices." \
   -justify center -aspect 800
frame $top.sep  -relief groove -bg black -height 2
pack $top.message1 $top.sep -side top -fill x

set showaddress 200
set tempshowaddress $showaddress
set setaddress 200
set tempsetaddress $setaddress


set a [frame $top.addresses]
label $a.l1 -text "Base Address : "
entry $a.entry1 -textvariable tempshowaddress -width 7 \
    -justify center -bd 1
bind $a.entry1 <KeyPress-Return> {submitShow}
button $a.submitshow -bd 1 -text "Press To Show Address" \
    -command {submitShow}
grid $a.l1 $a.entry1 -sticky ew -pady 4
grid $a.submitshow -sticky ew
grid configure $a.submitshow -columnspan 2
pack $a -side top -fill x -expand yes

set disp [frame $top.display -relief groove]

set ports "A B C Control "

foreach temp $ports {
    set value$temp 0
}

foreach port $ports {
    set lowcaseport [string tolower $port]
    # create the subframe for the display and checkbutton
    set ${lowcaseport}f [frame $disp.$lowcaseport -bd 2 -relief groove]
    set ${lowcaseport}l [label [set ${lowcaseport}f].label -text "$port" ]
    set ${lowcaseport}c [canvas [set ${lowcaseport}f].canvas]
        [set ${lowcaseport}c] configure -height 175 -width 20
    set ${lowcaseport}e [frame [set ${lowcaseport}f].checkframe]
    for {set i 0} {$i < 8} {incr i 1} {
        [set ${lowcaseport}c] create oval 5 [expr $i * 22 + 6] 15 [expr $i * 22 + 16] -fill green
        checkbutton [set ${lowcaseport}e].ckb$i -text $i -variable ${port}setnum$i -command "computeValue $port"
        pack [set ${lowcaseport}e].ckb$i -side top -fill y
    }
    set ${lowcaseport}v [label [set ${lowcaseport}f].value -textvariable value$port ]
    set ${lowcaseport}b [button [set ${lowcaseport}f].button -text "set" -command "setValue $port"]
    grid [set ${lowcaseport}l] -row 0 -column 0 -columnspan 2
    grid [set ${lowcaseport}c] -row 1 -column 0
    grid [set ${lowcaseport}e] -row 1 -column 1
    grid [set ${lowcaseport}v] -row 2 -column 0 -columnspan 2 -sticky ew
    grid [set ${lowcaseport}b] -row 3 -column 0 -columnspan 2 -sticky ew
    pack [set ${lowcaseport}f] -side left -padx 2 -pady 4
}

button $top.button1 -bg darkgray\
    -font {Helvetica -12 bold} -text CANCEL -command {destroy .}
pack $disp $top.button1 -side top -fill x

proc computeValue {whichport} {
    global port
    global value$whichport
    set value$whichport 0
    for {set j 0} {$j < 8} {incr j} {
        global ${whichport}setnum$j
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
    set value$whichport [expr [set value$whichport] + [expr [set ${whichport}setnum$j] * $temp]]
    }
}

foreach temp $ports {
    set portaddress($temp) 0x200
}

proc submitShow {} {
    global showaddress tempshowaddress ports portaddress
    set showaddress 0x$tempshowaddress
    set i 0
    foreach temp $ports {
        set portaddress($temp) [format "0x%x" [expr $showaddress + $i ]]
        incr i
    }
}


# fix this for portaddress
proc showAddress {} {
    global tempshowaddress showaddress value ports portaddress
    if {$showaddress < 0} {
        set tempshowaddress "error"
        set showaddress 0
    } else {
        foreach temp $ports {
            set lowcaseport [string tolower $temp]
            global ${lowcaseport}c
            set value1 0
            set value1 [inb [set portaddress($temp) ] ]
            for {set i 1} {$i <= 8} {incr i 1} {
                if {$value1 % 2} {
                    [set ${lowcaseport}c] itemconfigure $i -fill red
                } else {
                    [set ${lowcaseport}c] itemconfigure $i -fill green
                    }
                set value1 [expr $value1 / 2]
            }
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

proc setValue {portname} {
    global portaddress value$portname
    outb [set portaddress($portname)] [set value$portname]
}

# 'ioUpdate' updates the showAddress display repeatedly

proc ioUpdate {} {
    showAddress
    global ports portaddress
    # schedule this again
    after 100 ioUpdate
}

# start ioUpdate
 ioUpdate
