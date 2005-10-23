#!/bin/sh
# the next line restarts using emcsh \
exec bin/emcsh "$0" "$@"

# Autosizing Extended Tk GUI for the Enhanced Machine Controller
# GPL Copyright 2003 Raymond E. Henry <rehenry@up.net>
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

# This gui requires two option files TkEmcL for screens larger than 1000 wide
# and TkEmcS for smaller screens.

# fixme toggleJog stuff does not work with dual defined increments

set tkemc 1
package require msgcat

if ([info exists env(LANG)]) {
    msgcat::mclocale $env(LANG)
    msgcat::mcload "src/po"
}

if ([info exists env(EMC2_TCL_DIR)]) {
    set emc2tcldir $env(EMC2_TCL_DIR)
}


# wheelEvent code curtesy of Tsahi Levent-Levi - on activestate web site.
# this only works properly on TNG's RedHat 7.2 or newer os's.

proc wheelEvent { x y delta } {

    # Find out what's the widget we're on
    set act 0
    set widget [winfo containing $x $y]

    if {$widget != ""} {
        # Make sure we've got a vertical scrollbar for this widget
        if {[catch "$widget cget -yscrollcommand" cmd]} return

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
if {[string length $temp] == 0} {
    set temp 0.200
}
set displayCycleTime [expr {int($temp * 1000 + 0.5)}]

# set the debounce time for key repeats
set debounceTime 150

set toolfilename [emc_ini "TOOL_TABLE" "EMCIO"]
if {[string length $toolfilename] == 0} {
    set toolfilename "emc.tbl"
}

set paramfilename [emc_ini "PARAMETER_FILE" "RS274NGC"]
if {[string length $paramfilename] == 0} {
    set paramfilename "emc.var"
}

# determine what the axes and coordinates are.
set worldlabellist ""
set axiscoordmap ""
set numaxes [emc_ini "AXES" "TRAJ"]
set coordnames [ emc_ini "COORDINATES" "TRAJ"]

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
set helpfilename [emc_ini HELP_FILE DISPLAY]

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
            set jogIncrement 0.0001
        }
        set jogType $jogIncrement
    }
}

# given axis, determines speed and cont v. incr and jogs it neg
proc jogNeg {axis} {
    global activeAxis jogSpeed jogType jogIncrement axiscoordmap
    set activeAxis $axis
    if { [emc_teleop_enable] == 0 } {
	set axisToJog [ lindex $axiscoordmap $axis ]
    } else {
	set axisToJog $axis
    }
    if {$jogType == "continuous"} {
        emc_jog $axisToJog -$jogSpeed
    } else {
        emc_jog_incr $axisToJog -$jogSpeed $jogIncrement
    }
}

