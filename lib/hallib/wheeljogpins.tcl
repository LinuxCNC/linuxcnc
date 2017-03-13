# tcl file to enable jog pins
#   for all axis letters
#   for up to 9 joints
# errors are ignored
# scale defaults to 1, use ::argv otherwise for all

set scalevalue 1 ;# default
if {[llength $::argv] == 1} {
  set scalevalue $::argv
}
catch {
  foreach l {x y z z b c u v w} {
    setp axis.$l.jog-enable 1
    setp axis.$l.jog-scale $::scalevalue
  }
  for {set i 0} {$i < 9} {incr i} {
    setp joint.$i.jog-enable 1
    setp joint.$i.jog-scale  $::scalevalue
  }
}
