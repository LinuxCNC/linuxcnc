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
preset_machines = [
	{'name':'sherline', 'human':'Sherline (Untested)', 'index':1,
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
		'pin15':UNUSED_INPUT, 'pin15inv':0
	},
	{'name':'xylotex', 'human':'Xylotex (Untested)', 'index':2,
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
		'pin15':UNUSED_INPUT, 'pin15inv':0
	},	
	{'name':'tb6560_3axes', 'human':'TB6560 3 axes (Untested)', 'index':3,
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
		'pin15':UNUSED_INPUT, 'pin15inv':0		
	},		
	{'name':'tb6560_4axes', 'human':'TB6560 4 axes (Untested)', 'index':4,
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
		'pin15':UNUSED_INPUT, 'pin15inv':0
	},
	{'name':'stepcraft-420', 'human':'Stepcraft 420 (Untested)', 'index':5,
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
		'pin15':UNUSED_INPUT, 'pin15inv':0
	},
	{'name':'stepcraft-600', 'human':'Stepcraft 600 (Untested)', 'index':6,
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
		'pin15':UNUSED_INPUT, 'pin15inv':0
	},
	{'name':'stepcraft-840', 'human':'Stepcraft 840', 'index':7,
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
		'xhomepos':590, 'xminlim':0, 'xmaxlim':290, 'xhomesw':592, 'xhomevel':3, 'xlatchdir':0,
		'ysteprev':400, 'ymicrostep':1, 'ypulleyden':1, 'ypulleynum':1, 'yleadscrew':3, 'ymaxvel':40, 'ymaxacc' :150,
		'yhomepos':0, 'yminlim':0, 'ymaxlim':832, 'yhomesw':-2, 'yhomevel':-3, 'ylatchdir':0,
		'zsteprev':400, 'zmicrostep':1, 'zpulleyden':1, 'zpulleynum':1, 'zleadscrew':3, 'zmaxvel':30, 'zmaxacc' :150,
		'zhomepos':122, 'zminlim':0, 'zmaxlim':122, 'zhomesw':124, 'zhomevel':3, 'zlatchdir':0,
	}
]

"""
        for i in ('x','y','z','u','v'):
"""
"""
    # preset out pins
    def preset_sherline_outputs(self):
        self.w.pin2.set_active(1)
        self.w.pin3.set_active(0)
        self.w.pin4.set_active(3)
        self.w.pin5.set_active(2)
        self.w.pin6.set_active(5)
        self.w.pin7.set_active(4)
        self.w.pin8.set_active(7)
        self.w.pin9.set_active(6)

    def preset_xylotex_outputs(self):
        self.w.pin2.set_active(0)
        self.w.pin3.set_active(1)
        self.w.pin4.set_active(2)
        self.w.pin5.set_active(3)
        self.w.pin6.set_active(4)
        self.w.pin7.set_active(5)
        self.w.pin8.set_active(6)
        self.w.pin9.set_active(7)


    def preset_tb6560_3axes_outputs(self):
        SIG = self._p
        def index(signal):
            return self._p.hal_output_names.index(signal)
        # x axis
        self.w.pin1.set_active(index(SIG.XSTEP))
        self.w.pin16.set_active(index(SIG.XDIR))
        self.w.pin4.set_active(index(SIG.AMP))
        # Y axis
        self.w.pin14.set_active(index(SIG.YSTEP))
        self.w.pin7.set_active(index(SIG.YDIR))
        self.w.pin17.set_active(index(SIG.AMP))
        # Z axis
        self.w.pin3.set_active(index(SIG.ZSTEP))
        self.w.pin6.set_active(index(SIG.ZDIR))
        self.w.pin5.set_active(index(SIG.AMP))
        # spindle
        self.w.pin2.set_active(index(SIG.ON))

    def preset_tb6560_4axes_outputs(self):
        SIG = self._p
        def index(signal):
            return self._p.hal_output_names.index(signal)
        # x axis
        self.w.pin2.set_active(index(SIG.XSTEP))
        self.w.pin3.set_active(index(SIG.XDIR))
        self.w.pin1.set_active(index(SIG.AMP))
        # Y axis
        self.w.pin4.set_active(index(SIG.YSTEP))
        self.w.pin5.set_active(index(SIG.YDIR))
        # Z axis
        self.w.pin6.set_active(index(SIG.ZSTEP))
        self.w.pin7.set_active(index(SIG.ZDIR))
        # A axis
        self.w.pin8.set_active(index(SIG.ASTEP))
        self.w.pin9.set_active(index(SIG.ADIR))
"""
