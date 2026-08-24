/* Utility routines for kinematics modules
** License GPL Version 2
**
** utilities for use with switchkins.c
**---------------------------------------------------------------------
** identityKinematicsSetup()
** identityKinematicsForward()
** identityKinematicsInverse()
**
** Routines for identity kinematics using mapping created by
** map_coordinates_to_jnumbers()
**
**---------------------------------------------------------------------
** map_coordinates_to_jnumbers()
**
** Map a string of coordinate letters to joint numbers sequentially.
** If allow_duplicates==1, a coordinate letter may be specified more
** than once to assign it to multiple joint numbers (the kinematics
** module must support such usage).
**
**   Default mapping if coordinates==NULL is:
**           X:0 Y:1 Z:2 A:3 B:4 C:5 U:6 V:7 W:8
**
**   Example coordinates-to-joints mappings:
**      coordinates=XYZ      X:0   Y:1   Z:2
**      coordinates=ZYX      Z:0   Y:1   X:2
**      coordinates=XYZZZZ   x:0   Y:1   Z:2,3,4,5
**      coordinates=XXYZ     X:0,1 Y:2   Z:3
**---------------------------------------------------------------------
**
** mapped_joints_to_position()
**
** Update position based mapping created by map_coordinates_to_jnumbers()
** (used for identity-based forward kinematics)
**---------------------------------------------------------------------
**
** position_to_mapped_joints()
**
** Update joints (including joints for duplicate letters)
** based on mapping created by map_coordinates_to_jnumbers()
** (used for identity-based inverse kinematics)
**
**---------------------------------------------------------------------
*/

#include <rtapi.h>
#include <rtapi_string.h>
#include <rtapi_math.h>
#include <emcmotcfg.h>
#include <emcpos.h>
#include <kinematics.h>

// principal joint numbers based on module 'coordinates' parameter
static int JX = -1;
static int JY = -1;
static int JZ = -1;
static int JA = -1;
static int JB = -1;
static int JC = -1;
static int JU = -1;
static int JV = -1;
static int JW = -1;

// bitmaps indicate joints used for each axis letter
static int X_joints_bitmap;
static int Y_joints_bitmap;
static int Z_joints_bitmap;
static int A_joints_bitmap;
static int B_joints_bitmap;
static int C_joints_bitmap;
static int U_joints_bitmap;
static int V_joints_bitmap;
static int W_joints_bitmap;

static int map_initialized = 0;
#define MAX_COORDINATES_CHARS 32
static char used_coordinates[MAX_COORDINATES_CHARS+1];

