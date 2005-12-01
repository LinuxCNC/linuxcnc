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

# FIXME
# Change EMC_TOOL_ABORT in bridgeport,minimill,tkio to EMC_IO_ABORT

# convenient parallel port bit mapping functions

# parportSetBit <bit> <val> sets bit 0-11 to 0 or 1. This uses
# the IO_BASE_ADDRESS global as the base. Bits 8-11 get mapped
# to IO_BASE_ADDRESS + 2 automatically.
proc parportSetBit {bit val} {
    global IO_BASE_ADDRESS

    if {$bit < 0} {
        return
    }

    if {$bit < 8} {
        set mask [expr 1 << $bit]
        set inbyte [inb $IO_BASE_ADDRESS]
        if {$val == 0} {
            set inbyte [expr $inbyte & ~ $mask]
        } else {
            set inbyte [expr $inbyte | $mask]
        }
        outb $IO_BASE_ADDRESS $inbyte
        return
    }

    if {$bit < 12} {
        set mask [expr 1 << $bit - 8]
        set inbyte [inb [expr $IO_BASE_ADDRESS + 2]]
        if {$val == 0} {
            set inbyte [expr $inbyte & ~ $mask]
        } else {
            set inbyte [expr $inbyte | $mask]
        }
        outb [expr $IO_BASE_ADDRESS + 2] $inbyte
        return
    }

    # else out of range
    return
}

# parportCheckBit <bit> reads bit 0-11 and returns 0 or 1. This uses
# the IO_BASE_ADDRESS global as the base. Bits 8-11 get mapped
# to IO_BASE_ADDRESS + 2 automatically.
proc parportCheckBit {bit} {
    global IO_BASE_ADDRESS

    if {$bit < 0} {
        return 0
    }

    if {$bit < 8} {
        set mask [expr 1 << $bit]
        set inbyte [inb $IO_BASE_ADDRESS]
        if {[expr $inbyte & $mask] == $mask} {
            return 1
        }
        return 0
    }

    if {$bit < 12} {
        set mask [expr 1 << $bit - 8]
        set inbyte [inb [expr $IO_BASE_ADDRESS + 2]]
        if {[expr $inbyte & $mask] == $mask} {
            return 1
        }
        return 0
    }

    # else out of range
    return 0
}

# parportGetBit <bit> reads bit 0-4 and returns 0 or 1. This uses
# the IO_BASE_ADDRESS global as the base, read the next byte up,
# and shifts the result right by 3 to align the input bits properly.
proc parportGetBit {bit} {
    global IO_BASE_ADDRESS

    if {$bit < 0} {
        return 0
    }

    if {$bit < 5} {
        set mask [expr 1 << $bit]
        set inbyte [inb [expr $IO_BASE_ADDRESS + 1]]
        set inbyte [expr $inbyte >> 3]
        if {[expr $inbyte & $mask] == $mask} {
            return 1
        }
        return 0
    }

    # else out of range
    return 0
}

set toolCommand emc_tool_init
set toolState NEW_COMMAND
set toolStatus EXEC
set toolPrepped 0
set toolInSpindle 0

proc toolController {} {
    global toolCommand
    global toolPrepped toolInSpindle

    set cmd [lindex $toolCommand 0]

    if {! [string compare $cmd "emc_tool_init"]} {
        toolInit
    } elseif {! [string compare $cmd "emc_tool_halt"]} {
        toolHalt
    } elseif {! [string compare $cmd "emc_tool_abort"]} {
        toolAbort
    } elseif {! [string compare $cmd "emc_tool_prepare"]} {
        toolPrepare [lindex $toolCommand 1]
    } elseif {! [string compare $cmd "emc_tool_load"]} {
        toolLoad
    } elseif {! [string compare $cmd "emc_tool_unload"]} {
        toolUnload
    }

    emc_io_status_tool_prepped $toolPrepped
    emc_io_status_tool_in_spindle $toolInSpindle
}

proc toolInit {} {
    global toolState toolStatus

    if {$toolState == "NEW_COMMAND"} {
        set toolState S0
        set toolStatus DONE
    }
}

