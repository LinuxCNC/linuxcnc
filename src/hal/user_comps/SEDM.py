#!/usr/bin/python3.11
#07.03.2026 cleaned ~/Doenloads/SEDM of crudt
#  chgd .ini to use ~/Download/SEDM/Mcodes for mcodes
# TODO backup 
#    docstrings
#    get new clone of linuxcnc
#    add my SEDM to that
#     test
#     if ok, notify devs of intent
#     commit chgs.pulll rtequest ( jibberish GIT speak )
#     see gut howto from google
#
# TODO 26.02.2026
#  DONE BAVKUP
#  DONE clean up dbug prints
#  edit all test pgms ro use new etab file fdormat
#  auto edit rest of etabs to new format
#
# 25.02.2026 now using new etab format
#  work znegstraight zneg1orn usimg 19/ENC yable
#
# FIXED in SEDM.xml
#    SPA can look like int, not so wide
#
# 22.02.2026 work onn etab ccnvrsn
""" futue: meaninmhful 1st line inm Etabs
# Note: 1st line of etb can have info
# cmpl  GR+ ST-10mmsq  
"""

""" future: make fname dreom dfialogs
# The old naming 999999nn is not useful
#  it was a workaround for file naming whencnc has no alpha keys
#  and hal cant handle string file name
#  The fdile name might be bvuilt from fialogs
#  lkike  Tool mat? pick one GR CU WoCo GrCu Brass, Alum Steel
#         Work Matl ? St CU Alu 
#       Fp area ? pick from list 
#       Form  pick one  rin subgate  pick a numebr fort,m list//
#   these anf othe q's woul generate afile name
#   (hopefully in library )
"""

#
"""
#22.02.2026 many days on hdwr
# now back to cnvrt etabs to new dornat
# 1st new qwaS 99999919->88888819
# i SEE THE ETAB HAS LAST FEW ELEMENTYS UNUSED  vdi ve vw
# I sont have good values dore tehm\#
# so ??? keep -'s in etab or remove 'slots'
# I sup[pose fdor lonmg term. 
# the valyues sjould be left and populated later
#
"""

# 15.02.2026 cleaned ngc file of unused m codes

# 15.02.2026 pitch removed
#   M197 fixed, had bash -eq when it should be == ( dore string)
#
# 15.02.2026 IF jump was off
#   1st run of 1EWiglZneg has no jump, screws down ok
# BUT if vhkBtn JumpENA is left OFF
# and a 3nd runs is started
# then the checkntn get marked amd Jump is ON
# The chkbtn is set just before end pof 1st pgm run
#
#15.02.2026 1WiglZneg womt run 2nds time if jump not on
#
# 13.02.2026 TODO meaninggull triple quotw indos 

#
import linuxcnc
import hal
import time
import sys, subprocess
import os.path
from random import uniform
# SEDMhdrs.py must be in /usr/lib/python3.11
#   or    ~/yourRIP/lib/python    
from SEDMhdrs import *

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
	WiglRADi = int(round(SEDM.WiglRADf / SEDM.xyzSCALEfOUT))
	# get ctrLevel, above breakPt by ThisRA
	ctrLevel = bP[SEDM.ToolAxis]  - (WiglRADi * SEDM.CutDir) 
	# get anklePt
	if SEDM.ToolAxis == 2: # Z
			anklePt = ( ( 0,0,ctrLevel ) )
	if SEDM.ToolAxis == 1: # Y
			anklePt = ( ( 0,  ctrLevel, 0 ) )
	if SEDM.ToolAxis == 0: # X   13.02.2026 damn had = 2 and spent a day
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
	if SEDM.JumpLtype == JumpBoreType:
		AJDi = int(SEDM.AJD *1000)
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
	if SEDM.JumpLtype == JumpStairsType:
		#            0.050  *1000  = 50
		AJDi = int(SEDM.AJD * 1000)
		
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
	if SEDM.JumpLtype == JumpOrbLeadInType:
		#13.02.2026 TODO reduce   same as BoreL
		
		AJDi = int(SEDM.AJD *1000)
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
	if SEDM.JumpLtype == JumpOrbPathType:
		#
		AJDi = int(SEDM.AJD *1000)
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
def mkJT(): # sets SEDM.endJT
	"""
	creates float 
	repressenting when next Jump 
	should ha[[en
	"""
	Tnow = time.time()
	# vvv these are all floats
	SEDM.endJT = Tnow + SEDM.ET
