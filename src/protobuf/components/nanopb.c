// this component provides support library functions for
// other RT components using protobuf messaging
// it exports the nanopb library functions as
// declared in pb_decode.h and pb_encode.h
//
// actual message definitions derived from .proto files are in a
// separate module

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("Nanopb library support component for HAL");
MODULE_LICENSE("GPL");

#include <protobuf/pb-linuxcnc.h>
#include <protobuf/nanopb/pb_decode.h>
#include <protobuf/nanopb/pb_encode.h>

static int comp_id;
static const char *name = "nanopb";

int rtapi_app_main(void)
{
    if ((comp_id = hal_init(name)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed: %d\n",
			name, comp_id);
	return -1;
    }
    hal_ready(comp_id);
    rtapi_print_msg(RTAPI_MSG_DBG, "%s " VERSION " loaded\n", name);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

EXPORT_SYMBOL(pb_istream_from_buffer);
EXPORT_SYMBOL(pb_read);
EXPORT_SYMBOL(pb_decode);
EXPORT_SYMBOL(pb_decode_tag);
EXPORT_SYMBOL(pb_skip_field);
EXPORT_SYMBOL(pb_decode_varint);
EXPORT_SYMBOL(pb_ostream_from_buffer);
EXPORT_SYMBOL(pb_write);
EXPORT_SYMBOL(pb_encode);
EXPORT_SYMBOL(pb_encode_tag_for_field);
EXPORT_SYMBOL(pb_encode_tag);
EXPORT_SYMBOL(pb_encode_varint);
EXPORT_SYMBOL(pb_encode_svarint);
EXPORT_SYMBOL(pb_encode_string);
EXPORT_SYMBOL(pb_encode_fixed32);
EXPORT_SYMBOL(pb_encode_fixed64);
EXPORT_SYMBOL(pb_encode_submessage);

#ifdef NANOPB_INTERNALS
EXPORT_SYMBOL(pb_enc_varint);
EXPORT_SYMBOL(pb_enc_svarint);
EXPORT_SYMBOL(pb_enc_fixed32);
EXPORT_SYMBOL(pb_enc_fixed64);
EXPORT_SYMBOL(pb_enc_bytes);
EXPORT_SYMBOL(pb_enc_string);
EXPORT_SYMBOL(pb_dec_varint);
EXPORT_SYMBOL(pb_dec_svarint);
EXPORT_SYMBOL(pb_dec_fixed32);
EXPORT_SYMBOL(pb_dec_fixed64);
EXPORT_SYMBOL(pb_dec_bytes);
EXPORT_SYMBOL(pb_dec_string);
EXPORT_SYMBOL(pb_dec_submessage);
EXPORT_SYMBOL(pb_skip_varint);
EXPORT_SYMBOL(pb_skip_string);
#endif

#ifdef NANOPB_LEGACY
EXPORT_SYMBOL(pb_enc_submessage);
#endif