# given axis, determines speed and cont v. incr and jogs it pos
proc jogPos {axis} {
    global activeAxis jogSpeed jogType jogIncrement axiscoordmap
    set activeAxis $axis
    if { [emc_teleop_enable] == 0 } {
	set axisToJog [ lindex $axiscoordmap $axis ]
    } else {
	set axisToJog $axis
    }
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
    global minusAxis activeAxis
    
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
    global equalAxis activeAxis
    
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
set linjogincr 0.0100
set oldlinjogincr 0.0100
set templin [emc_ini  "DEFAULT_LINEAR_JOG_INCREMENT"  "DISPLAY"]
if {$templin != ""} {
    set oldlinjogincr $templin
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
        set jogplustext "JOG $axisname +"
        set jognegtext "JOG $axisname -"
        set axishometext "$axisname\n\nZ\nE\nR\nO"
        set maxJogSpeed [set maxJogSpeedAxis($temp) ]
        $feedscale configure -to $maxJogSpeed
        # add the linear v rotary jog increment stuff here.
        if {$axisType($oldaxis) == "LINEAR" && $axisType($temp) == "ANGULAR" } {
            set oldlinjogincr $jogIncrement
            grid forget $iframe
            grid configure $irframe -column 6 -row 0 -rowspan 5 -sticky nsew
            set jogIncrement $oldrotjogincr
        } elseif {$axisType($oldaxis) == "ANGULAR" && $axisType($temp) == "LINEAR" } {
            set oldrotjogincr $jogIncrement
            grid forget $irframe
            grid configure $iframe -column 6 -row 0 -rowspan 5 -sticky nsew
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
    if {[emc_estop] == "on"} {
        emc_estop off
        emc_wait done
        emc_machine on
        if { [info exists coord ] } {
            $coord.bzero configure -bg lightgreen -state normal
        }
    } else {
        emc_estop on
        emc_wait done
        emc_machine off
        if { [info exists coord ] } {
            $coord.bzero configure -bg lightgray -state disabled
        }
    }
}

proc toggleMachine {} {
    if {[emc_machine] == "off"} {
        emc_machine on
    } else {
        emc_machine off
    }
}

proc toggleMist {} {
    if {[emc_mist] == "off"} {
        emc_mist on
    } else {
        emc_mist off
    }
}

proc toggleFlood {} {
    if {[emc_flood] == "off"} {
        emc_flood on
    } else {
        emc_flood off
    }
}

proc toggleBrake {} {
    if {[emc_brake] == "on"} {
        emc_brake off
    } else {
        emc_brake on
    }
}


# Initialize feedoverride and lastfeedoverride before first calling

proc toggleFeedhold {} {
    global feedtext feedoverride lastfeedoverride
    set temp [emc_feed_override]
    if {$temp >= 2} {set lastfeedoverride $temp}
    switch -- $feedtext {
        FEEDHOLD {
            set feedoverride 0
            emc_feed_override $feedoverride
        }
        CONTINUE {
            set feedoverride $lastfeedoverride
            emc_feed_override $feedoverride
        }
        default {
        }
    }
}

# jog type only exists at the interface level

proc setJogType {which} {
    global jogType jogincrement jogcontinuous  iframe irframe
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
    if {[file exists $f]} {
        option readfile $f startupFile
        break
    } else {
        set optionfile ""
    }
}

proc popupAboutx {} {
    tk_messageBox  -title "ABOUT"  -default "ok" -message "TkMini \
        \n\nTcl/Tk GUI for Enhanced Machine Controller\n\nGPL Copyright 2003 \
        \nRay Henry <rehenry@up.net>
        \nThis software comes with ABSOLUTELY NO GUARANTEE!  \
        \nFor details see the copyright.html file in this directory."
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

# this is calib from tkemc
source $emc2tcldir/bin/emccalib.tcl

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
set filem [menubutton $menubar.file -text Program ]
set viewm [menubutton $menubar.view -text View ]
set settingsm [menubutton $menubar.settings -text Settings ]
set infom [menubutton $menubar.info -text Info ]
set helpm [menubutton $menubar.help -text "Help" ]

# Now add buttons for popins
set popins $menubar
set p1 [checkbutton $popins.b1 -text "Backplot" \
    -variable popArray(Plot) -command {popIn Plot} ]
set p2 [checkbutton $popins.b2 -text "Editor" \
    -variable popArray(Editor) -command {popIn Editor} ]
set p3 [checkbutton $popins.b3 -text "Offsets" \
    -variable popArray(Offsets) -command {popIn Offsets} ]
set p4 [checkbutton $popins.b4 -text "Tools" \
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
$filemenu add command -label "Reset" \
    -command {emc_task_plan_init} -underline 0
$filemenu add separator
$filemenu add command -label "Exit" \
    -command {after cancel updateStatus ; destroy . ; exit} \
    -accelerator $modifierstring+X -underline 1
bind . <$modifier-x> {after cancel updateStatus ; destroy . ; exit}

# layout menu
$viewmenu add checkbutton -label "Position Type" \
    -variable viewArray(Position) -command {viewInfo Position}
$viewmenu add checkbutton -label "Tool Info" \
    -variable viewArray(Tool) -command {viewInfo Tool}
$viewmenu add checkbutton -label "Offset Info" \
    -variable viewArray(Offset) -command {viewInfo Offset}
$viewmenu add separator
$viewmenu add command -label "Show Restart"  -command {showRestart}
$viewmenu add command -label "Hide Restart"  -command {hideRestart}
$viewmenu add separator
$viewmenu add command -label "Show Split Right"  \
    -command {rightConfig split }
$viewmenu add command -label "Show Mode Full"  \
    -command {rightConfig modefull }
$viewmenu add command -label "Show Popin Full"  \
    -command {rightConfig popfull }

# settings menu
$settingsmenu add radiobutton -label "Actual Position" \
    -variable actcmd -value actual
$settingsmenu add radiobutton -label "Commanded Position" \
    -variable actcmd -value commanded
$settingsmenu add separator
$settingsmenu add radiobutton -label "Machine Position" \
    -variable coords  -value machine
$settingsmenu add radiobutton -label "Relative Position" \
    -variable  coords -value relative
$settingsmenu add separator
$settingsmenu add command -label "Set Backlash" \
    -command {popupCalibration}

# info menu
$infomenu add command -label "Program File" \
    -command {mText "Program file is $programnamestring"}
$infomenu add command -label "Editor File" \
    -command {mText "Editor file is $editFilename"}
$infomenu add command -label "Parameter File" \
    -command {mText "Parameter file is $paramfilename"}
$infomenu add command -label "Tool File" \
    -command {mText "Tool file is $toolfilename"}
$infomenu add separator
$infomenu add command -label "Active G Codes" -command \
    {mText "Active codes include; \n$programcodestring"}
$infomenu add command -label "Check It" -command {checkIt}

# help menu
$helpmenu add checkbutton -label "Help..." -variable popArray(Help) -command {popIn Help} -underline 0
$helpmenu add command -label "About..." -command {popupAboutx} -underline 0


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
set abortbutton [button $up.abort -text "ABORT" -width 8  -takefocus 0 -borderwidth 2 \
    -bg yellow -fg black -activebackground gold3 -activeforeground white ]
bind $abortbutton <ButtonPress-1> {emc_abort ; showRestart}

# feedhold
set feedholdbutton [button $up.feedhold -textvariable feedtext -width 8  -takefocus 0 \
    -borderwidth 2 ]
bind $feedholdbutton <ButtonPress-1> {toggleFeedhold}

# initial feedhold conditions
set feedtext "CONTINUE"
set feedoverride [emc_feed_override]
set lastfeedoverride 100
toggleFeedhold

# mode MDI
set mdibutton [button $up.mdi -text "MDI" -width 8  -takefocus 0  -borderwidth 2    ]
bind $mdibutton <ButtonPress-1> {showMode mdi}

# mode AUTO
set autobutton [button $up.auto -text "AUTO" -width 8  -takefocus 0  -borderwidth 2    ]
bind $autobutton <ButtonPress-1> {showMode auto}

# mode MANUAL
set manbutton [button $up.manual -text "MANUAL" -width 8  -takefocus 0  -borderwidth 2   ]
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
set toollabel [label $tools.toollabel -text "Tool \#:" -anchor w]
set toolsetting [label $tools.toolsetting -textvariable toolsetting -width 2 -anchor w]
set tooloffsetlabel [label $tools.tooloffsetlabel -text "Length :" -anchor w]
set tooloffsetsetting [label $tools.tooloffsetsetting -textvariable tooloffsetsetting -anchor e]
grid configure $toollabel -row 0 -column 0 -sticky ew
grid configure $toolsetting -row 0 -column 1 -sticky ew
grid configure $tooloffsetlabel -row 0 -column 2 -sticky ew
grid configure $tooloffsetsetting -row 0 -column 3 -sticky ew
grid columnconfigure $tools 0 -weight 1
grid columnconfigure $tools 1 -weight 1
grid columnconfigure $tools 2 -weight 1

set offsets [frame $left.offsets ]
set offsetlabel [label $offsets.offsetlabel -text "Work Offsets: " -anchor w ]
set workoffsets [label $offsets.offsetsetting -textvariable offsetsetting ]
bind $offsets <ButtonPress-1> {mText "start offsets popup here"}
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
if {[string length $temp] == 0} {
    set temp 1
}
set maxFeedOverride [int [expr $temp * 100 + 0.5]]
set oridescalelength [expr $swidth / 4 ]

set oride [frame $left.oride ]
bind $oride <ButtonPress-1> {mText "start override popup here"}
set oridetop [frame $oride.top]
set oridebottom [frame $oride.bottom]
set oridelabel [label $oridetop.label -text "Feed Override:" ]
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
set mbutton [button $mview.button -text "-- MESSAGES --" -bd 2 \
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

# ----------RIGHT WIDGETS----------

# toggleView moves the variable widget stack focus up one
proc toggleView {} {
puts "toggleView's not doin nothin yet"
}

# popin control rutines
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
    global popinframe popArray
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


# ----------RIGHT - TOOL SETUP----------

proc popinTools {} {
    global popinframe tool tooltext toolframe toolnum
    set tool [frame $popinframe.tools ]
    label $tool.l1 -justify center -text "TOOL SETUP \n Click or tab to edit.  \
    Press enter to return to keyboard machine control."
    # put the tool file into an invisible widget
    set tooltext [text $tool.vartext]
    set toolframe [frame $tool.frame ]
    # set selt [label $toolframe.selt -text "  SELECT  " ]
    set poc [label $toolframe.poc -text "  TOOL NUMBER  " ]
    set len [label $toolframe.len -text "  LENGTH  " ]
    set diam [label $toolframe.diam -text "  DIAMETER  " ]
    set com [label $toolframe.com -text "  COMMENT  " ]
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
    if {[catch {open $toolfilename} programin]} {
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
    set tadd [button $toolframe.add -text "Add Extra Tool" -command {addTool} ]
    set trem [button $toolframe.rem -text "Remove Last Tool" -command {remTool} ]
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
        mText "Can't update the tool file while machine is in auto and $programstatusstring."
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
        mText "This is not a good number."
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
        mText "Can't update the tool file while machine is in auto and $programstatusstring."
    }
    focus -force "."
}

# -----end of tool setup


# ----------RIGHT - OFFSETS SYSTEM SETUP----------

set paramfilename [emc_ini "PARAMETER_FILE" "RS274NGC"]
if {[string length $paramfilename] == 0} {
    set paramfilename "emc.var"
}
set numaxis [emc_ini "AXES" "TRAJ"]
set nameaxis [emc_ini "COORDINATES" "TRAJ"]
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
    label $coord.l1 -text "COORDINATE SYSTEM SETUP \n\n \
        Click value to edit with keyboard.  Press enter to return to keyboard control of machine. \n "
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
    label $caxis.name -text "Axis "
    label $caxis.varval -text "Value "
    grid $caxis.name $caxis.varval -sticky news
    for {set i 0} {$i < $numaxis} {incr i} {
        label  $caxis.l$i -text "[lindex $nameaxis $i ]   "  -anchor e
        entry $caxis.e$i -textvariable val$i -fg darkred -bg white -relief flat -width 10 -takefocus 1
        button $caxis.b$i -text "Teach" -command "getLocation $i" -takefocus 0
        grid $caxis.l$i $caxis.e$i $caxis.b$i -sticky news
        bind $caxis.e$i <KeyPress-Return> {setVarValues ; loadVarFile }
    }
    # Build the control button widgets.
    set cbuttons [frame $coord.buttons]
    button $cbuttons.b0 -textvariable "zerocoordnumber" -width 16 \
        -command {getZero} -takefocus 0
    button $cbuttons.b4 -text "Write And Load File " -width 16 \
        -command {setVarValues ; loadVarFile} -takefocus 0
    label $cbuttons.l0 -text "Offset By Radius"
    label $cbuttons.l1 -text "Offset By Length"
    entry $cbuttons.e0 -textvariable touchoffradius -relief flat -bg white
    entry $cbuttons.e1 -textvariable touchofflength -relief flat -bg white
    bind $cbuttons.e0 <KeyPress-Return> {setTouchOff ; focus -force ".top"}
    bind $cbuttons.e1 <KeyPress-Return> {setTouchOff ; focus -force ".top"}
    radiobutton $cbuttons.r0 -text Subtract -variable touchoffdirection -value "-" -anchor w \
        -takefocus 0
    radiobutton $cbuttons.r1 -text Add -variable touchoffdirection -value "+" -anchor w \
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
        5221 {set zerocoordnumber "Zero All G54" }
        5241 {set  zerocoordnumber "Zero All G55" }
        5261 {set  zerocoordnumber "Zero All G56" }
        5281 {set  zerocoordnumber "Zero All G57" }
        5301 {set  zerocoordnumber "Zero All G58" }
        5321 {set  zerocoordnumber "Zero All G59" }
        5341 {set  zerocoordnumber "Zero All G59.1" }
        5361 {set  zerocoordnumber "Zero All G59.2" }
        5381 {set  zerocoordnumber "Zero All G59.3" }
        default {set  zerocoordnumber "Zero All ????" }
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

# Be a bit careful with this with steppers it homes all axi at the same time.
# Will have to add the var file read here rather than in popin
proc setAllZero { } {
    global vartext numaxis tooloffsetsetting
    findVarNumbers
    if {$tooloffsetsetting > 0.0001 } {
        mText "Can't set zero with a tool offset active so I issued G49 to cancel it."
        emc_mdi g49
    }
    # zero out g92 here
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
    if {[catch {open $name w} fileout]} {
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
        mText "Can't update the var file while machine is in auto and $programstatusstring."
    }
    focus -force "."
}

# -----end of coord -----

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
after 10000
set t2 [clock clicks]
set ticks [expr ($t2-$t1)/10000]

# procedure to show window .
proc popinPlot {} {
    global  plot popinframe plotSetup plotsetuptext
    set plot [frame $popinframe.plot]
    pack $plot -fill both -expand yes

  # build widget $plot.view
  frame $plot.view  \

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
    -text {Refresh}

  # build widget $plot.view.reset
  button $plot.view.reset \
    -command erasePlot \
    -padx {4} \
    -pady {3} \
    -text {Reset}

  #build frame for x rotate
  set frx [frame $plot.view.frx -relief raised -borderwidth 2]
  label $frx.l0 -text "rot-x" -width 4 -anchor w
  label $frx.l1 -textvariable x_rotate
  scale $frx.scale_x -command 3Dview -variable {x_rotate}  \
    -from {-180.0} -length {80} -orient {horizontal} -relief flat  \
    -to {180.0} -sliderlength 10 -width 12 -showvalue 0
  grid $frx.l0 $frx.l1 -sticky ew
  grid configure $frx.scale_x -columnspan 2 -sticky ew

  #build frame for y rotate
  set fry [frame $plot.view.fry -relief raised -borderwidth 2]
  label $fry.l0 -text "rot-y" -width 4  -anchor w
  label $fry.l1 -textvariable y_rotate
  scale $fry.scale_y -command 3Dview -variable {y_rotate}  \
    -from {-180.0} -length {80} -orient {horizontal} -relief flat  \
    -to {180.0} -sliderlength 10 -width 12 -showvalue 0
  grid $fry.l0 $fry.l1 -sticky ew
  grid configure $fry.scale_y -columnspan 2 -sticky ew

  #build frame for z rotate
  set frz [frame $plot.view.frz -relief raised -borderwidth 2]
  label $frz.l0 -text "rot-z" -width 4  -anchor w
  label $frz.l1 -textvariable z_rotate
  scale $frz.scale_z -command 3Dview -variable {z_rotate}  \
    -from {-180.0} -length {80} -orient {horizontal} -relief flat  \
    -to {180.0} -sliderlength 10 -width 12 -showvalue 0
  grid $frz.l0 $frz.l1 -sticky ew
  grid configure $frz.scale_z -columnspan 2 -sticky ew

  #build frame for zoom
  set frzm [frame $plot.view.frzm -relief raised -borderwidth 2]
  label $frzm.l0 -text zoom -width 4  -anchor w
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
  set plotsetuptext "TEST"
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
            set plotsetuptext {Hide Setup}
        } else {
            grid forget $plot.view
            set plotSetup 1
            set plotsetuptext {Show Setup}
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
setInitialPlotview
togglePlotSetup
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
    if {[llength $3d_plot] <=9} {
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

    if {[emc_program_status] == "running" } {
        set statusflag 1
    # If emc is running, issue a pause command
    # and wait until IDLE status is reached.
        emc_pause
    while {[emc_program_status] == "running" } {
        # Just loop until paused
    }
    } else {
        set statusflag 0
    }
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
    if {$statusflag == 1} {
        emc_resume
    }
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

    set X [emc_abs_act_pos 0]
    set Y [emc_abs_act_pos 1]
    set Z [emc_abs_act_pos 2]
    set A [emc_abs_act_pos 3]
    set B [emc_abs_act_pos 4]
    set C [emc_abs_act_pos 5]

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

# ----editor widgets -----

set editFilename ""
set initialDir $programDirectory

proc popinEditor {} {
    global editFilename textwin programnamestring
    global editwidth editheight popinframe

    set editTypes {{"All files" *} {Text files} {.txt}}

    if {![info exists editwidth]} {set editwidth 80}
    if {![info exists editheight]} {set editheight 40}
    set editframe [frame $popinframe.editor ]
    pack $editframe -side top -fill both -expand yes
    set textframe [frame $editframe.textframe ]
    set textwin [text  $textframe.textwin  -width $editwidth -height $editheight -padx 4 -wrap word \
         -yscrollcommand "editScrolltext $textframe" ]
    set scrolly $textframe.scrolly
    scrollbar $scrolly -orient vert -command "$textwin yview" -width 8
    pack $scrolly -side right -fill y
    pack $textwin -side top -fill both -expand true

    bind $textwin <Control-c> "editCopyIt $textwin; break"
    bind $textwin <Control-v> "editPasteIt $textwin; break"
    bind $textwin <Control-x> "editCutIt $textwin; break"
    bind $textwin <Control-a> "editSelectAll $textwin; break"

    set menubar [frame $editframe.menuframe -relief raised -bd 2]
    pack $menubar -side top -fill x -expand yes
    menubutton $menubar.file -text "file" -menu $menubar.file.menu
    menubutton $menubar.edit -text "edit" -menu $menubar.edit.menu
    menubutton $menubar.settings -text "settings" -menu $menubar.settings.menu
    menubutton $menubar.scripts -text "scripts" -menu $menubar.scripts.menu
    menubutton $menubar.help -text "Help" -menu $menubar.help.menu
    pack $menubar.file -side left
    pack $menubar.edit -side left
    pack $menubar.settings -side left
    pack $menubar.scripts -side left
    pack $menubar.help -side right
    set filemenu $menubar.file.menu
    set editmenu $menubar.edit.menu
    set settingsmenu $menubar.settings.menu
    set scriptsmenu $menubar.scripts.menu
    set helpmenu $menubar.help.menu
    menu $filemenu
    menu $editmenu
    menu $settingsmenu
    menu $scriptsmenu
    menu $helpmenu

    $filemenu add command -label "New..." -underline 0 -command "editNewFile" -accelerator "Ctrl+N"
    $filemenu add command -label "Open..." -underline 0 -command "editOpenFile" -accelerator "Ctrl+O"
    $filemenu add command -label "Save" -underline 0 -command "editSaveFile" -accelerator "Ctrl+S"
    $filemenu add command -label "Save As..." -underline 5 -command "editSaveFileAs"
    $filemenu add separator
    $filemenu add command -label "Save and Load" \
        -command "editSaveFile ; changeProgram" -underline 1

    $editmenu add command -label "Cut" -underline 2 -command "editCutIt $textwin" -accelerator "Ctrl+X"
    $editmenu add command -label "Copy" -underline 0 -command "editCopyIt $textwin" -accelerator "Ctrl+C"
    $editmenu add command -label "Paste" -underline 0 -command "editPasteIt $textwin" -accelerator "Ctrl+V"
    $editmenu add command -label "Undo" -underline 0 -command "editUndoIt $textwin" -accelerator "Ctrl+U"
    $editmenu add separator
    $editmenu add command -label "Select All" -underline 7 -command "focus $textwin; editSelectAll $textwin" -accelerator "Ctrl+A"
    $editmenu add separator
    $editmenu add command -label "Find Text" -underline 0 -command "editEnterText $textwin"
    $editmenu add command -label "Renumber File" -underline 0 -command "editSetLineNumber 1"

    $settingsmenu add command -label "No Numbering" -underline 0 -command "set startnumbering 0"
    $settingsmenu add separator
    $settingsmenu add command -label "Line Numbering" -underline 0 -command "editSetLineNumber 0"

    # adds a script menu that looks for *.ncw files and adds their filename to script menu
    set scriptdir [file join [pwd] tcl scripts ]
    set files [exec /bin/ls $scriptdir]
    foreach file $files {
        if {[string match *.ncw $file]} {
            set editfname [file rootname $file]
            $scriptsmenu add command -label $editfname -command "source $scriptdir/$file"
        }
    }
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
    if {[file isfile $fname] == 0} {
        return
    }
    if {[catch {open $fname} filein]} {
        puts stdout "can't open $fname"
    } else {
        $textwin delete 1.0 end
        $textwin insert end [read $filein]
        catch {close $filein}
    }
}

proc editOpenFile {} {
    global textwin editFilename initialDir
    set fname [tk_getOpenFile -initialdir $initialDir  ]
    set editFileDirectory [file dirname $fname]
    if {[string length $fname] == 0} {
        return
    }
    $textwin delete 1.0 end
    if {[catch {open $fname} filein]} {
        puts stdout "can't open $fname"
    } else {
        $textwin insert end [read $filein]
        catch {close $filein}
        set editFilename $fname
    }
}

proc editSaveFile {} {
    global editFilename textwin
    catch {file copy -force $editFilename $editFilename.bak}
    if {[catch {open $editFilename w} fileout]} {
        puts stdout "can't save $editFilename"
        return
    }
    puts $fileout [$textwin get 1.0 end]
    catch {close $fileout}
}

proc editNewFile {} {
    global editFilename textwin
    $textwin delete 1.0 end
    editSaveFileAs
}

proc editSaveFileAs {} {
    global editFilename editTypes initialDir
    set fname [tk_getSaveFile -initialdir $initialDir]
    if {[string length $fname] == 0} {
        return
    }
    set editFilename $fname
    editSaveFile
    return $fname
}

proc editCutIt {w} {
    global selecttext selectlocation
    set selecttext [selection get STRING]
    set selectlocation [$w index insert]
    $w delete "insert - [string length $selecttext] chars" insert
}

proc editCopyIt {w} {
    global selecttext
    set selecttext [selection get STRING]
    # should drop text tags here
    # should disable copy until a paste
}

proc editPasteIt {w} {
    global selecttext
    $w insert insert $selecttext
    # should set copy menubutton to normal here
}

proc editUndoIt {w} {
    global selecttext selectlocation
    $w mark set insert "$selectlocation -[string length $selecttext] chars"
    set templocation [$w index insert]
    puts "the text to undo is $selecttext and the place to put it is $templocation"
    $w insert insert $selecttext
}

proc editSelectAll {w} {
    event generate $w <Control-slash>
}

# Find text processes - editEnterText includes hard location from top right.
proc editEnterText {ed} {
    global textwin
    toplevel .find
    wm title .find "Find"
    set findit ".find"
    wm geometry $findit 275x150-50+100

    label $findit.label1 -text "Find :" -anchor e
    place $findit.label1 -x 5 -y 5 -width 60 -height 20

    entry $findit.entry1 -relief sunken -textvariable sword
    place $findit.entry1 -x 70  -y 5 -width 110 -height 20

    label $findit.label2 -text "Replace :" -anchor e
    place $findit.label2 -x 5 -y 30 -width 60 -height 20

    entry $findit.entry2 -relief sunken -textvariable rword
    place $findit.entry2 -x 70 -y 30 -width 110 -height 20

    button $findit.button1 -text "Find All" -command {editFindAll $sword}
    place $findit.button1 -x 5 -y 70 -width 130 -height 30

    button $findit.button2 -text "Replace All" -command {editReplaceAll $sword $rword}
    place $findit.button2 -x 5 -y 110 -width 130 -height 30

    button $findit.button3 -text "Skip This" -command {editSkipWord $sword}
    place $findit.button3 -x 140 -y 70 -width 130 -height 30

    button $findit.button4 -text "Replace This" -command {editReplaceWord $sword $rword}
    place $findit.button4 -x 140 -y 110 -width 130 -height 30

    button $findit.button5 -text Cancel -command "focus -force $textwin; destroy $findit"
    place $findit.button5 -x 210 -y 5 -width 60 -height 30

    button $findit.button6 -text Clear -command {
        $textwin tag delete $sword
        $textwin tag delete q
    }
    place $findit.button6 -x 210 -y 35 -width 60 -height 30
    # set focus to entry widget 1
    focus -force ".find"
    bind ".find" <FocusOut> {destroy ".find"}
}

proc editFindAll {sword} {
    global textwin
    set firstplace 1.0
    set l1 [string length $sword]
    scan [$textwin index end] %d nl
    set thisplace [$textwin index insert]
    for {set i 1} {$i < $nl} {incr i} {
        $textwin mark set last $i.end
        set lastplace [$textwin index last]
        set thisplace [$textwin search -forwards -nocase $sword $thisplace $lastplace]
        if {$thisplace != ""} {
            $textwin mark set insert $thisplace
            scan [$textwin index "insert + $l1 chars"] %f lastplace
            $textwin tag add $sword $thisplace $lastplace
            $textwin tag configure $sword -background lightblue
            $textwin mark set insert "insert + $l1 chars"
            set thisplace $lastplace
        } else {
            set thisplace $lastplace
        }
    }
    $textwin mark set insert 1.0
    editNextWord $sword
}

proc editNextWord {sword} {
    global textwin
    set findnext [$textwin tag nextrange $sword insert]
    if {$findnext == ""} {
        $textwin mark set insert 1.0
        $textwin see insert
        return
    }
    set start [lindex $findnext 0]
    set last [lindex $findnext end]
    catch {$textwin mark set insert $start}
    $textwin tag add q $start $last
    $textwin tag raise q
    $textwin tag configure q -background darkred -foreground white
    $textwin see "insert + 5 lines"
}

proc editSkipWord {sword} {
    global textwin
    set l1 [string length $sword]
    $textwin tag remove q insert "insert + $l1 chars"
    $textwin tag remove $sword insert "insert + $l1 chars"
    editNextWord $sword
}

proc editReplaceWord {sword rword} {
    global textwin
    set l1 [string length $sword]
    set l2 [string length $rword]
    $textwin tag remove q insert "insert + $l1 chars"
    $textwin tag remove $sword insert "insert + $l1 chars"
    $textwin delete insert "insert + $l1 chars"
    $textwin insert insert $rword
    $textwin mark set insert "insert + $l2 chars"
    editNextWord $sword
}

proc editReplaceAll {sword rword} {
    global textwin
    set l1 [string length $sword]
    set l2 [string length $rword]
    scan [$textwin index end] %d nl
    set thisplace [$textwin index 1.0]
    for {set i 1} {$i < $nl} {incr i} {
        $textwin mark set last $i.end
        set lastplace [$textwin index last]
        set thisplace [$textwin search -forwards -nocase $sword $thisplace $lastplace]
        if {$thisplace != ""} {
            $textwin mark set insert $thisplace
            $textwin delete insert "insert + $l1 chars"
            $textwin insert insert $rword
            $textwin mark set insert "insert + $l2 chars"
            set thisplace [$textwin index insert]
        } else {
            set thisplace $lastplace
        }
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
    wm title .linenumber "Set Line Numbering"
    wm geometry .linenumber 275x180-60+100
    set linenum [frame .linenumber.frame]
    pack $linenum -side top -fill both -expand yes
    label $linenum.label1 -text "Increment"
    place $linenum.label1 -x 5 -y 5
    radiobutton $linenum.incr1 -text One -variable lineincrement -value 1 -anchor w
    place $linenum.incr1 -x 10 -y 25 -width 80 -height 20
    radiobutton $linenum.incr2 -text Two -variable lineincrement -value 2 -anchor w
    place $linenum.incr2 -x 10 -y 45 -width 80 -height 20
    radiobutton $linenum.incr5 -text Five -variable lineincrement -value 5 -anchor w
    place $linenum.incr5 -x 10 -y 65 -width 80 -height 20
    radiobutton $linenum.incr10 -text Ten -variable lineincrement -value 10 -anchor w
    place $linenum.incr10 -x 10 -y 85 -width 80 -height 20
    label $linenum.label2 -text "Space"
    place $linenum.label2 -x 130 -y 5
    radiobutton $linenum.space1 -text "Single Space" -variable space -value { } -anchor w
    place $linenum.space1 -x 140 -y 25
    radiobutton $linenum.space2 -text "Double Space" -variable space -value {  } -anchor w
    place $linenum.space2 -x 140 -y 45
    radiobutton $linenum.space3 -text "Tab Space" -variable space -value {    } -anchor w
    place $linenum.space3 -x 140 -y 65
    button $linenum.ok -text OK -command {destroy .linenumber} -height 1 -width 9
    place $linenum.ok -x 160 -y 127
    label $linenum.label3 -text "Next Number : " -anchor e
    place $linenum.label3 -x 5 -y 130 -width 95
    entry $linenum.entry -width 6 -textvariable number
    place $linenum.entry -x 100 -y 130
    button $linenum.renum -text Renumber -command editReNumber -height 1 -width 9 -state disabled
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
        $textwin insert $i.0 n$number$space
        set l1 [string length n$number$space]
        $textwin mark set insert "$i.$l1"
        set character [$textwin get insert]
        if {$character == "/"} {
            $textwin insert $i.0 "/"
            $textwin delete insert
        }
        set character [$textwin get insert]
        if {$character == "n" || $character == "N"} {
            set firstplace [$textwin index insert]

            # find the last number in the n word
            $textwin mark set insert "insert + 1 chars"
            set character [$textwin get insert]
            while {[string match {[0-9]} $character] == 1} {
                $textwin mark set insert "insert + 1 chars"
                set character [$textwin get insert]
            }

            # find the first character of the next word using space and tab
            while {$character == " " || $character == " "} {
                $textwin mark set insert "insert + 1 chars"
                set character [$textwin get insert]
            }
            $textwin delete "$firstplace" "insert"
        }
        incr number $lineincrement
    }
    set startnumbering 0
}

# -----end editor ------

# -----RIGHT HELP-----

set helpFile [emc_ini HELP_FILE DISPLAY ]
proc popinHelp {} {
    global popinframe helpFile
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
    if { [file isfile $helpFile ] == 1} {
        set fname $helpFile
    }
    if {[catch {open $fname} filein]} {
        mText "can't open $fname"
    } else {
        $helptextwin delete 1.0 end
        $helptextwin insert end [read $filein]
        catch {close $filein}
    }
}

proc helpScrolltext {tf a b} {
    $tf.scrolly set $a $b
}


# ----- RIGHT DIAGNOSTIC -----

# pop in the diagnostics window
proc popinDiagnostics {} {
    global popinframe
    global taskhb taskcmd tasknum taskstatus
    global iohb iocmd ionum iostatus
    global motionhb motioncmd motionnum motionstatus
    set d [frame $popinframe.diagnostics]
    pack $d -side top -fill both -expand yes
    label $d.task -text "Task" -anchor center
    frame $d.taskhb
    label $d.taskhb.lab -text "Heartbeat:" -anchor w
    label $d.taskhb.val -textvariable taskhb -anchor e
    frame $d.taskcmd
    label $d.taskcmd.lab -text "Command:" -anchor w
    label $d.taskcmd.val -textvariable taskcmd -anchor e
    frame $d.tasknum
    label $d.tasknum.lab -text "Command #:" -anchor w
    label $d.tasknum.val -textvariable tasknum -anchor e
    frame $d.taskstatus
    label $d.taskstatus.lab -text "Status:" -anchor w
    label $d.taskstatus.val -textvariable taskstatus -anchor e
    pack $d.taskhb.lab -side left
    pack $d.taskhb.val -side right
    pack $d.taskcmd.lab -side left
    pack $d.taskcmd.val -side right
    pack $d.tasknum.lab -side left
    pack $d.tasknum.val -side right
    pack $d.taskstatus.lab -side left
    pack $d.taskstatus.val -side right
    label $d.io -text "I/O" -anchor center
    frame $d.iohb
    label $d.iohb.lab -text "Heartbeat:" -anchor w
    label $d.iohb.val -textvariable iohb -anchor e
    frame $d.iocmd
    label $d.iocmd.lab -text "Command:" -anchor w
    label $d.iocmd.val -textvariable iocmd -anchor e
    frame $d.ionum
    label $d.ionum.lab -text "Command #:" -anchor w
    label $d.ionum.val -textvariable ionum -anchor e
    frame $d.iostatus
    label $d.iostatus.lab -text "Status:" -anchor w
    label $d.iostatus.val -textvariable iostatus -anchor e
    pack $d.iohb.lab -side left
    pack $d.iohb.val -side right
    pack $d.iocmd.lab -side left
    pack $d.iocmd.val -side right
    pack $d.ionum.lab -side left
    pack $d.ionum.val -side right
    pack $d.iostatus.lab -side left
    pack $d.iostatus.val -side right
    label $d.motion -text "Motion" -anchor center
    frame $d.motionhb
    label $d.motionhb.lab -text "Heartbeat:" -anchor w
    label $d.motionhb.val -textvariable motionhb -anchor e
    frame $d.motioncmd
    label $d.motioncmd.lab -text "Command:" -anchor w
    label $d.motioncmd.val -textvariable motioncmd -anchor e
    frame $d.motionnum
    label $d.motionnum.lab -text "Command #:" -anchor w
    label $d.motionnum.val -textvariable motionnum -anchor e
    frame $d.motionstatus
    label $d.motionstatus.lab -text "Status:" -anchor w
    label $d.motionstatus.val -textvariable motionstatus -anchor e
    pack $d.motionhb.lab -side left
    pack $d.motionhb.val -side right
    pack $d.motioncmd.lab -side left
    pack $d.motioncmd.val -side right
    pack $d.motionnum.lab -side left
    pack $d.motionnum.val -side right
    pack $d.motionstatus.lab -side left
    pack $d.motionstatus.val -side right
    grid $d.task $d.io -sticky ew -padx 4 -pady 4
    grid $d.taskhb $d.iohb -sticky ew -padx 4
    grid $d.taskcmd $d.iocmd -sticky ew -padx 4
    grid $d.tasknum $d.ionum -sticky ew -padx 4
    grid $d.taskstatus $d.iostatus -sticky ew -padx 4
    grid $d.motion  -sticky ew -padx 4 -pady 4
    grid $d.motionhb  -sticky ew -padx 4
    grid $d.motioncmd -sticky ew -padx 4
    grid $d.motionnum -sticky ew -padx 4
    grid $d.motionstatus -sticky ew -padx 4
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

set limoridebutton [button $setframe.button -text "override limits" -command {toggleLimitOverride} -takefocus 0]
set limoridebuttonbg [$limoridebutton cget -background]
set limoridebuttonabg [$limoridebutton cget -activebackground]
# bind $limoridebutton <ButtonPress-1> {emc_override_limit}

set homebutton [button $setframe.home -text "home" -takefocus 0]
bind $homebutton <ButtonPress-1> {emc_home [ lindex $axiscoordmap $activeAxis]}

set offsetsetting ""

grid $limoridebutton -column 0 -row 0 -sticky nsew
grid $homebutton -column 0 -row 3 -sticky nsew


# ----------MANUAL MODE WIDGETS----------

# read the default jog speed
set temp [emc_ini "DEFAULT_VELOCITY" "TRAJ"]
if {[string length $temp] == 0} {
    set temp 1
}
set jogSpeed [int [expr $temp * 60 + 0.5]]
set defaultJogSpeed $jogSpeed

set maxJogSpeed 1

set feeddefault [button $manframe.default -text "DEFAULT" -width 10 -command {set jogSpeed $defaultJogSpeed}]
set feedlabel [label $manframe.label -text "Speed:" -width 10]
set feedvalue [label $manframe.value -textvariable jogSpeed -width 10 -anchor w]
set feedspace [label $manframe.space -text "" -width 10 ]
set feedrapid [button $manframe.rapid -text "RAPID" -width 10 -command {set jogSpeed $maxJogSpeed}]
set feedscale [scale $manframe.scale -length 270 -from 0 -to $maxJogSpeed -variable jogSpeed -orient horizontal -showvalue 0 -takefocus 0]
bind $feedlabel <ButtonPress-1> {popupJogSpeed}
bind $feedlabel <ButtonPress-3> {popupJogSpeed}
bind $feedvalue <ButtonPress-1> {popupJogSpeed}
bind $feedvalue <ButtonPress-3> {popupJogSpeed}


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

set jogplustext "JOG X +"
set jognegtext "JOG X -"

set jognegbutton [button $manframe.neg -textvariable jognegtext -width 10 -takefocus 0]
set jogposbutton [button $manframe.pos -textvariable jogplustext -width 10 -takefocus 0]
set jogincrement [button $manframe.incr -text "increment" -takefocus 0 \
    -width 8 -command {setJogType increment}]
set jogcontinuous [button $manframe.cont -text "continuous" -takefocus 0 \
    -width 8 -command {setJogType continuous}]
set joebutton [button $manframe.bzero -text "A\nL\nL\n\nZ\nE\nR\nO" \
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

grid configure $iframe -column 6 -row 0 -rowspan 5 -sticky nsew

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


# ----------SPINDLE CONTROL WIDGETS----------
# popped in with full right manual mode

set sdoing [label $spindframe.label1 -textvariable spindlelabel -relief flat \
        -width 20]
set sforbutton [button $spindframe.forward -text "Spindle Forward" -command {emc_spindle forward}]
set srevbutton [button $spindframe.reverse -text "Spindle Reverse" -command {emc_spindle reverse}]
set sstopbutton [button $spindframe.stop -text "Spindle off" -command {emc_spindle off}]

set decrbutton [button $spindframe.decr -text "Spindle Slower" -takefocus 0]
bind $decrbutton <ButtonPress-1> {emc_spindle decrease}
bind $decrbutton <ButtonRelease-1> {emc_spindle constant}

set incrbutton [button $spindframe.incr -text "Spindle Faster" -takefocus 0]
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
set mdilabel [label $mdiframe.label -text "MDI:" -width 4 -anchor w]
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
set programopenbutton [button $programframe.open -text "Open..." -width 7 \
    -command {fileDialog} -takefocus 0]
set programrunbutton [button $programframe.run -text "Run" -width 7\
    -command {emc_run $runMark ; set runMark 0} -takefocus 0]
set programpausebutton [button $programframe.pause -text "Pause" -width 7 \
    -command {emc_pause} -takefocus 0]
set programresumebutton [button $programframe.resume -text "Resume"  -width 7 \
    -command {emc_resume} -takefocus 0]
set programstepbutton [button $programframe.step -text "Step"  -width 7 \
    -command {emc_step} -takefocus 0]
set programverifybutton [button $programframe.verify -text "Verify"  -width 7 \
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

set activelabel [label $programrestart.l1 -text "RESTART LINE" ]
set activedecr [button $programrestart.b1 -text "Back" -command {incr restartline -1} ]
set activeincr [button $programrestart.b2 -text "Ahead" -command {incr restartline 1} ]
set activerestart [button $programrestart.b3 -text "Restart" -command {emc_run $restartline ; hideRestart} ]
pack $activelabel $activedecr $activeincr $activerestart -side top -fill both -expand yes

pack $programfiletext  -side left -fill y
pack $programrestart -side left -fill both -expand yes
pack $programframe $programfileframe -side top -anchor w -fill both -expand yes

proc fileDialog {} {
    global programDirectory programnamestring
    set types {
        {"All files" *}
        {"Text files" {.txt}}
        {"NC files" {.nc .ngc}}
    }
    set f [tk_getOpenFile -filetypes $types -initialdir $programDirectory]
    if {[string len $f] > 0} {
        set programDirectory [file dirname $f]
        set programnamestring $f
       loadProgramText
       loadProgram
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
        set tempx [ tk_dialog .d1 ???? "The interpreter is running. \n\
            Pressing OK will abort and load the new program" \
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
    if {[catch {open $programnamestring} programin]} {
        puts stdout "can't open $programnamestring"
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
set activeAxis 0
set minusAxis -1
set equalAxis -1
set syncingFeedOverride 0

# force explicit updates, so calls to emc_estop, for example, don't
# always cause an NML read; they just return last latched status
emc_update none

# Set up the display for manual mode, which may be reset
# immediately in updateStatus if it's not correct.
# This lets us check if there's a change and reset bindings, etc.
# on transitions instead of every cycle.
set modelabel "MANUAL"

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
    global taskhb taskcmd tasknum taskstatus
    global iohb iocmd ionum iostatus
    global motionhb motioncmd motionnum motionstatus
    global oldstatusstring jogSpeed
    global axiscoordmap
    global popinframe feedholdbutton stopbutton feedtext
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
    catch {set unitsetting [emc_display_linear_units]}
    if {$oldunitsetting != $unitsetting} {
        set oldunitsetting $unitsetting
    }

    if {[emc_estop] == "on"} {
        set estoplabel "ESTOPPED"
        $stopbutton configure -bg gray -fg black -relief sunken
    } elseif {[emc_machine] == "on"} {
        set estoplabel "ESTOP PUSH"
        $stopbutton configure -bg red -fg black -relief raised
    } else {
        set estoplabel "ESTOP RESET"
        $stopbutton configure -bg green -fg black -relief sunken
    }

    if {[emc_spindle] == "forward"} {
        set spindlelabel "SPINDLE FORWARD"
    } elseif {[emc_spindle] == "reverse"} {
        set spindlelabel "SPINDLE REVERSE"
    } elseif {[emc_spindle] == "off"} {
        set spindlelabel "SPINDLE OFF"
    } elseif {[emc_spindle] == "increase"} {
        set spindlelabel "SPINDLE INCREASE"
    } elseif {[emc_spindle] == "decrease"} {
        set spindlelabel "SPINDLE DECREASE"
    } else {
        set spindlelabel "SPINDLE ?"
    }

    if {[emc_brake] == "on"} {
        set brakelabel "BRAKE ON"
    } elseif {[emc_brake] == "off"} {
        set brakelabel "BRAKE OFF"
    } else {
        set brakelabel "BRAKE ?"
    }

    if {[emc_mist] == "on"} {
        set mistlabel "MIST ON"
    } elseif {[emc_mist] == "off"} {
        set mistlabel "MIST OFF"
    } else {
        set mistlabel "MIST ?"
    }

    if {[emc_flood] == "on"} {
        set floodlabel "FLOOD ON"
    } elseif {[emc_flood] == "off"} {
        set floodlabel "FLOOD OFF"
    } else {
        set floodlabel "FLOOD ?"
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
        if {[emc_joint_limit ${axnum} ] != "ok"} {
            [set "pos${axnum}d"] config -foreground red
        } elseif {[emc_joint_homed ${axnum} ] == "homed"} {
            [set "pos${axnum}d"] config -foreground darkgreen
        } else {
            [set "pos${axnum}d"] config -foreground gold3
        }
    }

    # set the feed override
    set realfeedoverride [emc_feed_override]
    if {$realfeedoverride == 0} {
            $feedholdbutton configure -bg green -activebackground green  -fg black -activeforeground darkgray
            set feedtext "CONTINUE"
    } else {
            $feedholdbutton configure -bg red -activebackground red -fg black -activeforeground white
             set feedtext "FEEDHOLD"
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
    if {[winfo exists $popinframe.plot]} {
        updatePlot
    }

   # get diagnostics info
    if {[winfo exists $popinframe.diagnostics]} {
        set taskhb [emc_task_heartbeat]
        set taskcmd [emc_task_command]
        set tasknum [emc_task_command_number]
        set taskstatus [emc_task_command_status]
        set iohb [emc_io_heartbeat]
        set iocmd [emc_io_command]
        set ionum [emc_io_command_number]
        set iostatus [emc_io_command_status]
        set motionhb [emc_motion_heartbeat]
        set motioncmd [emc_motion_command]
        set motionnum [emc_motion_command_number]
        set motionstatus [emc_motion_command_status]
    }

# schedule this again
    after $displayCycleTime updateMini
}

updateMini

#setup the initial conditions of the display here
rightConfig split

proc checkIt {} {
    global activeAxis
    mText " Return from ini [emc_ini IO_8255_ADDRESS EMCIO] "
}
