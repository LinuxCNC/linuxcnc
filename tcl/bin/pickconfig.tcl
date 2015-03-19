#!/bin/sh
# this line restarts using wish \
exec wish "$0" "$@"

###############################################################
# Description:  pickconfig.tcl
#               This file validates a LinuxCNC configuration
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

set logo [linuxcnc::image_search machinekit-wizard]
image create photo machinelogo

option add *font [linuxcnc::standard_font]
option add *Text.font [linuxcnc::standard_fixed_font]
option add *Entry*background white
option add *Listbox*background white
option add *Tree*background white


################### MAINTENANCE ITEMS #####################

# flat structure:
#    Ini files should always refer to ../../nc_files so
#    that hey will work in run-in-place and deb-installed
#    systems.
#    Config subdirs can be moved with little impact when
#    ../../nc_files relative link is used.
#
# hierarchical structure:
#    Ini files must refer to appropriate nc_files by
#    relative links so that they will work in run-in-place
#    and deb-installed systems.
#    Ini files must be edited if config subdirs are moved to
#    a different depth in the tree to update the relative
#    links
#
set ::make_flat_user_dirs  1 ;# 0 ==> hierarchical

# start on this node if no ~/.machinekitrc:
set ::default_start_node sim/axis/axis.ini

# exclude directories that should never be offered
set ::exclude_list [list common]

# support filenames that are never copied to user:
set ::never_copy_list [list maintainer.txt nodemocopy]

# emphasize sim ini configs that have the most support by reordering
# reorder: priority low to high:
set ::preferred_names [list \
                       low_graphics \
                       gmoccapy \
                       gscreen \
                       touchy \
                       ngcgui \
                       axis \
                       by_machine \
                       by_interface \
                       sim \
                       ]

# support creation of links for newly added _lib dirs in nc_files
set ::always_update_nc_files 1

################### PROCEDURE DEFINITIONS #####################

set desktopdir [exec bash -c {test -f ${XDG_CONFIG_HOME:-~/.config}/user-dirs.dirs && . ${XDG_CONFIG_HOME:-~/.config}/user-dirs.dirs; echo ${XDG_DESKTOP_DIR:-$HOME/Desktop}}]

