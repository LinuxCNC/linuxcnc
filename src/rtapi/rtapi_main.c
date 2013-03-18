
#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("RTAPI stubs for userland threadstyles");
MODULE_LICENSE("GPL2 or later");

int rtapi_app_main(void)
{
    rtapi_print_msg(RTAPI_MSG_ERR,"RTAPI startup\n");
    return 0;
}

void rtapi_app_exit(void)
{
    rtapi_print_msg(RTAPI_MSG_ERR,"RTAPI exit\n");
}
