

CLASSIC LADDER PROJECT
Copyright (C) 2001-2007 Marc Le Douarain
marc . le - douarain /AT\ laposte \DOT/ net
http://www.sourceforge.net/projects/classicladder
http://www.multimania.com/mavati/classicladder
February 2001

Version 0.7.123 (4 November 2007)
---------------------------------


A project to have a free ladder language in C.
Generally, you find this type of language on PLC to make the programs.
It allows you to realize little programs or bigger in an electric way.

This project is released under the terms of the LGPL licence.


GENESIS...
----------

I decided to program a ladder language only for test purposes at the start,
in february 2001. It was planned, that I would have to participate to a
new controller at work. And I was thinking that to have a ladder
language in it could be a nice option to considerate. And so I started to
code the first lines for calculating a rung with minimal elements and
displaying dynamically it under Gtk, to see if my first idea to realise all
this works.
And as quickly I've found that it advanced quite well, I've continued with
more complex elements : timer, multiples rungs, etc...
Voilà, here is this work...
And for the controller at job? Well, in fact, it's already a lot of work to
do the basic features of the traffic lights and I don't think we will add
a ladder language to it one day... ;-)


FEATURES...
-----------

Classic Ladder is coded 100% in C.
It can be used for educational purposes or anything you want...
The graphical user interface uses GTK.

In the actual version, the following elements are implemented :

   * Booleans elements
   * Rising / falling edges
   * New IEC Timers (since v0.7.120)
   * Timers (for compatibility, no more really usefull)
   * Monostables (for compatibility, no more really usefull)
   * Counters (since v0.7.80)
   * Compare of arithmetic expressions (since v0.4)

   * Booleans outputs
   * Set / Reset coils
   * Jumps
   * Calls to sub-routines (since v0.5.5)
   * Operate of arithmetic expressions (since v0.4)

Each rung can have a label (used for jumps) and a comment.
They are displayed at the top for the current rung.

There is a full editor for rungs since v0.3
You can:
    - add a rung after the current rung.
    - insert a rung before the current rung.
    - modify the current rung.
    - delete the current rung.
When editing, use the pointer of the toolbar if you want only to modify properties, else select the element
you want to place in the rung.

Many rungs can be displayed at the same time, and you can use a vertical scollbar.
The current rung is the rung with the blue bars on each side. When window resized, it is automatically
selected by choosing the toppest rung which is completely visible. You can manually select one between the
others below (if you can see them with a large vertical window).


Since v0.5.5, the program can be split in many sections.
There is a window 'Sections manager' where you can see all the sections defined, add/delete a section,
and modify the order executions of the main sections.
There is two types of sections available : main or sub-routine (SR).
The main sections are refreshed directly in the following order : top sections first, bottom sections last.
The sub-routines sections are refreshed when called from a rung. Each sub-routine (SR) has a unique number
used in the (C)all coils.
Load & Run "example_many_sections" for a demo example.

