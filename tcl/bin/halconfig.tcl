#!/bin/sh
# we need to find the tcl dir, it was exported from emc \
export EMC2_TCL_DIR
# and some apps need the emc2_bin_dir, so export that too \
export REALTIME
# the next line restarts using wish \
exec /usr/bin/wish "$0" "$@"

###############################################################
# Description:  halconfig.tcl
#               This file, shows a running hal configuration
#               and has menu for modifying and tuning
#
#  Author: Raymond E Henry
#  License: GPL Version 2
#
#  Copyright (c) 2006 All rights reserved.
#
#  Last change:
# $Revision$
# $Author$
# $Date$
###############################################################

#first define some default directories
set TCLBIN tcl/bin
set TCLSCRIPTS tcl/scripts
set TCLDIR tcl
set REALTIME scripts/realtime

if {[info exists env(EMC2_TCL_DIR)]} {
    set TCLBIN $env(EMC2_TCL_DIR)
    set TCLSCRIPTS $env(EMC2_TCL_DIR)
    set TCLBIN $TCLBIN/bin
    set TCLSCRIPTS $TCLSCRIPTS/scripts
    set TCLDIR $env(EMC2_TCL_DIR)
}

if {[info exists env(REALTIME)]} {
    set REALTIME $env(REALTIME)
}

puts "TCLBIN=$TCLBIN"

# Script accesses HAL through two modes halcmd show xxx for show
# and open halcmd -skf for building tree, watch, edit, and such 

# TO-DO
# add edit widget set
# add tune widget set
# flesh out the save and reload with modified .ini file.
# clean up and remove cruft!!!!!



#----------Testing of run environment----------
proc initializeConfig {} {
    global tcl_platform REALTIME
    # four ordinal tests are used as this script starts
    # 1 -- ms, if yes quit - else
    # 2 -- bwidget, if no quit - else
    # 3 -- Is script running in place if no quit - else
    # 4 -- running emc2 or hal if yes run this - else
    # 5 -- question setupconfig.tcl? if yes -> go after setup
    #      if no -> start scripts/demo
    
    # 1 -- since this script isn't worth a pinch of spit unless running
    # alongside a emc2 I'll use this to kill it until we find a way
    # to use it across a network
    switch $tcl_platform(platform) {
        windows {
            tk_messageBox  -icon error -type ok -message "Nothing to do, 'cause you're not running a real-time operating system.  Halconfig works best when it is interacting with a real, running HAL.  When you get this running along with EMC2 then you've got something."
            exit
            destroy .
        }
            default {
            option add *ScrolledWindow.size 14
        }
    }
    
    # 2 -- BWidget package is used for the Tree widget
    # with bdi, apt-get install bwidget will work
    # If you don't find it with this package it is available in
    # the Axis display package
    set isbwidget ""
    set isbwidget [glob -directory /usr/lib -nocomplain \
        -types d bwidget* ]
    if {$isbwidget == ""} {
        tk_messageBox -icon error -type ok -message "Can't find the bwidget package.  There is a debian bwidget package; install \nit with sudo apt-get install bwidget."
        exit
        destroy .
    } else {
        package require BWidget
    }
    
    # 3 -- Are we where we ought to be.
    #we don't need to be anywhere, but we need to reach halcmd

    set haltest [exec halcmd]
    if {$haltest == ""} {
         tk_messageBox -icon error -type ok -message "The configuration script needs to be run from the main EMC2 directory. Please add the emc2/bin dir to PATH before running this command"
         exit
         destroy .
    }
    
    #old test checking for emc2 topdir, doesn't work for installed systems
    #set thisdir [file tail [pwd]]
    #if { $thisdir != "emc2"} {
    #     tk_messageBox -icon error -type ok -message "The configuration script needs to be run from the main EMC2 directory"
    #     exit
    #     destroy .
    #}
    
    # 4 -- is a hal running?
    # first level of test is to see if module is in there.
    set temp ""
    set tempmod ""
    set temp [lsearch [exec /sbin/lsmod] hal_lib]
    if {$temp == -1} {
        noHal
    } else {
        # check status reply for "not" 
        set tempmod [exec $REALTIME "status"]
        set isnot [string match *not* $tempmod]
        if {$tempmod == "-1"} {
            if {$isnot != 1} {
                noHal
            }
        }
    }
}

# proc allows user to choose some startup stuff if env is okay.
set demoisup no
proc noHal {} {
    global demoisup REALTIME TCLBIN
    set demoisup no
    set thisanswer [tk_messageBox -type yesnocancel -message "It looks like HAL is not loaded.  If you'd like to go to the configuration and startup script press yes.  If you'd like to start a HAL demo press no. To stop this script, press cancel."]
    switch -- $thisanswer {
        yes {
            wm withdraw .
            [exec $TCLBIN/setupconfig.tcl]
        }
        no {
           # this works to start a hal but halconfig doesn't live
            set tmp [exec sudo $REALTIME load & ]
            set demoisup yes
            after 1000 
        }
        cancel {
            killHalConfig
        }
    }
}

