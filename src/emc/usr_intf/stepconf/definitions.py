#!/usr/bin/env python
# -*- encoding: utf-8 -*-
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
#************
import os

THEMEDIR = "/usr/share/themes"

# OUTPUT
XSTEP=0
XDIR=1
YSTEP=2
YDIR=3
ZSTEP=4
ZDIR=5
ASTEP=6
ADIR=7
USTEP=8
UDIR=9
VSTEP=10
VDIR=11
ON=12
CW=13
CCW=14
PWM=15
BRAKE=16 
MIST=17
FLOOD=18
ESTOP=19
AMP=20
PUMP=21
DOUT0=22
DOUT1=23
DOUT2=24
DOUT3=25
UNUSED_OUTPUT=26

# INPUT
ESTOP_IN=0
PROBE=1
PPR=2
PHA=3
PHB=4
HOME_X=5
HOME_Y=6
HOME_Z=7
HOME_A=8
HOME_U=9
HOME_V=10
MIN_HOME_X=11
MIN_HOME_Y=12
MIN_HOME_Z=13
MIN_HOME_A=14
MIN_HOME_U=15
MIN_HOME_V=16
MAX_HOME_X=17
MAX_HOME_Y=18
MAX_HOME_Z=19
MAX_HOME_A=20
MAX_HOME_U=21
MAX_HOME_V=22
BOTH_HOME_X=23
BOTH_HOME_Y=24
BOTH_HOME_Z=25
BOTH_HOME_A=26
BOTH_HOME_U=27
BOTH_HOME_V=28
MIN_X=29
MIN_Y=30
MIN_Z=31
MIN_A=32
MIN_U=33
MIN_V=34
MAX_X=35
MAX_Y=36
MAX_Z=37
MAX_A=38
MAX_U=39
MAX_V=40
BOTH_X=41
BOTH_Y=42
BOTH_Z=43
BOTH_A=44
BOTH_U=45
BOTH_V=46
ALL_LIMIT=47
ALL_HOME=48
ALL_LIMIT_HOME=49
DIN0=50
DIN1=51
DIN2=52
DIN3=53
UNUSED_INPUT=54

# output names
d_hal_output = {
	XSTEP:"xstep",
	XDIR:"xdir",
	YSTEP:"ystep",
	YDIR:"ydir",
	ZSTEP:"zstep",
	ZDIR:"zdir",
	ASTEP:"astep", 
	ADIR:"adir", 
	USTEP:"ustep",
	UDIR:"udir",
	VSTEP:"vstep",
	VDIR:"vdir",
	ON:"spindle-on",
	CW:"spindle-cw",
	CCW:"spindle-ccw",
	PWM:"spindle-pwm",
	BRAKE:"spindle-brake",
	MIST:"coolant-mist",
	FLOOD:"coolant-flood",
	ESTOP:"estop-out",
	AMP:"xenable", 
	PUMP:"charge-pump",
	DOUT0:"dout-00",
	DOUT1:"dout-01",
	DOUT2:"dout-02",
	DOUT3:"dout-03",
	UNUSED_OUTPUT:"unused-output"
}

