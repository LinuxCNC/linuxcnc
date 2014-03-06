# xhc-hb04.tcl: HALFILE for xhc-hb04 pendant

# Usage:
# In ini file, include:
#   [HAL]
#   HALFILE = existing halfiles
#   ...
#   HALFILE = xhc-hb04.tcl
#
#   [XHC-HB04_CONFIG]
#   layout = 2       (required: 1|2 are supported)
#   coords = x y z a (any unique four of xyzabcuvw)
#   coefs  = 1 1 1 1 (optional, filter coefs, 0 < coef < 1, not usually reqd)
#   scales = 1 1 1 1 (optional)
#   threadname = servo-thread (optional)
#   sequence = 1     (optional: 1|2)
#   jogmode = normal (optional: normal|vnormal|plus-minus)
#   require_pendant = yes (optional: yes|no)

#   [XHC-HB04_BUTTONS]
#   name = pin  (connect button to hal pin)
#   name = ""   (no connect button)
#   special cases:
#   start-pause = std_start_pause  (for usual behavior)
#   step = xhc-hb04.stepsize-up    (for usual behavior)
#   (see ini files for more exanples)

# Notes:
#    1) the 'start-pause' pin can be set to "std_start_pause" to
#       implement default behavior
#    2) the 'step' pin is normally connected to xhc-hb04.stepsize-up
#    3) non-root access to the usb device requires an additional
#       udev rule.  Typically, create /etc/udev/rules.d/90-xhc.rules:
#       SYSFS{idProduct}=="eb70", SYSFS{idVendor}=="10ce", MODE="666", OWNER="root", GROUP="users"
#       or (for ubuntu12 and up):
#       ATTR{idProduct}=="eb70",  ATTR{idVendor}=="10ce",  MODE="666", OWNER="root", GROUP="users"
#    4) For jogmode==vnormal (man motion -- see axis.N.jog-vel-mode),
#       the max movement is limited by the machine velocity and acceleration limits
#       such that delta_x = 0.5 * vmax**2/accelmx
#       so for sim example:
#       inch: vmax= 1.2   accelmax= 20  delta_x=0.036
#       mm:   vmax=30.48  acclemax=508  delta_x=0.9144
#       Typically:
#         (-s1) sequence 1 (1,10,100,1000) is ok for mm-based machines
#         (-s2) sequence 2 (1,5,10,20)     is ok for inch-based machines
#    4) jogmode==plus-minus implements halui plus-minus jogging which
#       seems to work in both joint and world modes
#       (tested on git master branch before integration of joints_axesN branch)
#
#    5) 19feb2014 notes for future work
#       jogging non-trivkins machines in world mode
#
#       jogmode==plus-minus-increment reserved for halui plus-minus-increment jogging
#       incremental, world-mode jogging is not working in current git master
#       (at this date, current git master has not merged a joint_axesN branch)
#
#       see:
#       http://www.linuxcnc.org/docs/html/man/man9/gantrykins.9.html
#       Joint-mode (aka Free mode) supports continuous and incremental jogging.
#       World-mode (aka Teleop mode) only supports continuous jogging.

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

proc is_uniq {list_name} {
  set tmp(xxxxxxxx) "" ;# make an array first
  foreach item $list_name {
    if {[array names tmp $item] == $item} {
      return 0 ;# not unique
    }
    set tmp($item) $item
  }
  return 1 ;# unique
} ;# is_uniq

proc pin_exists {name} {
  set line [lindex [split [show pin $name] \n] 2]
  if {"$line" == ""} {
    return 0 ;# fail
  }
  if [catch {scan $line "%d %s %s %s%s" owner type dir value pinname} msg] {
     return 0 ;# fail
  } else {
     #puts stderr "OK:$owner $type $dir $value $pinname"
     return 1 ;# ok
  }
} ;# pin_exists

