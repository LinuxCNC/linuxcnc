/********************************************************************
* Description: kinematics.h
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/

#ifndef KINEMATICS_H
#define KINEMATICS_H

#include "emcpos.h"		/* EmcPose */
#include "vtable.h"		// vtable signatures

/*
  The type of kinematics used.
  
  KINEMATICS_IDENTITY means that the joints and world coordinates are the
  same, as for slideway machines (XYZ milling machines). The EMC will allow
  changing from joint to world mode and vice versa. Also, the EMC will set
  the actual world position to be the actual joint positions (not commanded)
  by calling the forward kinematics each trajectory cycle.

  KINEMATICS_FORWARD_ONLY means that only the forward kinematics exist.
  Since the EMC requires at least the inverse kinematics, this should simply
  terminate the EMC.

  KINEMATICS_INVERSE_ONLY means that only the inverse kinematics exist.
  The forwards won't be called, and the EMC will only allow changing from
  joint to world mode at the home position.

  KINEMATICS_BOTH means that both the forward and inverse kins are defined.
  Like KINEMATICS_IDENTITY, the EMC will allow changing between world and
  joint modes. However, the kins are assumed to be somewhat expensive
  computationally, and the forwards won't be called at the trajectory rate
  to compute actual world coordinates from actual joint values.
*/

typedef enum {
    KINEMATICS_IDENTITY = 1,	/* forward=inverse, both well-behaved */
    KINEMATICS_FORWARD_ONLY,	/* forward but no inverse */
    KINEMATICS_INVERSE_ONLY,	/* inverse but no forward */
    KINEMATICS_BOTH		/* forward and inverse both */
} KINEMATICS_TYPE;

/* the forward flags are passed to the forward kinematics so that they
   can resolve ambiguities in the world coordinates for a given joint set,
   e.g., for hexpods, this would be platform-below-base, platform-above-base.

   The flags are also passed to the inverse kinematics and are set by them,
   which is how they are changed from their initial value. For example, for
   hexapods you could do a coordinated move that brings the platform up from
   below the base to above the base. The forward flags would be set to
   indicate this. */
typedef unsigned long int KINEMATICS_FORWARD_FLAGS;

/* the inverse flags are passed to the inverse kinematics so thay they
   can resolve ambiguities in the joint angles for a given world coordinate,
   e.g., for robots, this would be elbow-up, elbow-down, etc.

   The flags are also passed to the forward kinematics and are set by them,
   which is how they are changed from their initial value. For example, for
   robots you could do a joint move that brings the elbow from a down
   configuration to an up configuration. The inverse flags would be set to
   indicate this. */
typedef unsigned long int KINEMATICS_INVERSE_FLAGS;


// these methods used to be exported through symbol resolution, and now
// go through function pointers in the kins vtable:
#ifdef LEGACY_KINS_API

/* the forward kinematics take joint values and determine world coordinates,
   given forward kinematics flags to resolve any ambiguities. The inverse
   flags are set to indicate their value appropriate to the joint values
   passed in. */
extern int kinematicsForward(const double *joint,
			     struct EmcPose * world,
			     const KINEMATICS_FORWARD_FLAGS * fflags,
			     KINEMATICS_INVERSE_FLAGS * iflags);

/* the inverse kinematics take world coordinates and determine joint values,
   given the inverse kinematics flags to resolve any ambiguities. The forward
   flags are set to indicate their value appropriate to the world coordinates
   passed in. */
extern int kinematicsInverse(const struct EmcPose * world,
			     double *joint,
			     const KINEMATICS_INVERSE_FLAGS * iflags,
			     KINEMATICS_FORWARD_FLAGS * fflags);

/* the home kinematics function sets all its arguments to their proper
   values at the known home position. When called, these should be set,
   when known, to initial values, e.g., from an INI file. If the home
   kinematics can accept arbitrary starting points, these initial values
   should be used.
*/
extern int kinematicsHome(struct EmcPose * world,
			  double *joint,
			  KINEMATICS_FORWARD_FLAGS * fflags,
			  KINEMATICS_INVERSE_FLAGS * iflags);

extern KINEMATICS_TYPE kinematicsType(void);

#endif

typedef int (*vtk_kinematicsForward_t)(const double *joint,
				     struct EmcPose * world,
				     const KINEMATICS_FORWARD_FLAGS * fflags,
				     KINEMATICS_INVERSE_FLAGS * iflags);
typedef int (*vtk_kinematicsInverse_t)(const struct EmcPose * world,
				     double *joint,
				     const KINEMATICS_INVERSE_FLAGS * iflags,
				     KINEMATICS_FORWARD_FLAGS * fflags);

typedef int (* vtk_kinematicsHome_t)(struct EmcPose * world,
				   double *joint,
				   KINEMATICS_FORWARD_FLAGS * fflags,
				   KINEMATICS_INVERSE_FLAGS * iflags);


typedef KINEMATICS_TYPE  (*vtk_kinematicsType_t)(void);

typedef struct {
    vtk_kinematicsForward_t kinematicsForward;
    vtk_kinematicsInverse_t kinematicsInverse;
    //    vtk_kinematicsHome_t    kinematicsHome; // unused
    vtk_kinematicsType_t    kinematicsType;
} vtkins_t;

#endif
