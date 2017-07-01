#!/usr/bin/env python
#
#    This is stepconf, a graphical configuration editor for LinuxCNC
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
# GUI PAGE
#************
import os
from stepconf.definitions import *

def gui_page_prepare(self):
	self.w.gui_pyvcp.set_active(self.d.pyvcp)
	self.on_gui_pyvcp_toggled()
	if  not self.w.createconfig.get_active():
		if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/custompanel.xml" % self.d.machinename)):
			self.w.gui_rdo_custom_pyvcp.set_active(True)
	self.w.gui_select_axis.set_active(self.d.select_axis)
	self.w.gui_select_gmoccapy.set_active(self.d.select_gmoccapy)
	self.w.ladderconnect.set_active(self.d.ladderconnect)
	# Gladevcp
	self.w.gui_gladevcp.set_active(self.d.gladevcp)
	self.w.spindlespeedbar.set_active(self.d.spindlespeedbar)
	self.w.spindleatspeed.set_active(self.d.spindleatspeed)
	self.w.gladevcpforcemax.set_active(self.d.gladevcpforcemax )
	self.w.centerembededgvcp.set_active(self.d.centerembededgvcp)
	self.w.sideembededgvcp.set_active(self.d.sideembededgvcp)
	self.w.standalonegvcp.set_active(self.d.standalonegvcp)
	self.w.gladevcpposition.set_active(self.d.gladevcpposition)
	self.w.gladevcpsize.set_active(self.d.gladevcpsize )
	self.w.autotouchz.set_active(self.d.autotouchz)
	# Get max speed (speed / minutes)
	if self.d.axes == 0: maxspeed = max(self.d.xmaxvel, self.d.ymaxvel, self.d.zmaxvel)* 60 # X Y Z
	elif self.d.axes == 1: maxspeed = max(self.d.xmaxvel, self.d.ymaxvel, self.d.zmaxvel)* 60 # X Y Z A (A is too fast, excluded)
	elif self.d.axes == 2: maxspeed = max(self.d.xmaxvel, self.d.zmaxvel)* 60 # X Z
	elif self.d.axes == 3: maxspeed = max(self.d.xmaxvel, self.d.ymaxvel, self.d.umaxvel, self.d.vmaxvel)* 60 # X Y U V
	self.general_maxspeed = maxspeed
	
	"""
	if os.path.exists(THEMEDIR):
		self.get_installed_themes()
	"""

