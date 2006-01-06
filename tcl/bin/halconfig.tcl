#!/bin/sh
# the next line restarts using emcsh \
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

# TODO
# add a run under the file menu with reference to setupconfig.tcl
# add the control stuff for modify and tune
# include loadrt and addf with auto refreshHal calls
# and tests for already loaded if so unload first
# tests for already linked write pins before linkxx
# flesh out the save and reload.
# remove cruft!!!!!

proc initializeConfig {} {
    global tcl_platform
    # four ordinal tests are used as this script starts
    # 1 -- ms, if yes quit - else
    # 2 -- bwidget, if no quit - else
    # 3 -- Is script running in place if no quit - else
    # 4 -- running emc2 or hal if yes run this - else
    # 4 -- question setupconfig.tcl? if yes -> go after setup
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
    # If you don't find it with this package it is available in
    # the Axis display package
    set isbwidget ""
    set isbwidget [glob -directory /usr/lib -nocomplain \
        -types d bwidget* ]
    if {$isbwidget == ""} {
        tk_messageBox -icon error -type ok -message "Darn.  I didn't find the bwidget library in the usual place\n\n/usr/lib/bwidget-##.\n\nIf this is a linux box and you have the Axis display stuff loaded, it has an older copy of Bwidget that you could link to the location shown above."
        exit
        destroy .
    } else {
        package require BWidget
    }
    
    # 3 -- Are we where we ought to be.
    set thisdir [file tail [pwd]]
    if { $thisdir != "emc2"} {
         tk_messageBox -icon error -type ok -message "The configuration script needs to be run from the main EMC2 directory"
         exit
         destroy .
    }
    
    # 4 -- is a hal running?
    # first level of test is to see if module is in there.
    set temp ""
    set tempmod ""
    set temp [lsearch [exec /sbin/lsmod] hal_lib]
    if {$temp == -1} {
        noHal
    } else {
        # check status reply for "not" 
        set tempmod [exec scripts/realtime "status"]
        set isnot [string match *not* $tempmod]
        if {$tempmod == "-1"} {
            if {$isnot != 1} {
                noHal
            }
        }
    }
    # end of initial startup tests
}

proc noHal {} {
    global demoisup
    set demoisup no
    set thisanswer [tk_messageBox -type yesnocancel -message "It looks like HAL is not loaded.  If you'd like to go to the configuration and startup script press yes.  If you'd like to start a HAL demo press no. To stop this script, press cancel."]
    switch -- $thisanswer {
        yes {
            wm withdraw .
            [exec tcl/bin/setupconfig.tcl]
        }
        no {
           # this works to start a hal but halconfig doesn't live
            set tmp [exec sudo scripts/realtime load & ]
            set demoisup yes
            after 1000 
        }
        cancel {
            killHal
        }
    }
}

    
# run the init test process
initializeConfig
    
# provide for some initial i18n -- needs lots of text work
package require msgcat
if ([info exists env(LANG)]) {
    msgcat::mclocale $env(LANG)
    msgcat::mcload "../../src/po"
}

# startup a toplevel and give it a few default characteristics
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

# trap mouseclick on window manager delete
wm protocol . WM_DELETE_WINDOW askKill
proc askKill {} {
    global configdir
    set tmp [tk_messageBox -icon error -type yesno \
        -message "Would you like to save your configuration before you exit?"]
    switch -- $tmp {
        yes {saveHal $configdir ; killHal}
        no {killHal}
    }
}

proc killHal {} {
    global demoisup
    if {$demoisup == "yes"} {
        set demoisup no
        set tmp [exec sudo scripts/realtime unload & ]
    }
    exit
    destroy .
}

# add a few default characteristics to the display
foreach class { Button Checkbutton Entry Label Listbox Menu Menubutton \
    Message Radiobutton Scale } {
    option add *$class.borderWidth 1  100
}

##
# Create a few strings that control HAL based behavior
# nodenames are the text applied to the toplevel tree nodes
# they could be internationalized
set nodenames {Components Pins Parameters Signals Functions Threads}
# searchnames is the real name to be used to reference
set searchnames {comp pin param sig funct thread}
# add a few names to test and make subnodes to the signal node
# a linkpp signal will be sorted by it's pin name so ignore here
set signodes {X Y Z A "Spindle"}

# workmodes are set from the menu
# possible workmodes include showhal modifyhal tunehal watchhal ...
# only showhal does much special now.
set workmode showhal

##########
# Use a monofont to display properly
# Build the display areas to look like this

#########################################
#File  View  Settings             Help  #
#########################################
#  Tree         # Main                  #
#  Navigation   # Text display          #
#  (frame - $tf)# (frame - $df)         #
#  ($treew)     # (text - $disp)        #
#               # (displayThis {str})   #
#########################################
#  Command area                         #
#  (frame - $cf)                        #
#########################################

