#   This is a component of LinuxCNC
#   Copyright 2011 Michael Haberler <git@mah.priv.at>
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
import emccanon
import interpreter
import inspect


## {{{ http://code.activestate.com/recipes/145297/ (r1)
def lineno():
    """Returns the current line number in our program."""
    return inspect.currentframe().f_back.f_lineno

# example for creating moves in a Python oword sub
def canonmove(self, x,y,z):
    emccanon.STRAIGHT_FEED(lineno(),x,y,z,0,0,0,0,0,0)
    emccanon.STRAIGHT_TRAVERSE(lineno(),self.params["_ini[config]xpos"],self.params["_ini[config]ypos"],self.params["_ini[config]zpos"],0,0,0,0,0,0)
    return 0.0

