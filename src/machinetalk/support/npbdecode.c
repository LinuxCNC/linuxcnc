// example for protobuf decoding with NanoPB
//
// this is the method how RT components would process
// messages coming in/going out through ring buffers
//
// there's a tradeoff how repeated objects are described and encoded
// with NanoPB:

// depending on whether max_count is used, decoding differs:
// example: protobuf/proto/message.proto:
//
// message Telegram {
//     // option 1: decode by callback, unlimited repeat count
//     // the Telegram struct is much smaller than static allocation:
//
//     repeated Object           args   = 90;  // use #define ARGS_CALLBACK
//
//     // option 2: simpler one-step decoding, maximum repeat count,
//     // larger structure
//
//     repeated Object           args   = 90  [(nanopb).max_count = 20];
// }

#define ARGS_CALLBACK

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <machinetalk/include/pb-linuxcnc.h>
#include <machinetalk/nanopb/pb_decode.h>
#include <machinetalk/nanopb/pb_encode.h>

#include <machinetalk/generated/types.npb.h>
#include <machinetalk/generated/object.npb.h>
#include <machinetalk/generated/message.npb.h>
#include <machinetalk/generated/emcclass.npb.h>
#include <machinetalk/generated/rtapi_message.npb.h>

#undef USE_STRING_STREAM

bool callback(pb_istream_t *stream, uint8_t *buf, size_t count);

int bufsize = 10240;
const char *progname;

void print_value(pb_Value *v, char *tag)
{
    printf("%s.value.type = %d\n", tag, v->type);

    if (v->has_v_double)
	printf("%s.value.double = %f\n",tag, v->v_double);

    if (v->has_pose) {
	pb_EmcPose *e = &v->pose;
      printf("%s.pose: ", tag);
      if (e->tran.has_x)
	  printf("x=%f ", e->tran.x);
      if (e->tran.has_y)
	  printf("y=%f ", e->tran.y);
      if (e->tran.has_z)
	  printf("z=%f ", e->tran.z);
      if (e->has_a)
	  printf("a=%f ", e->a);
      if (e->has_b)
	  printf("a=%f ", e->b);
      if (e->has_c)
	  printf("a=%f ", e->c);
      if (e->has_u)
	  printf("a=%f ", e->u);
      if (e->has_v)
	  printf("a=%f ", e->v);
      if (e->has_w)
	  printf("a=%f ", e->w);
      printf("\n");
    }
}

#if 0
void print_object_detail(pb_Object *o, char *tag)
{
    printf("%s.object.type = %d\n", tag, o->type);
#ifndef ARGS_CALLBACK
    if (o->has_name)
	printf("%s.object.name = '%s'\n", tag, o->name);
#endif
    if (o->has_pin) {
	pb_Pin *p = &o->pin;
	printf("%s.pin.type = %d\n", tag, p->type);
#ifndef ARGS_CALLBACK
	if (p->has_name)
	    printf("%s.pin.name = '%s'\n", tag,p->name);
#endif
	if (p->has_halbit)
	    printf("%s.pin.bit = '%s'\n", tag,p->halbit ? "True":"False");
	if (p->has_halfloat)
	    printf("%s.pin.float = %f\n", tag,p->halfloat);
	if (p->has_hals32)
	    printf("%s.pin.s32 = %d\n", tag,p->hals32);
	if (p->has_halu32)
	    printf("%s.pin.u32 = %d\n", tag,p->halu32);
    }
    if (o->has_value)
	print_value(&o->value, tag);

}
#endif

bool print_string(pb_istream_t *stream, const pb_field_t *field, void *arg)
{
    uint8_t buffer[1024] = {0};

    /* We could read block-by-block to avoid the large buffer... */
    if (stream->bytes_left > sizeof(buffer) - 1)
        return false;

    if (!pb_read(stream, buffer, stream->bytes_left))
        return false;

    /* Print the string, in format comparable with protoc --decode.
     * Format comes from the arg defined in main().
     */
    printf((char*)arg, buffer);
    return true;
}