#
#05.02.2026 TODO small enuf to so inline
def chkJT(): # run this every loop while cuttinmg
	"""
	for Jump cycles:
	reports if Cut Time is over
	and it is time to Jump
	"""
	Tnow = time.time()
	timesUp = (Tnow >= SEDM.endJT)
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
	
	f =  SEDM.freebies # avoid looking it up 3x
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
def mkSEDMcomp():
	"""
	creates the LinuxCNC hal component
	for orbiting sink EDM.
	"""
	h = hal.component("SEDM")
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
	# defines in SEDMhdrs.py
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
	global SEDM # some SEDM pins gets chgd
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
		SEDM.PgmIsMM = False
		SEDM.xyzSCALEfIN = 0.0001
		SEDM.xyzSCALEfOUT = SEDM.xyzSCALEfIN
	else:# else tmp == 1 meaning METRIC
		SEDM.PgmIsMM = True
		SEDM.xyzSCALEfIN = 0.001
		SEDM.xyzSCALEfOUT = SEDM.xyzSCALEfIN
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
	global SEDM
	#
	s = linuxcnc.stat()
	s.poll()
	
	# get G17 G18 G19 info 
	t= int(s.gcodes[3])
	if t == 190:
		SEDM.ToolAxis = 0 # for X is toolAxis
	elif t == 180:
		SEDM.ToolAxis = 1 # for Y is ToolAxis
	elif t == 170:
		SEDM.ToolAxis = 2 # for Z is ToolAxis
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
	SEDM.xsp = SEDM.xFBf  # x posn feedback NOE
	SEDM.ysp = SEDM.yFBf
	SEDM.zsp = SEDM.zFBf
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
	
	The SEDM system is an automatically
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
		SEDM.NR =    NRval # local storage on SEDM pin
		SEDM.IP =    EtabDict[NRstr][0]  #'peak' current
		SEDM.P  =    EtabDict[NRstr][1]  #Pollarity
		SEDM.HV =    EtabDict[NRstr][2]  #ignitionVoltage
		"""
		 a milliamp supply used to initialize the spark,
		 a higher HV makes it easier for system to 'see' 
		 on smooth surfaces, where the highest point is 
		 harder to distiguish
		"""
		#
		# TODO vvv for real use
		#SEDM.GVHI = EtabDict[NRstr][3]
		#SEDM.GVLO = EtabDict[NRstr][4]
		# TODO  vvv for testing
		SEDM.GVHI =   50#35#40# 45# 2 #45 #
		SEDM.GVLO =   40#28#30# 35# 1 #40 #
		#
		SEDM.TON =   EtabDict[NRstr][5]  
		SEDM.TOF =   EtabDict[NRstr][6]  
		SEDM.ISO =   EtabDict[NRstr][7]  #in IsoPulse mode,
		#
		SEDM.AJD =   EtabDict[NRstr][8] 
		# 'peck' cycle jump distance, decimal mm
		SEDM.ET  =   EtabDict[NRstr][9]
		# 'peck' cycle cut duration, decimal Secs
		#
		SEDM.BWDmax =  EtabDict[NRstr][10]
		# max number of contiguous low voltage samples,
		# eceeding will cause abort and return to start point
		#
		SEDM.RADf    = EtabDict[NRstr][11]  #radius per side
		SEDM.SPAf   =  EtabDict[NRstr][12]  #spherical step angle
		# for sphertical orbit
		#  a hemisphere is a stack of circles
		#  these data are placeholders, neyonf my ability 
		#  to measueree and erecord
		SEDM.VEf  =   EtabDict[NRstr][13] #electrode wear
		SEDM.VWf  =   EtabDict[NRstr][14] #MMR metal removal rate
		SEDM.VDIf =   EtabDict[NRstr][15] #surface roughness
		#
		SEDM.ThisNR = NRval # non zero
	#
	else: # NRval == 0 means clear the gennrator
		NRstr = "0" #str(NRval)
		#
		SEDM.BWDcount    = 0
		SEDM.CutDir      = 0
		SEDM.EtabNum     = 0
		SEDM.BegNR       = 0
		SEDM.EndNR       = 0
		#
		SEDM.FullDEPTHf  = 0
		SEDM.MsrdUNSf    = 0
		SEDM.RADf        = 0
		SEDM.RufPtDEPTHf = 0
		#
		SEDM.state       = 0
		#
		SEDM.xsp         = 0
		SEDM.ysp         = 0
		SEDM.zsp         = 0
		#
		#data  specific to Etaab and TechGui
		SEDM.NR          = 0
		SEDM.IP          = 0
		SEDM.HV          = 0
		SEDM.P           = 0
		SEDM.ISO         = 0
		SEDM.BWDmax      = 0
		SEDM.AJD         = 0
		SEDM.ET          = 0
		#
		#31.01.2026 TON TOF are integer uSec
		SEDM.TON         = 0
		SEDM.TOF         = 0
		#
		SEDM.GVHI    = 0
		SEDM.GVLO    = 0
		#
		SEDM.SPAf        = 0
		#
		SEDM.VEf         = 0
		SEDM.VWf         = 0
		SEDM.VDIf        = 0
		#
		# TODO vvv
		# vvv **** spcl values NOT set to 0
		#25.11.2025 make sure user answered Pitch w value >=0
		SEDM.Pitch    = -1
		#
		SEDM.ThisNR      = -1 # after clean up
		#
		SEDM.inpo     = False
		SEDM.isEna    = False
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
			
			
			
			
			"""#
			#/////// for new
			#25.02.2026 FOR NEW STYLE ETABS,,,
			# OLDNOTE  skip #7  AJD   8 ET  15 RAD  16  SPA
			# NEWNOTE 24.02.2026 does lineParts start w NR or IP, 
			#  thats would chg all ndxsd  
			# NEW NOTE 24.02.2026 need to chg code for 3 indices
			# NEW NOTE 24.02.2026 new schem  8 AJD 9 ET  11 RAD 12 SPA
			# NEW NOTE 24.02.2026    those are the only floats in
			#    new xsheme
			# 25.02.2026 ndxs 7 8 15 16 
			#  are for lists beginning at IP
			# ? in 19/WBC got 15 16 look ok? 
			# RESULT [0] is val of IP (noy NR)
			# i say 8 AJD  9 ET   11 RAD  12 SPA  are floats in the dile (* mno cnvrsb needed)
			if ( (lPartNdx  != 8) and ( lPartNdx  != 9) and ( lPartNdx  != 11)   and (lPartNdx  != 12) ):
				tupl=tupl+( int(ftmp),) # weird   comma to make it
				#  a tuple so iy can be concvatenated
			#
			else:
				tupl=tupl+( ftmp, )
			# -----
			////// for new
			#"""#




			#"""#
			#\\\\\\ for old
			if ( (lPartNdx  != 9) and ( lPartNdx  != 10) and ( lPartNdx  != 12)   and (lPartNdx  != 13) ):
				tupl=tupl+( int(ftmp),) # weird   comma to make it a tuple so iy can be concvatenated
			else:
				tupl=tupl+( ftmp, )
			
			#/// end old
			#"""#




		#
		if tupl != ():
			numLines+=1
			EtabDict[ lineParts[0] ] = tupl[0:] # 25.02.2026 isnt [9:] same as []??
			# looked ok rint(9741,EtabDict[ lineParts[0] ] )
			"""
			print()
			print(989,EtabDict[lineParts[0]][9])
			print(989,EtabDict[lineParts[0]][10])
			print(989,EtabDict[lineParts[0]][12])
			print(989,EtabDict[lineParts[0]][13])
			print()
			# NR 25 got
			#3.0 10 0.0 0
			#NR 24 got
			#3.0 10 45.0 0
			#NR 12 got
			#3.0 10 3.0 0
			#which are
			# ET CB  SPA VDI
			"""
			
			"""
			# I want 8 9 11 12
			print()
			print(989,EtabDict[lineParts[0]][8])
			#print(989,lineParts)
			# got 989 ['25', '91', '0', '0', '40', '30', '440', '212', '0', '2.032', '3.0', '10', '0.635', '0.0', '0', '0', '0']
			# so lineparts begins at NR so interesting vcalue is at
			# EtabDict index + 1
			print(989,lineParts[9])
			#trap()
			
			print(989,EtabDict[lineParts[0]][9])
			print(989,lineParts[10])
			print(989,EtabDict[lineParts[0]][11])
			print(989,lineParts[12])
			print(989,EtabDict[lineParts[0]][12])
			print(989,lineParts[13])
			# either the list begins at NR or ...
			# those ^^^ data are correct 
			# check 0yh to see if EtabDict[NR][0] is NR (25) or IP (~100)
			#print(989,EtabDict[lineParts[0]][0])
			# NB this is LineParts[] not EtabDict
			# i get IP values  not NR valyes
			# result YES Etab9 is lineParts9
			print()
			"""