# use initialize_config for bwidget and .machinekitrc
proc initialize_config {} {
    # need bwidget
    set result [catch {package require BWidget 1.7}]
    if {$result != 0} {
        puts stderr $result
        tk_messageBox -icon error -type ok -message [msgcat::mc "Can't find the bwidget 1.7 package.  There is a debian bwidget package; install \nit with sudo apt-get install bwidget."]
        exit
        destroy .
    } else {
        if {[catch {open ~/.machinekitrc} programin]} {
            return 
        } else {
            set rcstring [read $programin]
            catch {close $programin}
            set ret [getVal $rcstring PICKCONFIG LAST_CONFIG ]
            set ::openmode 1
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

proc title {node} {
  if [file isfile $node] {
    set txt "CURRENT: [file tail $::selected_node]"
  } else {
    set txt ""
  }
  wm title . "[msgcat::mc "Machinekit Configuration Selector"] $txt"
}

proc find_usable_nodes {startdir} {
  return [exec find $startdir -type f \
         -name "*.ini" -o -name "*.demo" ]
} ;# find_usable_nodes

proc name_is_usable_nodename {node} {
    if {   [ regexp {.*\.ini$}  $node ] == 1 \
        || [ regexp {.*\.demo$} $node ] == 1 \
       } {
       return 1 ;# ok
    }
    return 0 ;# fail
} ;# name_is_usable_nodename {node}

proc ok_to_copy_config {filename} {
    # The following test determines when to copy files to a user directory:
    # If the directory for the selected file is not writable,
    # then it is presumed to be a system dir running from an install (by
    # a deb install typically) so copy the configuration to user directory.
    #
    # Otherwise, it is presumed to be a Run-In-Place directory and
    # copying to another directory is not wanted.
    #
    # For convenience in testing pickconfig by itself in rip builds:
    #    export debug_pickconfig=1
    # This forces copying to the user directory so that the copied
    # configs can be tested.

    if {    [info exists ::env(debug_pickconfig)] \
         && [string first $::myconfigs_node $filename]} {
      set ::writeanyway 1
    } else {
      set ::writeanyway 0
    }
    if {   ![file writable [file dirname $filename]]
        || $::writeanyway
       } {
        set filetype [file extension $filename]
        if {$filetype == ".ini"} { return 1 ;# ok }
        if {$filetype == ".demo"} {
           set nocopyfile [file join [file dirname $filename] nodemocopy]
           if {![file exists $nocopyfile]} { return 1 ;# ok }
        }
     }
     return 0 ;# not ok
} ;# ok_to_copy

# Notes on text displayed in detail_box widget:
# if node is usable (an ini file named xxx.ini or xxx.demo), then:
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
    set ::selected_node $node
    title $node
    if {$node == ""} return
    set node [lindex $node 0]

    $::tree selection set $node
    $::tree see $node

    set readme ""
    if [name_is_usable_nodename $node] {
	# acceptable name
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

        if [info exists ::make_shortcut_widget] {
          if [ok_to_copy_config $::inifile] {
            $::make_shortcut_widget config -state normal
          } else {
            $::make_shortcut_widget config -state disabled
          }
        }

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
       # expect file with unbroken paragraphs
       $::detail_box insert end $descr
   } else {
       if [file isdirectory $node] {
          if {"$node" == "$::myconfigs_node"} {
             $::detail_box insert end "Your existing configs"
          } elseif { "$node" == "$::sampleconfigs_node"} {
             $::detail_box insert end "Available configs"
          }
          #  else leave detail_box empty
       } else {
           # no description, gotta tell the user something
           $::detail_box insert end [msgcat::mc "No details available."]
       }
   }
   # lock it again
   $::detail_box configure -state disabled
}

################ MAIN PROGRAM STARTS HERE ####################
set ::configs_dir_list $linuxcnc::CONFIG_DIR
set ::myconfigs_node   $linuxcnc::USER_CONFIG_DIR
# order convention for items in the linuxcnc::USER_CONFIG_DIR list:
set ::sampleconfigs [lindex $::configs_dir_list end] ;# last item

set ::last_ini "none"
set ::last_ini [initialize_config]

set ::openmode 0
if {   [file exists [file join $::sampleconfigs $::default_start_node]]
    || "$::last_ini" != ""
   } {
  set ::openmode 1
}

# set options that are common to all widgets
foreach class { Button Entry Label Listbox Scale Text } {
    option add *$class.borderWidth 1  100
}

# make a toplevel and a master frame.
title ""
set logo [label .logo -image $logo]
set top [frame .main -borderwidth 0 -relief flat ]
pack $logo -side left -anchor nw
pack $top -side left -expand yes -fill both

wm geo . 780x480
wm minsize . 780 480

proc SW { args } {
    set res [eval ScrolledWindow $args]
    $res.vscroll configure -relief flat
    ::${res}:cmd configure -highlightthickness 1
    return $res
}
# a frame for packing the top window
set f1 [ frame $top.f1 ]

set message [msgcat::mc "Welcome to MachineKit.\n\nSelect a machine configuration from the list on the left.\nDetails about the selected configuration will appear in the display on the right.\nClick 'OK' to run the selected configuration"]

set lbl [ label $f1.lbl -text $message -justify left -padx 15 -pady 10 -wraplength 600 ]
pack $lbl -anchor w

# a subframe for the tree/detail box 
set f2 [ frame $f1.f2 -borderwidth 0 -relief flat -padx 15 ]

# Let the tree scroll
set s1 [ SW $f2.f3 -auto both]
$s1 configure -relief sunken -borderwidth 2
# the tree
set ::tree [Tree $s1.tree -highlightthickness 0 \
                          -width 25 -relief flat -padx 4 \
                          ]
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
    if {"$dir" == "$::myconfigs_node"} {
	    return [msgcat::mc "My Configurations"]
    }
    if {"$dir" == "$::sampleconfigs"} {
        set ::sampleconfigs_node $dir
	return [msgcat::mc "Sample Configurations"]
    }
    return $dir/
}

proc treeopen {args} {
  set beforevisible [$::tree visible $::selected_node]
  update
  set visible [$::tree visible $::selected_node]
  if {!$beforevisible && $visible && [info exists ::restorebox] } {
    set state [$::detail_box cget -state]
    $::detail_box configure -state normal
    $::detail_box insert end $::restorebox
    $::detail_box configure -state $state
    unset ::restorebox
  }
} ;# treeopen

proc treeclose {args} {
  update
  set visible [$::tree visible $::selected_node]
  if {!$visible && ![info exists ::restorebox] } {
    set ::restorebox [$::detail_box get 1.0 end]
    set state [$::detail_box cget -state]
    $::detail_box configure -state normal
    $::detail_box delete 1.0 end
    $::detail_box configure -state $state
  }
} ;# treeclose

proc walktree {dir} {
   if ![info exists ::openmode] {set ::openmode 0}
   if ![$::tree exists $dir] {
     set ::lvl $dir
     $::tree insert end root $dir -text [describe $dir] -open $::openmode
   }
  set bothlist [lsort [glob -nocomplain $dir/*]]
  if {   [info exists ::sampleconfigs_node]
      && "$dir" == "$::sampleconfigs_node"} {
    set bothlist [rearrange $bothlist]
  }
  set sortedlist {}
  set filelist {}
  set dirlist {}
  # display files before directories
  foreach item $bothlist {
     if [file isdirectory $item] {
       lappend dirlist $item
     } else {
       lappend sortedlist $item
     }
  }
  foreach f $dirlist { lappend sortedlist $f }

  foreach f $sortedlist {
     if [file isdirectory $f] {
       if {[lsearch $::exclude_list [file tail $f]] == 0} continue
       set foundini [find_usable_nodes $f]
       if {"$foundini" == ""} {
         verbose "no usable files, skipping directory: $f"
         continue
       }

       set text [file tail $f]
       $::tree insert end $::lvl $f -text $text -open $::openmode
       set restore $::lvl
       set ::lvl $f
       walktree $f ;# recursion
       set ::lvl $restore
     } else {
       if [name_is_usable_nodename $f] {
         set text [file rootname [file tail $f]]
         $::tree insert end $::lvl $f -text $text -open $::openmode
         incr ::config_count
         continue
       } else {
         verbose "skipping non-start_node file: $f"
       }
     }
  }
} ;# walktree

proc rearrange l {
  set taillist {}
  foreach item $l {
    lappend taillist [file tail $item]
  }
  foreach name $::preferred_names {
    set idx [lsearch $taillist $name]
    if {$idx < 0} continue
    set found [lindex $l $idx]
    set taillist [lreplace $taillist $idx $idx]
    set taillist [linsert $taillist 0 $name]
    set l [lreplace $l $idx $idx]
    set l [linsert $l 0 $found]
  }
  return $l
} ;# rearrange

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
  if {![file isdirectory $dir]} {
    verbose "pickconfig: skipping <$dir>, not a directory"
    continue
  } 
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
    set chosendir [format %s [file dirname $configname]]
    foreach d $::configs_dir_list {
      if {"$d" == "$chosendir"} {
        # found chosendir at level 0 of a directory in the ::configs_dir_list
        set copytodir [format %s [file join $::myconfigs_node [file tail $chosendir]]]
        break
      }
      # if chosendir not found at level 0, try subdirs
      if {0 != [string first "$d" $configname]} {
        continue
      } else {
        # found chosendir as a subdir
        set idx [expr 1 + [string length $d]] ;#
        set hiername [string range $chosendir $idx end]
        if $::make_flat_user_dirs {
          # Flat dir structure for copied configs
          # create copytodir at one level below ::myconfigs_node
          set flatname  [string map {/ .} $hiername]
          set copytodir [format %s [file join $::myconfigs_node $flatname]]
        } else {
          # Hierarchical dir structure for copied configs
          # create copytodir following hierarchy
          set copytodir [format %s [file join $::myconfigs_node $hiername]]
        }
        break
      }
    }
    set copybase $copytodir

    set i 0
    # distribution config ini files expect nc_files at same level as configs dir
    set ncfiles [file normalize [file join $::myconfigs_node ../nc_files]]
    file mkdir [file join $::myconfigs_node]

    set obsoletedir [file normalize [file join ~ emc2]]
    if [file isdir $obsoletedir] {
      tk_messageBox -title "Copy Configuration Notice" \
        -message "A directory named:\n \
                  $obsoletedir\n \
                  exists \n\n \
                  You may want to copy items to the new directory:\n \
                  [file normalize [file join $::myconfigs_node ..]]" \
        -type ok
    }

    if {$::always_update_nc_files || ![file exists $ncfiles]} {
        file mkdir $ncfiles ;# tcl: ok if it exists
        set refname  $linuxcnc::NCFILES_DIR
        set linkname [file join $ncfiles examples]
        if {[file exists $linkname]} {
          # tcl wont overwrite any existing link, so remove
          file delete $linkname
        }
        file link -symbolic $linkname $refname

        # liblist: libs used in inifiles for [RS274NGC]SUBROUTINE_PATH
        # example: ngcgui uses lib named ngcgui_lib

        set _libs [glob [file join $linuxcnc::NCFILES_DIR *_lib]]
        foreach lib $_libs {
           if ![file isdir $lib] continue
           lappend liblist $lib
        }
        foreach lib $liblist {
           set refname  [file join $linuxcnc::NCFILES_DIR $lib]
           set linkname [file join $ncfiles [file tail $lib]]
           # note for link, file exists test target of link
           if {[file exists $linkname]} {
             # tcl wont overwrite any existing link, so remove
             file delete $linkname
           }
           # avoid error if no target 
           if [file exists $refname] {
             file link -symbolic $linkname $refname
           }
        }
        set  dir [file tail $ncfiles] 
        set date [clock format [clock seconds] -format "%d%b%Y %T"]
        set   fd [open [file join $ncfiles readme.ngc] w]
        puts $fd "(info: readme.ngc)"
        set msg "(debug, readme.ngc autogenerated by:)\n"
        set msg "${msg}(debug, $::argv0)\n"
        set msg "${msg}(debug, $date)\n"
        set msg "${msg}(debug, Machinekit vmajor=#<_vmajor> vminor=#<_vminor>)\n"
        set msg "${msg}(debug, User writable directory: $dir)\n"
        set msg "${msg}(debug, Example files: $dir/examples)\n"
        foreach lib $liblist {
           set msg  "${msg}(debug, $lib subroutines: $dir/${lib}_lib)\n"
        }
        puts $fd $msg
        # make readme.ngc compatible with ngcgui:
        # (repeat the debug prints within the subroutine)
        puts $fd "o<readme> sub"
        puts $fd $msg
        puts $fd "   #<parm1> = #1 (=123 pvalue)"
        puts $fd "   (debug, readme.ngc: pvalue = #<parm1>)"
        puts $fd "o<readme> endsub"
        # include m2 to preculde message:
        #        "File ended with no percent sign or program end"
        puts $fd "m2"
        close $fd
    }
    while {1} {
        if [file exists $copytodir] {
          incr i
          set copytodir "$copybase-$i" ;# user may have protected directory, so bump name
          # limit attempts to avoid infinite loop for hard error
          if {$i > 1000} {
             puts stderr "$::argv0:$msg"
             tk_messageBox -icon error -type ok \
                  -message [msgcat::mc "Failed to mkdir for $copytodir\n<$msg>"]
             destroy .
             exit
          }
          continue ;# try again
        } else {
          # note: file mkdir will make parents as required
          if [catch { file mkdir $copytodir } msg] {
            continue ;# try again
          }
        }
        # A hierarchy of directories is allowed.
        # User selects an offered ini file.
        # All files in same directory are copied.
        # The target of a linked file is copied
        foreach f [glob -directory $chosendir *] {
          # nc_files (as a symlink) may be present for rip testing
          # nc_files (as a symlink) not copied here (handled elsewhere)
          if {   [file tail $f] == "nc_files"
              && [file type $f] == "link" } {
            verbose "pickconfig: not copying link: $f"
            continue 
          }
          if {[lsearch $::never_copy_list [file tail $f]] >= 0} continue
          # is_special: subdir is to be copied
          set is_special 0
          if { "" == [find_usable_nodes $f] } {
             # ok: no usable child files so the directory can be copied
             set is_special 1
          }

          switch [file type $f] {
            link      {
                        if {$is_special} {
                          # since is link, require:
                          # -r recursive, -L dereference
                          exec cp -rL [file join $chosendir $f] $copytodir
                        } else {
                          # to follow sym links correctly, use system cp:
                          exec cp [file join $chosendir $f] $copytodir
                        }
                      }
            file      {file copy "$f" $copytodir}
            directory { # recursive copy of subdirs
                        if {$is_special } {
                          # -r recursive
                          exec cp -r [file join $chosendir $f] $copytodir
                        }
                      }
            default   {puts stderr \
                       "prompt_copy:unsupported type=[file type $f] for $f"}
          }
        }
        foreach f [glob -directory $copytodir *] {
	    file attributes $f -permissions u+w
	    if {[file extension $f] == ".ini"} {
		set c [get_file_contents $f]
		# note: the following regsub _typically_ forces:
		# PROGRAM_PREFIX=/home/username/machinekit/nc_files
		regsub {(?n)^(PROGRAM_PREFIX\s*=\s*).*$} $c "\\1$ncfiles" c
		put_file_contents $f $c
	    }
            if {$::tcl_platform(platform) == "unix"} {
                file attributes $f -permissions u+w
            }
        }
        break
    }

    tk_dialog .d [msgcat::mc "Configuration Copied"] [msgcat::mc "The configuration file has been copied to %s. Next time, choose this location when starting MachineKit." $copytodir] info 0 [msgcat::mc "OK"]

    return $copytodir/[file tail $configname]
}


# add the selection set if a ::last_ini has been found in ~/.machinekitrc

if {   $::last_ini != -1 
    && [file exists $::last_ini]
    && ![catch {$::tree index $::last_ini}]} {
    set start_node $::last_ini
} else {
    set start_node ${::sampleconfigs}/$::default_start_node
}
if [catch {
            $::tree selection set $start_node
            wait_and_see $start_node
           }] {
  set ::openmode 0
  puts stderr "pickconfig: cannot find expected start_node <$start_node>, continuing"
}
# update and enable commands after initial setup of tree
update
set ::selected_node [$::tree selection get]
title $::selected_node
$::tree configure \
        -closecmd treeclose \
        -opencmd  treeopen


proc make_shortcut {inifile} {
    if {[catch {open $inifile} inifd]} { return }
    set inistring [read $inifd]
    close $inifd
    switch [file extension $inifile] {
      ".ini"  {set name [getVal $inistring EMC MACHINE]}
      ".demo" {set name [lindex [split [file tail $inifile] "."] 0]
              }
      default {set name linuxcnc-other}
    }
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
    checkbutton $f5.c -variable make_shortcut \
                      -text [msgcat::mc "Create Desktop Shortcut"] \
                      -state disabled
    pack $f5.c -side left -expand 1 -anchor w
    set ::make_shortcut_widget $f5.c
}

while {1} {
    focus $::tree
    vwait ::choice

    if { $::choice == "OK" } {
        if [ok_to_copy_config $::inifile] {
            set copied_inifile [prompt_copy $::inifile]
            if {$copied_inifile == ""} {
                continue
            } else {
               set ::inifile $copied_inifile
            }
	    if {$make_shortcut} { make_shortcut $::inifile }
        }
        puts $::inifile ;# this is the result for this script (to stdout)

        # test for ~/.machinekitrc file and modify if needed.
        # or make this file and add the var.

        if {[file exists ~/.machinekitrc]} {
            if {$::inifile == $::last_ini} {
                exit
            } else {
                if {[catch {open ~/.machinekitrc} programin]} {
                    return 
                } else {
                    set rcstring [read $programin]
                    catch {close $programin}
                }
                set ret [setVal $rcstring PICKCONFIG LAST_CONFIG $::inifile ]
                catch {file copy -force $name $name.bak}
                if {[catch {open ~/.machinekitrc w} fileout]} {
                    puts stdout [msgcat::mc "can't save %s" ~/.machinekitrc ]
                    exit
                }
                puts $fileout $ret
                catch {close $fileout}
                exit
            }
        }
        set newfilestring "# .machinekitrc is a startup configuration file for MachineKit. \n# format is INI like. \n# \[SECTION_NAME\] \n# VARNAME = varvalue \n# where section name is the name of the system writing to this file \n\n# written by pickconfig.tcl \n\[PICKCONFIG\]\nLAST_CONFIG = $::inifile\n"
                
        if {[catch {open ~/.machinekitrc w+} fileout]} {
            puts stderr [msgcat::mc "can't save %s" ~/.machinekitrc ]
            exit
        }

        puts $fileout $newfilestring
        catch {close $fileout}
    }
    break
}

exit
