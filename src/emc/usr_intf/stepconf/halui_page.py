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
# halui PAGE
#***************
def halui_page_prepare(self):
	# Clear listore
	self.w.lstStore1.clear()
	# Populate treeview
	for num, mdi_command in enumerate(self.d.halui_list_custom):
		self.w.lstStore1.append([num+1, mdi_command])

def halui_page_finish(self):
	self.d.halui_list_custom = []
	# Get first row
	treeiter = self.w.lstStore1.get_iter_first()
	if treeiter == None:
		return True
	self.d.halui_list_custom.append(self.w.lstStore1.get_value(treeiter, 1))
	while treeiter != None:
		treeiter = self.w.lstStore1.iter_next(treeiter)
		if treeiter != None:
			current_value = self.w.lstStore1.get_value(treeiter, 1).strip()
			# Check if value contains data
			if len(current_value) > 2:
				self.d.halui_list_custom.append(current_value)

def on_halui_btnAdd_clicked(self, *args):
	next_index = len(self.w.lstStore1) +1
	self.w.lstStore1.append([next_index, ""])

def on_halui_btnDel_clicked(self, *args):
	select = self.w.viewTable1.get_selection()
	model, treeiter = select.get_selected()
	if treeiter != None:
		# Remove selected row
		self.w.lstStore1.remove(treeiter)
		# Get first row
		treeiter = self.w.lstStore1.get_iter_first()
		if treeiter == None:
			return
		index = 1
		self.w.lstStore1.set_value(treeiter, 0, index)
		index = index +1
		# Cicle lstStore1 to update index
		while treeiter != None:
			treeiter = self.w.lstStore1.iter_next(treeiter)
			if treeiter != None:
				# Change index
				self.w.lstStore1.set_value(treeiter, 0, index)
				index = index +1

def on_halui_btnUp_clicked(self, *args):
	select = self.w.viewTable1.get_selection()
	model, treeiter = select.get_selected()
	if treeiter != None:
		prev_treeiter = model.iter_previous(treeiter)
		current_index = model[treeiter][0]
		# Move up and update first column (index)
		if((current_index -1) > 0):
			self.w.lstStore1.move_before(treeiter, prev_treeiter)
			self.w.lstStore1.set_value(treeiter, 0, current_index -1)
			self.w.lstStore1.set_value(prev_treeiter, 0, current_index)

def on_halui_btnDown_clicked(self, *args):
	select = self.w.viewTable1.get_selection()
	model, treeiter = select.get_selected()
	if treeiter != None:
		next_treeiter = model.iter_next(treeiter)
		current_index = model[treeiter][0]
		# Move down and update first column (index)
		if(next_treeiter != None):
			self.w.lstStore1.move_after(treeiter, next_treeiter)
			self.w.lstStore1.set_value(treeiter, 0, current_index +1)
			self.w.lstStore1.set_value(next_treeiter, 0, current_index)

def on_halui_row_changed(self, *args):
	newvalue = args[2]
	if (len(newvalue.strip()) <2):
		return
	select = self.w.viewTable1.get_selection()
	model, treeiter = select.get_selected()
	self.w.lstStore1.set_value(treeiter, 1, newvalue)

