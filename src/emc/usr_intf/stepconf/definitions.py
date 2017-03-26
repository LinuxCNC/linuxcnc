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

hal_output = [
	{"name":"xstep", "human":_("X Step"), 'index':XSTEP},
	{"name":"xdir", "human":_("X Direction"), 'index':XDIR},
	{"name":"ystep", "human":_("Y Step"), 'index':YSTEP},
	{"name":"ydir", "human":_("Y Direction"), 'index':YDIR},
	{"name":"zstep", "human":_("Z Step"), 'index':ZSTEP},
	{"name":"zdir", "human":_("Z Direction"), 'index':ZDIR},
	{"name":"astep", "human":_("A Step"), 'index':ASTEP},
	{"name":"adir", "human":_("A Direction"), 'index':ADIR},
	{"name":"ustep", "human":_("U Step"), 'index':USTEP},
	{"name":"udir", "human":_("U Direction"), 'index':UDIR},
	{"name":"vstep", "human":_("V Step"), 'index':VSTEP},
	{"name":"vdir", "human":_("V Direction"), 'index':VDIR},
	{"name":"spindle-on", "human":_("Spindle ON"), 'index':ON},
	{"name":"spindle-cw", "human":_("Spindle CW"), 'index':CW},
	{"name":"spindle-ccw", "human":_("Spindle CCW"), 'index':CCW},
	{"name":"spindle-pwm", "human":_("Spindle PWM"), 'index':PWM},
	{"name":"spindle-brake", "human":_("Spindle Brake"), 'index':BRAKE},
	{"name":"coolant-mist", "human":_("Coolant Mist"), 'index':MIST},
	{"name":"coolant-flood", "human":_("Coolant Flood"), 'index':FLOOD},
	{"name":"estop-out", "human":_("ESTOP Out"), 'index':ESTOP},
	{"name":"xenable", "human":_("Amplifier Enable"), 'index':AMP},
	{"name":"charge-pump", "human":_("Charge Pump"), 'index':PUMP},
	{"name":"dout-00", "human":_("Digital out 0"), 'index':DOUT0},
	{"name":"dout-01", "human":_("Digital out 1"), 'index':DOUT1},
	{"name":"dout-02", "human":_("Digital out 2"), 'index':DOUT2},
	{"name":"dout-03", "human":_("Digital out 3"), 'index':DOUT3},
	{"name":"unused-output", "human":_("Unused"), 'index':UNUSED_OUTPUT}
	]

