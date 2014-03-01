#!/bin/sh
# the next line restarts using emcsh \
exec ${LINUXCNC_EMCSH-emcsh} "$0" "$@"

###############################################################
# Description:  mini.tcl
#               A Tcl/Tk script that interfaces with EMC2.  It 
#               sets estop to off and machine on when started.
#               Provides a mostly flat single window display.
#
#  Authors: Ray Henry and Paul Corner
#  License: GPL Version 2
#
#  Copyright (c) 2003 All rights reserved.
#
###############################################################
#  INI variables include
#  [DISPLAY]
#  Introductory graphic
#  INTRO_GRAPHIC = emc2.gif
#  INTRO_TIME = 5
#  If time is set to zero no image is used
###############################################################
# Autosizing Extended Tk GUI for the Enhanced Machine Controller
# This gui requires two option files TkEmcL for screens larger 
# than 1000 wide and TkEmcS for smaller screens.
###############################################################


# Load the emc package, which defines variables for various useful paths
package require Linuxcnc
eval emc_init $argv

set tkemc 1

# wheelEvent code curtesy of Tsahi Levent-Levi - on activestate web site.
# this only works properly on TNG's RedHat 7.2 or newer os's.

proc wheelEvent { x y delta } {

    # Find out what's the widget we're on
    set act 0
    set widget [winfo containing $x $y]

    if {$widget != ""} {
        # Make sure we've got a vertical scrollbar for this widget
        if { [catch "$widget cget -yscrollcommand" cmd] } return

        if {$cmd != ""} {
            # Find out the scrollbar widget we're using
            set scroller [lindex $cmd 0]

            # Make sure we act
            set act 1
        }
    }

    if {$act == 1} {
        # Now we know we have to process the wheel mouse event
        set xy [$widget yview]
        set factor [expr [lindex $xy 1]-[lindex $xy 0]]

        # Make sure we activate the scrollbar's command
        set cmd "[$scroller cget -command] scroll [expr -int($delta/(120*$factor))] units"
        eval $cmd
    }
}
bind all <MouseWheel> "+wheelEvent %X %Y %D"

# Read configuration information from the ini file

# set the cycle time for display updating
set temp [emc_ini "CYCLE_TIME" "DISPLAY"]
if { [string length $temp] == 0} {
    set temp 0.200
}
set displayCycleTime [expr {int($temp * 1000 + 0.5)}]

# set the debounce time for key repeats
set debounceTime 150

set toolfilename [emc_ini "TOOL_TABLE" "EMCIO"]
if { [string length $toolfilename] == 0} {
    set toolfilename "emc.tbl"
}

set paramfilename [emc_ini "PARAMETER_FILE" "RS274NGC"]
if { [string length $paramfilename] == 0} {
    set paramfilename "emc.var"
}

# determine what the axes and coordinates are.
set worldlabellist ""
set axiscoordmap ""
set numaxes [emc_ini "AXES" "TRAJ"]
set coordnames [ emc_ini "COORDINATES" "TRAJ"]
set activeAxis 0

# AJ: took over coords setup from tkemc
# old style:
#     set coords [emc_ini "POSITION_OFFSET" "DISPLAY"]
# coords can be relative or machine
set temp [emc_ini "POSITION_OFFSET" "DISPLAY"]
if {$temp == "RELATIVE"} {
    set coords relative
} elseif {$temp == "MACHINE"} {
    set coords machine
} else {
    # not found, or invalid, so set to relative
    set coords relative
}

proc toggleRelAbs {} {
    global coords
    if {$coords == "relative"} {
        set coords machine
    } else {
        set coords relative
    }
}

# actcmd can be actual or commanded
set temp [emc_ini "POSITION_FEEDBACK" "DISPLAY"]
if {$temp == "ACTUAL"} {
    set actcmd actual
} elseif {$temp == "COMMANDED"} {
    set actcmd commanded
} else {
    # not found, or invalid, so set to actual
    set actcmd actual
}

proc toggleCmdAct {} {
    global actcmd
    if {$actcmd == "actual"} {
        set actcmd commanded
    } else {
        set actcmd actual
    }
}

set numcoords 0

# Assign names to each axis number
set possibles {X Y Z A B C}
set j "-1"
foreach axname $possibles {
    incr j
    global worldlabel${j} jointlabel${j}
    if { [ string first $axname $coordnames ] >= 0 } {
        set worldlabel${j} $axname
        set jointlabel${j} ${j}
        set numcoords [expr $numcoords +1]
        set worldlabellist [ concat $worldlabellist $axname]
        set axiscoordmap [ concat $axiscoordmap ${j} ]
    }
}

# Read maxJogSpeed and axisType for each axis and store them arrays
foreach temp $axiscoordmap {
    set maxJogSpeedAxis($temp) \
    [int [expr [emc_ini MAX_VELOCITY AXIS_$temp] * 60 +0.5]]
    set axisType($temp) [emc_ini TYPE AXIS_$temp]
 }

set programDirectory [emc_ini PROGRAM_PREFIX DISPLAY]
# set HELPDIRname [emc_ini HELP_FILE DISPLAY]

# set some initial values to variables to be used during a run
emc_set_timeout 1.0
emc_set_wait received
emc_update
set modifier Alt
set modifierstring "Alt"
set oldstatusstring "idle"
set runMark 0

# -----MISC Processes Jog-----

proc toggleJogType {} {
    global jogType jogIncrement

    if {$jogType == "continuous"} {
        set jogType $jogIncrement
    } else {
        set jogType continuous
    }
}

# fixme to work with both rotary and linear
proc toggleJogIncrement {} {
    global jogType jogIncrement
set units "0"
    if {$jogType == "continuous"} {
        set jogType $jogIncrement
    } else {
        if {$jogIncrement == "0.0001"} {
            set jogIncrement 0.0010
        } elseif {$jogIncrement == "0.0010"} {
            set jogIncrement 0.0100
        } elseif {$jogIncrement == "0.0100"} {
            set jogIncrement 0.1000
        } elseif {$jogIncrement == "0.1000"} {
            set jogIncrement 1.0000
        } elseif {$jogIncrement == "1.0000"} {
            set jogIncrement 5.0000
        } elseif {$jogIncrement == "5.0000"} {
            set jogIncrement 10.0000
        } elseif {$jogIncrement == "10.0000"} {
            set jogIncrement 0.0001
        }
        set units [emc_program_linear_units]
        # puts "program_units $units"
        set jogType $jogIncrement
    }
}

# given axis, determines speed and cont v. incr and jogs it neg
proc jogNeg {axis} {
    global activeAxis jogSpeed jogType jogIncrement axiscoordmap
set units "0"
    set activeAxis $axis
    if { [emc_teleop_enable] == 0 } {
	set axisToJog [ lindex $axiscoordmap $axis ]
    } else {
	set axisToJog $axis
    }

        catch { set units [emc_program_linear_units] }
        # puts "program_units $units $axisToJog"

    if {$jogType == "continuous"} {
        emc_jog $axisToJog -$jogSpeed
    } else {
        emc_jog_incr $axisToJog -$jogSpeed $jogIncrement
    }
}

# given axis, determines speed and cont v. incr and jogs it pos
proc jogPos {axis} {
    global activeAxis jogSpeed jogType jogIncrement axiscoordmap
set units "0"
    set activeAxis $axis
    if { [emc_teleop_enable] == 0 } {
	set axisToJog [ lindex $axiscoordmap $axis ]
    } else {
	set axisToJog $axis
    }

        catch { set units [emc_program_linear_units] }
        # puts "program_units $units $axisToJog"

    if {$jogType == "continuous"} {
        emc_jog $axisToJog $jogSpeed
    } else {
        emc_jog_incr $axisToJog $jogSpeed $jogIncrement
    }
}

# stops the active axis
proc jogStop {args} {
    global activeAxis jogType
    # only stop continuous jogs; let incremental jogs finish
    if {$jogType == "continuous"} {
        if {$args == "" } {
            emc_jog_stop $activeAxis
        } else {
            emc_jog_stop $args
        }
    }
}

proc incrJogSpeed {} {
    global jogSpeed maxJogSpeed

    if {$jogSpeed < $maxJogSpeed} {
        set jogSpeed [int [expr $jogSpeed + 1.5]]
    }
}

proc decrJogSpeed {} {
    global jogSpeed
    if {$jogSpeed > 1} {
        set jogSpeed [int [expr $jogSpeed - 0.5]]
    }
}

proc minusDone {} {
    global minusAxis
    jogStop $minusAxis
    bind ManualBindings <KeyPress-minus> minusDown
    set $minusAxis -1
}

proc minusDown {} {
    global activeAxis minusAxis
    
    if {$minusAxis < 0} {
	set minusAxis $activeAxis
    }
    bind ManualBindings <KeyPress-minus> {}
    after cancel minusDone
    jogNeg $activeAxis
}

proc minusUp {} {
    global debounceTime
    after cancel minusDone
    after $debounceTime minusDone
}

proc equalDone {} {
    global equalAxis
    jogStop $equalAxis
    bind ManualBindings <KeyPress-equal> equalDown
    set $equalAxis -1
}

proc equalDown {} {
    global activeAxis equalAxis

    if {$equalAxis < 0} {
	set equalAxis $activeAxis
    }
    bind ManualBindings <KeyPress-equal> {}
    after cancel equalDone
    jogPos $activeAxis
}

proc equalUp {} {
    global debounceTime
    after cancel equalDone
    after $debounceTime equalDone
}
proc leftDone {} {
    jogStop 0
    bind ManualBindings <KeyPress-Left> leftDown
}

proc leftDown {} {
    axisSelectx 0
    bind ManualBindings <KeyPress-Left> {}
    after cancel leftDone
    jogNeg 0
}

proc leftUp {} {
    global debounceTime
    after cancel leftDone
    after $debounceTime leftDone
}
proc rightDone {} {
    jogStop 0
    bind ManualBindings <KeyPress-Right> rightDown
}

proc rightDown {} {
    axisSelectx 0
    bind ManualBindings <KeyPress-Right> {}
    after cancel rightDone
    jogPos 0
}

proc rightUp {} {
    global debounceTime
    after cancel rightDone
    after $debounceTime rightDone
}
proc downDone {} {
    jogStop 1
    bind ManualBindings <KeyPress-Down> downDown
}

proc downDown {} {
    axisSelectx 1
    bind ManualBindings <KeyPress-Down> {}
    after cancel downDone
    jogNeg 1
}

proc downUp {} {
    global debounceTime
    after cancel downDone
    after $debounceTime downDone
}
proc upDone {} {
    jogStop 1
    bind ManualBindings <KeyPress-Up> upDown
}

proc upDown {} {
    axisSelectx 1
    bind ManualBindings <KeyPress-Up> {}
    after cancel upDone
    jogPos 1
}

proc upUp {} {
    global debounceTime
    after cancel upDone
    after $debounceTime upDone
}

proc priorDone {} {
    jogStop 2
    bind ManualBindings <KeyPress-Prior> priorDown
}

proc priorDown {} {
    axisSelectx 2
    bind ManualBindings <KeyPress-Prior> {}
    after cancel priorDone
    jogPos 2
}

proc priorUp {} {
    global debounceTime
    after cancel priorDone
    after $debounceTime priorDone
}

proc nextDone {} {
    jogStop 2
    bind ManualBindings <KeyPress-Next> nextDown
}

proc nextDown {} {
    axisSelectx 2
    bind ManualBindings <KeyPress-Next> {}
    after cancel nextDone
    jogNeg 2
}

proc nextUp {} {
    global debounceTime
    after cancel nextDone
    after $debounceTime nextDone
}

# -----MISC Processes Axis Select-----

# This process will accept axis names or numbers
# But number or name must point to a visible axis display

# Setup incremental jogs for both linear and rotary axes.
# Read default values for these if they exist in ini.
set jogType continuous
set linjogincr 0.100
set oldlinjogincr 0.0100
set templin [emc_ini  "DEFAULT_LINEAR_JOG_INCREMENT"  "DISPLAY"]
if {$templin != ""} {
#    set oldlinjogincr 1.0
}

set oldrotjogincr 1
set rotjogincr 1
set temprot [emc_ini  "DEFAULT_ROTARY_JOG_INCREMENT"  "DISPLAY"]
if {$temprot != ""} {
    set oldrotjogincr $temprot
}

set jogIncrement $oldlinjogincr

proc axisSelectx {axis} {
    global activeAxis axiscoordmap axisType jogplustext jognegtext worldlabellist
    global feedscale maxJogSpeedAxis maxJogSpeed axishometext
    global iframe irframe jogIncrement oldlinjogincr oldrotjogincr
    global unitsetting immframe
    # clear all display settings
    foreach axnum $axiscoordmap {
        global pos${axnum}
        [set pos${axnum}] config -relief flat
    }
    set axis [string toupper $axis]
    switch -- $axis {
        X {set temp 0}
        Y {set temp 1}
        Z {set temp 2}
        A {set temp 3}
        B {set temp 4}
        C {set temp 5}
        default {set temp $axis}
    }
    set oldaxis $activeAxis
    set activeAxis $temp
    set modex [emc_mode]
    if {$modex == "manual"} {
        [set pos${temp}] config -relief groove
        set axisname [lindex $worldlabellist $temp]
        set jogplustext [msgcat::mc "JOG %s +" $axisname]
        set jognegtext [msgcat::mc "JOG %s -" $axisname]
        set axishometext [msgcat::mc "%s\n\nZ\nE\nR\nO" $axisname]
        set maxJogSpeed [set maxJogSpeedAxis($temp) ]
        $feedscale configure -to $maxJogSpeed
        # add the linear v rotary jog increment stuff here.
        if {$axisType($oldaxis) == "LINEAR" && $axisType($temp) == "ANGULAR" } {
            set oldlinjogincr $jogIncrement
            grid forget $iframe
            grid forget $immframe
            grid configure $irframe -column 6 -row 0 -rowspan 5 -sticky nsew
            set jogIncrement $oldrotjogincr
        } elseif {$axisType($oldaxis) == "ANGULAR" && $axisType($temp) == "LINEAR" } {
            set oldrotjogincr $jogIncrement
            grid forget $irframe
            if { $unitsetting == "(mm)" } {
                grid forget $iframe
                grid configure $immframe -column 6 -row 0 -rowspan 5 -sticky nsew
            } else {
                grid forget $immframe
                grid configure $iframe -column 6 -row 0 -rowspan 5 -sticky nsew
            }
            set jogIncrement $oldlinjogincr
        }
    }
}

# -----MISC Processes Send MDI-----

proc sendMdi {mdi} {
    set tempmode [emc_mode]
    if {$tempmode != "mdi" } {
        emc_mode mdi
        emc_wait received
        emc_mdi $mdi
        emc_wait done
        emc_mode $tempmode
    } else {
        emc_mdi $mdi
    }
}

# -----MISC Processes Coordinate Names-----

proc setCoordNamesx {} {
    global coordnames worldlabellist axiscoordmap numcoords

    set possibles {X Y Z A B C}
    set j "-1"
    foreach axname $possibles {
        incr j
        global worldlabel${j} jointlabel${j}
        if { [ string first $axname $coordnames ] >= 0 } {
            set worldlabel${j} $axname
            set jointlabel${j} ${j}
            set numcoords [expr $numcoords +1]
            set worldlabellist [ concat $worldlabellist $axname]
            set axiscoordmap [ concat $axiscoordmap ${j} ]
        }
    }
}

# -----MISC Processes Key Bindings-----

proc setManualBindings {} {
    bindtags . {ManualBindings . all}
}

proc setAutoBindings {} {
    bindtags . {AutoBindings . all}
}

proc setMdiBindings {} {
    bindtags . {. all}
}

proc setKeyBindingsx {} {
    global modifier worldlabellist

    bind . <KeyPress-F1> {toggleEstop}
    bind . <KeyPress-F2> {toggleMachine}
    bind . <KeyPress-F3> {showMode manual}
    bind . <KeyPress-F4> {showMode auto}
    bind . <KeyPress-F5> {showMode mdi}
    bind . <KeyPress-F6> {emc_task_plan_init}
    bind . <KeyPress-F10> {break}
    bind . <KeyPress-Pause> {toggleFeedhold}
    bind . <Escape> {emc_abort}
    bind . <Alt-KeyPress-space> {popInToggle}
    bind . <Alt-KeyPress-space> {popInToggle}


    # key bindings for 6 axis operation based on axis name.
    foreach axname $worldlabellist {
        bind ManualBindings "<KeyPress-${axname}>" "axisSelectx ${axname}"
        bind ManualBindings "<KeyPress-[string tolower ${axname}]>" "axisSelectx ${axname}"
    }

    bind ManualBindings <KeyPress-grave> {emc_feed_override 0}
    bind ManualBindings <KeyPress-1> {emc_feed_override 10}
    bind ManualBindings <KeyPress-2> {emc_feed_override 20}
    bind ManualBindings <KeyPress-3> {emc_feed_override 30}
    bind ManualBindings <KeyPress-4> {emc_feed_override 40}
    bind ManualBindings <KeyPress-5> {emc_feed_override 50}
    bind ManualBindings <KeyPress-6> {emc_feed_override 60}
    bind ManualBindings <KeyPress-7> {emc_feed_override 70}
    bind ManualBindings <KeyPress-8> {emc_feed_override 80}
    bind ManualBindings <KeyPress-9> {emc_feed_override 90}
    bind ManualBindings <KeyPress-0> {emc_feed_override 100}
    bind ManualBindings <KeyPress-at> {toggleCmdAct}
    bind ManualBindings <KeyPress-numbersign> {toggleRelAbs}
    bind ManualBindings <KeyPress-dollar> {toggleJointWorld}
    bind ManualBindings <KeyPress-comma> {decrJogSpeed}
    bind ManualBindings <KeyPress-period> {incrJogSpeed}
    bind ManualBindings <KeyPress-j> {toggleJogType}
    bind ManualBindings <KeyPress-i> {toggleJogIncrement}
    bind ManualBindings <$modifier-t> {popupToolOffset}
    bind ManualBindings <$modifier-t> {popupToolOffset}
    bind ManualBindings <KeyPress-F7> {toggleMist}
    bind ManualBindings <KeyPress-F8> {toggleFlood}
    bind ManualBindings <KeyPress-F9> {toggleSpindleForward}
    bind ManualBindings <KeyPress-F10> {toggleSpindleReverse}
    bind ManualBindings <Home> {emc_home $activeAxis}
    bind ManualBindings <KeyPress-F11> spindleDecrDown
    bind ManualBindings <KeyRelease-F11> spindleDecrUp
    bind ManualBindings <KeyPress-F12> spindleIncrDown
    bind ManualBindings <KeyRelease-F12> spindleIncrUp
    bind ManualBindings <KeyPress-minus> minusDown
    bind ManualBindings <KeyRelease-minus> minusUp
    bind ManualBindings <KeyPress-equal> equalDown
    bind ManualBindings <KeyRelease-equal> equalUp
    bind ManualBindings <KeyPress-Left> leftDown
    bind ManualBindings <KeyRelease-Left> leftUp
    bind ManualBindings <KeyPress-Right> rightDown
    bind ManualBindings <KeyRelease-Right> rightUp
    bind ManualBindings <KeyPress-Down> downDown
    bind ManualBindings <KeyRelease-Down> downUp
    bind ManualBindings <KeyPress-Up> upDown
    bind ManualBindings <KeyRelease-Up> upUp
    bind ManualBindings <KeyPress-Prior> priorDown
    bind ManualBindings <KeyRelease-Prior> priorUp
    bind ManualBindings <KeyPress-Next> nextDown
    bind ManualBindings <KeyRelease-Next> nextUp

    bind AutoBindings <KeyPress-grave> {emc_feed_override 0}
    bind AutoBindings <KeyPress-1> {emc_feed_override 10}
    bind AutoBindings <KeyPress-2> {emc_feed_override 20}
    bind AutoBindings <KeyPress-3> {emc_feed_override 30}
    bind AutoBindings <KeyPress-4> {emc_feed_override 40}
    bind AutoBindings <KeyPress-5> {emc_feed_override 50}
    bind AutoBindings <KeyPress-6> {emc_feed_override 60}
    bind AutoBindings <KeyPress-7> {emc_feed_override 70}
    bind AutoBindings <KeyPress-8> {emc_feed_override 80}
    bind AutoBindings <KeyPress-9> {emc_feed_override 90}
    bind AutoBindings <KeyPress-0> {emc_feed_override 100}
    bind AutoBindings <KeyPress-at> {toggleCmdAct}
    bind AutoBindings <KeyPress-numbersign> {toggleRelAbs}
    bind AutoBindings <KeyPress-o> {fileDialog}
    bind AutoBindings <KeyPress-r> {emc_run $runMark ; set runMark 0}
    bind AutoBindings <KeyPress-p> {emc_pause}
    bind AutoBindings <KeyPress-s> {emc_resume}
    # bindings to step is different to allow for axis focus
    bind AutoBindings <KeyPress-Right> {emc_step}
    bind AutoBindings <KeyPress-Down> {emc_step}
    bind AutoBindings <KeyPress-v> {emc_run -1}
    bind AutoBindings <$modifier-t> {popupToolOffset}

}

