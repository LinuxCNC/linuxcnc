#!/usr/bin/env python
#
#    This is stepconf, a graphical configuration editor for LinuxCNC
#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#*************
# TOOL CHANGE
#*************
import shutil
import os
import errno
import xml.dom.minidom
import commands
from gi.repository import Gtk
from stepconf.definitions import *

def create_simple_probe_routine(self):
	# Used with graphical interface
	subroutine = os.path.expanduser("~/linuxcnc/configs/%s/%s" % (self.d.machinename, "probe_tool_lenght.ngc"))
	fngc = open(subroutine, "w")
	print >>fngc, ("""o<probe_tool_lenght> sub
(Set Z Zero for G54 coordinate)""")
	print >>fngc, ("G90")
	print >>fngc, ("G53 G0 Z%d" % (self.d.zmaxlim -1))
	print >>fngc, ("	G53 G0 X%d Y%d" % (self.d.probe_x_pos, self.d.probe_y_pos))
	print >>fngc, ("	G53 G0 Z%d" % (self.d.probe_z_pos))
	print >>fngc, ("""	G49 (Delete any reference)
G38.2 Z0 F200
G91 G0 Z1 (Off the switch)
(try again at lower speed)
G90
G38.2 Z0 F15""")
	#print >>fngc, ("	(reference length is #1000)")
	#print >>fngc, ("	#1000=#5063 (save reference tool length)")
	#print >>fngc, ("G10 L20 P1 Z[#1000]")
	print >>fngc, ("G10 L20 P1 Z[%s] (set G54 Z0 under probe switch)" % (self.d.probe_sensor_height))
	print >>fngc, ("G91 G0 Z1 (Off the switch)")
	print >>fngc, ("G90 (done)")
	print >>fngc, ("G53 G0 Z%d" % (self.d.zmaxlim -1))
	print >>fngc, ("o<probe_tool_lenght> endsub")
	fngc.close()