# run the init test process
initializeConfig
#
#----------end of environment tests----------

# provide for some initial i18n -- needs lots of text work
package require msgcat
if ([info exists env(LANG)]) {
    msgcat::mclocale $env(LANG)
    msgcat::mcload "../../src/po"
    #FIXME add location for installed lang files
}


#----------open channel to halcmd ----------
#
# These procs open a channel to halcmd, send commands and receive
# replies including % return and empty lines. They work like this;
# 1 -- openHALCMD creates the channel and sets var "fid" as its
# ident. It registers fid with the event handler for "read" events.
# 2 -- exHAL sets global var chanflag to rd, sends a proper command,
# and sets "vwait chanflag" in the event handler.  So exHAL waits
# when channel read is seen while getsHAL builds a global variable
# named returnstring.
# 3 -- At the end of read, halcmd sends % a couple times
# so getsHAL watches and changes chanflag to wr which causes
# exHAL to return the value of returnstring to whomever initiated
# the command by issuing something like "set myret [exHAL argv]."

proc openHALCMD {} {
    global fid
    set fid [open "|halcmd -skf" "r+"]
    fconfigure $fid -buffering line -blocking on
    gets $fid
    fileevent $fid readable {getsHAL}
}

proc exHAL {argv} {
    global fid chanflag returnstring
    set chanflag rd
    puts $fid "${argv}"
#    puts "Proc exHAL, fid = '$fid', argv = '$argv'"
    vwait chanflag
    set tmp $returnstring
    set returnstring ""
    return $tmp
}

# getsHAL appends lines to returnstring until it receives a line with just a percent sign "%" on it
# Once the % is received, the global var chanflag is set to "wr"
set returnstring ""
proc getsHAL {} {
    global fid chanflag returnstring
    gets $fid tmp
    if {$tmp != "\%"} {
        append returnstring $tmp
    } else {
        set chanflag wr
    }
}

openHALCMD
#
#----------end of open halcmd----------


#----------start toplevel----------
#
# including size and position

wm title . "HAL Configuration"
wm protocol . WM_DELETE_WINDOW tk_
set masterwidth 700
set masterheight 450
# set fixed size for configuration display and center
set xmax [winfo screenwidth .]
set ymax [winfo screenheight .]
set x [expr ($xmax - $masterwidth )  / 2 ]
set y [expr ($ymax - $masterheight )  / 2]
wm geometry . "${masterwidth}x${masterheight}+$x+$y"
set top [frame .main ]
pack $top -padx 4 -pady 4 -fill both -expand yes

# add a few default characteristics to the display
foreach class { Button Checkbutton Entry Label Listbox Menu Menubutton \
    Message Radiobutton Scale } {
    option add *$class.borderWidth 1  100
}

# trap mouse click on window manager delete and ask to save
wm protocol . WM_DELETE_WINDOW askKill
proc askKill {} {
    global configdir
    set tmp [tk_messageBox -icon error -type yesnocancel \
        -message "Would you like to save your configuration before you exit?"]
    switch -- $tmp {
        yes {saveHAL $configdir ; killHalConfig}
        no {killHalConfig}
        cancel {return}
    }
}

# clean up a couple of possible problems during shutdown
proc killHalConfig {} {
    global demoisup fid REALTIME
    if {$demoisup == "yes"} {
        set demoisup no
        set tmp [exec sudo $REALTIME unload & ]
    }
    if {[info exists fid] && $fid != ""} {
        catch flush $fid
        catch close $fid
    }
    exit
    destroy .
}

# workmodes are set from the menu
# possible workmodes include showhal watchhal modifyhal tunehal 
set workmode showhal

##########
# Use a monofont to display properly
# Build the display areas to look like this

#########################################
#File  View  Settings             Help  #
#########################################
#  Tree         # Main                  #
#  Navigation   # Text display          #
#  (frame - $tf)# (frame - $ds)         #
#  ($treew)     # (text - $disp)        #
#               # (displayThis {str})   #
#               #########################
#               #  Command area         #
#               #  ()                   #
#########################################

# build the tree frame left side
set tf [frame $top.maint]

# Build right side display and control area
# Each mode will have a unique set of widgets inside frame
set showhal [labelframe $top.s -text Show ]
set watchhal [labelframe $top.w -text Watch ]
set modifyhal [labelframe $top.m -text Modify ]
set tunehal [labelframe $top.t -text Tune ]

