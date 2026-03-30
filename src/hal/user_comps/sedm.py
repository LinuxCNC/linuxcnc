#!/usr/bin/env python3
#
#26.03.2026 clean comments and doclines
#
#*
#* This library is free software; you can redistribute it and/or modify it
#* under the terms of the GNU Lesser General Public License as published
#* by the Free Software Foundation; either version 2 of the License, or
#* (at your option) any later version.
#*
#* This library is distributed in the hope that it will be useful, but
#* WITHOUT ANY WARRANTY; without even the implied warranty of
#* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#* Lesser General Public License for more details.
#*
#* You should have received a copy of the GNU Lesser General Public
#* License along with this library; if not, write to the Free Software
#* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301-1307
#* USA.
#*

import linuxcnc
import hal
import time
import sys, subprocess
import os.path
from random import uniform

# 23.03.2026 no more headers file \sedmhdrs.py'
"""
# 23.03.2026 build pkgs failed
# I think >I< put this file where a std build would find it
# but a pkg build may not
# The prpose of this file was to isolate a lot of 'equares'
# but because Python has no .h/inmclude/or hgeader files,
# I made it a python file accessible by 'import'
# That trick might make build pkgs or some rule break/fail.
# I will put thi inside the top[ of the sedm.py comp srrc file
# 
#
# OLD NOTES  sedmhdrs.py
# resides in /usr/lib/python3.11
#   or       ~/yourRIP/lib/python
#
"""
# beg--------- CONSTANTS --------

# JumpLtypes The detour path varies with jumptype
#  sometime a single line, and sometimes 3 with and angle between
JumpBoreType      = 1 # use part of sexisyting BoreL
JumpStairsType    = 2 # create a leg shape beg at BP end at BP
JumpOrbLeadInType = 3 #use section of existing Legshape
JumpOrbPathType   = 4 #create dwtour wqith legshape 
#JumpOrbPathDetour = 5 #NO dont jump , thertes only 1 posn where there may be stock, else its just backaway to clear low V
NoJump = 10 # avoid 0 for unitialized cars

# motion directions 
FWD = +1
HOLD = 0
BWD = -1

# how many consecutive BWD evaluations before ABORT
ContBWDmax = 20 # maxximum continouous   low voltage sample, causes ABORT

#02.09.2025 i  want to set GVMAX and GV<IN using this vv data . so it innn Edm3 comp
#  since pin and  'constant' had same anmes, i added uscore to constant
GVMIN = 0
GVMAX = 100

#24.03.2026 dx not needed as global
#ndx = 0 # need this global or pass, global may be correct here

NEG = -1
POS = +1

CIRCLE = 0
SQUARE = 1

# ---- states of state machine ----
# WaitFullDepthMsrdUNS     = 1
WaitFullDepthRplaneDist  = 1
WaitOrbitTypeWiglRADf    = 2
WaitEttabNumberMsrdUNS   = 3
WaitBegEndNR             = 4
WaitPitch                = 5
WaitJumpwANTED           = 6
WaitGenReady             = 7
WaitM199                 = 8
# no 9 no 10 no good reason
WaitPlunge               = 11
WaitDoPlungeOrbit        = 12
WaitAllNRsDone           = 13
WaitThisOrbitDone        = 14
WaitNewNR                = 15
CleanUpPutAway           = 16
# ------ end states of state macghine

# ----- end of constants -------

def mkStairsJumpLegL( bP ):
	"""
	purpose:
	 Creates Jaeay and Jback lines for jump ( peck EDM)\
	when cutting a bore wiyh WoglRAD >0AND JumpENA checked.
	
	usage: pass a Tuple descibing breakPt
	 measured drom Start Point Origin (SPO)
	 
	description:
	 The jump path needs to be calculated
	on the fly. Powe is off during jump,
	so delays are not dangerous
	( and Python is fast)
	 The Jump 'master' path is a line from
	breakPt to anklePt to SPO.
	 The 'master' line is like a leg,
	it has a toe, a foot line , 
	an ankle, a shin line and a hip (SPO)
	 The masterr path is sliced into 2 mini-paths.
	  1) Jaway: from btrakPt
	   (tool position on periphery), 
	   to the bore centerline ( anklePt)
	   tp Start Point Origin (SPO)
	   Slicing the 'amster' path
	   at the AJD distance, 
	   craetes a 'hiPt' ( top of jump)
	  2) Jback: from hiPt to anklePt
	    back to breakPt ( aka 'toe')
	
	 The length of the jump is initialized
	to the  Etabl's AJD for the prsent
	power stage (ThisNR) 
	 AJD can be adjusted during cut.
	 Jump cycles alwys return to breakPt.
	 Jump cycle move without respect to 
	gap sensing.
	 A small AJD may not travel to anklePt.
	 An early jump may be clipped to SPO
	 or even onto the dootL.
	"""
	SPO = (0,0,0) #StartPtOffset nit Posn
	#
	WiglRADi = int(round(sedm.WiglRADf / sedm.xyzSCALEfOUT))
	# get ctrLevel, above breakPt by ThisRA
	ctrLevel = bP[sedm.ToolAxis]  - (WiglRADi * sedm.CutDir) 
	# get anklePt
	if sedm.ToolAxis == 2: # Z
			anklePt = ( ( 0,0,ctrLevel ) )
	if sedm.ToolAxis == 1: # Y
			anklePt = ( ( 0,  ctrLevel, 0 ) )
	if sedm.ToolAxis == 0: # X   13.02.2026 damn had = 2 and spent a day
			anklePt = ( ( ctrLevel, 0, 0 ) )
	footL = L3D(bP, anklePt)
	lFL = len(footL)-1
	ankleNdx = footL.index(anklePt)
	shinL = L3D(anklePt,SPO)
	footl = footL[:ankleNdx] # slice off anklPt from footL, 
	legL = footL + shinL
	return legL,ankleNdx
#
def mkJupLJdnL( L, bPndx):
	#
	if sedm.JumpLtype == JumpBoreType:
		AJDi = int(sedm.AJD *1000)
		#
		bPtupl = L[bPndx]
		hiPtNdx = bPndx - AJDi
		if hiPtNdx < 0: # clip to 1st eleme ndx
			hiPtNdx = 0
		hiPtTupl = L[hiPtNdx]
		#
		Jback = L # use not reveresed XDList
		FirstDnNdx = hiPtNdx +1 # for Jdn
		#
		Jback = Jback[FirstDnNdx : bPndx+1] #+1 elese clipped
		#
		Jaway = []
		for d in L:
			Jaway.append(d)
		Jaway.reverse()# list goes UP from [0] to [last]
		#
		firstUpNdx = Jaway.index(bPtupl) + 1
		hiPtUpNdx  = Jaway.index(hiPtTupl)
		#
		Jaway = Jaway[firstUpNdx : hiPtUpNdx + 1] # +1 else last missed
		#
		
		return Jaway, Jback
		#
	if sedm.JumpLtype == JumpStairsType:
		#            0.050  *1000  = 50
		AJDi = int(sedm.AJD * 1000)
		
		bPtupl = L[bPndx]
		JL, ankleNdx = mkStairsJumpLegL( bPtupl )   # ignore ankleNdx
		#
		# now make HupL and JdnL fdrom JL 
		#
		lJL = len(JL) - 1
		if AJDi > lJL :# ex if 50 > len(JL)-1
			AJDi = lJL# dore stairs, 1st jump >>>
		hiPtNdx = AJDi
		hiPtTupl = JL[hiPtNdx]# will use later to find inex in rev'd list
		#
		Jaway = JL[:hiPtNdx]
		#
		Jback = JL # usecopy of orig
		Jback.reverse() # this list is from hip [0[ to toe [last]
		hiPtNdx = JL.index(hiPtTupl)
		Jback = Jback[hiPtNdx:]
		
		return Jaway, Jback
	#
	if sedm.JumpLtype == JumpOrbLeadInType:
		#13.02.2026 TODO reduce   same as BoreL
		
		AJDi = int(sedm.AJD *1000)
		#
		bPtupl = L[bPndx]
		hiPtNdx = bPndx - AJDi
		if hiPtNdx < 0: # clip to 1st eleme ndx
			hiPtNdx = 0
		hiPtTupl = L[hiPtNdx]
		#
		Jback = L # use not reveresed XDList
		FirstDnNdx = hiPtNdx +1 # for Jdn
		#
		Jback = Jback[FirstDnNdx : bPndx+1] #+1 elese clipped
		#
		Jaway = []
		for d in L:
			Jaway.append(d)
		Jaway.reverse()# list goes UP from [0] to [last]
		#
		firstUpNdx = Jaway.index(bPtupl) + 1
		hiPtUpNdx  = Jaway.index(hiPtTupl)
		#
		Jaway = Jaway[firstUpNdx : hiPtUpNdx + 1] # +1 else last missed
		
		return Jaway, Jback
	#
	if sedm.JumpLtype == JumpOrbPathType:
		#
		AJDi = int(sedm.AJD *1000)
		bPtupl = L[bPndx]
		JL, ankleNdx = mkDetourLegL( L[bPndx] )   # ignore ankleNdx
		#
		lJL = len(JL) -1
		if AJDi > lJL :
			AJDi = lJL  # 13.02.2026 damnit had AJD>>I<<
		hiPtNdx = AJDi
		
		hiPtTupl = JL[hiPtNdx]# will use later to find inex in rev'd list
		#
		Jaway = JL[:hiPtNdx]
		#
		Jback = JL # usecopy of orig
		Jback.reverse() # this list is from hip [0[ to toe [last]
		# vvv hiPtNdx chgs becux liest is reveresed now
		hiPtNdx = JL.index(hiPtTupl)
		Jback = Jback[hiPtNdx:]
		return Jaway, Jback
	#
	if sedm/JumpLtype == NoJump:
		print(357,"dont call mkJupLJdnL) wityh NoJimp")
		trap()
#
# vvv rqrs JumpL and doMove()
def doJump(JupL,JdnL):
	"""
	moves tool away from stock
	for the distance of AJD
	then returns
	"""
	#
	for p in JupL:	#jump up
		doMove(p)
	#
	for p in JdnL:
		doMove(p)
#
#05.02.2026 TODO small enuf to so inline
def mkJT(): # sets sedm.endJT
	"""
	creates float 
	repressenting when next Jump 
	should ha[[en
	"""
	Tnow = time.time()
	# vvv these are all floats
	sedm.endJT = Tnow + sedm.ET
#
#05.02.2026 TODO small enuf to so inline
def chkJT(): # run this every loop while cuttinmg
	"""
	for Jump cycles:
	reports if Cut Time is over
	and it is time to Jump
	"""
	Tnow = time.time()
	timesUp = (Tnow >= sedm.endJT)
	return timesUp
#
def IsFreebie(posn): # posn is an x y z tuplke
	"""
	 IsFreebie decide if teh gap value cam be ignored
	esp at beginning of cuty, near SPO.
	 Freebies are needed for simulation.
	 When a real EDM cut runs,
	there initially is some clearance between tool and work.
	 So a real Gap value would be high for that
	distance.
	 In simulation, the gap is a random
	value and _might_ be low.
	 This low woul;d cause teh simulation
	to retreat, and that is a problem
	brcause there is no place to retreat.
	 So, do simulation, I added a cube shaped zone
	at th Start Point.
	 The zone is 2*Freebies in size,
	and any motion in that zone ihnores
	the gap value PV.
	 Freebies are set in NOTheaders.py
	"""
	
	f =  sedm.freebies # avoid looking it up 3x
	if   abs(posn[0]) > f:
		return False
	elif abs(posn[1])> f:
		return False
	elif abs(posn[2]) > f:
		return  False
	else:
		return True
