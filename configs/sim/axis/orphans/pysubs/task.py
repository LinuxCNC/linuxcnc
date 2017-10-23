#   This is a component of LinuxCNC
#   Copyright 2011, 2013, 2014 Dewey Garrett <dgarrett@panix.com>,
#   Michael Haberler <git@mah.priv.at>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#
import sys
import hal
import emccanon
import interpreter

try:
    import emctask
    import customtask
except ImportError:
    pass

try:
    import cPickle as pickle
except ImportError:
    import pickle

def starttask():
    global pytask
    import emc
    ini = emc.ini(emctask.ini_filename())
    t = ini.find("PYTHON", "PYTHON_TASK")
    if int(t) if t else 0:
        pytask = customtask.CustomTask()

if 'emctask' in sys.builtin_module_names:
    starttask()
