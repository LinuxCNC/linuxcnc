/********************************************************************
* Description: kins_module.h
*   The module side of a kinematics module: what a module is written with,
*   as opposed to kinematics.h, which is what motion and the interpreter
*   call.  The joint map and identity helpers, the tool frame library, the
*   generic Jacobian, and the parameter block form of a module with the
*   shared code that runs an ops table.  Nothing here needs HAL; the pins
*   made from a module's table are in kins_rt.h.
*
* License: GPL Version 2
********************************************************************/
#ifndef __LINUXCNC_KINS_MODULE_H
#define __LINUXCNC_KINS_MODULE_H

#include "kinematics.h"

#ifdef __cplusplus
extern "C" {
#endif

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


/* The generic implementation of the above, driven by a module's own frame
   functions, so that a module gets it for free once it supplies them.  A
   module with a closed form registers that instead: it is faster, and it
   knows its own degenerate poses without having to find them.

   num_joints is the length of seed and of each row of solutions. */
typedef int (*kinsFrameFunc)(const double *joint,
                             PmRotationMatrix *rot,
                             const KINEMATICS_FORWARD_FLAGS *fflags);

/* Which joints turn the work: a bit per joint whose motion changes the
   work frame at the seed.  This is what a caller needs to hold the table
   still while the head orients the tool (Heidenhain COORD ROT), or to let
   it take part (TABLE ROT), without a config entry naming it.  Returns 0
   or -1 if the frame cannot be evaluated. */
extern int toolFrameWorkJoints(kinsFrameFunc work, int num_joints,
                               const double *seed, unsigned int *mask);

/* The two rotaries that orient the tool, told apart.  One has its axis
   fixed in the machine frame, the primary, and the other has its axis
   carried by the first, the secondary.  The two poses that reach one tool
   direction differ in the sign of the secondary, which is what a caller
   needs to name a pose rather than count them, Heidenhain's SEQ+ and SEQ-.

   Both are found from the module's own tool frame, by turning each joint a
   little and reading the axis of the rotation that results, so a module
   declares nothing and a switchkins type that turns nothing answers -1.
   Returns 0 with both joints set, or -1 where the machine has any number
   of orienting rotaries but two, a robot wrist among them, or where the
   frame cannot be evaluated. */
extern int toolFrameOrientJoints(kinsFrameFunc tool, int num_joints,
                                 const double *seed,
                                 int *primary, int *secondary);

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

/* The Jacobian of a serial arm of six revolute joints from its
   Denavit-Hartenberg chain.  Two conventions carry that name and put the
   four parameters on different links; this is the modified one of John J.
   Craig, Introduction to Robotics: Mechanics and Control, where link i is
   Rx(alpha[i]) Tx(a[i]) Rz(joint[i]) Tz(d[i]), the joint turning about the
   z of the frame Rx and Tx leave it in.  (The original 1955 convention is
   Rz(theta) Tz(d) Tx(a) Rx(alpha), and a table written for it does not fit
   here.)  The tool point `tool` lies along the z of the last frame, and
   the pose of that point is reported as X Y Z and the RPY of the last
   frame, R = Rz(C) Ry(B) Rx(A), as pmMatRpyConvert() does.
   Each joint's axis crossed with the vector from it to the tool point
   gives the point's rate per radian of the joint, the axis itself the
   angular rate; that 6x6 inverted is the joint rate per unit of twist, and
   the RPY rates reach the twist through the matrix of the axes each one
   turns about, which is where world's B and C come in.  alpha and joint in
   degrees, a, d and tool in the module's length unit.  Rows 0 to 5 of jac
   are filled, the rest zero.  Returns 0, or -1 at a singular pose, where no
   finite joint rate follows the pose. */
extern int kinsJacobianFromDhArm(const double alpha[6], const double a[6],
                                 const double d[6], const double *joint,
                                 double tool, const EmcPose *world,
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
#define KINS_MAX_TYPES   SWITCHKINS_MAX_TYPES

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
    int      pose_seed_ok;                    /* pose_seed came from a solve that succeeded */
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
   a consumer may use to skip the maths altogether.  primary says this is
   the module's working transform, the type G43.4 switches to.  machine says
   this is the machine frame type, the tool left out, the pivot or the flange
   in machine coordinates, which G13.1 and G49 select and G53.5 moves in; a
   module that leaves it unset on every type has its identity type stand in.
   A machine frame type does not read the tool offset in the parameter block,
   so a working transform that leaves the tool out anyway, a robot's flange,
   carries primary and machine both. */
typedef struct kins_ops {
    kins_forward_fn         forward;
    kins_inverse_fn         inverse;
    kins_frame_fn           work;
    kins_frame_fn           tool;
    const PmRotationMatrix *native;     /* NULL means TOOL_FRAME_SPINDLE */
    kins_jacobian_fn        jacobian;
    int                     fwd_iterates;
    int                     identity;           /* joints are axes */
    int                     primary;            /* the working transform */
    int                     machine;            /* the machine frame */
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

#ifdef __cplusplus
}
#endif

#endif // __LINUXCNC_KINS_MODULE_H