#
# 14.02.2026 TODO likely there are unused/refundamt pis
#   clean them out
def mksedmcomp():
	"""
	creates the LinuxCNC hal component
	for orbiting sink EDM.
	"""
	h = hal.component("sedm")
	#
	h.newpin("EDMgrade",     hal.HAL_S32,   hal.HAL_OUT)
	h.newpin("EDpeek",     hal.HAL_BIT,   hal.HAL_IN)
	
	h.newpin("EDreturn",   hal.HAL_BIT,   hal.HAL_IN)
	h.newpin("EDquit",     hal.HAL_BIT,   hal.HAL_IN)
	h.newpin("QuitHit",     hal.HAL_BIT,   hal.HAL_OUT)
	#08.12.2025 new
	h.newpin("BwdMaxHit",     hal.HAL_BIT,   hal.HAL_OUT)
	
	# 29.11.2025 3 new pins to force FWD HOLD ForceBWD
	h.newpin("ForceFWD",     hal.HAL_BIT,   hal.HAL_IN)
	h.newpin("ForceHOLD",    hal.HAL_BIT,   hal.HAL_IN)
	h.newpin("ForceBWD",     hal.HAL_BIT,   hal.HAL_IN)
	
	h.newpin("XOffsetCmd",     hal.HAL_S32,   hal.HAL_OUT)
	h.newpin("YOffsetCmd",     hal.HAL_S32,   hal.HAL_OUT)
	h.newpin("ZOffsetCmd",     hal.HAL_S32,   hal.HAL_OUT)
	#
	h.newpin("isEna",     hal.HAL_BIT,   hal.HAL_IN)
	#
	h.newpin("xyzSCALEfIN",  hal.HAL_FLOAT,   hal.HAL_IN)
	h.newpin("xyzSCALEfOUT", hal.HAL_FLOAT,   hal.HAL_OUT)
	h.newpin("mlt",          hal.HAL_FLOAT,   hal.HAL_OUT)
	#
	h.newpin("xFBf",     hal.HAL_FLOAT,   hal.HAL_IN)
	h.newpin("yFBf",     hal.HAL_FLOAT,   hal.HAL_IN)
	h.newpin("zFBf",     hal.HAL_FLOAT,   hal.HAL_IN)
	#
	h.newpin("xsp",     hal.HAL_FLOAT,   hal.HAL_OUT)
	h.newpin("ysp",     hal.HAL_FLOAT,   hal.HAL_OUT)
	h.newpin("zsp",     hal.HAL_FLOAT,   hal.HAL_OUT)
	#
	h.newpin("ctr",     hal.HAL_U32,   hal.HAL_OUT)
	#
	h.newpin("inpo",     hal.HAL_BIT,   hal.HAL_IN)
	#
	h.newpin("XoffsetNow",     hal.HAL_S32,   hal.HAL_IN)
	h.newpin("YoffsetNow",     hal.HAL_S32,   hal.HAL_IN)
	h.newpin("ZoffsetNow",     hal.HAL_S32,   hal.HAL_IN)
	#
	h.newpin("state",        hal.HAL_U32,   hal.HAL_OUT)
	#
	h.newpin("PgmStop",      hal.HAL_BIT,   hal.HAL_OUT)
	#
	h.newpin("disableOsc",       hal.HAL_BIT,   hal.HAL_OUT)
	
	h.newpin("NR",           hal.HAL_U32,   hal.HAL_OUT) 
	# the etab file is a flat database with numbered lines of 
	# data NR is the line number key into a dict
	h.newpin("IP",           hal.HAL_U32,   hal.HAL_OUT)
	h.newpin("P",            hal.HAL_U32,   hal.HAL_OUT)
	h.newpin("HV",           hal.HAL_U32,   hal.HAL_OUT)
	
	h.newpin("GVHI",         hal.HAL_FLOAT,   hal.HAL_OUT)
	h.newpin("GVLO",         hal.HAL_FLOAT,   hal.HAL_OUT)
	
	h.newpin("TON",              hal.HAL_FLOAT,   hal.HAL_OUT)
	h.newpin("TOF",              hal.HAL_FLOAT,   hal.HAL_OUT)
	
	h.newpin("ISO",          hal.HAL_U32,   hal.HAL_OUT)
	# if True,
	#  ON time is dev'd AFTER ionization ( makes uniform craters)
	h.newpin("AJD",          hal.HAL_FLOAT, hal.HAL_OUT)
	h.newpin("ET",           hal.HAL_FLOAT, hal.HAL_OUT)

	#new 01.02.2026 the sys time when next jump occurs
	h.newpin("endJT",           hal.HAL_FLOAT, hal.HAL_OUT)
	# 06.02.2026 new pin JumpLtype
	# defines in sedmhdrs.py
	h.newpin("JumpLtype",           hal.HAL_U32, hal.HAL_IN)
	# vvv new 01.02.2026, cnx to gui checkbutton
	h.newpin("JumpENA",         hal.HAL_BIT, hal.HAL_IN)
	# Jump rqrs ENA and ON
	h.newpin("JumpOn",         hal.HAL_BIT, hal.HAL_OUT)
	
	h.newpin("ChgJumpENA",      hal.HAL_BIT, hal.HAL_OUT)
	
	h.newpin("BWDmax",       hal.HAL_U32,   hal.HAL_OUT)
	# max continuous LoVoltage samples, will stop vut
	#15.02.2026 not used h.newpin("PulseCount",   hal.HAL_U32,   hal.HAL_IN)#
	h.newpin("freebies",      hal.HAL_U32,   hal.HAL_IN)#
	#
	h.newpin("RADf",          hal.HAL_FLOAT, hal.HAL_OUT)
	# orbital radius
	h.newpin("RADi",          hal.HAL_FLOAT, hal.HAL_OUT)
	# rad as ounts
	
	#vvv nerw 28.11.2025 alc 1x and save it for later
	h.newpin("XtraRADf",          hal.HAL_FLOAT, hal.HAL_IN)#

	#vvv err when U32 and initd/reinitydx to -1
	h.newpin("Pitch",       hal.HAL_S32,   hal.HAL_IN)
	# defines a ation of pattern elemnent to toolaxis steps. 
	# like rise & run on stairs
	
	# wthere AJD and ET are used, can have overriding switch
	h.newpin("SPAf",          hal.HAL_FLOAT, hal.HAL_OUT)
	h.newpin("VDIf",          hal.HAL_FLOAT, hal.HAL_OUT)
	h.newpin("VEf",           hal.HAL_FLOAT, hal.HAL_OUT)
	h.newpin("VWf",           hal.HAL_FLOAT,   hal.HAL_OUT)
	#
	# 14.11.2025 new vvv used in egtEvalPV()
	h.newpin("BWDcount",       hal.HAL_U32,   hal.HAL_OUT)
	#
	h.newpin("EtabNum",      hal.HAL_U32,    hal.HAL_IN)# filename , is a number as in old Heidnhain days
	h.newpin("ThisNR",       hal.HAL_S32,    hal.HAL_OUT)   
	h.newpin("BegNR",        hal.HAL_U32,    hal.HAL_IN)   
	h.newpin("EndNR",        hal.HAL_U32,    hal.HAL_IN )
	h.newpin("NewNR",        hal.HAL_S32,    hal.HAL_IN) 
	#Edm3 request  next RADf, implies  dec-inng NR
	#
	h.newpin("UNSf",          hal.HAL_FLOAT, hal.HAL_OUT)
	h.newpin("ThisRADf",      hal.HAL_FLOAT, hal.HAL_OUT)
	#
	h.newpin("GenReady",     hal.HAL_S32,    hal.HAL_IN) # was it but code cant set  that  DOUT) 
	#
	h.newpin("restart",       hal.HAL_BIT,   hal.HAL_IN)
	#
	h.newpin("FullDEPTHf",      hal.HAL_FLOAT,   hal.HAL_IN)
	h.newpin("RufPtDEPTHf",     hal.HAL_FLOAT,   hal.HAL_OUT)
	#  in EDM, the tool never goes to the blueprint depth, 
	# because it has 'overburn',
	#  so the FullDepth is reduced to "RoughingPoint'. 
	# All orbits are ecursions from this point
	#
	h.newpin("WiglRADf",        hal.HAL_FLOAT,   hal.HAL_IN)
	# roughing 'wiggle' keeps sides of tool cleaner, 
	# should be just a few microns
	h.newpin("MsrdUNSf",        hal.HAL_FLOAT,   hal.HAL_IN)
	# user must make  tool smaller than blueprint by UNderSize,
	#  its  ok to make smaller, but latger is not allowed
	h.newpin("OrbitType",       hal.HAL_U32,     hal.HAL_IN)
	# only CIRCL and DQUARE for now, 
	# Later Spherical, Vextor, Undercut, SubGate, CashrwGate,
	# Ribulator ) make rib tool a lot narrower
	# tna blueprint, buy hop left and right to
	# get correct wifth
	h.newpin("PlungeOrbitWanted",  hal.HAL_S32, hal.HAL_IN)
	# if BegNR !1= 25, then user can get 1st cut to orbit. 
	# User may have already cut the net shape.

	h.newpin("ToolAxis",       hal.HAL_U32,     hal.HAL_OUT)
	# orbits occur about an axis, 
	# the orbit can be -X or +x, also +x -X +Y -Y 
	# (Linuxcnc does not have tiltable planes, 
	#  so use sine vises etc to orient work )
	h.newpin("CutDir",         hal.HAL_S32,     hal.HAL_OUT) 
	# cuts can be POS or NEG along ToolAxis
	h.newpin("PgmIsMM",        hal.HAL_BIT,     hal.HAL_IN)
	#15.02.2026 not used h.newpin("RndPlaces",      hal.HAL_U32,     hal.HAL_IN)
	#
	#15.02.2026 not used h.newpin("EOin" ,hal.HAL_BIT,hal.HAL_IN)
	#15.02.2026 not used h.newpin("EOout",hal.HAL_BIT,hal.HAL_OUT)
	#
	#22.11.2025 add pin and esit M162
	h.newpin("RPlaneDist", hal.HAL_FLOAT,hal.HAL_IN)
	#
	h.newpin("PauseCtl", hal.HAL_BIT,hal.HAL_OUT)
	h.newpin("abort", hal.HAL_BIT,hal.HAL_IN)
	
	h.ready()
	return h
#
def getPgmUnits():
	"""
	query system for
	 unit of measure.  MM or INCH,
	set pins dor SmallestCmdZise  
	to .0001 (inch) or .001 (mm(
	"""
	global sedm # some sedm pins gets chgd
	#
	s = linuxcnc.stat()
	s.poll()
	#
	tmp = s.program_units # 1 2 or 3
	if tmp == 3:
		# 3 is  CM
		msg = "Centimeter units not supported"
		c = linuxcnc.command()
		c.error_msg(msg)
		raise SystemExit
	# tmp will be 0 1  or 2 right now
	tmp = tmp - 1 # now 0 means INCH   and 1   means MM
	if tmp == 0:# if INCH
		sedm.PgmIsMM = False
		sedm.xyzSCALEfIN = 0.0001
		sedm.xyzSCALEfOUT = sedm.xyzSCALEfIN
	else:# else tmp == 1 meaning METRIC
		sedm.PgmIsMM = True
		sedm.xyzSCALEfIN = 0.001
		sedm.xyzSCALEfOUT = sedm.xyzSCALEfIN
#
def trap():
	"""
	utility to stop comp proceeding
	"""
	a = 1
	while a == 1:
		a = 1
#
def getToolAxis():# s now global
	"""
	 All orbiting is arouns an axis.
	 Gets ToolAxis accorng to PLANE
	  G17 = 2 (Z toolAxis)
	  G18 = 1 (Y toolAxis)
	  G19 = 0 (X toolAxis)
	"""
	global sedm
	#
	s = linuxcnc.stat()
	s.poll()
	
	# get G17 G18 G19 info 
	t= int(s.gcodes[3])
	if t == 190:
		sedm.ToolAxis = 0 # for X is toolAxis
	elif t == 180:
		sedm.ToolAxis = 1 # for Y is ToolAxis
	elif t == 170:
		sedm.ToolAxis = 2 # for Z is ToolAxis
	else:
		#TODO 16.11.2025 allow any of xyz abv uvw
		print(t," Err invalid Plane, shouyld be 190 180 170")
		raise SystemExit
