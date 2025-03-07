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

#ifndef EXTRA
#define EXTRA
#endif

const char *fmt, *efmt;
    int result;

    result = dbuf_get_string(o, &fmt);
    if(result < 0) return result;

    // cppcheck-suppress selfAssignment
    if(*fmt) fmt = gettext(fmt);

    while((efmt = strchr(fmt, '%'))) {
        int modifier_l;
        int code = get_code(&efmt, &modifier_l);
	// A format should be a "few" characters long, like "+999.999lf".
	// However, it is never sure how long they are. An artificial limit
	// must be imposed unless we use variable length arrays (VLAs) or use
	// dynamic memory. VLAs are not really allowed in kernel and are the
	// reason for this comment. Using dynamic memory is slow and must be
	// discouraged here.
	// A limit to 63+1 characters seems fair. If the format is larger than
	// the limit, then the print will not be right, but it doesn't crash
	// either. The next round will skip properly because 'efmt' points to
	// after the format.
        char block[63 + 1];
        int fmt_len = efmt - fmt;
	if(fmt_len >= (int)sizeof(block))
		fmt_len = sizeof(block) - 1;
        memcpy(block, fmt, fmt_len);
        block[fmt_len] = 0;

        switch(code) {
            case '%':
                result = PRINT("%s", block);
                break;
            case 'c': case 'd': case 'i': case 'x': case 'u': case 'X':
                if(modifier_l)
            case 'p':
                {
                    long l;
                    result = dbuf_get_long(o, &l);
                    if(result < 0) return SET_ERRNO(result);
                    result = PRINT(block, l);
                } else {
                    int i;
                    result = dbuf_get_int(o, &i);
                    if(result < 0) return SET_ERRNO(result);
                    result = PRINT(block, i);
                }
                break;
            case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
                {
                    double d;
                    result = dbuf_get_double(o, &d);
                    if(result < 0) return SET_ERRNO(result);
                    result = PRINT(block, d);
                }
                break;
            case 's':
                {
                    const char *s;
                    result = dbuf_get_string(o, &s);
                    if(result < 0) return SET_ERRNO(result);
                    // cppcheck-suppress selfAssignment
                    if(*s) s = gettext(s);
                    result = PRINT(block, s);
                }
                break;
        }
        if(result < 0) return SET_ERRNO(result);
        EXTRA
        fmt = efmt;
    }
    if(*fmt) {
        result = PRINT("%s", fmt);
        if(result < 0) return SET_ERRNO(result);
        // The below expansion of 'EXTRA' makes no sense. The function is about
        // to exit and changing anything here does not change the outcome in
        // any way. Therefore, make it a comment.
        // EXTRA
    }

    return SET_ERRNO(result);
#undef PRINT
#undef EXTRA
