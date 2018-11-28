#!/usr/bin/env python
# -*- encoding: utf-8 -*-
#
#    This is pncconf, a graphical configuration editor for LinuxCNC
#    Chris Morley copyright 2009
#    pncconf 1.1 revamped by Chris Morley 2014
#    This is based from stepconf, a graphical configuration editor for linuxcnc
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
#
#

import os
import linuxcnc

class Private_Data:
    def __init__(self,app,bin_dir,base_dir):
        BIN = bin_dir
        BASE = base_dir
        S = self
        # This holds page information:
        # [widget name,title.initialized state,active state]
        # The widget name is also the filename 'widget.glade'
        # if not initialized, the page will be loaded and added,
        # only if page is shown (and signals selected in glade editor will be ignored)
        self.available_page =[['intro', _('PNCconf'),True,True],
                                ['start',_('Start'),True,True],
                                ['base',_('Base Information'),True,True],
                                ['screen',_('Screen'),True,True],
                                ['vcp',_('VCP'),True,True],
                                ['external',_('External Controls'),True,True],
                                ['mesa0',_('Mesa Card 0'),False,True],
                                ['mesa1',_('Mesa Card 1'),False,True],
                                ['pport1',_('Parallel Port'),False,True],
                                ['pport2',_('Parallel Port 2'),False,True],
                                ['x_motor',_('X Motor'),True,True],
                                ['x_axis',_('X Axis'),True,True],
                                ['y_motor',_('Y Motor'),True,True],
                                ['y_axis',_('Y Axis'),True,True],
                                ['z_motor',_('Z Motor'),True,True],
                                ['z_axis',_('Z Axis'),True,True],
                                ['a_motor',_('A Motor'),True,True],
                                ['a_axis',_('A Axis'),True,True],
                                ['s_motor',_('Spindle Motor'),True,True],
                                ['options',_('Options'),True,True],
                                ['realtime',_('Realtime'),True,True],
                                ['finished',_('Almost Done'),True,True]
                            ]
        self.EXTRA_MESA_FIRMWAREDATA = []
        self.prepare_block = False
        self._AXIS = 1
        self._TKLINUXCNC = 2
        self._GMOCCAPY = 3
        self._TOUCHY = 4

        self._SSCOMBOLEN = 60
        self._IMPERIAL = 0
        self._METRIC = 1

        self.DATADIR = linuxcnc.SHARE + "/linuxcnc/pncconf"
        self.WIZARD = os.path.join(self.DATADIR, "linuxcnc-wizard.gif")
        if not os.path.isfile(self.WIZARD):
            self.WIZARD = os.path.join("/etc/linuxcnc/linuxcnc-wizard.gif")
        if not os.path.isfile(self.WIZARD):
            self.WIZARD = os.path.join("/usr/share/linuxcnc/linuxcnc-wizard.gif")
        if not os.path.isfile(self.WIZARD):
            wizdir = os.path.join(os.path.abspath(BIN), "..")
            self.WIZARD = os.path.join(wizdir, "linuxcnc-wizard.gif")

        self.ICONDIR = os.path.join(os.path.abspath(BIN), "..")
        self.LINUXCNCICON = os.path.join(self.ICONDIR, "linuxcncicon.png")
        if not os.path.isfile(self.LINUXCNCICON):
            self.LINUXCNCICON = os.path.join("/etc/linuxcnc/linuxcnc-wizard.gif")
        if not os.path.isfile(self.LINUXCNCICON):
            self.LINUXCNCICON = os.path.join("/usr/share/linuxcnc/linuxcncicon.png")

        self.DISTDIR = os.path.join(os.path.abspath(BIN), "..", "configs", "common")
        if not os.path.isdir(self.DISTDIR):
            self.DISTDIR = os.path.join(os.path.abspath(BIN), "..", "share", "doc", "linuxcnc", "sample-configs", "common")
        if not os.path.isdir(self.DISTDIR):
            self.DISTDIR = os.path.join(os.path.abspath(BIN), "..", "linuxcnc", "sample-configs", "common")
        if not os.path.isdir(self.DISTDIR):
            self.DISTDIR = "/usr/share/doc/linuxcnc/examples/sample-configs/common"
        self.HELPDIR = os.path.join(BASE, "share", "linuxcnc", "pncconf", "pncconf-help")
        if not os.path.exists(self.HELPDIR):
            self.HELPDIR = os.path.join(BASE, "src", "emc", "usr_intf", "pncconf", "pncconf-help")
        self.FIRMDIR = "/lib/firmware/hm2/"
        self.THEMEDIR = "/usr/share/themes"
        self.MESABLACKLIST = ["5i22","7i43","4i65","4i68","SVST8_3P.xml"]

        #****************************************
        # mesa component combo box/firmware text
        #****************************************
        (   self.NUSED,self.DUMMY ) = self.pintype_notused = [ _("Not Used"),_("Dummy") ]
        (   self.AMP8I20,self.DUMMY2 ) = self.pintype_8i20 = [ _("8i20 Servo Drive"),_("Dummy") ]
        (   self.POTO,self.POTE,self.POTD ) = self.pintype_potentiometer = [
             _("POT Output"),_("POT Enable"),_("POT Dir") ]
        (   self.GPIOI,self.GPIOO,self.GPIOD) = self.pintype_gpio = [
             _("GPIO Input"),_("GPIO Output"),_("GPIO O Drain") ]
        (   self.ENCA,self.ENCB,self.ENCI,self.ENCM 
        ) = self.pintype_encoder = [
            _("Quad Enc-A"),_("Quad Enc-B"),_("Quad Enc-I"),_("Quad Enc-M") ]
        ( self. MXE0,self.MXE1,self.MXEI,self.MXEM,self.MXES 
        ) = self.pintype_muxencoder = [
            _("Muxed Enc 0"),_("Muxed Enc 1"),_("muxed Enc I"),_("Muxed Enc M"),_("mux select") ]
        (   self.RES0,self.RES1,self.RES2,self.RES3,self.RES4,self.RES5,self.RESU
        ) = self.pintype_resolver = [
            _("Resolver 0 Encoder"),_("Resolver 1 Encoder"),_("Resolver 2 Encoder"),
            _("Resolver 3 Encoder"),_("Resolver 4 Encoder"),_("Resolver 5 Encoder"),"resolver" ]

        (   self.STEPA,self.STEPB,self.STEPC,self.STEPD,self.STEPE,self.STEPF
        ) = self.pintype_stepper = [
            _("Step Gen-A"),_("Dir Gen-B"),_("Step/Dir Gen-C"), _("Step/Dir Gen-D"),
            _("Step/Dir Gen-E"),_("Step/dir Gen-F") ]

        (   self.PWMP,self.PWMD,self.PWME ) = self.pintype_pwm = [ _("PWM Gen-P"),_("PWM Gen-D"),_("PWM Gen-E") ]
        (   self.PDMP,self.PDMD,self.PDME ) = self.pintype_pdm = [ _("PDM Gen-P"),_("PDM Gen-D"),_("PDM Gen-E") ]
        (   self.UDMU,self.UDMD,self.UDME ) = self.pintype_udm = [ _("UDM -Up"),_("UDM-Down"),_("UDM-E") ]

        (   self.TPPWMA,self.TPPWMB,self.TPPWMC,self.TPPWMAN,self.TPPWMBN,self.TPPWMCN,self.TPPWME,self.TPPWMF
        ) = self.pintype_tp_pwm = [
            _("Motor Phase A"),_("Motor Phase B"),_("Motor Phase C"),
            _("Motor Phase A Not"),_("Motor Phase B Not") ,_("Motor Phase C Not"),_("Motor Enable"),_("Motor Fault") ]

        (   self.TXDATA0,self.RXDATA0,self.TXEN0,
            self.TXDATA1,self.RXDATA1,self.TXEN1,
            self.TXDATA2,self.RXDATA2,self.TXEN2,
            self.TXDATA3,self.RXDATA3,self.TXEN3,
            self.TXDATA4,self.RXDATA4,self.TXEN4,
            self.TXDATA5,self.RXDATA5,self.TXEN5,
            self.TXDATA6,self.RXDATA6,self.TXEN6,
            self.TXDATA7,self.RXDATA7,self.TXEN7,
            self.SS7I76M0,self.SS7I76M2,self.SS7I76M3,
            self.SS7I77M0,self.SS7I77M1,self.SS7I77M3,self.SS7I77M4
        ) = self.pintype_sserial = [ _("SSERIAL-P0-TX"),_("SSERIAL-P0-RX"),_("SSERIAL-P0-EN"),
            _("SSERIAL-P1-TX"),_("SSERIAL-P1-RX"),_("SSERIAL-P1-EN"),
            _("SSERIAL-P2-TX"),_("SSERIAL-P2-RX"),_("SSERIAL-P2-EN"),
            _("SSERIAL-P3-TX"),_("SSERIAL-P3-RX"),_("SSERIAL-P3-EN"),
            _("SSERIAL-P4-TX"),_("SSERIAL-P4-RX"),_("SSERIAL-P4-EN"),
            _("SSERIAL-P5-TX"),_("SSERIAL-P5-RX"),_("SSERIAL-P5-EN"),
            _("SSERIAL-P6-TX"),_("SSERIAL-P6-RX"),_("SSERIAL-P6-EN"),
            _("SSERIAL-P7-TX"),_("SSERIAL-P7-RX"),_("SSERIAL-P7-EN"),
            _("7i76 I/O (SS0)"),_("7i76 I/O (SS2)"),_("7i76 I/O (SS3)"),
            _("7i77 I/O-SS0"),_("7i77 Analog-SS1"),_("7i77 I/O-SS3"),_("7i77 Analog-SS4"),]

        (   self.ANALOGIN, self.DUMMY3) = self.pintype_analog_in = [ _("Analog Input"),_("Dummy")]

        #***************************
        # HAL signal names
        #***************************
        (S.UNUSED_UNUSED,S.UNUSED_DUMMY) = self.hal_notused_names = ["unused-unused","unused_dummy"]

        (   self.UNUSED_OUTPUT,
            S.ON,S.CW,S.CCW,S.BRAKE,
            S.MIST,S.FLOOD,S.ESTOP,S.AMP,
            S.XAMP,S.YAMP,S.ZAMP,S.AAMP,
            S.PUMP,S.FORCE_PIN_TRUE,S.DOUT0,S.DOUT1,S.DOUT2,S.DOUT3,
            S.X_HALL1_OUT,S.X_HALL2_OUT,S.X_HALL3_OUT,S.X_C1_OUT,S.X_C2_OUT,S.X_C4_OUT,S.X_C8_OUT,
            S.Y_HALL1_OUT,S.Y_HALL2_OUT,S.Y_HALL3_OUT,S.Y_C1_OUT,S.Y_C2_OUT,S.Y_C4_OUT,S.Y_C8_OUT,
            S.Z_HALL1_OUT,S.Z_HALL2_OUT,S.Z_HALL3_OUT,S.Z_C1_OUT,S.Z_C2_OUT,S.Z_C4_OUT,S.Z_C8_OUT,
            S.A_HALL1_OUT,S.A_HALL2_OUT,S.A_HALL3_OUT,S.A_C1_OUT,S.A_C2_OUT,S.A_C4_OUT,S.A_C8_OUT,
            S_HALL1_OUT,S.S_HALL2_OUT,S.S_HALL3_OUT,S.S_C1_OUT,S.S_C2_OUT,S.S_C4_OUT,S.S_C8_OUT
        ) = self.hal_output_names = [
            "unused-output", 
            "spindle-on", "spindle-cw", "spindle-ccw", "spindle-brake",
            "coolant-mist", "coolant-flood", "estop-out", "machine-is-enabled",
            "x-enable", "y-enable", "z-enable", "a-enable",
            "charge-pump", "force-pin-true", "dout-00", "dout-01", "dout-02", "dout-03",
            "x-hall1-out","x-hall2-out","x-hall3-out","x-gray-c1-out","x-gray-c2-out","x-gray-C4-out","x-gray-C8-out",
            "y-hall1-out","y-hall2-out","y-hall3-out","y-gray-c1-out","y-gray-c2-out","y-gray-C4-out","y-gray-C8-out",
            "z-hall1-out","z-hall2-out","z-hall3-out","z-gray-c1-out","z-gray-c2-out","z-gray-C4-out","z-gray-C8-out",
            "a-hall1-out","a-hall2-out","a-hall3-out","a-gray-c1-out","a-gray-c2-out","a-gray-C4-out","a-gray-C8-out",
            "s-hall1-out","s-hall2-out","s-hall3-out","s-gray-c1-out","s-gray-c2-out","s-gray-C4-out","s-gray-C8-out", ]

        ###
        (   S.UNUSED_INPUT,
            S.MIN_X,S.MIN_Y,S.MIN_Z,S.MIN_A,
            S.MAX_X,S.MAX_Y,S.MAX_Z,S.MAX_A,
            S.BOTH_X,S.BOTH_Y,S.BOTH_Z,S.BOTH_A,S.ALL_LIMIT,
            S.HOME_X,S.HOME_Y,S.HOME_Z,S.HOME_A,S.ALL_HOME,
            S.MIN_HOME_X,S.MIN_HOME_Y,S.MIN_HOME_Z,S.MIN_HOME_A,
            S.MAX_HOME_X,S.MAX_HOME_Y,S.MAX_HOME_Z,S.MAX_HOME_A,
            S.BOTH_HOME_X,S.BOTH_HOME_Y,S.BOTH_HOME_Z,S.BOTH_HOME_A,S.ALL_LIMIT_HOME,
            S.DIN0,S.DIN1,S.DIN2,S.DIN3,
            S.SELECT_A,S.SELECT_B,S.SELECT_C,S.SELECT_D,
            S.JOGA,S.JOGB,S.JOGC,S.JOGD,S.FOA,S.FOB,S.FOC,S.FOD,
            S.SOA,S.SOB,S.SOC,S.SOD,S.MVOA,S.MVOB,S.MVOC,S.MVOD,
            S.FOE,S.SOE,S.MVOE,
            S.SPINDLE_CW,S.SPINDLE_CCW,S.SPINDLE_STOP,S.SPINDLE_AT_SPEED,S.GEAR_SELECT_A,
            S.CYCLE_START,S.ABORT,S.SINGLE_STEP,
            S.ESTOP_IN,S.PROBE,
            S.JOGX_P,S.JOGX_N,S.JOGY_P,S.JOGY_N,S.JOGZ_P,S.JOGZ_N,S.JOGA_P,S.JOGA_N,S.JOGSLCT_P,S.JOGSLCT_N,
            S.X_HALL1_IN,S.X_HALL2_IN,S.X_HALL3_IN,S.X_C1_IN,S.X_C2_IN,S.X_C4_IN,S.X_C8_IN,
            S.Y_HALL1_IN,S.Y_HALL2_IN,S.Y_HALL3_IN,S.Y_C1_IN,S.Y_C2_IN,S.Y_C4_IN,S.Y_C8_IN,
            S.Z_HALL1_IN,S.Z_HALL2_IN,S.Z_HALL3_IN,S.Z_C1_IN,S.Z_C2_IN,S.Z_C4_IN,S.Z_C8_IN,
            S.A_HALL1_IN,S.A_HALL2_IN,S.A_HALL3_IN,S.A_C1_IN,S.A_C2_IN,S.A_C4_IN,S.A_C8_IN,
            S.S_HALL1_IN,S.S_HALL2_IN,S.S_HALL3_IN,S.S_C1_IN,S.S_C2_IN,S.S_C4_IN,S.S_C8_IN,
            S.MIN_X2,S.MIN_Y2,S.MIN_Z2,S.MIN_A2,
            S.MAX_X2,S.MAX_Y2,S.MAX_Z2,S.MAX_A2,
            S.BOTH_X2,S.BOTH_Y2,S.BOTH_Z2,S.BOTH_A2,S.ALL_LIMIT,
            S.HOME_X2,S.HOME_Y2,S.HOME_Z2,S.HOME_A2,S.ALL_HOME,
            S.MIN_HOME_X2,S.MIN_HOME_Y2,S.MIN_HOME_Z2,S.MIN_HOME_A2,
            S.MAX_HOME_X2,S.MAX_HOME_Y2,S.MAX_HOME_Z2,S.MAX_HOME_A2,
            S.BOTH_HOME_X2,S.BOTH_HOME_Y2,S.BOTH_HOME_Z2,S.BOTH_HOME_A2
        ) = self.hal_input_names = [
            "unused-input",
            "min-x", "min-y", "min-z", "min-a",
            "max-x", "max-y", "max-z", "max-a",
            "both-x", "both-y", "both-z", "both-a","all-limit",
            "home-x", "home-y", "home-z", "home-a","all-home",
            "min-home-x", "min-home-y", "min-home-z", "min-home-a",
            "max-home-x", "max-home-y", "max-home-z", "max-home-a",
            "both-home-x", "both-home-y", "both-home-z", "both-home-a", "all-limit-home",
            "din-00", "din-01", "din-02", "din-03",
            "joint-select-a","joint-select-b","joint-select-c","joint-select-d",
            "jog-incr-a","jog-incr-b","jog-incr-c","jog-incr-d","fo-incr-a","fo-incr-b","fo-incr-c","fo-incr-d",
            "so-incr-a","so-incr-b","so-incr-c","so-incr-d","mvo-incr-a","mvo-incr-b","mvo-incr-c","mvo-incr-d",
            "fo-enable","so-enable","mvo-enable",
            "spindle-manual-cw","spindle-manual-ccw","spindle-manual-stop","spindle-at-speed","gear-select-a",
            "cycle-start","abort","single-step",
            "estop-ext", "probe-in",
            "jog-x-pos","jog-x-neg","jog-y-pos","jog-y-neg",
            "jog-z-pos","jog-z-neg","jog-a-pos","jog-a-neg","jog-selected-pos","jog-selected-neg",
            "x-hall1-in","x-hall2-in","x-hall3-in","x-gray-c1-in","x-gray-c2-in","x-gray-C4-in","x-gray-C8-in",
            "y-hall1-in","y-hall2-in","y-hall3-in","y-gray-c1-in","y-gray-c2-in","y-gray-C4-in","y-gray-C8-in",
            "z-hall1-in","z-hall2-in","z-hall3-in","z-gray-c1-in","z-gray-c2-in","z-gray-C4-in","z-gray-C8-in",
            "a-hall1-in","a-hall2-in","a-hall3-in","a-gray-c1-in","a-gray-c2-in","a-gray-C4-in","a-gray-C8-in",
            "s-hall1-in","s-hall2-in","s-hall3-in","s-gray-c1-in","s-gray-c2-in","s-gray-C4-in","s-gray-C8-in" ,
            "min-x2", "min-y2", "min-z2", "min-a2",
            "max-x2", "max-y2", "max-z2", "max-a2",
            "both-x2", "both-y2", "both-z2", "both-a2","all-limit",
            "home-x2", "home-y2", "home-z2", "home-a2","all-home",
            "min-home-x2", "min-home-y2", "min-home-z2", "min-home-a2",
            "max-home-x2", "max-home-y2", "max-home-z2", "max-home-a2",
            "both-home-x2", "both-home-y2", "both-home-z2", "both-home-a2"]

        (   S.UNUSED_PWM,
            S.X_PWM_PULSE,S.X_PWM_DIR,S.X_PWM_ENABLE,  S.Y_PWM_PULSE,S.Y_PWM_DIR,S.Y_PWM_ENABLE,
            S.Z_PWM_PULSE,S.Z_PWM_DIR,S.Z_PWM_ENABLE,  S.A_PWM_PULSE,S.A_PWM_DIR,S.A_PWM_ENABLE,
            S.SPINDLE_PWM_PULSE,S.SPINDLE_PWM_DIR,S.SPINDLE_PWM_ENABLE,
            S.X2_PWM_PULSE,S.X2_PWM_DIR,S.X2_PWM_ENABLE,  S.Y2_PWM_PULSE,S.Y2_PWM_DIR,S.Y2_PWM_ENABLE,
            S.Z2_PWM_PULSE,S.Z2_PWM_DIR,S.Z2_PWM_ENABLE,  S.A2_PWM_PULSE,S.A2_PWM_DIR,S.A2_PWM_ENABLE
        ) = self.hal_pwm_output_names = [
            "unused-pwm",
            "x-pwm-pulse", "x-pwm-dir", "x-pwm-enable", "y-pwm-pulse", "y-pwm-dir", "y-pwm-enable",
            "z-pwm-pulse", "z-pwm-dir", "z-pwm-enable", "a-pwm-pulse", "a-pwm-dir", "a-pwm-enable", 
            "s-pwm-pulse", "s-pwm-dir", "s-pwm-enable",
            "x2-pwm-pulse", "x2-pwm-dir", "x2-pwm-enable", "y2-pwm-pulse", "y2-pwm-dir", "y2-pwm-enable",
            "z2-pwm-pulse", "z2-pwm-dir", "z2-pwm-enable", "a2-pwm-pulse", "a2-pwm-dir", "a2-pwm-enable"]

        (   S.UNUSED_ENCODER,
            S.X_ENCODER_A,S.X_ENCODER_B,S.X_ENCODER_I,S.X_ENCODER_M,
            S.Y_ENCODER_A,S.Y_ENCODER_B,S.Y_ENCODER_I,S.Y_ENCODER_M,
            S.Z_ENCODER_A,S.Z_ENCODER_B,S.Z_ENCODER_I,S.Z_ENCODER_M,
            S.A_ENCODER_A,S.A_ENCODER_B,S.A_ENCODER_I,S.A_ENCODER_M,
            S.SPINDLE_ENCODER_A,S.SPINDLE_ENCODER_B,S.SPINDLE_ENCODER_I,S.SPINDLE_ENCODER_M,
            S.X_MPG_A,S.X_MPG_B,S.X_MPG_I,S.X_MPG_M,S.Y_MPG_A,S.Y_MPG_B,S.Y_MPG_I,S.Y_MPG_M,
            S.Z_MPG_A,S.Z_MPG_B,S.Z_MPG_I,S.Z_MPG_M,S.A_MPG_A,S.A_MPG_B,S.A_MPG_I,A_MPG_M,
            S.SELECT_MPG_A,S.SELECT_MPG_B,S.SELECT_MPG_I,S.SELECT_MPG_M,
            S.FO_MPG_A,S.FO_MPG_B,S.FO_MPG_I,S.FO_MPG_M,S.SO_MPG_A,S.SO_MPG_B,S.SO_MPG_I,S.SO_MPG_I,
            S.MVO_MPG_A,S.MVO_MPG_B,S.MVO_MPG_I,S.MVO_MPG_I,
            S.X2_ENCODER_A,S.X2_ENCODER_B,S.X2_ENCODER_I,S.X2_ENCODER_M,
            S.Y2_ENCODER_A,S.Y2_ENCODER_B,S.Y2_ENCODER_I,S.Y2_ENCODER_M,
            S.Z2_ENCODER_A,S.Z2_ENCODER_B,S.Z2_ENCODER_I,S.Z2_ENCODER_M,
            S.A2_ENCODER_A,S.A2_ENCODER_B,S.A2_ENCODER_I,S.A2_ENCODER_M,
        )  = self.hal_encoder_input_names = [
             "unused-encoder",
            "x-encoder-a", "x-encoder-b", "x-encoder-i", "x-encoder-m",
            "y-encoder-a", "y-encoder-b", "y-encoder-i", "y-encoder-m",
            "z-encoder-a", "z-encoder-b", "z-encoder-i", "z-encoder-m", 
            "a-encoder-a", "a-encoder-b", "a-encoder-i", "a-encoder-m",
            "s-encoder-a","s-encoder-b","s-encoder-i", "s-encoder-m",
            "x-mpg-a","x-mpg-b", "x-mpg-i", "x-mpg-m", "y-mpg-a", "y-mpg-b", "y-mpg-i", "y-mpg-m",
            "z-mpg-a","z-mpg-b", "z-mpg-i", "z-mpg-m", "a-mpg-a", "a-mpg-b", "a-mpg-i", "a-mpg-m",
            "select-mpg-a", "select-mpg-b", "select-mpg-i", "select-mpg-m",
            "fo-mpg-a","fo-mpg-b","fo-mpg-i","fo-mpg-m","so-mpg-a","so-mpg-b","so-mpg-i","so-mpg-m",
            "mvo-mpg-a","mvo-mpg-b","mvo-mpg-i","mvo-mpg-m",
            "x2-encoder-a", "x2-encoder-b", "x2-encoder-i", "x2-encoder-m",
            "y2-encoder-a", "y2-encoder-b", "y2-encoder-i", "y2-encoder-m",
            "z2-encoder-a", "z2-encoder-b", "z2-encoder-i", "z2-encoder-m",
            "a2-encoder-a", "a2-encoder-b", "a2-encoder-i", "a2-encoder-m",]

        (   S.USED_RESOLVER,S.X_RESOLVER,S.Y_RESOLVER,
                S.Z_RESOLVER,S.A_RESOLVER,S.S_RESOLVER
        ) = self.hal_resolver_input_names = ["unused-resolver","x-resolver","y-resolver",
                "z-resolver","a-resolver","s-resolver"]

        (   S.USED_8I20,S.X_8I20,S.Y_8I20,S.Z_8I20,S.A_8I20,S_8I20
        ) = self.hal_8i20_input_names =[
            "unused-8i20","x-8i20","y-8i20","z-8i20","a-8i20","s-8i20"]

        (   S.USED_POT,S_POT_OUT,S_POT_ENABLE  )= self.hal_pot_output_names = ["unused-pot",
            "s-pot-output","s-pot-enable"]

        (   S.UNUSED_STEPGEN,
            S.X_STEPGEN_STEP,S.X_STEPGEN_DIR,S.X_STEPGEN_PHC,
            S.X_STEPGEN_PHD,S.X_STEPGEN_PHE,S.X_STEPGEN_PHF,
            S.Y_STEPGEN_STEP,S.X_STEPGEN_DIR,S.X_STEPGEN_PHC,
            S.X_STEPGEN_PHD,S.X_STEPGEN_PHE,S.X_STEPGEN_PHF,
            S.Z_STEPGEN_STEP,S.Z_STEPGEN_DIR,S.Z_STEPGEN_PHC,
            S.Z_STEPGEN_PHD,S.Z_STEPGEN_PHE,S.Z_STEPGEN_PHF,
            S.A_STEPGEN_STEP,S.A_STEPGEN_DIR,S.A_STEPGEN_PHC,
            S.A_STEPGEN_PHD,S.A_STEPGEN_PHE,S.A_STEPGEN_PHF,
            S.SPINDLE_STEPGEN_STEP,S.SPINDLE_STEPGEN_DIR,S.SPINDLE_STEPGEN_PHC,
            S.SPINDLE_STEPGEN_PHD,S.SPINDLE_STEPGEN_PHE,S.SPINDLE_STEPGEN_PHF,
            S.X2_STEPGEN_STEP,S.X2_STEPGEN_DIR,S.X2_STEPGEN_PHC,
            S.X2_STEPGEN_PHD,S.X2_STEPGEN_PHE,S.X2_STEPGEN_PHF,
            S.Y2_STEPGEN_STEP,S.Y2_STEPGEN_DIR,S.Y2_STEPGEN_PHC,
            S.Y2_STEPGEN_PHD,S.Y2_STEPGEN_PHE,S.Y2_STEPGEN_PHF,
            S.Z2_STEPGEN_STEP,S.Z2_STEPGEN_DIR,S.Z2_STEPGEN_PHC,
            S.Z2_STEPGEN_PHD,S.Z2_STEPGEN_PHE,S.Z2_STEPGEN_PHF,
            S.CHARGE_PUMP_STEP,S.CHARGE_PUMP_DIR,S.CHARGE_PUMP_PHC,
            S.CHARGE_PUMP_PHD,S.CHARGE_PUMP_PHE,S.CHARGE_PUMP_PHF
        ) = self.hal_stepper_names =[
            "unused-stepgen", 
            "x-stepgen-step", "x-stepgen-dir", "x-stepgen-phase-c",
            "x-stepgen-phase-d", "x-stepgen-phase-e", "x-stepgen-phase-f",
            "y-stepgen-step", "y-stepgen-dir", "y-stepgen-phase-c",
            "y-stepgen-phase-d", "y-stepgen-phase-e", "y-stepgen-phase-f",
            "z-stepgen-step", "z-stepgen-dir", "z-stepgen-phase-c",
            "z-stepgen-phase-d", "z-stepgen-phase-e", "z-stepgen-phase-f",
            "a-stepgen-step", "a-stepgen-dir", "a-stepgen-phase-c",
            "a-stepgen-phase-d", "a-stepgen-phase-e", "a-stepgen-phase-f",
            "s-stepgen-step", "s-stepgen-dir", "s-stepgen-phase-c",
            "s-stepgen-phase-d", "s-stepgen-phase-e", "s-stepgen-phase-f",
            "x2-stepgen-step", "x2-stepgen-dir", "x2-stepgen-phase-c",
            "x2-stepgen-phase-d", "x2-stepgen-phase-e", "x2-stepgen-phase-f",
            "y2-stepgen-step", "y2-stepgen-dir", "y2-stepgen-phase-c",
            "y2-stepgen-phase-d", "y2-stepgen-phase-e", "y2-stepgen-phase-f",
            "z2-stepgen-step", "z2-stepgen-dir", "z2-stepgen-phase-c",
            "z2-stepgen-phase-d", "z2-stepgen-phase-e", "z2-stepgen-phase-f",
            "charge-pump-out","cp-dir","cp-pc","cp-pd","cp-fe","cp-pf"]

        (   S.UNUSED_TPPWM,
            S.X_TPPWM_A,S.X_TPPWM_B,S.X_TPPWM_C,S.X_TPPWM_AN,
            S.X_TPPWM_BN,S.X_TPPWM_CN,S.X_TPPWM_ENABLE,S.X_TPPWM_FAULT,
            S.Y_TPPWM_A,S.Y_TPPWM_B,S.Y_TPPWM_C,S.Y_TPPWM_AN,
            S.Y_TPPWM_BN,S.Y_TPPWM_CN,S.Y_TPPWM_ENABLE,S.Y_TPPWM_FAULT,
            S.Z_TPPWM_A,S.Z_TPPWM_B,S.Z_TPPWM_C,S.Z_TPPWM_AN,
            S.Z_TPPWM_BN,S.Z_TPPWM_CN,S.Z_TPPWM_ENABLE,S.Z_TPPWM_FAULT,
            S.A_TPPWM_A,S.A_TPPWM_B,S.A_TPPWM_C,S.A_TPPWM_AN,
            S.A_TPPWM_BN,S.A_TPPWM_CN,S.A_TPPWM_ENABLE,S.A_TPPWM_FAULT,
            S_TPPWM_A,S.S_TPPWM_B,S.S_TPPWM_C,S.S_TPPWM_AN,
            S.S_TPPWM_BN,S.S_TPPWM_CN,S.S_TPPWM_ENABLE,S.S_TPPWM_FAULT
        ) = self.hal_tppwm_output_names= [
            "unused-tppwm",
            "x-tppwm-a","x-tppwm-b","x-tppwm-c","x-tppwm-anot",
            "x-tppwm-bnot","x-tppwm-cnot", "x-tppwm-enable","x-tppwm-fault",
            "y-tppwm-a","y-tppwm-b","y-tppwm-c","y-tppwm-anot",
            "y-tppwm-bnot","y-tppwm-cnot", "y-tppwm-enable","y-tppwm-fault",
            "z-tppwm-a","z-tppwm-b","z-tppwm-c","z-tppwm-anot",
            "z-tppwm-bnot","z-tppwm-cnot", "z-tppwm-enable","z-tppwm-fault",
            "a-tppwm-a","a-tppwm-b","a-tppwm-c","a-tppwm-anot",
            "a-tppwm-bnot","a-tppwm-cnot", "a-tppwm-enable","a-tppwm-fault",
            "s-tppwm-a","s-tppwm-b","s-tppwm-c","s-tppwm-anot",
            "s-tppwm-bnot","s-tppwm-cnot", "s-tppwm-enable","s-tppwm-fault"]

        (   S.UNUSED_SSERIAL, S.A8I20_T, S.A8I20_R, S.A8I20_E,
            S.I7I64_T, S.I7I64_R, S.I7I64_E, S.I7I69_T, S.I7I69_R, S.I7I69_E,
            S.I7I70_T, S.I7I70_R, S.I7I70_E, S.I7I71_T, S.I7I71_R, S.I7I71_E, 
            S.I7I76_M0_T, S.I7I76_M0_R, S.I7I76_M0_E, S.I7I76_M2_T, S.I7I76_M2_R, S.I7I76_M2_E,
            S.I7I77_M0_T, S.I7I77_M0_R, S.I7I77_M0_E, S.I7I77_M3_T, S.I7I77_M3_R, S.I7I77_M3_E,
            S.I7I73_M0_T, S.I7I73_M0_R, S.I7I73_M0_E, S.I7I84_M0_T, S.I7I84_M0_R, S.I7I84_M0_E,
            S.I7I84_M3_T, S.I7I84_M3_R, S.I7I84_M3_E,
        ) = self.hal_sserial_names = [
            "unused-sserial","8i20-t","8i20-r","8i20-e",
            "7i64-t","7i64-r","7i64-e","7i69-t","7i69-r","7i69-e",
            "7i70-t","7i70-r","7i70-e","7i71-t","7i71-r","7i71-e",
            "7i76-m0-t","7i76-m0-r","7i76-m0-e", "7i76-m2-t","7i76-m2-r","7i76-m2-e",
            "7i77-m0-t","7i77-m0-r","7i77-m0-e","7i77-m3-t","7i77-m3-r","7i77-m3-e",
            "7i73-m1-t","7i73-m1-r","7i73-m1-e","7i84-m0-t","7i84-m0-r","7i84-m0-e","7i84-m3-t","7i84-m3-r","7i84-m3-e"]

        (S.UNUSED_ANALOG_IN) = self.hal_analog_input_names = ["unused-analog-input"]

        #*************************
        # Human names for HAL signals
        #*************************

        # These have three levels of columns
        home = [[_("X Home"),S.HOME_X ], [_("Y Home"),S.HOME_Y ], [_("Z Home"),S.HOME_Z ], [_("A Home"),S.HOME_A ],[_("All Home"),S.ALL_HOME ] ]
        home2 = [[_("X2 Tandem Home"),S.HOME_X2 ], [_("Y2 Tandem Home"),S.HOME_Y2 ], [_("Z2 Tandem Home"),S.HOME_Z2 ], [_("A2 Tandem Home"),S.HOME_A2 ] ]
        home_limits_shared = [[_("X Minimum Limit + Home"),S.MIN_HOME_X ],
            [_("Y Minimum Limit + Home"),S.MIN_HOME_Y ], [_("Z Minimum Limit + Home"),S.MIN_HOME_Z ],
            [_("A Minimum Limit + Home"),S.MIN_HOME_A ],[_("X Maximum Limit + Home"),S.MAX_HOME_X ],
            [_("Y Maximum Limit + Home"),S.MAX_HOME_Y ], [_("Z Maximum Limit + Home"),S.MAX_HOME_Z ],
            [_("A Maximum Limit + Home"),S.MAX_HOME_A ],[_("X Both Limit + Home"),S.BOTH_HOME_X ], 
            [_("Y Both Limit + Home"),S.BOTH_HOME_Y ], [_("Z Both Limit + Home"),S.BOTH_HOME_Z ],
            [_("A Both Limit + Home"),S.BOTH_HOME_A ], [_("All Limits + Home"),S.ALL_LIMIT_HOME ] ]
        home_limits_shared2 = [[_("X2 Minimum Limit + Home"),S.MIN_HOME_X2 ],
            [_("Y2 Minimum Limit + Home"),S.MIN_HOME_Y2 ], [_("Z2 Minimum Limit + Home"),S.MIN_HOME_Z2 ],
            [_("A2 Minimum Limit + Home"),S.MIN_HOME_A2 ],[_("X2 Maximum Limit + Home"),S.MAX_HOME_X2 ],
            [_("Y2 Maximum Limit + Home"),S.MAX_HOME_Y2 ], [_("Z2 Maximum Limit + Home"),S.MAX_HOME_Z2 ],
            [_("A2 Maximum Limit + Home"),S.MAX_HOME_A2 ],[_("X2 Both Limit + Home"),S.BOTH_HOME_X2 ], 
            [_("Y2 Both Limit + Home"),S.BOTH_HOME_Y2 ], [_("Z2 Both Limit + Home"),S.BOTH_HOME_Z2 ],
            [_("A2 Both Limit + Home"),S.BOTH_HOME_A2 ], ]

        digital = [ [_("Digital in 0"),S.DIN0 ], [_("Digital in 1"),S.DIN1 ], [_("Digital in 2"),S.DIN2 ], [_("Digital in 3"),S.DIN3 ] ]
        axis_select = [[_("Joint select A"),S.SELECT_A ],[_("Joint select B"),S.SELECT_B ],
                [_("Joint select C"),S.SELECT_C ], [_("Joint select D"),S.SELECT_D ] ]
        override = [[_("Jog incr A"),S.JOGA ],[_("Jog incr B"),S.JOGB ],[_("Jog incr C"),S.JOGC ],
            [_("Jog incr D"),S.JOGD ],[_("Feed Override incr A"),S.FOA ],[_("Feed Override incr B"), S.FOB],
            [_("Feed Override incr C"),S.FOC ],[_("Feed Override incr D"),S.FOD ],[_("Spindle Override incr A"),S.SOA ],
            [_("Spindle Override incr B"),S.SOB ],[_("Spindle Override incr C"),S.SOC ],[_("Spindle Override incr D"),S.SOD ],
            [_("Max Vel Override incr A"),S.MVOA ],[_("Max Vel Override incr B"),S.MVOB ],[_("Max Vel Override incr C"),S.MVOC ],
            [_("Max Vel Override incr D"),S.MVOD ], [_("Feed Override enable"),S.FOE ], [_("Spindle Override enable"),S.SOE ],
            [_("Max Vel Override enable"),S.MVOE ] ]
        spindle = [ [_("Manual Spindle CW"),S.SPINDLE_CW ],[_("Manual Spindle CCW"),S.SPINDLE_CCW ],[_("Manual Spindle Stop"),S.SPINDLE_STOP ],
            [_("Spindle Up-To-Speed"),S.SPINDLE_AT_SPEED ],[_("Gear Select A"),S.GEAR_SELECT_A ] ]
        operation =  [[_("Cycle Start"),S.CYCLE_START ],[_("Abort"),S.ABORT ],[_("Single Step"),S.SINGLE_STEP ] ]
        control = [[_("ESTOP In"),S.ESTOP_IN ], [_("Probe In"),S.PROBE ] ]
        rapid = [[_("Jog X +"),S.JOGX_P ],[_("Jog X -"),S.JOGX_N ],[_("Jog Y +"),S.JOGY_P ],[_("Jog Y -"),S.JOGY_N ],
            [_("Jog Z +"),S.JOGZ_P ],[_("Jog Z -"),S.JOGZ_N ],[_("Jog A +"),S.JOGA_P ],[_("Jog A -"),S.JOGA_N ],
            [_("Jog button selected +"),S.JOGSLCT_P ],[_("Jog button selected -"),S.JOGSLCT_N ] ]
        xmotor_control = [[_("X HALL 1"),S.X_HALL1_IN ],[_("X HALL 2"),S.X_HALL2_IN ],[_("X HALL 3"),S.X_HALL3_IN ],
            [_("X Gray C1"),S.X_C1_IN ],[_("X Gray C2"),S.X_C2_IN ],[_("X Gray C4"),S.X_C4_IN ],[_("X Gray C8"),S.X_C8_IN ]]
        ymotor_control = [[_("Y HALL 1"),S.Y_HALL1_IN ],[_("Y HALL 2"),S.Y_HALL2_IN ],[_("Y HALL 3"),S.Y_HALL3_IN ],
            [_("Y Gray C1"),S.Y_C1_IN ],[_("Y Gray C2"),S.Y_C2_IN ],[_("Y Gray C4"),S.Y_C4_IN ],[_("Y Gray C8"),S.Y_C8_IN ]]
        zmotor_control = [[_("Z HALL 1"),S.Z_HALL1_IN ],[_("Z HALL 2"),S.Z_HALL2_IN ],[_("Z HALL 3"),S.Z_HALL1_IN ],
            [_("Z Gray C1"),S.Z_C1_IN ],[_("Z Gray C2"),S.Z_C2_IN ],[_("Z Gray C4"),S.Z_C4_IN ],[_("Z Gray C8"),S.Z_C8_IN ]]
        amotor_control = [[_("A HALL 1"),S.A_HALL1_IN ],[_("A HALL 2"),S.A_HALL2_IN ],[_("A HALL 3"),S.A_HALL3_IN ],
            [_("A Gray C1"),S.A_C1_IN ],[_("A Gray C2"),S.A_C2_IN ],[_("A Gray C4"),S.A_C4_IN ],[_("A Gray C8"),S.A_C8_IN ]]
        smotor_control = [[_("S HALL 1"),S.S_HALL1_IN ],[_("S HALL 2"),S.S_HALL2_IN ],[_("S HALL 3"),S.S_HALL3_IN ],
            [_("S Gray C1"),S.S_C1_IN ],[_("S Gray C2"),S.S_C2_IN ],[_("S Gray C4"),S.S_C4_IN ],[_("S Gray C8"),S.S_C8_IN ]]
        limit = [[_("X Minimum Limit"),S.MIN_X ], [_("Y Minimum Limit"),S.MIN_Y ], [_("Z Minimum Limit"),S.MIN_Z ], [_("A Minimum Limit"),S.MIN_A ],
            [_("X Maximum Limit"),S.MAX_X ], [_("Y Maximum Limit"),S.MAX_Y ], [_("Z Maximum Limit"),S.MAX_Z ], [_("A Maximum Limit"),S.MAX_A ],
            [_("X Both Limit"),S.BOTH_X ], [_("Y Both Limit"),S.BOTH_Y ], [_("Z Both Limit"),S.BOTH_Z ], [_("A Both Limit"), S.BOTH_A],
            [_("All Limits"),S.ALL_LIMIT] ]
        limit2 = [[_("X2 Minimum Limit"),S.MIN_X2 ], [_("Y2 Minimum Limit"),S.MIN_Y2 ], [_("Z2 Minimum Limit"),S.MIN_Z2 ], [_("A2 Minimum Limit"),S.MIN_A2 ],
            [_("X2 Maximum Limit"),S.MAX_X2 ], [_("Y2 Maximum Limit"),S.MAX_Y2 ], [_("Z2 Maximum Limit"),S.MAX_Z2 ], [_("A2 Maximum Limit"),S.MAX_A2 ],
            [_("X2 Both Limit"),S.BOTH_X2 ], [_("Y2 Both Limit"),S.BOTH_Y2 ], [_("Z2 Both Limit"),S.BOTH_Z2 ], [_("A2 Both Limit"), S.BOTH_A2], ]

        blimits = [[_("Main Axis"),limit],[_("Tandem Axis"),limit2]]
        bhome = [[_("Main Axis"),home],[_("Tandem Axis"),home2]]
        bshared = [[_("Main Axis"),home_limits_shared],[_("Tandem Axis"),home_limits_shared2]]
        self.human_input_names = [ [_("Unused Input"),S.UNUSED_INPUT],[_("Limits"),blimits],
            [_("home"),bhome],[_("Limts/Home Shared"),bshared],
            [_("Digital"),digital],[_("Axis Selection"),axis_select],[_("Overrides"),override],
            [_("Spindle"),spindle],[_("Operation"),operation],[_("External Control"),control],
            [_("Axis rapid"),rapid],[_("X BLDC Control"),xmotor_control],
            [_("Y BLDC Control"),ymotor_control],[_("Z BLDC Control"),zmotor_control],
            [_("A BLDC Control"),amotor_control],
            [_("S BLDC Control"),smotor_control],[_("Custom Signals"),[]] ]
        #
        tpwm = [[_("X2 Tandem PWM"), S.X2_PWM_PULSE], [_("Y2 Tandem PWM"), S.Y2_PWM_PULSE],
             [_("Z2 Tandem PWM"), S.Z2_PWM_PULSE], [_("A2 Tandem PWM"), S.A2_PWM_PULSE]]
        mpwm =[[_("X Axis PWM"), S.X_PWM_PULSE],[_("Y Axis PWM"), S.Y_PWM_PULSE],
            [_("Z Axis PWM"), S.Z_PWM_PULSE],[_("A Axis PWM"), S.A_PWM_PULSE]]
        tandem_pwm = [[_("Main Axis"),mpwm],[_("Tandem Axis"),tpwm]]
        self.human_pwm_output_names =[ [_("Unused PWM Gen"), S.UNUSED_PWM],[_("Axis PWM"),
            tandem_pwm],[_("Spindle PWM"), S.SPINDLE_PWM_PULSE],
            [_("Custom Signals"),[]] ]
        #
        main_step = [[_("X Axis StepGen"),S.X_STEPGEN_STEP],
                [_("Y Axis StepGen"),S.Y_STEPGEN_STEP],[_("Z Axis StepGen"),S.Z_STEPGEN_STEP],
                [_("A Axis StepGen"),S.A_STEPGEN_STEP] ]
        tandem_step = [ [_("X2 Tandem StepGen"),S.X2_STEPGEN_STEP],[_("Y2 Tandem StepGen"),S.Y2_STEPGEN_STEP],
                [_("Z2 Tandem StepGen"),S.Z2_STEPGEN_STEP]]
        bstep = [[_("Main Axis"),main_step],[_("Tandem Axis"),tandem_step]]
        self.human_stepper_names = [ [_("Unused StepGen"),S.UNUSED_STEPGEN],
            [_("Axis"),bstep],
            [_("Charge Pump StepGen"), S.CHARGE_PUMP_STEP], [_("Spindle StepGen"), S.SPINDLE_STEPGEN_STEP],
            [_("Custom Signals"),[]] ]
        #
        axis = [[_("X Encoder"), S.X_ENCODER_A], [_("Y Encoder"), S.Y_ENCODER_A], 
                [_("Z Encoder"), S.Z_ENCODER_A], [_("A Encoder"), S.A_ENCODER_A],
                ]
        taxis = [[_("X2 Tandem Encoder"), S.X2_ENCODER_A], [_("Y2 Tandem Encoder"), S.Y2_ENCODER_A],
                 [_("Z2 Tandem Encoder"), S.Z2_ENCODER_A], [_("A2 Tandem Encoder"), S.A2_ENCODER_A]]
        mpg = [[_("X Hand Wheel"), S.X_MPG_A], [_("Y Hand Wheel"), S.Y_MPG_A],
                 [_("Z Hand Wheel"), S.Z_MPG_A], [_("A Hand Wheel"), S.A_MPG_A],
                 [_("Multi Hand Wheel"), S.SELECT_MPG_A]]
        over = [[_("Feed Override"),  S.FO_MPG_A], [_("spindle Override"),  S.SO_MPG_A],[_("Max Vel Override"),  S.MVO_MPG_A]]
        tandem_enc = [[_("Main Axis"),axis],[_("Tandem Axis"),taxis]]
        self.human_encoder_input_names = [ [_("Unused Encoder"), S.UNUSED_ENCODER],[_("Axis Encoder"), tandem_enc],
             [_("Spindle Encoder"), S.SPINDLE_ENCODER_A], [_("MPG Jog Controls"), mpg],[_("Override MPG control"), over],
            [_("Custom Signals"),[]] ]

        # These have two levels of columns
        self.human_notused_names = [ [_("Unused Unused"),[] ] ]
        spindle_output = [_("Spindle ON"),_("Spindle CW"), _("Spindle CCW"), _("Spindle Brake") ]
        coolant_output = [_("Coolant Mist"), _("Coolant Flood")]
        control_output = [_("ESTOP Out"), _("Machine Is Enabled"),_("X Amplifier Enable"),
            _("Y Amplifier Enable"),_("Z Amplifier Enable"), _("A Amplifier Enable"),
            _("Charge Pump"),_("Force Pin True")]
        digital_output = [_("Digital out 0"), _("Digital out 1"), _("Digital out 2"), _("Digital out 3")]
        xmotor_control = [_("X HALL 1"),_("X HALL 2"),_("X HALL 3"),
                _("X Gray C1"),_("X Gray C2"),_("X Gray C4"),_("X Gray C8")]
        ymotor_control = [_("Y HALL 1"),_("Y HALL 2"),_("Y HALL 3"),
                _("Y Gray C1"),_("Y Gray C2"),_("Y Gray C4"),_("Y Gray C8")]
        zmotor_control = [_("Z HALL 1"),_("Z HALL 2"),_("Z HALL 3"),
                _("Z Gray C1"),_("Z Gray C2"),_("Z Gray C4"),_("Z Gray C8")]
        amotor_control = [_("A HALL 1"),_("A HALL 2"),_("A HALL 3"),
                _("A Gray C1"),_("A Gray C2"),_("A Gray C4"),_("A Gray C8")]
        smotor_control = [_("S HALL 1"),_("S HALL 2"),_("S HALL 3"),
                _("S Gray C1"),_("S Gray C2"),_("S Gray C4"),_("S Gray C8")]

        self.human_output_names = [ [_("Unused Output"),[]],[_("Spindle"),spindle_output],
            [_("Coolant"),coolant_output],[_("Control"),control_output],
            [_("Digital"),digital_output],[_("X BLDC Control"),xmotor_control],
            [_("Y BLDC Control"),ymotor_control],[_("Z BLDC Control"),zmotor_control],
            [_("A BLDC Control"),amotor_control],[_(" S BLDC Control"),smotor_control,],
            [_("Custom Signals"),[]]  ]

        self.human_8i20_input_names =[ [_("Unused 8I20"),[]],[_("X Axis"), []],[_("Y Axis"), []],[_("Z Axis"), []],
            [_("A Axis"), []],[_("Spindle"), []],[_("Custom Signals"),[]] ]

        self.human_resolver_input_names =[ [_("Unused Resolver"),[]],[_("X Resolver"), []],
            [_("Y Resolver"), []],[_("Z Resolver"), []],
            [_("A Resolver"), []],[_("S Resolver"), []],[_("Custom Signals"),[]] ]

        self.human_pot_output_names =[ [_("Unused Analog Output"),[]],[_("Spindle Output"), []],
            [_("Custom Signals"),[]] ]


        self.human_tppwm_output_names = [ [_("Unused TPPWM Gen"),[]], [_("X Axis BL Driver"),[]],
            [ _("Y Axis BL Driver"),[]], [_("Z Axis BL Driver"),[]],
            [_("A Axis BL Driver"),[]], [_("S Axis BL Driver"),[]],
            [_("Custom Signals"),[]] ]

        self.human_sserial_names = [ [_("Unused Channel"),[]],[_("8i20 Amplifier Card"),[]],
            [ _("7i64 I/O Card"),[]],[ _("7i69 I/O Card"),[]],
            [ _("7i70 I/O Card"),[]],[ _("7i71 I/O Card"),[]],
            [ _("7i76 Mode 0 I/O Card"),[]],[ _("7i76 Mode 2 I/O Card"),[]],
            [ _("7i77 Mode 0 I/O Card"),[]],[ _("7i77 Mode 3 I/O Card"),[]],
            [ _("7i73 Mode 1 Pendant Card"),[]],
            [ _("7i84 Mode 1 I/O Card"),[]],[ _("7i84 Mode 3 I/O Card"),[]], ]

        self.human_analog_input_names = [ [_("Unused Analog In"),[]],[_("Custom Signals"),[]] ]

        #*******************************
        # internal firmware data
        #*******************************
        self._BOARDTITLE = 0;self._BOARDNAME = 1;self._FIRMWARE = 2;self._DIRECTORY = 3;self._HALDRIVER = 4;
        self._MAXENC = 5;self._ENCPINS = 6;self._MAXRES = 7;self._RESPINS = 8;self._MAXPWM = 9;
        self._PWMPINS = 10;self._MAXTPPWM = 11;self._TTPWMPINMS = 12;self._MAXSTEP = 13;self._STEPPINS = 14;
        self._MAXSSERIALPORTS = 15;self._MAXSSERIALCHANNELS = 16;self._SSDEVICES=17;self._HASWATCHDOG = 25;
        self._MAXGPIO = 26;self._LOWFREQ = 27;self._HIFREQ = 28;self._NUMOFCNCTRS = 29;self._STARTOFDATA = 30