def gui_page_finish(self):
	self.d.select_axis = self.w.gui_select_axis.get_active()
	self.d.select_gmoccapy = self.w.gui_select_gmoccapy.get_active()
	self.d.pyvcp = self.w.gui_pyvcp.get_active()
	if self.d.pyvcp == True:
		if self.w.gui_rdo_default_pyvcp.get_active() == True:
			self.pyvcptype = 0 # default pyvcp
			self.d.pyvcpname = "pyvcp_default.xml" # name for [DISPLAY] section in INI file
			self.d.pyvcphaltype = 1
			# Create panel
			#self.create_pyvcp_panel("pyvcp_test.xml")
			# Connection to hal
			self.create_pyvcp_hal()
			self.d.halui = 1
		if self.w.gui_rdo_custom_pyvcp.get_active() == True:
			self.pyvcptype = 1 # custom pyvcp
			self.d.pyvcpname = "pyvcp_custom.xml" # name for [DISPLAY] section in INI file
		"""
		for i in self.d.halui_list:
			print(i)
		for i in self.d.hal_postgui_list:
			print(i)
		"""
		"""
		else:
			if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/custompanel.xml" % self.d.machinename)):
				if not self.a.warning_dialog(MESS_PYVCP_REWRITE,False):
					return True
		"""
	# Gladevcp
	self.d.gladevcp = self.w.gui_gladevcp.get_active()        
	if self.d.gladevcp == True:
		if self.w.gui_rdo_default_gladevcp.get_active() == True:
			self.d.gladevcpname = "glade_default.ui"
		if self.w.gui_rdo_custom_galdevcp.get_active() == True:
			self.d.gladevcpname = "glade_custom.ui"
		else:
			if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/glade_custom.ui" % self.d.machinename)):
				if not self.a.warning_dialog(MESS_GLADEVCP_REWRITE,False):
					return True
	
	self.d.spindlespeedbar = self.w.spindlespeedbar.get_active()
	self.d.spindleatspeed = self.w.spindleatspeed.get_active()
	self.d.gladevcpforcemax = self.w.gladevcpforcemax.get_active()
	self.d.centerembededgvcp = self.w.centerembededgvcp.get_active()
	self.d.sideembededgvcp = self.w.sideembededgvcp.get_active()
	self.d.standalonegvcp = self.w.standalonegvcp.get_active()
	self.d.gladevcpposition = self.w.gladevcpposition.get_active()
	self.d.gladevcpsize = self.w.gladevcpsize.get_active()
	self.d.autotouchz = self.w.autotouchz.get_active()
	
	self.d.maxspeeddisplay = self.w.maxspeeddisplay.get_value()
	self.d.gladevcpwidth = self.w.gladevcpwidth.get_value()
	self.d.gladevcpheight = self.w.gladevcpheight.get_value()
	self.d.gladevcpxpos = self.w.gladevcpxpos.get_value()
	self.d.gladevcpypos = self.w.gladevcpypos.get_value()   
	
	# set HALUI commands based on the user requested glade buttons
	hal_zerox = "G10 L20 P0 X0 ( Set X to zero )"
	hal_zeroy = "G10 L20 P0 Y0 ( Set Y to zero )"
	hal_zeroz = "G10 L20 P0 Z0 ( Set Z to zero )"
	hal_zeroa = "G10 L20 P0 A0 ( Set A to zero )"
	
	self.d.zerox = self.w.zerox.get_active()
	self.d.zeroy = self.w.zeroy.get_active()
	self.d.zeroz = self.w.zeroz.get_active()
	self.d.zeroa = self.w.zeroa.get_active()
	
	if  self.d.zerox:
		self.d.halui_list.append(hal_zerox)
	if  self.d.zeroy:
		self.d.halui_list.append(hal_zeroy)
	if  self.d.zeroz:
		self.d.halui_list.append(hal_zeroz)
	if  self.d.zeroa:
		self.d.halui_list.append(hal_zeroa)

	if(self.d.zerox or self.d.zeroy or self.d.zeroz or self.d.zeroa):
		self.d.halui = 1
		
	#self.d.gladevcptheme = self.w.gladevcptheme.get_active_text()
	# make sure there is a copy of the choosen gladevcp panel in /tmp/
	# We will copy it later into our config folder
	self.create_gladevcp_panel(self)
	if self.w.autotouchz.get_active():
		self.d.classicladder = True
		if not self.w.ladderexist.get_active():
			self.w.laddertouchz.set_active(True)