# -----MISC Processes Change Mode -----

proc showMode {which} {
    global down up defbg mdientry
    foreach tempmode {manual auto mdi} {
        pack forget $down.$tempmode
        $up.$tempmode configure -bg $defbg
    }
    pack $down.$which -side top -fill both -expand yes -padx 4 -pady 4
    $down.$which configure -takefocus 1
    $up.$which configure -bg lightgreen
    $up.[emc_mode] configure -bg lightgreen

    switch -- $which {
        manual {
            $mdientry configure -state disabled
            setManualBindings
            emc_mode manual
        }
        auto {
            $mdientry configure -state disabled
            setAutoBindings
            emc_mode auto
        }
        mdi {
            $mdientry configure -state normal
            focus $mdientry
            setMdiBindings
            emc_mode mdi
        }
        default {
        }
    }
}

# -----MISC Processes Toggles -----

proc toggleEstop {} {
    global coord
    if { [emc_estop] == "on"} {
        emc_estop off
        emc_wait done
        emc_machine on
        if { [info exists coord ] } {
            catch {$coord.bzero configure -bg lightgreen -state normal}
        }
    } else {
        emc_estop on
        emc_wait done
        emc_machine off
        if { [info exists coord ] } {
            catch {$coord.bzero configure -bg lightgray -state disabled}
        }
    }
}

proc toggleMachine {} {
    if { [emc_machine] == "off"} {
        emc_machine on
    } else {
        emc_machine off
    }
}

proc toggleMist {} {
    if { [emc_mist] == "off"} {
        emc_mist on
    } else {
        emc_mist off
    }
}

proc toggleFlood {} {
    if { [emc_flood] == "off"} {
        emc_flood on
    } else {
        emc_flood off
    }
}

proc toggleBrake {} {
    if { [emc_brake] == "on"} {
        emc_brake off
    } else {
        emc_brake on
    }
}


# Initialize feedoverride and lastfeedoverride before first calling

proc toggleFeedhold {} {
    global feedvalue feedoverride lastfeedoverride
    set temp [emc_feed_override]
    if {$temp >= 2} {set lastfeedoverride $temp}
    switch -- $feedvalue {
        feedhold {
            set feedoverride 0
            emc_feed_override $feedoverride
        }
        continue {
            set feedoverride $lastfeedoverride
            emc_feed_override $feedoverride
        }
        default {
        }
    }
}

# jog type only exists at the interface level

proc setJogType {which} {
    global jogType jogincrement jogcontinuous iframe immframe irframe
    if {$which == "continuous"} {
        set jogType "continuous"
        set thisstate disabled
        $jogcontinuous configure -relief sunken
        $jogincrement configure -relief raised
    } else {
        set jogType "increment"
        set thisstate normal
        $jogcontinuous configure -relief raised
        $jogincrement configure -relief sunken
    }
    for {set x 1} {$x < 6} {incr x} {
        eval [list "${iframe}.r${x}" configure -state ${thisstate}]
        eval [list "${immframe}.r${x}" configure -state ${thisstate}]
        eval [list "${irframe}.r${x}" configure -state ${thisstate}]
    }
}

# -----MISC Processes Set Font -----

proc setfontx {} {
    global fontfamily fontsize fontstyle axiscoordmap

    set nf [list $fontfamily $fontsize $fontstyle]
    foreach axnum $axiscoordmap {
        global "pos${axnum}l" "pos${axnum}d"
        [set "pos${axnum}l"] config -font $nf
        [set "pos${axnum}d"] config -font $nf
    }
}

# -----Begin Widget Definitions-----

# Set the source directory for other tcl files sourced here
# set tclsource [file join [pwd] tcl ]
# set exsourced [file join $tclsource sourced]

# update emc tcl lib directories using tcl/lib
# set libemctcl [file join $tclsource lib]
# source [file join $libemctcl updateLib]
# updateLib $libemctcl

# test for windows flag
set windows 0

# Autosize the gui for the screen available
set sgeom ""
set tmpsize [wm maxsize .]
set swidth [lindex $tmpsize 0]
set sheight [lindex $tmpsize 1]

if {$swidth <=1000} {
    set optionfile S
    } else {
    set optionfile L
}

# read the application defaults
foreach f "TkEmc$optionfile /usr/X11R6/lib/X11/app-defaults/TkEmc$optionfile" {
    if { [file exists $f] } {
        option readfile $f startupFile
        break
    } else {
        set optionfile ""
    }
}

proc popupAboutx {} {
    tk_messageBox  -title [msgcat::mc "About"]  -default "ok" -message [msgcat::mc "TkMini \
        \n\nTcl/Tk GUI for Enhanced Machine Controller\n\nGPL Copyright 2003 \
        \nRay Henry <rehenry@up.net>\n\n\
        3D backplotter by Paul Corner <paul_c@users.sourceforge.net>\n\
        \nThis software comes with ABSOLUTELY NO GUARANTEE!  \
        \nFor details see the copyright.html file in this directory."]
}

proc rightConfig {which} {
    global right rup down manframe spindframe programfiletext offsetactive rightwhich
    place forget $rup
    place forget $down
    grid forget $spindframe
    set rightwhich $which
    switch -- $which {
        split {
            place $rup -x 0 -y 0 -relwidth 1 -relheight 0.7
            place $down -x 0 -rely 0.7 -relwidth 1 -relheight 0.3
        }
        modefull {
            set temp [emc_mode]
            if {$temp == "manual"} {
                grid $spindframe -column 0 -row 5 -sticky nsew -pady 5
                grid configure $spindframe -columnspan 7
                grid rowconfigure $manframe 5 -weight 2
            }
            place $down -x 0 -y 0 -relwidth 1 -relheight 1
        }
        popfull {
            place $rup -x 0 -y 0 -relwidth 1 -relheight 1
        }
        default {}
    }
}

# use the top-level window as our top-level window, and name it
wm title "." [emc_ini  MACHINE  EMC]
set top [frame .top ]
pack $top -side top -fill both -expand yes

set stwidth [expr $swidth * 0.9]
set stheight [expr $sheight * 0.9 ]
set stwidth [expr int($stwidth)]
set stheight [expr int($stheight) ]
append sgeom $stwidth x $stheight + 0 + 0
wm geometry "." $sgeom
pack $top -side top -fill both -expand yes

# Pick up colors for resets
if {$optionfile != "" } {
    set defbg [option get $top background background]
    set deffg [option get $top foreground foreground]
} else {
    set defbg lightgray
    set deffg black
}

# add a 2 col 4 row array of frames within top frame.
set menubar [frame $top.menuline -borderwidth 2 -relief raised ]
set up [frame $top.up -borderwidth 2 -relief raised]
set left [frame $top.left -borderwidth 2 -relief raised]
set right [frame $top.right -borderwidth 2 -relief raised]
set rup [frame $right.up]
set down [frame $right.down]
set popinframe $right.up

set sizex [ expr $sheight * 0.04 ]
set sizex1 [ expr $sheight * 0.05 ]
set sizex2 [ expr $sizex + $sizex1 ]
place $menubar -x 0 -y 0 -relwidth 1 -height $sizex
place $up -x 0 -y $sizex -relwidth 1 -height $sizex1
place $left -x 0 -y $sizex2 -relwidth .3 -relheight 1 -height -$sizex2
place $right -relx .3 -y $sizex2 -relwidth .7 -relheight 1 -height -$sizex2

# build the top menu bar with menubuttons
set filem [menubutton $menubar.file -text [msgcat::mc "Program"] ]
set viewm [menubutton $menubar.view -text [msgcat::mc "View"] ]
set settingsm [menubutton $menubar.settings -text [msgcat::mc "Settings"] ]
set infom [menubutton $menubar.info -text [msgcat::mc "Info"] ]
set helpm [menubutton $menubar.help -text [msgcat::mc "Help"] ]

# Now add buttons for popins
set popins $menubar
set p1 [checkbutton $popins.b1 -text [msgcat::mc "Backplot"] \
    -variable popArray(Plot) -command {popIn Plot} ]
set p2 [checkbutton $popins.b2 -text [msgcat::mc "Editor"] \
    -variable popArray(Editor) -command {popIn Editor} ]
set p3 [checkbutton $popins.b3 -text [msgcat::mc "Offsets"] \
    -variable popArray(Offsets) -command {popIn Offsets} ]
set p4 [checkbutton $popins.b4 -text [msgcat::mc "Tools"] \
    -variable popArray(Tools) -command {popIn Tools} ]

pack $filem $viewm $settingsm $infom -side left -padx 10
pack $p2 $p1 $p4 $p3 -side left -fill both -expand yes -padx 4
pack $helpm -side right -padx 10

set filemenu [menu $filem.menu -tearoff 0 ]
set viewmenu [menu $viewm.menu -tearoff 0]
set settingsmenu [menu $settingsm.menu -tearoff 0]
set infomenu [menu $infom.menu -tearoff 0]
set helpmenu [menu $helpm.menu -tearoff 0]

$filem configure -menu $filemenu
$viewm configure -menu $viewmenu
$settingsm configure -menu $settingsmenu
$infom configure -menu $infomenu
$helpm configure -menu $helpmenu

# build the individual menu cascades

# File menu
$filemenu add command -label [msgcat::mc "Reset"] \
    -command {emc_task_plan_init} -underline 0
$filemenu add separator
$filemenu add command -label [msgcat::mc "Exit"] \
    -command {after cancel updateStatus ; destroy . ; exit} \
    -accelerator $modifierstring+X -underline 1
bind . <$modifier-x> {after cancel updateStatus ; destroy . ; exit}

# layout menu
$viewmenu add checkbutton -label [msgcat::mc "Position Type"] \
    -variable viewArray(Position) -command {viewInfo Position}
$viewmenu add checkbutton -label [msgcat::mc "Tool Info"] \
    -variable viewArray(Tool) -command {viewInfo Tool}
$viewmenu add checkbutton -label [msgcat::mc "Offset Info"] \
    -variable viewArray(Offset) -command {viewInfo Offset}
$viewmenu add separator
$viewmenu add command -label [msgcat::mc "Show Restart"]  -command {showRestart}
$viewmenu add command -label [msgcat::mc "Hide Restart"]  -command {hideRestart}
$viewmenu add separator
$viewmenu add command -label [msgcat::mc "Show Split Right"]  \
    -command {rightConfig split }
$viewmenu add command -label [msgcat::mc "Show Mode Full"]  \
    -command {rightConfig modefull }
$viewmenu add command -label [msgcat::mc "Show Popin Full"]  \
    -command {rightConfig popfull }

# settings menu
$settingsmenu add radiobutton -label [msgcat::mc "Actual Position"] \
    -variable actcmd -value actual
$settingsmenu add radiobutton -label [msgcat::mc "Commanded Position"] \
    -variable actcmd -value commanded
$settingsmenu add separator
$settingsmenu add radiobutton -label [msgcat::mc "Machine Position"] \
    -variable coords  -value machine
$settingsmenu add radiobutton -label [msgcat::mc "Relative Position"] \
    -variable  coords -value relative
$settingsmenu add separator
$settingsmenu add command -label [msgcat::mc "Calibration..."] \
    -command "exec $linuxcnc::TCL_BIN_DIR/emccalib.tcl -- -ini $EMC_INIFILE &"
$settingsmenu add command -label [msgcat::mc "HAL Show..."] \
    -command "exec $linuxcnc::TCL_BIN_DIR/halshow.tcl &"
$settingsmenu add command -label [msgcat::mc "HAL Config..."] \
    -command "exec $linuxcnc::TCL_BIN_DIR/halconfig.tcl -- -ini $EMC_INIFILE &"


# info menu
$infomenu add command -label [msgcat::mc "Program File"] \
    -command {mText [msgcat::mc "Program file is %s" $programnamestring]}
$infomenu add command -label [msgcat::mc "Editor File"] \
    -command {mText [msgcat::mc "Editor file is %s" $editFilename]}
$infomenu add command -label [msgcat::mc "Parameter File"] \
    -command {mText [msgcat::mc "Parameter file is %s" $paramfilename]}
$infomenu add command -label [msgcat::mc "Tool File"] \
    -command {mText [msgcat::mc "Tool file is %s" $toolfilename]}
$infomenu add separator
$infomenu add command -label [msgcat::mc "Active G Codes"] -command \
    {mText [msgcat::mc "Active codes include; \n%s" $programcodestring]}

# help menu
$helpmenu add checkbutton -label [msgcat::mc "Help..."] -variable popArray(Help) -command {popIn Help} -underline 0
$helpmenu add command -label [msgcat::mc "About..."] -command {popupAboutx} -underline 0

# ----------UP WIDGETS----------

# Fill the up frame with buttons.
# estop -- note toggleEstop is with coordinates setup
set estoplabel [emc_estop]
set stopbutton [button $up.stopbutton -textvariable estoplabel \
    -bg red -activebackground red3 -activeforeground white \
        -width 8 -height 2  -takefocus 0 \
        -command {
            toggleEstop
        }
    ]

# abortbutton
set abortbutton [button $up.abort -text [msgcat::mc "ABORT"] -width 8  -takefocus 0 -borderwidth 2 \
    -bg yellow -fg black -activebackground gold3 -activeforeground white ]
bind $abortbutton <ButtonPress-1> {emc_abort ; showRestart}

# feedhold
set feedholdbutton [button $up.feedhold -textvariable feedlabel -width 8  -takefocus 0 \
    -borderwidth 2 ]
bind $feedholdbutton <ButtonPress-1> {toggleFeedhold}

# initial feedhold conditions
set feedvalue "continue"
set feedlabel [msgcat::mc "CONTINUE"]
set feedoverride [emc_feed_override]
set lastfeedoverride 100
toggleFeedhold

# mode MDI
set mdibutton [button $up.mdi -text [msgcat::mc "MDI"] -width 8  -takefocus 0  -borderwidth 2    ]
bind $mdibutton <ButtonPress-1> {showMode mdi}

# mode AUTO
set autobutton [button $up.auto -text [msgcat::mc "AUTO"] -width 8  -takefocus 0  -borderwidth 2    ]
bind $autobutton <ButtonPress-1> {showMode auto}

# mode MANUAL
set manbutton [button $up.manual -text [msgcat::mc "MANUAL"] -width 8  -takefocus 0  -borderwidth 2   ]
bind $manbutton <ButtonPress-1> {showMode manual}

# temp Settings
# set setbutton [button $up.set -text "S"  -takefocus 0  -borderwidth 2  ]
# bind $setbutton <ButtonPress-1> {popupSetupPage}

pack $stopbutton $abortbutton $feedholdbutton $mdibutton $autobutton \
    $manbutton -side right -fill both -expand yes


# ----------LEFT WIDGETS ----------

# The left widgets show machine state and position
# First set some variables for use by the widgets when defined

set toolsetting 0
set tooloffsetsetting 0.0000
set offsetsetting ""
set unitsetting "auto"
set oldunitsetting $unitsetting
# jointworld can be joint or world
# set initial conditions for older emc systems using identity between joint/world.

    # set the unit information
    catch {set unitsetting [emc_display_linear_units] }
    if {$oldunitsetting != $unitsetting} {
        set oldunitsetting $unitsetting
    }


# FIXME - Remove the jointworld stuff throughout
set jointworld "world"
foreach axnum $axiscoordmap {
    set "poslabel${axnum}" [set "worldlabel${axnum}"]
    # create poslabel name list for globals
    lappend posnames poslabel${axnum} posdigit${axnum}
}

set fontfamily Courier
set fontsize 36
set fontstyle bold
set userfont [emc_ini "POSITION_FONT" "DISPLAY"]

if {$userfont != ""} {
    set fontfamily [lindex $userfont 0]
    set fontsize [lindex $userfont 1]
    set fontstyle [lindex $userfont 2]
    set nf $userfont
}

# Machine display widgets are grouped into frames

set tools [frame $left.tools ]
set toollabel [label $tools.toollabel -text [msgcat::mc "Tool \#:"] -anchor w]
set toolsetting [label $tools.toolsetting -textvariable toolsetting -width 2 -anchor w]
set tooloffsetlabel [label $tools.tooloffsetlabel -text [msgcat::mc "Length :"] -anchor w]
set tooloffsetsetting [label $tools.tooloffsetsetting -textvariable tooloffsetsetting -anchor e]
grid configure $toollabel -row 0 -column 0 -sticky ew
grid configure $toolsetting -row 0 -column 1 -sticky ew
grid configure $tooloffsetlabel -row 0 -column 2 -sticky ew
grid configure $tooloffsetsetting -row 0 -column 3 -sticky ew
grid columnconfigure $tools 0 -weight 1
grid columnconfigure $tools 1 -weight 1
grid columnconfigure $tools 2 -weight 1

