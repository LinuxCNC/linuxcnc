/********************************************************************
* Description: interp_array.cc
*
*   This file just allocates space for the static arrays used by the
*   interpreter.
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
********************************************************************/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "rtapi_math.h"
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "rs274ngc.hh"
#include "rs274ngc_return.hh"
#include "rs274ngc_interp.hh"
#include "interp_internal.hh"	// interpreter private definitions

/* Interpreter global arrays for g_codes and m_codes. The nth entry
in each array is the modal group number corresponding to the nth
code. Entries which are -1 represent illegal codes. Remember g_codes
in this interpreter are multiplied by 10.

The modal g groups and group numbers defined in [NCMS, pages 71 - 73]
(see also [Fanuc, pages 43 - 45]) are used here, except the canned
cycles (g80 - g89), which comprise modal g group 9 in [Fanuc], are
treated here as being in the same modal group (group 1) with the
straight moves and arcs (g0, g1, g2,g3).  [Fanuc, page 45] says only
one g_code from any one group may appear on a line, and we are
following that rule. The straight_probe move, g38.2, is in group 1; it
is not defined in [NCMS].

Some g_codes are non-modal (g4, g10, g28, g30, g53, g92, g92.1, g92.2,
and g92.3 here - many more in [NCMS]). [Fanuc] and [NCMS] put all
these in the same group 0, so we do also. Logically, there are two
subgroups, those which require coordinate values (g10, g28, g30, and
g92) and those which do not (g4, g53, g92.1, g92.2, and g92.3).
The subgroups are identified by itemization when necessary.

Those in group 0 which require coordinate values may not be on the
same line as those in group 1 (except g80) because they would be
competing for the coordinate values. Others in group 0 may be used on
the same line as those in group 1.

A total of 52 G-codes are implemented.

The groups are:
group  0 = {g4,g10,g28,g30,g52,g53,g92,g92.1,g92.2,g92.3} - NON-MODAL
            dwell, setup, return to ref1, return to ref2,
            local coordinate system, motion in machine coordinates,
            set and unset axis offsets
group  1 = {g0,g1,g2,g3,g33,g33.1,g38.2,g38.3,g38.4,g38.5,g73,g76,g80,
            g81,g82,g83,g84,g85,g86,g87,g88,g89} - motion
group  2 = {g17,g17.1,g18,g18.1,g19,g19.1}   - plane selection
group  3 = {g90,g91}       - distance mode
group  4 = {g90.1,g91.1}   - arc IJK distance mode
group  5 = {g93,g94,g95}   - feed rate mode
group  6 = {g20,g21}       - units
group  7 = {g40,g41,g42}   - cutter diameter compensation
group  8 = {g43,g49}       - tool length offset
group 10 = {g98,g99}       - return mode in canned cycles
group 12 = {g54,g55,g56,g57,g58,g59,g59.1,g59.2,g59.3} - coordinate system
group 13 = {g61,g61.1,g64} - control mode (path following)
group 14 = {g96,g97}       - spindle speed mode
group 15 = {G07,G08}       - lathe diameter mode
*/
// This stops indent from reformatting the following code.
// *INDENT-OFF*
const int Interp::_gees[] = {
/*   0 */   1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/*  20 */   1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/*  40 */ //0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/*  40 */   0,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1, 1, 1, 0,-1,-1,-1,-1,-1,-1,
/*  60 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,15,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/*  80 */  15,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 100 */   0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 120 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 140 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 160 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 2, 2,-1,-1,-1,-1,-1,-1,-1,-1,
/* 180 */   2, 2,-1,-1,-1,-1,-1,-1,-1,-1, 2, 2,-1,-1,-1,-1,-1,-1,-1,-1,
/* 200 */   6,-1,-1,-1,-1,-1,-1,-1,-1,-1, 6,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 220 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 240 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 260 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 280 */   0, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 300 */   0, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 320 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1, 1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 340 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 360 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 380 */  -1,-1, 1, 1, 1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 400 */   7,-1,-1,-1,-1,-1,-1,-1,-1,-1, 7, 7,-1,-1,-1,-1,-1,-1,-1,-1,
/* 420 */   7, 7,-1,-1,-1,-1,-1,-1,-1,-1, 8, 8,-1,-1,-1,-1,-1,-1,-1,-1,
/* 440 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 460 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 480 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 8,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 500 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 520 */   0,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 540 */  12,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 560 */  12,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 580 */  12,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,12,12,-1,-1,-1,-1,-1,-1,
/* 600 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,13,13,-1,-1,-1,-1,-1,-1,-1,-1,
/* 620 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 640 */  13,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 660 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 680 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 700 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 720 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 740 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 760 */   1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 780 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 800 */   1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 820 */   1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 840 */   1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 860 */   1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 880 */   1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 900 */   3, 4,-1,-1,-1,-1,-1,-1,-1,-1, 3, 4,-1,-1,-1,-1,-1,-1,-1,-1,
/* 920 */   0, 0, 0, 0,-1,-1,-1,-1,-1,-1, 5,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 940 */   5,-1,-1,-1,-1,-1,-1,-1,-1,-1, 5,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 960 */  14,-1,-1,-1,-1,-1,-1,-1,-1,-1,14,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 980 */  10,-1,-1,-1,-1,-1,-1,-1,-1,-1,10,-1,-1,-1,-1,-1,-1,-1,-1,-1};