#
def mkThisRADf(): # used every ThisNR EXCEPT 25  ( handled by mkRufPtTup/// l)

	SEDM.XtraRADf =  round(SEDM.MsrdUNSf - SEDM.UNSf,3)

	if SEDM.ThisNR != 25: # call herte b4 dec'd
		SEDM.ThisRADf = SEDM.RADf + SEDM.XtraRADf
		# now make an INT of 'steps' in ThisRADf
		# store it in SEDM.RADi
		x = SEDM.ThisRADf
		# SEDM.xyzSCALEfOUT or IN are .002 for MM and .0001 for Inch
		x = x * (1/SEDM.xyzSCALEfOUT)
		x = round(x)
		SEDM.RADi = int(x)
	else: # ThisNR == 25
		SEDM.UNSf = SEDM.RADf
		SEDM.ThisRADf = 0.0 # there is no orbit on NR 25, only wigl
		SEDM.RADi = 0 # there is no orbit on NR 25, only wigl
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
def getEvalPV( Ltype,L,ndx ): # rtns FWD HOLD BWD for EDM
	# TODO06.02.2026 xhg pnow  s tp l[ndx]
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
	similare tro an .ini file ( called SEDMhdrs,py)
	
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
	SEDM.QuitHit = False; #no lingetring flags
	#
	# ceck if time to jump
	if (SEDM.JumpENA == True) and (SEDM.JumpOn == True):
		t2jump = chkJT()
		if t2jump:
			JupL,JdnL = mkJupLJdnL(L,ndx)
			doJump(JupL,JdnL)
			mkJT() # make a new endJT
			#
	if SEDM.EDpeek == True:
		SEDM.disableOsc = True      # power off asap , during tool withdrawl
		
		# return began to work, but at bP it rtnd to SPO
		# i trhibnk becus PEEK still active
		# ao tyurn it off like the other btns get turnmed off
		# YAY peek return quit work 
		SEDM.EDpeek = False
		
		peekL = []
		retL = []
		peekL, retL = mkPeekL( L[ndx] )
		doExitL(peekL)
		#
		# at SPO, user just did PEEK
		while 1: 
			if SEDM.EDquit == True:# vvv already at SPO becuz ^^^
				SEDM.disableOsc = True
				SEDM.QuitHit = True
				SEDM.EDquit = False #  release  btn
				return BWD #retval is bogud,  caller must test Quit and Return before eval
			if SEDM.EDreturn == True :# more readble than elseif ,
				#  the reason why is not hidden
				
				# this vvv setgen is dore RETURN after PEEK
				setGen(SEDM.ThisNR) #  maybe useless. unnecc
				
				SEDM.disableOsc = False
				doExitL(retL)# rwtL was made during Peek hanfler
				SEDM.EDreturn = False # 05.01.2026 missimg turn of btn 
				#
				
				# NO DONT RETURN JUDST CONTINUE return FWD # cade in ca;;er excpect FWD to continue
				# well don t leave thid func, cont into get PV
				#break # tricky with frerebies, well freebies is 20 now
				
				return FWD
		#end while 1
	#end if SEDM.EDpeek == True
	#/// can i get return wokinmg 
	elif IsFreebie(L[ndx]) == True:
		return FWD # was EDMgrade = FWD
		#ng EDMgrade = FWD # get return continuing???
	#///
	else: # else PEEK was not pressed se use PV
		pv = uniform(GVMIN, GVMAX)# GVMAX GV<IN from SEDMhdrs.py
		if   pv > SEDM.GVHI: # HIGVLIM  in SEDMhdrs.py 
			SEDM.BWDcount = 0
			#29.12.2025 wasreturn FWD
			EDMgrade = FWD
		elif pv < SEDM.GVLO:# LOGVLIM  in SEDMhdrs.py 
			SEDM.BWDcount += 1
			if SEDM.BWDcount >= SEDM.BWDmax:
				SEDM.BwdMaxHit == True
			EDMgrade = BWD
		else:
			# 29.12.2025 was return HOLD # caller can ignore it
			EDMgrade = HOLD
		# common exit for PV
		SEDM.EDMgrade = EDMgrade
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
	if SEDM.ToolAxis == 2:    #Z
		cZ = EntryPt[2] 
		cZ -= SEDM.RADi * SEDM.CutDir
		CtrPt = ( ( 0, 0, cZ)  )
	elif SEDM.ToolAxis == 1:  #Y
		cY =EntryPt[1] 
		#10.02.2026 vvv i used  -=  for toolAxis =2
		#cY += SEDM.RADi * SEDM.CutDir
		cY -= SEDM.RADi * SEDM.CutDir
		CtrPt = ( ( 0, cY, 0 ) )
	else:                     #X
		cX = EntryPt[0] 
		#10.02.2026 vvv i used  -=  for toolAxis =2
		#cX += SEDM.RADi * SEDM.CutDir
		cX -= SEDM.RADi * SEDM.CutDir
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
	if SEDM.ToolAxis == 2:
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
			ctrPosn = ( (0,0, pNow[SEDM.ToolAxis] - (SEDM.RADi * SEDM.CutDir) )  )
	elif SEDM.ToolAxis == 1:
		if (pNow[0] == 0) and (pNow[2] == 0):
			xL = L3D( pNow, SPO)
			rL = L3D( SPO,pNow)
			
			#test asjacenvy
			return xL,rl
		else:# else NOT at ctr
			#27.11.2025 subtract
			ctrPosn = ( (0, pNow[SEDM.ToolAxis] - (SEDM.RADi * SEDM.CutDir) ,0) )
	elif SEDM.ToolAxis == 0:
		if (pNow[1] == 0) and (pNow[2] == 0):
			xL = L3D( pNow, SPO)
			rL = L3D( SPO, pNow)
			
			# test adjacenvy
			return xL, rL
		else:# else NOT atr ctr
			#27.11.2025 subtract
			# 12.02.2026 was
			#ctrPosn = ( (posn[SEDM.ToolAxis] - (SEDM.RADi * SEDM.CutDir),0,0) ) 
			ctrPosn = ( (pNow[SEDM.ToolAxis] - (SEDM.RADi * SEDM.CutDir),0,0) ) 
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
		if SEDM.ToolAxis == 2: #Z  plnne is XY
			tupl = (a,     b,      cLevel )
		
		
		elif SEDM.ToolAxis == 1: #Y  plane is ZX
			tupl = ( a,    cLevel, b)
		elif SEDM.ToolAxis == 0: #X  plane is YZ
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
	for ndx in range(l-1,  -1, -1):#  loop bwds thri octant 1 data
		if SEDM.ToolAxis == 2: #Z G17
			nux = oct1L[ndx][1]
			nuy = oct1L[ndx][0]
			nuz = oct1L[ndx][2]
			tupl=( (nux,nuy,nuz) )
		elif SEDM.ToolAxis == 1: #Y G18
			nux = oct1L[ndx][2]
			nuy = oct1L[ndx][1]
			nuz = oct1L[ndx][0]
			tupl=( (nux,nuy,nuz) )
		elif SEDM.ToolAxis == 0: #X G19
			nux = oct1L[ndx][0]
			nuy = oct1L[ndx][2]
			nuz = oct1L[ndx][1]
			tupl=( (nux,nuy,nuz) )
		#
		oct2L.append(tupl)
	#
	# ------- beg octant 3
	oct3L = []
	for ndx in range(l-1,  -1, -1):#  loop bwds thri octant 1 data
		if SEDM.ToolAxis == 2: #Z G17
			nux = oct2L[ndx][0]
			nuy = oct2L[ndx][1]
			nuz = oct2L[ndx][2]
			tupl=( (-nux,nuy,nuz) )
		elif SEDM.ToolAxis == 1: #Y G18
			nux = oct2L[ndx][0]
			nuy = oct2L[ndx][1]
			nuz = oct2L[ndx][2]
			tupl=( (-nux,nuy,nuz) )
		elif SEDM.ToolAxis == 0: #X G19
			nux = oct2L[ndx][0]
			nuy = oct2L[ndx][1]
			nuz = oct2L[ndx][2]
			tupl=( (nux,-nuy,nuz) )
		#
		oct3L.append(tupl)
	# ------- beg octant 4
	oct4L = []
	for ndx in range(l-1,  -1, -1):#  loop bwds thri octant 1 data
		if SEDM.ToolAxis == 2: #Z G17
			nux = oct3L[ndx][1]
			nuy = oct3L[ndx][0]
			nuz = oct3L[ndx][2]
			tupl=( (-nux,-nuy,nuz) )
		elif SEDM.ToolAxis == 1: #Y G18
			nux = oct3L[ndx][2]
			nuy = oct3L[ndx][1]
			nuz = oct3L[ndx][0]
			tupl=( (-nux,nuy,-nuz) )
		elif SEDM.ToolAxis == 0: #X G19
			nux = oct3L[ndx][0]
			nuy = oct3L[ndx][2]
			nuz = oct3L[ndx][1]
			tupl=( (nux,-nuy,-nuz) )
		#
		oct4L.append(tupl)
	# ------- beg octant 5
	oct5L = []
	for ndx in range(l-1,  -1, -1):#  loop bwds thri octant 1 data
		if SEDM.ToolAxis == 2: #Z G17
			nux = oct4L[ndx][0]
			nuy = oct4L[ndx][1]
			nuz = oct4L[ndx][2]
			tupl=( (nux,-nuy,nuz) )
		elif SEDM.ToolAxis == 1: #Y G18
			nux = oct4L[ndx][0]
			nuy = oct4L[ndx][1]
			nuz = oct4L[ndx][2]
			tupl=( (nux,nuy,-nuz) )
		elif SEDM.ToolAxis == 0: #X G19
			nux = oct4L[ndx][0]
			nuy = oct4L[ndx][1]
			nuz = oct4L[ndx][2]
			tupl=( (nux,nuy,-nuz) )
		#
		oct5L.append(tupl)
	# ------- beg octant 6
	oct6L = []
	for ndx in range(l-1,  -1, -1):#  loop bwds thri octant 1 data
		if SEDM.ToolAxis == 2: #Z G17
			nux = oct5L[ndx][1]
			nuy = oct5L[ndx][0]
			nuz = oct5L[ndx][2]
			tupl=( (nux,nuy,nuz) )
		elif SEDM.ToolAxis == 1: #Y G18
			nux = oct5L[ndx][2]
			nuy = oct5L[ndx][1]
			nuz = oct5L[ndx][0]
			tupl=( (nux,nuy,nuz) )
		elif SEDM.ToolAxis == 0: #X G19
			nux = oct5L[ndx][0]
			nuy = oct5L[ndx][2]
			nuz = oct5L[ndx][1]
			tupl=( (nux,nuy,nuz) )
		#
		oct6L.append(tupl)
	# ------- beg octant 7
	oct7L = []
	for ndx in range(l-1,  -1, -1):#  loop bwds thri octant 1 data
		if SEDM.ToolAxis == 2: #Z G17
			nux = oct6L[ndx][0]
			nuy = oct6L[ndx][1]
			nuz = oct6L[ndx][2]
			tupl=( (-nux,nuy,nuz) )
		elif SEDM.ToolAxis == 1: #Y G18
			nux = oct6L[ndx][0]
			nuy = oct6L[ndx][1]
			nuz = oct6L[ndx][2]
			tupl=( (-nux,nuy,nuz) )
		elif SEDM.ToolAxis == 0: #X G19
			nux = oct6L[ndx][0]
			nuy = oct6L[ndx][1]
			nuz = oct6L[ndx][2]
			tupl=( (nux,-nuy,nuz) )
		#
		oct7L.append(tupl)
	# ------- beg octant 8
	oct8L = []
	for ndx in range(l-1,  -1, -1):#  loop bwds thri octant 1 data
		if SEDM.ToolAxis == 2: #Z G17
			nux = oct7L[ndx][1]
			nuy = oct7L[ndx][0]
			nuz = oct7L[ndx][2]
			tupl=( (-nux,-nuy,nuz) )
		elif SEDM.ToolAxis == 1: #Y G18
			nux = oct7L[ndx][2]
			nuy = oct7L[ndx][1]
			nuz = oct7L[ndx][0]
			tupl=( (-nux,nuy,-nuz) )
		elif SEDM.ToolAxis == 0: #X G19
			nux = oct7L[ndx][0]
			nuy = oct7L[ndx][2]
			nuz = oct7L[ndx][1]
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
	rMida = radi #like 5,0  was  SEDM.RADf # wasradCounts
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
	if SEDM.ToolAxis== 2: #Z G17  XY  plane
		topRtHalfL = L3D( (rMida, rMidb ,  sqrDp) , (tRa,tRb ,  sqrDp)  )
		topL       = L3D( (	tRa, tRb ,  sqrDp) , (tLa,tLb ,  sqrDp)  )
		leftL      = L3D( (	tLa,    tLb ,  sqrDp) , (bLa, bLb ,  sqrDp)  )
		botL       = L3D( (	bLa, bLb ,  sqrDp) , (bRa, bRb ,  sqrDp)  )
		botRtHalfL = L3D((bRa,bRb,sqrDp),(rMida,rMidb,sqrDp))

	if SEDM.ToolAxis== 1:
		topRtHalfL = L3D( (rMida,   sqrDp, rMidb ) , (tRa,   sqrDp ,tRb )  )
		topL       = L3D( (	tRa,   sqrDp, tRb ) , (tLa,  sqrDp, tRb )  )
		leftL      = L3D( (	tLa,   sqrDp ,tLb ) , (bLa,   sqrDp, bLb)  )
		botL       = L3D( (	bLa,   sqrDp, bLb) , (bRa,   sqrDp, bRb )  )
		botRtHalfL = L3D( ( bRa,   sqrDp, bRb ) , (rMida,   sqrDp, rMidb ) )
	if SEDM.ToolAxis== 0:
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
	
	if SEDM.OrbitType == CIRCLE:
		orbL = mkcL( radi, cLevel)
		entryPt = orbL[0]
		# add a cxopy of 1st posn onto  end of list
		orbL.append(entryPt) # dipe 1st to last
		#
	if SEDM.OrbitType == SQUARE:
		orbL = mksqrL( radi,cLevel)
		entryPt = orbL[0]
		orbL.append(entryPt) # dipe 1st to last
		#
	# 04.01.2026 c hec klist for SPO
	SPO = ( ( 0,0,0 ) )
	
	""" re: tall skinny triangles
	# this code never tripped
	# so the tall skiunny trinagle tip
	# is Not SPO 
	# or
	# not due to the path list
	for p in orbL:
		if p == SPO:
			print(1555,"orbL containds SPO")
	"""
	#
	return orbL
