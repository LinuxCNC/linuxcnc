// support library functions for
// other RT components using protobuf messaging
// it exports the nanopb library functions as
// declared in pb_decode.h and pb_encode.h
//
// actual message definitions derived from .proto files are in a
// separate module

#include "rtapi_export.h"

#include <machinetalk/include/pb-linuxcnc.h>
#include <machinetalk/nanopb/pb_decode.h>
#include <machinetalk/nanopb/pb_encode.h>

bool pb_encode_dest(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    pb_dest_t *d = (pb_dest_t *)arg;

    if (!pb_encode_tag_for_field(stream, field))
        return false;
    if (d->callback)
	if (!(d->callback(d)))
	    return false;
    return pb_encode_string(stream, d->dest, d->size);
}

bool pb_decode_dest(pb_istream_t *stream, const pb_field_t *field, void *arg)
{
    pb_dest_t *d = arg;
    if (stream->bytes_left != d->size)
        return false;
    if (!pb_read(stream, d->dest, stream->bytes_left))
        return false;
    if (d->callback)
	return d->callback(d);
    return true;
}

#ifdef RTAPI
EXPORT_SYMBOL(pb_encode_dest);
EXPORT_SYMBOL(pb_decode_dest);

// library functions
EXPORT_SYMBOL(pb_istream_from_buffer);
EXPORT_SYMBOL(pb_read);
EXPORT_SYMBOL(pb_decode);
EXPORT_SYMBOL(pb_decode_noinit);
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
#endif
