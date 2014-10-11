#!/bin/bash

#-----------------------------------------------------------------------
# Copyright: 2014
# Author:    Dewey Garrett <dgarrett@panix.com>
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#-----------------------------------------------------------------------

# Note: man milltask for list of available ini hal pins

sim_pin \
   ini.traj_arc_blend_enable \
   ini.traj_default_velocity \
   ini.traj_default_acceleration \
   ini.traj_max_velocity \
   ini.traj_max_acceleration \
   ini.0.max_velocity \
   ini.0.max_acceleration \
   ini.1.max_velocity \
   ini.1.max_acceleration \
   ini.2.max_velocity \
   ini.2.max_acceleration \

exit 0

