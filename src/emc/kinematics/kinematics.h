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

#ifndef __LINUXCNC_KINEMATICS_H
#define __LINUXCNC_KINEMATICS_H

#include "emcpos.h" /* EmcPose */
#include "rtapi_bool.h"

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
    KINEMATICS_IDENTITY = 1,/* forward=inverse, both well-behaved */
    KINEMATICS_FORWARD_ONLY,/* forward but no inverse */
    KINEMATICS_INVERSE_ONLY,/* inverse but no forward */
    KINEMATICS_BOTH         /* forward and inverse both */
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

/* the inverse flags are passed to the inverse kinematics so that they
   can resolve ambiguities in the joint angles for a given world coordinate,
   e.g., for robots, this would be elbow-up, elbow-down, etc.

   The flags are also passed to the forward kinematics and are set by them,
   which is how they are changed from their initial value. For example, for
   robots you could do a joint move that brings the elbow from a down
   configuration to an up configuration. The inverse flags would be set to
   indicate this. */
typedef unsigned long int KINEMATICS_INVERSE_FLAGS;

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

/* These two give the orientation of the tool and of the workpiece for a set
   of joint values.  Each returns a rotation whose columns are that frame's
   axes expressed in MACHINE coordinates, the frame fixed to the bed that
   nothing rotates.  Note that this is not the frame kinematicsForward()
   reports positions in, which is attached to the workpiece; see the
   Kinematics Conventions chapter.

   They are reported separately, and not as the single work-to-tool rotation,
   because the product cannot be taken apart again.  A consumer that has to
   place both bodies, a simulation model or a preview, needs each one against
   the machine.  A consumer that wants the tool in workpiece coordinates,
   which is what a tilted work plane asks for, composes them itself:

       tool_in_work = transpose(work) * tool

   The third column of the tool frame is the tool axis: a direction, not to be
   confused with the tool length, which is the distance applied along it.  It
   runs from the tool tip towards the holder.  The origin of the tool frame is
   the controlled point that kinematicsForward() reports for the same joints.

   The frame is what the joints do.  The virtual rotation about the tool axis
   that a tilted work plane applies, the pre-rot pin on the in-tree
   components, is not part of it: it is a rotation of the coordinate system,
   applied by whoever programs in the frame, which is where Heidenhain's
   COORD ROT, Fanuc's feature coordinate system and Siemens' swivel frame keep
   it as well.  A consumer that wants tool x as programmed multiplies the
   frame by that rotation itself; it has the pin.

   A module whose own maths is in the other sense, which is every module built
   on the ISO 9787 flange frame or on Denavit-Hartenberg parameters, does not
   fix that up by hand: it declares the rotation relating its frame to the
   convention and the shared code applies it.  Reversing the tool axis is a
   rotation, not a sign.  Negating the third column alone gives determinant -1,
   a reflection, and which half turn is used decides where tool x ends up.

   A machine that turns only the tool returns the identity for the work frame,
   and one that turns only the work returns the identity for the tool frame.
   Machines that do both, which is every table-rotary head-rotary mill, return
   a non-trivial pair and are the reason for reporting them apart.

   Both are optional.  Modules built on switchkins.c export them always and
   return -1 for a switchkins type that has not supplied one; other modules
   need not export them at all, so a caller resolving them dynamically has to
   cope with their absence.

   Return 0 on success, -1 if the frame is not available. */
extern int kinematicsToolFrame(const double *joint,
                               PmRotationMatrix *rot,
                               const KINEMATICS_FORWARD_FLAGS *fflags);

extern int kinematicsWorkFrame(const double *joint,
                               PmRotationMatrix *rot,
                               const KINEMATICS_FORWARD_FLAGS *fflags);

/* parameters for use with switchkins.c */
typedef struct kinematics_parms {
  char* sparm;     // module string parameter passed to kins
  char* kinsname;  // must agree with module(file) name
  char* halprefix; // for hal pin hames
  char* required_coordinates;
  int   max_joints;
  int   allow_duplicates;
  int   fwd_iterates_mask; // identify kins types that use iterative
                           // forward kinematics (typ: genhex)
                           // bitmask: 0x0 none
                           // bitmask: 0x1 bit0: switchkins_type==0
                           // bitmask: 0x2 bit1: switchkins_type==1
                           // bitmask: 0x4 bit2: switchkins_type==2
  int   gui_kinstype; // may be reqd for parallel kins with vismach
                      // to select switchkins_type for gui pins
} kparms;

/* map letters in a coordinates string to joint numbers
** sequentially.  Axis indices are 0:x,1:y,...,etc
** Example: coordinates=XYZYAC
** Result:  axis_idx_for_jno[0] = 0 ==> X
**          axis_idx_for_jno[1] = 1 ==> Y
**          axis_idx_for_jno[2] = 2 ==> Z
**          axis_idx_for_jno[3] = 1 ==> Y (duplicate allowed)
**          axis_idx_for_jno[4] = 1 ==> A
**          axis_idx_for_jno[5] = 1 ==> C
*/
extern int map_coordinates_to_jnumbers(const char *coordinates,
                                       const int  max_joints,
                                       const int  allow_duplicates,
                                             int  axis_idx_for_jno[]);