def create_pyvcp_hal(self):
	"""
	net my-probe pyvcp.probe => halui.mdi-command-10
	"""

	"""
	MDI_COMMAND = O<100> call
	"""
	self.d.halui_list = []
	self.d.hal_postgui_list = []
	
	# X
	self.d.hal_postgui_list.append("# connect the X PyVCP buttons")
	self.d.hal_postgui_list.append("net my-jogxminus halui.axis.x.minus <= pyvcp.x-minus")
	self.d.hal_postgui_list.append("net my-jogxplus halui.axis.x.plus <= pyvcp.x-plus")
	self.d.halui_list.append("G54 G10 L20 P1 x0 M100")
	self.d.hal_postgui_list.append("net my-x-null pyvcp.x-null => halui.mdi-command-%02d" % (len(self.d.halui_list)-1) )
	self.d.halui_list.append("G0 x0 F1000")
	self.d.hal_postgui_list.append("net my-x-zero pyvcp.x-zero => halui.mdi-command-%02d" % (len(self.d.halui_list)-1) )
	
	# Y
	if self.d.axes in(0, 1, 3):
		self.d.hal_postgui_list.append("# connect the Y PyVCP buttons")
		self.d.hal_postgui_list.append("net my-jogyminus halui.axis.y.minus <= pyvcp.y-minus")
		self.d.hal_postgui_list.append("net my-jogyplus halui.axis.y.plus <= pyvcp.y-plus")
		self.d.halui_list.append("G54 G10 L20 P1 y0 M100")
		self.d.hal_postgui_list.append("net my-y-null pyvcp.y-null => halui.mdi-command-%02d" % (len(self.d.halui_list)-1) )
		self.d.halui_list.append("G0 y0 F1000")
		self.d.hal_postgui_list.append("net my-y-zero pyvcp.y-zero => halui.mdi-command-%02d" % (len(self.d.halui_list)-1) )
	# Z
	if self.d.axes in(0, 1, 2):
		self.d.hal_postgui_list.append("# connect the Z PyVCP buttons")
		self.d.hal_postgui_list.append("net my-jogzminus halui.axis.z.minus <= pyvcp.z-minus")
		self.d.hal_postgui_list.append("net my-jogzplus halui.axis.z.plus <= pyvcp.z-plus")
		self.d.halui_list.append("G54 G10 L20 P1 z0 M100")
		self.d.hal_postgui_list.append("net my-z-null pyvcp.z-null => halui.mdi-command-%02d" % (len(self.d.halui_list)-1) )
		self.d.halui_list.append("G0 z0 F400")
		self.d.hal_postgui_list.append("net my-z-zero pyvcp.z-zero => halui.mdi-command-%02d" % (len(self.d.halui_list)-1) )
	# A
	if self.d.axes == 1:
		self.d.hal_postgui_list.append("# connect the A PyVCP buttons")
		self.d.hal_postgui_list.append("net my-jogzminus halui.axis.a.minus <= pyvcp.a-minus")
		self.d.hal_postgui_list.append("net my-jogzplus halui.axis.a.plus <= pyvcp.a-plus")
		self.d.halui_list.append("G54 G10 L20 P1 a0 M100")
		self.d.hal_postgui_list.append("net my-a-null pyvcp.a-null => halui.mdi-command-%02d" % (len(self.d.halui_list)-1) )
		self.d.halui_list.append("G0 a0 F1000")
		self.d.hal_postgui_list.append("net my-a-zero pyvcp.a-zero => halui.mdi-command-%02d" % (len(self.d.halui_list)-1) )
	# UV
	if self.d.axes == 3:
		self.d.hal_postgui_list.append("# connect the U PyVCP buttons")
		self.d.hal_postgui_list.append("net my-jogzminus halui.axis.u.minus <= pyvcp.u-minus")
		self.d.hal_postgui_list.append("net my-jogzplus halui.axis.u.plus <= pyvcp.u-plus")
		self.d.halui_list.append("G54 G10 L20 P1 u0 M100")
		self.d.hal_postgui_list.append("net my-u-null pyvcp.u-null => halui.mdi-command-%02d" % (len(self.d.halui_list)-1) )
		self.d.halui_list.append("G0 u0 F1000")
		self.d.hal_postgui_list.append("net my-u-zero pyvcp.u-zero => halui.mdi-command-%02d" % (len(self.d.halui_list)-1) )

		self.d.hal_postgui_list.append("# connect the V PyVCP buttons")
		self.d.hal_postgui_list.append("net my-jogzminus halui.axis.v.minus <= pyvcp.v-minus")
		self.d.hal_postgui_list.append("net my-jogzplus halui.axis.v.plus <= pyvcp.v-plus")
		self.d.halui_list.append("G54 G10 L20 P1 v0 M100")
		self.d.hal_postgui_list.append("net my-v-null pyvcp.v-null => halui.mdi-command-%02d" % (len(self.d.halui_list)-1) )
		self.d.halui_list.append("G0 v0 F1000")
		self.d.hal_postgui_list.append("net my-v-zero pyvcp.v-zero => halui.mdi-command-%02d" % (len(self.d.halui_list)-1) )

	# JOG
	self.d.hal_postgui_list.append("# connect the PyVCP jog speed slider")
	self.d.hal_postgui_list.append("net my-jogspeed halui.axis.jog-speed <= pyvcp.jog-speed-f")

	# Spindle speed
	if (self.a.has_spindle_speed_control() or self.a.has_spindle_encoder()):
		inputs = self.a.build_input_set()
		encoder = d_hal_input[PHA] in inputs
		if encoder:
			self.d.hal_postgui_list.append("# **** Setup of spindle speed display using pyvcp -START ****")
			self.d.hal_postgui_list.append("# **** Use ACTUAL spindle velocity from spindle encoder")
			self.d.hal_postgui_list.append("# **** spindle-velocity-feedback-rps bounces around so we filter it with lowpass")
			self.d.hal_postgui_list.append("# **** spindle-velocity-feedback-rps is signed so we use absolute component to remove sign") 
			self.d.hal_postgui_list.append("# **** ACTUAL velocity is in RPS not RPM so we scale it.")
			self.d.hal_postgui_list.append("")
			self.d.hal_postgui_list.append("setp scale.0.gain 60")
			self.d.hal_postgui_list.append("setp lowpass.0.gain %f")% self.d.spindlefiltergain
			self.d.hal_postgui_list.append("net spindle-velocity-feedback-rps               => lowpass.0.in")
			self.d.hal_postgui_list.append("net spindle-fb-filtered-rps      lowpass.0.out  => abs.0.in")
			self.d.hal_postgui_list.append("net spindle-fb-filtered-abs-rps  abs.0.out      => scale.0.in")
			self.d.hal_postgui_list.append("net spindle-fb-filtered-abs-rpm  scale.0.out    => pyvcp.spindle-speed")
			self.d.hal_postgui_list.append("")
			self.d.hal_postgui_list.append("# **** set up spindle at speed indicator ****")
			if self.d.usespindleatspeed:
				self.d.hal_postgui_list.append("")
				self.d.hal_postgui_list.append("net spindle-cmd-rps-abs             =>  near.0.in1")
				self.d.hal_postgui_list.append("net spindle-velocity-feedback-rps   =>  near.0.in2")
				self.d.hal_postgui_list.append("net spindle-at-speed                <=  near.0.out")
				self.d.hal_postgui_list.append("setp near.0.scale %f")% self.d.spindlenearscale
			else:
				self.d.hal_postgui_list.append("# **** force spindle at speed indicator true because we chose no feedback ****")
				self.d.hal_postgui_list.append("")
				self.d.hal_postgui_list.append("sets spindle-at-speed true")
			self.d.hal_postgui_list.append("net spindle-at-speed       => pyvcp.spindle-at-speed-led")
		else:
			self.d.hal_postgui_list.append("# **** Use COMMANDED spindle velocity from LinuxCNC because no spindle encoder was specified")
			self.d.hal_postgui_list.append("")
			self.d.hal_postgui_list.append("net spindle-cmd-rpm-abs    => pyvcp.spindle-speed")
			self.d.hal_postgui_list.append("")
			self.d.hal_postgui_list.append("# **** force spindle at speed indicator true because we have no feedback ****")
			self.d.hal_postgui_list.append("")
			self.d.hal_postgui_list.append("net spindle-at-speed => pyvcp.spindle-at-speed-led")
			self.d.hal_postgui_list.append("sets spindle-at-speed true")

	# TOOL LENGHT SENSOR
	inputs = self.a.build_input_set()
	if (d_hal_input[PROBE] in inputs):
		self.d.halui_list.append("G30.1")
		self.d.hal_postgui_list.append("net my-set-sensor-position pyvcp.set-sensor-position => halui.mdi-command-%02d" % (len(self.d.halui_list)-1) )
		self.d.halui_list.append("G30")
		self.d.hal_postgui_list.append("net my-goto-sensor-position pyvcp.goto-sensor-position => halui.mdi-command-%02d" % (len(self.d.halui_list)-1) )
		self.d.halui_list.append("o<pyvcp_probe> call")
		self.d.hal_postgui_list.append("net my-probe pyvcp.probe => halui.mdi-command-%02d" % (len(self.d.halui_list)-1) )

