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
#include "emcmotcfg.h" /* EMCMOT_MAX_JOINTS, EMCMOT_MAX_AXIS */
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
  const struct kins_param_desc_tag *params; // geometry table, see below
  int   nparams;
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

/* How each joint responds to a unit rate of each pose coordinate:

       jac[j][a] = d joint[j] / d pose[a]

   Rows are joints, columns are pose coordinates in EmcPose order, x y z a b
   c u v w.  This is the derivative of kinematicsInverse(): multiply it by a
   pose velocity and the result is the joint velocity that motion will
   command, which is what a feed limit checks against the joint limits.  A
   row that grows without bound is a pose approaching a singularity, where
   the joints cannot keep up with any world speed at all.

   Each entry is in joint units per pose unit, whatever units the module's
   own forward and inverse already use.  Nothing is converted here: a caller
   that feeds pose rates in EmcPose units gets joint rates in the units
   motion already commands, and never has to know which unit a rotary joint
   is in.  On every module in the tree both are degrees, so a table rotary's
   own row is a plain 1 in its own column.

   The columns are pose coordinates, so the answer lives in the work frame,
   where kinematicsForward() reports positions.  The a, b and c columns are
   rates of the pose words, the wrapped linear axes the planner already
   treats as coordinates, and not an angular velocity vector.  That makes
   this a different object from the frames above, which are orientations
   and are given against the machine; see the Kinematics Conventions
   chapter.

   joint and world are one pose in both descriptions: world is what
   kinematicsForward() reports for joint under these flags.  Both are given
   because a closed form differentiates at the joints while the generic
   default perturbs the pose, and iflags keeps every inverse the default
   calls on the same solution branch.  Rows past the module's joint count
   are zero.

   Optional, like the frames.  Modules built on switchkins.c export it
   always and answer for every type, since it can always be obtained from
   the inverse where a frame cannot; other modules need not export it, and
   a caller that resolves it dynamically and finds nothing can call
   kinsJacobianFromInverse() itself with the module's inverse.

   Returns 0, or -1 if the module cannot answer at this pose. */
extern int kinematicsJacobian(const double *joint,
                              const EmcPose *world,
                              double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                              const KINEMATICS_INVERSE_FLAGS *iflags);

typedef int (*kinsInverseFunc)(const EmcPose *world,
                               double *joint,
                               const KINEMATICS_INVERSE_FLAGS *iflags,
                               KINEMATICS_FORWARD_FLAGS *fflags);

/* The generic Jacobian, by central differences of an inverse about world:
   two inverse calls per pose coordinate, eighteen in all, on the solution
   branch iflags selects.  The joint array handed to every call starts from
   joint, so a module that reads its joint argument sees the machine where
   it is.

   The answer is as good as the inverse: a closed form gives it to rounding,
   an inverse that iterates to a tolerance gives it to that tolerance over
   the step, and should supply its own.  num_joints is the module's joint
   count.  Returns 0, or -1 if any inverse fails. */
#define KINS_JACOBIAN_STEP      1e-3    /* pose units, either kind */

extern int kinsJacobianFromInverse(kinsInverseFunc inverse,
                                   int num_joints,
                                   const double *joint,
                                   const EmcPose *world,
                                   const KINEMATICS_INVERSE_FLAGS *iflags,
                                   double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS]);

/* For a module whose inverse computes a position P and then hands it to
   position_to_mapped_joints(): given dP[axis][pose], how each coordinate of
   P responds to each pose coordinate, fill in jac so that every joint gets
   the row of the letter it is mapped to.  Duplicate letters get duplicate
   rows, which is the gantry case. */
extern int kinsJacobianFromMappedAxes(int max_joints,
                                      const double dP[EMCMOT_MAX_AXIS][EMCMOT_MAX_AXIS],
                                      double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS]);