# use place manager for fixed locations of frames within top
# place show for starting up
place configure $tf -in $top -x 2 -y 2 -relheight .99 -relwidth .3
place configure $showhal -in $top -relx .31 -y 2 -relheight .99 -relwidth .69

# slider process is used for several widgets
proc sSlide {f a b} {
    $f.sc set $a $b
}

# Build the menu
# fixme clean up the underlines so they are unique under each set
set menubar [menu $top.menubar -tearoff 0]
set filemenu [menu $menubar.file -tearoff 0]
    $menubar add cascade -label [msgcat::mc "File"] \
            -menu $filemenu -underline 0
        $filemenu add command -label [msgcat::mc "Run Hal"] \
            -command {} -underline 0 -state disabled
        $filemenu add command -label [msgcat::mc "Refresh"] \
            -command {refreshHAL} -underline 0
        $filemenu add command -label [msgcat::mc "Save"] \
            -command {saveHAL save} -underline 0 -state disabled
        $filemenu add command -label [msgcat::mc "Save As"] \
            -command {saveHAL saveas} -underline 0 -state disabled
        $filemenu add command -label [msgcat::mc "Save and Exit"] \
            -command {saveHAL saveandexit} -underline 1 -state disabled
        $filemenu add command -label [msgcat::mc "Exit"] \
            -command {killHalConfig} -underline 0
set viewmenu [menu $menubar.view -tearoff 0]
    $menubar add cascade -label [msgcat::mc "View"] \
            -menu $viewmenu -underline 0
        $viewmenu add command -label [msgcat::mc "Expand Tree"] \
            -command {showNode {open}} -underline 1
        $viewmenu add command -label [msgcat::mc "Collapse Tree"] \
            -command {showNode {close}} -underline 1
        $viewmenu add separator
        $viewmenu add command -label [msgcat::mc "Expand Pins"] \
            -command {showNode {pin}} -underline 1
        $viewmenu add command -label [msgcat::mc "Expand Parameters"] \
            -command {showNode {param}} -underline 1
        $viewmenu add command -label [msgcat::mc "Expand Signals"] \
            -command {showNode {sig}} -underline 1
        $viewmenu add separator
        $viewmenu add command -label [msgcat::mc "Erase Watch"] \
            -command {watchReset all} -underline 1
set settingsmenu [menu $menubar.settings -tearoff 0]
    $menubar add cascade -label [msgcat::mc "Settings"] \
            -menu $settingsmenu -underline 0
        $settingsmenu add radiobutton -label [msgcat::mc "Show"] \
            -variable workmode -value showhal -underline 0 \
            -command {showMode showhal }
        $settingsmenu add radiobutton -label [msgcat::mc "Watch"] \
            -variable workmode -value watchhal  -underline 0 \
            -command {showMode watchhal }
        $settingsmenu add radiobutton -label [msgcat::mc "Modify"] \
            -variable workmode -value modifyhal  -underline 0 \
            -command {showMode modifyhal }
        $settingsmenu add radiobutton -label [msgcat::mc "Tune"] \
            -variable workmode -value tunehal  -underline 0 \
            -command {showMode tunehal }
            
set helpmenu [menu $menubar.help -tearoff 0]
    $menubar add cascade -label [msgcat::mc "Help"] \
            -menu $helpmenu -underline 0
        $helpmenu add command -label [msgcat::mc "About"] \
            -command {showHelp about} -underline 0
        $helpmenu add command -label [msgcat::mc "Main"] \
            -command {showHelp main} -underline 0
        $helpmenu add command -label [msgcat::mc "Component"] \
            -command {showHelp component} -underline 0
        $helpmenu add command -label [msgcat::mc "Pin"] \
            -command {showHelp pin} -underline 0
        $helpmenu add command -label [msgcat::mc "Parameter"] \
            -command {showHelp parameter} -underline 0
        $helpmenu add command -label [msgcat::mc "Signal"] \
            -command {showHelp signal} -underline 0
        $helpmenu add command -label [msgcat::mc "Function"] \
            -command {showHelp function} -underline 0
        $helpmenu add command -label [msgcat::mc "Thread"] \
            -command {showHelp thread} -underline 0
. configure -menu $menubar

# build the tree widgets left side
set treew [Tree $tf.t  -width 10 -yscrollcommand "sSlide $tf" ]
set str $tf.sc
scrollbar $str -orient vert -command "$treew yview"
pack $str -side right -fill y
pack $treew -side right -fill both -expand yes
$treew bindText <Button-1> {workMode   }