def on_show_gladepvc_clicked(self,*args):
	self.test_glade_panel(self)

def on_show_pyvcp_clicked(self,*args):
	self.testpanel(self)

def on_gui_gladevcp_toggled(self,*args):
	# TODO
	i= self.w.gui_gladevcp.get_active()
	if  self.w.createconfig.get_active():
		self.w.gui_rdo_custom_galdevcp.set_sensitive(False)
	else:
		self.w.gui_rdo_custom_galdevcp.set_sensitive(i)
	self.w.gui_gladevcp_box.set_sensitive(i) 

def on_gui_pyvcp_toggled(self,*args):
	i= self.w.gui_pyvcp.get_active()
	if  self.w.createconfig.get_active():
		self.w.gui_rdo_custom_pyvcp.set_sensitive(False)
	else:
		self.w.gui_rdo_custom_pyvcp.set_sensitive(i)
	self.w.gui_pyvcp_box.set_sensitive(i)

"""
def get_installed_themes(self):
    data1 = self.d.gladevcptheme
    data2 = prefs.getpref('gtk_theme', 'Follow System Theme', str)
    model = self.widgets.themestore
    model.clear()
    model.append(("Follow System Theme",))
    temp1 = temp2 = 0
    names = os.listdir(_PD.THEMEDIR)
    names.sort()
    for search,dirs in enumerate(names):
        model.append((dirs,))
        if dirs  == data1:
            temp1 = search+1
        if dirs  == data2:
            temp2 = search+1
    self.widgets.gladevcptheme.set_active(temp1)
    self.widgets.touchytheme.set_active(temp2)
"""

