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
*
* Last change:
* $Revision$
* $Author$
* $Date$
********************************************************************/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "rs274ngc.hh"
#include "rs274ngc_return.hh"
#include "rs274ngc_errors.cc"

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
group  0 = {g4,g10,g28,g30,g53,g92,g92.1,g92.2,g92.3} - NON-MODAL
            dwell, setup, return to ref1, return to ref2,
            motion in machine coordinates, set and unset axis offsets
group  1 = {g0,g1,g2,g3,g33,g38.2,g80,g81,g82,g83,g84,g85,g86,g87,g88,g89} - motion
group  2 = {g17,g18,g19}   - plane selection
group  3 = {g90,g91}       - distance mode
group  5 = {g93,g94}       - feed rate mode
group  6 = {g20,g21}       - units
group  7 = {g40,g41,g42}   - cutter diameter compensation
group  8 = {g43,g49}       - tool length offset
group 10 = {g98,g99}       - return mode in canned cycles
group 12 = {g54,g55,g56,g57,g58,g59,g59.1,g59.2,g59.3} - coordinate system
group 13 = {g61,g61.1,g64} - control mode

*/
// This stops indent from reformatting the following code.
// *INDENT-OFF*
const int Interp::_gees[] = {
/*   0 */   1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/*  20 */   1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/*  40 */   0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/*  60 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/*  80 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 100 */   0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 120 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 140 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 160 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 180 */   2,-1,-1,-1,-1,-1,-1,-1,-1,-1, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 200 */   6,-1,-1,-1,-1,-1,-1,-1,-1,-1, 6,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 220 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 240 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 260 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 280 */   0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 300 */   0,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 320 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 340 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 360 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 380 */  -1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 400 */   7,-1,-1,-1,-1,-1,-1,-1,-1,-1, 7,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 420 */   7,-1,-1,-1,-1,-1,-1,-1,-1,-1, 8,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 440 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 460 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 480 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 8,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 500 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 520 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 540 */  12,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 560 */  12,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 580 */  12,-1,-1,-1,-1,-1,-1,-1,-1,-1,12,12,12,12,-1,-1,-1,-1,-1,-1,
/* 600 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,13,13,-1,-1,-1,-1,-1,-1,-1,-1,
/* 620 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 640 */  13,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 660 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 680 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 700 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 720 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 740 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 760 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 780 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 800 */   1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 820 */   1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 840 */   1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 860 */   1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 880 */   1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 900 */   3,-1,-1,-1,-1,-1,-1,-1,-1,-1, 3,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 920 */   0, 0, 0, 0,-1,-1,-1,-1,-1,-1, 5,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 940 */   5,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 960 */  -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
/* 980 */  10,-1,-1,-1,-1,-1,-1,-1,-1,-1,10,-1,-1,-1,-1,-1,-1,-1,-1,-1};

/*

Modal groups and modal group numbers for M codes are not described in
[Fanuc]. We have used the groups from [NCMS] and added M60, as an
extension of the language for pallet shuttle and stop. This version has
no codes related to axis clamping.

The groups are:
group 4 = {m0,m1,m2,m30,m60} - stopping
group 5 = {m62,m63,m64,m65}  - turn I/O point on/off
group 6 = {m6}               - tool change
group 7 = {m3,m4,m5}         - spindle turning
group 8 = {m7,m8,m9}         - coolant
group 9 = {m48,m49}          - feed and speed override switch bypass
group 100+ = {m100..m199}    - user-defined
*/

const int Interp::_ems[] = {
   4,  4,  4,  7,  7,  7,  6,  8,  8,  8,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   4, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1,  9,  9,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   4, -1,  5,  5,  5,  5, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
  -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
   110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
   120, 121, 122, 123, 124, 125, 126, 127, 128, 129,
   130, 131, 132, 133, 134, 135, 136, 137, 138, 139,
   140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
   150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
   160, 161, 162, 163, 164, 165, 166, 167, 168, 169,
   170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
   180, 181, 182, 183, 184, 185, 186, 187, 188, 189,
   190, 191, 192, 193, 194, 195, 196, 197, 198, 199};

/*

This is an array of the index numbers of system parameters that must
be included in a file used with the rs274ngc_restore_parameters
function. The array is used by that function and by the
rs274ngc_save_parameters function.

*/

const int Interp::_required_parameters[] = {
 5161, 5162, 5163,   /* G28 home */
 5164, 5165, 5166, /* A, B, & C */
 5181, 5182, 5183,   /* G30 home */
 5184, 5185, 5186, /*A, B, & C */
 5211, 5212, 5213,   /* G92 offsets */
 5214, 5215, 5216, /*A, B. & C */
 5220,               /* selected coordinate */
 5221, 5222, 5223,   /* coordinate system 1 */
 5224, 5225, 5226, /* A, B, & C */
 5241, 5242, 5243,   /* coordinate system 2 */
 5244, 5245, 5246, /* A, B, & C */
 5261, 5262, 5263,   /* coordinate system 3 */
 5264, 5265, 5266, /* A, B, & C */
 5281, 5282, 5283,   /* coordinate system 4 */
 5284, 5285, 5286, /* A, B, & C */
 5301, 5302, 5303,   /* coordinate system 5 */
 5304, 5305, 5306, /* A, B, & C */
 5321, 5322, 5323,   /* coordinate system 6 */
 5324, 5325, 5326, /* A, B, & C */
 5341, 5342, 5343,   /* coordinate system 7 */
 5344, 5345, 5346, /* A, B, & C */
 5361, 5362, 5363,   /* coordinate system 8 */
 5364, 5365, 5366, /* A, B, & C */
 5381, 5382, 5383,   /* coordinate system 9 */
 5384, 5385, 5386, /* A, B, & C */
 RS274NGC_MAX_PARAMETERS
};

/*

_readers is an array of pointers to functions that read.
It is used by read_one_item.

*/
const read_function_pointer Interp::_readers[] = {
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0, 0, 0, read_parameter_setting,0,      0,      0,      0,
read_comment, 0, 0,     0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
0,      0,      0,      0,      0,      0,      0,      read_a, read_b, read_c,
read_d, 0,      read_f, read_g, read_h, read_i, read_j, read_k, read_l, read_m,
0,      0,      read_p, read_q, read_r, read_s, read_t, 0     , 0,      0,
read_x, read_y, read_z};
// *INDENT-ON*
// And now indent can continue.
/****************************************************************************/

/* There are four global variables*. The first three are _gees, _ems,
and _readers. The last one, declared here, is for interpreter settings */

setup Interp::_setup;

/* The notion of "global variables" is a misnomer - These last four should only
   be accessable by the interpreter and not exported to the rest of emc */


