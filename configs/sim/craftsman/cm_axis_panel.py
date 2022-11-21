########    Craftsman Axis Panel     #########
####     Creator: Piet van Rensburg      #####
####     Company: Craftsman CNC          #####
####     wwww.craftsmancnc.co.nz         #####
##############################################

#GLADEVCP = -u /home/craftsman/linuxcnc/configs/craftsmancnc/cm_axis_panel.py /home/craftsman/linuxcnc/configs/craftsmancnc/cm_axis_panel.glade

import pygtk
pygtk.require("2.0")
import gtk
import pango
import gobject
import glib
import linuxcnc

import globals

lc = linuxcnc.command()
ls = linuxcnc.stat()

class HandlerClass:

	def change_focus():
		os.system('wmctrl -a "AXIS" &')
	
	def home_all(self, gtkobj,data=None):
		self.reset_offsets
		lc.mode(linuxcnc.MODE_MDI)
		lc.mdi('M103')
		lc.mode(linuxcnc.MODE_MANUAL)
		lc.home(-1)
		self.change_focus
		
	def home_x(self, gtkobj,data=None):
		lc.mode(linuxcnc.MODE_MDI)
		lc.mdi('M103')
		lc.mode(linuxcnc.MODE_MANUAL)
		lc.home(0)
		self.change_focus
	
	def home_y(self, gtkobj,data=None):
		lc.mode(linuxcnc.MODE_MDI)
		lc.mdi('M103')
		lc.mode(linuxcnc.MODE_MANUAL)
		lc.home(1)
		self.change_focus
	
	def home_z(self, gtkobj,data=None):
		lc.mode(linuxcnc.MODE_MDI)
		lc.mdi('M103')
		lc.mode(linuxcnc.MODE_MANUAL)
		lc.home(2)
		self.change_focus
	
	def touch_z(self, gtkobj,data=None):
		lc.mode(linuxcnc.MODE_MDI)
		lc.mdi('o<m101> call')
	
	def zero_x(self, gtkobj,data=None):
		lc.mode(linuxcnc.MODE_MDI)
		lc.mdi('G92 X0')
		self.change_focus
	
	def zero_y(self, gtkobj,data=None):
		lc.mode(linuxcnc.MODE_MDI)
		lc.mdi('G92 Y0')
		self.change_focus
	
	def zero_z(self, gtkobj,data=None):
		lc.mode(linuxcnc.MODE_MDI)
		lc.mdi('G92 Z0')
		self.change_focus
	
	def zero_a(self, gtkobj,data=None):
		lc.mode(linuxcnc.MODE_MDI)
		lc.mdi('G92 A0')
		self.change_focus
	
	def reset_offsets():
		lc.mode(linuxcnc.MODE_MDI)
		lc.mdi('G92.1')
	
	def set_axis_zero(axis):
		lc.mode(linuxcnc.MODE_MDI)
		lc.mdi('G92 ' + axis + '0')
		lc.wait_complete()

	def run_spindle(self, gtkobj,data=None):
		ls.poll()
		if ls.spindle_enabled == 0:
			sp_speed = self.builder.get_object('entry2').get_text()
			try:
				sp_sp = float(sp_speed)
				if sp_sp < 3000:
					sp_speed = '3000'
			except:
				sp_speed = '3000'
			lc.mode(linuxcnc.MODE_MDI)
			lc.mdi('M3 S' + sp_speed) #3000')
			gtkobj.set_label("Stop spindle")
			ls.poll()
			self.builder.get_object('entry2').set_text("% 0.0f" % (ls.spindle_speed))
		else:
			lc.mode(linuxcnc.MODE_MDI)
			lc.mdi('M5')
			gtkobj.set_label("Start spindle")
		self.change_focus()
		
	def current_spindle_speed(self, gtkobj, data=None):
		value = gtkobj.hal_pin.get()
		if value:
			self.builder.get_object('hal_button7').set_label("Stop spindle")
		else:
			self.builder.get_object('hal_button7').set_label("Start spindle")
		ls.poll()
		self.builder.get_object('entry2').set_text("% 0.0f" % (ls.spindle_speed))
	
	def set_spindle_speed(self, gtkobj,data=None):
		ls.poll()
		sp_speed = self.builder.get_object('entry2').get_text()
		try:
			sp_sp = float(sp_speed)
			if sp_sp < 3000:
				sp_speed = '3000'
		except:
			sp_speed = '3000'
		lc.mode(linuxcnc.MODE_MDI)
		lc.mdi('S' + sp_speed)
		self.change_focus()
		
		
	def tool_number_hal_pin_changed_cb(self, gtkobj,data=None):
		n = int(self.builder.get_object('tool_number').get_text())
		globals.set_tooldata(n)
		globals.get_tooldata()
		tool_desc = globals.tool_desc
		if n > 0:
			self.builder.get_object('tool_desc').set_text(tool_desc)
			ls.poll()
			curr_line = ls.current_line
			self.builder.get_object("tool_changing").set_active(True)			
			
	def tool_number_prep(self, gtkobj,data=None):
		pass
		#n = int(self.builder.get_object('tool_prep').get_text())
		#globals.set_tooldata(n)
		#globals.get_tooldata()
		#tool_desc = globals.tool_desc
		#if n > 0:
			#self.builder.get_object('tool_desc').set_text(tool_desc)
			#self.builder.get_object('tool_number').set_text(self.builder.get_object('tool_prep').get_text())
	
	def start_after_tool_change(self, gtkobj,data=None):
		globals.get_toolchangedata()
		self.curr_line = int(globals.change_tool)
		lc.mode(linuxcnc.MODE_AUTO)		
		lc.auto(linuxcnc.AUTO_RUN, self.curr_line + 1)
		self.builder.get_object("tool_changing").set_active(False)

	def __init__(self, halcomp,builder,useropts):
		self.builder = builder
		self.curr_line = 0

def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