#***************
# PYVCP TEST
#***************
def testpanel(self,w):
	panelname = os.path.join(self.a.distdir, "configurable_options/pyvcp")
	if self.w.gui_rdo_default_pyvcp.get_active() == True:
		panel = "pyvcp_test.xml"
		# Prepare file
		self.create_pyvcp_panel("pyvcp_test.xml")
		folder = "/tmp"
		halrun = os.popen("cd %s\nhalrun -Is > /dev/null"%(folder), "w" )
		if self.a.debug:
			halrun.write("echo\n")
		halrun.write("loadusr -Wn displaytest pyvcp -c displaytest %(panel)s\n" %{'panel':panel,})
	if self.w.gui_rdo_custom_pyvcp.get_active() == True:
		panel = "custompanel.xml"
		panelname = os.path.expanduser("~/linuxcnc/configs/%s" % self.d.machinename)
		halrun = os.popen("cd %(panelname)s\nhalrun -Is > /dev/null"% {'panelname':panelname,}, "w" )    
		halrun.write("loadusr -Wn displaytest pyvcp -c displaytest %(panel)s\n" %{'panel':panel,})
	#if self.w.gui_rdo_pyvcp_spindle.get_active() == True:
	#    halrun.write("setp displaytest.spindle-speed 1000\n")
	halrun.write("waitusr displaytest\n")
	halrun.flush()
	halrun.close()

def create_pyvcp_panel(self, filename):
	directory = "/tmp/"
	filename = os.path.join(directory, filename)
	file = open(filename, "w")
	"""
	if self.d.axes == 0:
		print("# X Y Z")
	elif self.d.axes == 1:
		print("# X Y Z A")
	elif self.d.axes == 2:
		print("# X Z")
	elif self.d.axes == 3:
		print("# X Y U V")
	else:
		print "___________________unknown self.d.axes",self.d.axes
	"""
	print >>file, ("""<?xml version="1.0"?>
<pyvcp>
<labelframe text="Jog Buttons">
	<font>("Helvetica",16)</font>
	<!-- the X jog buttons -->
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"x-plus"</halpin>
		<text>"X+"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"x-minus"</halpin>
		<text>"X-"</text>
		</button>
	</hbox>
""")
	if self.d.axes in(0, 1, 3):
		print >>file, ("""
	<!-- the Y jog buttons -->
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"y-plus"</halpin>
		<text>"Y+"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"y-minus"</halpin>
		<text>"Y-"</text>
		</button>
	</hbox>
""")
	if self.d.axes in(0, 1, 2):
		print >>file, ("""
	<!-- the Z jog buttons -->
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"z-plus"</halpin>
		<text>"Z+"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"z-minus"</halpin>
		<text>"Z-"</text>
		</button>
	</hbox>
""")
	if self.d.axes == 1:
		print >>file, ("""
	<!-- the A jog buttons -->
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"a-plus"</halpin>
		<text>"A+"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"a-minus"</halpin>
		<text>"A-"</text>
		</button>
	</hbox>
""")
	if self.d.axes == 3:
		print >>file, ("""
	<!-- the U jog buttons -->
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"u-plus"</halpin>
		<text>"U+"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"u-minus"</halpin>
		<text>"U-"</text>
		</button>
	</hbox>
	<!-- the V jog buttons -->
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"v-plus"</halpin>
		<text>"V+"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>3</width>
		<halpin>"v-minus"</halpin>
		<text>"V-"</text>
		</button>
	</hbox>
""")
	print >>file, ("""
	<!-- the jog speed slider -->
	<vbox>
		<relief>RAISED</relief>
		<bd>3</bd>
		<label>
		<text>"Jog Speed"</text>
		<font>("Helvetica",16)</font>
		</label>
		<scale>
		<font>("Helvetica",14)</font>
		<halpin>"jog-speed"</halpin>
		<resolution>1</resolution>
		<orient>HORIZONTAL</orient>
		<min_>0</min_>
""")
	print >>file, ("		<max_>%s</max_>" % self.general_maxspeed)
	print >>file, ("""
		</scale>
	</vbox>
""")
	print >>file, ("""
</labelframe>
<!-- nullpunkt -->
<labelframe text="G54 set zero">
	<font>("Helvetica",16)</font>
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"x-null"</halpin>
		<text>"X=0"</text>
		</button>
""")
	if self.d.axes in(0, 1, 3):
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"y-null"</halpin>
		<text>"Y=0"</text>
		</button>