#
def L3D(begTupl,endTupl):
	"""
	L3D returns a list of tuples
	from begTupl to endTupl.
	Uses Brteasenham's line alg, #D version
	"""
	# 
	ListOfPoints = [] #empty lcist for positions on line
	#
	x1 = begTupl[0]
	y1 = begTupl[1]
	z1 = begTupl[2]
	# stoe initial position into list
	ListOfPoints.append( ( x1,y1,z1 )  )
	
	x2 = endTupl[0]
	y2 = endTupl[1]
	z2 = endTupl[2]
	
	# find vvector component lengths
	#  the longest will always get inc'd when creating list
	dx = abs(x2 - x1)
	dy = abs(y2 - y1)
	dz = abs(z2 - z1)
	#
	# get direction of vvecotor components
	#   does X go Right or Left?
	if (x2 > x1):
		xs = 1
	else:
		xs = -1
	#   does Y go Fwd or Bwd?
	if (y2 > y1):
		ys = 1
	else:
		ys = -1
	#   does Z go  Upr oir Down?
	if (z2 > z1):
		zs = 1
	else:
		zs = -1
	# calc each point/tuple along  line
	#   case: Driving axis is X axis"
	if (dx >= dy and dx >= dz):        
		p1 = 2 * dy - dx
		p2 = 2 * dz - dx
		while (x1 != x2):
			x1 += xs
			if (p1 >= 0):
				y1 += ys
				p1 -= 2 * dx
			if (p2 >= 0):
				z1 += zs
				p2 -= 2 * dx
			p1 += 2 * dy
			p2 += 2 * dz
			ListOfPoints.append( ( x1, y1, z1) )
	#   case: Driving axis is Y axis
	elif (dy >= dx and dy >= dz):       
		p1 = 2 * dx - dy
		p2 = 2 * dz - dy
		while (y1 != y2):
			y1 += ys
			if (p1 >= 0):
				x1 += xs
				p1 -= 2 * dy
			if (p2 >= 0):
				z1 += zs
				p2 -= 2 * dy
			p1 += 2 * dx
			p2 += 2 * dz
			ListOfPoints.append( ( x1, y1, z1) )
	#   case: Driving axis is Z axis
	else:        
		p1 = 2 * dy - dz
		p2 = 2 * dx - dz
		while (z1 != z2):
			z1 += zs
			if (p1 >= 0):
				y1 += ys
				p1 -= 2 * dz
			if (p2 >= 0):
				x1 += xs
				p2 -= 2 * dz
			p1 += 2 * dy
			p2 += 2 * dx
			ListOfPoints.append( ( x1, y1, z1) )
	#
	return ListOfPoints
#
# 14.02.2026 TODO
#   do not scatter initializations thru code
# slammed a few lines down vvv 
# RufPtTupl = None # make it globally readable
AllLines=[]
fqfn="" # Fully Qualified File Name   ibcludes absolute path
EtabDict ={}#  each EDM gnerator settling (Stage) 
# is accessed as EtaabDict[Nr]
#  vvv dummy value, ui just so it is global
RufPtTupl = ( (-100000,-100000,-100000) )

def getStartPtF(): 
	"""
	 All cuts measure from StartPointOrigin (DPO)
	 All cuts begin at Rplane,
	'above' the SPO, outside of stock.
	
	 only callewd 1x per cut
	
	 14.02.2026 TODO years agao early in this code
	 i had issues with vakues not being rady
	 and adopted using short sleep's to ensure values were ok
	 TODO old voodoo should be understood and sleeps removed
	"""
	# wait for position to be true
	time.sleep(0.1)# was ng at 0.01
	# collect posn at rest
	sedm.xsp = sedm.xFBf  # x posn feedback NOE
	sedm.ysp = sedm.yFBf
	sedm.zsp = sedm.zFBf
#
def setGen(NRval):
	"""
	Sets parametyers in the EDM POWER SUPPLY
	( aka Generator or Spark Generator)
	
	This func can be cvalled 2 ways
	1) with the key to a dictionary or parameters
	2) with key '0' meaning clear 
	the generator.
	
	The dunction relies on the data order in the dile .
	
	The sedm system is an automatically
	sequencing system.
	When a single tool 'roughs' the net shape,
	The system automtically decreaes the power
	and rfinne the foem.
	There are many 'finitions, 
	carried out automatically.
	
	The Generator will get
	all data neccesary to cut.
	As of 14.02.2026, the Generaor
	is only a PyVCP xml GUI
	That Gui could become the user I/F to the real gnenratyor
	
	There are a _lot_ of pins to a  generator
	14.02.2026 TODO
	reviewe list  
	remove unneede code
	
	The data for generator is held
	in file called 'ETables'.
	Each Etable is a list power stages
	from more powqerdul to less powewrful.
	Eaxh stage has a key 'NR'
	each key can retrieve a 
	set of data for the generator
	NRs run from 25 to 1.
	Etables always begin at NR 25.
	So the NR decreases as the cut progresses.
	"""
	global EtaabDict
	#
	if NRval != 0:
		"""
		 0 is for a breand new Etab
		 or before abort/resime,  
		   no stages used yet
		"""
		NRstr = str(NRval)#key  to  dict is  of form 'keyname' ( note ticks~)
		#
		sedm.NR =    NRval # local storage on sedm pin
		sedm.IP =    EtabDict[NRstr][0]  #'peak' current
		sedm.P  =    EtabDict[NRstr][1]  #Pollarity
		sedm.HV =    EtabDict[NRstr][2]  #ignitionVoltage
		"""
		 a milliamp supply used to initialize the spark,
		 a higher HV makes it easier for system to 'see' 
		 on smooth surfaces, where the highest point is 
		 harder to distiguish
		"""
		#
		# TODO vvv for real use
		#sedm.GVHI = EtabDict[NRstr][3]
		#sedm.GVLO = EtabDict[NRstr][4]
		# TODO  vvv for testing
		sedm.GVHI =   50#35#40# 45# 2 #45 #
		sedm.GVLO =   40#28#30# 35# 1 #40 #
		#
		sedm.TON =   EtabDict[NRstr][5]  
		sedm.TOF =   EtabDict[NRstr][6]  
		sedm.ISO =   EtabDict[NRstr][7]  #in IsoPulse mode,
		#
		sedm.AJD =   EtabDict[NRstr][8] 
		# 'peck' cycle jump distance, decimal mm
		sedm.ET  =   EtabDict[NRstr][9]
		# 'peck' cycle cut duration, decimal Secs
		#
		sedm.BWDmax =  EtabDict[NRstr][10]
		# max number of contiguous low voltage samples,
		# eceeding will cause abort and return to start point
		#
		sedm.RADf    = EtabDict[NRstr][11]  #radius per side
		sedm.SPAf   =  EtabDict[NRstr][12]  #spherical step angle
		# for sphertical orbit
		#  a hemisphere is a stack of circles
		#  these data are placeholders, neyonf my ability 
		#  to measueree and erecord
		sedm.VEf  =   EtabDict[NRstr][13] #electrode wear
		sedm.VWf  =   EtabDict[NRstr][14] #MMR metal removal rate
		sedm.VDIf =   EtabDict[NRstr][15] #surface roughness
		#
		sedm.ThisNR = NRval # non zero
	#
	else: # NRval == 0 means clear the gennrator
		NRstr = "0" #str(NRval)
		#
		sedm.BWDcount    = 0
		sedm.CutDir      = 0
		sedm.EtabNum     = 0
		sedm.BegNR       = 0
		sedm.EndNR       = 0
		#
		sedm.FullDEPTHf  = 0
		sedm.MsrdUNSf    = 0
		sedm.RADf        = 0
		sedm.RufPtDEPTHf = 0
		#
		sedm.state       = 0
		#
		sedm.xsp         = 0
		sedm.ysp         = 0
		sedm.zsp         = 0
		#
		#data  specific to Etaab and TechGui
		sedm.NR          = 0
		sedm.IP          = 0
		sedm.HV          = 0
		sedm.P           = 0
		sedm.ISO         = 0
		sedm.BWDmax      = 0
		sedm.AJD         = 0
		sedm.ET          = 0
		#
		#31.01.2026 TON TOF are integer uSec
		sedm.TON         = 0
		sedm.TOF         = 0
		#
		sedm.GVHI    = 0
		sedm.GVLO    = 0
		#
		sedm.SPAf        = 0
		#
		sedm.VEf         = 0
		sedm.VWf         = 0
		sedm.VDIf        = 0
		#
		# TODO vvv
		# vvv **** spcl values NOT set to 0
		#25.11.2025 make sure user answered Pitch w value >=0
		sedm.Pitch    = -1
		#
		sedm.ThisNR      = -1 # after clean up
		#
		sedm.inpo     = False
		sedm.isEna    = False
#
def parseEtab(fqfn):
	"""
	 builds a dict from an ENC file
	  key = NR,
	  value =  list   of paarms
	 Eaxh NR is a set of cuttimg paramers.
	 The Largets NR ( 25) is where most cuts begin
	 It will havea large overburn and no orbit.
	 The subbsequent NRs have decreasing power
	  and increasing orbital 'radius'.
	 The enrty for 'radius' on NR 25 is actually
	  UNS 'undersize'. 
	 The user enters the "measured underze'. 
	 This must be less than or equal to the 
	  Etable's undersize. The difference is
	  accomodated by the code. The Correct 
	  cavity size and dpeth are maintained.
	 The energy resulting from the NR's parameters,
	  will have a 'reach' propotional to the product
	  of OnTime and Current and OpenVoltage 
	  andthe Dielectric strength of the medium.
	     ( in general ). 
	 As the joules decrease, the tool needs to
	  get closer to the stock to get the spark 
	  phenomna to occur.
	  
	 That translation is the orbiting motion.
	  The tool never gets to the 'print' depth.
	 The tool position is restrined by the overburn value.
	 The overburn values are derived from many published
	 data. (sort of LLM for EDM)
	 These data were 'curve fit' to get generic formulae
	  independant of commercial manufactures. 
	"""
	
	global AllLines #
	global EtabDict #
	#
	# open raed and close etab fille
	fhndl=open(fqfn, 'r') #
	if os.path.isfile(fqfn) != True: #
		print("ffile nopt exist ", fqfn) #
		raise SystemExit #
	#
	AllLines = fhndl.readlines() #
	# TODO  need err hndlimg if file not found and file  
	#  empty or file ng
	fhndl.close() #
	#
	# strip newlines
	for i in range(0,len(AllLines)-1): #
		AllLines[i]=AllLines[i].rstrip() #
	#
	# begin build dict  key  is NR  val  is a list (of params)
	EtabDict ={} # #  each NR or Staage is accessed as EtaabDict[Nr]
	#
	#  get rid  of 1st   2 lines, they just make file man readable
	AllLines.pop(0) # # 1st line has etab fname old style 999999nn.E INCJ or MM  and sometomes useless P           
	AllLines.pop(0) # # get wid of what was 2md line read
	#	
	#######################
	# clean up  list of lines
	# remove trailg whitespace
	ll= len(AllLines)-1 #
	for i in range(0,ll): #
		AllLines[i]=AllLines[i].rstrip() #
	# remove empty lines
	for i in range(0,ll): #
		if AllLines[i]=="": #
			AllLines.pop(i) #
		else: #
			# remove lines begommomg woth space char
			if AllLines[i][0]==" ": #
				AllLines.pop(i) #
			else: #
				# remove line brgiining w neewline
				if AllLines[i][0]=='\n': #
					AllLines.pop(i) #
				else:
					# remove lines == '[END]'
					if AllLines[i]=="[END]":
						AllLines.pop(i)
	#
	# ========CNVRT Strgs to Floats  and Ints ==============
	numLines=0 #
	#	##########
	#	#  BUILD DICT
	#	##########
	nl= len(AllLines)-1 #
	
	#process all remainiung lines
	for  i  in range(0,nl): #
		# break long string into list of substrings
		lineParts=AllLines[i].split() #
		
		# linnePartrs[0] is IP
		nP = len(lineParts)-1 #
		#
		tupl=() #
		#  TODO  +1 seems wrong, but it 'worked'
		for lPartNdx in range(1, nP+1): # # skip 0th thats Nr the key  lpl already  is leen *vlah)  -1
			
			ftmp=float(lineParts[lPartNdx]) #
			#
			if ( (lPartNdx  != 9) and ( lPartNdx  != 10) and ( lPartNdx  != 12)   and (lPartNdx  != 13) ):
				tupl=tupl+( int(ftmp),) # weird   comma to make it a tuple so iy can be concvatenated
			else:
				tupl=tupl+( ftmp, )
			#

		#
		if tupl != ():
			numLines+=1
			EtabDict[ lineParts[0] ] = tupl[0:] # 25.02.2026 isnt [9:] same as []??
			# looked ok rint(9741,EtabDict[ lineParts[0] ] )