hal_output = [
	{"name":d_hal_output[XSTEP], "human":_("X Step"), 'index':XSTEP},
	{"name":d_hal_output[XDIR], "human":_("X Direction"), 'index':XDIR},
	{"name":d_hal_output[YSTEP], "human":_("Y Step"), 'index':YSTEP},
	{"name":d_hal_output[YDIR], "human":_("Y Direction"), 'index':YDIR},
	{"name":d_hal_output[ZSTEP], "human":_("Z Step"), 'index':ZSTEP},
	{"name":d_hal_output[ZDIR], "human":_("Z Direction"), 'index':ZDIR},
	{"name":d_hal_output[ASTEP], "human":_("A Step"), 'index':ASTEP},
	{"name":d_hal_output[ADIR], "human":_("A Direction"), 'index':ADIR},
	{"name":d_hal_output[USTEP], "human":_("U Step"), 'index':USTEP},
	{"name":d_hal_output[UDIR], "human":_("U Direction"), 'index':UDIR},
	{"name":d_hal_output[VSTEP], "human":_("V Step"), 'index':VSTEP},
	{"name":d_hal_output[VDIR], "human":_("V Direction"), 'index':VDIR},
	{"name":d_hal_output[ON], "human":_("Spindle ON"), 'index':ON},
	{"name":d_hal_output[CW], "human":_("Spindle CW"), 'index':CW},
	{"name":d_hal_output[CCW], "human":_("Spindle CCW"), 'index':CCW},
	{"name":d_hal_output[PWM], "human":_("Spindle PWM"), 'index':PWM},
	{"name":d_hal_output[BRAKE], "human":_("Spindle Brake"), 'index':BRAKE},
	{"name":d_hal_output[MIST], "human":_("Coolant Mist"), 'index':MIST},
	{"name":d_hal_output[FLOOD], "human":_("Coolant Flood"), 'index':FLOOD},
	{"name":d_hal_output[ESTOP], "human":_("ESTOP Out"), 'index':ESTOP},
	{"name":d_hal_output[AMP], "human":_("Amplifier Enable"), 'index':AMP},
	{"name":d_hal_output[PUMP], "human":_("Charge Pump"), 'index':PUMP},
	{"name":d_hal_output[DOUT0], "human":_("Digital out 0"), 'index':DOUT0},
	{"name":d_hal_output[DOUT1], "human":_("Digital out 1"), 'index':DOUT1},
	{"name":d_hal_output[DOUT2], "human":_("Digital out 2"), 'index':DOUT2},
	{"name":d_hal_output[DOUT3], "human":_("Digital out 3"), 'index':DOUT3},
	{"name":d_hal_output[UNUSED_OUTPUT], "human":_("Unused"), 'index':UNUSED_OUTPUT}
	]
	
d_hal_input = {
	ESTOP_IN:"estop-ext",
	PROBE:"probe-in",
	PPR:"spindle-index",
	PHA:"spindle-phase-a",
	PHB:"spindle-phase-b",
	HOME_X:"home-x",
	HOME_Y:"home-y",
	HOME_Z:"home-z", 
	HOME_A:"home-a",
	HOME_U:"home-u",
	HOME_V:"home-v",
	MIN_HOME_X:"min-home-x",
	MIN_HOME_Y:"min-home-y",
	MIN_HOME_Z:"min-home-z",
	MIN_HOME_A:"min-home-a",
	MIN_HOME_U:"min-home-u",
	MIN_HOME_V:"min-home-v",
	MAX_HOME_X:"max-home-x",
	MAX_HOME_Y:"max-home-y",
	MAX_HOME_Z:"max-home-z",
	MAX_HOME_A:"max-home-a",
	MAX_HOME_U:"max-home-u",
	MAX_HOME_V:"max-home-v",
	BOTH_HOME_X:"both-home-x",
	BOTH_HOME_Y:"both-home-y",
	BOTH_HOME_Z:"both-home-z",
	BOTH_HOME_A:"both-home-a",
	BOTH_HOME_U:"both-home-u",
	BOTH_HOME_V:"both-home-v",
	MIN_X:"min-x",
	MIN_Y:"min-y",
	MIN_Z:"min-z",
	MIN_A:"min-a",
	MIN_U:"min-u",
	MIN_V:"min-v",
	MAX_X:"max-x",
	MAX_Y:"max-y",
	MAX_Z:"max-z",
	MAX_A:"max-a",
	MAX_U:"max-u",
	MAX_V:"max-v",
	BOTH_X:"both-x",
	BOTH_Y:"both-y",
	BOTH_Z:"both-z",
	BOTH_A:"both-a",
	BOTH_U:"both-u",
	BOTH_V:"both-v",
	ALL_LIMIT:"all-limit",
	ALL_HOME:"all-home",
	ALL_LIMIT_HOME:"all-limit-home",
	DIN0:"din-00",
	DIN1:"din-01",
	DIN2:"din-02",
	DIN3:"din-03",
	UNUSED_INPUT:"unused-input"
}