int map_coordinates_to_jnumbers(const char *coordinates,
                                const int  max_joints,
                                const int  allow_duplicates,
                                int   axis_idx_for_jno[] ) //result
{
    char* errtag="map_coordinates_to_jnumbers: ERROR:\n  ";
    int   jno=0;
    bool  found=0;
    int   dups[EMCMOT_MAX_AXIS];
    const char *coords = coordinates;
    char  coord_letter[] = {'X','Y','Z','A','B','C','U','V','W'};
    int   i;

    if (strlen(coordinates) > MAX_COORDINATES_CHARS) {
        rtapi_print_msg(RTAPI_MSG_ERR,
             "%s: map_coordinates_to_jnumbers too many chars:%s\n"
             ,__FILE__,coordinates);
        return -1;

    }
    // Note: may be called multiple times for different switchkins
    // types but coordinates must agree
    if (used_coordinates[0] == 0) {
        strcpy(used_coordinates,coordinates);
    } else {
        if (strcasecmp(coordinates,used_coordinates)) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                 "%s: map_coordinates_to_jnumbers altered:%s %s\n"
                 ,__FILE__,used_coordinates,coordinates);
            return -1;
        }
    }
    for (i=0; i<EMCMOT_MAX_AXIS; i++) {dups[i] = 0;}

    if ( (max_joints <= 0) || (max_joints > EMCMOT_MAX_JOINTS) ) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s bogus max_joints=%d\n",
          errtag,max_joints);
        return -1;
    }

    // init all axis_idx_for_jno[] (-1 means unspecified)
    for(jno=0; jno<EMCMOT_MAX_JOINTS; jno++) { axis_idx_for_jno[jno] = -1; }

    if (coords == NULL) { coords = "XYZABCUVW"; }
    jno = 0; // begin: assign joint numbers at 0th coords position
    while (*coords) {
        found = 0;
        switch(*coords) {
          case 'x': case 'X': axis_idx_for_jno[jno]= 0;dups[0]++;found=1;break;
          case 'y': case 'Y': axis_idx_for_jno[jno]= 1;dups[1]++;found=1;break;
          case 'z': case 'Z': axis_idx_for_jno[jno]= 2;dups[2]++;found=1;break;
          case 'a': case 'A': axis_idx_for_jno[jno]= 3;dups[3]++;found=1;break;
          case 'b': case 'B': axis_idx_for_jno[jno]= 4;dups[4]++;found=1;break;
          case 'c': case 'C': axis_idx_for_jno[jno]= 5;dups[5]++;found=1;break;
          case 'u': case 'U': axis_idx_for_jno[jno]= 6;dups[6]++;found=1;break;
          case 'v': case 'V': axis_idx_for_jno[jno]= 7;dups[7]++;found=1;break;
          case 'w': case 'W': axis_idx_for_jno[jno]= 8;dups[8]++;found=1;break;
          case ' ': case '\t': coords++;continue; //whitespace
        }
        if (found) {
            coords++; // next coordinates letter
            jno++;    // next joint number
        } else {
            rtapi_print_msg(RTAPI_MSG_ERR,
              "%s Invalid character '%c' in coordinates '%s'\n",
              errtag,*coords,coordinates);
            return -1;
        }
        if (jno > max_joints) {
            rtapi_print_msg(RTAPI_MSG_ERR,
              "%s too many coordinates <%s> for max_joints=%d\n",
              errtag,coordinates,max_joints);
            return -1;
        }
    } // while

    if (!found) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s missing coordinates '%s'\n",
          errtag,coordinates);
        return -1;
    }
    if (!allow_duplicates) {
        int ano;
        for(ano=0; ano<EMCMOT_MAX_AXIS; ano++) {
            if (dups[ano] > 1) {
                rtapi_print_msg(RTAPI_MSG_ERR,
                "%s duplicates not allowed in coordinates=%s, letter=%c\n",
                errtag,coordinates,coord_letter[ano]);
                return -1;
            }
        }
    }

    for (jno=0; jno < max_joints; jno++) {
      int bitnumber = 1<<jno;
      /* Assign principal joint (first joint listed for a coordinate letter
      ** (using the coordinates module parameter) and use for forward
      ** kinematics.
      ** Assign a bitmap for duplicate joints listed and use for inverse
      **  kinematics.
      **
      ** example: coordinates=xyzbcwy (duplicate y)
      **          JX=0 X_joints_bitmap=0x01 joints: 0
      **          JY=1 Y_joints_bitmap=0x42 joints: 1 and 6
      **          JZ=2 Z_joints_bitmap=0x04 joints: 2
      **          JB=3 C_joints_bitmap=0x10 joints: 3
      **          JC=4 C_joints_bitmap=0x10 joints: 4
      **          JW=5 C_joints_bitmap=0x10 joints: 5
      **
      ** xyzabcuvw letters
      ** 012345678 indices
      */
      if (axis_idx_for_jno[jno] == 0) {
         if (JX == -1)  JX=jno;
         X_joints_bitmap |= bitnumber;
      }
      if (axis_idx_for_jno[jno] == 1) {
         if (JY == -1)  JY=jno;
         Y_joints_bitmap |= bitnumber;
      }
      if (axis_idx_for_jno[jno] == 2) {
         if (JZ == -1)  JZ=jno;
         Z_joints_bitmap |= bitnumber;
      }
      if (axis_idx_for_jno[jno] == 3) {
         if (JA == -1)  JA=jno;
         A_joints_bitmap |= bitnumber;
      }
      if (axis_idx_for_jno[jno] == 4) {
         if (JB == -1)  JB=jno;
         B_joints_bitmap |= bitnumber;
      }
      if (axis_idx_for_jno[jno] == 5) {
         if (JC == -1)  JC=jno;
         C_joints_bitmap |= bitnumber;
      }
      if (axis_idx_for_jno[jno] == 6) {
         if (JU == -1)  JU=jno;
         U_joints_bitmap |= bitnumber;
      }
      if (axis_idx_for_jno[jno] == 7) {
         if (JV == -1)  JV=jno;
         V_joints_bitmap |= bitnumber;
      }
      if (axis_idx_for_jno[jno] == 8) {
         if (JW == -1)  JW=jno;
         W_joints_bitmap |= bitnumber;
      }
    }
    map_initialized = 1;
    return 0;
} //map_coordinates_to_jnumbers()

int mapped_joints_to_position(const int max_joints,
                              const double * joints,
                              EmcPose * pos)
{
    int jno;
    if (!map_initialized) {
        rtapi_print_msg(RTAPI_MSG_ERR,
             "mapped_joints_to_position() before map_initialized\n");
        return -1;
    }
    for (jno=0; jno < max_joints; jno++) {
        int bit = 1<<jno;
        if ( bit & X_joints_bitmap ) pos->tran.x = joints[JX];
        if ( bit & Y_joints_bitmap ) pos->tran.y = joints[JY];
        if ( bit & Z_joints_bitmap ) pos->tran.z = joints[JZ];
        if ( bit & A_joints_bitmap ) pos->a      = joints[JA];
        if ( bit & B_joints_bitmap ) pos->b      = joints[JB];
        if ( bit & C_joints_bitmap ) pos->c      = joints[JC];
        if ( bit & U_joints_bitmap ) pos->u      = joints[JU];
        if ( bit & V_joints_bitmap ) pos->v      = joints[JV];
        if ( bit & W_joints_bitmap ) pos->w      = joints[JW];
    }
    return 0;
} // mapped_joints_to_position()

