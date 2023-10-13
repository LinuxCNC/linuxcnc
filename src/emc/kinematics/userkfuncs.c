/* userkfuncs.c: template file for a user set of
**               switchable kinematics functions.
** License GPL Version 2
**
** Example Usage (for customizing the genser-switchkins module):
** (works with rtpreempt only rtai --> Makefile needs work)
**
**        LDIR is LinuxCNC git root directory
**        UDIR is user directory (not in LinuxCNC git tree)
**  1) $ cp LDIR/src/emc/kinematics/userkfuncs.c  UDIR/my_userk.c
**  2) $ edit   UDIR/my_userk.c as required
**  3) $ source LDIR/scripts/rip-environment
**  4) For genser-switchkins module use make command line option:
**     $ cd LDIR/src
**     $ userkfuncs=UDIR/my_userk.c make && sudo make setuid
*/

// typical includes:
//#include "rtapi_math.h" // if reqd
#include "kinematics.h"
#include "hal.h"

// Add for kins based on genserkins:
// #include "genserkins.h" //includes gomath,hal

//**********************************************************************
// static local variables and functions go here

static int userk_inited = 0;
static struct udata {
    hal_s32_t *fct;
    hal_s32_t *ict;
} *udata;

//**********************************************************************
int userkKinematicsSetup(const int   comp_id,
                         const char* coordinates,
                         kparms*     kp)
{
    int res=0;
    rtapi_print("\nuserkKinematicsSetup:\n"
                  "   %s <%s> max_joints=%d allow_duplicates=%d\n\n",
                __FILE__,coordinates,
                kp->max_joints,kp->allow_duplicates);


    udata = hal_malloc(sizeof(struct udata));
    if (!udata) goto error;

    // HAL_IO used to allow resetting demo pins:
    res += hal_pin_s32_new("userk.fct", HAL_IO, &(udata->fct), comp_id);
    res += hal_pin_s32_new("userk.ict", HAL_IO, &(udata->ict), comp_id);
    if (res) goto error;

    userk_inited = 1;
    return 0; // 0 ==> OK

error:
    return -1;
}

int userkKinematicsForward(const double *joint,
                           struct EmcPose * world,
                           const KINEMATICS_FORWARD_FLAGS * fflags,
                           KINEMATICS_INVERSE_FLAGS * iflags)
{
    if (!userk_inited) {
        rtapi_print_msg(RTAPI_MSG_ERR,
             "userkKinematics: not initialized\n");
        return -1;
    }
    (*udata->fct)++;
    return identityKinematicsForward(joint,world,fflags,iflags);
}

int userkKinematicsInverse(const EmcPose * pos,
                           double *joint,
                           const KINEMATICS_INVERSE_FLAGS * iflags,
                           KINEMATICS_FORWARD_FLAGS * fflags)
{
    (*udata->ict)++;
    return identityKinematicsInverse(pos,joint,iflags,fflags);
}