#
def mkThisRADf(): # used every ThisNR EXCEPT 25  ( handled by mkRufPtTup/// l)

	sedm.XtraRADf =  round(sedm.MsrdUNSf - sedm.UNSf,3)

	if sedm.ThisNR != 25: # call herte b4 dec'd
		sedm.ThisRADf = sedm.RADf + sedm.XtraRADf
		# now make an INT of 'steps' in ThisRADf
		# store it in sedm.RADi
		x = sedm.ThisRADf
		# sedm.xyzSCALEfOUT or IN are .002 for MM and .0001 for Inch
		x = x * (1/sedm.xyzSCALEfOUT)
		x = round(x)
		sedm.RADi = int(x)
	else: # ThisNR == 25
		sedm.UNSf = sedm.RADf
		sedm.ThisRADf = 0.0 # there is no orbit on NR 25, only wigl
		sedm.RADi = 0 # there is no orbit on NR 25, only wigl
#
#05.02.2026 this vvv looks at ctrl variable PV
# In EDM it would be called GapValue 
#  a voltage analgous to distannce  between 
# too and workpice
# This valie is constantly monitored,
# So, it is ahanfly place tp do other checks
# like\:
#  did user pree PEEK nutton?
#  Is it time to do a 'jump' ( fluching techique)
#06.02.2026 vvv chg to pass ndx not tipl
#  CutL should become global
# 24.03.2026 no need to pass Ltype, use sedm.JumpLtype instead
#24.03.2026 vhg ndx to myNdx so gloab ndx not needed
def getEvalPV(L,myNdx ): # rtns FWD HOLD BWD for EDM
	# TODO06.02.2026 chg pnow  s to l[myNdx]
	""" doc  line
	Rtruns a value that controls tool position.
	This value is 1 of FWD HOLD or BWD.
	The value is from a random.uniform call,
	which is filtered by a software
	window comparator.
	
	The returned value is similar to the
	GapValue in many EDM process controls.
	
	The limits for the call to uniform()
	are FvMin and GvMax taken from a file
	similare tro an .ini file ( called sedmhdrs,py)
	
	The evaluation of the uniform value uses
	2 threshodls forming the window comparator.
	These thrsholds are GvHi and GvLo.
	
	Thos 2 parameters are obtained from a cutting technology tabvle.
	( ETable )
	
	The value from uniform can be above the GvHi threshold,
	and evaulates to FWD.
	Or, the values can be below the GvLo threshold,
	evaluating to DWD.
	Else, the value is between the thresholds,
	evaluating to HOLD.
	
	Th EDM motion is simply followinga rpedetermined
	list of XYZ tuples. Thes tuples are adjacent, and the
	FWD HOLD VWD deide the next smae or previous tuple
	in the lts.
	"""
	
	# for debugm i pu EDMgrade on a pin do halmeter can onserve
	# 03.01.2026 straight kubne bore alwayts FWD , 1wiglZneg gets bwds gets holds
	#
	
	SPO = ( (0,0,0) )
	"""
	# This dunction is central to the entire system
	# so central, and called fo often,
	# it is also where the operattor
	# can choose to PEEK RETURN or QUIT.
	#
	"""
	#
	# handle PEEK Return QUIT btns
	sedm.QuitHit = False; #no lingetring flags
	#
	# ceck if time to jump
	if (sedm.JumpENA == True) and (sedm.JumpOn == True):
		t2jump = chkJT()
		if t2jump:
			# vvv uses sedm.JunpLtype
			JupL,JdnL = mkJupLJdnL(L,myNdx)
			doJump(JupL,JdnL)
			mkJT() # make a new endJT
			#
	if sedm.EDpeek == True:
		sedm.disableOsc = True      # power off asap , during tool withdrawl
		
		# return began to work, but at bP it rtnd to SPO
		# i trhibnk becus PEEK still active
		# ao tyurn it off like the other btns get turnmed off
		# YAY peek return quit work 
		sedm.EDpeek = False
		
		peekL = []
		retL = []
		peekL, retL = mkPeekL( L[myNdx] )
		doExitL(peekL)
		#
		# at SPO, user just did PEEK
		while 1: 
			if sedm.EDquit == True:# vvv already at SPO becuz ^^^
				sedm.disableOsc = True
				sedm.QuitHit = True
				sedm.EDquit = False #  release  btn
				return BWD #retval is bogud,  caller must test Quit and Return before eval
			if sedm.EDreturn == True :# more readble than elseif ,
				#  the reason why is not hidden
				
				# this vvv setgen is dore RETURN after PEEK
				setGen(sedm.ThisNR) #  maybe useless. unnecc
				
				sedm.disableOsc = False
				doExitL(retL)# rwtL was made during Peek hanfler
				sedm.EDreturn = False # 05.01.2026 missimg turn of btn 
				#
				
				# NO DONT RETURN JUDST CONTINUE return FWD # cade in ca;;er excpect FWD to continue
				# well don t leave thid func, cont into get PV
				#break # tricky with frerebies, well freebies is 20 now
				
				return FWD
		#end while 1
	#end if sedm.EDpeek == True
	#/// can i get return wokinmg 
	elif IsFreebie(L[myNdx]) == True:
		return FWD # was EDMgrade = FWD
		#ng EDMgrade = FWD # get return continuing???
	#///
	else: # else PEEK was not pressed se use PV
		pv = uniform(GVMIN, GVMAX)# GVMAX GV<IN from sedmhdrs.py
		if   pv > sedm.GVHI: # HIGVLIM  in sedmhdrs.py 
			sedm.BWDcount = 0
			#29.12.2025 wasreturn FWD
			EDMgrade = FWD
		elif pv < sedm.GVLO:# LOGVLIM  in sedmhdrs.py 
			sedm.BWDcount += 1
			if sedm.BWDcount >= sedm.BWDmax:
				sedm.BwdMaxHit == True
			EDMgrade = BWD
		else:
			# 29.12.2025 was return HOLD # caller can ignore it
			EDMgrade = HOLD
		# common exit for PV
		sedm.EDMgrade = EDMgrade
		return EDMgrade
# end test
#
def mkDetourLegL( ToePt ):
	# this makes a legL fro, toe to amnkl to jip
	#
	SPO = (0,0,0) #StartPtOffset nit Posn
	#
	footL = L3D( ToePt , RufPtTupl)
	# slice off ankle, its also on end of shinL
	lfL = len(footL)-1
	footL = footL[:lfL]
	
	# make shinL
	shinL = L3D(RufPtTupl ,SPO)
	#
	lsL = len(shinL) - 1
	ankleNdx = lfL
	# join foot bone to shinbone
	legL = footL + shinL
	#
	#03.01.2026dbug
	# make sure footL[0] === TorPt
	return legL, ankleNdx
#
# TODO vvv BAD NAME 
#  code reads  like it can use
#  CtrPt that is NOT RufPtTupl
#10.02.2026 in YposWiglNR17-15.ngc
# when moving from rufpttuple to peri
# there is no Z component
# but the X should be inc'ing from 0 tp radius
#
def mkOrbitEntryLegL( EntryPt ):
	SPO = (0,0,0) #StartPtOffset nit Posn
	
	# make CtrPt tupl using radi and entrypt
	if sedm.ToolAxis == 2:    #Z
		cZ = EntryPt[2] 
		cZ -= sedm.RADi * sedm.CutDir
		CtrPt = ( ( 0, 0, cZ)  )
	elif sedm.ToolAxis == 1:  #Y
		cY =EntryPt[1] 
		#10.02.2026 vvv i used  -=  for toolAxis =2
		#cY += sedm.RADi * sedm.CutDir
		cY -= sedm.RADi * sedm.CutDir
		CtrPt = ( ( 0, cY, 0 ) )
	else:                     #X
		cX = EntryPt[0] 
		#10.02.2026 vvv i used  -=  for toolAxis =2
		#cX += sedm.RADi * sedm.CutDir
		cX -= sedm.RADi * sedm.CutDir
		CtrPt = ( ( cX, 0, 0)  )
	#
	footL = L3D( EntryPt, CtrPt ) #  path exits at  'TOE'
	# I want footL[0] to be on peri
	# becuz 1st parm in L3D is [0]th 
	lfL = len(footL)-1
	ankleNdx =  lfL
	footL = footL[:lfL]# slice off ankle, 
	#  its in the shinL anyway
	#
	shinL = L3D( CtrPt, SPO) # 1st parm is 0th 
	# join foot bone to shinbone
	legL = footL + shinL 
	# ndx anklNdx is wghere tool is start of EntryLegL
	#
	# should cont to doOrbL
	return legL, ankleNdx
#
def mkFootLeadInL( PathEntryPt ):
	# passing PathEntryPt allows HORZ
	#  or 45deg approackh to periemeter
	SPO = (0,0,0) #StartPtOffset nit Posn
	#
	footL = L3D( RufPtTupl, PathEntryPt ) #  path exits at  'TOE'
	footLen = len(footL)
	footL = footL[1:footLen -1]
	#
	return FootLeadInL
#
def mkPeekL( pNow ):
	
	SPO = ( (0,0,0) )
	if sedm.ToolAxis == 2:
		# if at ctr
		if (pNow[0] == 0) and (pNow[1] == 0):
			xL = L3D( pNow, SPO)
			rL = L3D( SPO, pNow)
			
			# test adjacency
			return xL,rL
		else:# else NOT at ctr
			# 27.11.2025 if cutting sown,
			# then REDUCE the distancve to 0,0,0
			# so, subtract an neg numbert to get a less neg result
			ctrPosn = ( (0,0, pNow[sedm.ToolAxis] - (sedm.RADi * sedm.CutDir) )  )
	elif sedm.ToolAxis == 1:
		if (pNow[0] == 0) and (pNow[2] == 0):
			xL = L3D( pNow, SPO)
			rL = L3D( SPO,pNow)
			
			#test asjacenvy
			return xL,rl
		else:# else NOT at ctr
			#27.11.2025 subtract
			ctrPosn = ( (0, pNow[sedm.ToolAxis] - (sedm.RADi * sedm.CutDir) ,0) )
	elif sedm.ToolAxis == 0:
		if (pNow[1] == 0) and (pNow[2] == 0):
			xL = L3D( pNow, SPO)
			rL = L3D( SPO, pNow)
			
			# test adjacenvy
			return xL, rL
		else:# else NOT atr ctr
			#27.11.2025 subtract
			# 12.02.2026 was
			#ctrPosn = ( (posn[sedm.ToolAxis] - (sedm.RADi * sedm.CutDir),0,0) ) 
			ctrPosn = ( (pNow[sedm.ToolAxis] - (sedm.RADi * sedm.CutDir),0,0) ) 
	# only those NOT atr xtr are left
	footL = L3D( pNow,ctrPosn)
	shinL = L3D(ctrPosn,SPO)
	#
	xL = footL[:(len(footL)-1) ] + shinL
	# 27.11.2025 each sublist need to be reversed
	shinL.reverse()
	footL.reverse()
	rL = shinL[:(len(shinL)-1) ] + footL
	
	# test  adjacency xL
	#msg = "in mkPeekL, exit path 'xL' is not adjacent"
	#chkListAdj(xL,msg) # wonmt process if false, 
	#
	return xL,rL
