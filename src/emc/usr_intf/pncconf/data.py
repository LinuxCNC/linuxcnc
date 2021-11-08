#!/usr/bin/env python3
# -*- encoding: utf-8 -*-
#    This is pncconf, a graphical configuration editor for LinuxCNC
#    Chris Morley copyright 2009
#    This is based from stepconf, a graphical configuration editor for linuxcnc
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
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
import os
import sys
import errno
import hashlib
import xml.dom.minidom

import subprocess

def md5sum(filename):
    try:
        f = open(filename, "rb")
    except IOError:
        return None
    else:
        return hashlib.md5(f.read()).hexdigest()

# warning any data that doesn't start with a _ will be saved
# and reloaded - this could change the type
class Data:
    def __init__(self, app, private_data_instance,base_dir, linuxcnc_version):
        global _PD
        global _APP
        global _BASE
        _APP = app
        _PD = private_data_instance
        _BASE = base_dir
        self.linuxcnc_version = linuxcnc_version
        # custom signal name lists
        self.halencoderinputsignames = []
        self.halmuxencodersignames = []
        self.halresolversignames = []
        self.hal8i20signames = []
        self.halpwmoutputsignames = []
        self.haltppwmoutputsignames = []
        self.halinputsignames = []
        self.haloutputsignames = []
        self.halsteppersignames = []
        self.halpotsignames = []
        self.halanaloginsignames = []

        # internal flags
        self._mesa0_arrayloaded = False
        self._mesa0_configured = False
        self._mesa1_configured = False
        self._mesa1_arrayloaded = False
        self._components_is_prepared = False

        # internal combobox arrays
        self._notusedliststore = None
        self._gpioliststore = None
        self._stepperliststore = None
        self._encoderliststore = None
        self._muxencoderliststore = None
        self._resolverliststore = None
        self._8i20liststore = None
        self._pwmliststore = None
        self._tppwmliststore = None
        self._sserialliststore = None
        self._potliststore = None
        self._analoginliststore = None

        self._gpioosignaltree = None
        self._gpioisignaltree = None
        self._steppersignaltree = None
        self._encodersignaltree = None
        self._muxencodersignaltree = None
        self._resolversignaltree = None
        self._8i20signaltree = None
        self._pwmcontrolsignaltree = None
        self._pwmrelatedsignaltree = None
        self._tppwmsignaltree = None
        self._sserialsignaltree = None
        self._potsignaltree = None
        self._analoginsignaltree = None

        # pncconf default options
        self.createsymlink = 1
        self.createshortcut = 0
        self.useinisubstitution = False
        self._lastconfigname= ""
        self._chooselastconfig = True
        self._preference_version = 1.0
        self._pncconf_version = 3.0 # This is the actual version of this program
        self.pncconf_loaded_version = 3.0 # This will version number for new configs or be overwritten by a loaded file to the files version.
        self._re_editmode = False
        self._customfirmwarefilename = "~/Desktop/custom_firmware/firmware.py"
        self.advanced_option = False
        self._substitution_list=[]

        # basic machine data
        self.help = "help-welcome.txt"
        self.machinename = _("my_LinuxCNC_machine")
        self.frontend = _PD._AXIS
        self.axes = 0 # XYZ
        self.include_spindle = True
        self.available_axes = []
        self.baseperiod = 50000
        self.servoperiod = 1000000
        self.units = _PD._IMPERIAL # inch
        self.limitsnone = True
        self.limitswitch = False
        self.limitshared = False
        self.homenone = True
        self.homeswitch = False
        self.homeindex = False
        self.homeboth = False
        self.limitstype = 0
        self.homingtype = 0
        self.usbdevicename = "none"
        self.joystickjog = False
        self.joystickjograpidrate0 = 0.1
        self.joystickjograpidrate1 = 1.0
        self.joystickjograpidrate2 = 10.0
        self.joystickjograpidrate3 = 100.0
        self.joycmdrapida = ""
        self.joycmdrapidb = ""
        self.joycmdxpos = ""
        self.joycmdxneg = ""
        self.joycmdypos = ""
        self.joycmdyneg = ""
        self.joycmdzpos = ""
        self.joycmdzneg = ""
        self.joycmdapos = ""
        self.joycmdaneg = ""
        self.joycmdrapid = ""
        self.joycmdanalogx = ""
        self.joycmdanalogy = ""
        self.joycmdanalogz = ""
        self.joycmdanaloga = ""
        self.externaljog = False
        self.singlejogbuttons = False
        self.multijogbuttons = False
        self.jograpidrate = 1.0

        self.externalmpg = False
        self.guimpg = True
        self.multimpg = False
        self.sharedmpg = False
        self.incrselect = False
        self.mpgincrvalue0 = 0     # all incr-select low
        self.mpgincrvalue1 = .0001 # incr-select-a  high
        self.mpgincrvalue2 = .0005 # b
        self.mpgincrvalue3 = .001  # ab
        self.mpgincrvalue4 = .005  # c
        self.mpgincrvalue5 = .01   # ac
        self.mpgincrvalue6 = .05   # bc
        self.mpgincrvalue7 = .1    # abc
        self.mpgincrvalue8 = .125 # d
        self.mpgincrvalue9 = .125  # ad
        self.mpgincrvalue10 = .125  # bd
        self.mpgincrvalue11 = .125  # abd
        self.mpgincrvalue12 = .125 # cd
        self.mpgincrvalue13 = .125 # acd
        self.mpgincrvalue14 = .125 # bcd
        self.mpgincrvalue15 = .125 # abcd
        self.mpgdebounce = True
        self.mpgdebouncetime = .2
        self.mpggraycode = False
        self.mpgignorefalse = False

        self.externalfo = False
        self.fo_usempg = False
        self.fo_useswitch = False
        self.foincrvalue0 = 0  # all incr-select low
        self.foincrvalue1 = 5  # incr-select-a  high
        self.foincrvalue2 = 10  # b
        self.foincrvalue3 = 25  # ab
        self.foincrvalue4 = 50 # c
        self.foincrvalue5 = 75 # ac
        self.foincrvalue6 = 90 # bc
        self.foincrvalue7 = 100 # abc
        self.foincrvalue8 = 110  # d
        self.foincrvalue9 = 125  # ad
        self.foincrvalue10 = 140  # bd
        self.foincrvalue11 = 150  # abd
        self.foincrvalue12 = 165 # cd
        self.foincrvalue13 = 180 # acd
        self.foincrvalue14 = 190 # bcd
        self.foincrvalue15 = 200 # abcd
        self.fodebounce = True
        self.fodebouncetime = .2
        self.fograycode = False
        self.foignorefalse = False

        self.externalso = False
        self.so_usempg = False
        self.so_useswitch = False
        self.soincrvalue0 = 0  # all incr-select low
        self.soincrvalue1 = 5  # incr-select-a  high
        self.soincrvalue2 = 10  # b
        self.soincrvalue3 = 25  # ab
        self.soincrvalue4 = 50 # c
        self.soincrvalue5 = 75 # ac
        self.soincrvalue6 = 90 # bc
        self.soincrvalue7 = 100 # abc
        self.soincrvalue8 = 110  # d
        self.soincrvalue9 = 125  # ad
        self.soincrvalue10 = 140  # bd
        self.soincrvalue11 = 150  # abd
        self.soincrvalue12 = 165 # cd
        self.soincrvalue13 = 180 # acd
        self.soincrvalue14 = 190 # bcd
        self.soincrvalue15 = 200 # abcd
        self.sodebounce = True
        self.sodebouncetime = .2
        self.sograycode = False
        self.soignorefalse = False

        self.externalmvo = False
        self.mvo_usempg = False
        self.mvo_useswitch = False
        self.mvoincrvalue0 = 0  # all incr-select low
        self.mvoincrvalue1 = 5  # incr-select-a  high
        self.mvoincrvalue2 = 10  # b
        self.mvoincrvalue3 = 25  # ab
        self.mvoincrvalue4 = 50 # c
        self.mvoincrvalue5 = 75 # ac
        self.mvoincrvalue6 = 90 # bc
        self.mvoincrvalue7 = 100 # abc
        self.mvoincrvalue8 = 100  # d
        self.mvoincrvalue9 = 100  # ad
        self.mvoincrvalue10 = 100  # bd
        self.mvoincrvalue11 = 100  # abd
        self.mvoincrvalue12 = 100 # cd
        self.mvoincrvalue13 = 100 # acd
        self.mvoincrvalue14 = 100 # bcd
        self.mvoincrvalue15 = 100 # abcd
        self.mvodebounce = True
        self.mvodebouncetime = .2
        self.mvograycode = False
        self.mvoignorefalse = False

        self.gearselect = False
        self.gsincrvalue0 = 0  # all incr-select low
        self.gsincrvalue1 = 0  # incr-select-a  high
        self.gsincrvalue2 = 0  # b
        self.gsincrvalue3 = 0  # ab
        self.gsincrvalue4 = 0 # c
        self.gsincrvalue5 = 0 # ac
        self.gsincrvalue6 = 0 # bc
        self.gsincrvalue7 = 0 # abc
        self.gsincrvalue8 = 0  # d
        self.gsincrvalue9 = 0  # ad
        self.gsincrvalue10 = 0  # bd
        self.gsincrvalue11 = 0  # abd
        self.gsincrvalue12 = 0 # cd
        self.gsincrvalue13 = 0 # acd
        self.gsincrvalue14 = 0 # bcd
        self.gsincrvalue15 = 0 # abcd
        self.gsdebounce = True
        self.gsdebouncetime = .2
        self.gsgraycode = False
        self.gsignorefalse = False

        self.serial_vfd = False

        # GUI frontend defaults
        self.position_offset = 1 # relative
        self.position_feedback = 1 # actual
        self.max_feed_override = 2.0 # percentage
        self.min_spindle_override = .5
        self.max_spindle_override = 1.0
        # These are for AXIS gui only
        # linear jog defaults are set with: set_axis_unit_defaults()
        self.default_angular_velocity = 12
        self.min_angular_velocity = 3
        self.max_angular_velocity = 180
        self.increments_metric = "5mm 1mm .5mm .1mm .05mm .01mm .005mm"
        self.increments_imperial= ".1in .05in .01in .005in .001in .0005in .0001in"
        self.editor = "gedit"
        self.geometry = "xyz"
        self.axisforcemax = False
        self.axissize = [False,0,0]
        self.axisposition = [False,0,0]

        # Gmoccapy
        self.gmcpytheme = "Follow System Theme"

        # Touchy only
        self.touchysize = [False,0,0]
        self.touchyposition = [False,0,0]
        self.touchytheme = "Follow System Theme"
        self.touchyforcemax = False
        self.touchyabscolor = "default"
        self.touchyrelcolor = "default"
        self.touchydtgcolor = "default"
        self.touchyerrcolor = "default"

        # QtPlasmaC
        self.qtplasmacmode = 0
        self.qtplasmacscreen = 0
        self.qtplasmacestop = 0
        self.qtplasmacpmx = ""
        self.increments_metric_qtplasmac = "10mm 1mm .1mm .01mm .001mm"
        self.increments_imperial_qtplasmac= "1in .1in .01in .001in .0001in"
        self.qtplasmac_bnames = ["OHMIC\TEST","PROBE\TEST","SINGLE\CUT","NORMAL\CUT","TORCH\PULSE","FRAMING", \
                                 "","","","","","","","","","","","","",""]
        self.qtplasmac_bcodes = ["ohmic-test","probe-test 10","single-cut","cut-type","torch-pulse 0.5","framing", \
                                 "","","","","","","","","","","","","",""]
        self._arcvpin = None
        self.voltsmodel = "10"
        self.voltsfjumper = "32"
        self.voltszerof = 100.0
        self.voltsfullf = 999.
        self.voltsrdiv = 20

        # LinuxCNC assorted defaults and options
        self.toolchangeprompt = True
        self.multimpg = False
        self.require_homing = True
        self.individual_homing = False
        self.restore_joint_position = False
        self.random_toolchanger = False
        self.raise_z_on_toolchange = False
        self.allow_spindle_on_toolchange = False
        self.customhal = False
        self.usebldc = False

        # These components can be used by pncconf so we must keep track of them.
        # in case the user needs some for a custom HAL file
        self.userneededpid = 0
        self.userneededabs = 0
        self.userneededscale = 0
        self.userneededmux16 = 0
        self.userneededlowpass = 0
        self.userneededbldc = 0

        # pyvcp data
        self.pyvcp = 0 # not included
        self.pyvcpname = "custom.xml"
        self.pyvcpexist = False
        self.pyvcp1 = False
        self.pyvcpblank = True
        self.pyvcphaltype = 0 # no HAL connections specified
        self.pyvcpconnect = 1 # HAL connections allowed
        self.pyvcpwidth = 200
        self.pyvcpheight = 200
        self.pyvcpxpos = 0
        self.pyvcpypos = 0
        self.pyvcpposition = False
        self.pyvcpsize = False

        # gladevcp data
        self.gladevcp = False # not included
        self.gladesample = True
        self.gladeexists = False
        self.spindlespeedbar = True
        self.spindleatspeed = True
        self.maxspeeddisplay = 1000
        self.zerox = False
        self.zeroy = False
        self.zeroz = False
        self.zeroa = False
        self.autotouchz = False
        self.gladevcphaluicmds = 0
        self.centerembededgvcp = True
        self.sideembededgvcp = False
        self.standalonegvcp = False
        self.gladevcpposition = False
        self.gladevcpsize = False
        self.gladevcpforcemax = False
        self.gladevcpwidth = 200
        self.gladevcpheight = 200
        self.gladevcpxpos = 0
        self.gladevcpypos = 0
        self.gladevcptheme = "Follow System Theme"

        # classicladder data
        self.classicladder = 0 # not included
        self.digitsin = 15 # default number of pins
        self.digitsout = 15
        self.s32in = 10
        self.s32out = 10
        self.floatsin = 10
        self.floatsout = 10
        self.bitmem = 50
        self.wordmem = 50
        self.tempexists = 0 # not present ( a blank CL program edited through pncconf)
        self.laddername = "custom.clp"
        self.modbus = 0 # not included
        self.ladderhaltype = 0 # no HAL connections specified
        self.ladderconnect = 1 # HAL connections allowed
        self.ladderexist = False
        self.cl_haluicmds = 0
        self.laddertouchz = False

        # stepper timing data
        self.drivertype = "other"
        self.steptime = 5000
        self.stepspace = 5000
        self.dirhold = 20000
        self.dirsetup = 20000
        self.latency = 15000
        self.period = 25000

        # For parallel port
        self.pp1_direction = 1 # output
        self.ioaddr1 = "0"
        self.ioaddr2 = "1"
        self.pp2_direction = 0 # input
        self.ioaddr3 = "2"
        self.pp3_direction = 0 # input
        self.number_pports = 0

        for connector in("pp1","pp2","pp3"):
            # initialize parport input / inv pins
            for i in (2,3,4,5,6,7,8,9,10,11,12,13,15):
                pinname ="%s_Ipin%d"% (connector,i)
                self[pinname] = _PD.UNUSED_INPUT
                pinname ="%s_Ipin%d_inv"% (connector,i)
                self[pinname] = False
            # initialize parport output / inv pins
            for i in (1,2,3,4,5,6,7,8,9,14,16,17):
                pinname ="%s_Opin%d"% (connector,i)
                self[pinname] = _PD.UNUSED_OUTPUT
                pinname ="%s_Opin%d_inv"% (connector,i)
                self[pinname] = False

        self.number_mesa = 1 # number of cards
        # for first mesa card
        self.mesa0_currentfirmwaredata = None
        self.mesa0_boardtitle = "5i25-Internal Data"
        self.mesa0_firmware = _PD.MESA_INTERNAL_FIRMWAREDATA[0][2]
        self.mesa0_parportaddrs = "0x378"
        self.mesa0_card_addrs = "192.168.1.121"
        self.mesa0_isawatchdog = 1
        self.mesa0_pwm_frequency = 20000
        self.mesa0_pdm_frequency = 6000000
        self.mesa0_3pwm_frequency = 20000
        self.mesa0_watchdog_timeout = 5000000
        self.mesa0_numof_encodergens = 1
        self.mesa0_numof_resolvers = 0
        self.mesa0_numof_pwmgens = 0
        self.mesa0_numof_tppwmgens = 0
        self.mesa0_numof_stepgens = 5
        self.mesa0_numof_sserialports = 1
        self.mesa0_numof_sserialchannels = 2

        # second mesa card
        self.mesa1_currentfirmwaredata = _PD.MESA_INTERNAL_FIRMWAREDATA[1]
        self.mesa1_boardtitle = "5i25-Internal Data"
        self.mesa1_firmware = _PD.MESA_INTERNAL_FIRMWAREDATA[0][2]
        self.mesa1_parportaddrs = "0x378"
        self.mesa1_card_addrs = "192.168.1.121"
        self.mesa1_isawatchdog = 1
        self.mesa1_pwm_frequency = 20000
        self.mesa1_pdm_frequency = 6000000
        self.mesa1_3pwm_frequency = 20000
        self.mesa1_watchdog_timeout = 5000000
        self.mesa1_numof_encodergens = 1
        self.mesa1_numof_resolvers = 0
        self.mesa1_numof_pwmgens = 0
        self.mesa1_numof_tppwmgens = 0
        self.mesa1_numof_stepgens = 5
        self.mesa1_numof_sserialports = 1
        self.mesa1_numof_sserialchannels = 2

        for boardnum in(0,1):
            connector = 2
            pinname ="mesa%dc%dpin"% (boardnum,connector)
            self[pinname+"0"] = _PD.UNUSED_ENCODER
            self[pinname+"0type"] = _PD.ENCB
            self[pinname+"1"] = _PD.UNUSED_ENCODER
            self[pinname+"1type"] = _PD.ENCA
            self[pinname+"2"] = _PD.UNUSED_ENCODER
            self[pinname+"2type"] = _PD.ENCB
            self[pinname+"3"] = _PD.UNUSED_ENCODER
            self[pinname+"3type"] = _PD.ENCA
            self[pinname+"4"] = _PD.UNUSED_ENCODER
            self[pinname+"4type"] = _PD.ENCI
            self[pinname+"5"] = _PD.UNUSED_ENCODER
            self[pinname+"5type"] = _PD.ENCI
            self[pinname+"6"] = _PD.UNUSED_PWM
            self[pinname+"6type"] = _PD.PWMP
            self[pinname+"7"] = _PD.UNUSED_PWM
            self[pinname+"7type"] = _PD.PWMP
            self[pinname+"8"] = _PD.UNUSED_PWM
            self[pinname+"8type"] = _PD.PWMD
            self[pinname+"9"] = _PD.UNUSED_PWM
            self[pinname+"9type"] = _PD.PWMD
            self[pinname+"10"] = _PD.UNUSED_PWM
            self[pinname+"10type"] = _PD.PWME
            self[pinname+"11"] = _PD.UNUSED_PWM
            self[pinname+"11type"] = _PD.PWME
            self[pinname+"12"] = _PD.UNUSED_ENCODER
            self[pinname+"12type"] = _PD.ENCB
            self[pinname+"13"] = _PD.UNUSED_ENCODER
            self[pinname+"13type"] = _PD.ENCA
            self[pinname+"14"] = _PD.UNUSED_ENCODER
            self[pinname+"14type"] = _PD.ENCB
            self[pinname+"15"] = _PD.UNUSED_ENCODER
            self[pinname+"15type"] = _PD.ENCA
            self[pinname+"16"] = _PD.UNUSED_ENCODER
            self[pinname+"16type"] = _PD.ENCI
            self[pinname+"17"] = _PD.UNUSED_ENCODER
            self[pinname+"17type"] = _PD.ENCI
            self[pinname+"18"] = _PD.UNUSED_PWM
            self[pinname+"18type"] = _PD.PWMP
            self[pinname+"19"] = _PD.UNUSED_PWM
            self[pinname+"19type"] = _PD.PWMP
            self[pinname+"20"] = _PD.UNUSED_PWM
            self[pinname+"20type"] = _PD.PWMD
            self[pinname+"21"] = _PD.UNUSED_PWM
            self[pinname+"21type"] = _PD.PWMD
            self[pinname+"22"] = _PD.UNUSED_PWM
            self[pinname+"22type"] = _PD.PWME
            self[pinname+"23"] = _PD.UNUSED_PWM
            self[pinname+"23type"] = _PD.PWME
        for boardnum in(0,1):
            for connector in(1,3,4,5,6,7,8,9):
                # This initializes GPIO input pins
                for i in range(0,16):
                    pinname ="mesa%dc%dpin%d"% (boardnum,connector,i)
                    self[pinname] = _PD.UNUSED_INPUT
                    pinname ="mesa%dc%dpin%dtype"% (boardnum,connector,i)
                    self[pinname] = _PD.GPIOI
                # This initializes GPIO output pins
                for i in range(16,24):
                    pinname ="mesa%dc%dpin%d"% (boardnum,connector,i)
                    self[pinname] = _PD.UNUSED_OUTPUT
                    pinname ="mesa%dc%dpin%dtype"% (boardnum,connector,i)
                    self[pinname] = _PD.GPIOO
            for connector in(1,2,3,4,5,6,7,8,9):
                # This initializes the mesa inverse pins
                for i in range(0,24):
                    pinname ="mesa%dc%dpin%dinv"% (boardnum,connector,i)
                    self[pinname] = False
            port = 0 #TODO up to 4 ports
            for channel in range(0,_PD._NUM_CHANNELS):
                self["mesa%dsserial%d_%dsubboard"% (boardnum, port,channel)] = "none"
                # This initializes pins
                for i in range(0,_PD._SSCOMBOLEN):
                    pinname ="mesa%dsserial%d_%dpin%d"% (boardnum, port,channel,i)
                    if i < 24:
                        self[pinname] = _PD.UNUSED_INPUT
                    else:
                        self[pinname] = _PD.UNUSED_OUTPUT
                    pinname ="mesa%dsserial%d_%dpin%dtype"% (boardnum, port,channel,i)
                    if i < 24:
                        self[pinname] = _PD.GPIOI
                    else:
                        self[pinname] = _PD.GPIOO
                    pinname ="mesa%dsserial%d_%dpin%dinv"% (boardnum, port,channel,i)
                    self[pinname] = False

        # halui data
        self.halui = 0 # not included
        # Command list
        for i in range(0,15):
                pinname ="halui_cmd%s"% i
                self[pinname] = ""

        #HAL component command list
        self.loadcompservo = []
        self.addcompservo = []
        self.loadcompbase = []
        self.addcompbase = []

        self.xhomesequence = 1
        self.yhomesequence = 2
        self.zhomesequence = 0
        self.ahomesequence = 3

        # common axis data
        for temp in("x","y","z","a","s"):

            self[temp+"drivertype"]= "custom"
            self[temp+"steprev"]= 200
            self[temp+"microstep"]= 5
            self[temp+"motor_pulleydriver"]= 1
            self[temp+"motor_pulleydriven"]= 1
            self[temp+"cbmicrosteps"]= False
            self[temp+"cbmotor_pulley"]= False
            if not temp == 's':
                self[temp+"motor_wormdriver"]= 1
                self[temp+"motor_wormdriven"]= 1
                self[temp+"encoder_pulleydriver"]= 1
                self[temp+"encoder_pulleydriven"]= 1
                self[temp+"encoder_wormdriver"]= 1
                self[temp+"encoder_wormdriven"]= 1
                self[temp+"motor_leadscrew"]= 5
                self[temp+"encoder_leadscrew"]= 5
                self[temp+"motor_leadscrew_tpi"]= 5
                self[temp+"encoder_leadscrew_tpi"]= 5
                self[temp+"cbencoder_pitch"]= False
                self[temp+"cbencoder_tpi"]= False
                self[temp+"cbencoder_worm"]= False
                self[temp+"cbencoder_pulley"]= False
                self[temp+"cbmotor_pitch"]= False
                self[temp+"cbmotor_tpi"]= False
                self[temp+"cbmotor_worm"]= False
                self[temp+"outputscale"]= 10
                self[temp+"outputminlimit"]= -10
                self[temp+"outputmaxlimit"]= 10
            else:
                self[temp+"motor_gear1driver"]= 1
                self[temp+"motor_gear1driven"]= 1
                self[temp+"motor_gear2driver"]= 1
                self[temp+"motor_gear2driven"]= 1
                self[temp+"motor_max"] = 2000
                self[temp+"cbmotor_gear1"]= False
                self[temp+"cbmotor_gear2"]= False
                self[temp+"negative_rot"]= False
                self[temp+"rbvoltage_5"]= False
                self[temp+"outputscale2"]= 10
                self[temp+"outputmaxvoltage"]= 10
            self[temp+"encodercounts"]= 4000
            self[temp+"usecomp"]= 0
            self[temp+"compfilename"]= temp+"compensation"
            self[temp+"comptype"]= 0
            self[temp+"usebacklash"]= 0
            self[temp+"backlash"]= 0
            self[temp+"invertmotor"]= 0
            self[temp+"invertencoder"]= 0
            self[temp+"3pwmscale"]= 1
            self[temp+"3pwmdeadtime"]= 500
            self[temp+"maxoutput"]= 0
            self[temp+"P"]= None
            self[temp+"I"]= 0
            self[temp+"D"]= 0
            self[temp+"FF0"]= 0
            self[temp+"FF1"]= 1
            self[temp+"FF2"]= 0
            self[temp+"bias"]= 0
            self[temp+"deadband"]= 0
            self[temp+"steptime"]= 5000
            self[temp+"stepspace"]= 5000
            self[temp+"dirhold"]= 10000
            self[temp+"dirsetup"]= 10000
            self[temp+"homepos"]= 0
            self[temp+"homesw"]=  0
            self[temp+"hometandemsw"]=  0
            self[temp+"homefinalvel"]= 0
            self[temp+"latchdir"]= 0
            self[temp+"searchdir"]= 0
            self[temp+"usehomeindex"]= 0
            self[temp+"stepscale"]= 0
            self[temp+"encoderscale"]= 0
            self[temp+"bldc_option"]= False
            self[temp+"bldc_config"]= ""
            self[temp+"bldc_no_feedback"]= False
            self[temp+"bldc_absolute_feedback"]= False
            self[temp+"bldc_incremental_feedback"]= True
            self[temp+"bldc_use_hall"]= True
            self[temp+"bldc_use_encoder"]= False
            self[temp+"bldc_fanuc_alignment"]= False
            self[temp+"bldc_use_index"]= False
            self[temp+"bldc_digital_output"]= False
            self[temp+"bldc_six_outputs"]= False
            self[temp+"bldc_force_trapz"]= False
            self[temp+"bldc_emulated_feedback"]= False
            self[temp+"bldc_output_hall"]= False
            self[temp+"bldc_output_fanuc"]= False
            self[temp+"bldc_scale"]= 512
            self[temp+"bldc_poles"]= 4
            self[temp+"bldc_lead_angle"]= 90
            self[temp+"bldc_inital_value"]= 0.2
            self[temp+"bldc_encoder_offset"]= 0
            self[temp+"bldc_reverse"]= False
            self[temp+"bldc_drive_offset"]= 0.0
            self[temp+"bldc_pattern_out"]= 25
            self[temp+"bldc_pattern_in"]= 25
            self[temp+"8i20maxcurrent"] = 5.0

        # set xyz axes defaults depending on units true = imperial
        self.set_axis_unit_defaults(True)

        # rotary tables need bigger limits
        self.aminlim = -9999
        self.amaxlim =  9999
        self.amaxvel = 360
        self.amaxacc = 1200
        self.ahomevel = 0
        self.ahomesearchvel= 2
        self.ahomelatchvel= .5
        self.aminferror= .05
        self.amaxferror= .5
        # adjust spindle settings
        self.soutputscale = 2000
        self.smaxoutput = 2000
        self.sP = 0
        self.sFF1= 0
        self.sFF0= 1
        self.smaxvel = 2000
        self.smaxacc = 300
        self.snearrange = 200.0
        self.snearscale = 1.20
        self.sfiltergain = 1.0
        self.suseatspeed = False
        self.suseoutputrange2 = False
        self.scaleselect = False
        self.susenegativevoltage = True
        self.susenearrange = False
        self.ssingleinputencoder = False
        self.mitsub_vfd = False
        self.mitsub_vfd_port = '/dev/ttyS0'
        self.mitsub_vfd_id = (('spindle_vfd',1))
        self.mitsub_vfd_baud = 9600
        self.mitsub_vfd_command_scale = 1
        self.mitsub_vfd_feedback_scale = 1
        self.gs2_vfd = True
        self.gs2_vfd_port = '/dev/ttyS0'
        self.gs2_vfd_slave = 1
        self.gs2_vfd_baud = 9600
        self.gs2_vfd_accel = 10
        self.gs2_vfd_deaccel = 0

    # change the XYZ axis defaults to metric or imperial
    # This only sets data that makes sense to change eg gear ratio don't change
    def set_axis_unit_defaults(self, units=True):
        if units: # imperial
            # set GUI defaults
            self.max_linear_velocity = 1 # 60 inches per min
            self.default_linear_velocity = .25 # 15 inches per min
            self.min_linear_velocity = .01667

            # axes defaults
            for i in ('x','y','z'):
                self[i+'maxvel'] = 1
                self[i+'maxacc'] = 30
                self[i+'homevel'] = .05
                self[i+"homesearchvel"]= .05
                self[i+"homelatchvel"]= .025
                self[i+"minferror"]= .05
                self[i+"maxferror"]= .5
                if not i == 'z':
                    self[i+'minlim'] = 0
                    self[i+'maxlim'] = 8
                else:
                    self.zminlim = -4
                    self.zmaxlim = 0
        else: # metric
            # set gui defaults
            self.max_linear_velocity = 25 # 1500 mm per min
            self.default_linear_velocity = 6 # 380 mm per min
            self.min_linear_velocity = .5

            # axes defaults
            for i in ('x','y','z'):
                self[i+'maxvel'] = 25
                self[i+'maxacc'] = 750
                self[i+'homevel'] = 1.5
                self[i+"homesearchvel"]= 1
                self[i+"homelatchvel"]= .5
                self[i+"minferror"]= 1
                self[i+"maxferror"]= 10
                if not i =='z':
                    self[i+'minlim'] = 0
                    self[i+'maxlim'] = 200
                else:
                    self.zminlim = -100
                    self.zmaxlim = 0

    def load(self, filename, app=None, force=False):
        self.pncconf_loaded_version = 0.0
        def str2bool(s):
            return s == 'True'

        converters = {'string': str, 'float': float, 'int': int, 'bool': str2bool, 'eval': eval}

        d = xml.dom.minidom.parse(open(filename, "r"))
        for n in d.getElementsByTagName("property"):
            name = n.getAttribute("name")
            conv = converters[n.getAttribute('type')]
            text = n.getAttribute('value')
            setattr(self, name, conv(text))

        # this loads custom signal names created by the user
        # adds endings to the custom signal name when put in
        # hal signal name arrays
        #encoders
        temp =[]; i = False
        for i in  self.halencoderinputsignames:
            temp.append((i,i+"-a"))
            for j in(["-a","-b","-i","-m"]):
                _PD.hal_encoder_input_names.append(i+j)
        end = len(_PD.human_encoder_input_names)-1
        if i:  _PD.human_encoder_input_names[end][1]= temp
        #resolvers
        temp = []; i = False
        for i in  self.halresolversignames:
            temp.append(i)
            _PD.hal_resolver_input_names.append(i)
        end = len(_PD.human_resolver_input_names)-1
        if i: _PD.human_resolver_input_names[end][1]= temp
        # 8I20 amplifier card
        temp = []; i = False
        for i in  self.hal8i20signames:
            temp.append(i)
            _PD.hal_8i20_input_names.append(i)
        end = len(_PD.human_8i20_input_names)-1
        if i: _PD.human_8i20_input_names[end][1]= temp
        # potentiometer
        temp = []; i = False
        for i in  self.halpotsignames:
            temp.append(i)
            for j in(["-output","-enable"]):
                _PD.hal_pot_output_names.append(i+j)
        end = len(_PD.human_pot_output_names)-1
        if i: _PD.human_pot_output_names[end][1]= temp
        # analog input
        temp = []; i = False
        for i in  self.halanaloginsignames:
            temp.append(i)
            _PD.hal_analog_input_names.append(i)
        end = len(_PD.human_analog_input_names)-1
        if i: _PD.human_analog_input_names[end][1]= temp
        #pwm
        temp =[]; i = False
        for i in  self.halpwmoutputsignames:
            temp.append((i,i+"-pulse"))
            for j in(["-pulse","-dir","-enable"]):
                _PD.hal_pwm_output_names.append(i+j)
        end = len(_PD.human_pwm_output_names)-1
        if i: _PD.human_pwm_output_names[end][1]= temp
        # tppwm
        temp =[]; i = False
        for i in  self.haltppwmoutputsignames:
            temp.append(i)
            for j in(["-a","-b","-c","-anot","-bnot","-cnot","-fault","-enable"]):
                _PD.hal_tppwm_output_names.append(i+j)
        end = len(_PD.human_tppwm_output_names)-1
        if i:  _PD.human_tppwm_output_names[end][1]= temp
        # GPIO Input
        temp = []; i = False
        for i in  self.halinputsignames:
            temp.append((i, i))
            _PD.hal_input_names.append(i)
        end = len(_PD.human_input_names)-1
        if i: _PD.human_input_names[end][1]= temp
        # GPIO Output
        temp = []; i = False
        for i in  self.haloutputsignames:
            temp.append(i)
            _PD.hal_output_names.append(i)
        end = len(_PD.human_output_names)-1
        if i: _PD.human_output_names[end][1]= temp
        # steppers
        temp = []; i = False
        for i in  self.halsteppersignames:
            temp.append((i,i+"-step"))
            for j in(["-step","-dir","-c","-d","-e","-f"]):
                _PD.hal_stepper_names.append(i+j)
        end = len(_PD.human_stepper_names)-1
        if i: _PD.human_stepper_names[end][1]= temp

        warnings = []
        warnings2 = []
        if self.pncconf_loaded_version < self._pncconf_version:
            warnings.append(_("This configuration was saved with an earlier version of pncconf which may be incompatible.\n\
If it doesn't plainly cause an error, you still may want to save it with another name and check it. Safer to start from scratch.\n\
If you have a REALLY large config that you wish to convert to this newer version of PNConf - ask on the LinuxCNC forum - it may be possible..") )
        for f, m in self.md5sums:
            m1 = md5sum(f)
            if m1 and m != m1:
                warnings2.append(_("File %r was modified since it was written by PNCconf") % f)
        if not warnings and not warnings2: return
        if warnings2:
            warnings.append("")
            warnings.append(_("Saving this configuration file will discard configuration changes made outside PNCconf."))
        if warnings:
            warnings = warnings + warnings2
        self.pncconf_loaded_version = self._pncconf_version
        if app:
            dialog = gtk.MessageDialog(app.widgets.window1,
                gtk.DIALOG_MODAL | gtk.DialogFlags.DESTROY_WITH_PARENT,
                gtk.MESSAGE_WARNING, gtk.ButtonsType.OK,
                     "\n".join(warnings))
            dialog.show_all()
            dialog.run()
            dialog.destroy()
        else:
            for para in warnings:
                for line in textwrap.wrap(para, 78): print(line)
                print()
            print()
            if force: return
            response = input(_("Continue? "))
            if response[0] not in _("yY"): raise SystemExit(1)

    def add_md5sum(self, filename, mode="r"):
        self.md5sums.append((filename, md5sum(filename)))

    def save(self,basedir):
        base = basedir
        ncfiles = os.path.expanduser("~/linuxcnc/nc_files")
        if not os.path.exists(ncfiles):
            _APP.makedirs(ncfiles)
            examples = os.path.join(_BASE, "share", "linuxcnc", "ncfiles")
            if not os.path.exists(examples):
                examples = os.path.join(_BASE, "nc_files")
            if os.path.exists(examples):
                os.symlink(examples, os.path.join(ncfiles, "examples"))
        _APP.makedirs(base)
        _APP.makedirs(base+"/backups")

        self.md5sums = []

        filename = os.path.join(base, "tool.tbl")
        file = open(filename, "w")
        print("T0 P0 ;", file=file)
        print("T1 P1 ;", file=file)
        print("T2 P2 ;", file=file)
        print("T3 P3 ;", file=file)
        file.close()

        filename = "%s.pncconf" % base

        d = xml.dom.minidom.getDOMImplementation().createDocument(
                            None, "pncconf", None)
        e = d.documentElement

        for k, v in sorted(self.__dict__.items()):
            if k.startswith("_"): continue
            n = d.createElement('property')
            e.appendChild(n)

            if isinstance(v, float): n.setAttribute('type', 'float')
            elif isinstance(v, bool): n.setAttribute('type', 'bool')
            elif isinstance(v, int): n.setAttribute('type', 'int')
            elif isinstance(v, list): n.setAttribute('type', 'eval')
            else: n.setAttribute('type', 'string')

            n.setAttribute('name', k)
            n.setAttribute('value', str(v))

        d.writexml(open(filename, "wt"), addindent="  ", newl="\n")
        print("%s" % base)

        # write pncconf hidden preference file
        filename = os.path.expanduser("~/.pncconf-preferences")
        print(filename)
        d2 = xml.dom.minidom.getDOMImplementation().createDocument(
                            None, "int-pncconf", None)
        e2 = d2.documentElement

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'float')
        n2.setAttribute('name', "version")
        n2.setAttribute('value', str("%f"%self._preference_version))

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'bool')
        n2.setAttribute('name', "always_shortcut")
        n2.setAttribute('value', str("%s"% self.createshortcut))

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'bool')
        n2.setAttribute('name', "always_link")
        n2.setAttribute('value', str("%s"% self.createsymlink))

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'bool')
        n2.setAttribute('name', "use_ini_substitution")
        n2.setAttribute('value', str("%s"% self.useinisubstitution))

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'bool')
        n2.setAttribute('name', "show_advanced_pages")
        n2.setAttribute('value', str("%s"% self.advanced_option))

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'bool')
        n2.setAttribute('name', "chooselastconfig")
        n2.setAttribute('value', str("%s"% self._chooselastconfig))

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'string')
        n2.setAttribute('name', "machinename")
        n2.setAttribute('value', str("%s"%self.machinename))

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'eval')
        n2.setAttribute('name', "MESABLACKLIST")
        n2.setAttribute('value', str(_PD.MESABLACKLIST))

        n2 = d2.createElement('property')
        e2.appendChild(n2)
        n2.setAttribute('type', 'string')
        n2.setAttribute('name', "customfirmwarefilename")
        n2.setAttribute('value', str("%s"% self._customfirmwarefilename))

        d2.writexml(open(filename, "wt"), addindent="  ", newl="\n")

        # write to Touchy preference file directly
        if self.frontend == _PD._TOUCHY:
            #print("Setting TOUCHY preferences")
            templist = {"touchyabscolor":"abs_textcolor","touchyrelcolor":"rel_textcolor",
                        "touchydtgcolor":"dtg_textcolor","touchyerrcolor":"err_textcolor"}
            for key,value in templist.items():
                prefs.putpref(value, self[key], str)
            if self.touchyposition[0] or self.touchysize[0]:
                    pos = size = ""
                    if self.touchyposition[0]:
                        pos = "+%d+%d"% (self.touchyposition[1],self.touchyposition[2])
                    if self.touchysize[0]:
                        size = "%dx%d"% (self.touchysize[1],self.touchysize[2])
                    geo = "%s%s"%(size,pos)
            else: geo = "default"
            prefs.putpref('window_geometry',geo, str)
            prefs.putpref('gtk_theme',self.touchytheme, str)
            prefs.putpref('window_force_max', self.touchyforcemax, bool)

        # write AXIS rc file for geometry
        if self.frontend == _PD._AXIS and (self.axisposition[0] or self.axissize[0] or self.axisforcemax):
            filename = os.path.expanduser("~/.axisrc")
            if _APP.warning_dialog(_PD.MESS_REPLACE_RC_FILE, False):
                f1 = open(filename, "w")
                if self.axisposition[0] or self.axissize[0]:
                    #print("Setting AXIS geometry option)
                    pos = size = ""
                    if self.axisposition[0]:
                        pos = "+%d+%d"% (self.axisposition[1],self.axisposition[2])
                    if self.axissize[0]:
                        size = "%dx%d"% (self.axissize[1],self.axissize[2])
                    geo = "%s%s"%(size,pos)
                    print("""root_window.tk.call("wm","geometry",".","%s")"""%(geo), file=f1)
                if self.axisforcemax:
                    #print("Setting AXIS forcemax option")
                    print("""# Find the largest size possible and set AXIS to it""", file=f1)
                    print("""maxgeo=root_window.tk.call("wm","maxsize",".")""", file=f1)
                    print("""try:""", file=f1)
                    print("""   fullsize=maxgeo.split(' ')[0] + 'x' + maxgeo.split(' ')[1]""", file=f1)
                    print("""except:""", file=f1)
                    print("""   fullsize=str(maxgeo[0]) + 'x' + str(maxgeo[1])""", file=f1)
                    print("""root_window.tk.call("wm","geometry",".",fullsize)""", file=f1)
                    print("""# Uncomment for fullscreen""", file=f1)
                    print("""#root_window.attributes('-fullscreen', True)""", file=f1)

        # make system link and shortcut to pncconf files
        # see http://freedesktop.org/wiki/Software/xdg-user-dirs
        desktop = subprocess.getoutput("""
            test -f ${XDG_CONFIG_HOME:-~/.config}/user-dirs.dirs && . ${XDG_CONFIG_HOME:-~/.config}/user-dirs.dirs
            echo ${XDG_DESKTOP_DIR:-$HOME/Desktop}""")
        if self.createsymlink:
            shortcut = os.path.join(desktop, self.machinename)
            if os.path.exists(desktop) and not os.path.exists(shortcut):
                os.symlink(base,shortcut)

        if self.createshortcut and os.path.exists(desktop):
            if os.path.exists(_BASE + "/scripts/linuxcnc"):
                scriptspath = (_BASE + "/scripts/linuxcnc")
            else:
                scriptspath ="linuxcnc"

            filename = os.path.join(desktop, "%s.desktop" % self.machinename)
            file = open(filename, "w")
            print("[Desktop Entry]", file=file)
            print("Version=1.0", file=file)
            print("Terminal=false", file=file)
            print("Name=" + _("launch %s") % self.machinename, file=file)
            print("Exec=%s %s/%s.ini" \
                         % ( scriptspath, base, self.machinename ), file=file)
            print("Type=Application", file=file)
            print("Comment=" + _("Desktop Launcher for LinuxCNC config made by PNCconf"), file=file)
            print("Icon=%s"% _PD.LINUXCNCICON, file=file)
            file.close()
            # Ubuntu 10.04 require launcher to have execute permissions
            os.chmod(filename,0o775)

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