/*

Modal groups and modal group numbers for M codes are not described in
[Fanuc]. We have used the groups from [NCMS] and added M60, as an
extension of the language for pallet shuttle and stop. This version has
no codes related to axis clamping.

The groups are:
group 4 = {m0,m1,m2,m30,m60} - stopping
group 5 = {m62,m63,m64,m65,  - turn I/O point on/off
           m66}              - wait for Input
group 6 = {m6,m61}           - tool change
group 7 = {m3,m4,m5,m19}     - spindle turning, orient
group 8 = {m7,m8,m9}         - coolant
group 9 = {m48,m49,          - feed and speed override switch bypass
           m50,              - feed override switch bypass           P1 to turn on, P0 to turn off
	   m51,              - spindle speed override switch bypass  P1 to turn on, P0 to turn off
	   m52,              - adaptive feed override switch bypass  P1 to turn on, P0 to turn off
	   m53}              - feedstop override switch bypass       P1 to turn on, P0 to turn off
group 10 = {m100..m199}      - user-defined
*/

const int Interp::_ems[] = {
   4,  4,  4,  7,  7,  7,  6,  8,  8,  8,  //  9
  -1, -1, -1, -1, -1, -1, -1, -1, -1,  7,  // 19
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 29
   4, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 39
  -1, -1, -1, -1, -1, -1, -1, -1,  9,  9,  // 49
   9,  9,  9,  9, -1, -1, -1, -1, -1, -1,  // 59
   4,  6,  5,  5,  5,  5,  5,  5,  5, -1,  // 69
   7,  7,  7, 7, -1, -1, -1, -1, -1, -1,  // 79
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 89
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  // 99
   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, //109
   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, //119
   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, //129
   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, //139
   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, //149
   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, //159
   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, //169
   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, //179
   10, 10, 10, 10, 10, 10, 10, 10, 10, 10, //189
   10, 10, 10, 10, 10, 10, 10, 10, 10, 10};//199

/*

This is an array of the index numbers of system parameters that must
be included in a file used with the Interp::restore_parameters
function. The array is used by that function and by the
Interp::save_parameters function.

*/

const int Interp::_required_parameters[] = {
 5161, 5162, 5163,   /* G28 home */
 5164, 5165, 5166, /* A, B, & C */
 5167, 5168, 5169, /* U, V, & W */
 5181, 5182, 5183,   /* G30 home */
 5184, 5185, 5186, /* A, B, & C */
 5187, 5188, 5189, /* U, V, & W */
 5210, /* G92 is currently applied */
 5211, 5212, 5213,   /* G92 offsets */
 5214, 5215, 5216, /* A, B, & C */
 5217, 5218, 5219, /* U, V, & W */
 5220,               /* selected coordinate */
 5221, 5222, 5223,   /* coordinate system 1 */
 5224, 5225, 5226, /* A, B, & C */
 5227, 5228, 5229, /* U, V, & W */
 5230,
 5241, 5242, 5243,   /* coordinate system 2 */
 5244, 5245, 5246, /* A, B, & C */
 5247, 5248, 5249, /* U, V, & W */
 5250,
 5261, 5262, 5263,   /* coordinate system 3 */
 5264, 5265, 5266, /* A, B, & C */
 5267, 5268, 5269, /* U, V, & W */
 5270,
 5281, 5282, 5283,   /* coordinate system 4 */
 5284, 5285, 5286, /* A, B, & C */
 5287, 5288, 5289, /* U, V, & W */
 5290,
 5301, 5302, 5303,   /* coordinate system 5 */
 5304, 5305, 5306, /* A, B, & C */
 5307, 5308, 5309, /* U, V, & W */
 5310,
 5321, 5322, 5323,   /* coordinate system 6 */
 5324, 5325, 5326, /* A, B, & C */
 5327, 5328, 5329, /* U, V, & W */
 5330,
 5341, 5342, 5343,   /* coordinate system 7 */
 5344, 5345, 5346, /* A, B, & C */
 5347, 5348, 5349, /* U, V, & W */
 5350,
 5361, 5362, 5363,   /* coordinate system 8 */
 5364, 5365, 5366, /* A, B, & C */
 5367, 5368, 5369, /* U, V, & W */
 5370,
 5381, 5382, 5383,   /* coordinate system 9 */
 5384, 5385, 5386, /* A, B, & C */
 5387, 5388, 5389, /* U, V, & W */
 5390,
 RS274NGC_MAX_PARAMETERS
};

