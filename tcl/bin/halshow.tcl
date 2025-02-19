#!/bin/sh
# the next line restarts using emcsh \
exec $LINUXCNC_EMCSH "$0" "$@"

###############################################################
# Description:  halshow.tcl
#               This file, shows a running hal configuration
#               and has menu for modifying and tuning
#
#  Author: Raymond E Henry
#  License: GPL Version 2
#
#  Copyright (c) 2006-2009 All rights reserved.
###############################################################
# FIXME -- empty mod entry widgets after execute
# FIXME -- please hal param naming conventions aren't
###############################################################

# Load the linuxcnc.tcl file, which defines variables for various useful paths
source [file join [file dirname [info script]] .. linuxcnc.tcl]
# Load file for canvasbuttons
source [file join [file dirname [info script]] cbutton.tcl]

package require BWidget

# add a few default characteristics to the display
foreach class { Button Checkbutton Entry Label Listbox Menu Menubutton \
    Message Radiobutton Scale } {
    option add *$class.borderWidth 1  100
}

# get config file path from running linuxcnc process if not invoked by GUI
set config_path ""
catch {set linuxcnc_process [exec ps -e -o stat,command | grep "^S" | grep -o "linuxcnc \\/.*\\.ini" ]
    regexp { \/.*\/} $linuxcnc_process config_path
    set config_path [string trim $config_path]
}
if {[info exists ::env(CONFIG_DIR)]} {
    set ::INIDIR "$::env(CONFIG_DIR)"
    set ::INIFILE "$::env(CONFIG_DIR)/halshow.preferences"
} elseif {[file isdirectory $config_path]} {
    set ::INIDIR "${config_path}"
    set ::INIFILE "${config_path}halshow.preferences"
} else {
    set ::INIDIR "~"
    set ::INIFILE "~/.halshow_preferences"
}
# puts stderr "Halshow inifile: $::INIFILE"

proc readIni {} {
    # check that the file is readable
    if { ![file readable $::INIFILE]} {
        # puts stderr "\[halshow\] Settings file not found, using defaults"
        return -1
    } elseif { [catch {source $::INIFILE}] } { 
        puts stderr "\[halshow\] Error in settings file $::INIFILE, using defaults.\n" 
        return -1
    } else {
        return 0
    }
}

set ::initPhase true
set ::autoSaveWatchlist 1
set ::use_prefs true
proc saveIni {} {
    # The flag 'initPhase' prevents saving on the first FocusIn event
    if {!$::initPhase && $::use_prefs} {
        # open the file for writing (truncates if file exists)
        if { [catch {set fc [open $::INIFILE w]}] } {
            puts stder "\[halshow\] Unable to save settings to \"$INIFILE\"."
        } else {
            # write file
            puts $fc "# Halshow settings"
            puts $fc "# This file is generated automatically."
            puts $fc "wm geometry . [wm geometry .]"
            puts $fc "placeFrames $::ratio"
            puts $fc "set ::ratio $::ratio"
            puts $fc "set ::old_w_leftf $::old_w_leftf"
            if {$::autoSaveWatchlist} {
                puts $fc "set ::watchlist {"
                foreach elem $::watchlist {
                    puts $fc "    $elem"
                }
                puts $fc "}"
            }
            puts $fc "set ::workmode $::workmode"
            puts $fc "set ::watchInterval $::watchInterval"
            puts $fc "set ::col1_width $::col1_width"
            puts $fc "set ::ffmts $::ffmts"
            puts $fc "set ::ifmts $::ifmts"
            puts $fc "set ::alwaysOnTop $::alwaysOnTop"
            puts $fc "set ::autoSaveWatchlist $::autoSaveWatchlist"
            close $fc
        }
    }
}

# This overwrites the default error message dialog to be able to set it on top
proc bgerror {message} {
    tk_messageBox -title "Application Error" -message [msgcat::mc "Error"] \
    -detail $message -icon error -type ok 
    wm attributes . -topmost $::alwaysOnTop
}

#----------start toplevel----------
#
set ::titlename [msgcat::mc "Halshow"]
wm title . $::titlename
wm protocol . WM_DELETE_WINDOW tk_
image create photo applicationIcon -data {
    iVBORw0KGgoAAAANSUhEUgAAACgAAAAoCAYAAACM/rhtAAAACXBIWXMAAA7EAAAOxAGVKw4bAAAB
    DklEQVRYhe2X0Q2DIBCGoekUDCATOoUTngOwBn1oLjWNHD8HKG34HgXh4zTAb4komoF53C2Q41n6
    gvf+9DkRVcucAQuyWEz8ENa+21uLQp84J3dsS1VYS1YQkWN6SEIVROQ0fRFEQe+9asIY21Vx+G1m
    CtYyBWsRBYnIWFs+qLXtThSogiWSmgVJZAW5EsjE3KfleQxVEJHsIWdMwW3mIznodYvpJZJi+G1G
    rOC+71d5JBEFl2W5yiPJ/2WSzTmx/dZMkpI7tt2WSSQ5pockVEFETtMXIZtJNBNuzs1MMgxTsJbf
    FiQis4ZQPOgawrWZpERSsyAJOJMgE3Of2zKJJNlDzhhFJhn2usXMTPLFCzyRcikArbPDAAAAAElF
    TkSuQmCC
}

