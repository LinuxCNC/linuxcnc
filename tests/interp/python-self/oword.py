#   This is a component of LinuxCNC
#   Copyright 2012 Michael Haberler <git@mah.priv.at>
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
import interpreter 

def retainparam1(self,x):
    self.param1 = x

def checkparam1_retained(self):
    if hasattr(self,'param1'):
        print "param1 was retained, value = ", self.param1
    else:
        return "param1 was not retained across invocations"

    # test object identity
    if hasattr(interpreter,'this'):
	print "this is self:", self is interpreter.this
    else:
        print "module interpreter: no 'this' attribute"
        
