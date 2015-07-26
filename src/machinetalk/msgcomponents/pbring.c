// demo actor component to show sending/receiving protobuf-encoded messages over HAL rings
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */
#include "hal_ring.h"
#include "hal_priv.h"

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
    pb_Container rx, tx;
} pbring_inst_t;

enum {
    NBP_SIZE_FAIL   = -100,
    NBP_ENCODE_FAIL = -101,
    NBP_DECODE_FAIL = -102,
    RB_RESERVE_FAIL = -103,
    RB_WRITE_FAIL   = -104,
} rtmsg_errors_t;

static int comp_id;
static char *compname = "pbring";

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("Actor Test Component");
MODULE_LICENSE("GPL");

// marks this comp as instantiable so halcmd/cython API can do the right thing:
RTAPI_TAG(HAL, HC_INSTANTIABLE);

static int txnoencode = 0;
RTAPI_MP_INT(txnoencode, "pass unencoded pb_Container struct instead of pb message");

static int rxnodecode = 0;
RTAPI_MP_INT(rxnodecode, "expect unencoded pb_Container struct instead of pb message");


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
static int npb_send_msg(const void *msg, const pb_field_t *fields,
			ringbuffer_t *rb,  const hal_funct_args_t *fa)
{
    void *buffer;
    int retval;
    size_t size;

    // determine size
    pb_ostream_t sstream = PB_OSTREAM_SIZING;
    if (!pb_encode(&sstream, fields,  msg)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: sizing pb_encode(): %s written=%zu\n",
			fa_funct_name(fa), PB_GET_ERROR(&sstream), sstream.bytes_written);
	return NBP_SIZE_FAIL;
    }
    size = sstream.bytes_written;

    // preallocate memory in ringbuffer
    if ((retval = record_write_begin(rb, (void **)&buffer, size)) != 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: record_write_begin(%zu) failed: %d\n",
			fa_funct_name(fa), size, retval);
	return RB_RESERVE_FAIL;
    }

    // zero-copy encode directly into ringbuffer
    pb_ostream_t rstream = pb_ostream_from_buffer(buffer, size);
    if (!pb_encode(&rstream, fields,  msg)) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: pb_encode failed: %s, size=%zu written=%zu\n",
			fa_funct_name(fa),
			PB_GET_ERROR(&rstream),
			size,
			rstream.bytes_written);
	return NBP_ENCODE_FAIL;
    }

    // send it off
    if ((retval = record_write_end(rb, buffer, rstream.bytes_written))) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: record_write_end(%zu) failed: %d\n",
			fa_funct_name(fa), size, retval);
	return RB_WRITE_FAIL;
    }
    return 0;
}

static int decode_msg(pbring_inst_t *p,  pb_istream_t *stream,  const hal_funct_args_t *fa)
{
    if (!pb_decode(stream, pb_Container_fields, &p->rx)) {
	*(p->decodefail) += 1;
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: pb_decode(Container) failed: '%s'\n",
			fa_funct_name(fa), PB_GET_ERROR(stream));
	return -1;
    }

    if (p->rx.has_motcmd) {
	rtapi_print_msg(RTAPI_MSG_ERR, "Container.motcmd command=%d num=%d\n",
			p->rx.motcmd.command,p->rx.motcmd.commandNum);
	if (p->rx.motcmd.has_pos) {
	    char buf[200];
	    rtapi_format_pose(buf, sizeof(buf), &p->rx.motcmd.pos);
	    rtapi_print_msg(RTAPI_MSG_ERR, "motcmd: %s\n", buf);
	}
    }
    return 0;
}

static int update_pbring(void *arg, const hal_funct_args_t *fa)
{
    pbring_inst_t *p = (pbring_inst_t *) arg;

    ring_size_t cmdsize = record_next_size(&p->to_rt_rb);
    if (cmdsize < 0) {
	// command ring empty
	*(p->underrun) += 1;
	return 0;
    }
    const void *cmdbuffer = record_next(&p->to_rt_rb);
    pb_istream_t stream = pb_istream_from_buffer((void *) cmdbuffer, cmdsize);
    int retval;

    if (!decode_msg(p, &stream, fa)) {

	// process command here
	// prepare reply
	p->tx.has_motstat = true;
	p->tx.type =  pb_ContainerType_MT_MOTSTATUS;
	p->tx.note.funcs.encode = npb_encode_string;
	p->tx.note.arg = "hi there!";

	p->tx.motstat = (pb_MotionStatus) {
	    .commandEcho = p->rx.motcmd.command,
	    .commandNumEcho = p->rx.motcmd.commandNum,
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
	if ((retval = npb_send_msg(&p->tx, pb_Container_fields,
				   &p->from_rt_rb, 0))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,"error reply failed %d", retval);
	    *(p->sendfailed) +=1;
	} else
	    *(p->sent) += 1;
    } else {
	char errmsg[255];

	rtapi_snprintf(errmsg, sizeof(errmsg),
		       "%s: failed to decode (size %d): %s",
		       fa_funct_name(fa),
		       cmdsize,
		       PB_GET_ERROR(&stream));

	p->tx = (pb_Container) {
	    .type = pb_ContainerType_MT_MOTSTATUS, // FIXME
	    .note.funcs.encode = npb_encode_string,
	    .note.arg = (void *) errmsg
	};
	int retval;
	if ((retval = npb_send_msg(&p->tx, pb_Container_fields,
				   &p->from_rt_rb, 0))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,"error reply failed %d", retval);
	    *(p->sendfailed) += 1;
	} else
	    *(p->sent) += 1;
    }
    record_shift(&p->to_rt_rb);
    *(p->received) += 1;
    return 0;
}


