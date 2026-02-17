/********************************************************************
* Description: tripodkins.c
*   Kinematics for 3 axis Tripod machine
*
*   Derived from a work by Fred Proctor
*
* Author: 
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/

/*
  These kinematics are for a tripod with point vertices.

  Vertices A, B, and C are the base, and vertex D is the controlled point.
  Three tripod strut lengths AD, BD, and CD are the joints that move
  point D around.

  Point A is the origin, with coordinates (0, 0, 0). Point B lies on the
  x axis, with coordinates (Bx, 0, 0). Point C lies in the xy plane, with
  coordinates (Cx, Cy, 0). Point D has coordinates (Dx, Dy, Dz).

  The controlled Cartesian values are Dx, Dy, and Dz. A frame attached to
  D, say with x parallel to AD and y in the plane ABD, would change its
  orientation as the strut lengths changed. The orientation of this frame
  relative to the world frame is not computed.

  With respect to the kinematics functions,

  pos->tran.x = Dx
  pos->tran.y = Dy
  pos->tran.z = Dz
  pos->a,b,c  = 0

  joints[0] = AD
  joints[1] = BD
  joints[2] = CD

  The inverse kinematics have no singularities. Any values for Dx, Dy, and
  Dz will yield numerical results. Of course, these may be beyond the
  strut length limits, but there are no singular effects like infinite speed.

  The forward kinematics has a singularity due to the triangle inequalities
  for triangles ABD, BCD, and CAD. When any of these approach the limit,
  Dz is zero and D lies in the base plane.

  The forward kinematics flags, referred to in kinematicsForward and
  set in kinematicsInverse, let the forward kinematics select between
  the positive and negative values of Dz for given strut values.
  Dz > 0 is "above", Dz < 0 is "below". Dz = 0 is the singularity.

  fflags == 0 selects Dz > 0,
  fflags != 0 selects Dz < 0.

  The inverse kinematics flags let the inverse kinematics select between
  multiple valid solutions of strut lengths for given Cartesian values
  for D. There are no multiple solutions: D constrains the strut lengths
  completely. So, the inverse flags are ignored.
 */

#include "kinematics.h"             /* these decls */
#include "rtapi_math.h"

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

/* Parameters struct for tripod kinematics */
typedef struct {
    double bx;  /* X coordinate of point B */
    double cx;  /* X coordinate of point C */
    double cy;  /* Y coordinate of point C */
} tripod_params_t;

#ifndef tripod_sq
#define tripod_sq(x) ((x)*(x))
#endif

/*
 * Pure forward kinematics - strut lengths to Cartesian position
 *
 * Takes three strut lengths and computes Dx, Dy, and Dz.
 * The forward flag resolves D above/below the xy plane.
 *
 * Returns 0 on success, -1 on singularity
 */
static int tripod_forward_math(const tripod_params_t *params,
                               const double *joints,
                               EmcPose *pos,
                               const KINEMATICS_FORWARD_FLAGS *fflags,
                               KINEMATICS_INVERSE_FLAGS *iflags)
{
    (void)iflags;
    double P, Q, R;
    double s, t, u;
    double AD = joints[0];
    double BD = joints[1];
    double CD = joints[2];
    double Bx_v = params->bx;
    double Cx_v = params->cx;
    double Cy_v = params->cy;

    P = tripod_sq(AD);
    Q = tripod_sq(BD) - tripod_sq(Bx_v);
    R = tripod_sq(CD) - tripod_sq(Cx_v) - tripod_sq(Cy_v);
    s = -2.0 * Bx_v;
    t = -2.0 * Cx_v;
    u = -2.0 * Cy_v;

    if (s == 0.0) {
        /* points A and B coincident */
        return -1;
    }
    pos->tran.x = (Q - P) / s;

    if (u == 0.0) {
        /* points A B C are colinear */
        return -1;
    }
    pos->tran.y = (R - Q - (t - s) * pos->tran.x) / u;
    pos->tran.z = P - tripod_sq(pos->tran.x) - tripod_sq(pos->tran.y);
    if (pos->tran.z < 0.0) {
        /* triangle inequality violated */
        return -1;
    }
    pos->tran.z = sqrt(pos->tran.z);
    if (fflags && *fflags) {
        pos->tran.z = -pos->tran.z;
    }

    pos->a = 0.0;
    pos->b = 0.0;
    pos->c = 0.0;
    pos->u = 0.0;
    pos->v = 0.0;
    pos->w = 0.0;

    return 0;
}

/*
 * Pure inverse kinematics - Cartesian position to strut lengths
 *
 * Returns 0 on success
 */
