// example for encoding a printf-style argument list in
// a protobuf message.
//
// reason why this is needed: kernel printf does not
// support floats. The method will be to generate printf-messages
// like outline here, and funnel them to userland with HAL rings
// for final formatting and output.
//
// inspired by Jeff Epler's dbuf.*/stashf.* code.
//
// justification for this code: if wrapped into a protobuf
// message, we have one special-purpose queue/datatype/support code less.


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include <machinetalk/include/pb-linuxcnc.h>
#include <machinetalk/nanopb/pb_encode.h>

#include <machinetalk/generated/types.npb.h>
#include <machinetalk/generated/value.npb.h>
#include <machinetalk/generated/rtapi_message.npb.h>
#include "rtapi.h"

#define MIN(a,b) ((a)<(b)?(a):(b))

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

int pbvprintf( pb_RTAPI_Message *msg, int level, const char *fmt, va_list ap)
{
    int modifier_l, code;
    pb_Value *v;

    msg->msglevel = level;
    strncpy(msg->format, fmt, MIN(strlen(fmt),
				  sizeof(msg->format)));
    msg->arg_count = 0;

    while((fmt = strchr(fmt, '%'))) {

	v = &msg->arg[msg->arg_count];

        code = get_code(&fmt, &modifier_l);

        switch(code) {
        case '%':
            break;
        case 'c': case 'd': case 'i': case 'x': case 'u': case 'X':
            if(modifier_l) {
	    case 'p':
		// dbuf_put_long(o, va_arg(ap, long));
		v->type = pb_ValueType_INT32;
		v->has_v_int32 = true;
		v->v_int32 =  va_arg(ap, long);
		msg->arg_count++;
            } else {
		// XXX not sure about long vs int encoding in protobuf types
                // dbuf_put_int(o, va_arg(ap, int));
		v->type = pb_ValueType_INT32;
		v->has_v_int32 = true;
		v->v_int32 =  va_arg(ap, long);
		msg->arg_count++;

            }
            break;
        case 'e': case 'E': case 'f': case 'F': case 'g': case 'G':
	    //            dbuf_put_double(o, va_arg(ap, double));
	    v->type = pb_ValueType_DOUBLE;
	    v->has_v_double = true;
	    v->v_double =  va_arg(ap, double);
	    msg->arg_count++;
            break;
        case 's':
            // dbuf_put_string(o, va_arg(ap, const char *));
	    v->type = pb_ValueType_STRING;
	    v->has_v_string = true;
	    strncpy(v->v_string,va_arg(ap, const char *), sizeof(v->v_string));
	    msg->arg_count++;
            break;
        default:
            //return SET_ERRNO(-EINVAL);  // XXX
	    return -EINVAL;
            break;
        }
    }

    return 0;
}

int pbprintf(pb_RTAPI_Message *msg, int level, const char *fmt, ...) {
    va_list ap;
    int result;

    va_start(ap, fmt);
    result = pbvprintf(msg, level,fmt, ap);
    va_end(ap);

    return result;
}

int main (int argc, char *argv[ ])
{
    uint8_t buffer[5120];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    int result;

    pb_RTAPI_Message msg;
    memset(&msg, 0, sizeof(msg));

    switch (argc) {
    case 1:
	// pb_encode an unitialized message
	break;

    case 2:
	msg.msglevel = RTAPI_MSG_ERR;
	strcpy(msg.format, "all manual");
	msg.arg_count = 0;
	break;

    case 3:
	pbprintf(&msg, RTAPI_MSG_ERR,
		 "no format specifier here\n");
	break;

    case 4:
	pbprintf(&msg, RTAPI_MSG_WARN,
		 "foo=%ld bar=%g string='%s' hex=0x%lx\n",
		 4711, 2.71828, "astring", 0xdeadbeef);
	break;
    }

    if ((result = pb_encode(&stream, pb_RTAPI_Message_fields, &msg))) {
	fwrite(buffer, 1, stream.bytes_written, stdout);
        exit(0);
    } else {
	fprintf(stderr, "rtprintf: pb_encode failed: %zu - %s\n",
	       stream.bytes_written, PB_GET_ERROR(&stream));

	exit(1);
    }

}
