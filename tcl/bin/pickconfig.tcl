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
#    pickconfig [options]
#
#   Validates a desired emc2 config or asks the user to choose
#   one, then prints the complete path to the ini file.
#
# options:
#  --configs-path <dir>[:<dir>]
#
#   Required - if not supplied, exits with an error
#   Colon separated list of directorys in which to search
#   for configurations. 
#
#  --config <config-name>
#
#   Desired configuration name.  If specified, pickconfig
#   will search the configs-path and use the first sub-
#   directory that matches.  If there is no match, or if
#   this option was not specified, it will display a list
#   of all configurations and ask the user to choose one.
#
#  --ini <ini-name>
#
#   Desired ini file name.  If specified, and if a matching
#   one is found in the chosen config, it will be used.  If
#   no match exists, or if this option was not specified,
#   and the chosen config contains more than one ini file 
#   it will list all the ini files and ask the user to choose
#   one.  If the chosen config contains only one ini file, 
#   that file will be used regardless of any -ini option.
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

# a kluge for older systems that don't have 'file normalize'
# (which was introduced with tcl 8.4)
# this version converts to an absolute path and eliminates 
# any references to ./ and ../, but it doesn't do all the 
# other things that normalize can do, such as resolve symlinks.
# it seems good enough for what this program needs to do

proc file_normalize { input } {
    # make path absolute
    if { [ file pathtype $input ] != "absolute" } {
	set input [ file join [ pwd ] $input ]
    }
    # split into component parts
    set path_list [ file split $input ]
    # remove any instances of ./
    set idx [ lsearch $path_list "." ]
    while { $idx > 0 } {
	set path_list [ lreplace $path_list $idx $idx ]
	set idx [ lsearch $path_list "." ]
    }
    # remove any instances of ../ (and the preceding directory name)
    set idx [ lsearch $path_list ".." ]
    while { $idx > 0 } {
	set path_list [ lreplace $path_list [ expr $idx -1 ] $idx ]
	set idx [ lsearch $path_list ".." ]
    }
    set output ""
    foreach name $path_list {
	set output [ file join $output $name ]
    }
    return $output
}

# main button callback, it assigns the button name to 'choice'

proc button_pushed { button_name } {
    global choice

    set choice $button_name
}

# generic popup, displays a message and waits for "OK" click

proc popup { message } {
    global choice top logo

    bind . <Escape> {button_pushed OK}
    bind . <Return> {button_pushed OK}
    wm protocol . WM_DELETE_WINDOW {button_pushed OK}
    set f1 [ frame $top.f1 ]
    set lbl [ label $f1.lbl -text $message -padx 20 -pady 10 ]
    set but [ button $f1.but -text OK -command "button_pushed OK" -default active -width 8]
    bind [winfo toplevel $top] <Return> { button_pushed OK }
    pack $lbl -side top
    pack $but -side bottom -padx 10 -pady 10
    pack $f1 -anchor nw -fill both -expand 1

    wizard_event_loop $f1

    pack forget $f1
    destroy $f1
}

# config validator  FIXME - needs rewritten to support multiple dirs
# or maybe its obsolete
#
# this gets a config name, which can have multiple forms
# it returns either a fully qualified path to the config directory
# (or ini file, if specified), or displays an error message and returns ""
#
# input can be:
# THE FOLLOWING SHOULD NOW ALLOW ANY DIR IN CONFIGS PATH
#   name of config dir (assumed to be inside emc2 main configs dir)
#   name of config dir and ini file (assumed to be inside main configs dir)
# DO WE WANT TO ALLOW CONFIGS NOT IN CONFIGS PATH?
#   absolute path to config dir (need not be in the emc2 main configs dir)
#   relative path to config dir (need not be in the emc2 main configs dir)
#   absolute path to ini file (need not be in the emc2 main configs dir)
#   relative path to ini file (need not be in the emc2 main configs dir)