#----------tree widget handlers----------
#
# the tree node name needs to be nearly
# equivalent to the hal element name but since no two
# nodes can be named the same, and pins and params often are
# we append the hal type to the front of it.
# so a param like pid.0.FF0
# is broken down into "pid 0 FF0"
# a node is made below param named param+pid
# below it a node named param+pid.0
# and below a leaf param+pid.0.FF0
#
# a global var -- treenodes -- holds the names of existing nodes
# nodenames are the text applied to the toplevel tree nodes
# they could be internationalized
set nodenames {Components Pins Parameters Signals Functions Threads}
# searchnames is the real name to be used to reference
set searchnames {comp pin param sig funct thread}
# add a few names to test and make subnodes to the signal node
# a linkpp signal is sorted by it's pin name so ignore here
set signodes {X Y Z A "Spindle"}


# refreshHAL is the starting point for building/rebuilding a tree.
# First empty tmpnodes, the list of all nodes that are open
# Look through tree for current list of open nodes
# Clean out the entire tree
# Call listHAL to read HAL for searchnames and build new tree
# Open old nodes if they still exist
set treenodes ""
proc refreshHAL {} {
    global treew treenodes oldvar
    set tmpnodes ""
    # look through tree for nodes that are displayed
    foreach node $treenodes {
        if {[$treew itemcget $node -open]} {
            lappend tmpnodes $node
        }
    }
    # clean out the old tree
    $treew delete [$treew nodes root]
    # reread hal and make new nodes
    listHAL
    # read opennodes and set tree state if they still exist
    foreach node $tmpnodes {
        if {[$treew exists $node]} {
            $treew opentree $node no
        }
    }
    showHAL $oldvar
}

# listhal gets $searchname stuff
# and calls makeNodeX with list of stuff found.
proc listHAL {} {
    global searchnames nodenames
    set i 0
    foreach node $searchnames {
        writeNode "$i root $node [lindex $nodenames $i] 1"
        set ${node}str [exHAL "list $node" ]
#        puts "this is $node --\n [set ${node}str]"
        switch -- $node {
            pin {-}
            param {
                makeNodeP $node [set ${node}str]
            }
            sig {
                makeNodeSig $sigstr
            }
            comp {-}
            funct {-}
            thread {
                makeNodeOther $node [set ${node}str]
            }
            default {}
        }
    
    incr i
    }
}

proc makeNodeP {which pstring} {
    global treew 
    # make an array to hold position counts
    array set pcounts {1 1 2 1 3 1 4 1 5 1}
    # consider each listed element
    foreach p $pstring {
        set elementlist [split $p "." ]
        set lastnode [llength $elementlist]
        set i 1
        foreach element $elementlist {
            switch $i {
                1 {
                    set snode "$which+$element"
                    if {! [$treew exists "$snode"] } {
                        set leaf [expr $lastnode - 1]
                        set j [lindex [array get pcounts 1] end]
                        writeNode "$j $which $snode $element $leaf"
                        array set pcounts "1 [incr j] 2 1 3 1 4 1 5 1"
                        set j 0
                    }
                    set i 2
                }
                2 {
                    set ssnode "$snode.$element"
                    if {! [$treew exists "$ssnode"] } {
                        set leaf [expr $lastnode - 2]
                        set j [lindex [array get pcounts 2] end]
                        writeNode "$j $snode $ssnode $element $leaf"
                        array set pcounts "2 [incr j] 3 1 4 1 5 1"
                        set j 0
                    }
                    set i 3
                }
                3 {
                    set sssnode "$ssnode.$element"
                    if {! [$treew exists "$sssnode"] } {
                        set leaf [expr $lastnode - 3]
                        set j [lindex [array get pcounts 3] end]
                        writeNode "$j $ssnode $sssnode $element $leaf"
                        array set pcounts "3 [incr j] 4 1 5 1"
                        set j 0
                    }
                    set i 4
                }
                4 {
                    set ssssnode "$sssnode.$element"
                    if {! [$treew exists "$ssssnode"] } {
                        set leaf [expr $lastnode - 4]
                        set j [lindex [array get pcounts 4] end]
                        writeNode "$j $sssnode $ssssnode $element $leaf"
                        array set pcounts "4 [incr j] 5 1"
                        set j 0
                    }
                    set i 5
                }
                5 {
                    set sssssnode "$ssssnode.$element"
                    if {! [$treew exists "$sssssnode"] } {
                        set leaf [expr $lastnode - 5]
                        set j [lindex [array get pcounts 5] end]
                        writeNode "$j $ssssnode $sssssnode $element $leaf"
                        array set pcounts "5 [incr j]"
                        set j 0
                    }
                }
              # end of node making switch
            }
           # end of element foreach
         }
        # end of param foreach
    }
    # empty the counts array in preparation for next proc call
    array unset pcounts {}
}