static int tripod_inverse_math(const tripod_params_t *params,
                               const EmcPose *pos,
                               double *joints,
                               const KINEMATICS_INVERSE_FLAGS *iflags,
                               KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)iflags;
    double Dx = pos->tran.x;
    double Dy = pos->tran.y;
    double Dz = pos->tran.z;
    double Bx_v = params->bx;
    double Cx_v = params->cx;
    double Cy_v = params->cy;

    joints[0] = sqrt(tripod_sq(Dx) + tripod_sq(Dy) + tripod_sq(Dz));
    joints[1] = sqrt(tripod_sq(Dx - Bx_v) + tripod_sq(Dy) + tripod_sq(Dz));
    joints[2] = sqrt(tripod_sq(Dx - Cx_v) + tripod_sq(Dy - Cy_v) + tripod_sq(Dz));

    if (fflags) {
        *fflags = 0;
        if (Dz < 0.0) {
            *fflags = 1;
        }
    }

    return 0;
}

#include "hal.h"
struct haldata {
    hal_float_t *bx, *cx, *cy;
} *haldata = 0;

#define Bx (*(haldata->bx))
#define Cx (*(haldata->cx))
#define Cy (*(haldata->cy))

/*
  forward kinematics takes three strut lengths and computes Dx, Dy, and Dz
  pos->tran.x,y,z, respectively. The forward flag is used to resolve
  D above/below the xy plane. The inverse flags are not set since there
  are no ambiguities going from world to joint coordinates.

  The forward kins are derived as follows:

  1. Let x, y, z be Dx, Dy, Dz to save pixels. Cartesian displacement from
  D to A, B, and C gives

  AD^2 = x^2 + y^2 + z^2
  BD^2 = (x - Bx)^2 + y^2 + z^2
  CD^2 = (x - Cx)^2 + (y - Cy)^2 + z^2

  This yields

  I.   P = x^2 + y^2 + z^2
  II.  Q = x^2 + y^2 + z^2 + sx
  III. R = x^2 + y^2 + z^2 + tx + uy

  Where

  P = AD^2,
  Q = BD^2 - Bx^2
  R = CD^2 - Cx^2 - Cy^2
  s = -2Bx
  t = -2Cx
  u = -2Cy

  II - I gives Q - P = sx, so x = (Q - P)/s, s != 0. The constraint on s
  means that Bx != 0, or points A and B can't be the same.

  III - II gives R - Q = (t - s)x + uy, so y = (R - Q - (t - s)x)/u, u != 0.
  The constraint on u means that Cy != 0, or points A B C can't be collinear.

  Substituting x, y into I gives z = sqrt(P - x^2 - y^2), which has two
  solutions. Positive means the tripod is above the xy plane, negative
  means below.
*/
int kinematicsForward(const double * joints,
                      EmcPose * pos,
                      const KINEMATICS_FORWARD_FLAGS * fflags,
                      KINEMATICS_INVERSE_FLAGS * iflags)
{
    tripod_params_t params;
    params.bx = Bx;
    params.cx = Cx;
    params.cy = Cy;
    return tripod_forward_math(&params, joints, pos, fflags, iflags);
}

int kinematicsInverse(const EmcPose * pos,
                      double * joints,
                      const KINEMATICS_INVERSE_FLAGS * iflags,
                      KINEMATICS_FORWARD_FLAGS * fflags)
{
    tripod_params_t params;
    params.bx = Bx;
    params.cx = Cx;
    params.cy = Cy;
    return tripod_inverse_math(&params, pos, joints, iflags, fflags);
}

KINEMATICS_TYPE kinematicsType()
{
  return KINEMATICS_BOTH;
}

#ifdef MAIN

#include <stdio.h>
#include <string.h>

