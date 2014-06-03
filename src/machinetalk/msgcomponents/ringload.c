
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */
#include "hal_priv.h"		/* HAL private API decls */
#include "hal_ring.h"	        /* ringbuffer declarations */

/* module information */
MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("HAL ring loading demo Component for EMC HAL");
MODULE_LICENSE("GPL");

static int num_rings = 1;
RTAPI_MP_INT(num_rings, "number of rings");
static int size = 4096;
RTAPI_MP_INT(size, "size of ring buffer");
static int mode;
RTAPI_MP_INT(mode, "buffer mode - 0: record (default); 1: stream");
static int spsize = 0;
RTAPI_MP_INT(spsize, "size of scratchpad area");
static int in_halmem = 1;
RTAPI_MP_INT(in_halmem, "allocate ring in HAL shared memory");

static int comp_id;		/* component ID */

int rtapi_app_main(void)
{
    int n, retval;
    char ringname[HAL_NAME_LEN + 1];
    int flags = 0;

    comp_id = hal_init("ringload");
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"ringload: ERROR: hal_init() failed: %d\n", comp_id);
	return -1;
    }
    if (mode)
	flags = RINGTYPE_STREAM;
    if (in_halmem)
	flags |= ALLOC_HALMEM;

    for (n = 0; n < num_rings; n++) {
	snprintf(ringname, HAL_NAME_LEN, "ring_%d",n);
	if ((retval = hal_ring_new(ringname, size, spsize,flags))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "ringload: failed to create new ring %s: %d\n",
			    ringname, retval);
	}
	rtapi_print_msg(RTAPI_MSG_DBG,
			"ringload: ring '%s' %s mode created\n",
			ringname, mode ? "stream":"record");
    }
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    int retval, n;
    char ringname[HAL_NAME_LEN + 1];

    for (n = 0; n < num_rings; n++) {
	snprintf(ringname, HAL_NAME_LEN, "ring_%d",n);
	if ((retval = hal_ring_delete(ringname))) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
			    "ringload: failed to delete ring %s: %d\n",
			    ringname, retval);
	}
    }
    hal_exit(comp_id);
}