proc toolHalt {} {
    global toolState toolStatus

    if {$toolState == "NEW_COMMAND"} {
        set toolState S0
        set toolStatus DONE
    }
}

proc toolAbort {} {
    global toolState toolStatus

    if {$toolState == "NEW_COMMAND"} {
        set toolState S0
        set toolStatus DONE
    }
}

proc toolPrepare {tool} {
    global toolState toolStatus
    global toolPrepped toolInSpindle

    if {$toolState == "NEW_COMMAND"} {
        if {$tool != 0} {
            set toolPrepped $tool
        }
        set toolState S0
        set toolStatus DONE
    }
}

proc toolLoad {} {
    global toolState toolStatus
    global toolPrepped toolInSpindle

    if {$toolState == "NEW_COMMAND"} {
        if {$toolPrepped != 0} {
            set toolInSpindle $toolPrepped
            set toolPrepped 0
        }
        set toolState S0
        set toolStatus DONE
    }
}

proc toolUnload {} {
    global toolState toolStatus
    global toolPrepped toolInSpindle

    if {$toolState == "NEW_COMMAND"} {
        if {$toolInSpindle != 0} {
            set toolInSpindle 0
        }
        set toolState S0
        set toolStatus DONE
    }
}

set spindleCommand emc_spindle_init
set spindleState NEW_COMMAND
set spindleStatus EXEC
set spindleSpeed 0

proc doSpindleDirection {forward} {
    global SPINDLE_FORWARD_INDEX SPINDLE_FORWARD_POLARITY
    global SPINDLE_REVERSE_INDEX SPINDLE_REVERSE_POLARITY

    if {$forward > 0} {
        parportSetBit $SPINDLE_FORWARD_INDEX $SPINDLE_FORWARD_POLARITY
        parportSetBit $SPINDLE_REVERSE_INDEX [expr ! $SPINDLE_REVERSE_POLARITY]
    } elseif {$forward < 0} {
        parportSetBit $SPINDLE_FORWARD_INDEX [expr ! $SPINDLE_FORWARD_POLARITY]
        parportSetBit $SPINDLE_REVERSE_INDEX $SPINDLE_REVERSE_POLARITY
    } else {
        parportSetBit $SPINDLE_FORWARD_INDEX [expr ! $SPINDLE_FORWARD_POLARITY]
        parportSetBit $SPINDLE_REVERSE_INDEX [expr ! $SPINDLE_REVERSE_POLARITY]
    }
}

proc doSpindleIncrease {increase} {
    global SPINDLE_INCREASE_INDEX SPINDLE_INCREASE_POLARITY
    global SPINDLE_DECREASE_INDEX SPINDLE_DECREASE_POLARITY

    if {$increase > 0} {
        parportSetBit $SPINDLE_INCREASE_INDEX $SPINDLE_INCREASE_POLARITY
        parportSetBit $SPINDLE_DECREASE_INDEX [expr ! $SPINDLE_DECREASE_POLARITY]
    } elseif {$increase < 0} {
        parportSetBit $SPINDLE_INCREASE_INDEX [expr ! $SPINDLE_INCREASE_POLARITY]
        parportSetBit $SPINDLE_DECREASE_INDEX $SPINDLE_DECREASE_POLARITY
    } else {
        parportSetBit $SPINDLE_INCREASE_INDEX [expr ! $SPINDLE_INCREASE_POLARITY]
        parportSetBit $SPINDLE_DECREASE_INDEX [expr ! $SPINDLE_DECREASE_POLARITY]
    }
}

proc doSpindleBrake {on} {
    global SPINDLE_BRAKE_INDEX SPINDLE_BRAKE_POLARITY

    if {$on} {
        parportSetBit $SPINDLE_BRAKE_INDEX $SPINDLE_BRAKE_POLARITY
    } else {
        parportSetBit $SPINDLE_BRAKE_INDEX [expr ! $SPINDLE_BRAKE_POLARITY]
    }
}