# board title, boardname, firmwarename, firmware directory,Hal driver name,
# max encoders, number of pins per encoder,
# max resolver gens, # of pins,
# max pwm gens, # of pins
# max tppwmgens , # of pins
# max step gens, number of pins per step gen,
# max smart serial, number of channels,
# discovered sserial devices,
# spare,spare,spare,spare,spare,spare,spare,
# has watchdog, max GPIOI, 
# low frequency rate , hi frequency rate, 
# available connector numbers,  then list of component type and logical number
        self.MESA_INTERNAL_FIRMWAREDATA = [
    # 5i25 ####################
    ['5i25-Internal Data', '5i25', '7i76x2 -With One 7i76', '5i25', 'hm2_pci',
         1,3, 0,0, 0,3, 0,0, 5,2, 1,2, [],0,0,0,0,0,0,0, 1, 34, 33, 200, [3, 2],
        # TAB 3
        [S.STEPB,0],[S.STEPA,0],[S.STEPB,1],[S.STEPA,1],[S.STEPB,2],[S.STEPA,2],[S.STEPB,3],[S.STEPA,3],[S.STEPB,4],[S.STEPA,4],
        [S.SS7I76M0,0],[S.RXDATA0,0],[S.TXDATA1,0],[S.RXDATA1,0],[S.ENCI,0],[S.ENCB,0],[S.ENCA,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        # TAB 2
        [S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],
        [S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],],

    ["5i25-Internal Data", "5i25", "7i76x2", "5i25", "hm2_pci",
         2,3, 0,0, 0,3, 0,0, 10,2, 1,4, [],0,0,0,0,0,0,0, 1, 34 , 33, 200, [3,2],
        # TAB 3
        [S.STEPB,0],[S.STEPA,0],[S.STEPB,1],[S.STEPA,1],[S.STEPB,2],[S.STEPA,2],[S.STEPB,3],[S.STEPA,3],[S.STEPB,4],[S.STEPA,4],
        [S.SS7I76M0,0],[S.RXDATA0,0],[S.TXDATA1,0],[S.RXDATA1,0],[S.ENCI,0],[S.ENCB,0],[S.ENCA,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        # TAB 2
        [S.STEPB,5],[S.STEPA,5],[S.STEPB,6],[S.STEPA,6],[S.STEPB,7],[S.STEPA,7],[S.STEPB,8],[S.STEPA,8],[S.STEPB,9],[S.STEPA,9],
        [S.SS7I76M2,0],[S.RXDATA2,0],[S.TXDATA3,0],[S.RXDATA3,0],[S.ENCI,1],[S.ENCB,1],[S.ENCA,1],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],],

    ['5i25-Internal Data', '5i25', '7i77_7i76', '5i25', 'hm2_pci',
         6,3, 0,0, 0,3, 0,0, 5,2, 1,5, [],0,0,0,0,0,0,0, 1, 34, 33, 200, [3, 2],
        # Tab 3
        [S.TXEN2, 0],[S.TXDATA2, 0],[S.RXDATA2, 0],[S.SS7I77M1, 0],[S.RXDATA1, 0],[S.SS7I77M0, 0],[S.RXDATA0, 0],[S.MXES, 0],[S.MXE0, 0],[S.MXE1, 0],
        [S.MXEI, 0],[S.MXE0, 1],[S.MXE1, 1],[S.MXEI, 1],[S.MXE0, 2],[S.MXE1, 2],[S.MXEI, 2],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        # TAB 2
        [S.STEPB,0],[S.STEPA,0],[S.STEPB,1],[S.STEPA,1],[S.STEPB,2],[S.STEPA,2],[S.STEPB,3],[S.STEPA,3],[S.STEPB,4],[S.STEPA,4],
        [S.SS7I76M3,0],[S.RXDATA3,0],[S.TXDATA4,0],[S.RXDATA4,0],[S.ENCI,0],[S.ENCB,0],[S.ENCA,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],],

    ['5i25-Internal Data', '5i25', '7i77x2 With One 7i77', '5i25', 'hm2_pci',
         6,3, 0,0, 0,3, 0,0, 0,2, 1,3, [],0,0,0,0,0,0,0, 1, 34, 33, 200, [3, 2],
        # TAB 3
        [S.TXEN2, 0],[S.TXDATA2, 0],[S.RXDATA2, 0],[S.SS7I77M1, 0],[S.RXDATA1, 0],[S.SS7I77M0, 0],[S.RXDATA0, 0],[S.MXES, 0],[S.MXE0, 0],[S.MXE1, 0],
        [S.MXEI, 0],[S.MXE0, 1],[S.MXE1, 1],[S.MXEI, 1],[S.MXE0, 2],[S.MXE1, 2],[S.MXEI, 2],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        # TAB 2
        [S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],
        [S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],],

    ['5i25-Internal Data', '5i25', '7i77x2', '5i25', 'hm2_pci',
        12,3, 0,0, 0,3, 0,0, 0,2, 1,6, [],0,0,0,0,0,0,0, 1, 34, 33, 200, [3, 2], 
        # TAB 3
        [S.TXEN2, 0],[S.TXDATA2, 0],[S.RXDATA2, 0],[S.SS7I77M1, 0],[S.RXDATA1, 0],[S.SS7I77M0, 0],[S.RXDATA0, 0],[S.MXES, 0],[S.MXE0, 0],[S.MXE1, 0],
        [S.MXEI, 0],[S.MXE0, 1],[S.MXE1, 1],[S.MXEI, 1],[S.MXE0, 2],[S.MXE1, 2],[S.MXEI, 2],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        # TAB 2
        [S.TXEN5, 0],[S.TXDATA5, 0],[S.RXDATA5, 0],[S.SS7I77M4, 0],[S.RXDATA4, 0],[S.SS7I77M3, 0],[S.RXDATA3, 0],[S.MXES, 3],[S.MXE0, 3],[S.MXE1, 3],
        [S.MXEI, 3],[S.MXE0, 4],[S.MXE1, 4],[S.MXEI, 4],[S.MXE0, 5],[S.MXE1, 5],[S.MXEI, 5],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],],

    ['5i25-Internal Data', '5i25', 'prob_rfx2', '5i25', 'hm2_pci',
        2,3, 0,0, 2,3, 0,0, 8,2, 0,0, [],0,0,0,0,0,0,0, 1, 34, 33, 200,[3, 2],
        # TAB 3
        [S.GPIOI, 0],[S.PWMP, 0],[S.STEPA, 0],[S.GPIOI, 0],[S.STEPB, 0],[S.PWMD, 0],[S.STEPA, 1],[S.GPIOI, 0],[S.STEPB, 1],[S.STEPA, 2],
        [S.STEPB, 2],[S.STEPA, 3],[S.STEPB, 3],[S.GPIOI, 0],[S.ENCA, 0],[S.ENCB, 0],[S.ENCI, 0],
        [S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],
        # TAB 2
        [S.GPIOI, 0],[S.PWMP, 1],[S.STEPA, 4],[S.GPIOI, 0],[S.STEPB, 4],[S.PWMD, 1],[S.STEPA, 5],[S.GPIOI, 0],[S.STEPB, 5],
        [S.STEPA, 6],[S.STEPB, 6],[S.STEPA, 7],[S.STEPB, 7],[S.GPIOI, 0],[S.ENCA, 1],[S.ENCB, 1],[S.ENCI, 1],
        [S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],],

    ['5i25-Internal Data', '5i25', 'G540x2', '5i25', 'hm2_pci', 2,3, 0,0, 2,1, 0,0, 10,2, 0,0, 0,0,0,0,0,0,0,0, 1, 34, 33, 200,[3, 2],
        # TAB 3
        [S.GPIOI, 0],[S.PWMP, 0],[S.STEPA, 0],[S.GPIOI, 0],[S.STEPB, 0],[S.STEPA, 4],[S.STEPA, 1],[S.GPIOI, 0],[S.STEPB, 1],[S.STEPA, 2],
        [S.STEPB, 2],[S.STEPA, 3],[S.STEPB, 3],[S.ENCA, 0],[S.ENCB, 0],[S.ENCI, 0],[S.GPIOI, 0],
        [S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],
        # TAB 2
        [S.GPIOI, 0],[S.PWMP, 1],[S.STEPA, 5],[S.GPIOI, 0],[S.STEPB, 5],[S.STEPA, 9],[S.STEPA, 6],[S.GPIOI, 0],[S.STEPB, 6],[S.STEPA, 7],
        [S.STEPB, 7],[S.STEPA, 8],[S.STEPB, 8],[S.ENCA, 1],[S.ENCB, 1],[S.ENCI, 1],[S.GPIOI, 0],
        [S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],[S.NUSED, 0],],

    # 5i20 #####################
    ["5i20", "5i20", "SV12", "5i20", "hm2_pci",
        12,3, 0,0, 12,3, 0,0, 0,0, 0,0, [],0,0,0,0,0,0,0, 1, 72 , 33, 100, [2,3,4],
        [S.ENCB,1],[S.ENCA,1],[S.ENCB,0],[S.ENCA,0],[S.ENCI,1],[S.ENCI,0],[S.PWMP,1],[S.PWMP,0],[S.PWMD,1],[S.PWMD,0],[S.PWME,1],[S.PWME,0],
        [S.ENCB,3],[S.ENCA,3],[S.ENCB,2],[S.ENCA,2],[S.ENCI,3],[S.ENCI,2],[S.PWMP,3],[S.PWMP,2],[S.PWMD,3],[S.PWMD,2],[S.PWME,3],[S.PWME,2],
        [S.ENCB,5],[S.ENCA,5],[S.ENCB,4],[S.ENCA,4],[S.ENCI,5],[S.ENCI,4],[S.PWMP,5],[S.PWMP,4],[S.PWMD,5],[S.PWMD,4],[S.PWME,5],[S.PWME,4],
        [S.ENCB,7],[S.ENCA,7],[S.ENCB,6],[S.ENCA,6],[S.ENCI,7],[S.ENCI,6],[S.PWMP,7],[S.PWMP,6],[S.PWMD,7],[S.PWMD,6],[S.PWME,7],[S.PWME,6],
        [S.ENCB,9],[S.ENCA,9],[S.ENCB,8],[S.ENCA,8],[S.ENCI,9],[S.ENCI,8],[S.PWMP,9],[S.PWMP,8],[S.PWMD,9],[S.PWMD,8],[S.PWME,9],[S.PWME,8],
        [S.ENCB,11],[S.ENCA,11],[S.ENCB,10],[S.ENCA,10],[S.ENCI,11],[S.ENCI,10],[S.PWMP,11],[S.PWMP,10],[S.PWMD,11],[S.PWMD,10],[S.PWME,11],[S.PWME,10]
    ],
    ["5i20", "5i20", "SVST8_4", "5i20", "hm2_pci",
        8,3, 0,0, 8,3, 0,0, 4,2, 0,0, [],0,0,0,0,0,0,0, 1, 72, 33, 100, [2,3,4],
      [S.ENCB,1],[S.ENCA,1],[S.ENCB,0],[S.ENCA,0],[S.ENCI,1],[S.ENCI,0],[S.PWMP,1],[S.PWMP,0],[S.PWMD,1],[S.PWMD,0],[S.PWME,1],[S.PWME,0],
      [S.ENCB,3],[S.ENCA,3],[S.ENCB,2],[S.ENCA,2],[S.ENCI,3],[S.ENCI,2],[S.PWMP,3],[S.PWMP,2],[S.PWMD,3],[S.PWMD,2],[S.PWME,3],[S.PWME,2],
      [S.ENCB,5],[S.ENCA,5],[S.ENCB,4],[S.ENCA,4],[S.ENCI,5],[S.ENCI,4],[S.PWMP,5],[S.PWMP,4],[S.PWMD,5],[S.PWMD,4],[S.PWME,5],[S.PWME,4],
      [S.ENCB,7],[S.ENCA,7],[S.ENCB,6],[S.ENCA,6],[S.ENCI,7],[S.ENCI,6],[S.PWMP,7],[S.PWMP,6],[S.PWMD,7],[S.PWMD,6],[S.PWME,7],[S.PWME,6],
      [S.STEPA,0],[S.STEPB,0],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0],[S.STEPA,1],[S.STEPB,1],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0],
      [S.STEPA,2],[S.STEPB,2],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0],[S.STEPA,3],[S.STEPB,3],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0]
     ],

    # 7i76e ####################
    ['7i76e-Internal Data', '7i76e', '7i76e', '7i76', 'hm2_eth',
        1,3, 0,0, 0,3, 0,0, 5,2, 1,2, [],0,0,0,0,0,0,0, 1, 34, 33, 200, [1, 2, 3],
        # TAB 1
        [S.STEPB,0],[S.STEPA,0],[S.STEPB,1],[S.STEPA,1],[S.STEPB,2],[S.STEPA,2],[S.STEPB,3],[S.STEPA,3],[S.STEPB,4],[S.STEPA,4],
        [S.SS7I76M0,0],[S.RXDATA0,0],[S.TXDATA1,0],[S.RXDATA1,0],[S.ENCI,0],[S.ENCB,0],[S.ENCA,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        # TAB 2
        [S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],
        [S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        # TAB 3
        [S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],
        [S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],],

    # 7i92 ####################
    ["7i92-Internal Data", "7i92", "7i76x2 with one 7i76 ", "7i92", "hm2_eth",
         1,3, 0,0, 0,3, 0,0, 5,2, 1,2, [],0,0,0,0,0,0,0, 1, 34 , 33, 200, [2,1],
        # TAB 2
        [S.STEPB,0],[S.STEPA,0],[S.STEPB,1],[S.STEPA,1],[S.STEPB,2],[S.STEPA,2],[S.STEPB,3],[S.STEPA,3],[S.STEPB,4],[S.STEPA,4],
        [S.SS7I76M0,0],[S.RXDATA0,0],[S.TXDATA1,0],[S.RXDATA1,0],[S.ENCI,0],[S.ENCB,0],[S.ENCA,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        # TAB 1
        [S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],
        [S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],],

    ["7i92-Internal Data", "7i92", "7i76x2", "7i92", "hm2_eth",
        2,3, 0,0, 0,3, 0,0, 10,2, 1,4, [],0,0,0,0,0,0,0, 1, 34 , 33, 200, [2,1],
        # TAB 2
        [S.STEPB,0],[S.STEPA,0],[S.STEPB,1],[S.STEPA,1],[S.STEPB,2],[S.STEPA,2],[S.STEPB,3],[S.STEPA,3],[S.STEPB,4],[S.STEPA,4],
        [S.SS7I76M0,0],[S.RXDATA0,0],[S.TXDATA1,0],[S.RXDATA1,0],[S.ENCI,0],[S.ENCB,0],[S.ENCA,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        # TAB 1
        [S.STEPB,5],[S.STEPA,5],[S.STEPB,6],[S.STEPA,6],[S.STEPB,7],[S.STEPA,7],[S.STEPB,8],[S.STEPA,8],[S.STEPB,9],[S.STEPA,9],
        [S.SS7I76M2,0],[S.RXDATA2,0],[S.TXDATA3,0],[S.RXDATA3,0],[S.ENCI,1],[S.ENCB,1],[S.ENCA,1],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],],

    ['7i92-Internal Data', '7i92', '7i77x2 with on 7i77', '7i92', 'hm2_eth',
        6,3, 0,0, 0,3, 0,0, 0,2, 1,3, [],0,0,0,0,0,0,0, 1, 34, 33, 200, [2, 1], 
        # TAB 2
        [S.TXEN2, 0],[S.TXDATA2, 0],[S.RXDATA2, 0],[S.SS7I77M1, 0],[S.RXDATA1, 0],[S.SS7I77M0, 0],[S.RXDATA0, 0],[S.MXES, 0],[S.MXE0, 0],[S.MXE1, 0],
        [S.MXEI, 0],[S.MXE0, 1],[S.MXE1, 1],[S.MXEI, 1],[S.MXE0, 2],[S.MXE1, 2],[S.MXEI, 2],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        # TAB 1
        [S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],
        [S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],[S.GPIOI, 0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],],

    ['7i92-Internal Data', '7i92', '7i77x2', '7i92', 'hm2_eth',
        12,3, 0,0, 0,3, 0,0, 0,2, 1,6, [],0,0,0,0,0,0,0, 1, 34, 33, 200, [2, 1], 
        # TAB 2
        [S.TXEN2, 0],[S.TXDATA2, 0],[S.RXDATA2, 0],[S.SS7I77M1, 0],[S.RXDATA1, 0],[S.SS7I77M0, 0],[S.RXDATA0, 0],[S.MXES, 0],[S.MXE0, 0],[S.MXE1, 0],
        [S.MXEI, 0],[S.MXE0, 1],[S.MXE1, 1],[S.MXEI, 1],[S.MXE0, 2],[S.MXE1, 2],[S.MXEI, 2],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        # TAB 1
        [S.TXEN5, 0],[S.TXDATA5, 0],[S.RXDATA5, 0],[S.SS7I77M4, 0],[S.RXDATA4, 0],[S.SS7I77M3, 0],[S.RXDATA3, 0],[S.MXES, 3],[S.MXE0, 3],[S.MXE1, 3],
        [S.MXEI, 3],[S.MXE0, 4],[S.MXE1, 4],[S.MXEI, 4],[S.MXE0, 5],[S.MXE1, 5],[S.MXEI, 5],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],],

    ["7i92-Internal Data", "7i92", "7i77_7i76", "7i92", "hm2_eth",
        6,3, 0,0, 0,3, 0,0, 5,2, 1,5, 0,0,0,0,0,0,0,0, 1, 34 , 33, 200, [2,1],
        # TAB 2
        [S.TXEN2, 0],[S.TXDATA2, 0],[S.RXDATA2, 0],[S.SS7I77M1, 0],[S.RXDATA1, 0],[S.SS7I77M0, 0],[S.RXDATA0, 0],[S.MXES, 0],[S.MXE0, 0],[S.MXE1, 0],
        [S.MXEI, 0],[S.MXE0, 1],[S.MXE1, 1],[S.MXEI, 1],[S.MXE0, 2],[S.MXE1, 2],[S.MXEI, 2],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        # TAB 1
        [S.STEPB,0],[S.STEPA,0],[S.STEPB,1],[S.STEPA,1],[S.STEPB,2],[S.STEPA,2],[S.STEPB,3],[S.STEPA,3],[S.STEPB,4],[S.STEPA,4],
        [S.SS7I76M3,0],[S.RXDATA3,0],[S.TXDATA4,0],[S.RXDATA4,0],[S.ENCI,0],[S.ENCB,0],[S.ENCA,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],],

    # 7i80HD #################
    ["7i80HD-Internal Data", "7i80", "SV12", "7i80HD", "hm2_eth",
        12,3, 0,0, 12,3, 0,0, 0,0, 0,0, [],0,0,0,0,0,0,0, 1, 72 , 33, 100, [2,3,4],
        [S.ENCB,1],[S.ENCA,1],[S.ENCB,0],[S.ENCA,0],[S.ENCI,1],[S.ENCI,0],[S.PWMP,1],[S.PWMP,0],[S.PWMD,1],[S.PWMD,0],[S.PWME,1],[S.PWME,0],
        [S.ENCB,3],[S.ENCA,3],[S.ENCB,2],[S.ENCA,2],[S.ENCI,3],[S.ENCI,2],[S.PWMP,3],[S.PWMP,2],[S.PWMD,3],[S.PWMD,2],[S.PWME,3],[S.PWME,2],
        [S.ENCB,5],[S.ENCA,5],[S.ENCB,4],[S.ENCA,4],[S.ENCI,5],[S.ENCI,4],[S.PWMP,5],[S.PWMP,4],[S.PWMD,5],[S.PWMD,4],[S.PWME,5],[S.PWME,4],
        [S.ENCB,7],[S.ENCA,7],[S.ENCB,6],[S.ENCA,6],[S.ENCI,7],[S.ENCI,6],[S.PWMP,7],[S.PWMP,6],[S.PWMD,7],[S.PWMD,6],[S.PWME,7],[S.PWME,6],
        [S.ENCB,9],[S.ENCA,9],[S.ENCB,8],[S.ENCA,8],[S.ENCI,9],[S.ENCI,8],[S.PWMP,9],[S.PWMP,8],[S.PWMD,9],[S.PWMD,8],[S.PWME,9],[S.PWME,8],
        [S.ENCB,11],[S.ENCA,11],[S.ENCB,10],[S.ENCA,10],[S.ENCI,11],[S.ENCI,10],[S.PWMP,11],[S.PWMP,10],[S.PWMD,11],[S.PWMD,10],[S.PWME,11],[S.PWME,10],],

    ["7i80HD-Internal Data", "7i80", "SVST8_4", "7i80HD", "hm2_eth",
      8,3, 0,0, 8,3, 0,0, 4,2, 0,0, [],0,0,0,0,0,0,0, 1, 72, 33, 100, [2,3,4],
      [S.ENCB,1],[S.ENCA,1],[S.ENCB,0],[S.ENCA,0],[S.ENCI,1],[S.ENCI,0],[S.PWMP,1],[S.PWMP,0],[S.PWMD,1],[S.PWMD,0],[S.PWME,1],[S.PWME,0],
      [S.ENCB,3],[S.ENCA,3],[S.ENCB,2],[S.ENCA,2],[S.ENCI,3],[S.ENCI,2],[S.PWMP,3],[S.PWMP,2],[S.PWMD,3],[S.PWMD,2],[S.PWME,3],[S.PWME,2],
      [S.ENCB,5],[S.ENCA,5],[S.ENCB,4],[S.ENCA,4],[S.ENCI,5],[S.ENCI,4],[S.PWMP,5],[S.PWMP,4],[S.PWMD,5],[S.PWMD,4],[S.PWME,5],[S.PWME,4],
      [S.ENCB,7],[S.ENCA,7],[S.ENCB,6],[S.ENCA,6],[S.ENCI,7],[S.ENCI,6],[S.PWMP,7],[S.PWMP,6],[S.PWMD,7],[S.PWMD,6],[S.PWME,7],[S.PWME,6],
      [S.STEPA,0],[S.STEPB,0],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0],[S.STEPA,1],[S.STEPB,1],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0],
      [S.STEPA,2],[S.STEPB,2],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0],[S.STEPA,3],[S.STEPB,3],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0],[S.GPIOI,0] ],
    ]

        #**************************
        # mesa daugther board data
        #**************************
        self._NUM_CHANNELS = 6
        self._SUBBOARDNAME = 0; self._SUBFIRMNAME = 1; self._SUBMODE = 2;self._SUBCONLIST = 3;self._SUBSTARTOFDATA = 12 # 4-10 spare for now.

        self.MESA_DAUGHTERDATA = [ ["8i20", "8i20", 0,[_("Axis Selection"),"Not Used","Not Used"], 0,0,0,0,0,0,0,0,
    [S.AMP8I20,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0] ],
["7i64", "7i64", 0,[_("7i64-Input\nP3 and P4"),_("7i64-Output\nP2 and P5"),_("7i64-Analog In")], 0,0,0,0,0,0,0,0,
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.ANALOGIN,0],[S.ANALOGIN,1],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0] ],
["7i69", "7i69", 0,[_("7i69\nP2"),_("7i69\nP3"),"Not Used"], 0,0,0,0,0,0,0,0,
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0] ],
["7i70", "7i70", 0,[_("7i70-Input\nTB3"),_("7i70-Input\nTB2"),"Not Used"], 0,0,0,0,0,0,0,0,
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0] ],
["7i71", "7i71", 0,[_("7i71-Output\nTB3"),_("7i71-Output\nTB2"),"Not Used"], 0,0,0,0,0,0,0,0,
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0] ],
["7i76", "7i76-m0", 0,[_("7i76-I/O\nTB6"),_("7i76-I/O\nTB5"),_("7i76-Analog Output\nTB4")], 0,0,0,0,0,0,0,0,
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
    [S.POTO,0],[S.POTE,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0] ],
