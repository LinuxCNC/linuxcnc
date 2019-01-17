#!/bin/sh
# the next line restarts using emcsh \
exec ${LINUXCNC_EMCSH-emcsh} "$0" "$@"
###############################################################
# Description:  tkemc
#               A Tcl/Tk GUI script that defines default  
#               emc interface functionality. 
#
#  Derived from a work by Fred Proctor & Will Shackleford
#  Author: 
#  License: GPL Version 2
#
#  Copyright (c) 2006-2009 All rights reserved.
###############################################################
# Uses the emcsh library of tcl/tk functions.
# Provides default display or operators for each NML message
###############################################################


# Load the emc package, which defines variables for various useful paths
package require Linuxcnc
eval emc_init $argv

# constants:
set ::prog [file tail $::argv0] ;# program name
set ::JOGJOINT  1               ;# joint jog
set ::JOGTELEOP 0               ;# teleop jog
set ::KINEMATICS_IDENTITY 1     ;# src/emc/kinematics/kinematics.h
set ::MAX_JOINTS 9              ;# src/emc/motion/emcmotcfg.h


# Tk GUI for the Enhanced Machine Controller

# Notes

# 1. Widget focus. Keyboard traversal with TAB sets the focus to some of the
# widgets. When they're focused, arrow keys and numbers don't work. To fix
# this, most widgets that would get focus have the focus disabled with
# -takefocus 0. A few remain; these are added to the widgets that bindtags
# applies to when switching between ManualBindings, AutoBindings

# 2. Adds tkbackplot and scripts to menu.
# 3. Adds info to help menu that calls tcl.sysinfo.tcl and uses emc/emcversion

# Set global that signifies we're the tkemc, so that other sourced scripts
# can know how to pack themselves. This lets these scripts pack into the
# main window "." if "tkemc" is not defined, or in their own top-level window
# if "tkemc" is defined. emclog.tcl looks at this.
set tkemc 1

# Internationalisation (i18n)
# in order to use i18n, all the strings will be called [msgcat::mc "string-foo"]
# instead of "string-foo".
# Thus msgcat searches for a translation of the string, and in case one isn't 
# found, the original string is used.
# In order to properly use locale's the env variable LANG is queried.
# If LANG is defined, then the folder src/po is searched for files
# called *.msg, (e.g. en_US.msg).
package require msgcat
if ([info exists env(LANG)]) {
    msgcat::mclocale
    msgcat::mcload $linuxcnc::LANG_DIR
}

# read the application defaults
set TKEMCCONF $linuxcnc::TCL_DIR/TkLinuxCNC
if {[file exists $TKEMCCONF]} {
    option readfile $TKEMCCONF startupFile
}
if {[package vsatisfies [package require Tk] 8.5.0]} {
    option add *Checkbutton.selectColor blue startupFile
    option add *Radiobutton.selectColor blue startupFile
}
foreach f {TkLinuxCNC /usr/X11R6/lib/X11/app-defaults/TkLinuxCNC /etc/X11/app-defaults/TkLinuxCNC} {
    if {[file exists $f]} {
	option readfile $f
	break
    }
}

# test for windows flag
set windows 0

foreach arg $argv {
    if { $arg == "-win" } {
	set windows 1
    }
}

# Read the ini file to determine what the axes and coordinates are.

set ::worldlabellist "X Y Z A B C U V W"
set ::numjoints [emc_ini "JOINTS" "KINS"]
if {$::numjoints == 0} {
  puts "$::prog: \[KINS\]JOINTS is missing or 0"
  exit 1
}

set ::coordnames [ emc_ini "COORDINATES" "TRAJ" ]
if {[string first " " $::coordnames] < 0 } {
  # split to acommodate "XYZ" style (as well as {X Y Z} style)
  set ::coordnames [split $::coordnames ""]
}

for {set idx 0} {$idx < $::MAX_JOINTS} {incr idx} {
   set worldlabel$idx [lindex  $::worldlabellist $idx ]
   set jointlabel$idx "$idx"
}

# set the cycle time for display updating
set temp [emc_ini "CYCLE_TIME" "DISPLAY"]
if {[string length $temp] == 0} {
    set temp 0.200
}
set displayCycleTime [expr {int($temp * 1000 + 0.5)}]
set syncDelayTime    $displayCycleTime ;#increasing improves slider behavior

# set the debounce time for key repeats
set debounceTime 150

if { $windows == 0 } {
    # load the generic editor "startEditor <name>"
    source $linuxcnc::TCL_BIN_DIR/genedit.tcl

    # load the EMC calibrator
#    source $linuxcnc::TCL_BIN_DIR/emccalib.tcl

    # load the EMC data logger
#    source $linuxcnc::TCL_BIN_DIR/emclog.tcl

    # load the EMC performance tester
    source $linuxcnc::TCL_BIN_DIR/emctesting.tcl

    # load the EMC debug level setter
    source $linuxcnc::TCL_BIN_DIR/emcdebug.tcl

    # load the backplotter
    source $linuxcnc::TCL_BIN_DIR/tkbackplot.tcl

    # load balloon help
    source $linuxcnc::TCL_SCRIPT_DIR/balloon.tcl
    source $linuxcnc::TCL_SCRIPT_DIR/emchelp.tcl
    set do_balloons [emc_ini "BALLOON_HELP" "DISPLAY"]
    if {$do_balloons == ""} {
        set do_balloons 0
    }
}

# the line from which to run the program; 0 or 1 means from start
set runMark 0
proc popupRunMark {mark} {
    global runMark
    catch {destroy .runmark}
    toplevel .runmark
    wm title .runmark [msgcat::mc "Set Run Mark"]
    label .runmark.msg -font {Helvetica 12 bold} -wraplength 4i -justify left \
        -text [msgcat::mc "Set run mark at line %s?" $mark]
    pack .runmark.msg -side top
    frame .runmark.buttons
    pack .runmark.buttons -side bottom -fill x -pady 2m
    button .runmark.buttons.ok -text [msgcat::mc "OK"] -default active \
        -command "set runMark $mark; destroy .runmark"
    button .runmark.buttons.cancel -text [msgcat::mc "Cancel"] \
        -command "destroy .runmark"
    pack .runmark.buttons.ok .runmark.buttons.cancel -side left -expand 1
    bind .runmark <Return> "set runMark $mark; destroy .runmark"
}

# pop up the program editor
proc popupProgramEditor {} {
    global programnamestring

    # create an editor as top-level ".programEditor"
    if {[file isfile $programnamestring]} {
        geneditStart programEditor $programnamestring
    } else {
        geneditStart programEditor [msgcat::mc "untitled"].ngc
    }

    frame .programEditor.buttons
    pack .programEditor.buttons -side bottom -fill x -pady 2m
    button .programEditor.buttons.mark -text [msgcat::mc "Set Run Mark"] -command \
        {popupRunMark [int [.programEditor.textframe.textwin index insert]]}
    pack .programEditor.buttons.mark -side left -expand 1
}

set toolfilename [emc_ini "TOOL_TABLE" "EMCIO"]
if {[string length $toolfilename] == 0} {
    set toolfilename "emc.tbl"
}

set paramfilename [emc_ini "PARAMETER_FILE" "RS274NGC"]
if {[string length $paramfilename] == 0} {
    set paramfilename "emc.var"
}

# pop up the parameter file editor
proc popupParamEditor {} {
    global paramfilename

    # create an editor as top-level ".paramEditor"
    if {[file isfile $paramfilename]} {
        geneditStart paramEditor $paramfilename
    } else {
        geneditStart paramEditor [msgcat::mc "untitled"].var
    }

    # disable "Save As..." menu, since we don't support changing it
    .paramEditor.menubar.file entryconfigure 3 -state disabled

    frame .paramEditor.buttons
    pack .paramEditor.buttons -side bottom -fill x -pady 2m
    button .paramEditor.buttons.load -text [msgcat::mc "Load Parameter File"] -command {geneditSaveFile paramEditor; emc_task_plan_init}
    button .paramEditor.buttons.cancel -text [msgcat::mc "Cancel"] -default active -command {destroy .paramEditor}
    pack .paramEditor.buttons.load .paramEditor.buttons.cancel -side left -expand 1
}

set helpfilename [emc_ini "HELP_FILE" "DISPLAY"]
if {[string length $helpfilename] == 0} {
    set helpfilename "tklinucnc.txt"
}

# pop up the help window
proc popupHelp {} {
    global helpfilename
    global HELPDIR

    # create an editor as top-level ".help"
    if {[file isfile $linuxcnc::HELP_DIR/$helpfilename]} {
        geneditStart help $linuxcnc::HELP_DIR/$helpfilename
    } else {
        geneditStart help $linuxcnc::HELP_DIR/tklinucnc.txt
    }

    # disable menu entries that don't apply to read-only text
    .help.menubar.file entryconfigure 0 -state disabled
    .help.menubar.file entryconfigure 1 -state disabled
    .help.menubar.file entryconfigure 2 -state disabled
    .help.menubar.file entryconfigure 3 -state disabled
    .help.menubar.edit entryconfigure 0 -state disabled
    .help.menubar.edit entryconfigure 2 -state disabled
    .help.textframe.textwin configure -state disabled

    frame .help.buttons
    pack .help.buttons -side bottom -fill x -pady 2m
    button .help.buttons.ok -text [msgcat::mc "OK"] -default active -command {destroy .help}
    pack .help.buttons.ok -side left -expand 1
    bind .help <Return> {destroy .help}
}

