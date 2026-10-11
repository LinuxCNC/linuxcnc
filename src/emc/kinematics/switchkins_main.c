/*
  Copyright 2019 Dewey Garrett <dgarrett@panix.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/* switchkins_main.c provides rtapi_app_main() for kinematics modules
*  built around switchkins.c.  A module that gets its rtapi_app_main()
*  from somewhere else (a halcompile component, for instance) links
*  switchkins.c alone and calls switchkinsInit() itself.
*
*  Using modules must supply function: switchkinsSetup()
*/
#include <rtapi.h>
#include <rtapi_app.h>
#include <hal.h>

#include "switchkins.h"

static char *coordinates;
RTAPI_MP_STRING(coordinates, "Axes-to-joints-ordering");
static char *sparm;
RTAPI_MP_STRING(sparm,  "switchkins module-specific parameter");

MODULE_LICENSE("GPL");

static int comp_id = -1;

int rtapi_app_main(void)
{
    kparms kp;
    KS ksetup[3] = {NULL};
    KF kfwd[3]   = {NULL};
    KI kinv[3]   = {NULL};
    int i;

    // defaults prior to switchkinsSetup() call
    kp.kinsname   = NULL;
    kp.halprefix  = NULL;
    kp.required_coordinates = "";
    kp.max_joints        =  0; // Setup must supply
    kp.allow_duplicates  =  0;
    kp.fwd_iterates_mask =  0;
    kp.gui_kinstype      = -1; // negative means: not used

    kp.sparm = sparm; // module parm passed to kins

    // switchkinsSetup() provides types 0,1,2 and may also call
    // switchkinsRegister() for any others
    if (switchkinsSetup(&kp,
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

    if (!kp.kinsname) {
        rtapi_print_msg(RTAPI_MSG_ERR,"\nSwitchkins FAIL:<Missing kinsname>\n");
        return -1;
    }

    comp_id = hal_init(kp.kinsname);
    if (comp_id < 0) return comp_id;

    if (switchkinsInit(comp_id, &kp, coordinates)) {
        hal_exit(comp_id);
        return -1;
    }

    hal_ready(comp_id);
    return 0;
} // rtapi_app_main()

void rtapi_app_exit(void) { hal_exit(comp_id); }