int position_to_mapped_joints(const int max_joints,
                              const EmcPose * pos,
                              double* joints)
{
    int jno;
    if (!map_initialized) {
        rtapi_print_msg(RTAPI_MSG_ERR,
             "position_to_mapped_joints before map_initialized\n");
        return -1;
    }
    for (jno=0; jno < max_joints; jno++) {
        int bit = 1<<jno;
        if ( bit & X_joints_bitmap ) joints[jno] = pos->tran.x;
        if ( bit & Y_joints_bitmap ) joints[jno] = pos->tran.y;
        if ( bit & Z_joints_bitmap ) joints[jno] = pos->tran.z;
        if ( bit & A_joints_bitmap ) joints[jno] = pos->a;
        if ( bit & B_joints_bitmap ) joints[jno] = pos->b;
        if ( bit & C_joints_bitmap ) joints[jno] = pos->c;
        if ( bit & U_joints_bitmap ) joints[jno] = pos->u;
        if ( bit & V_joints_bitmap ) joints[jno] = pos->v;
        if ( bit & W_joints_bitmap ) joints[jno] = pos->w;
    }
    return 0;
} // position_to_mapped_joints()

static int identity_kinematics_initialized = 0;
static int identity_max_joints;

int identityKinematicsSetup(const int   comp_id,
                            const char* coordinates,
                            kparms*     kp)
{
    (void)comp_id;
    int axis_idx_for_jno[EMCMOT_MAX_JOINTS];
    int jno;
    int show=0;
    bool islathe;

    identity_max_joints = strlen(coordinates);

    if (map_coordinates_to_jnumbers(coordinates,
                                    kp->max_joints,
                                    kp->allow_duplicates,
                                    axis_idx_for_jno)) {
       return -1; //mapping failed
    }

    /* print message for unconventional ordering;
    **   a) duplicate coordinate letters
    **   b) letters not ordered by "XYZABCUVW" sequence
    **      (use kinstype=both works best for these)
    */
    for (jno=0; jno<identity_max_joints; jno++) {
        if (axis_idx_for_jno[jno] == -1) break; //fini
        if (axis_idx_for_jno[jno] != jno) { show++; } //not default order
    }
    islathe = !strcasecmp(coordinates,"xz"); // no show if simple lathe
    if (show && !islathe) {
        rtapi_print("\nidentityKinematicsSetup: coordinates:%s\n", coordinates);
        char *p="XYZABCUVW";
        for (jno=0; jno<identity_max_joints; jno++) {
            if (axis_idx_for_jno[jno] == -1) break; //fini
            rtapi_print("   Joint %d ==> Axis %c\n",
                       jno,*(p+axis_idx_for_jno[jno]));
        }
        if (kinematicsType() != KINEMATICS_BOTH) {
            rtapi_print("identityKinematicsSetup: Recommend: kinstype=both\n");
        }
        rtapi_print("\n");
    }

    identity_kinematics_initialized = 1;
    return 0;
} // identityKinematicsSetup()

int identityKinematicsForward(const double *joints,
                              EmcPose * pos,
                              const KINEMATICS_FORWARD_FLAGS * fflags,
                              KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)fflags;
    (void)iflags;
    if (!identity_kinematics_initialized) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "identityKinematicsForward: not initialized\n");
        return -1;
    }

    // support multiple-joint-per-coordinate-letter assignments:
    mapped_joints_to_position(identity_max_joints,joints,pos);
    return 0;
} // identityKinematicsForward()

int identityKinematicsInverse(const EmcPose * pos,
                              double *joints,
                              const KINEMATICS_INVERSE_FLAGS * iflags,
                              KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)iflags;
    (void)fflags;
    if (!identity_kinematics_initialized) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "identityKinematicsInverse: not initialized\n");
        return -1;
    }

    // support multiple-joint-per-coordinate-letter assignments:
    position_to_mapped_joints(identity_max_joints,pos,joints);

    return 0;
} // identityKinematicsInverse()

const PmRotationMatrix TOOL_FRAME_SPINDLE = {
    { 1, 0, 0},   // tool x
    { 0, 1, 0},   // tool y
    { 0, 0, 1}    // tool axis
};

// half turn about tool x: reverses the tool axis and tool y, keeps tool x,
// and keeps the frame right-handed.  Negating the tool axis on its own would
// leave a reflection, which is not a frame any machine can hold.
const PmRotationMatrix TOOL_FRAME_FLANGE = {
    { 1,  0,  0},
    { 0, -1,  0},
    { 0,  0, -1}
};