# signal node assumes more about HAL than pins or params.
# For this reason the variable signodes
# supplies a set of sig -> subnodes to build around
# then it finds dot separated sigs and builds a node with them
# these are most often made by linkpp sorts of commands
# finally it makes leaves under sig for remaining signals
proc makeNodeSig {sigstring} {
    global treew signodes
    # build sublists dotstring, each signode element, and remainder
    foreach nodename $signodes {
        set nodesig$nodename ""
    }
    set dotsig ""
    set remainder ""
    foreach tmp $sigstring {
        set i 0
        if {[string match *.* $tmp]} {
            lappend dotsig $tmp
            set i 1
        }
   
        foreach nodename $signodes {
            if {[string match *$nodename* $tmp]} {
                lappend nodesig$nodename $tmp
                set i 1
            }
        }
        if {$i == 0} {
            lappend remainder $tmp
        }
    }
    set i 0
    # build the signode named nodes and leaves
    foreach nodename $signodes {
        set tmpstring [set nodesig$nodename]
        if {$tmpstring != ""} {
            set snode "sig+$nodename"
            writeNode "$i sig $snode $nodename 1"
            incr i
            set j 0
            foreach tmp [set nodesig$nodename] {
                set ssnode sig+$tmp
                writeNode "$j $snode $ssnode $tmp 0"
                incr j
            }
        }
    }
    set j 0
    # build the linkpp based signals just below signode
    foreach tmp $dotsig {
        set tmplist [split $tmp "."]
        set tmpmain [lindex $tmplist 0]
        set tmpname [lindex $tmplist end] 
        set snode sig+$tmpmain
        if {! [$treew exists "$snode"] } {
            writeNode "$i sig $snode $tmpmain 1"
            incr i
        }
        set ssnode sig+$tmp
        writeNode "$j $snode $ssnode $tmp 0"
        incr j
    }
    # build the remaining leaves at the bottom of list
    foreach tmp $remainder {
        set snode sig+$tmp
        writeNode "$i sig $snode $tmp 0"
        incr i
    }

}

proc makeNodeOther {which otherstring} {
    global treew
    set i 0
    foreach element $otherstring {
        set snode "$which+$element"
        if {! [$treew exists "$snode"] } {
            set leaf 0
            writeNode "$i $which $snode $element $leaf"
        }
        incr i
    }
}

# writeNode handles tree node insertion for makeNodeX
# builds a global list -- treenodes -- but not leaves
proc writeNode {arg} {
    global treew treenodes
    scan $arg {%i %s %s %s %i} j base node name leaf
    $treew insert $j  $base  $node -text $name
    if {$leaf > 0} {
        lappend treenodes $node
    }
}

proc showNode {which} {
    global treew
    switch -- $which {
        open {-}
        close {
            foreach type {pin param sig} {
                $treew ${which}tree $type
            }
        }
        pin {-}
        param {-}
        sig {
            foreach type {pin param sig} {
                $treew closetree $type
            }
            $treew opentree $which
            $treew see $which
        }
        default {}
    }
    focus -force $treew
}

# Tree code above looks like crap but works fairly well
# Startup needs [showMode "showhal"] before refreshHAL
#
#----------end of tree building processes----------

# each element of controlarray is a complete node name
array set controlarray {}

# showmode handles the menu selection of mode
# modelist is used to separate first time from repeats
set modelist {0 0 0 0}
proc showMode {mode} {
    global top showhal watchhal modifyhal tunehal modelist
    switch -- $mode {
        showhal {
            if {![lindex $modelist 0]} {
                makeShow
                lset modelist 0 1
            } 
        }
        watchhal {
            if {![lindex $modelist 1]} {
                makeWatch
                lset modelist 1 1
            }
        }
        modifyhal {
            if {![lindex $modelist 2]} {
                makeModify
                lset modelist 2 1
            }
        }
        tunehal {
            if {![lindex $modelist 3]} {
                makeTune
                lset modelist 3 1
            }
        }
        default {
        }
    }
    # handles swap of the display frames if needed
    if {![winfo ismapped [set $mode]]} {
        foreach eachdisplay {showhal watchhal modifyhal tunehal} {
            place forget [set $eachdisplay]
        }
        place configure [set $mode] -in $top -relx .31 -y 2 \
            -relheight .99 -relwidth .69
    }
}

