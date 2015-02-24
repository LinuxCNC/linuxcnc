
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include "hal_priv.h"

#include "vtexample.h"

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("demo component exporting a vtable");
MODULE_LICENSE("GPL");

static char *name = "example-vtable";
RTAPI_MP_STRING(name, "vtable name");

static int version = 1234;
RTAPI_MP_INT(version, "vtable version tag");

static int comp_id, vtable_id;
static char *compname = "vtexport";

extern vtexample_t demo_vtable;  // see vcode.c & vtexample.h

int rtapi_app_main(void)
{
    comp_id = hal_init(compname);
    if (comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed - %d\n",
			compname, comp_id);
	return -1;
    }
    hal_ready(comp_id);

    vtable_id = hal_export_vtable(name, version, &demo_vtable, comp_id);
    if (vtable_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR,
			"%s: ERROR: hal_export_vtable(%s,%d,%p) failed: %d\n",
			compname, name, version, &demo_vtable, vtable_id );
	hal_exit(comp_id);
	return -ENOENT;
    }
    return 0;
}

void rtapi_app_exit(void)
{
    int retval = hal_remove_vtable(vtable_id);
    if (retval < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "%s: vtable %d not removed\n",
			  compname, vtable_id);
    } else {
	// safe to exit comp
	hal_exit(comp_id);
    }
}