set offsets [frame $left.offsets ]
set offsetlabel [label $offsets.offsetlabel -text [msgcat::mc "Work Offsets: "] -anchor w ]
set workoffsets [label $offsets.offsetsetting -textvariable offsetsetting ]
bind $offsets <ButtonPress-1> {mText [msgcat::mc "start offsets popup here"]}
grid configure $offsetlabel -row 0 -column 0 -sticky ew
grid configure $workoffsets -row 1 -column 0 -sticky ew
grid columnconfigure $offsets 1 -weight 1

# Defines position display widgets and some position related variables
set position [frame $left.position ]

# Selector frame
set coords [string tolower [emc_ini POSITION_OFFSET DISPLAY]]
set actcmd [string tolower [emc_ini POSITION_FEEDBACK DISPLAY]]
set selector [frame $left.sel ]
set relmac [label $selector.rel -textvariable coords -width 8 -takefocus 0 -anchor c  ]
set actcom [label $selector.act -textvariable actcmd -width 8 -takefocus 0 -anchor c  ]
set linunit [label $selector.unit -textvariable unitsetting  -width 8 -takefocus 0 -anchor c ]
pack $relmac $actcom $linunit -side left -fill x -expand yes
# pack $selector -side top -fill x -expand yes

foreach axnum $axiscoordmap {
    set pos${axnum} [frame $position.pos${axnum} -borderwidth 3]
    pack [set pos${axnum}] -side top -fill x -expand yes
    set "pos${axnum}l" [label [set "pos${axnum}"].l -textvariable poslabel${axnum} -width 1 -anchor w]
    set "pos${axnum}d" [label [set "pos${axnum}"].d -textvariable posdigit${axnum} -width 6 -anchor w]
    pack [set "pos${axnum}l"] -side left
    pack [set "pos${axnum}d"] -side left -fill x -expand yes
    bind [set "pos${axnum}l"] <ButtonPress-1> "axisSelectx ${axnum}"
    bind [set "pos${axnum}d"] <ButtonPress-1> "axisSelectx ${axnum}"
    # add widget names to posnames
    lappend posnames pos${axnum} "pos${axnum}l" "pos${axnum}d" jointlabel${axnum} worldlabel${axnum}
}

set realfeedoverride [emc_feed_override]
set feedoverride $realfeedoverride
set oldfeedoverride $feedoverride

# read the max feed override from the ini file
set temp [emc_ini "MAX_FEED_OVERRIDE" "DISPLAY"]
if { [string length $temp] == 0} {
    set temp 1
}
set maxFeedOverride [int [expr $temp * 100 + 0.5]]
set oridescalelength [expr $swidth / 4 ]

set oride [frame $left.oride ]
bind $oride <ButtonPress-1> {mText [msgcat::mc "start override popup here"]}
set oridetop [frame $oride.top]
set oridebottom [frame $oride.bottom]
set oridelabel [label $oridetop.label -text [msgcat::mc "Feed Override:"] ]
set oridevalue [label $oridetop.value -textvariable realfeedoverride ]
set oridescale [scale $oridebottom.scale -length $oridescalelength -from 0 -to $maxFeedOverride \
    -variable realfeedoverride -orient horizontal -showvalue 0 -command {emc_feed_override} -takefocus 0]
pack $oridelabel -side left
pack $oridevalue -side right -padx 2m ; # don't bump against label
pack $oridescale -side top
pack $oridetop $oridebottom -side top -fill x

# Messages are reported here.
set mview [frame $left.message ]
pack $mview -side top -fill both -expand yes
set mbutton [button $mview.button -text [msgcat::mc "-- MESSAGES --"] -bd 2 \
    -command {$mtext delete 1.0 end; set messagenumber 1} ]
bind "." <Alt-KeyPress-m> {$mtext delete 1.0 end; set messagenumber 1}
pack $mbutton -side top -padx 3 -pady 3 -fill x
set mtext [text $mview.text -relief flat -width 30 -wrap word -takefocus 0]
pack $mtext -side top -padx 2 -pady 2 -fill both -expand yes

# proc mText writes messages into the mtext widget using mText {text to be displayed}
set messagenumber 1
proc mText {text} {
    global mtext messagenumber
    $mtext insert end "$messagenumber - $text \n-----\n"
    incr messagenumber
}

# Grid the left side machine displays here

grid $position -row 1 -sticky ew -padx 2 -pady 2
grid $oride -row 4 -sticky ew -padx 2 -pady 2
grid $mview -row 6 -sticky nsew -padx 2 -pady 2

grid columnconfigure $left 0 -weight 1
# grid rowconfigure $left 5 -weight 1

proc viewInfo {what} {
    global viewArray selector tools offsets
    if {$viewArray($what) == 1} {
        switch -- $what {
            Position {grid $selector -row 0 -sticky ew -padx 2 -pady 2}
            Tool {grid $tools -row 2 -sticky ew -padx 2 -pady 2}
            Offset {grid $offsets -row 3 -sticky ew -padx 2 -pady 2}
            default { }
        }
    } else {
        switch -- $what {
            Position {grid forget $selector }
            Tool {grid forget $tools }
            Offset {grid forget $offsets }
            default { }
        }
    }
}

setfontx
update

# ----------RIGHT WIDGETS----------

# toggleView moves the variable widget stack focus up one
proc toggleView {} {
puts [msgcat::mc "toggleView's not doin nothin yet"]
}

# popin control routines
# This is the basic traverse keypress for changing popins.
bind . <Alt-KeyPress-space> {popInToggle}
bind . <ButtonPress-3> {popInToggle}

proc popInToggle {} {
    global popinframe
    set temp ""
    set temp [winfo children $popinframe]
    if {$temp != ""} {
        foreach win $temp {
            catch "$win.textframe.textwin config -state disabled"
            pack forget $win
        }
        lower [lindex $temp end]
        set thisone [lindex [winfo children $popinframe] end ]
        pack $thisone -fill both -expand yes
        catch "$thisone.textframe.textwin config -state normal"
    }
}

proc popIn {which} {
    global popinframe popArray undo_id
    # this uses pack forget to hide all existing popins
    set temp ""
    set temp [winfo children $popinframe]
    if {$temp != ""} {
        foreach win $temp {
            catch "$win.textframe.textwin config -state disabled"
            pack forget $win
        }
    }
    # this executes the new popin command
    if {$popArray($which) == 1 } {
        popin$which
    } else {
    set temp [string tolower $which]
    destroy $popinframe.$temp
    popInToggle
    }
}


# Be a bit careful with this with steppers it homes all axi at the same time.
# Will have to add the var file read here rather than in popin
proc setAllZero { } {
    global vartext numaxis tooloffsetsetting
    findVarNumbers
    if {$tooloffsetsetting > 0.0001 } {
        mText [msgcat::mc "Can't set zero with a tool offset active so I issued G49 to cancel it."]
        emc_mode mdi
        emc_wait received
        emc_mdi g49
    }
    # zero out g92 here
    emc_mode mdi
    emc_wait received
    emc_mdi "g92.1"
    # Now zero out all coordinate offfsets
    foreach firstcoord {5211 5221 5241 5261 5281 5301 5321 5341 5361 5381} {
        for {set i 0} {$i < $numaxis} {incr i} {
            global val$i oval$i
            set val$i 0.000000
            set oval$i 0.000000
            set locate [$vartext search [expr $firstcoord + $i] 1.0 ]
            $vartext mark set insert "$locate +5c"
            $vartext delete insert "insert lineend"
            $vartext insert insert [set val$i]
        }
        setVarValues
    }
    # Now write to file and load to emc
    loadVarFile
    # Now set g54 as current offset
    emc_mode mdi
    emc_wait received
    emc_mdi "g54"
    # Now home all axes.
    emc_mode "manual"
    for {set i 0} {$i < $numaxis} {incr i}  {
        emc_home "$i"
    }
}

proc setVarValues {} {
    global vartext
    global numaxis val0 val1 val2 val3 val4 val5
    global num0 num1 num2 num3 num4 num5
    for {set i 0} {$i < $numaxis} {incr i} {
        set locate [$vartext search [set num$i] 1.0 ]
        $vartext mark set insert "$locate +5c"
        $vartext delete insert "insert lineend"
        $vartext insert insert [set val$i]
    }
}

proc saveFile {textwidget name} {
    global $textwidget
    catch {file copy -force $name $name.bak}
    if { [catch {open $name w} fileout] } {
        mText "can't save $name"
        return
    }
    puts $fileout [$textwidget get 1.0 end]
    catch {close $fileout}
}

proc loadVarFile {} {
    global top vartext paramfilename programstatusstring
    saveFile $vartext $paramfilename
    if {$programstatusstring == "idle"} {
        emc_task_plan_init
    } else {
        mText [msgcat::mc "Can't update the var file while machine is in auto and %s." $programstatusstring]
    }
    focus -force "."
}

# -----end of coord -----

# -----RIGHT HELP-----

proc popinHelp {} {
    set HELPFILE [emc_ini HELP_FILE DISPLAY ]
    global popinframe
    set helpwidth 80
    set helpheight 30
    set helpframe [frame $popinframe.help ]
    pack $helpframe -side top -fill both -expand yes
    set helptextframe [frame $helpframe.textframe ]
    set helptextwin [text  $helptextframe.textwin  -width $helpwidth -height $helpheight -padx 4 \
        -wrap word -yscrollcommand "helpScrolltext $helptextframe" ]
    set scrolly $helptextframe.scrolly
    scrollbar $scrolly -orient vert -command "$helptextwin yview" -width 12
    pack $scrolly -side right -fill y
    pack $helptextwin -side top -fill both -expand true
    pack $helptextframe -side top -fill both -expand yes
    # insert contents of filename, if it exists
    set fname $linuxcnc::HELP_DIR/$HELPFILE
    if { [catch {open $fname} filein] } {
        mText [msgcat::mc "can't open %s" $fname]
    } else {
        $helptextwin delete 1.0 end
        $helptextwin insert end [read $filein]
        catch {close $filein}
    }
}

proc helpScrolltext {tf a b} {
    $tf.scrolly set $a $b
}

# ----------DOWN WIDGETS----------

# add down frames for each emc mode and setup

set mdiframe [frame $down.mdi]
set autoframe [frame $down.auto]
set manframe [frame $down.manual]
set setframe [frame $down.set]
set spindframe [frame $manframe.spindle]

# FIXME REMOVE THESE

# Below are the settings window widgets packed by process tcl/lib/extkemc/showMode.tcl
# These are being removed to other locations and will be cleaned completely

set limoridebutton [button $setframe.button -text [msgcat::mc "override limits"] -command {toggleLimitOverride} -takefocus 0]
set limoridebuttonbg [$limoridebutton cget -background]
set limoridebuttonabg [$limoridebutton cget -activebackground]
# bind $limoridebutton <ButtonPress-1> {emc_override_limit}

set homebutton [button $setframe.home -text [msgcat::mc "home"] -takefocus 0]
bind $homebutton <ButtonPress-1> {emc_home [ lindex $axiscoordmap $activeAxis] }

set offsetsetting ""

grid $limoridebutton -column 0 -row 0 -sticky nsew
grid $homebutton -column 0 -row 3 -sticky nsew


# ----------MANUAL MODE WIDGETS----------

# read the default jog speed
set temp [emc_ini "DEFAULT_VELOCITY" "TRAJ"]
if { [string length $temp] == 0} {
    set temp 1
}
set jogSpeed [int [expr $temp * 60 + 0.5]]
set defaultJogSpeed $jogSpeed

set maxJogSpeed 1

set feeddefault [button $manframe.default -text [msgcat::mc "DEFAULT"] -width 10 -command {set jogSpeed $defaultJogSpeed}]
set feedlabel [label $manframe.label -text [msgcat::mc "Speed:"] -width 10]
set feedvalue [label $manframe.value -textvariable jogSpeed -width 10 -anchor w]
set feedspace [label $manframe.space -text "" -width 10 ]
set feedrapid [button $manframe.rapid -text [msgcat::mc "RAPID"] -width 10 -command {set jogSpeed $maxJogSpeed}]
set feedscale [scale $manframe.scale -length 270 -from 0 -to $maxJogSpeed -variable jogSpeed -orient horizontal -showvalue 0 -takefocus 0]
bind $feedlabel <ButtonPress-1> {popupJogSpeed}
bind $feedlabel <ButtonPress-3> {popupJogSpeed}
bind $feedvalue <ButtonPress-1> {popupJogSpeed}
bind $feedvalue <ButtonPress-3> {popupJogSpeed}

proc popupJogSpeed {} {
    global jogSpeed maxJogSpeed popupJogSpeedEntry

    if {[winfo exists .jogspeedpopup]} {
        wm deiconify .jogspeedpopup
        raise .jogspeedpopup
        focus .jogspeedpopup
        return
    }
    toplevel .jogspeedpopup
    wm title .jogspeedpopup [msgcat::mc "Set Jog Speed"]

    # initialize value to current jog speed
    set popupJogSpeedEntry $jogSpeed

    frame .jogspeedpopup.input
    label .jogspeedpopup.input.label -text [msgcat::mc "Set jog speed:"]
    entry .jogspeedpopup.input.entry -textvariable popupJogSpeedEntry -width 20
    frame .jogspeedpopup.buttons
    button .jogspeedpopup.buttons.ok -text [msgcat::mc "OK"] -default active -command {set jogSpeed $popupJogSpeedEntry; destroy .jogspeedpopup}
    button .jogspeedpopup.buttons.cancel -text [msgcat::mc "Cancel"] -command "destroy .jogspeedpopup"
    pack .jogspeedpopup.input.label .jogspeedpopup.input.entry -side left
    pack .jogspeedpopup.input -side top
    pack .jogspeedpopup.buttons -side bottom -fill x -pady 2m
    pack .jogspeedpopup.buttons.ok .jogspeedpopup.buttons.cancel -side left -expand 1
    bind .jogspeedpopup <Return> {set jogSpeed $popupJogSpeedEntry; destroy .jogspeedpopup}

    focus .jogspeedpopup.input.entry
    .jogspeedpopup.input.entry select range 0 end
}

# Experimental increment button sets for linear and rotary axes

set iframe [frame $manframe.linearincrement -borderwidth 1 -relief sunken ]
set irframe [frame $manframe.rotaryincrement -borderwidth 1 -relief sunken ]

radiobutton $iframe.r1 -text "0.0001" -variable jogIncrement -value "0.0001" -anchor w \
        -anchor w -padx 4 -command {set jogtype increment} -state disabled -width 7
radiobutton $iframe.r2 -text "0.0010" -variable jogIncrement -value "0.0010" -anchor w \
        -anchor w -padx 4 -command {set jogtype increment} -state disabled
radiobutton $iframe.r3 -text "0.0100" -variable jogIncrement -value "0.0100" -anchor w \
        -anchor w -padx 4 -command {set jogtype increment} -state disabled
radiobutton $iframe.r4 -text "0.1000" -variable jogIncrement -value "0.1000" -anchor w \
        -anchor w -padx 4 -command {set jogtype increment} -state disabled
radiobutton $iframe.r5 -text "1.0000" -variable jogIncrement -value "1.0000" -anchor w \
        -anchor w -padx 4 -command {set jogtype increment} -state disabled
pack $iframe.r5 $iframe.r4 $iframe.r3 $iframe.r2 $iframe.r1 -side top -fill both -expand yes

set immframe [frame $manframe.metricincrement -borderwidth 1 -relief sunken ]
radiobutton $immframe.r1 -text "0.01" -variable jogIncrement -value "0.0100" -anchor w \
        -anchor w -padx 4 -command {set jogtype increment} -state disabled -width 7
radiobutton $immframe.r2 -text "0.10" -variable jogIncrement -value "0.1000" -anchor w \
        -anchor w -padx 4 -command {set jogtype increment} -state disabled
radiobutton $immframe.r3 -text "1.00" -variable jogIncrement -value "1.0000" -anchor w \
        -anchor w -padx 4 -command {set jogtype increment} -state disabled
radiobutton $immframe.r4 -text "5.00" -variable jogIncrement -value "5.0000" -anchor w \
        -anchor w -padx 4 -command {set jogtype increment} -state disabled
radiobutton $immframe.r5 -text "10.00" -variable jogIncrement -value "10.0000" -anchor w \
        -anchor w -padx 4 -command {set jogtype increment} -state disabled
pack $immframe.r5 $immframe.r4 $immframe.r3 $immframe.r2 $immframe.r1 -side top -fill both -expand yes


radiobutton $irframe.r1 -text "0.01" -variable jogIncrement -value 0.01 -anchor w \
        -anchor w -padx 4 -command {set jogtype increment} -state disabled  -width 7
radiobutton $irframe.r2 -text "0.10" -variable jogIncrement -value 0.1 -anchor w \
        -anchor w -padx 4 -command {set jogtype increment} -state disabled
radiobutton $irframe.r3 -text "1.00" -variable jogIncrement -value 1 -anchor w \
        -anchor w -padx 4 -command {set jogtype increment} -state disabled
radiobutton $irframe.r4 -text "15.00" -variable jogIncrement -value 15 -anchor w \
        -anchor w -padx 4 -command {set jogtype increment} -state disabled
radiobutton $irframe.r5 -text "90.00" -variable jogIncrement -value 90  -anchor w \
        -anchor w -padx 4 -command {set jogtype increment} -state disabled
pack $irframe.r5 $irframe.r4 $irframe.r3 $irframe.r2 $irframe.r1 -side top -fill both -expand yes

set jogplustext [msgcat::mc "JOG X +"]
set jognegtext [msgcat::mc "JOG X -"]

set jognegbutton [button $manframe.neg -textvariable jognegtext -width 10 -takefocus 0]
set jogposbutton [button $manframe.pos -textvariable jogplustext -width 10 -takefocus 0]
set jogincrement [button $manframe.incr -text [msgcat::mc "increment"] -takefocus 0 \
    -width 8 -command {setJogType increment}]
set jogcontinuous [button $manframe.cont -text [msgcat::mc "continuous"] -takefocus 0 \
    -width 8 -command {setJogType continuous}]
set joebutton [button $manframe.bzero -text [msgcat::mc "A\nL\nL\n\nZ\nE\nR\nO"] \
    -command {setAllZero} -bg lightgreen -activebackground green3 \
    -activeforeground white]
set homebutton [button $manframe.bhome -textvariable axishometext \
    -command {emc_home $activeAxis} -bg lightgreen -activebackground green3 \
    -activeforeground white]
if { [emc_machine] == "on" } {
    $joebutton configure -state normal -bg lightgreen
    $homebutton configure -state normal -bg lightgreen
}

bind $jognegbutton <ButtonPress-1> {jogNeg $activeAxis}
bind $jognegbutton <ButtonRelease-1> {jogStop}
bind $jogposbutton <ButtonPress-1> {jogPos $activeAxis}
bind $jogposbutton <ButtonRelease-1> {jogStop}

