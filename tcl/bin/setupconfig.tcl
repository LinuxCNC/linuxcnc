#!/bin/sh
# this line restarts using wish \
exec wish "$0" "$@"

###############################################################
# Description:  setupconfig.tcl
#               This file, shows existing emc2 configuration
#               and allows copying and naming
#
#  Author: John Kasunich & Raymond E Henry
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
#    setupconfig [options] 
#
# options:
#  --configs-dir <dir>
#
#   Assume that <dir> is the configs/ directory, and that
#   subdirectories of <dir> contain individual configs.
#   If --configs-dir is not given, the script attempts to
#   find the directory itself, but that works only if the
#   environment variable EMC2_ORIG_CONFIGS_DIR is set, or
#   if the script was run from the configs dir, or one 
#   directly above or below it.
#
#  --run [<config-name>/<ini-name>]
#
#   Validate the config-name/ini-name that was supplied (if
#   any), if not supplied or invalid, go directly to the 
#   screen that asks the user to choose a config.  Once
#   the config is validated (or the user chooses one and 
#   clicks "Run"), invoke the run script to run EMC using
#   that configuration.
#
#  --get-config [<config-name>/<ini-name>]
#
#   Validate the config-name/ini-name that was supplied (if
#   any), if not supplied or invalid, go directly to the 
#   screen that asks the user to choose a config.  Once
#   the config is validated (or the user chooses one and 
#   clicks 'RUN'), do _not_ invoke the run script to run it,
#   instead simply print the chosen <config/ini> string and
#   end.  (This mode is for use by the run script.)
#
#  --new
#
#   Skip the initial menu and go straight to the "create
#   new config" sequence.
#
#  --run, --get-config, and --new are mutually exclusive. 
#  If none of these is specified, setupconfig displays a 
#  sequence of menus so the user can manipulate configurations
#  and/or run emc. 
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

# reads the directory, and fills in "config_list" and "details_list"