proc connect_pins {} {
  foreach bname [lsort [array names ::XHC_HB04_BUTTONS]] {
    set thepin $::XHC_HB04_BUTTONS($bname)
    set thepin [lindex $thepin 0]

    if {"$thepin" == "\"\""} {
      #puts stderr "$::progname: no pin defined for <$bname>"
      continue
    }
    # this pin is can specify std behavior
    if {   ([string tolower $bname] == "start-pause")
        && ([string tolower $thepin] == "std_start_pause")
       } {
      std_start_pause_button
      puts stderr "$::progname: using std_start_pause_button"
      continue
    }
    # these are warnings in the ini file examples but aren't real pins
    if {[string tolower "$thepin"] == "caution"} {
      puts stderr "$::progname: skipping button $bname marked <$thepin>"
      continue
    }
    set fullbname xhc-hb04.button-$bname
    if ![pin_exists $fullbname] {
      puts stderr "$::progname: !!! <$fullbname> pin does not exist, continuing"
      continue
    }
    if ![pin_exists $thepin] {
      puts stderr "$::progname: !!! <$thepin> target pin does not exist, continuing"
      continue
    }

    net pendant:$bname $fullbname => $thepin
  }
} ;# connect_pins

proc wheel_setup {jogmode} {
  # defaults if not in inifile:
  set ::XHC_HB04_CONFIG(coef,0) 1.0
  set ::XHC_HB04_CONFIG(coef,1) 1.0
  set ::XHC_HB04_CONFIG(coef,2) 1.0
  set ::XHC_HB04_CONFIG(coef,3) 1.0
  if [info exists ::XHC_HB04_CONFIG(coefs)] {
    set idx 0
    foreach g $::XHC_HB04_CONFIG(coefs) {
      set g1 $g
      if {$g < 0} {
         set g [expr -1 * $g]
         puts stderr "$::progname: coef #$idx must be positive was:$g1, is:$g"
      }
      if {$g > 1} {
         set g .5
         puts stderr "$::progname: coef #$idx must < 1 coef was:$g1, is:$g"
      }
      set ::XHC_HB04_CONFIG(coef,$idx) $g
      incr idx
    }
  }
  # defaults if not in inifile:
  set ::XHC_HB04_CONFIG(scale,0) 1.0
  set ::XHC_HB04_CONFIG(scale,1) 1.0
  set ::XHC_HB04_CONFIG(scale,2) 1.0
  set ::XHC_HB04_CONFIG(scale,3) 1.0
  if [info exists ::XHC_HB04_CONFIG(scales)] {
    set idx 0
    foreach g $::XHC_HB04_CONFIG(scales) {
      set ::XHC_HB04_CONFIG(scale,$idx) $g
      incr idx
    }
  }

  net pendant:jog-scale      <= xhc-hb04.jog.scale
  net pendant:jog-counts     <= xhc-hb04.jog.counts
  net pendant:jog-counts-neg <= xhc-hb04.jog.counts-neg

  set anames {x y z a}
  # xhc-hb04.cc hardcodes pin names as: x y z a
  # herein: Use names in order of the [XHC_HB04_CONFIG]coords
  #         specification in the inifile.
  #         These pin names will be a little confusing when
  #         using alternate axis sequences but the signal
  #         names will align correctly.
  #         With this method, any coord (xyzabcuvw) can be
  #         controlled by the wheel (providing it exists)
  #
  set idx 0
  foreach coord $::XHC_HB04_CONFIG(coords) {
    set axno $::XHC_HB04_CONFIG($coord,axno)

    setp pendant_util.coef$idx  $::XHC_HB04_CONFIG(coef,$idx)
    setp pendant_util.scale$idx $::XHC_HB04_CONFIG(scale,$idx)

    set acoord [lindex $anames $idx]
    net pendant:pos-$coord    halui.axis.$axno.pos-feedback \
                           => xhc-hb04.$acoord.pos-absolute
    net pendant:pos-rel-$coord    halui.axis.$axno.pos-relative \
                               => xhc-hb04.$acoord.pos-relative

    net pendant:jog-scale => axis.$axno.jog-scale

    net pendant:jog-counts                 => pendant_util.in$idx
    net pendant:jog-counts-$coord-filtered <= pendant_util.out$idx \
                                           => axis.$axno.jog-counts

    switch $jogmode {
      normal - vnormal {
        net pendant:jog-$coord    xhc-hb04.jog.enable-$acoord \
                               => axis.$axno.jog-enable
      }
      plus-minus {
        # connect halui plus,minus pins
        net pendant:jog-plus-$coord     xhc-hb04.jog.plus-$acoord  \
                                     => halui.jog.$axno.plus
        net pendant:jog-minus-$coord    xhc-hb04.jog.minus-$acoord \
                                     => halui.jog.$axno.minus
      }
    }
    switch $jogmode {
      vnormal {
        setp axis.$axno.jog-vel-mode 1
      }
    }

    incr idx
  }

  switch $jogmode {
    normal - vnormal {
      net pendant:jog-speed <= halui.max-velocity.value
      # not used: xhc-hb04.jog.velocity
      # not used: xhc-hb04.jog.max-velocity
    }
    plus-minus {
      # Note: the xhc-hb04 driver manages xhc-hb04.jog.velocity
      net pendant:jog-max-velocity <= halui.max-velocity.value
      net pendant:jog-max-velocity => xhc-hb04.jog.max-velocity
      net pendant:jog-speed        <= xhc-hb04.jog.velocity
      net pendant:jog-speed        => halui.jog-speed
    }
  }

  setp halui.feed-override.scale 0.01
  net pendant:jog-counts  => halui.feed-override.counts

  setp halui.spindle-override.scale 0.01
  net pendant:jog-counts  => halui.spindle-override.counts


  net pendant:jog-feed      halui.feed-override.count-enable \
                         <= xhc-hb04.jog.enable-feed-override
  net pendant:jog-feed2     halui.feed-override.value \
                         => xhc-hb04.feed-override

  net pendant:jog-spindle   halui.spindle-override.count-enable
  net pendant:jog-spindle   <= xhc-hb04.jog.enable-spindle-override
  net pendant:jog-spindle2  halui.spindle-override.value \
                         => xhc-hb04.spindle-override
  net pendant:spindle-rps   motion.spindle-speed-cmd-rps \
                         => xhc-hb04.spindle-rps
} ;# wheel_setup