extern int mapped_joints_to_position(const int max_joints,
                                     const double* joints,
                                     EmcPose*  pose);

extern int position_to_mapped_joints(const int max_joints,
                                     const EmcPose* pos,
                                     double* joints);

extern int identityKinematicsSetup(const int   comp_id,
                                   const char* coordinates,
                                   kparms*     ksetup_parms);

extern int identityKinematicsForward(const double *joint,
                                     struct EmcPose * world,
                                     const KINEMATICS_FORWARD_FLAGS * fflags,
                                     KINEMATICS_INVERSE_FLAGS * iflags);

extern int identityKinematicsInverse(const struct EmcPose * world,
                                     double *joint,
                                     const KINEMATICS_INVERSE_FLAGS * iflags,
                                     KINEMATICS_FORWARD_FLAGS * fflags);

/* joints are axes, so neither frame ever turns */
extern int identityKinematicsToolFrame(const double *joint,
                                       PmRotationMatrix *rot,
                                       const KINEMATICS_FORWARD_FLAGS *fflags);

extern int identityKinematicsWorkFrame(const double *joint,
                                       PmRotationMatrix *rot,
                                       const KINEMATICS_FORWARD_FLAGS *fflags);

/* Rotations relating a module's own frame to the tool frame convention.
   TOOL_FRAME_SPINDLE is the identity, for maths already in the convention.
   TOOL_FRAME_FLANGE is the half turn about tool x that turns an ISO 9787
   flange frame, whose z points out of the mechanical interface towards the
   work, into the convention. */
extern const PmRotationMatrix TOOL_FRAME_SPINDLE;
extern const PmRotationMatrix TOOL_FRAME_FLANGE;

/* Post-multiply a module's native frame by the rotation it declared, in
   place.  Modules built on switchkins.c never call this, the dispatch does it
   for them; a standalone module calls it before returning.
   Returns 0, or -1 if native is not a proper rotation. */
extern int toolFrameApplyNative(PmRotationMatrix *rot,
                                const PmRotationMatrix *native);

/* out = transpose(work) * tool, the tool frame in workpiece coordinates.
   out may alias neither input. */
extern int toolFrameInWork(const PmRotationMatrix *work,
                           const PmRotationMatrix *tool,
                           PmRotationMatrix *out);

/* True if m is orthonormal with determinant +1, so a frame a machine can
   actually hold.  Used to check a declared rotation once, at load. */
extern int toolFrameIsProper(const PmRotationMatrix *m);

/* The inverse of kinematicsToolFrame(): which joint values point the tool
   along a requested direction.  This is the question a tilted work plane asks
   when it has to orient the machine, and the one vector format G-code asks
   for every block.

   axis_in_work is the wanted tool axis and x_in_work the wanted tool x, both
   in workpiece coordinates, both in the sense of transpose(work) * tool.
   x_in_work may be NULL, which leaves the spin about the tool free.  Where it
   is given, the two have to be at right angles, being two axes of one frame.

   Asking for tool x does not require a joint that can reach it.  A five axis
   machine spends both rotaries on the tool axis, and the turn about that axis
   is not a joint at all: it is the virtual rotation, the pre-rot pin on the
   in-tree components.  So where the joints can place tool x, on a machine with
   a third orientation joint, they do and tool_spin comes back zero; where they
   cannot, the joints reach the axis and tool_spin carries the turn about it
   that finishes the job, in radians, in the sense of the virtual rotation.
   Either way the caller writes one path, and which kind of machine it has is a
   number that happens to be zero rather than a branch.  tool_spin may be NULL,
   but then a request for tool x that the joints cannot reach has nowhere to
   put its answer and reports no solutions.

   seed is a full set of joint values, normally where the machine is now.  The
   joints that do not affect the tool orientation are copied from it, and it
   breaks the tie where a machine has more orientation joints than the request
   constrains.

   held is a bit per joint, bit n for joint n, naming the joints the caller
   does not want moved; they keep their seed value and the request is solved
   with the rest.  Zero lets every joint that turns the tool take part.  This
   is the caller's policy and not the module's: a table rotary turns the tool
   against the work as surely as a head rotary does, so with nothing held a
   machine with a table and a two axis head has a spare orientation joint, and
   a bare tool axis leaves a family.  A tilted work plane that keeps the table
   where it is, as the TWP remap does and as Heidenhain's M138 says, holds it
   and gets the two head solutions and the spin about the tool that finishes
   the frame.

   The request is normalised on the way in: axis_in_work is scaled to unit
   length and x_in_work has its component along the axis removed, so the
   rounded numbers a program carries do not make an orientation unreachable.
   A zero vector, or a tool x within a millionth of a radian of lying along
   the axis, is still refused, since neither describes a frame.

   solutions receives max_solutions complete sets of joint values, one after
   another, each num_joints long.  free_directions, if not NULL, receives one
   entry per solution: 0 where the joints are pinned down, and n where the
   solution is one point of an n dimensional family, which happens at a
   singular pose and on a machine with a spare orientation joint.  In that case
   one representative is reported, the one nearest the seed, because the answer
   is a continuum and a list of samples from it would be arbitrary.

   Joint limits are not applied and no solution is preferred over another: the
   module answers what the geometry permits, and the caller picks by whatever
   rule it works to, shortest move or positive rotation only or whatever else.

   Returns the number of solutions, 0 if the orientation cannot be reached, or
   -1 if the module cannot answer.

   This is not a realtime routine.  It searches, and how long it takes depends
   on the machine and the request. */