["7i76", "7i76-m2", 0,[_("7i76-I/O\nTB6"),_("7i76-I/O\nTB5"),_("7i76-Analog Output\nTB4")], 0,0,0,0,0,0,0,0,
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.ENCB,0],[S.ENCA,0],[S.ENCB,1],[S.ENCA,1],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
    [S.POTO,0],[S.POTE,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0] ],
["7i77", "7i77-m0", 0,[_("7i77-I/O\nTB8"),_("7i77-I/O\nTB7"),_("7i77-Analog Output\nTB5")], 0,0,0,0,0,0,0,0,
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
    [S.PWME,0],[S.PWMP,0],[S.PWMD,0],[S.PWMP,1],[S.PWMD,1],[S.PWMP,2],[S.PWMD,2],[S.PWMP,3],[S.PWMD,3],[S.PWMP,4],[S.PWMD,4],[S.PWMP,5],
    [S.PWMD,5],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0] ],
["7i77", "7i77-m3", 0,[_("7i77-I/O\nTB8"),_("7i77-I/O\nTB7"),_("7i77-Analog Output\nTB5")], 0,0,0,0,0,0,0,0,
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.ENCB,0],[S.ENCA,0],[S.ENCB,1],[S.ENCA,1],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
    [S.PWME,0],[S.PWMP,0],[S.PWMD,0],[S.PWMP,1],[S.PWMD,1],[S.PWMP,2],[S.PWMD,2],[S.PWMP,3],[S.PWMD,3],[S.PWMP,4],[S.PWMD,4],[S.PWMP,5],
    [S.PWMD,5],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0] ],
