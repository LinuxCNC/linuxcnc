//    This is a component of emc
//    Copyright © 2009 Jeff Epler
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

#ifndef STASHF_H
#define STASHF_H
#include <stdarg.h>
#ifndef RTAPI
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct dbuf_iter;

int vstashf(struct dbuf_iter *o, const char *fmt, va_list ap);
int stashf(struct dbuf_iter *o, const char *fmt, ...);
int snprintdbuf(char *buf, int n, struct dbuf_iter *o);
#ifndef RTAPI
int printdbuf(struct dbuf_iter *o);
int fprintdbuf(FILE *f, struct dbuf_iter *o);
#endif

#ifdef __cplusplus
}
#endif
#endif