set axnum 0
set numaxes 4
set oldvar "zzz"
# build show mode right side
proc makeShow {} {
    global showhal disp
    set what full
    if {$what == "full"} {
        set stf [frame $showhal.tf]
        set disp [text $stf.tx -relief flat -width 40 -wrap word \
            -height 28 -yscrollcommand "sSlide $stf"]
        set stt $stf.sc
        scrollbar $stt -orient vert -command "$disp yview"
        pack $stt -side right -fill y
        pack $disp -side right -fill both -expand yes
        set seps [frame $showhal.sep -bg black -borderwidth 2]
        set cfent [frame $showhal.command]
        set lab [label $cfent.label -text "Enter HAL command :"]
        set com [entry $cfent.entry -textvariable halcommand]
        bind $com <KeyPress-Return> {exHAL $halcommand ; refreshHAL}
        set ex [button $cfent.execute -text Execute \
            -command {exHAL $halcommand ; refreshHAL} ]
        pack $lab -side left -padx 5 -pady 3
        pack $com -side left -fill x -expand yes -pady 3
        pack $ex -side left -padx 5 -pady 3
        pack $stf $seps $cfent -side top -fill x -expand yes
    }
}

proc makeWatch {} {
    global cisp watchhal
    set cisp [canvas $watchhal.c ]
    pack $cisp -side right -fill both -expand yes
}

proc makeModify {} {
    global modifyhal modtext focusname mcsets
    set mcsets { {loadrt 1} {unloadrt 1} {addf 1} {delf 1} \
        {linkpp 2} {linkps 2} {linksp 2} {newsig 2} {delsig 1} }
    set moddisplay [frame $modifyhal.textframe ]
    set modtext [text $moddisplay.text -width 40 -height 8 -wrap word \
        -takefocus 0 -relief groove]
    pack $modtext -side top -fill both -expand yes
    grid configure $moddisplay -row 0 -column 0 -columnspan 4 -sticky nsew
    set j 1
    foreach cset $mcsets {
        set halmodcmd ""
        scan $cset { %s %i } commandname numvars
        if {$numvars == 2} {
            set colspan 1
        } else {
            set colspan 2
        }
        label $modifyhal.$commandname -text "$commandname: " -anchor w
        grid configure $modifyhal.$commandname -row $j -column 0 -sticky nsew
        for {set i 1} {$i <= $numvars} {incr i} {
            set $commandname$i ""
            entry $modifyhal.e$commandname$i -textvariable $commandname$i \
                -bg white
            bind $modifyhal.e$commandname$i <Button-1> \
                "copyVar $modifyhal.e$commandname$i"
            grid configure $modifyhal.e$commandname$i -row $j -column $i \
                -columnspan $colspan -sticky nsew
        }
        if {$i == "2"} {
            set tmp [subst {$commandname \$${commandname}1}]
        } else {
            set tmp [subst {$commandname \$${commandname}1 \$${commandname}2}]
        }
        button $modifyhal.b$commandname -text "Execute" \
            -command [subst {modHAL "$tmp"}]
        grid configure $modifyhal.b$commandname -row $j -column 3 -sticky nsew
        incr j
    }
}

array set modtypes {
    comp ".main.m.eloadrt1 .main.m.eunloadrt1"
    funct ".main.m.eaddf1 .main.m.edelf1"
    thread ".main.m.eaddf1"
    pin ".main.m.elinkpp1 .main.m.elinkpp2 .main.m.elinkps1 \
        .main.m.elinksp2" 
    sig ".main.m.elinkps2 .main.m.elinksp1 .main.m.enewsig1 \
        .main.m.edelsig1"
}

set modlist ".main.m.eloadrt1 .main.m.eunloadrt1 .main.m.eaddf1 .main.m.edelf1 .main.m.elinkpp1 .main.m.elinkpp2 .main.m.elinkps1 .main.m.elinksp2 .main.m.elinkps2 .main.m.elinksp1 .main.m.enewsig1 .main.m.edelsig1"

# FIXME allow for more than one tunable system
# FIXME get the number of axes from ini if available
set numaxes 4
proc makeTune {} {
    global tunehal numaxes
    # tunelist holds the found tunable modules
    set tunelist ""
    set ret [exHAL "list comp"]
    foreach sys { ppmc stg m5i20 evoreg stepgen freqgen  } {
        set tmp [lsearch -inline -regexp $ret $sys]
        if {$tmp != -1} {
            append tunelist $tmp
        }
    }
    # we start listing possible tuning variables
    set pidlist {Pgain Igain Dgain FF0 FF1 bias deadband}
    set hal_ppmc "$pidlist ppmc.0.encoder.00.scale"
    set tb [frame $tunehal.buttonframe -bg red ]
    for {set j 0 } {$j < $numaxes } {incr j} {
        set tmpb [button $tb.b$j -text "Axis $j" -command "tuneAxis $j"]
        pack $tmpb -side left -fill x -expand yes
    }
    grid configure $tb -columnspan 3 -sticky nsew
    # create the top labels
#    set axlabel [label $tunehal.label -text "Axis : " -anchor e]
#    set axname [label $tunehal.lname -textvariable axnum]
    set i 1
#    grid configure $axlabel $axname -sticky nsew
    # create the set of tuning widgets
    foreach var [set $tunelist] {
        set na [label $tunehal.l$i -text "$var : " -anchor e]
        set ent [entry $tunehal.e$i -textvariable var$i]
        set bt [button $tunehal.b$i -text set \
            -command [setTuneVar "$i"]]
        grid configure $na $ent $bt -sticky nsew
        incr i
    }
    tuneAxis 0
    tuneHAL 0
}

