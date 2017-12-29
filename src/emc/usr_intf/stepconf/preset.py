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
from stepconf.definitions import *


# PRESET
# Please do not use 0 as index
GECKO201=100
GECKO202=101
GECKO203V=102
GECKO210=103
GECKO212=104
GECKO320=105
GECKO540=106
L297=200
PMDX150=201
SHERLINE=202
XYLOTEX=203
OEM750=204
JVLSMD41=205
HOBBYCNC=206
KELING=207
TB6560_3AXES=300
TB6560_4AXES=301
STEPCRAFT420=400
STEPCRAFT600=401
STEPCRAFT840=402
STEPCRAFT840_HF=403
STEPCRAFT840_HF_TOOL_CHANGE=404
LEADSHINE_MX3660=600

preset_machines = [
	{'name':'gecko201', 'human':_("Gecko 201") + ' (Untested)', 'index':GECKO201,
		'steptime':500, 'stepspace':4000, 'dirhold':20000, 'dirsetup':1000
	},
	{'name':'gecko202', 'human':_("Gecko 202") + ' (Untested)', 'index':GECKO202,
		'steptime':500, 'stepspace':4500, 'dirhold':20000, 'dirsetup':1000
	},
	{'name':'gecko203v', 'human':_("Gecko 203v") + ' (Untested)', 'index':GECKO203V,
		'steptime':1000, 'stepspace':2000, 'dirhold':200, 'dirsetup':200
	},
	{'name':'gecko210', 'human':_("Gecko 210") + ' (Untested)', 'index':GECKO210,
		'steptime':500, 'stepspace':4000, 'dirhold':20000, 'dirsetup':1000
	},
	{'name':'gecko212', 'human':_("Gecko 212") + ' (Untested)', 'index':GECKO212,
		'steptime':500, 'stepspace':4000, 'dirhold':20000, 'dirsetup':1000
	},
	{'name':'gecko320', 'human':_("Gecko 320") + ' (Untested)', 'index':GECKO320,
		'steptime':3500, 'stepspace':500, 'dirhold':200, 'dirsetup':200
	},
	{'name':'gecko540', 'human':_("Gecko 540") + ' (Untested)', 'index':GECKO540,
		'steptime':1000, 'stepspace':2000, 'dirhold':200, 'dirsetup':200
	},
	{'name':'oem750', 'human':_("Parker-Compumotor oem750") + ' (Untested)', 'index':OEM750,
		'steptime':1000, 'stepspace':1000, 'dirhold':1000, 'dirsetup':200000
	},
	{'name':'l297', 'human':_("L297") + ' (Untested)', 'index':L297,
		'steptime':500, 'stepspace':4000, 'dirhold':4000, 'dirsetup':1000
	},
	{'name':'pmdx150', 'human':_("PMDX-150") + ' (Untested)', 'index':PMDX150,
		'steptime':1000, 'stepspace':2000, 'dirhold':1000, 'dirsetup':1000
	},
	{'name':'jvlsmd41', 'human':_("JVL-SMD41 or 42") + ' (Untested)', 'index':JVLSMD41,
		'steptime':500, 'stepspace':500, 'dirhold':2500, 'dirsetup':2500
	},
	{'name':'hobbycnc', 'human':_("Hobbycnc Pro Chopper") + ' (Untested)', 'index':HOBBYCNC,
		'steptime':2000, 'stepspace':2000, 'dirhold':2000, 'dirsetup':2000
	},
	{'name':'keling', 'human':_("Keling 4030") + ' (Untested)', 'index':KELING,
		'steptime':5000, 'stepspace':5000, 'dirhold':20000, 'dirsetup':20000
	},
	{'name':'sherline', 'human':_("Sherline") + ' (Untested)', 'index':SHERLINE,
		'pin1':UNUSED_OUTPUT, 'pin1inv':0,
		'pin2':XDIR, 'pin2inv':0,
		'pin3':XSTEP, 'pin3inv':0,
		'pin4':YDIR, 'pin4inv':0,
		'pin5':YSTEP, 'pin5inv':0,
		'pin6':ZDIR, 'pin6inv':0,
		'pin7':ZSTEP, 'pin7inv':0,
		'pin8':ADIR, 'pin8inv':0,
		'pin9':ASTEP, 'pin9inv':0,
		'pin14':UNUSED_OUTPUT, 'pin14inv':0,
		'pin16':UNUSED_OUTPUT, 'pin16inv':0,
		'pin17':UNUSED_OUTPUT, 'pin17inv':0,
		'pin10':UNUSED_INPUT, 'pin10inv':0,
		'pin11':UNUSED_INPUT, 'pin11inv':0,
		'pin12':UNUSED_INPUT, 'pin12inv':0,
		'pin13':UNUSED_INPUT, 'pin13inv':0,
		'pin15':UNUSED_INPUT, 'pin15inv':0,
		'steptime':22000, 'stepspace':22000, 'dirhold':100000, 'dirsetup':100000
	},
	{'name':'xylotex', 'human': _("Xylotex") + ' (Untested)', 'index':XYLOTEX,
		'pin1':UNUSED_OUTPUT, 'pin1inv':0,
		'pin2':XSTEP, 'pin2inv':0,
		'pin3':XDIR, 'pin3inv':0,
		'pin4':YSTEP, 'pin4inv':0,
		'pin5':YDIR, 'pin5inv':0,
		'pin6':ZSTEP, 'pin6inv':0,
		'pin7':ZDIR, 'pin7inv':0,
		'pin8':ASTEP, 'pin8inv':0,
		'pin9':ADIR, 'pin9inv':0,
		'pin14':UNUSED_OUTPUT, 'pin14inv':0,
		'pin16':UNUSED_OUTPUT, 'pin16inv':0,
		'pin17':UNUSED_OUTPUT, 'pin17inv':0,
		'pin10':UNUSED_INPUT, 'pin10inv':0,
		'pin11':UNUSED_INPUT, 'pin11inv':0,
		'pin12':UNUSED_INPUT, 'pin12inv':0,
		'pin13':UNUSED_INPUT, 'pin13inv':0,
		'pin15':UNUSED_INPUT, 'pin15inv':0,
		'steptime':2000, 'stepspace':1000, 'dirhold':200, 'dirsetup':200
	},
	{'name':'tb6560_3axes', 'human':_("TB6560 3 axes") + ' (Untested)', 'index':TB6560_3AXES,
		'pin1':XSTEP, 'pin1inv':0,
		'pin2':ON, 'pin2inv':0,
		'pin3':ZSTEP, 'pin3inv':0,
		'pin4':AMP, 'pin4inv':0,
		'pin5':AMP, 'pin5inv':0,			
		'pin6':ZDIR, 'pin6inv':0,
		'pin7':YDIR, 'pin7inv':0,
		'pin8':UNUSED_OUTPUT, 'pin8inv':0,
		'pin9':UNUSED_OUTPUT, 'pin9inv':0,
		'pin14':YSTEP, 'pin14inv':0,		
		'pin16':XDIR, 'pin16inv':0,
		'pin17':AMP, 'pin17inv':0,
		'pin10':UNUSED_INPUT, 'pin10inv':0,
		'pin11':UNUSED_INPUT, 'pin11inv':0,
		'pin12':UNUSED_INPUT, 'pin12inv':0,
		'pin13':UNUSED_INPUT, 'pin13inv':0,
		'pin15':UNUSED_INPUT, 'pin15inv':0,
		'steptime':5000, 'stepspace':5000, 'dirhold':20000, 'dirsetup':20000
	},
	{'name':'tb6560_4axes', 'human':_("TB6560 4 axes") + ' (Untested)', 'index':TB6560_4AXES,
		'pin1':AMP, 'pin1inv':0,
		'pin2':XSTEP, 'pin2inv':0,
		'pin3':XDIR, 'pin3inv':0,
		'pin4':YSTEP, 'pin4inv':0,
		'pin5':YDIR, 'pin5inv':0,
		'pin6':ZSTEP, 'pin6inv':0,
		'pin7':ZDIR, 'pin7inv':0,
		'pin8':ASTEP, 'pin8inv':0,
		'pin9':ADIR, 'pin9inv':0,
		'pin14':UNUSED_OUTPUT, 'pin14inv':0,
		'pin16':UNUSED_OUTPUT, 'pin16inv':0,
		'pin17':UNUSED_OUTPUT, 'pin17inv':0,
		'pin10':UNUSED_INPUT, 'pin10inv':0,
		'pin11':UNUSED_INPUT, 'pin11inv':0,
		'pin12':UNUSED_INPUT, 'pin12inv':0,
		'pin13':UNUSED_INPUT, 'pin13inv':0,
		'pin15':UNUSED_INPUT, 'pin15inv':0,
		'steptime':5000, 'stepspace':5000, 'dirhold':20000, 'dirsetup':20000
	},
	{'name':'stepcraft420', 'human':_("Stepcraft 420") + ' (Untested)', 'index':STEPCRAFT420, 'units':MM,
		'pin1':ON, 'pin1inv':0,
		'pin2':XDIR, 'pin2inv':1,
		'pin3':XSTEP, 'pin3inv':0,
		'pin4':YDIR, 'pin4inv':1,
		'pin5':YSTEP, 'pin5inv':0,
		'pin6':ZDIR, 'pin6inv':0,
		'pin7':ZSTEP,'pin7inv':0,
		'pin8':UNUSED_OUTPUT, 'pin8inv':0,
		'pin9':UNUSED_OUTPUT, 'pin9inv':0,
		'pin14':DOUT2, 'pin14inv':0,
		'pin16':DOUT3, 'pin16inv':0,
		'pin17':UNUSED_OUTPUT, 'pin17inv':0,
		'pin10':PROBE, 'pin10inv':0,
		'pin11':ESTOP_IN, 'pin11inv':1,
		'pin12':ALL_HOME, 'pin12inv':0,
		'pin13':UNUSED_INPUT, 'pin13inv':0,
		'pin15':UNUSED_INPUT, 'pin15inv':0,
		'xsteprev':400, 'xmicrostep':1, 'xpulleyden':1, 'xpulleynum':1, 'xleadscrew':3, 'xmaxvel':40, 'xmaxacc' :150,
		'xhomepos':290, 'xminlim':0, 'xmaxlim':290, 'xhomesw':292, 'xhomevel':3, 'xlatchdir':0,
		'ysteprev':400, 'ymicrostep':1, 'ypulleyden':1, 'ypulleynum':1, 'yleadscrew':3, 'ymaxvel':40, 'ymaxacc' :150,
		'yhomepos':0, 'yminlim':0, 'ymaxlim':412, 'yhomesw':-2, 'yhomevel':-3, 'ylatchdir':0,
		'zsteprev':400, 'zmicrostep':1, 'zpulleyden':1, 'zpulleynum':1, 'zleadscrew':3, 'zmaxvel':30, 'zmaxacc' :150,
		'zhomepos':122, 'zminlim':0, 'zmaxlim':122, 'zhomesw':124, 'zhomevel':3, 'zlatchdir':0,
		'probe_x_pos':258, 'probe_y_pos':6, 'probe_z_pos':80, 'probe_sensor_height':33.7,
		'steptime':1000, 'stepspace':1000, 'dirhold':20000, 'dirsetup':20000
	},
	{'name':'stepcraft600', 'human':_("Stepcraft 600") + ' (Untested)', 'index':STEPCRAFT600, 'units':MM,
		'pin1':ON, 'pin1inv':0,
		'pin2':XDIR, 'pin2inv':1,
		'pin3':XSTEP, 'pin3inv':0,
		'pin4':YDIR, 'pin4inv':1,
		'pin5':YSTEP, 'pin5inv':0,
		'pin6':ZDIR, 'pin6inv':0,
		'pin7':ZSTEP,'pin7inv':0,
		'pin8':UNUSED_OUTPUT, 'pin8inv':0,
		'pin9':UNUSED_OUTPUT, 'pin9inv':0,
		'pin14':DOUT2, 'pin14inv':0,
		'pin16':DOUT3, 'pin16inv':0,
		'pin17':UNUSED_OUTPUT, 'pin17inv':0,
		'pin10':PROBE, 'pin10inv':0,
		'pin11':ESTOP_IN, 'pin11inv':1,
		'pin12':ALL_HOME, 'pin12inv':0,
		'pin13':UNUSED_INPUT, 'pin13inv':0,
		'pin15':UNUSED_INPUT, 'pin15inv':0,
		'xsteprev':400, 'xmicrostep':1, 'xpulleyden':1, 'xpulleynum':1, 'xleadscrew':3, 'xmaxvel':40, 'xmaxacc' :150,
		'xhomepos':410, 'xminlim':0, 'xmaxlim':410, 'xhomesw':412, 'xhomevel':3, 'xlatchdir':0,
		'ysteprev':400, 'ymicrostep':1, 'ypulleyden':1, 'ypulleynum':1, 'yleadscrew':3, 'ymaxvel':40, 'ymaxacc' :150,
		'yhomepos':0, 'yminlim':0, 'ymaxlim':592, 'yhomesw':-2, 'yhomevel':-3, 'ylatchdir':0,
		'zsteprev':400, 'zmicrostep':1, 'zpulleyden':1, 'zpulleynum':1, 'zleadscrew':3, 'zmaxvel':30, 'zmaxacc' :150,
		'zhomepos':122, 'zminlim':0, 'zmaxlim':122, 'zhomesw':124, 'zhomevel':3, 'zlatchdir':0,
		'probe_x_pos':378, 'probe_y_pos':6, 'probe_z_pos':80, 'probe_sensor_height':33.7,
		'steptime':1000, 'stepspace':1000, 'dirhold':20000, 'dirsetup':20000
	},
	{'name':'stepcraft840', 'human':_("Stepcraft 840"), 'index':STEPCRAFT840, 'units':MM,
		'pin1':ON, 'pin1inv':0,
		'pin2':XDIR, 'pin2inv':1,
		'pin3':XSTEP, 'pin3inv':0,
		'pin4':YDIR, 'pin4inv':1,
		'pin5':YSTEP, 'pin5inv':0,
		'pin6':ZDIR, 'pin6inv':0,
		'pin7':ZSTEP,'pin7inv':0,
		'pin8':UNUSED_OUTPUT, 'pin8inv':0,
		'pin9':UNUSED_OUTPUT, 'pin9inv':0,
		'pin14':DOUT2, 'pin14inv':0,
		'pin16':DOUT3, 'pin16inv':0,
		'pin17':UNUSED_OUTPUT, 'pin17inv':0,
		'pin10':PROBE, 'pin10inv':0,
		'pin11':ESTOP_IN, 'pin11inv':1,
		'pin12':ALL_HOME, 'pin12inv':0,
		'pin13':UNUSED_INPUT, 'pin13inv':0,
		'pin15':UNUSED_INPUT, 'pin15inv':0,
		'xsteprev':400, 'xmicrostep':1, 'xpulleyden':1, 'xpulleynum':1, 'xleadscrew':3, 'xmaxvel':40, 'xmaxacc' :150,
		'xhomepos':590, 'xminlim':0, 'xmaxlim':590, 'xhomesw':592, 'xhomevel':3, 'xlatchdir':0,
		'ysteprev':400, 'ymicrostep':1, 'ypulleyden':1, 'ypulleynum':1, 'yleadscrew':3, 'ymaxvel':40, 'ymaxacc' :150,
		'yhomepos':0, 'yminlim':0, 'ymaxlim':832, 'yhomesw':-2, 'yhomevel':-3, 'ylatchdir':0,
		'zsteprev':400, 'zmicrostep':1, 'zpulleyden':1, 'zpulleynum':1, 'zleadscrew':3, 'zmaxvel':30, 'zmaxacc' :150,
		'zhomepos':122, 'zminlim':0, 'zmaxlim':122, 'zhomesw':124, 'zhomevel':3, 'zlatchdir':0,
		'probe_x_pos':558, 'probe_y_pos':7, 'probe_z_pos':80, 'probe_sensor_height':33.7,
		'steptime':1000, 'stepspace':1000, 'dirhold':20000, 'dirsetup':20000
	},
	{'name':'stepcraft840_HF', 'human':_("Stepcraft 840 + HF"), 'index':STEPCRAFT840_HF, 'units':MM,
		'pin1':ON, 'pin1inv':0,
		'pin2':XDIR, 'pin2inv':1,
		'pin3':XSTEP, 'pin3inv':0,
		'pin4':YDIR, 'pin4inv':1,
		'pin5':YSTEP, 'pin5inv':0,
		'pin6':ZDIR, 'pin6inv':0,
		'pin7':ZSTEP,'pin7inv':0,
		'pin8':UNUSED_OUTPUT, 'pin8inv':0,
		'pin9':UNUSED_OUTPUT, 'pin9inv':0,
		'pin14':DOUT2, 'pin14inv':0,
		'pin16':DOUT3, 'pin16inv':0,
		'pin17':PWM, 'pin17inv':0,
		'pin10':PROBE, 'pin10inv':0,
		'pin11':ESTOP_IN, 'pin11inv':1,
		'pin12':ALL_HOME, 'pin12inv':0,
		'pin13':UNUSED_INPUT, 'pin13inv':0,
		'pin15':UNUSED_INPUT, 'pin15inv':0,
		'xsteprev':400, 'xmicrostep':1, 'xpulleyden':1, 'xpulleynum':1, 'xleadscrew':3, 'xmaxvel':40, 'xmaxacc' :150,
		'xhomepos':590, 'xminlim':0, 'xmaxlim':590, 'xhomesw':592, 'xhomevel':3, 'xlatchdir':0,
		'ysteprev':400, 'ymicrostep':1, 'ypulleyden':1, 'ypulleynum':1, 'yleadscrew':3, 'ymaxvel':40, 'ymaxacc' :150,
		'yhomepos':0, 'yminlim':0, 'ymaxlim':832, 'yhomesw':-2, 'yhomevel':-3, 'ylatchdir':0,
		'zsteprev':400, 'zmicrostep':1, 'zpulleyden':1, 'zpulleynum':1, 'zleadscrew':3, 'zmaxvel':30, 'zmaxacc' :150,
		'zhomepos':122, 'zminlim':0, 'zmaxlim':122, 'zhomesw':124, 'zhomevel':3, 'zlatchdir':0,
		'probe_x_pos':558, 'probe_y_pos':7, 'probe_z_pos':80, 'probe_sensor_height':33.7,
		'steptime':1000, 'stepspace':1000, 'dirhold':20000, 'dirsetup':20000,
		'spindlecarrier':1500, 
		'spindlepwm1':0.18, 'spindlepwm2':1.0, 'spindlespeed1':3000, 'spindlespeed2':20000,
		'spindleatspeed':0, 'spindlecpr':1000, 'spindlefiltergain':0.010, 'spindlenearscale':150,
	},
	{'name':'stepcraft840_HF_TOOL_CHANGE', 'human':_("Stepcraft 840 + HF + Tool Change"), 'index':STEPCRAFT840_HF_TOOL_CHANGE, 'units':MM,
		'pin1':ON, 'pin1inv':0,
		'pin2':XDIR, 'pin2inv':1,
		'pin3':XSTEP, 'pin3inv':0,
		'pin4':YDIR, 'pin4inv':1,
		'pin5':YSTEP, 'pin5inv':0,
		'pin6':ZDIR, 'pin6inv':0,
		'pin7':ZSTEP,'pin7inv':0,
		'pin8':UNUSED_OUTPUT, 'pin8inv':0,
		'pin9':UNUSED_OUTPUT, 'pin9inv':0,
		'pin14':TOOL_CHANGE, 'pin14inv':0,
		'pin16':DOUT3, 'pin16inv':0,
		'pin17':PWM, 'pin17inv':0,
		'pin10':PROBE, 'pin10inv':0,
		'pin11':ESTOP_IN, 'pin11inv':1,
		'pin12':ALL_HOME, 'pin12inv':0,
		'pin13':UNUSED_INPUT, 'pin13inv':0,
		'pin15':UNUSED_INPUT, 'pin15inv':0,
		'xsteprev':400, 'xmicrostep':1, 'xpulleyden':1, 'xpulleynum':1, 'xleadscrew':3, 'xmaxvel':40, 'xmaxacc' :150,
		'xhomepos':590, 'xminlim':0, 'xmaxlim':590, 'xhomesw':592, 'xhomevel':3, 'xlatchdir':0,
		'ysteprev':400, 'ymicrostep':1, 'ypulleyden':1, 'ypulleynum':1, 'yleadscrew':3, 'ymaxvel':40, 'ymaxacc' :150,
		'yhomepos':0, 'yminlim':0, 'ymaxlim':832, 'yhomesw':-2, 'yhomevel':-3, 'ylatchdir':0,
		'zsteprev':400, 'zmicrostep':1, 'zpulleyden':1, 'zpulleynum':1, 'zleadscrew':3, 'zmaxvel':30, 'zmaxacc' :150,
		'zhomepos':122, 'zminlim':0, 'zmaxlim':122, 'zhomesw':124, 'zhomevel':3, 'zlatchdir':0,
		'probe_x_pos':558, 'probe_y_pos':7, 'probe_z_pos':80, 'probe_sensor_height':32.6,
		'steptime':1000, 'stepspace':1000, 'dirhold':20000, 'dirsetup':20000,
		'spindlecarrier':1500, 
		'spindlepwm1':0.18, 'spindlepwm2':1.0, 'spindlespeed1':3000, 'spindlespeed2':20000,
		'spindleatspeed':0, 'spindlecpr':1000, 'spindlefiltergain':0.010, 'spindlenearscale':150,
	},
		{'name':'leadshineMX3660', 'human':_("Leadshine MX3660 3 axis") + ' (Untested)', 'index':LEADSHINE_MX3660, 'units':MM,
		'pin1':DOUT1, 'pin1inv':0,
		'pin2':XSTEP, 'pin2inv':1,
		'pin3':XDIR, 'pin3inv':0,
		'pin4':YSTEP, 'pin4inv':1,
		'pin5':YDIR, 'pin5inv':0,
		'pin6':ZSTEP, 'pin6inv':0,
		'pin7':ZDIR,'pin7inv':0,
		'pin8':DOUT2, 'pin8inv':0,
		'pin9':DOUT3, 'pin9inv':0,
		'pin14':PWM, 'pin14inv':0,
		'pin16':PUMP, 'pin16inv':0,
		'pin17':DOUT0, 'pin17inv':0,
		'pin10':DIN0, 'pin10inv':0,
		'pin11':DIN1, 'pin11inv':1,
		'pin12':DIN2, 'pin12inv':0,
		'pin13':DIN3, 'pin13inv':0,
		'pin15':ESTOP_IN, 'pin15inv':0
	}
]
