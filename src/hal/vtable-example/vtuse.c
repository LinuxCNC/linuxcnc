
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include "hal_priv.h"

#include "vtexample.h"

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("demo component using a vtable");
MODULE_LICENSE("GPL");

static char *name = "example-vtable";
RTAPI_MP_STRING(name, "vtable name");

static int version = 1234;
RTAPI_MP_INT(version, "vtable version tag");

static int comp_id, vtable_id;
static char *compname = "vtuse";

static vtexample_t *vtp;

int rtapi_app_main(void)
{
    comp_id = hal_init(compname);
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed - %d\n",
			compname, comp_id);
	return -1;
    }

    vtable_id = hal_reference_vtable(name, version, (void **) &vtp);
    if (vtable_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"ERROR: %s: hal_reference_vtable(%s,%d) failed: %d\n",
			compname, name, version, vtable_id);
	hal_exit(comp_id);
	return -ENOENT;
    }
    hal_ready(comp_id);

    // at this point, the vtable can be used:
    vtp->hello(compname);

    return 0;
}

void rtapi_app_exit(void)
{

    // the vtable can still be used:
    vtp->goodbye(compname);

    int retval = hal_unreference_vtable(vtable_id);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: hal_unreference_vtable(%d) failed: %d",
			compname, vtable_id, retval);
    }
    // NB: vtable methods may not be referenced anymore here
    // vtp is a dangling reference (!)
    // calling an vtp->method() here will crash rtapi_app (userland) or panic (kthreads)

    hal_exit(comp_id);
}