/* joints are axes: a 1 per joint in the column of its letter */
extern int identityKinematicsJacobian(const double *joint,
                                      const EmcPose *world,
                                      double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                                      const KINEMATICS_INVERSE_FLAGS *iflags);

/* ------------------------------------------------------------------------
   Kinematics as pure functions of what the caller passes in.

   Everything above reads its geometry from HAL pins the module created and
   keeps its mode and scratch in statics, so it can only answer for the
   machine as it is now, from inside the module.  The forms below take the
   same questions with the machine described by the caller: a parameter
   block naming the kinematics type, the joint map, the tool and the
   geometry, and a scratch block for what an iterative method carries
   between calls.  Nothing is read from HAL and nothing is kept, so one copy
   of the maths serves motion, a planner evaluating poses the machine has
   not reached, task checking a program at load, and a tool asking what if.

   A module declares its geometry as a table of named entries.  In RT the
   shared code makes one HAL pin per entry, with the names configs already
   use, and copies the pins into the block before every call; outside RT the
   caller fills the block from wherever it likes.  The maths reads
   p->geometry[i] where it read a pin.

   The existing entry points stay and are supplied once, by kins_single.c
   for a module with one kinematics type and by switchkins.c for one with
   several, so nothing that calls kinematicsForward() changes.  A module
   that does not provide these forms keeps working as it did; it just cannot
   be evaluated outside RT.
   ------------------------------------------------------------------------ */

#define KINS_MAX_PARAMS 96      /* genhexkins declares 84 */
#define KINS_MAX_TYPES   9      /* kinematics types a module may provide */

typedef enum {
    KINS_PARAM_FLOAT = 0,
    KINS_PARAM_BIT,
    KINS_PARAM_S32,
    KINS_PARAM_U32
} kins_param_type;

typedef enum {
    KINS_IN = 0,    /* read into the block before a call */
    KINS_OUT,       /* a result, written from kins_scratch.out[] after it */
    KINS_IO         /* read like an input; the pin is HAL_IO so it can be poked */
} kins_param_dir;

/* One entry of a module's geometry table.  name follows the module's HAL
   prefix.  An entry with tool set is the tool length along the tool axis:
   the shared code puts its value in kins_params.tool.tran.z as well, which
   is what the maths should read, so that a caller outside RT can supply
   the tool from the tool table without there being a pin. */
typedef struct kins_param_desc_tag {
    const char      *name;
    kins_param_type  type;
    kins_param_dir   dir;
    int              tool;
    double           dflt;
} kins_param_desc;

/* The machine, as far as the kinematics is concerned.  One copy may be
   shared by any number of callers: nothing writes it during a call. */
typedef struct kins_params {
    int      size;                            /* sizeof(kins_params) */
    int      ktype;                           /* kinematics type, 0 if one */
    int      max_joints;                      /* joints the map covers */
    int      joint_of_axis[EMCMOT_MAX_AXIS];  /* principal joint per letter */
    int      joints_of_axis[EMCMOT_MAX_AXIS]; /* bit per joint, duplicates */
    EmcPose  tool;                            /* tool offset, tool.tran.z along the tool axis */
    double   geometry[KINS_MAX_PARAMS];       /* the table, in its order */
} kins_params;

/* What one caller carries between its own calls: the last pose an
   iterative forward found, which seeds the next, and what a module reports
   about its last call.  Never shared between callers. */
typedef struct kins_scratch {
    EmcPose  pose_seed;                       /* start an iterative forward here */
    int      have_pose_seed;
    double   joint_seed[EMCMOT_MAX_JOINTS];   /* start an iterative inverse here */
    int      have_joint_seed;
    int      iterations;
    int      failed;
    double   aux[8];                          /* whatever else a module carries between calls */
    double   out[KINS_MAX_PARAMS];            /* the table's KINS_OUT entries */
} kins_scratch;