# pop up the diagnostics window
proc popupDiagnostics {} {
    set d .diagnostics

    if {[winfo exists $d]} {
        wm deiconify $d
        raise $d
        focus $d
        return
    }
    toplevel $d
    wm title $d [msgcat::mc "LinuxCNC Diagnostics"]

    label $d.task -text [msgcat::mc "Task"] -width 30 -anchor center
    frame $d.taskhb
    label $d.taskhb.lab -text [msgcat::mc "Heartbeat:"] -anchor w
    label $d.taskhb.val -textvariable taskhb -anchor e
    frame $d.taskcmd
    label $d.taskcmd.lab -text [msgcat::mc "Command:"] -anchor w
    label $d.taskcmd.val -textvariable taskcmd -anchor e
    frame $d.tasknum
    label $d.tasknum.lab -text [msgcat::mc "Command #:"] -anchor w
    label $d.tasknum.val -textvariable tasknum -anchor e
    frame $d.taskstatus
    label $d.taskstatus.lab -text [msgcat::mc "Status:"] -anchor w
    label $d.taskstatus.val -textvariable taskstatus -anchor e
    pack $d.task $d.taskhb $d.taskcmd $d.tasknum $d.taskstatus -side top -fill x
    pack $d.taskhb.lab -side left
    pack $d.taskhb.val -side right
    pack $d.taskcmd.lab -side left
    pack $d.taskcmd.val -side right
    pack $d.tasknum.lab -side left
    pack $d.tasknum.val -side right
    pack $d.taskstatus.lab -side left
    pack $d.taskstatus.val -side right

    label $d.io -text [msgcat::mc "Io"] -width 30 -anchor center
    frame $d.iohb
    label $d.iohb.lab -text [msgcat::mc "Heartbeat:"] -anchor w
    label $d.iohb.val -textvariable iohb -anchor e
    frame $d.iocmd
    label $d.iocmd.lab -text [msgcat::mc "Command:"] -anchor w
    label $d.iocmd.val -textvariable iocmd -anchor e
    frame $d.ionum
    label $d.ionum.lab -text [msgcat::mc "Command #:"] -anchor w
    label $d.ionum.val -textvariable ionum -anchor e
    frame $d.iostatus
    label $d.iostatus.lab -text [msgcat::mc "Status:"] -anchor w
    label $d.iostatus.val -textvariable iostatus -anchor e
    pack $d.io $d.iohb $d.iocmd $d.ionum $d.iostatus -side top -fill x
    pack $d.iohb.lab -side left
    pack $d.iohb.val -side right
    pack $d.iocmd.lab -side left
    pack $d.iocmd.val -side right
    pack $d.ionum.lab -side left
    pack $d.ionum.val -side right
    pack $d.iostatus.lab -side left
    pack $d.iostatus.val -side right

    label $d.motion -text [msgcat::mc "Motion"] -width 30 -anchor center
    frame $d.motionhb
    label $d.motionhb.lab -text [msgcat::mc "Heartbeat:"] -anchor w
    label $d.motionhb.val -textvariable motionhb -anchor e
    frame $d.motioncmd
    label $d.motioncmd.lab -text [msgcat::mc "Command:"] -anchor w
    label $d.motioncmd.val -textvariable motioncmd -anchor e
    frame $d.motionnum
    label $d.motionnum.lab -text [msgcat::mc "Command #:"] -anchor w
    label $d.motionnum.val -textvariable motionnum -anchor e
    frame $d.motionstatus
    label $d.motionstatus.lab -text [msgcat::mc "Status:"] -anchor w
    label $d.motionstatus.val -textvariable motionstatus -anchor e
    pack $d.motion $d.motionhb $d.motioncmd $d.motionnum $d.motionstatus -side top -fill x
    pack $d.motionhb.lab -side left
    pack $d.motionhb.val -side right
    pack $d.motioncmd.lab -side left
    pack $d.motioncmd.val -side right
    pack $d.motionnum.lab -side left
    pack $d.motionnum.val -side right
    pack $d.motionstatus.lab -side left
    pack $d.motionstatus.val -side right

    frame $d.buttons
    button $d.buttons.ok -text [msgcat::mc "OK"] -default active -command "destroy $d"

    pack $d.buttons -side bottom -fill x -pady 2m
    pack $d.buttons.ok -side left -expand 1
    bind $d <Return> "destroy $d"
}

# pop up the about box
proc popupAbout {} {
    global env
    if {[winfo exists .about]} {
        wm deiconify .about
        raise .about
        focus .about
        return
    }
    toplevel .about
    wm title .about [msgcat::mc "About TkLinuxCNC"]
    message .about.msg -aspect 1000 -justify center -font {Helvetica 12 bold} -text [ format "%s\n(LinuxCNC %s)" [msgcat::mc "TkLinuxcnc\n\nTcl/Tk GUI for LinuxCNC\n\nGPL Version 2 (2012)"] $env(LINUXCNCVERSION) ]
    frame .about.buttons
    button .about.buttons.ok -default active -text [msgcat::mc "OK"] -command "destroy .about"
    pack .about.msg -side top
    pack .about.buttons -side bottom -fill x -pady 2m
    pack .about.buttons.ok -side left -expand 1
    bind .about <Return> "destroy .about"
}

proc toolenter {} {
  #G10 L1 P[tool number]
  #       R[radius]
  #       X[offset] Y[offset] Z[offset]
  #       A[offset] B[offset] C[offset]
  #       U[offset] V[offset] W[offset]
  #       I[frontangle] J[backangle] Q[orientation]
  set cmd ""
  foreach item "$::coordnames" {
    set tag   $item ;# X|Y|...
    set value [string trim $::tentry($item)]
    switch $item {
      Diam    {set tag R}
      Front   {set tag I}
      Back    {set tag J}
      Orient  {set tag Q}
    }
    if {"$item" == "Diam" && "$value" != ""} {
      set value [expr $::tentry(Diam)/2.0]
    }
    if {"$value" != ""} { set cmd "$cmd $tag$value"}
  }
  if {"$cmd" == ""} return
  set restore [emc_mode]
  emc_mode mdi
  emc_mdi "G10 L1 P$::tentry(toolno) $cmd"
  emc_mdi G43
  emc_mode $restore
}

proc popupToolOffset {} {

    if {[winfo exists .tooloffset]} {
        wm deiconify .tooloffset
        raise .tooloffset
        focus .tooloffset
        return
    }
    toplevel .tooloffset
    wm title .tooloffset [msgcat::mc "Set Tool Offset"]

    # pre-load the entries with values

    frame .tooloffset.tool
    label .tooloffset.tool.label -text [msgcat::mc "Tool:"] -anchor e -width 8
    entry .tooloffset.tool.entry -textvariable ::tentry(toolno) -width 10
    pack .tooloffset.tool -side top
    pack .tooloffset.tool.label .tooloffset.tool.entry -side left

    foreach item "$::coordnames" {
      set witem [string tolower $item]
      frame .tooloffset.$witem
      label .tooloffset.$witem.label -text [msgcat::mc "$item:"] -anchor e -width 8
      entry .tooloffset.$witem.entry -textvariable ::tentry($item) -width 10
      pack  .tooloffset.$witem -side top
      pack  .tooloffset.$witem.label .tooloffset.$witem.entry -side left
    }

    frame .tooloffset.buttons
    button .tooloffset.buttons.ok -text [msgcat::mc "OK"] -default active -command {toolenter; destroy .tooloffset}
    button .tooloffset.buttons.cancel -text [msgcat::mc "Cancel"] -command {destroy .tooloffset}

    pack .tooloffset.buttons -side bottom -fill x -pady 2m
    pack .tooloffset.buttons.ok .tooloffset.buttons.cancel -side left -expand 1
    bind .tooloffset <Return> {toolenter; destroy .tooloffset}

}

# set waiting on EMC to wait until command is received, not finished, with
# timeout of 10 seconds
emc_set_timeout 1.0
emc_set_wait received

# force an update
emc_update

set modifier Alt
set modifierstring "Alt"

set programDirectory [emc_ini "PROGRAM_PREFIX" "DISPLAY"]

# process to load the program text display widget
proc loadProgramText {} {
    global programnamestring programfiletext
    # clear out program text
    $programfiletext config -state normal
    $programfiletext delete 1.0 end
    # close the current program
    catch {close $programin}
    # open the new program, if it's not "none"
    if {[catch {open $programnamestring} programin]} {
        puts stdout [msgcat::mc "can't open %s" $programnamestring]
    } else {
        $programfiletext insert end [read $programin]
        $programfiletext config -state disabled
        catch {close $programin}
    }
}

# set initial value for program status comparison
set oldstatusstring "idle"

proc fileDialog {} {
    global programDirectory
    global programnamestring
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
        emc_mode auto
        emc_abort
        emc_open $programnamestring
        loadProgramText
    }
}

