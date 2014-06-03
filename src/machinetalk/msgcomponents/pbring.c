// demo actor component to show sending/receiving protobuf-encoded messages over HAL rings
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */
#include "hal_ring.h"

// the nanopb library and compiled message definitions are brought in once by
// 'halcmd loadrt pbmsgs' (message descriptors for parsing and generating pb msgs)
// the nanopb library functions per se are now linked into hal_lib.so
#include <machinetalk/include/pb-linuxcnc.h>
#include <machinetalk/nanopb/pb_decode.h>
#include <machinetalk/nanopb/pb_encode.h>
#include <machinetalk/include/container.h>

typedef struct {
    hal_u32_t *underrun;	// number of thread invocations with no new command available
    hal_u32_t *received;	// number of commands received
    hal_u32_t *sent;	        // number of responses sent
    hal_u32_t *sendfailed;	// number of messages failed to send
    hal_u32_t *decodefail;	// number of messages which failed to protobuf decode
    ringbuffer_t to_rt_rb;      // incoming ringbuffer
    ringbuffer_t from_rt_rb;    // outgoing ringbuffer
} pbring_inst_t;

enum {
    NBP_SIZE_FAIL   = -100,
    NBP_ENCODE_FAIL = -101,
    NBP_DECODE_FAIL = -102,
    RB_RESERVE_FAIL = -103,
    RB_WRITE_FAIL   = -104,
} rtmsg_errors_t;

#define MAX_INST 16

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("Actor Test Component");
MODULE_LICENSE("GPL");

static int count = 1;
RTAPI_MP_INT(count, "number of instances");

static char *name = "pbring";
RTAPI_MP_STRING(name,  "component name");

static char *command = "command";
RTAPI_MP_STRING(command,  "name of command ring");

static int csize = 65536;
RTAPI_MP_INT(csize, "size of command ring");

static char *response = "response";
RTAPI_MP_STRING(response,  "name of response ring");

static int rsize = 65536;
RTAPI_MP_INT(rsize, "size of response ring");

static int msgsize = 0;
RTAPI_MP_INT(msgsize, "ringbuffer allocation for outgoing messages");

static int txnoencode = 0;
RTAPI_MP_INT(txnoencode, "pass unencoded pb_Container struct instead of pb message");

static int rxnodecode = 0;
RTAPI_MP_INT(rxnodecode, "expect unencoded pb_Container struct instead of pb message");

static pbring_inst_t *pbring_array; // instance data array
static int comp_id;

// if the thread function is called by different threads, rx and tx will have
// to move to the instance data structure
static pb_Container rx, tx;



static int export_pbring(const char *name, int num, pbring_inst_t *p);

static void rtapi_format_pose(char *buf, unsigned long int size,  pb_EmcPose *p)
{
    unsigned sz = size;
    char *b = buf; // FIXME guard against buffer overflow
    int n;

    pb_PmCartesian *c = &p->tran;
    if (c->has_x) { n = rtapi_snprintf(b, sz, "x=%f ", c->x); b += n; }
    if (c->has_y) { n = rtapi_snprintf(b, sz, "y=%f ", c->y); b += n; }
    if (c->has_z) { n = rtapi_snprintf(b, sz, "z=%f ", c->z); b += n; }
    if (p->has_a) { n = rtapi_snprintf(b, sz, "a=%f ", p->a); b += n; }
    if (p->has_b) { n = rtapi_snprintf(b, sz, "b=%f ", p->b); b += n; }
    if (p->has_c) { n = rtapi_snprintf(b, sz, "c=%f ", p->c); b += n; }
    if (p->has_u) { n = rtapi_snprintf(b, sz, "u=%f ", p->u); b += n; }
    if (p->has_v) { n = rtapi_snprintf(b, sz, "v=%f ", p->v); b += n; }
    if (p->has_w) { n = rtapi_snprintf(b, sz, "w=%f ", p->w); b += n; }
}

bool npb_encode_string(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
    char *str = (char *)*arg;
    if (!pb_encode_tag_for_field(stream, field))
        return false;
    return pb_encode_string(stream, (uint8_t*)str, strlen(str));
}

