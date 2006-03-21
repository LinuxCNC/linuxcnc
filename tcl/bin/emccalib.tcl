#!/bin/sh
# we need to find the tcl dir, it was exported from emc \
export EMC2_TCL_DIR
# and some apps need the realtime script, so export that too \
export REALTIME
# and the emc2 version string \
export EMC2VERSION
# the next line restarts using emcsh \
exec $EMC2_EMCSH "$0" "$@"

###############################################################
# Description:  emccalib.tcl
#               A Tcl/Tk script that interfaces with EMC2 
#               through tkemc.  It configures tuning variables 
#               on the fly and saves final values when the 
#               script is shut down.
#
#  Author: Ray Henry
#  License: GPL Version 2
#
#  Copyright (c) 2003 All rights reserved.
#
#  Last change:
# $Revision$
# $Author$
# $Date$
###############################################################
# EMC parameter setting application
# Needs emcsh to run-- 
#    1 find and load the ini file
#    2 find the number of axes for the system.
#    3 set up an array holding section names and line numbers 
#        arrayname is sectionarray
#    4 read the ini for .hal file names
#        list is named halfilelist
#    5 read the .hal files for ini references.
#    6 make a set of widgets with HAL file and HAL names
#    7 Build display widgets for each HAL element
#        (wonder how to scale sliders if used)
#    8 Issue halcmd as values change
#    9 Write changes to ini when done.
###############################################################

package require msgcat
if ([info exists env(LANG)]) {
    msgcat::mclocale $env(LANG)
    msgcat::mcload "../src/po"
    #FIXME need to add location for installed po files
}