["7i73", "7i73-m1", 0,[_("7i73-I/O\n"),"7i73-I/O\n ","7i73-Analog/Encoders\n "], 0,0,0,0,0,0,0,0,
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.NUSED,0],[S.NUSED,0],
        [S.ANALOGIN,0],[S.ANALOGIN,1],[S.ANALOGIN,2],[S.ANALOGIN,3],[S.ENCA,0],[S.ENCA,1],
        [S.ENCA,2],[S.ENCA,3],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
    [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0] ],
["7i84", "7i84-m0", 0,[_("7i84-I/O\nTB3"),_("7i84-I/O\nTB2"),"Not Used"], 0,0,0,0,0,0,0,0,
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0] ],
["7i84", "7i84-m3", 0,[_("7i84-I/O-MPG\nTB3"),_("7i84-I/O\nTB2"),"Not Used"], 0,0,0,0,0,0,0,0,
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.ENCB,0],[S.ENCA,0],[S.ENCB,1],[S.ENCA,1],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],
        [S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOI,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],[S.GPIOO,100],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],
        [S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0],[S.NUSED,0] ],
["error"]
 ]
        self.MESA_FIRMWAREDATA = []
        # internalname / displayed name / steptime/ step space / direction hold / direction setup
        self.alldrivertypes = [
                            ["gecko201", _("Gecko 201"), 500, 4000, 20000, 1000],
                            ["gecko202", _("Gecko 202"), 500, 4500, 20000, 1000],
                            ["gecko203v", _("Gecko 203v"), 1000, 2000, 200 , 200],
                            ["gecko210", _("Gecko 210"),  500, 4000, 20000, 1000],
                            ["gecko212", _("Gecko 212"),  500, 4000, 20000, 1000],
                            ["gecko320", _("Gecko 320"),  3500, 500, 200, 200],
                            ["gecko540", _("Gecko 540"),  1000, 2000, 200, 200],
                            ["l297", _("L297"), 500,  4000, 4000, 1000],
                            ["pmdx150", _("PMDX-150"), 1000, 2000, 1000, 1000],
                            ["sherline", _("Sherline"), 22000, 22000, 100000, 100000],
                            ["xylotex", _("Xylotex 8S-3"), 2000, 1000, 200, 200],
                            ["oem750", _("Parker-Compumotor oem750"), 1000, 1000, 1000, 200000],
                            ["jvlsmd41", _("JVL-SMD41 or 42"), 500, 500, 2500, 2500],
                            ["hobbycnc", _("Hobbycnc Pro Chopper"), 2000, 2000, 2000, 2000],
                            ["keling", _("Keling 4030"), 5000, 5000, 20000, 20000],
                            ]

        self.MESA_BOARDNAMES = []

        self.MESS_START = _('Start')
        self.MESS_FWD = _('Forward')
        self.MESS_DONE = _('Done')
        self.MESS_CL_REWRITE =_("OK to replace existing custom ladder program?\nExisting Custom.clp will be renamed custom_backup.clp.\nAny existing file named -custom_backup.clp- will be lost. ")
        self.MESS_CL_EDITED = _("You edited a ladder program and have selected a different program to copy to your configuration file.\nThe edited program will be lost.\n\nAre you sure?  ")
        self.MESS_NO_ESTOP = _("You need to designate an E-stop input pin in the Parallel Port Setup page for this program.")
        self.MESS_PYVCP_REWRITE =_("OK to replace existing custom pyvcp panel and custom_postgui.hal file ?\nExisting custompanel.xml and custom_postgui.hal will be renamed custompanel_backup.xml and postgui_backup.hal.\nAny existing file named custompanel_backup.xml and custom_postgui.hal will be lost. ")
        self.MESS_ABORT = _("Quit PNCconf and discard changes?")
        self.MESS_QUIT = _("The configuration has been built and saved.\nDo you want to quit?")
        self.MESS_NO_REALTIME = _("You are using a simulated-realtime version of LinuxCNC, so testing / tuning of hardware is unavailable.")
        self.MESS_KERNEL_WRONG = _("You are using a realtime version of LinuxCNC but didn't load a realtime kernel so testing / tuning of hardware is\
                 unavailable.\nThis is possibly because you updated the OS and it doesn't automatically load the RTAI kernel anymore.\n"+
            "You are using the  %(actual)s  kernel.\nYou need to use kernel:")% {'actual':os.uname()[2]}

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)
