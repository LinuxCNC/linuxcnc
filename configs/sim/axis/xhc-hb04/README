These sim configurations demonstrate use of an XHC-HB04 wireless MPG pendant in Machinekit using a HAL module created by Frederic Rible (frible@teaser.fr) (Copyright (C) 2013).  Ref:

http://wiki.linuxcnc.org/cgi-bin/wiki.pl?Using_A_XHC-HB04_Wireless_MPG_Pendant

The HAL module is designed for the XHC-HB04 V3.0 identified as USB device 10CE:EB70.
Most of the pendant features are handled:
- buttons converted to hal pins
- jog wheel with variable scale (use stepsize-up pin to change in sequence)
- sequences provided are:
     (0.001,0.010,0.100,1.000)  typ for mm-base machine
     and
     (0.001,0.005,0.010,0.020)  typ for inch-base machine
- LCD screen displays machine and relative positions

Sim configurations are provided for two known button configurations:

layout1 -- 16 buttons
layout2 -- 18 buttons (more common)

The button names are defined in the files xhc-hb04-layout*.cfg in a stanza labeled XHC-HB04 with definitions as BUTTON=XX:name where XX is a hexadecimal code used by the device.

Example:
   [XHC-HB04]
   BUTTON=01:button-stop
   BUTTON=03:button-rewind
   ...

A hal pin (Type=bit, Dir=OUT) is created for each button, for example:
   xhc-hb04.button-stop
   xhc-hb04.button-rewind
   ...

Additional hal pins are created by the xhc-hb04 driver and connected as required by the halfile included for the demo configurations. 

A working and connected xhc-hb04 device is required for the demonstration configurations.  If not connected, the sim configs will start without pendant functionality.

A udev rules file is added to the system (with a deb install) to give correct user permissions for USB access.

If using a Run-In-Place (RIP) build, create the file /etc/udev/rules.d/90-xhc.rules with the following single line:
ATTR{idProduct}=="eb70", ATTR{idVendor}=="10ce", MODE="666", OWNER="root", GROUP="users"

The sim configurations use a single halfile: xhc-hb04.tcl.  This halfile uses a custom comp file (xhc_hb04_util.comp) and can be added to existing configurations by adding an ini file entry for:
  [HAL]HALFILE=xhc-hb04.tcl

This HALFILE entry should follow other HALFILE entries.

Optional configuration ini file items and the defaults values are:

[XHC_HB04_CONFIG]
layout     = 2       (1: 16 buttons | 2: 18 buttons)
coords     = x y z a (4 max)
coefs      = 1 1 1 1 (filter coefs 0 < coef < 1)
scales     = 1 1 1 1 (plus/minus factors)
threadname = servo-thread
require_pendant = yes (yes | no)
jogmode    = normal   (normal | vnormal | plus-minus)
sequence   = 1        (1|2)
                      1: 0.001,0.010,0.100,1.000 (typ for mm machine)
                      2: 0.001,0.005,0.010,0.020 (typ for inch machine)

The filter coefs can be used to slow the response to wheel steps but are usually not needed since the xhc-hb04 component implements smoothing.

The movement per wheel increment is normally controlled by connecting the STEP button to the hal pin xhc-hb04.stepsize-up. Each press of the STEP button will cause the wheel increment to increase according to the sequence specified.  The sequence 
starts over after the maximum value is reached.

The scale factor for each axis can be set with the scales item.

When require_pendant = no, the xhc-hb04 hal pins will be created even if the pendant is not connected at startup.  A new connection, a disconnect, and a reconnect are supported.
----------------------------------
jogmodes:
  normal: postion mode, the axis will move exactly regardless of how long it takes.  Beware: the machine may keep moving after you stop rotating the jog wheel.

  vnormal: velocity mode, the axis will stop when the wheel stops.  Beware: the amount moved per wheel detent may be less than specified if the machine velocity and acceleration cannot accomplish the move in time.

  plus-minus: halui plus-minus jogging (may be useful for world-mode jogging in non-trivkins machines).
----------------------------------
sequence: Typically use 1 for mm-based machine, 2 for inch-based machine

----------------------------------
Pendant buttons are connected to hal pins with ini file items.
Examples:
[XHC_HB04_BUTTONS]
stop        = halui.program.stop
goto-zero   = halui.mdi-command-00
step        = xhc-hb04.stepsize-up
rewind      = halui.program.step
macro-1     = halui.mdi-command-01
mode        = ""  (placeholder)

With the above items, connections are made as:
  Signal_name       Source pin                   Destination pin
  ----------------- ---------------------------- ------------------------
  pendant:stop      <== xhc-hb04.button-stop      ==> halui.program.stop
  pendant:goto-zero <== xhc-hb04.button-goto-zero ==> halui.mdi-command-00
  pendant:step      <== xhc-hb04.button-step      ==> xhc-hb04.stepsize-up
  etc ...


Note: halui MDI commands are defined in a [HALUI] stanza:
[HALUI]
MDI_COMMAND = G0 X0 Y0 Z0
MDI_COMMAND = M101

The commands are assigned to pins (Type=bit, Dir=IN) named halui.mdi-command-nn whre
nn corresponds to their ordered appearance beginning with nn=00.  (man halui for more information)
----------------------------------
A halfile named monitor_xhc-hb04.tcl is included to monitor disconnects and reconnects of the pendant.  This script runs in the background and will pop up a message when the pendant is disconnected or reconnected.  Usage is optional; if used its ini file entry must follow the entry for xhc-hb04.tcl.  (Note: when using require_pendant=1, disconnects are detected but reconnects are not supported)
----------------------------------

An xhc-hb04 pendant can be added to existing configurations.
  1) copy files to the configuration directory:
     $ cp xhc-hb04.tcl         your_config_dir/
     $ cp monitor_xhc-hb04.tcl your_config_dir/
     $ cp xhc-hb04-layout*.cfg your_config_dir/
  2) Edit existing configuration ini file to:
    a) add [HAL]HALFILE=xhc-hb04.tcl
    b) add [HAL]HALFILE=monitor_xhc-hb04.tcl
    c) add stanza [XHC-HB04_CONFIG]
    d) add stanza [XHC-HB04_BUTTONS]
    e) add or update stanza for [HALUI]

See the demo ini files for more detailed examples.