Since v0.6.0, a new language has been integrated : a "sequential" also called "grafcet".
In the section manager, select the language wanted when you add a section.
Load & Run "example_sequential" for a demo example.
The editor for sequential is arrived in v0.7.0
When using the link button ('\.) in the toolbar, when clicking on transition, the top or
bottom part of it is very important !!!


List of known variables :
%Bxxx : Bit memory xxx (boolean)
%Wxxx : Word memory xxx (32 bits integer)
%Txx.R : Timer xx running (boolean, user read only)
%Txx.D : Timer xx done (boolean, user read only)
%Txx.V : Timer xx current value (integer, user read only)
%Txx.P : Timer xx preset (integer)
%Mxx.R : Monostable xx running (boolean)
%Mxx.V : Monostable xx current value (integer, user read only)
%Mxx.P : Monostable xx preset (integer)
%Cxx.D : Counter xx done (boolean, user read only)
%Cxx.E : Counter xx empty overflow (boolean, user read only)
%Cxx.F : Counter xx full overflow (boolean, user read only)
%Cxx.V : Counter xx current value (integer, user read only)
%Cxx.P : Counter xx preset (integer)
%TMxx.Q : IEC Timer xx output (boolean, user read only)
%TMxx.V : IEC Timer xx current value (integer, user read only)
%TMxx.P : IEC Timer xx preset (integer)
%Ixxx : Physical input xxx (boolean) - see hardware -
%Qxxx : Physical output xxx (boolean) - see hardware -
%Xxxx : Activity of step xxx (sequential language)
%Xxxx.V : Time of activity in seconds of step xxx (sequential language)

Each variable can be associated to a symbol name that can be then used instead of the
real variable name.
Partial symbol or complete symbol are possible.
A complete symbol corresponds directly to a real variable name:
ex: "InputK4" => %I17
A partial symbol is a part of a variable name without an attribute:
ex: "MyTimer" => %TM0 (that can not be used directly, but by adding
the attribute wanted: MyTimer.Q (= %TM0.Q)

In the arithmetic expressions, variables indexed with another one can be used.
Example: %W0[%W4] => if %W4 equals 23 it corresponds to %W23 !


ClassicLadder can run in real-time with RTLinux, RTAI or Xenomai (optional). See below.


REQUIREMENTS...
---------------
For using Classic Ladder as it is, you need :

   * Linux
   * Gtk version 2

I personnaly used the following distribution : Mandriva 2006.

In a console :
type make if you want to recompile the sources.
type ./classicladder to launch this project.
or ./classicladder xxxxxxxx to start with using project datas xxxxxxxx.

Ensure you've the package libgtk+1.2-devel installed before to compile
(no more necessary since v0.7.60 using GTK2).

To compile on PowerPC processor, in the Makefile, add the caracter '#' to comment
the line MAKE_IO_ACCESS.

A Windows port is available, but not with all the i/o features, and in an older
version.


MODBUS SERVER INCLUDED (SLAVE TO CONNECT TO A SCADA)...
-------------------------------------------------------
ClassicLadder has a Modbus/TCP server integrated. Default port is 9502, standard
502 requires that application is launched with root privileges.
List of functions code supported are: 1, 2, 3, 4, 5, 6, 15 and 16.
Modbus bits and words correspondence table is actually not parametrable and correspond
directly to the %B and %W variables.
Infos on modbus protocol are available here:
http://www.modbus.org
http://www.sourceforge.net/projects/jamod
http://www.modicon.com/techpubs/toc7.html


REAL-TIME SUPPORT WITH XENOMAI (IN USER-SPACE)...
-------------------------------------------------
Added in v0.7.92, september 2006.
See http://www.xenomai.org
xeno-config command must be in your path.
Uncomment the corresponding line in the Makefile.
Then type "make clean;make" to compile the project.
With a "./classicladder" you will launch the real-time version !!!
Xenomai rules! And is advised instead of a RTLinux/RTAI kernel module version.
To have real-time in user-land is really easier than with the module, the future
is here, and now !!!


REAL-TIME SUPPORT WITH RTLINUX...
---------------------------------
To have RTLinux v3 installed before is required (see http://www.rtlinux.org)
Be sure that the hello example works after RTLinux installation (perhaps
you will have to type dmesg to verify if the hello texts was correctly displayed).
With this version, the refresh of the rungs is done in real-time.
Verify the symbolic link (or real directory depending how you've installed)
"/usr/rtlinux" pointing on the rtlinux directory, exists.
Here, I've done the following :
cd /usr
ln -sf /usr/src/your-rtlinux+linux-dir/rtlinux-3.1 rtlinux


In a console, type the following to recompile and run :
make clean;make rtl_support
su
./run_rt

run_rt script accepts two optional arguments : first for the name of project to load,
second for a config file with sizes to alloc at startup.

I've tested here the real-time version with RTLinux v3.2pre3 and Linux kernel v2.4.20
compiled with gcc v3.2


REAL-TIME SUPPORT WITH RTAI...
------------------------------
Verify the symbolic link (or real directory depending how you've installed)
"/usr/src/rtai" pointing on the rtai directory, exists.
Here, I've done the following :
ln -sf /usr/src/your-rtai  /usr/src/rtai

In a console, type the following to recompile and run :
make clean
make rtai_support
su
./run_rt

run_rt script accepts two optional arguments : first for the name of project to load,
second for a config file with sizes to alloc at startup.

Tested with 2.4.22 kernel patched with ADEOS and compiled with gcc-3.2 running
on a Debian (Sid) system.


HARDWARE (LOCAL INPUTS/OUTPUTS)...
----------------------------------
Since v0.6.5, hardware interface has been completly rewritten (before limited
to the parallels ports only).
First, if you use ClassicLadder to drive real things, it can crash, and I
will not be responsible if it produces any damages !

You can configure any logical inputs/ouputs with any addresses ports or a
Comedi device (see www.comedi.org). Comedi is a collection of drivers
for many data acquisition boards.
You configure the mapping in the config window : tabs "Inputs" and "Outputs".
In each line, you set the first logical I/O that will be mapped with the
hardware you will set.
For a direct port access, you then give the address in hexa to use.
For a comedi device, you then give the sub-device associated.
After, you set the first channel offset and the number of channels
to map.
For example : I5 mapped with 4 channels means that I5, I6, I7 and I8
are affected to an hardware channel. So be carrefull not to overlap.

Comedi or direct access can works with Linux version or RTLinux version.
For direct access under Linux, the application must be launched as
root, as it used the ioperm( ) functions.

Per default, Comedi support is not compiled. Take a look at the Makefile
and uncomment the COMEDI_SUPPORT line.
Comedi is included since RLinux3.2pre2, but you still must download
the comedilib archive (comedi_config) !!!

Here I've made a test with the builtin parallel port.
You will find the 2 projects : 'parallel_port_direct' and
'parallel_port_comedi'.

Here the pins and I/O map for the direct project:

* -- Parallel port 1 --
Inputs     DB25Pin     ClassicLadder
S3         15          I3
S4         13          I4
S5         12          I5
S6         10          I6
S7         11          I7
Outputs    DB25Pin     Classicladder
D0         2           Q0
D1         3           Q1
D2         4           Q2
D3         5           Q3
D4         6           Q4
D5         7           Q5
D6         8           Q6
D7         9           Q7

I've tested with 3 DELs and 4 switches on the parallel port 1.
The switches on inputs are linked to ground (pin 24) :
           _n_
   (Sx) o--   --O (24)

The DELs are linked in serial with a resistance of 330 ohms to ground (pin 25) :
                      ,
   (Dx) o--[ 330 ]--|>|--o (25)


HARDWARE (DISTRIBUTED INPUTS/OUTPUTS)...
----------------------------------------
ClassicLadder can use distributed inputs/outputs on modules using the modbus
protocol ("master": pooling slaves).
The slaves and theirs I/O can be configured in the config window.
2 exclusive modes are available : ethernet using Modbus/TCP and serial using Modbus/RTU.
For the serial one, the serial port and the speed must be correctly set in the config file
(manually with a text editor), and then you must pass this file to classicladder with -c
argument. No parity is used.
If no port name for serial is set, IP mode will be used...
The slave address is the slave address (Modbus/RTU) or the IP address.
The IP address can be followed per the port number to use (xx.xx.xx.xx:pppp) else
the port 502 will be used per default.
The modbus address element 1 is the first one (0 in the frame).
2 products have been used for tests: a Modbus/TCP one (Adam-6051, http://www.advantech.com)
and a serial Modbus/RTU one (http://www.ipac.ws)
See examples: adam-6051 and modbus_rtu_serial.
Web links: http://www.modbus.org and this interesting one: http://www.iatips.com/modbus.html


EMBEDDED VERSION...
-------------------
If you want to make an embedded version of classicladder without the GTK interface:
comment the GTK_INTERFACE line in the Makefile.
Type 'CRTL-C' if you want to exit the Linux application.

Since v0.6.4 it is possible to define maximum sizes for the differents arrays to save
a lot of memory if you're short on it on your target.
You must give a config file at startup... Well, no since v0.7.110, the sizes defined
are saved in the project file, and are used for the first project file loaded (in the
commande line).


LINKS...
--------
You should take a look at the MAT project. Classicladder has been integrated to this project to
have a relay ladder logic to program this PLC.
http://mat.sourceforge.net/

Another project using ClassicLadder: EMC2, used to control machine tools.
http://www.linuxcnc.org/

Comedi project at www.comedi.org


KNOWN LIMITATIONS /  BUGS...
----------------------------
* Scan of physical inputs are not filtered.
* Modbus master for distributed I/O recently developped, should be use with caution...
  Generally, the logic used on a PLC is: get inputs, apply logic (rungs/grafcet), and then
  set ouputs, but it is absolutely not the case with distributed I/O. Perhaps it could cause
  some troubles...?


FUTURE / TO DO LIST...
----------------------
see file TODO.txt