proc isSpindleDirection {} {
    global SPINDLE_FORWARD_INDEX SPINDLE_FORWARD_POLARITY
    global SPINDLE_REVERSE_INDEX SPINDLE_REVERSE_POLARITY

    if {[parportCheckBit $SPINDLE_FORWARD_INDEX] == $SPINDLE_FORWARD_POLARITY} {
        return 1
    } elseif {[parportCheckBit $SPINDLE_REVERSE_INDEX] == $SPINDLE_REVERSE_POLARITY} {
        return -1
    } else {
        return 0
    }
}

proc isSpindleIncrease {} {
    global SPINDLE_INCREASE_INDEX SPINDLE_INCREASE_POLARITY
    global SPINDLE_DECREASE_INDEX SPINDLE_DECREASE_POLARITY

    if {[parportCheckBit $SPINDLE_INCREASE_INDEX] == $SPINDLE_INCREASE_POLARITY} {
        return 1
    } elseif {[parportCheckBit $SPINDLE_DECREASE_INDEX] == $SPINDLE_DECREASE_POLARITY} {
        return -1
    } else {
        return 0
    }
}

proc isSpindleBrake {} {
    global SPINDLE_BRAKE_INDEX SPINDLE_BRAKE_POLARITY

    if {[parportCheckBit $SPINDLE_BRAKE_INDEX] == $SPINDLE_BRAKE_POLARITY} {
        return on
    } else {
        return off
    }
}

proc spindleController {} {
    global spindleCommand
    global spindleSpeed

    set cmd [lindex $spindleCommand 0]

    if {! [string compare $cmd "emc_spindle_init"]} {
        spindleInit
    } elseif {! [string compare $cmd "emc_spindle_halt"]} {
        spindleHalt
    } elseif {! [string compare $cmd "emc_spindle_abort"]} {
        spindleAbort
    } elseif {! [string compare $cmd "emc_spindle_on"]} {
        spindleOn [lindex $spindleCommand 1]
    } elseif {! [string compare $cmd "emc_spindle_off"]} {
        spindleOff
    } elseif {! [string compare $cmd "emc_spindle_forward"]} {
        spindleForward
    } elseif {! [string compare $cmd "emc_spindle_reverse"]} {
        spindleReverse
    } elseif {! [string compare $cmd "emc_spindle_stop"]} {
        spindleStop
    } elseif {! [string compare $cmd "emc_spindle_increase"]} {
        spindleIncrease
    } elseif {! [string compare $cmd "emc_spindle_decrease"]} {
        spindleDecrease
    } elseif {! [string compare $cmd "emc_spindle_constant"]} {
        spindleConstant
    } elseif {! [string compare $cmd "emc_spindle_brake_release"]} {
        spindleBrakeRelease
    } elseif {! [string compare $cmd "emc_spindle_brake_engage"]} {
        spindleBrakeEngage
    }

    emc_io_status_spindle_speed $spindleSpeed
    set temp [isSpindleDirection]
    emc_io_status_spindle_direction $temp
    if {$temp == 0} {
        emc_io_status_spindle_enabled off
    } else {
        emc_io_status_spindle_enabled on
    }
    emc_io_status_spindle_increasing [isSpindleIncrease]
    emc_io_status_spindle_brake [isSpindleBrake]
}

proc spindleInit {} {
    global spindleState spindleStatus

    if {$spindleState == "NEW_COMMAND"} {
        doSpindleDirection 0
        doSpindleIncrease 0
        doSpindleBrake 1
        set spindleState S0
        set spindleStatus DONE
    }
}

proc spindleHalt {} {
    global spindleState spindleStatus

    if {$spindleState == "NEW_COMMAND"} {
        doSpindleDirection 0
        doSpindleIncrease 0
        doSpindleBrake 1
        set spindleState S0
        set spindleStatus DONE
    }
}

proc spindleAbort {} {
    global spindleState spindleStatus

    if {$spindleState == "NEW_COMMAND"} {
        set spindleState S0
        set spindleStatus DONE
    }
}