""")
	if self.d.axes in(0, 1, 2):
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"z-null"</halpin>
		<text>"Z=0"</text>
		</button>
""")
	if self.d.axes == 1:
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"a-null"</halpin>
		<text>"A=0"</text>
		</button>
""")
	if self.d.axes == 3:
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"u-null"</halpin>
		<text>"U=0"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"v-null"</halpin>
		<text>"V=0"</text>
		</button>
""")
	print >>file, ("""
	</hbox>
</labelframe>
<!-- Go to zero -->
<labelframe text="Go to zero">
	<font>("Helvetica",16)</font>
	<hbox>
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"x-zero"</halpin>
		<text>"-X-"</text>
		</button>
""")
	if self.d.axes in(0, 1, 3):
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"y-zero"</halpin>
		<text>"-Y-"</text>
		</button>
""")
	if self.d.axes in(0, 1, 2):
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"z-zero"</halpin>
		<text>"-Z-"</text>
		</button>
""")
	if self.d.axes == 1:
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"a-zero"</halpin>
		<text>"-A-"</text>
		</button>
""")
	if self.d.axes == 3:
		print >>file, ("""
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"u-zero"</halpin>
		<text>"-U-"</text>
		</button>
		<button>
		<font>("Helvetica",20)</font>
		<width>2</width>
		<halpin>"v-zero"</halpin>
		<text>"-V-"</text>
		</button>
""")
	print >>file, ("""
	</hbox>
</labelframe>
""")
	# Check for spindle speed
	if (self.a.has_spindle_speed_control() or self.a.has_spindle_encoder()):
		print >>file, ("""
<vbox>
<relief>RIDGE</relief>
<bd>6</bd>
	<label>
			<text>"Spindle Speed:"</text>
			<font>("Helvetica",20)</font>
	</label>
	<bar>
			<halpin>"spindle-speed"</halpin>
			<max_>3000</max_>
	</bar>
	<label>
			<text>"Spindle-At-Speed:"</text>
			<font>("Helvetica",20)</font>
	</label>
	<hbox>
		<label>
				<text>"             "</text>
				<font>("Helvetica",20)</font>
		</label>
		<led>
			<halpin>"spindle-at-speed-led"</halpin>
			<size>30</size>
			<on_color>"green"</on_color>
			<off_color>"red"</off_color>
		</led>
	</hbox>
</vbox>
""")

	# Check for tool lenght sensor
	inputs = self.a.build_input_set()
	if (d_hal_input[PROBE] in inputs):
			print >>file, ("""
<!-- Tool length sensor -->
<labelframe text="Tool length sensor">
<font>("Helvetica",16)</font>
	<hbox>
		<button>
		<!-- G30.1 -->
		<font>("Helvetica",12)</font>
		<width>8</width>
		<halpin>"set-sensor-position"</halpin>
		<text>"Set position"</text>
		</button>
		<button>
		<!-- G30 -->
		<font>("Helvetica",12)</font>
		<width>8</width>
		<halpin>"goto-sensor-position"</halpin>
		<text>"Go to position"</text>
		</button>
	</hbox>
	<hbox>
		<button>
		<font>("Helvetica",12)</font>
		<width>8</width>
		<halpin>"probe"</halpin>
		<text>"Probe"</text>
		</button>
	</hbox>
</labelframe>
""")
	print >>file, ("""
</pyvcp>
""")
	file.close()

#***************
# GLADEVCP TEST
#***************
def test_glade_panel(self,w):
	panelname = os.path.join(self.a.distdir, "configurable_options/gladevcp")
	if self.w.gui_rdo_default_gladevcp.get_active() == True:
		self.create_gladevcp_panel(w)
		self.display_gladevcp_panel()
	if self.w.gui_rdo_custom_galdevcp.get_active() == True:
		None


def display_gladevcp_panel(self):
	pos = "+0+0"
	size = "200x200"
	options = ""
	folder = "/tmp"
	if not self.w.createconfig.get_active():
		folder = os.path.expanduser("~/linuxcnc/configs/%s" % self.d.machinename)
		if not os.path.exists(folder + "/gvcp-panel.ui"):
			self.a.warning_dialog (_("""You specified there is an existing gladefile, \
