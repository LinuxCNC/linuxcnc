#!/bin/sh
# the next line restarts using emcsh \
exec /usr/bin/wish "$0" "$@"

# ********************************************************************
# Description:  setupconfig.tcl
#               This file, shows existing emc2 configuration
#               and allows copying and naming
#
#  Author: Raymond E Henry
#  License: GPL Version 2
#    
#  Copyright (c) 2005 All rights reserved.
# 
#  Last change: 
# $Revision$
# $Author$
# $Date$
# ********************************************************************/

# options
foreach class { Button Checkbutton Entry Label Listbox Menu Menubutton \
    Message Radiobutton Scale } {
    option add *$class.borderWidth 1  100
}

# locate the configs directory
set BASEDIR ""
if {[info exists env(EMC2_ORIG_CONFIG_DIR)]} {
  set BASEDIR $env(EEMC2_ORIG_CONFIG_DIR)
}
if {$BASEDIR == ""} {
  set BASEDIR configs/ 
}

# make a toplevel and a master frame.
wm title . "EMC2 Sample Configuration Display"
# wm geometry . 600x400 
set top [frame .main -borderwidth 2 -relief raised -padx 10 -pady 10 ]
pack $top -expand yes -fill both

# major widgets
set topr [labelframe $top.radio -relief groove -text Configurations ]
set topd [text $top.textdisplay -width 40 -height 30 -wrap word \
  -padx 4 -pady 10 -relief flat -takefocus 0]
set topl [label $top.l -text "Enter configuration name :" ]
set tope [entry $top.e -textvariable myname ]
set topokay [button $top.b1 -text Okay \
  -command {writeFileSet $existingconfigs $myname} ]
set topcancel [button $top.b2 -text Cancel -command exit ]

# radiobuttons inside topr are constructed from BASEDIR names
set configs [string tolower [glob -tails -path $BASEDIR *] ]

# we want to exclude cvs and common directories from the listing
foreach conf $configs { 
  if {[lsearch $conf cvs]} {
    if {[lsearch $conf common]} {
      radiobutton $topr.$conf -text $conf -variable existingconfigs \
        -value $conf -anchor w -command "getReadme $conf" 
      pack $topr.$conf -side top -expand yes -fill both -padx 4 -pady 2
    }
  }
}

# widget layout 
grid configure  $topr -column 0 -row 0 -rowspan 3 \
  -sticky nsew -ipadx 5 -ipady 5
grid configure $topd -column 1 -row 0 -columnspan 2\
  -sticky nsew -ipadx 5 -ipady 5
grid configure $topl -column 1 -row 1 \
  -sticky nsew -ipadx 5 -ipady 5
grid configure $tope -column 2 -row 1 \
  -sticky nsew -ipadx 5 -ipady 5
grid configure $topokay -column 1 -row 2 \
  -sticky nsew -ipadx 5 -ipady 5
grid configure $topcancel -column 2 -row 2 \
  -sticky nsew -ipadx 5 -ipady 5
 
# variable myname holds the name to be used for the configuration set.
set myname generic1

# processes
proc getReadme {which} {
  global BASEDIR
   set fname  [file join $BASEDIR $which README]
    if {[catch {open $fname} filein]} {
        displayThis "can't open $fname"
    } else {
        set tmpreadme [read $filein]
        catch {close $filein}
        displayThis $tmpreadme
    }
}

proc displayThis {str} {
  global topd
  $topd delete 1.0 end
  $topd insert end $str
}

proc writeFileSet {which nameit} {
  # this call to displayThis is to be removed as soon as the proper code is
  # in place here.
  displayThis  "you've asked to save $which as $nameit.  Problem is I don't have the code needed to do this yet.  Sorry!  Soon I will though and you'll be happy with me."
  # now figure out exactly what to copy and how.

}

# initial text to display
displayThis "   ----------EMC2 Configuration----------\n\nThis little wizard will help you set up a working copy of some default emc2 configuration files that you can edit to suit your machine.  \n\n
Click on any of the names in the configuration frame along the left edge.  You should see a brief description of what that configuration is about. (Each note replaces this text.)  \n\n
When you have found the one you want to work from, enter a name in the box below and press the Okay button  Okay will copy the configuration file set that you have selected into a new directory in the emc/configs location.  You will then be able to start EMC2 up using it by issuing the command \n\n     emc <mydirectory> \n\nThe Cancel button will remove this wizard without copying anything. \n\n"