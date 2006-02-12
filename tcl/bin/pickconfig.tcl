#!/bin/sh
# this line restarts using wish \
exec wish "$0" "$@"

###############################################################
# Description:  pickconfig.tcl
#               This file validates an emc2 configuration
#               passed to it, or prompts the user to choose
#               one.
#
#  Author: John Kasunich
#  License: GPL Version 2
#
#  Copyright (c) 2005 All rights reserved.
#
#  Last change:
# $Revision$
# $Author$
# $Date$
###############################################################
#
# usage:
#    pickconfig <config-path>
#
#    <config-path> is one or more directories (separated with
#    colons) in which pickconfig should search for configs.
#    pickconfig will open a GUI window displaying all configs
#    that it finds and ask the user to choose one, then print
#    the path to the chosen config and exit.
#
###############################################################

# This logo is used on all the pages
set wizard_image_search "/etc/emc2/emc2-wizard.gif /usr/local/etc/emc2/emc2-wizard.gif emc2-wizard.gif"

foreach wizard_image $wizard_image_search {
    if { [file exists $wizard_image] } {
        set logo [image create photo -file $wizard_image]
        break
    }
}

option add *font {Helvetica -12}
option add *Text*background white
option add *Entry*background white
option add *Listbox*background white

################### PROCEDURE DEFINITIONS #####################

# Internationalisation (i18n)
# in order to use i18n, all the strings will be called [msgcat::mc "string-foo"]
# instead of "string-foo".
# Thus msgcat searches for a translation of the string, and in case one isn't 
# found, the original string is used.
# In order to properly use locale's the env variable LANG is queried.
# If LANG is defined, then the folder src/po is searched for files
# called *.msg, (e.g. en_US.msg).
# That file should contain all the translations regarding tcl scripts 
# (tkemc, mini, setupconfig.tcl, etc)

package require msgcat
if ([info exists env(LANG)]) {
    msgcat::mclocale $env(LANG)
    msgcat::mcload "../../src/po"
}

proc initialize_config {} {
    # need bwidget
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
}

# main button callback, it assigns the button name to 'choice'

proc button_pushed { button_name } {
    global choice

    set choice $button_name
}

# slider process is used for several widgets
proc sSlide {f a b} {
    $f.sc set $a $b
}

# called when user clicks tree node
proc node_clicked {} {
    global tree detail_box button_ok inifile

    set node [$tree selection get]
    if {$node == ""} return
    
    $tree selection set $node
    if { [ regexp {.*\.ini$} $node ] == 1 } {
	# an ini node, acceptable
	# enable changes to the details widget
	$detail_box configure -state normal
	# remove old text
	$detail_box delete 1.0 end
	# add new text
	set readme [ file join [ file dirname $node ] README ]
	if { [ file isfile $readme ] } {
	    # description found, read it
	    set descr [ read -nonewline [ open $readme ]]
	    # reformat - remove line breaks, preserve paragraph breaks
	    regsub -all {([^\n])\n([^\n])} $descr {\1 \2} descr
	    # and display it
	    $detail_box insert end $descr
	} else {
	    # no description, gotta tell the user something
	    $detail_box insert end [msgcat::mc "No details available."]
	}
	# lock it again
	$detail_box configure -state disabled
	# save selection
	set inifile $node
        # enable the OK button
	$button_ok configure -state normal
	bind . <Return> {button_pushed OK}
    } else {
	# not an ini node, can't use it
	# enable changes to the details widget
	$detail_box configure -state normal
	# remove old text
	$detail_box delete 1.0 end
	# lock it again
	$detail_box configure -state disabled
	# clear selection
	set inifile ""
	# disable the OK button
	$button_ok configure -state disabled
	bind . <Return> ""

    }
}

################ MAIN PROGRAM STARTS HERE ####################

# parse command line 
set configs_path [ lindex $argv 0 ]
if { $configs_path == "" } {
    puts stderr [ msgcat::mc "ERROR: must specify a path to search for configurations" ]
    exit 1
}
# split into a list of dirs
set configs_dir_list [ split $configs_path ":" ]
 
# set options that are common to all widgets
foreach class { Button Entry Label Listbox Scale Text } {
    option add *$class.borderWidth 1  100
}

# make a toplevel and a master frame.
wm title . [msgcat::mc "EMC2 Configuration Selector"]
set logo [label .logo -image $logo]
set top [frame .main -borderwidth 0 -relief flat ]
pack $logo -side left -anchor nw
pack $top -side left -expand yes -fill both

wm geo . 780x480
wm resiz . 0 0

initialize_config

# a frame for packing the top window
set f1 [ frame $top.f1 ]

set message "Welcome to EMC2.\n\nSelect a machine configuration from the list on the left.\nDetails about the selected configuration will appear in the display on the right.\nClick 'OK' to run the selected configuration"