#
def mkcL( radi, cLevel ): # pass LeadInLine
	# BRESENHAM CIRCLE PATH PLANNER, THANK YOU MR b, NO FLOATS NEEDED! AND FAST
	#--------beg octant 1 of 8 
	oct1L =  [] 
	a = radi  
	b = 0
	#
	da = 1 - ( radi + radi )
	db = 1#   dTER = 1
	re = 0 # radius error
	#
	while a >= b: #
		if sedm.ToolAxis == 2: #Z  plnne is XY
			tupl = (a,     b,      cLevel )
		
		
		elif sedm.ToolAxis == 1: #Y  plane is ZX
			tupl = ( a,    cLevel, b)
		elif sedm.ToolAxis == 0: #X  plane is YZ
			tupl = (cLevel, a,     b)
		oct1L.append( tupl )
		# -------------------
		# ----- mid loop
		b = b + 1  		# always inc b
		re = re + db	#  re = re + dTER
		db = db + 2 	#dTER = dTER + 2
		if (  (   (re + re ) + da  ) > 0 ):#  if (( (re + re) + dSEC  ) > 0 ):
			a = a - 1  
			re = re + da #re = re + dSEC
			da = da + 2 #  dSEC = dSEC + 2
	# -- end while 
	l = len(oct1L) # calc 1x and keep it available
	# ------- end  octant 1
	#
	# ------- beg octant 2
	oct2L = []
	# 24.03.2026 no need gor global index, ndx is init'd here
	for bcNdx in range(l-1,  -1, -1):#  loop bwds thri octant 1 data
		if sedm.ToolAxis == 2: #Z G17
			nux = oct1L[bcNdx][1]
			nuy = oct1L[bcNdx][0]
			nuz = oct1L[bcNdx][2]
			tupl=( (nux,nuy,nuz) )
		elif sedm.ToolAxis == 1: #Y G18
			nux = oct1L[bcNdx][2]
			nuy = oct1L[bcNdx][1]
			nuz = oct1L[bcNdx][0]
			tupl=( (nux,nuy,nuz) )
		elif sedm.ToolAxis == 0: #X G19
			nux = oct1L[bcNdx][0]
			nuy = oct1L[bcNdx][2]
			nuz = oct1L[bcNdx][1]
			tupl=( (nux,nuy,nuz) )
		#
		oct2L.append(tupl)
	#
	# ------- beg octant 3
	oct3L = []
	#24.03.2026 no need for gloabl bcNdx, it is initd here
	for bcNdx in range(l-1,  -1, -1):#  loop bwds thri octant 1 data
		if sedm.ToolAxis == 2: #Z G17
			nux = oct2L[bcNdx][0]
			nuy = oct2L[bcNdx][1]
			nuz = oct2L[bcNdx][2]
			tupl=( (-nux,nuy,nuz) )
		elif sedm.ToolAxis == 1: #Y G18
			nux = oct2L[bcNdx][0]
			nuy = oct2L[bcNdx][1]
			nuz = oct2L[bcNdx][2]
			tupl=( (-nux,nuy,nuz) )
		elif sedm.ToolAxis == 0: #X G19
			nux = oct2L[bcNdx][0]
			nuy = oct2L[bcNdx][1]
			nuz = oct2L[bcNdx][2]
			tupl=( (nux,-nuy,nuz) )
		#
		oct3L.append(tupl)
	# ------- beg octant 4
	oct4L = []
	#24.03.2026 no need for global bcNdx, initd here
	for bcNdx in range(l-1,  -1, -1):#  loop bwds thri octant 1 data
		if sedm.ToolAxis == 2: #Z G17
			nux = oct3L[bcNdx][1]
			nuy = oct3L[bcNdx][0]
			nuz = oct3L[bcNdx][2]
			tupl=( (-nux,-nuy,nuz) )
		elif sedm.ToolAxis == 1: #Y G18
			nux = oct3L[bcNdx][2]
			nuy = oct3L[bcNdx][1]
			nuz = oct3L[bcNdx][0]
			tupl=( (-nux,nuy,-nuz) )
		elif sedm.ToolAxis == 0: #X G19
			nux = oct3L[bcNdx][0]
			nuy = oct3L[bcNdx][2]
			nuz = oct3L[bcNdx][1]
			tupl=( (nux,-nuy,-nuz) )
		#
		oct4L.append(tupl)
	# ------- beg octant 5
	oct5L = []
	#24.03.2026 no need for global bcNdx, initd here
	for bcNdx in range(l-1,  -1, -1):#  loop bwds thri octant 1 data
		if sedm.ToolAxis == 2: #Z G17
			nux = oct4L[bcNdx][0]
			nuy = oct4L[bcNdx][1]
			nuz = oct4L[bcNdx][2]
			tupl=( (nux,-nuy,nuz) )
		elif sedm.ToolAxis == 1: #Y G18
			nux = oct4L[bcNdx][0]
			nuy = oct4L[bcNdx][1]
			nuz = oct4L[bcNdx][2]
			tupl=( (nux,nuy,-nuz) )
		elif sedm.ToolAxis == 0: #X G19
			nux = oct4L[bcNdx][0]
			nuy = oct4L[bcNdx][1]
			nuz = oct4L[bcNdx][2]
			tupl=( (nux,nuy,-nuz) )
		#
		oct5L.append(tupl)
	# ------- beg octant 6
	oct6L = []
	#24.03.2026 no need for global bcNdx, initd here
	for bcNdx in range(l-1,  -1, -1):#  loop bwds thri octant 1 data
		if sedm.ToolAxis == 2: #Z G17
			nux = oct5L[bcNdx][1]
			nuy = oct5L[bcNdx][0]
			nuz = oct5L[bcNdx][2]
			tupl=( (nux,nuy,nuz) )
		elif sedm.ToolAxis == 1: #Y G18
			nux = oct5L[bcNdx][2]
			nuy = oct5L[bcNdx][1]
			nuz = oct5L[bcNdx][0]
			tupl=( (nux,nuy,nuz) )
		elif sedm.ToolAxis == 0: #X G19
			nux = oct5L[bcNdx][0]
			nuy = oct5L[bcNdx][2]
			nuz = oct5L[bcNdx][1]
			tupl=( (nux,nuy,nuz) )
		#
		oct6L.append(tupl)
	# ------- beg octant 7
	oct7L = []
	#24.03.2026 no need for global bcNdx, initd here
	for bcNdx in range(l-1,  -1, -1):#  loop bwds thri octant 1 data
		if sedm.ToolAxis == 2: #Z G17
			nux = oct6L[bcNdx][0]
			nuy = oct6L[bcNdx][1]
			nuz = oct6L[bcNdx][2]
			tupl=( (-nux,nuy,nuz) )
		elif sedm.ToolAxis == 1: #Y G18
			nux = oct6L[bcNdx][0]
			nuy = oct6L[bcNdx][1]
			nuz = oct6L[bcNdx][2]
			tupl=( (-nux,nuy,nuz) )
		elif sedm.ToolAxis == 0: #X G19
			nux = oct6L[bcNdx][0]
			nuy = oct6L[bcNdx][1]
			nuz = oct6L[bcNdx][2]
			tupl=( (nux,-nuy,nuz) )
		#
		oct7L.append(tupl)
	# ------- beg octant 8
	oct8L = []
	#24.03.2026 no need for global bcNdx, initd here
	for bcNdx in range(l-1,  -1, -1):#  loop bwds thri octant 1 data
		if sedm.ToolAxis == 2: #Z G17
			nux = oct7L[bcNdx][1]
			nuy = oct7L[bcNdx][0]
			nuz = oct7L[bcNdx][2]
			tupl=( (-nux,-nuy,nuz) )
		elif sedm.ToolAxis == 1: #Y G18
			nux = oct7L[bcNdx][2]
			nuy = oct7L[bcNdx][1]
			nuz = oct7L[bcNdx][0]
			tupl=( (-nux,nuy,-nuz) )
		elif sedm.ToolAxis == 0: #X G19
			nux = oct7L[bcNdx][0]
			nuy = oct7L[bcNdx][2]
			nuz = oct7L[bcNdx][1]
			tupl=( (nux,-nuy,-nuz) )
		#
		oct8L.append(tupl)
	#
	#
	# concat octants 
	# then remove duplicates
	#  where lists overlap
	cL=[]
	#07.10.2025 damnit i didnt continu l;ibe correvct, fo iy hard way
	cL = oct1L + oct2L + oct3L + oct4L \
	+ oct5L +  oct6L + oct7L + oct8L
	#
	# vvv trick to removve duplicate list entries
	cL = list(dict.fromkeys(cL))
	
	return cL
#
def mksqrL( radi,cLevel):# yeah radius is bad word but its ok
	sqrL = []
	# 29.11.2025 RADi passed
	
	# Right Middle a,b
	rMida = radi #like 5,0  was  sedm.RADf # wasradCounts
	rMidb =  0
	# Top Right a,b
	tRa =  rMida # was Mda  eg 5,5
	tRb =  rMida # wsa Mda
	# Top Left a,b
	tLa = -(rMida)
	tLb =  rMida
	# Bot Left a,b
	bLa =  tLa
	bLb = -(rMida)
	# Bot Right a,b
	bRa =  tRa
	bRb =  bLb
	#
	# BEWARE looks like i calc thuis 2x, once outrside, onece insidee func
	#sqrDp =  RufPtTupl[ ToolAxis ] + ( CutDir * radi )
	
	sqrDp =  cLevel
	
	# ---------------
	if sedm.ToolAxis== 2: #Z G17  XY  plane
		topRtHalfL = L3D( (rMida, rMidb ,  sqrDp) , (tRa,tRb ,  sqrDp)  )
		topL       = L3D( (	tRa, tRb ,  sqrDp) , (tLa,tLb ,  sqrDp)  )
		leftL      = L3D( (	tLa,    tLb ,  sqrDp) , (bLa, bLb ,  sqrDp)  )
		botL       = L3D( (	bLa, bLb ,  sqrDp) , (bRa, bRb ,  sqrDp)  )
		botRtHalfL = L3D((bRa,bRb,sqrDp),(rMida,rMidb,sqrDp))

	if sedm.ToolAxis== 1:
		topRtHalfL = L3D( (rMida,   sqrDp, rMidb ) , (tRa,   sqrDp ,tRb )  )
		topL       = L3D( (	tRa,   sqrDp, tRb ) , (tLa,  sqrDp, tRb )  )
		leftL      = L3D( (	tLa,   sqrDp ,tLb ) , (bLa,   sqrDp, bLb)  )
		botL       = L3D( (	bLa,   sqrDp, bLb) , (bRa,   sqrDp, bRb )  )
		botRtHalfL = L3D( ( bRa,   sqrDp, bRb ) , (rMida,   sqrDp, rMidb ) )
	if sedm.ToolAxis== 0:
		topRtHalfL = L3D( (   sqrDp, rMida, rMidb ) , (   sqrDp, tRa,tRb )  )
		topL       = L3D( (  sqrDp, tRa, tRb ) , (  sqrDp, tLa, tLb )  )
		leftL      = L3D( (	  sqrDp ,tLa,  tLb ) , (   sqrDp, bLa, bLb)  )
		botL       = L3D( (	  sqrDp, bLa, bLb) , (  sqrDp, bRa, bRb )  )
		botRtHalfL = L3D( (   sqrDp, bRa, bRb ) , (  sqrDp, rMida, rMidb ) )
	#-----------------------
	sqrL  = topRtHalfL[0: len(topRtHalfL) -1 ] \
		+ topL[0: len(topL)-1]\
		+ leftL[0: len(leftL)-1]\
		+ botL[0: len(botL)-1]\
		+ botRtHalfL[0: len(botRtHalfL)-1]\
	#
	#sqrL.append( topRtHalfL[0])
	
	# TODO last cell is dupw of  1st cell
	return sqrL 
