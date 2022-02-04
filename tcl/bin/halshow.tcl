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

#----------start toplevel----------
#
set ::titlename [msgcat::mc "HAL Show"]
wm title . $::titlename
wm protocol . WM_DELETE_WINDOW tk_
set masterwidth 700
set masterheight 475
# set fixed size for configuration display and center
set xmax [winfo screenwidth .]
set ymax [winfo screenheight .]
set x [expr ($xmax - $masterwidth )  / 2 ]
set y [expr ($ymax - $masterheight )  / 2]
wm geometry . "${masterwidth}x${masterheight}+$x+$y"
wm minsize . [int [expr $masterwidth*0.3]] [int [expr $masterheight*0.5]]
wm attributes . -topmost yes

# trap mouse click on window manager delete and ask to save
wm protocol . WM_DELETE_WINDOW askKill
proc askKill {} {
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

set main [frame .main -padx 10 -pady 10]
pack $main -fill both -expand yes

# build frames from left side
set tf [frame $main.maint]
set top [NoteBook $main.note]

# Each mode has a unique set of widgets inside tab
set showhal [$top insert 0 ps -text [msgcat::mc " SHOW "] -raisecmd {showMode showhal} ]
set ::watchhal [$top insert 1 pw -text [msgcat::mc " WATCH "] -raisecmd {showMode watchhal}]

# use place manager to fix locations of frames within top
place configure $tf -in $main -x 0 -y 0 -relheight 1 -relwidth .3
place configure $top -in $main -relx .3 -y 0 -relheight 1 -relwidth .7

# slider process is used for several widgets
proc sSlide {f a b} {
    $f.sc set $a $b
}

# Build menu
# fixme clean up the underlines so they are unique under each set
set menubar [menu $top.menubar -tearoff 0]
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

set watchmenu [menu $menubar.watch -tearoff 1]
    $menubar add cascade -label [msgcat::mc "Watch"] \
            -menu $watchmenu
        $watchmenu add command -label [msgcat::mc "Add pin"] \
            -command {watchHAL pin+[entrybox "" [msgcat::mc "Add to watch"] "Pin"]}
        $watchmenu add command -label [msgcat::mc "Add signal"] \
            -command {watchHAL sig+[entrybox "" [msgcat::mc "Add to watch"] "Signal"]}
        $watchmenu add command -label [msgcat::mc "Add parameter"] \
            -command {watchHAL param+[entrybox "" [msgcat::mc "Add to watch"] "Parameter"]}
        $watchmenu add separator
        $watchmenu add command -label [msgcat::mc "Set Watch interval"] \
            -command {setWatchInterval}
        $watchmenu add command -label [msgcat::mc "Reload Watch"] \
            -command {reloadWatch}
        $watchmenu add command -label [msgcat::mc "Erase Watch"] \
            -command {watchReset all}

. configure -menu $menubar

# build the tree widgets left side
set ::treew [Tree $tf.t  -width 10 -yscrollcommand "sSlide $tf" ]
set str $tf.sc
scrollbar $str -orient vert -command "$::treew yview"
pack $str -side right -fill y
pack $::treew -side right -fill both -expand yes
$::treew bindText <Button-1> {workMode   }

#----------tree widget handlers----------
# a global var -- ::treenodes -- holds the names of existing nodes
# ::nodenames are the text applied to the toplevel tree nodes
# they could be internationalized here but the international name
# must contain no whitespace.  I'm not certain how to do that.
set ::nodenames {Components Pins Parameters Signals Functions Threads}

# ::searchnames is the real name to be used to reference
set ::searchnames {comp pin param sig funct thread}
set ::signodes {X Y Z A B C U V W "Spindle"}

set ::treenodes ""
proc refreshHAL {} {
    set tmpnodes ""
    # look through tree for nodes that are displayed
    foreach node $::treenodes {
        if {[$::treew itemcget $node -open]} {
            lappend tmpnodes $node
        }
    }
    # clean out the old tree
    $::treew delete [$::treew nodes root]
    # reread hal and make new nodes
    listHAL
    # read opennodes and set tree state if they still exist
    foreach node $tmpnodes {
        if {[$::treew exists $node]} {
            $::treew opentree $node no
        }
    }
    showHAL $::oldvar
}

# listhal gets $searchname stuff
# and calls makeNodeX with list of stuff found.
proc listHAL {} {
    set i 0
    foreach node $::searchnames {
        writeNode "$i root $node [lindex $::nodenames $i] 1"
        set ${node}str [hal list $node]
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
    if {$leaf > 0} {
        lappend ::treenodes $node
    }
}

proc showNode {which} {
    switch -- $which {
        open {-}
        close {
            foreach type {pin param sig} {
                $::treew ${which}tree $type
            }
        }
        pin {-}
        param {-}
        sig {
            foreach type {pin param sig} {
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

    set f2 [frame .f2 -borderwidth 5]
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
                 -width 0 -height 1  \
                 -borderwidth 2 -relief groove ]
    pack $::showtext -side left -fill both -anchor w -expand 1

    bind $::disp <Button-3> {copySelection 1}
    bind . <Control-KeyPress-c> {copySelection 0}
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
    pack $::cisp -side left -fill both -expand yes
    pack $::watchhal.s -side left -fill y -expand no
    bind $::cisp <Configure> {
        set ::canvaswidth %w
        reloadWatch
    }
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
                  {{ANY}     {.*}}\
                }

set ::watchlist ""
set ::watchstring ""
set ::canvaswidth 438 
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
    # return if variable is already used.
    if {[lsearch $::watchlist $which] != -1} {
        return
    }
    set tmplist [split $which +]
    set vartype [lindex $tmplist 0]
    if {$vartype != "pin" && $vartype != "param" && $vartype != "sig"} {
        # cannot watch components, functions, or threads
        return
    }
    set varname [lindex $tmplist end]
    if {$vartype == "sig"} {
        # stype (and gets) fail when the item clicked is not a leaf
        # e.g., clicking "Signals / X"
        if {[catch {hal stype $varname} type]} { return }
    } else {
        # ptype (and getp) fail when the item clicked is not a leaf
        # e.g., clicking "Pins / axis / 0"
        if {[catch {hal ptype $varname} type]} { return }
    }

    lappend ::watchlist $which
    set i [llength $::watchlist]
    set label [lindex [split $which +] end]

     # check if pin or param is writable
    set writable 0
    set showret [join [hal show $vartype $label] " "]
    if {$vartype == "pin"} {
        if {[string index [lindex $showret 9] 0] == "I"} {
            # check if signals are connected to pin
            if {[string first "==" [lindex $showret 12] 0] < 0} {
                set writable 1
            } else {
                set writable -1
            }
        }
    } elseif {$vartype == "param"} {
        if {[lindex $showret 8] == "RW"} {
            set writable 1
        }
    }

    $::cisp create text 100 [expr $i * 20 + 13] -text $label \
            -anchor w -tag $label
    if {$type == "bit"} {
        $::cisp create oval 10 [expr $i * 20 + 5] 25 [expr $i * 20 + 20] \
            -fill lightgray -tag oval$i
        if {$writable == 1} {
            canvasbutton::canvasbutton $::cisp [expr $::canvaswidth - 48] \
                [expr {$i * 20 + 4}] 24 17 "Set" [list hal_setp $label 1] 1
            canvasbutton::canvasbutton $::cisp [expr $::canvaswidth - 20] \
                [expr {$i * 20 + 4}] 24 17 "Clr" [list hal_setp $label 0] 1
        } elseif {$writable == -1} {
            canvasbutton::canvasbutton $::cisp [expr $::canvaswidth - 48] \
                [expr $i * 20 + 4] 24 17 "Set" [list hal_setp $label 1] 0
            canvasbutton::canvasbutton $::cisp [expr $::canvaswidth - 20] \
                [expr $i * 20 + 4] 24 17 "Clr" [list hal_setp $label 0] 0
        }
    } else {
        $::cisp create text 10 [expr $i * 20 + 12] -text "" \
            -anchor w -tag text$i
       
        if {$writable == 1} {
            canvasbutton::canvasbutton $::cisp [expr $::canvaswidth - 48] \
                [expr $i * 20 + 4] 52 17 "Set val" [list setValue $label] 1
        } elseif {$writable == -1} {
            canvasbutton::canvasbutton $::cisp [expr $::canvaswidth - 48] \
                [expr $i * 20 + 4] 52 17 "Set val" [list setValue $label] 0
        }
    }
    if {$i > 1} {$::cisp create line 10 [expr $i * 20 + 3] [expr $::canvaswidth - 52] \
        [expr $i * 20 + 3] -fill grey75}
    $::cisp bind $label <Button-3> [list popupmenu $label $i $writable $which %X %Y]
    $::cisp configure -scrollregion [$::cisp bbox all]
    $::cisp yview moveto 1.0
    set tmplist [split $which +]
    set vartype [lindex $tmplist 0]
    set varname [lindex $tmplist end]
    lappend ::watchstring "$i $vartype $varname "
    refreshItem $i $vartype $label
}

proc popupmenu {label index writable which x y} {
    # create menu
    set m [menu .popupMenu$index -tearoff false]
    # add entries
    $m add command -label [msgcat::mc "Copy"] -command [list copyName $label]
    if {$writable} {
        $m add command -label [msgcat::mc "Set to .."] -command [list setValue $label]
    }
    if {$writable == -1} {
        $m add command -label [msgcat::mc "Unlink pin"] -command [list unlinkp $label $index]
    }
    $m add command -label [msgcat::mc "Remove"] -command [list watchReset $label]
    # show menu
    tk_popup $m $x $y
    bind $m <FocusOut> [list destroy $m]
}

proc hal_setp {label val} {
    eval hal "setp $label $val"
}

proc copyName {label} {
    clipboard clear
    clipboard append $label
}

proc setValue {label} {
    set val [eval hal "getp $label"]
    set val [entrybox $val [msgcat::mc "Set"] $label]
    if {$val != "cancel"} {
        eval hal "setp $label $val"
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
        wm attributes $wn -topmost yes
        variable entryVal
        set entryVal $defVal
        label .top.lbl -text $label
        entry .top.en -textvariable entryVal
        # -validate all-validatecommand {expr {[string is double %P] || [string is bool %P]}}
        .top.en icursor end
        button .top.but -command {set ret $entryVal} -text $buttonText
        bind .top.en <Return> {set ret $entryVal}
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
            # use format if provided
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
                foreach var $watchlist_copy {
                    watchHAL $var
                }
            } else {            
                watchHAL zzz
            }
        }
    }
}

proc reloadWatch {} {
    set watchlist_copy $::watchlist
    watchReset all
    foreach item $watchlist_copy { watchHAL $item }
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
  watchReset all
  $::top raise pw
  foreach item $wl { watchHAL $item }
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
  set ::last_watchfile_tail [file tail    $sfile]
  set ::last_watchfile_dir  [file dirname $sfile]
}

#----------start up the displays----------
makeShow
makeWatch
refreshHAL
$top raise ps

set firststr [msgcat::mc "Commands may be tested here but they will NOT be saved"]

$::showtext delete 1.0 end
$::showtext insert end $firststr
$::showtext config -state disabled

proc usage {} {
  set prog [file tail $::argv0]
  puts "Usage:"
  puts "  $prog \[Options\] \[watchfile\]"
  puts "  Options:"
  puts "           --help    (this help)"
  puts "           --fformat format_string_for_float"
  puts "           --iformat format_string_for_int"
  puts ""
  puts "Notes:"
  puts "       Create watchfile in halshow using: 'File/Save Watch List'"
  puts "       linuxcnc must be running for standalone usage"
  exit 0
}

if {[llength $::argv] > 0} {
  set idx 0
  while {$idx < [llength $::argv]} {
     switch [lindex $::argv $idx] {
       "--help"    {incr idx; usage}
       "--iformat" {incr idx;
                    set ::ifmt [lindex $::argv $idx]
                    incr idx
                   }
       "--fformat" {incr idx;
                    set ::ffmt [lindex $::argv $idx]
                    incr idx
                   }
       default { set watchfile [lindex $::argv $idx]
                 if [file readable $watchfile] {
                    loadwatchlist $watchfile
                    set ::last_watchfile_tail [file tail    $watchfile]
                    set ::last_watchfile_dir  [file dirname $watchfile]
                 } else {
                    puts "\nCannot read file <$watchfile>\n"
                    usage
                 }
                 incr idx
               }
     }
   }
}
