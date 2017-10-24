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


def create_tool_change_routine(self):
	# Subroutine from orangecat
	subroutine = os.path.expanduser("~/linuxcnc/configs/%s/%s" % (self.d.machinename, FILE_TOOL_CHANGE))
	fngc = open(subroutine, "w")
	print >>fngc, ("""
O<tool-change> SUB
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

(------------------------------- CONFIGURATION PARAMETERS ----------------------------------------------)
#<_UseInches> =           0     ( set to 1 to use inches here, or 0 to use millimeters; should match units on tool.tbl dimensions )
#<_TravelZ> =          41.0     ( machine Z coordinate for travelling, typically near max Z to avoid ever hitting the work )
#<_TravelFeed> =     1000.0     ( feedrate used for general Z moves when avoiding G0 )
#<_ProbeX> =          145.0     ( machine X coordinate of switch/touch-off plate )
#<_ProbeY> =            0.0     ( machine Y coordinate of switch/touch-off plate )
#<_ProbeFastZ> =        5.0     ( machine Z coord to move to before starting probe, longest tool should not touch switch at this Z )
#<_ProbeMinZ> =       -37.0     ( machine Z coord to stop probe, shortest tool must touch switch at this Z, must be > min Z )
#<_ProbeRetract> =      1.5     ( small distance to retract before approaching switch/touch-off plate second time )
#<_ProbeFastFeed> =   400.0     ( feed rate for moving to _ProbeFastZ )
#<_ProbeFeed1> =       80.0     ( feed rate for touching switch/touch-off plate first time )
#<_ProbeFeed2> =       10.0     ( feed rate for touching switch/touch-off plate second time )
#<_ToolChangeX> =       0.0     ( machine X coordinate to pause at for manual tool changing )
#<_ToolChangeY> =     -50.0     ( machine Y coordinate to pause at for manual tool changing )
#<_MistOnDuringProbe> =   1     ( set to 1 for mist, or 2 for coolant, or 0 for nothing during probing, to clear switch of swarf )
(-------------------------------------------------------------------------------------------------------)

O100 IF [ EXISTS[#<_ToolDidFirst>] EQ 0 ]
        #<_ToolDidFirst> = 0
O100 ENDIF
O105 IF [ #<_ToolDidFirst> EQ 0 ]
   G49                                         ( clear tool length compensation prior to saving state if this is first time )
O105 ENDIF

M6                                             ( do the normal M6 stuff )
M70                                            ( save current modal state )

M9                                             ( turn off coolant, will be restored on return if it was on )
M5                                             ( turn off spindle, cannot be on during the probe )
G[21 - #<_UseInches>]                          ( use inches or millimeters as required here, units will be restored on return )
G30.1                                          ( save current position in #5181-#5183... )
G49                                            ( clear tool length compensation )
G90                                            ( use absolute positioning here )
G94                                            ( use feedrate in units/min )
G40                                            ( turn cutter radius compensation off here )

O200 IF [ #<_ToolDidFirst> EQ 0 ]
  G53 G1 F[#<_TravelFeed>] Z[#<_TravelZ>]      ( go to high travel level on Z )
  G53 G0 X[#<_ProbeX>] Y[#<_ProbeY>]           ( to probe switch )
  G53 G1 F[#<_ProbeFastFeed>] Z[#<_ProbeFastZ>]( move tool closer to switch -- we shouldn't hit it )
  G54 G1 F[#<_ProbeFeed1>] G91                 ( use relative positioning )
  O101 IF [ #<_MistOnDuringProbe> EQ 1 OR #<_MistOnDuringProbe> EQ 2 ]
    M[7 + #<_MistOnDuringProbe> - 1]           ( turn on mist/coolant )
  O101 ENDIF
  G38.2 Z[#<_ProbeMinZ> - #<_ProbeFastZ>] F[#<_ProbeFeed1>]    ( trip switch slowly )
  G0 Z[#<_ProbeRetract>]                       ( go up slightly )
  G38.2 Z[#<_ProbeRetract>*-1.25] F[#<_ProbeFeed2>]   ( trip switch very slowly )
  M9                                           ( turn off mist )
  G90                                          ( use absolute positioning )
  #<_ToolZRef> = #5063                         ( save trip point )
  #<_ToolZLast> = #<_ToolZRef>                 ( save last tool Z position )
  G53 G1 F[#<_TravelFeed>] Z[#<_TravelZ>]      ( return to safe level )
  G53 G0 X[#5181] Y[#5182]                     ( return to where we were in X Y)
  G53 G1 F[#<_TravelFeed>] Z[#5183]            ( return to where we were in Z )
  M72                                          ( restore modal state )
  #<_ToolDidFirst> = 1                         ( we have been in this section to set reference value already )
O200 ELSE
  G53 G1 F[#<_TravelFeed>] Z[#<_TravelZ>]      ( go to high travel level on Z )
  G53 G0 X[#<_ToolChangeX>] Y[#<_ToolChangeY>] ( nice place for changing tool )
  O107 IF [#<_UseInches> EQ 1 ]
    #<ToolDiamIn> = #5410
    #<ToolDiamMM> = [ #<ToolDiamIn> * 25.4 ]
  O107 ELSE
    #<ToolDiamMM> = #5410
    #<ToolDiamIn> = [ #<ToolDiamMM> / 25.4 ]
  O107 ENDIF
  O102 IF [ #<_current_tool> EQ 0 AND #<ToolDiamIn> EQ 0 ]
    (MSG, Change tool then click Resume )
  O102 ELSE
    #<ToolDiamMM> = [ #<ToolDiamIn> * 25.4 ]
    (DEBUG, Change to tool #<_current_tool> with diameter #<ToolDiamMM>mm, #<ToolDiamIn>in then click Resume )
  O102 ENDIF
  M0                                           ( pause execution )
 
  G53 G0 X[#<_ProbeX>] Y[#<_ProbeY>]           ( to high place directly over switch )
  G53 G1 F[#<_ProbeFastFeed>] Z[#<_ProbeFastZ>]( move tool closer to switch -- we shouldn't hit it )
  G54 G1 F[#<_ProbeFeed1>] G91                 ( use relative positioning )
  O103 IF [ #<_MistOnDuringProbe> EQ 1 OR #<_MistOnDuringProbe> EQ 2 ]
    M[7 + #<_MistOnDuringProbe> - 1]           ( turn on mist/coolant )
  O103 ENDIF
  G38.2 Z[#<_ProbeMinZ> - #<_ProbeFastZ>] F[#<_ProbeFeed1>]     ( trip switch slowly )
  G0 Z[#<_ProbeRetract>]                       ( go up slightly )
  G38.2 Z[#<_ProbeRetract>*-1.25] F[#<_ProbeFeed2>]   ( trip switch very slowly )
  M9                                           ( turn off mist )
  G90                                          ( use absolute positioning )
  #<_ToolZ> = #5063                            ( save new tool length )
  G43.1 Z[#<_ToolZ> - #<_ToolZRef>]            ( set new tool length Z offset, we do this now to show operator even though it has to be set again after M72 )
  G53 G1 F[#<_TravelFeed>] Z[#<_TravelZ>]      ( return to high travel level )
  G53 G0 X[#5181] Y[#5182]                     ( return to where we were in X Y)
  G53 G1 F[#<_TravelFeed>] Z[#5183 - #<_ToolZLast> + #<_ToolZ>]   ( return to where we were in Z, ajusting for tool length change )
  #<_ToolZLast> = #<_ToolZ>                    ( save last tool length )
  
  M72                                          ( restore modal state )
  G43.1 Z[#<_ToolZ> - #<_ToolZRef>]            ( set new tool length Z offset )
O200 ENDIF

O<tool-change> ENDSUB
M2""")
	fngc.close()
	# END

def create_tool_job_begin_routine(self):
	# Subroutine from orangecat
	subroutine = os.path.expanduser("~/linuxcnc/configs/%s/%s" % (self.d.machinename, FILE_TOOL_JOB_BEGIN))
	fngc = open(subroutine, "w")
	print >>fngc, ("""
O<tool-job-begin> SUB
( Filename: tool-job-begin.ngc )
( LinuxCNC Manual Tool-Change Subroutines for Milling Machines version 1.1: subroutine 2/2 )
( Intended to be run as a remapped M600 command. Used to indicate that the next tool change, M6, is the first tool of a job. )

#<_ToolDidFirst> = 0                         ( new job, we haven't yet called <tool-change> for the first time )
G49                                          ( clear tool height adjustment )

O<tool-job-begin> ENDSUB
M2""")
	fngc.close()
	# END
