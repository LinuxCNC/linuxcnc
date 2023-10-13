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
# trivial example
def _pi(self, *args):
    return 3.1415926535

# the Python equivalent of '#<_motion_mode> :
# return the currently active motion code (times 10)
def _py_motion_mode(self, *args):
    return self.active_g_codes[1]

def _error(self, *args):
    # this sets the error message and aborts execution (except in (debug,#<_err>))
    return "badly botched"

# return a whacky type to exercise the error reporting mechanism
def _badreturntype(self, *args):
    return object()