def create_tool_change_routine(self):
	subroutine = os.path.expanduser("~/linuxcnc/configs/%s/%s%s" % (self.d.machinename, FILE_TOOL_CHANGE, ".ngc"))
	fngc = open(subroutine, "w")
	print >>fngc, ("""
O<tool_change> SUB
( Filename: tool-change.ngc )
( LinuxCNC Manual Tool-Change Subroutines for Milling Machines version 1.1: subroutine 1/2 )
(  BEFORE USING CHANGE "CONFIGURATION PARAMETERS" BELOW FOR YOUR MACHINE! )
(  )
( In the LinuxCNC .ini config file, under the [RS274NGC] section add: )
(    # change/add/use SUBROUTINE_PATH to point to the location where these tool-change subroutines are located: )
(    SUBROUTINE_PATH = /home/linuxcnc/linuxcnc/nc_files )
(    REMAP=M6    modalgroup=6 ngc=tool-change )
(    REMAP=M600  modalgroup=6 ngc=tool-job-begin )
( and under the [EMCIO] section add: )
(    TOOL_CHANGE_AT_G30 = 0 )
( and ensure neither TOOL_CHANGE_POSITION nor TOOL_CHANGE_QUILL_UP is set. )
(  )
( In the LinuxCNC .hal config file, map some input pin to be the probe input, e.g.: )
(    net probe-z parport.0.pin-12-in => motion.probe-input )
(  )
( Usage: M6 in the g-code will invoke a manual tool change with automatic tool height adjustment. )
(        M600 is used at the beginning of the first g-code file of a job so that the next M6 will measure the tool for reference )
(             instead of caluculating a tool length offset. It can also be invoked manually through the MDI before a job starts. )
(  )
( General theory of operation: touches each tool off to the tool height sensor. The first tool is used as the reference, all )
(     subsequent tools adjust the tool offset. The tip of the tool is always placed back at the position it started in before )
(     any of the subroutines are called. It is moved away by raising Z to _TravelZ before moving towards the switch, and when )
(     moving back from the switch again moves at height _TravelZ before going straight back down to the original position. Set )
(     all necessary modes to ensure correct operation no matter what state the program is in when this is called. We eliminate )
(     almost all side effects by saving and restoring the modal state. )
(  )
( Side effects: sets G30, sets motion mode to G1. )

(------------------------------- CONFIGURATION PARAMETERS ----------------------------------------------)""")
	if(self.d.units == INCH):
		units = 1
	else:
		units = 0
	print >>fngc, "#<_UseInches> =           %d     ( set to 1 to use inches here, or 0 to use millimeters; should match units on tool.tbl dimensions )" % units
	print >>fngc, "#<_TravelZ> =          %.2f     ( machine Z coordinate for travelling, typically near max Z to avoid ever hitting the work )" % (self.d.zmaxlim -1)
	print >>fngc, "#<_TravelFeed> =     %.2f     ( feedrate used for general Z moves when avoiding G0 )" % 1000.0
	print >>fngc, "#<_ProbeX> =          %.2f     ( machine X coordinate of switch/touch-off plate )" % self.d.probe_x_pos
	print >>fngc, "#<_ProbeY> =            %.2f    ( machine Y coordinate of switch/touch-off plate )" % self.d.probe_y_pos
	print >>fngc, "#<_ProbeFastZ> =        %.2f     ( machine Z coord to move to before starting probe, longest tool should not touch switch at this Z )" % self.d.probe_z_pos
	print >>fngc, "#<_ProbeMinZ> =       %.2f     ( machine Z coord to stop probe, shortest tool must touch switch at this Z, must be > min Z )" % (self.d.probe_sensor_height + 10)
	print >>fngc, "#<_ProbeRetract> =      %.2f     ( small distance to retract before approaching switch/touch-off plate second time )" % 1.5
	print >>fngc, "#<_ProbeFastFeed> =   %.2f     ( feed rate for moving to _ProbeFastZ )" % 400.0
	print >>fngc, "#<_ProbeFeed1> =       %.2f     ( feed rate for touching switch/touch-off plate first time )" % 200.0 
	print >>fngc, "#<_ProbeFeed2> =       %.2f     ( feed rate for touching switch/touch-off plate second time )" % 15.0 
	print >>fngc, "#<_ToolChangeX> =       %.2f     ( machine X coordinate to pause at for manual tool changing )" % self.d.probe_x_pos
	print >>fngc, "#<_ToolChangeY> =     %.2f     ( machine Y coordinate to pause at for manual tool changing )" % self.d.probe_y_pos
	print >>fngc, "#<_MistOnDuringProbe> =   %d     ( set to 1 for mist, or 2 for coolant, or 0 for nothing during probing, to clear switch of swarf )" % 1

	#print >>fngc, ("(DEBUG, 5183 #5183, 5063 #5063, 5223 #5223)")

	print >>fngc, ("""
(-------------------------------------------------------------------------------------------------------)
O100 IF [ EXISTS[#<_ToolDidFirst>] EQ 0 ]
	#<_ToolDidFirst> = 0
O100 ENDIF
O105 IF [ #<_ToolDidFirst> EQ 0 ]
	G49                                         ( clear tool length compensation prior to saving state if this is first time )
O105 ENDIF

M6                                             ( do the normal M6 stuff )
O107 IF [#<_UseInches> EQ 1 ]
	#<ToolDiamIn> = #5410
	#<ToolDiamMM> = [ #<ToolDiamIn> * 25.4 ]
O107 ELSE
	#<ToolDiamMM> = #5410
	#<ToolDiamIn> = [ #<ToolDiamMM> / 25.4 ]
O107 ENDIF

O210 IF [ EXISTS[#<_lastTool>] EQ 0 ]
	#<_lastTool> = -1                           ( return 0 if _OldTool doesn't exists )
O210 ENDIF

O211 IF [ #<_lastTool> EQ #<_current_tool> ]
	(DEBUG, current tool #<_current_tool> same as last tool #<_lastTool>)
	G4 P2                                        ( Wait 2 seconds for spindle restart )
	O211 return                                  ( do not request tool length check if is the same tool )
O211 ENDIF
#<_lastTool> = #<_current_tool>                  ( Save current tool number )

M70                                            ( save current modal state )

M9                                             ( turn off coolant, will be restored on return if it was on )
M5                                             ( turn off spindle, cannot be on during the probe )
G[21 - #<_UseInches>]                          ( use inches or millimeters as required here, units will be restored on return )
G30.1                                          ( save current position in #5181-#5183... )
G49                                            ( clear tool length compensation )
G90                                            ( use absolute positioning here )
G94                                            ( use feedrate in units/min )
G40                                            ( turn cutter radius compensation off here )

G53 G1 F[#<_TravelFeed>] Z[#<_TravelZ>]      ( go to high travel level on Z )
G53 G0 X[#<_ProbeX>] Y[#<_ProbeY>]           ( to probe switch )
O102 IF [ #<_current_tool> EQ 0 AND #<ToolDiamIn> EQ 0 ]
	(MSG, Change tool then click Resume )
O102 ELSE
	O108 IF [#<_UseInches> EQ 1 ]
		(DEBUG, Change to tool #<_current_tool> with diameter #<ToolDiamIn>inches then click Resume )
	O108 ELSE
		(DEBUG, Change to tool #<_current_tool> with diameter #<ToolDiamMM>mm then click Resume )
	O108 ENDIF
O102 ENDIF
M0                                           ( pause execution )

G53 G1 F[#<_ProbeFastFeed>] Z[#<_ProbeFastZ>]( move tool closer to switch -- we shouldn't hit it )
G54 G1 F[#<_ProbeFeed1>]
G91                 ( use relative positioning )
O101 IF [ #<_MistOnDuringProbe> EQ 1 OR #<_MistOnDuringProbe> EQ 2 ]
	M[7 + #<_MistOnDuringProbe> - 1]           ( turn on mist/coolant )
O101 ENDIF

G38.2 Z[#<_ProbeMinZ> - #<_ProbeFastZ>] F[#<_ProbeFeed1>]    ( trip switch slowly )
G0 Z[#<_ProbeRetract>]                       ( go up slightly )
G38.2 Z[#<_ProbeRetract>*-1.25] F[#<_ProbeFeed2>]   ( trip switch very slowly )""")
	print >>fngc, ("G10 L20 P1 Z[%s] (set G54 Z0 under probe switch)" % (self.d.probe_sensor_height))
	print >>fngc, ("O200 IF [ #<_ToolDidFirst> EQ 0 ]")
	#print >>fngc, ("	#<_ToolZRef> = #5063                         ( save trip point )")
	print >>fngc, ("	#<_ToolZRef> = #5223                         ( save trip point )")
	print >>fngc, ("""
	#<_ToolZLast> = #<_ToolZRef>                 ( save last tool Z position )
	#<_ToolDidFirst> = 1                         ( we have been in this section to set reference value already )
""")
	print >>fngc, ("O200 ELSE")
	#print >>fngc, ("	#<_ToolZRef> = #5063                         ( save trip point )")
	print >>fngc, ("	#<_ToolZRef> = #5223                         ( save trip point )")
	print >>fngc, ("""
O200 ENDIF
#<_ToolZDiff> = [#<_ToolZLast> - #<_ToolZRef>]
""")
	#print >>fngc, ("(DEBUG, ToolZRef> #<_ToolZRef>, ToolZLast #<_ToolZLast>, DIFF #<_ToolZDiff>, 5183 #5183, 5063 #5063)")
	print >>fngc, ("""M9                                           ( turn off mist )
G90                                          ( use absolute positioning )
G53 G1 F[#<_TravelFeed>] Z[#<_TravelZ>]      ( return to safe level )
G53 G0 X[#5181] Y[#5182]                     ( return to where we were in X Y)""")
	#print >>fngc, ("G53 G1 F[#<_TravelFeed>] Z[#5183 - #<_ToolZDiff>]   ( return to where we were in Z, ajusting for tool length change )")
	print >>fngc, ("G53 G1 F[#<_TravelFeed>] Z[#5183 - #<_ToolZDiff>]   ( return to where we were in Z, ajusting for tool length change )")
	print >>fngc, ("#<_ToolZLast> = #<_ToolZRef>                 ( save last tool Z position )")
	print >>fngc, ("""
M72                                          ( restore modal state )
O<tool-change> ENDSUB
M2
%""")
	fngc.close()


def create_tool_job_begin_routine(self):
	# Subroutine from orangecat
	subroutine = os.path.expanduser("~/linuxcnc/configs/%s/%s%s" % (self.d.machinename, FILE_TOOL_JOB_BEGIN, ".ngc"))
	fngc = open(subroutine, "w")
	print >>fngc, ("""
O<tool_job_begin> SUB
( Filename: tool-job-begin.ngc )
( LinuxCNC Manual Tool-Change Subroutines for Milling Machines version 1.1: subroutine 2/2 )
( Intended to be run as a remapped M600 command. Used to indicate that the next tool change, M6, is the first tool of a job. )

#<_ToolDidFirst> = 0                         ( new job, we haven't yet called <tool-change> for the first time )
G49                                          ( clear tool height adjustment )

O<tool-job-begin> ENDSUB
M2
""")
	fngc.close()
	# END