proc get_config_list {} {
    global config_list details_list configs_dir

    # clear config and description lists
    set config_list [ list ]
    set details_list [ list ]
    # look at all subdirs
    foreach dir_name [ lsort [ glob */ ] ] {
	cd $dir_name
	# is there an ini file (or more than one) inside?
	set inifnames [concat [ glob -nocomplain *.ini ]]
	if { [llength $inifnames]!= "0" } {
	    # yes, this is a viable config
	    # strip trailing / 
	    regsub "/" $dir_name "" config_name
	    # and save it
	    lappend config_list $config_name
	    # look for a README file
	    if { [ file isfile "README" ] } {
		# description found, read it
		set descr [ read -nonewline [ open "README" ]]
		# reformat - remove line breaks, preserve paragraph breaks
		regsub -all {([^\n])\n([^\n])} $descr {\1 \2} descr
		# and save it
		lappend details_list $descr
	    } else {
		# no description, gotta tell the user something
		lappend details_list [msgcat::mc "No details available."]
	    }
	}
	# back to main configs directory
	cd ..
    }
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

# button_entry_change - Utility function for button_depends_on_entry
proc button_entry_change {b t name1 name2 op} {
    upvar \#0 $t v
    if {$v == ""} {
        $b configure -state disabled
    } else {
        $b configure -state normal
    }
}

# button_depends_on_variable - Sometimes, a button should not be enabled until
# text is entered
proc button_depends_on_variable {b v e} {
    global top
    global $v
    set b $top.f1.f2.[string tolower $b]
    set cmd [list button_entry_change $b $v]
    bind $e <Destroy> [list trace remove variable $v write $cmd]
    trace add variable $v write $cmd
    button_entry_change $b $v . . .
}

# button_listbox_change - Utility function for button_depends_on_listbox
proc button_listbox_change {b lb} {
    global top
    set b $top.f1.f2.[string tolower $b]
    if {[$lb curselection] == {}} {
        $b configure -state disabled
    } {
        $b configure -state normal
    }
}


# button_depends_on_listbox - Sometimes, a button should not be enabled until
# an item is selected This function enforces that.
proc button_depends_on_listbox {b lb} {
    bind $lb <<ListboxSelect>> "+[list button_listbox_change $b $lb]"
    button_listbox_change $b $lb
}

# wizard_event_loop - call this after setting up the wizard.  Returned
# value is the english name of the button chosen, which is also in $choice
# The wizard is not yet destroyed when this function returns.
proc wizard_event_loop {f1} {
    if {[winfo exists $f1.f2]} {raise $f1.f2}
    update idletasks
    focus [tk_focusNext .]
    set choice "none"
    vwait choice
}


# generic wizard page - defines a frame with multiple buttons at the bottom
# Returns the frame widget so page specific stuff can be packed on top

proc wizard_page { buttons {default ""} {abort ""}} {
    global choice top

    set f1 [ frame $top.f1 ]
    set f2 [ frame $f1.f2 ]
    set tl [winfo toplevel $top]

    bind $tl <Return> {}
    bind $tl <Escape> {}
    wm protocol $tl WM_DELETE_WINDOW { button_pushed WM_CLOSE }

    foreach button_name $buttons {
	set bname [ string tolower $button_name ]
        set text [msgcat::mc $button_name]
                
        button $f2.$bname -text $text -command "button_pushed \"$button_name\""

        set font [$f2.$bname cget -font]
        set ave_width [font measure $font "0"]
        set text_width [font measure $font $text]
        if {$text_width > 8*$ave_width} { 
            $f2.$bname configure -width 0
        } else {
            $f2.$bname configure -width 8
        }
	pack $f2.$bname -side left -padx 4 -pady 4
        if {$button_name == $default} {
            $f2.$bname configure -default active
            bind $tl <Return> [list $f2.$bname invoke]
        } else { 
            $f2.$bname configure -default normal
        }
        if {$button_name == $abort} {
            bind $tl <Escape> [list $f2.$bname invoke]
            wm protocol $tl WM_DELETE_WINDOW [list $f2.$bname invoke]
        }
    }
    pack $f2 -side bottom
    return $f1
}

# detail picker - lets you select from a list, displays details
# of the selected item

proc detail_picker { parent_wgt item_text item_list detail_text detail_list } {
    # need some globals to talk to our callback function
    global d_p_item_list d_p_item_widget d_p_detail_list d_p_detail_widget
    # and one for the result
    global detail_picker_selection

    # init the globals
    set d_p_item_list $item_list
    set d_p_detail_list $detail_list
    set detail_picker_selection ""

    # frame for the whole thing
    set f1 [ frame $parent_wgt.f1 ]
    
    # label for the item list
    set l1 [ label $f1.l1 -text $item_text -justify left]
    pack $l1 -pady 6 -anchor w

    # subframe for the list and its scrollbar
    set f2 [ frame $f1.f2]

    # listbox for the items
    set la [ label $f2.la -text [msgcat::mc "Available Configurations:"]]
    pack $la -side top -anchor w
    set fl [frame $f2.fl -highlightt 1 ]
    set lb [ listbox $fl.lb -highlightt 0 ]
    set d_p_item_widget $lb
    # pack the listbox into its subframe
    pack $lb -side left -fill y -expand y
    # hook up the callback to display the description
    bind $lb <<ListboxSelect>> detail_picker_refresh
    # load the list with names
    foreach item $item_list {
        $lb insert end $item
    }
    # if more than 'max' entries, use a scrollbar
    set max_items 16
    if { [ $lb size ] >= $max_items } {
	# need a scrollbar
	set lscr [ scrollbar $fl.scr -command "$lb yview" ]
	# set the listbox to the max height
	$lb configure -height $max_items
	# link it to the scrollbar
	$lb configure -yscrollcommand "$lscr set"
	# pack the scrollbar into the subframe
	pack $lscr -fill y -side right
    } else {
	# no scrollbar needed, make the box fit the list  (plus some 
	# space, so the user can tell that he is seeing the entire list)
	$lb configure -height [ expr { [ $lb size ] + 1 } ]
    }
    # pack the subframe into the main frame
    pack $fl -side left -anchor nw -padx 3 -fill y -expand 1
    pack $f2 -side left -anchor nw -padx 3 -fill y -expand 1

    # label for the details box
    set l2 [ label $f1.l2 -text $detail_text ]
    pack $l2 -anchor w -padx 3
	
    # subframe for the detail box and its scrollbar
    set f3 [ frame $f1.f3 -highlightt 1 -borderwidth 2 -relief sunken]
    # a text box to display the details
    set tb [ text $f3.tb -width 60 -height 10 -wrap word -padx 6 -pady 6 \
             -takefocus 0 -highlightt 0 -state disabled \
             -relief flat -borderwidth 0 ]
    set d_p_detail_widget $tb
    # pack the text box into its subframe
    pack $tb -side left -fill y -expand y
    # need a scrollbar
    set dscr [ scrollbar $f3.scr -command "$tb yview" -takefocus 1 -highlightt 0 -bd 0 -elementborderwidth 2 ]
    # link the text box to the scrollbar
    $tb configure -yscrollcommand "$dscr set"
    # pack the scrollbar into the subframe
    pack $dscr -fill y -side right
    # pack the subframe into the main frame
    pack $f3 -side left -anchor nw -padx 3 -fill y
    # and finally pack the main frame into the parent
    pack $f1 -fill y -expand 1

    set lb
}

# callback to display the details when the user selects different items
proc detail_picker_refresh {} {
    # need some globals from the main function
    global d_p_item_list d_p_item_widget d_p_detail_list d_p_detail_widget
    # and one for the result
    global detail_picker_selection

    # get ID of current selection
    set pick [ $d_p_item_widget curselection ]
    # save name of current selection
    set detail_picker_selection [ lindex $d_p_item_list $pick ]
    # get the details
    set detail [ lindex $d_p_detail_list $pick ]
    # enable changes to the details widget
    $d_p_detail_widget configure -state normal
    # jam the new text in there
    $d_p_detail_widget delete 1.0 end
    $d_p_detail_widget insert end $detail
    # lock it again
    $d_p_detail_widget configure -state disabled
}

# proc to select an item (used to set initial selection)
proc detail_picker_select { item } {
    # need globals to talk to the list and widget
    global d_p_item_list d_p_item_widget 

    set pick [ lsearch $d_p_item_list $item ]
    # if 'item' isn't in the list, do nothing
    if { $pick != -1 } {
	# clear any existing selection
	$d_p_item_widget selection clear 0 end
	# mark the new selected item
	$d_p_item_widget selection set $pick $pick
	# invoke the callback to refresh
        event generate $d_p_item_widget <<ListboxSelect>>
    }
}

################### MENU PROCEDURE DEFINITIONS #####################

proc main_page {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state

    set msg [msgcat::mc "Welcome to EMC2!\n\nTo run EMC2 with an existing configuration, click the 'Run' button.\nTo create a new configuration, edit, backup, or restore a configuration,\nor do other configuration related tasks, click the 'Configure' button."]

    set f1 [ wizard_page { "Run" "Configure" "Quit" } "Run" "Quit" ]
    set l1 [ label $f1.l1 -text $msg -justify left]
    pack $l1 -padx 10 -pady 10 -anchor w
    pack $f1 -anchor nw -fill both -expand 1

    wizard_event_loop $f1

    pack forget $f1
    destroy $f1

    switch $choice {
	"Quit" {
	    set wizard_state "quit"
	    return
	}
	"Configure" {
	    set wizard_state "config_manager"
	    return
	}
	"Run" {
	    set wizard_state "choose_run_config"
	    return
	}
    }
}    

proc check_run_config {} {
    # need globals to communicate with wizard page buttons,
    # and for the configuration lists
    global choice top wizard_state
    global config_list details_list detail_picker_selection
    # more globals for new and run config info
    global new_config_name new_config_template new_config_readme
    global run_config_name run_ini_name

    # if a config directory is already known, skip this stage
    if { $run_config_name != "" } {
	set wizard_state "check_run_ini"
	return
    }
    # otherwise, need to pick a config
    set wizard_state "choose_run_config"
    return
}
 
proc choose_run_config {} {
    # need globals to communicate with wizard page buttons,
    # and for the configuration lists
    global choice top wizard_state
    global config_list details_list detail_picker_selection
    # more globals for new and run config info
    global new_config_name new_config_template new_config_readme
    global run_config_name run_ini_name

    # read the directory and set up list of configs
    get_config_list

    # messages
    set t1 [msgcat::mc "Please select an EMC2 configuration from the list below and click 'Run'.\nTo create a new configuration, backup, restore, or erase configurations, click 'Configure'."]
    set t2 [msgcat::mc "Details about the selected configuration:"]
    
    #set up a wizard page with two buttons
    set f1 [ wizard_page { "Run" "Configure" "Quit" } Run Quit ]
    # add a detail picker to it with the configs
    set lb [detail_picker $f1 $t1 $config_list $t2 $details_list]
    button_depends_on_listbox "Run" $lb
    bind $lb <Double-Button-1> [list button_pushed Run]
    # done
    pack $f1 -anchor nw -fill both -expand 1

    # pre-select the current run config, if any
    if { $run_config_name != "" } {
	detail_picker_select [ file tail $run_config_name ]
    }

    wizard_event_loop $f1

    # a button was pushed, save selection
    set value $detail_picker_selection
    # clear the window
    pack forget $f1
    destroy $f1

    switch $choice {
        "Configure" {
	    set wizard_state "config_manager"
	    return
        }
	"Quit" {
            exit
	}
	"Run" {
	    if { $value == "" } {
		popup [msgcat::mc "You must choose a config if you want to run EMC2."]
		return
	    }
	    set run_config_name [ file_normalize $value ]
	    set wizard_state "check_run_ini"
	    return
	}
    }
}

proc check_run_ini {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state
    global run_config_name
    
    if { [ file extension $run_config_name ] == ".ini" } {
	# already have the ini
	set wizard_state "execute_run"
	return
    }
    # get the names of all ini files in the selected directory
    set temp [ glob -directory $run_config_name *.ini ]
    set inis [ list ]
    foreach ini $temp {
	lappend inis [ file tail $ini ]
    }
    if { [ llength $inis ] == 0 } {
	popup [ format [ msgcat::mc "ERROR: no ini file(s) found in config directory\n\n'%s'\n\n" ] $run_config_name ][ msgcat::mc "Click 'OK' to continue." ]
	set wizard_state "main_page"
	return
    }
    if { [ llength $inis ] == 1 } {
	# only one ini file, use it
	set run_config_name [ file join $run_config_name [ lindex $inis 0 ] ]
	set wizard_state "execute_run"
	return
    }
    # more than one, must choose
    set wizard_state "choose_run_ini"
    return
}

proc choose_run_ini {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state
    global run_config_name
    
    # get the names of all ini files in the selected directory
    set temp [ glob -directory $run_config_name *.ini ]
    set inis [ list ]
    foreach ini $temp {
	lappend inis [ file tail $ini ]
    }
    # set up a list box so the user can pick one
    set f1 [ wizard_page { "< Back" "Cancel" "Next >" } "Next >" "Cancel" ]
    set l1 [ label $f1.l1 -text [msgcat::mc "The config contains multiple ini files.\nPick one from the list below and click 'Next'."] -justify left]
    pack $l1 -padx 10 -pady 10 -anchor w
    # listbox for the ini files
    set lb [ listbox $f1.lb ]
    # pack the listbox into the wizard page
    pack $lb
    # load the list with names
    foreach ini $inis {
        $lb insert end [ file rootname $ini ]
    }
    button_depends_on_listbox "Next >" $lb
    bind $lb <Double-Button-1> [list button_pushed "Next >"]
    # set the size of the list box
    $lb configure -height 8
    # pack the page
    pack $f1 -anchor nw -fill both -expand 1

    wizard_event_loop $f1

    set pick [ $lb curselection ]
    # clear the window
    pack forget $f1
    destroy $f1

    switch $choice {
	"Cancel" {
	    set wizard_state "main_page"
	    return
	}
	"< Back" {
	    set wizard_state "choose_run_config"
	    return
	}
	"Next >" {
	    if { $pick == "" } {
		popup [msgcat::mc "You must select one ini file."]
		return
	    }
	    # add name of selected ini file to directory name
	    set run_config_name [ file join $run_config_name [ lindex $inis $pick ] ]
	    # done
	    set wizard_state "execute_run"
	    return
	}
    }
}

proc execute_run {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state
    global run_config_name run_mode

    if { $run_mode == "print" } {
	# all we need to do is print the desired config name and exit
	puts $run_config_name
	exit 0
    }
    
    # not done yet
    popup [ format [ msgcat::mc "The next step is to invoke the run script to run\n'%s'\n\nBut thats not coded yet, so just click OK to quit." ] $run_config_name ]
    set wizard_state "quit"
    return
}

proc config_manager {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state
    global config_list details_list detail_picker_selection
    # more globals for new and run config info
    global new_config_name new_config_template new_config_readme
    global run_config_name run_ini_name
    global selected_config_name

    # read the directory and set up list of configs
    get_config_list

    # messages
    set t1 [msgcat::mc "This page is used to manage EMC2 machine configurations.\n"]
    set t2 [msgcat::mc "The list below shows all of the existing EMC2\nconfigurations on this computer.\n\nSelect a config, then click one of the buttons below.\n"]
    set t3 [msgcat::mc "Details about the selected configuration:"]

    set f1 [ wizard_page { "Edit" "Backup" "Restore" "Delete" "New" "Cancel" } "Edit" "Cancel" ]
    set l1 [ label $f1.l1 -text $t1 -justify left]
    pack $l1 -padx 10 -pady 2 -anchor w
    # add a detail picker to it with the configs
    set lb [detail_picker $f1 $t2 $config_list $t3 $details_list]
    button_depends_on_listbox Edit $lb
    button_depends_on_listbox Backup $lb
    button_depends_on_listbox Restore $lb
    button_depends_on_listbox Delete $lb
    bind $lb <Double-Button-1> [list button_pushed "Edit"]
    pack $f1 -anchor nw -fill both -expand 1

    wizard_event_loop $f1

    # a button was pushed, save selection
    set value $detail_picker_selection
    # clear the window
    pack forget $f1
    destroy $f1

    switch $choice {
	"Cancel" {
	    set wizard_state "main_page"
	    return
	}
	"Edit" {
	    if { $value == "" } {
		popup [ msgcat::mc "You must choose a config first." ]
		return
	    }
	    set selected_config_name $value
	    set wizard_state "not_implemented"
	    return
	}
	"Backup" {
	    if { $value == "" } {
		popup [ msgcat::mc "You must choose a config first." ]
		return
	    }
	    set selected_config_name $value
	    set wizard_state "backup_verify"
	    return
	}
	"Restore" {
	    if { $value == "" } {
		popup [ msgcat::mc "You must choose a config first." ]
		return
	    }
	    set selected_config_name $value
	    set wizard_state "not_implemented"
	    return
	}
	"Delete" {
	    if { $value == "" } {
		popup [ msgcat::mc "You must choose a config first." ]
		return
	    }
	    set selected_config_name $value
	    set wizard_state "delete_verify"
	    return
	}
	"New" {
	    set wizard_state "new_intro"
	    return
	}
    }
}    

proc not_implemented {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state
    
    # not done yet
    popup [msgcat::mc "Sorry, this function is not implemented yet.\n\n"][ msgcat::mc "Click 'OK' to return to the main menu"]
    set wizard_state "config_manager"
    return
}

proc new_intro {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state
    global new_config_name new_config_template new_config_readme

    set f1 [ wizard_page { "< Back" "Cancel" "Next >" } "Next >" "Cancel"]
    set l1 [ label $f1.l1 -text [msgcat::mc "You have chosen to create a new EMC2 configuration.\n\nThe next few screens will walk you through the process."] -justify left]
    pack $l1 -padx 10 -pady 10 -anchor w
    pack $f1 -anchor nw -fill both -expand 1

    # get ready for a fresh start
    set new_config_name ""
    set new_config_template ""
    set new_config_readme ""
   
    wizard_event_loop $f1

    pack forget $f1
    destroy $f1

    switch $choice {
	"Cancel" {
	    set wizard_state "config_manager"
	    return
	}
	"< Back" {
	    set wizard_state "config_manager"
	    return
	}
	"Next >" {
	    set wizard_state "new_get_name"
	    return
	}
    }
}    

proc good_filename_character c { regexp {^[-\000-\037a-zA-Z0-9._]*$} $c }

proc new_get_name {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state new_config_name new_name

    set f1 [ wizard_page { "< Back" "Cancel" "Next >" } "Next >" "Cancel"]
    set l1 [ label $f1.l1 -text [msgcat::mc "Please select a name for your new configuration."] -justify left]
    set l2 [ label $f1.l2 -text [msgcat::mc "(This will become a directory name, so please use only letters,\ndigits, period, dash, or underscore.)"] -justify left]
    set e1 [ entry $f1.e1 -width 30 -relief sunken -bg white -takefocus 1 -textvariable new_name]

    bind $e1 <Key> { if {![good_filename_character %A]} break }
    
    if {![info exists new_config_name]} {set new_config_name ""}
    set new_name $new_config_name
    pack $l1 -padx 10 -pady 10 -anchor w
    pack $e1 -padx 10 -pady 1
    pack $l2 -padx 10 -pady 10 -anchor w
    pack $f1 -anchor nw -fill both -expand 1

    button_depends_on_variable "Next >" new_name $f1.e1

    wizard_event_loop $f1

    set value [ $e1 get ]
    pack forget $f1
    destroy $f1

    switch $choice {
	"Cancel" {
	    set wizard_state "config_manager"
	    return
	}
	"< Back" {
	    set wizard_state "new_intro"
	    return
	}
	"Next >" {
	    if { $value == "" } {
		popup [ msgcat::mc "You must enter a name." ]
		return
	    }
	    if { [ regexp {[^[:alnum:]_\-.]} $value ] == 1 } {
		popup [ format [msgcat::mc "'%s' contains illegal characters.\nPlease choose a new name." ] $value ]
		return
	    }
	    if { [ file exists $value ] == 1 } {
		popup [ format [msgcat::mc "A directory or file called '%s' already exists.\nPlease choose a new name." ] $value ]
		return
	    }
	    set new_config_name $value
	    set wizard_state "new_get_template"
	    return
	}
    }
}    
    
proc new_get_template {} {
    # need globals to communicate with wizard page buttons,
    # and for the configuration lists
    global choice top wizard_state
    global config_list details_list detail_picker_selection
    global new_config_name new_config_template new_config_readme

    # read the directory and set up list of possible templates
    get_config_list

    # messages
    set t1 [msgcat::mc "Please select one of these existing configurations as the template\nfor your new configuration.\n\nAll the files associated with the template will be copied into your new\nconfig, so you can make whatever modifications are needed."]
    set t2 [msgcat::mc "Details about the selected configuration:"]
    
    #set up a wizard page with three buttons
    set f1 [ wizard_page { "< Back" "Cancel" "Next >" } "Next >" "Cancel"]
    # add a header line
    set l1 [ label $f1.l1 -text [ format [ msgcat::mc "Creating new EMC2 configuration '%s'" ] $new_config_name ] -justify left]
    pack $l1 -pady 10 -anchor w
    # add a detail picker to it with the configs
    set lb [detail_picker $f1 $t1 $config_list $t2 $details_list]
    button_depends_on_listbox "Next >" $lb
    bind $lb <Double-Button-1> [list button_pushed "Next >"]
    # done
    pack $f1 -anchor nw -fill both -expand 1

    wizard_event_loop $f1

    set value $detail_picker_selection
    pack forget $f1
    destroy $f1

    switch $choice {
	"Cancel" {
	    set wizard_state "config_manager"
	    return
	}
	"< Back" {
	    set wizard_state "new_get_name"
	    return
	}
	"Next >" {
	    if { $value == "" } {
		popup [msgcat::mc "You must choose a template." ]
		return
	    }
	    if { [ file isdirectory $value ] != 1 } {
		popup [msgcat::mc "A internal error has occurred, or the template directory was deleted.\n"][ msgcat::mc "Click 'OK' to exit"]
		set wizard_state "quit"
		return
	    }
	    set new_config_template $value
	    # look up the details that match this template
	    # NOTE: we do this here, instead of at the beginning of
	    # "new_get_description" so that anything the user has typed
	    # survives if he goes back to the new description page.
	    # His data is only overwritten by the template if he goes
	    # all the way back to the template selection page (this one).
	    if { [ file isfile "$new_config_template/README" ] } {
		# description found, read it
		set descr [ read -nonewline [ open "$new_config_template/README" ]]
		# and save it
		set new_config_readme $descr
	    } else {
		# no description, gotta tell the user something
		set new_config_readme [msgcat::mc "Enter a description here"]
	    }
	    set wizard_state "new_get_description"
	    return
	}
    }
}

proc new_get_description {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state 
    global new_config_name new_config_template new_config_readme

    set f1 [ wizard_page { "< Back" "Cancel" "Next >" } "Next >" "Cancel"]
    # add a header line
    set l1 [ label $f1.l1 -text [ format [ msgcat::mc "Creating new EMC2 configuration '%s'\nbased on template '%s'" ] $new_config_name $new_config_template ] -justify left]
    pack $l1 -pady 10 -anchor w
    set l2 [ label $f1.l2 -text [msgcat::mc "Please enter a description of your configuration.\n\nThe box below has been preloaded with the description of the template, but\nit is strongly recommended that you revise it.  At a minimum,\nput your name and some specifics about your machine here."] -justify left ]
    set l3 [ label $f1.l3 -text [msgcat::mc "(If you ever need help, someone may ask you to send them your\nconfiguration, and this information could be very usefull.)"] -justify left ]

    # subframe for the text entry box and its scrollbar
    set f3 [ frame $f1.f3 -highlightt 1 ]
    #  text box
    set tb [ text $f3.tb -width 60 -height 10 -wrap word -padx 6 -pady 6 \
             -relief sunken -state normal -bg white -highlightt 0 ]
    bind $tb <Control-Return> {# Nothing}
    set binding [bind Text <Return>]
    if {$binding == ""} { set binding [bind Text <Key>] }
    bind $tb <Return> $binding
    bind $tb <Return> +break
    $tb insert end $new_config_readme
    # pack the text box into its subframe
    pack $tb -side left -fill y -expand y
    # need a scrollbar
    set scr [ scrollbar $f3.sc -command "$tb yview" -takefocus 1 -highlightt 0 ]
    # link the text box to the scrollbar
    $tb configure -yscrollcommand "$scr set"
    # pack the scrollbar into the subframe
    pack $scr -fill y -side right
  
    # pack things into the main frame    
    pack $l1 -padx 10 -pady 10 -anchor w
    pack $l2 -padx 10 -pady 10 -anchor w
    pack $f3 -padx 10 -pady 1
    pack $l3 -padx 10 -pady 10 -anchor w
    pack $f1 -anchor nw -fill both -expand 1

    wizard_event_loop $f1

    set value [ $tb get 1.0 end ]
    pack forget $f1
    destroy $f1

    switch $choice {
	"Cancel" {
	    set wizard_state "config_manager"
	    return
	}
	"< Back" {
	    set wizard_state "new_get_template"
	    return
	}
	"Next >" {
	    if { $value == "\n" } {
		popup [msgcat::mc "You must enter at least one word."]
		return
	    }
	    set new_config_readme $value
	    set wizard_state "new_verify"
	    return
	}
    }
}    

proc new_verify {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state 
    global new_config_name new_config_template new_config_readme

    set f1 [ wizard_page { "< Back" "Cancel" "Finish" } "Finish" "Cancel"]
    # add a header line
    set l1 [ label $f1.l1 -text [ format [ msgcat::mc "You are about to create a new EMC2 configuration.\n\nPlease verify that this is what you want:\n\nName: '%s'\nTemplate: '%s'\nDescription:" ] $new_config_name $new_config_template ] -justify left ]
    pack $l1 -pady 10
    set l2 [ label $f1.l2 -text [msgcat::mc "If this information is correct, click 'Finish' to create\nthe configuration directory and begin copying files."] -justify left ]

    # subframe for the text box and its scrollbar
    set f3 [ frame $f1.f3 -highlightt 1 ]
    #  text box
    set tb [ text $f3.tb -width 60 -height 10 -wrap word -padx 6 -pady 6 \
             -relief sunken -takefocus 1 -state normal -highlightt 0 ]
    $tb insert end $new_config_readme
    $tb configure -state disabled
    # pack the text box into its subframe
    pack $tb -side left -fill y -expand y
    # need a scrollbar
    set scr [ scrollbar $f3.sc -command "$tb yview" -takefocus 1 ]
    # link the text box to the scrollbar
    $tb configure -yscrollcommand "$scr set"
    # pack the scrollbar into the subframe
    pack $scr -fill y -side right
  
    # pack things into the main frame    
    pack $l1 -padx 10 -pady 10 -anchor w
    pack $f3 -padx 10 -pady 1
    pack $l2 -padx 10 -pady 10 -anchor w
    pack $f1 -anchor nw -fill both -expand 1

    wizard_event_loop $f1

    pack forget $f1
    destroy $f1

    switch $choice {
	"Cancel" {
	    set wizard_state "config_manager"
	    return
	}
	"< Back" {
	    set wizard_state "new_get_description"
	    return
	}
	"Finish" {
	    set wizard_state "new_do_copying"
	    return
	}
    }
}    

proc new_do_copying {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state
    global new_config_name new_config_template new_config_readme

    # set up page, only one button
    set f1 [ wizard_page { "Cancel" } {} "Cancel" ]
    set choice "none"
    # set up labels for the five progress messages that will be used
    set l1 [ label $f1.l1 -text " " -width 70 -justify left ]
    set l2 [ label $f1.l2 -text " " -width 70 -justify left ]
    set l3 [ label $f1.l3 -text " " -width 70 -justify left ]
    set l4 [ label $f1.l4 -text " " -width 70 -justify left ]
    set l5 [ label $f1.l5 -text " " -width 70 -justify left ]
    set lerr [ label $f1.lerr -text " " -width 70 -justify center ]
    # pack everything
    pack $l1 -padx 10 -pady 1
    pack $l2 -padx 10 -pady 1
    pack $l3 -padx 10 -pady 1
    pack $l4 -padx 10 -pady 1
    pack $l5 -padx 10 -pady 1
    pack $lerr -padx 10 -pady 10
    pack $f1 -anchor nw -fill both -expand 1

    # set text for first message
    $l1 configure -text [ format [ msgcat::mc "Creating new config directory '%s'..." ] $new_config_name ]
    # update display
    update
    # create directory
    file mkdir $new_config_name
    # test for success
    if { [ file isdirectory $new_config_name ] != 1 } {
	# display error message
	$lerr -text [msgcat::mc "ERROR: Config directory could not be created."]
	vwait choice
	pack forget $f1
	destroy $f1
	set wizard_state "new_config_error"
	return
    }
    $l1 configure -text [ $l1 cget -text ][ msgcat::mc "Done" ]
    update
    if { $choice != "none" } {
	pack forget $f1
	destroy $f1
	set wizard_state "new_config_error"
	return
    }

    # switch to template directory
    cd $new_config_template
    # get list of files from template dir
    set flist [ glob * ]
    # switch back to main configs directory
    cd ..

    # split list into ini and non-ini files
    set inifiles [ list ]
    set otherfiles [ list ]
    foreach fname $flist {
	set filetype "normal"
	# apply several tests to the file
	if { [ file isfile $new_config_template/$fname ] != 1 } {
	    set filetype "not_a_file"
	}
	if { [ regexp ".*\.ini$" $fname ] == 1 } {
	    set filetype "ini"
	}
	if { [regexp ".*\.bak$" $fname ] == 1 } {
	    set filetype "backup"
	}
	if { [regexp ".*\~$" $fname ] == 1 } {
	    set filetype "backup"
	}
	if { $fname == "README" } {
	    set filetype "readme"
	}
	# ok, testing complete, add it to appropriate list
	if { $filetype == "normal" } {
	    lappend otherfiles $fname
	}
	if { $filetype == "ini" } {
	    lappend inifiles $fname
	}
    }
    # now we need to parse the ini file(s) and do 2 things:
    # 1) determine what files from the common/ dir are needed
    # 2) change the ini to read those files from the local dir
    $l2 configure -text [msgcat::mc "Checking for ini file(s)..."]
    update
    if { $choice != "none" } {
	pack forget $f1
	destroy $f1
	set wizard_state "new_config_error"
	return
    }
    set commonfiles [ list ]
    foreach ininame $inifiles {
	$l2 configure -text [ format [ msgcat::mc "Transferring ini file '%s'..." ] $ininame ]
	update
	if { $choice != "none" } {
	    pack forget $f1
	    destroy $f1
	    set wizard_state "new_config_error"
	    return
	}
	set inifilein [ open $new_config_template/$ininame r ]
	# read the entire file into one long string
	set initextin [ read $inifilein ]
	close $inifilein

	# remove '../common/' so the file is fetched from the local dir
	regsub -all {\.\./common/([^[:space:]]+)} $initextin {\1} initextout

	# write to the new ini file
	set inifileout [ open $new_config_name/$ininame w ]
	puts -nonewline $inifileout $initextout
	close $inifileout

	# split ini file into lines
	set inilines [ split $initextin \n ]
	foreach iniline $inilines {
	    # add a trailing newline to each line
	    set iniline $iniline\n
	    # detect "../common/" and use substitution to remove all text before and after filename
	    if { [ regsub {.*\.\./common/([^[:space:]]+)[[:space:]].*} $iniline {\1} commonfile ] == 1 } {
		# '../common/' detected, need to copy the file from common dir
		lappend commonfiles $commonfile
	    }	
	}
    }
    # done with inifiles
    $l2 configure -text [ $l2 cget -text ][ msgcat::mc "Done" ]
    update
    if { $choice != "none" } {
	pack forget $f1
	destroy $f1
	set wizard_state "new_config_error"
	return
    }

    # do README file
    $l3 configure -text [msgcat::mc "Writing description file 'README'..."]
    update
    if { $choice != "none" } {
	pack forget $f1
	destroy $f1
	set wizard_state "new_config_error"
	return
    }
    # write to the new README file
    set readmefile [ open $new_config_name/README w ]
    puts $readmefile $new_config_readme
    close $readmefile
    $l3 configure -text [ $l3 cget -text ][ msgcat::mc "Done" ]
    update
    if { $choice != "none" } {
	pack forget $f1
	destroy $f1
	set wizard_state "new_config_error"
	return
    }

    # do other files from template dir
    $l4 configure -text [msgcat::mc "Checking for other template file(s)..."]
    update
    if { $choice != "none" } {
	pack forget $f1
	destroy $f1
	set wizard_state "new_config_error"
	return
    }
    foreach fname $otherfiles {
	$l4 configure -text [ format [ msgcat::mc "Copying template file '%s'..." ] $fname ]
	update
	if { $choice != "none" } {
	    pack forget $f1
	    destroy $f1
	    set wizard_state "new_config_error"
	    return
	}
	set filein [ open $new_config_template/$fname r ]
	# read the entire file into one long string
	set filetext [ read $filein ]
	close $filein
	# write to the new directory
	set fileout [ open $new_config_name/$fname w ]
	puts -nonewline $fileout $filetext
	close $fileout
    }
    $l4 configure -text [ $l4 cget -text ][ msgcat::mc "Done" ]
    update
    if { $choice != "none" } {
	pack forget $f1
	destroy $f1
	set wizard_state "new_config_error"
	return
    }

    $l5 configure -text [msgcat::mc "Checking for common file(s)..."]
    update
    if { $choice != "none" } {
	pack forget $f1
	destroy $f1
	set wizard_state "new_config_error"
	return
    }
    # remove duplicate common files
    # first sort, so duplicates are together
    set commontemp [ lsort $commonfiles ]
    # make an empty list
    set commonfiles [ list ]
    # compare each item on input list to last item on output list
    foreach commonfile $commontemp {
	if { $commonfile != [ lindex $commonfiles end ] } {
	    # they don't match, the input item is not yet in output
	    lappend commonfiles $commonfile
	}
    }
    foreach fname $commonfiles {
	$l5 configure -text [ format [ msgcat::mc "Copying common file '%s'..." ] $fname ]
	update
	if { $choice != "none" } {
	    pack forget $f1
	    destroy $f1
	    set wizard_state "new_config_error"
	    return
	}
	set filein [ open common/$fname r ]
	# read the entire file into one long string
	set filetext [ read $filein ]
	close $filein
	# write to the new directory
	set fileout [ open $new_config_name/$fname w ]
	puts -nonewline $fileout $filetext
	close $fileout
    }
    $l5 configure -text [ $l5 cget -text ][ msgcat::mc "Done" ]
    update
    if { $choice != "none" } {
	pack forget $f1
	destroy $f1
	set wizard_state "new_config_error"
	return
    }
    pack forget $f1
    destroy $f1
    set wizard_state "new_config_done"
}

proc new_config_done {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state
    global new_config_name run_config_name
    
    popup [ format [ msgcat::mc "Your new configuration '%s' has been created.\n" ] $new_config_name ][ msgcat::mc "Click 'OK' to return to the main menu." ]
    # set default run config to the newly created one
    set run_config_name $new_config_name
    set wizard_state "config_manager"
    return
}

proc new_config_error {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state
    
    # not done yet
    popup [ msgcat::mc "An error happened while creating the new configuration.\nIdeally this code would clean everything up and\nthen return to the main menu, but the cleanup isn't done yet.\n" ][ msgcat::mc "Click 'OK' to return to the main menu." ]
    set wizard_state "config_manager"
    return
}


proc delete_verify {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state 
    global selected_config_name

    set f1 [ wizard_page { "Cancel" "Finish" } "" "Cancel"]
    # add a header line
    set l1 [ label $f1.l1 -text [ format [ msgcat::mc "WARNING!\n\nYou are about to delete the existing EMC2 configuration called:\n\n'%s'\n\nThis means that the directory\n'%s'\nand all files and subdirectories in it will be deleted." ] $selected_config_name [ file join [ pwd ] $selected_config_name ] ] -justify left ]
    pack $l1 -pady 10 -anchor w
    pack $f1 -anchor nw -fill both -expand 1

    wizard_event_loop $f1

    pack forget $f1
    destroy $f1

    switch $choice {
	"Cancel" {
	    set wizard_state "config_manager"
	    return
	}
	"Finish" {
	    set wizard_state "delete_do_it"
	    return
	}
    }
}    

proc delete_do_it {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state
    global selected_config_name

    # set up page, only one button
    set f1 [ wizard_page { "Cancel" } "" "Cancel"]
    set choice "none"
    # set up labels
    set l1 [ label $f1.l1 -text [ format [ msgcat::mc "Deleting config '%s'..." ] $selected_config_name ] -width 70 -justify left ]
    set lerr [ label $f1.lerr -text " " -width 70 -justify center ]
    pack $l1 -padx 10 -pady 1 -anchor w
    pack $lerr -padx 10 -pady 10
    pack $f1 -anchor nw -fill both -expand 1
    # update display
    update
    # verify that we are in the right place
    if { [ file isdirectory $selected_config_name ] != 1 } {
	# display error message
	$lerr -text [msgcat::mc "ERROR: Config directory not found."]
	vwait choice
	pack forget $f1
	destroy $f1
	set wizard_state "delete_config_error"
	return
    }
    # switch to the directory
    cd $selected_config_name
    # delete everything (except hidden files)
    eval exec rm -rf [ glob * ]
    # delete hidden files (ones that start with '.' but not '..' )
    regsub \.\. [ glob .?* ] "" hidden
    eval exec rm -rf $hidden
    # switch back to main configs directory
    cd ..
    # delete the directory
    exec rmdir $selected_config_name
    # all done, update GUI
    update
    # and clean up
    if { $choice != "none" } {
	pack forget $f1
	destroy $f1
	set wizard_state "delete_config_error"
	return
    }
    pack forget $f1
    destroy $f1
    set wizard_state "delete_config_done"
}

proc delete_config_done {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state
    global selected_config_name
    
    popup [ format [ msgcat::mc "The configuration '%s' has been deleted.\n" ] $selected_config_name ][ msgcat::mc "Click 'OK' to return to the main menu." ]
    # clear selected config name, that config is gone now
    set selected_config_name ""
    set wizard_state "config_manager"
    return
}

proc delete_config_error {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state
    
    # not done yet
    popup [ msgcat::mc "An error happened while deleting the configuration.\n" ][ msgcat::mc "Click 'OK' to return to the main menu." ]
    set wizard_state "config_manager"
    return
}

proc browse_save {e1} {
    set p [$e1 get]
    set f [tk_getSaveFile \
        -initialdir [file dirname $p] \
        -initialfile  [file tail $p] \
        -parent . \
        -title "Save backup as" \
        -filetypes {
            {{Compressed TAR file} {.tar.gz}}
    }]
    if {$f != ""} { $e1 delete 0 end; $e1 insert end $f }
}

proc backup_verify {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state 
    global selected_config_name backup_path

    set f1 [ wizard_page { "Cancel" "Finish" } "Finish" "Cancel"]
    # add a header line
    
    set timetag [ clock format [ clock seconds ] -format %Y-%m-%d-%H%M ]
    set backup_file $selected_config_name-$timetag
    set backup_path [ file join [ pwd ] backup $backup_file.tar.gz ]
    
    # introductory message    
    set l1 [ label $f1.l1 -text [ format [ msgcat::mc "You are about to create a backup of the '%s'configuration.\nThe backup will be stored in the file shown below (change the name if you want it elsewhere)." ] $selected_config_name ] -justify left]
    pack $l1 -pady 10 -anchor w
    
    frame $f1.filename
    # entry with the filename
    set e1 [ entry $f1.filename.e1 -width 0 ]
    $e1 insert 0 $backup_path
    pack $e1 -side left -fill x -expand 1
    button $f1.filename.browse -command [list browse_save $e1] \
        -text Browse... -pady 0
    pack $f1.filename.browse -side left -fill y
    pack $f1.filename -fill x -expand 0
    pack $f1 -anchor nw -fill both -expand 1

    wizard_event_loop $f1

    set backup_path [ $e1 get ]
    pack forget $f1
    destroy $f1

    switch $choice {
	"Cancel" {
	    set wizard_state "config_manager"
	    return
	}
	"Finish" {
	
	    set wizard_state "backup_do_it"
	    return
	}
    }
}    

proc backup_do_it {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state
    global selected_config_name backup_path

    # set up page, only one button
    set f1 [ wizard_page { "Cancel" } "" "Cancel"]
    set choice "none"
    # set up labels
    set l1 [ label $f1.l1 -text [ format [ msgcat::mc "Backing up config '%s'..." ] $selected_config_name ] -width 70 -justify left ]
    set lerr [ label $f1.lerr -text " " -width 70 -justify center ]
    pack $l1 -padx 10 -pady 1 -anchor w
    pack $lerr -padx 10 -pady 10
    pack $f1 -anchor nw -fill both -expand 1
    # update display
    update
    # verify that we are in the right place
    if { [ file isdirectory $selected_config_name ] != 1 } {
	# display error message
	$lerr -text [msgcat::mc "ERROR: Config directory not found."]
	vwait choice
	pack forget $f1
	destroy $f1
	set wizard_state "backup_config_error"
	return
    }
    # make sure there is a configs/backup directory
    if { [ file isdirectory backup ] != 1 } {
	# create it
	exec mkdir backup
	# see if that worked
	if { [ file isdirectory backup ] != 1 } {
	    $lerr -text [msgcat::mc "ERROR: Could not create backup directory."]
	    vwait choice
	    pack forget $f1
	    destroy $f1
	    set wizard_state "backup_config_error"
	    return
	}
    }
    
    # this is less than perfect - for one thing, if there is a CVS subdir it recurses
    # down into it.
    
    exec tar -czf $backup_path $selected_config_name
    
    # all done, update GUI
    update
    # and clean up
    if { $choice != "none" } {
	pack forget $f1
	destroy $f1
	set wizard_state "backup_config_error"
	return
    }
    pack forget $f1
    destroy $f1
    set wizard_state "backup_config_done"
}


proc backup_config_done {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state
    global selected_config_name backup_path
    
    popup [ format [ msgcat::mc "The configuration '%s' has been backed up to\n'%s'\n\n" ] $selected_config_name $backup_path ][ msgcat::mc "Click 'OK' to return to the main menu." ]
    set wizard_state "config_manager"
    return
}

proc backup_config_error {} {
    # need globals to communicate with wizard page buttons
    global choice top wizard_state
    
    # not done yet
    popup [ msgcat::mc "An error happened while backing up the configuration.\n" ][ msgcat::mc "Click 'OK' to return to the main menu." ]
    set wizard_state "config_manager"
    return
}

# config validator
#
# this gets a config name, which can have multiple forms
# it returns either a fully qualified path to the config directory
# (or ini file, if specified), or displays an error message and returns ""
#
# input can be:
#   name of config dir (assumed to be inside emc2 main configs dir)
#   name of config dir and ini file (assumed to be inside main configs dir)
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


################ MAIN PROGRAM STARTS HERE ####################

# set options that are common to all widgets
foreach class { Button Entry Label Listbox Scale Text } {
    option add *$class.borderWidth 1  100
}

# make a toplevel and a master frame.
wm title . [msgcat::mc "EMC2 Configuration Manager"]
set logo [label .logo -image $logo]
set top [frame .main -borderwidth 0 -relief flat ]
# want these too, but on windoze they cause an error? -padx 10 -pady 10 ]
pack $logo -side left -anchor nw
pack $top -side left -expand yes -fill both

# parse command line 

# did the user specify --configs-dir?
set configs_index [ lsearch $argv "--configs-dir" ]
if { $configs_index >= 0 } {
    # yes, get the directory name
    set configs_dir [ lindex $argv [ expr $configs_index + 1 ]]
    if { $configs_dir == "" || [ string equal -length 1 $configs_dir "-" ] == 1 } {
	popup [msgcat::mc "ERROR: option '--configs-dir' must be followed by a directory name.\n\n"][ msgcat::mc "Click 'OK' to exit." ]
	exit -1
    }
    if { [ file isdirectory $configs_dir ] != 1 } {
	popup [ format [ msgcat::mc "ERROR: '--configs-dir' argument is\n\n'%s'\n\nwhich is not a directory.\n\n"] $configs_dir ][ msgcat::mc "Click 'OK' to exit." ]
	exit -1
    }
    # make into absolute path to directory
    set configs_dir [ file_normalize $configs_dir ]
    # remove option and its argument from argv
    set argv [ lreplace $argv $configs_index [expr $configs_index + 1]]
} else {
    # configs dir not specified,figure it out later
    set configs_dir ""
}
# we have several alternate ways to find the configs directory
# we'll try them until one succeeds (or all fail and we give up)
if { $configs_dir == "" } {
    # try env variable
    if { [ info exists env(EMC2_CONFIG_DIR) ] } {
	set configs_dir [ file_normalize $env(EMC2_CONFIG_DIR) ]
	if { [ file isdirectory $configs_dir ] != 1 } {
	    popup [ format [ msgcat::mc "ERROR: environment variable EMC2_CONFIG_DIR is\n\n'%s'\n\nwhich is not a directory.\n\n" ] $env(EMC2_CONFIG_DIR) ][ msgcat::mc "Click 'OK' to exit." ]
	    exit -1
	}
    }
}
if {$configs_dir == ""} {
    # maybe we're running in the top level EMC2 dir
    if { [ file isdirectory "configs" ] } {
	set configs_dir [ file_normalize "configs" ]
    }
}
if {$configs_dir == ""} {
    # maybe we're running in the configs dir or another at that level
    if { [ file isdirectory "../configs" ] } {
	set configs_dir [ file_normalize "../configs" ]
    }
}
if {$configs_dir == ""} {
    # maybe we're running in an inidividual config dir
    if { [ file isdirectory "../../configs" ] } {
	set configs_dir [ file_normalize "../../configs" ]
    }
}
# if we still don't know where the configs are, we're screwed....
if {$configs_dir == ""} {
    # give up
    popup [msgcat::mc "ERROR: Cannot find the EMC2 configurations directory.\nYou can specify the directory with the '--configs-dir <dir>' option.\n\n"][ msgcat::mc "Click 'OK' to exit." ]
    exit -1
}

# check for more than one of --new, --run, and --getconfig
set option_name ""
set option_index ""
set num_opts 0
foreach option_type { "--new" "--run" "--get-config" } {
    set temp [ lsearch $argv $option_type ]
    if { $temp >= 0 } {
	set num_opts [ expr $num_opts + 1 ]
	set option_name $option_type
	set option_index $temp
    }
}
if { $num_opts > 1 } {
    popup [ msgcat::mc "ERROR: options '--run', '--new', and '--get-config' are\nmutually exclusive, please specify only one.\n\n"][msgcat::mc "Click 'OK' to exit."]
    exit -1
}

# process --run, --new, or --get-config
switch -- $option_name {
    "--run" {
	# get arg that follows '--run' (if any)
	set run_config_name [ lindex $argv [ expr $option_index + 1 ] ]
	# delete option and its arg from argv
	set argv [ lreplace $argv $option_index [expr $option_index + 1] ]
	# validate filename
	set run_config_name [ resolve_config $run_config_name ]
	# set mode and initial state
	set run_mode "run"
	set wizard_state "check_run_config"
    }
    "--get-config" {
	# get arg that follows '--get-config' (if any)
	set run_config_name [ lindex $argv [ expr $option_index + 1 ] ]
	# delete option and its arg from argv
	set argv [ lreplace $argv $option_index [expr $option_index + 1] ]
	# validate filename
	set run_config_name [ resolve_config $run_config_name ]
	# set mode and initial state
	set run_mode "print"
	set wizard_state "check_run_config"
    }
    "--new" {
	# delete option from argv
	set argv [ lreplace $argv $option_index $option_index ]
	# set mode and initial state
	set run_config_name ""
	set run_mode "run"
	set wizard_state "new_intro"
    }
    "" {
	# set mode and initial state
	set run_config_name ""
	set run_mode "run"
	set wizard_state "main_page"
    }
}


# at this point all legal options and their args have been deleted, 
# anything left in 'argv' is an error
if { [ llength $argv ] != 0 } {
    popup [ format [ msgcat::mc "ERROR: unknown command line option:\n\n'%s'\n\n"] $argv ][msgcat::mc "Click 'OK' to exit."]
    exit -1
}

set old_dir [ pwd ]
cd $configs_dir

proc state_machine {} {
    global choice wizard_state run_config_name

    while { $wizard_state != "quit" } {
	# execute the code associated with the current state
	$wizard_state
    }
}

wm geo . 780x480
wm resiz . 0 0
state_machine

cd $old_dir

exit


