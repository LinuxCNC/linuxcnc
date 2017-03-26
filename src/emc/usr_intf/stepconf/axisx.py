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
#*******************
# AXIS X PAGE
#*******************
def axisx_prepare(self):
	self.axis_prepare('x')
def axisx_finish(self):
	self.axis_done('x')
# AXIS X callbacks
def on_xsteprev_changed(self, *args): self.a.update_pps('x')
def on_xmicrostep_changed(self, *args): self.a.update_pps('x')
def on_xpulleyden_changed(self, *args): self.a.update_pps('x')
def on_xpulleynum_changed(self, *args): self.a.update_pps('x')
def on_xleadscrew_changed(self, *args): self.a.update_pps('x')
def on_xmaxvel_changed(self, *args): self.a.update_pps('x')
def on_xmaxacc_changed(self, *args): self.a.update_pps('x')
def on_xaxistest_clicked(self, *args): self.a.test_axis('x')
def on_xpreset_button_clicked(self, *args): self.preset_axis('x')