typedef int (*kins_forward_fn)(const kins_params *p, kins_scratch *s,
                               const double *joint, EmcPose *pos,
                               const KINEMATICS_FORWARD_FLAGS *fflags,
                               KINEMATICS_INVERSE_FLAGS *iflags);

typedef int (*kins_inverse_fn)(const kins_params *p, kins_scratch *s,
                               const EmcPose *pos, double *joint,
                               const KINEMATICS_INVERSE_FLAGS *iflags,
                               KINEMATICS_FORWARD_FLAGS *fflags);

typedef int (*kins_frame_fn)(const kins_params *p, const double *joint,
                             PmRotationMatrix *rot,
                             const KINEMATICS_FORWARD_FLAGS *fflags);

typedef int (*kins_jacobian_fn)(const kins_params *p, const double *joint,
                                const EmcPose *pos,
                                double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                                const KINEMATICS_INVERSE_FLAGS *iflags);

/* The maths of one kinematics type.  forward and inverse are required; the
   frames, the native rotation and the Jacobian are optional as before, and
   a missing Jacobian is differenced from the inverse.  fwd_iterates says the
   forward starts from the pose it is handed, so the shared code seeds it
   with the last answer after a switch.  identity says joints are axes, which
   a consumer may use to skip the maths altogether. */
typedef struct kins_ops {
    kins_forward_fn         forward;
    kins_inverse_fn         inverse;
    kins_frame_fn           work;
    kins_frame_fn           tool;
    const PmRotationMatrix *native;     /* NULL means TOOL_FRAME_SPINDLE */
    kins_jacobian_fn        jacobian;
    int                     fwd_iterates;
    int                     identity;           /* joints are axes */
} kins_ops;

/* A module described for a caller outside RT: its table, its joint
   conventions and the maths of each type.  ops[t] is NULL for a type the
   module still implements the old way. */
typedef struct kins_module_info {
    const char            *name;
    const char            *halprefix;
    const kins_param_desc *params;
    int                    nparams;
    const char            *required_coordinates;
    int                    max_joints;          /* the most the module allows */
    int                    allow_duplicates;
    int                    ntypes;
    const kins_ops        *ops[KINS_MAX_TYPES];
} kins_module_info;

/* Exported by every module that provides the forms above.  coordinates and
   sparm are the module parameters the RT instance was loaded with; a module
   whose types depend on them replays that choice here.  Meant for a copy of
   the module loaded outside RT; the RT instance answers from its own state
   without redoing its setup.  Returns 0, or -1 with info untouched. */
extern int kinsDescribe(const char *coordinates, const char *sparm,
                        kins_module_info *info);

/* Fill a block for a module: size, the joint map from coordinates (checked
   against required_coordinates, the joint limit and the duplicate rule),
   ktype 0, no tool, and every geometry entry at its table default.  A
   caller then overwrites what it knows better.  Returns 0 or -1. */
extern int kinsParamsInit(kins_params *p,
                          const kins_module_info *info,
                          const char *coordinates);

/* The joint map alone, into a block, with no other field touched. */
extern int kinsParamsMapCoordinates(kins_params *p,
                                    const char *coordinates,
                                    int max_joints,
                                    int allow_duplicates,
                                    const char *required_coordinates);

/* Reset a scratch to "no seed, nothing reported". */
extern void kinsScratchInit(kins_scratch *s);

/* The map helpers above, reading the map from the block instead of from
   the statics that map_coordinates_to_jnumbers() fills. */
extern int kinsMappedJointsToPose(const kins_params *p,
                                  const double *joints, EmcPose *pos);
extern int kinsPoseToMappedJoints(const kins_params *p,
                                  const EmcPose *pos, double *joints);
extern int kinsJacobianFromMappedAxesP(const kins_params *p,
                                       const double dP[EMCMOT_MAX_AXIS][EMCMOT_MAX_AXIS],
                                       double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS]);