grid configure $homebutton -column 0 -row 0 -rowspan 5 -sticky nsew
grid $feeddefault -column 1 -row 0 -sticky nsew
grid $feedlabel -column 2 -row 0 -sticky nsew
grid $feedvalue -column 3 -row 0 -sticky nsew
grid $feedspace -column 4 -row 0 -sticky nsew
grid $feedrapid -column 5 -row 0 -sticky nsew
grid $feedscale -column 1 -row 1 -sticky nsew
grid configure $feedscale -columnspan 5
grid $jognegbutton -column 1 -row 2 -sticky nsew
grid configure $jognegbutton -columnspan 2 -rowspan 3
grid $jogincrement -column 3 -row 2 -sticky nsew -padx 4
grid $jogcontinuous -column 3 -row 4 -sticky nsew -padx 4
grid $jogposbutton -column 4 -row 2 -sticky nsew
grid configure $jogposbutton -columnspan 2 -rowspan 3

if { $unitsetting == "(mm)" } {
    grid configure $immframe -column 6 -row 0 -rowspan 5 -sticky nsew
} else {
    grid configure $iframe -column 6 -row 0 -rowspan 5 -sticky nsew
}

# grid $manframe.r5 -column 6 -row 0 -sticky nsew
# grid $manframe.r4 -column 6 -row 1 -sticky nsew
# grid $manframe.r3 -column 6 -row 2 -sticky nsew
# grid $manframe.r2 -column 6 -row 3 -sticky nsew
# grid $manframe.r1 -column 6 -row 4 -sticky nsew


grid configure $joebutton -column 7 -row 0 -rowspan 5 -sticky ns
grid columnconfigure $manframe 0 -weight 1
grid columnconfigure $manframe 1 -weight 3
grid columnconfigure $manframe 2 -weight 1
grid columnconfigure $manframe 3 -weight 1
grid columnconfigure $manframe 4 -weight 1
grid columnconfigure $manframe 5 -weight 3
grid columnconfigure $manframe 6 -weight 1
grid rowconfigure $manframe 0 -weight 1
grid rowconfigure $manframe 1 -weight 1
grid rowconfigure $manframe 2 -weight 1
grid rowconfigure $manframe 3 -weight 1
grid rowconfigure $manframe 4 -weight 1

# Initialize with axis 0
axisSelectx [ lindex $axiscoordmap 0 ]

update

# ----------SPINDLE CONTROL WIDGETS----------
# popped in with full right manual mode

set sdoing [label $spindframe.label1 -textvariable spindlelabel -relief flat \
        -width 20]
set sforbutton [button $spindframe.forward -text [msgcat::mc "Spindle Forward"] -command {emc_spindle forward}]
set srevbutton [button $spindframe.reverse -text [msgcat::mc "Spindle Reverse"] -command {emc_spindle reverse}]
set sstopbutton [button $spindframe.stop -text [msgcat::mc "Spindle off"] -command {emc_spindle off}]

set decrbutton [button $spindframe.decr -text [msgcat::mc "Spindle Slower"] -takefocus 0]
bind $decrbutton <ButtonPress-1> {emc_spindle decrease}
bind $decrbutton <ButtonRelease-1> {emc_spindle constant}

set incrbutton [button $spindframe.incr -text [msgcat::mc "Spindle Faster"] -takefocus 0]
bind $incrbutton <ButtonPress-1> {emc_spindle increase}
bind $incrbutton <ButtonRelease-1> {emc_spindle constant}

set brakebutton [button $spindframe.brake -textvariable brakelabel -relief raised \
    -command {toggleBrake}]

set mistbutton [button $spindframe.mist -textvariable mistlabel -relief raised -width 15 \
    -command {toggleMist}
    ]
set floodbutton [button $spindframe.flood -textvariable floodlabel -relief raised -width 15 \
    -command {toggleFlood}
    ]

grid $decrbutton -column 0 -row 0 -sticky nsew
grid $sdoing -column 1 -row 0 -sticky nsew
grid $incrbutton -column 2 -row 0 -sticky nsew
grid $srevbutton -column 0 -row 1 -sticky nsew
grid $sstopbutton -column 1 -row 1 -sticky nsew
grid $sforbutton -column 2 -row 1 -sticky nsew
grid $mistbutton -column 0 -row 2 -sticky nsew
grid $brakebutton -column 1 -row 2 -sticky nsew
grid $floodbutton -column 2 -row 2 -sticky nsew
grid columnconfigure $spindframe 0 -weight 1
grid columnconfigure $spindframe 1 -weight 1
grid columnconfigure $spindframe 2 -weight 1
grid rowconfigure $spindframe 0 -weight 1
grid rowconfigure $spindframe 1 -weight 1
grid rowconfigure $spindframe 2 -weight 1




# ----------MDI MODE WIDGETS----------

set mditext ""
set mdilabel [label $mdiframe.label -text [msgcat::mc "MDI:"] -width 4 -anchor w]
set mdientry [entry $mdiframe.entry -textvariable mditext -width 73 -takefocus 1]
bind $mdientry <Return> {$mdientry select range 0 end ; sendMdi $mditext}

set programcodestring ""
set programcodes [label $mdiframe.programcodes -textvariable programcodestring]

grid $mdilabel
grid $mdientry
grid $programcodes


# ----------AUTO MODE WIDGETS----------
# FIXME    only popin restart widgets after abort or estop. remove on start

set programnamestring "none"
set programstatusstring "none"
set programin 0
set activeLine 0

# programstatusname is "Program: <name>" half of programstatus
# set programstatusname [frame $programstatus.name]
# set programstatusnamelabel [label $programstatusname.label -text "Program: "]
# set programstatusnamevalue [label $programstatusname.value -textvariable programnamestring -width 40 -anchor e]

# programstatusstatus is "Status: idle" half of programstatus
# set programstatusstatus [frame $programstatus.status]
# set programstatusstatuslabel [label $programstatusstatus.label -text "  -  Status: "]

# pack $programstatusname $programstatusstatus -side left
# pack $programstatusnamelabel $programstatusnamevalue -side left
# pack $programstatusstatuslabel $programstatusstatusvalue -side left

# programframe is the frame for the button widgets for run, pause, etc.
set programframe [frame $autoframe.programframe]
set programopenbutton [button $programframe.open -text [msgcat::mc "Open..."] -width 7 \
    -command {fileDialog} -takefocus 0]
set programrunbutton [button $programframe.run -text [msgcat::mc "Run"] -width 7\
    -command {emc_run $runMark ; set runMark 0} -takefocus 0]
set programpausebutton [button $programframe.pause -text [msgcat::mc "Pause"] -width 7 \
    -command {emc_pause} -takefocus 0]
set programresumebutton [button $programframe.resume -text [msgcat::mc "Resume"]  -width 7 \
    -command {emc_resume} -takefocus 0]
set programstepbutton [button $programframe.step -text [msgcat::mc "Step"]  -width 7 \
    -command {emc_step} -takefocus 0]
set programverifybutton [button $programframe.verify -text [msgcat::mc "Verify"]  -width 7 \
    -command {emc_run -1} -takefocus 0]
set programstatusstatusvalue [label $programframe.value -textvariable programstatusstring  -width 7 ]
pack $programopenbutton $programrunbutton $programpausebutton $programresumebutton \
    $programstepbutton $programverifybutton $programstatusstatusvalue -side left -fill x -expand true

set textwidth "70"
set textheight "40"
set restartline 1
set oldrestartline 1

# programfileframe is the frame for the program text widget showing the file
set programfileframe [frame $autoframe.programfileframe]
set programfiletext [text $programfileframe.text -height $textheight -width $textwidth \
    -borderwidth 1 -state disabled]
$programfiletext tag configure highlight -background red3 -foreground white
$programfiletext tag configure restart -background blue  -foreground white

set programrestart [frame $programfileframe.restart ]
# leave these unpacked until abort in auto or extop in auto.

set activelabel [label $programrestart.l1 -text [msgcat::mc "RESTART LINE"] ]
set activedecr [button $programrestart.b1 -text [msgcat::mc "Back"] -command {incr restartline -1} ]
set activeincr [button $programrestart.b2 -text [msgcat::mc "Ahead"] -command {incr restartline 1} ]
set activerestart [button $programrestart.b3 -text [msgcat::mc "Restart"] -command {emc_run $restartline ; hideRestart} ]
pack $activelabel $activedecr $activeincr $activerestart -side top -fill both -expand yes

pack $programfiletext  -side left -fill y
pack $programrestart -side left -fill both -expand yes
pack $programframe $programfileframe -side top -anchor w -fill both -expand yes

proc fileDialog {} {
    global programDirectory programnamestring

    set allfilestring [msgcat::mc "All files"]
    set textfilestring [msgcat::mc "Text files"]
    set ncfilestring [msgcat::mc "NC files"]

    set all [list ]
    lappend all $allfilestring
    lappend all *
    set text [list ]
    lappend text $textfilestring
    lappend text ".txt"
    set nc [list ]
    lappend nc $ncfilestring
    lappend nc ".nc .ngc"

    set types [list ]
    lappend types $all
    lappend types $text
    lappend types $nc

    set f [tk_getOpenFile -filetypes $types -initialdir $programDirectory]
    if {[string len $f] > 0} {
        set programDirectory [file dirname $f]
        set programnamestring $f
        loadProgramText
        loadProgram
        catch {openoninit $f }
        outccount
    }
}

# processes to load the program run display widget

proc changeProgram {} {
    global programnamestring editFilename
    set programnamestring $editFilename
    loadProgram
    loadProgramText
}

proc loadProgram {} {
    global programnamestring programstatusstring
    if { $programstatusstring != "idle" } {
        set tempx [ tk_dialog .d1 ???? [msgcat::mc "The interpreter is running. \n\
            Pressing OK will abort and load the new program"] \
            "" 0 Ok Cancel ]
    } else {
        set tempx 0
    }
    if {$tempx == 0} {
        emc_abort
        emc_wait done
        emc_open $programnamestring
    }
}

proc loadProgramText {} {
    global programnamestring programfiletext
    # clear out program text
    $programfiletext config -state normal
    $programfiletext delete 1.0 end
    # close the current program
    catch {close $programin}
    # open the new program, if it's not "none"
    if { [catch {open $programnamestring} programin] } {
        puts stdout [msgcat::mc "can't open %s" $programnamestring]
    } else {
        $programfiletext insert end [read $programin]
        $programfiletext config -state disabled
        catch {close $programin}
    }
}

proc hideRestart {} {
    global activelabel activedecr activeincr activerestart
    pack forget $activelabel $activedecr $activeincr $activerestart
}

proc showRestart {} {
    global activelabel activedecr activeincr activerestart
    pack $activelabel $activedecr $activeincr $activerestart -side top -fill both -expand yes
}

hideRestart
toggleEstop


# ----------INITIAL VALUES FOR LOOP----------

setKeyBindingsx
set syncingFeedOverride 0
set minusAxis -1
set equalAxis -1

# force explicit updates, so calls to emc_estop, for example, don't
# always cause an NML read; they just return last latched status
emc_update none

# Set up the display for manual mode, which may be reset
# immediately in updateStatus if it's not correct.
# This lets us check if there's a change and reset bindings, etc.
# on transitions instead of every cycle.
set modelabel [msgcat::mc "MANUAL"]

# Set manual bindings
showMode manual

# Record that we're in manual display mode
set modeInDisplay "manual"
set oldmode "manual"
set jogType incremental
setJogType continuous

set lastjointworld $jointworld
set lastactcmd $actcmd
set lastcoords $coords
set emc_teleop_enable_command_given 0
set offsetactive 3
set oldoffsetactive 3
set oldrestartline 0

axisSelectx "X"


# ----------LOOP TO SET VALUES ----------

proc updateMini {} {
    global emc_teleop_enable_command_given
    global lastjointworld lastactcmd lastcoords
    global displayCycleTime
    global mistbutton floodbutton spindlebutton brakebutton
    global modeInDisplay
    global estoplabel modelabel mistlabel floodlabel lubelabel spindlelabel brakelabel
    global toolsetting tooloffsetsetting offsetsetting
    global unitlabel unitsetting oldunitsetting
    global actcmd coords jointworld axiscoordmap worldlabellist
    global posnames activeAxis axisType
    foreach item $posnames {
        global $item
    }
    global radiorel radiocmd radioact radioabs
    global limoridebutton limoridebuttonbg limoridebuttonabg stopbutton
    global realfeedoverride feedoverride syncingFeedOverride
    global mdientry
    global programcodestring
    global programnamestring programin activeLine programstatusstring
    global programfiletext
#    global taskhb taskcmd tasknum taskstatus
#    global iohb iocmd ionum iostatus
#    global motionhb motioncmd motionnum motionstatus
    global oldstatusstring jogSpeed
    global axiscoordmap
    global popinframe feedholdbutton stopbutton feedvalue feedlabel
    global isnew oldmode workoffsets defbg offsetactive oldoffsetactive
    global restartline oldrestartline

    # force an update of text log
    set thisError [emc_error]
    set thisText [emc_operator_text]
    set thisDisplay [emc_operator_display]
    if {$thisError != "ok"} {mText $thisError}
    if {$thisText != "ok"} {mText $thisText}
    if {$thisDisplay != "ok"} {mText $thisDisplay}

    # force an update of status
    emc_update

    # now update labels

    # set the unit information
    catch {set unitsetting [emc_display_linear_units] }
    if {$oldunitsetting != $unitsetting} {
        set oldunitsetting $unitsetting
    }

    if { [emc_estop] == "on"} {
        set estoplabel [msgcat::mc "ESTOPPED"]
        $stopbutton configure -bg gray -fg black -relief sunken
    } elseif { [emc_machine] == "on"} {
        set estoplabel [msgcat::mc "ESTOP PUSH"]
        $stopbutton configure -bg red -fg black -relief raised
    } else {
        set estoplabel [msgcat::mc "ESTOP RESET"]
        $stopbutton configure -bg green -fg black -relief sunken
    }

    if { [emc_spindle] == "forward"} {
        set spindlelabel [msgcat::mc "SPINDLE FORWARD"]
    } elseif { [emc_spindle] == "reverse"} {
        set spindlelabel [msgcat::mc "SPINDLE REVERSE"]
    } elseif { [emc_spindle] == "off"} {
        set spindlelabel [msgcat::mc "SPINDLE OFF"]
    } elseif { [emc_spindle] == "increase"} {
        set spindlelabel [msgcat::mc "SPINDLE INCREASE"]
    } elseif { [emc_spindle] == "decrease"} {
        set spindlelabel [msgcat::mc "SPINDLE DECREASE"]
    } else {
        set spindlelabel [msgcat::mc "SPINDLE ?"]
    }

    if { [emc_brake] == "on"} {
        set brakelabel [msgcat::mc "BRAKE ON"]
    } elseif { [emc_brake] == "off"} {
        set brakelabel [msgcat::mc "BRAKE OFF"]
    } else {
        set brakelabel [msgcat::mc "BRAKE ?"]
    }

    if { [emc_mist] == "on"} {
        set mistlabel [msgcat::mc "MIST ON"]
    } elseif { [emc_mist] == "off"} {
        set mistlabel [msgcat::mc "MIST OFF"]
    } else {
        set mistlabel [msgcat::mc "MIST ?"]
    }

    if { [emc_flood] == "on"} {
        set floodlabel [msgcat::mc "FLOOD ON"]
    } elseif { [emc_flood] == "off"} {
        set floodlabel [msgcat::mc "FLOOD OFF"]
    } else {
        set floodlabel [msgcat::mc "FLOOD ?"]
    }

    # set the tool information
    set toolsetting [emc_tool]
    set tooloffsetsetting [format "%.4f" [emc_tool_offset]]

    set temp [emc_mode]
    if {$temp != $modeInDisplay} {
        showMode $temp
        axisSelectx $activeAxis
        if {$temp == "auto"} {
            $mdientry config -state disabled
            focus .
            setAutoBindings
        } elseif {$temp == "mdi"} {
            focus $mdientry
            $mdientry select range 0 end
            setMdiBindings
        } else {
            focus .
            setManualBindings
        }
        set modeInDisplay $temp
    }

    if {$jointworld == "joint" } {
        if { $lastjointworld != "joint" } {
            foreach axnum $axiscoordmap {
                set "poslabel${axnum}" [set "jointlabel${axnum}"]
            }
            set lastactcmd $actcmd
            set lastcoords $coords
            set actcmd "actual"
            set coords "machine"
            $radiorel config -state disabled
            $radioabs config -state disabled
            $radioact config -state disabled
            $radiocmd config -state disabled
            set lastjointworld $jointworld
            if { [emc_teleop_enable] == 1  && $modeInDisplay == "manual" } {
                emc_teleop_enable 0
                set emc_teleop_enable_command_given 0
            }
        }
        foreach axnum $axiscoordmap {
            set "posdigit${axnum}" [format "%8.4f" [emc_joint_pos ${axnum}] ]
        }
    } else {
        if { $lastjointworld != "world" } {
            if { [emc_teleop_enable] == 0  && [emc_kinematics_type] != 1 && $modeInDisplay == "manual" } {
                if { $emc_teleop_enable_command_given == 0 } {
                    emc_teleop_enable 1
                    set emc_teleop_enable_command_given 1
                } else {
                    set jointworld "joint"
                    set emc_teleop_enable_command_given 0
                }
            } else {
                foreach axnum $axiscoordmap {
                    set "poslabel${axnum}" [set "worldlabel${axnum}"]
                }
                foreach axname $worldlabellist {
                    set temp "[emc_pos_offset ${axname}]"
                }
                set actcmd $lastactcmd
                set coords $lastcoords
                $radiorel config -state normal
                $radioabs config -state normal
                $radioact config -state normal
                $radiocmd config -state normal
                set lastjointworld $jointworld
            }
        }
        if { $lastjointworld == "world" } {
            if {$coords == "relative" && $actcmd == "commanded"} {
                set whizbang {emc_rel_cmd_pos}
            } elseif {$coords == "relative" && $actcmd == "actual"} {
                set whizbang {emc_rel_act_pos}
            } elseif {$coords == "machine" && $actcmd == "commanded"} {
                set whizbang {emc_abs_cmd_pos}
            } else {
                set whizbang {emc_abs_act_pos}
            }
            foreach axnum $axiscoordmap {
                if {$axisType($axnum) == "LINEAR"} {
                    set posdigit${axnum} [format "%8.4f" [eval $whizbang $axnum]]
                } else {
                    set posdigit${axnum} [format "%8.2f" [expr [eval $whizbang $axnum]]]
                }
            }
        }
    }

    # set the offset information
    set offsetsetting ""
    foreach axname $worldlabellist {
        set temp [format "${axname}%.4f" [emc_pos_offset $axname] ]
        set offsetsetting  "$offsetsetting [join $temp ]"
    }

    # color the numbers
    foreach axnum $axiscoordmap {
        if { [emc_joint_limit ${axnum} ] != "ok"} {
            [set "pos${axnum}d"] config -foreground red
        } elseif { [emc_joint_homed ${axnum} ] == "homed"} {
            [set "pos${axnum}d"] config -foreground darkgreen
        } else {
            [set "pos${axnum}d"] config -foreground gold3
        }
    }

    # set the feed override
    set realfeedoverride [emc_feed_override]
    if {$realfeedoverride == 0} {
            $feedholdbutton configure -bg green -activebackground green  -fg black -activeforeground darkgray
            set feedvalue "continue"
            set feedlabel [msgcat::mc "CONTINUE"]
    } else {
            $feedholdbutton configure -bg red -activebackground red -fg black -activeforeground white
             set feedvalue "feedhold"
             set feedlabel [msgcat::mc "FEEDHOLD"]
    }

    # temporary fix for 0 jog speed problem
    if {$jogSpeed <= 0} {set jogSpeed .000001}

    # fill in the program codes
    set programcodestring [emc_program_codes]

    # fill in the program status
    set oldstatusstring $programstatusstring
    set programstatusstring [emc_program_status]

    # load new program text on status change
    if {$oldstatusstring == "idle" && $programstatusstring == "running"} {
        loadProgramText
    }

    # compute a text line offset for showing activeline or resetline
    set offsetactive [expr [ winfo height $programfiletext ] / \
        [expr  [ font actual $programfiletext  -size ] +4 ] / 2 ]

    # highlight the active line
    if {$programstatusstring != "idle" } {
      set temp [emc_program_line]
        if {$temp != $activeLine} {
          $programfiletext tag remove highlight $activeLine.0 $activeLine.end
          set activeLine $temp
          set restartline $activeLine
          set oldrestartline $restartline
          $programfiletext tag add highlight $activeLine.0 $activeLine.end
          $programfiletext see [expr $activeLine.0 + $offsetactive ]
        }
    } else {
        if {$activeLine > 0} {
            if { $restartline > $activeLine } {
                $programfiletext tag add restart $restartline.0 $restartline.end
                $programfiletext see [expr $restartline.0 + $offsetactive ]
            } elseif { $restartline < $activeLine } {
                $programfiletext tag add restart $restartline.0 $restartline.end
                $programfiletext see [expr $restartline.0 - 3 ]
            }
        }
        if {$restartline != $oldrestartline} {
            $programfiletext tag remove restart $oldrestartline.0 $oldrestartline.end
            set oldrestartline $restartline
        }

    }

    # enable plotting if plotter exists
    if { [winfo exists $popinframe.plot] } {
        updatePlot
    }


# schedule this again
    after $displayCycleTime updateMini
}

