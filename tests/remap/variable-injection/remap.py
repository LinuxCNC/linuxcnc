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
from interpreter import *


def prolog405(self, **words):
    self.params["foo405"] = 42
    return INTERP_OK

# rm405 will bump foo405 by 1

def epilog405(self, **words):
    if self.params["foo405"] != 43:
        self.set_errormsg("foo405 != 43 in epilog405")
        return INTERP_ERROR
    return INTERP_OK


def prolog406(self, **words):
    self.params["foo406"] = 42
    return INTERP_OK

# rm406 will bump foo406 by 1

def epilog406(self, **words):
    if self.params["foo406"] != 43:
        self.set_errormsg("foo406 != 43 in epilog406")
        return INTERP_ERROR
    return INTERP_OK


def prolog407(self, **words):
    self.params["foo407"] = 42
    return INTERP_OK

# rm407 will bump foo407 by 1

def epilog407(self, **words):
    if self.params["foo407"] != 43:
        self.set_errormsg("foo407 != 43 in epilog407")
        return INTERP_ERROR
    return INTERP_OK