hal_input = [
	{"name":d_hal_input[ESTOP_IN], "human":_("ESTOP In"), 'index':ESTOP_IN},
	{"name":d_hal_input[PROBE], "human":_("Probe In"), 'index':PROBE},
	{"name":d_hal_input[PPR], "human":_("Spindle Index"), 'index':PPR},
	{"name":d_hal_input[PHA], "human":_("Spindle Phase A"), 'index':PHA},
	{"name":d_hal_input[PHB], "human":_("Spindle Phase B"), 'index':PHB},
	{"name":d_hal_input[HOME_X], "human":_("Home X"), 'index':HOME_X},
	{"name":d_hal_input[HOME_Y], "human":_("Home Y"), 'index':HOME_Y},
	{"name":d_hal_input[HOME_Z], "human":_("Home Z"), 'index':HOME_Z},
	{"name":d_hal_input[HOME_A], "human":_("Home A"), 'index':HOME_A},
	{"name":d_hal_input[HOME_U], "human":_("Home U"), 'index':HOME_U},
	{"name":d_hal_input[HOME_V], "human":_("Home V"), 'index':HOME_V},
	{"name":d_hal_input[MIN_HOME_X], "human":_("Minimum Limit + Home X"), 'index':MIN_HOME_X},
	{"name":d_hal_input[MIN_HOME_Y], "human":_("Minimum Limit + Home Y"), 'index':MIN_HOME_Y},
	{"name":d_hal_input[MIN_HOME_Z], "human":_("Minimum Limit + Home Z"), 'index':MIN_HOME_Z},
	{"name":d_hal_input[MIN_HOME_A], "human":_("Minimum Limit + Home A"), 'index':MIN_HOME_A},
	{"name":d_hal_input[MIN_HOME_U], "human":_("Minimum Limit + Home U"), 'index':MIN_HOME_U},
	{"name":d_hal_input[MIN_HOME_V], "human":_("Minimum Limit + Home V"), 'index':MIN_HOME_V},
	{"name":d_hal_input[MAX_HOME_X], "human":_("Maximum Limit + Home X"), 'index':MAX_HOME_X},
	{"name":d_hal_input[MAX_HOME_Y], "human":_("Maximum Limit + Home Y"), 'index':MAX_HOME_Y},
	{"name":d_hal_input[MAX_HOME_Z], "human":_("Maximum Limit + Home Z"), 'index':MAX_HOME_Z},
	{"name":d_hal_input[MAX_HOME_A], "human":_("Maximum Limit + Home A"), 'index':MAX_HOME_A},
	{"name":d_hal_input[MAX_HOME_U], "human":_("Maximum Limit + Home U"), 'index':MAX_HOME_U},
	{"name":d_hal_input[MAX_HOME_V], "human":_("Maximum Limit + Home V"), 'index':MAX_HOME_V},
	{"name":d_hal_input[BOTH_HOME_X], "human":_("Both Limit + Home X"), 'index':BOTH_HOME_X},
	{"name":d_hal_input[BOTH_HOME_Y], "human":_("Both Limit + Home Y"), 'index':BOTH_HOME_Y},
	{"name":d_hal_input[BOTH_HOME_Z], "human":_("Both Limit + Home Z"), 'index':BOTH_HOME_Z},
	{"name":d_hal_input[BOTH_HOME_A], "human":_("Both Limit + Home A"), 'index':BOTH_HOME_A},
	{"name":d_hal_input[BOTH_HOME_U], "human":_("Both Limit + Home U"), 'index':BOTH_HOME_U},
	{"name":d_hal_input[BOTH_HOME_V], "human":_("Both Limit + Home V"), 'index':BOTH_HOME_V},
	{"name":d_hal_input[MIN_X], "human":_("Minimum Limit X"), 'index':MIN_X},
	{"name":d_hal_input[MIN_Y], "human":_("Minimum Limit Y"), 'index':MIN_Y},
	{"name":d_hal_input[MIN_Z], "human":_("Minimum Limit Z"), 'index':MIN_Z},
	{"name":d_hal_input[MIN_A], "human":_("Minimum Limit A"), 'index':MIN_A},
	{"name":d_hal_input[MIN_U], "human":_("Minimum Limit U"), 'index':MIN_U},
	{"name":d_hal_input[MIN_V], "human":_("Minimum Limit V"), 'index':MIN_V},
	{"name":d_hal_input[MAX_X], "human":_("Maximum Limit X"), 'index':MAX_X},
	{"name":d_hal_input[MAX_Y], "human":_("Maximum Limit Y"), 'index':MAX_Y},
	{"name":d_hal_input[MAX_Z], "human":_("Maximum Limit Z"), 'index':MAX_Z},
	{"name":d_hal_input[MAX_A], "human":_("Maximum Limit A"), 'index':MAX_A},
	{"name":d_hal_input[MAX_U], "human":_("Maximum Limit U"), 'index':MAX_U},
	{"name":d_hal_input[MAX_V], "human":_("Maximum Limit V"), 'index':MAX_V},
	{"name":d_hal_input[BOTH_X], "human":_("Both Limit X"), 'index':BOTH_X},
	{"name":d_hal_input[BOTH_Y], "human":_("Both Limit Y"), 'index':BOTH_Y},
	{"name":d_hal_input[BOTH_Z], "human":_("Both Limit Z"), 'index':BOTH_Z},
	{"name":d_hal_input[BOTH_A], "human":_("Both Limit A"), 'index':BOTH_A},
	{"name":d_hal_input[BOTH_U], "human":_("Both Limit U"), 'index':BOTH_U},
	{"name":d_hal_input[BOTH_V], "human":_("Both Limit V"), 'index':BOTH_V},
	{"name":d_hal_input[ALL_LIMIT], "human":_("All limits"), 'index':ALL_LIMIT},
	{"name":d_hal_input[ALL_HOME], "human":_("All home"), 'index':ALL_HOME},
	{"name":d_hal_input[ALL_LIMIT_HOME], "human":_("All limits + homes"), 'index':ALL_LIMIT_HOME},
	{"name":d_hal_input[DIN0], "human":_("Digital in 0"), 'index':DIN0},
	{"name":d_hal_input[DIN1], "human":_("Digital in 1"), 'index':DIN1},
	{"name":d_hal_input[DIN2], "human":_("Digital in 2"), 'index':DIN2},
	{"name":d_hal_input[DIN3], "human":_("Digital in 3"), 'index':DIN3},
	{"name":d_hal_input[UNUSED_INPUT], "human":_("Unused"), 'index':UNUSED_INPUT}
	]

