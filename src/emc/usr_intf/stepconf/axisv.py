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

#********************
# AXIS V PAGE
#********************
def axisv_prepare(self):
	self.axis_prepare('v')
def axisv_finish(self):
	self.axis_done('v')
# AXIS V callbacks
def on_vsteprev_changed(self, *args): self.a.update_pps('v')
def on_vmicrostep_changed(self, *args): self.a.update_pps('v')
def on_vpulleyden_changed(self, *args): self.a.update_pps('v')
def on_vpulleynum_changed(self, *args): self.a.update_pps('v')
def on_vleadscrew_changed(self, *args): self.a.update_pps('v')
def on_vmaxvel_changed(self, *args): self.a.update_pps('v')
def on_vmaxacc_changed(self, *args): self.a.update_pps('v')
def on_vaxistest_clicked(self, *args): self.a.test_axis('v')
def on_vpreset_button_clicked(self, *args): self.preset_axis('v')
