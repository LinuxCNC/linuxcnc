//    Copyright (C) 2009-2010 Jeff Epler <jepler@unpythonic.net>
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
#ifndef TOOL_PARSE_H
#define TOOL_PARSE_H

#include "emctool.h"
#ifdef CPLUSPLUS
extern "C"  {
#endif

int loadToolTable(const char *filename,
	struct CANON_TOOL_TABLE toolTable[CANON_POCKETS_MAX],
	char *ttcomments[CANON_POCKETS_MAX],
	int random_toolchanger
	);

int saveToolTable(const char *filename,
	CANON_TOOL_TABLE toolTable[],
	char *ttcomments[CANON_POCKETS_MAX],
	int random_toolchanger
	);

#ifdef CPLUSPLUS
}
#endif

#endif
