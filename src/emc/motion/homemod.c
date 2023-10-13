/*
**Notes:
**  1) The rtapi_app_main() creates a minimal component (homemod)
**     allowing use with:
**     'halcmd loadrt homemod' (loads module: homemod.so)
**  2) homing.c provides all functions required by subsequent load
**     of motmod
*/
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
MODULE_LICENSE("GPL");

#define HOMEMOD_DEBUG
#undef  HOMEMOD_DEBUG

// provision for testing use of module parameters:
static char *home_parms;
RTAPI_MP_STRING(home_parms,"Example home parms");

static int homemod_id;
int rtapi_app_main(void)
{
#ifdef HOMEMOD_DEBUG
    if (home_parms) {
        rtapi_print("%s:%s: home_parms=%s\n",__FILE__,__FUNCTION__,home_parms);
    }
#endif

    char* emsg;
    homemod_id = hal_init("homemod"); // dlopen(".../homemod.so")
    if (homemod_id < 0) {emsg="hal_init()"; goto error;}

    hal_ready(homemod_id);
    return 0;

error:
    rtapi_print_msg(RTAPI_MSG_ERR,"\nhomemod FAIL:<%s>\n",emsg);
    hal_exit(homemod_id);
    return -1;
}

void rtapi_app_exit(void)
{
    hal_exit(homemod_id);
    return;
}