exclusive_input = {
    HOME_X: (MAX_HOME_X, MIN_HOME_X, BOTH_HOME_X, ALL_HOME, ALL_LIMIT_HOME),
    HOME_Y: (MAX_HOME_Y, MIN_HOME_Y, BOTH_HOME_Y, ALL_HOME, ALL_LIMIT_HOME),
    HOME_Z: (MAX_HOME_Z, MIN_HOME_Z, BOTH_HOME_Z, ALL_HOME, ALL_LIMIT_HOME),
    HOME_A: (MAX_HOME_A, MIN_HOME_A, BOTH_HOME_A, ALL_HOME, ALL_LIMIT_HOME),

    MAX_HOME_X: (HOME_X, MIN_HOME_X, MAX_HOME_X, BOTH_HOME_X, ALL_LIMIT, ALL_HOME, ALL_LIMIT_HOME),
    MAX_HOME_Y: (HOME_Y, MIN_HOME_Y, MAX_HOME_Y, BOTH_HOME_Y, ALL_LIMIT, ALL_HOME, ALL_LIMIT_HOME),
    MAX_HOME_Z: (HOME_Z, MIN_HOME_Z, MAX_HOME_Z, BOTH_HOME_Z, ALL_LIMIT, ALL_HOME, ALL_LIMIT_HOME),
    MAX_HOME_A: (HOME_A, MIN_HOME_A, MAX_HOME_A, BOTH_HOME_A, ALL_LIMIT, ALL_HOME, ALL_LIMIT_HOME),

    MIN_HOME_X:  (HOME_X, MAX_HOME_X, BOTH_HOME_X, ALL_LIMIT, ALL_HOME, ALL_LIMIT_HOME),
    MIN_HOME_Y:  (HOME_Y, MAX_HOME_Y, BOTH_HOME_Y, ALL_LIMIT, ALL_HOME, ALL_LIMIT_HOME),
    MIN_HOME_Z:  (HOME_Z, MAX_HOME_Z, BOTH_HOME_Z, ALL_LIMIT, ALL_HOME, ALL_LIMIT_HOME),
    MIN_HOME_A:  (HOME_A, MAX_HOME_A, BOTH_HOME_A, ALL_LIMIT, ALL_HOME, ALL_LIMIT_HOME),

    BOTH_HOME_X:  (HOME_X, MAX_HOME_X, MIN_HOME_X, ALL_LIMIT, ALL_HOME, ALL_LIMIT_HOME),
    BOTH_HOME_Y:  (HOME_Y, MAX_HOME_Y, MIN_HOME_Y, ALL_LIMIT, ALL_HOME, ALL_LIMIT_HOME),
    BOTH_HOME_Z:  (HOME_Z, MAX_HOME_Z, MIN_HOME_Z, ALL_LIMIT, ALL_HOME, ALL_LIMIT_HOME),
    BOTH_HOME_A:  (HOME_A, MAX_HOME_A, MIN_HOME_A, ALL_LIMIT, ALL_HOME, ALL_LIMIT_HOME),

    MIN_X: (BOTH_X, BOTH_HOME_X, MIN_HOME_X, ALL_LIMIT, ALL_LIMIT_HOME),
    MIN_Y: (BOTH_Y, BOTH_HOME_Y, MIN_HOME_Y, ALL_LIMIT, ALL_LIMIT_HOME),
    MIN_Z: (BOTH_Z, BOTH_HOME_Z, MIN_HOME_Z, ALL_LIMIT, ALL_LIMIT_HOME),
    MIN_A: (BOTH_A, BOTH_HOME_A, MIN_HOME_A, ALL_LIMIT, ALL_LIMIT_HOME),

    MAX_X: (BOTH_X, BOTH_HOME_X, MIN_HOME_X, ALL_LIMIT, ALL_LIMIT_HOME),
    MAX_Y: (BOTH_Y, BOTH_HOME_Y, MIN_HOME_Y, ALL_LIMIT, ALL_LIMIT_HOME),
    MAX_Z: (BOTH_Z, BOTH_HOME_Z, MIN_HOME_Z, ALL_LIMIT, ALL_LIMIT_HOME),
    MAX_A: (BOTH_A, BOTH_HOME_A, MIN_HOME_A, ALL_LIMIT, ALL_LIMIT_HOME),

    BOTH_X: (MIN_X, MAX_X, MIN_HOME_X, MAX_HOME_X, BOTH_HOME_X, ALL_LIMIT, ALL_LIMIT_HOME),
    BOTH_Y: (MIN_Y, MAX_Y, MIN_HOME_Y, MAX_HOME_Y, BOTH_HOME_Y, ALL_LIMIT, ALL_LIMIT_HOME),
    BOTH_Z: (MIN_Z, MAX_Z, MIN_HOME_Z, MAX_HOME_Z, BOTH_HOME_Z, ALL_LIMIT, ALL_LIMIT_HOME),
    BOTH_A: (MIN_A, MAX_A, MIN_HOME_A, MAX_HOME_A, BOTH_HOME_A, ALL_LIMIT, ALL_LIMIT_HOME),

    ALL_LIMIT: (
        MIN_X, MAX_X, BOTH_X, MIN_HOME_X, MAX_HOME_X, BOTH_HOME_X,
        MIN_Y, MAX_Y, BOTH_Y, MIN_HOME_Y, MAX_HOME_Y, BOTH_HOME_Y,
        MIN_Z, MAX_Z, BOTH_Z, MIN_HOME_Z, MAX_HOME_Z, BOTH_HOME_Z,
        MIN_A, MAX_A, BOTH_A, MIN_HOME_A, MAX_HOME_A, BOTH_HOME_A,
        ALL_LIMIT_HOME),
    ALL_HOME: (
        HOME_X, MIN_HOME_X, MAX_HOME_X, BOTH_HOME_X,
        HOME_Y, MIN_HOME_Y, MAX_HOME_Y, BOTH_HOME_Y,
        HOME_Z, MIN_HOME_Z, MAX_HOME_Z, BOTH_HOME_Z,
        HOME_A, MIN_HOME_A, MAX_HOME_A, BOTH_HOME_A,
        ALL_LIMIT_HOME),
    ALL_LIMIT_HOME: (
        HOME_X, MIN_HOME_X, MAX_HOME_X, BOTH_HOME_X,
        HOME_Y, MIN_HOME_Y, MAX_HOME_Y, BOTH_HOME_Y,
        HOME_Z, MIN_HOME_Z, MAX_HOME_Z, BOTH_HOME_Z,
        HOME_A, MIN_HOME_A, MAX_HOME_A, BOTH_HOME_A,
        MIN_X, MAX_X, BOTH_X, MIN_HOME_X, MAX_HOME_X, BOTH_HOME_X,
        MIN_Y, MAX_Y, BOTH_Y, MIN_HOME_Y, MAX_HOME_Y, BOTH_HOME_Y,
        MIN_Z, MAX_Z, BOTH_Z, MIN_HOME_Z, MAX_HOME_Z, BOTH_HOME_Z,
        MIN_A, MAX_A, BOTH_A, MIN_HOME_A, MAX_HOME_A, BOTH_HOME_A,
        ALL_LIMIT, ALL_HOME),
}
