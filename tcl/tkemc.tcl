#!/bin/sh
# the next line restarts using emcsh \
exec emcsh "$0" "$@"

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

# read the application defaults
foreach f {TkEmc configs/TkEmc /usr/X11R6/lib/X11/app-defaults/TkEmc} {
    if {[file exists $f]} {
        option readfile $f startupFile
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

set worldlabellist ""
set axiscoordmap ""
set numaxes [emc_ini "AXES" "TRAJ"]
set coordnames [ emc_ini "COORDINATES" "TRAJ"]

set numcoords 0

if { [ string first "X" $coordnames ] >= 0 } {
    set numcoords $numcoords+1
    set worldlabellist [ concat $worldlabellist X]
    set axiscoordmap [ concat $axiscoordmap 0  ]
}

if { [ string first "Y" $coordnames ] >= 0 } {
    set numcoords $numcoords+1
    set worldlabellist [ concat $worldlabellist Y]
    set axiscoordmap [ concat $axiscoordmap  1 ]
}

if { [ string first "Z" $coordnames ] >= 0 } {
    set numcoords $numcoords+1
    set worldlabellist [ concat $worldlabellist Z]
    set axiscoordmap [ concat $axiscoordmap  2 ]
}

if { [ string first "R" $coordnames ] >= 0 } {
    set numcoords $numcoords+1
    set worldlabellist [ concat $worldlabellist A]
    set axiscoordmap [ concat $axiscoordmap  3 ]
}

if { [ string first "P" $coordnames ] >= 0 } {
    set numcoords $numcoords+1
    set worldlabellist [ concat $worldlabellist B]
    set axiscoordmap [ concat $axiscoordmap  4 ]
}

if { [ string first "W" $coordnames ] >= 0 } {
    set numcoords $numcoords+1
    set worldlabellist [ concat $worldlabellist C]
    set axiscoordmap [ concat $axiscoordmap  5 ]
}

if { [ string first "A" $coordnames ] >= 0 } {
    set numcoords $numcoords+1
    set worldlabellist [ concat $worldlabellist A]
    set axiscoordmap [ concat $axiscoordmap  3 ]
}

if { [ string first "B" $coordnames ] >= 0 } {
    set numcoords $numcoords+1
    set worldlabellist [ concat $worldlabellist B]
    set axiscoordmap [ concat $axiscoordmap  4 ]
}

if { [ string first "C" $coordnames ] >= 0 } {
    set numcoords $numcoords+1
    set worldlabellist [ concat $worldlabellist C]
    set axiscoordmap [ concat $axiscoordmap  5 ]
}

set worldlabellist [ concat $worldlabellist "-" "-" "-" "-" "-" "-" ]
set axiscoordmap [ concat $axiscoordmap 0 0 0 0 0 0 ]
set worldlabel0 [lindex  $worldlabellist 0 ]
set worldlabel1 [lindex  $worldlabellist 1 ]
set worldlabel2 [lindex  $worldlabellist 2 ]
set worldlabel3 [lindex  $worldlabellist 3 ]
set worldlabel4 [lindex  $worldlabellist 4 ]
set worldlabel5 [lindex  $worldlabellist 5 ]

set jointlabel0 "0"
set jointlabel1 "1"
set jointlabel2 "2"
set jointlabel3 "3"
set jointlabel4 "4"
set jointlabel5 "5"

# set the cycle time for display updating
set temp [emc_ini "CYCLE_TIME" "DISPLAY"]
if {[string length $temp] == 0} {
    set temp 0.200
}
set displayCycleTime [expr {int($temp * 1000 + 0.5)}]

# set the debounce time for key repeats
set debounceTime 150

if { $windows == 0 } {
    # load the generic editor "startEditor <name>"
    source bin/genedit.tcl

    # load the EMC calibrator
    source bin/emccalib.tcl

    # load the EMC data logger
    source bin/emclog.tcl

    # load the EMC performance tester
    source bin/emctesting.tcl

    # load the EMC debug level setter
    source bin/emcdebug.tcl

    # load the backplotter
    source bin/tkbackplot.tcl

    # load balloon help
    source scripts/balloon.tcl
    source scripts/emchelp.tcl
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
    wm title .runmark "Set Run Mark"

    label .runmark.msg -font {Helvetica 12 bold} -wraplength 4i -justify left -text "Set run mark at line $mark?"
    pack .runmark.msg -side top

    frame .runmark.buttons
    pack .runmark.buttons -side bottom -fill x -pady 2m
    button .runmark.buttons.ok -text "OK" -default active -command "set runMark $mark; destroy .runmark"
    button .runmark.buttons.cancel -text "Cancel" -command "destroy .runmark"
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
        geneditStart programEditor "untitled.ngc"
    }

    frame .programEditor.buttons
    pack .programEditor.buttons -side bottom -fill x -pady 2m
    button .programEditor.buttons.mark -text "Set Run Mark" -command {popupRunMark [int [.programEditor.textframe.textwin index insert]]}
    pack .programEditor.buttons.mark -side left -expand 1
}

set toolfilename [emc_ini "TOOL_TABLE" "EMCIO"]
if {[string length $toolfilename] == 0} {
    set toolfilename "emc.tbl"
}

# pop up the tool table editor
proc popupToolEditor {} {
    global toolfilename

    # create an editor as top-level ".toolEditor"
    if {[file isfile $toolfilename]} {
        geneditStart toolEditor $toolfilename
    } else {
        geneditStart toolEditor "untitled.tbl"
    }

    frame .toolEditor.buttons
    pack .toolEditor.buttons -side bottom -fill x -pady 2m
    button .toolEditor.buttons.load -text "Load Tool Table" -command {geneditSaveFile toolEditor; emc_load_tool_table $toolfilename}
    button .toolEditor.buttons.cancel -text "Cancel" -default active -command {destroy .toolEditor}
    pack .toolEditor.buttons.load .toolEditor.buttons.cancel -side left -expand 1
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
        geneditStart paramEditor "untitled.var"
    }

    # disable "Save As..." menu, since we don't support changing it
    .paramEditor.menubar.file entryconfigure 3 -state disabled

    frame .paramEditor.buttons
    pack .paramEditor.buttons -side bottom -fill x -pady 2m
    button .paramEditor.buttons.load -text "Load Parameter File" -command {geneditSaveFile paramEditor; emc_task_plan_init}
    button .paramEditor.buttons.cancel -text "Cancel" -default active -command {destroy .paramEditor}
    pack .paramEditor.buttons.load .paramEditor.buttons.cancel -side left -expand 1
}

set helpfilename [emc_ini "HELP_FILE" "DISPLAY"]
if {[string length $helpfilename] == 0} {
    set helpfilename "doc/help.txt"
}