set lbl [ label $f1.lbl -text $message -justify left -padx 20 -pady 10 -wraplength 600 ]
pack $lbl -anchor w

# a subframe for the tree/detail box 
set f2 [ frame $f1.f2 -highlightt 0 -borderwidth 0 -relief flat]

# subframe for the tree and its scrollbar
set f3 [ frame $f2.f3 -highlightt 1 -borderwidth 2 -relief sunken]
# the tree
set tree [Tree $f3.tree  -width 25 -relief flat -padx 4 ]
# pack the tree into its subframe
pack $tree -side left -fill y -expand y
bind $tree <<TreeSelect>> { node_clicked }

# bwidget 1.7.0 does not generate <<TreeSelect>> events for keyboard navigation.
# These bindings fix that.
bind $tree.c <KeyPress-Up> [list +event generate $tree <<TreeSelect>>]
bind $tree.c <KeyPress-Down> [list +event generate $tree <<TreeSelect>>]

# need a scrollbar
set tscr [ scrollbar $f3.scr -command "$tree yview" -takefocus 1 -highlightt 0 -bd 0 -elementborderwidth 2 ]
# link the tree to the scrollbar
$tree configure -yscrollcommand "$tscr set"
# pack the scrollbar into the subframe
pack $tscr -side right -fill y -expand y
# pack the subframe into the main frame
pack $f3 -side left -anchor center -padx 3 -fill y -expand y

# subframe for the detail box and its scrollbar
set f4 [ frame $f2.f4 -highlightt 1 -borderwidth 2 -relief sunken]
# a text box to display the details
set tb [ text $f4.tb -width 45 -wrap word -padx 6 -pady 6 \
         -takefocus 0 -highlightt 0 -state disabled \
         -relief flat -borderwidth 0 ]
set detail_box $tb
# pack the text box into its subframe
pack $tb -side left -fill both -expand y 
# need a scrollbar
set dscr [ scrollbar $f4.scr -command "$tb yview" -takefocus 1 -highlightt 0 -bd 0 -elementborderwidth 2 ]
# link the text box to the scrollbar
$tb configure -yscrollcommand "$dscr set"
# pack the scrollbar into the subframe
pack $dscr -side right -fill y -expand y
# pack the subframe into the main frame
pack $f4 -side right -padx 3 -fill both -expand y

pack $f2 

# a subframe for the buttons
set f5 [ frame $f1.f5 ]
button $f5.cancel -text Cancel -command "button_pushed Cancel"
$f5.cancel configure -width 8
pack $f5.cancel -side right -padx 4 -pady 4
button $f5.ok -text OK -command "button_pushed OK"
set button_ok $f5.ok
$button_ok configure -state disabled
$f5.ok configure -width 8
pack $f5.ok -side right -padx 4 -pady 4
pack $f5 -side bottom -anchor e -fill none -expand n -padx 15

pack $f1 -fill both -expand y

set config_count 0
foreach dir $configs_dir_list {
    set dir_in_tree 0
    set subdir_list [ glob -nocomplain $dir/*/ ]
    foreach subdir $subdir_list {
	set inifile_list [ glob -nocomplain $subdir*.ini ]
	if { [ llength $inifile_list ] == 1 } {
	    # only one ini file, no second level
	    # add dir to tree if not already
	    if { $dir_in_tree == 0 } {
		$tree insert end root $dir -text $dir/ -open 1
		set dir_in_tree 1
	    }
	    # add inifile to tree
	    set inifile [ lindex $inifile_list 0 ]
            set parts [file split $inifile]
	    $tree insert end $dir $inifile -text [lindex $parts end-1] -open 1
	    incr config_count
	} elseif { [ llength $inifile_list ] > 1 } {
	    # multiples, use second level
	    # add dir to tree if not already
	    if { $dir_in_tree == 0 } {
		$tree insert end root $dir -text $dir/ -open 1
		set dir_in_tree 1
	    }
	    # add subdir to tree
	    $tree insert end $dir $subdir -text [ file tail $subdir ] -open 1
	    # add second level entries, one per inifile
	    foreach inifile $inifile_list {
		# add inifile to tree
                set text [file rootname [file tail $inifile]]
		$tree insert end $subdir $inifile -text $text -open 1
		incr config_count
	    }
	}
    }
}

if { $config_count == 0 } {
    puts stderr [ msgcat::mc "ERROR: no configurations found in path" ]
    exit 1
}

bind . <Escape> {button_pushed Cancel}
bind . <Return> ""
wm protocol . WM_DELETE_WINDOW {button_pushed Cancel}

vwait choice

if { $choice == "OK" } {
    puts $inifile
}

exit