But there is not one in the machine-named folder.."""),True)
			return
	self.create_gladevcp_panel(self)
	if self.w.gladevcpposition.get_active() == True:
		xpos = self.w.gladevcpxpos.get_value()
		ypos = self.w.gladevcpypos.get_value()
		pos = "+%d+%d"% (xpos,ypos)
	if self.w.gladevcpsize.get_active() == True:
		width = self.w.gladevcpwidth.get_value()
		height = self.w.gladevcpheight.get_value()
		size = "%dx%d"% (width,height)
	"""
	if not self.w.gladevcptheme.get_active_text() == "Follow System Theme":
		options ="-t %s"% (self.w.gladevcptheme.get_active_text())
		print options
	"""
	halrun = os.popen("cd %s\nhalrun -Is > /dev/null"%(folder), "w" )
	if self.a.debug:
		halrun.write("echo\n")
	halrun.write("loadusr -Wn displaytest gladevcp -g %(size)s%(pos)s -c displaytest %(option)s gvcp-panel.ui\n" %{
					'size':size,'pos':pos,'option':options})
	if self.w.spindlespeedbar.get_active():
		halrun.write("setp displaytest.spindle-speed 500\n")
	if self.w.zerox.get_active():
		halrun.write("setp displaytest.zero-x-active true\n")
	if self.w.zeroy.get_active():
		halrun.write("setp displaytest.zero-y-active true\n")
	if self.w.zeroz.get_active():
		halrun.write("setp displaytest.zero-z-active true\n")
	if self.w.zeroa.get_active():
		halrun.write("setp displaytest.zero-a-active true\n")
	if self.w.autotouchz.get_active():
		halrun.write("setp displaytest.auto-touch-z-active true\n")
	if self.w.spindleatspeed.get_active():
		halrun.write("setp displaytest.spindle-at-speed-led true\n")
	halrun.write("setp displaytest.button-box-active true\n")
	halrun.write("waitusr displaytest\n")
	halrun.flush()
	halrun.close()

def create_gladevcp_panel(self,w):
	directory = "/tmp/"
	filename = os.path.join(directory, "gvcp-panel.ui")
	file = open(filename, "w")
	print >>file, ("""<?xml version="1.0"?>
