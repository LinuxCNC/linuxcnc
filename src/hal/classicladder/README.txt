
CLASSIC LADDER PROJECT
Copyright (C) 2001-2004 Marc Le Douarain
marc.le-douarain AT laposte DOT net
http://www.sourceforge.net/projects/classicladder
http://www.multimania.com/mavati/classicladder
February 2001

Version 0.7.3 (28 December 2004)
--------------------------------


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
   * Timers
   * Monostables
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

Since v0.5.3, many rungs can be displayed at the same time, and you can use a vertical scollbar.
The current rung is the rung with the blue bars on each side. It is automatically selected for the moment :
it is the toppest rung which is completely visible.

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
Bxxx : Bit memory xxx (boolean)
Wxxx : Word memory xxx (32 bits integer)
Txx,R : Timer xx running (boolean)
Txx,D : Timer xx done (boolean)
Mxx,R : Monostable xx running (boolean)
Ixxx : Physical input xxx (boolean) - see hardware -
Qxxx : Physical output xxx (boolean) - see hardware -
Xxxx : Activity of step xxx (sequential language)
Xxxx,V : Time of activity in seconds of step xxx (sequential language)

ClassicLadder can run in real-time with RTLinux or RTAI (optional). See below.


REQUIREMENTS...
---------------
For using Classic Ladder as it is, you need :

   * Linux
   * Gtk

I personnaly used the following distribution : Mandrake v9.2 (kernel v2.4.22)

In a console :
type make if you want to recompile the sources.
type ./classicladder to launch this project.
or ./classicladder xxxxxxxx to start with using project datas xxxxxxxx.

Ensure you've the package libgtk+1.2-devel installed before to compile.


MODBUS SERVER INCLUDED (SLAVE)...
---------------------------------
ClassicLadder has a Modbus/TCP server integrated. Default port is 9502, standard
502 requires that application is launched with root privileges.
List of functions code supported are: 1, 2, 3, 4, 5, 6, 15 and 16.
Modbus bits and words correspondence table is actually not parametrable and correspond
directly to the %B and %W variables.
Infos on modbus protocol are available here:
http://www.modbus.org
http://www.sourceforge.net/projects/jamod
http://www.modicon.com/techpubs/toc7.html


HARDWARE...
-----------
This version of ClassicLadder runs in the EMC2 HAL environment. When the real time
classicladder_rt HAL component is loaded, it exports I/O pins to the HAL which can be
connected to any other HAL pins. The other HAL pins may represent real hardware pins,
if exported from a HAL driver.


KNOWN LIMITATIONS /  BUGS...
----------------------------
* There is actually some problems with (C)all (J)umps coils :
  - you should have only one (J)/(C) coil in each rung, else the others top (J)/(C) coils
    will not be taken into account.
  - if you've a Jump coil in the rung, the coils at the bottom of it are always refreshed even
    if the Jump coil is true (they shouldn't).
* Scan of physical inputs are not filtered.
* If you add an arithmetic expression (eval or compar), and let it blank, its buffer will be
  reused again next time you add one.
* The rung activated, is always the rung which is on the top visible (clicking on another one has
  no effect for now).

  
FUTURE / TO DO...
-----------------
* New words variables : Txx,V Txx,P Mxx,V Mxx,P : timers and monostables presets and
  current values (integers)
* To have the following functions modes for the timers family : TON, TOFF, TP as
  defined in IEC61131. And delete the monostables???
* Perhaps the config file with sizes should be placed directly in each project dir?. So
  in the config window we can modify the values associated to the current project
  edited. Theses new values would be taken into account at the next run of classicladder
  with this project path given at startup.
  Add 'refresh_time' parameter in the config file (actually 100 ms in hard).
* For the library, defining 4 functions pointers for I/O access instead of some duplicated
  code?
* Mnemonics possible for all variables. Adding the '%' caracter at the beginning of all
  official variables names and take a look at the same time to be in conformity with
  IEC61131.
* Add new family blocks: counters.
* Add filter value in ms on each line in config input window.
* Modbus/UDP mode support.
* Your ideas, improvements, bug-fixes (?!?!), etc...