#
def mkDetourWiglL( posn): #SEDM.ToolAxis ):
	#
	WiglRADi = int(round(SEDM.WiglRADf / SEDM.xyzSCALEfOUT))
	
	cDeep = posn[SEDM.ToolAxis] - ( WiglRADi * SEDM.CutDir)
	
	if SEDM.ToolAxis == 2:
		CtrNow = (0,0,cDeep) #posn[SEDM.ToolAxis])
	elif SEDM.ToolAxis == 1:
		CtrNow = (0,cDeep,0) #posn[SEDM.ToolAxis],0)
	elif SEDM.ToolAxis == 0:
		CtrNow = (cDeep,0,0) #posn[SEDM.ToolAxis],0,0)
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

	#12.02.2026 new 3 lines do vvv alays if SEDM.JumpENA == True:
	SEDM.JumpLtype = JumpOrbPathType
	SEDM.JumpOn = True
	
	OrbEndPt = OrbL[endNdx]
	
	#02.01.2026 why start at ndx 1  other codes start at ndx 0
	ndx = 1
	#
	doMove(OrbL[ndx])
	#
	# ??? importannt ??? not yet SEDM.JumpoOn = True
	while 1: # # seq is 
		nextDir = getEvalPV( JumpOrbPathType,OrbL,ndx )
		#
		if (SEDM.BwdMaxHit == True) or (SEDM.QuitHit == True):
			return 
			# rtn w flag set    let CUPA move to ctr, move to SPO
		else:
			if nextDir == FWD:# check all done and SUCCESS
				if ndx == endNdx: #goodLim = len(OrbL) -1
					#
					# if ALREADY at end of orbList and FWD
					loL = L3D(OrbL[ndx],RufPtTupl)
					doExitL(loL) # MOVES TO RUFPTTUPL  NOT SPO
					return 
					# rtn w NO flag set   p = RufPtTupl no flags
					#
				else: # else  FWD and ndx < goodLim
					oldP = OrbL[ndx]
					ndx += 1
					newP = OrbL[ndx]
					#
					doMove(newP)
					#test fatyal flags
					if (SEDM.BwdMaxHit == True) or (SEDM.QuitHit == True):
						SEDM.JumpOn = False
						return 
						# rtn w flag set    
						# let CUPA move to ctr, move to SPO
			#
			elif nextDir == BWD: 
				# done aLREADY   1st check for fatal
				#
				bP = OrbL[ndx] #make a detour path  frm BreakPt bP
				legL,ankleNdx = mkDetourLegL(bP)
				doDetourLegL(legL,begNdx)
				# 0 is BegNdx , hwre to start in List
				#
				if ( SEDM.BwdMaxHit == True) or (SEDM.QuitHit == True):
					SEDM.JumpOn = False
					return 
