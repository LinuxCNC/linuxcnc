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

#   [XHC-HB04_BUTTONS]
#   name = pin  (connect button to hal pin)
#   name = ""   (no connect button)
#   (see ini files for more exanples)

# Notes:
#    1) presumes an existing thread named servo-thread
#    2) the 'start-pause' pin is RESERVED since it is connected herein
#    3) the 'step' pin is used by xhc-hb04.cc to manage the jogwheel
#       step sizes so use caution in connecting it for other purposes
#    2) non-root access to the usb device requires an additional
#       udev rule.  Typically, create /etc/udev/rules.d/90-xhc.rules:
#       SYSFS{idProduct}=="eb70", SYSFS{idVendor}=="10ce", MODE="666", OWNER="root", GROUP="users"
#       or (for ubuntu12 and up):
#       ATTR{idProduct}=="eb70",  ATTR{idVendor}=="10ce",  MODE="666", OWNER="root", GROUP="users"

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

proc is_uniq {list_name} {  ;# make list unique
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
    # this pin is reserved for use with xhc-hb04-util
    if {[string tolower $bname] == "start-pause"} {
      puts stderr "$::progname: skipping button start-pause <$thepin>"
      puts stderr "$::progname: the start-pause pin usage is builtin"
      continue
    }
    # these are used in the ini file examples but aren't real pins
    if {   [string tolower "$thepin"] == "reserved"
        || [string tolower "$thepin"] == "caution"
       } {
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

proc wheel_setup {} {
  # defaults if not in inifile:
  set ::XHC_HB04_CONFIG(coef,0) 1.0
  set ::XHC_HB04_CONFIG(coef,1) 1.0
  set ::XHC_HB04_CONFIG(coef,2) 1.0
  set ::XHC_HB04_CONFIG(coef,3) 1.0
  if [info exists ::XHC_HB04_CONFIG(coefs)] {
    set ::XHC_HB04_CONFIG(coefs) [string trim $::XHC_HB04_CONFIG(coefs) "{}"]
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
    set ::XHC_HB04_CONFIG(scales) [string trim $::XHC_HB04_CONFIG(scales) "{}"]
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
    set coord [string trim [string tolower $coord] -] ;# strip -
    set axno $::XHC_HB04_CONFIG($coord,axno)

    setp pendant_util.coef$idx  $::XHC_HB04_CONFIG(coef,$idx)
    setp pendant_util.scale$idx $::XHC_HB04_CONFIG(scale,$idx)

    set acoord [lindex $anames $idx]
    net pendant:pos-$coord    halui.axis.$axno.pos-feedback \
                           => xhc-hb04.$acoord.pos-absolute
    net pendant:pos-rel-$coord    halui.axis.$axno.pos-relative \
                               => xhc-hb04.$acoord.pos-relative

    net pendant:jog-$coord    xhc-hb04.jog.enable-$acoord \
                           => axis.$axno.jog-enable
    net pendant:jog-scale => axis.$axno.jog-scale

    net pendant:jog-counts                 => pendant_util.in$idx
    net pendant:jog-counts-$coord-filtered <= pendant_util.out$idx \
                                           => axis.$axno.jog-counts
    incr idx
  }

  setp halui.feed-override.scale 0.01
  net pendant:jog-counts  => halui.feed-override.counts

  setp halui.spindle-override.scale 0.01
  net pendant:jog-counts  => halui.spindle-override.counts

  net pendant:jog-speed      halui.jog-speed <= halui.max-velocity.value

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

proc start_pause_button {} {
  # hardcoded setup for button-start-pause
  loadrt xhc_hb04_util names=pendant_util
  addf   pendant_util servo-thread ;# hardcoded thread name
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
} ;# start_pause_button

proc popup_msg {msg} {
  puts stderr "$msg"
  if [catch {package require Tk
             wm withdraw .
             tk_messageBox \
                 -title "$::progname: loadusr fail" \
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

if [catch {loadusr -W xhc-hb04 -I $cfg -H} msg] {
  set msg "\n$::progname: loadusr xhc-hb04:\n<$msg>\n\n"
  set msg "$msg Is it plugged in?\n\n"
  set msg "$msg Are permissions correct?\n\n"
  set msg "$msg Continuing without xhc-hb04\n"
  popup_msg "$msg"
  return ;# not an exit
}

set ct 0; foreach coord {x y z a b c u v w} {
  set ::XHC_HB04_CONFIG($coord,axno) $ct;  incr ct
}

if [info exists ::XHC_HB04_CONFIG(coords)] {
  set ::XHC_HB04_CONFIG(coords) [string trim $::XHC_HB04_CONFIG(coords) "{}"]
  if ![is_uniq $::XHC_HB04_CONFIG(coords)] {
    err_exit "coords must be unique, not: <$::XHC_HB04_CONFIG(coords)>"
  }
} else {
  set ::XHC_HB04_CONFIG(coords) {x y z a} ;# default
}

start_pause_button ;# special handling for this button
connect_pins       ;# per ini file items: [XHC_HB04_BUTTONS]buttonname=pin
wheel_setup        ;# jog wheel per ini file items:
                    #     [XHC_HB04_CONFIG]coords,coefs,scales
#parray ::XHC_HB04_CONFIG
