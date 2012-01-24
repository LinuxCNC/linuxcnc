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
#  Copyright (c) 2005-2009 All rights reserved.
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

# Load the linuxcnc.tcl file, which defines variables for various useful paths
source [file join [file dirname [info script]] .. linuxcnc.tcl]

set logo [linuxcnc::image_search linuxcnc-wizard]
image create photo machinelogo

option add *font [linuxcnc::standard_font]
option add *Text.font [linuxcnc::standard_fixed_font]
option add *Entry*background white
option add *Listbox*background white
option add *Tree*background white

################### PROCEDURE DEFINITIONS #####################

set desktopdir [exec bash -c {test -f ${XDG_CONFIG_HOME:-~/.config}/user-dirs.dirs && . ${XDG_CONFIG_HOME:-~/.config}/user-dirs.dirs; echo ${XDG_DESKTOP_DIR:-$HOME/Desktop}}]

# use initialize_config for bwidget and .linuxcncrc
proc initialize_config {} {
    # need bwidget
    set result [catch {package require BWidget 1.7}]
    if {$result != 0} {
        puts stderr $result
        tk_messageBox -icon error -type ok -message [msgcat::mc "Can't find the bwidget 1.7 package.  There is a debian bwidget package; install \nit with sudo apt-get install bwidget."]
        exit
        destroy .
    } else {
        if {[catch {open ~/.linuxcncrc} programin]} {
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

# main button callback, it assigns the button name to '::choice'

proc button_pushed { button_name } {

    set ::choice $button_name
}

# slider process is used for several widgets
proc sSlide {f a b} {
    $f.sc set $a $b
}

# if node is an ini file named xxx.ini, then:
#    show xxx.txt      if it exists
# else
#    show README       if it exists in the directory for node
# else
#    show "No Details available"
#
# if node is a directory, then:
#    show README       if it exists in the directory for node

# called when user clicks tree node
proc node_clicked {} {

    set node [$::tree selection get]
    if {$node == ""} return
    set node [lindex $node 0]

    $::tree selection set $node
    $::tree see $node

    set readme ""
    if { [ regexp {.*\.ini$} $node ] == 1 } {
	# an ini node, acceptable
	# enable changes to the details widget
	set node [format %s $node]
	set dir [ file dirname $node]
	set name [ file rootname [file tail $node ] ]
	set readme [file join $dir $name.txt]
	if { ![ file exists $readme ] } {
	    set readme [file join [ file dirname $node ] README ]
	}
	set image [file join $dir $name.gif]
	if { ![ file exists $image ] } {
	    set image [ file join [ file dirname $node ] logo.gif ]
	}
	if { [ file readable $image ] } {
	    machinelogo blank
	    machinelogo read $image
	    puts stderr "using image $image"
	    $::detail_box image create end -image machinelogo
	    $::detail_box insert end "\n"
	    $::detail_box tag configure centered -justify center
	    $::detail_box tag add centered 0.0 0.end
	}
	# save selection
	set ::inifile $node
	# enable the OK button
	$::button_ok configure -state normal
	bind . <Return> {button_pushed OK}
	bind . <KP_Enter> {button_pushed OK}
    } else {
	if {[file isdirectory $node]} {
	    set readme [file join $node README]
	    # clear selection
	    set ::inifile ""
	    # disable the OK button
	    $::button_ok configure -state disabled
	    bind . <Return> ""
	    bind . <KP_Enter> ""
        }
   }

   # remove old text
   $::detail_box configure -state normal
   $::detail_box delete 1.0 end
   if { [ file readable $readme ] } {
       # description found, read it
       set descr [ read -nonewline [ open $readme ]]
       # reformat - remove line breaks, preserve paragraph breaks
       regsub -all {([^\n])\n([^\n])} $descr {\1 \2} descr
       # and display it
       $::detail_box insert end $descr
   } else {
       if [file isdirectory $node] {
           # leave detail_box empty
       } else {
           # no description, gotta tell the user something
           $::detail_box insert end [msgcat::mc "No details available."]
       }
   }
   # lock it again
   $::detail_box configure -state disabled
}

################ MAIN PROGRAM STARTS HERE ####################
set configs_dir_list $linuxcnc::CONFIG_DIR

# set options that are common to all widgets
foreach class { Button Entry Label Listbox Scale Text } {
    option add *$class.borderWidth 1  100
}

# make a toplevel and a master frame.
wm title . [msgcat::mc "LinuxCNC Configuration Selector"]
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

set message [msgcat::mc "Welcome to LinuxCNC.\n\nSelect a machine configuration from the list on the left.\nDetails about the selected configuration will appear in the display on the right.\nClick 'OK' to run the selected configuration"]

set lbl [ label $f1.lbl -text $message -justify left -padx 15 -pady 10 -wraplength 600 ]
pack $lbl -anchor w

# a subframe for the tree/detail box 
set f2 [ frame $f1.f2 -borderwidth 0 -relief flat -padx 15 ]

# Let the tree scroll
set s1 [ SW $f2.f3 -auto both]
$s1 configure -relief sunken -borderwidth 2
# the tree
set ::tree [Tree $s1.tree -highlightthickness 0 -width 25 -relief flat -padx 4 ]
$s1 setwidget $::tree
pack $s1 -fill y -expand n -side left

# pack the tree into its subframe
bind $::tree <<TreeSelect>> { node_clicked }
$::tree bindText <Double-1> { $f5.ok invoke ;# }

# bwidget 1.7.0 does not generate <<TreeSelect>> events for keyboard navigation.
# These bindings fix that.
bind $::tree.c <KeyPress-Up> [list +event generate $::tree <<TreeSelect>>]
bind $::tree.c <KeyPress-Down> [list +event generate $::tree <<TreeSelect>>]

# Let the text scroll
set f4 [ SW $f2.f4 -scrollbar vertical]
$s1 configure
# a text box to display the details
set tb [ text $f4.tb -width 30 -wrap word -padx 6 -pady 6 \
         -takefocus 0 -state disabled \
         -relief flat -borderwidth 0 -height 12]
$f4.vscroll configure -takefocus 1
$f4 setwidget $tb
set ::detail_box $tb
# pack the subframe into the main frame
pack $f2 -side top -fill both -expand y
pack $f4 -side left -padx 3 -fill both -expand y


# a subframe for the buttons
set f5 [ frame $f1.f5 ]
button $f5.ok -text [msgcat::mc "OK"] -command "button_pushed OK" -width 8 -default active
set ::button_ok $f5.ok
$::button_ok configure -state disabled
button $f5.cancel -text [msgcat::mc  "Cancel"] -command "button_pushed Cancel" -width 8 -default normal
pack $f5.cancel -side right -padx 4 -pady 4
pack $f5.ok -side right -padx 4 -pady 4
pack $f5 -side bottom -anchor e -fill x -expand n -padx 15

pack $f1 -fill both -expand y

set ::config_count 0

proc describe {dir} {
    if {[string compare $dir $linuxcnc::USER_CONFIG_DIR] == 0} {
	return [msgcat::mc "My Configurations"]
    }
    if {[string compare $dir [lindex $linuxcnc::CONFIG_DIR end]] == 0} {
	return [msgcat::mc "Sample Configurations"]
    }
    return $dir/
}

proc walktree {dir} {
   if ![info exists ::openmode] {set ::openmode 1}
   if ![$::tree exists $dir] {
     set ::lvl $dir
     $::tree insert end root $dir -text [describe $dir] -open $::openmode
   }
  foreach f [lsort [glob -nocomplain $dir/*]] {
     if [file isdirectory $f] {
       set foundini [exec find $f -type f -name "*.ini"]
       if {"$foundini" == ""} {
         verbose "no ini files, skipping $f"
         continue
       }
       $::tree insert end $::lvl $f -text [file tail $f] -open $::openmode
       set restore $::lvl
       set ::lvl $f
       walktree $f ;# recursion
       set ::lvl $restore
     } else {
       if { [ regexp {.*\.ini$} $f] == 1 } {
         set text [file rootname [file tail $f]]
         $::tree insert end $::lvl $f -text $text -open $::openmode
         incr ::config_count
         continue
       } else {
         verbose "skipping non-ini file: $f"
       }
     }
  }
} ;# walktree

proc verbose {msg} {
  if ![info exists ::env(verbose_pickconfig)] return
  puts stderr "pickconfig:$msg"
}

proc minimal_tree {node} {
  if {"$node" == "root"} return
  set p [$::tree parent $node]
  foreach c [$::tree nodes $p] {
    if {"$c" == "$node"} continue
    $::tree closetree $c
  }
  minimal_tree $p ;#recursion
} ;# minimal_tree

foreach dir $::configs_dir_list {
  if {[info exists visited($dir)]} continue
  set visited($dir) {}
  walktree $dir
}

if { $::config_count == 0 } {
    puts stderr [msgcat::mc "ERROR: no configurations found in path '%s'" $configs_dir_list]
    exit 1
}

bind . <Escape> {button_pushed Cancel}
bind . <Return> ""
bind . <KP_Enter> ""

wm protocol . WM_DELETE_WINDOW {button_pushed Cancel}

proc wait_and_see {node {wait 10}} {
    set yv [$::tree yview]
    if {![winfo viewable $::tree] || [lindex $yv 0] == [lindex $yv 1]} {
        after $wait [list wait_and_see $node [expr $wait*2]]
    } else {
        $::tree see $node
        $::tree xview moveto 0.0
        minimal_tree $node
        node_clicked
    }
}

proc get_file_contents {f} {
    set fd [open $f]
    set contents [read $fd]
    close $fd
    return $contents
}

proc put_file_contents {f c} {
    set fd [open $f w]
    puts -nonewline $fd $c
    close $fd
}

proc prompt_copy configname {

    set res [tk_dialog .d [msgcat::mc "Copy Configuration?"] [msgcat::mc "Would you like to copy the %s configuration to your home directory so you can customize it?" $configname] warning 0 [msgcat::mc "Yes"] [msgcat::mc "Cancel"]]

    if {$res == -1 || $res == 1} { return "" }
    set configdir [format %s [file dirname $configname]]
    foreach d $linuxcnc::CONFIG_DIR {
      if {"$d" == "$configdir"} {
        # found configdir at level 0 of a directory in the linuxcnc::CONFIG_DIR list
        set copydir [format %s [file join $linuxcnc::USER_CONFIG_DIR [file tail $configdir]]]
        break
      }
      # if configdir not found at level 0, try subdirs
      if {0 != [string first "$d" $configname]} {
        continue
      } else {
        # found configdir as a subdir so create copydir at same level
        # so that ini file items specified as relative links (like ../../)
        # will work in both run-in-place and packaged builds
        set     idx [expr 1 + [string length $d]]
        set copydir [string range $configdir $idx end]
        set copydir [format %s [file join $linuxcnc::USER_CONFIG_DIR $copydir]]
        break
      }
    }
    set copybase $copydir

    set i 0
    set ncfiles [file normalize [file join ~ emc2 nc_files]]
    file mkdir [file join ~ emc2 configs]
    if {![file exists $ncfiles]} {
        file mkdir $ncfiles
        file link -symbolic [file join $ncfiles/examples] $linuxcnc::NCFILES_DIR

        # liblist: libs used in inifiles for [RS274NGC]SUBROUTINE_PATH
        # example: ngcgui uses lib named ngcgui_lib
        set liblist {ngcgui gladevcp}
        foreach lib $liblist {
           file link -symbolic [file join $ncfiles/${lib}_lib] \
                               [file join $linuxcnc::NCFILES_DIR ${lib}_lib]
        }
        set  dir [file tail $ncfiles] 
        set date [clock format [clock seconds] -format "%d%b%Y %T"]
        set   fd [open [file join $ncfiles readme.ngc] w]
        puts $fd "(debug, readme.ngc autogenerated by:)"
        puts $fd "(debug, $::argv0)" 
        puts $fd "(debug, $date)" 
        puts $fd "(debug, LinuxCNC vmajor=#<_vmajor> vminor=#<_vminor>)"
        puts $fd "(debug, User writable directory: $dir)"
        puts $fd "(debug, Example files: $dir/examples)"
        foreach lib $liblist {
           puts $fd "(debug, $lib subroutines: $dir/${lib}_lib)"
        }
        puts $fd "m2"
        close $fd
    }
    while {1} {
        # note: file mkdir will make parents as required
        if [catch { file mkdir $copydir } msg] {
            incr i
            set copydir "$copybase-$i" ;# user may have protected directory, so bump name
            # limit attempts to avoid infinite loop for hard error
            if {$i > 1000} {
              puts stderr "$::argv0:$msg"
              tk_messageBox -icon error -type ok \
                -message [msgcat::mc "Failed to mkdir for $copydir\n<$msg>"]
              destroy .
              exit
            }
            continue
        }
        # A hierarchy of directories is allowed.
        # User selects an offered ini file.
        # All files in same directory (but not subdirectories)
        # are copied.  The target of a linked file is copied
        foreach f [glob -directory $configdir *] {
          switch [file type $f] {
            link      {file copy [file join $configdir \
                                 [file readlink $f]] $copydir}
            file      {file copy "$f" $copydir}
            directory {}
            default   {puts stderr \
                       "prompt_copy:unsupported type=[file type $f] for $f"}
          }
        }
        foreach f [glob -directory $copydir *] {
	    file attributes $f -permissions u+w
	    if {[file extension $f] == ".ini"} {
		set c [get_file_contents $f]
		regsub {(?n)^(PROGRAM_PREFIX\s*=\s*).*$} $c "\\1$ncfiles" c
		put_file_contents $f $c
	    }
            if {$::tcl_platform(platform) == "unix"} {
                file attributes $f -permissions u+w
            }
        }
        break
    }

    tk_dialog .d [msgcat::mc "Configuration Copied"] [msgcat::mc "The configuration file has been copied to %s. Next time, choose this location when starting LinuxCNC." $copydir] info 0 [msgcat::mc "OK"]

    return $copydir/[file tail $configname]
}


# add the selection set if a last_ini has been found in ~/.linuxcncrc

if {$last_ini != -1 && [file exists $last_ini] && ![catch {$::tree index $last_ini}]} {
    $::tree selection set $last_ini
    wait_and_see $last_ini
}

proc make_shortcut {inifile} {
    if {[catch {open $inifile} inifd]} { return }
    set inistring [read $inifd]
    close $inifd
    set name [getVal $inistring EMC MACHINE]
    set filename0 [file join $::desktopdir [file rootname [file tail $inifile]]]
    set filename ${filename0}.desktop
    set i 0
    while {[file exists $filename]} {
	incr i
	set filename $filename0${i}.desktop
    }
    exec linuxcncmkdesktop $inifile $name > $filename
    file attributes $filename -permissions +x
    tk_dialog .d [msgcat::mc "Shortcut Created"] [msgcat::mc "A shortcut to this configuration file has been created on your desktop.  You can use it to automatically launch this configuration."] info 0 [msgcat::mc "OK"]
}

set make_shortcut 0
if {[file isdir $::desktopdir]} {
    checkbutton $f5.c -variable make_shortcut -text [msgcat::mc "Create Desktop Shortcut"]
    pack $f5.c -side left -expand 1 -anchor w
}

while {1} {
    focus $::tree
    vwait ::choice

    if { $::choice == "OK" } {
        if {![file writable [file dirname $::inifile]]} {
            set copied_inifile [prompt_copy $::inifile]
            if {$copied_inifile == ""} { continue }
            set ::inifile $copied_inifile
        }
	if {$make_shortcut} { make_shortcut $::inifile }
        puts $::inifile

        # test for ~/.linuxcncrc file and modify if needed.
        # or make this file and add the var.

        if {[file exists ~/.linuxcncrc]} {
            if {$::inifile == $last_ini} {
                exit
            } else {
                if {[catch {open ~/.linuxcncrc} programin]} {
                    return 
                } else {
                    set rcstring [read $programin]
                    catch {close $programin}
                }
                set ret [setVal $rcstring PICKCONFIG LAST_CONFIG $::inifile ]
                catch {file copy -force $name $name.bak}
                if {[catch {open ~/.linuxcncrc w} fileout]} {
                    puts stdout [msgcat::mc "can't save %s" ~/.linuxcncrc ]
                    exit
                }
                puts $fileout $ret
                catch {close $fileout}
                exit
            }
        }
        set newfilestring "# .linuxcncrc is a startup configuration file for LinuxCNC. \n# format is INI like. \n# \[SECTION_NAME\] \n# VARNAME = varvalue \n# where section name is the name of the system writing to this file \n\n# written by pickconfig.tcl \n\[PICKCONFIG\]\nLAST_CONFIG = $::inifile\n"
                
        if {[catch {open ~/.linuxcncrc w+} fileout]} {
            puts stderr [msgcat::mc "can't save %s" ~/.linuxcncrc ]
            exit
        }

        puts $fileout $newfilestring
        catch {close $fileout}
    }
    break
}

exit