int toolFrameIsProper(const PmRotationMatrix *m)
{
    const double c[3][3] = {
        { m->x.x, m->y.x, m->z.x },
        { m->x.y, m->y.y, m->z.y },
        { m->x.z, m->y.z, m->z.z }
    };
    double det;
    int a, b, k;

    for (a = 0; a < 3; a++) {
        for (b = a; b < 3; b++) {
            double dot = 0;
            for (k = 0; k < 3; k++) { dot += c[k][a] * c[k][b]; }
            if (fabs(dot - (a == b ? 1.0 : 0.0)) > 1e-9) { return 0; }
        }
    }

    det = c[0][0] * (c[1][1]*c[2][2] - c[1][2]*c[2][1])
        - c[0][1] * (c[1][0]*c[2][2] - c[1][2]*c[2][0])
        + c[0][2] * (c[1][0]*c[2][1] - c[1][1]*c[2][0]);

    return fabs(det - 1.0) <= 1e-9;
} // toolFrameIsProper()

int toolFrameApplyNative(PmRotationMatrix *rot,
                         const PmRotationMatrix *native)
{
    // rot holds the module's own frame, native the rotation relating it to
    // the convention, so the answer is rot * native: the declared rotation is
    // expressed in the module's frame, not in machine coordinates.
    const double r[3][3] = {
        { rot->x.x, rot->y.x, rot->z.x },
        { rot->x.y, rot->y.y, rot->z.y },
        { rot->x.z, rot->y.z, rot->z.z }
    };
    const double n[3][3] = {
        { native->x.x, native->y.x, native->z.x },
        { native->x.y, native->y.y, native->z.y },
        { native->x.z, native->y.z, native->z.z }
    };
    double m[3][3];
    int a, b, k;

    if (!toolFrameIsProper(native)) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "toolFrameApplyNative: declared rotation is not a proper rotation\n");
        return -1;
    }

    for (a = 0; a < 3; a++) {
        for (b = 0; b < 3; b++) {
            m[a][b] = 0;
            for (k = 0; k < 3; k++) { m[a][b] += r[a][k] * n[k][b]; }
        }
    }

    rot->x.x = m[0][0]; rot->y.x = m[0][1]; rot->z.x = m[0][2];
    rot->x.y = m[1][0]; rot->y.y = m[1][1]; rot->z.y = m[1][2];
    rot->x.z = m[2][0]; rot->y.z = m[2][1]; rot->z.z = m[2][2];

    return 0;
} // toolFrameApplyNative()

int toolFrameInWork(const PmRotationMatrix *work,
                    const PmRotationMatrix *tool,
                    PmRotationMatrix *out)
{
    // transpose(work) * tool: both are given against the machine, and
    // transposing the work frame turns "machine to work" out of "work to
    // machine" without a general inverse, because a rotation is orthonormal
    const double w[3][3] = {
        { work->x.x, work->y.x, work->z.x },
        { work->x.y, work->y.y, work->z.y },
        { work->x.z, work->y.z, work->z.z }
    };
    const double t[3][3] = {
        { tool->x.x, tool->y.x, tool->z.x },
        { tool->x.y, tool->y.y, tool->z.y },
        { tool->x.z, tool->y.z, tool->z.z }
    };
    double m[3][3];
    int a, b, k;

    for (a = 0; a < 3; a++) {
        for (b = 0; b < 3; b++) {
            m[a][b] = 0;
            for (k = 0; k < 3; k++) { m[a][b] += w[k][a] * t[k][b]; }
        }
    }

    out->x.x = m[0][0]; out->y.x = m[0][1]; out->z.x = m[0][2];
    out->x.y = m[1][0]; out->y.y = m[1][1]; out->z.y = m[1][2];
    out->x.z = m[2][0]; out->y.z = m[2][1]; out->z.z = m[2][2];

    return 0;
} // toolFrameInWork()

int identityKinematicsWorkFrame(const double *joints,
                                PmRotationMatrix *rot,
                                const KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)joints;
    (void)fflags;
    // nothing carries the work, so it stays square with the machine
    *rot = TOOL_FRAME_SPINDLE;
    return 0;
} // identityKinematicsWorkFrame()

int identityKinematicsToolFrame(const double *joints,
                                PmRotationMatrix *rot,
                                const KINEMATICS_FORWARD_FLAGS *fflags)
{
    (void)joints;
    (void)fflags;
    // joints are axes, so the tool stays square with the machine
    *rot = TOOL_FRAME_SPINDLE;
    return 0;
} // identityKinematicsToolFrame()

//----------------------------------------------------------------------
// toolFrameSolve()
//
// The inverse of the tool orientation, built on nothing but a module's own
// work and tool frame functions, so that supplying those is enough and no
// module has to hand-derive a formula.
//
// The problem is small: the only joints that can turn the tool are rotary
// ones, there are rarely more than three of them, and the orientation is a
// function of those joints alone.  So the routine finds which joints move
// transpose(work) * tool, and solves for them by damped least squares from a
// spread of starting points, keeping the roots that are distinct.
//
// Three things are worth naming because they are what the naive version gets
// wrong.
//
// The damping is adaptive.  At a singular pose the Jacobian loses rank, and a
// fixed small damping turns the noise in the near-null direction into a step
// of thousands of degrees.  Raising the damping when a step fails and lowering
// it when one succeeds is what keeps those poses solvable at all.
//
// The Jacobian is taken with central differences.  A one sided difference has
// an error of the same order as the step, and it appears as a spurious small
// singular value, which is exactly what the rank test must not see.
//
// The joint unit is discovered rather than assumed.  Every module in the tree
// takes rotary joints in degrees, but the interface does not say so, and the
// search has to cover exactly one turn.  Adding a whole turn and asking
// whether the frame came back settles it, and rescaling into a unit where one
// turn is 2*pi makes the damping and the step limits the same on any module.
//----------------------------------------------------------------------

