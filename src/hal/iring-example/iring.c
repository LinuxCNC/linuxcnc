#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include "hal_priv.h"
#include "hal_accessor.h"
#include "hal_iring.h"


MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("iring API demo");
MODULE_LICENSE("GPL");
RTAPI_TAG(HAL,HC_INSTANTIABLE);

static int comp_id;
static char *compname = "iring";

// the message struct passed through the ringbuffer
struct imsg {
    hal_s32_t cnt;
    hal_s32_t val;
};

// size the ringbuffer so it can hold up to 3 message structs
#define IRING_SIZE (3 * record_usage(sizeof(struct imsg)))

struct inst_data {
    // used by sample
    bit_pin_ptr toggle;
    hal_bit_t   prev_toggle; // change tracking
    hal_s32_t   transitions; // # of transitions on toggle
    s32_pin_ptr value;       // sampled at toggle edge
    u32_pin_ptr write_fail;  // canary value
    u32_pin_ptr writes;      // canary value

    // used by update
    s32_pin_ptr output;      // reflects value
    u32_pin_ptr count;       // at a certain transition count
    u32_pin_ptr updates;     // canary - of valid messages

    // ringbuffer shared between functs
    iring_t *ir;
};

static int sample(void *arg, const hal_funct_args_t *fa)
{
    struct inst_data *ip = arg;
    hal_bit_t t = get_bit_pin(ip->toggle);

    if (t ^ ip->prev_toggle) { // edge detect
	ip->prev_toggle = t;
	ip->transitions++;

	// pass a message to the other thread funct
	struct imsg *im;
	if (record_write_begin(&ip->ir->rb, (void **)&im, sizeof(struct imsg))) {
	    incr_u32_pin(ip->write_fail, 1);
	    return 0;
	}
	im->cnt = ip->transitions;
	im->val = get_s32_pin(ip->value);
	record_write_end(&ip->ir->rb, im, sizeof(struct imsg));

	incr_u32_pin(ip->writes, 1);
    }
    return 0;
}

static int update(void *arg, const hal_funct_args_t *fa)
{
    struct inst_data *ip = arg;
    struct imsg *im = NULL;;
    ringsize_t size = 0;

    // retrieve apply all messages. Could be optimized.
    while (record_read(&ip->ir->rb, (const void**)&im, &size) == 0) {
	HAL_ASSERT(im != NULL);
	HAL_ASSERT(size == sizeof(struct imsg));
	set_s32_pin(ip->output, im->val);
	set_u32_pin(ip->count, im->cnt);
	incr_u32_pin(ip->updates, 1);
	record_shift(&ip->ir->rb);
    }
    return 0;
}

static int instantiate_iring(const char *name,
			     const int argc,
			     const char**argv)
{
    struct inst_data *ip;
    int inst_id;
    if ((inst_id = hal_inst_create(name, comp_id,
				  sizeof(struct inst_data),
				   (void **)&ip)) < 0)
	return -1;

    ip->toggle = halx_pin_bit_newf(HAL_IN, inst_id, "%s.toggle", name);
    HAL_ASSERT(!bit_pin_null(ip->toggle));

    ip->value = halx_pin_s32_newf(HAL_IN, inst_id, "%s.value", name);
    HAL_ASSERT(!s32_pin_null(ip->value));

    ip->write_fail = halx_pin_u32_newf(HAL_OUT, inst_id, "%s.write_fail", name);
    HAL_ASSERT(!u32_pin_null(ip->write_fail));

    ip->writes = halx_pin_u32_newf(HAL_OUT, inst_id, "%s.writes", name);
    HAL_ASSERT(!u32_pin_null(ip->writes));

    ip->output = halx_pin_s32_newf(HAL_OUT, inst_id, "%s.output", name);
    HAL_ASSERT(!s32_pin_null(ip->output));

    ip->count = halx_pin_u32_newf(HAL_OUT, inst_id, "%s.count", name);
    HAL_ASSERT(!u32_pin_null(ip->count));

    ip->updates = halx_pin_u32_newf(HAL_OUT, inst_id, "%s.updates", name);
    HAL_ASSERT(!u32_pin_null(ip->updates));

    ip->ir = hal_iring_alloc(IRING_SIZE);
    HAL_ASSERT(ip->ir != NULL);

    hal_export_xfunct_args_t xfunct_args = {
        .type = FS_XTHREADFUNC,
        .funct.x = sample,
        .arg = ip,
        .uses_fp = 0,
        .reentrant = 0,
        .owner_id = inst_id
    };
    hal_export_xfunctf(&xfunct_args, "%s.sample", name);
    xfunct_args.funct.x = update;
    hal_export_xfunctf(&xfunct_args, "%s.update", name);
    return 0;
}


static int delete_iring(const char *name, void *inst, const int inst_size)
{
    struct inst_data *ip = inst;

    // irings must be explicitly freed for now
    hal_iring_free(&ip->ir);
    HAL_ASSERT(ip->ir == NULL);
    return 0;
}

int rtapi_app_main(void)
{
    comp_id = hal_xinit(TYPE_RT, 0, 0, instantiate_iring, delete_iring, compname);
    if (comp_id < 0)
	return comp_id;

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}
