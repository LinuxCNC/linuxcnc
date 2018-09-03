#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include "hal_priv.h"
#include "hal_accessor.h"

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("pseudorandom number generator");
MODULE_LICENSE("GPL");
RTAPI_TAG(HAL,HC_INSTANTIABLE);

static int low = 0;
RTAPI_IP_INT(low, "lower end of range");

static int high = 0;
RTAPI_IP_INT(high, "upper end of range");

static int type = 0;
RTAPI_IP_INT(type, "output type: 0:s32, 1: float");

static int comp_id;
static char *compname = "random";

/* lran2.h
 * by Wolfram Gloger 1996.
 *
 * A small, portable pseudo-random number generator.
 */
#define LRAN2_MAX 714025l /* constants for portable */
#define IA	  1366l	  /* random number generator */
#define IC	  150889l /* (see e.g. `Numerical Recipes') */

struct lran2_st {
    long x, y, v[97];
};

static void lran2_init(struct lran2_st* d, long seed);
static long lran2(struct lran2_st* d);
//

struct inst_data {
    int wantfloat;
    struct lran2_st status;

    s32_pin_ptr sout;
    s32_pin_ptr supper;
    s32_pin_ptr slower;

    float_pin_ptr fout;
    float_pin_ptr fupper;
    float_pin_ptr flower;
};

static double lran2_range(struct inst_data *ip,
			  const double low,
			  const double high)
{
    return low + (lran2(&ip->status) / ((double)LRAN2_MAX / (high-low)));
}
/* [M,N): */

/* double randMToN(double M, double N) */
/* { */
/*     return M + (rand() / ( (double)RAND_MAX / (N-M) ) ) ;   */
/* } */
static int random(void *arg, const hal_funct_args_t *fa)
{
    struct inst_data *ip = arg;
    if (ip->wantfloat) {
	hal_float_t r = lran2_range(ip,
				    get_float_pin(ip->flower),
				    get_float_pin(ip->fupper));
	set_float_pin(ip->fout, (hal_float_t)r);
    } else {
	hal_s32_t r = lran2_range(ip,
				  get_s32_pin(ip->slower),
				  get_s32_pin(ip->supper));
	set_s32_pin(ip->sout, r);
    }
    return 0;
}

static int instantiate_random(const char *name,
			      const int argc,
			      const char**argv)
{
    struct inst_data *ip;
    int inst_id;

    inst_id = hal_inst_create(name, comp_id,
			      sizeof(struct inst_data),
			      (void **)&ip);
    hal_errorcount(1); // clear HAL error counter

    ip->wantfloat = (type != 0);


    if (ip->wantfloat) {
	ip->fupper = halxd_pin_float_newf(HAL_IN, inst_id,
					  (hal_float_t) high, "%s.max", name);
	ip->flower = halxd_pin_float_newf(HAL_IN, inst_id,
					  (hal_float_t) low, "%s.min", name);
	ip->fout = halx_pin_float_newf(HAL_OUT, inst_id, "%s.out", name);
    } else {
	ip->supper = halxd_pin_s32_newf(HAL_IN, inst_id, high, "%s.max", name);
	ip->slower = halxd_pin_s32_newf(HAL_IN, inst_id, low, "%s.min", name);
	ip->sout = halx_pin_s32_newf(HAL_OUT, inst_id, "%s.out", name);
    }
    lran2_init(&ip->status, (long) rtapi_get_clocks());
    hal_export_xfunct_args_t xfunct_args = {
        .type = FS_XTHREADFUNC,
        .funct.x = random,
        .arg = ip,
        .uses_fp = ip->wantfloat,
        .reentrant = 0,
        .owner_id = inst_id
    };
    hal_export_xfunctf(&xfunct_args, "%s.funct", name);
    if (hal_errorcount(0)) // uh oh
	return -EINVAL;
    return 0;
}


int rtapi_app_main(void)
{
    comp_id = hal_xinit(TYPE_RT, 0, 0,
			instantiate_random,
			NULL, compname);
    if (comp_id < 0)
	return comp_id;
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}

static void
lran2_init(struct lran2_st* d, long seed)
{
    long x;
    int j;

    x = (IC - seed) % LRAN2_MAX;
    if(x < 0) x = -x;
    for(j=0; j<97; j++) {
	x = (IA*x + IC) % LRAN2_MAX;
	d->v[j] = x;
    }
    d->x = (IA*x + IC) % LRAN2_MAX;
    d->y = d->x;
}

static long
lran2(struct lran2_st* d)
{
    int j = (d->y % 97);

    d->y = d->v[j];
    d->x = (IA*d->x + IC) % LRAN2_MAX;
    d->v[j] = d->x;
    return d->y;
}

