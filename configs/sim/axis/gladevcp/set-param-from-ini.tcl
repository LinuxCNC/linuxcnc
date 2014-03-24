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

if ![info exists ::env(set-param-from-ini)] {
  set ::env(set-param-from-ini) $::argv
  set ::env(EXAMPLE_CFG) "set-param-from-ini.cfg"
  exec ./set-param-from-ini.tcl & ;# restart this script with env(ini_hal_pins) set
  exit 0
} else {
  after $delay_ms
  if [catch {
    eval exec gladevcp -u ./meter_scale.py ./meter_scale.ui &
  } msg] {
    puts "$::argv0: gladevcp failed:"
    puts "  $msg"
  }
  
  after $delay_ms
  if [catch {
    eval exec sim_pin $pins &
  } msg ] {
    puts "$::argv0: sim_pin failed:"
    puts "  $msg"
  }
  exit 0
}