#
def doExitL( xL): # no  getEvalPV, just cloicck out stepsxl[0] is  
	ndx = 0
	goodLim = len(xL)-1
	badLim = 0
	while ndx < goodLim: #
		#
		oldP = xL[ ndx ]
		ndx += 1
		newP =  xL[ ndx ]
		#
		doMove( newP )
		time.sleep(.001)
#
def doMove( posn): # caller must make sure podn is adjacen to last
	#
	# 02.01.2026 neccc   this is the cmd to move  to posn
	SEDM.XOffsetCmd = posn[0]
	SEDM.YOffsetCmd = posn[1]
	SEDM.ZOffsetCmd = posn[2]
	# !?!? NECC yes !!!thius is MIN and NECC
	time.sleep(0.001)
#
def getAtctr(posn):
	#rtns atctr CtrPosn
	if SEDM.ToolAxis == 2:# Z
		if (posn[0] == 0)  and (posn[1] == 0):
			atctr = True
		else:# else tool is NOT at ctr
			CtrPosn = ( ( 0,0,posn[SEDM.ToolAxis] ) )
			atctr = False
	elif SEDM.ToolAxis == 1 : # Y
		if (posn[0] == 0)  and (posn[2] == 0):
			atctr = True
		else:
			CtrPogsn = ( ( 0,posn[SEDM.ToolAxis],0 ) )
			atctr = False
	elif SEDM.ToolAxis == 0:# X
		if (posn[1] == 0)  and (posn[2] == 0):
			atctr =  True
		else:
			CtrPosn = ( ( posn[SEDM.ToolAxis],0,0 ) )
			atctr = False
#
def mkStairsL( cL):  # now  RufPtTupl is global;ly readable
	#
	s = SEDM.CutDir # just a short name
	StairsL=[] # empty list to hold stairsteps
	WiglRADi = int(round(SEDM.WiglRADf / SEDM.xyzSCALEfOUT))
	TopStairLevel = ( s * WiglRADi)
	# 23.12.2025 REMEBNER rUFpTtUPL MAY BE RADI ABOVE 
	# DEEPEST PT OF STAIRS
	beg = abs(TopStairLevel)
	RufPtDepth = RufPtTupl[SEDM.ToolAxis]
	BotStairLevel = RufPtTupl[SEDM.ToolAxis] + TopStairLevel
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
		if SEDM.ToolAxis == 2: # Z G17  plabe is XY
			p=( tmpX, tmpY,  deepNow)
		elif SEDM.ToolAxis == 1: # Y G18  plane is ZX
			p=( tmpX, deepNow, tmpZ)
		elif SEDM.ToolAxis == 0: # X G19  plabe is YZ
			p=( deepNow, tmpY, tmpZ)
		#
		StairsL.append(p)
		#
	return StairsL
#
# TODO 05.02.2026 ugly   lotda ferad cats 
def doWiglLeadIn(liL):
	#
	ndx = 0
	#
	lim = len(liL) - 1
	#
	while 1: #
		nextDir = getEvalPV( NoJump,liL,ndx )# 0 indicates dont jump
		if SEDM.QuitHit  == True:
			return BWD # well,QuitHit is bnetter name (nit taken)
		elif SEDM.BwdMaxHit == True:
			return BWD # well,BwdMaxHit is better name ( but taken)
		else:
			if nextDir == FWD:  
				# FWD is towards liL[lim] (stairs top step)
				#
				if ndx >= lim: # all done  if at lim and FWD
					# caller must look at rtn'd FWD 
					# and set state = CUPA
					return FWD # well, Done_Success is  better name
				#
				else:
					oldP  = liL[ndx]
					ndx += 1
					newP = liL[ndx]
				#
			elif nextDir == BWD:   # BWD is tiwards lissr[0]
				#
				if ndx <= 0:# if at badLim and  BWD
					# is ndx wronG
					SEDM.BwdMaxHit = True # well , really BwdTooMuch is bettwer name
					#caller must look at rtnd BWD and set state CUPA
					return BWD; # well TooManyBwd is better name
				#
				else:
					oldP  = liL[ndx]
					ndx -= 1
					newP = liL[ndx]
		#
		SPO = ( ( 0,0,0 ) )
		
		""" re: tall skinmmy triamg;es
		# vvv this never tripped
		if newP == SPO:
			print(2229,"in doWiglLeadIn newP == SPO")
			trap()
		#
		"""
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
	if SEDM.WiglRADf != 0:# else doBoreL( BoreL )
		WiglRADi = int(round(SEDM.WiglRADf / SEDM.xyzSCALEfOUT))
		if SEDM.ToolAxis == 2:
			liLdest = ( (WiglRADi, 0, WiglRADi * SEDM.CutDir) )
		elif SEDM.ToolAxis == 1:
			liLdest = ( (WiglRADi, WiglRADi * SEDM.CutDir, 0) )
		elif SEDM.ToolAxis == 0:
			liLdest = ( (WiglRADi * SEDM.CutDir ,WiglRADi, 0) )
		#
		# make a list of circle pts
		cLevel = liLdest[SEDM.ToolAxis] # cLevel is ToolAxis dimension at lilDest
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
		SEDM.disableOsc = False # power on
		#
		####################
		# process WiglLiL StairsL WiglLoL
		#  NB  BwdMaxCount is handled in geTevalPV
		#  NOT in fo Move
		######################
		doWiglLeadIn(liL)
		#31.01.2026 ??? allow jump on ewigl leadin limne//\
		
		if (SEDM.QuitHit == True) or (SEDM.BwdMaxHit == True) :
			return BWD #caller must look foe fatal flag and set state accordingly
		# else: # liL success, begin StairsL
		#
		#process StairsL
		doStairsL( sL)
		# 31.01.2026 allow jump on stairs
		
		
		#
		if (SEDM.QuitHit == True) or (SEDM.BwdMaxHit == True) :
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
		if SEDM.JumpENA == True:
			#06.02.2026 this may be good palces
			# to sets pin JumpLtype
			SEDM.JumpLtype = JumpBoreType #1
			#vvv dunno BPndx now,  putyin getEval
			# THIS BELONNGS IN GEETEVALPV  JumL = mkJupLJdnL(JumpBoreType,BoreL, BPndx)
			# no, let getREva;lPV get L LType BP
			#  and let getEvalPV call mkJupLJdnL
			# do i need an SEDM.cutLndx??
			# already got SEDM.JumpLtype
			# do I need global ThisCutL??
			
			mkJT() # sets SEDM.endJT 
			# halmeter ahoew 1.401 # suspiciously like ET in etab + 1mS 
			# check the \yimenow' valu
			
			# use an LED or Halmeter to watch
		#power on
		SEDM.disableOsc = False
		#
		# process BoreL
		doBoreL(BoreL)
		# allow jump on BoreList
		
		#
		#test fatal
		if (SEDM.QuitHit == True) or (SEDM.BwdMaxHit == True) :
			return BWD #caller must look foe fatal flag and set state accordingly
		# else: else dObreL() success
	# tool now at RufPtTupl
	# power off
	SEDM.disableOsc = True
	return FWD
