#********************************************************************
# Description:  emchelp.tcl
#               This file, 'emchelp.tcl', implements messages for tkemc 
#               baloon help.
#
# Author: 
# License: GPL Version 2
#    
# Copyright (c) 2005-2009 All rights reserved.
#********************************************************************/
# Functions that return help strings for various EMC settings

# Units help, for help with widgets that get their settings from
# emc_display_linear_units
proc unithelp {u} {
    if {$u == "inch"} {
	return "\
Displayed values are in inches, regardless of the\n\
programmed units."
    }
    if {$u == "mm"} {
	return "\
Displayed values are in millimeters, regardless of\n\
the programmed units."
    }
    if {$u == "cm"} {
	return "\
Displayed values are in centimeters, regardless of\n\
the programmed units."
    }
    if {$u == "(inch)"} {
	return "\
Displayed values are in program units,\n\
currently set to inches."
    }
    if {$u == "(mm)"} {
	return "\
Displayed values are in program units,\n\
currently set to millimeters."
    }
    if {$u == "(cm)"} {
	return "\
Displayed values are in program units,\n\
currently set to centimeters."
    }
    # else "custom"
    return "\
Custom units are in effect. See the UNITSentries\n\
in the.ini file for their value, in mm."
}

# Work offset help
proc offsethelp {} {
    return "\
These are the currently active coordinate offsets,\n\
which give the position of the program origin\n\
relative to the machine origin.\n\n\
Right-clicking on an axis name or position and\n\
entering a value will change the offset so that\n\
the value you enter will be the current position."
}
