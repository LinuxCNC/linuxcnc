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
option add *Entry*background white
option add *Listbox*background white
option add *Tree*background white

################### PROCEDURE DEFINITIONS #####################

#first define some default directories
set TCLDIR tcl
set LANGDIR src/po

if {[info exists env(EMC2_TCL_DIR)]} {
    set TCLDIR $env(EMC2_TCL_DIR)
}

if {[info exists env(EMC2_LANG_DIR)]} {
    set LANGDIR $env(EMC2_LANG_DIR)
}

package require msgcat
if ([info exists env(LANG)]) {
    msgcat::mclocale $env(LANG)
    msgcat::mcload $LANGDIR
}

# use initialize_config for bwidget and .emcrc

proc initialize_config {} {
    # need bwidget
    set result [catch {package require BWidget 1.7}]
    if {$result != 0} {
        puts stderr $result
        tk_messageBox -icon error -type ok -message [msgcat::mc "Can't find the bwidget 1.7 package.  There is a debian bwidget package; install \nit with sudo apt-get install bwidget."]
        exit
        destroy .
    } else {
        if {[catch {open ~/.emcrc} programin]} {
            return 
        } else {
            set rcstring [read $programin]
            catch {close $programin}
            set ret [getVal $rcstring PICKCONFIG LAST_CONFIG ]
            return $ret
        }
    }
}


# FIXME add trap for comment on the first of a line.
proc getVal {stringa sect var} {
    set x [regexp -indices -- "$sect.*$var *= *" $stringa  indexes]
    if {$x } {
        set startindex [lindex $indexes 1]
        set x [regexp -start $startindex -linestop -- ".*" \
            $stringa varval ]
        set varval [string trim $varval]
    }
    return $varval
}

