#!/usr/bin/tclsh

# script for linuxcnc simulator using touchy gui
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
