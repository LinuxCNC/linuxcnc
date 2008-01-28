

CLASSIC LADDER PROJECT
Copyright (C) 2001-2006 Marc Le Douarain
marc . le - douarain /AT\ laposte \DOT/ net
http://www.sourceforge.net/projects/classicladder
http://www.multimania.com/mavati/classicladder
February 2001

Version 0.7.100 (4 November 2006)
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
   * Timers
   * Monostables
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
ex: "MyTimer" => %T0 (that can not be used directly, but by adding
the attribute wanted: MyTimer.D (= %T0.D)


ClassicLadder can run in real-time with RTLinux or RTAI (optional). See below.


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
* There is actually some problems with (C)all (J)umps coils :
  - you should have only one (J)/(C) coil in each rung, else the others top (J)/(C) coils
    will not be taken into account.
  - if you've a Jump coil in the rung, the coils at the bottom of it are always refreshed even
    if the Jump coil is true (they shouldn't).
* Scan of physical inputs are not filtered.
* If you add an arithmetic expression (eval or compar), and let it blank, its buffer will be
  reused again next time you add one.
* Modbus master for distributed I/O recently developped, should be use with caution...
  Generally, the logic used on a PLC is: get inputs, apply logic (rungs/grafcet), and then
  set ouputs, but it is absolutely not the case with distributed I/O. Perhaps it could cause
  some troubles...?


FUTURE / TO DO LIST...
----------------------
see file TODO.txt

