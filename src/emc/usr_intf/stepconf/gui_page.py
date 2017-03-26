#!/usr/bin/env python
#
#    This is stepconf, a graphical configuration editor for LinuxCNC
#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
#    stepconf 1.1 revamped by Chris Morley 2014
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
			self.w.gui_rdo_pyvcp_custom.set_active(True)
	self.w.gui_select_axis.set_active(self.d.select_axis)
	self.w.gui_select_gmoccapy.set_active(self.d.select_gmoccapy)
	self.w.ladderconnect.set_active(self.d.ladderconnect)
	self.w.gui_pyvcpconnect.set_active(self.d.pyvcpconnect)

def gui_page_finish(self):
	SIG = self._p
	self.d.select_axis = self.w.gui_select_axis.get_active()
	self.d.select_gmoccapy = self.w.gui_select_gmoccapy.get_active()
	self.d.pyvcp = self.w.gui_pyvcp.get_active()
	self.d.pyvcpconnect = self.w.gui_pyvcpconnect.get_active()    
	if self.d.pyvcp == True:
	   if self.w.gui_rdo_pyvcp_blank.get_active() == True:
		  self.d.pyvcpname = "blank.xml"
		  self.pyvcphaltype = 0
	   if self.w.gui_rdo_pyvcp_spindle.get_active() == True:
		  self.d.pyvcpname = "spindle.xml"
		  self.d.pyvcphaltype = 1
	   if self.w.gui_rdo_pyvcp_custom.get_active() == True:
		  self.d.pyvcpname = "custompanel.xml"
	   else:
		  if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/custompanel.xml" % self.d.machinename)):
			 if not self.a.warning_dialog(self._p.MESS_PYVCP_REWRITE,False):
			   return True

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
		self.w.gui_rdo_pyvcp_custom.set_sensitive(False)
	else:
		self.w.gui_rdo_pyvcp_custom.set_sensitive(i)
	self.w.gui_pyvcp_box.set_sensitive(i)

#***************
# GLADEVCP TEST
#***************
def test_glade_panel(self,w):
	panelname = os.path.join(self.a.distdir, "configurable_options/gladevcp")
	if self.w.rdo_gladevcp_blank.get_active() == True:
		print 'no sample requested'
		return True
	if self.w.rdo_default_display.get_active() == True:
		panel = "default_panel.glade"
	if self.w.rdo_custom_galdevcp.get_active() == True:
		panel = "custom_galdevcp.glade"
		panelname = os.path.expanduser("~/linuxcnc/configs/%s" % self.d.machinename)
	halrun = os.popen("cd %(panelname)s\nhalrun -Is > /dev/null"% {'panelname':panelname,}, "w" )    
	halrun.write("loadusr -Wn displaytest pyvcp -c displaytest %(panel)s\n" %{'panel':panel,})
	if self.w.rdo_default_display.get_active() == True:
		halrun.write("setp displaytest.spindle-speed 1000\n")
	halrun.write("waitusr displaytest\n")
	halrun.flush()
	halrun.close()  
	
#***************
# PYVCP TEST
#***************
def testpanel(self,w):
	panelname = os.path.join(self.a.distdir, "configurable_options/pyvcp")
	if self.w.gui_rdo_pyvcp_blank.get_active() == True:
		print 'no sample requested'
		return True
	if self.w.gui_rdo_pyvcp_spindle.get_active() == True:
		panel = "spindle.xml"
	if self.w.gui_rdo_pyvcp_custom.get_active() == True:
		panel = "custompanel.xml"
		panelname = os.path.expanduser("~/linuxcnc/configs/%s" % self.d.machinename)
	halrun = os.popen("cd %(panelname)s\nhalrun -Is > /dev/null"% {'panelname':panelname,}, "w" )    
	halrun.write("loadusr -Wn displaytest pyvcp -c displaytest %(panel)s\n" %{'panel':panel,})
	if self.w.gui_rdo_pyvcp_spindle.get_active() == True:
		halrun.write("setp displaytest.spindle-speed 1000\n")
	halrun.write("waitusr displaytest\n")
	halrun.flush()
	halrun.close() 