proc setVal {stringb sect var newval} {
    set x [regexp -indices -- "$sect.*$var *= *" $stringb  start ]
    if {$x } {
        set startindex [expr [lindex $start 1] +1]
        set x [regexp -indices -start $startindex -linestop -- ".*" $stringb end ]
        set endindex [lindex $end 1]
        set newstring [string replace $stringb $startindex $endindex $newval]
        return $newstring
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
    $tree see $node
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

set last_ini "none"
set last_ini [initialize_config]

proc SW { args } {
    set res [eval ScrolledWindow $args]
    $res.vscroll configure -relief flat
    ::${res}:cmd configure -highlightthickness 1
    return $res
}
# a frame for packing the top window
set f1 [ frame $top.f1 ]

set message [msgcat::mc "Welcome to EMC2.\n\nSelect a machine configuration from the list on the left.\nDetails about the selected configuration will appear in the display on the right.\nClick 'OK' to run the selected configuration"]

set lbl [ label $f1.lbl -text $message -justify left -padx 15 -pady 10 -wraplength 600 ]
pack $lbl -anchor w

# a subframe for the tree/detail box 
set f2 [ frame $f1.f2 -borderwidth 0 -relief flat -padx 15 ]

# Let the tree scroll
set s1 [ SW $f2.f3 -auto both]
$s1 configure -relief sunken -borderwidth 2
# the tree
set tree [Tree $s1.tree -highlightthickness 0 -width 25 -relief flat -padx 4 ]
$s1 setwidget $tree
pack $s1 -fill y -expand n -side left

# pack the tree into its subframe
bind $tree <<TreeSelect>> { node_clicked }
$tree bindText <Double-1> { $f5.ok invoke ;# }

# bwidget 1.7.0 does not generate <<TreeSelect>> events for keyboard navigation.
# These bindings fix that.
bind $tree.c <KeyPress-Up> [list +event generate $tree <<TreeSelect>>]
bind $tree.c <KeyPress-Down> [list +event generate $tree <<TreeSelect>>]

# Let the text scroll
set f4 [ SW $f2.f4 -scrollbar vertical]
$s1 configure
# a text box to display the details
set tb [ text $f4.tb -width 30 -wrap word -padx 6 -pady 6 \
         -takefocus 0 -state disabled \
         -relief flat -borderwidth 0 -height 12]
$f4.vscroll configure -takefocus 1
$f4 setwidget $tb
set detail_box $tb
# pack the subframe into the main frame
pack $f2 -side top -fill both -expand y
pack $f4 -side left -padx 3 -fill both -expand y


# a subframe for the buttons
set f5 [ frame $f1.f5 ]
button $f5.ok -text [msgcat::mc "OK"] -command "button_pushed OK" -width 8 -default active
set button_ok $f5.ok
$button_ok configure -state disabled
button $f5.cancel -text [msgcat::mc  "Cancel"] -command "button_pushed Cancel" -width 8 -default normal
pack $f5.cancel -side right -padx 4 -pady 4
pack $f5.ok -side right -padx 4 -pady 4
pack $f5 -side bottom -anchor e -fill none -expand n -padx 15

pack $f1 -fill both -expand y

set config_count 0
foreach dir $configs_dir_list {
    set dir [file normalize $dir]
    if {[info exists seen($dir)]} continue
    set seen($dir) 1
    set dir_in_tree 0
    set subdir_list [ glob -nocomplain $dir/*/ ]
    set subdir_list [ lsort $subdir_list ]
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
	    # multiples, use second level and sort
  	    set inifile_list [ lsort $inifile_list ]
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
unset seen

if { $config_count == 0 } {
    puts stderr [ format [ msgcat::mc "ERROR: no configurations found in path '%s'" ] $configs_path ]
    exit 1
}

bind . <Escape> {button_pushed Cancel}
bind . <Return> ""
wm protocol . WM_DELETE_WINDOW {button_pushed Cancel}

proc wait_and_see {node {wait 10}} {
    global tree
    set yv [$tree yview]
    if {![winfo viewable $tree] || [lindex $yv 0] == [lindex $yv 1]} {
        after $wait wait_and_see $node [expr $wait*2]
    } else {
        $tree see $node
        $tree xview moveto 0.0
        node_clicked
    }
}

proc prompt_copy configname {
    set res [tk_dialog .d "Copy Configuration?" [subst [msgcat::mc {The\
configuration you selected, $configname, is in a directory you cannot\
write to.

This will cause certain features not to function properly, and you will be\
unable to modify the .ini and .hal files.

It is recommended that you make a copy of this configuration to your home\
directory.  Do you wish to copy the configuration?}]] \
    warning 0 {Yes} {No} {Cancel}]
    if {$res == -1 || $res == 2} { return "" }
    if {$res == 1} { return $configname }
    set configdir [file dirname $configname]
    set copydir [file normalize [file join ~ emc2 configs [file tail $configdir]]]
    set copybase $copydir
    set i 0
    file mkdir [file join ~ emc2 configs]
    while {1} {
        if [catch { exec mkdir $copydir }] {
            incr i
            set copydir "$copybase-$i"
            continue
        }

        eval file copy [glob -directory $configdir *] [list $copydir]
        break
    }

    tk_dialog .d "Configuration Copied" [subst [msgcat::mc {\
The configuration file has been copied to $copydir.  Next time, choose this\
location when starting emc.}]] info 0 OK

    return $copydir/[file tail $configname]
}

# add the selection set if a last_ini has been found in ~/.emcrc

if {$last_ini != -1 && [file exists $last_ini] && ![catch {$tree index $last_ini}]} {
    $tree selection set $last_ini
    wait_and_see $last_ini
}

while {1} {
    focus $tree
    vwait choice

    if { $choice == "OK" } {
        if {![file writable [file dirname $inifile]]} {
            set copied_inifile [prompt_copy $inifile]
            if {$copied_inifile == ""} { continue }
            set inifile $copied_inifile
        }
        puts $inifile

        # test for ~/.emcrc file and modify if needed.
        # or make this file and add the var.

        if {[file exists ~/.emcrc]} {
            if {$inifile == $last_ini} {
                exit
            } else {
                if {[catch {open ~/.emcrc} programin]} {
                    return 
                } else {
                    set rcstring [read $programin]
                    catch {close $programin}
                }
                set ret [setVal $rcstring PICKCONFIG LAST_CONFIG $inifile ]
                catch {file copy -force $name $name.bak}
                if {[catch {open ~/.emcrc w} fileout]} {
                    puts stdout [msgcat::mc "can't save %s" ~/.emcrc ]
                    exit
                }
                puts $fileout $ret
                catch {close $fileout}
                exit
            }
        }
        set newfilestring "# .emcrc is a startup configuration file for EMC2. \n# format is INI like. \n# \[SECTION_NAME\] \n# VARNAME = varvalue \n# where section name is the name of the system writing to this file \n\n# written by pickconfig.tcl \n\[PICKCONFIG\]\nLAST_CONFIG = $inifile\n"
                
        if {[catch {open ~/.emcrc w+} fileout]} {
            puts stdout [msgcat::mc "can't save %s" ~/.emcrc ]
            exit
        }
        puts $fileout $newfilestring
        catch {close $fileout}
    }
    break
}

exit