#
def mkOrbitPathL(radi, cLevel):
	
	# 10.02.2026 ???? doe opL contain leadinline>>>
	
	if sedm.OrbitType == CIRCLE:
		orbL = mkcL( radi, cLevel)
		entryPt = orbL[0]
		# add a cxopy of 1st posn onto  end of list
		orbL.append(entryPt) # dipe 1st to last
		#
	if sedm.OrbitType == SQUARE:
		orbL = mksqrL( radi,cLevel)
		entryPt = orbL[0]
		orbL.append(entryPt) # dipe 1st to last
		#
	# 04.01.2026 c hec klist for SPO
	SPO = ( ( 0,0,0 ) )
	
	return orbL
#
def mkDetourWiglL( posn): #sedm.ToolAxis ):
	#
	WiglRADi = int(round(sedm.WiglRADf / sedm.xyzSCALEfOUT))
	
	cDeep = posn[sedm.ToolAxis] - ( WiglRADi * sedm.CutDir)
	
	if sedm.ToolAxis == 2:
		CtrNow = (0,0,cDeep) #posn[sedm.ToolAxis])
	elif sedm.ToolAxis == 1:
		CtrNow = (0,cDeep,0) #posn[sedm.ToolAxis],0)
	elif sedm.ToolAxis == 0:
		CtrNow = (cDeep,0,0) #posn[sedm.ToolAxis],0,0)
	#
	footL = L3D(posn,CtrNow)
	#
	shinL = L3D( CtrNow, (0,0,0)  )
	WiglEscL = footL + shinL
	ankleNdx = len(footL)
	#
	return WiglEscL, ankleNdx
#
def doOrbL( OrbL): # begins at EnrtyPt
	# ends at RufPtTupl   NOT at SPO
	SPO = ( ( 0,0,0) )

	# 12.02.2026 TODO chg to goodLim badLim so it reads like others
	endNdx = len(OrbL) -1
	begNdx = 0

	#12.02.2026 new 3 lines do vvv alays if sedm.JumpENA == True:
	sedm.JumpLtype = JumpOrbPathType
	sedm.JumpOn = True
	
	OrbEndPt = OrbL[endNdx]
	
	#02.01.2026 why start at ndx 1  other codes start at ndx 0
	#24.03.2026 chg ndx to oLndx
	oLndx = 1
	#
	doMove(OrbL[oLndx])
	#
	# ??? importannt ??? not yet sedm.JumpoOn = True
	while 1: # # seq is 
		#24.03.2026 vhg vvv 
		#nextDir = getEvalPV( JumpOrbPathType,OrbL,ndx )
		# 24.03.2026 vhg to vvv
		nextDir = getEvalPV( OrbL,oLndx )
		#
		if (sedm.BwdMaxHit == True) or (sedm.QuitHit == True):
			return 
			# rtn w flag set    let CUPA move to ctr, move to SPO
		else:
			if nextDir == FWD:# check all done and SUCCESS
				if oLndx == endNdx: #goodLim = len(OrbL) -1
					#
					# if ALREADY at end of orbList and FWD
					loL = L3D(OrbL[oLndx],RufPtTupl)
					doExitL(loL) # MOVES TO RUFPTTUPL  NOT SPO
					return 
					# rtn w NO flag set   p = RufPtTupl no flags
					#
				else: # else  FWD and ndx < goodLim
					oldP = OrbL[oLndx]
					oLndx += 1
					newP = OrbL[oLndx]
					#
					doMove(newP)
					#test fatyal flags
					if (sedm.BwdMaxHit == True) or (sedm.QuitHit == True):
						sedm.JumpOn = False
						return 
						# rtn w flag set    
						# let CUPA move to ctr, move to SPO
			#
			elif nextDir == BWD: 
				# done aLREADY   1st check for fatal
				#
				bP = OrbL[oLndx] #make a detour path  frm BreakPt bP
				legL,ankleNdx = mkDetourLegL(bP)
				doDetourLegL(legL,begNdx)
				# 0 is BegNdx , hwre to start in List
				#
				if ( sedm.BwdMaxHit == True) or (sedm.QuitHit == True):
					sedm.JumpOn = False
					return 
#
def doExitL( xL): # no  getEvalPV, just cloicck out stepsxl[0] is  
	#24.03.2026 chg ndx to xNdx 
	xNdx= 0
	goodLim = len(xL)-1
	badLim = 0
	while xNdx < goodLim: #
		#
		oldP = xL[ xNdx ]
		xNdx += 1
		newP = xL[ xNdx ]
		#
		doMove( newP )
		time.sleep(.001)
#
def doMove( posn): # caller must make sure podn is adjacen to last
	#
	# 02.01.2026 neccc   this is the cmd to move  to posn
	sedm.XOffsetCmd = posn[0]
	sedm.YOffsetCmd = posn[1]
	sedm.ZOffsetCmd = posn[2]
	# !?!? NECC yes !!!thius is MIN and NECC
	time.sleep(0.001)
#
def getAtctr(posn):
	#rtns atctr CtrPosn
	if sedm.ToolAxis == 2:# Z
		if (posn[0] == 0)  and (posn[1] == 0):
			atctr = True
		else:# else tool is NOT at ctr
			CtrPosn = ( ( 0,0,posn[sedm.ToolAxis] ) )
			atctr = False
	elif sedm.ToolAxis == 1 : # Y
		if (posn[0] == 0)  and (posn[2] == 0):
			atctr = True
		else:
			CtrPogsn = ( ( 0,posn[sedm.ToolAxis],0 ) )
			atctr = False
	elif sedm.ToolAxis == 0:# X
		if (posn[1] == 0)  and (posn[2] == 0):
			atctr =  True
		else:
			CtrPosn = ( ( posn[sedm.ToolAxis],0,0 ) )
			atctr = False
#
def mkStairsL( cL):  # now  RufPtTupl is global;ly readable
	#
	s = sedm.CutDir # just a short name
	StairsL=[] # empty list to hold stairsteps
	WiglRADi = int(round(sedm.WiglRADf / sedm.xyzSCALEfOUT))
	TopStairLevel = ( s * WiglRADi)
	# 23.12.2025 REMEBNER rUFpTtUPL MAY BE RADI ABOVE 
	# DEEPEST PT OF STAIRS
	beg = abs(TopStairLevel)
	RufPtDepth = RufPtTupl[sedm.ToolAxis]
	BotStairLevel = RufPtTupl[sedm.ToolAxis] + TopStairLevel
	end = abs(BotStairLevel)
	#
	cLlen =  len(cL) # yes, not -1
	# loop runs from 0 to dpethOfStiars
	# DepthOfStairs is 
	# (deepest - topstairsDept)
	for i in range(0,(end - beg) + 1 ):  # 61):
		cLndx = i % cLlen
		deepNow = TopStairLevel + (i * s)
		# get 2 axis from cL pattern, 
		# calc ToolAxis posn, then store tupl
		tmpX, tmpY, tmpZ = cL[cLndx] 
		# retrieve but ignore toolaxis value
		# Next, calc ToolAxis posn
		if sedm.ToolAxis == 2: # Z G17  plabe is XY
			p=( tmpX, tmpY,  deepNow)
		elif sedm.ToolAxis == 1: # Y G18  plane is ZX
			p=( tmpX, deepNow, tmpZ)
		elif sedm.ToolAxis == 0: # X G19  plabe is YZ
			p=( deepNow, tmpY, tmpZ)
		#
		StairsL.append(p)
		#
	return StairsL
#
# TODO 05.02.2026 ugly   lotda ferad cats 
def doWiglLeadIn(liL):
	#
	#24.03.2026 chg mdx to wiglNdx
	wiglNdx = 0
	#
	lim = len(liL) - 1
	#
	sedm.JumpLtype = JumpOrbLeadInType
	# 25.03.2026  'e' missing from pin
	# ??? bad Geany? bbad memory? i have not esited near here for many days
	
	#print("JumpBoreType =1, JumpStairsType =2 JumpOrbLeadInType =3 JumpOrbPathType =4 JumpOrbPathDetour =5 NoJump = 10"
	#
	while 1: #
		nextDir = getEvalPV( liL,wiglNdx )# 0 indicates dont jump
		if sedm.QuitHit  == True:
			return BWD # well,QuitHit is bnetter name (nit taken)
		elif sedm.BwdMaxHit == True:
			return BWD # well,BwdMaxHit is better name ( but taken)
		else:
			if nextDir == FWD:  
				# FWD is towards liL[lim] (stairs top step)
				#
				if wiglNdx >= lim: # all done  if at lim and FWD
					# caller must look at rtn'd FWD 
					# and set state = CUPA
					return FWD # well, Done_Success is  better name
				#
				else:
					oldP = liL[wiglNdx]
					#25.03.2026 vvv there shoulf be no 'ndx'  asll were cleaned out yesterday
					wiglNdx += 1
					newP = liL[wiglNdx]
				#
			elif nextDir == BWD:   # BWD is tiwards lissr[0]
				#
				if wiglNdx <= 0:# if at badLim and  BWD
					# is ndx wronG
					sedm.BwdMaxHit = True # well , really BwdTooMuch is bettwer name
					#caller must look at rtnd BWD and set state CUPA
					return BWD; # well TooManyBwd is better name
				#
				else:
					oldP = liL[wiglNdx]
					wiglNdx -= 1
					newP = liL[wiglNdx]
		#
		SPO = ( ( 0,0,0 ) )
		
		doMove(newP)
#
def doPlunge(): 
	#
	SPO = ( (0,0,0) ) # Start Point Offset
	#
	#  decide use wigl or not
	"""
	# wigl is halpful 
	#  i sides  of toool get 'slimy' 
	#  the motion makes sides spark
	"""
	#
	if sedm.WiglRADf != 0:# else doBoreL( BoreL )
		WiglRADi = int(round(sedm.WiglRADf / sedm.xyzSCALEfOUT))
		if sedm.ToolAxis == 2:
			liLdest = ( (WiglRADi, 0, WiglRADi * sedm.CutDir) )
		elif sedm.ToolAxis == 1:
			liLdest = ( (WiglRADi, WiglRADi * sedm.CutDir, 0) )
		elif sedm.ToolAxis == 0:
			liLdest = ( (WiglRADi * sedm.CutDir ,WiglRADi, 0) )
		#
		# make a list of circle pts
		cLevel = liLdest[sedm.ToolAxis] # cLevel is ToolAxis dimension at lilDest
		cL =  mkcL( WiglRADi, cLevel )
		# make lead in line . dtartPyt to TopSair
		EntryPt = cL[0]
		# npow lead in line can be made
		liL = L3D( SPO,EntryPt)
		# mkStairss ,use cL as template,
		# and moces along ToolAxis for eah cell in template
		sL = mkStairsL(cL)
		# make StairsExitL loL ends at RufPtTupl
		# 29.12.2025 fix bad ]) closure
		wigLoL=L3D(sL[len(sL)-1],RufPtTupl)
		#######################
		#   now all mini paths have been planeed
		#  and verides  adjacent
		#  begin processing them using PV  FWD HOLD BWD
		
		
		#######################
		# power up and start cutting
		sedm.disableOsc = False # power on
		#
		####################
		# process WiglLiL StairsL WiglLoL
		#  NB  BwdMaxCount is handled in geTevalPV
		#  NOT in fo Move
		######################
		doWiglLeadIn(liL)
		#31.01.2026 ??? allow jump on ewigl leadin limne//\
		
		if (sedm.QuitHit == True) or (sedm.BwdMaxHit == True) :
			return BWD #caller must look foe fatal flag and set state accordingly
		# else: # liL success, begin StairsL
		#
		#process StairsL
		doStairsL( sL)
		# 31.01.2026 allow jump on stairs
		
		
		#
		if (sedm.QuitHit == True) or (sedm.BwdMaxHit == True) :
			return BWD #caller must look foe fatal flag and set state accordingly
		# else: # StairsL sL success, beginloL
		#
		# do  Exit L
		doExitL(wigLoL)
	#
	else: #WiglRADf == 0 so doBore  ( simplecut
		
		# make BoreL
		BoreL = L3D(SPO,RufPtTupl)
		#
		#05.02.2026 jump
		if sedm.JumpENA == True:
			#06.02.2026 this may be good palces
			# to sets pin JumpLtype
			sedm.JumpLtype = JumpBoreType #1
			
			#vvv dunno BPndx now,  putyin getEval
			# THIS BELONNGS IN GEETEVALPV  JumL = mkJupLJdnL(JumpBoreType,BoreL, BPndx)
			# no, let getREva;lPV get L LType BP
			#  and let getEvalPV call mkJupLJdnL
			# do i need an sedm.cutLndx??
			# already got sedm.JumpLtype
			# do I need global ThisCutL??
			
			mkJT() # sets sedm.endJT 
			# halmeter ahoew 1.401 # suspiciously like ET in etab + 1mS 
			# check the \yimenow' valu
			
			# use an LED or Halmeter to watch
		#power on
		sedm.disableOsc = False
		#
		# process BoreL
		doBoreL(BoreL)
		# allow jump on BoreList
		
		#
		#test fatal
		if (sedm.QuitHit == True) or (sedm.BwdMaxHit == True) :
			return BWD #caller must look foe fatal flag and set state accordingly
		# else: else dObreL() success
	# tool now at RufPtTupl
	# power off
	sedm.disableOsc = True
	return FWD