set lastaxistuned 0
proc tuneAxis {which} {
    global tunehal numaxes lastaxistuned
    $tunehal.buttonframe.b$lastaxistuned configure -bg gray98
    $tunehal.buttonframe.b$which configure -bg yellow
    set lastaxistuned $which
}

# all clicks on tree node names go into workMode
# action depends upon selected mode and node
# oldvar keeps the last HAL variable for refresh

proc workMode {which} {
    global workmode oldvar thisvar
    set thisvar $which
    switch -- $workmode {
        showhal {
            showHAL $which
        }
        watchhal {
            watchHAL $which
        }
        # doesn't do anything now with bind to entry widgets
        modifyhal {
            setModifyVar $which
        }
        tunehal {
            tuneHAL $which
        }
        default {
            swapDisplay display showhal
            displayThis "Mode went way wrong."
        }
    }
    set oldvar $which
}


# process uses it's own halcmd show so that displayed
# info looks like what is in the Hal_Introduction.pdf
proc showHAL {which} {
    global disp
    if {![info exists disp]} {return}
    if {$which == "zzz"} {
        displayThis "Select a node to show."
        return
    }
    set thisnode $which
    set thislist [split $which "+"]
    set searchbase [lindex $thislist 0]
    set searchstring [lindex $thislist 1]
    set thisret [exec halcmd show $searchbase $searchstring]
    displayThis $thisret
}


set watchlist ""
set watchstring ""
proc watchHAL {which} {
    global watchlist watchstring watching cisp
    if {$which == "zzz"} {
        $cisp create text 40 [expr 1 * 20 + 12] -anchor w -tag firstmessage\
            -text "<-- Select a Leaf.  Click on its name."
        set watchlist ""
        set watchstring ""
        return
    } else {
        $cisp delete firstmessage
    }
    # return if variable is already used.
    if {[lsearch $watchlist $which] != -1} {
        return
    }
    lappend watchlist $which
    set i [llength $watchlist]
    set label [lindex [split $which +] end]
    set tmplist [split $which +]
    set vartype [lindex $tmplist 0]
    set varname [lindex $tmplist end]
    set ret [exHAL "show $vartype $varname"]
    if {[lsearch $ret "bit"] != -1 } {
        $cisp create oval 20 [expr $i * 20 + 5] 35 [expr $i * 20 + 20] \
            -fill firebrick4 -tag oval$i
        $cisp create text 80 [expr $i * 20 + 12] -text $label \
            -anchor w -tag $label
    } else {
        # other gets a text display for value
        $cisp create text 10 [expr $i * 20 + 12] -text "xxxx" \
            -anchor w -tag text$i
        $cisp create text 80 [expr $i * 20 + 12] -text $label \
            -anchor w -tag $label
        }
    set tmplist [split $which +]
    set vartype [lindex $tmplist 0]
    set varname [lindex $tmplist end]
    lappend watchstring "$i $vartype $varname "
    if {$watching == 0} {watchLoop} 
}

# watchHAL prepares a string of {i HALtype name} sets
# watchLoop submits these to halcmd and sets canvas
# color or value based on reply
set watching 0
proc watchLoop {} {
    global cisp watchstring watching workmode
    set watching 1
    set which $watchstring
    foreach var $which {
        scan $var {%i %s %s} cnum vartype varname
        if {$vartype == "sig" } {
            set ret [lindex [exHAL "show $vartype $varname"] 1]
        } else {
            set ret [lindex [exHAL "show $vartype $varname"] 3]
        }
        if {$ret == "TRUE"} {
            $cisp itemconfigure oval$cnum -fill yellow
        } elseif {$ret == "FALSE"} {
            $cisp itemconfigure oval$cnum -fill firebrick4
        } else {
            set value [expr $ret]
            $cisp itemconfigure text$cnum -text $value
        }
    }
    if {$workmode == "watchhal"} {
        after 1000 watchLoop
    } else {
        set watching 0
    }
}

proc watchReset {del} {
    global watchlist cisp
    $cisp delete all
    switch -- $del {
        all {
            watchHAL zzz
            return
        }
        default {
            set place [lsearch $watchlist $del]
            if {$place != -1 } {
            set watchlist [lreplace $watchlist $place]
                foreach var $watchlist {
                    watchHAL $var
                }
            } else {            
                watchHAL zzz
            }
        }
    }
}