#
def getCutDir(): 
	if   SEDM.FullDEPTHf > 0:
		SEDM.CutDir = POS
	elif SEDM.FullDEPTHf < 0:
		SEDM.CutDir = NEG
	else:# else SEDM.FullDEPTHf == 0
		msg ="1694 RufPt is same as StartPt"
		c = linuxcnc.command()
		c.error_msg(msg)
		raise SystemExit
#
def chkAtCtr(posn):
	atctr = False # guilty tiill proven innocent
	if SEDM.ToolAxis == 2:
		if (posn[0] == 0) and ( posn[1] == 0): # if x and y are 0
			atctr = True
	if SEDM.ToolAxis == 1:
		if (posn[0] == 0) and ( posn[2] == 0):# if x and z are 0
			atctr = True
	if SEDM.ToolAxis == 0:
		if (posn[0] == 0) and ( posn[2] == 0):# if y and z are 0
			atctr = True
	return atctr
#
def mkRufPtTupl(): #called 1x per cut, setGen(25) before call
	global RufPtTupl
	SEDM.ThisRADf = 0.0
	SEDM.RADi     = 0
	#
	if SEDM.CutDir == NEG:
		tmp = SEDM.FullDEPTHf + SEDM.MsrdUNSf # makes LESS neg
		tmp = tmp - SEDM.RPlaneDist # more neg  to get CutDiost
		SEDM.FullDEPTHf = tmp
		
		SEDM.RufPtDEPTHf  = round(tmp,3)
		RufPtINT = int( round(tmp / SEDM.xyzSCALEfOUT ))
	if SEDM.CutDir == POS: # say cyt fro -15 to -10
		tmp = SEDM.FullDEPTHf - SEDM.MsrdUNSf # makes LESS neg
		tmp = tmp - SEDM.RPlaneDist # more neg  to get CutDiost
		SEDM.FullDEPTHf = tmp
		
		SEDM.RufPtDEPTHf  = round(tmp,3)
		RufPtINT = int( round(tmp / SEDM.xyzSCALEfOUT ))
	# 2nd create RufPtTupl
	if SEDM.ToolAxis == 0:
		RufPtTupl = ( ( RufPtINT,0,0) )
	elif SEDM.ToolAxis ==1:
		RufPtTupl = ( ( 0, RufPtINT,0) )
	elif SEDM.ToolAxis == 2:
		RufPtTupl = ( ( 0, 0, RufPtINT) )
#
def doCtrSpo():# move tool to ctr then to StartPtOffset
	#
	posn = (  (SEDM.XOffsetCmd , SEDM.YOffsetCmd , SEDM.ZOffsetCmd ) )
	atctr = chkAtCtr(posn)
	if atctr != True:
		# TODO isa RufPt correct for all cases??
		rcL = L3D(posn,RufPtTupl)
		doExitL(rcL)
		posn = (  (SEDM.XOffsetCmd , SEDM.YOffsetCmd , SEDM.ZOffsetCmd ) )
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
	ndx = 0 
	lim = len(BoreL)-1
	
	# vvv brware JumpENA and JumpOn
	# JumpENA set in techGui, higher level than KumpOn
	# JumpOn set in code, ineach of 4 jump list typrs
	SEDM.JumpOn = True # turn off after L complted/failed
	#
	while 1: # TODO while 1 is bad form, find a proper limit
		#
		# 07.01.2026 work jump imn at top of hgwile
		#
		nextDir = getEvalPV(JumpBoreType, BoreL,ndx )
		# test w 1st run NO jump chheckntn
		# and 3nd smae no chkbtn, 3nd hangs
		if (SEDM.QuitHit == True) or (SEDM.BwdMaxHit == True):
			SEDM.JumpOn = False # turn off after L complted/failed
			return BWD
		#
		if nextDir != HOLD:
			if nextDir == FWD:
				BwdMaxCount = False
				if ndx == lim:
					SEDM.JumpOn = False 
					# turn off after L complted/failed
					return FWD # ~ OK  
				else:
					ndx += 1 # doMove comes later
			elif nextDir == BWD:
				if ndx == 0: # at SPO and gap eval is BWD
					SEDM.BwdMaxHit= True
					SEDM.JumpOn = False
					# turn off after L complted/failed
					return BWD # 05.01.2026 new  had no ret vak
				else:
					ndx -=  1 # doMove comes later
			newP = BoreL[ndx]
			SPO = ( ( 0,0,0 ) )
			if newP == SPO:
				SEDM.JumpOn = False
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
	SPO = (0,0,0) 
	ndx = 0  
	GoodLim = len(StairsL)-1
	BadLim = 0# StairsL inclunde liL do sL[0] is startPt )also is SPO)
	EntryPt = StairsL[0]
	
	ctr = 0 # prob dteing backed up to ndx 0 vd just stated at ndx0
	#
	SEDM.JumpLtype = JumpStairsType
	SEDM.JumpOn = True
	#
	while ndx <= GoodLim:
		# vvv this will jump if needed
		nextDir = getEvalPV(JumpStairsType, StairsL,ndx )
		#
		if (SEDM.QuitHit   == True) or (SEDM.BwdMaxHit == True):
			return BWD
		#
		if   nextDir == FWD:# FWD is toqerda BotStep (  dowqn stairs)
			if ndx >= GoodLim: # >= BptStep
				return FWD # 12.02.2026 why return FWD why etn anythinf
			else:
				oldP = StairsL[ndx]   # keep copy for adjancency tests
				ndx += 1
				newP = StairsL[ndx]
				#
				# 12.02.2026 vvv paranoia
				if StairsL[ndx] == SPO:
					print(1812,"in doStairsL   StairsL[ndx] == SPO")
					trap()
				#
				doMove( StairsL[ndx] ) # more dlear meaning
		#
		elif nextDir == BWD:   # BWD is tiwards startposn
			bP = StairsL[ndx] # BreaakPt
			#
			DetourWiglL, ankleNdx = mkDetourWiglL( bP )
			#
			doDetourWiglL( DetourWiglL, ankleNdx) 
			#
			# test fdatal flags
			if (SEDM.QuitHit   == True) or (SEDM.BwdMaxHit == True):
				return BWD
