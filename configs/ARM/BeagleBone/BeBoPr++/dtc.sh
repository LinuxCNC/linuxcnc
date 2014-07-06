#!/bin/sh
# Copyright 2013
# Charles Steinkuehler <charles@steinkuehler.net>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

for dto in BB-LCNC-BEBOPRBR-00A0 cape-bebopr-brdg-R3 cape-bebopr-pp-R3 ; do
   dtc -O dtb -o ${dto}.dtbo -b 0 -@ ${dto}.dts && sudo cp ${dto}.dtbo /lib/firmware/
done
