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
# AXIS U PAGE
#********************
def axisu_prepare(self):
	self.axis_prepare('u')
def axisu_finish(self):
	self.axis_done('u')
# AXIS U callbacks
def on_usteprev_changed(self, *args): self.a.update_pps('u')
def on_umicrostep_changed(self, *args): self.a.update_pps('u')
def on_upulleyden_changed(self, *args): self.a.update_pps('u')
def on_upulleynum_changed(self, *args): self.a.update_pps('u')
def on_uleadscrew_changed(self, *args): self.a.update_pps('u')
def on_umaxvel_changed(self, *args): self.a.update_pps('u')
def on_umaxacc_changed(self, *args): self.a.update_pps('u')
def on_uaxistest_clicked(self, *args): self.a.test_axis('u')
def on_upreset_button_clicked(self, *args): self.preset_axis('u')