image create photo preferencesIcon -data {
    iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAYAAAAf8/9hAAAABmJLR0QA/wD/AP+gvaeTAAAACXBI
    WXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH4QcJEgQMrJwPLAAAAB10RVh0Q29tbWVudABDcmVhdGVk
    IHdpdGggVGhlIEdJTVDvZCVuAAAB2UlEQVQ4y8WTTWsTYRSFn2lrgzSdNCOhJkM3gcbdhGRCSIpY
    dSU1WYkIFQQhZP5BEVx1IYgrETct/QOlLQhDslEMWEGRvPmYooLbJJSJNCVZtIgfcWFnSNt01YV3
    dXnP5XDvOeeFc5Z0FvD02ZO+0z9aenzm3JjT3L13J6PrcRNAiEoWIJ8zWF1bOYadJHMJdD1u5nMG
    lmUBmKGgSstuEAqqoGOmkmk0TQPoD5KMDbJZlsWlaT+3swvuWywRJUbUxU/WqNP4Fb/40/+9GLky
    S6vRYvvddq9ttz071k5vyjflmfBOUCqVEKKS/fL567djGziChYIqALVarWfb318JUd1UFOWqJI0+
    XMjcCjjn6Hrc1WLEYcrnDGKJf6v6fD652WxubqxvmZ1O570sewPOOfmcMdyF1bUVQkGVWCLKwcHh
    4dxcegkgHA4b3W63B8jVcp3CbvG0Bm9ev132K34xOeldvHHtJj9//bjQbrcDkcjsA0ni8vz1eXnc
    M05FVBCikn3x/OX9oTamkmladgN1RkWdUS8eQbIzk0qmAcyN9S3XxpHBdTRNY8/ep2AWqZbrAFTL
    dQpmkT1738nBcA2OVO27SdQxM9MZCrtFJ5nmx08fTiXx3H/h/9dfs1mvKwIMuy0AAAAASUVORK5C
    YII=
}
wm iconphoto . -default applicationIcon
set masterwidth 700
set masterheight 475
# set fixed size for configuration display and center
set xmax [winfo screenwidth .]
set ymax [winfo screenheight .]
set x [expr ($xmax - $masterwidth )  / 2 ]
set y [expr ($ymax - $masterheight )  / 2]
wm geometry . "${masterwidth}x${masterheight}+$x+$y" 
wm minsize . 230 240

# save settings after switching to another window
bind . <FocusOut> {saveIni}

# trap mouse click on window manager delete and ask to save
wm protocol . WM_DELETE_WINDOW askKill
proc askKill {} {
    saveIni
    killHalConfig
}

# clean up a possible problems during shutdown
proc killHalConfig {} {
    if {[info exists fid] && $::fid != ""} {
        catch flush $::fid
        catch close $::fid
    }
    destroy .
    exit
}

set ::main [frame .main -padx 6 -pady 3]
pack $::main -fill both -expand yes

# build frames from left side
set ::leftf [frame $::main.left]
set ::tf [frame $::leftf.tf]
set ::rightf [frame $::main.right]
set ::nb [NoteBook $::rightf.note]
pack $::nb -side right -fill both -expand yes

# Each mode has a unique set of widgets inside tab
set showhal [$::nb insert 0 ps -text [msgcat::mc " SHOW "] -raisecmd {showMode showhal} ]
set ::watchhal [$::nb insert 1 pw -text [msgcat::mc " WATCH "] -raisecmd {showMode watchhal}]
set ::settings [$::nb insert 2 set -text [msgcat::mc " SETTINGS "] -raisecmd {showMode settings}]

# use place manager to fix locations of frames within top
proc placeFrames {ratio} {
    place configure $::leftf -in $::main -x 0 -y 0 -relheight 1 -relwidth $ratio
    place configure $::rightf -in $::main -relx $ratio -y 0 -relheight 1 -relwidth [expr 1-$ratio]
    set ::ratio $ratio
}

placeFrames 0.3

set ::geometryOld [wm geometry .]
proc checkSizeChanged {w} {
    if {$w == "." || $w == ".f2.show.grip"} {
        if {[wm geometry .] == $::geometryOld} {
            return
        }
    } elseif {$w == "force"} {
    } else {
        return
    }
    if {$::watchlist_len > 20} reloadWatch
    set ::geometryOld [wm geometry .]
}

set ::ratioOld $::ratio
proc checkRatioChanged {} {
    if {$::ratio != $::ratioOld} {
        if {$::watchlist_len > 20} reloadWatch
    }
    set ::ratioOld $::ratio
}

# slider process is used for several widgets
proc sSlide {f a b} {
    $f.sc set $a $b
}

# Build menu
# fixme clean up the underlines so they are unique under each set
set menubar [menu $::rightf.menubar -tearoff 0]
set filemenu [menu $menubar.file -tearoff 1]
    $menubar add cascade -label [msgcat::mc "File"] \
            -menu $filemenu
        $filemenu add command -label [msgcat::mc "Load Watch List"] \
            -command {getwatchlist}            
        set ::savelabel1 [msgcat::mc "Save Watch List"] ;# identifier for entryconfigure
        set ::savelabel2 [msgcat::mc "Save Watch List (multiline)"] ;# identifier for entryconfigure
        $filemenu add command -label [msgcat::mc $::savelabel1] \
            -command savewatchlist
        $filemenu add command -label [msgcat::mc $::savelabel2] \
            -command [list savewatchlist multiline]
        $filemenu add command -label [msgcat::mc "Exit"] \
            -command {destroy .; exit}
        $filemenu configure -postcommand {
          if {$::watchlist != ""} {
            $filemenu entryconfigure [msgcat::mc $::savelabel1] -state normal
            $filemenu entryconfigure [msgcat::mc $::savelabel2] -state normal
          } else {
            $filemenu entryconfigure [msgcat::mc $::savelabel1] -state disabled
            $filemenu entryconfigure [msgcat::mc $::savelabel2] -state disabled
          }
        }
set viewmenu [menu $menubar.view -tearoff 0]
    $menubar add cascade -label [msgcat::mc "Tree View"] \
            -menu $viewmenu
        $viewmenu add command -label [msgcat::mc "Expand All"] \
            -command {showNode {open}}
        $viewmenu add command -label [msgcat::mc "Collapse All"] \
            -command {showNode {close}}
        $viewmenu add separator
        $viewmenu add command -label [msgcat::mc "Expand Pins"] \
            -command {showNode {pin}}
        $viewmenu add command -label [msgcat::mc "Expand Parameters"] \
            -command {showNode {param}}
        $viewmenu add command -label [msgcat::mc "Expand Signals"] \
            -command {showNode {sig}}
        $viewmenu add separator
        $viewmenu add command -label [msgcat::mc "Reload tree view"] \
            -command {refreshHAL}

set watchmenu [menu $menubar.watch -tearoff 1]
    $menubar add cascade -label [msgcat::mc "Watch"] \
            -menu $watchmenu
        $watchmenu add command -label [msgcat::mc "Add pin"] \
            -command {addToWatch pin [msgcat::mc "Pin"]}
        $watchmenu add command -label [msgcat::mc "Add signal"] \
            -command {addToWatch sig [msgcat::mc "Signal"]}
        $watchmenu add command -label [msgcat::mc "Add parameter"] \
            -command {addToWatch param [msgcat::mc "Parameter"]}
        $watchmenu add separator
        $watchmenu add command -label [msgcat::mc "Reload Watch"] \
            -command {reloadWatch}
        $watchmenu add command -label [msgcat::mc "Erase Watch"] \
            -command {
                watchReset all
                setStatusbar [msgcat::mc "Watchlist cleared"]
            }

