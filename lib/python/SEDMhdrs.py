#!/usr/bin/env python3 
#
# SEDMhdrs.py
# resides in /usr/lib/python3.11
#   or       ~/yourRIP/lib/python
#
#06.02.2026 add new JumpLtype(s)
# is infor for mkJumpL where to get dat
# some rqrs build fromBreakPy
# some will use sections of know lumpLine
#
# contains variables commom to other
#  can be  imported by:
# feom SEDMhdrs  import *
#
# 09082025 assing backup limit
"""
#shows a way to get ~header  files
#  in python3
#
# 18072025
# python has no 'header' files
# and  has 'import'  not @inmcliude"
# so 
# i have some 'token's whcih are know to  funcs
# and the funs are indiv .py   files
# so 
#    from SEDMhdrs import *
# result:
# YAY all variables ~conmstants are available
# even tab completed
#
# BEWARE all ~consnta can be edited in a single file
#  BUT
# if name.value is  chgd,
#  then  chgs are needed
#  in all files that used it
#
"""

# JumpLtype
JumpBoreType = 1 # use part of sexisyting BoreL
#JumpStairsLeadIn dont jump on StairsLeadin too small
JumpStairsType = 2 # create a leg shape beg at BP end at BP
JumpOrbLeadInType = 3 #use section of existing Legshape
# in Heidenhain, i didnt jump if too far away... how did i tell?
JumpOrbPathType = 4 #create dwtour wqith legshape 
#JumpOrbPathDetour = 5 #NO dont jump , thertes only 1 posn where there may be stock, else its just backaway to clear low V
NoJump = 10 # avoid 0 for unitialized cars


# beg--------- CINSTANTS --------
FWD = +1
# vvv 'HOLD' ??? what name to use.. pick one damnit
HOLD = 0
BWD = -1
# 18.11.2025 vvv new    when already doFail, this is rtnd
DEAD = 666

ContBWDmax = 20 # maxximum continouous   low voltage sample, causes ABORT


LEADIN = 1
PERIPHERY = 2
LEADOUT = 3

ESCAPE_END_AT_MIN =   1 # token means 
ESCAPE_END_AT_MAX =   2 # token meaning 
ESCAPE_DO_MORE_FWD =  3 # token meaning last move was FWD ( away from periphery ) and not near ends of escapepath, caller should process more points
ESCAPE_DO_MORE_WAIT = 4 # token meaning last move was WAIT ( no motion ) and not near ends of escapepath, caller should process more points
ESCAPE_DO_MORE_BWD =  5 # token meaning last move was BWD ( towards periphery ) and not near ends of escapepath, caller should process more points

NO_PLACE_TO_GO = 6 # at orb vctr and gv is ko  FIX w x retreatr code TRBD

BACK_ON_PERI_OK =7 # escLine finished ok

BACK_AFTER_PERI_AND_OK = 8# escLine finished ok
# new 09042025
BACK_AFTER_BORE_AND_OK = 9# escLine finished ok

# neww 25072025
MISMATH_DISPT_RUFPT = 10
# use BO_PLACE_TO_GO   FAIL_ON_PERI     = 21
OK_START_LEADOUT = 22

tooManyContinuousBWDs = 11
EscOkBackOnPath       = 12
EscapeFailNeedAbort   = 13

PathFinishedOK        = 14

# 17.08.2025 GVMIN GVMAX set  by NOT hdrs.py
#03072025 GVVMIN GVMAX now globals 'constyants"
#02.09.2025 i  want to set GVMAX and GV<IN using this vv data . so it innn Edm3 comp
#  since pin and  'constant' had same anmes, i added uscore to constant
GVMIN = 0
GVMAX = 100
#18.08.2025 now   LOGVLIM HIGVLIM are  in ETAB
#LOGVLIM = 30
#HIGVLIM = 60

# vvv USED im time.ssleep(zzz)
zzz = 0.010#0.0101#0.05#2

# TODO   this vvv is NOT a comnstant, 
#  it   gets chgd
#  so,
#  is it valid to have in this ~~header file??
ndx = 0 # need this global or pass, global may be correct here
# TODO us vvv imported? 

lOndx = 0
# end --------- CINSTANTS --------

#04.09.2025  whuy canyt io ise thios vvv 
# i get 'not defined'
#    print(">>",ETABPATH)
#               ^^^^^^^^
# NameError: name 'ETABPATH' is not defined

ETABPATH = "."

# G20 is 0(INCH)  G21 is 1(MM)
INCHUNIT = 0
MMUNIT = 1

NEG = -1
POS = +1

CIRCLE = 0
SQUARE = 1

# WaitFullDepthMsrdUNS     = 1
WaitFullDepthRplaneDist  = 1
WaitOrbitTypeWiglRADf    = 2

# WaitEttabNumber          = 3 
WaitEttabNumberMsrdUNS   = 3

WaitBegEndNR             = 4
WaitPitch                = 5
WaitJumpwANTED           = 6
WaitGenReady             = 7
WaitM199                 = 8

WaitPlunge               = 11
WaitDoPlungeOrbit        = 12

WaitAllNRsDone           = 13
WaitThisOrbitDone        = 14
WaitNewNR                = 15
CleanUpPutAway           = 16
# 12.11.2025 chgd name WaitRestart              = 17

WaitNewCut               = 20
