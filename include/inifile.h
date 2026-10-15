//
// IniFile - Ini-file reader and query class
// Copyright (C) 2026 B.Stultiens
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
//
#ifndef __LINUXCNC_INI_INIFILE_H
#define __LINUXCNC_INI_INIFILE_H

#ifdef __cplusplus
#warning "Including inifile.h in C++ code is inefficient. You should use the C++ API in inifile.hh instead."
#endif

#include <limits.h>
#include <stddef.h>
#include <stdbool.h>

#include <rtapi_stdint.h>

//
// C-API interface functions
//
#ifdef __cplusplus
extern "C" {
#endif

// There is no real limit in the C++ version. This value is for compatibility.
// It has been increased from the original 256 to PATH_MAX to allow for full
// paths to be properly encapsulated.
#define INI_MAX_LINELEN PATH_MAX

int TildeExpansion(const char *file, char *path, size_t size);

int iniFindString(const char *inipath, const char *tag, const char *section, char *buf, size_t bufsize);
int iniFindBool(const char *inipath, const char *tag, const char *section, bool *result);
int iniFindSInt(const char *inipath, const char *tag, const char *section, rtapi_s64 *result);
int iniFindUInt(const char *inipath, const char *tag, const char *section, rtapi_u64 *result);
int iniFindDouble(const char *inipath, const char *tag, const char *section, double *result);

// Compatibility with existing code
// Maps to iniFindSInt() and truncates the result
int iniFindInt(const char *inipath, const char *tag, const char *section, int *result);

#ifdef __cplusplus
}
#endif

#endif
// vim: ts=4 sw=4