# pop up the help window
proc popupHelp {} {
    global helpfilename

    # create an editor as top-level ".help"
    if {[file isfile $helpfilename]} {
        geneditStart help $helpfilename
    } else {
        geneditStart help "help.txt"
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
    button .help.buttons.ok -text "OK" -default active -command {destroy .help}
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
    wm title $d "EMC Diagnostics"

    label $d.task -text "Task" -width 30 -anchor center
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
    pack $d.task $d.taskhb $d.taskcmd $d.tasknum $d.taskstatus -side top -fill x
    pack $d.taskhb.lab -side left
    pack $d.taskhb.val -side right
    pack $d.taskcmd.lab -side left
    pack $d.taskcmd.val -side right
    pack $d.tasknum.lab -side left
    pack $d.tasknum.val -side right
    pack $d.taskstatus.lab -side left
    pack $d.taskstatus.val -side right

    label $d.io -text "Io" -width 30 -anchor center
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
    pack $d.io $d.iohb $d.iocmd $d.ionum $d.iostatus -side top -fill x
    pack $d.iohb.lab -side left
    pack $d.iohb.val -side right
    pack $d.iocmd.lab -side left
    pack $d.iocmd.val -side right
    pack $d.ionum.lab -side left
    pack $d.ionum.val -side right
    pack $d.iostatus.lab -side left
    pack $d.iostatus.val -side right

    label $d.motion -text "Motion" -width 30 -anchor center
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
    button $d.buttons.ok -text OK -default active -command "destroy $d"

    pack $d.buttons -side bottom -fill x -pady 2m
    pack $d.buttons.ok -side left -expand 1
    bind $d <Return> "destroy $d"
}

# pop up the about box
proc popupAbout {} {
    if {[winfo exists .about]} {
        wm deiconify .about
        raise .about
        focus .about
        return
    }
    toplevel .about
    wm title .about "About TkEmc"
    message .about.msg -aspect 1000 -justify center -font {Helvetica 12 bold} -text "TkEmc\n\nTcl/Tk GUI for Enhanced Machine Controller\n\nPublic Domain (1999)"
    frame .about.buttons
    button .about.buttons.ok -default active -text OK -command "destroy .about"
    pack .about.msg -side top
    pack .about.buttons -side bottom -fill x -pady 2m
    pack .about.buttons.ok -side left -expand 1
    bind .about <Return> "destroy .about"
}

proc popupToolOffset {} {
    global toolsetting tooloffsetsetting
    global toolentry toollengthentry tooldiameterentry

    if {[winfo exists .tooloffset]} {
        wm deiconify .tooloffset
        raise .tooloffset
        focus .tooloffset
        return
    }
    toplevel .tooloffset
    wm title .tooloffset "Set Tool Offset"

    # pre-load the entries with values
    set toolentry $toolsetting
    set toollengthentry $tooloffsetsetting
    set tooldiameterentry 0.0 ; # no value provided by controller

    frame .tooloffset.tool
    label .tooloffset.tool.label -text "Tool:" -anchor w
    entry .tooloffset.tool.entry -textvariable toolentry -width 20

    frame .tooloffset.length
    label .tooloffset.length.label -text "Length:" -anchor w
    entry .tooloffset.length.entry -textvariable toollengthentry -width 20

    frame .tooloffset.diameter
    label .tooloffset.diameter.label -text "Diameter:" -anchor w
    entry .tooloffset.diameter.entry -textvariable tooldiameterentry -width 20

    frame .tooloffset.buttons
    button .tooloffset.buttons.ok -text OK -default active -command {emc_set_tool_offset $toolentry $toollengthentry $tooldiameterentry; destroy .tooloffset}
    button .tooloffset.buttons.cancel -text Cancel -command {destroy .tooloffset}

    pack .tooloffset.tool -side top
    pack .tooloffset.tool.label .tooloffset.tool.entry -side left
    pack .tooloffset.length -side top
    pack .tooloffset.length.label .tooloffset.length.entry -side left
    pack .tooloffset.diameter -side top
    pack .tooloffset.diameter.label .tooloffset.diameter.entry -side left

    pack .tooloffset.buttons -side bottom -fill x -pady 2m
    pack .tooloffset.buttons.ok .tooloffset.buttons.cancel -side left -expand 1
    bind .tooloffset <Return> {emc_set_tool_offset $toolentry $toollengthentry $tooldiameterentry; destroy .tooloffset}

    focus .tooloffset.length.entry
    .tooloffset.length.entry select range 0 end
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
        puts stdout "can't open $programnamestring"
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

    set types {
        {"All files" *}
        {"Text files" {.txt}}
        {"NC files" {.nc .ngc}}
    }
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

set jointworld "joint"

proc toggleJointWorld {} {
    global jointworld
    if {$jointworld == "joint"} {
        set jointworld world
    } else {
        set jointworld joint
    }
}

proc toggleJogType {} {
    global jogType jogIncrement

    if {$jogType == "continuous"} {
        set jogType $jogIncrement
    } else {
        set jogType continuous
    }
}

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

    if { [emc_teleop_enable] == 1 } {
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
    
    if { [emc_teleop_enable] == 1 } {
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
proc jogStop {} {
    global activeAxis jogType

    # only stop continuous jogs; let incremental jogs finish
    if {$jogType == "continuous"} {
        emc_jog_stop $activeAxis
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

# use the top-level window as our top-level window, and name it
wm title . "TkEMC"

# create the main window top frame
set top [frame .top]
pack $top -side top -fill both -expand true

# build the top menu bar
set menubar [menu $top.menubar -tearoff 0]

# add the File menu
set filemenu [menu $menubar.file -tearoff 0]
$menubar add cascade -label "File" -menu $filemenu -underline 0
$filemenu add command -label "Open..." -command {fileDialog} -underline 0
bind . <$modifier-o> {fileDialog}
$filemenu add command -label "Edit..." -command {popupProgramEditor} -underline 0
$filemenu add command -label "Reset" -command {emc_task_plan_init} -underline 0
$filemenu add separator
$filemenu add command -label "Exit" -command {after cancel updateStatus ; destroy . ; exit} -accelerator $modifierstring+X -underline 1
bind . <$modifier-x> {after cancel updateStatus ; destroy . ; exit}

# add the View menu
set viewmenu [menu $menubar.view -tearoff 0]
$menubar add cascade -label "View" -menu $viewmenu -underline 0
$viewmenu add command -label "Tools..." -command {popupToolEditor} -underline 0
$viewmenu add command -label "Offsets and Variables..." -command {popupParamEditor} -underline 0
$viewmenu add command -label "Diagnostics..." -command {popupDiagnostics} -underline 0
$viewmenu add command -label "Backplot..." -command {popupPlot} -underline 0

# add the Settings menu
set settingsmenu [menu $menubar.settings -tearoff 0]
$menubar add cascade -label "Settings" -menu $settingsmenu -underline 0
$settingsmenu add command -label "Calibration..." -command {popupCalibration} -underline 0
$settingsmenu add command -label "Logging..." -command {popupLog} -underline 0
$settingsmenu add command -label "Testing..." -command {popupTesting} -underline 0
$settingsmenu add command -label "Debug..." -command {popupDebug} -underline 0
$settingsmenu add command -label "Font..." -command {popupFont} -underline 0

# add the Units menu
set unitsmenu [menu $menubar.units -tearoff 0]
$menubar add cascade -label "Units" -menu $unitsmenu -underline 0
$unitsmenu add command -label "auto" -command {emc_linear_unit_conversion auto} -underline 0
$unitsmenu add command -label "inches" -command {emc_linear_unit_conversion inch} -underline 0
$unitsmenu add command -label "mm" -command {emc_linear_unit_conversion mm} -underline 0
$unitsmenu add command -label "cm" -command {emc_linear_unit_conversion cm} -underline 0

# Add a scripts menu that looks for *.tcl files in tcl/scripts subdirectory
set scriptsmenu [menu $menubar.scripts -tearoff 1]
$menubar add cascade -label "Scripts" -menu $scriptsmenu -underline 1
set scriptdir scripts

if { $windows == 0 } {
    set files [exec /bin/ls $scriptdir]
    foreach file $files {
	if {[string match *.tcl $file]} {
	    set fname [file rootname $file]
	    # if it's executable, arrange for direct execution
	    if {[file executable $scriptdir/$file]} {
		$scriptsmenu add command -label $fname -command "exec $scriptdir/$file -- -ini $EMC_INIFILE &"
	    }
	}
    }
}

# add the help menu
set helpmenu [menu $menubar.help -tearoff 0]
$menubar add cascade -label "Help" -menu $helpmenu -underline 0
$helpmenu add command -label "Help..." -command {popupHelp} -underline 0
$helpmenu add check -label "Balloon help" -variable do_balloons
$helpmenu add separator
$helpmenu add command -label "Info..." -command {catch {exec sysinfo.tcl -- -ini $EMC_INIFILE}} -underline 0
$helpmenu add separator
$helpmenu add command -label "About..." -command {popupAbout} -underline 0

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
$estopmenu add command -label "Estop on" -command {emc_estop on}
$estopmenu add command -label "Estop off" -command {emc_estop off}
$estopmenu add separator
$estopmenu add command -label "Machine on" -command {emc_machine on}
$estopmenu add command -label "Machine off" -command {emc_machine off}

pack $estopbutton -side top -fill both -expand true

set modebutton [menubutton $commandbuttonsl1.mode -textvariable modelabel -direction below -relief raised -width 15]
set modemenu [menu $modebutton.menu -tearoff 0]
$modebutton configure -menu $modemenu
$modemenu add command -label "Manual" -command {emc_mode manual}
$modemenu add command -label "Auto" -command {emc_mode auto}
$modemenu add command -label "MDI" -command {emc_mode mdi}

pack $modebutton -side top -fill both -expand true

set mistbutton [menubutton $commandbuttonsl2.mist -textvariable mistlabel -direction below -relief raised -width 15]
set mistmenu [menu $mistbutton.menu -tearoff 0]
$mistbutton configure -menu $mistmenu
$mistmenu add command -label "Mist on" -command {emc_mist on}
$mistmenu add command -label "Mist off" -command {emc_mist off}

pack $mistbutton -side top -fill both -expand true

set floodbutton [menubutton $commandbuttonsl2.flood -textvariable floodlabel -direction below -relief raised -width 15]
set floodmenu [menu $floodbutton.menu -tearoff 0]
$floodbutton configure -menu $floodmenu
$floodmenu add command -label "Flood on" -command {emc_flood on}
$floodmenu add command -label "Flood off" -command {emc_flood off}

pack $floodbutton -side top -fill both -expand true

set lubebutton [menubutton $commandbuttonsl2.lube -textvariable lubelabel -direction below -relief raised -width 15]
set lubemenu [menu $lubebutton.menu -tearoff 0]
$lubebutton configure -menu $lubemenu
$lubemenu add command -label "Lube on" -command {emc_lube on}
$lubemenu add command -label "Lube off" -command {emc_lube off}

# FIXME-- there's probably a better place to put the lube button
# if {[emc_ini LUBE_WRITE_INDEX EMCIO] != ""} {
#     pack $lubebutton -side top -fill both -expand true
# }

set decrbutton [button $commandbuttonsl3t1.decr -text "<" -takefocus 0]

pack $decrbutton -side left -fill both -expand true

bind $decrbutton <ButtonPress-1> {emc_spindle decrease}
bind $decrbutton <ButtonRelease-1> {emc_spindle constant}

set spindlebutton [menubutton $commandbuttonsl3t1.spindle -textvariable spindlelabel -direction below -relief raised -width 16]
set spindlemenu [menu $spindlebutton.menu -tearoff 0]
$spindlebutton configure -menu $spindlemenu
$spindlemenu add command -label "Spindle forward" -command {emc_spindle forward}
$spindlemenu add command -label "Spindle reverse" -command {emc_spindle reverse}
$spindlemenu add command -label "Spindle off" -command {emc_spindle off}

pack $spindlebutton -side left -fill both -expand true

set incrbutton [button $commandbuttonsl3t1.incr -text ">" -takefocus 0]

pack $incrbutton -side left -fill both -expand true

bind $incrbutton <ButtonPress-1> {emc_spindle increase}
bind $incrbutton <ButtonRelease-1> {emc_spindle constant}

set brakebutton [menubutton $commandbuttonsl3.brake -textvariable brakelabel -direction below -relief raised -width 25]
set brakemenu [menu $brakebutton.menu -tearoff 0]
$brakebutton configure -menu $brakemenu
$brakemenu add command -label "Brake on" -command {emc_brake on}
$brakemenu add command -label "Brake off" -command {emc_brake off}

pack $brakebutton -side top -fill both -expand true

set abortbutton [button $commandbuttons.abort -text "ABORT" -width 15 -height 3 -takefocus 0]

pack $abortbutton -side left -fill both -expand true

bind $abortbutton <ButtonPress-1> {emc_abort}
pack $abortbutton -side top -fill both -expand true

set toolsetting 0
set tooloffsetsetting 0.0000
set offsetsetting "X0.0000 Y0.0000 Z0.0000"
set unitsetting "custom"
set oldunitsetting $unitsetting

set settings [frame $top.settings]
pack $settings -side top -anchor w -pady 2m
set toollabel [label $settings.toollabel -text "Tool:" -anchor w]
set toolsetting [label $settings.toolsetting -textvariable toolsetting -width 4 -anchor w]
set tooloffsetlabel [label $settings.tooloffsetlabel -text "Offset:" -anchor w]
set tooloffsetsetting [label $settings.tooloffsetsetting -textvariable tooloffsetsetting -width 10 -anchor e]
set offsetlabel [label $settings.offsetlabel -text "Work Offsets:" -anchor w]
set offsetsetting [label $settings.offsetsetting -textvariable offsetsetting -width 30 -anchor w]
set unitlabel [label $settings.unitlabel -textvariable unitsetting -width 6 -anchor e]

pack $toollabel -side left -padx 1m
pack $toolsetting -side left -padx 1m
pack $tooloffsetlabel -side left -padx 1m
pack $tooloffsetsetting -side left -padx 1m
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

# jointworld can be joint or world
if {  [emc_kinematics_type] == 1 } {
    set jointworld "world"
    set poslabel0 $worldlabel0
    set poslabel1 $worldlabel1
    set poslabel2 $worldlabel2
    set poslabel3 $worldlabel3
    set poslabel4 $worldlabel4
    set poslabel5 $worldlabel5
} else {
    set jointworld "joint"
    set poslabel0 $jointlabel0
    set poslabel1 $jointlabel1
    set poslabel2 $jointlabel2
    set poslabel3 $jointlabel3
    set poslabel4 $jointlabel4
    set poslabel5 $jointlabel5
}    

# read the default jog speed
set temp [emc_ini "DEFAULT_VELOCITY" "TRAJ"]
if {[string length $temp] == 0} {
    set temp 1
}
set jogSpeed [expr {int($temp * 60 + 0.5)}]

# read the max jog speed
set temp [emc_ini "MAX_VELOCITY" "TRAJ"]
if {[string length $temp] == 0} {
    set temp 1
}
set maxJogSpeed [expr {int($temp * 60 + 0.5)}]

set jogType continuous
set jogIncrement 0.0010

set move [frame $top.move]
set position [frame $move.position]
set pos0 [frame $position.pos0 -borderwidth 4]
set pos1 [frame $position.pos1 -borderwidth 4]
set pos2 [frame $position.pos2 -borderwidth 4]
set pos3 [frame $position.pos3 -borderwidth 4]
set pos4 [frame $position.pos4 -borderwidth 4]
set pos5 [frame $position.pos5 -borderwidth 4]
set bun [frame $move.bun]
set limoride [frame $bun.limoride]
set coordsel [frame $bun.coordsel]
set relabssel [frame $coordsel.relabssel]
set actcmdsel [frame $coordsel.actcmdsel]
set jog [frame $bun.jog]
set dojog [frame $jog.dojog]

pack $move -side top -fill both -expand true
pack $position -side left
pack $pos0 -side top
if { $numaxes > 1 } {
    pack $pos1 -side top
}
if { $numaxes > 2 } {
    pack $pos2 -side top
}
if { $numaxes > 3 } {
    pack $pos3 -side top
}
if { $numaxes > 4 } {
    pack $pos4 -side top
}
if { $numaxes > 5 } {
    pack $pos5 -side top
}
pack $bun -side right -anchor n ; # don't fill or expand these-- looks funny
pack $limoride -side top -pady 2m
pack $coordsel -side top -pady 2m
pack $relabssel -side left
pack $actcmdsel -side right
pack $jog -side top -pady 2m
# continuous jog button will be packed later
pack $dojog -side bottom

set pos0l [label $pos0.l -textvariable poslabel0 -width 1 -anchor w]
set pos0d [label $pos0.d -textvariable posdigit0 -width 10 -anchor w]

set pos1l [label $pos1.l -textvariable poslabel1 -width 1 -anchor w]
set pos1d [label $pos1.d -textvariable posdigit1 -width 10 -anchor w]

set pos2l [label $pos2.l -textvariable poslabel2 -width 1 -anchor w]
set pos2d [label $pos2.d -textvariable posdigit2 -width 10 -anchor w]

set pos3l [label $pos3.l -textvariable poslabel3 -width 1 -anchor w]
set pos3d [label $pos3.d -textvariable posdigit3 -width 10 -anchor w]

set pos4l [label $pos4.l -textvariable poslabel4 -width 1 -anchor w]
set pos4d [label $pos4.d -textvariable posdigit4 -width 10 -anchor w]

set pos5l [label $pos5.l -textvariable poslabel5 -width 1 -anchor w]
set pos5d [label $pos5.d -textvariable posdigit5 -width 10 -anchor w]

pack $pos0l -side left
pack $pos0d -side right
pack $pos1l -side left
pack $pos1d -side right
pack $pos2l -side left
pack $pos2d -side right
pack $pos3l -side left
pack $pos3d -side right
pack $pos4l -side left
pack $pos4d -side right
pack $pos5l -side left
pack $pos5d -side right

bind $pos0l <ButtonPress-1> {axisSelect 0}
bind $pos0d <ButtonPress-1> {axisSelect 0}
bind $pos1l <ButtonPress-1> {axisSelect 1}
bind $pos1d <ButtonPress-1> {axisSelect 1}
bind $pos2l <ButtonPress-1> {axisSelect 2}
bind $pos2d <ButtonPress-1> {axisSelect 2}
bind $pos3l <ButtonPress-1> {axisSelect 3}
bind $pos3d <ButtonPress-1> {axisSelect 3}
bind $pos4l <ButtonPress-1> {axisSelect 4}
bind $pos4d <ButtonPress-1> {axisSelect 4}
bind $pos5l <ButtonPress-1> {axisSelect 5}
bind $pos5d <ButtonPress-1> {axisSelect 5}

bind $pos0l <ButtonPress-3> {axisSelect 0; popupAxisOffset 0}
bind $pos0d <ButtonPress-3> {axisSelect 0; popupAxisOffset 0}
bind $pos1l <ButtonPress-3> {axisSelect 1; popupAxisOffset 1}
bind $pos1d <ButtonPress-3> {axisSelect 1; popupAxisOffset 1}
bind $pos2l <ButtonPress-3> {axisSelect 2; popupAxisOffset 2}
bind $pos2d <ButtonPress-3> {axisSelect 2; popupAxisOffset 2}
bind $pos3l <ButtonPress-3> {axisSelect 3; popupAxisOffset 3}
bind $pos3d <ButtonPress-3> {axisSelect 3; popupAxisOffset 3}
bind $pos4l <ButtonPress-3> {axisSelect 4; popupAxisOffset 4}
bind $pos4d <ButtonPress-3> {axisSelect 4; popupAxisOffset 4}
bind $pos5l <ButtonPress-3> {axisSelect 5; popupAxisOffset 5}
bind $pos5d <ButtonPress-3> {axisSelect 5; popupAxisOffset 5}

# set the position display font and radio button variables to their
# ini file values, if present. Otherwise leave the font alone, as it
# comes from the X resource file, and don't bother parsing this for
# the radio buttons but set them to some typical value.

set userfont [emc_ini "POSITION_FONT" "DISPLAY"]
if {$userfont != ""} {
    set fontfamily [lindex $userfont 0]
    set fontsize [lindex $userfont 1]
    set fontstyle [lindex $userfont 2]
    # now override the X resource value with the ini file font
    set nf $userfont
    $pos0l config -font $nf
    $pos0d config -font $nf
    $pos1l config -font $nf
    $pos1d config -font $nf
    $pos2l config -font $nf
    $pos2d config -font $nf
    $pos3l config -font $nf
    $pos3d config -font $nf
    $pos4l config -font $nf
    $pos4d config -font $nf
    $pos5l config -font $nf
    $pos5d config -font $nf
} else {
# FIXME-- can get the actual font from the TkEmc X resource value, which is
# a pain to parse into family-size-style. Here we'll just default the 
# buttons to these typical settings
    set fontfamily Courier
    set fontsize 56
    set fontstyle bold
}

proc setfont {} {
    global fontfamily fontsize fontstyle
    global pos0l pos0d
    global pos1l pos1d
    global pos2l pos2d
    global pos3l pos3d
    global pos4l pos4d
    global pos5l pos5d

    set nf [list $fontfamily $fontsize $fontstyle]

    $pos0l config -font $nf
    $pos0d config -font $nf
    $pos1l config -font $nf
    $pos1d config -font $nf
    $pos2l config -font $nf
    $pos2d config -font $nf
    $pos3l config -font $nf
    $pos3d config -font $nf
    $pos4l config -font $nf
    $pos4d config -font $nf
    $pos5l config -font $nf
    $pos5d config -font $nf
}

set limoridebutton [button $limoride.button -text "override limits" -command {toggleLimitOverride} -takefocus 0]
pack $limoridebutton -side top
set limoridebuttonbg [$limoridebutton cget -background]
set limoridebuttonabg [$limoridebutton cget -activebackground]
bind $limoridebutton <ButtonPress-1> {emc_override_limit}

set radiorel [radiobutton $coordsel.rel -text "relative" -variable coords -value relative -command {} -takefocus 0]
set radioabs [radiobutton $coordsel.abs -text "machine" -variable coords -value machine -command {} -takefocus 0]

set radioact [radiobutton $coordsel.act -text "actual" -variable actcmd -value actual -command {} -takefocus 0]
set radiocmd [radiobutton $coordsel.cmd -text "commanded" -variable actcmd -value commanded -command {} -takefocus 0]

set radiojoint [radiobutton $coordsel.joint -text "joint" -variable jointworld -value joint -command {} -takefocus 0]
set radioworld [radiobutton $coordsel.world -text "world" -variable jointworld -value world -command {} -takefocus 0]

pack $radiorel -side top -anchor w
pack $radioabs -side top -anchor w
pack $radioact -side top -anchor w
pack $radiocmd -side top -anchor w
pack $radiojoint -side top -anchor w
pack $radioworld -side top -anchor w

set jogtypebutton [menubutton $jog.type -textvariable jogType -direction below -relief raised -width 16]
set jogtypemenu [menu $jogtypebutton.menu -tearoff 0]
$jogtypebutton configure -menu $jogtypemenu
$jogtypemenu add command -label "continous" -command {set jogType continuous ; set jogIncrement 0.0000}
$jogtypemenu add command -label "0.0001" -command {set jogType 0.0001 ; set jogIncrement 0.0001}
$jogtypemenu add command -label "0.0010" -command {set jogType 0.0010 ; set jogIncrement 0.0010}
$jogtypemenu add command -label "0.0100" -command {set jogType 0.0100 ; set jogIncrement 0.0100}
$jogtypemenu add command -label "0.1000" -command {set jogType 0.1000 ; set jogIncrement 0.1000}
$jogtypemenu add command -label "1.0000" -command {set jogType 1.0000 ; set jogIncrement 1.0000}

pack $jogtypebutton -side top

set jognegbutton [button $dojog.neg -text "-" -takefocus 0]
set homebutton [button $dojog.home -text "home" -takefocus 0]
set jogposbutton [button $dojog.pos -text "+" -takefocus 0]

pack $jognegbutton -side left
pack $homebutton -side left
pack $jogposbutton -side left

bind $jognegbutton <ButtonPress-1> {jogNeg $activeAxis}
bind $jognegbutton <ButtonRelease-1> {jogStop}
bind $homebutton <ButtonPress-1> {emc_home $activeAxis}
bind $jogposbutton <ButtonPress-1> {jogPos $activeAxis}
bind $jogposbutton <ButtonRelease-1> {jogStop}

proc axisSelect {axis} {
    global pos0 pos1 pos2 pos3 pos4 pos5
    global activeAxis

    $pos0 config -relief flat
    $pos1 config -relief flat
    $pos2 config -relief flat
    $pos3 config -relief flat
    $pos4 config -relief flat
    $pos5 config -relief flat
    set activeAxis $axis
   
    if {$axis == 0} {
	$pos0 config -relief groove
    } elseif {$axis == 1} {
        $pos1 config -relief groove
    } elseif {$axis == 2} {
        $pos2 config -relief groove
    } elseif {$axis == 3} {
        $pos3 config -relief groove
    } elseif {$axis == 4} {
        $pos4 config -relief groove
    } elseif {$axis == 5} {
        $pos5 config -relief groove
    }
}

# force first selected axis
axisSelect 0

proc setAxisOffset {axis offset} {
    global worldlabellist
    set axislabel [lindex $worldlabellist $axis ]
    set string [format "G92 %s%f\n" $axislabel $offset]

    emc_mdi $string
}

proc popupAxisOffset {axis} {
    global axisoffsettext

    if {[winfo exists .axisoffset]} {
        wm deiconify .axisoffset
        raise .axisoffset
        focus .axisoffset
        return
    }
    toplevel .axisoffset
    wm title .axisoffset "Axis Offset"
    frame .axisoffset.input
    label .axisoffset.input.label -text "Set axis value:"
    entry .axisoffset.input.entry -textvariable axisoffsettext -width 20
    frame .axisoffset.buttons
    button .axisoffset.buttons.ok -text OK -default active -command "setAxisOffset $axis \$axisoffsettext; destroy .axisoffset"
    button .axisoffset.buttons.cancel -text Cancel -command "destroy .axisoffset"
    pack .axisoffset.input.label .axisoffset.input.entry -side left
    pack .axisoffset.input -side top
    pack .axisoffset.buttons -side bottom -fill x -pady 2m
    pack .axisoffset.buttons.ok .axisoffset.buttons.cancel -side left -expand 1
    bind .axisoffset <Return> "setAxisOffset $axis \$axisoffsettext; destroy .axisoffset"

    focus .axisoffset.input.entry
    set axisoffsettext 0.0
    .axisoffset.input.entry select range 0 end
}

proc incrJogSpeed {} {
    global jogSpeed maxJogSpeed

    if {$jogSpeed < $maxJogSpeed} {
        set jogSpeed [expr {int($jogSpeed + 1.5)}]
    }
}

proc decrJogSpeed {} {
    global jogSpeed

    if {$jogSpeed > 1} {
        set jogSpeed [expr {int($jogSpeed - 0.5)}]
    }
}

proc popupJogSpeed {} {
    global jogSpeed maxJogSpeed popupJogSpeedEntry

    if {[winfo exists .jogspeedpopup]} {
        wm deiconify .jogspeedpopup
        raise .jogspeedpopup
        focus .jogspeedpopup
        return
    }
    toplevel .jogspeedpopup
    wm title .jogspeedpopup "Set Jog Speed"

    # initialize value to current jog speed
    set popupJogSpeedEntry $jogSpeed

    frame .jogspeedpopup.input
    label .jogspeedpopup.input.label -text "Set jog speed:"
    entry .jogspeedpopup.input.entry -textvariable popupJogSpeedEntry -width 20
    frame .jogspeedpopup.buttons
    button .jogspeedpopup.buttons.ok -text OK -default active -command {set jogSpeed $popupJogSpeedEntry; destroy .jogspeedpopup}
    button .jogspeedpopup.buttons.cancel -text Cancel -command "destroy .jogspeedpopup"
    pack .jogspeedpopup.input.label .jogspeedpopup.input.entry -side left
    pack .jogspeedpopup.input -side top
    pack .jogspeedpopup.buttons -side bottom -fill x -pady 2m
    pack .jogspeedpopup.buttons.ok .jogspeedpopup.buttons.cancel -side left -expand 1
    bind .jogspeedpopup <Return> {set jogSpeed $popupJogSpeedEntry; destroy .jogspeedpopup}

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
    wm title .oridepopup "Set Feed Override"

    # initialize value to current feed override
    set popupOrideEntry $realfeedoverride

    frame .oridepopup.input
    label .oridepopup.input.label -text "Set feed override:"
    entry .oridepopup.input.entry -textvariable popupOrideEntry -width 20
    frame .oridepopup.buttons
    button .oridepopup.buttons.ok -text OK -default active -command {set feedoverride $popupOrideEntry; emc_feed_override $feedoverride; destroy .oridepopup}
    button .oridepopup.buttons.cancel -text Cancel -command "destroy .oridepopup"
    pack .oridepopup.input.label .oridepopup.input.entry -side left
    pack .oridepopup.input -side top
    pack .oridepopup.buttons -side bottom -fill x -pady 2m
    pack .oridepopup.buttons.ok .oridepopup.buttons.cancel -side left -expand 1
    bind .oridepopup <Return> {set feedoverride $popupOrideEntry; emc_feed_override $feedoverride; destroy .oridepopup}

    focus .oridepopup.input.entry
    .oridepopup.input.entry select range 0 end
}

set realfeedoverride [emc_feed_override]
set feedoverride $realfeedoverride

# read the max feed override from the ini file
set temp [emc_ini "MAX_FEED_OVERRIDE" "DISPLAY"]
if {[string length $temp] == 0} {
    set temp 1
}
set maxFeedOverride [expr {int($temp * 100 + 0.5)}]

set controls [frame $top.controls]
pack $controls -side top -anchor w

set feed [frame $controls.feed]
set feedtop [frame $feed.top]
set feedbottom [frame $feed.bottom]
set feedlabel [label $feedtop.label -text "Axis Speed:" -width 12]
set feedvalue [label $feedtop.value -textvariable jogSpeed -width 6]
set feedscale [scale $feedbottom.scale -length 270 -from 0 -to $maxJogSpeed -variable jogSpeed -orient horizontal -showvalue 0 -takefocus 0]

pack $feed -side left
pack $feedtop -side top
pack $feedbottom -side top
pack $feedlabel -side left
pack $feedvalue -side left -padx 2m ; # don't bump against label
pack $feedscale -side top

bind $feedlabel <ButtonPress-1> {popupJogSpeed}
bind $feedlabel <ButtonPress-3> {popupJogSpeed}
bind $feedvalue <ButtonPress-1> {popupJogSpeed}
bind $feedvalue <ButtonPress-3> {popupJogSpeed}

set oride [frame $controls.oride]
set oridetop [frame $oride.top]
set oridebottom [frame $oride.bottom]
set oridelabel [label $oridetop.label -text "Feed Override:" -width 12]
set oridevalue [label $oridetop.value -textvariable realfeedoverride -width 6]
set oridescale [scale $oridebottom.scale -length 270 -from 1 -to $maxFeedOverride -variable feedoverride -orient horizontal -showvalue 0 -command {emc_feed_override} -takefocus 0]

pack $oride
pack $oridetop -side top
pack $oridebottom -side top
pack $oridelabel -side left
pack $oridevalue -side left -padx 2m ; # don't bump against label
pack $oridescale -side top

bind $oridelabel <ButtonPress-1> {popupOride}
bind $oridelabel <ButtonPress-3> {popupOride}
bind $oridevalue <ButtonPress-1> {popupOride}
bind $oridevalue <ButtonPress-3> {popupOride}

proc sendMdi {mdi} {
    emc_mdi $mdi
}

set mditext ""
set mdi [frame $top.mdi]
set mdilabel [label $mdi.label -text "MDI:" -width 4 -anchor w]
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
set programstatusnamelabel [label $programstatusname.label -text "Program: "]
set programstatusnamevalue [label $programstatusname.value -textvariable programnamestring -width 50 -anchor e]

# programstatusstatus is "Status: idle" half of programstatus
set programstatusstatus [frame $programstatus.status]
set programstatusstatuslabel [label $programstatusstatus.label -text "  -  Status: "]
set programstatusstatusvalue [label $programstatusstatus.value -textvariable programstatusstring -anchor w]

pack $programstatus -side top -anchor w
pack $programstatusname $programstatusstatus -side left
pack $programstatusnamelabel $programstatusnamevalue -side left
pack $programstatusstatuslabel $programstatusstatusvalue -side left

# programframe is the frame for the button widgets for run, pause, etc.
set programframe [frame $top.programframe]
set programopenbutton [button $programframe.open -text "Open..." -command {fileDialog} -takefocus 0]
set programrunbutton [button $programframe.run -text "Run" -command {emc_run $runMark ; set runMark 0} -takefocus 0]
set programpausebutton [button $programframe.pause -text "Pause" -command {emc_pause} -takefocus 0]
set programresumebutton [button $programframe.resume -text "Resume" -command {emc_resume} -takefocus 0]
set programstepbutton [button $programframe.step -text "Step" -command {emc_step} -takefocus 0]
set programverifybutton [button $programframe.verify -text "Verify" -command {emc_run -1} -takefocus 0]

pack $programframe -side top -anchor w -fill both -expand true
pack $programopenbutton $programrunbutton $programpausebutton $programresumebutton $programstepbutton $programverifybutton -side left -fill both -expand true

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

#bind ManualBindings <KeyPress-1> {emc_feed_override 10}
#bind ManualBindings <KeyPress-2> {emc_feed_override 20}
#bind ManualBindings <KeyPress-3> {emc_feed_override 30}
#bind ManualBindings <KeyPress-4> {emc_feed_override 40}
#bind ManualBindings <KeyPress-5> {emc_feed_override 50}
#bind ManualBindings <KeyPress-6> {emc_feed_override 60}
#bind ManualBindings <KeyPress-7> {emc_feed_override 70}
#bind ManualBindings <KeyPress-8> {emc_feed_override 80}
#bind ManualBindings <KeyPress-9> {emc_feed_override 90}
#bind ManualBindings <KeyPress-0> {emc_feed_override 100}
# These key bindings really needed to be changed for 6 axis operation.
bind ManualBindings <KeyPress-0> {axisSelect 0}
bind ManualBindings <KeyPress-grave> {axisSelect 0}
bind ManualBindings <KeyPress-1> {axisSelect 1}
bind ManualBindings <KeyPress-2> {axisSelect 2}
bind ManualBindings <KeyPress-3> {axisSelect 3}
bind ManualBindings <KeyPress-4> {axisSelect 4}
bind ManualBindings <KeyPress-5> {axisSelect 5}
bind ManualBindings <KeyPress-at> {toggleCmdAct}
bind ManualBindings <KeyPress-numbersign> {toggleRelAbs}
bind ManualBindings <KeyPress-dollar> {toggleJointWorld}
bind ManualBindings <KeyPress-comma> {decrJogSpeed}
bind ManualBindings <KeyPress-period> {incrJogSpeed}
bind ManualBindings <KeyPress-c> {toggleJogType}
bind ManualBindings <KeyPress-i> {toggleJogIncrement}
bind ManualBindings <KeyPress-x> {axisSelect 0}
bind ManualBindings <KeyPress-y> {axisSelect 1}
bind ManualBindings <KeyPress-z> {axisSelect 2}
bind ManualBindings <$modifier-t> {popupToolOffset}
bind ManualBindings <KeyPress-b> {emc_brake off}
bind ManualBindings <Shift-KeyPress-B> {emc_brake on}
bind ManualBindings <$modifier-t> {popupToolOffset}
bind ManualBindings <KeyPress-F7> {toggleMist}
bind ManualBindings <KeyPress-F8> {toggleFlood}
bind ManualBindings <KeyPress-F9> {toggleSpindleForward}
bind ManualBindings <KeyPress-F10> {toggleSpindleReverse}
bind ManualBindings <Home> {emc_home $activeAxis}

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

set activeAxis 0

proc minusDone {} {
    jogStop
    bind ManualBindings <KeyPress-minus> minusDown
}

proc minusDown {} {
    global activeAxis
    bind ManualBindings <KeyPress-minus> {}
    after cancel minusDone
    jogNeg $activeAxis
}

proc minusUp {} {
    global debounceTime

    after cancel minusDone
    after $debounceTime minusDone
}

bind ManualBindings <KeyPress-minus> minusDown
bind ManualBindings <KeyRelease-minus> minusUp

proc equalDone {} {
    jogStop
    bind ManualBindings <KeyPress-equal> equalDown
}

proc equalDown {} {
    global activeAxis
    bind ManualBindings <KeyPress-equal> {}
    after cancel equalDone
    jogPos $activeAxis
}

proc equalUp {} {
    global debounceTime

    after cancel equalDone
    after $debounceTime equalDone
}

bind ManualBindings <KeyPress-equal> equalDown
bind ManualBindings <KeyRelease-equal> equalUp

proc leftDone {} {
    jogStop
    bind ManualBindings <KeyPress-Left> leftDown
}

proc leftDown {} {
    axisSelect 0
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
    jogStop
    bind ManualBindings <KeyPress-Right> rightDown
}

proc rightDown {} {
    axisSelect 0
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
    jogStop
    bind ManualBindings <KeyPress-Down> downDown
}

proc downDown {} {
    axisSelect 1
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
    jogStop
    bind ManualBindings <KeyPress-Up> upDown
}

proc upDown {} {
    axisSelect 1
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
    jogStop
    bind ManualBindings <KeyPress-Prior> priorDown
}

proc priorDown {} {
    axisSelect 2
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
    jogStop
    bind ManualBindings <KeyPress-Next> nextDown
}

proc nextDown {} {
    axisSelect 2
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

bind . <Escape> {emc_abort}

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
    wm title .error Error

    label .error.msg -font {Helvetica 12 bold} -wraplength 4i -justify left -text "Error: $err"
    pack .error.msg -side top

    frame .error.buttons
    pack .error.buttons -side bottom -fill x -pady 2m
    button .error.buttons.ok -text "OK" -default active -command "destroy .error"
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
    button .opertext.buttons.ok -text "OK" -default active -command "destroy .opertext"
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
    button .operdisplay.buttons.ok -text "OK" -default active -command "destroy .operdisplay"
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
    wm title $m "Set Font"

    set a [frame $m.radio]
    pack $a -side top -expand 1
    set f [frame $a.family]
    set z [frame $a.size]
    set y [frame $a.style]
    pack $f $z $y -side left -anchor n -expand 1

    label $f.label -text Font
    pack $f.label
    foreach i {Helvetica Courier Times} {
	radiobutton $f.b$i -text $i -variable fontfamily -value $i
	pack $f.b$i -side top -anchor w
    }

    label $z.label -text Size
    pack $z.label
    foreach i {24 30 36 48 56 64} {
	radiobutton $z.b$i -text $i -variable fontsize -value $i
	pack $z.b$i -side top -anchor w
    }

    label $y.label -text Style
    pack $y.label
    foreach i {roman bold italic} {
	radiobutton $y.b$i -text $i -variable fontstyle -value $i
	pack $y.b$i -side top -anchor w
    }

    frame $m.buttons
    pack $m.buttons -side bottom -fill x -pady 2m
    button $m.buttons.ok -text "OK" -default active -command "setfont ; destroy $m"
    button $m.buttons.cancel -text "Cancel" -command "destroy $m"
    pack $m.buttons.ok $m.buttons.cancel -side left -expand 1
    bind .setfont <Return> "setfont ; destroy $m"
}

set syncingFeedOverride 0
proc syncFeedOverride {} {
    global feedoverride realfeedoverride syncingFeedOverride

    set feedoverride $realfeedoverride
    set syncingFeedOverride 0
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
set modelabel "MANUAL"
$mistbutton config -state normal
$floodbutton config -state normal
$spindlebutton config -state normal
$brakebutton config -state normal
$mdientry config -state disabled
# Set manual bindings
setManualBindings
# Record that we're in manual display mode
set modeInDisplay "manual"

set lastjointworld $jointworld
set lastactcmd $actcmd
set lastcoords $coords
set emc_teleop_enable_command_given 0

proc updateStatus {} {
    global emc_teleop_enable_command_given
    global jointlabel0 jointlabel1 jointlabel2 jointlabel3 jointlabel4 jointlabel5
    global worldlabel0 worldlabel1 worldlabel2 worldlabel3 worldlabel4 worldlabel5
    global lastjointworld lastactcmd lastcoords
    global displayCycleTime
    global mistbutton floodbutton spindlebutton brakebutton
    global modeInDisplay
    global estoplabel modelabel mistlabel floodlabel lubelabel spindlelabel brakelabel
    global toolsetting tooloffsetsetting offsetsetting 
    global unitlabel unitsetting oldunitsetting
    global actcmd coords jointworld
    # FIXME-- use for loop for these
    global poslabel0 posdigit0
    global poslabel1 posdigit1
    global poslabel2 posdigit2
    global poslabel3 posdigit3
    global poslabel4 posdigit4
    global poslabel5 posdigit5
    global pos0l pos0d
    global pos1l pos1d
    global pos2l pos2d
    global pos3l pos3d
    global pos4l pos4d
    global pos5l pos5d
    # end
    global radiorel radiocmd radioact radioabs
    global limoridebutton limoridebuttonbg limoridebuttonabg
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
        set estoplabel "ESTOP"
    } elseif {[emc_machine] == "on"} {
        set estoplabel "ON"
    } else {
        set estoplabel "ESTOP RESET"
    }

    set temp [emc_mode]
    if {$temp != $modeInDisplay} {
        if {$temp == "auto"} {
            set modelabel "AUTO"
            $mistbutton config -state disabled
            $floodbutton config -state disabled
            $spindlebutton config -state disabled
            $brakebutton config -state disabled
            $mdientry config -state disabled
            focus .
            setAutoBindings
        } elseif {$temp == "mdi"} {
            set modelabel "MDI"
            $mistbutton config -state disabled
            $floodbutton config -state disabled
            $spindlebutton config -state disabled
            $brakebutton config -state disabled
            $mdientry config -state normal
            focus $mdientry
            $mdientry select range 0 end
            setMdiBindings
        } else {
	    if { $jointworld == "world" && [emc_kinematics_type] != 1 && ! [emc_teleop_enable ] } {
		emc_teleop_enable 1
	    }
            set modelabel "MANUAL"
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

    if {[emc_lube] == "on"} {
        set lubelabel "LUBE ON"
    } elseif {[emc_lube] == "off"} {
        set lubelabel "LUBE OFF"
    } else {
        set lubelabel "LUBE ?"
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

    # set the tool information
    set toolsetting [emc_tool]
    set tooloffsetsetting [format "%.4f" [emc_tool_offset]]

    # set the offset information
    set offsetsetting [format "X%.4f Y%.4f Z%.4f" [emc_pos_offset "X"] [emc_pos_offset "Y"] [emc_pos_offset "Z"] ]

    # set the unit information
    set unitsetting [emc_display_linear_units]
    if {$oldunitsetting != $unitsetting} {
	set oldunitsetting $unitsetting
	set_balloon $unitlabel [unithelp $unitsetting]
    }

    # format the numbers with 4 digits-dot-4 digits
    # coords relative machine
    # actcmd: commanded actual
    # FIXME-- use for loop for these
    if {$jointworld == "joint" } {
	if { $lastjointworld != "joint" } {
	    set poslabel0 $jointlabel0
	    set poslabel1 $jointlabel1
	    set poslabel2 $jointlabel2
	    set poslabel3 $jointlabel3
	    set poslabel4 $jointlabel4
	    set poslabel5 $jointlabel5
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

        set posdigit0 [format "%9.4f" [emc_joint_pos 0] ]
        set posdigit1 [format "%9.4f" [emc_joint_pos 1] ]
        set posdigit2 [format "%9.4f" [emc_joint_pos 2] ]
        set posdigit3 [format "%9.4f" [emc_joint_pos 3] ]
        set posdigit4 [format "%9.4f" [emc_joint_pos 4] ]
        set posdigit5 [format "%9.4f" [emc_joint_pos 5] ]
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
		set poslabel0 $worldlabel0
		set poslabel1 $worldlabel1
		set poslabel2 $worldlabel2
		set poslabel3 $worldlabel3
		set poslabel4 $worldlabel4
		set poslabel5 $worldlabel5
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
		set posdigit0 [format "%9.4f" [emc_rel_cmd_pos [ lindex $axiscoordmap 0 ] ] ]
		set posdigit1 [format "%9.4f" [emc_rel_cmd_pos [ lindex $axiscoordmap 1 ] ] ]
		set posdigit2 [format "%9.4f" [emc_rel_cmd_pos [ lindex $axiscoordmap 2 ] ] ]
		set posdigit3 [format "%9.4f" [emc_rel_cmd_pos [ lindex $axiscoordmap 3 ] ] ]
		set posdigit4 [format "%9.4f" [emc_rel_cmd_pos [ lindex $axiscoordmap 4 ] ] ]
		set posdigit5 [format "%9.4f" [emc_rel_cmd_pos [ lindex $axiscoordmap 5 ] ] ]
	    } elseif {$coords == "relative" && $actcmd == "actual"} {
		set posdigit0 [format "%9.4f" [emc_rel_act_pos [ lindex $axiscoordmap 0 ] ] ]
	    set posdigit1 [format "%9.4f" [emc_rel_act_pos [ lindex $axiscoordmap 1 ] ] ]
		set posdigit2 [format "%9.4f" [emc_rel_act_pos [ lindex $axiscoordmap 2 ] ] ]
		set posdigit3 [format "%9.4f" [emc_rel_act_pos [ lindex $axiscoordmap 3 ] ] ]
		set posdigit4 [format "%9.4f" [emc_rel_act_pos [ lindex $axiscoordmap 4 ] ] ]
		set posdigit5 [format "%9.4f" [emc_rel_act_pos [ lindex $axiscoordmap 5 ] ] ]
	    } elseif {$coords == "machine" && $actcmd == "commanded"} {
	    set posdigit0 [format "%9.4f" [emc_abs_cmd_pos [ lindex $axiscoordmap 0 ] ] ]
		set posdigit1 [format "%9.4f" [emc_abs_cmd_pos [ lindex $axiscoordmap 1 ] ] ]
		set posdigit2 [format "%9.4f" [emc_abs_cmd_pos [ lindex $axiscoordmap 2 ] ] ]
		set posdigit3 [format "%9.4f" [emc_abs_cmd_pos [ lindex $axiscoordmap 3 ] ] ]
		set posdigit4 [format "%9.4f" [emc_abs_cmd_pos [ lindex $axiscoordmap 4 ] ] ]
		set posdigit5 [format "%9.4f" [emc_abs_cmd_pos [ lindex $axiscoordmap 5 ] ] ]
	    } else {
		# $coords == "machine" && $actcmd == "actual"
		set posdigit0 [format "%9.4f" [emc_abs_act_pos [ lindex $axiscoordmap 0 ] ] ]
		set posdigit1 [format "%9.4f" [emc_abs_act_pos [ lindex $axiscoordmap 1 ] ] ]
		set posdigit2 [format "%9.4f" [emc_abs_act_pos [ lindex $axiscoordmap 2 ] ] ]
		set posdigit3 [format "%9.4f" [emc_abs_act_pos [ lindex $axiscoordmap 3 ] ] ]
		set posdigit4 [format "%9.4f" [emc_abs_act_pos [ lindex $axiscoordmap 4 ] ] ]
		set posdigit5 [format "%9.4f" [emc_abs_act_pos [ lindex $axiscoordmap 5 ] ] ]
	    }
	}
    }

    # color the numbers
    # FIXME-- use for loop for these

    if {[emc_joint_limit 0] != "ok"} {
        $pos0l config -foreground red
        $pos0d config -foreground red
    } elseif {[emc_joint_homed 0] == "homed"} {
        $pos0l config -foreground green
        $pos0d config -foreground green
    } else {
        $pos0l config -foreground yellow
        $pos0d config -foreground yellow
    }

    if {[emc_joint_limit 1] != "ok"} {
        $pos1l config -foreground red
        $pos1d config -foreground red
    } elseif {[emc_joint_homed 1] == "homed"} {
        $pos1l config -foreground green
        $pos1d config -foreground green
    } else {
        $pos1l config -foreground yellow
        $pos1d config -foreground yellow
    }

    if {[emc_joint_limit 2] != "ok"} {
        $pos2l config -foreground red
        $pos2d config -foreground red
    } elseif {[emc_joint_homed 2] == "homed"} {
        $pos2l config -foreground green
        $pos2d config -foreground green
    } else {
        $pos2l config -foreground yellow
        $pos2d config -foreground yellow
    }

    if {[emc_joint_limit 3] != "ok"} {
        $pos3l config -foreground red
        $pos3d config -foreground red
    } elseif {[emc_joint_homed 3] == "homed"} {
        $pos3l config -foreground green
        $pos3d config -foreground green
    } else {
        $pos3l config -foreground yellow
        $pos3d config -foreground yellow
    }

    if {[emc_joint_limit 4] != "ok"} {
        $pos4l config -foreground red
        $pos4d config -foreground red
    } elseif {[emc_joint_homed 4] == "homed"} {
        $pos4l config -foreground green
        $pos4d config -foreground green
    } else {
        $pos4l config -foreground yellow
        $pos4d config -foreground yellow
    }

    if {[emc_joint_limit 5] != "ok"} {
        $pos5l config -foreground red
        $pos5d config -foreground red
    } elseif {[emc_joint_homed 5] == "homed"} {
        $pos5l config -foreground green
        $pos5d config -foreground green
    } else {
        $pos5l config -foreground yellow
        $pos5d config -foreground yellow
    }

    # color the limit override button red if active
    if {[emc_override_limit]} {
        $limoridebutton config -background red
        $limoridebutton config -activebackground red
    } else {
        $limoridebutton config -background $limoridebuttonbg
        $limoridebutton config -activebackground $limoridebuttonabg
    }

    # set the feed override
    set realfeedoverride [emc_feed_override]
    # do me
    if {$realfeedoverride != $feedoverride && $syncingFeedOverride == 0} {
        set syncingFeedOverride 1
        after $displayCycleTime syncFeedOverride
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
    $programfiletext see $activeLine.0

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

updateStatus