#
def getCutDir(): 
	if   sedm.FullDEPTHf > 0:
		sedm.CutDir = POS
	elif sedm.FullDEPTHf < 0:
		sedm.CutDir = NEG
	else:# else sedm.FullDEPTHf == 0
		msg ="1694 RufPt is same as StartPt"
		c = linuxcnc.command()
		c.error_msg(msg)
		raise SystemExit
#
def chkAtCtr(posn):
	atctr = False # guilty tiill proven innocent
	if sedm.ToolAxis == 2:
		if (posn[0] == 0) and ( posn[1] == 0): # if x and y are 0
			atctr = True
	if sedm.ToolAxis == 1:
		if (posn[0] == 0) and ( posn[2] == 0):# if x and z are 0
			atctr = True
	if sedm.ToolAxis == 0:
		if (posn[0] == 0) and ( posn[2] == 0):# if y and z are 0
			atctr = True
	return atctr
#
def mkRufPtTupl(): #called 1x per cut, setGen(25) before call
	global RufPtTupl
	sedm.ThisRADf = 0.0
	sedm.RADi     = 0
	#
	if sedm.CutDir == NEG:
		tmp = sedm.FullDEPTHf + sedm.MsrdUNSf # makes LESS neg
		tmp = tmp - sedm.RPlaneDist # more neg  to get CutDiost
		sedm.FullDEPTHf = tmp
		
		sedm.RufPtDEPTHf  = round(tmp,3)
		RufPtINT = int( round(tmp / sedm.xyzSCALEfOUT ))
	if sedm.CutDir == POS: # say cyt fro -15 to -10
		tmp = sedm.FullDEPTHf - sedm.MsrdUNSf # makes LESS neg
		tmp = tmp - sedm.RPlaneDist # more neg  to get CutDiost
		sedm.FullDEPTHf = tmp
		
		sedm.RufPtDEPTHf  = round(tmp,3)
		RufPtINT = int( round(tmp / sedm.xyzSCALEfOUT ))
	# 2nd create RufPtTupl
	if sedm.ToolAxis == 0:
		RufPtTupl = ( ( RufPtINT,0,0) )
	elif sedm.ToolAxis ==1:
		RufPtTupl = ( ( 0, RufPtINT,0) )
	elif sedm.ToolAxis == 2:
		RufPtTupl = ( ( 0, 0, RufPtINT) )
#
def doCtrSpo():# move tool to ctr then to StartPtOffset
	#
	posn = (  (sedm.XOffsetCmd , sedm.YOffsetCmd , sedm.ZOffsetCmd ) )
	atctr = chkAtCtr(posn)
	if atctr != True:
		# TODO isa RufPt correct for all cases??
		rcL = L3D(posn,RufPtTupl)
		doExitL(rcL)
		posn = (  (sedm.XOffsetCmd , sedm.YOffsetCmd , sedm.ZOffsetCmd ) )
	if posn != SPO:
		xL = L3D(posn,SPO)
		doExitL(xL)
#
def stop_ngc_program():
	c = linuxcnc.command()
	s = linuxcnc.stat()
	s.poll()
	c.abort()
	# if at first you dony abort  HAMMER it
	#if s.exec_state == linuxcnc.EXEC_RUNNING:
	while s.exec_state != linuxcnc.EXEC_DONE:
		print("Stopping current NGC program...")
		# HAMMER the abort command
		c.abort()
#
def doBoreL( BoreL ): # ,  destPt):
	#
	#24.03.2026 add set pin for jump typr
	sedm.JumpLtype = JumpBoreType
	#24.03.2026 chg ndx to boreBdx
	boreNdx = 0 
	lim = len(BoreL)-1
	
	# vvv brware JumpENA and JumpOn
	# JumpENA set in techGui, higher level than KumpOn
	# JumpOn set in code, ineach of 4 jump list typrs
	sedm.JumpOn = True # turn off after L complted/failed
	#
	while 1: # TODO while 1 is bad form, find a proper limit
		#
		# 07.01.2026 work jump imn at top of hgwile
		#
		nextDir = getEvalPV(BoreL,boreNdx )
		# test w 1st run NO jump chheckntn
		# and 3nd smae no chkbtn, 3nd hangs
		if (sedm.QuitHit == True) or (sedm.BwdMaxHit == True):
			sedm.JumpOn = False # turn off after L complted/failed
			return BWD
		#
		if nextDir != HOLD:
			if nextDir == FWD:
				BwdMaxCount = False
				if boreNdx == lim:
					sedm.JumpOn = False 
					# turn off after L complted/failed
					return FWD # ~ OK  
				else:
					boreNdx += 1 # doMove comes later
			elif nextDir == BWD:
				if boreNdx == 0: # at SPO and gap eval is BWD
					sedm.BwdMaxHit= True
					sedm.JumpOn = False
					# turn off after L complted/failed
					return BWD # 05.01.2026 new  had no ret vak
				else:
					boreNdx -=  1 # doMove comes later
			newP = BoreL[boreNdx]
			SPO = ( ( 0,0,0 ) )
			if newP == SPO:
				sedm.JumpOn = False
				# turn off after L complted/failed
				trap()
			doMove(newP)
	# all exitts have JumpOn saet False  
#
def doStairsL( StairsL):
	"""
	StairsL list begins at TopStep
	             ends at BotStep
	 FWD moveds down stairts
	 BWD call detor
	"""
	#
	#
	SPO = (0,0,0) 
	# 24.03.2026 chg ndx to sNdx
	sNdx = 0  
	GoodLim = len(StairsL)-1
	BadLim = 0# StairsL inclunde liL do sL[0] is startPt )also is SPO)
	EntryPt = StairsL[0]
	
	ctr = 0 # prob dteing backed up to sNdx 0 vd just stated at ndx0
	#
	sedm.JumpLtype = JumpStairsType
	
	sedm.JumpOn = True
	#
	while sNdx <= GoodLim:
		# vvv this will jump if needed
		nextDir = getEvalPV(StairsL,sNdx )
		#
		if (sedm.QuitHit   == True) or (sedm.BwdMaxHit == True):
			return BWD
		#
		if   nextDir == FWD:# FWD is toqerda BotStep (  dowqn stairs)
			if sNdx >= GoodLim: # >= BptStep
				return FWD # 12.02.2026 why return FWD why etn anythinf
			else:
				oldP = StairsL[sNdx]   # keep copy for adjancency tests
				sNdx += 1
				newP = StairsL[sNdx]
				#
				# 12.02.2026 vvv paranoia
				# vv re unresolved phantom lurchs to SPO, nevert triggerts, but i see AXIS trace
				if StairsL[sNdx] == SPO:
					print(1812,"in doStairsL   StairsL[ndx] == SPO")
					trap()
				#
				doMove( StairsL[sNdx] ) # more dlear meaning
		#
		elif nextDir == BWD:   # BWD is tiwards startposn
			bP = StairsL[sNdx] # BreaakPt
			#
			DetourWiglL, ankleNdx = mkDetourWiglL( bP )
			#
			doDetourWiglL( DetourWiglL, ankleNdx) 
			#
			# test fdatal flags
			if (sedm.QuitHit   == True) or (sedm.BwdMaxHit == True):
				return BWD
#
def doOrbitEntryLegL( legL, ankleNdx):
	#
	SPO = ( ( 0,0,0) )
	#24.03.2026 chg ndx to entryNdx
	entryNdx = ankleNdx
	#
	goodLim =len(legL) -1
	badLim = 0
	#
	#do vvv alays if sedm.JumpENA == True:
	sedm.JumpLtype = JumpOrbLeadInType
	sedm.JumpOn = True
	#
	while 1: # # seq is 
		#
		nextDir = getEvalPV( legL,entryNdx )
		#
		if (sedm.BwdMaxHit == True) or (sedm.QuitHit == True):
			sedm.JumpOn = False
			return # let CUPA move to ctr, move to SPO
		else:
			if nextDir == FWD:# FWD is towards RufPt
				if entryNdx >= goodLim:
					sedm.JumpOn = False
					return # no flags
				else:
					oldP = legL[entryNdx]
					if entryNdx > 0:
						entryNdx += 1 
			elif nextDir == BWD:
				if entryNdx <= badLim : # backed up to SPO
					sedm.BwdMaxHit = True
					sedm.JumpOn = False
					return
				else:
					oldP = legL[entryNdx]
					entryNdx -= 1 
			#
			if nextDir != HOLD: # move but stay in limits
				newP = legL[entryNdx]
				#
				doMove(newP)
#
def doDetourWiglL( DetourWiglL,ankleNdx):
	
	# 01.01.2026 i thought itrapped ctr ==0 and thidzDir == BWD
	# i thought that condition was related to
	# 'tall skinnmy Tri" vs wanted Strairs
	# buit isee no such trap.test, so implement it
	# and run a loop test
	#
	ctr = 0 # 01.01.2026 trapTri test
	
	#arrgh
	# FWD must DEC mdx towards toe
	# BWDF must INC mdx towrads HIP
	#
	SPO = (0,0,0)
	#24.03.2026 vhg ndx to wiglDtrNdx
	wiglDtrNdx = 0 #
	
	posnb = DetourWiglL[wiglDtrNdx]
	#posn = ( ( sedm.XOffsetCmd , sedm.YOffsetCmd , sedm.ZOffsetCmd ) )
	
	goodLim = 0 # toe
	badLim = len(DetourWiglL)-1 # hip
	
	sedm.JumpLtype = JumpStairsType
	while 1: # begin lpp[
		#
		#07.01.2026 work jump into yop of while loop
		#

		#
		nextDir = getEvalPV( DetourWiglL,wiglDtrNdx )
		#
		if nextDir == FWD:   # FWD is GOOD  butr nmust DEC ndx
			wiglDtrNdx -= 1 #
			if wiglDtrNdx <= goodLim:
				return FWD # success
		#
		elif nextDir == BWD:   # BWD INCs ndx towards SPO  BAD
			oldP = DetourWiglL[wiglDtrNdx] # where tool was on entry to this dunc
			wiglDtrNdx += 1 # BWD INCs the ndx towards HIP SPO

			#01.01.2026 new
			ctr += 1
			#
			if wiglDtrNdx  >= badLim: #
				sedm.BwdMaxHit = True
				return BWD # yes FWD means successm bad wors, correct axtion
			#
			newP = DetourWiglL[wiglDtrNdx] # get  newP from list, ndx already INCd
			doMove( newP)
