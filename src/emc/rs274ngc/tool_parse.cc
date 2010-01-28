#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "emcglb.h"
#include "emctool.h"
#include "tool_parse.h"

int loadToolTable(const char *filename,
			 CANON_TOOL_TABLE toolTable[],
			 int fms[],
			 char *ttcomments[],
			 int random_toolchanger)
{
    int fakepocket = 0;
    int t;
    FILE *fp;
    char buffer[CANON_TOOL_ENTRY_LEN];
    int pocket = 0;

    if(!filename) return -1;

    // open tool table file
    if (NULL == (fp = fopen(filename, "r"))) {
	// can't open file
	return -1;
    }
    // clear out tool table
    for (t = random_toolchanger? 0: 1; t < CANON_POCKETS_MAX; t++) {
        toolTable[t].toolno = -1;
        ZERO_EMC_POSE(toolTable[t].offset);
        toolTable[t].diameter = 0.0;
        toolTable[t].frontangle = 0.0;
        toolTable[t].backangle = 0.0;
        toolTable[t].orientation = 0;
        if(fms) fms[t] = 0;
        if(ttcomments) ttcomments[t][0] = '\0';
    }

    /*
      Override 0's with codes from tool file
      File format is:

      <header>
      <pocket # 0..CANON_TOOL_MAX> <FMS id> <length> <diameter>
      ...

    */

    while (!feof(fp)) {
        const char *token;
        char *buff, *comment;
        int toolno, orientation, valid = 0;
        EmcPose offset;  // tlo
        double diameter, frontangle, backangle;

        // for nonrandom machines, just read the tools into pockets 1..n
        // no matter their tool numbers.  NB leave the spindle pocket 0
        // unchanged/empty.

        if (NULL == fgets(buffer, CANON_TOOL_ENTRY_LEN, fp)) {
            break;
        }

        toolno = -1;
        diameter = frontangle = backangle = 0.0;
        orientation = 0;
        ZERO_EMC_POSE(offset);
        buff = strtok(buffer, ";");
        comment = strtok(NULL, "\n");

        token = strtok(buff, " ");
        while (token != NULL) {
            valid = 1;
            switch (toupper(token[0])) {
            case 'T':
                if (sscanf(&token[1], "%d", &toolno) != 1)
                    valid = 0;
                break;
            case 'P':
                if (sscanf(&token[1], "%d", &pocket) != 1) {
                    valid = 0;
                    break;
                }
                if (!random_toolchanger) {
                    fakepocket++;
                    if (fakepocket >= CANON_POCKETS_MAX) {
                        printf("too many tools. skipping tool %d\n", toolno);
                        valid = 0;
                        break;
                    }
                    if(fms) fms[fakepocket] = pocket;
                    pocket = fakepocket;
                }
                if (pocket < 0 || pocket >= CANON_POCKETS_MAX) {
                    printf("max pocket number is %d. skipping tool %d\n", CANON_POCKETS_MAX - 1, toolno);
                    valid = 0;
                    break;
                }
                break;
            case 'D':
                if (sscanf(&token[1], "%lf", &diameter) != 1)
                    valid = 0;
                break;
            case 'X':
                if (sscanf(&token[1], "%lf", &offset.tran.x) != 1)
                    valid = 0;
                break;
            case 'Y':
                if (sscanf(&token[1], "%lf", &offset.tran.y) != 1)
                    valid = 0;
                break;
            case 'Z':
                if (sscanf(&token[1], "%lf", &offset.tran.z) != 1)
                    valid = 0;
                break;
            case 'A':
                if (sscanf(&token[1], "%lf", &offset.a) != 1)
                    valid = 0;
                break;
            case 'B':
                if (sscanf(&token[1], "%lf", &offset.b) != 1)
                    valid = 0;
                break;
            case 'C':
                if (sscanf(&token[1], "%lf", &offset.c) != 1)
                    valid = 0;
                break;
            case 'U':
                if (sscanf(&token[1], "%lf", &offset.u) != 1)
                    valid = 0;
                break;
            case 'V':
                if (sscanf(&token[1], "%lf", &offset.v) != 1)
                    valid = 0;
                break;
            case 'W':
                if (sscanf(&token[1], "%lf", &offset.w) != 1)
                    valid = 0;
                break;
            case 'I':
                if (sscanf(&token[1], "%lf", &frontangle) != 1)
                    valid = 0;
                break;
            case 'J':
                if (sscanf(&token[1], "%lf", &backangle) != 1)
                    valid = 0;
                break;
            case 'Q':
                if (sscanf(&token[1], "%d", &orientation) != 1)
                    valid = 0;
                break;
            default:
                if (strncmp(token, "\n", 1) != 0)
                    valid = 0;
                break;
            }
            token = strtok(NULL, " ");
        }
        if (valid) {
            toolTable[pocket].toolno = toolno;
            toolTable[pocket].offset = offset;
            toolTable[pocket].diameter = diameter;
            toolTable[pocket].frontangle = frontangle;
            toolTable[pocket].backangle = backangle;
            toolTable[pocket].orientation = orientation;

            if (ttcomments && comment)
                strcpy(ttcomments[pocket], comment);
        }
        if (!random_toolchanger && toolTable[0].toolno == toolTable[pocket].toolno) {
            toolTable[0] = toolTable[pocket];
        }
    }

    // close the file
    fclose(fp);

    return 0;
}
