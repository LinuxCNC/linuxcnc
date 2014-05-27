// example for reading a Container header in case
// the protobuf compiler, library and header are not available
// in which case we use the included nanopb library
// to read the container header and extract
// message type,length and payload
//
// run:  encdec  | rawread
// Output:
// wiretype=5 tag=1 fixed32=63
// wiretype=0 tag=2 varint=4
// wiretype=2 tag=4

// interpretation (see also protobuf/proto/message.proto:Container):
//
// field tag#1 is the length field, wiretype fixed32(5), value 63
// field tag#2 is a varint(0), value = 4 (type field, value MT_COMMAND)
// filed tag#4 is length-delimited submessage(2), tag=4: it's a Command mesage
//
// see also: https://developers.google.com/protocol-buffers/docs/encoding?hl=en-US
//
// Michael Haberler 3/2013
// based on nanopb/tests/test_decode2.c

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <protobuf/pb-linuxcnc.h>
#include <protobuf/nanopb/pb_decode.h>
#include <protobuf/nanopb/pb_encode.h>

#include <protobuf/container.h>


bool print_container(pb_istream_t *stream)
{
    uint64_t type, msgtype;
    uint32_t length, tag;
    pb_wire_type_t wiretype;
    bool eof;

    // decode the type tag
    if (!pb_decode_tag(stream, &wiretype, &tag, &eof))  {
	printf("Parsing tag#1 failed: %s\n", PB_GET_ERROR(stream));
    }

    if (!pb_decode_varint(stream, &type)) {
	printf("Parsing required type field#1 failed: %s\n", PB_GET_ERROR(stream));
    }

    if (!pb_decode_tag(stream, &wiretype, &tag, &eof))  {
	printf("Parsing tag#2 failed: %s\n", PB_GET_ERROR(stream));
    }
    assert(tag == 1);      // tag number of length field
    assert(wiretype == 5); // encoding of length field = fixed32

    if (!pb_decode_fixed32(stream, &length)) {
	printf("Parsing field#1 failed: %s\n", PB_GET_ERROR(stream));
    }
    printf("wiretype=%d tag=%d fixed32=%d (total message length)\n",
	   wiretype, tag, length);

    if (!pb_decode_tag(stream, &wiretype, &tag, &eof))  {
	printf("Parsing tag#2 failed: %s\n", PB_GET_ERROR(stream));
    }
    assert(tag == 2);           // tag number of type field
    assert(wiretype == 0);      // encoding of length field = varint

    if (!pb_decode_varint(stream, &msgtype)) {
	printf("Parsing field#2 failed: %s\n", PB_GET_ERROR(stream));
    }
    printf("wiretype=%d tag=%d varint=%llu (msgtype)\n", wiretype, tag, msgtype);

    // the actual message follows.
    // It is a submessage encoded in length-delimited format
    if (!pb_decode_tag(stream, &wiretype, &tag, &eof))  {
	printf("Parsing tag#3 failed: %s\n", PB_GET_ERROR(stream));
    }
    printf("wiretype=%d tag=%d (submessage type)\n", wiretype, tag);
    assert(wiretype == 2); // length-delimited format


    printf("submessage: %s NML; %s Motion\n",
	   is_NML_container(tag) ? "is" : "not",
	   is_Motion_container(tag) ? "is" : "not");

    // decoding the submessage left as exercise
    return true;
}

/* This binds the pb_istream_t to stdin */
bool callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
    FILE *file = (FILE*)stream->state;
    bool status;

    status = (fread(buf, 1, count, file) == count);

    if (feof(file))
        stream->bytes_left = 0;

    return status;
}

int main()
{

    pb_istream_t stream = {&callback, stdin, 10000};
    if (!print_container(&stream))
    {
        printf("Parsing failed: %s\n", PB_GET_ERROR(&stream));
        return 1;
    } else {
        return 0;
    }
}