set spindleOnCnt 0
proc spindleOn {speed} {
    global spindleState spindleStatus
    global spindleSpeed
    global spindleOnCnt SPINDLE_ON_WAIT CYCLE_TIME

    if {$spindleState == "NEW_COMMAND" && [isEstop] == "on"} {
        set spindleState S0
        set spindleStatus DONE
    } elseif {$spindleState == "NEW_COMMAND" && [isEstop] == "off"} {
        doSpindleBrake 0
        set spindleOnCnt $SPINDLE_ON_WAIT
        set spindleState S1
        set spindleStatus EXEC
    } elseif {$spindleState == "S1"} {
        set spindleOnCnt [expr $spindleOnCnt - $CYCLE_TIME]
        if {$spindleOnCnt < 0} {
            set spindleState S2
        }
    } elseif {$spindleState == "S2"} {
        doSpindleDirection $speed
        set spindleSpeed $speed
        set spindleState S0
        set spindleStatus DONE
    } elseif {$spindleState == "S0"} {
    }
}

set spindleOffCnt 0
proc spindleOff {} {
    global spindleState spindleStatus
    global spindleSpeed
    global spindleOffCnt SPINDLE_OFF_WAIT CYCLE_TIME

    if {$spindleState == "NEW_COMMAND"} {
        doSpindleDirection 0
        set spindleOffCnt $SPINDLE_OFF_WAIT
        set spindleState S1
        set spindleStatus EXEC
    } elseif {$spindleState == "S1"} {
        set spindleOffCnt [expr $spindleOffCnt - $CYCLE_TIME]
        if {$spindleOffCnt < 0} {
            set spindleState S2
        }
    } elseif {$spindleState == "S2"} {
        doSpindleBrake 1
        set spindleSpeed 0
        set spindleState S0
        set spindleStatus DONE
    } elseif {$spindleState == "S0"} {
    }
}

proc spindleForward {} {
    global spindleState spindleStatus

    if {$spindleState == "NEW_COMMAND"} {
        doSpindleDirection 1
        set spindleState S0
        set spindleStatus DONE
    }
}

proc spindleReverse {} {
    global spindleState spindleStatus

    if {$spindleState == "NEW_COMMAND"} {
        doSpindleDirection -1
        set spindleState S0
        set spindleStatus DONE
    }
}

proc spindleStop {} {
    global spindleState spindleStatus

    if {$spindleState == "NEW_COMMAND"} {
        doSpindleDirection 0
        set spindleState S0
        set spindleStatus DONE
    }
}

proc spindleIncrease {} {
    global spindleState spindleStatus

    if {$spindleState == "NEW_COMMAND"} {
        doSpindleIncrease 1
        set spindleState S0
        set spindleStatus DONE
    }
}

proc spindleDecrease {} {
    global spindleState spindleStatus

    if {$spindleState == "NEW_COMMAND"} {
        doSpindleIncrease -1
        set spindleState S0
        set spindleStatus DONE
    }
}

proc spindleConstant {} {
    global spindleState spindleStatus

    if {$spindleState == "NEW_COMMAND"} {
        doSpindleIncrease 0
        set spindleState S0
        set spindleStatus DONE
    }
}

proc spindleBrakeRelease {} {
    global spindleState spindleStatus

    if {$spindleState == "NEW_COMMAND"} {
        doSpindleBrake 0
        set spindleState S0
        set spindleStatus DONE
    }
}

proc spindleBrakeEngage {} {
    global spindleState spindleStatus

    if {$spindleState == "NEW_COMMAND"} {
        doSpindleBrake 1
        set spindleState S0
        set spindleStatus DONE
    }
}

set coolantCommand emc_coolant_init
set coolantState NEW_COMMAND
set coolantStatus EXEC

proc doMist {on} {
    global MIST_COOLANT_INDEX MIST_COOLANT_POLARITY

    if {$on} {
        parportSetBit $MIST_COOLANT_INDEX $MIST_COOLANT_POLARITY
    } else {
        parportSetBit $MIST_COOLANT_INDEX [expr ! $MIST_COOLANT_POLARITY]
    }
}

proc doFlood {on} {
    global FLOOD_COOLANT_INDEX FLOOD_COOLANT_POLARITY

    if {$on} {
        parportSetBit $FLOOD_COOLANT_INDEX $FLOOD_COOLANT_POLARITY
    } else {
        parportSetBit $FLOOD_COOLANT_INDEX [expr ! $FLOOD_COOLANT_POLARITY]
    }
}

