#!/usr/bin/env python
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
#http://stackoverflow.com/questions/361/generate-list-of-all-possible-permutations-of-a-string
def nextPermutation(perm):
    k0 = None
    for i in range(len(perm)-1):
        if perm[i]<perm[i+1]:
            k0=i
    if k0 == None:
        return None

    l0 = k0+1
    for i in range(k0+1, len(perm)):
        if perm[k0] < perm[i]:
            l0 = i

    perm[k0], perm[l0] = perm[l0], perm[k0]
    perm[k0+1:] = reversed(perm[k0+1:])
    return perm


perm=["g88.1x1", "m405","m406","m407","m408"]

while perm:
    print perm
    perm = nextPermutation(perm)

perm=["g88.1x1", "m407","m408","m409","m410"]

while perm:
    print perm
    perm = nextPermutation(perm)
