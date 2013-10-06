#!/usr/bin/tclsh

#-----------------------------------------------------------------------
# Copyright: 2013
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
set pins [list touchy.cycle-start motion.feed-hold touchy.abort]

if ![info exists ::env(touchy_sim)] {
  set ::env(touchy_sim) $::argv
  exec ./touchy_sim.tcl & ;# restart this script with env(touchy_sim) set
  exit 0
} else {
  after $delay_ms
  if [catch {
    eval exec sim_pin $pins &
  } msg ] {
    puts "$::argv0: sim_pin failed:"
    puts "  $msg"
  }
  exit 0
}