proc isMist {} {
    global MIST_COOLANT_INDEX MIST_COOLANT_POLARITY

    if {[parportCheckBit $MIST_COOLANT_INDEX] == $MIST_COOLANT_POLARITY} {
        return on
    }
    return off
}

proc isFlood {} {
    global FLOOD_COOLANT_INDEX FLOOD_COOLANT_POLARITY

    if {[parportCheckBit $FLOOD_COOLANT_INDEX] == $FLOOD_COOLANT_POLARITY} {
        return on
    }
    return off
}

proc coolantController {} {
    global coolantCommand

    set cmd [lindex $coolantCommand 0]

    if {! [string compare $cmd "emc_coolant_init"]} {
        coolantInit
    } elseif {! [string compare $cmd "emc_coolant_halt"]} {
        coolantHalt
    } elseif {! [string compare $cmd "emc_coolant_abort"]} {
        coolantAbort
    } elseif {! [string compare $cmd "emc_coolant_mist_on"]} {
        coolantMistOn
    } elseif {! [string compare $cmd "emc_coolant_mist_off"]} {
        coolantMistOff
    } elseif {! [string compare $cmd "emc_coolant_flood_on"]} {
        coolantFloodOn
    } elseif {! [string compare $cmd "emc_coolant_flood_off"]} {
        coolantFloodOff
    }

    emc_io_status_mist [isMist]
    emc_io_status_flood [isFlood]
}

proc coolantInit {} {
    global coolantState coolantStatus

    if {$coolantState == "NEW_COMMAND"} {
        doMist 0
        doFlood 0
        set coolantState S0
        set coolantStatus DONE
    }
}

proc coolantHalt {} {
    global coolantState coolantStatus

    if {$coolantState == "NEW_COMMAND"} {
        doMist 0
        doFlood 0
        set coolantState S0
        set coolantStatus DONE
    }
}

proc coolantAbort {} {
    global coolantState coolantStatus

    if {$coolantState == "NEW_COMMAND"} {
        set coolantState S0
        set coolantStatus DONE
    }
}

proc coolantMistOn {} {
    global coolantState coolantStatus

    if {$coolantState == "NEW_COMMAND"} {
        doMist 1
        set coolantState S0
        set coolantStatus DONE
    }
}

proc coolantMistOff {} {
    global coolantState coolantStatus

    if {$coolantState == "NEW_COMMAND"} {
        doMist 0
        set coolantState S0
        set coolantStatus DONE
    }
}

proc coolantFloodOn {} {
    global coolantState coolantStatus

    if {$coolantState == "NEW_COMMAND"} {
        doFlood 1
        set coolantState S0
        set coolantStatus DONE
    }
}

proc coolantFloodOff {} {
    global coolantState coolantStatus

    if {$coolantState == "NEW_COMMAND"} {
        doFlood 0
        set coolantState S0
        set coolantStatus DONE
    }
}

set auxCommand emc_aux_init
set auxState NEW_COMMAND
set auxStatus EXEC

# 'simulate' is a flag that tells the controller to ignore input bits
# and use associated output bits instead. This has the effect of looping
# the estop out bit into the estop in bit, so you can come out of estop
# without being wired to real electronics.
set simulate 0
set simEstop 1

proc doEstop {on} {
    global ESTOP_WRITE_INDEX ESTOP_WRITE_POLARITY
    global simulate simEstop

    if {$on} {
        parportSetBit $ESTOP_WRITE_INDEX $ESTOP_WRITE_POLARITY
        if {$simulate} {
            set simEstop 1
        }
    } else {
        parportSetBit $ESTOP_WRITE_INDEX [expr ! $ESTOP_WRITE_POLARITY]
        if {$simulate} {
            set simEstop 0
        }
    }
}