# The layout is fixed sized and uses the place manager to
# fix this layout.  It can be expanded by the user after startup.

set top [frame .main -bg white]
pack $top -padx 4 -pady 4 -fill both -expand yes

# fixme clean up the underlines so they are unique under each set
set menubar [menu $top.menubar -tearoff 0]
set filemenu [menu $menubar.file -tearoff 0]
    $menubar add cascade -label [msgcat::mc "File"] \
            -menu $filemenu -underline 0
        $filemenu add command -label [msgcat::mc "Run Hal"] \
            -command {} -underline 0
        $filemenu add command -label [msgcat::mc "Refresh"] \
            -command {refreshHal} -underline 0
        $filemenu add command -label [msgcat::mc "Save"] \
            -command {saveHal save} -underline 0
        $filemenu add command -label [msgcat::mc "Save and Exit"] \
            -command {saveHal saveandexit} -underline 1
        $filemenu add command -label [msgcat::mc "Exit"] \
            -command {saveHal exit} -underline 0
set viewmenu [menu $menubar.view -tearoff 0]
    $menubar add cascade -label [msgcat::mc "View"] \
            -menu $viewmenu -underline 0
        $viewmenu add command -label [msgcat::mc "Components"] \
            -command {showHal comp} -underline 1
        $viewmenu add command -label [msgcat::mc "Pins"] \
            -command {showHal pin} -underline 1
        $viewmenu add command -label [msgcat::mc "Parameters"] \
            -command {showHal param} -underline 1
        $viewmenu add command -label [msgcat::mc "Signals"] \
            -command {showHal sig} -underline 1
        $viewmenu add command -label [msgcat::mc "Functions"] \
            -command {showHal funct} -underline 1
        $viewmenu add command -label [msgcat::mc "Threads"] \
            -command {showHal thread} -underline 1
set settingsmenu [menu $menubar.settings -tearoff 0]
    $menubar add cascade -label [msgcat::mc "Settings"] \
            -menu $settingsmenu -underline 0
        $settingsmenu add command -label [msgcat::mc "Show"] \
            -command {set workmode showhal} -underline 0
        $settingsmenu add command -label [msgcat::mc "Modify"] \
            -command {displayThis {} ; set workmode modifyhal} \
            -underline 0
        $settingsmenu add command -label [msgcat::mc "Tune"] \
            -command {set workmode tunehal} -underline 0
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

# build the tree area in the upper left
# fixme add a slider
set tf [frame $top.maint]
set treew [Tree $tf.t  -width 10]
pack $treew -side top -fill both -expand yes

# This binding to treew returns the name of the node clicked
# so a call using it includes that one arg.
# no two nodes can have the same name even with different paths
# so this return will be unique to each pin, param, or signal
$treew bindText <Button-1> {showHal   }

# build text area upper right for display
# add a slider
set df [frame $top.maind ]
set disp [text $df.tx -bg grey93 -relief flat -width 40 -wrap word]
pack $disp -side top -fill both -expand yes

# create a small lower display area for editing work.
set cf [labelframe $top.mainc -text "Control" -bg gray96 ]
# set up a temporary entry widget and button for editing
set lab [label $cf.label -text "Enter a proper HAL command :"]
set com [entry $cf.entry -textvariable halcommand]
set ex [button $cf.execute -text Execute \
    -command {exHal $halcommand} ]
pack $lab -side left -padx 5
pack $com -side left -fill x -expand yes
pack $ex -side left -padx 5

# use place manager for fixed locations of frames within top
place configure $tf -in $top -x 2 -y 2 -relheight .69 -relwidth .3
place configure $df -in $top -relx .31 -y 2 -relheight .69 -relwidth .69
place configure $cf -in $top -x 2 -rely .7 -relheight .29 -relwidth .99

# temporary hacked up entry stuff.
proc exHal {what} {
    set word0 ""
    set word1 ""
    set word2 ""
    set word3 ""
    set numwords [llength $what]
    for {set j 0} {$j < $numwords} {incr j} {
        set word$j [lindex $what $j]
        puts "[set word$j]"
    }
    exec bin/halcmd $word0 $word1 $word2 $word3
    refreshHal
}

# writeNode handles actual tree node insertion
proc writeNode {arg} {
    global treew treenodes
#    puts $arg
    set j [lindex $arg 0]
    set base [lindex $arg 1]
    set node [lindex $arg 2]
    set name [lindex $arg 3]
    set leaf [lindex $arg 4]
    $treew insert $j  $base  $node -text $name
    if {$leaf > 0} {
        lappend treenodes $node
    }
}