proc popupCalibration {} {
    global axisentry activeAxis top initext HALCMD
    global logo numaxes EMC_INIFILE 
    set numaxes [emc_ini "AXES" "TRAJ"]
    for {set i 0} {$i<$numaxes} {incr i} {
        global inilookup$i af$i
    }
    
    # default location for halcmd is in ./bin/
    # if this file is needed to run a different location of halcmd
    # the env(HALCMD) will hold the absolute path to it
    set HALCMD ../../bin/halcmd

    # get the absolute path to HALCMD if it exists
    if {[info exists env(HALCMD)]} {
        set HALCMD $env(HALCMD)
    }

    # This logo is used on all the pages
    set wizard_image_search {
        /etc/emc2/emc2-wizard.gif \
        /usr/local/etc/emc2/emc2-wizard.gif \
        emc2-wizard.gif}

    set logo ""
    foreach wizard_image $wizard_image_search {
        if { [file exists $wizard_image] } {
            set logo [image create photo -file $wizard_image]
            break
        }
    }

    # Find the name of the ini file used for this run.
    set thisinifile "$EMC_INIFILE"
    set thisconfigdir [file dirname $thisinifile]

    wm title . [msgcat::mc "EMC2 Axis Calibration"]
    set logo [label .logo -image $logo]
    set top [frame .main ]
    pack $logo -side left -anchor nw
    pack $top -side left -expand yes -fill both \
        -padx 18 -pady 18 -anchor n

    # use "top" as name of working frame
    set axisframe [frame $top.axes ]
    for {set i 0} {$i<$numaxes} {incr i} {
        button $axisframe.b$i -text "Axis $i" -command "selectAxis $i"
        pack  $axisframe.b$i -side left -fill both -expand yes
    }
   
    # Make a text widget to hold the ini file and
    # put a copy of the inifile in this text widget
    set initext [text $top.initext]
    $initext config -state normal
    $initext delete 1.0 end
    if {[catch {open $thisinifile} programin]} {
        return
    } else {
        $initext insert end [read $programin]
        catch {close $programin}
    }

    # setup array with section names and line numbers
    # sections are without [] around
    # line numbering starts from 1 as do unix line numbers
    array set sectionarray {}
    scan [$initext index end] %d nl
    set inilastline $nl
    for {set i 1} {$i < $nl} {incr i} {
        if { [$initext get $i.0] == "\[" } {
            set inisectionname [$initext get $i.1 $i.end]
            set inisectionname [string trimright $inisectionname \] ]
            array set sectionarray "$inisectionname $i"
        }
    }

    # Find the HALFILE names between HAL and TRAJ
    set startline $sectionarray(HAL)
    set endline $sectionarray(TRAJ)
    set halfilelist ""
    for {set i $startline} {$i < $endline} {incr i} {
        set thisstring [$initext get $i.0 $i.end]
        if { [lindex $thisstring 0] == "HALFILE" } {
            set thishalname [lindex $thisstring end]
            lappend halfilelist $thisconfigdir/$thishalname
        }
    }

    # make a second text widget for scratch writing
    set scratchtext [text $top.scratchtext]

    # do some initial work for each axis
    for {set i 0} {$i < $numaxes} {incr i} {
        set inilookup$i ""
        set af$i [frame $top.ax$i]
    }

    # Search each .hal file for [AXIS
    foreach fname $halfilelist {
        $scratchtext config -state normal
        $scratchtext delete 1.0 end
        if {[catch {open $fname} programin]} {
            return
        } else {
            $scratchtext insert end [read $programin]
            catch {close $programin}
        }
        scan [$scratchtext index end] %d nl
        for {set i 1} {$i < $nl} {incr i} {
            set tmpstring [$scratchtext get $i.0 $i.end]
            if {[lsearch -regexp $tmpstring AXIS] > -1} { 
                for {set j 0} {$j < $numaxes} {incr j} {
                    if {[lsearch -regexp $tmpstring AXIS_$j] > -1} {
                        lappend inilookup$j $tmpstring
                        break
                    }
                }
            }
        }
    }

    # Now build var names and entry widgets for each found var
    # this code is a real "hard way" to do this but...
    # thishalline is the setp command to issue through halcmd 
    # thisinisection is the ini section to edit
    # this ininame is the ini variable name to change with new value
    for {set i 0} {$i<$numaxes} {incr i} {
        set thisinisection AXIS_$i
        set j 0
        foreach thishalline [set inilookup$i] {
            set thisininame [lindex [split $thishalline "\]" ] end ]
            set thishalcommand [lindex $thishalline 1]
            global tvar$i$j
            set tmpvar tvar$i$j
            set $tmpvar [expr [lindex [split [exec $HALCMD -s show param $thishalcommand] " "] 3]]
            label [set af$i].label$j -text "$thisininame" \
                -width 20 -anchor e
            entry [set af$i].entry$j -width 12 \
                -textvariable $tmpvar
            grid [set af$i].label$j [set af$i].entry$j
            incr j

        }
    }
    
    frame $top.buttons
    button $top.buttons.ok -text "Save" -default active \
        -command {changeIt save } -state disabled
    button $top.buttons.apply -text Apply \
        -command {changeIt apply} -state disabled
    button $top.buttons.revert -text Revert \
        -command {changeIt revert} -state disabled
    button $top.buttons.cancel -text Quit \
        -command {changeIt quit}
    pack $top.buttons.ok $top.buttons.apply $top.buttons.revert \
        $top.buttons.cancel -side left -expand 1
#    pack $axisframe -side top -fill x -expand yes
#    pack $top.buttons -side bottom -fill x -expand yes
    grid configure $axisframe -row 0 -sticky ew
    grid rowconfigure $top 1 -weight 1
    grid configure $top.buttons -row 2 -sticky ew

}

proc selectAxis {which} {
    global numaxes axisentry top
    set axisentry $which
    for {set i 0} {$i<$numaxes} {incr i} {
        global af$i
        $top.axes.b$i configure -relief raised
        grid remove [set af$i]
    }
    $top.axes.b$which configure -relief sunken
    grid configure [set af$which] -row 1 -sticky nsew
}

proc changeIt {how } {
    switch -- $how {
    save {set tmp save}
    apply {set tmp apply}
    revert {set tmp revert}
    quit {
        set tmp quit
        # build a check for changed values here and ask
        destroy .
    }
    default {}
    }
    puts "pressed $tmp"
}


popupCalibration
selectAxis 0