// constructor - init all HAL pins, params, funct etc here
static int instantiate(const char *name, const int argc, const char**argv)
{
    struct inst_data *inst;
    int retval, inst_id;

    // allocate a named instance, and some HAL memory for the instance data
    if ((inst_id = hal_inst_create(name, comp_id,
				   sizeof(pbring_inst_t),
				   (void **)&inst)) < 0)
	return inst_id; // HAL library will log the failure cause

    pbring_inst_t *p = (pbring_inst_t *)inst;
    if ((retval = hal_pin_u32_newf(HAL_OUT, &(p->underrun),
				   inst_id, "%s.underrun", name)))
	return retval;
    if ((retval = hal_pin_u32_newf(HAL_OUT, &(p->received),
				   inst_id, "%s.received", name)))
	return retval;
    if ((retval = hal_pin_u32_newf(HAL_OUT, &(p->sent),
				   inst_id, "%s.sent", name)))
	return retval;
    if ((retval = hal_pin_u32_newf(HAL_OUT, &(p->sendfailed),
				   inst_id, "%s.sendfailed", name)))
	return retval;
    if ((retval = hal_pin_u32_newf(HAL_OUT, &(p->decodefail),
				   inst_id, "%s.decodefail", name)))
	return retval;
    *(p->underrun) = 0;
    *(p->received) = 0;
    *(p->sent) = 0;
    *(p->sendfailed) = 0;
    *(p->decodefail) = 0;

    unsigned flags;
    if ((retval = hal_ring_attachf(&(p->to_rt_rb), &flags,  "%s.in", name)) < 0)
	return retval;
    if ((flags & RINGTYPE_MASK) != RINGTYPE_RECORD) {
	HALERR("ring %s.in not a record mode ring: mode=%d",name, flags & RINGTYPE_MASK);
	return -EINVAL;
    }
    if ((retval = hal_ring_attachf(&(p->from_rt_rb), &flags,  "%s.out", name)) < 0)
	return retval;
    if ((flags & RINGTYPE_MASK) != RINGTYPE_RECORD) {
	HALERR("ring %s.out not a record mode ring: mode=%d",name, flags & RINGTYPE_MASK);
	return -EINVAL;
    }
    p->to_rt_rb.header->reader = inst_id;
    p->from_rt_rb.header->writer = inst_id;

    // exporting as extended thread function:
    hal_export_xfunct_args_t update_xf = {
        .type = FS_XTHREADFUNC,
        .funct.x = update_pbring,
        .arg = inst,
        .uses_fp = 1,
        .reentrant = 0,
        .owner_id = inst_id
    };
    return hal_export_xfunctf(&update_xf, "%s.update", name);
}

static int delete(const char *name, void *inst, const int inst_size)
{
    pbring_inst_t *p = (pbring_inst_t *)inst;
    int retval;
    char ringname[HAL_NAME_LEN + 1];

    rtapi_snprintf(ringname, sizeof(ringname), "%s.in", name);
    p->to_rt_rb.header->reader = 0;
    if ((retval = hal_ring_detach(ringname, &p->to_rt_rb)) < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: hal_ring_detach(%s) failed: %d\n",
			name, ringname, retval);
    }
    p->from_rt_rb.header->writer = 0;
    rtapi_snprintf(ringname, sizeof(ringname), "%s.out", name);
    if ((retval = hal_ring_detach(ringname, &p->from_rt_rb)) < 0)
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: hal_ring_detach(%s) failed: %d\n",
			name, ringname, retval);
    return retval;
}

int rtapi_app_main(void)
{
    comp_id = hal_xinit(TYPE_RT, 0, 0, instantiate, delete, compname);
    if (comp_id  < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: hal_xinit(%s) failed: %d\n",
			compname, compname, comp_id);
	return comp_id;
    }

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}