/* Identity as pure functions: joints are axes through the block's map. */
extern int kinsIdentityForward(const kins_params *p, kins_scratch *s,
                               const double *joint, EmcPose *pos,
                               const KINEMATICS_FORWARD_FLAGS *fflags,
                               KINEMATICS_INVERSE_FLAGS *iflags);
extern int kinsIdentityInverse(const kins_params *p, kins_scratch *s,
                               const EmcPose *pos, double *joint,
                               const KINEMATICS_INVERSE_FLAGS *iflags,
                               KINEMATICS_FORWARD_FLAGS *fflags);
extern int kinsIdentityFrame(const kins_params *p, const double *joint,
                             PmRotationMatrix *rot,
                             const KINEMATICS_FORWARD_FLAGS *fflags);
extern int kinsIdentityJacobian(const kins_params *p, const double *joint,
                                const EmcPose *pos,
                                double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                                const KINEMATICS_INVERSE_FLAGS *iflags);
extern const kins_ops KINS_IDENTITY_OPS;

/* The five questions asked of an ops table, with the defaults applied:
   identity for a missing frame, the native rotation applied to the tool
   frame, and the Jacobian differenced from the inverse when there is no
   closed form.  These are what the RT wrappers and a caller outside RT
   both go through, so both get the same answers. */
extern int kinsOpsForward(const kins_ops *ops, const kins_params *p,
                          kins_scratch *s, const double *joint, EmcPose *pos,
                          const KINEMATICS_FORWARD_FLAGS *fflags,
                          KINEMATICS_INVERSE_FLAGS *iflags);
extern int kinsOpsInverse(const kins_ops *ops, const kins_params *p,
                          kins_scratch *s, const EmcPose *pos, double *joint,
                          const KINEMATICS_INVERSE_FLAGS *iflags,
                          KINEMATICS_FORWARD_FLAGS *fflags);
extern int kinsOpsWorkFrame(const kins_ops *ops, const kins_params *p,
                            const double *joint, PmRotationMatrix *rot,
                            const KINEMATICS_FORWARD_FLAGS *fflags);
extern int kinsOpsToolFrame(const kins_ops *ops, const kins_params *p,
                            const double *joint, PmRotationMatrix *rot,
                            const KINEMATICS_FORWARD_FLAGS *fflags);
extern int kinsOpsJacobian(const kins_ops *ops, const kins_params *p,
                           kins_scratch *s, const double *joint,
                           const EmcPose *pos,
                           double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                           const KINEMATICS_INVERSE_FLAGS *iflags);

extern int kinematicsSwitchable(void);
extern int kinematicsSwitch(int switchkins_type);

/* The tool offset motion applies, handed to the module.  Motion calls this
   whenever the offset changes (G43, G49) and references it weakly, so a
   module that does not export it still loads and keeps reading whatever
   tool pin it has.  kins_single.c and switchkins.c export it for every
   module written on the parameter block: the tool then comes from the tool
   table through motion, and the module's tool pin, where it has one, is
   read only until motion has spoken. */
extern int kinematicsSetTool(const EmcPose *tool);
//NOTE: switchable kinematics may require Interp::Synch
//      before/after invoking kinematicsSwitch()
//      A convenient command to synch is: M66 E0 L0

#define KINS_NOT_SWITCHABLE \
extern int kinematicsSwitchable() {return 0;} \
extern int kinematicsSwitch(int switchkins_type) { (void)switchkins_type; return 0;} \
EXPORT_SYMBOL(kinematicsSwitchable); \
EXPORT_SYMBOL(kinematicsSwitch);


// support for template for user-defined switchkins_type==2
extern const kins_ops USERK_OPS;

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
// xyzac,xyzbc (trtfuncs.c): one geometry table, the maths of each machine
extern const kins_param_desc TRT_PARAMS[];
extern const int TRT_NPARAMS;
extern const kins_ops XYZAC_OPS;
extern const kins_ops XYZBC_OPS;

//*********************************************************************
