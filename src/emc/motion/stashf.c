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
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include "stashf.h"
#include "dbuf.h"
#include "rtapi.h"
#include "rtapi_errno.h"
#include "rtapi_string.h"
#include <stdarg.h>

#ifdef RTAPI
#include <linux/kernel.h>
#define gettext(s) s
#define alloca __builtin_alloca
#else
#include <stdio.h>
#include <libintl.h>
#include <alloca.h>
#endif

static int SET_ERRNO(int value) {
#ifdef RTAPI
    return value;
#else
    if(value < 0) {
        errno = value;
        return -1;
    } else {
        return value;
    }
#endif
}

static int get_code(const char **fmt_io, int *modifier_l) {
    const char *fmt = *fmt_io;
    *modifier_l = 0;
    fmt++;
    for(; *fmt; fmt++) {
        switch(*fmt) {
            case 'l':
                *modifier_l = 1;
                break;
            // integers
            case 'd': case 'i': case 'x': case 'u': case 'X':
            // doubles
            case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
            // char; string
            case 'c': case 's':
            // pointer
            case 'p':
            // literal percent
            case '%': goto format_end;
        }
    }
format_end:
    *fmt_io = fmt+1;
    return *fmt;
}

int vstashf(struct dbuf_iter *o, const char *fmt, va_list ap) {
    int modifier_l;

    dbuf_put_string(o, fmt);

    while((fmt = strchr(fmt, '%'))) {
        int code = get_code(&fmt, &modifier_l);

        switch(code) {
        case '%':
            break;
        case 'c': case 'd': case 'i': case 'x': case 'u': case 'X':
            if(modifier_l) {
        case 'p':
                dbuf_put_long(o, va_arg(ap, long));
            } else {
                dbuf_put_int(o, va_arg(ap, int));
            }
            break;
        case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
            dbuf_put_double(o, va_arg(ap, double));
            break;
        case 's':
            dbuf_put_string(o, va_arg(ap, const char *));
            break;
        default:
            return SET_ERRNO(-EINVAL);
            break;
        }
    }
    return 0;
}

int stashf(struct dbuf_iter *o, const char *fmt, ...) {
    va_list ap;
    int result;

    va_start(ap, fmt);
    result = vstashf(o, fmt, ap);
    va_end(ap);

    return result;
}

#ifdef RTAPI
#define PRINT(...) rtapi_snprintf(buf, n, ## __VA_ARGS__)
#else
#define PRINT(...) snprintf(buf, n, ## __VA_ARGS__)
#endif
#define EXTRA buf += result; n -= result; if(n<0) n = 0;
int snprintdbuf(char *buf, int n, struct dbuf_iter *o) {
#include "stashf_wrap.h"
}

#ifndef RTAPI
#define PRINT(...) fprintf(f, ## __VA_ARGS__)
int fprintdbuf(FILE *f, struct dbuf_iter *o) {
#include "stashf_wrap.h"
}

#define PRINT(...) printf(__VA_ARGS__)
int printdbuf(struct dbuf_iter *o) {
#include "stashf_wrap.h"
}
#endif
