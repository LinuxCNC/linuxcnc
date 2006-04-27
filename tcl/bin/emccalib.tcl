#!/bin/sh
# we need to find the tcl dir, it was exported from emc \
export EMC2_TCL_DIR
# \
export HALCMD
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
# Needs emcsh to run
# Saves changes to ini file if requested
###############################################################

set LANGDIR $env(EMC2_TCL_DIR)/../src/po
if {[info exists env(EMC2_LANG_DIR)]} {
    set LANGDIR $env(EMC2_LANG_DIR)
}

package require msgcat
if ([info exists env(LANG)]) {
    msgcat::mclocale $env(LANG)
    msgcat::mcload $LANGDIR
}

package require BWidget

# trap mouse click on window manager delete and run changeIt quit.
wm protocol . WM_DELETE_WINDOW {changeIt quit}

proc popupCalibration {} {
    global axisentry activeAxis main top initext HALCMD sectionarray env
    global logo numaxes EMC_INIFILE namearray commandarray valarray thisinifile
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
        emc2-wizard.gif
    }
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
    set main [frame .main ]
    set top [NoteBook .main.top ]
    pack $logo -side left -anchor nw
    pack $main -side left -expand yes -fill both \
        -padx 18 -pady 18 -anchor n

    # make a NoteBook page to hold widgets for each axis
    for {set j 0} {$j < $numaxes} {incr j} {
        set af$j [$top insert $j page$j -text [msgcat::mc "Axis %d" $j] -raisecmd "selectAxis $j"]
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
                        set thisininame [string trimright [lindex [split $tmpstring "\]" ] end ] ]
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

    frame $main.buttons
    button $main.buttons.ok -text [msgcat::mc "Save"] -default active \
        -command {changeIt save } -state disabled
    button $main.buttons.apply -text [msgcat::mc "Apply"] \
        -command {changeIt apply}
    button $main.buttons.revert -text [msgcat::mc "Revert"] \
        -command {changeIt revert} -state disabled
    button $main.buttons.cancel -text [msgcat::mc "Quit"] \
        -command {changeIt quit}
    pack $main.buttons.ok $main.buttons.apply $main.buttons.revert \
        $main.buttons.cancel -side left -fill x -expand 1

    # grid the top display to keep stuff in place
    grid rowconfigure $main 1 -weight 1
    grid configure $top -row 1 -sticky nsew 
    grid configure $main.buttons -row 2 -sticky ew -ipadx 20
}

proc selectAxis {which} {
    global axisentry
    set axisentry $which
}

proc changeIt {how } {
    global axisentry sectionarray namearray commandarray valarray HALCMD
    global numaxes main oldvalarray initext
    switch -- $how {
        save {
            for {set i 0} {$i<$numaxes} {incr i} {
                set varnames [lindex [array get namearray $i] end]
                set varnames [concat $varnames]
                set upvarnames [string toupper $varnames]
                set varcommands [lindex [array get commandarray $i] end]
                set maxvarnum [llength $varnames]
                set sectname "AXIS_$i"
                if {$i == [expr $numaxes-1]} {
                    set endsect EMCIO
                } else {
                    set endsect AXIS_[expr $i+1]
                }
                set sectnum "[set sectionarray($sectname)]"
                set nextsectnum "[set sectionarray($endsect)]"
                $initext configure -state normal
                for {set ind $sectnum} {$ind < $nextsectnum} {incr ind} {
                    switch -- [$initext get $ind.0] {
                        "#" {}
                        default {
                            set tmpstr [$initext get $ind.0 $ind.end]
                            set tmpvar [lindex $tmpstr 0]
                            set tmpindx [lsearch $upvarnames $tmpvar]
                            if {$tmpindx != -1} {
                                set cmd [lindex $varcommands $tmpindx]
                                $initext mark set insert $ind.0
                                set newval [expr [lindex [split \
                                    [exec $HALCMD -s show param $cmd] " "] 3]]
                                set tmptest [string first "e" $newval]
                                if {[string first "e" $newval] > 1} {
                                    set newval [format %f $newval]
                                }
                                # keep everything leading up to the var value, 
                                # replace the value, and keep everything after the value
                                regsub {(^.*=[ \t]*)[^ \t]*(.*)} $tmpstr "\\1$newval\\2" newvar
                                $initext delete insert "insert lineend"
                                $initext insert insert $newvar
                            }
                        }
                    }
                }
                $initext configure -state disabled
            }
            saveFile
            for {set j 0} {$j < $numaxes} {incr j} {
                set cmds [set commandarray($j)]
                set newvals ""
                set k 0
                foreach cmd $cmds {
                    set val [expr [lindex [split \
                        [exec $HALCMD -s show param $cmd] " "] 3]]                    
                    append newvals "$val " 
                    incr k
                }
                set valarray($j) $newvals
            }
        }
        apply {
            $main.buttons.ok configure -state normal
            $main.buttons.revert configure -state normal
            set varnames [lindex [array get namearray $axisentry] end]
            set varcommands [lindex [array get commandarray $axisentry] end]
            set maxvarnum [llength $varnames]
            for {set listnum 0} {$listnum < $maxvarnum} {incr listnum} {
                set var "axis$axisentry-[lindex $varnames $listnum]"
                global $var
                set tmpval [set $var]
                set tmpcmd [lindex $varcommands $listnum]
                # get list of old values before changeIt apply changes them
                set tmpold [expr [lindex [split \
                    [exec $HALCMD -s show param $tmpcmd] " "] 3]]
                lappend oldvalarray($axisentry) $tmpold
                set thisret [exec $HALCMD setp $tmpcmd $tmpval]
            }
        }
        revert {
            set varnames [lindex [array get namearray $axisentry] end]
            set varcommands [lindex [array get commandarray $axisentry] end]
            set maxvarnum [llength $varnames]
            set oldvarvals [lindex [array get oldvalarray $axisentry] end]
            for {set listnum 0} {$listnum < $maxvarnum} {incr listnum} {
                set tmpval [lindex $oldvarvals $listnum]
                set tmpcmd [lindex $varcommands $listnum]
                set thisret [exec $HALCMD setp $tmpcmd $tmpval]
                # update the display
                set var axis$axisentry-[lindex $varnames $listnum]
                global $var
                set $var $tmpval
            }
            $main.buttons.revert configure -state disabled
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
                        set answer [tk_messageBox -message [msgcat::mc "The HAL parameter \n \
                            %s \n has changed. \n Really quit?" $cmd] \
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

proc saveFile {} {
    global initext thisinifile
    set name $thisinifile
    catch {file copy -force $name $name.bak}
    if {[catch {open $name w} fileout]} {
        puts stdout [msgcat::mc "can't save %s" $name]
        return
    }
    puts $fileout [$initext get 1.0 end]
    catch {close $fileout}
}


popupCalibration
$top raise page0