/*
  Interactive testing of kins.

  Syntax: a.out <Bx> <Cx> <Cy>
*/
int main(int argc, char *argv[])
{
#ifndef BUFFERLEN
#define BUFFERLEN 256
#endif
  char buffer[BUFFERLEN];
  char cmd[BUFFERLEN];
  EmcPose pos, vel;
  double joints[3]={0.0,0.0,0.0}, jointvels[3]={0.0,0.0,0.0};
  char inverse;
  char flags;
  KINEMATICS_FORWARD_FLAGS fflags;

  inverse = 0;			/* forwards, by default */
  flags = 0;			/* didn't provide flags */
  fflags = 0;			/* above xy plane, by default */
  if (argc != 4 ||
      1 != sscanf(argv[1], "%lf", &Bx) ||
      1 != sscanf(argv[2], "%lf", &Cx) ||
      1 != sscanf(argv[3], "%lf", &Cy)) {
    fprintf(stderr, "syntax: %s Bx Cx Cy\n", argv[0]);
    return 1;
  }

  while (! feof(stdin)) {
    if (inverse) {
	printf("inv> ");
    }
    else {
	printf("fwd> ");
    }
    fflush(stdout);

    if (NULL == fgets(buffer, BUFFERLEN, stdin)) {
      break;
    }
    if (1 != sscanf(buffer, "%255s", cmd)) {
      continue;
    }

    if (! strcmp(cmd, "quit")) {
      break;
    }
    if (! strcmp(cmd, "i")) {
      inverse = 1;
      continue;
    }
    if (! strcmp(cmd, "f")) {
      inverse = 0;
      continue;
    }
    if (! strcmp(cmd, "ff")) {
      if (1 != sscanf(buffer, "%*s %lu", &fflags)) {
	printf("need forward flag\n");
      }
      continue;
    }

    if (inverse) {		/* inverse kins */
      if (3 != sscanf(buffer, "%lf %lf %lf", 
		      &pos.tran.x,
		      &pos.tran.y,
		      &pos.tran.z)) {
	printf("need X Y Z\n");
	continue;
      }
      if (0 != kinematicsInverse(&pos, joints, NULL, &fflags)) {
	printf("inverse kin error\n");
      }
      else {
	printf("%f\t%f\t%f\n", joints[0], joints[1], joints[2]);
	if (0 != kinematicsForward(joints, &pos, &fflags, NULL)) {
	  printf("forward kin error\n");
	}
	else {
	  printf("%f\t%f\t%f\n", pos.tran.x, pos.tran.y, pos.tran.z);
	}
      }
    }
    else {			/* forward kins */
      if (flags) {
	if (4 != sscanf(buffer, "%lf %lf %lf %lu",
			&joints[0],
			&joints[1],
			&joints[2],
			&fflags)) {
	  printf("need 3 strut values and flag\n");
	  continue;
	}
      }
      else {
	if (3 != sscanf(buffer, "%lf %lf %lf", 
			&joints[0],
			&joints[1],
			&joints[2])) {
	  printf("need 3 strut values\n");
	  continue;
	}
      }
      if (0 != kinematicsForward(joints, &pos, &fflags, NULL)) {
	printf("forward kin error\n");
      }
      else {
	printf("%f\t%f\t%f\n", pos.tran.x, pos.tran.y, pos.tran.z);
	if (0 != kinematicsInverse(&pos, joints, NULL, &fflags)) {
	  printf("inverse kin error\n");
	}
	else {
	  printf("%f\t%f\t%f\n", joints[0], joints[1], joints[2]);
	}
      }
    }
  } /* end while (! feof(stdin)) */

  return 0;
}

#endif /* MAIN */

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"

const char* kinematicsGetName(void) { return "tripodkins"; }

KINS_NOT_SWITCHABLE
EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsForward);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsGetName);

MODULE_LICENSE("GPL");



int comp_id;
int rtapi_app_main(void) {
    int res = 0;

    comp_id = hal_init("tripodkins");
    if(comp_id < 0) return comp_id;

    haldata = hal_malloc(sizeof(struct haldata));
    if(!haldata) goto error;

    if((res = hal_pin_float_new("tripodkins.Bx", HAL_IO, &(haldata->bx), comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("tripodkins.Cx", HAL_IO, &(haldata->cx), comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("tripodkins.Cy", HAL_IO, &(haldata->cy), comp_id)) < 0) goto error;

    Bx = Cx = Cy = 1.0;
    hal_ready(comp_id);
    return 0;

error:
    hal_exit(comp_id);
    return res;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }

/* ========================================================================
 * Non-RT interface for userspace trajectory planner
 * ======================================================================== */
#include "kinematics_params.h"

int nonrt_kinematicsForward(const void *params,
                            const double *joints,
                            EmcPose *pos)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    tripod_params_t p;
    p.bx = kp->params.tripod.bx;
    p.cx = kp->params.tripod.cx;
    p.cy = kp->params.tripod.cy;
    return tripod_forward_math(&p, joints, pos, NULL, NULL);
}

int nonrt_kinematicsInverse(const void *params,
                            const EmcPose *pos,
                            double *joints)
{
    const kinematics_params_t *kp = (const kinematics_params_t *)params;
    tripod_params_t p;
    p.bx = kp->params.tripod.bx;
    p.cx = kp->params.tripod.cx;
    p.cy = kp->params.tripod.cy;
    return tripod_inverse_math(&p, pos, joints, NULL, NULL);
}

int nonrt_refresh(void *params,
                  int (*read_float)(const char *, double *),
                  int (*read_bit)(const char *, int *),
                  int (*read_s32)(const char *, int *))
{
    kinematics_params_t *kp = (kinematics_params_t *)params;
    (void)read_bit; (void)read_s32;

    if (read_float("tripodkins.Bx", &kp->params.tripod.bx) != 0)
        return -1;
    if (read_float("tripodkins.Cx", &kp->params.tripod.cx) != 0)
        return -1;
    if (read_float("tripodkins.Cy", &kp->params.tripod.cy) != 0)
        return -1;

    return 0;
}

int nonrt_is_identity(void) { return 0; }

EXPORT_SYMBOL(nonrt_kinematicsForward);
EXPORT_SYMBOL(nonrt_kinematicsInverse);
EXPORT_SYMBOL(nonrt_refresh);
EXPORT_SYMBOL(nonrt_is_identity);