proc resolve_config { input } {
    global configs_dir

    # no error messages if blank, just return
    if { $input == "" } {
	return ""
    }
    # make into an absolute path
    set abs_input [ file_normalize $input ]
    # is it a directory? 
    if { [ file isdirectory $abs_input ] == 1 } {
	# it is a path to a directory, any .ini files inside?
	set inis [ glob -nocomplain -directory $abs_input *.ini ]
	if { [ llength $inis ] == 0 } {
	    popup [ format [ msgcat::mc "ERROR: Not a valid config directory (contains no .ini files)\n\n'%s'\n(%s)\n\n" ] $input $abs_input ][ msgcat::mc "Click 'OK' to continue." ]
	    return ""
	}
	return $abs_input
    }
    # is it a file?
    if { [ file isfile $abs_input ] == 1 } {
	# it is a path to a file, it is an .ini file?
	if { [ file extension $abs_input ] != ".ini" } {
	    popup [ format [ msgcat::mc "ERROR: Not a valid ini file (must end with '.ini')\n\n'%s'\n(%s)\n\n" ] $input $abs_input ][ msgcat::mc "Click 'OK' to continue." ]
	    return ""
	}
	return $abs_input
    }
    # check in main configs dir
    set abs_input [ file join $configs_dir $input ]
    # is it of the form <config-name>?
    if { [ llength [ file split $input ] ] == 1 } {
	# yes, does the config directory exist?   
	if { [ file isdirectory $abs_input ] == 1 } {
	     # it is a directory, any .ini files inside?
	    set inis [ glob -nocomplain -directory $abs_input *.ini ]
	    if { [ llength $inis ] != 0 } {
		# yes, done
		return $abs_input
	    }
	    popup [ format [ msgcat::mc "ERROR: Not a valid config directory (contains no .ini files)\n\n'%s'\n(%s)\n\n"] $input $abs_input ][ msgcat::mc "Click 'OK' to continue." ]
	    return ""
	}
	popup [ format [ msgcat::mc "ERROR: Config not found\n\n'%s'\n(%s)\n\n" ] $input $abs_input ][ msgcat::mc "Click 'OK' to continue." ]
	return ""
    }
    # is it of the form <config-name>/<ini-name>?
    if { [ llength [ file split $input ] ] == 2 } {
	# yes, if no extension, add .ini
	if { [ file extension $abs_input ] == "" } {
	    set abs_input $abs_input.ini
	}
	# does the ini file exist?   
	if { [ file isfile $abs_input ] == 1 } {
	    # yes, done
	    return $abs_input
	}
	popup [ format [ msgcat::mc "ERROR: config/ini not found\n\n'%s'\n(%s)\n\n" ] $input $abs_input ][ msgcat::mc "Click 'OK' to continue." ]
	return ""
    }
    popup [format [ msgcat::mc "ERROR: Not a valid config name\n(must be either <config-name> or <config-name>/<ini-name>)\n\n'%s'\n\n"] $input ][ msgcat::mc "Click 'OK' to continue." ]
    return ""
}   




################### FIXME - this divider is just an editing aid, remove it ###################

proc find_config { config } {
    global configs_dir_list

    foreach configs_dir $configs_dir_list {
	if { [ file isdirectory [ file join $configs_dir $config ]] == 1 } {
	    return [ file join $configs_dir $config ]
	}
    }
    return
}

# slider process is used for several widgets
proc sSlide {f a b} {
    $f.sc set $a $b
}

# called when user clicks tree node
proc node_clicked {} {
    global tree detail_box button_ok

    set node [$tree selection get]
    if {$node == ""} return
    
    if { [ regexp {.*\.ini$} $node ] == 1 } {
	# an ini node, acceptable
	set dir [ file dirname $node ]
	set readme [ file join $dir README ]
	$tree selection set $node
        # enable the OK button
        $button_ok configure -state normal
    } else {
	# not an ini node, can't select it
	if { [ $tree parent $node ] != "root" } {
	    # its a subdir with more than one ini
	    # so it should have a readme
	    set dir $node
	    set readme [ file join $dir README ]
	} else {
	    # its one of the top level path dirs
	    # not a config, but a dir that holds configs
	    set dir ""
	    set readme ""
	}
	#$tree selection clear
	# disable the OK button
        $button_ok configure -state disabled
    }
    # enable changes to the details widget
    $detail_box configure -state normal
    # remove old test
    $detail_box delete 1.0 end
    # add new text
    if { $readme != "" } {
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
    }
    # lock it again
    $detail_box configure -state disabled
}




