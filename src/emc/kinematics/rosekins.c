/*
  Copyright 2016 Dewey Garrett <dgarrett@panix.com>

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

#include "kinematics.h"
#include "posemath.h"
#include "hal.h"
#include "rtapi.h"
#include "rtapi_math.h"
#include "rtapi_app.h"

const char* kinematicsGetName(void) { return "rosekins"; }

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsGetName);
MODULE_LICENSE("GPL");

/* ---- Math types and functions (from rosekins_math.h) ---- */

#ifndef PM_PI
#define PM_PI 3.14159265358979323846
#endif
#ifndef PM_2_PI
#define PM_2_PI 6.28318530717958647692
#endif
#ifndef TO_RAD
#define TO_RAD (PM_PI / 180.0)
#endif
#ifndef TO_DEG
#define TO_DEG (180.0 / PM_PI)
#endif
#ifndef rosekins_hypot
#define rosekins_hypot(a,b) (sqrt((a)*(a)+(b)*(b)))
#endif

typedef struct {
    int oldquad;
    int revolutions;
} rosekins_state_t;

typedef struct {
    double revolutions;
    double theta_degrees;
    double bigtheta_degrees;
} rosekins_output_t;

static int rosekins_forward_math(const double *joints, EmcPose *pos)
{
    double radius = joints[0];
    double z = joints[1];
    double theta = TO_RAD * joints[2];

    pos->tran.x = radius * cos(theta);
    pos->tran.y = radius * sin(theta);
    pos->tran.z = z;
    pos->a = 0;
    pos->b = 0;
    pos->c = 0;
    pos->u = 0;
    pos->v = 0;
    pos->w = 0;

    return 0;
}

static int rosekins_inverse_math(const EmcPose *pos, double *joints,
                                 rosekins_state_t *state,
                                 rosekins_output_t *output)
{
    double theta, bigtheta;
    int nowquad = 0;
    double x = pos->tran.x;
    double y = pos->tran.y;
    double z = pos->tran.z;

    if      (x >= 0 && y >= 0) nowquad = 1;
    else if (x <  0 && y >= 0) nowquad = 2;
    else if (x <  0 && y <  0) nowquad = 3;
    else if (x >= 0 && y <  0) nowquad = 4;

    if (state->oldquad == 2 && nowquad == 3) { state->revolutions += 1; }
    if (state->oldquad == 3 && nowquad == 2) { state->revolutions -= 1; }

    theta = atan2(y, x);
    bigtheta = theta + PM_2_PI * state->revolutions;

    if (output) {
        output->revolutions = state->revolutions;
        output->theta_degrees = theta * TO_DEG;
        output->bigtheta_degrees = bigtheta * TO_DEG;
    }

    joints[0] = rosekins_hypot(x, y);
    joints[1] = z;
    joints[2] = TO_DEG * bigtheta;
    joints[3] = 0;
    joints[4] = 0;
    joints[5] = 0;
    joints[6] = 0;
    joints[7] = 0;
    joints[8] = 0;

    state->oldquad = nowquad;
    return 0;
}

/* ---- End math functions ---- */

struct haldata {
    hal_float_t *revolutions;
    hal_float_t *theta_degrees;
    hal_float_t *bigtheta_degrees;
} *haldata;

/* State for revolution tracking */
static rosekins_state_t kins_state;

int kinematicsForward(const double *joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    (void)iflags;
    return rosekins_forward_math(joints, pos);
}

int kinematicsInverse(const EmcPose * pos,
                      double *joints,
                      const KINEMATICS_INVERSE_FLAGS * iflags,
                      KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)iflags;
    (void)fflags;
    rosekins_output_t output;
    int result = rosekins_inverse_math(pos, joints, &kins_state, &output);

    /* Write diagnostic values to HAL pins */
    *(haldata->revolutions) = output.revolutions;
    *(haldata->theta_degrees) = output.theta_degrees;
    *(haldata->bigtheta_degrees) = output.bigtheta_degrees;

    return result;
}

KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

static int comp_id;

void rtapi_app_exit(void) { hal_exit(comp_id); }

int rtapi_app_main(void) {
    int ans;
    comp_id = hal_init("rosekins");
    if(comp_id < 0) return comp_id;

    haldata = hal_malloc(sizeof(struct haldata));

    if((ans = hal_pin_float_new("rosekins.revolutions",
              HAL_OUT, &(haldata->revolutions), comp_id)) < 0) goto error;
    if((ans = hal_pin_float_new("rosekins.theta_degrees",
              HAL_OUT, &(haldata->theta_degrees), comp_id)) < 0) goto error;
    if((ans = hal_pin_float_new("rosekins.bigtheta_degrees",
              HAL_OUT, &(haldata->bigtheta_degrees), comp_id)) < 0) goto error;

    hal_ready(comp_id);
    return 0;

error:
    return ans;
}

/* ---- nonrt interface for userspace planner ---- */

#include "kinematics_params.h"

static rosekins_state_t nonrt_state = {0, 0};

int nonrt_kinematicsForward(const void *params,
                            const double *joints, EmcPose *pos)
{
    (void)params;
    return rosekins_forward_math(joints, pos);
}

int nonrt_kinematicsInverse(const void *params,
                            const EmcPose *pos, double *joints)
{
    (void)params;
    return rosekins_inverse_math(pos, joints, &nonrt_state, NULL);
}

int nonrt_refresh(void *params,
                  int (*read_float)(const char *, double *),
                  int (*read_bit)(const char *, int *),
                  int (*read_s32)(const char *, int *))
{
    (void)params;
    (void)read_float;
    (void)read_bit;
    (void)read_s32;
    return 0;
}

int nonrt_is_identity(void)
{
    return 0;
}

EXPORT_SYMBOL(nonrt_kinematicsForward);
EXPORT_SYMBOL(nonrt_kinematicsInverse);
EXPORT_SYMBOL(nonrt_refresh);
EXPORT_SYMBOL(nonrt_is_identity);