. configure -menu $menubar

proc addToWatch {type name} {
    set var [entrybox "" [msgcat::mc "Add to watch"] $name]
    if {$var != "cancel"} {
        if {[watchHAL $type+$var] == ""} {
            setStatusbar "'$var' [msgcat::mc "added"]"
        }
    }   
}

# frame for scaling ratio
set gripf [frame $::leftf.grip -borderwidth 3 -width 8 -cursor sb_h_double_arrow]
pack $gripf -side right -fill y
pack $::tf -fill both -expand yes

# grip symbol for changing the ratio of left and right frame
set grip [frame $gripf.grip -relief groove -borderwidth 2 -width 2 -height 20]
pack [frame $gripf.topfill] -side top -expand y ; # add frames to center grip
pack $grip
pack [frame $gripf.bottomfill] -side bottom -expand y
set ::grip_clicked false
bind $gripf <Motion> [list scaleFrames]
bind $gripf <ButtonPress-1> {set ::grip_clicked true}
bind $gripf <ButtonRelease-1> {
    set ::grip_clicked false
    checkRatioChanged
}
bind $grip <Motion> [list scaleFrames]
bind $grip <ButtonPress-1> {set ::grip_clicked true}
bind $grip <ButtonRelease-1> {
    set ::grip_clicked false
    checkRatioChanged
}

# frame to hide tree
set fh [frame $::tf.fh -borderwidth 0 -relief raised]
pack $fh -fill x
set fh.top [frame $::tf.fh.top]
pack $fh.top -fill x
set fh.bot [frame $::tf.fh.bot]
set bh [button $fh.top.bh -borderwidth 0 -text » -padx 4 -pady 1]
pack $bh -side right
bind $bh <Button-1> [list hideListview true]
# preferences button
set bp [checkbutton $fh.top.bpref -image preferencesIcon -indicatoron false -variable ::bp_state -borderwidth 0 -height 20 -width 20]
pack $bp -side right -pady 0
bind $bp <Button-1> {
    if {$::bp_state} {
        pack forget $fh.bot
    } else {
        pack $fh.bot -fill x
    }
}
set cb_fp [checkbutton $fh.bot.fp -variable ::search_full_path -text [msgcat::mc "Full path (regex)"]]
pack $cb_fp -side left
bind $cb_fp <ButtonRelease-1> {refreshHAL}
# filter entry
set ::txt_filt [msgcat::mc "Filter tree"]
set ::fe_active false
set fe [entry $fh.top.fe -textvariable txt_filt -foreground grey50]
pack $fe -fill x -expand y -side left -pady 1
bind $fe <FocusIn> {
    if {!$::fe_active} {
        set ::txt_filt ""
        $fe configure -foreground black
        set ::fe_active true
    }
}
bind $fe <FocusOut> {
    if {$::txt_filt == ""} {
        set ::txt_filt [msgcat::mc "Filter tree"]
        $fe configure -foreground grey50
        set ::fe_active false
    }
}
bind $fe <KeyPress-Return> {refreshHAL}

# frame to show tree
set ::fs [frame $::rightf.fs -borderwidth 1 -relief raised -width 24]
set bs [button $::fs.bs -borderwidth 0  -text « -padx 5 -pady 0]
pack $bs -side top
bind $bs <Button-1> [list showListview]
# add canvas to create rotated text
set clbl [canvas $::fs.clbl -width 20]
pack $clbl
$clbl create text 10 5 -angle 90 -anchor e -text [msgcat::mc "Tree View"] -font [list "" 10]

proc hideListview {resizeWindow} {
    place $::fs -width 24 -relheight 1.0
    pack forget $::nb
    place $::nb -anchor ne -relx 1.0 -relwidth 1.0 -width -33 -relheight 1.0
    placeFrames 0
    if {$resizeWindow} {
        set ::old_w_leftf [winfo width $::leftf]
        set new_w [expr [winfo width $::nb] + [$::fs cget -width] + 9 + 2* [.main cget -padx]]
        set new_x [int [expr [winfo x .] + [winfo width .] - $new_w - 3]]
        # offset added here because [winfo geometry .] differs from [wm geometry .]    
        set y [expr [winfo y .] - 61]
        wm geometry . "${new_w}x[winfo height .]+$new_x+$y"
        tkwait visibility $::fs
   }
}

set ::old_w_leftf 160
proc showListview {} {
    place forget $::fs
    place configure $::nb -relwidth 1.0 -width 0
    # recalc ratio
    set ratio [expr double($::old_w_leftf) / ([winfo width $::nb] + $::old_w_leftf)]
    placeFrames $ratio
    set new_w [int [expr  $::old_w_leftf + [winfo width $::nb] + 2*[.main cget -padx]]]
    set new_x [int [expr [winfo x .] + [winfo width .] - $new_w - 3]] 
    # offset added here because [winfo geometry .] differs from [wm geometry .]
    set y [expr [winfo y .] - 61]
    wm geometry . "${new_w}x[winfo height .]+$new_x+$y"
    tkwait visibility $::tf
}

# scale left and right frame while dragging
proc scaleFrames {} {
    if {$::grip_clicked} {
        set xpos [expr {[winfo pointerx .] - [winfo x .]}]
        set padx [$::main cget -padx]
        if {$xpos >= [expr $padx+4] && $xpos <= [expr [winfo width .]-$padx-4]} {
            set ratio [expr double ($xpos-$padx+4)/([winfo width .]-2*$padx)]
            placeFrames $ratio
        }
    }
}

# build the tree widgets left side
set ::treew [Tree $::tf.t  -width 10 -yscrollcommand "sSlide $::tf" ]
set str $::tf.sc
scrollbar $str -orient vert -command "$::treew yview"
pack $str -side right -fill y
pack $::treew -side right -fill both -expand yes
$::treew bindText <Button-1> {workMode}
$::treew bindText <Button-3> {popupmenu_tree %X %Y}
$::treew configure -selectbackground "orange3"