proc isEstop {} {
    global ESTOP_SENSE_INDEX ESTOP_SENSE_POLARITY
    global simulate simEstop

    if {$simulate} {
        if {$simEstop == 0} {
            return off
        } else {
            return on
        }
    } else {
        if {[parportGetBit $ESTOP_SENSE_INDEX] == $ESTOP_SENSE_POLARITY} {
            return on
        }
        return off
    }
}

proc checkEstop {} {
    global ESTOP_WRITE_INDEX ESTOP_WRITE_POLARITY
    global simulate simEstop

    if {$simulate} {
        if {$simEstop == 0} {
            return off
        } else {
            return on
        }
    } else {
        if {[parportCheckBit $ESTOP_WRITE_INDEX] == $ESTOP_WRITE_POLARITY} {
            return on
        }
        return off
    }
}

proc auxController {} {
    global auxCommand

    set cmd [lindex $auxCommand 0]

    if {! [string compare $cmd "emc_aux_init"]} {
        auxInit
    } elseif {! [string compare $cmd "emc_aux_halt"]} {
        auxHalt
    } elseif {! [string compare $cmd "emc_aux_abort"]} {
        auxAbort
    } elseif {! [string compare $cmd "emc_aux_estop_on"]} {
        auxEstopOn
    } elseif {! [string compare $cmd "emc_aux_estop_off"]} {
        auxEstopOff
    }

    set eIn [isEstop]
    set eOut [checkEstop]

    # if sense says it is and we haven't commanded it, make
    # sure we do
    if {$eIn == "on" && $eOut == "off"} {
        doEstop 1
        set eOut "on"
    }

    # set estop status to be "on" if either sense or command is "on"
    if {$eIn == "on" || $eOut == "on"} {
        emc_io_status_estop "on"
    } else {
        emc_io_status_estop "off"
    }
}

proc auxInit {} {
    global auxState auxStatus

    if {$auxState == "NEW_COMMAND"} {
        doEstop 1
        set auxState S0
        set auxStatus DONE
    }
}

proc auxHalt {} {
    global auxState auxStatus

    if {$auxState == "NEW_COMMAND"} {
        doEstop 1
        set auxState S0
        set auxStatus DONE
    }
}

proc auxAbort {} {
    global auxState auxStatus

    if {$auxState == "NEW_COMMAND"} {
        set auxState S0
        set auxStatus DONE
    }
}

proc auxEstopOn {} {
    global auxState auxStatus

    if {$auxState == "NEW_COMMAND"} {
        doEstop 1
        doSpindleDirection 0
        doSpindleIncrease 0
        doSpindleBrake 1
        set auxState S0
        set auxStatus DONE
    }
}

proc auxEstopOff {} {
    global auxState auxStatus

    if {$auxState == "NEW_COMMAND"} {
        doEstop 0
        set auxState S0
        set auxStatus DONE
    }
}

set lubeCommand emc_lube_init
set lubeState NEW_COMMAND
set lubeStatus EXEC

proc isLubeLevel {} {
    global LUBE_SENSE_INDEX LUBE_SENSE_POLARITY

    if {[parportGetBit $LUBE_SENSE_INDEX] == $LUBE_SENSE_POLARITY} {
        return ok
    }
    return low
}

proc lubeController {} {
    global lubeCommand

    set cmd [lindex $lubeCommand 0]

    if {! [string compare $cmd "emc_lube_init"]} {
        lubeInit
    } elseif {! [string compare $cmd "emc_lube_halt"]} {
        lubeHalt
    } elseif {! [string compare $cmd "emc_lube_abort"]} {
        lubeAbort
    } elseif {! [string compare $cmd "emc_lube_on"]} {
        lubeOn
    } elseif {! [string compare $cmd "emc_lube_off"]} {
        lubeOff
    }

    emc_io_status_lube_level [isLubeLevel]
}

proc lubeInit {} {
    global lubeState lubeStatus

    if {$lubeState == "NEW_COMMAND"} {
        # fake lube status since no sensor for on/off
        emc_io_status_lube off
        set lubeState S0
        set lubeStatus DONE
    }
}

proc lubeHalt {} {
    global lubeState lubeStatus

    if {$lubeState == "NEW_COMMAND"} {
        # fake lube status since no sensor for on/off
        emc_io_status_lube off
        set lubeState S0
        set lubeStatus DONE
    }
}