const int Interp::_readonly_parameters[] = {
 5400, // tool toolno
 5401, // tool x offset
 5402, // tool y offset
 5403, // tool z offset
 5404, // tool a offset
 5405, // tool b offset
 5406, // tool c offset
 5407, // tool u offset
 5408, // tool v offset
 5409, // tool w offset
 5410, // tool diameter
 5411, // tool frontangle
 5412, // tool backangle
 5413, // tool orientation
 5420, 5421, 5422, 5423, 5424, 5425, 5426, 5427, 5428, // current X Y ... W
};
const int Interp::_n_readonly_parameters = sizeof(_readonly_parameters)/sizeof(int);

/* _readers is an array of pointers to functions that read.
   It is used by read_one_item.

   Each read function is placed in the array according to the ASCII character it
   corresponds to. Whilst a switch statement could have been used in read_one_item,
   using an array of function pointers allows a new read_foo to be added quickly
   in this one table.
   
   At some point, it may be advantageous to add a read_$ or read_n for perhaps
   macro or jump labels..
   */
const read_function_pointer Interp::default_readers[256] = {
/* 00 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/* 10 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
/* 20 */
0, 0, 0,
&Interp::read_parameter_setting, // reads # or ASCII 0x23
0, 0, 0, 0,
&Interp::read_comment, // reads ( or ASCII 0x28
0, 0, 0, 0, 0, 0, 0, 
/* 30 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
&Interp::read_semicolon, 
0, 0, 0, 0,
/* 40 */
&Interp::read_atsign, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
/* 50 */
0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &Interp::read_carat, 0,
/* 60 */
0,
&Interp::read_a, // reads a or ASCII 0x61
&Interp::read_b, // reads b or ASCII 0x62
&Interp::read_c, // reads c or ASCII 0x63
&Interp::read_d, // reads d or ASCII 0x64
&Interp::read_e, // reads d or ASCII 0x65
&Interp::read_f, // reads f or ASCII 0x66
&Interp::read_g, // reads g or ASCII 0x67
&Interp::read_h, // reads h or ASCII 0x68
&Interp::read_i, // reads i or ASCII 0x69
&Interp::read_j, // reads j or ASCII 0x6A
&Interp::read_k, // reads k or ASCII 0x6B
&Interp::read_l, // reads l or ASCII 0x6C
&Interp::read_m, // reads m or ASCII 0x6D
0, 0,
&Interp::read_p, // reads p or ASCII 0x70
&Interp::read_q, // reads q or ASCII 0x71
&Interp::read_r, // reads r or ASCII 0x72
&Interp::read_s, // reads s or ASCII 0x73
&Interp::read_t, // reads t or ASCII 0x74
&Interp::read_u,
&Interp::read_v,
&Interp::read_w,
&Interp::read_x, // reads x or ASCII 0x78
&Interp::read_y, // reads y or ASCII 0x79
&Interp::read_z}; // reads z or ASCII 0x7A
// *INDENT-ON*
// And now indent can continue.
/****************************************************************************/

/* There are four global variables*. The first three are _gees, _ems,
and _readers. */

/* The notion of "global variables" is a misnomer - These last four should only
   be accessable by the interpreter and not exported to the rest of emc */


