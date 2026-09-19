/*
  License GPL Version 2
*/

/* switchkins_setup.c: the part of a switchkins module that depends on the
*  module supplying switchkinsSetup().  Kept apart from switchkins.c so
*  that a halcompile component, which registers its types itself and has
*  no switchkinsSetup(), can link the core without it.
*
*  switchkinsRunSetup() is what rtapi_app_main() and EXTRA_SETUP() call
*  before switchkinsInit().  kinsDescribe() is the description a copy of
*  the module loaded outside RT answers with: the RT instance describes
*  itself from its own state, a fresh copy replays setup first, so the
*  types come out the way the module parameters decide them.
*/
#include <rtapi.h>
#include <rtapi_string.h>
#include <hal.h>

#include <switchkins.h>

int switchkinsRunSetup(kparms* kp, const char* sparm)
{
    KS ksetup[3] = {NULL};
    KF kfwd[3]   = {NULL};
    KI kinv[3]   = {NULL};
    int i;

    if (!kp) { return -1; }
    memset(kp, 0, sizeof(*kp));

    // defaults prior to switchkinsSetup() call
    kp->kinsname   = NULL;
    kp->halprefix  = NULL;
    kp->required_coordinates = "";
    kp->max_joints        =  0; // Setup must supply
    kp->allow_duplicates  =  0;
    kp->fwd_iterates_mask =  0;
    kp->gui_kinstype      = -1; // negative means: not used

    kp->sparm = (char*)sparm; // module parm passed to kins

    // switchkinsSetup() provides types 0,1,2 and may also call
    // switchkinsRegister() or switchkinsRegisterOps() for any others
    if (switchkinsSetup(kp,
                        &ksetup[0], &ksetup[1], &ksetup[2],
                        &kfwd[0],   &kfwd[1],   &kfwd[2],
                        &kinv[0],   &kinv[1],   &kinv[2])) {
        rtapi_print_msg(RTAPI_MSG_ERR,"\nSwitchkins FAIL:<setup>\n");
        return -1;
    }

    // the types switchkinsSetup() supplied go in by the same route as
    // any other, so that providing one twice is caught
    for (i=0; i < 3; i++) {
        if (!ksetup[i] && !kfwd[i] && !kinv[i]) { continue; }
        if (switchkinsRegister(i, ksetup[i], kfwd[i], kinv[i])) { return -1; }
    }

    if (!kp->kinsname) {
        rtapi_print_msg(RTAPI_MSG_ERR,"\nSwitchkins FAIL:<Missing kinsname>\n");
        return -1;
    }
    return 0;
} // switchkinsRunSetup()

int kinsDescribe(const char *coordinates, const char *sparm,
                 kins_module_info *info)
{
    static kparms kp;
    (void)coordinates; // the map is the caller's business, see kinsParamsInit()

    if (!info) { return -1; }

    // the RT instance knows itself already
    if (switchkinsDescribe(info) == 0) { return 0; }

    // a copy outside RT: register the types the way the module would
    if (switchkinsRunSetup(&kp, sparm)) { return -1; }
    if (switchkinsDescribeSetup(&kp, info)) { return -1; }
    return 0;
} // kinsDescribe()

EXPORT_SYMBOL(switchkinsRunSetup);
EXPORT_SYMBOL(kinsDescribe);
