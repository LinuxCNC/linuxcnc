#!/usr/bin/tclsh


# pass 1: set environmental variable as flag
#         then restart this script in background
#         exit
# pass 2: delay then start sim_pin

set delay_ms 3000
set pins [list \
          meter_scale.hal_table1 \
          meter_scale.max-value \
          meter_scale.meter \
         ]

if ![info exists ::env(gladevcp_probe)] {
  set ::env(gladevcp_probe) $::argv
  exec ./probe.tcl & ;# restart this script with env(ini_hal_pins) set
  exit 0
} else {
  after $delay_ms
  if [catch {
    eval exec gladevcp -d -d -u probe.py -U debug=3 -H probe.hal probe.ui &
  } msg] {
    puts "$::argv0: gladevcp failed:"
    puts "  $msg"
  }
  after $delay_ms
  if [catch {
    exec simulate_probe *
  } msg] {
    puts "$::argv0: simulate_probe failed:"
    puts "  $msg"
  }
  exit 0
}