#
def doOrbitEntryLegL( legL, ankleNdx):
	SPO = ( ( 0,0,0) )
	ndx = ankleNdx
	#
	goodLim =len(legL) -1
	badLim = 0
	#
	#do vvv alays if SEDM.JumpENA == True:
	SEDM.JumpLtype = JumpOrbLeadInType
	SEDM.JumpOn = True
	#
	while 1: # # seq is 
		#
		nextDir = getEvalPV( SEDM.JumpLtype, legL,ndx )
		#
		if (SEDM.BwdMaxHit == True) or (SEDM.QuitHit == True):
			SEDM.JumpOn = False
			return # let CUPA move to ctr, move to SPO
		else:
			if nextDir == FWD:# FWD is towards RufPt
				if ndx >= goodLim:
					SEDM.JumpOn = False
					return # no flags
				else:
					oldP = legL[ndx]
					if ndx > 0:
						ndx += 1 
			elif nextDir == BWD:
				if ndx <= badLim : # backed up to SPO
					SEDM.BwdMaxHit = True
					SEDM.JumpOn = False
					return
				else:
					oldP = legL[ndx]
					ndx -= 1 
			#
			if nextDir != HOLD: # move but stay in limits
				newP = legL[ndx]
				#
				""" re: tall skinny triales
				# vv never triggered
				if newP == SPO:
					print(1772,"in doOrbitEntryLegL newP == SPO|")
					SEDM.JumpOn = False
					trap()
					#
				"""
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
	
	ndx = 0 #
	
	posnb = DetourWiglL[ndx]
	#posn = ( ( SEDM.XOffsetCmd , SEDM.YOffsetCmd , SEDM.ZOffsetCmd ) )
	
	goodLim = 0 # toe
	badLim = len(DetourWiglL)-1 # hip
	
	while 1: # begin lpp[
		#
		#07.01.2026 work jump into yop of while loop
		#

		#
		nextDir = getEvalPV( NoJump,DetourWiglL,ndx )
		#
		if nextDir == FWD:   # FWD is GOOD  butr nmust DEC ndx
			ndx -= 1 #
			if ndx <= goodLim:
				return FWD # success
		#
		elif nextDir == BWD:   # BWD INCs ndx towards SPO  BAD
			oldP = DetourWiglL[ndx] # where tool was on entry to this dunc
			ndx += 1 # BWD INCs the ndx towards HIP SPO

			#01.01.2026 new
			ctr += 1
			#
			if ndx  >= badLim: #
				SEDM.BwdMaxHit = True
				return BWD # yes FWD means successm bad wors, correct axtion
			#
			newP = DetourWiglL[ndx] # get  newP from list, ndx already INCd
			""" re tall skinny triaNGLES
			# vvv never tripped
			# check  old and new re adjacent
			# vvv doesbnt show up on tall skinng tri prob
			if aj(oldP, newP ) == False:
				#print(1652,"in doDetourWiglL() TRAP prev possn was ", oldP)
				#print(1853,"in doDetourWiglL() TRAP new posn    is ", newP)
				#print( 1854," ndx of nmewP is  TRAP ", ndx)
				
				#if ndx > 0:# next line accesec ndx - 1 fo besure ndx > 0
				#	print("in doDetourWiglL TRAP prev posn in list ",DetourWiglL[ndx-1])
				#	# NB the prev posnn is asj to last cmds  posn
					
				#print("in doDetourWiglL TRAP next posn in list ",DetourWiglL[ndx+1])
				#print("in doDetourWiglL TRAP 0th posn in list ",DetourWiglL[0])
				#print("in doDetourWiglL TRAP whole list ",DetourWiglL)
				# NB he next posn in list is too far deom last  cmd [posn
				print(2191,"aj(oldP, newP ) == False")
				trap()
			else: # aj(oldP, newP ) == True
				# dupe posn = DetourWiglL[ndx]
				# dumb doMove( posn)
				
				if newP == SPO:
					print(1812,"in doDetourWiglL  newP == SPO")
					trap()
				
				doMove( newP)
			#
			if newP == SPO:
				print(1812,"in doDetourWiglL  newP == SPO")
				trap()
			"""
			doMove( newP)
#
def doDetourLegL( legL, BegNdx): # wasankleNdx):
	# BegNdx is 0 when making detour from peri
	SPO = ( ( 0,0,0) )
	#
	ndx = BegNdx #maybe ankle, maybe toe,    wasankleNdx # Begin at ankl ndx
	goodLim =0  #TOE
	badLim =  len(legL) -1 # HIP
	
	#stepNum = 0
	#
	while 1: # # seq is 
		#
		#07.01.2026 work jump into yop of while loop
		#
		nextDir = getEvalPV( NoJump,legL,ndx )# 0 means  DONT JUMMP
		# check fataal flags
		if (SEDM.BwdMaxHit == True) or (SEDM.QuitHit == True):
			return BWD# let CUPA move to ctr, move to SPO
		#
		if nextDir == FWD:# FWD is DEC   towards TOE, towards ndx == 0
			if ndx == 0:  #aka goodLim: # ndx 0  is TOE
				return# ok retn to caller, we all done w fetote and gotr a FWD PV   no flags # no rtn value  needed
			else: #FWD and ndx != 0, not at TOE
				#oldP = legL[ndx]
				####### legL is list w [0] = toe
				######## legL[badLim == SPO)
				######## ??? is legL[-1] == SPO???   YES
				######## DONT DEC ndx if ndx == 0
				ndx -= 1 # FWD is DEC  towards TOE towrads PeriPath Towars ndx == 0
				newP = legL[ndx]
		elif nextDir == BWD:   # BWD is INC ndx  BWD is tiwards SPO , ndx  gets larger
			if (ndx == badLim): # and  (stepNum != 0): # backed up to SPO
				SEDM.BwdMaxHit = True
				return
			else:
				#oldP = legL[ndx]
				ndx += 1 # BWD is towards HIP. ndx is larger
				newP = legL[ndx]
		if (nextDir != HOLD):
			
			""" re TST nevcer tyrihghered
			if newP == SPO:
				print(1731,"in doDetourLegL newP == SPO|")
				trap()
			"""
			doMove( newP )

# ......... end funcs needing Hump code
#
SEDM = mkSEDMcomp()