proc std_start_pause_button {} {
  # hardcoded setup for button-start-pause
  net    pendant:start-or-pause <= xhc-hb04.button-start-pause \
                                => pendant_util.start-or-pause

  net    pendant:is-idle    <= halui.program.is-idle \
                            => pendant_util.is-idle
  net    pendant:is-paused  <= halui.program.is-paused \
                            => pendant_util.is-paused
  net    pendant:is-running <= halui.program.is-running \
                            => pendant_util.is-running

  net    pendant:program-resume pendant_util.resume => halui.program.resume
  net    pendant:program-pause  pendant_util.pause => halui.program.pause
  net    pendant:program-run    pendant_util.run => halui.program.run
} ;# std_start_pause_button

proc popup_msg {msg} {
  puts stderr "$msg"
  if [catch {package require Tk
             wm withdraw .
             tk_messageBox \
                 -title "$::progname: loadusr" \
                 -type ok \
                 -message "$msg"
             destroy .
            } msg] {
     puts stderr "$msg"
  }
} ;# popup_msg

proc err_exit {msg} {
  puts stderr "\n$::progname: $msg\n"
  exit 1
} ;# err_exit

# begin------------------------------------------------------------------------
set ::progname "xhc-hb04.tcl"
set cfg xhc-hb04-layout2.cfg ;# default

foreach name [array names ::XHC_HB04_CONFIG] {
  set ::XHC_HB04_CONFIG($name) [string trim $::XHC_HB04_CONFIG($name) "{}"]
}