proc addSubTree {item} {
    if {[string first "+" $item] > 0} {
        set item [regsub "\\+" $item " "]
        set list [eval hal "show $item"]
        regexp ".*(?=\\s)" $item type
        addToWatch $type $list
    }
}

#----------tree widget handlers----------
# a global var -- ::treenodes -- holds the names of existing nodes
# ::nodenames are the text applied to the toplevel tree nodes
# they could be internationalized here but the international name
# must contain no whitespace.  I'm not certain how to do that.
set ::nodenames {Components Pins Parameters Signals Functions Threads}

# ::searchnames is the real name to be used to reference
set ::searchnames {comp pin param sig funct thread}

set ::treenodes ""
proc refreshHAL {} {
    set tmpnodes ""
    # look through tree for nodes that are displayed
    foreach node $::treenodes {
        catch {
            if {[$::treew itemcget $node -open]} {
                lappend tmpnodes $node
            }
        }
    }
    # clean out the old tree
    $::treew delete $::searchnames
    # reread hal and make new nodes
    listHAL
    # read opennodes and set tree state if they still exist
    # foreach node $tmpnodes {
    #     if {[$::treew exists $node]} {
    #         $::treew opentree $node no
    #     }
    # }
    showHAL $::oldvar
}

# listhal gets $searchname stuff
# and calls makeNodeX with list of stuff found.
proc listHAL {} {
    set i 0
    foreach node $::searchnames {
        writeNode "$i root $node [lindex $::nodenames $i] 1"
        set ${node}str [hal list $node]

        # remove items from tree that do not match the regex
        if {$::fe_active && $::txt_filt != ""} {
            set temp [split [string trim [set ${node}str]] " "]
            set ${node}str ""
            foreach path $temp {
                if {$::search_full_path} {
                    if {[regexp $::txt_filt $path]} {
                        lappend ${node}str $path
                    }
                } else {
                    set items [split $path "."]
                    foreach item $items {
                        if {[regexp $::txt_filt $item]} {
                            lappend ${node}str $path
                            break
                        }
                    }
                }
            }
        }

        switch -- $node {
            pin {-}
            param {
                makeNodeP $node [set ${node}str]
            }
            sig {
                makeNodeP $node [set ${node}str]
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
    # make an array to hold position counts
    array set pcounts {1 1}
    # consider each listed element
    foreach p $pstring {
        set elementlist [split $p "." ]
        set lastnode [llength $elementlist]
        set i 1
        set snode {}
        foreach element $elementlist {
            if {$snode == {}} {
                set parent $which; set snode "$which+$element"
            } else {
                set parent $snode; set snode "$snode.$element"
            }
            set leaf [expr {$i == $lastnode}]
            set j $pcounts($i)
            if {! [$::treew exists "$snode"] } {
                writeNode [list $j $parent $snode $element $leaf]
            }
            incr pcounts($i)
            incr i
            set pcounts($i) 1
           # end of element foreach
        }
        # end of param foreach
    }
    # empty the counts array in preparation for next proc call
    array unset pcounts {}
}

proc makeNodeOther {which otherstring} {
    set i 0
    foreach element $otherstring {
        set snode "$which+$element"
        if {! [$::treew exists "$snode"] } {
            set leaf 0
            writeNode "$i $which $snode $element $leaf"
        }
        incr i
    }
}

# writeNode handles tree node insertion for makeNodeX
# builds a global list -- ::treenodes -- but not leaves
proc writeNode {arg} {
    scan $arg {%i %s %s %s %i} j base node name leaf
    $::treew insert end  $base  $node -text $name

    if {$::txt_filt != ""} {
        # strip/extract leading type
        set plusPos [string first "+" $node]
        set subnode [string replace $node 0 $plusPos]
        set type [string range $node 0 $plusPos]

        if {$::search_full_path && $plusPos > 0} {
            set match_str ""
            set return [regexp $::txt_filt $subnode match_str]
            if {$return} {
                set match_start [string first $match_str $subnode]
                set match_end [expr $match_start + [string length $match_str]]
                set match_end_next_p [string first "." $subnode $match_end]
                set match_start_prev_p [string last "." $subnode $match_start]
                if {$match_end_next_p > 0} {
                    set subnode [string replace $subnode $match_end_next_p end]
                }
                set match_items [string replace $subnode 0 $match_start_prev_p]
                set n_items [llength [split $match_items "."]]
                openTreePath $type$subnode $n_items
            }
        } elseif {[regexp $::txt_filt $name]} {
            openTreePath $node 1
        }
    }
    if {$leaf > 0} {
        lappend ::treenodes $node
    }
}

proc openTreePath {path_in highlight_n} {
    if {$path_in=="root"} {return}
    # this is needed if comp name includes a '+'
    set plusPos [string first "+" $path_in]
    set path [string replace $path_in $plusPos $plusPos "."]

    set items [split $path "."]
    set items_reduced [lreplace $items end end]
    set path ""
    set i 0
    set highlight [expr [llength $items] - $highlight_n]

    foreach item $items_reduced {
        if {$i==0} {
            set path $item
        } elseif {$i==1} {
            set path [string cat $path "+" $item]
        } else {
            set path [string cat $path "." $item]
        }
        catch {
            $::treew opentree $path no
            if {$i >= $highlight} {
                $::treew selection add $path
            }
        }
        incr i 1
    }
    catch {$::treew selection add $path_in}
}

proc showNode {which} {
    switch -- $which {
        open {-}
        close {
            foreach type $::searchnames {
                $::treew ${which}tree $type
            }
        }
        pin {-}
        param {-}
        sig {
            foreach type $::searchnames {
                $::treew closetree $type
            }
            $::treew opentree $which
            $::treew see $which
        }
        default {}
    }
    focus -force $::treew
}

#
#----------end of tree building processes----------

set ::oldvar "zzz"
# build show mode right side
proc makeShow {} {

    set f1 [frame $::showhal.f1 -relief ridge -borderwidth 5]
    pack $f1 -fill both -expand 1
    pack [frame $f1.top] -side top -fill both -expand 1
    set ::disp [text $f1.top.tx  -wrap word -takefocus 0 -state disabled \
             -width 0 -height 10\
             -relief ridge -borderwidth 0 -yscrollcommand "sSlide $f1.top"]
    pack $::disp -side left -fill both -expand yes
    pack [scrollbar $f1.top.sc -orient vert -command "$::disp yview"]\
         -side left -fill y

    set f2 [frame .f2 -borderwidth 0]
    pack $f2 -fill x -expand 0
    pack [frame $f2.b] \
         -side top -fill x -anchor w
    pack [label $f2.b.label -text [msgcat::mc "HAL command :"] ]\
         -side left -padx 5 -pady 3
    set com [entry $f2.b.entry -textvariable halcommand]
    pack $com -side left -fill x -expand 1 -pady 3
    bind $com <KeyPress-Return> {showEx $halcommand}
    bind $com <Control-KeyPress-v> {
        if {[%W selection present]} {%W delete sel.first sel.last}
    }
    bind $com <Up> {moveHist %W -1}
    bind $com <Down> {moveHist %W 1}  
    set ex [button $f2.b.execute -text [msgcat::mc "Execute"] \
            -command {showEx $halcommand} ]
    pack $ex -side left -padx 5 -pady 3
    pack [frame $f2.show -height 5] \
         -side top -fill both -expand 1
    set ::showtext [text $f2.show.txt \
                 -width 0 -height 1 -bg grey85 \
                 -borderwidth 2 -relief sunken]
    pack $::showtext -side left -fill both -anchor w -expand 1 -pady {0 5} -padx 5
    pack [ttk::sizegrip $f2.show.grip] -side right -anchor se

    bind $::disp <Button-3> {popupmenu_text %X %Y}
    bind . <Control-KeyPress-c> {copySelection 0}
    bind $f2.show.grip <ButtonRelease-1> {checkSizeChanged %W}
    bind . <FocusIn> {checkSizeChanged %W}
}

proc copySelection {clear} {
    clipboard clear
    catch {clipboard append [selection get]}
    if {$clear} {
        selection clear
    }
}

proc makeWatch {} {
    set ::cisp [canvas $::watchhal.c -yscrollcommand [list $::watchhal.s set]]
    scrollbar $::watchhal.s -command [list $::cisp yview] -orient v
    pack $::watchhal.s -side right -fill y -expand no
    pack $::cisp -side right -fill both -expand yes
    bind $::cisp <Configure> {
        if {$::watchlist_len <= 20} reloadWatch
    }
}

proc makeSettings {} {
    proc addTextSetting {frame var descr} {
        pack [frame $frame.$var] -fill x -anchor w -pady 2
        pack [entry $frame.$var.entry -textvariable $var -width 5] -side right
        pack [label $frame.$var.label -text [msgcat::mc $descr] -justify left]\
            -side left -padx 2
    }
    proc addBoolSetting {frame var descr} {
        pack [frame $frame.$var] -fill x -anchor w -pady 2
        pack [checkbutton $frame.$var.checkbox -variable $var] -side right
        pack [label $frame.$var.label -text [msgcat::mc $descr] -justify left]\
            -side left -padx 0
    }
    set f1 [frame $::settings.f1]
    pack $f1 -expand 0 -side left
    addTextSetting $f1 ::watchInterval [msgcat::mc "Update interval (in ms)"]
    addTextSetting $f1 ::col1_width [msgcat::mc "Column width for value in watch tab"]
    pack [label $f1.label -text [msgcat::mc "Override format string (leave empty for default)"] \
        -justify left]  -anchor w -pady 2 -padx 2
    addTextSetting $f1 ::ffmts "    [msgcat::mc "Float"]"
    addTextSetting $f1 ::ifmts "    [msgcat::mc "Integer"]"
    set ::ffmt_setting $f1.::ffmts
    set ::ifmt_setting $f1.::ifmts
    addBoolSetting $f1 ::alwaysOnTop [msgcat::mc "Always on top\n(Note: May not\
        working with all desktop environments)"]
    addBoolSetting $f1 ::autoSaveWatchlist [msgcat::mc "Remember watchlist"]
    pack [button $f1.apply -text [msgcat::mc "Apply"] \
        -command {
            wm attributes . -topmost $::alwaysOnTop
            reloadWatch
            }] -side right -padx 5 -pady 10
    set ::infotext [text $f1.infotext -bd 0 -bg grey85 -wrap word -font [list "" 10]]
    $::infotext insert end "([msgcat::mc "Settings stored in: "] $::INIFILE)"
    $::infotext config -state disabled
    pack $::infotext -pady {20 0} -side left
}

# showmode handles the tab selection of mode
proc showMode {mode} {
    set ::workmode $mode
    if {$mode=="watchhal"} {
        watchLoop
    }
}

# all clicks on tree node names go into workMode
# keeps the last HAL variable for refresh
proc workMode {which} {
    set ::thisvar $which
    switch -- $::workmode {
        showhal {
            showHAL $which
        }
        watchhal {
            watchHAL $which
            setStatusbar ""
        }
        default {
            showMode showhal
            displayThis "Mode went way wrong."
        }
    }
    set ::oldvar $which
}

# process uses it's own halcmd show so that displayed
# info looks like what is in the Hal_Introduction.pdf
proc showHAL {which} {
    if {![info exists ::disp]} {return}
    if {$which == "zzz"} {
        displayThis [msgcat::mc "Select a node to show."]
        return
    }
    set thisnode $which
    set thislist [split $which "+"]
    set searchbase [lindex $thislist 0]
    set searchstring [lindex $thislist 1]
    set thisret [hal show $searchbase $searchstring]
    displayThis $thisret
}

proc showEx {what} {
    addToHist $what
    set str [eval hal $what]
    $::disp configure -state normal
    $::disp delete 1.0 end
    $::disp insert end $str
    $::disp configure -state disabled
}

set ::hist ""
set ::i_hist 0
proc addToHist {s} {
    if {$s == ""} return
    if [string compare $s [lindex $::hist end]] {
        lappend ::hist $s
        set ::i_hist [expr [llength $::hist]-1]
    }
}

proc moveHist {w where} {
    incr ::i_hist $where
    if {[set ::i_hist]<0} {set ::i_hist 0}
    if {[set ::i_hist]>=[llength $::hist]+1} {
        set ::i_hist [llength $::hist]
    }
    set ::[$w cget -textvariable] [lindex $::hist [set ::i_hist]]
    $w icursor end
}

set ::last_watchfile_tail my.halshow
set ::last_watchfile_dir  [pwd]
set ::filetypes { {{HALSHOW} {.halshow}}\
                  {{TXT}     {.txt}}\
                  {{ANY}     {*}}\
                }
set ::watchlist ""
set ::watchlist_len 0
set ::watchstring ""
set ::col1_width 100
proc watchHAL {which} {
    if {$which == "zzz"} {
        $::cisp create text 40 [expr 1 * 20 + 12] -anchor w -tag firstmessage\
            -text [msgcat::mc "<-- Select a Leaf.  Click on its name."]
        set ::watchlist ""
        set ::watchstring ""
        return
    } else {
        $::cisp delete firstmessage
    }
    set tmplist [split $which +]
    set vartype [lindex $tmplist 0]
    set varname [lindex $tmplist end]
    # return if variable is already used.
    if {[lsearch $::watchlist $which] != -1} {
        setStatusbar "'$varname' [msgcat::mc "already in list"]"
        return "Item already in list"
    }
    if {$vartype != "pin" && $vartype != "param" && $vartype != "sig"} {
        # cannot watch components, functions, or threads
        return
    }
    if {$vartype == "sig"} {
        # stype (and gets) fail when the item clicked is not a leaf
        # e.g., clicking "Signals / X"
        if {[catch {hal stype $varname} type]} { 
            setStatusbar $type
            return $type
        }
    } else {
        # ptype (and getp) fail when the item clicked is not a leaf
        # e.g., clicking "Pins / axis / 0"
        if {[catch {hal ptype $varname} type]} { 
            setStatusbar $type
            return $type
        }
    }

    lappend ::watchlist $which
    set ::watchlist_len [llength $::watchlist]
    set i $::watchlist_len
    set label [lindex [split $which +] end]
    set labelcolor black
     # check if pin or param is writable
     # var writable: 1=yes, 0=no, -1=writable but connected to signal
    set writable 0
    set showret [join [hal show $vartype $label] " "]
    if {$vartype == "pin"} {
        # check if pin is input
        if {[string index [lindex $showret 9] 0] == "I"} {
            # check if signals are connected to pin
            if {[string first "==" [lindex $showret 12] 0] < 0} {
                set writable 1
            } else {
                set writable -1
            }
        }
    } elseif {$vartype == "param"} {
        # check if parameter is writable
        if {[lindex $showret 8] == "RW"} {
            set writable 1
        }
        set labelcolor #6e3400
    } elseif {$vartype == "sig"} {
        # puts stderr "return $showret, found: [string first "<==" $showret 0]"
        # check if signal has no writers
        if {[string first "<==" $showret 0] < 0} {
            set writable 1
        }
        set labelcolor blue3
    }

    $::cisp create text $::col1_width [expr $i * 20 + 13] -text $label \
            -anchor w -tag $label -fill $labelcolor
    set canvaswidth [winfo width $::cisp]
    if {$type == "bit"} {
        $::cisp create oval 10 [expr $i * 20 + 5] 25 [expr $i * 20 + 20] \
            -fill lightgray -tag oval$i
        if {$writable == 1} {
            if {$vartype == "sig"} {
                canvasbutton::canvasbutton $::cisp [expr $canvaswidth - 48] \
                    [expr {$i * 20 + 4}] 24 17 "Set" [list hal_sets $label 1] 1
                canvasbutton::canvasbutton $::cisp [expr $canvaswidth - 20] \
                    [expr {$i * 20 + 4}] 24 17 "Clr" [list hal_sets $label 0] 1
            } else {
                canvasbutton::canvasbutton $::cisp [expr $canvaswidth - 48] \
                    [expr {$i * 20 + 4}] 24 17 "Set" [list hal_setp $label 1] 1
                canvasbutton::canvasbutton $::cisp [expr $canvaswidth - 20] \
                    [expr {$i * 20 + 4}] 24 17 "Clr" [list hal_setp $label 0] 1
            }
        } elseif {$writable == -1} {
            canvasbutton::canvasbutton $::cisp [expr $canvaswidth - 48] \
                [expr $i * 20 + 4] 24 17 "Set" [] 0
            canvasbutton::canvasbutton $::cisp [expr $canvaswidth - 20] \
                [expr $i * 20 + 4] 24 17 "Clr" [] 0
        }
    } else {
        $::cisp create text 10 [expr $i * 20 + 12] -text "" \
            -anchor w -tag text$i

        if {$writable == 1} {
            if {$vartype == "sig"} {
                canvasbutton::canvasbutton $::cisp [expr $canvaswidth - 48] \
                    [expr $i * 20 + 4] 52 17 "Set val" [list setsValue $label] 1
            } else {
                canvasbutton::canvasbutton $::cisp [expr $canvaswidth - 48] \
                    [expr $i * 20 + 4] 52 17 "Set val" [list setpValue $label] 1
            }
        } elseif {$writable == -1} {
            canvasbutton::canvasbutton $::cisp [expr $canvaswidth - 48] \
                [expr $i * 20 + 4] 52 17 "Set val" [] 0
        }
    }
    if {$i > 1} {$::cisp create line 10 [expr $i * 20 + 3] [expr $canvaswidth - 52] \
        [expr $i * 20 + 3] -fill grey70}
    $::cisp bind $label <Button-3> [list popupmenu_watch $vartype $label $i $writable $which %X %Y]
    $::cisp configure -scrollregion [$::cisp bbox all]
    $::cisp yview moveto 1.0
    set tmplist [split $which +]
    set vartype [lindex $tmplist 0]
    set varname [lindex $tmplist end]
    lappend ::watchstring "$i $vartype $varname "
    refreshItem $i $vartype $label
}

proc popupmenu_watch {vartype label index writable which x y} {
    # create menu
    set m [menu .popupMenu$index -tearoff false]
    # add entries
    $m add command -label [msgcat::mc "Copy"] -command [list copyName $label]
    if {$writable} {
        $m add command -label [msgcat::mc "Set to .."] -command [list setpValue $label]
    }
    if {$writable == -1} {
        $m add command -label [msgcat::mc "Unlink pin"] -command [list unlinkp $label $index]
    }
    $m add command -label [msgcat::mc "Show in Tree"] -command "refreshHAL; openTreePath $vartype+$label 1"
    $m add command -label [msgcat::mc "Remove"] -command [list watchReset $label]
    # show menu
    tk_popup $m $x $y
    bind $m <FocusOut> [list destroy $m]
}

proc popupmenu_text {x y} {
    # create menu
    set m [menu .popupMenuText -tearoff false]
    # add entries
    $m add command -label [msgcat::mc "Copy"] -command {copySelection 0}
    $m add command -label [msgcat::mc "Add as Pin(s)"] -command {addToWatch "pin" [join [selection get] " "]}
    $m add command -label [msgcat::mc "Add as Signal(s)"] -command {addToWatch "sig" [join [selection get] " "]}
    $m add command -label [msgcat::mc "Add as Param(s)"] -command {addToWatch "param" [join [selection get] " "]}
    # show menu
    tk_popup $m $x $y
    bind $m <FocusOut> [list destroy $m]
}

proc popupmenu_tree {x y item} {
    if {[string first "+" $item] > 0} {
        # create menu
        set m [menu .popupMenuText -tearoff false]
        # add entries
        $m add command -label [msgcat::mc "Add all sub-items to watch"] -command "addSubTree $item"
        # show menu
        tk_popup $m $x $y
        bind $m <FocusOut> [list destroy $m]
    }
}

proc addToWatch {type selection} {
    set varcount 0
    catch {
        foreach item $selection {
            if {![catch {hal [string index $type 0]type $item} return]} { 
                if {[watchHAL "$type+$item"] == ""} {
                incr varcount 
                } 
            }
        }
    }
    setStatusbar [msgcat::mc "$varcount item(s) added"]
}

proc hal_setp {label val} {
    eval hal "setp $label $val"
}

proc hal_sets {label val} {
    eval hal "sets $label $val"
}

proc copyName {label} {
    clipboard clear
    clipboard append $label
}

proc setpValue {label} {
    set val [eval hal "getp $label"]
    set val [entrybox $val [msgcat::mc "Set"] $label]
    if {$val != "cancel"} {
        eval hal "setp $label $val"
    }
}

proc setsValue {label} {
    set val [eval hal "gets $label"]
    set val [entrybox $val [msgcat::mc "Set"] $label]
    if {$val != "cancel"} {
        eval hal "sets $label $val"
    }
}

proc unlinkp {label i} {
    # when unlink command successful --> rebuild list
    if {[eval hal "unlinkp $label"] == "Pin '$label' unlinked"} {
        reloadWatch       
    }
}

proc entrybox {defVal buttonText label} {
    if {[winfo exists .top]} {
        raise .top
        focus .top
        return "cancel"
    } else {
        set wn [toplevel .top]
        wm title $wn [msgcat::mc "User input"]
        set xpos "[ expr {[winfo rootx [winfo parent $wn]]+ \
            ([winfo width [winfo parent $wn]]-[winfo reqwidth $wn])/2}]"
        set ypos "[ expr {[winfo rooty [winfo parent $wn]]+ \
            ([winfo height [winfo parent $wn]]-[winfo reqheight $wn])/2}]"
        wm geometry $wn "+$xpos+$ypos"
        wm attributes $wn -topmost $::alwaysOnTop
        variable entryVal
        set entryVal $defVal
        label .top.lbl -text $label
        entry .top.en -textvariable entryVal
        # -validate all-validatecommand {expr {[string is double %P] || [string is bool %P]}}
        .top.en icursor end
        button .top.but -command {set ret $entryVal} -text $buttonText
        bind .top.en <Return> {set ret $entryVal}
        bind .top.en <KP_Enter> {set ret $entryVal}
        wm protocol .top WM_DELETE_WINDOW {set ret "cancel"}; # on X clicked
        pack {*}[winfo children .top]
        focus .top.en
        vwait ret
        unset -nocomplain ret
        unset -nocomplain entryVal
        destroy .top
        return $::ret
    }
}

# watchHAL prepares a string of {i HALtype name} sets
# watchLoop submits these to halcmd and sets canvas
# color or value based on reply
set ::watching 0
set ::watchInterval 100
proc watchLoop {} {
    set ::watching 1
    set which $::watchstring
    foreach var $which {
        scan $var {%i %s %s} cnum vartype varname
        refreshItem $cnum $vartype $varname
    }
    if {$::workmode == "watchhal"} {
        after $::watchInterval watchLoop
    } else {
        set ::watching 0
    }
}

proc setWatchInterval {} {
    while {true} {
        set interval [entrybox $::watchInterval [msgcat::mc "Set"] \
            [msgcat::mc "Update interval for this session (ms)"]]
        if {$interval < 1} {
            tk_messageBox -message [msgcat::mc "Value out of range"] -type ok -icon warning
        } elseif {$interval == "cancel"} {
            break;
        } else {
            set ::watchInterval $interval
            break
        }
    }
}

proc refreshItem {cnum vartype varname} {
    if {$vartype == "sig" } {
        set ret [hal gets $varname]
        set varnumtype [hal stype $varname]
    } else {
        set ret [hal getp $varname]
        set varnumtype [hal ptype $varname]
    }
    if {$ret == "TRUE"} {
        $::cisp itemconfigure oval$cnum -fill yellow
    } elseif {$ret == "FALSE"} {
        $::cisp itemconfigure oval$cnum -fill firebrick4
    } else {
        switch $varnumtype {
            u32 - s32  {set varnumtype int}
            float      {set varnumtype float}
        }
        if [catch { set value [expr $ret] } ] {
            set value $ret ;# allow display of a nan
        } else {
            # use format if provided via settings
            if {$::ffmts != "" && ("$varnumtype" == "float")} {
                set value [format "$::ffmts" $ret]
            }
            if {$::ifmts != "" && ("$varnumtype" == "int")} {
                set value [format "$::ifmts" $ret]
            }
            # use format if provided via command line
            if {[info exists ::ffmt] && ("$varnumtype" == "float")} {
                set value [format "$::ffmt" $ret]
            }
            if {[info exists ::ifmt] && ("$varnumtype" == "int")} {
                set value [format "$::ifmt" $ret]
            }
        }
        $::cisp itemconfigure text$cnum -text $value
    }
}

proc watchReset {del} {
    $::cisp delete all
    switch -- $del {
        all {
            watchHAL zzz
            return
        }
        default {
            set item [string map {+ "\\+"} $del]; # escape '+' for regexp
            set place [lsearch -regexp $::watchlist $item]
            if {$place != -1 } {
                set ::watchlist [lreplace $::watchlist $place $place]
                set watchlist_copy $::watchlist
                set ::watchlist ""
                set scrollbar_pos [lindex [$::cisp yview] 0]
                foreach var $watchlist_copy {
                    watchHAL $var
                }
                $::cisp yview moveto [expr $scrollbar_pos * (1 + 1/double([llength $watchlist_copy]))]
                setStatusbar "'$del' [msgcat::mc "removed from list"]"
            } else {            
                watchHAL zzz
            }
        }
    }
}

proc reloadWatch {} {
    set watchlist_copy $::watchlist
    set scrollbar_pos [lindex [$::cisp yview] 0]
    watchReset all
    foreach item $watchlist_copy { watchHAL $item }
    $::cisp yview moveto $scrollbar_pos
}

# proc switches the insert and removal of upper right text
# This also removes any modify array variables
proc displayThis {str} {
    $::disp configure -state normal
    $::disp delete 1.0 end
    $::disp insert end $str
    $::disp configure -state disabled
}

proc getwatchlist {} {
  set lfile [tk_getOpenFile \
            -filetypes   $::filetypes\
            -initialdir  $::last_watchfile_dir\
            -initialfile $::last_watchfile_tail\
            -title       [msgcat::mc "Load a watch list"]\
            ]
  loadwatchlist $lfile
}

proc loadwatchlist {filename} {
  if {"$filename" == ""} return
  set f [open $filename r]
  set wl ""
  while {![eof $f]} {
     set nextline [string trim [gets $f]]
     if {[string first "#" $nextline] == 0} continue ;# ignore comment lines
     set wl "$wl $nextline"
  }
  close $f
  set ::last_watchfile_tail [file tail    $filename]
  set ::last_watchfile_dir  [file dirname $filename]
  wm title . "$::last_watchfile_tail - $::titlename"
  if {"$wl" == ""} return

  # Backup auto-saved watchlist
  set backupFile [string map {"//" "/"} $::INIDIR/.halshow_watchlist_backup]
  writeWatchlist $backupFile multiline

  watchReset all
  $::nb raise pw
  foreach item $wl { watchHAL $item }
  setStatusbar  "$::last_watchfile_tail [msgcat::mc "loaded"], [msgcat::mc "saved backup for old watchlist in"] $backupFile"
}

proc savewatchlist { {fmt oneline} } {
  if {"$::watchlist" == ""} {
    return -code error "savewatchlist: null ::watchlist"
  }
  set sfile [tk_getSaveFile \
            -filetypes   $::filetypes\
            -initialdir  $::last_watchfile_dir\
            -initialfile $::last_watchfile_tail\
            -title       [msgcat::mc "Save current watch list"]\
            ]
  writeWatchlist $sfile $fmt
  set ::last_watchfile_tail [file tail    $sfile]
  set ::last_watchfile_dir  [file dirname $sfile]
  wm title . "$::last_watchfile_tail - $::titlename"
}

proc writeWatchlist {sfile fmt} {
    if {"$sfile" == ""} return
    set f [open $sfile w]
    switch $fmt {
        multiline {
            puts $f "# halshow watchlist created [clock format [clock seconds]]\n"
            foreach line $::watchlist {
            puts $f $line
            }
        }
        default {puts $f $::watchlist}
    }
    close $f
}

#----------start up the displays----------
makeShow
makeWatch
makeSettings
refreshHAL
$::nb raise ps

proc setStatusbar {message} {
    $::showtext config -state normal
    $::showtext delete 1.0 end
    $::showtext insert end $message
    $::showtext config -state disabled
}
setStatusbar [msgcat::mc "Commands may be tested here but they will NOT be saved"]

proc usage {} {
  set prog [file tail $::argv0]
  puts "Usage:"
  puts "  $prog \[Options\] \[watchfile\]"
  puts "  Options:"
  puts "           --help    (this help)"
  puts "           --fformat format_string_for_float"
  puts "           --iformat format_string_for_int"
  puts "           --noprefs don't use preference file to save settings"
  puts ""
  puts "Notes:"
  puts "       Create watchfile in halshow using: 'File/Save Watch List'."
  puts "       LinuxCNC must be running for standalone usage."
  exit 0
}

if {[llength $::argv] > 0} {
    set idx 0
    while {$idx < [llength $::argv]} {
        switch [lindex $::argv $idx] {
            "--help" {
                incr idx; usage
            }
            "--iformat" {
                incr idx;
                set ::ifmt [lindex $::argv $idx]
                incr idx
                $::ifmt_setting.label configure -text "    [msgcat::mc "Integer (disabled by \"--iformat\" argument)"]"
                $::ifmt_setting.entry configure -state disabled
            }
            "--fformat" {
                incr idx;
                set ::ffmt [lindex $::argv $idx]
                incr idx
                $::ffmt_setting.label configure -text "    [msgcat::mc "Float (disabled by \"--fformat\" argument)"]"
                $::ffmt_setting.entry configure -state disabled
            }
            "--noprefs" {
                set ::use_prefs false
                $::infotext configure -fg red
                $::infotext config -state normal
                $::infotext replace 0.0 end  "[msgcat::mc "\"--noprefs\" option set. Settings will not be saved!"]"
                $::infotext config -state disabled
                incr idx
            }
            default {
                set watchfile [lindex $::argv $idx]
                incr idx
            }
        }
    }
}

# Loading the settings from the file.
# This overrides the default settings above.
if {$::use_prefs} {
    readIni
    if {$::ratio == 0} {
        hideListview false
    }
    if {$::workmode == "watchhal"} {
        $::nb raise pw
    }
}

# Load watchlist from file
if {[info exists watchfile]} {
    if [file readable $watchfile] {
        loadwatchlist $watchfile
        set ::last_watchfile_tail [file tail    $watchfile]
        set ::last_watchfile_dir  [file dirname $watchfile]
    } else {
        puts "\nCannot read file <$watchfile>\n"
        usage
    }
}

wm attributes . -topmost $::alwaysOnTop
tkwait visibility .
set ::initPhase false