proc lubeAbort {} {
    global lubeState lubeStatus

    if {$lubeState == "NEW_COMMAND"} {
        set lubeState S0
        set lubeStatus DONE
    }
}

proc lubeOn {} {
    global lubeState lubeStatus

    if {$lubeState == "NEW_COMMAND"} {
        # fake lube status since no sensor for on/off
        emc_io_status_lube on
        set lubeState S0
        set lubeStatus DONE
    }
}

proc lubeOff {} {
    global lubeState lubeStatus

    if {$lubeState == "NEW_COMMAND"} {
        # fake lube status since no sensor for on/off
        emc_io_status_lube off
        set lubeState S0
        set lubeStatus DONE
    }
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
    wm title .about "About TkIO"
    message .about.msg -aspect 1000 -justify center -font {Helvetica 12 bold} -text "TkIO\n\nTcl/Tk IO Controller for Enhanced Machine Controller\n\nPublic Domain (2000)"
    frame .about.buttons
    button .about.buttons.ok -default active -text OK -command "destroy .about"
    pack .about.msg -side top
    pack .about.buttons -side bottom -fill x -pady 2m
    pack .about.buttons.ok -side left -expand 1
    bind .about <Return> "destroy .about"
}

# use the top-level window as our top-level window, and name it
wm title . "TkIO"

# create the main window top frame
set top [frame .top]
pack $top -side top -fill both -expand true

# build the top menu bar
set menubar [menu $top.menubar -tearoff 0]

# add the File menu
set filemenu [menu $menubar.file -tearoff 0]
$menubar add cascade -label "File" -menu $filemenu -underline 0
$filemenu add command -label "Exit" -command {after cancel doit; destroy . ; exit} -accelerator Alt+X -underline 1
bind . <Alt-x> {after cancel doit ; destroy . ; exit}

# add the help menu
set helpmenu [menu $menubar.help -tearoff 0]
$menubar add cascade -label "Help" -menu $helpmenu -underline 0
$helpmenu add command -label "About..." -command {popupAbout} -underline 0

. configure -menu $menubar

set command_string "none"
set command_type 0
set command_number 0
set echo_serial_number 0
set done_status "done"
set heartbeat 0

frame $top.iohb
label $top.iohb.lab -text "Heartbeat:" -anchor w
label $top.iohb.val -textvariable heartbeat -anchor e
frame $top.iocmd
label $top.iocmd.lab -text "Command:" -anchor w
label $top.iocmd.val -textvariable command_type -anchor e
frame $top.ionum
label $top.ionum.lab -text "Command #:" -anchor w
label $top.ionum.val -textvariable echo_serial_number -anchor e
frame $top.iostatus
label $top.iostatus.lab -text "Status:" -anchor w
label $top.iostatus.val -textvariable done_status -anchor e
pack $top.iohb $top.iocmd $top.ionum $top.iostatus -side top -fill x
pack $top.iohb.lab -side left
pack $top.iohb.val -side right
pack $top.iocmd.lab -side left
pack $top.iocmd.val -side right
pack $top.ionum.lab -side left
pack $top.ionum.val -side right
pack $top.iostatus.lab -side left
pack $top.iostatus.val -side right