hal_input = [
	{"name":"estop-ext", "human":_("ESTOP In"), 'index':ESTOP_IN},
	{"name":"probe-in", "human":_("Probe In"), 'index':PROBE},
	{"name":"spindle-index", "human":_("Spindle Index"), 'index':PPR},
	{"name":"spindle-phase-a", "human":_("Spindle Phase A"), 'index':PHA},
	{"name":"spindle-phase-b", "human":_("Spindle Phase B"), 'index':PHB},
	{"name":"home-x", "human":_("Home X"), 'index':HOME_X},
	{"name":"home-y", "human":_("Home Y"), 'index':HOME_Y},
	{"name":"home-z", "human":_("Home Z"), 'index':HOME_Z},
	{"name":"home-a", "human":_("Home A"), 'index':HOME_A},
	{"name":"home-u", "human":_("Home U"), 'index':HOME_U},
	{"name":"home-v", "human":_("Home V"), 'index':HOME_V},
	{"name":"min-home-x", "human":_("Minimum Limit + Home X"), 'index':MIN_HOME_X},
	{"name":"min-home-y", "human":_("Minimum Limit + Home Y"), 'index':MIN_HOME_Y},
	{"name":"min-home-z", "human":_("Minimum Limit + Home Z"), 'index':MIN_HOME_Z},
	{"name":"min-home-a", "human":_("Minimum Limit + Home A"), 'index':MIN_HOME_A},
	{"name":"min-home-u", "human":_("Minimum Limit + Home U"), 'index':MIN_HOME_U},
	{"name":"min-home-v", "human":_("Minimum Limit + Home V"), 'index':MIN_HOME_V},
	{"name":"max-home-x", "human":_("Maximum Limit + Home X"), 'index':MAX_HOME_X},
	{"name":"max-home-y", "human":_("Maximum Limit + Home Y"), 'index':MAX_HOME_Y},
	{"name":"max-home-z", "human":_("Maximum Limit + Home Z"), 'index':MAX_HOME_Z},
	{"name":"max-home-a", "human":_("Maximum Limit + Home A"), 'index':MAX_HOME_A},
	{"name":"max-home-u", "human":_("Maximum Limit + Home U"), 'index':MAX_HOME_U},
	{"name":"max-home-v", "human":_("Maximum Limit + Home V"), 'index':MAX_HOME_V},
	{"name":"both-home-x", "human":_("Both Limit + Home X"), 'index':BOTH_HOME_X},
	{"name":"both-home-y", "human":_("Both Limit + Home Y"), 'index':BOTH_HOME_Y},
	{"name":"both-home-z", "human":_("Both Limit + Home Z"), 'index':BOTH_HOME_Z},
	{"name":"both-home-a", "human":_("Both Limit + Home A"), 'index':BOTH_HOME_A},
	{"name":"both-home-u", "human":_("Both Limit + Home U"), 'index':BOTH_HOME_U},
	{"name":"both-home-v", "human":_("Both Limit + Home V"), 'index':BOTH_HOME_V},
	{"name":"min-x", "human":_("Minimum Limit X"), 'index':MIN_X},
	{"name":"min-y", "human":_("Minimum Limit Y"), 'index':MIN_Y},
	{"name":"min-z", "human":_("Minimum Limit Z"), 'index':MIN_Z},
	{"name":"min-a", "human":_("Minimum Limit A"), 'index':MIN_A},
	{"name":"min-u", "human":_("Minimum Limit U"), 'index':MIN_U},
	{"name":"min-v", "human":_("Minimum Limit V"), 'index':MIN_V},
	{"name":"max-x", "human":_("Maximum Limit X"), 'index':MAX_X},
	{"name":"max-y", "human":_("Maximum Limit Y"), 'index':MAX_Y},
	{"name":"max-z", "human":_("Maximum Limit Z"), 'index':MAX_Z},
	{"name":"max-a", "human":_("Maximum Limit A"), 'index':MAX_A},
	{"name":"max-u", "human":_("Maximum Limit U"), 'index':MAX_U},
	{"name":"max-v", "human":_("Maximum Limit V"), 'index':MAX_V},
	{"name":"both-x", "human":_("Both Limit X"), 'index':BOTH_X},
	{"name":"both-y", "human":_("Both Limit Y"), 'index':BOTH_Y},
	{"name":"both-z", "human":_("Both Limit Z"), 'index':BOTH_Z},
	{"name":"both-a", "human":_("Both Limit A"), 'index':BOTH_A},
	{"name":"both-u", "human":_("Both Limit U"), 'index':BOTH_U},
	{"name":"both-v", "human":_("Both Limit V"), 'index':BOTH_V},
	{"name":"all-limit", "human":_("All limits"), 'index':ALL_LIMIT},
	{"name":"all-home", "human":_("All home"), 'index':ALL_HOME},
	{"name":"all-limit-home", "human":_("All limits + homes"), 'index':ALL_LIMIT_HOME},
	{"name":"din-00", "human":_("Digital in 0"), 'index':DIN0},
	{"name":"din-01", "human":_("Digital in 1"), 'index':DIN1},
	{"name":"din-02", "human":_("Digital in 2"), 'index':DIN2},
	{"name":"din-03", "human":_("Digital in 3"), 'index':DIN3},
	{"name":"unused-input", "human":_("Unused"), 'index':UNUSED_INPUT}
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
