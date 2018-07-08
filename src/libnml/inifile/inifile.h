//    Header for C-linkage apis in liblinuxcncini
//    Copyright (C) 2012 Jeff Epler
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

#ifndef LINUXCNC_INIFILE_H
#define LINUXCNC_INIFILE_H

#include <stdio.h>
#include <fcntl.h>

#ifdef __cplusplus
extern "C"
{
#endif

extern const char *iniFind(FILE *fp, const char *tag, const char *section);
extern const int iniFindInt(FILE *fp, const char *tag, const char *section, int *result);
extern const int iniFindDouble(FILE *fp, const char *tag, const char *section, double *result);
extern int TildeExpansion(const char *file, char *path, size_t size);

#ifdef __cplusplus
}
#endif
#endif