################ MAIN PROGRAM STARTS HERE ####################

# set options that are common to all widgets
foreach class { Button Entry Label Listbox Scale Text } {
    option add *$class.borderWidth 1  100
}

# parse command line 

# process --configs-path
set option_index [ lsearch $argv "--configs-path" ]
if { $option_index >= 0 } {
    # get the directory(s)
    set configs_path [ lindex $argv [ expr $option_index + 1 ]]
    if { $configs_path == "" || [ string equal -length 1 $configs_path "-" ] == 1 } {
	puts stderr [msgcat::mc "ERROR: option '--configs-path' must be followed by directory name(s)"]
	exit 1
    }
    # split into a list of dirs
    set configs_dir_list [ split $configs_path ":" ]
    # remove option and its argument from argv
    set argv [ lreplace $argv $option_index [expr $option_index + 1]]
} else {
    puts stderr [ msgcat::mc "ERROR: must specify --configs-path <path-to-configs>" ]
    exit 1
}

# process --config
set option_index [ lsearch $argv "--config" ]
if { $option_index >= 0 } {
    # get arg that follows '--config' (if any)
    set config_name [ lindex $argv [ expr $option_index + 1 ] ]
    # make sure it exists and isn't another option
    if { $config_name == "" || [ string equal -length 1 $config_name "-" ] == 1 } {
	puts stderr [msgcat::mc "ERROR: option '--config' must be followed by a name"]
	exit 1
    }
    # delete option and its arg from argv
    set argv [ lreplace $argv $option_index [expr $option_index + 1] ]
} else {
    set config_name ""
}

# process --ini
set option_index [ lsearch $argv "--ini" ]
if { $option_index >= 0 } {
    # get arg that follows '--ini' (if any)
    set ini_name [ lindex $argv [ expr $option_index + 1 ] ]
    # make sure it exists and isn't another option
    if { $ini_name == "" || [ string equal -length 1 $ini_name "-" ] == 1 } {
	puts stderr [msgcat::mc "ERROR: option '--ini' must be followed by a name"]
	exit 1
    }
    # delete option and its arg from argv
    set argv [ lreplace $argv $option_index [expr $option_index + 1] ]
} else {
    set ini_name ""
}

# at this point all legal options and their args have been deleted, 
# anything left in 'argv' is an error
if { [ llength $argv ] != 0 } {
    puts stderr [ format [ msgcat::mc "ERROR: unknown command line option: '%s'"] $argv ]
    exit 1
}

# if --config (and possibly --ini) were specified, try to find
# a matching entry before popping up a GUI





# make a toplevel and a master frame.
wm title . [msgcat::mc "EMC2 Configuration Selector"]
set logo [label .logo -image $logo]
set top [frame .main -borderwidth 0 -relief flat ]
pack $logo -side left -anchor nw
pack $top -side left -expand yes -fill both

wm geo . 780x480
wm resiz . 0 0

initialize_config

# THE FOLLOWING SHOULD PROBABLY BE A PROC

# a frame for packing the top window
set f1 [ frame $top.f1 ]

set message "Welcome to EMC2.\n\nSelect a machine configuration from the list on the left (items ending in '.ini' are configurations).\nDetails about the selected configuration will appear in the display on the right.\nClick 'OK' to run the selected configuration"

set message "THIS PROGRAM IS NOT FINISHED.  DON'T USE IT.  IF YOU USE IT AND IT BREAKS, SERVES YOU RIGHT!  IT WAS COMMITTED TO CVS ONLY TO ALLOW ANOTHER DEVELOPER TO SEE IT!!!"

set lbl [ label $f1.lbl -text $message -justify left -padx 20 -pady 10 -wraplength 600 ]
pack $lbl

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
pack $f5 -side bottom -anchor e -fill none -expand n

pack $f1 -fill both -expand y

update

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
		# add subdir to tree
		$tree insert end $subdir $inifile -text [ file tail $inifile ] -open 1
	    }
	}
    }
}

# THE PROC SHOULD END HERE

bind . <Escape> {button_pushed Cancel}
# bind . <Return> {button_pushed OK}
wm protocol . WM_DELETE_WINDOW {button_pushed Cancel}



vwait choice
puts $choice


exit


