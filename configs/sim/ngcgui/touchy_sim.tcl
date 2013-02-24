#!/usr/bin/tclsh

# script for linuxcnc simulator using touchy gui
# pass 1: start this script again with env(touchy_sim) set
# pass 2: try ($attempts) to start sim_pin

if ![info exists ::env(touchy_sim)] {
  set ::env(touchy_sim) $::argv
  exec ./touchy_sim.tcl & ;# this script with env(touchy_sim) set
  exit 0
} else {
  set attempts 10
  for {set i 0} {$i < $attempts} {incr i} {
    after 1000
    if ![catch {
         exec sim_pin touchy.cycle-start motion.feed-hold touchy.abort &
      } ] {
      break
    }
  }
  exit 0
}