# setup the initial conditions of the display here
rightConfig split
update
updateMini
# display should now run while adding in the following components.


# ----------RIGHT - TOOL SETUP----------

proc popinTools {} {
    global popinframe tool tooltext toolframe toolnum
    set tool [frame $popinframe.tools ]
    label $tool.l1 -justify center -text [msgcat::mc "TOOL SETUP \n Click or tab to edit.  \
    Press enter to return to keyboard machine control."]
    # put the tool file into an invisible widget
    set tooltext [text $tool.vartext]
    set toolframe [frame $tool.frame ]
    # set selt [label $toolframe.selt -text "  SELECT  " ]
    set poc [label $toolframe.poc -text [msgcat::mc "  TOOL NUMBER  "] ]
    set len [label $toolframe.len -text [msgcat::mc "  LENGTH  "] ]
    set diam [label $toolframe.diam -text [msgcat::mc "  DIAMETER  "] ]
    set com [label $toolframe.com -text [msgcat::mc "  COMMENT  "] ]
    grid $poc $len $diam $com -sticky ew
    set toolnum 1
    loadToolText
    setToolDisplay
    pack $tool  -fill both -expand yes
}

proc loadToolText {} {
    global tooltext toolfilename
    set toolfilename [emc_ini "TOOL_TABLE" "EMCIO"]
    $tooltext config -state normal
    $tooltext delete 1.0 end
    if { [catch {open $toolfilename} programin] } {
        return
    } else {
        $tooltext insert end [read $programin]
        catch {close $programin}
    }
}

proc setToolDisplay { } {
    global tool tooltext toolframe toolfilename tarray
    catch {unset tarray}
    scan [$tooltext index end] %d nl
    for {set i 2} {$i < $nl} {incr i} {
        set thisline [$tooltext get $i.0 $i.end ]
        set nel [ llength $thisline ]
        if {$thisline != ""} {
            set tarray(${i}0) [lindex $thisline 0 ]
            set tarray(${i}2) [lindex $thisline 2 ]
            set tarray(${i}3) [lindex $thisline 3 ]
            set tarray(${i}4)  ""
            for {set in 4} {$in < $nel} {incr in} {
                lappend tarray(${i}4) [lindex $thisline $in]
            }
            if { [winfo exists $toolframe.l1$i ]  == 0 }  {
                label $toolframe.l1$i -text [set tarray(${i}0) ]
                entry $toolframe.l2$i -textvariable tarray(${i}2) -relief flat -bg white -width 10
                bind $toolframe.l2$i <KeyPress-Return> {updateToolFile}
                entry $toolframe.l3$i -textvariable tarray(${i}3) -relief flat -bg white -width 10
                bind $toolframe.l3$i <KeyPress-Return> {updateToolFile}
                entry $toolframe.l4$i -textvariable tarray(${i}4) -relief flat -bg white -width 10
                bind $toolframe.l4$i <KeyPress-Return> {updateToolFile}
                grid $toolframe.l1$i $toolframe.l2$i $toolframe.l3$i $toolframe.l4$i  -sticky ew
            }
        }
    }
    grid $tool.l1 -padx 12 -pady 12 -sticky ew
    grid $toolframe -sticky ew
    setToolButtons
}

proc setToolButtons { } {
    global toolframe
    set tadd [button $toolframe.add -text [msgcat::mc "Add Extra Tool"] -command {addTool} ]
    set trem [button $toolframe.rem -text [msgcat::mc "Remove Last Tool"] -command {remTool} ]
    grid configure $tadd $trem -columnspan 2 -sticky nsew -pady 5
}

proc addTool {} {
    global toolframe tarray popinframe
    set tempnum [array size tarray]
    set templine [expr $tempnum / 4]
    set toolnumber [expr $templine + 1]
    set toolline [expr $templine + 2]
    set tarray(${toolline}0) $toolnumber
    set tarray(${toolline}2) 0.0
    set tarray(${toolline}3) 0.0
    set tarray(${toolline}4) empty
    updateToolFile
    destroy $popinframe.tools
    popinTools
}

proc remTool {} {
    global tarray tooltext popinframe toolfilename programstatusstring
    set tempnum [array size tarray]
    set templine [expr $tempnum  / 4 + 1 ]
    $tooltext delete $templine.0 [expr $templine +1].0
    saveFile $tooltext $toolfilename
    if {$programstatusstring == "idle"} {
        emc_load_tool_table $toolfilename
    } else {
        mText [msgcat::mc "Can't update the tool file while machine is in auto and %s." $programstatusstring]
    }
    destroy $popinframe.tools
    focus -force "."
    popinTools
}

proc examineVal {widgetname value} {
    global $widgetname
    set temp 0
    catch [set temp [expr $value + 0.0 ] ]
    if {$temp == 0} {
        mText [msgcat::mc "This is not a good number."]
#        focus -force $widgetname
    }
}

proc updateToolFile {} {
    global top tooltext toolfilename tarray programstatusstring
    $tooltext config -state normal
    $tooltext delete 2.0 end
    set tempnum [array size tarray]
    set templines [expr $tempnum  / 4 ]
    set toollines [expr $templines + 1]
    for {set i 2} {$i <= $toollines} {incr i} {
        $tooltext mark set insert end
        $tooltext insert insert "
        " ; # this is a newline to insert in text.  Do not remove
        set thistoolline [ join "$tarray(${i}0) $tarray(${i}0) $tarray(${i}2) $tarray(${i}3) $tarray(${i}4)" "   " ]
        $tooltext mark set insert "$i.0 "
        $tooltext insert insert $thistoolline
    }
    saveFile $tooltext $toolfilename
    if {$programstatusstring == "idle"} {
        emc_load_tool_table $toolfilename
    } else {
        mText [msgcat::mc "Can't update the tool file while machine is in auto and %s." $programstatusstring]
    }
    focus -force "."
}

# ----------end of tool setup----------

update

# ----------RIGHT - OFFSETS SYSTEM SETUP----------

set paramfilename [emc_ini "PARAMETER_FILE" "RS274NGC"]
if { [string length $paramfilename] == 0} {
    set paramfilename "emc.var"
}
set numaxis [emc_ini "AXES" "TRAJ"]
set nameaxis [emc_ini "COORDINATES" "TRAJ"]
# put the parm file into an invisible widget
set vartext [text $top.vartext]
$vartext config -state normal
$vartext delete 1.0 end
if { [catch {open $paramfilename} programin] } {
    return
} else {
    $vartext insert end [read $programin]
    catch {close $programin}
}

# store touchoffradius in var 5218
set touchoffradius 0.0000
# store touchofflength in var 5219
set touchofflength 0.0000
set touchoffdirection "+"

proc popinOffsets { } {
    global coord popinframe numaxis paramfilename nameaxis vartext coordsys
    global num0 num1 num2 num3 num4 num5 val0 val1 val2 val3 val4 val5
    global oval0 oval1 oval2 oval3 oval4 oval5
    global zerocoordnumber touchoffradius touchofflength touchoffdirection
    set coord [frame $popinframe.offsets ]
    label $coord.l1 -text [msgcat::mc "COORDINATE SYSTEM SETUP \n\n \
        Click value to edit with keyboard.  Press enter to return to keyboard control of machine. \n "]
    # Build the coordinates radio buttons.
    set sel [frame $coord.selectors -borderwidth 2 -relief groove]
    radiobutton $sel.r540 -text G54 -variable coordsys -value 5221 -anchor w \
        -command {findVarNumbers} -takefocus 0
    radiobutton $sel.r550 -text G55 -variable coordsys -value 5241 -anchor w \
        -command {findVarNumbers} -takefocus 0
    radiobutton $sel.r560 -text G56 -variable coordsys -value 5261 -anchor w \
        -command {findVarNumbers} -takefocus 0
    radiobutton $sel.r570 -text G57 -variable coordsys -value 5281 -anchor w \
        -command {findVarNumbers} -takefocus 0
    radiobutton $sel.r580 -text G58 -variable coordsys -value 5301 -anchor w \
        -command {findVarNumbers} -takefocus 0
    radiobutton $sel.r590 -text G59 -variable coordsys -value 5321 -anchor w \
        -command {findVarNumbers} -takefocus 0
    radiobutton $sel.r591 -text G59.1 -variable coordsys -value 5341 -anchor w \
        -command {findVarNumbers} -takefocus 0
    radiobutton $sel.r592 -text G59.2 -variable coordsys -value 5361 -anchor w \
        -command {findVarNumbers} -takefocus 0
    radiobutton $sel.r593 -text G59.3 -variable coordsys -value 5381 -anchor w \
        -command {findVarNumbers} -takefocus 0
    pack $sel.r540 $sel.r550 $sel.r560 $sel.r570 $sel.r580 \
        $sel.r590 $sel.r591 $sel.r592 $sel.r593 -side top -fill x

    # Build the variable numbers and value entry widgets.
    set caxis [frame $coord.col]
    label $caxis.name -text [msgcat::mc "Axis "]
    label $caxis.varval -text [msgcat::mc "Value "]
    grid $caxis.name $caxis.varval -sticky news
    for {set i 0} {$i < $numaxis} {incr i} {
        label  $caxis.l$i -text "[lindex $nameaxis $i ]   "  -anchor e
        entry $caxis.e$i -textvariable val$i -fg darkred -bg white -relief flat -width 10 -takefocus 1
        button $caxis.b$i -text [msgcat::mc "Teach"] -command "getLocation $i" -takefocus 0
        grid $caxis.l$i $caxis.e$i $caxis.b$i -sticky news
        bind $caxis.e$i <KeyPress-Return> {setVarValues ; loadVarFile }
    }
    # Build the control button widgets.
    set cbuttons [frame $coord.buttons]
    button $cbuttons.b0 -textvariable "zerocoordnumber" -width 16 \
        -command {getZero} -takefocus 0
    button $cbuttons.b4 -text [msgcat::mc "Write And Load File "] -width 16 \
        -command {setVarValues ; loadVarFile} -takefocus 0
    label $cbuttons.l0 -text [msgcat::mc "Offset By Radius"]
    label $cbuttons.l1 -text [msgcat::mc "Offset By Length"]
    entry $cbuttons.e0 -textvariable touchoffradius -relief flat -bg white
    entry $cbuttons.e1 -textvariable touchofflength -relief flat -bg white
    bind $cbuttons.e0 <KeyPress-Return> {setTouchOff ; focus -force ".top"}
    bind $cbuttons.e1 <KeyPress-Return> {setTouchOff ; focus -force ".top"}
    radiobutton $cbuttons.r0 -text [msgcat::mc "Subtract"] -variable touchoffdirection -value "-" -anchor w \
        -takefocus 0
    radiobutton $cbuttons.r1 -text [msgcat::mc "Add"] -variable touchoffdirection -value "+" -anchor w \
        -takefocus 0
    grid $cbuttons.l0 $cbuttons.e0  -sticky nsew
    grid $cbuttons.l1 $cbuttons.e1  -sticky nsew
    grid $cbuttons.l1 $cbuttons.e1  -sticky nsew
    grid $cbuttons.r0 $cbuttons.r1  -sticky nsew
    grid $cbuttons.b0 $cbuttons.b4  -sticky nsew

    # grid the coord widgets
    grid configure $coord.l1 -row 0 -column 0 -columnspan 2 -pady 6
    grid configure $sel -row 2 -column 0 -padx 10 -pady 10 -rowspan 2
    grid configure $caxis -row 2 -column 1
    grid configure $cbuttons -row 3 -column 1 -sticky ew

    pack $coord -side top -padx 2 -pady 2 -fill both -expand yes
    findVarNumbers
    isTouchOff
}

proc findVarSystem {} {
    global coordsys zerocoordnumber
    switch -- $coordsys {
        5221 {set zerocoordnumber  [msgcat::mc "Zero All G54"] }
        5241 {set  zerocoordnumber [msgcat::mc "Zero All G55"] }
        5261 {set  zerocoordnumber [msgcat::mc "Zero All G56"] }
        5281 {set  zerocoordnumber [msgcat::mc "Zero All G57"] }
        5301 {set  zerocoordnumber [msgcat::mc "Zero All G58"] }
        5321 {set  zerocoordnumber [msgcat::mc "Zero All G59"] }
        5341 {set  zerocoordnumber [msgcat::mc "Zero All G59.1"] }
        5361 {set  zerocoordnumber [msgcat::mc "Zero All G59.2"] }
        5381 {set  zerocoordnumber [msgcat::mc "Zero All G59.3"] }
        default {set  zerocoordnumber [msgcat::mc "Zero All ????"] }
    }
}

set touchoffradiusvar 5218
set touchofflengthvar 5219

proc isTouchOff {} {
    global vartext touchoffradiusvar touchofflengthvar paramfilename
    global touchoffradius touchofflength
    foreach var "touchoffradius  touchofflength" {
        set locate [$vartext search [set ${var}var ] 1.0 ]
        if {$locate != ""} {
            set locate [expr int($locate)]
            set valtemp [$vartext get $locate.4 "$locate.end"]
            set $var [string trim $valtemp]
        } else {
            set varnumber 0
            set indexer 1
            while {$varnumber < [set ${var}var ] } {
                set varnumber [$vartext get $indexer.0 $indexer.4]
                incr indexer
            }
            $vartext mark set insert [expr $indexer -1].0
            $vartext insert insert "[set ${var}var ] [set $var] \n"
        }
    }
    saveFile $vartext $paramfilename
}

proc setTouchOff {} {
    global vartext paramfilename touchoffradiusvar touchofflengthvar
    global touchoffradius touchofflength
    # set radius
    set locate [$vartext search $touchoffradiusvar 1.0 ]
    $vartext mark set insert $locate
    set locate [expr int($locate)]
    $vartext delete $locate.0 "$locate.end"
    $vartext insert insert "$touchoffradiusvar     $touchoffradius"
    #set length
    set locate [$vartext search $touchofflengthvar 1.0 ]
    $vartext mark set insert $locate
    set locate [expr int($locate)]
    $vartext delete $locate.0 "$locate.end"
    $vartext insert insert "$touchofflengthvar     $touchofflength"
    saveFile $vartext $paramfilename
    loadVarFile
}

set coordsys 5241
proc findVarNumbers {} {
    global coordsys numaxis num0 num1 num2 num3 num4 num5
    # set the initial coordinate system and find values.
    set numaxis [emc_ini "AXES" "TRAJ"]
    set nameaxis [emc_ini "COORDINATES" "TRAJ"]
    for {set i 0} {$i < $numaxis} {incr i} {
        set num$i [expr $coordsys +$i]
        set val$i 0.000000
    }
    findVarValues
    findVarSystem
}

proc findVarValues {}  {
    global vartext
    global numaxis val0 val1 val2 val3 val4 val5
    global num0 num1 num2 num3 num4 num5
    global numaxis oval0 oval1 oval2 oval3 oval4 oval5
    for {set i 0} {$i < $numaxis} {incr i} {
        set val$i 0.000000
    }
    set locate "1.0"
    for {set i 0} {$i < $numaxis} {incr i}  {
        set oval$i [set val$i]
        set locate [$vartext search [set num$i] 1.0 ]
        set locate [expr int($locate)]
        set valtemp [$vartext get $locate.4 "$locate.end"]
        set val$i [string trim $valtemp]
    }
}

proc getZero {} {
    global numaxis val0 val1 val2 val3 val4 val5
    for {set i 0} {$i < $numaxis} {incr i} {
        set val$i 0.000000
    }
}

proc getLocation {axnum} {
    global numaxis val0 val1 val2 val3 val4 val5
    global zerocoordnumber touchoffradius touchofflength touchoffdirection
    switch -- $axnum {
        "0" -
        "1" {
            set val$axnum [expr [emc_abs_act_pos $axnum] $touchoffdirection $touchoffradius ]
        }
        "2" {
            set val$axnum [expr [emc_abs_act_pos $axnum] $touchoffdirection $touchofflength ]
        }
        all {
            for {set i 0} {$i < $numaxis} {incr i} {
            set val$i [emc_abs_act_pos $i]
            }
        }
        "default" {
            set val$axnum [emc_abs_act_pos $axnum]
        }
    }
}

# ----------end of OFFSETS SYSTEM SETUP----------

update

