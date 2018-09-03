
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"
#include "hal_priv.h"

MODULE_AUTHOR("Michael Haberler");
MODULE_DESCRIPTION("demo component for extended funct API");
MODULE_LICENSE("GPL");

static int comp_id;
static char *compname = "ufdemo";


// a tradional RT thread function.
static void legacy_funct(void *arg, long period)
{
    // period is somewhat useless as it only gives the nominal, but
    // not the actual thread invocation time (which already has been
    // measured in the calling library code)
}

// an extended RT thread function, more useful arguments passed in:
// extended thread functions can be addf'd like legacy functions, the right thing will happen
// time observation for free!
static int xthread_funct(void *arg, const hal_funct_args_t *fa)
{
    long period __attribute__((unused))  = fa_period(fa);

    // the following accessors are available here:

    // fa_period(fa) - formerly 'long period'
    // fa_thread_start_time(fa): _actual_ start time of thread invocation
    // fa_start_time(fa):        _actual_ start time of function invocation
    // fa_thread_name(fa): name of the calling thread (char *)
    // fa_funct_name(fa):  name of the this called function (char *)

    return 0;
}


// a 'userland function' - exported similar to a thread function
// supports user-triggerd one-off execution of code in RT
//
// can be called by:
//     halcmd call ufdemo.demo-funct [arg1 [...]]
//
// (or an equivalent HAL library function, or Python)
// argument handling like "main(int argc, char **argv)"
// userland functions cannot be added to a threads - no point
static int usrfunct_demo(const hal_funct_args_t *fa)
{
    const int argc = fa_argc(fa);
    const char **argv = fa_argv(fa);

    rtapi_print_msg(RTAPI_MSG_INFO, "%s: userfunct '%s' called, arg='%s' argc=%d\n",
		    compname,  fa_funct_name(fa), (const char *)fa_arg(fa), argc);
    int i;
    for (i = 0; i < argc; i++)
	rtapi_print_msg(RTAPI_MSG_INFO, "    argv[%d] = \"%s\"\n",
			i,argv[i]);
    return argc;
}

int rtapi_app_main(void)
{

    comp_id = hal_init(compname);
    if (comp_id < 0)
	return -1;

    // exporting a legacy thread function - as per 'man hal_export_funct':
    if (hal_export_funct("ufdemo.legacy-funct", legacy_funct,
			 "l-instance-data", 0, 0, comp_id))
	return -1;

    // exporting an extended thread function:
    hal_export_xfunct_args_t xtf = {
	.type = FS_XTHREADFUNC,
	.funct.x = xthread_funct,
	.arg = "x-instance-data",
	.uses_fp = 0,
	.reentrant = 0,
	.owner_id = comp_id
    };
    if (hal_export_xfunctf(&xtf,"%s.xthread-funct", compname))
	return -1;

    // exporting a userland-callable thread function:
    hal_export_xfunct_args_t uf = {
	.type = FS_USERLAND,
	.funct.u = usrfunct_demo,
	.arg = "u-instance-data",
	.owner_id = comp_id
    };
    if (hal_export_xfunctf( &uf, "%s.demo-funct", compname))
	return -1;

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void)
{
    hal_exit(comp_id);
}
