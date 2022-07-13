//    Copyright 2013 Jeff Epler <jepler@unpythonic.net>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#ifndef EMCTOOL_H
#define EMCTOOL_H

#include "emcpos.h"

/* pocketno: 0..(CANON_POCKETS_MAX-1) (0: spindle)
** toolno:   no restrictions          (0: notool)
*/
#define CANON_POCKETS_MAX 1001	// max size of carousel handled
#define CANON_TOOL_ENTRY_LEN 256	// how long each file line can be

struct CANON_TOOL_TABLE {
    int toolno;
    int pocketno;
    EmcPose offset;
    double diameter;
    double frontangle;
    double backangle;
    int orientation;
};

#endif
