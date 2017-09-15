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
# Demo Python O-word subroutine - call as:
# o<square> [5]
# (debug, #<_value>)
#
# this function expects exactly one parameter from NGC and will throw an exception otherwise
# which aint particularly bright
def square(self,x):
    return  x*x

# a function taking a variable number of arguments
# o<multiply> [5] [7]
# (debug, #<_value>)
# o<multiply> [5] [7] [9] [16]
# (debug, #<_value>)

import operator

# you'd be better of doing it this way:
def multiply(self, *args):
    print "multiply: number of arguments=", len(args)
    return reduce(operator.mul, args)

