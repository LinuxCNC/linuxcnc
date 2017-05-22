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
#*************
# FINISH PAGE
#*************
import shutil
import os
import errno
from gi.repository import Gtk

def finished_prepare(self):
    pass
def finished_finish(self):
    self.build_config()

def build_config(self):
    base = self.build_base()
    self.d.save(base)
    self.d.save_preferences()
    self.a.INI.write_inifile(base)
    self.a.HAL.write_halfile(base)
    self.copy(base, "tool.tbl")
    if self.a.warning_dialog(self._p.MESS_QUIT,False):
        Gtk.main_quit()

def build_base(self):
    base = os.path.expanduser("~/linuxcnc/configs/%s" % self.d.machinename)
    ncfiles = os.path.expanduser("~/linuxcnc/nc_files")
    if not os.path.exists(ncfiles):
        try:
            os.makedirs(ncfiles)
        except os.error, detail:
            if detail.errno != errno.EEXIST: raise

        examples = os.path.join(BASE, "share", "linuxcnc", "ncfiles")
        if not os.path.exists(examples):
            examples = os.path.join(BASE, "nc_files")
        if os.path.exists(examples):
            os.symlink(examples, os.path.join(ncfiles, "examples"))
    try:
        os.makedirs(base)
    except os.error, detail:
        if detail.errno != errno.EEXIST: raise
    return base


def copy(self, base, filename):
    dest = os.path.join(base, filename)
    if not os.path.exists(dest):
        shutil.copy(os.path.join(self.a.distdir, filename), dest)