#
def doDetourLegL( legL, BegNdx): # wasankleNdx):
	#
	# BegNdx is 0 when making detour from peri
	SPO = ( ( 0,0,0) )
	#
	#24.03.2026 chg ndx to detourLegLndx
	detourLegLndx = BegNdx #maybe ankle, maybe toe,    wasankleNdx # Begin at ankl ndx
	goodLim =0  #TOE
	badLim =  len(legL) -1 # HIP
	
	#stepNum = 0
	#
	sedm.JumpLtype = JumpOrbPathType
	#
	while 1: # # seq is 
		#
		#07.01.2026 work jump into yop of while loop
		#
		nextDir = getEvalPV( legL,detourLegLndx )# 0 means  DONT JUMMP
		# check fataal flags
		if (sedm.BwdMaxHit == True) or (sedm.QuitHit == True):
			return BWD# let CUPA move to ctr, move to SPO
		#
		if nextDir == FWD:# FWD is DEC   towards TOE, towards ndx == 0
			if detourLegLndx == 0:  #aka goodLim: # ndx 0  is TOE
				return# ok retn to caller, we all done w fetote and gotr a FWD PV   no flags # no rtn value  needed
			else: #FWD and ndx != 0, not at TOE
				#oldP = legL[ndx]
				####### legL is list w [0] = toe
				######## legL[badLim == SPO)
				######## ??? is legL[-1] == SPO???   YES
				######## DONT DEC ndx if ndx == 0
				detourLegLndx -= 1 # FWD is DEC  towards TOE towrads PeriPath Towars ndx == 0
				newP = legL[detourLegLndx]
		elif nextDir == BWD:   # BWD is INC ndx  BWD is tiwards SPO , ndx  gets larger
			if (detourLegLndx == badLim): # and  (stepNum != 0): # backed up to SPO
				sedm.BwdMaxHit = True
				return
			else:
				#oldP = legL[ndx]
				detourLegLndx += 1 # BWD is towards HIP. ndx is larger
				newP = legL[detourLegLndx]
		if (nextDir != HOLD):
			
			""" re TST nevcer tyrihghered
			if newP == SPO:
				print(1731,"in doDetourLegL newP == SPO|")
				trap()
			"""
			doMove( newP )

# ......... end funcs needing Hump code
#
sedm = mksedmcomp()


try: # sedm preparation 
	# vvv initl state of state machine
	sedm.state = WaitFullDepthRplaneDist
	#
	# i needed a while for the try, EDMmode is  ON:Y uised to keep the while open
	EDMmode = True 
	time.sleep(0.10) # nECC as well as QBreaker
	sedm.xyzSCALEfOUT = sedm.xyzSCALEfIN
	sedm.mlt = 1/sedm.xyzSCALEfOUT
	
	getPgmUnits() # should ONLY be called 1x per pgm ( tho user could tryt G20 G21 G20 G31 blah
	#
	sedm.ctr = 0
	
	while EDMmode == True:
		if sedm.isEna == True: # set by M199  clrd by M198
			# vvv new 11.12.2025  reset at top, not bot
			#sedm.ctr = 0
			
			# peek rwtr wuit btns enabled at start up  NG
			# so wait isEna
			# ng  all eNOT greyed at startup
			# ~ok catch22
			#  when panel si loaded, the btns are enabled
			# and the nets needed tro DISable them are not yet enabled by M199
			# so TODO fix btns ena too eartly
			# BTW PEEK enable as soon as 1st getEvalPV()_ ca;;ed
			# these vvv 2 lines are va
			sedm.BwdMaxHit = False
			sedm.BWDcount = 0
			
			time.sleep(0.10) # nECC as well as QBreaker
			# vvv call 1s
			if sedm.NR == sedm.BegNR:
				getStartPtF()
			#
			sedm.disableOsc = True
			
			
			
			#----------------------------------------
			#----------- begin state machine --------
			#----------------------------------------
			if sedm.state ==  WaitFullDepthRplaneDist:
				
				
				#25.11.2025 this is top of state machuien for subsequent iters
				time.sleep(0.1)
				
				# M162 sets FullDEPTHf and RPlaneDist
				sedm.restart = False
				
				# vvv doews NOT use mly
				getToolAxis() # ths need to run for each tool change
				
				if sedm.RPlaneDist != 0:
					#vvv asets
					#	sedm.CutDir
					#	sedm.FullDEPTHf = sedm.FullDEPTHf + sedm.RPlaneDist
					getCutDir() # also combine RPlane and FullDpeeth tto  make CutTraavel
					#
					sedm.state = WaitOrbitTypeWiglRADf 
				#
				else: # sedm.RPlaneDist == 0:
					# FAIL becuz RPlaneDisst == 0
					msg = "sedm.RPlaneDist = 0"
					msg = msg + str(sedm.RPlaneDist)
					c = linuxcnc.command()
					c.error_msg(msg)
					raise SystemExit
			#
			if sedm.state == WaitOrbitTypeWiglRADf:
				# test OrbitType valid
				if (sedm.OrbitType == CIRCLE) or (sedm.OrbitType == SQUARE) :
					# ^^^ must be circle or sqr for now 26.11.2025
					#
					# test WiglRADf valid ( >=0)
					# M163 sedm.OrbitType sedm.WiglRADf vi dignals
					if sedm.WiglRADf >= 0:#15.11.2025 vhg to >=  hung at 0
						sedm.state = WaitEttabNumberMsrdUNS
			#
			#TODO 25.11.2025 no testing dict creation success
			if sedm.state == WaitEttabNumberMsrdUNS:
				# 
				# Path to ETAB is \local'
				etabPath = "./etabs"#25.02.2026 <<<  make etab path ='HERE'
				#
				if (sedm.EtabNum > 0):
					# construct file name from number
					EtabNumStr = str(sedm.EtabNum) #"99999944"
					fqfn = etabPath + "/" + EtabNumStr +".enc"
					#
					# read file, make dict of lists made from lines in file
					parseEtab(fqfn) 
					#
					sedm.state =  WaitBegEndNR
			#
			if sedm.state == WaitBegEndNR:
				if(sedm.BegNR != 0) and (sedm.EndNR != 0 ):
					if (sedm.EndNR <= sedm.BegNR)and(sedm.EndNR > 0):
						if sedm.BegNR !=  25:
							sedm.PlungeOrbitWanted = True
						# dont progress until EndNR <= BegNR
						# the PlungOrnbWantewds is a side issue
						sedm.state = WaitPitch
					#
					# I dont handfle plungeOrbWanted correctly
					#  ... dont undeterdtansd yet...
			#
			if sedm.state == WaitPitch: # WaitPitch is state 5
				#15.02.2026 pitch is not used now
				# so not good to wait for M???
				# so i shoirt shank this state
				sedm.state = WaitGenReady
			#
			# TODO JumpENA is BIT, will never be -1
			#if sedm.state == WaitJumpwANTED:
			#	if sedm.JumpENA != -1:  #insist M166 is used( oper must say he wants.doersmnt want jump)
			#		sedm.state = WaitGenReady
			#
			if sedm.state == WaitGenReady:# i need UNS to caLC
				# ThisRADf, SO NAME GENREADY MISLEADING MORE LIKE # # GENPrepared MAYBE
				#vvv makes UNSf, sedm.RufPtDEPTHf, RufPtTupl, 
				if sedm.ThisNR == sedm.BegNR:
					# temp set ThisNR = 25 to get UND asnd???
					# reset ThisNR to BegNR afterwards
					
					
					# this chink is dfor spcl case
					#  where BegNR != 25
					#  and mkThusRAD and UND not normally called
					#  so, ThisNR is LIEF to, just to get UNS
					# bur 25.02.2026 in new scheme
					#  thhe etavDixr['25'][11] hol;ds UNS ( tho collumn hdr sez RAD
					sedm.ThisNR = 25
					setGen(sedm.ThisNR)
					mkThisRADf() # get UNS 
					
				else: # 15.02.2026 thius line was missing
					# reset to BegNR
					sedm.ThisNR = sedm.BegNR
					
				#
				mkRufPtTupl()# the 25 could be embedded inside mkRufPtTupl, but keeping it outside shows better
				
				setGen(sedm.BegNR)
				sedm.UNSf = EtabDict['25'][11]
				sedm.GenReady = 1 # domt say True  it can be -1 0 or 1
				sedm.state = WaitPlunge
			#
			if sedm.state == WaitPlunge:
				sedm.disableOsc = False # turn ON power to tll
				doPlunge()              # main entry to plunge
				# we are done with NR 25, so dec ThisNR
				if sedm.BegNR == 25:
					sedm.ThisNR -= 1
					
				#else leave nr alone,
				#  user may wantplungeOrb when BegNR != 25
				sedm.disableOsc = True # turn OFF power to tool
				# duting DoPlunge some fatal falgs may have been set
				if sedm.QuitHit == True:
					sedm.state = CleanUpPutAway # handle fatal flag, exit clean
				elif sedm.BwdMaxHit == True:
					sedm.state = CleanUpPutAway # handle fatal flag, exit clean
				else:
					sedm.state = WaitDoPlungeOrbit
			#
			# if here Gen NR == ThisNR < 25
			# TODO 13.12.2025 state can be removed
			if sedm.state == WaitDoPlungeOrbit: # MISSING 03.12.2025
				sedm.state = WaitAllNRsDone
			#
			if sedm.state == WaitAllNRsDone: # 13  NRs remaining are ORBITS
				if (sedm.QuitHit == True)or(sedm.BwdMaxHit == True):
					sedm.state =  CleanUpPutAway
					# ??? break??? no rtn in state mc
				else:# no fatalflags
					if (sedm.ThisNR < sedm.EndNR):#  all NRs are done, 
						sedm.state = CleanUpPutAway 
					else: #else do more orbits, ThisNR  IS NOT   EndNR, so  do more orbits
						#
						setGen(sedm.ThisNR) # get power back on
						
						sedm.disableOsc = False
						# get paths: legEntryL  legL opL
						mkThisRADf()
						#
						cLevel = RufPtTupl[sedm.ToolAxis] + (sedm.RADi * sedm.CutDir)
						opL = mkOrbitPathL(sedm.RADi,cLevel)
						entryPt = opL[0]
						#
						legL,ankleNdx = mkOrbitEntryLegL(entryPt)
						#
						ankleTupl = legL[ankleNdx]
						legL.reverse()
						ankleNdx = legL.index(ankleTupl)
						#
						# sedm.JumpLtype is set inside doOrbitEntryLegL() to 3
						doOrbitEntryLegL(legL,ankleNdx) #
						#
						
						# test for fatal flags
						if (sedm.BwdMaxHit == True) or (sedm.QuitHit == True):
							sedm.state = CleanUpPutAway # let CUPA move to ctr, move to SPO
						else:
							#>>> need to set sedm.jumpltype???
							# sedm.JumpLtype sets to JumpOrbPathType
							doOrbL(opL)# whwrw does doOrbL end???
							#
							if (sedm.QuitHit == True)or(sedm.BwdMaxHit == True):
								sedm.state = CleanUpPutAway
							else: # turn off power, dec ThisNR
								sedm.disableOsc = True
								sedm.ThisNR = sedm.ThisNR - 1
								# any more NRstoprocess???
								if sedm.ThisNR < sedm.EndNR:
									sedm.state = CleanUpPutAway
			#
			if sedm.state == CleanUpPutAway : # ALSO SUCCRESS
				SPO = ( (0,0,0) )
				doCtrSpo()
				c = linuxcnc.command()
				sedm.disableOsc = True
				sedm.isEna = False
				sedm.BWDcount = 0
				#
				setGen(0)
				sedm.state = WaitFullDepthRplaneDist
				sedm.ctr +=  1 # report count ( for multiple cut loops )
				if sedm.QuitHit == True:
					sedm.QuitHit = False
					msg = "Operator Aborted"
					c.error_msg(msg) # stop_ngc_program()
					stop_ngc_program()
				elif sedm.BwdMaxHit == True:
					sedm.BwdMaxHit = False
					msg = "BwdMaxHit True"
					c.error_msg(msg)
					stop_ngc_program()
				else: # Succesful completion
					c.auto(linuxcnc.AUTO_RESUME)
#
except KeyboardInterrupt:
	raise SystemExit