# ----editor widgets -----
set saveTextMsg 0
set editFilename ""
set initialDir $programDirectory
set MODIFIED [msgcat::mc "Modified..."]

set winTitle "mini"
set version "Version 0.7.9"

proc popinEditor {} {
    global editFilename textwin programnamestring
    global editwidth editheight popinframe undo_id

    if { ![info exists editwidth] } {set editwidth 80}
    if { ![info exists editheight] } {set editheight 40}
    set editframe [frame $popinframe.editor ]
    pack $editframe -side top -fill both -expand yes
    set textframe [frame $editframe.textframe ]
    set textwin [text  $textframe.textwin  -width $editwidth -height $editheight -padx 4 -wrap word \
         -yscrollcommand "editScrolltext $textframe" -bg "white"]
    set scrolly $textframe.scrolly
    scrollbar $scrolly -orient vert -command "$textwin yview" -width 8
    pack $scrolly -side right -fill y
    pack $textwin -side top -fill both -expand true

    set menubar [frame $editframe.menuframe -relief raised -bd 2]
    pack $menubar -side top -fill x -expand yes
    menubutton $menubar.file -text [msgcat::mc "File"] -menu $menubar.file.menu
    menubutton $menubar.edit -text [msgcat::mc "Edit"] -menu $menubar.edit.menu
    menubutton $menubar.settings -text [msgcat::mc "Settings"] -menu $menubar.settings.menu
    menubutton $menubar.help -text [msgcat::mc "Help"] -menu $menubar.help.menu
    pack $menubar.file -side left
    pack $menubar.edit -side left
    pack $menubar.settings -side left
    pack $menubar.help -side right
    set filemenu $menubar.file.menu
    set editmenu $menubar.edit.menu
    set settingsmenu $menubar.settings.menu
    set helpmenu $menubar.help.menu
    menu $filemenu
    menu $editmenu
    menu $settingsmenu
    menu $helpmenu

    $filemenu add command -label [msgcat::mc "New"] -underline 0 -command "filesetasnew" -accelerator Ctrl+n
    $filemenu add command -label [msgcat::mc "Open..."] -underline 0 -command "filetoopen" -accelerator Ctrl+o
    $filemenu add command -label [msgcat::mc "Save"] -underline 0 -command "filetosave" -accelerator Ctrl+s
    $filemenu add command -label [msgcat::mc "Save As..."] -underline 5 -command "filesaveas"
    $filemenu add separator
    $filemenu add command -label [msgcat::mc "Save and Load"] -command "filetosave ; changeProgram" -underline 1

    $editmenu add command -label [msgcat::mc "Undo"] -underline 0 -command "undo_menu_proc" -accelerator Ctrl+z
    $editmenu add command -label [msgcat::mc "Redo"] -underline 0 -command "redo_menu_proc" -accelerator Ctrl+y
    $editmenu add separator
    $editmenu add command -label [msgcat::mc "Cut"] -underline 2 -command "cuttext" -accelerator "Ctrl+X"
    $editmenu add command -label [msgcat::mc "Copy"] -underline 0 -command "copytext" -accelerator "Ctrl+C"
    $editmenu add command -label [msgcat::mc "Paste"] -underline 0 -command "pastetext" -accelerator "Ctrl+V"
    $editmenu add command -label [msgcat::mc "Delete"] -underline 0 -command "deletetext" -accelerator Del
    $editmenu add separator
    $editmenu add command -label [msgcat::mc "Select All"] -underline 7 -command "$textwin tag add sel 1.0 end" -accelerator "Ctrl+A"
    $editmenu add separator
    $editmenu add command -label [msgcat::mc "Find..."] -underline 0 -command "findtext find" -accelerator Ctrl+f
    $editmenu add command -label [msgcat::mc "Replace..."] -underline 0 -command "findtext replace" -accelerator Ctrl+r
    $editmenu add command -label [msgcat::mc "Renumber File..."] -underline 0 -command "editSetLineNumber 1"

    $settingsmenu add command -label [msgcat::mc "No Numbering"] -underline 0 -command "set startnumbering 0"
    $settingsmenu add separator
    $settingsmenu add command -label [msgcat::mc "Line Numbering..."] -underline 0 -command "editSetLineNumber 0"

    $helpmenu add command -label [msgcat::mc "Help..."] -underline 0 -command "helpme"
    $helpmenu add command -label [msgcat::mc "About..."] -underline 0 -command "aboutme"

    bind $textwin <Control-x> {cuttext}
    bind $textwin <Control-c> {copytext}
    bind $textwin <Control-s> {filetosave}
    bind Text <Control-o> {}
    bind Text <Control-f> {}
    bind $textwin <Control-o> {filetoopen}
    bind $textwin <Control-z> {undo_menu_proc}
    bind $textwin <Control-y> {redo_menu_proc}
    bind $textwin <Control-f> {findtext find}
    bind $textwin <Control-r> {findtext replace}
    event delete <<Cut>> <Control-x>
    event delete <<Paste>> <Control-v>
    event delete <<Paste>> <Control-Key-y>
    # more bindings
    bind Text <Control-v> {}
    bind $textwin <Control-v> {pastetext}


    pack $textframe -side top -fill both -expand yes
    # insert contents of filename, if it exists
    if { [file isfile $programnamestring] == 1} {
        set editFilename $programnamestring
    }
    if {$editFilename == "" } {
        set $editFilename new
        loadEditorText
    } else {
        loadEditorText
    }
    set undo_id [new textUndoer $textwin]
}

proc editScrolltext {tf a b} {
    $tf.scrolly set $a $b
}

proc loadEditorText { } {
    global textwin editFilename
    if {$editFilename == "new" } {
        editOpenFile
    } else {
    set fname $editFilename
    }
    if { [file isfile $fname] == 0} {
        return
    }
    if { [catch {open $fname} filein] } {
        puts stdout "can't open $fname"
    } else {
        $textwin delete 1.0 end
        $textwin insert end [read $filein]
        catch {close $filein}
    }
}

# Any positive integer can be used for lineincrement.
# A 0 startnumbering value means lines will not be numbered when enter is pressed.
set startnumbering 0

# Space refers to the distance between n words and other text. Tab space is set
# here but could be single or double space.  Change what's between the "".
set space "     "

# Number refers to the start up value of line numbering.
set number 0
set lineincrement 10

proc editLineIncrement {} {
    global startnumbering number lineincrement space textwin
    if {$startnumbering != 0} {
        $textwin insert insert "n$number$space"
        incr number $lineincrement
    }
}

# editSetLineNumber uses a hard coded popup location from top right.
proc editSetLineNumber {what} {
    global  startnumbering number lineincrement textwin linenum
    toplevel .linenumber
    wm title .linenumber [msgcat::mc "Set Line Numbering"]
    wm geometry .linenumber 275x180-60+100
    set linenum [frame .linenumber.frame]
    pack $linenum -side top -fill both -expand yes
    label $linenum.label1 -text [msgcat::mc "Increment"]
    place $linenum.label1 -x 5 -y 5
    radiobutton $linenum.incr1 -text [msgcat::mc "One"] -variable lineincrement -value 1 -anchor w
    place $linenum.incr1 -x 10 -y 25 -width 80 -height 20
    radiobutton $linenum.incr2 -text [msgcat::mc "Two"] -variable lineincrement -value 2 -anchor w
    place $linenum.incr2 -x 10 -y 45 -width 80 -height 20
    radiobutton $linenum.incr5 -text [msgcat::mc "Five"] -variable lineincrement -value 5 -anchor w
    place $linenum.incr5 -x 10 -y 65 -width 80 -height 20
    radiobutton $linenum.incr10 -text [msgcat::mc "Ten"] -variable lineincrement -value 10 -anchor w
    place $linenum.incr10 -x 10 -y 85 -width 80 -height 20
    label $linenum.label2 -text [msgcat::mc "Space"]
    place $linenum.label2 -x 130 -y 5
    radiobutton $linenum.space1 -text [msgcat::mc "Single Space"] -variable space -value { } -anchor w
    place $linenum.space1 -x 140 -y 25
    radiobutton $linenum.space2 -text [msgcat::mc "Double Space"] -variable space -value {  } -anchor w
    place $linenum.space2 -x 140 -y 45
    radiobutton $linenum.space3 -text [msgcat::mc "Tab Space"] -variable space -value {    } -anchor w
    place $linenum.space3 -x 140 -y 65
    button $linenum.ok -text OK -command {destroy .linenumber} -height 1 -width 9
    place $linenum.ok -x 160 -y 127
    label $linenum.label3 -text [msgcat::mc "Next Number: "] -anchor e
    place $linenum.label3 -x 5 -y 130 -width 95
    entry $linenum.entry -width 6 -textvariable number
    place $linenum.entry -x 100 -y 130
    button $linenum.renum -text [msgcat::mc "Renumber"] -command editReNumber -height 1 -width 9 -state disabled
    if {$what} {
        $linenum.renum configure -state normal
    }
    place $linenum.renum -x 160 -y 96
    set temp [expr $number - $lineincrement]
    if {$temp > 0} {
        set number $temp
    } else {
        set number 0
    }
    set startnumbering 1
    focus -force ".linenumber"
    bind ".linenumber" <FocusOut> {destroy ".linenumber"}
}

# String match with a while loop [0-9 tab space] 1 if true 0 if no match
proc editReNumber {} {
    global textwin number lineincrement space linenum
    scan [$textwin index end] %d nl
    for {set i 1} {$i < $nl} {incr i} {
        if {$number > 99999} {set number 0}
        set editline [$textwin get $i.0 $i.end ]
        set editline [string trimleft $editline " "]
        if { ! [ regexp ^% $editline ] } {
            if { [ regexp -nocase {^[nN](\d*)(\s*)} $editline ] } {
                regsub -nocase {^[nN](\d*)(\s*)} $editline "n$number$space" editline
            } else {
                set editline "n$number$space$editline"
            }
        }
        $textwin delete $i.0 $i.end
        $textwin insert $i.0 "$editline"

        incr number $lineincrement
    }
    set startnumbering 0
}

###############################################################################
# 
# Tk NotePad is designed to be a single Tcl/Tk script, that is functional cross 
# platform, but is intended mainly for Linux.
# 
# This script is freeware, however there is some 'borrowed code' now contained in 
# this script. See the file license.txt to see what that means. Basically I 
# modified their code and am now redistributing it, and giving them proper credit.
# As I understand it that is they way it works. This script itself then becomes 
# yours to modify, crop, cut, paste, or whatever. It is distributed under the 
# Tcl/Tk liscense, the licesnse.txt file, and I guess that makes it LGPL? I'm not
# a lawyer, so don't ask me!
# 
# NOTE: It works on Windows, but BETTER on Linux!
# 
# 	Joseph Acosta joeja@mindspring.com
# 
###############################################################################

# this proc just sets the title to what it is passed
proc settitle {WinTitleName} {
    global winTitle editFilename
    wm title . "$winTitle - $WinTitleName"
    set editFilename $WinTitleName
}

# proc to open files or read a pipe
proc openoninit {thefile} {
    global textwin
    $textwin delete 0.0 end
    if [string match " " $thefile] {  
        fconfigure stdin -blocking 0
        set incoming [read stdin 1]
        if [expr [string length $incoming] == 0] {
            fconfigure stdin -blocking 1
        } else {
            fconfigure stdin -blocking 1
            $textwin insert end $incoming
            while {![eof stdin]} {
                $textwin insert end [read -nonewline stdin]
            }
        }
    } else {
        if [ file exists $thefile ] {
            set newnamefile [open $thefile r]
        } else {
            set newnamefile [open $thefile a+]
        }
        while {![eof $newnamefile]} {
	       $textwin insert end [read -nonewline $newnamefile ] 
        }
        close $newnamefile
        settitle $thefile
    }
}