#define TFS_MAX_RES     6       // three for the tool axis, three for tool x
#define TFS_ITERS      60
#define TFS_FD_STEP     1e-6    // internal radians
#define TFS_MOVED_TOL   1e-9    // frame difference that counts as movement
#define TFS_RANK_TOL    1e-4    // a direction worth less than this is free
#define TFS_SOLVED      1e-18   // sum of squared residuals
#define TFS_STEP_LIMIT  0.4     // internal radians per iteration

typedef struct {
    kinsFrameFunc work;
    kinsFrameFunc tool;
    int    num_joints;
    const  double *seed;
    int    nfree;
    int    free[TOOL_FRAME_MAX_FREE];
    double scale[TOOL_FRAME_MAX_FREE];  // joint units per internal radian
    int    nres;
    double want[TFS_MAX_RES];
    double joint[EMCMOT_MAX_JOINTS];    // scratch, rebuilt on every call
} tfs_ctx;

// transpose(work) * tool at a joint set, as the columns the request names
static int tfs_frame(tfs_ctx *c, const double *joint, double *axis, double *xdir)
{
    KINEMATICS_FORWARD_FLAGS fflags = 0;
    PmRotationMatrix w, t, m;

    if (c->work(joint, &w, &fflags)) { return -1; }
    if (c->tool(joint, &t, &fflags)) { return -1; }
    toolFrameInWork(&w, &t, &m);

    axis[0] = m.z.x; axis[1] = m.z.y; axis[2] = m.z.z;
    xdir[0] = m.x.x; xdir[1] = m.x.y; xdir[2] = m.x.z;
    return 0;
}

// joint values for a point of the internal search space
static void tfs_joints(tfs_ctx *c, const double *u)
{
    int i;
    for (i = 0; i < c->num_joints; i++) { c->joint[i] = c->seed[i]; }
    for (i = 0; i < c->nfree; i++) {
        c->joint[c->free[i]] = u[i] * c->scale[i];
    }
}

static int tfs_res(tfs_ctx *c, const double *u, double *r)
{
    double axis[3], xdir[3];
    int i;

    tfs_joints(c, u);
    if (tfs_frame(c, c->joint, axis, xdir)) { return -1; }

    for (i = 0; i < 3; i++) { r[i] = axis[i] - c->want[i]; }
    if (c->nres > 3) {
        for (i = 0; i < 3; i++) { r[3+i] = xdir[i] - c->want[3+i]; }
    }
    return 0;
}

static double tfs_norm2(const double *r, int n)
{
    double s = 0;
    int i;
    for (i = 0; i < n; i++) { s += r[i]*r[i]; }
    return s;
}

static int tfs_jac(tfs_ctx *c, const double *u, double J[TFS_MAX_RES][TOOL_FRAME_MAX_FREE])
{
    double up[TOOL_FRAME_MAX_FREE], rp[TFS_MAX_RES], rm[TFS_MAX_RES];
    int i, k;

    for (k = 0; k < c->nfree; k++) {
        for (i = 0; i < c->nfree; i++) { up[i] = u[i]; }
        up[k] = u[k] + TFS_FD_STEP;
        if (tfs_res(c, up, rp)) { return -1; }
        up[k] = u[k] - TFS_FD_STEP;
        if (tfs_res(c, up, rm)) { return -1; }
        for (i = 0; i < c->nres; i++) {
            J[i][k] = (rp[i] - rm[i]) / (2*TFS_FD_STEP);
        }
    }
    return 0;
}

// in place inverse of an n by n matrix by Gauss-Jordan with partial pivoting,
// n being at most TOOL_FRAME_MAX_FREE
static int tfs_inv(double A[TOOL_FRAME_MAX_FREE][TOOL_FRAME_MAX_FREE], int n)
{
    double aug[TOOL_FRAME_MAX_FREE][2*TOOL_FRAME_MAX_FREE];
    int i, j, col, piv;

    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) { aug[i][j] = A[i][j]; }
        for (j = 0; j < n; j++) { aug[i][n+j] = (i == j) ? 1.0 : 0.0; }
    }
    for (col = 0; col < n; col++) {
        piv = col;
        for (i = col+1; i < n; i++) {
            if (fabs(aug[i][col]) > fabs(aug[piv][col])) { piv = i; }
        }
        if (fabs(aug[piv][col]) < 1e-300) { return -1; }
        if (piv != col) {
            for (j = 0; j < 2*n; j++) {
                double sw = aug[col][j]; aug[col][j] = aug[piv][j]; aug[piv][j] = sw;
            }
        }
        {
            double d = aug[col][col];
            for (j = 0; j < 2*n; j++) { aug[col][j] /= d; }
        }
        for (i = 0; i < n; i++) {
            double f = aug[i][col];
            if (i == col || f == 0.0) { continue; }
            for (j = 0; j < 2*n; j++) { aug[i][j] -= f*aug[col][j]; }
        }
    }
    for (i = 0; i < n; i++) {
        for (j = 0; j < n; j++) { A[i][j] = aug[i][n+j]; }
    }
    return 0;
}

