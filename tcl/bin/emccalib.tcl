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
#    5 read the .hal files for ini references
#        One list for each axis to $numaxes
#        Lists named inilookup0 where number is axis number
#    6 make a set of widgets with ini file names
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
    global logo numaxes EMC_INIFILE namearray commandarray valarray
    set numaxes [emc_ini "AXES" "TRAJ"]
    for {set i 0} {$i<$numaxes} {incr i} {
        global af$i
    }
    
    # default location for halcmd is in ./bin/
    # if this file is needed to run a different location of halcmd
    # the env(HALCMD) will hold the absolute path to it
    set HALCMD ../../bin/halcmd

    # get the absolute path to HALCMD if it exists
    if {[info exists env(HALCMD)]} {
        set HALCMD $env(HALCMD)
    }

    # Wizard logo
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

    # make a frame to hold widgets for each axis
    for {set j 0} {$j < $numaxes} {incr j} {
        set af$j [frame $top.ax$j]
    }

    # New halconfig allows for ini reads from most any location
    # For axis tuning search each .hal file for AXIS
    foreach fname $halfilelist {
        $scratchtext config -state normal
        $scratchtext delete 1.0 end
        if {[catch {open $fname} programin]} {
            return
        } else {
            $scratchtext insert end [read $programin]
            catch {close $programin}
        }
       
        # find the ini references in this hal and build widgets        
        scan [$scratchtext index end] %d nl
        for {set i 1} {$i < $nl} {incr i} {
            set tmpstring [$scratchtext get $i.0 $i.end]
            if {[lsearch -regexp $tmpstring AXIS] > -1} { 
                for {set j 0} {$j < $numaxes} {incr j} {
                    if {[lsearch -regexp $tmpstring AXIS_$j] > -1} {
                        # this is a hal file search ordered loop
                        set thisininame [lindex [split $tmpstring "\]" ] end ]
                        set lowername "[string tolower $thisininame]"
                        set thishalcommand [lindex $tmpstring 1]
                        set tmpval [expr [lindex [split \
                            [exec $HALCMD -s show param $thishalcommand] " "] 3]]
                        global axis$j-$lowername
                        set axis$j-$lowername $tmpval
                        set thislabel [label [set af$j].label-$j-$lowername \
                            -text "$thisininame" -width 20 -anchor e]
                        set thisentry [entry [set af$j].entry-$lowername \
                        -width 12 -textvariable axis$j-$lowername]
                        grid $thislabel $thisentry
                        lappend namearray($j) $lowername
                        lappend commandarray($j) $thishalcommand
                        lappend valarray($j) $tmpval
                        break
                    }
                }
            }
        }
    }

    frame $top.buttons
    button $top.buttons.ok -text "Save" -default active \
        -command {changeIt save } -state disabled
    button $top.buttons.apply -text Apply \
        -command {changeIt apply}
    button $top.buttons.revert -text Revert \
        -command {changeIt revert}
    button $top.buttons.cancel -text Quit \
        -command {changeIt quit}
    pack $top.buttons.ok $top.buttons.apply $top.buttons.revert \
        $top.buttons.cancel -side left -expand 1
    # grid the top display to keep stuff in place
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
    global axisentry sectionarray namearray commandarray valarray HALCMD
    global numaxes
    switch -- $how {
    save {
        set tmp save
        # need to lookup each hal param value
        # compare with ini value from initext using array get sectionarray   
        # for section line numbers
        # change those that need to be changed.
        # and open $EMC_INIFILE and save contents of initext widget
    }
    apply {
        set varnames [lindex [array get namearray $axisentry] end]
        set varcommands [lindex [array get commandarray $axisentry] end]
        set maxvarnum [llength $varnames]
        for {set listnum 0} {$listnum < $maxvarnum} {incr listnum} {
            set var axis$axisentry-[lindex $varnames $listnum]
            global $var
            set tmpval [set $var]
            set tmpcmd [lindex $varcommands $listnum]
            set thisret [exec $HALCMD setp $tmpcmd $tmpval]
        }
    }
    revert {
        # get list of old values from apply before it changes them
        # need some work here to setup variables
        
        # for {set listnum 0} {$listnum < $maxvarnum} {incr listnum} {
        # set thisret [exec $HALCMD setp $tmpcmd $oldval]
        # }
    }
    quit {
        # build a check for changed values here and ask
        for {set j 0} {$j < $numaxes} {incr j} {
            set oldvals [lindex [array get valarray $j] 1]
            set cmds [lindex [array get commandarray $j] 1]
            set k 0
            foreach cmd $cmds {
                set tmpval [expr [lindex [split \
                    [exec $HALCMD -s show param $cmd] " "] 3]]
                set oldval [lindex $oldvals $k]
                if {$tmpval != $oldval} {
                    set answer [tk_messageBox -message "The HAL parameter \n \
                        $cmd \n has changed. \n Really quit?" \
                        -type yesno -icon question] 
                    switch -- $answer { \
                            yes exit
                            no {return}
                    }
                
                }
                incr k
            }
        
        }

        destroy .
    }
    default {}
    }
}

popupCalibration
selectAxis 0

