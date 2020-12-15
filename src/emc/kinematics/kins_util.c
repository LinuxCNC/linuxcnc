/* Utility routines for kinematics modules
** License GPL Version 2
**
**
**
**
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
*/

#include "rtapi.h"
#include "motion.h"
#include "rtapi_string.h"

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

    for (i=0; i<EMCMOT_MAX_AXIS; i++) {dups[i] = 0;}

    if ( (max_joints <= 0) || (max_joints > EMCMOT_MAX_JOINTS) ) {
        rtapi_print_msg(RTAPI_MSG_ERR,"%s bogus max_joints=%d\n",
          errtag,max_joints);
        return -1;
    }

    // init all axis_idx_for_jno[] to -1 ==> unspecified
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
    return 0;
} //map_coordinates_to_jnumbers()

// IDENTITY kinematics implementation (local)
// joint number assignments when switched to identity kinematics
// are set by module coordinates= parameter,default ordering is:
#define DEFAULT_LETTER_TOJOINT_MAP "XYZABC"
static int JX = -1;
static int JY = -1;
static int JZ = -1;
static int JA = -1;
static int JB = -1;
static int JC = -1;
static int JU = -1;
static int JV = -1;
static int JW = -1;

static int identity_kinematics_inited = 0;

int identityKinematicsSetup(const int   comp_id,
                            const char* coordinates,
                            kparms*     kp)
{
    int axis_idx_for_jno[EMCMOT_MAX_JOINTS];
    int jno;
    int islathe;
    int show=0;

    if (map_coordinates_to_jnumbers(coordinates,
                                    kp->max_joints,
                                    kp->allow_duplicates,
                                    axis_idx_for_jno)) {
       return -1; //mapping failed
    }

    for (jno=0; jno<EMCMOT_MAX_JOINTS; jno++) {
      if (axis_idx_for_jno[jno] == 0) {JX = jno;}
      if (axis_idx_for_jno[jno] == 1) {JY = jno;}
      if (axis_idx_for_jno[jno] == 2) {JZ = jno;}
      if (axis_idx_for_jno[jno] == 3) {JA = jno;}
      if (axis_idx_for_jno[jno] == 4) {JB = jno;}
      if (axis_idx_for_jno[jno] == 5) {JC = jno;}
      if (axis_idx_for_jno[jno] == 6) {JU = jno;}
      if (axis_idx_for_jno[jno] == 7) {JV = jno;}
      if (axis_idx_for_jno[jno] == 8) {JW = jno;}
    }

#if 0
    rtapi_print("\nidentity_kin_init coordinates=%s identity assignments:\n",
               coordinates);
    for (jno=0; jno<EMCMOT_MAXJOINTS; jno++) {
        if (axis_idx_for_jno[jno] == -1) break; //fini
        rtapi_print("   Joint %d ==> Axis %c\n",
                   jno,*("XYZABCUVW"+axis_idx_for_jno[jno]));
    }
#endif

    /* print message for unconventional ordering;
    **   a) duplicate coordinate letters
    **   b) letters not ordered by "XYZABCUVW" sequence
    **      (use kinstype=both works best for these)
    */
    for (jno=0; jno<EMCMOT_MAX_JOINTS; jno++) {
        if (axis_idx_for_jno[jno] == -1) break; //fini
        if (axis_idx_for_jno[jno] != jno) { show++; } //not default order
    }
    islathe = !strcasecmp(coordinates,"xz"); // no show if simple lathe
    if (show && !islathe) {
        rtapi_print("\nidentityKinematicsSetup: coordinates:%s\n", coordinates);
        char *p="XYZABCUVW";
        for (jno=0; jno<EMCMOT_MAX_JOINTS; jno++) {
            if (axis_idx_for_jno[jno] == -1) break; //fini
            rtapi_print("   Joint %d ==> Axis %c\n",
                       jno,*(p+axis_idx_for_jno[jno]));
        }
        if (kinematicsType() != KINEMATICS_BOTH) {
            rtapi_print("identityKinematicsSetup: Recommend: kinstype=both\n");
        }
        rtapi_print("\n");
    }

    identity_kinematics_inited = 1;
    return 0;
} // identityKinematicsSetup()

int identityKinematicsForward(const double *joints,
                              EmcPose * pos,
                              const KINEMATICS_FORWARD_FLAGS * fflags,
                              KINEMATICS_INVERSE_FLAGS * iflags)
{
    if (!identity_kinematics_inited) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "identityKinematicsForward: not initialized\n");
        return -1;
    }

    if (JX >= 0) pos->tran.x = joints[JX];
    if (JY >= 0) pos->tran.y = joints[JY];
    if (JZ >= 0) pos->tran.z = joints[JZ];
    if (JA >= 0) pos->a      = joints[JA];
    if (JB >= 0) pos->b      = joints[JB];
    if (JC >= 0) pos->c      = joints[JC];
    if (JU >= 0) pos->u      = joints[JU];
    if (JV >= 0) pos->v      = joints[JV];
    if (JW >= 0) pos->w      = joints[JW];
    return 0;
} // identityKinematicsForward()

int identityKinematicsInverse(const EmcPose * pos,
                              double *joints,
                              const KINEMATICS_INVERSE_FLAGS * iflags,
                              KINEMATICS_FORWARD_FLAGS * fflags)
{
    if (!identity_kinematics_inited) {
        rtapi_print_msg(RTAPI_MSG_ERR,
            "identityKinematicsInverse: not initialized\n");
        return -1;
    }

    if (JX >= 0) joints[JX] = pos->tran.x;
    if (JY >= 0) joints[JY] = pos->tran.y;
    if (JZ >= 0) joints[JZ] = pos->tran.z;
    if (JA >= 0) joints[JA] = pos->a;
    if (JB >= 0) joints[JB] = pos->b;
    if (JC >= 0) joints[JC] = pos->c;
    if (JU >= 0) joints[JU] = pos->u;
    if (JV >= 0) joints[JV] = pos->v;
    if (JW >= 0) joints[JW] = pos->w;
    return 0;
} // identityKinematicsInverse()