// rank by counting pivots, which is all that is needed to say how many
// directions the request leaves free
static int tfs_rank(const double J[TFS_MAX_RES][TOOL_FRAME_MAX_FREE], int m, int n)
{
    double a[TFS_MAX_RES][TOOL_FRAME_MAX_FREE];
    double big = 0;
    int i, j, col, piv, rank = 0;

    for (i = 0; i < m; i++) {
        for (j = 0; j < n; j++) {
            a[i][j] = J[i][j];
            if (fabs(a[i][j]) > big) { big = fabs(a[i][j]); }
        }
    }
    if (big <= 0) { return 0; }

    for (col = 0; col < n && rank < m; col++) {
        piv = rank;
        for (i = rank+1; i < m; i++) {
            if (fabs(a[i][col]) > fabs(a[piv][col])) { piv = i; }
        }
        if (fabs(a[piv][col]) < TFS_RANK_TOL*big) { continue; }
        if (piv != rank) {
            for (j = 0; j < n; j++) {
                double sw = a[rank][j]; a[rank][j] = a[piv][j]; a[piv][j] = sw;
            }
        }
        for (i = rank+1; i < m; i++) {
            double f = a[i][col]/a[rank][col];
            for (j = 0; j < n; j++) { a[i][j] -= f*a[rank][j]; }
        }
        rank++;
    }
    return rank;
}

// damped least squares with adaptive damping.  Returns 1 when the residual is
// down to the solved threshold, 0 otherwise, and leaves u where it stopped.
static int tfs_levmar(tfs_ctx *c, double *u)
{
    double r[TFS_MAX_RES], r2[TFS_MAX_RES];
    double J[TFS_MAX_RES][TOOL_FRAME_MAX_FREE];
    double A[TOOL_FRAME_MAX_FREE][TOOL_FRAME_MAX_FREE];
    double g[TOOL_FRAME_MAX_FREE], step[TOOL_FRAME_MAX_FREE];
    double u2[TOOL_FRAME_MAX_FREE];
    double f, f2, lambda = 1e-3;
    int i, j, k, it;

    if (tfs_res(c, u, r)) { return 0; }
    f = tfs_norm2(r, c->nres);

    for (it = 0; it < TFS_ITERS && f > TFS_SOLVED; it++) {
        double trace = 0, big = 0;

        if (tfs_jac(c, u, J)) { return 0; }

        for (i = 0; i < c->nfree; i++) {
            for (j = 0; j < c->nfree; j++) {
                double s = 0;
                for (k = 0; k < c->nres; k++) { s += J[k][i]*J[k][j]; }
                A[i][j] = s;
            }
            trace += A[i][i];
            g[i] = 0;
            for (k = 0; k < c->nres; k++) { g[i] += J[k][i]*r[k]; }
        }
        trace = trace/c->nfree + 1e-30;

        for (i = 0; i < c->nfree; i++) { A[i][i] += lambda*trace; }
        if (tfs_inv(A, c->nfree)) { return 0; }

        for (i = 0; i < c->nfree; i++) {
            step[i] = 0;
            for (j = 0; j < c->nfree; j++) { step[i] -= A[i][j]*g[j]; }
            if (fabs(step[i]) > big) { big = fabs(step[i]); }
        }
        if (big > TFS_STEP_LIMIT) {
            for (i = 0; i < c->nfree; i++) { step[i] *= TFS_STEP_LIMIT/big; }
        }
        for (i = 0; i < c->nfree; i++) { u2[i] = u[i] + step[i]; }

        if (tfs_res(c, u2, r2)) { return 0; }
        f2 = tfs_norm2(r2, c->nres);

        if (f2 < f) {
            for (i = 0; i < c->nfree; i++) { u[i] = u2[i]; }
            for (i = 0; i < c->nres; i++) { r[i] = r2[i]; }
            f = f2;
            lambda *= 0.3;
            if (lambda < 1e-12) { lambda = 1e-12; }
        } else {
            lambda *= 4.0;
            if (lambda > 1e12) { break; }
        }
    }
    return f <= TFS_SOLVED;
}

static double tfs_wrap(double a)
{
    while (a >  PM_PI) { a -= 2*PM_PI; }
    while (a < -PM_PI) { a += 2*PM_PI; }
    return a;
}

