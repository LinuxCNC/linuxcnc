#!/usr/bin/tclsh

#-----------------------------------------------------------------------
# Copyright: 2013,2014
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

# pass 1: set environmental variable as flag
#         then restart this script in background
#         exit
# pass 2: delay then start sim_pin

set delay_ms 3000
set pins [list \
             ini.traj_default_velocity \
             ini.traj_default_acceleration \
             ini.traj_max_velocity \
             ini.traj_max_acceleration \
             ini.0.max_velocity \
             ini.0.max_acceleration \
             ini.0.min_limit \
             ini.0.max_limit \
             ini.0.backlash \
             ini.0.ferror \
             ini.0.min_ferror \
         ]

proc show_all {} {
  package require Hal
  puts "ini hal pins:"
  puts [hal show pin ini]
} ;# show_all

if ![info exists ::env(ini_hal_pins)] {
  set ::env(ini_hal_pins) $::argv
  exec ./ini_hal_pins.tcl & ;# restart this script with env(ini_hal_pins) set
  exit 0
} else {
  after $delay_ms
  show_all
  if [catch {
    eval exec sim_pin $pins &
  } msg ] {
    puts "$::argv0: sim_pin failed:"
    puts "  $msg"
  }
  exit 0
}
