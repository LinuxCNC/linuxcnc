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

#ifndef DBUF_H
#define DBUF_H
#include "rtapi_string.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dbuf {
    size_t sz;
    unsigned char *data;
};

struct dbuf_iter {
    struct dbuf *base;
    size_t offset;
};

struct dbuf *dbuf_new(unsigned sz);
void dbuf_delete(struct dbuf *d);
void dbuf_init(struct dbuf *d, unsigned char *data, unsigned sz);

struct dbuf_iter *dbuf_iter_new(struct dbuf *d);
void dbuf_iter_delete(struct dbuf_iter *di);
void dbuf_iter_init(struct dbuf_iter *di, struct dbuf *d);

int dbuf_put_byte(struct dbuf_iter *di, unsigned char data);
int dbuf_put_bytes(struct dbuf_iter *di, const unsigned char *data, unsigned sz);
int dbuf_put_int(struct dbuf_iter *di, int i);
int dbuf_put_long(struct dbuf_iter *di, long i);
int dbuf_put_float(struct dbuf_iter *di, float i);
int dbuf_put_double(struct dbuf_iter *di, double i);
int dbuf_put_string(struct dbuf_iter *di, const char *s);

int dbuf_get_byte(struct dbuf_iter *di, unsigned char *b);
int dbuf_get_bytes(struct dbuf_iter *di, unsigned char *data, unsigned sz);
int dbuf_get_int(struct dbuf_iter *di, int *i);
int dbuf_get_long(struct dbuf_iter *di, long *i);
int dbuf_get_float(struct dbuf_iter *di, float *i);
int dbuf_get_double(struct dbuf_iter *di, double *i);
int dbuf_get_string(struct dbuf_iter *di, const char **s);

#ifdef __cplusplus
}
#endif
#endif
