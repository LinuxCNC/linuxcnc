// instantiable lookup table component with configurable number of pins
// usage:
//
// halcmd loadrt lutn
// halcmd newinst lutn and2.0 pincount=2 function=0x8
// halcmd newinst lutn or2.0  pincount=2 function=0xe


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
#include "hal_accessor.h"

#include <stdio.h>
#include <string>
using namespace std;
#include <pin.hh>
#define CPPINS

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("instantiable lookup table component with configurable number of pins");
MODULE_LICENSE("GPL");
RTAPI_TAG(HAL,HC_INSTANTIABLE);

static int comp_id;
static const char *compname = "cpptest";

struct inst_data {
    Pin p;
    int pincount;
    hal_u32_t functn;
    bit_pin_ptr out;
    bit_pin_ptr in[0];
};


static unsigned functn = 0xff;
RTAPI_IP_UINT(functn, "lookup function - see man lut5");

static int pincount = 2;
RTAPI_IP_INT(pincount, "number of input pins, in0..inN");

static const char *foo = "fasel";
RTAPI_IP_STRING(foo, "famous foo param");

static int lutn(void *arg, const hal_funct_args_t *fa)
{
    // using the extended thread function export call makes the following
    // context visible here:
    //   fa_period(fa)            the period parameter in the legacy signature
    //   fa_start_time(fa)        the start time of this function
    //   fa_thread_start_time(fa) the start time of the first function in the funct chain
    //   fa_thread_name(fa)       the name of the calling thread
    //   fa_funct_name(fa)        the name of this funct
    // otherwise it is fully backwards compatible.
    // This makes - among others - timing data available 'for free' if needed.
    // see hal_export_xfunctf() below for the usage difference to hal_export_functf().

    // pointer to the instance data blob, as allocated by hal_inst_create()
    struct inst_data *ip = (struct inst_data *)arg;
    int shift = 0, i;

    for (i = 0; i < ip->pincount; i++)
	if (get_bit_pin(ip->in[i])) shift += (1 << i);

    set_bit_pin(ip->out, (ip->functn & (1 << shift)) != 0);

#ifdef CPPINS
    ip->p += 100;
    hal_s32_t foo = ip->p;
    if (foo <= ip->p)
	HALDBG("true");

    if (123 <= ip->p)
	HALDBG("true");

    ip->p |= 0x0f; // ip->p;
#endif
    return 0;
}

static int instantiate_lutn(const char *name,
			    const int argc,
			    const char**argv)
{
    struct inst_data *ip;
    int i, inst_id;

    if ((pincount < 1) || (pincount > 5)) {
	hal_print_msg(RTAPI_MSG_ERR,
		      "%s: invalid parameter pincount=%d, valid range=1..5",
		      name, pincount);
	return -1;
    }
    if ((functn == 0) || (functn == -1)) {
	hal_print_msg(RTAPI_MSG_ERR,
		      "%s: functn=0x%x does not make sense",
		      name, functn);
	return -1;
    }

    if ((inst_id = hal_inst_create(name, comp_id,
				  sizeof(struct inst_data) + pincount * sizeof(hal_bit_t *),
				   (void **)&ip)) < 0)
	return -1;

    HALDBG("name='%s' pincount=%d functn=0x%x ip=%p foo='%s'",
	   name, pincount, functn, ip, foo);


    // record instance params
    ip->pincount = pincount;
    ip->functn = functn;

    // export per-instance HAL objects
    for (i = 0; i < ip->pincount; i++) {
	ip->in[i] = halx_pin_bit_newf(HAL_IN, inst_id, "%s.in%d", name,i);
	if (bit_pin_null(ip->in[i]))
	    return _halerrno;
    }
    ip->out = halx_pin_bit_newf(HAL_OUT, inst_id, "%s.out", name);
    if (bit_pin_null(ip->out))
	return _halerrno;


#ifdef CPPINS
    ip->p =  Pin(HAL_OUT, inst_id, 12345, "%s.test", name);
    HALDBG("sizeof(Pin) = %zu", sizeof(Pin));
#endif

    // exporting 'lutn' as an extended thread function
    // this has the advantage of better context exposure in the thread function
    // (e.g thread and funct name, timing information etc)
    // it is fully backwards compatible and the recommened way of doing an export
    hal_export_xfunct_args_t xf = {};
    xf.type = FS_XTHREADFUNC;
    xf.funct.x = lutn;
    xf.arg = ip;
    xf.uses_fp = 0;
    xf.reentrant = 0;
    xf.owner_id = inst_id;
    return hal_export_xfunctf(&xf, "%s.funct", name);
}

extern "C" int rtapi_app_main(void)
{

    string s(compname);
    
    comp_id = hal_xinit(TYPE_RT, 0, 0, instantiate_lutn, NULL, compname);
    if (comp_id < 0)
	return comp_id;

    hal_ready(comp_id);
    fprintf(stderr, "-----%s\n", s.c_str());
    return 0;
}

extern "C" void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}
