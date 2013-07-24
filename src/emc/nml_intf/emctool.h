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
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#ifndef EMCTOOL_H
#define EMCTOOL_H

#include "emcpos.h"

/* Tools are numbered 1..CANON_TOOL_MAX, with tool 0 meaning no tool. */
#define CANON_POCKETS_MAX 56	// max size of carousel handled
#define CANON_TOOL_ENTRY_LEN 256	// how long each file line can be

struct CANON_TOOL_TABLE {
    int toolno;
    EmcPose offset;
    double diameter;
    double frontangle;
    double backangle;
    int orientation;
};

#endif