proc toggleEstop {} {
    if {[emc_estop] == "on"} {
        emc_estop off
    } else {
        emc_estop on
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

proc toggleSpindleForward {} {
    if {[emc_spindle] == "off"} {
        emc_spindle forward
    } else {
        emc_spindle off
    }
}

proc toggleSpindleReverse {} {
    if {[emc_spindle] == "off"} {
        emc_spindle reverse
    } else {
        emc_spindle off
    }
}

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

proc toggleRelAbs {} {
    global coords
    if {$coords == "relative"} {
        set coords machine
    } else {
        set coords relative
    }
}

proc toggleCmdAct {} {
    global actcmd
    if {$actcmd == "actual"} {
        set actcmd commanded
    } else {
        set actcmd actual
    }
}

set ::jointworld "joint"

proc toggleJointWorld {} {
    if { [emc_kinematics_type] == $::KINEMATICS_IDENTITY} {
        return
    }
    if {$::jointworld == "joint"} {
        set ::jointworld world
        emc_teleop_enable 1
    } else {
        set ::jointworld joint
        emc_teleop_enable 0
    }
    emc_wait done
}

proc set_jorw {jorw} {
    switch $jorw {
        world   {
          set ::jointworld world
          if {     [emc_kinematics_type] != $::KINEMATICS_IDENTITY
              && ! [emc_teleop_enable]
             } {
               emc_teleop_enable 1
             }
        }
        joint   {
          set ::jointworld joint
          if {   [emc_kinematics_type] != $::KINEMATICS_IDENTITY
              && [emc_teleop_enable]
             } {
               emc_teleop_enable 0
             }
        }
        default {puts "set_jorw: unknown <$jorw>"}
    }
}

proc toggleJogType {} {
    global jogLabel jogType jogIncrement

    if {$jogType == "continuous"} {
        set jogType $jogIncrement
	set jogLabel $jogIncrement
    } else {
        set jogType continuous
	set jogLabel [msgcat::mc "continuous"]
    }
}

proc toggleJogIncrement {} {
    global jogLabel jogType jogIncrement

    if {$jogType == "continuous"} {
        set jogType $jogIncrement
	set jogLabel $jogIncrement
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
	set jogLabel $jogIncrement
    }
}

proc jogNeg {jora} {
    set ::activeJA $jora
    switch $::jogJointType($jora) {
      linear  {set speed $::linearJogSpeed}
      angular {set speed $::angularJogSpeed}
      undefined {puts "undefined JogJointType for $jora";return}
    }
    if { [emc_teleop_enable] == 1 } {
        set letter  [set ::poslabel$jora]
	set jatojog [lsearch $::worldlabellist $letter]
        set jogmode $::JOGTELEOP
    } else {
	set jatojog $jora
        set jogmode $::JOGJOINT
    }
    if {$::jogType == "continuous"} {
        emc_jog      $jatojog $jogmode -$speed
    } else {
        emc_jog_incr $jatojog $jogmode -$speed $::jogIncrement
    }
}

proc jogPos {jora} {
    set ::activeJA $jora
    switch $::jogJointType($jora) {
      linear  {set speed $::linearJogSpeed}
      angular {set speed $::angularJogSpeed}
      undefined {puts "undefined JogJointType for $jora";return}
    }
    if { [emc_teleop_enable] == 1 } {
        set letter  [set ::poslabel$jora]
        set jatojog [lsearch $::worldlabellist $letter]
        set jogmode $::JOGTELEOP
    } else {
	set jatojog $jora
        set jogmode $::JOGJOINT
    }
    if {$::jogType == "continuous"} {
        emc_jog      $jatojog $jogmode $speed
    } else {
        emc_jog_incr $jatojog $jogmode $speed $::jogIncrement
    }
}

proc jogStop {jora} {
    if { [emc_teleop_enable] == 1 } {
        set jogmode $::JOGTELEOP
        set letter [set ::poslabel$jora]
        set jatostop  [lsearch $::worldlabellist $letter]
    } else {
        set jogmode $::JOGJOINT
        set jatostop  $jora
    }
    # only stop continuous jogs; let incremental jogs finish
    if {$::jogType == "continuous"} {
        emc_jog_stop $jatostop $jogmode
    }
}

# toggles the limit override state
proc toggleLimitOverride {} {
    if {[emc_override_limit]} {
        emc_override_limit 0
    } else {
        emc_override_limit 1
    }
}

# toggles the optional stop
proc toggleOptionalStop {} {
    if {[emc_optional_stop]} {
        emc_optional_stop 0
    } else {
        emc_optional_stop 1
    }
}


# use the top-level window as our top-level window, and name it
set title [emc_ini "MACHINE" "EMC"]
wm title . $title

# create the main window top frame
set top [frame .top]
pack $top -side top -fill both -expand true

# build the top menu bar
set menubar [menu $top.menubar -tearoff 0]

# add the File menu
set filemenu [menu $menubar.file -tearoff 0]
$menubar add cascade -label [msgcat::mc "File"] -menu $filemenu -underline 0
$filemenu add command -label [msgcat::mc "Open..."] -command {fileDialog} -underline 0
bind . <$modifier-o> {fileDialog}
$filemenu add command -label [msgcat::mc "Edit..."] -command {popupProgramEditor} -underline 0
set tooleditor [emc_ini "TOOL_EDITOR" "DISPLAY"]
if {$tooleditor == ""} {
  set tooleditor tooledit
}
$filemenu add command -label [msgcat::mc "Tool Table Editor..."] \
                      -command "eval exec $tooleditor [file normalize $toolfilename] &" \
                      -underline 0
$filemenu add command -label [msgcat::mc "Reload Tool Table"] \
                      -command "emc_load_tool_table [file normalize $toolfilename]"
$filemenu add command -label [msgcat::mc "Reset"] -command {emc_task_plan_init} -underline 0
$filemenu add separator
$filemenu add command -label [msgcat::mc "Exit"] -command {after cancel updateStatus ; destroy . ; exit} -accelerator $modifierstring+X -underline 1
bind . <$modifier-x> {after cancel updateStatus ; destroy . ; exit}

# add the View menu
set viewmenu [menu $menubar.view -tearoff 0]
$menubar add cascade -label [msgcat::mc "View"] -menu $viewmenu -underline 0
$viewmenu add command -label [msgcat::mc "Offsets and Variables..."] -command {popupParamEditor} -underline 0
$viewmenu add command -label [msgcat::mc "Diagnostics..."] -command {popupDiagnostics} -underline 0
$viewmenu add command -label [msgcat::mc "Backplot..."] -command {popupPlot} -underline 0

# add the Settings menu
set settingsmenu [menu $menubar.settings -tearoff 0]
$menubar add cascade -label [msgcat::mc "Settings"] -menu $settingsmenu -underline 0
$settingsmenu add command -label [msgcat::mc "Calibration..."] -command "exec $linuxcnc::TCL_BIN_DIR/emccalib.tcl -- -ini $EMC_INIFILE &"
$settingsmenu add command -label [msgcat::mc "Testing..."] -command {popupTesting} -underline 0 -state disabled
$settingsmenu add command -label [msgcat::mc "Debug..."] -command {popupDebug} -underline 0
$settingsmenu add command -label [msgcat::mc "Font..."] -command {popupFont} -underline 0

# add the Units menu
set unitsmenu [menu $menubar.units -tearoff 0]
$menubar add cascade -label [msgcat::mc "Units"] -menu $unitsmenu -underline 0
$unitsmenu add command -label [msgcat::mc "auto"] -command {emc_linear_unit_conversion auto} -underline 0
$unitsmenu add command -label [msgcat::mc "inches"] -command {emc_linear_unit_conversion inch} -underline 0
$unitsmenu add command -label [msgcat::mc "mm"] -command {emc_linear_unit_conversion mm} -underline 0
$unitsmenu add command -label [msgcat::mc "cm"] -command {emc_linear_unit_conversion cm} -underline 0

# add the Utilities menu
set utilsmenu [menu $menubar.utils -tearoff 1]
$menubar add cascade -label [msgcat::mc "Utilities"] -menu $utilsmenu -underline 1
$utilsmenu add command -label [msgcat::mc "Hal Scope"] -command {exec halscope -- -ini $EMC_INIFILE &}
$utilsmenu add command -label [msgcat::mc "Hal Meter"] -command {exec halmeter &}

# Add a scripts menu that looks for *.tcl files in tcl/scripts subdirectory
set scriptsmenu [menu $menubar.scripts -tearoff 1]
$menubar add cascade -label [msgcat::mc "Scripts"] -menu $scriptsmenu -underline 1

if { $windows == 0 } {
    set files [exec /bin/ls $linuxcnc::TCL_SCRIPT_DIR]
    foreach file $files {
	if {[string match *.tcl $file]} {
	    set fname [file rootname $file]
	    # if it's executable, arrange for direct execution
	    if {[file executable $linuxcnc::TCL_SCRIPT_DIR/$file]} {
		$scriptsmenu add command -label [msgcat::mc "Set Coordinates"] -command "exec $linuxcnc::TCL_SCRIPT_DIR/$file -- -ini $EMC_INIFILE &"
	    }
	}
    }
}

# add halconfig, to help for HAL setup, it's under Scripts, but it's in the TCL_BIN_DIR
$scriptsmenu add separator
$scriptsmenu add command -label [msgcat::mc "HAL Show"] -command "exec $linuxcnc::TCL_BIN_DIR/halshow.tcl &"
$scriptsmenu add command -label [msgcat::mc "HAL Config"] -command "exec $linuxcnc::TCL_BIN_DIR/halconfig.tcl -- -ini $EMC_INIFILE &"

# add the help menu
set helpmenu [menu $menubar.help -tearoff 0]
$menubar add cascade -label [msgcat::mc "Help"] -menu $helpmenu -underline 0
$helpmenu add command -label [msgcat::mc "Help..."] -command {popupHelp} -underline 0
$helpmenu add check -label [msgcat::mc "Balloon help"] -variable do_balloons
#$helpmenu add separator
#$helpmenu add command -label [msgcat::mc "Info..."] -command {catch {exec tcl/sysinfo.tcl -- -ini $EMC_INIFILE}} -underline 0
$helpmenu add separator
$helpmenu add command -label [msgcat::mc "About..."] -command {popupAbout} -underline 0

. configure -menu $menubar

set commandbuttons [frame $top.commandbuttons] ; # whole group
set commandbuttonsl1 [frame $commandbuttons.l1] ; # estop/mode
set commandbuttonsl2 [frame $commandbuttons.l2] ; # mist/flood
set commandbuttonsl3 [frame $commandbuttons.l3] ; # </spindle/>//brake
set commandbuttonsl3t1 [frame $commandbuttonsl3.t1] ; # </spindle/>

pack $commandbuttons -side top -fill both -expand true
pack $commandbuttonsl1 -side left -fill both -expand true
pack $commandbuttonsl2 -side left -fill both -expand true
pack $commandbuttonsl3 -side left -fill both -expand true
pack $commandbuttonsl3t1 -side top -fill both -expand true

set estopbutton [menubutton $commandbuttonsl1.estop -textvariable estoplabel -direction below -relief raised -width 15]
set estopmenu [menu $estopbutton.menu -tearoff 0]
$estopbutton configure -menu $estopmenu
$estopmenu add command -label [msgcat::mc "Estop on"] -command {emc_estop on}
$estopmenu add command -label [msgcat::mc "Estop off"] -command {emc_estop off}
$estopmenu add separator
$estopmenu add command -label [msgcat::mc "Machine on"] -command {emc_machine on}
$estopmenu add command -label [msgcat::mc "Machine off"] -command {emc_machine off}

pack $estopbutton -side top -fill both -expand true

set modebutton [menubutton $commandbuttonsl1.mode -textvariable modelabel -direction below -relief raised -width 15]
set modemenu [menu $modebutton.menu -tearoff 0]
$modebutton configure -menu $modemenu
$modemenu add command -label [msgcat::mc "Manual"] -command {emc_mode manual}
$modemenu add command -label [msgcat::mc "Auto"] -command {emc_mode auto}
$modemenu add command -label [msgcat::mc "MDI"] -command {emc_mode mdi}

pack $modebutton -side top -fill both -expand true

set mistbutton [menubutton $commandbuttonsl2.mist -textvariable mistlabel -direction below -relief raised -width 15]
set mistmenu [menu $mistbutton.menu -tearoff 0]
$mistbutton configure -menu $mistmenu
$mistmenu add command -label [msgcat::mc "Mist on"] -command {emc_mist on}
$mistmenu add command -label [msgcat::mc "Mist off"] -command {emc_mist off}

pack $mistbutton -side top -fill both -expand true

set floodbutton [menubutton $commandbuttonsl2.flood -textvariable floodlabel -direction below -relief raised -width 15]
set floodmenu [menu $floodbutton.menu -tearoff 0]
$floodbutton configure -menu $floodmenu
$floodmenu add command -label [msgcat::mc "Flood on"] -command {emc_flood on}
$floodmenu add command -label [msgcat::mc "Flood off"] -command {emc_flood off}

pack $floodbutton -side top -fill both -expand true

set lubebutton [menubutton $commandbuttonsl2.lube -textvariable lubelabel -direction below -relief raised -width 15]
set lubemenu [menu $lubebutton.menu -tearoff 0]
$lubebutton configure -menu $lubemenu
$lubemenu add command -label [msgcat::mc "Lube on"] -command {emc_lube on}
$lubemenu add command -label [msgcat::mc "Lube off"] -command {emc_lube off}

# FIXME-- there's probably a better place to put the lube button
# if {[emc_ini LUBE_WRITE_INDEX EMCIO] != ""} {
#     pack $lubebutton -side top -fill both -expand true
# }

set decrbutton [button $commandbuttonsl3t1.decr -text "<" -takefocus 0]

pack $decrbutton -side left -fill both -expand true

bind $decrbutton <ButtonPress-1> {emc_spindle decrease}
bind $decrbutton <ButtonRelease-1> {emc_spindle constant}

set spindlebutton [menubutton $commandbuttonsl3t1.spindle -textvariable spindlelabel -direction below -relief raised -width 15]
set spindlemenu [menu $spindlebutton.menu -tearoff 0]
$spindlebutton configure -menu $spindlemenu
$spindlemenu add command -label [msgcat::mc "Spindle forward"] -command {emc_spindle forward}
$spindlemenu add command -label [msgcat::mc "Spindle reverse"] -command {emc_spindle reverse}
$spindlemenu add command -label [msgcat::mc "Spindle off"] -command {emc_spindle off}

pack $spindlebutton -side left -fill both -expand true

set incrbutton [button $commandbuttonsl3t1.incr -text ">" -takefocus 0]

pack $incrbutton -side left -fill both -expand true

bind $incrbutton <ButtonPress-1> {emc_spindle increase}
bind $incrbutton <ButtonRelease-1> {emc_spindle constant}

set brakebutton [menubutton $commandbuttonsl3.brake -textvariable brakelabel -direction below -relief raised -width 25]
set brakemenu [menu $brakebutton.menu -tearoff 0]
$brakebutton configure -menu $brakemenu
$brakemenu add command -label [msgcat::mc "Brake on"] -command {emc_brake on}
$brakemenu add command -label [msgcat::mc "Brake off"] -command {emc_brake off}

pack $brakebutton -side top -fill both -expand true

set abortbutton [button $commandbuttons.abort -text [msgcat::mc "ABORT"] -width 15 -height 3 -takefocus 0]

pack $abortbutton -side left -fill both -expand true

bind $abortbutton <ButtonPress-1> {tkemc_abort}
pack $abortbutton -side top -fill both -expand true

set ::tentry(toolno) 0
set tooloffsetsetting "X0.0000 Y0.0000 Z0.0000"
set offsetsetting "X0.0000 Y0.0000 Z0.0000"
set unitsetting "custom"
set oldunitsetting $unitsetting

set settings [frame $top.settings]
pack $settings -side top -anchor w
set toollabel [label $settings.toollabel -text [msgcat::mc "Tool:"] -anchor w]
set toolsetting [label $settings.toolsetting -textvariable ::tentry(toolno) -width 4 -anchor w]
set tooloffsetlabel [label $settings.tooloffsetlabel -text [msgcat::mc "Offset:"] -anchor w]
set tooloffsetsetting [label $settings.tooloffsetsetting -textvariable tooloffsetsetting -width 30 -anchor w]
set unitlabel [label $settings.unitlabel -textvariable unitsetting -width 6 -anchor e]

set settings1 [frame $top.settings1]
pack $settings1 -side top -anchor w
set offsetlabel [label $settings1.offsetlabel -text [msgcat::mc "Work Offsets:"] -anchor w]
set offsetsetting [label $settings1.offsetsetting -textvariable offsetsetting -width 80 -anchor w]

pack $toollabel -side left -padx 1m -pady .3m
pack $toolsetting -side left -pady .3m
pack $tooloffsetlabel -side left  -pady .3m
pack $tooloffsetsetting -side left -pady .3m
pack $offsetlabel -side left -padx 1m
pack $offsetsetting -side left -padx 1m
pack $unitlabel -side left -padx 1m

# set up help for these
set_balloon $offsetlabel [offsethelp]
set_balloon $offsetsetting [offsethelp]

bind $tooloffsetlabel <ButtonPress-1> {popupToolOffset}
bind $tooloffsetsetting <ButtonPress-1> {popupToolOffset}
bind $tooloffsetlabel <ButtonPress-3> {popupToolOffset}
bind $tooloffsetsetting <ButtonPress-3> {popupToolOffset}


# process ini file settings for jog speed -- angular and linear
# use TRAJ settings only
set vitems "DEFAULT_VELOCITY DEFAULT_ANGULAR_VELOCITY DEFAULT_LINEAR_VELOCITY\
                MAX_ANGULAR_VELOCITY     MAX_LINEAR_VELOCITY"
foreach item $vitems {
  set temp [emc_ini "$item" "TRAJ"]
  if {"$temp" != ""} {set tempini($item) $temp}
}      
if [info exists tempini(DEFAULT_LINEAR_VELOCITY)] {
  set temp     $tempini(DEFAULT_LINEAR_VELOCITY)
} elseif [info exists tempini(DEFAULT_VELOCITY)] {
  set temp           $tempini(DEFAULT_VELOCITY)
} else {
  set temp 1
}
set ::linearJogSpeed [expr int($temp * 60 + 0.5)] ;# units/minute

if [info exists tempini(MAX_LINEAR_VELOCITY)] {
  set temp     $tempini(MAX_LINEAR_VELOCITY)
} elseif [info exists tempini(MAX_VELOCITY)] {
  set temp           $tempini(MAX_VELOCITY)
} else {
  set temp 1
}
set ::linearJogSpeedMax [expr int($temp * 60 + 0.5)] ;# units/minute

if [info exists tempini(DEFAULT_ANGULAR_VELOCITY)] {
  set temp     $tempini(DEFAULT_ANGULAR_VELOCITY)
  set ::angularJogSpeed [expr {int($temp * 60 + 0.5)}] ;# units/minute
}
# no angular jog slider if no DEFAULT_ANGULAR_VELOCITY
if [info exists ::angularJogSpeed] {
  if [info exists tempini(MAX_ANGULAR_VELOCITY)] {
    set temp     $tempini(MAX_ANGULAR_VELOCITY)
    set ::angularJogSpeedMax [expr {int($temp * 60 + 0.5)}] ;# units/minute
  } else {
    set ::angularJogSpeedMax $::angularJogSpeed
  }
}

for {set joint 0} {$joint < $::MAX_JOINTS} {incr joint} {
  set temp [emc_ini "TYPE" "JOINT_$joint"]
  switch $temp {
    LINEAR  {set ::jogJointType($joint) linear}
    ANGULAR {
      if [info exists ::angularJogSpeed] {
        set ::jogJointType($joint) angular
      } else {
        # insufficient information to jog:
        set ::jogJointType($joint) undefined
      }
    }
  }
}
catch {unset vitems tempini} ;# remove clutter

set jogType continuous
set jogIncrement 0.0010

set move [frame $top.move]
set position [frame $move.position] ;# frame holding joint/axis position frames
# make a frame for each joint/axis position
for {set idx 0} {$idx < $::MAX_JOINTS} {incr idx} {
   set pos$idx [frame $position.pos$idx -borderwidth 4]
}

set bun [frame $move.bun]
set limoride [frame $bun.limoride]
set coordsel [frame $bun.coordsel]
set relabssel [frame $coordsel.relabssel]
set actcmdsel [frame $coordsel.actcmdsel]
set jog [frame $bun.jog]
set dojog [frame $jog.dojog]
pack $move -side top -fill both -expand true
pack $position -side left

set jno 0; set jointcount 0
for {set idx 0} {$idx < $::MAX_JOINTS} {incr idx} {
  set letter [lindex $::worldlabellist $idx]
  #  0-->X, 1-->Y, ...
  if {[ string first $letter $::coordnames ] <0 } { continue }
  set positionframe [set pos$jno] ;# typ: pos0,pos1,...
  set worldlabel$jno $letter
  if {[emc_kinematics_type] == $::KINEMATICS_IDENTITY} {
    set poslabel$jno   $letter ;# identity, only show axis letter
  } else {
    set poslabel$jno   $jno ;# not identity, initially show joint no.
  }
  pack $positionframe -side top
  incr jointcount ;# controls font size
  incr jno
}

pack $bun -side right -anchor n ; # don't fill or expand these-- looks funny
pack $limoride -side top -pady 2m
pack $coordsel -side top -pady 2m
pack $relabssel -side left
pack $actcmdsel -side right
pack $jog -side top -pady 2m
# continuous jog button will be packed later
pack $dojog -side bottom

# labels for posNl: position poslabelN may be number (joint) or letter (axis)
#        and posNd: digits   posdigitN
for {set idx 0} {$idx < $::MAX_JOINTS} {incr idx} {
    set parent  [set pos${idx}]
    set pos${idx}l [label ${parent}.l -textvariable poslabel$idx -width 1 -anchor w]
    set pos${idx}d [label ${parent}.d -textvariable posdigit$idx -width 10 -anchor w]

    set wl [set pos${idx}l]
    set wd [set pos${idx}d]
    pack $wl -side left
    pack $wd -side right
    bind $wl <ButtonPress-1> [list jaSelect $idx]
    bind $wd <ButtonPress-1> [list jaSelect $idx]
    bind $wl <ButtonPress-3> "jaSelect $idx; popupAxisOffset $idx"
    bind $wd <ButtonPress-3> "jaSelect $idx; popupAxisOffset $idx"
}

# set the position display font and radio button variables to their
# ini file values, if present. Otherwise leave the font alone, as it
# comes from the X resource file, and don't bother parsing this for
# the radio buttons but set them to some typical value.

proc setfont {} {
    for {set idx 0} {$idx < $::MAX_JOINTS} {incr idx} {
      global pos${idx}l pos${idx}d
    }
    global fontfamily fontsize fontstyle

    set nf [list $fontfamily $fontsize $fontstyle]
    for {set idx 0} {$idx < $::MAX_JOINTS} {incr idx} {
      set pos_l [set pos${idx}l]
      set pos_d [set pos${idx}d]
      $pos_l config -font $nf
      $pos_d config -font $nf
    }
}

set userfont [emc_ini "POSITION_FONT" "DISPLAY"]
if {$userfont != ""} {
    set fontfamily [font actual $userfont -family]
    set fontsize [font actual $userfont -size]
    set fontstyle [font actual $userfont -weight]
} elseif {[lsearch [font families] {courier 10 pitch}] != -1} {
    set fontfamily {courier 10 pitch}
     if {$jointcount > 3} {
	set fontsize 24
    } else {
	set fontsize 48
    }
    set fontstyle bold
} else {
    set fontfamily courier
    if {$jointcount > 3} {
	set fontsize 24
    } else {
        set fontsize 48
    }
    set fontstyle bold
}
setfont

set limoridebutton [button $limoride.button -text [msgcat::mc "override limits"] -command {toggleLimitOverride} -takefocus 0]
pack $limoridebutton -side top
set limoridebuttonbg [$limoridebutton cget -background]
set limoridebuttonabg [$limoridebutton cget -activebackground]
bind $limoridebutton <ButtonPress-1> {emc_override_limit}

set radiorel [radiobutton $coordsel.rel -text [msgcat::mc "relative"] -variable coords -value relative -command {} -takefocus 0]
set radioabs [radiobutton $coordsel.abs -text [msgcat::mc "machine"] -variable coords -value machine -command {} -takefocus 0]

set radioact [radiobutton $coordsel.act -text [msgcat::mc "actual"] -variable actcmd -value actual -command {} -takefocus 0]
set radiocmd [radiobutton $coordsel.cmd -text [msgcat::mc "commanded"] -variable actcmd -value commanded -command {} -takefocus 0]

if {[emc_kinematics_type] != $::KINEMATICS_IDENTITY} {
  set radiojoint [radiobutton $coordsel.joint -text [msgcat::mc "joint"] -variable ::jointworld -value joint -command {set_jorw joint} -takefocus 0]
  set radioworld [radiobutton $coordsel.world -text [msgcat::mc "world"] -variable ::jointworld -value world -command {set_jorw world} -takefocus 0]
}

pack $radiorel -side top -anchor w
pack $radioabs -side top -anchor w
pack $radioact -side top -anchor w
pack $radiocmd -side top -anchor w
if {[emc_kinematics_type] != $::KINEMATICS_IDENTITY} {
  pack $radiojoint -side top -anchor w
  pack $radioworld -side top -anchor w
}

set jogLabel [msgcat::mc "continuous"]
set jogtypebutton [menubutton $jog.type -textvariable jogLabel -direction below -relief raised -width 16]
set jogtypemenu [menu $jogtypebutton.menu -tearoff 0]
$jogtypebutton configure -menu $jogtypemenu
$jogtypemenu add command -label [msgcat::mc "continuous"] -command {set jogType continuous ; set jogIncrement 0.0000 ; set jogLabel [msgcat::mc "continuous"]}

$jogtypemenu add command -label "0.0001" -command {set jogType 0.0001 ; set jogIncrement 0.0001 ; set jogLabel 0.0001}
$jogtypemenu add command -label "0.0010" -command {set jogType 0.0010 ; set jogIncrement 0.0010 ; set jogLabel 0.0010}
$jogtypemenu add command -label "0.0100" -command {set jogType 0.0100 ; set jogIncrement 0.0100 ; set jogLabel 0.0100}
$jogtypemenu add command -label "0.1000" -command {set jogType 0.1000 ; set jogIncrement 0.1000 ; set jogLabel 0.1000}
$jogtypemenu add command -label "1.0000" -command {set jogType 1.0000 ; set jogIncrement 1.0000 ; set jogLabel 1.0000}

pack $jogtypebutton -side top

set jognegbutton [button $dojog.neg -text "-" -takefocus 0]
set homebutton [button $dojog.home -text [msgcat::mc "home"] -takefocus 0]
set jogposbutton [button $dojog.pos -text "+" -takefocus 0]

pack $jognegbutton -side left
pack $homebutton -side left
pack $jogposbutton -side left

bind $jognegbutton <ButtonPress-1>   {jogNeg   $::activeJA}
bind $jognegbutton <ButtonRelease-1> {jogStop  $::activeJA}
bind $homebutton   <ButtonPress-1>   {emc_home $::activeJA}
bind $jogposbutton <ButtonPress-1>   {jogPos   $::activeJA}
bind $jogposbutton <ButtonRelease-1> {jogStop  $::activeJA}

proc jaSelect {ja} {
    for {set idx 0} {$idx < $::MAX_JOINTS} {incr idx} {
      global pos${idx}
    }

    for {set idx 0} {$idx < $::MAX_JOINTS} {incr idx} {
      set pos [set pos${idx}]
      $pos config -relief flat
      if {$ja == $idx} {
         $pos config -relief groove
      }
    }
    set ::activeJA $ja
}

# force first selected ja
jaSelect 0

proc setAxisOffset {axisnum offset} {
    set letter  [lindex $::coordnames $axisnum]
    set axisnum [lsearch $::worldlabellist $letter]
    set string [format "G92 %s%f\n" $letter $offset]

    set restore_mode [emc_mode]
    emc_mode mdi
    emc_mdi $string
    emc_mode $restore_mode
}

proc popupAxisOffset {axisnum} {
    global axisoffsettext

    if {[winfo exists .axisoffset]} {
        wm deiconify .axisoffset
        raise .axisoffset
        focus .axisoffset
        return
    }
    toplevel .axisoffset
    set letter  [lindex $::coordnames $axisnum]
    wm title .axisoffset [msgcat::mc "Axis Offset <$letter>"]
    frame .axisoffset.input
    label .axisoffset.input.label -text [msgcat::mc "Set axis value:"]
    entry .axisoffset.input.entry -textvariable axisoffsettext -width 20
    frame .axisoffset.buttons
    button .axisoffset.buttons.ok -text [msgcat::mc "OK"] -default active \
           -command "setAxisOffset $axisnum \$axisoffsettext; destroy .axisoffset"
    button .axisoffset.buttons.cancel -text [msgcat::mc "Cancel"] \
           -command "destroy .axisoffset"
    pack .axisoffset.input.label .axisoffset.input.entry -side left
    pack .axisoffset.input -side top
    pack .axisoffset.buttons -side bottom -fill x -pady 2m
    pack .axisoffset.buttons.ok .axisoffset.buttons.cancel -side left -expand 1
    bind .axisoffset <Return> "setAxisOffset $axisnum \$axisoffsettext; destroy .axisoffset"

    focus .axisoffset.input.entry
    set axisoffsettext 0.0
    .axisoffset.input.entry select range 0 end
}

proc incrJogSpeed {jointtype} {
    switch $jointtype {
      linear  {
        if {$::linearJogSpeed < $::linearJogSpeedMax} {
            set ::linearJogSpeed [expr {int($::linearJogSpeed + 1.5)}]
        }
      }
      angular {
        if {$::angularJogSpeed < $::angularJogSpeedMax} {
            set ::angularJogSpeed [expr {int($::angularJogSpeed + 1.5)}]
        }
      }
    }
}

proc decrJogSpeed {jointtype} {
    switch $jointtype {
      linear  {
        if {$::linearJogSpeed > 1} {
            set ::linearJogSpeed [expr {int($::linearJogSpeed - 0.5)}]
        }
      }
      angular {
        if {$::angularJogSpeed > 1} {
            set ::angularJogSpeed [expr {int($::angularJogSpeed - 0.5)}]
        }
      }
    }
}

proc popupJogSpeed {jointtype} {
    global maxJogSpeed popupJogSpeedEntry

    if {[winfo exists .jogspeedpopup]} {
        wm deiconify .jogspeedpopup
        raise .jogspeedpopup
        focus .jogspeedpopup
        return
    }
    toplevel .jogspeedpopup
    wm title .jogspeedpopup [msgcat::mc "Set Jog Speed"]

    # initialize value to current jog speed
    switch $jointtype {
      linear  {set popupJogSpeedEntry $::linearJogSpeed
               set item ::linearJogSpeed
      }
      angular {set popupJogSpeedEntry $::angularJogSpeed
               set item ::angularJogSpeed
      }
    }

    frame .jogspeedpopup.input
    label .jogspeedpopup.input.label -text [msgcat::mc "Set jog speed:"]
    entry .jogspeedpopup.input.entry -textvariable popupJogSpeedEntry -width 20
    frame .jogspeedpopup.buttons
    button .jogspeedpopup.buttons.ok -text [msgcat::mc "OK"] -default active -command "set $item \$popupJogSpeedEntry; destroy .jogspeedpopup"
    button .jogspeedpopup.buttons.cancel -text [msgcat::mc "Cancel"] -command "destroy .jogspeedpopup"
    pack .jogspeedpopup.input.label .jogspeedpopup.input.entry -side left
    pack .jogspeedpopup.input -side top
    pack .jogspeedpopup.buttons -side bottom -fill x -pady 2m
    pack .jogspeedpopup.buttons.ok .jogspeedpopup.buttons.cancel -side left -expand 1
    bind .jogspeedpopup <Return> "set $item \$popupJogSpeedEntry; destroy .jogspeedpopup"

    focus .jogspeedpopup.input.entry
    .jogspeedpopup.input.entry select range 0 end
}

proc popupOride {} {
    global feedoverride realfeedoverride popupOrideEntry

    if {[winfo exists .oridepopup]} {
        wm deiconify .oridepopup
        raise .oridepopup
        focus .oridepopup
        return
    }
    toplevel .oridepopup
    wm title .oridepopup [msgcat::mc "Set Feed Override"]

    # initialize value to current feed override
    set popupOrideEntry $realfeedoverride

    frame .oridepopup.input
    label .oridepopup.input.label -text [msgcat::mc "Set feed override:"]
    entry .oridepopup.input.entry -textvariable popupOrideEntry -width 20
    frame .oridepopup.buttons
    button .oridepopup.buttons.ok -text [msgcat::mc "OK"] -default active -command {set feedoverride $popupOrideEntry; emc_feed_override $feedoverride; destroy .oridepopup}
    button .oridepopup.buttons.cancel -text [msgcat::mc "Cancel"] -command "destroy .oridepopup"
    pack .oridepopup.input.label .oridepopup.input.entry -side left
    pack .oridepopup.input -side top
    pack .oridepopup.buttons -side bottom -fill x -pady 2m
    pack .oridepopup.buttons.ok .oridepopup.buttons.cancel -side left -expand 1
    bind .oridepopup <Return> {set feedoverride $popupOrideEntry; emc_feed_override $feedoverride; destroy .oridepopup}

    focus .oridepopup.input.entry
    .oridepopup.input.entry select range 0 end
}

proc popupSoride {} {
    global spindleoverride realspindleoverride popupSorideEntry

    if {[winfo exists .soridepopup]} {
        wm deiconify .soridepopup
        raise .soridepopup
        focus .soridepopup
        return
    }
    toplevel .soridepopup
    wm title .soridepopup [msgcat::mc "Set Spindle Override"]

    # initialize value to current feed override
    set popupSorideEntry $realspindleoverride

    frame .soridepopup.input
    label .soridepopup.input.label -text [msgcat::mc "Set spindle speed override:"]
    entry .soridepopup.input.entry -textvariable popupSorideEntry -width 20
    frame .soridepopup.buttons
    button .soridepopup.buttons.ok -text [msgcat::mc "OK"] -default active -command {set spindleoverride $popupSorideEntry; emc_spindle_override $spindleoverride; destroy .soridepopup}
    button .soridepopup.buttons.cancel -text [msgcat::mc "Cancel"] -command "destroy .soridepopup"
    pack .soridepopup.input.label .soridepopup.input.entry -side left
    pack .soridepopup.input -side top
    pack .soridepopup.buttons -side bottom -fill x -pady 2m
    pack .soridepopup.buttons.ok .soridepopup.buttons.cancel -side left -expand 1
    bind .soridepopup <Return> {set spindleoverride $popupSorideEntry; emc_spindle_override $spindleoverride; destroy .soridepopup}

    focus .soridepopup.input.entry
    .soridepopup.input.entry select range 0 end
}

set realfeedoverride [emc_feed_override]
set feedoverride $realfeedoverride

set realspindleoverride [emc_spindle_override]
set spindleoverride $realspindleoverride

# read the max feed override from the ini file
set temp [emc_ini "MAX_FEED_OVERRIDE" "DISPLAY"]
if {[string length $temp] == 0} {
    set temp 1
}
set maxFeedOverride [expr {int($temp * 100 + 0.5)}]

# read the min spindle override from the ini file
set temp [emc_ini "MIN_SPINDLE_OVERRIDE" "DISPLAY"]
if {[string length $temp] == 0} {
    set temp 1
}
set minSpindleOverride [expr {int($temp * 100 + 0.5)}]

# read the max spindle override from the ini file
set temp [emc_ini "MAX_SPINDLE_OVERRIDE" "DISPLAY"]
if {[string length $temp] == 0} {
    set temp 1
}
set maxSpindleOverride [expr {int($temp * 100 + 0.5)}]

set controls [frame $top.controls -relief ridge -bd 3]
pack $controls -side top -anchor w -fill x -expand 1

set lcontrols [frame $controls.left]
pack $lcontrols -side left -anchor nw -fill x -expand 1

set rcontrols [frame $controls.right]
pack $rcontrols -side left -anchor nw -fill x -expand 1

set linearjog [frame $lcontrols.linearjog]
set linearjogtop [frame $linearjog.top]
set linearjogbottom [frame $linearjog.bottom]
set linearjoglabel [label $linearjogtop.label \
               -text [msgcat::mc "Linear Jog Speed"] ]
set linearjoglabelunits [label $linearjogtop.units \
                        -textvariable unitsetting -width 6 -anchor e]
set linearjoglabelunitssuffix [label $linearjogtop.suffix -text "/min:"]
set linearjogvalue [label $linearjogtop.value \
                   -textvariable ::linearJogSpeed -width 6]
set linearjogscale [scale $linearjogbottom.scale \
               -from 0 -to $::linearJogSpeedMax -variable ::linearJogSpeed \
               -orient horizontal -showvalue 0 -takefocus 0]


pack $linearjog       -side top  -fill x -expand 1 -anchor nw
pack $linearjogtop    -side top  -fill x -expand 1
pack $linearjogbottom -side top  -fill x -expand 1

pack $linearjoglabel  -side left
pack $linearjoglabelunits -side left
pack $linearjoglabelunitssuffix -side left
pack $linearjogvalue -side right -padx 1m ; # don't bump against label
pack $linearjogscale -side top -fill x -expand 1

bind $linearjoglabel <ButtonPress-1> {popupJogSpeed linear}
bind $linearjoglabel <ButtonPress-3> {popupJogSpeed linear}
bind $linearjogvalue <ButtonPress-1> {popupJogSpeed linear}
bind $linearjogvalue <ButtonPress-3> {popupJogSpeed linear}

if [info exists ::angularJogSpeed] {
  set angularjog [frame $lcontrols.angularjog]
  set angularjogtop [frame $angularjog.top]
  set angularjogbottom [frame $angularjog.bottom]
  set angularjoglabel [label $angularjogtop.label \
                 -text [msgcat::mc "Angular Jog Speed (deg)/min:"] ]
  set angularjogvalue [label $angularjogtop.value \
                      -textvariable ::angularJogSpeed -width 6]
  set angularjogscale [scale $angularjogbottom.scale \
               -from 0 -to $::angularJogSpeedMax -variable ::angularJogSpeed \
               -orient horizontal -showvalue 0 -takefocus 0]

  pack $angularjog       -side top -fill x -expand 1
  pack $angularjogtop    -side top -fill x -expand 1
  pack $angularjogbottom -side top -fill x -expand 1

  pack $angularjoglabel -side left -anchor nw
  pack $angularjogvalue -side right -padx 1m ; # don't bump against label
  pack $angularjogscale -side top -fill x -expand 1

  bind $angularjoglabel <ButtonPress-1> {popupJogSpeed angular}
  bind $angularjoglabel <ButtonPress-3> {popupJogSpeed angular}
  bind $angularjogvalue <ButtonPress-1> {popupJogSpeed angular}
  bind $angularjogvalue <ButtonPress-3> {popupJogSpeed angular}
}

set oride [frame $rcontrols.oride]
set oridetop [frame $oride.top]
set oridebottom [frame $oride.bottom]
set oridelabel [label $oridetop.label -text [msgcat::mc "Feed Override:"] ]
set oridevalue [label $oridetop.value -textvariable realfeedoverride -width 6]
set oridescale [scale $oridebottom.scale  -from 1 -to $maxFeedOverride -variable feedoverride -orient horizontal -showvalue 0 -command {emc_feed_override} -takefocus 0]

pack $oride       -side top -fill x -expand 1
pack $oridetop    -side top -fill x -expand 1
pack $oridebottom -side top -fill x -expand 1

pack $oridelabel -side left
pack $oridevalue -side right -padx 1m ; # don't bump against label
pack $oridescale -side top -fill x -expand 1

bind $oridelabel <ButtonPress-1> {popupOride}
bind $oridelabel <ButtonPress-3> {popupOride}
bind $oridevalue <ButtonPress-1> {popupOride}
bind $oridevalue <ButtonPress-3> {popupOride}

set soride [frame $rcontrols.soride]
set soridetop [frame $soride.top]
set soridebottom [frame $soride.bottom]
set soridelabel [label $soridetop.label -text [msgcat::mc "Spindle speed Override:"] ]
set soridevalue [label $soridetop.value -textvariable realspindleoverride -width 6]
set soridescale [scale $soridebottom.scale  -from $minSpindleOverride -to $maxSpindleOverride -variable spindleoverride -orient horizontal -showvalue 0 -command {emc_spindle_override} -takefocus 0]

pack $soride       -side top -fill x -expand 1
pack $soridetop    -side top -fill x -expand 1
pack $soridebottom -side top -fill x -expand 1

pack $soridelabel -side left
pack $soridevalue -side right -padx 1m ; # don't bump against label
pack $soridescale -side top -fill x -expand 1

bind $soridelabel <ButtonPress-1> {popupSoride}
bind $soridelabel <ButtonPress-3> {popupSoride}
bind $soridevalue <ButtonPress-1> {popupSoride}
bind $soridevalue <ButtonPress-3> {popupSoride}


proc sendMdi {mdi} {
    emc_mdi $mdi
}

set mditext ""
set mdi [frame $top.mdi]
set mdilabel [label $mdi.label -text [msgcat::mc "MDI:"] -width 4 -anchor w]
set mdientry [entry $mdi.entry -textvariable mditext -takefocus 0 -width 73]

pack $mdi -side top -anchor w
pack $mdilabel $mdientry -side left

bind $mdientry <Return> {$mdientry select range 0 end ; sendMdi $mditext}

set programcodestring ""
set programcodes [label $top.programcodes -textvariable programcodestring]
pack $programcodes -side top -anchor w

set programnamestring "none"
set programstatusstring "none"
set programin 0
set activeLine 0

# programstatus is "Program: <name> Status: idle"
set programstatus [frame $top.programstatus]

# programstatusname is "Program: <name>" half of programstatus
set programstatusname [frame $programstatus.name]
set programstatusnamelabel [label $programstatusname.label -text [msgcat::mc "Program: "]]
set programstatusnamevalue [label $programstatusname.value -textvariable programnamestring -width 50 -anchor e]

# programstatusstatus is "Status: idle" half of programstatus
set programstatusstatus [frame $programstatus.status]
set programstatusstatuslabel [label $programstatusstatus.label -text [msgcat::mc "  -  Status: "]]
set programstatusstatusvalue [label $programstatusstatus.value -textvariable programstatusstring -anchor w]

pack $programstatus -side top -anchor w
pack $programstatusname $programstatusstatus -side left
pack $programstatusnamelabel $programstatusnamevalue -side left
pack $programstatusstatuslabel $programstatusstatusvalue -side left

# programframe is the frame for the button widgets for run, pause, etc.
set programframe [frame $top.programframe]
set programopenbutton [button $programframe.open -text [msgcat::mc "Open..."] -command {fileDialog} -takefocus 0]
set programrunbutton [button $programframe.run -text [msgcat::mc "Run"] -command {emc_run $runMark ; set runMark 0} -takefocus 0]
set programpausebutton [button $programframe.pause -text [msgcat::mc "Pause"] -command {emc_pause} -takefocus 0]
set programresumebutton [button $programframe.resume -text [msgcat::mc "Resume"] -command {emc_resume} -takefocus 0]
set programstepbutton [button $programframe.step -text [msgcat::mc "Step"] -command {emc_step} -takefocus 0]
set programverifybutton [button $programframe.verify -text [msgcat::mc "Verify"] -command {emc_run -1} -takefocus 0]
set opstopbutton [button $programframe.opstop -text [msgcat::mc "Optional Stop"] -command {toggleOptionalStop} -takefocus 0]

set opstopbuttonbg [$opstopbutton cget -background]
set opstopbuttonabg [$opstopbutton cget -activebackground]

set homedcolor   lightgreen
set unhomedcolor yellow
set opstopcolor  darkgreen
set limitcolor   red

pack $programframe -side top -anchor w -fill both -expand true
pack $programopenbutton $programrunbutton $programpausebutton $programresumebutton $programstepbutton $programverifybutton $opstopbutton -side left -fill both -expand true

# programfileframe is the frame for the program text widget showing the file
set programfileframe [frame $top.programfileframe]
set programfiletext [text $programfileframe.text -height 6 -width 78 -relief sunken -state disabled]
# FIXME-- hard-coded resources
$programfiletext tag configure highlight -background white -foreground black

pack $programfileframe -side top -anchor w -fill both -expand true
pack $programfiletext -side top -fill both -expand true

# Note: to check keysyms, use wish and do:
# bind . <KeyPress> {puts stdout {%%K=%K %%A=%A}}

bind . <KeyPress-F1> {toggleEstop}
bind . <KeyPress-F2> {toggleMachine}
bind . <KeyPress-F3> {emc_mode manual}
bind . <KeyPress-F4> {emc_mode auto}
bind . <KeyPress-F5> {emc_mode mdi}
bind . <KeyPress-F6> {emc_task_plan_init}
bind . <KeyPress-F10> {break}

bind ManualBindings <KeyPress-grave> [list jaSelect 0]
for {set idx 0} {$idx < $::numjoints} {incr idx} {
  bind ManualBindings <KeyPress-$idx>   [list jaSelect $idx]
}
bind ManualBindings <KeyPress-at> {toggleCmdAct}
bind ManualBindings <KeyPress-numbersign> {toggleRelAbs}
bind ManualBindings <KeyPress-dollar> {toggleJointWorld}
bind ManualBindings <KeyPress-comma> {decrJogSpeed linear}
bind ManualBindings <KeyPress-period> {incrJogSpeed linear}
bind ManualBindings <KeyPress-semicolon>  {decrJogSpeed angular}
bind ManualBindings <KeyPress-apostrophe> {incrJogSpeed angular}
bind ManualBindings <KeyPress-c> {toggleJogType}
bind ManualBindings <KeyPress-i> {toggleJogIncrement}

foreach {letter number} {x 0 y 1 z 2} {
  if {[lsearch $::coordnames [string toupper $letter]] < 0} { continue } 
  bind ManualBindings <KeyPress-$letter> [list jaSelect $number]
}

bind ManualBindings <$modifier-t> {popupToolOffset}
bind ManualBindings <KeyPress-b> {emc_brake off}
bind ManualBindings <Shift-KeyPress-B> {emc_brake on}
bind ManualBindings <$modifier-t> {popupToolOffset}
bind ManualBindings <KeyPress-F7> {toggleMist}
bind ManualBindings <KeyPress-F8> {toggleFlood}
bind ManualBindings <KeyPress-F9> {toggleSpindleForward}
bind ManualBindings <KeyPress-F10> {toggleSpindleReverse}
bind ManualBindings <Home> {emc_home $::activeJA}

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
bind AutoBindings <KeyPress-a> {emc_step}
bind AutoBindings <KeyPress-v> {emc_run -1}
bind AutoBindings <$modifier-t> {popupToolOffset}

proc spindleDecrDone {} {
    emc_spindle constant
    bind ManualBindings <KeyPress-F11> spindleDecrDown
}

proc spindleDecrDown {} {
    bind ManualBindings <KeyPress-F11> {}
    after cancel spindleDecrDone
    emc_spindle decrease
}

proc spindleDecrUp {} {
    global debounceTime

    after cancel spindleDecrDone
    after $debounceTime spindleDecrDone
}

bind ManualBindings <KeyPress-F11> spindleDecrDown
bind ManualBindings <KeyRelease-F11> spindleDecrUp

proc spindleIncrDone {} {
    emc_spindle constant
    bind ManualBindings <KeyPress-F12> spindleIncrDown
}

proc spindleIncrDown {} {
    bind ManualBindings <KeyPress-F12> {}
    after cancel spindleIncrDone
    emc_spindle increase
}

proc spindleIncrUp {} {
    global debounceTime

    after cancel spindleIncrDone
    after $debounceTime spindleIncrDone
}

bind ManualBindings <KeyPress-F12> spindleIncrDown
bind ManualBindings <KeyRelease-F12> spindleIncrUp

set ::activeJA 0
set minusJoint -1
set equalJoint -1

proc minusDone {} {
    global minusJoint
    
    jogStop $minusJoint
    bind ManualBindings <KeyPress-minus> minusDown
    set minusJoint -1
}

proc minusDown {} {
    global minusJoint
    
    if {$minusJoint < 0} {
       set minusJoint $::activeJA
    }

    bind ManualBindings <KeyPress-minus> {}
    after cancel minusDone
    jogNeg $minusJoint
}

proc minusUp {} {
    global debounceTime 

    after cancel minusDone
    after $debounceTime minusDone
}

bind ManualBindings <KeyPress-minus> minusDown
bind ManualBindings <KeyRelease-minus> minusUp

proc equalDone {} {
    global equalJoint
    
    jogStop $equalJoint
    set equalJoint -1
    bind ManualBindings <KeyPress-equal> equalDown
}

proc equalDown {} {
    global equalJoint

    if {$equalJoint < 0} {
       set equalJoint $::activeJA
    }

    bind ManualBindings <KeyPress-equal> {}
    after cancel equalDone
    jogPos $equalJoint
}

proc equalUp {} {
    global debounceTime

    after cancel equalDone
    after $debounceTime equalDone
}

bind ManualBindings <KeyPress-equal> equalDown
bind ManualBindings <KeyRelease-equal> equalUp

proc leftDone {} {
    jogStop 0
    bind ManualBindings <KeyPress-Left> leftDown
}

proc leftDown {} {
    jaSelect 0
    bind ManualBindings <KeyPress-Left> {}
    after cancel leftDone
    jogNeg 0
}

proc leftUp {} {
    global debounceTime

    after cancel leftDone
    after $debounceTime leftDone
}

bind ManualBindings <KeyPress-Left> leftDown
bind ManualBindings <KeyRelease-Left> leftUp

proc rightDone {} {
    jogStop 0
    bind ManualBindings <KeyPress-Right> rightDown
}

proc rightDown {} {
    jaSelect 0
    bind ManualBindings <KeyPress-Right> {}
    after cancel rightDone
    jogPos 0
}

proc rightUp {} {
    global debounceTime

    after cancel rightDone
    after $debounceTime rightDone
}

bind ManualBindings <KeyPress-Right> rightDown
bind ManualBindings <KeyRelease-Right> rightUp

proc downDone {} {
    jogStop 1
    bind ManualBindings <KeyPress-Down> downDown
}

proc downDown {} {
    jaSelect 1
    bind ManualBindings <KeyPress-Down> {}
    after cancel downDone
    jogNeg 1
}

proc downUp {} {
    global debounceTime

    after cancel downDone
    after $debounceTime downDone
}

bind ManualBindings <KeyPress-Down> downDown
bind ManualBindings <KeyRelease-Down> downUp

proc upDone {} {
    jogStop 1
    bind ManualBindings <KeyPress-Up> upDown
}

proc upDown {} {
    jaSelect 1
    bind ManualBindings <KeyPress-Up> {}
    after cancel upDone
    jogPos 1
}

proc upUp {} {
    global debounceTime

    after cancel upDone
    after $debounceTime upDone
}

bind ManualBindings <KeyPress-Up> upDown
bind ManualBindings <KeyRelease-Up> upUp

proc priorDone {} {
    jogStop 2
    bind ManualBindings <KeyPress-Prior> priorDown
}

proc priorDown {} {
    jaSelect 2
    bind ManualBindings <KeyPress-Prior> {}
    after cancel priorDone
    jogPos 2
}

proc priorUp {} {
    global debounceTime

    after cancel priorDone
    after $debounceTime priorDone
}

bind ManualBindings <KeyPress-Prior> priorDown
bind ManualBindings <KeyRelease-Prior> priorUp

proc nextDone {} {
    jogStop 2
    bind ManualBindings <KeyPress-Next> nextDown
}

proc nextDown {} {
    jaSelect 2
    bind ManualBindings <KeyPress-Next> {}
    after cancel nextDone
    jogNeg 2
}

proc nextUp {} {
    global debounceTime

    after cancel nextDone
    after $debounceTime nextDone
}

bind ManualBindings <KeyPress-Next> nextDown
bind ManualBindings <KeyRelease-Next> nextUp

bind . <Escape> {tkemc_abort}

# force explicit updates, so calls to emc_estop, for example, don't
# always cause an NML read; they just return last latched status
emc_update none

proc popupError {err} {
    if {[winfo exists .error]} {
        # FIXME-- log subsequent errors?
        puts stdout $err
        return
    }
    toplevel .error
    wm title .error [msgcat::mc "Error"]

    label .error.msg -font {Helvetica 12 bold} -wraplength 4i -justify left -text " $err "
    pack .error.msg -side top

    frame .error.buttons
    pack .error.buttons -side bottom -fill x -pady 2m
    button .error.buttons.ok -text [msgcat::mc "OK"] -default active -command "destroy .error"
    pack .error.buttons.ok -side left -expand 1
    bind .error <Return> "destroy .error"
}

proc popupOperatorText {otext} {
    if {[winfo exists .opertext]} {
        # FIXME-- log subsequent text?
        puts stdout $otext
        return
    }
    toplevel .opertext
    wm title .opertext Information

    label .opertext.msg -font {Helvetica 12 bold} -wraplength 4i -justify left -text "$otext"
    pack .opertext.msg -side top

    frame .opertext.buttons
    pack .opertext.buttons -side bottom -fill x -pady 2m
    button .opertext.buttons.ok -text [msgcat::mc "OK"] -default active -command "destroy .opertext"
    pack .opertext.buttons.ok -side left -expand 1
    bind .opertext <Return> "destroy .opertext"
}

proc popupOperatorDisplay {odisplay} {
    if {[winfo exists .operdisplay]} {
        # FIXME-- log subsequent display messages?
        puts stdout $odisplay
        return
    }
    toplevel .operdisplay
    wm title .operdisplay Information

    label .operdisplay.msg -font {Helvetica 12 bold} -wraplength 4i -justify left -text "$odisplay"
    pack .operdisplay.msg -side top

    frame .operdisplay.buttons
    pack .operdisplay.buttons -side bottom -fill x -pady 2m
    button .operdisplay.buttons.ok -text [msgcat::mc "OK"] -default active -command "destroy .operdisplay"
    pack .operdisplay.buttons.ok -side left -expand 1
    bind .operdisplay <Return> "destroy .operdisplay"
}

proc popupFont {} {
    global fontfamily fontsize fontstyle

    if {[winfo exists .setfont]} {
        wm deiconify .setfont
        raise .setfont
        focus .setfont
        return
    }
    set m [toplevel .setfont]
    wm title $m [msgcat::mc "Set Font"]

    set a [frame $m.radio]
    pack $a -side top -expand 1
    set f [frame $a.family]
    set z [frame $a.size]
    set y [frame $a.style]
    pack $f $z $y -side left -anchor n -expand 1

    label $f.label -text [msgcat::mc "Font"]
    pack $f.label
    foreach i {Helvetica Courier {courier 10 pitch} Times} {
	radiobutton $f.b$i -text $i -variable fontfamily -value $i
	pack $f.b$i -side top -anchor w
    }

    label $z.label -text [msgcat::mc "Size"]
    pack $z.label
    foreach i {24 30 36 48 56 64} {
	radiobutton $z.b$i -text $i -variable fontsize -value $i
	pack $z.b$i -side top -anchor w
    }

    label $y.label -text [msgcat::mc "Style"]
    pack $y.label
    foreach i {roman bold italic} {
	radiobutton $y.b$i -text $i -variable fontstyle -value $i
	pack $y.b$i -side top -anchor w
    }

    frame $m.buttons
    pack $m.buttons -side bottom -fill x -pady 2m
    button $m.buttons.ok -text [msgcat::mc "OK"] -default active -command "setfont ; destroy $m"
    button $m.buttons.cancel -text [msgcat::mc "Cancel"] -command "destroy $m"
    pack $m.buttons.ok $m.buttons.cancel -side left -expand 1
    bind .setfont <Return> "setfont ; destroy $m"
}

set syncingFeedOverride 0
proc syncFeedOverride {} {
    global feedoverride realfeedoverride syncingFeedOverride

    set feedoverride $realfeedoverride
    set syncingFeedOverride 0
}

set syncingSpindleOverride 0
proc syncSpindleOverride {} {
    global spindleoverride realspindleoverride syncingSpindleOverride

    set spindleoverride $realspindleoverride
    set syncingSpindleOverride 0
}


proc setManualBindings {} {
    bindtags . {ManualBindings . all}
}

proc setAutoBindings {} {
    bindtags . {AutoBindings . all}
}

proc setMdiBindings {} {
    bindtags . {. all}
}

# Set up the display for manual mode, which may be reset
# immediately in updateStatus if it's not correct.
# This lets us check if there's a change and reset bindings, etc.
# on transitions instead of every cycle.
set modelabel [msgcat::mc "MANUAL"]
$mistbutton config -state normal
$floodbutton config -state normal
$spindlebutton config -state normal
$brakebutton config -state normal
$mdientry config -state disabled
# Set manual bindings
setManualBindings
# Record that we're in manual display mode
set modeInDisplay "manual"

set ::lastjointworld $::jointworld
set lastactcmd $actcmd
set lastcoords $coords
set emc_teleop_enable_command_given 0

proc updateStatus {} {
    for {set idx 0} {$idx < $::MAX_JOINTS} {incr idx} {
       global poslabel$idx posdigit$idx
       global pos${idx}l   pos${idx}d
       global jointlabel${idx} worldlabel${idx}
    }
    global emc_teleop_enable_command_given
    global lastactcmd lastcoords
    global displayCycleTime syncDelayTime
    global mistbutton floodbutton spindlebutton brakebutton
    global modeInDisplay
    global estoplabel modelabel mistlabel floodlabel lubelabel spindlelabel brakelabel
    global tooloffsetsetting offsetsetting 
    global unitlabel unitsetting oldunitsetting
    global actcmd coords
    global radiorel radiocmd radioact radioabs
    global limoridebutton limoridebuttonbg limoridebuttonabg
    global opstopbutton opstopbuttonbg opstopbuttonabg
    global realfeedoverride feedoverride syncingFeedOverride
    global realspindleoverride spindleoverride syncingSpindleOverride
    global mdientry
    global programcodestring
    global programnamestring programin activeLine programstatusstring
    global programfiletext
    global taskhb taskcmd tasknum taskstatus
    global iohb iocmd ionum iostatus
    global motionhb motioncmd motionnum motionstatus
    global oldstatusstring
    global homedcolor unhomedcolor opstopcolor limitcolor

    # force an update of error log
    set thisError [emc_error]
    if {$thisError != "ok"} {
        popupError $thisError
    }

    # force an update of operator information messages
    set thisError [emc_operator_text]
    if {$thisError != "ok"} {
        popupOperatorText $thisError
    }

    # force an update of operator request messages
    set thisError [emc_operator_display]
    if {$thisError != "ok"} {
        popupOperatorDisplay $thisError
    }

    # force an update of status
    emc_update

    # now update labels

    if {[emc_estop] == "on"} {
        set estoplabel [msgcat::mc "ESTOP"]
    } elseif {[emc_machine] == "on"} {
        set estoplabel [msgcat::mc "ON"]
    } else {
        set estoplabel [msgcat::mc "ESTOP RESET"]
    }

    set temp [emc_mode]
    if {$temp != $modeInDisplay} {
        if {$temp == "auto"} {
            set modelabel [msgcat::mc "AUTO"]
            $mistbutton config -state disabled
            $floodbutton config -state disabled
            $spindlebutton config -state disabled
            $brakebutton config -state disabled
            $mdientry config -state disabled
            focus .
            setAutoBindings
        } elseif {$temp == "mdi"} {
            set modelabel [msgcat::mc "MDI"]
            $mistbutton config -state disabled
            $floodbutton config -state disabled
            $spindlebutton config -state disabled
            $brakebutton config -state disabled
            $mdientry config -state normal
            focus $mdientry
            $mdientry select range 0 end
            setMdiBindings
        } else {
            set modelabel [msgcat::mc "MANUAL"]
            $mistbutton config -state normal
            $floodbutton config -state normal
            $spindlebutton config -state normal
            $brakebutton config -state normal
            $mdientry config -state normal
            focus .
            setManualBindings
        }
        set modeInDisplay $temp
    }

    if {[emc_mist] == "on"} {
        set mistlabel [msgcat::mc "MIST ON"]
    } elseif {[emc_mist] == "off"} {
        set mistlabel [msgcat::mc "MIST OFF"]
    } else {
        set mistlabel [msgcat::mc "MIST ?"]
    }

    if {[emc_flood] == "on"} {
        set floodlabel [msgcat::mc "FLOOD ON"]
    } elseif {[emc_flood] == "off"} {
        set floodlabel [msgcat::mc "FLOOD OFF"]
    } else {
        set floodlabel [msgcat::mc "FLOOD ?"]
    }

    if {[emc_lube] == "on"} {
        set lubelabel [msgcat::mc "LUBE ON"]
    } elseif {[emc_lube] == "off"} {
        set lubelabel [msgcat::mc "LUBE OFF"]
    } else {
        set lubelabel [msgcat::mc "LUBE ?"]
    }

    if {[emc_spindle] == "forward"} {
        set spindlelabel [msgcat::mc "SPINDLE FORWARD"]
    } elseif {[emc_spindle] == "reverse"} {
        set spindlelabel [msgcat::mc "SPINDLE REVERSE"]
    } elseif {[emc_spindle] == "off"} {
        set spindlelabel [msgcat::mc "SPINDLE OFF"]
    } elseif {[emc_spindle] == "increase"} {
        set spindlelabel [msgcat::mc "SPINDLE INCREASE"]
    } elseif {[emc_spindle] == "decrease"} {
        set spindlelabel [msgcat::mc "SPINDLE DECREASE"]
    } else {
        set spindlelabel [msgcat::mc "SPINDLE ?"]
    }

    if {[emc_brake] == "on"} {
        set brakelabel [msgcat::mc "BRAKE ON"]
    } elseif {[emc_brake] == "off"} {
        set brakelabel [msgcat::mc "BRAKE OFF"]
    } else {
        set brakelabel [msgcat::mc "BRAKE ?"]
    }

    # set the tool information, inhibit update if .tooloffset popup in progress
    if {![winfo exists .tooloffset] || ![winfo ismapped .tooloffset]} {
      set ::tentry(toolno) [emc_tool]
      set tooloffsetsetting [format "X%.4f Y%.4f Z%.4f" [emc_tool_offset X] [emc_tool_offset Y] [emc_tool_offset Z]]
      # note: currently no emc_tool_offset options for diam,front,back,orient
      foreach item "$::coordnames" {
        set ::tentry($item) [format %.4f [emc_tool_offset $item]]
      }
    }

    # set the offset information
    set offsetsetting [lsearch -inline [emc_program_codes] {G5[4-9]*}]
    for {set i 0} {$i < $::numjoints} {incr i} {
    if { [lsearch $::coordnames [lindex $::worldlabellist $i]] != -1 } {
      set fstr [lindex $::worldlabellist $i]
      set spec " $fstr%.4f"
      set args [emc_pos_offset $fstr]
      append offsetsetting [format $spec $args]
      }
    }

    # set the unit information
    set unitsetting [emc_display_linear_units]
    if {$oldunitsetting != $unitsetting} {
	set oldunitsetting $unitsetting
	set_balloon $unitlabel [unithelp $unitsetting]
    }
    
    # detect external motion_mode change
    set teleop_mode [emc_teleop_enable]
    if {$::jointworld == "joint" && $teleop_mode} {
      set ::jointworld "world" ;# radiobutton var
    } elseif {$::jointworld == "world" && !$teleop_mode} {
      set ::jointworld "joint"
    }

    # format the numbers with 4 digits-dot-4 digits
    # coords relative machine
    # actcmd: commanded actual
    if {$::jointworld == "joint" } {
        # JOINT
        if { $::lastjointworld != "joint" } {
            for {set idx 0} {$idx < $::MAX_JOINTS} {incr idx} {
              set poslabel$idx [set jointlabel$idx]
            }
            set lastactcmd $actcmd
            set lastcoords $coords
            set actcmd "actual"
            set coords "machine"
            $radiorel config -state disabled
            $radioabs config -state disabled
            $radioact config -state disabled
            $radiocmd config -state disabled
            set ::lastjointworld $::jointworld
            if {   [emc_teleop_enable] == 1
                && $modeInDisplay == "manual"
               } {
                emc_teleop_enable 0
                set emc_teleop_enable_command_given 0
            }
        }
        for {set idx 0} {$idx < $::numjoints} {incr idx} {
          set posdigit$idx [format "%9.4f" [emc_joint_pos $idx] ]
        }
    } else {
        # TELEOP
        if { $::lastjointworld != "world" } {
            if {   [emc_teleop_enable] == 0
                && [emc_kinematics_type] != $::KINEMATICS_IDENTITY
                && $modeInDisplay == "manual"
               } {
                if { $emc_teleop_enable_command_given == 0 } {
                    emc_teleop_enable 1
                    set emc_teleop_enable_command_given 1
                } else {
                    set ::jointworld "joint"
                    set emc_teleop_enable_command_given 0
                }
            } else {
                for {set idx 0} {$idx < $::MAX_JOINTS} {incr idx} {
                  set poslabel$idx [set worldlabel$idx]
                }
                set actcmd $lastactcmd
                set coords $lastcoords
                $radiorel config -state normal
                $radioabs config -state normal
                $radioact config -state normal
                $radiocmd config -state normal
                set ::lastjointworld $::jointworld
            }
        } 
        if { $::lastjointworld == "world" } {
          for {set idx 0} {$idx < $::numjoints} {incr idx} {
            set letter  [lindex $::coordnames $idx]
            set axisnum [lsearch $::worldlabellist $letter]
            if {$coords == "relative" && $actcmd == "commanded"} {
                set posdigit$idx [format "%9.4f" [emc_rel_cmd_pos $letter ] ]
            } elseif {$coords == "relative" && $actcmd == "actual"} {
                set posdigit$idx [format "%9.4f" [emc_rel_act_pos $letter  ] ]
            } elseif {$coords == "machine" && $actcmd == "commanded"} {
                set posdigit$idx [format "%9.4f" [emc_abs_cmd_pos $letter ] ]
            } else {
                # $coords == "machine" && $actcmd == "actual"
                set posdigit$idx [format "%9.4f" [emc_abs_act_pos $letter ] ]
            }
        }
      }
    }

    # color the numbers
    for {set idx 0} {$idx < $::MAX_JOINTS} {incr idx} {
        set pos_label [set pos${idx}l]
        set pos_digit [set pos${idx}d]
        if {[emc_joint_limit $idx] != "ok"} {
            $pos_label config -foreground $limitcolor
            $pos_digit config -foreground $limitcolor
        } elseif {[emc_joint_homed $idx] == "homed"} {
            $pos_label config -foreground $homedcolor
            $pos_digit config -foreground $homedcolor
        } else {
            $pos_label config -foreground $unhomedcolor
            $pos_digit config -foreground $unhomedcolor
        }
    }
    
    # color the limit override button $limitcolor if active
    if {[emc_override_limit]} {
        $limoridebutton config -background $limitcolor
        $limoridebutton config -activebackground $limitcolor
    } else {
        $limoridebutton config -background $limoridebuttonbg
        $limoridebutton config -activebackground $limoridebuttonabg
    }

    # color the optional stop button opstopcolor if active
    if {[emc_optional_stop]} {
        $opstopbutton config -background       $opstopcolor
        $opstopbutton config -activebackground $opstopcolor
    } else {
        $opstopbutton config -background $opstopbuttonbg
        $opstopbutton config -activebackground $opstopbuttonabg
    }

    # set the feed override
    set realfeedoverride [emc_feed_override]
    # do me
    if {$realfeedoverride != $feedoverride && $syncingFeedOverride == 0} {
        set syncingFeedOverride 1
        after $syncDelayTime syncFeedOverride
    }

    # set the spindle override
    set realspindleoverride [emc_spindle_override]
    # do me
    if {$realspindleoverride != $spindleoverride && $syncingSpindleOverride == 0} {
        set syncingSpindleOverride 1
        after $syncDelayTime syncSpindleOverride
    }


    # temporary fix for 0 jog speed problem
    if {$::linearJogSpeed  <= 0} {set ::linearJogSpeed  .000001}
    if {[info exists ::angularJogSpeed] && $::angularJogSpeed <= 0} {
      set ::angularJogSpeed .000001
    }

    # fill in the program codes
    set programcodestring [emc_program_codes]

    # fill in the program status
    set oldstatusstring $programstatusstring
    set programstatusstring [emc_program_status]

    # load new program text on status change
    if {$oldstatusstring == "idle" && $programstatusstring == "running"} {
        loadProgramText
    }

    # update the current file name and load program text
    set temp [emc_program]
    if {$temp != $programnamestring} {
        if {$temp == "none"} {
        } else {
            set programnamestring $temp
            loadProgramText
        }
    }

    # highlight the active line
    set temp [emc_program_line]
    if {$temp != $activeLine} {
        $programfiletext tag remove highlight $activeLine.0 $activeLine.end
        set activeLine $temp
        $programfiletext tag add highlight $activeLine.0 $activeLine.end
    }

    # position text so line shows
    $programfiletext yview $activeLine.0

    # enable plotting if plotter exists
    if {[winfo exists .plot]} {
        updatePlot
    }

    # get diagnostics info
    if {[winfo exists .diagnostics]} {
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
    after $displayCycleTime updateStatus
}

# abort to a sane state and also coerce the interpreter to send an
# EMC_TRAJ_SET_ORIGIN message so the stat buffer (and our gui) gets updated
proc tkemc_abort {} {
    emc_abort
    set oldmode [emc_mode]
    emc_mode mdi
    emc_mdi "G55"
    emc_mdi "G54"
    emc_mode $oldmode
}

updateStatus

set temp [emc_ini "USER_COMMAND_FILE" "DISPLAY"]
if [file exists $temp] {
  source $temp
}