# the main controller procedure
proc doit {} {
    global command_string command_type command_number
    global echo_serial_number done_status heartbeat
    global cycletime
    global toolCommand toolState toolStatus
    global spindleCommand spindleState spindleStatus
    global coolantCommand coolantState coolantStatus
    global auxCommand auxState auxStatus
    global lubeCommand lubeState lubeStatus

    if {[emc_io_read_command] == 0} {
        set command_string [emc_io_get_command]
        set command_type [emc_io_get_command_type]
        set command_number [emc_io_get_serial_number]

        if {$command_number != $echo_serial_number} {
            # got a new command
            # copy the command type into NML status
            emc_io_status_command_type $command_type
            # save the serial number for checking new commands next time
            set echo_serial_number $command_number
            # copy the serial number into NML status
            emc_io_status_echo_serial_number $echo_serial_number

            if {[string length $command_string] > 0} {
                if {[string match "emc_tool_*" $command_string]} {
                    set toolCommand $command_string
                    set toolState NEW_COMMAND
                } elseif {[string match "emc_spindle_*" $command_string]} {
                    set spindleCommand $command_string
                    set spindleState NEW_COMMAND
                } elseif {[string match "emc_coolant_*" $command_string]} {
                    set coolantCommand $command_string
                    set coolantState NEW_COMMAND
                } elseif {[string match "emc_aux_*" $command_string]} {
                    set auxCommand $command_string
                    set auxState NEW_COMMAND
                } elseif {[string match "emc_lube_*" $command_string]} {
                    set lubeCommand $command_string
                    set lubeState NEW_COMMAND
                } else {
                    # FIXME-- handle unknown commands better, perhaps with
                    # emc_io_write_error
                    puts stdout [format "unknown command ``%s''" $command_string]
                }
            }
        }
    }

    # run the controller hierarchy
    toolController
    spindleController
    coolantController
    auxController
    lubeController

    # write status
    incr heartbeat
    emc_io_status_heartbeat $heartbeat
    emc_io_write_status

    # update local labels
    if {$toolStatus == "DONE" && $spindleStatus == "DONE" && $coolantStatus == "DONE" && $auxStatus == "DONE" && $lubeStatus == "DONE"} {
        set done_status "done"
    } elseif {$toolStatus == "ERROR" || $spindleStatus == "ERROR" || $coolantStatus == "ERROR" || $auxStatus == "ERROR" || $lubeStatus == "ERROR"} {
        set done_status "error"
    } else {
        set done_status "exec"
    }
        emc_io_status_status $done_status

    # and go again
    after $cycletime doit
}

# Take a list of {<section> <var> <default value>} and
# call emc_ini to load them with their specified value or the
# default value if not found
proc iniLoad {} {
    foreach {sec var def} {
        EMCIO CYCLE_TIME 0.100
        EMCIO TOOL_TABLE smhome.tbl
        EMCIO IO_BASE_ADDRESS 0x278
        EMCIO SPINDLE_OFF_WAIT 1.0
        EMCIO SPINDLE_ON_WAIT 1.5
        EMCIO ESTOP_SENSE_INDEX 1
        EMCIO LUBE_SENSE_INDEX 2
        EMCIO ESTOP_SENSE_POLARITY 1
        EMCIO LUBE_SENSE_POLARITY 1
        EMCIO SPINDLE_FORWARD_INDEX 1
        EMCIO SPINDLE_REVERSE_INDEX 0
        EMCIO MIST_COOLANT_INDEX 6
        EMCIO FLOOD_COOLANT_INDEX 7
        EMCIO SPINDLE_DECREASE_INDEX 8
        EMCIO SPINDLE_INCREASE_INDEX 9
        EMCIO ESTOP_WRITE_INDEX 10
        EMCIO SPINDLE_BRAKE_INDEX 11
        EMCIO SPINDLE_ON_INDEX 3
        EMCIO SPINDLE_FORWARD_POLARITY 0
        EMCIO SPINDLE_REVERSE_POLARITY 0
        EMCIO MIST_COOLANT_POLARITY 0
        EMCIO FLOOD_COOLANT_POLARITY 0
        EMCIO SPINDLE_DECREASE_POLARITY 1
        EMCIO SPINDLE_INCREASE_POLARITY 1
        EMCIO ESTOP_WRITE_POLARITY 1
        EMCIO SPINDLE_BRAKE_POLARITY 0
        EMCIO SPINDLE_ENABLE_POLARITY 1
    } {
        set temp [emc_ini $var $sec]
        if {[string length $temp] == 0} {
            set temp $def
        }
        global $var
        set $var $temp
    }
}

# load the ini file vars
iniLoad

# convert CYCLE_TIME to milliseconds
set cycletime [ expr {int($CYCLE_TIME * 1000 + 0.5)}]

# connect to NML
if {[emc_io_connect] == -1} {
    exit 1
}

# run the main controller procedure
doit