<interface>
	<!-- interface-requires gladevcp 0.0 -->
	<requires lib="gtk+" version="2.16"/>
	<!-- interface-naming-policy project-wide -->
	<object class="GtkWindow" id="window1">
	<property name="width_request">100</property>
		<child>
		  <object class="GtkVBox" id="vbox1">
			<property name="visible">True</property>""")
	#if self.w.spindlespeedbar.get_active():
	#	print >>file, ("""
	# Check for spindle speed
	if (self.a.has_spindle_speed_control() or self.a.has_spindle_encoder()):
		print >>file, ("""
		<child>
		  <object class="HAL_HBar" id="spindle-speed">
			<property name="visible">True</property>
			<property name="force_height">36</property>""")
		print >>file, ("""<property name="max">%(maxrpm)d</property>"""%{'maxrpm':self.general_maxspeed })
		print >>file, ("""
			<property name="z0_color">#0000ffff0000</property>
			<property name="value">44.25</property>
			<property name="z1_color">#ffffffff0000</property>
			<property name="bg_color">#bebebebebebe</property>
			<property name="text_template">Spindle: % 4d RPM</property>
			<property name="z0_border">0.94999998807907104</property>
			<property name="z2_color">#ffff00000000</property>
			<property name="show_limits">False</property>
		  </object>
		  <packing>
			<property name="expand">False</property>
			<property name="position">0</property>
		  </packing>
		</child>""" )
	#if self.w.spindleatspeed.get_active():
	#	print >>file, ("""
	# Check for spindle speed
	if (self.a.has_spindle_speed_control() or self.a.has_spindle_encoder()):
		print >>file, ("""
		<child>
		  <object class="GtkHBox" id="hbox1">
			<property name="visible">True</property>
			<child>
			  <object class="GtkLabel" id="label1">
				<property name="visible">True</property>
				<property name="ypad">5</property>
				<property name="label" translatable="yes"> Spindle Up To Speed </property>
			  </object>
			  <packing>
				<property name="expand">False</property>
				<property name="fill">False</property>
				<property name="position">0</property>
			  </packing>
			</child>
			<child>
			  <object class="HAL_LED" id="spindle-at-speed-led">
				<property name="visible">True</property>
				<property name="led_shape">2</property>
				<property name="on_color">green</property>
				<property name="led_size">5</property>
			  </object>
			  <packing>
				<property name="expand">False</property>
				<property name="fill">False</property>
				<property name="padding">10</property>
				<property name="position">1</property>
			  </packing>
			</child>
		  </object>
		  <packing>
			<property name="expand">False</property>
			<property name="position">1</property>
		  </packing>
		</child>""")
	print >>file, ("""
		<child>
		  <object class="HAL_Table" id="button-box-active">
			<property name="visible">True</property>
			<property name="n_rows">5</property>
			<property name="homogeneous">False</property>""")
	#if self.w.zerox.get_active():
	#	print >>file, ("""
	print >>file, ("""
			<child>
			  <object class="HAL_HBox" id="zero-x-active">
				<property name="visible">True</property>
				<child>
				  <object class="HAL_Button" id="zero-x">
					<property name="label" translatable="yes">Zero X</property>
					<property name="visible">True</property>
					<property name="can_focus">True</property>
					<property name="receives_default">True</property>
				  </object>
				  <packing>
					<property name="position">0</property>
				  </packing>
				</child>
			  </object>
			</child>""")

	#if self.w.zeroy.get_active():
	#	print >>file, ("""
	if self.d.axes in(0, 1, 3):
		print >>file, ("""
			<child>
			  <object class="HAL_HBox" id="zero-y-active">
				<property name="visible">True</property>
				<child>
				  <object class="HAL_Button" id="zero-y">
					<property name="label" translatable="yes">Zero Y</property>
					<property name="visible">True</property>
					<property name="can_focus">True</property>
					<property name="receives_default">True</property>
				  </object>
				  <packing>
					<property name="position">0</property>
				  </packing>
				</child>
			  </object>
			  <packing>
				<property name="top_attach">1</property>
				<property name="bottom_attach">2</property>
			  </packing>
			</child>""")
	#if self.w.zeroz.get_active():
	#	print >>file, ("""
	if self.d.axes in(0, 1, 2):
		print >>file, ("""
			<child>
			  <object class="HAL_HBox" id="zero-z-active">
				<property name="visible">True</property>
				<child>
				  <object class="HAL_Button" id="zero-z">
					<property name="label" translatable="yes">Zero Z</property>
					<property name="visible">True</property>
					<property name="can_focus">True</property>
					<property name="receives_default">True</property>
				  </object>
				  <packing>
					<property name="position">0</property>
				  </packing>
				</child>
			  </object>
			  <packing>
				<property name="top_attach">2</property>
				<property name="bottom_attach">3</property>
			  </packing>
			</child>""")
	#if self.w.zeroa.get_active():
	#	print >>file, ("""
	if self.d.axes == 1:
		print >>file, ("""
			<child>
			  <object class="HAL_HBox" id="zero-a-active">
				<property name="visible">True</property>
				<child>
				  <object class="HAL_Button" id="zero-a">
					<property name="label" translatable="yes">Zero A</property>
					<property name="visible">True</property>
					<property name="can_focus">True</property>
					<property name="receives_default">True</property>
				  </object>
				  <packing>
					<property name="position">0</property>
				  </packing>
				</child>
			  </object>
			  <packing>
				<property name="top_attach">3</property>
				<property name="bottom_attach">4</property>
			  </packing>
			</child>""")
	if self.w.autotouchz.get_active():
		print >>file, ("""
			<child>
			  <object class="HAL_HBox" id="auto-touch-z-active">
				<property name="visible">True</property>
				<child>
				  <object class="HAL_Button" id="auto-touch-z">
					<property name="label" translatable="yes">Z  Auto Touch Off</property>
					<property name="visible">True</property>
					<property name="can_focus">True</property>
					<property name="receives_default">True</property>
					<property name="yalign">0.56000000238418579</property>
				  </object>
				  <packing>
					<property name="position">0</property>
				  </packing>
				</child>
			  </object>
			  <packing>
				<property name="top_attach">4</property>
				<property name="bottom_attach">5</property>
			  </packing>
			</child>""")
	print >>file, ("""
		  </object>
		  <packing>
			<property name="expand">False</property>
			<property name="fill">False</property>
			<property name="position">2</property>
		  </packing>
		</child>
	  </object>
	</child>
	</object>
</interface>""")
	file.close()