# help menu
proc helpme {} {
	tk_messageBox -title [msgcat::mc "Basic Help"] -type ok -message [msgcat::mc "This is a simple ASCII editor like many others.

Ctrl+O  Open
Ctrl+S  Save
Ctrl+Z  Undo
Ctrl+Y  Redo
Ctrl+X  Cut
Ctrl+C  Copy
Ctrl+V  Paste
Del     Delete
Ctrl+A  Select All

Ctrl+F  Find
Ctrl+R  Replace "]

}

# about menu
proc aboutme {} {
        global winTitle version
	tk_messageBox -title [msgcat::mc "About"] -type ok -message [msgcat::mc "tknotepad by Joseph Acosta. <joeja@mindspring.com>\n\n\
        Modified for EMC by: Paul Corner <paul_c@users.sourceforge.net>"]
}

# generic case switcher for message box
proc switchcase {yesfn nofn} {
    global saveTextMsg
    if [ expr [string compare $saveTextMsg 1] ==0 ] { 
	set answer [tk_messageBox -message [msgcat::mc "The contents of this file may have changed, do you wish to to save your changes?"] \
	-title [msgcat::mc "New Confirm?"] -type yesnocancel -icon question]
	case $answer {
	     yes { if {[eval $yesfn] == 1} { $nofn } }
             no {$nofn }
	}
    } else {
   	$nofn
    }
}

# new file
proc filesetasnew {} {
    global editFilename winTitle
    switchcase filetosave setTextTitleAsNew
}

proc setTextTitleAsNew {} {
    global textwin
    $textwin delete 0.0 end
    global winTitle editFilename
    set editFilename " "
    wm title . $winTitle
    outccount
}

# bring up open win
proc showopenwin {} {
    global programDirectory

    set allfilestring [msgcat::mc "All files"]
    set textfilestring [msgcat::mc "Text files"]
    set ncfilestring [msgcat::mc "NC files"]

    set all [list ]
    lappend all $allfilestring
    lappend all *
    set text [list ]
    lappend text $textfilestring
    lappend text ".txt"
    set nc [list ]
    lappend nc $ncfilestring
    lappend nc ".nc .ngc"

    set types [list ]
    lappend types $all
    lappend types $text
    lappend types $nc

    set file [tk_getOpenFile -filetypes $types -parent . -initialdir $programDirectory]
###if [string compare $file ""]
    if { [string len $file] > 0} {
        set programDirectory [file dirname $file]
        setTextTitleAsNew
        openoninit $file
        outccount
    }
}

#open an existing file
proc filetoopen {} {
  	switchcase filetosave showopenwin
}

# generic save function
proc writesave {nametosave} {
    global textwin
    set FileNameToSave [open $nametosave w+]
    puts -nonewline $FileNameToSave [$textwin get 0.0 end]
    close $FileNameToSave
    outccount
}

#save a file
proc filetosave {} {
    global editFilename
    #check if file exists file
    if [file exists $editFilename] {
	writesave $editFilename
        return 1
    } else {
	 return [eval filesaveas]
    }
}

#save a file as
proc filesaveas {} {
    global editFilename

    set allfilestring [msgcat::mc "All files"]
    set textfilestring [msgcat::mc "Text files"]
    set ncfilestring [msgcat::mc "NC files"]

    set all [list ]
    lappend all $allfilestring
    lappend all *
    set text [list ]
    lappend text $textfilestring
    lappend text ".txt"
    set nc [list ]
    lappend nc $ncfilestring
    lappend nc ".nc .ngc"

    set types [list ]
    lappend types $all
    lappend types $text
    lappend types $nc

    set myfile [tk_getSaveFile -filetypes $types -parent .  -initialdir "~/gcode" -initialfile $editFilename]
    if { [expr [string compare $myfile ""]] != 0} {
	writesave  $myfile 
	settitle $myfile
        return 1
    }
    return 0
}

# proc to set child window position
proc setwingeom {wintoset} {
    wm resizable $wintoset 0 0
    set myx [expr (([winfo screenwidth .]/2) - ([winfo reqwidth $wintoset]))]
    set myy [expr (([winfo screenheight .]/2) - ([winfo reqheight $wintoset]/2))]
    wm geometry $wintoset +$myx+$myy
    set topwin [ winfo parent $wintoset ]
    if { [ winfo viewable [ winfo toplevel $topwin ] ] } {
        wm transient $wintoset $topwin
    }
}

# procedure to setup the printer
proc printseupselection {} {
	global printCommand
	set print .print
	catch {destroy $print}
	toplevel $print
	wm title $print [msgcat::mc "Print Setup"]
	setwingeom $print
	frame $print.top 
	frame $print.bottom
	label $print.top.label -text [msgcat::mc "Print Command: "]
	entry $print.top.print -textvariable printsetupnew -width 40
	$print.top.print delete 0 end
	set printvar $printCommand 
	$print.top.print insert 0 $printvar
	button $print.bottom.ok -text [msgcat::mc "OK"] -command "addtoprint $print"
	button $print.bottom.cancel -text [msgcat::mc "Cancel"] -command "destroy $print"

	pack $print.top -side top -expand 0 
	pack $print.bottom -side bottom -expand 0 
	pack $print.top.label $print.top.print -in $print.top -side left -fill x -fill y
	pack $print.bottom.ok $print.bottom.cancel -in $print.bottom -side left -fill x -fill y
	bind $print <Return> "addtoprint $print"
	bind $print <Escape> "destroy $print"

    proc addtoprint {prnt} {
         global printCommand
         set printCommand [$prnt.top.print get]
         destroy $prnt
    }
}

# procedure to print
proc selectprint {} {
    global textwin
    set TempPrintFile [open /tmp/tkpadtmpfile w]
    puts -nonewline $TempPrintFile [$textwin get 0.0 end]
    close $TempPrintFile
    global printCommand
    set prncmd $printCommand	
    eval exec $prncmd /tmp/tkpadtmpfile
    eval exec rm -f /tmp/tkpadtmpfile
}

#cut text procedure
proc deletetext {} {
    set cuttexts [selection own]
    if {$cuttexts != "" } {
        $cuttexts delete sel.first sel.last
        selection clear
    }
    inccount
}

#cut text procedure
proc cuttext {} {
    global textwin
    tk_textCut $textwin
    inccount
}

#copy text procedure
proc copytext {} {
    global textwin
    tk_textCopy $textwin
    inccount
}

#paste text procedure
proc pastetext {} {
    global textwin
    global tcl_platform
    if {"$tcl_platform(platform)" == "unix"} {
	catch {$textwin delete sel.first sel.last}
    }
    tk_textPaste $textwin
    inccount
}

proc FindIt {w} {
    global textwin
    global SearchString SearchPos SearchDir findcase 
    $textwin tag configure sel -background green
    if {$SearchString!=""} {
    	if {$findcase=="1"} {
            set caset "-exact"
	} else {
	    set caset "-nocase"
	}
	if {$SearchDir == "forwards"} {
	    set limit end
	} else {
	    set limit 1.0
	}
	set SearchPos [ $textwin search -count len $caset -$SearchDir $SearchString $SearchPos $limit]
	set len [string length $SearchString]
	if {$SearchPos != ""} {
            $textwin see $SearchPos
	    tk::TextSetCursor $textwin $SearchPos
	    $textwin tag add sel $SearchPos  "$SearchPos + $len char"

	    if {$SearchDir == "forwards"} {
                set SearchPos "$SearchPos + $len char"
	    }         
        } else {
	    set SearchPos "0.0"
	}
    }
    focus $textwin
}

proc ReplaceIt {} {
    global SearchString SearchDir ReplaceString SearchPos findcase
    global textwin
	if {$SearchString != ""} {
	    if {$findcase=="1"} {
		set caset "-exact"
	    } else {
		set caset "-nocase"
	    }
	    if {$SearchDir == "forwards"} {
		set limit end
	    } else {
		set limit 1.0
	    }
	    set SearchPos [ $textwin search -count len $caset -$SearchDir $SearchString $SearchPos $limit]
		set len [string length $SearchString]
	    if {$SearchPos != ""} {
        		$textwin see $SearchPos
               		$textwin delete $SearchPos "$SearchPos+$len char"
        		$textwin insert $SearchPos $ReplaceString
		if {$SearchDir == "forwards"} {
        			set SearchPos "$SearchPos+$len char"
		}         
	    } else {
	       	set SearchPos "0.0"
	    }
	}
	inccount
}

proc ReplaceAll {} {
      global SearchPos SearchString
       if {$SearchString != ""} {
                ReplaceIt
	while {$SearchPos!="0.0"} {
		ReplaceIt
	}
       }
}

proc CancelFind {w} {
    global textwin
    $textwin tag delete tg1
    destroy $w
}

proc ResetFind {} {
    global SearchPos
    set SearchPos insert
}

# procedure to find text
proc findtext {typ} {
	global SearchString SearchDir ReplaceString findcase c find
	set find .find
	catch {destroy $find}
	toplevel $find
	wm title $find [msgcat::mc "Find"]
	setwingeom $find
	ResetFind
	frame $find.l
	frame $find.l.f1
	label $find.l.f1.label -text [msgcat::mc "Find what:"] -width 11  
	entry $find.l.f1.entry  -textvariable SearchString -width 30 
	pack $find.l.f1.label $find.l.f1.entry -side left
	$find.l.f1.entry selection range 0 end
	if {$typ=="replace"} {
		frame $find.l.f2
		label $find.l.f2.label2 -text [msgcat::mc "Replace with:"] -width 11
		entry $find.l.f2.entry2  -textvariable ReplaceString -width 30 
		pack $find.l.f2.label2 $find.l.f2.entry2 -side left
		pack $find.l.f1 $find.l.f2 -side top
	} else {
		pack $find.l.f1
	}
	frame $find.f2
	button $find.f2.button1 -text [msgcat::mc "Find Next"] -command "FindIt $find" -width 10 -height 1 -underline 5 
	button $find.f2.button2 -text [msgcat::mc "Cancel"] -command "CancelFind $find" -width 10 -underline 0
	if {$typ=="replace"} {
		button $find.f2.button3 -text [msgcat::mc "Replace"] -command ReplaceIt -width 10 -height 1 -underline 0
		button $find.f2.button4 -text [msgcat::mc "Replace All"] -command ReplaceAll -width 10 -height 1 -underline 8		
		pack $find.f2.button3 $find.f2.button4 $find.f2.button2  -pady 4
	} else {
		pack $find.f2.button1 $find.f2.button2  -pady 4
	}
	frame $find.l.f4
	frame $find.l.f4.f3 -borderwidth 2 -relief groove
	radiobutton $find.l.f4.f3.up -text [msgcat::mc "Up"] -underline 0 -variable SearchDir -value "backwards" 
	radiobutton $find.l.f4.f3.down -text [msgcat::mc "Down"]  -underline 0 -variable SearchDir -value "forwards" 
	$find.l.f4.f3.down invoke
	pack $find.l.f4.f3.up $find.l.f4.f3.down -side left 
	checkbutton $find.l.f4.cbox1 -text [msgcat::mc "Match case"] -variable findcase -underline 0 
	pack $find.l.f4.cbox1 $find.l.f4.f3 -side left -padx 10
	pack $find.l.f4 -pady 11
	pack $find.l $find.f2 -side left -padx 1
	bind $find <Escape> "destroy $find"

     # each widget must be bound to the events of the other widgets
     proc bindevnt {widgetnm types find} {
	if {$types=="replace"} {
		bind $widgetnm <Return> "ReplaceIt"
		bind $widgetnm <Control-r> "ReplaceIt"
		bind $widgetnm <Control-a> "ReplaceAll"
	} else {
		bind $widgetnm <Return> "FindIt $find"
		bind $widgetnm <Control-n> "FindIt $find"
	}
	bind $widgetnm <Control-m> { $find.l.f4.cbox1 invoke }
	bind $widgetnm <Control-u> { $find.l.f4.f3.up invoke }
	bind $widgetnm <Control-d> { $find.l.f4.f3.down invoke }
     }
	if {$typ == "replace"} {
   		bindevnt $find.f2.button3 $typ $find
		bindevnt $find.f2.button4 $typ $find
	} else {
		bindevnt $find.f2.button1 $typ $find
  	        bindevnt $find.f2.button2 $typ $find
	}
        bindevnt $find.l.f4.f3.up  $typ $find
        bindevnt $find.l.f4.f3.down $typ $find
        bindevnt $find.l.f4.cbox1 $typ $find
	bindevnt $find.l.f1.entry $typ $find	
	bind $find <Control-c> "destroy $find"
	focus $find.l.f1.entry
	grab $find
}

# proc for find next
proc findnext {typof} {
	global SearchString SearchDir ReplaceString findcase c find
	if [catch {expr [string compare $SearchString "" ] }] {
		findtext $typof
	} else {
	 	FindIt $find
	}
}

#procedure to set the time change %R to %I:%M for 12 hour time display
proc printtime {} {
    global textwin
    $textwin insert insert [clock format [clock seconds] -format "%R %p %D"]
    inccount
}

# binding for wordwrap
proc wraptext {} {
    global wordWrap
    if [expr [string compare $wordWrap word] == 0] {
	set wordWrap none	
    } else {
	set wordWrap word
    }
    $textwin configure -wrap $wordWrap
}

## NOTE modifiedstatus is declared in the linenum.pth 
## so if it it not included we dont want to throw the error
## we just want to ignore, thus the catch...
# this sets saveTextMsg to 1 for message boxes
proc inccount {} {
    global saveTextMsg MODIFIED
    set saveTextMsg 1
    catch {modifiedstatus $MODIFIED}
}
# this resets saveTextMsg to 0
proc outccount {} {
    global saveTextMsg 
    set saveTextMsg 0
    catch {modifiedstatus " "}
}

#set zed_dir [file dirname [info script]] 
# here is where the undo stuff begins
if {![info exists classNewId]} {
    # work around object creation between multiple include of this file problem
    set classNewId 0
}

proc new {className args} {
    # calls the constructor for the class with optional arguments
    # and returns a unique object identifier independent of the class name

    global classNewId
    # use local variable for id for new can be called recursively
    set id [incr classNewId]
    if {[llength [info procs ${className}:$className]]>0} {
        # avoid catch to track errors
        eval ${className}:$className $id $args
    }
    return $id
}

proc delete {className id} {
    # calls the destructor for the class and delete all the object data members

    if {[llength [info procs ${className}:~$className]]>0} {
        # avoid catch to track errors
        ${className}:~$className $id
    }
    global $className
    # and delete all this object array members if any (assume that they were stored as $className($id,memberName))
    foreach name [array names $className "$id,*"] {
        unset ${className}($name)
    }
}

proc lifo:lifo {id {size 2147483647}} {
    global lifo
    set lifo($id,maximumSize) $size
    lifo:empty $id
}

proc lifo:push {id data} {
    global lifo 
    inccount
    lifo:tidyUp $id
    if {$lifo($id,size)>=$lifo($id,maximumSize)} {
        unset lifo($id,data,$lifo($id,first))
        incr lifo($id,first)
        incr lifo($id,size) -1
    }
    set lifo($id,data,[incr lifo($id,last)]) $data
    incr lifo($id,size)
}

proc lifo:pop {id} {
    global lifo 
    inccount
    lifo:tidyUp $id
    if {$lifo($id,last)<$lifo($id,first)} {
        error "lifo($id) pop error, empty"
    }
    # delay unsetting popped data to improve performance by avoiding a data copy
    set lifo($id,unset) $lifo($id,last)
    incr lifo($id,last) -1
    incr lifo($id,size) -1
    return $lifo($id,data,$lifo($id,unset))
}

proc lifo:tidyUp {id} {
    global lifo
    if {[info exists lifo($id,unset)]} {
        unset lifo($id,data,$lifo($id,unset))
        unset lifo($id,unset)
    }
}

proc lifo:empty {id} {
    global lifo
    lifo:tidyUp $id
    foreach name [array names lifo $id,data,*] {
        unset lifo($name)
    }
    set lifo($id,size) 0
    set lifo($id,first) 0
    set lifo($id,last) -1
}

proc textUndoer:textUndoer {id widget {depth 2147483647}} {
    global textUndoer

    if {[string compare [winfo class $widget] Text]!=0} {
        error "textUndoer error: widget $widget is not a text widget"
    }
    set textUndoer($id,widget) $widget
    set textUndoer($id,originalBindingTags) [bindtags $widget]
    bindtags $widget [concat $textUndoer($id,originalBindingTags) UndoBindings($id)]

    bind UndoBindings($id) <Control-u> "textUndoer:undo $id"

    # self destruct automatically when text widget is gone
    bind UndoBindings($id) <Destroy> "delete textUndoer $id"
    # rename widget command
    rename $widget [set textUndoer($id,originalCommand) textUndoer:original$widget]
    # and intercept modifying instructions before calling original command
    proc $widget {args} "textUndoer:checkpoint $id \$args; 
		global search_count;
		eval $textUndoer($id,originalCommand) \$args"

    set textUndoer($id,commandStack) [new lifo $depth]
    set textUndoer($id,cursorStack) [new lifo $depth]
    #lee 
    textRedoer:textRedoer $id $widget $depth 
}

proc textUndoer:~textUndoer {id} {
    global textUndoer

    bindtags $textUndoer($id,widget) $textUndoer($id,originalBindingTags)
    rename $textUndoer($id,widget) ""
    catch { rename $textUndoer($id,originalCommand) $textUndoer($id,widget) }
    delete lifo $textUndoer($id,commandStack)
    delete lifo $textUndoer($id,cursorStack)
    #lee
    textRedoer:~textRedoer $id
}

proc textUndoer:checkpoint {id arguments} {
    global textUndoer textRedoer

    # do nothing if non modifying command
    if {[string compare [lindex $arguments 0] insert]==0} {
        textUndoer:processInsertion $id [lrange $arguments 1 end]
        if {$textRedoer($id,redo) == 0} {
           textRedoer:reset $id
        }
    }
    if {[string compare [lindex $arguments 0] delete]==0} {
        textUndoer:processDeletion $id [lrange $arguments 1 end]
        if {$textRedoer($id,redo) == 0} {
           textRedoer:reset $id
        }
    }
}

proc textUndoer:processInsertion {id arguments} {
    global textUndoer

    set number [llength $arguments]
    set length 0
    # calculate total insertion length while skipping tags in arguments
    for {set index 1} {$index<$number} {incr index 2} {
        incr length [string length [lindex $arguments $index]]
    }
    if {$length>0} {
        set index [$textUndoer($id,originalCommand) index [lindex $arguments 0]]
        lifo:push $textUndoer($id,commandStack) "delete $index $index+${length}c"
        lifo:push $textUndoer($id,cursorStack) [$textUndoer($id,originalCommand) index insert]
    }
}

proc textUndoer:processDeletion {id arguments} {
    global textUndoer

    set command $textUndoer($id,originalCommand)
    lifo:push $textUndoer($id,cursorStack) [$command index insert]

    set start [$command index [lindex $arguments 0]]
    if {[llength $arguments]>1} {
        lifo:push $textUndoer($id,commandStack) "insert $start [list [$command get $start [lindex $arguments 1]]]"
    } else {
        lifo:push $textUndoer($id,commandStack) "insert $start [list [$command get $start]]"
    }
}

proc textUndoer:undo {id} {
    global textUndoer

puts "$textUndoer($id,commandStack)"

    if {[catch {set cursor [lifo:pop $textUndoer($id,cursorStack)]}]} {
        return
    }
    if {[catch {set popArgs [lifo:pop $textUndoer($id,commandStack)]}]} {
        return
    }
    textRedoer:checkpoint $id $popArgs
    
    eval $textUndoer($id,originalCommand) $popArgs
    # now restore cursor position
    $textUndoer($id,originalCommand) mark set insert $cursor
    # make sure insertion point can be seen
    $textUndoer($id,originalCommand) see insert
}

proc textUndoer:reset {id} {
    global textUndoer
    lifo:empty $textUndoer($id,commandStack)
    lifo:empty $textUndoer($id,cursorStack)
}

proc textRedoer:textRedoer {id widget {depth 2147483647}} {
    global textRedoer
    if {[string compare [winfo class $widget] Text]!=0} {
        error "textRedoer error: widget $widget is not a text widget"
    }
    set textRedoer($id,commandStack) [new lifo $depth]
    set textRedoer($id,cursorStack) [new lifo $depth]
    set textRedoer($id,redo) 0
}

proc textRedoer:~textRedoer {id} {
    global textRedoer
    delete lifo $textRedoer($id,commandStack)
    delete lifo $textRedoer($id,cursorStack)
}

proc textRedoer:checkpoint {id arguments} {
    global textUndoer textRedoer
    # do nothing if non modifying command
    if {[string compare [lindex $arguments 0] insert]==0} {
        textRedoer:processInsertion $id [lrange $arguments 1 end]
    }
    if {[string compare [lindex $arguments 0] delete]==0} {
        textRedoer:processDeletion $id [lrange $arguments 1 end]
    }
}

proc textRedoer:processInsertion {id arguments} {
    global textUndoer textRedoer
    set number [llength $arguments]
    set length 0
    # calculate total insertion length while skipping tags in arguments
    for {set index 1} {$index<$number} {incr index 2} {
        incr length [string length [lindex $arguments $index]]
    }
    if {$length>0} {
        set index [$textUndoer($id,originalCommand) index [lindex $arguments 0]]
        lifo:push $textRedoer($id,commandStack) "delete $index $index+${length}c"
        lifo:push $textRedoer($id,cursorStack) [$textUndoer($id,originalCommand) index insert]
    }
}

proc textRedoer:processDeletion {id arguments} {
    global textUndoer textRedoer
    set command $textUndoer($id,originalCommand)
    lifo:push $textRedoer($id,cursorStack) [$command index insert]

    set start [$command index [lindex $arguments 0]]
    if {[llength $arguments]>1} {
        lifo:push $textRedoer($id,commandStack) "insert $start [list [$command get $start [lindex $arguments 1]]]"
    } else {
        lifo:push $textRedoer($id,commandStack) "insert $start [list [$command get $start]]"
    }
}

proc textRedoer:redo {id} {
    global textUndoer textRedoer
    if {[catch {set cursor [lifo:pop $textRedoer($id,cursorStack)]}]} {
        return
    }
    set textRedoer($id,redo) 1
    set popArgs [lifo:pop $textRedoer($id,commandStack)]     
    textUndoer:checkpoint $id $popArgs
    eval $textUndoer($id,originalCommand) $popArgs
    set textRedoer($id,redo) 0
    # now restore cursor position
    $textUndoer($id,originalCommand) mark set insert $cursor
    # make sure insertion point can be seen
    $textUndoer($id,originalCommand) see insert
}

proc textRedoer:reset {id} {
    global textRedoer
    lifo:empty $textRedoer($id,commandStack)
    lifo:empty $textRedoer($id,cursorStack)
}

# end of where you'd source in undo.tcl
#set undo_id [new textUndoer $textwin]

proc undo_menu_proc {} {
	global undo_id
	textUndoer:undo $undo_id
	inccount
}

proc redo_menu_proc {} {
	global undo_id
	textRedoer:redo $undo_id
	inccount
}

# -----end editor ------

update

# ----------RIGHT - BACKPLOT----------

# These set the axis directions.  +1 one plots like graph
# paper with positive up and to the right.
# to reverse an axis change its value to -1 (don't put other numbers here)
set xdir 1
set ydir -1
set zdir -1
set adir 1
set bdir 1
set cdir 1

# These set top left and bottom right of the canvas
global topx topy bot x boty
set topx -1000
set botx  1000
set topy -500
set boty 500

# This variable sets the default size of the plot and can be thought of as number of
# pixels per inch of machine motion.  The default (40) will give about .55 screen
# inch per inch moved.

set size 40

# Set the colours for different modes here
set plotcolour {limegreen black red blue yellow3 white}

# Evaluate the number of clock clicks per second
# Value dependent on machine !!
set t1 [clock clicks]
after 1000
set t2 [clock clicks]
set ticks [expr ($t2-$t1)/1000]

# procedure to show window .
proc popinPlot {} {
    global  plot popinframe plotSetup plotsetuptext
    set plot [frame $popinframe.plot]
    pack $plot -fill both -expand yes

  # build widget $plot.view
  frame $plot.view  

  # build widget $plot.view.3dview
  canvas $plot.view.3dview \
    -background linen \
    -height {90} \
    -highlightbackground {#ffffff} \
    -highlightcolor {#000000} \
    -relief {raised} \
    -width {80}

  # build widget $plot.view.redraw
  button $plot.view.redraw \
    -command redraw \
    -padx {4} \
    -pady {3} \
    -text [msgcat::mc "Refresh"]

  # build widget $plot.view.reset
  button $plot.view.reset \
    -command erasePlot \
    -padx {4} \
    -pady {3} \
    -text [msgcat::mc "Reset"]

  #build frame for x rotate
  set frx [frame $plot.view.frx -relief raised -borderwidth 2]
  label $frx.l0 -text [msgcat::mc "rot-x"] -width 4 -anchor w
  label $frx.l1 -textvariable x_rotate
  scale $frx.scale_x -command 3Dview -variable {x_rotate}  \
    -from {-180.0} -length {80} -orient {horizontal} -relief flat  \
    -to {180.0} -sliderlength 10 -width 12 -showvalue 0
  grid $frx.l0 $frx.l1 -sticky ew
  grid configure $frx.scale_x -columnspan 2 -sticky ew

  #build frame for y rotate
  set fry [frame $plot.view.fry -relief raised -borderwidth 2]
  label $fry.l0 -text [msgcat::mc "rot-y"] -width 4  -anchor w
  label $fry.l1 -textvariable y_rotate
  scale $fry.scale_y -command 3Dview -variable {y_rotate}  \
    -from {-180.0} -length {80} -orient {horizontal} -relief flat  \
    -to {180.0} -sliderlength 10 -width 12 -showvalue 0
  grid $fry.l0 $fry.l1 -sticky ew
  grid configure $fry.scale_y -columnspan 2 -sticky ew

  #build frame for z rotate
  set frz [frame $plot.view.frz -relief raised -borderwidth 2]
  label $frz.l0 -text [msgcat::mc "rot-z"] -width 4  -anchor w
  label $frz.l1 -textvariable z_rotate
  scale $frz.scale_z -command 3Dview -variable {z_rotate}  \
    -from {-180.0} -length {80} -orient {horizontal} -relief flat  \
    -to {180.0} -sliderlength 10 -width 12 -showvalue 0
  grid $frz.l0 $frz.l1 -sticky ew
  grid configure $frz.scale_z -columnspan 2 -sticky ew

  #build frame for zoom
  set frzm [frame $plot.view.frzm -relief raised -borderwidth 2]
  label $frzm.l0 -text [msgcat::mc "zoom"] -width 4  -anchor w
  label $frzm.l1 -textvariable zoom_level
  scale $frzm.scale_zoom -command 3Dview -variable {zoom_level}  \
    -from {-10.0} -length {80} -orient {horizontal} -relief flat  \
    -to {10.0} -sliderlength 10 -width 12 -showvalue 0
  grid $frzm.l0 $frzm.l1 -sticky ew
  grid configure $frzm.scale_zoom -columnspan 2 -sticky ew

  # build widget $plot.menu
    frame $plot.menu

  # build widget $plot.menu.viewxy - stock view
  button $plot.menu.viewxy \
    -command { \
      set x_rotate -90 ; \
      set y_rotate 0 ; \
      set z_rotate 0 ; \
      3Dview ; redraw } \
    -padx {4} \
    -width 10 \
    -text {X - Y}

  # build widget $plot.menu.viewxz - stock view
  button $plot.menu.viewxz \
    -command { \
      set x_rotate 0 ; \
      set y_rotate 0 ; \
      set z_rotate 0 ; \
      3Dview ; redraw } \
    -padx {4} \
    -width 10 \
    -text {X - Z}

  # build widget $plot.menu.viewyz - stock view
  button $plot.menu.viewyz \
    -command { \
      set x_rotate 0 ; \
      set y_rotate 0 ; \
      set z_rotate 90 ; \
      3Dview ; redraw } \
    -padx {4} \
    -width 10 \
    -text {Y - Z}

  # build widget $plot.menu.view3d - stock view
  button $plot.menu.view3d \
    -command { \
      set x_rotate -27 ; \
      set y_rotate 17 ; \
      set z_rotate 30 ; \
      3Dview ; redraw } \
    -padx {4} \
    -width 10 \
    -text {3D}

  # build widget $plot.menu.setup
  set plotsetuptext [msgcat::mc "TEST"]
  button $plot.menu.setup \
    -command { togglePlotSetup} \
    -padx {4} \
    -width 10 \
    -textvariable plotsetuptext

  # build widget $plot.3dplot
  frame $plot.3dplot \
    -borderwidth {2} \
    -relief {raised} \

  # build widget $plot.3dplot.vscroll
  scrollbar $plot.3dplot.vscroll \
    -width 10 \
   -command "$plot.3dplot.3dplot yview" \
    -cursor {left_ptr} \
    -relief {raised}

  # build widget $plot.3dplot.hscroll
  scrollbar $plot.3dplot.hscroll \
    -width 10 \
    -command "$plot.3dplot.3dplot xview" \
    -cursor {left_ptr} \
    -orient {horizontal} \
    -relief {raised}

  # build widget $plot.3dplot.3dplot
  canvas $plot.3dplot.3dplot \
    -background white \
    -relief {raised} \
    -width 180 \
    -scrollregion {-1500 -750 1500 750} \
    -xscrollcommand {$plot.3dplot.hscroll set} \
    -yscrollcommand {$plot.3dplot.vscroll set}

   # grid master $plot
    set plotSetup 1
    proc togglePlotSetup {} {
        global plot plotSetup plotsetuptext
        if {$plotSetup == 1} {
            grid configure $plot.view -row 1 -column 1 \
                -sticky nsew -padx 1 -pady 1
            set plotSetup 0
            set plotsetuptext [msgcat::mc "Hide Setup"]
        } else {
            grid forget $plot.view
            set plotSetup 1
            set plotsetuptext [msgcat::mc "Show Setup"]
        }
    }

  grid configure $plot.menu -row 0 -column 0 -columnspan 2 -sticky nsew -padx 1
  grid configure $plot.3dplot -row 1 -column 0 -sticky nsew -padx 1

  grid columnconfigure $plot 0 -weight 1
  grid rowconfigure $plot 1 -weight 1

  # pack master $plot.view
  pack configure $plot.view.3dview -anchor n -padx 2 -pady 2
  pack configure $frx -fill x
  pack configure $fry -fill x
  pack configure $frz -fill x
  pack configure $frzm -fill x
  pack configure $plot.view.redraw -fill both -expand yes
  pack configure $plot.view.reset -fill x

  # pack master $plot.menu revised to view
  pack configure $plot.menu.viewxy -side left -fill x -expand yes
  pack configure $plot.menu.viewxz -side left -fill x -expand yes
  pack configure $plot.menu.viewyz -side left -fill x -expand yes
  pack configure $plot.menu.view3d -side left -fill x -expand yes
  pack configure $plot.menu.setup -side left -fill x -expand yes

  # pack master $plot.3dplot
  pack configure $plot.3dplot.vscroll -fill y -side left
  pack configure $plot.3dplot.hscroll -fill x -side bottom
  pack configure $plot.3dplot.3dplot -side left -fill both -expand yes

  # build canvas items $plot.view.3dview
  # build canvas items $plot.3dplot.3dplot
    global Xlast Ylast Zlast Alast Blast Clast
    global X Y Z A B C 3d_plot
            set 3d_plot {0 0 0 0}
	set Xlast [format "%8.4f" $X ]
	set Ylast [format "%8.4f" $Y ]
	set Zlast [format "%8.4f" $Z ]
 	set Alast [format "%8.4f" $A ]
	set Blast [format "%8.4f" $B ]
	set Clast [format "%8.4f" $C ]
centerPlot
update
setInitialPlotview
update
togglePlotSetup
update
}

proc vector {} {
    global plot
    global X Y Z A B C
    # 3D vector conversion
    # X Y and Z point is converted into polar notation
    # then rotated about the A B and C axis.
    # Finally to be converted back into rectangular co-ords.
    #
    # As the three lines of the 3D Origin need to be resolved as well
    # as the cutter position, I thought a proc would be more efficient.

    # Rotate about A - X axis
    set angle [expr $A * 0.01745329]
     if { $Y != 0 || $Z != 0 } {
    set angle [expr atan2($Y,$Z) + $angle]
    }
    set vector [expr hypot($Y,$Z)]
    set Z [expr $vector * cos($angle)]
    set Y [expr $vector * sin($angle)]

    # Rotate about B - Y axis
    set angle [expr $B * 0.01745329]
    if { $X != 0 || $Z != 0 } {
    set angle [expr atan2($Z,$X) + $angle]
    }
    set vector [expr hypot($X,$Z)]
    set X [expr $vector * cos($angle)]
    set Z [expr $vector * sin($angle)]

    # Rotate about C - Z axis
    set angle [expr $C * 0.01745329]
    if { $X != 0 || $Y != 0 } {
    set angle [expr atan2($Y,$X) + $angle]
    }
    set vector [expr hypot($X,$Y)]
    set X [expr $vector * cos($angle)]
    set Y [expr $vector * sin($angle)]
}

# initialize global variables
global 3dview plot
set 3dview "$rup.plot.view.3dview"
global {3dplot}
set 3dplot  "$rup.plot.3dplot.3dplot"
global {X}
set {X} {0}
global {Y}
set {Y} {0}
global {Z}
set {Z} {0}
global {A}
set {A} {0}
global {B}
set {B} {0}
global {C}
set {C} {0}
global {x_rotate}
set {x_rotate} {0}
global {xlast}
set {xlast} {0}
global {xnext}
set {xnext} {0}
global {y_rotate}
set {y_rotate} {0}
global {ylast}
set {ylast} {0}
global {ynext}
set {ynext} {0}
global {z_rotate}
set {z_rotate} {0}
global {zoom}
set {zoom} {0}
global {zoom_level}
set {zoom_level} {0}

set lastline ??
set Gmode 4
set Glast 5
set delta_Alast 0
set delta_Blast 0
set delta_Clast 0

proc centerPlot {} {
    global 3dplot topx topy botx boty plot
    global X Z centerx centery
    global 3d_plot
    global Xlast Ylast Zlast Glast
    # Get current canvas size
    set plotx [$3dplot cget -width]
    set ploty [$3dplot cget -height]
    # Computes view (0,0) and attempts to center it
    set totalx [expr {abs($topx) + abs($botx)}]
    set lowerx [expr {abs($topx) - ($plotx / 2)}]
    if {$lowerx <= 0} {
        set centerx 0
    } else {
        set centerx [expr {double($lowerx) / double($totalx)*1.05}]
    }
    set totaly [expr {abs($topy) + abs($boty)}]
    set lowery [expr {abs($topy) - ($ploty / 2)}]
    if {$lowery <= 0} {
        set centery 0
    } else {
        set centery [expr {double($lowery) / double($totaly)*1.05}]
    }
    $3dplot addtag all all
    $3dplot delete all
    $3dplot xview moveto $centerx
    $3dplot yview moveto $centery
    # Delete all plot data except the last one.
    set 3d_plot [list $Xlast $Ylast $Zlast $Glast]
    3Dview
    redraw
}

proc erasePlot {} {
    global 3dplot topx topy botx boty plot
    global X Z centerx centery
    global 3d_plot
    global Xlast Ylast Zlast Glast
    $3dplot addtag all all
    $3dplot delete all
    # Delete all plot data except the last one.
    set 3d_plot [list $Xlast $Ylast $Zlast $Glast]
    3Dview
    redraw
}

# Procedure: 3Dview
proc 3Dview {args} {
    global x_rotate y_rotate z_rotate
    global 3dview plotcolour
    global X Y Z A B C
    global 3d_plot a_plot b_plot c_plot

    set 3d_object {25 25 25 3 25 -25 25 3 -25 -25 25 3 -25 25 25 3 25 25 25 3 \
        0 0 -30 2 -25 -25 25 2 -25 25 25 3 0 0 -30 2 25 -25 25 4}

    set atemp $A; set btemp $B; set ctemp $C
    set A $x_rotate ; set B $y_rotate ; set C $z_rotate

    # Delete the image and put in an origin point.
    $3dview delete all
    $3dview create oval 42 42 48 48 -fill green

    # set the first set of coordinates from the list then call vector
    set X [lindex $3d_object 0]
    set Y [lindex $3d_object 1]
    set Z [lindex $3d_object 2]
    set colour [lindex $3d_object 3]

    vector

    # then use these to start the plot
     set xlast $X ; set ylast $Z

    foreach {X Y Z colour} $3d_object {
        vector
        set xnext $X ; set ynext $Z
        $3dview create line [expr $xlast+45] [expr $ylast+45] [expr $xnext+45] [expr $ynext+45] \
          -fill [lindex $plotcolour $colour]
        set xlast $X ; set ylast $Z
    }

    set A $atemp; set B $btemp; set C $ctemp
    # If 3d_plot has been reset - do a redraw first
    #  to update the rotates and zoom
    if { [llength $3d_plot] <=9} {
        redraw
     }
}

# Procedure: redraw
proc redraw {args} {
    global zoom_level zoom
    global 3d_plot ticks
    global xlast ylast
    global x_rotate y_rotate z_rotate
    global a_plot b_plot c_plot
    global A B C X Y Z
    global 3dplot plotcolour
    global centerx centery

    set atemp $A; set btemp $B; set ctemp $C
    set a_plot $x_rotate
    set b_plot $y_rotate
    set c_plot $z_rotate
    set zoom $zoom_level

   if { $zoom <0 } {
        set zoom  [ expr abs(1.0/($zoom-1)) ]
   } else {
       set zoom [ expr $zoom+1]
   }

    # if { [emc_program_status] == "running" } {
    #     set statusflag 1
    # # If emc is running, issue a pause command
    # # and wait until IDLE status is reached.
    #     emc_pause
    # while { [emc_program_status] == "running" } {
    #     # Just loop until paused
    # }
    # } else {
    #     set statusflag 0
    # }
    # time the redraw proc
    set timer [clock clicks]

    $3dplot delete all

    # set the first set of coordinates from the list then call vector
    set X [lindex $3d_plot 0]
    set Y [lindex $3d_plot 1]
    set Z [lindex $3d_plot 2]
    set colour [lindex $3d_plot 3]
    set A $x_rotate
    set B $y_rotate
    set C $z_rotate
    vector

    # then use these to start the plot
    set x_last $X ; set y_last $Z
    foreach {X Y Z colour} $3d_plot {
        set A $x_rotate
        set B $y_rotate
        set C $z_rotate
        vector
        set x_next $X ; set y_next $Z
        $3dplot create line [expr $x_last*$zoom+$centerx] [expr $y_last*$zoom+$centery] \
            [expr $x_next*$zoom+$centerx] [expr $y_next*$zoom+$centery] \
            -fill [lindex $plotcolour $colour]
        set x_last $X ; set y_last $Z
   }

    set xlast $X ; set ylast $Z

    $3dplot create line [expr $x_last*$zoom+$centerx] [expr $y_last*$zoom+$centery] \
        [expr ($x_last*$zoom+$centerx)+5] [expr ($y_last*$zoom+$centery)-5] \
        -fill red -arrow first -tags 3darrow
    set A $atemp; set B $btemp; set C $ctemp

    set timer [expr ([clock clicks] - $timer)/$ticks]
    # If emc was forced to pause - restart.
    # if {$statusflag == 1} {
    #     emc_resume
    # }
}

proc updatePlot {} {
    global size 3dplot plot
    global xdir ydir zdir zoom
    global adir bdir cdir
    global programfiletext posdigit0 posdigit1 posdigit2 activeLine plotcolour
    global posdigit3 posdigit4 posdigit5
    global Xlast Ylast Zlast Alast Blast Clast lastline
    global xlast ylast xnext ynext centerx centery
    global X Y Z A B C
    global a_plot b_plot c_plot 3d_plot Gmode Glast
    global delta_Alast delta_Blast delta_Clast
    global unitsetting
    global worldlabellist

    # hack to divide scale for mm plotting
    if {$unitsetting == "(mm)" } {
        set scaler 25
    } else {
        set scaler 1
    }

    # Color plot line by setting active line to upcase currentline
        set currentline [string toupper [$programfiletext get $activeLine.0 $activeLine.end]]
    # Search currentline for g0-3 but make modal with no else
    if { [string first G2 $currentline] != -1 || \
        [string first G02 $currentline] != -1  } {
            set Gmode 2
    } elseif { [string first G3 $currentline] != -1 || \
        [string first G03 $currentline] != -1 } {
            set Gmode 3
    } elseif { [string first G1 $currentline] != -1 || \
        [string first G01 $currentline] != -1 } {
            set Gmode 1
    } elseif { [string first G0 $currentline] != -1 } {
        set Gmode 0
    }
    set delta_A 0
    set delta_B 0
    set delta_C 0


if { [ string first X $worldlabellist ] >= 0 } {
    set X [emc_abs_act_pos 0]
} else {
    set X 0
}
if { [ string first Y $worldlabellist ] >= 0 } {
    set Y [emc_abs_act_pos 1]
} else {
    set Y 0
}
if { [ string first Z $worldlabellist ] >= 0 } {
    set Z [emc_abs_act_pos 2]
} else {
    set Z 0
}
if { [ string first A $worldlabellist ] >= 0 } {
    set A [emc_abs_act_pos 3]
} else {
    set A 0
}
if { [ string first B $worldlabellist ] >= 0 } {
    set B [emc_abs_act_pos 4]
} else {
    set B 0
}
if { [ string first C $worldlabellist ] >= 0 } {
    set C [emc_abs_act_pos 5]
} else {
    set C 0
}

    set X [expr "$X * $size * $xdir" / $scaler]
    set Y [expr "$Y * $size * $ydir" / $scaler]
    set Z [expr "$Z * $size * $zdir" / $scaler]
    set A [expr "$A * $adir" / $scaler]
    set B [expr "$B * $bdir" / $scaler]
    set C [expr "$C * $cdir" / $scaler]

    vector
    set X [format "%10.1f" $X]
    set Y [format "%10.1f" $Y]
    set Z [format "%10.1f" $Z]

    if {$Alast != $A || $Blast != $B || $Clast != $C || $Gmode != $Gmode} {
      lappend 3d_plot $X $Y $Z $Glast
    } else {
      if {$Xlast != $X || $Ylast != $Y || $Zlast != $Z} {
        # Test to see if move is a straight line
        set d_x [expr $X-$Xlast]
        set d_y [expr $Y-$Ylast]
        set d_z [expr $Z-$Zlast]
    # Calculate the angles for the relative move
         if { $d_z != 0 } {
            set delta_A [expr atan2($d_y,$d_z)]
        } else {
            set delta_A 1.570796327
        }
     if { $d_x != 0 } {
    set delta_B [expr atan2($d_z,$d_x)]
    } else {
    set delta_B 1.570796327
    }
     if { $d_x != 0 } {
    set delta_C [expr atan2($d_y,$d_x)]
    } else {
    set delta_C 1.570796327
    }

# If the relative angles of the current move are the same
#  as the last relative move - Then it *must* be a continuation
# of a straight line move !
    if { $delta_A != $delta_Alast || $delta_B != $delta_Blast \
        || $delta_C != $delta_Clast } {
# If not, save the vector.
    lappend 3d_plot $Xlast $Ylast $Zlast $Glast
    }
    set delta_Alast $delta_A
    set delta_Blast $delta_B
    set delta_Clast $delta_C

    }
	}
	set Xlast $X
	set Ylast $Y
	set Zlast $Z
	set Alast $A
	set Blast $B
	set Clast $C
	set Glast $Gmode

# Plot 3D xy and move tool if X Y or Z has changed
	set A $a_plot; set B $b_plot; set C $c_plot
	vector
	set xnext $X ; set ynext $Z
	  if {$Gmode >= 5} {
	        set plotcolor yellow4
	     } else {
	        set plotcolor [lindex $plotcolour $Gmode]
	    }
	$3dplot create line [expr $xlast*$zoom+$centerx] [expr $ylast*$zoom+$centery] \
	   [expr $xnext*$zoom+$centerx] [expr $ynext*$zoom+$centery] \
	    -fill [lindex $plotcolour $Gmode]
	$3dplot move 3darrow [expr ($xnext - $xlast)*$zoom] [expr ($ynext - $ylast)*$zoom]
	set xlast $X ; set ylast $Z
}

#set initial conditions for 3d plot
proc setInitialPlotview {} {
    global x_rotate y_rotate z_rotate
    set x_rotate -27
    set y_rotate 17
    set z_rotate 30
    3Dview
    redraw
}

# -----end of backplot-----

