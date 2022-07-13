#!/usr/bin/tclsh
# script to ensure e:[xyz]counts are zero if machine off

package require Hal
set ::prog [file tail [info script]]
set ::periodic_delay_ms 200

proc monitor_ecounts {} {
  catch {after cancel $::a_id}
  if {   ![hal getp halui.machine.is-on]
      && (   0 != [hal gets e:xcounts]
          || 0 != [hal gets e:ycounts]
          || 0 != [hal gets e:zcounts]) } {
    # handle deferred connection to signal
    if [catch { hal sets e:xcounts 0
                hal sets e:ycounts 0
                hal sets e:zcounts 0
              } msg] {
       puts stderr msg=$msg
       puts stderr "$::prog exiting"
       exit 1
    }
  }
  set ::a_id [after $::periodic_delay_ms monitor_ecounts]
}
monitor_ecounts
vwait ::forever