// which joints turn the tool, and what one turn of each is worth in its own
// units.  Returns the count, or -1 if a joint moves the tool without having a
// period, which the search has no way to bound.
static int tfs_survey(tfs_ctx *c)
{
    double base_axis[3], base_x[3], axis[3], xdir[3];
    static const double candidate[2] = { 360.0, 2*PM_PI };
    int i, k, n = 0;

    for (i = 0; i < c->num_joints; i++) { c->joint[i] = c->seed[i]; }
    if (tfs_frame(c, c->joint, base_axis, base_x)) { return -1; }

    for (i = 0; i < c->num_joints; i++) {
        double moved = 0;
        int p;

        for (k = 0; k < c->num_joints; k++) { c->joint[k] = c->seed[k]; }
        c->joint[i] = c->seed[i] + 1e-4;
        if (tfs_frame(c, c->joint, axis, xdir)) { return -1; }
        for (k = 0; k < 3; k++) {
            if (fabs(axis[k] - base_axis[k]) > moved) { moved = fabs(axis[k] - base_axis[k]); }
            if (fabs(xdir[k] - base_x[k])   > moved) { moved = fabs(xdir[k] - base_x[k]); }
        }
        if (moved <= TFS_MOVED_TOL) { continue; }

        if (n >= TOOL_FRAME_MAX_FREE) { return -1; }

        c->scale[n] = 0;
        for (p = 0; p < 2; p++) {
            double back = 0;
            c->joint[i] = c->seed[i] + candidate[p];
            if (tfs_frame(c, c->joint, axis, xdir)) { return -1; }
            for (k = 0; k < 3; k++) {
                if (fabs(axis[k] - base_axis[k]) > back) { back = fabs(axis[k] - base_axis[k]); }
                if (fabs(xdir[k] - base_x[k])    > back) { back = fabs(xdir[k] - base_x[k]); }
            }
            if (back <= TFS_MOVED_TOL) {
                c->scale[n] = candidate[p]/(2*PM_PI);
                break;
            }
        }
        if (c->scale[n] == 0) { return -1; }

        c->free[n] = i;
        n++;
    }
    c->nfree = n;
    return n;
}

// enumerate the roots for whatever the context currently constrains
static int tfs_search(tfs_ctx *c,
                      double *solutions,
                      int max_solutions,
                      int *free_directions)
{
    double kept[TOOL_FRAME_MAX_SOLUTIONS][TOOL_FRAME_MAX_FREE];
    double u[TOOL_FRAME_MAX_FREE], useed[TOOL_FRAME_MAX_FREE];
    double r[TFS_MAX_RES], J[TFS_MAX_RES][TOOL_FRAME_MAX_FREE];
    int index[TOOL_FRAME_MAX_FREE];
    int found = 0, per_axis, first = 1, i, k;

    for (i = 0; i < TOOL_FRAME_MAX_FREE; i++) { u[i] = 0; useed[i] = 0; }

    // nothing on this machine turns the tool, so the only candidate is where
    // the machine already is
    if (c->nfree == 0) {
        if (tfs_res(c, u, r)) { return -1; }
        if (tfs_norm2(r, c->nres) > TFS_SOLVED) { return 0; }
        for (i = 0; i < c->num_joints; i++) { solutions[i] = c->seed[i]; }
        if (free_directions) { free_directions[0] = 0; }
        return 1;
    }

    for (i = 0; i < c->nfree; i++) {
        useed[i] = c->seed[c->free[i]] / c->scale[i];
        index[i] = 0;
    }

    // Quarter turns of each free joint, starting from where the machine is so
    // that a machine with a free direction reports the answer nearest its
    // present pose.  Two per turn already enters every basin on the machines
    // in the tree, and four is the margin for one that is not: the roots are
    // few and widely separated, because they come from the two branches of an
    // arc cosine and not from anything finely structured.
    per_axis = 4;

    for (;;) {
        int solved, rank, dup = 0;

        if (first) {
            for (i = 0; i < c->nfree; i++) { u[i] = useed[i]; }
        } else {
            for (i = 0; i < c->nfree; i++) {
                u[i] = -PM_PI + (2*PM_PI*index[i])/per_axis;
            }
        }

        solved = tfs_levmar(c, u);
        if (solved) {
            for (i = 0; i < c->nfree; i++) { u[i] = tfs_wrap(u[i]); }
            if (tfs_res(c, u, r) || tfs_jac(c, u, J)) { return -1; }

            rank = tfs_rank((const double (*)[TOOL_FRAME_MAX_FREE])J,
                            c->nres, c->nfree);
            tfs_joints(c, u);

            // a rank deficient root means the request does not pin the machine
            // down and the answer is a continuum.  Report this one point of it
            // and say so, rather than returning samples of a curve alongside
            // roots that mean something else.
            if (c->nfree - rank > 0) {
                for (i = 0; i < c->num_joints; i++) { solutions[i] = c->joint[i]; }
                if (free_directions) { free_directions[0] = c->nfree - rank; }
                return 1;
            }

            // Two roots are the same pose if going from one to the other
            // does not move the tool.  That covers landing on a root already
            // found, and it also covers the case a distance test would get
            // wrong: near a singularity the search reaches points a long way
            // apart in joint values whose frames differ by less than it can
            // resolve, and those are one answer and not several.
            for (k = 0; k < found; k++) {
                double mid[TOOL_FRAME_MAX_FREE] = {0};

                for (i = 0; i < c->nfree; i++) {
                    mid[i] = kept[k][i] + tfs_wrap(u[i] - kept[k][i])/2;
                }
                if (tfs_res(c, mid, r)) { return -1; }
                if (tfs_norm2(r, c->nres) <= TFS_SOLVED) { dup = 1; break; }
            }

            if (!dup) {
                // the dedupe evaluated other points, so rebuild this one
                tfs_joints(c, u);
                for (i = 0; i < c->num_joints; i++) {
                    solutions[found*c->num_joints + i] = c->joint[i];
                }
                if (free_directions) { free_directions[found] = 0; }
                for (i = 0; i < c->nfree; i++) { kept[found][i] = u[i]; }
                found++;
                if (found >= max_solutions) { return found; }
            }
        }

        if (first) { first = 0; continue; }

        for (i = 0; i < c->nfree; i++) {
            if (++index[i] < per_axis) { break; }
            index[i] = 0;
        }
        if (i == c->nfree) { break; }
    }

    return found;
}

