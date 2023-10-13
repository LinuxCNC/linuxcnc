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

#ifdef RTAPI
#else
#include <stdlib.h>
#endif
#include "rtapi_string.h"
#include "rtapi_errno.h"
#include "dbuf.h"

typedef struct dbuf dbuf;
typedef struct dbuf_iter dbuf_iter;

#ifndef RTAPI
dbuf *dbuf_new(unsigned sz) {
    dbuf *d = malloc(sizeof(dbuf) + sz);
    if(d) dbuf_init(d, (unsigned char*)(d+1), sz);
    return d;
}
void dbuf_delete(dbuf *d) {
    if(d) free(d);
}
#endif

void dbuf_init(dbuf *d, unsigned char *data, unsigned sz) {
    if(!d) return;
    d->data = data;
    d->sz = sz;
    memset(data, 0, sz);
}

#ifndef RTAPI
dbuf_iter *dbuf_iter_new(dbuf *d) {
    dbuf_iter *di = malloc(sizeof(dbuf_iter));
    if(di) dbuf_iter_init(di, d);
    return di;
}

void dbuf_iter_delete(dbuf_iter *di) {
    if(di) free(di);
}
#endif


void dbuf_iter_init(dbuf_iter *di, dbuf *d) {
    if(!di) return;
    di->base = d;
    di->offset = 0;
}

int dbuf_put_byte(dbuf_iter *di, unsigned char data) {
    dbuf *d;

    if(!di) return -EINVAL;
    d = di->base;

    if(!d) return -EINVAL;

    if(di->offset == d->sz) return -ENOSPC;
    d->data[di->offset++] = data;

    return 0;
}

int dbuf_put_bytes(dbuf_iter *di, const unsigned char *data, unsigned sz) {
    dbuf *d;
    unsigned char *p;

    if(!di) return -EINVAL;

    d = di->base;
    if(!d) return -EINVAL;

    if(d->sz - di->offset < sz) return -ENOSPC;

    p = d->data + di->offset;
    di->offset += sz;

    for(; sz; sz--) {
        *p++ = *data++;
    }
    return sz;
}

int dbuf_put_int(dbuf_iter *di, int i) {
    return dbuf_put_bytes(di, (unsigned char *)&i, sizeof(int));
}

int dbuf_put_long(dbuf_iter *di, long i) {
    return dbuf_put_bytes(di, (unsigned char *)&i, sizeof(long));
}

int dbuf_put_float(dbuf_iter *di, float i) {
    return dbuf_put_bytes(di, (unsigned char *)&i, sizeof(float));
}

int dbuf_put_double(dbuf_iter *di, double i) {
    return dbuf_put_bytes(di, (unsigned char *)&i, sizeof(double));
}

int dbuf_put_string(dbuf_iter *di, const char *s) {
    return dbuf_put_bytes(di, (unsigned char *)s, strlen(s)+1);
}

int dbuf_get_byte(dbuf_iter *di, unsigned char *b) {
    dbuf *d;

    if(!di) return -EINVAL;
    d = di->base;

    if(!d) return -EINVAL;

    if(di->offset == d->sz) return -EAGAIN;

    *b = d->data[di->offset++];
    return 0;
}

int dbuf_get_bytes(dbuf_iter *di, unsigned char *data, unsigned sz) {
    dbuf *d;
    unsigned char *p;

    if(!di) return -EINVAL;

    d = di->base;
    if(!d) return -EINVAL;

    if(d->sz - di->offset < sz) return -EAGAIN;
    p = d->data + di->offset;
    di->offset += sz;
    for(; sz; sz--) {
        *data++ = *p++;
    }
    return sz;
}

int dbuf_get_int(dbuf_iter *di, int *i) {
    return dbuf_get_bytes(di, (unsigned char *)i, sizeof(int));
}

int dbuf_get_long(dbuf_iter *di, long *i) {
    return dbuf_get_bytes(di, (unsigned char *)i, sizeof(long));
}

int dbuf_get_float(dbuf_iter *di, float *i) {
    return dbuf_get_bytes(di, (unsigned char *)i, sizeof(float));
}

int dbuf_get_double(dbuf_iter *di, double *i) {
    return dbuf_get_bytes(di, (unsigned char *)i, sizeof(double));
}

int dbuf_get_string(dbuf_iter *di, const char **s) {
    dbuf *d;
    unsigned char *p, ch=0;
    int result;

    if(!di) return -EINVAL;

    d = di->base;
    if(!d) return -EINVAL;

    if(d->sz == di->offset) return -EAGAIN;
    p = d->data + di->offset;
    *s = (char*)p;

    do {
        result = dbuf_get_byte(di, &ch);
    } while(result >= 0 && ch != 0);

    // dbuf_put_string should never put when there's no room for the trailing
    // NUL, but let's be safe and not return a string if there's no terminated
    // string in the dbuf
    if(ch != 0) {
        *s = 0;
        return -EAGAIN;
    }

    return 0;
}
