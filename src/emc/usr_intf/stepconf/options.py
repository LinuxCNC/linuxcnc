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
#***************
# options PAGE
#***************
import os
def options_prepare(self):
	self.w.classicladder.set_active(self.d.classicladder)
	self.w.modbus.set_active(self.d.modbus)
	self.w.digitsin.set_value(self.d.digitsin)
	self.w.digitsout.set_value(self.d.digitsout)
	self.w.s32in.set_value(self.d.s32in)
	self.w.s32out.set_value(self.d.s32out)
	self.w.floatsin.set_value(self.d.floatsin)
	self.w.floatsout.set_value(self.d.floatsout)
	self.w.halui.set_active(self.d.halui_custom)
	self.page_set_state('halui_page', self.w.halui.get_active())
	self.w.ladderconnect.set_active(self.d.ladderconnect)
	self.on_classicladder_toggled()
	self.w.manualtoolchange.set_active(self.d.manualtoolchange)
	if  not self.w.createconfig.get_active():
	   if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/custom.clp" % self.d.machinename)):
			self.w.radiobutton4.set_active(True)

def options_finish(self):
	SIG = self._p
	self.d.classicladder = self.w.classicladder.get_active()
	self.d.modbus = self.w.modbus.get_active()
	self.d.digitsin = self.w.digitsin.get_value()
	self.d.digitsout = self.w.digitsout.get_value()
	self.d.s32in = self.w.s32in.get_value()
	self.d.s32out = self.w.s32out.get_value()
	self.d.floatsin = self.w.floatsin.get_value()
	self.d.floatsout = self.w.floatsout.get_value()
	self.d.halui_custom = self.w.halui.get_active()  
	self.d.ladderconnect = self.w.ladderconnect.get_active()   
	self.d.manualtoolchange = self.w.manualtoolchange.get_active()       
	if self.d.classicladder:
	   if self.w.radiobutton1.get_active() == True:
		  if self.d.tempexists:
			   self.d.laddername='TEMP.clp'
		  else:
			   self.d.laddername= 'blank.clp'
			   self.d.ladderhaltype = 0
	   if self.w.radiobutton2.get_active() == True:
		  self.d.laddername = 'estop.clp'
		  inputs = self.a.build_input_set()
		  if SIG.ESTOP_IN not in inputs:
			 self.a.warning_dialog(self._p.MESS_NO_ESTOP,True)
			 return True # don't advance the page
		  self.d.ladderhaltype = 1
	   if self.w.radiobutton3.get_active() == True:
			 self.d.laddername = 'serialmodbus.clp'
			 self.d.modbus = 1
			 self.w.modbus.set_active(self.d.modbus) 
			 self.d.ladderhaltype = 0          
	   if self.w.radiobutton4.get_active() == True:
		  self.d.laddername='custom.clp'
	   else:
		   if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/custom.clp" % self.d.machinename)):
			  if not self.a.warning_dialog(self._p.MESS_CL_REWRITE,False):
				 return True # don't advance the page
	   if self.w.radiobutton1.get_active() == False:
		  if os.path.exists(os.path.join(self._p.distdir, "configurable_options/ladder/TEMP.clp")):
			 if not self.a.warning_dialog(self._p.MESS_CL_EDITTED,False):
			   return True # don't advance the page

# options page callback
def on_loadladder_clicked(self, *args):
	self.load_ladder(self)

def on_classicladder_toggled(self, *args):
	i= self.w.classicladder.get_active()
	self.w.ladder_box.set_sensitive(i)
	if  self.w.createconfig.get_active():
		self.w.radiobutton4.set_sensitive(False)
	else:
		self.w.radiobutton4.set_sensitive(i)
	if not i:
		self.w.clpins_expander.set_expanded(False)

def on_halui_toggled(self, *args):
	self.page_set_state('halui_page', self.w.halui.get_active())


#**************
# LADDER TEST
#**************
def load_ladder(self,w):         
	newfilename = os.path.join(self.a.distdir, "configurable_options/ladder/TEMP.clp")    
	self.d.modbus = self.w.modbus.get_active()
	self.halrun = halrun = os.popen("halrun -Is", "w")
	halrun.write(""" 
		  loadrt threads period1=%(period)d name1=fast fp1=0 period2=1000000 name2=slow\n
		  loadrt classicladder_rt numPhysInputs=%(din)d numPhysOutputs=%(dout)d numS32in=%(sin)d numS32out=%(sout)d\
				 numFloatIn=%(fin)d numFloatOut=%(fout)d\n
		  addf classicladder.0.refresh slow\n
		  start\n
				  """ % {
				  'period': 50000,
				  'din': self.w.digitsin.get_value(),
				  'dout': self.w.digitsout.get_value(),
				  'sin': self.w.s32in.get_value(),
				  'sout': self.w.s32out.get_value(), 
				  'fin':self.w.floatsin.get_value(),
				  'fout':self.w.floatsout.get_value(),
			 })
	if self.w.radiobutton1.get_active() == True:
		if self.d.tempexists:
		   self.d.laddername='TEMP.clp'
		else:
		   self.d.laddername= 'blank.clp'
	if self.w.radiobutton2.get_active() == True:
		self.d.laddername= 'estop.clp'
	if self.w.radiobutton3.get_active() == True:
		self.d.laddername = 'serialmodbus.clp'
		self.d.modbus = True
		self.w.modbus.set_active(self.d.modbus)
	if self.w.radiobutton4.get_active() == True:
		self.d.laddername='custom.clp'
		originalfile = filename = os.path.expanduser("~/linuxcnc/configs/%s/custom.clp" % self.d.machinename)
	else:
		filename = os.path.join(self.a.distdir, "configurable_options/ladder/"+ self.d.laddername)        
	if self.d.modbus == True: 
		halrun.write("loadusr -w classicladder --modmaster --newpath=%(newfilename)s %(filename)s\
			\n" %          { 'newfilename':newfilename ,'filename':filename })
	else:
		halrun.write("loadusr -w classicladder --newpath=%(newfilename)s %(filename)s\n" % { 'newfilename':newfilename ,'filename':filename })
	halrun.flush()
	halrun.close()
	if os.path.exists(newfilename):
		self.d.tempexists = True
		self.w.newladder.set_text('Edited ladder program')
		self.w.radiobutton1.set_active(True)
	else:
		self.d.tempexists = 0