# listhal sets listed pin, param, and sig stuff from a running hal
# it is run from refresh of the navigation tree
proc listHal {} {
    global searchnames nodenames
    set i 0
    foreach node $searchnames {
        writeNode "$i root $node [lindex $nodenames $i] 1"
        incr i
    }
    set pinstring [exec bin/halcmd list "pin"]
    set paramstring [exec bin/halcmd list "param"]
    set sigstring [exec bin/halcmd list "sig"]
    makeNodeP param $paramstring
    makeNodeP pin $pinstring
    makeNodeSig $sigstring
}

# the tree node name needs to be nearly
# equivalent to the hal element name but since no two
# nodes can be named the same, and pins and params often are
# we append the hal type to the front of it.
# so a param like pid.0.FF0
# is broken down into "pid 0 FF0"
# a node is made below param named param+pid
# below it a node named param+pid.0
# and below a leaf param+pid.0.FF0

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
                    set i 4
                }
                5 {
                    set sssssnode "$ssssnode.$element"]
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

# signal node is not easy and assumes more about HAL than
# pins or params.  For this reason the variable signodes
# supplies a set of sig -> subnodes to build around
# then it finds dot separated sigs and builds a node with them
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
            break
        }
   
        foreach nodename $signodes {
            if {[string match *$nodename* $tmp]} {
                lappend nodesig$nodename $tmp
                set i 1
                break
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
        writeNode "$i sig $snode $tmp 1"
        incr i
    }

}

# refreshHal is the starting point for building a tree.
set opennodes "none"
proc refreshHal {} {
    global treew treenodes opennodes
    if {[info exists treenodes]} {
        if {[info exists tmpnodes]} {unset tmpnodes}
        foreach node $treenodes {
            if {[$treew itemcget $node -open]} {
                lappend tmpnodes $node
            }
        }
    }
    if {[info exists tmpnodes]} {
        set opennodes $tmpnodes
    }
    # clean out the tree
    $treew delete [$treew nodes root]
    # reread hal and make nodes
    listHal
    # read opennodes and set tree state
    foreach node $opennodes {
        if {[$treew exists $node]} {
            $treew opentree $node no
        }
    }
}

# Tree code above looks like crap but works fairly well

# create an empty array to hold modify mode elements
# array elements are created from what is clicked in tree
array set controlarray {}

# all clicks on tree node names go into showHal
# process uses it's own halcmd for showing
# action depends upon workmode which is not complete
proc showHal {which} {
    global workmode controlarray
    set thisnode $which
    set thislist [split $which "+"]
    set searchbase [lindex $thislist 0]
    set searchstring [lindex $thislist 1]
    switch -- $workmode {
        showhal {
            set thisret [exec bin/halcmd show $searchbase $searchstring]
            displayThis $thisret
        }
        modifyhal {
            set asize [array size controlarray]
            array set controlarray "$asize $which"
            set thisret [exec bin/halcmd -s show $searchbase $searchstring]
            displayAddThis $thisret
        }
        tunehal {
            displayThis "Move along.  Nothing to see here yet."
        }
        default {
            displayThis "Something went way wrong with settings."
        }
    }
}

# proc switches the insert and removal of upper right stuff
# This also removes any modify array variables
proc displayThis {str} {
  global disp controlarray
  $disp delete 1.0 end
  $disp insert end $str
  array unset controlarray {}
#  updateControl
}

# fixme from here down.
# no delete of existing display with this
# fixme, header lines for this
proc displayAddThis {str} {
    global disp controlarray
    $disp insert end "\n$str"
#    updateControl
}

# reads controlarray and builds control display
proc updateControl {} {
    global controlarray
#    puts "setup a control display from"
#    puts "[array get controlarray]"
}

# buildThis is written in anticipation of the need to use
# the string returned by clicking a tree node in different
# ways for each different type of hal element
# and for each different type of configuration task

proc buildThis {what} {
  # firstword is the type of hal element pin, param, sig, etc
  set firstword [lindex [split $what .] 0]
  # nodeshow is the remainder of the returned node name
  set nodeshow [string replace $what 0 [string length $firstword] ]
  # here we pass the pair of args to a show routine.
  # these could as easily be passed to a linkxx routine
  # the specific selected task would be a switch command here
  if {$firstword == "pin" || $firstword == "param"} {
    getHalSearch $firstword $nodeshow
  } else {
    displayThis "This is where some extra handling of the tree node return is needed."
  }
}

proc getHalSearch {which what} {
  set tmpstr "empty"
  set tmpstr [exec bin/halcmd "show" $which $what]
  displayThis $tmpstr
}

proc saveHal {which} {
    puts "I would save if I could save.  My plan is to issue bin/halcmd save net to handle the most of the work.  Will need to save loadrts and details in xxx_load.hal.  This had the arg $which with it."
    switch -- $which {
        save {}
        saveandexit {}
        exit {destroy . }
    }
}

# start the tree building process
refreshHal

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

     Halconfig is an EMC2 configuration tool.  It should be started from the emc2 directory and will require that you have started an instance of emc2 or work up a new configuration from scratch.

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
