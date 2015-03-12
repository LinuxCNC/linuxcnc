// instantiable lookup table component with configurable number of pins
// usage:
//
// halcmd loadrt lutn
// halcmd newinst lutn and2.0 inputs=2 function=0x8
// halcmd newinst lutn or2.0  inputs=2 function=0xe


//
// # src/hal/icomp-example$ python lut5.py -n2 'i0 & i1'
// # expression = i0 & i1
// #in: i4 i3 i2 i1 i0 out weight
// # 0:  0  0  0  0  0  0
// # 1:  0  0  0  0  1  0
// # 2:  0  0  0  1  0  0
// # 3:  0  0  0  1  1  1   0x8
// # setp lut5.N.function 0x8
// # src/hal/icomp-example$ python lut5.py -n2 'i0 | i1'
// # expression = i0 | i1
// #in: i4 i3 i2 i1 i0 out weight
// # 0:  0  0  0  0  0  0
// # 1:  0  0  0  0  1  1   0x2
// # 2:  0  0  0  1  0  1   0x4
// # 3:  0  0  0  1  1  1   0x8
// # setp lut5.N.function 0xe


#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include "hal_priv.h"

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("instantiable lookup table component with configurable number of pins");
MODULE_LICENSE("GPL");

static int comp_id;
static char *compname = "lutn";

struct inst_data {
    int inputs;
    hal_u32_t function;
    hal_bit_t *out;
    hal_bit_t *in[0];
};

static int function = 0;
RTAPI_IP_INT(function, "lookup function - see man lut5");

static int inputs = 0;
RTAPI_IP_INT(inputs, "number of input pins, in0..inN");

static void lutn(void *arg, long period)
{
    struct inst_data *ip = arg;

    int shift = 0, i;

    for (i = 0; i < ip->inputs; i++)
	if (*(ip->in[i])) shift += (1 << i);

    *(ip->out) = (ip->function & (1 << shift)) != 0;
}

static int instantiate_lutn(const char *name,
			    const int argc,
			    const char**argv)
{
    struct inst_data *ip;
    int i, inst_id;



    if ((inputs < 1) || (inputs > 5)) {
	hal_print_msg(RTAPI_MSG_ERR,
		      "%s: invalid parameter inputs=%d, valid range=1..5",
		      name, inputs);
	return -1;
    }
    if ((function == 0) || (function == -1)) {
	hal_print_msg(RTAPI_MSG_ERR,
		      "%s: function=0x%x does not make sense",
		      name, function);
	return -1;
    }

    if ((inst_id = hal_inst_create(name, comp_id,
				  sizeof(struct inst_data) + inputs * sizeof(hal_bit_t *),
				   (void **)&ip)) < 0)
	return -1;

    hal_print_msg(RTAPI_MSG_DBG,
		  "%s: name='%s' inputs=%d function=0x%x ip=%p",
		  __FUNCTION__, name, inputs, function, ip);
    // record instance params
    ip->inputs = inputs;
    ip->function = function;

    // export per-instance HAL objects
    for (i = 0; i < ip->inputs; i++)
	if (hal_pin_bit_newf(HAL_IN, &(ip->in[i]), inst_id, "%s.in%d", name, i))
	    return -1;
    if (hal_pin_bit_newf(HAL_OUT, &(ip->out), inst_id, "%s.out", name))
	return -1;
   if (hal_export_functf(lutn, ip, 0, 0, inst_id, "%s.funct", name))
	return -1;
    return 0;
}


int rtapi_app_main(void)
{
    comp_id = hal_xinit(TYPE_RT, 0, 0, instantiate_lutn, NULL, compname);
    if (comp_id < 0)
	return -1;

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}