// encode message struct in msg according to 'fields', and send off via rb
// return 0 on success or < 0 on failure; see rtmsg_errors_t
//
// runtime optimization:
// if prealloc > 0, skip the message sizing step, and allocate
// prealloc bytes in the ringbuffer, assuming the result will fit
// record_write_end() will prune the allocation to actual needs so
// nothing is lost except temporarily larger buffer requirements
// if prealloc was insufficient to hold the result, fail.
//
static int npb_send_msg(const void *msg, const pb_field_t *fields,
			ringbuffer_t *rb, size_t prealloc)
{
    void *buffer;
    int retval;
    size_t size;

    if (prealloc == 0) {
	// determine size
	pb_ostream_t sstream = PB_OSTREAM_SIZING;
	if (!pb_encode(&sstream, fields,  msg)) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: sizing pb_encode(): %s written=%zu\n",
			    name, PB_GET_ERROR(&sstream), sstream.bytes_written);
	    return NBP_SIZE_FAIL;
	}
	size = sstream.bytes_written;
    } else
	size = prealloc;

    // preallocate memory in ringbuffer
    if ((retval = record_write_begin(rb, (void **)&buffer, size)) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: record_write_begin(%d/%zu) failed: %d\n",
			name, msgsize, size, retval);
	return RB_RESERVE_FAIL;
    }

    // zero-copy encode directly into ringbuffer
    pb_ostream_t rstream = pb_ostream_from_buffer(buffer, size);
    if (!pb_encode(&rstream, fields,  msg)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: pb_encode failed: %s, msgsize=%d written=%zu\n",
			name, PB_GET_ERROR(&rstream), msgsize, rstream.bytes_written);
	return NBP_ENCODE_FAIL;
    }

    // send it off
    if ((retval = record_write_end(rb, buffer, rstream.bytes_written))) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: record_write_end(%d) failed: %d\n",
			name,  msgsize, retval);
	return RB_WRITE_FAIL;
    }
    return 0;
}

static int decode_msg(pbring_inst_t *p,  pb_istream_t *stream)
{

    if (!pb_decode(stream, pb_Container_fields, &rx)) {
	*(p->decodefail) += 1;
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: pb_decode(Container) failed: '%s'\n",
			name, PB_GET_ERROR(stream));
	return -1;
    }

    if (rx.has_motcmd) {
	rtapi_print_msg(RTAPI_MSG_ERR, "Container.motcmd command=%d num=%d\n",
			rx.motcmd.command,rx.motcmd.commandNum);
	if (rx.motcmd.has_pos) {
	    char buf[200];
	    rtapi_format_pose(buf, sizeof(buf), &rx.motcmd.pos);
	    rtapi_print_msg(RTAPI_MSG_ERR, "motcmd: %s\n", buf);
	}
    }
    return 0;
}

static void update_pbring(void *arg, long l)
{
    pbring_inst_t *p = (pbring_inst_t *) arg;

    ring_size_t cmdsize = record_next_size(&p->to_rt_rb);
    if (cmdsize < 0) {
	// command ring empty
	*(p->underrun) += 1;
	return;
    }
    const void *cmdbuffer = record_next(&p->to_rt_rb);
    pb_istream_t stream = pb_istream_from_buffer((void *) cmdbuffer, cmdsize);
    int retval;

    if (!decode_msg(p, &stream)) {

	// process command here
	// prepare reply
	tx.has_motstat = true;
	tx.type =  pb_ContainerType_MT_MOTSTATUS;
	tx.note.funcs.encode = npb_encode_string;
	tx.note.arg = "hi there!";

	tx.motstat = (pb_MotionStatus) {
	    .commandEcho = rx.motcmd.command,
	    .commandNumEcho = rx.motcmd.commandNum,
	    .commandStatus = pb_cmd_status_t_EMCMOT_COMMAND_OK,
	    .has_carte_pos_fb = true,
	    .carte_pos_fb = {
		.tran = {
		    .has_x = true,
		    .x = 42.0,
		    .has_y = true,
		    .y = 13.56,
		    .has_z = true,
		    .z = 27.12},
		.has_a = true,
		.a = 3.14
	    }
	};
	if ((retval = npb_send_msg(&tx, pb_Container_fields,
				   &p->from_rt_rb, msgsize))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,"error reply failed %d", retval);
	    *(p->sendfailed) +=1;
	} else
	    *(p->sent) += 1;
    } else {
	char errmsg[255];

	snprintf(errmsg, sizeof(errmsg), "%s: failed to decode (size %d): %s",
		 name, cmdsize, PB_GET_ERROR(&stream));

	tx = (pb_Container) {
	    .type = pb_ContainerType_MT_MOTSTATUS, // FIXME
	    .note.funcs.encode = npb_encode_string,
	    .note.arg = (void *) errmsg
	};
	int retval;
	if ((retval = npb_send_msg(&tx, pb_Container_fields,
				   &p->from_rt_rb, msgsize))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,"error reply failed %d", retval);
	    *(p->sendfailed) += 1;
	} else
	    *(p->sent) += 1;
    }
    record_shift(&p->to_rt_rb);
    *(p->received) += 1;
}

