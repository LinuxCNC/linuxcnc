#include "rtapi.h"
#include "motion.h"

/* Utility routines for kinematics modules
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
int map_coordinates_to_jnumbers(char *coordinates,
                                int  max_joints,
                                int  allow_duplicates,
                                int  axis_idx_for_jno[] ) //result
{
    char* errtag="map_coordinates_to_jnumbers: ERROR:\n  ";
    int  jno=0;
    int  found=0;
    int  dups[EMCMOT_MAX_AXIS] = {0};
    char *coords = coordinates;
    char coord_letter[] = {'X','Y','Z','A','B','C','U','V','W'};

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