#ifdef ARGS_CALLBACK
bool print_member(pb_istream_t *stream, const pb_field_t *field, void *cbdata)
{

    return 0;
}
#if 0
bool print_object(pb_istream_t *stream, const pb_field_t *field, char *tag)
{
    pb_Object obj = {0};
    obj = (pb_Object) {
	.name.funcs.decode = print_string,
	.name.arg = "object.name = '%s'\n",
	.pin.name.funcs.decode = print_string,
	.pin.name.arg = "pin.name = '%s'\n",
	.pin.oldname.funcs.decode = print_string,
	.pin.oldname.arg = "pin.oldname = '%s'\n",
	.comp.name.funcs.decode = print_string,
	.comp.name.arg = "comp.name = '%s'\n",
	.comp.args.funcs.decode = print_string,
	.comp.args.arg = "comp.arg = '%s'\n",
	.function.name.funcs.decode = print_string,
	.function.name.arg = "function.name = '%s'\n",
	.group.name.funcs.decode = print_string,
	.group.name.arg = "group.name = '%s'\n",
	.member.name.funcs.decode = print_member,
	.member.name.arg = "group.member = '%s'\n",

	/* .instance.name.funcs.decode = print_string, */
	/* .instance.name.arg = "instance.name = '%s'\n", */

	/* .origin.name.funcs.decode = print_string, */
	/* .origin.name.arg = "origin.name = '%s'\n", */

	.param.name.funcs.decode = print_string,
	.param.name.arg = "param.name = '%s'\n",
	.param.oldname.funcs.decode = print_string,
	.param.oldname.arg = "param.oldname = '%s'\n",
	.ring.name.funcs.decode = print_string,
	.ring.name.arg = "ring.name = '%s'\n",
	.signal.name.funcs.decode = print_string,
	.signal.name.arg = "signal.name = '%s'\n",

	.thread.name.funcs.decode = print_string,
	.thread.name.arg = "origin.name = '%s'\n",
    };

    if (!pb_decode(stream, pb_Object_fields, &obj)) {
        return false;
    }
    print_object_detail(&obj, tag);
    return true;
}
#endif
#endif

int main(int argc, char **argv)
{
    // stdin stream example
    pb_istream_t stdin_stream = {&callback, stdin, SIZE_MAX};

    pb_istream_t stream;
    pb_Container c;

    progname = argv[0];
    memset(&c, 0, sizeof(c));

    if (argc > 1) {
	// string stream example
	void *buf;
	int size = 0;

	buf = malloc(size);
	assert(buf != NULL);
	size = read(0, buf, bufsize);
	if (size < 0) {
	    perror("read");
	    exit(errno);
	}
	stream = pb_istream_from_buffer(buf, size);
    } else
	stream = stdin_stream;

    c.type = pb_ContainerType_MT_HALUPDATE;
#ifdef ARGS_CALLBACK
    /* c.arg.funcs.decode = &print_object; */
    /* c.arg.arg = "arg"; */
#endif

    if (!pb_decode(&stream, pb_Container_fields, &c)) {
	fprintf(stderr, "%s: pb_decode(container) failed: '%s'\n",
			progname, PB_GET_ERROR(&stream));
	exit(1);
    }

#ifndef ARGS_CALLBACK
    printf("c.args.args_count = %d\n", c.args.args_count);
    int i;
    for (i = 0; i < c.args.args_count; i++) {
	Object *ob = &c.args.args[i];
	print_object(ob);
    }
#endif

    exit(0);
}


// stdin stream example from
// http://koti.kapsi.fi/~jpa/nanopb/docs/concepts.html#field-callbacks
bool callback(pb_istream_t *stream, uint8_t *buf, size_t count)
{
   FILE *file = (FILE*)stream->state;
   bool status;

   if (buf == NULL) {
       while (count-- && fgetc(file) != EOF);
       return count == 0;
   }
   status = (fread(buf, 1, count, file) == count);
   if (feof(file))
       stream->bytes_left = 0;
   return status;
}
