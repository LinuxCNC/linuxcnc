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
#   find the directory itself, but that works only if it
#   was run from the configs dir, or one directly above
#   or below it.
#
#  --run [<config-name>/<ini-name>]
#
#   Validate the config-name/ini-name that was supplied (if
#   any), if not supplied or invalid, go directly to the 
#   screen that asks the user to choose a config.  Once
#   the config is validated (or the user chooses one and 
#   clicks "RUN", invoke the run script to run EMC using
#   that configuration.
#
#  --get-config [<config-name>/<ini-name>]
#
#   Validate the config-name/ini-name that was supplied (if
#   any), if not supplied or invalid, go directly to the 
#   screen that asks the user to choose a config.  Once
#   the config is validated (or the user chooses one and 
#   clicks 'RUN', do _not_ invoke the run script to run it,
#   instead simply print the chosen <config/ini> string and
#   end.  (This mode is for use by the run script.)
#
# --run and --get-config are mutually exclusive.  If neither
# is specificed, setupconfig displays a sequence of menus so
# the user can manipulate configurations and run emc. 
#
###############################################################


################### PROCEDURE DEFINITIONS #####################

# reads the directory, and fills in "config_list" and "details_list"

proc get_config_list {} {
    global config_list details_list basedir

    # clear config and description lists
    set config_list [ list ]
    set details_list [ list ]
    # look at all subdirs
    foreach dir_name [ glob */ ] {
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
		lappend details_list "No details available."
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
    global choice top

    set f1 [ frame $top.f1 ]
    set lbl [ label $f1.lbl -text $message -padx 20 -pady 10 ]
    set but [ button $f1.but -text OK -command "button_pushed OK" ]
    pack $lbl -side top
    pack $but -side bottom -padx 10 -pady 10
    pack $f1
    set choice "none"
    vwait choice
    pack forget $f1
    destroy $f1
}

# generic wizard page - defines a frame with multiple buttons at the bottom
# Returns the frame widget so page specific stuff can be packed on top

proc wizard_page { buttons } {
    global choice top

    set f1 [ frame $top.f1 ]
    set f2 [ frame $f1.f2 ]
    foreach button_name $buttons {
	set bname [ string tolower $button_name ]
        button $f2.$bname -text $button_name -command "button_pushed $button_name"
	pack $f2.$bname -side left -padx 10 -pady 10
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
    set l1 [ label $f1.l1 -text $item_text ]
    pack $l1 -pady 6

    # subframe for the list and its scrollbar
    set f2 [ frame $f1.f2 ]

    # listbox for the items
    set lb [ listbox $f2.lb ]
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
    set max_items 6
    if { [ $lb size ] >= $max_items } {
	# need a scrollbar
	set lscr [ scrollbar $f2.scr -command "$lb yview" ]
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
    pack $f2

    # label for the details box
    set l2 [ label $f1.l2 -text $detail_text ]
    pack $l2 -pady 6
	
    # subframe for the detail box and its scrollbar
    set f3 [ frame $f1.f3 ]
    # a text box to display the details
    set tb [ text $f3.tb -width 60 -height 10 -wrap word -padx 6 -pady 6 \
             -relief sunken -takefocus 0 -state disabled ]
    set d_p_detail_widget $tb
    # pack the text box into its subframe
    pack $tb -side left -fill y -expand y
    # need a scrollbar
    set dscr [ scrollbar $f3.scr -command "$tb yview" ]
    # link the text box to the scrollbar
    $tb configure -yscrollcommand "$dscr set"
    # pack the scrollbar into the subframe
    pack $dscr -fill y -side right
    # pack the subframe into the main frame
    pack $f3
    # and finally pack the main frame into the parent
    pack $f1
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

################### MENU PROCEDURE DEFINITIONS #####################

proc main_page {} {
    # need globals to comminicate with wizard page buttons
    global choice top wizard_state

    set t10 "Welcome to EMC2!\n"
    set t20 "To run EMC2 with an existing configuration,"
    set t21 "click the RUN button.\n"
    set t30 "To create a new configuration, edit a configuration, backup or"
    set t31 "restore a configuration, or do other configuration related tasks,"
    set t32 "click the CONFIG button.\n"

    set f1 [ wizard_page { "CONFIG" "QUIT" "RUN" } ]
    set l1 [ label $f1.l1 -text [ join [ list $t10 $t20 $t21 $t30 $t31 $t32 ] \n ]]
    pack $l1 -padx 10 -pady 10
    pack $f1

    set choice "none"
    vwait choice
    pack forget $f1
    destroy $f1

    switch $choice {
	"QUIT" {
	    set wizard_state "quit"
	    return
	}
	"CONFIG" {
	    set wizard_state "config_manager"
	    return
	}
	"RUN" {
	    set wizard_state "choose_run_config"
	    return
	}
    }
}    

proc choose_run_config {} {
    # need globals to comminicate with wizard page buttons,
    # and for the configuration lists
    global choice top wizard_state
    global config_list details_list detail_picker_selection
    # more globals for new and run config info
    global new_config_name new_config_template new_config_readme
    global run_config_name run_ini_name

    # read the directory and set up list of configs
    get_config_list

    # messages
    set t1 "Please select an EMC2 configuration from the list below and click 'RUN'."
    set t2 "\nDetails about the selected configuration:"
    
    #set up a wizard page with two buttons
    set f1 [ wizard_page { "CANCEL" " RUN " } ]
    # add a detail picker to it with the configs
    detail_picker $f1 $t1 $config_list $t2 $details_list
    # done
    pack $f1

    # prep for the event loop
    set choice "none"
    # enter event loop
    vwait choice
    # a button was pushed, save selection
    set value $detail_picker_selection
    # clear the window
    pack forget $f1
    destroy $f1

    switch $choice {
	"CANCEL" {
	    set wizard_state "main_page"
	    return
	}
	"RUN" {
	    if { $value == "" } {
		popup "You must choose a config if you want to run EMC2!"
		return
	    }
	    set run_config_name $value
	    set wizard_state "choose_run_ini"
	    return
	}
    }
}

proc choose_run_ini {} {
    # need globals to comminicate with wizard page buttons
    global choice top wizard_state
    
    # not done yet
    popup "The next step is to see if there is more than one .ini fil in\nthe chosen config, and if so, to pick one.\n\nBut thats not coded yet, so when you click OK, the program will end"
    set wizard_state "quit"
    return
}

proc config_manager {} {
    # need globals to comminicate with wizard page buttons
    global choice top wizard_state
    global config_list details_list detail_picker_selection
    # more globals for new and run config info
    global new_config_name new_config_template new_config_readme
    global run_config_name run_ini_name

    # read the directory and set up list of configs
    get_config_list

    # messages
    set t1 "This page is used to manage EMC2 machine configurations.\n"
    set t2 "The list below shows all of the existing EMC2\nconfigurations on this computer.\n\nSelect a config, then click one of the buttons below.\n"
    set t3 "\nDetails about the selected configuration:"

    set f1 [ wizard_page { "EDIT" "BACKUP" "RESTORE" "DELETE" "NEW" "CANCEL" } ]
    set l1 [ label $f1.l1 -text $t1 ]
    pack $l1 -padx 10 -pady 2
    # add a detail picker to it with the configs
    detail_picker $f1 $t2 $config_list $t3 $details_list
    pack $f1

    # prep for the event loop
    set choice "none"
    # enter event loop
    vwait choice
    # a button was pushed, save selection
    set value $detail_picker_selection
    # clear the window
    pack forget $f1
    destroy $f1

    switch $choice {
	"CANCEL" {
	    set wizard_state "main_page"
	    return
	}
	"EDIT" {
	    if { $value == "" } {
		popup "You must choose a config first!"
		return
	    }
	    set selected_config_name $value
	    set wizard_state "not_implemented"
	    return
	}
	"BACKUP" {
	    if { $value == "" } {
		popup "You must choose a config first!"
		return
	    }
	    set selected_config_name $value
	    set wizard_state "not_implemented"
	    return
	}
	"RESTORE" {
	    if { $value == "" } {
		popup "You must choose a config first!"
		return
	    }
	    set selected_config_name $value
	    set wizard_state "not_implemented"
	    return
	}
	"DELETE" {
	    if { $value == "" } {
		popup "You must choose a config first!"
		return
	    }
	    set selected_config_name $value
	    set wizard_state "not_implemented"
	    return
	}
	"NEW" {
	    set wizard_state "new_intro"
	    return
	}
    }
}    

proc not_implemented {} {
    # need globals to comminicate with wizard page buttons
    global choice top wizard_state
    
    # not done yet
    popup "Sorry, this function is not implemented yet.\n\nClick OK to return to the menu"
    set wizard_state "config_manager"
    return
}

proc new_intro {} {
    # need globals to comminicate with wizard page buttons
    global choice top wizard_state
    global new_config_name new_config_template new_config_readme

    set f1 [ wizard_page { "<--BACK" "CANCEL" "NEXT-->" } ]
    set l1 [ label $f1.l1 -text "You have chosen to create a new EMC2 configuration.\n\nThe next few screens will walk you through the process." ]
    pack $l1 -padx 10 -pady 10
    pack $f1

    # get ready for a fresh start
    set new_config_name ""
    set new_config_template ""
    set new_config_readme ""
   
    set choice "none"
    vwait choice
    pack forget $f1
    destroy $f1

    switch $choice {
	"CANCEL" {
	    set wizard_state "config_manager"
	    return
	}
	"<--BACK" {
	    set wizard_state "config_manager"
	    return
	}
	"NEXT-->" {
	    set wizard_state "new_get_name"
	    return
	}
    }
}    

proc new_get_name {} {
    # need globals to comminicate with wizard page buttons
    global choice top wizard_state new_config_name

    set f1 [ wizard_page { "<--BACK" "CANCEL" "NEXT-->" } ]
    set l1 [ label $f1.l1 -text "Please select a name for your new configuration." ]
    set l2 [ label $f1.l2 -text "(This will become a directory name, so please use only letters,\ndigits, period, dash, or underscore.)" ]
    set e1 [ entry $f1.e1 -width 30 -relief sunken -bg white -takefocus 1 ]
    $e1 insert 0 $new_config_name
    pack $l1 -padx 10 -pady 10
    pack $e1 -padx 10 -pady 1
    pack $l2 -padx 10 -pady 10
    pack $f1

    set choice "none"
    vwait choice
    set value [ $e1 get ]
    pack forget $f1
    destroy $f1

    switch $choice {
	"CANCEL" {
	    set wizard_state "config_manager"
	    return
	}
	"<--BACK" {
	    set wizard_state "new_intro"
	    return
	}
	"NEXT-->" {
	    if { $value == "" } {
		popup "You must enter a name!"
		return
	    }
	    if { [ regexp {[^[:alnum:]_\-.]} $value ] == 1 } {
		popup "'$value' contains illegal characters!\nPlease choose a new name."
		return
	    }
	    if { [ file exists $value ] == 1 } {
		popup "A directory or file called '$value' already exists!\nPlease choose a new name."
		return
	    }
	    set new_config_name $value
	    set wizard_state "new_get_template"
	    return
	}
    }
}    
    
proc new_get_template {} {
    # need globals to comminicate with wizard page buttons,
    # and for the configuration lists
    global choice top wizard_state
    global config_list details_list detail_picker_selection
    global new_config_name new_config_template new_config_readme

    # read the directory and set up list of possible templates
    get_config_list

    # messages
    set t1 "Please select one of these existing configurations as the template\nfor your new configuration.\n\nAll the files associated with the template will be copied into your new\nconfig, so you can make whatever modifications are needed."
    set t2 "\nDetails about the selected configuration:"
    
    #set up a wizard page with three buttons
    set f1 [ wizard_page { "<--BACK" "CANCEL" "NEXT-->" } ]
    # add a header line
    set l1 [ label $f1.l1 -text "Creating new EMC2 configuration '$new_config_name'" ]
    pack $l1 -pady 10
    # add a detail picker to it with the configs
    detail_picker $f1 $t1 $config_list $t2 $details_list
    # done
    pack $f1

    set choice "none"
    vwait choice
    set value $detail_picker_selection
    pack forget $f1
    destroy $f1

    switch $choice {
	"CANCEL" {
	    set wizard_state "config_manager"
	    return
	}
	"<--BACK" {
	    set wizard_state "new_get_name"
	    return
	}
	"NEXT-->" {
	    if { $value == "" } {
		popup "You must choose a template!"
		return
	    }
	    if { [ file isdirectory $value ] != 1 } {
		popup "A internal error has occurred, or the template directory was deleted.\nClick OK to quit"
		set wizard_state "quit"
		return
	    }
	    set new_config_template $value
	    # look up the details that match this template
	    # NOTE: we do this here, instead of at the beginning of "new_get_description"
	    #  so that anything the user has typed survives if he goes back to the new
	    #  description page.  His data is only overwritten by the template if he
	    #  goes all the way back to the template slection page (this one).
	    if { [ file isfile "$new_config_template/README" ] } {
		# description found, read it
		set descr [ read -nonewline [ open "$new_config_template/README" ]]
		# and save it
		set new_config_readme $descr
	    } else {
		# no description, gotta tell the user something
		set new_config_readme "Enter a description here"
	    }
	    set wizard_state "new_get_description"
	    return
	}
    }
}

proc new_get_description {} {
    # need globals to comminicate with wizard page buttons
    global choice top wizard_state 
    global new_config_name new_config_template new_config_readme

    set f1 [ wizard_page { "<--BACK" "CANCEL" "NEXT-->" } ]
    # add a header line
    set l1 [ label $f1.l1 -text "Creating new EMC2 configuration '$new_config_name'\nbased on template '$new_config_template'" ]
    pack $l1 -pady 10
    set l2 [ label $f1.l2 -text "Please enter a description of your configuration.\n\nThe box below has been preloaded with the description of the template, but\nit is strongly recommended that you revise it.  At a minimum,\nput your name and some specifics about your machine here." ]
    set l3 [ label $f1.l3 -text "(If you ever need help, someone may ask you to send them your\nconfiguration, and this information could be very usefull.)" ]

    # subframe for the text entry box and its scrollbar
    set f3 [ frame $f1.f3 ]
    #  text box
    set tb [ text $f3.tb -width 60 -height 10 -wrap word -padx 6 -pady 6 \
             -relief sunken -takefocus 1 -state normal -bg white ]
    $tb insert end $new_config_readme
    # pack the text box into its subframe
    pack $tb -side left -fill y -expand y
    # need a scrollbar
    set scr [ scrollbar $f3.sc -command "$tb yview" ]
    # link the text box to the scrollbar
    $tb configure -yscrollcommand "$scr set"
    # pack the scrollbar into the subframe
    pack $scr -fill y -side right
  
    # pack things into the main frame    
    pack $l1 -padx 10 -pady 10
    pack $l2 -padx 10 -pady 10
    pack $f3 -padx 10 -pady 1
    pack $l3 -padx 10 -pady 10
    pack $f1

    set choice "none"
    vwait choice
    set value [ $tb get 1.0 end ]
    pack forget $f1
    destroy $f1

    switch $choice {
	"CANCEL" {
	    set wizard_state "config_manager"
	    return
	}
	"<--BACK" {
	    set wizard_state "new_get_template"
	    return
	}
	"NEXT-->" {
	    if { $value == "\n" } {
		popup "You must enter at least one word!"
		return
	    }
	    set new_config_readme $value
	    set wizard_state "new_verify"
	    return
	}
    }
}    

proc new_verify {} {
    # need globals to comminicate with wizard page buttons
    global choice top wizard_state 
    global new_config_name new_config_template new_config_readme

    set f1 [ wizard_page { "<--BACK" "CANCEL" "NEXT-->" } ]
    # add a header line
    set l1 [ label $f1.l1 -text "You are about to create a new EMC2 configuration.\n\nPlease verify that this is what you want:\n\nName '$new_config_name'\nTemplate: '$new_config_template'\nDescription:" ]
    pack $l1 -pady 10
    set l2 [ label $f1.l2 -text "If this information is correct, click NEXT to create\nthe configuration directory and begin copying files." ]

    # subframe for the text box and its scrollbar
    set f3 [ frame $f1.f3 ]
    #  text box
    set tb [ text $f3.tb -width 60 -height 10 -wrap word -padx 6 -pady 6 \
             -relief sunken -takefocus 1 -state normal ]
    $tb insert end $new_config_readme
    $tb configure -state disabled
    # pack the text box into its subframe
    pack $tb -side left -fill y -expand y
    # need a scrollbar
    set scr [ scrollbar $f3.sc -command "$tb yview" ]
    # link the text box to the scrollbar
    $tb configure -yscrollcommand "$scr set"
    # pack the scrollbar into the subframe
    pack $scr -fill y -side right
  
    # pack things into the main frame    
    pack $l1 -padx 10 -pady 10
    pack $f3 -padx 10 -pady 1
    pack $l2 -padx 10 -pady 10
    pack $f1

    set choice "none"
    vwait choice
    pack forget $f1
    destroy $f1

    switch $choice {
	"CANCEL" {
	    set wizard_state "config_manager"
	    return
	}
	"<--BACK" {
	    set wizard_state "new_get_description"
	    return
	}
	"NEXT-->" {
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
    set f1 [ wizard_page { "CANCEL" } ]
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
    pack $f1

    # set text for first message
    $l1 configure -text "Creating new config directory '$new_config_name'..."
    # update display
    update
    # create directory
    file mkdir $new_config_name
    # test for success
    if { [ file isdirectory $new_config_name ] != 1 } {
	# display error message
	$lerr -text "ERROR: Config directory could not be created."
	vwait choice
	set wizard_state "new_config_error"
	pack forget $f1
	destroy $f1
	return
    }
    $l1 configure -text "[ $l1 cget -text ] Done"
    update
    if { $choice != "none" } {
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
    $l2 configure -text "Checking for ini file(s)..."
    update
    if { $choice != "none" } {
	set wizard_state "new_config_error"
	return
    }
    set commonfiles [ list ]
    foreach ininame $inifiles {
	$l2 configure -text "Transferring ini file '$fname'..."
	update
	if { $choice != "none" } {
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
    $l2 configure -text "[ $l2 cget -text ] Done"
    update
    if { $choice != "none" } {
	set wizard_state "new_config_error"
	return
    }

    # do README file
    $l3 configure -text "Writing description file 'README'..."
    update
    # write to the new README file
    set readmefile [ open $new_config_name/README w ]
    puts $readmefile $new_config_readme
    close $readmefile
    $l3 configure -text "[ $l3 cget -text ] Done"
    update
    if { $choice != "none" } {
	set wizard_state "new_config_error"
	return
    }

    # do other files from template dir
    $l4 configure -text "Checking for other template file(s)..."
    update
    if { $choice != "none" } {
	set wizard_state "new_config_error"
	return
    }
    foreach fname $otherfiles {
	$l4 configure -text "Copying template file '$fname'..."
	update
	if { $choice != "none" } {
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
    $l4 configure -text "[ $l4 cget -text ] Done"
    update
    if { $choice != "none" } {
	set wizard_state "new_config_error"
	return
    }

    $l5 configure -text "Checking for common file(s)..."
    update
    if { $choice != "none" } {
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
	$l5 configure -text "Copying common file '$fname'..."
	update
	if { $choice != "none" } {
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
    $l5 configure -text "[ $l5 cget -text ] Done"
    update
    if { $choice != "none" } {
	set wizard_state "new_config_error"
	return
    }

    pack forget $f1
    destroy $f1
    set wizard_state "new_config_done"
}

proc new_config_done {} {
    # need globals to comminicate with wizard page buttons
    global choice top wizard_state
    global new_config_name
    
    popup "Your new configuration '$new_config_name' has been created.\nClick OK to return to the main menu."
    set wizard_state "config_manager"
    return
}

proc new_config_error {} {
    # need globals to comminicate with wizard page buttons
    global choice top wizard_state
    
    # not done yet
    popup "An error happened while creating the new configuration.\nIdeally this code would clean everything up and\nthen return to the main menu, but the cleanup isn't done yet.\nClick OK to return to the main menu."
    set wizard_state "main_page"
    return
}


################ MAIN PROGRAM STARTS HERE ####################


proc state_machine {} {
    global choice wizard_state

    while { $wizard_state != "quit" } {
	puts "state is $wizard_state"
	# execute the code associated with the current state
	$wizard_state
    }
}

# set options that are common to all widgets
foreach class { Button Entry Label Listbox Scale Text } {
    option add *$class.borderWidth 1  100
}

# locate the configs directory
set basedir ""
if {[info exists env(EMC2_ORIG_CONFIG_DIR)]} {
    set basedir $env(EMC2_ORIG_CONFIG_DIR)
}
if {$basedir == ""} {
    # maybe we're running in the top level EMC2 dir
    if { [ file isdirectory "configs" ] } {
	set basedir configs/
    }
}
if {$basedir == ""} {
    # maybe we're running in the configs dir or another at that level
    if { [ file isdirectory "../configs" ] } {
	set basedir ../configs
    }
}
if {$basedir == ""} {
    # maybe we're running in an inidividual config dir
    if { [ file isdirectory "../../configs" ] } {
	set basedir ../
    }
}
if {$basedir == ""} {
    # give up
    puts "Can't find configs directory"
    puts "Check environment variable EMC2_ORIG_CONFIG_DIR"
    exit -1
}

set old_dir [ pwd ]
cd $basedir

# make a toplevel and a master frame.
wm title . "EMC2 Configuration Manager"
set top [frame .main -borderwidth 2 -relief raised ]
# want these too, but on windoze they cause an error? -padx 10 -pady 10 ]
pack $top -expand yes -fill both

# initialize a bunch of globals
set run_config_name ""
set run_ini_name ""
set selected_config_name ""

set wizard_state "main_page"

if { $argc == 1 } {
    if { [ lindex $argv 0 ] == "-run" } {
	set wizard_state "choose_run_config"
    } else {
	puts "need to validate and run config '[ lindex $argv 0 ]'"
	puts "without popping up a GUI window"
	puts "but that isn't done yet, so we exit instead"
	exit -1
    }
}

state_machine
puts Quitting!

cd $old_dir

exit