// the turn about the tool axis that carries the tool x this pose achieves onto
// the one the caller asked for
static int tfs_spin(tfs_ctx *c, const double *joint,
                    const PmCartesian *x_in_work, double *spin)
{
    KINEMATICS_FORWARD_FLAGS fflags = 0;
    PmRotationMatrix w, t, m;
    double along_x, along_y;

    if (c->work(joint, &w, &fflags)) { return -1; }
    if (c->tool(joint, &t, &fflags)) { return -1; }
    toolFrameInWork(&w, &t, &m);

    along_x = m.x.x*x_in_work->x + m.x.y*x_in_work->y + m.x.z*x_in_work->z;
    along_y = m.y.x*x_in_work->x + m.y.y*x_in_work->y + m.y.z*x_in_work->z;

    *spin = atan2(along_y, along_x);
    return 0;
}

int toolFrameSolve(kinsFrameFunc work,
                   kinsFrameFunc tool,
                   int num_joints,
                   const PmCartesian *axis_in_work,
                   const PmCartesian *x_in_work,
                   const double *seed,
                   double *solutions,
                   int max_solutions,
                   int *free_directions,
                   double *tool_spin)
{
    tfs_ctx c;
    int found, i;

    if (!work || !tool || !seed || !solutions || !axis_in_work
        || num_joints <= 0 || num_joints > EMCMOT_MAX_JOINTS
        || max_solutions <= 0) {
        return -1;
    }
    if (max_solutions > TOOL_FRAME_MAX_SOLUTIONS) {
        max_solutions = TOOL_FRAME_MAX_SOLUTIONS;
    }

    c.work = work;
    c.tool = tool;
    c.num_joints = num_joints;
    c.seed = seed;
    c.nres = x_in_work ? 6 : 3;
    c.want[0] = axis_in_work->x;
    c.want[1] = axis_in_work->y;
    c.want[2] = axis_in_work->z;
    if (x_in_work) {
        double square = axis_in_work->x * x_in_work->x
                      + axis_in_work->y * x_in_work->y
                      + axis_in_work->z * x_in_work->z;

        // the two vectors are two axes of one frame, so a request where they
        // are not at right angles is not a frame and cannot be reached by
        // anything
        if (fabs(square) > 1e-6) { return -1; }

        c.want[3] = x_in_work->x;
        c.want[4] = x_in_work->y;
        c.want[5] = x_in_work->z;
    }

    if (tfs_survey(&c) < 0) { return -1; }

    found = tfs_search(&c, solutions, max_solutions, free_directions);
    if (found != 0 || !x_in_work) {
        if (tool_spin) {
            for (i = 0; i < (found > 0 ? found : 0); i++) { tool_spin[i] = 0; }
        }
        return found;
    }

    // The joints cannot place tool x, which is the ordinary case: a five axis
    // machine spends both rotaries reaching the tool axis and the turn about
    // that axis is not a joint at all.  It is still reachable, as a rotation
    // of the frame rather than a motion of the machine, so answer with the
    // poses that reach the axis and the turn that finishes the job.  That is
    // what a control does with a Heidenhain base vector or a Fanuc G68.2
    // block, neither of which refuses the program for asking.
    if (!tool_spin) { return 0; }

    c.nres = 3;
    found = tfs_search(&c, solutions, max_solutions, free_directions);
    if (found <= 0) { return found; }

    for (i = 0; i < found; i++) {
        if (tfs_spin(&c, solutions + i*num_joints, x_in_work, &tool_spin[i])) {
            return -1;
        }
    }
    return found;
}