if [info exists ::XHC_HB04_CONFIG(layout)] {
  switch ${::XHC_HB04_CONFIG(layout)} {
    1 {set cfg xhc-hb04-layout1.cfg}
    2 {set cfg xhc-hb04-layout2.cfg}
    default {
      set msg "Unknown layout:<$::XHC_HB04_CONFIG(layout)>"
      set msg "$msg\ntrying: $cfg"
      popup_msg "$msg"
      # keep going
    }
  }
}

if ![file exists $cfg] {
  set msg "Cannot find file: <$cfg>\nCannot configure pendant\n"
  set msg "$msg\nContinuing without xhc-hb04"
  popup_msg "$msg"
  return ;# not an exit
}

# require_pendant==yes: use -x, dont create pins unless connected
# require_pendant==no:          create pins if not connected
if ![info exists ::XHC_HB04_CONFIG(require_pendant)] {
  set ::XHC_HB04_CONFIG(require_pendant) yes ;# default
}
set dashx -x
switch $::XHC_HB04_CONFIG(require_pendant) {
  no      {set dashx ""}
}

if [info exists ::XHC_HB04_CONFIG(sequence)] {
  set dashs "-s $::XHC_HB04_CONFIG(sequence)"
} else {
  set dashs ""
}

set cmd "loadusr -W xhc-hb04 $dashx $dashs -I $cfg -H"
if [catch {eval $cmd} msg] {
  set msg "\n$::progname: loadusr xhc-hb04:\n<$msg>\n\n"
  set msg "$msg Is it plugged in?\n\n"
  set msg "$msg Are permissions correct?\n\n"
  set msg "$msg Continuing without xhc-hb04\n"
  set msg "$msg \nFailing cmd:\n$cmd"
  popup_msg "$msg"
  return ;# not an exit
}

# jogmodes:
#   normal: use motion pins:
#               axis.N.jog-counts
#               axis.N.jog-enable
#               axis.N.jog-scale  (machine units per count)

#   plus-minus: use halui pins:
#               halui.jog.N.plus  (jog in + dir at jog-speed)
#               halui.jog.N.minus (jog in - dir at jog-speed)
#               halui.jog-speed   (applies to plus-minus jogging only)
#
if ![info exists ::XHC_HB04_CONFIG(jogmode)] {
  set ::XHC_HB04_CONFIG(jogmode) normal ;# default
}

set jogmode $::XHC_HB04_CONFIG(jogmode)
switch $jogmode {
  normal {}
  vnormal {}
  plus-minus {}
  default {
    set ::XHC_HB04_CONFIG(jogmode) normal
    set msg "Unkknown jogmode <$jogmode>"
    set msg "$msg  Using $::XHC_HB04_CONFIG(jogmode)"
    popup_msg "$msg"
  }
}

set ct 0; foreach coord {x y z a b c u v w} {
  set ::XHC_HB04_CONFIG($coord,axno) $ct;  incr ct
}

if [info exists ::XHC_HB04_CONFIG(coords)] {
  if ![is_uniq $::XHC_HB04_CONFIG(coords)] {
    err_exit "coords must be unique, not: <$::XHC_HB04_CONFIG(coords)>"
  }
} else {
  set ::XHC_HB04_CONFIG(coords) {x y z a} ;# default
}

if ![info exists ::XHC_HB04_CONFIG(threadname)] {
  set ::XHC_HB04_CONFIG(threadname) "servo-thread" ;# default
}
loadrt xhc_hb04_util names=pendant_util
addf   pendant_util $::XHC_HB04_CONFIG(threadname)

connect_pins    ;# per ini file items: [XHC_HB04_BUTTONS]buttonname=pin
wheel_setup  $::XHC_HB04_CONFIG(jogmode)
                 # jog wheel per ini file items:
                 #     [XHC_HB04_CONFIG]coords,coefs,scales
#parray ::XHC_HB04_CONFIG