static int create_or_attach(const char *ringname, int size, ringbuffer_t * rb)
{
    int retval;

    // for messaging with protobuf, use record mode
    // default mode 0 = record mode
    if ((retval = hal_ring_new(ringname, size, 0, 0))) {
	if (retval == -EEXIST) {
	    rtapi_print_msg(RTAPI_MSG_INFO,
			    "%s: using existing ring '%s'\n", name, ringname);
	} else {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: failed to create new ring %s\n", name, ringname);
	    return retval;
	}
    }
    if ((retval = hal_ring_attach(ringname, rb, NULL))) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: hal_ring_attach(%s) failed - %d\n",
			name, command, retval);
	return retval;
    }
    return 0;
}

int rtapi_app_main(void)
{
    int i, retval;

    if ((count <= 0) || (count > MAX_INST)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: invalid count: %d\n", name, count);
	return -1;
    }
    if ((comp_id = hal_init(name)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: hal_init(%s) failed: %d\n",
			name, name, comp_id);
	return comp_id;
    }

    pbring_array = hal_malloc(count * sizeof(pbring_inst_t));
    if (pbring_array == 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: hal_malloc() failed\n", name);
	hal_exit(comp_id);
	return -1;
    }

    for (i = 0; i < count; i++) {
	if ((retval = export_pbring(name, i, &pbring_array[i]))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: ERROR: var export failed\n", name);
	    hal_exit(comp_id);
	    return -1;
	}
    }

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    int i, retval;
    char ringname[HAL_NAME_LEN + 1];

    for (i = 0; i < count; i++) {
	pbring_inst_t *p = &pbring_array[i];

	rtapi_snprintf(ringname, sizeof(ringname), "%s.%d.in", name, i);
	p->to_rt_rb.header->reader = 0;
	if ((retval = hal_ring_detach(ringname, &p->to_rt_rb)) < 0)
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: hal_ring_detach(%s) failed: %d\n",
			    name, ringname, retval);
	// hal_ring_delete(ringname, comp_id);

	p->from_rt_rb.header->writer = 0;
	rtapi_snprintf(ringname, sizeof(ringname), "%s.%d.out", name, i);
	if ((retval = hal_ring_detach(ringname, &p->from_rt_rb)) < 0)
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "%s: hal_ring_detach(%s) failed: %d\n",
			    name, ringname, retval);
	// hal_ring_delete(ringname, comp_id);
    }
    hal_exit(comp_id);
}

static int export_pbring(const char *comp, int n, pbring_inst_t *p)
{
    int retval;
    char name[HAL_NAME_LEN + 1];

    if ((retval = hal_pin_u32_newf(HAL_OUT, &(p->underrun),
				      comp_id, "%s.%d.underrun", comp, n)))
	return retval;

    if ((retval = hal_pin_u32_newf(HAL_OUT, &(p->received),
				      comp_id, "%s.%d.received", comp, n)))
	return retval;

    if ((retval = hal_pin_u32_newf(HAL_OUT, &(p->sent),
				      comp_id, "%s.%d.sent", comp, n)))
	return retval;

    if ((retval = hal_pin_u32_newf(HAL_OUT, &(p->sendfailed),
				      comp_id, "%s.%d.sendfailed", comp, n)))
	return retval;

    if ((retval = hal_pin_u32_newf(HAL_OUT, &(p->decodefail),
				      comp_id, "%s.%d.decodefail", comp, n)))
	return retval;

    *(p->underrun) = 0;
    *(p->received) = 0;
    *(p->sent) = 0;
    *(p->sendfailed) = 0;
    *(p->decodefail) = 0;

    rtapi_snprintf(name, sizeof(name), "%s.%d.in", comp, n);
    if ((retval = create_or_attach(name, csize, &(p->to_rt_rb))))
	return retval;

    rtapi_snprintf(name, sizeof(name), "%s.%d.out", comp, n);
    if ((retval = create_or_attach(name, rsize, &(p->from_rt_rb))))
	return retval;

    p->to_rt_rb.header->reader = comp_id;
    p->from_rt_rb.header->writer = comp_id;

    rtapi_snprintf(name, sizeof(name), "%s.%d.update", comp, n);
    if ((retval = hal_export_funct(name, update_pbring, &(pbring_array[n]),
				   1, 0, comp_id))) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: %s funct export failed\n", comp, name);
	hal_exit(comp_id);
	return -1;
    }
    return 0;
}