try: # SEDM preparation 
	# vvv initl state of state machine
	SEDM.state = WaitFullDepthRplaneDist
	#
	# i needed a while for the try, EDMmode is  ON:Y uised to keep the while open
	EDMmode = True 
	time.sleep(0.10) # nECC as well as QBreaker
	SEDM.xyzSCALEfOUT = SEDM.xyzSCALEfIN
	SEDM.mlt = 1/SEDM.xyzSCALEfOUT
	
	getPgmUnits() # should ONLY be called 1x per pgm ( tho user could tryt G20 G21 G20 G31 blah
	#
	SEDM.ctr = 0
	
	while EDMmode == True:
		if SEDM.isEna == True: # set by M199  clrd by M198
			# vvv new 11.12.2025  reset at top, not bot
			#SEDM.ctr = 0
			
			# peek rwtr wuit btns enabled at start up  NG
			# so wait isEna
			# ng  all eNOT greyed at startup
			# ~ok catch22
			#  when panel si loaded, the btns are enabled
			# and the nets needed tro DISable them are not yet enabled by M199
			# so TODO fix btns ena too eartly
			# BTW PEEK enable as soon as 1st getEvalPV()_ ca;;ed
			# these vvv 2 lines are va
			SEDM.BwdMaxHit = False
			SEDM.BWDcount = 0
			
			time.sleep(0.10) # nECC as well as QBreaker
			# vvv call 1s
			if SEDM.NR == SEDM.BegNR:
				getStartPtF()
			#
			SEDM.disableOsc = True
			
			
			
			#----------------------------------------
			#----------- begin state machine --------
			#----------------------------------------
			if SEDM.state ==  WaitFullDepthRplaneDist:
				
				
				#25.11.2025 this is top of state machuien for subsequent iters
				time.sleep(0.1)
				
				# M162 sets FullDEPTHf and RPlaneDist
				SEDM.restart = False
				
				# vvv doews NOT use mly
				getToolAxis() # ths need to run for each tool change
				
				if SEDM.RPlaneDist != 0:
					#vvv asets
					#	SEDM.CutDir
					#	SEDM.FullDEPTHf = SEDM.FullDEPTHf + SEDM.RPlaneDist
					getCutDir() # also combine RPlane and FullDpeeth tto  make CutTraavel
					#
					SEDM.state = WaitOrbitTypeWiglRADf 
				#
				else: # SEDM.RPlaneDist == 0:
					# FAIL becuz RPlaneDisst == 0
					msg = "SEDM.RPlaneDist = 0"
					msg = msg + str(SEDM.RPlaneDist)
					c = linuxcnc.command()
					c.error_msg(msg)
					raise SystemExit
			#
			if SEDM.state == WaitOrbitTypeWiglRADf:
				# test OrbitType valid
				if (SEDM.OrbitType == CIRCLE) or (SEDM.OrbitType == SQUARE) :
					# ^^^ must be circle or sqr for now 26.11.2025
					#
					# test WiglRADf valid ( >=0)
					# M163 SEDM.OrbitType SEDM.WiglRADf vi dignals
					if SEDM.WiglRADf >= 0:#15.11.2025 vhg to >=  hung at 0
						SEDM.state = WaitEttabNumberMsrdUNS
			#
			#TODO 25.11.2025 no testing dict creation success
			if SEDM.state == WaitEttabNumberMsrdUNS:
				# 
				# Path to ETAB is \local'
				etabPath = "./Etabs"#25.02.2026 <<<  make etab path ='HERE'
				#
				if (SEDM.EtabNum > 0):
					# construct file name from number
					EtabNumStr = str(SEDM.EtabNum) #"99999944"
					fqfn = etabPath + "/" + EtabNumStr +".ENC"
					#
					# read file, make dict of lists made from lines in file
					parseEtab(fqfn) 
					#
					SEDM.state =  WaitBegEndNR
			#
			if SEDM.state == WaitBegEndNR:
				if(SEDM.BegNR != 0) and (SEDM.EndNR != 0 ):
					if (SEDM.EndNR <= SEDM.BegNR)and(SEDM.EndNR > 0):
						if SEDM.BegNR !=  25:
							SEDM.PlungeOrbitWanted = True
						# dont progress until EndNR <= BegNR
						# the PlungOrnbWantewds is a side issue
						SEDM.state = WaitPitch
					#
					# I dont handfle plungeOrbWanted correctly
					#  ... dont undeterdtansd yet...
			#
			if SEDM.state == WaitPitch: # WaitPitch is state 5
				#15.02.2026 pitch is not used now
				# so not good to wait for M???
				# so i shoirt shank this state
				SEDM.state = WaitGenReady
			#
			# TODO JumpENA is BIT, will never be -1
			#if SEDM.state == WaitJumpwANTED:
			#	if SEDM.JumpENA != -1:  #insist M166 is used( oper must say he wants.doersmnt want jump)
			#		SEDM.state = WaitGenReady
			#
			if SEDM.state == WaitGenReady:# i need UNS to caLC
				# ThisRADf, SO NAME GENREADY MISLEADING MORE LIKE # # GENPrepared MAYBE
				#vvv makes UNSf, SEDM.RufPtDEPTHf, RufPtTupl, 
				if SEDM.ThisNR == SEDM.BegNR:
					# temp set ThisNR = 25 to get UND asnd???
					# reset ThisNR to BegNR afterwards
					
					
					# this chink is dfor spcl case
					#  where BegNR != 25
					#  and mkThusRAD and UND not normally called
					#  so, ThisNR is LIEF to, just to get UNS
					# bur 25.02.2026 in new scheme
					#  thhe etavDixr['25'][11] hol;ds UNS ( tho collumn hdr sez RAD
					SEDM.ThisNR = 25
					setGen(SEDM.ThisNR)
					mkThisRADf() # get UNS 
					
				else: # 15.02.2026 thius line was missing
					# reset to BegNR
					SEDM.ThisNR = SEDM.BegNR
					
				#
				mkRufPtTupl()# the 25 could be embedded inside mkRufPtTupl, but keeping it outside shows better
				
				setGen(SEDM.BegNR)
				SEDM.UNSf = EtabDict['25'][11]
				SEDM.GenReady = 1 # domt say True  it can be -1 0 or 1
				SEDM.state = WaitPlunge
			#
			if SEDM.state == WaitPlunge:
				SEDM.disableOsc = False # turn ON power to tll
				doPlunge()              # main entry to plunge
				# we are done with NR 25, so dec ThisNR
				if SEDM.BegNR == 25:
					SEDM.ThisNR -= 1
					
				#else leave nr alone,
				#  user may wantplungeOrb when BegNR != 25
				SEDM.disableOsc = True # turn OFF power to tool
				# duting DoPlunge some fatal falgs may have been set
				if SEDM.QuitHit == True:
					SEDM.state = CleanUpPutAway # handle fatal flag, exit clean
				elif SEDM.BwdMaxHit == True:
					SEDM.state = CleanUpPutAway # handle fatal flag, exit clean
				else:
					SEDM.state = WaitDoPlungeOrbit
			#
			# if here Gen NR == ThisNR < 25
			# TODO 13.12.2025 state can be removed
			if SEDM.state == WaitDoPlungeOrbit: # MISSING 03.12.2025
				SEDM.state = WaitAllNRsDone
			#
			if SEDM.state == WaitAllNRsDone: # 13  NRs remaining are ORBITS
				if (SEDM.QuitHit == True)or(SEDM.BwdMaxHit == True):
					SEDM.state =  CleanUpPutAway
					# ??? break??? no rtn in state mc
				else:# no fatalflags
					if (SEDM.ThisNR < SEDM.EndNR):#  all NRs are done, 
						SEDM.state = CleanUpPutAway 
					else: #else do more orbits, ThisNR  IS NOT   EndNR, so  do more orbits
						#
						setGen(SEDM.ThisNR) # get power back on
						
						SEDM.disableOsc = False
						# get paths: legEntryL  legL opL
						mkThisRADf()
						#
						cLevel = RufPtTupl[SEDM.ToolAxis] + (SEDM.RADi * SEDM.CutDir)
						opL = mkOrbitPathL(SEDM.RADi,cLevel)
						entryPt = opL[0]
						#
						legL,ankleNdx = mkOrbitEntryLegL(entryPt)
						#
						ankleTupl = legL[ankleNdx]
						legL.reverse()
						ankleNdx = legL.index(ankleTupl)
						#
						doOrbitEntryLegL(legL,ankleNdx) #
						#
						# test for fatal flags
						if (SEDM.BwdMaxHit == True) or (SEDM.QuitHit == True):
							SEDM.state = CleanUpPutAway # let CUPA move to ctr, move to SPO
						else:
							#
							doOrbL(opL)# whwrw does doOrbL end???
							#
							if (SEDM.QuitHit == True)or(SEDM.BwdMaxHit == True):
								SEDM.state = CleanUpPutAway
							else: # turn off power, dec ThisNR
								SEDM.disableOsc = True
								SEDM.ThisNR = SEDM.ThisNR - 1
								# any more NRstoprocess???
								if SEDM.ThisNR < SEDM.EndNR:
									SEDM.state = CleanUpPutAway
			#
			if SEDM.state == CleanUpPutAway : # ALSO SUCCRESS
				SPO = ( (0,0,0) )
				doCtrSpo()
				c = linuxcnc.command()
				SEDM.disableOsc = True
				SEDM.isEna = False
				SEDM.BWDcount = 0
				#
				setGen(0)
				SEDM.state = WaitFullDepthRplaneDist
				SEDM.ctr +=  1 # report count ( for multiple cut loops )
				if SEDM.QuitHit == True:
					SEDM.QuitHit = False
					msg = "Operator Aborted"
					c.error_msg(msg) # stop_ngc_program()
					stop_ngc_program()
				elif SEDM.BwdMaxHit == True:
					SEDM.BwdMaxHit = False
					msg = "BwdMaxHit True"
					c.error_msg(msg)
					stop_ngc_program()
				else: # Succesful completion
					c.auto(linuxcnc.AUTO_RESUME)
#
except KeyboardInterrupt:
	raise SystemExit