# modHAL gets the return from a halcmd and displays it.
proc modHAL {command} {
    global modtext
    set str [exHAL "$command"]
    $modtext delete 1.0 end
    $modtext insert end $str
    refreshHAL
}

proc setModifyVar {which} {
    global modtext treenodes modifyhal modlist modtypes
    # test to see if which is a leaf or node
    set isleaf [lsearch $treenodes $which]
    if {$isleaf == -1} {
        foreach w $modlist {
            $w configure -bg white
        }
        set tmp [split $which +]
        scan $tmp { %s %s } haltype varname
        # setup mode of entry widgets in modifyhal so they match
        # haltype above and so a click on available widget
        # inserts the varname above
        switch -- $haltype {
            pin {
                set str "Click a highlighted entry where $which should go."
                set tmp [lindex [array get modtypes pin] 1]
                foreach widget $tmp {
                    $widget configure -bg lightgreen
                }
            }
            param {
                set str "Nothing to be done for parameters here. Try the tuning page"
            }
            sig {
                set str "Click a highlighted entry where $which should go."
                set tmp [lindex [array get modtypes sig] 1]
                foreach widget $tmp {
                    $widget configure -bg lightgreen
                }
            }
            comp {
                set str "Click a highlighted entry where $which should go."
                set tmp [lindex [array get modtypes comp] 1]
                foreach widget $tmp {
                    $widget configure -bg lightgreen
                }
            }
            funct {
                set str "Click a highlighted entry where $which should go."
                set tmp [lindex [array get modtypes funct] 1]
                foreach widget $tmp {
                    $widget configure -bg lightgreen
                }
            }
            thread {
                set str "Click a highlighted entry where $which should go."
                set tmp [lindex [array get modtypes thread] 1]
                foreach widget $tmp {
                    $widget configure -bg lightgreen
                }
            }
        }            
    } else {
        set str "$which is not a leaf, try again"
    }
    $modtext delete 1.0 end
    $modtext insert end $str
}

proc copyVar {var} {
    global thisvar
#    puts $var
    set tmpvar [lindex [split $thisvar +] end]
    $var insert 0 $tmpvar

}

proc modMod {which} {


}   
proc halMOD {argv} {
    
}

proc setTuneVar {which} {

}

proc tuneHAL {which} {
    

}



# proc switches the insert and removal of upper right text
# This also removes any modify array variables
proc displayThis {str} {
    global disp controlarray
    $disp delete 1.0 end
    $disp insert end $str
}


#----------start up the displays----------
showMode showhal
refreshHAL


#----------save config----------
#
# save will assume that restarting is from a comp and netlist
# we need to build these files and copy .ini for the new
proc saveHAL {which} {
    switch -- $which {
        save {
            displayThis [exec halcmd save "comp"]
        }
        saveas {}
        saveandexit {
            exit {destroy . }
        }
    }
}


#----------Help processes----------
#
proc showHelp {which} {
    global helpabout helpmain helpcomponent helppin helpparameter
    global helpsignal helpfunction helpthread
    switch -- $which {
        about {displayThis  $helpabout}
        main {displayThis  $helpmain}
        component {displayThis  $helpcomponent}
        pin {displayThis  $helppin}
        parameter {displayThis  $helpparameter}
        signal {displayThis  $helpsignal}
        function {displayThis  $helpfunction}
        thread {displayThis  $helpthread}
        default {I'm lost here in help land}
    }
}

# Help includes sections for each of these
# Components Pins Parameters Signals Functions Threads

set helpabout {
     Copyright Raymond E Henry. 2006
     License: GPL Version 2

     Halconfig is an EMC2 configuration tool.  It should be started from the emc2 directory and will require that you have started an instance of emc2 or work up a new configuration starting with a demo rt script.

     This script is not for the faint hearted and carries no warranty or liability for its use to the extent allowed by law.}

set helpmain {
    You can show any part of a running hal by clicking on the node names in the tree in the upper left side of the display.  You can expand a node if it has a + in front and then click on these names to show their attributes as well.  Attributes are displayed in the same box you are reading now.

    The settings menu will provide the major way of moving from showing HAL attributes to modifying them or tuning them.  When working in modify mode, various control buttons are shown in the lower control frame.
}

set helpcomponent {
    This is the component display
}

set helppin {
    This is the pin help
}

set helpparameter {
    This is the parameter help
}

set helpsignal {
    This is the signal help
}

set helpfunction {
    This is the function help
}

set helpthread {
    This is the thread help
}

#
#----------end of help processes----------

# temp testing stuff
# set inf globals
# puts "Temp listing $inf "
# puts [info $inf]