#define TOOL_FRAME_MAX_SOLUTIONS 8
#define TOOL_FRAME_MAX_FREE      4

extern int kinematicsToolFrameInverse(const PmCartesian *axis_in_work,
                                      const PmCartesian *x_in_work,
                                      const double *seed,
                                      unsigned int held,
                                      double *solutions,
                                      int max_solutions,
                                      int *free_directions,
                                      double *tool_spin);

/* The generic implementation of the above, driven by a module's own frame
   functions, so that a module gets it for free once it supplies them.  A
   module with a closed form registers that instead: it is faster, and it
   knows its own degenerate poses without having to find them.

   num_joints is the length of seed and of each row of solutions. */
typedef int (*kinsFrameFunc)(const double *joint,
                             PmRotationMatrix *rot,
                             const KINEMATICS_FORWARD_FLAGS *fflags);

extern int toolFrameSolve(kinsFrameFunc work,
                          kinsFrameFunc tool,
                          int num_joints,
                          const PmCartesian *axis_in_work,
                          const PmCartesian *x_in_work,
                          const double *seed,
                          unsigned int held,
                          double *solutions,
                          int max_solutions,
                          int *free_directions,
                          double *tool_spin);

extern int kinematicsSwitchable(void);
extern int kinematicsSwitch(int switchkins_type);
//NOTE: switchable kinematics may require Interp::Synch
//      before/after invoking kinematicsSwitch()
//      A convenient command to synch is: M66 E0 L0

#define KINS_NOT_SWITCHABLE \
extern int kinematicsSwitchable() {return 0;} \
extern int kinematicsSwitch(int switchkins_type) { (void)switchkins_type; return 0;} \
EXPORT_SYMBOL(kinematicsSwitchable); \
EXPORT_SYMBOL(kinematicsSwitch);


// support for template for user-defined switchkins_type==2
extern int userkKinematicsSetup(const int   comp_id,
                                const char* coordinates,
                                kparms*     ksetup_parms);

extern int userkKinematicsForward(const double *joint,
                                  struct EmcPose * world,
                                  const KINEMATICS_FORWARD_FLAGS * fflags,
                                  KINEMATICS_INVERSE_FLAGS * iflags);

extern int userkKinematicsInverse(const struct EmcPose * world,
                                  double *joint,
                                  const KINEMATICS_INVERSE_FLAGS * iflags,
                                  KINEMATICS_FORWARD_FLAGS * fflags);
#endif
//*********************************************************************
// xyzac,xyzbc;
extern int trtKinematicsSetup(const int   comp_id,
                              const char* coordinates,
                              kparms*     ksetup_parms);

extern int xyzacKinematicsForward(const double *joints,
                                  EmcPose * pos,
                                  const KINEMATICS_FORWARD_FLAGS * fflags,
                                  KINEMATICS_INVERSE_FLAGS * iflags);

extern int xyzacKinematicsInverse(const EmcPose * pos,
                                  double *joints,
                                  const KINEMATICS_INVERSE_FLAGS * iflags,
                                  KINEMATICS_FORWARD_FLAGS * fflags);

extern int xyzacKinematicsToolFrame(const double *joints,
                                   PmRotationMatrix *rot,
                                   const KINEMATICS_FORWARD_FLAGS *fflags);

extern int xyzacKinematicsWorkFrame(const double *joints,
                                   PmRotationMatrix *rot,
                                   const KINEMATICS_FORWARD_FLAGS *fflags);


extern int xyzbcKinematicsForward(const double *joints,
                                  EmcPose * pos,
                                  const KINEMATICS_FORWARD_FLAGS * fflags,
                                  KINEMATICS_INVERSE_FLAGS * iflags);

extern int xyzbcKinematicsInverse(const EmcPose * pos,
                                  double *joints,
                                  const KINEMATICS_INVERSE_FLAGS * iflags,
                                  KINEMATICS_FORWARD_FLAGS * fflags);

extern int xyzbcKinematicsToolFrame(const double *joints,
                                   PmRotationMatrix *rot,
                                   const KINEMATICS_FORWARD_FLAGS *fflags);

extern int xyzbcKinematicsWorkFrame(const double *joints,
                                   PmRotationMatrix *rot,
                                   const KINEMATICS_FORWARD_FLAGS *fflags);

//*********************************************************************
