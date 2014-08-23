//    Copyright (C) 2009-2010 Jeff Epler <jepler@unpythonic.net>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "emcglb.h"
#include "emctool.h"
#include "tool_parse.h"

static bool scan_old_style(
	char *buffer,
	CANON_TOOL_TABLE toolTable[],
	int fms[],
	char *ttcomments[],
	int random_toolchanger,
	int &fakepocket) {
    int scanned, toolno, pocket, orientation;
    double zoffset, xoffset, diameter, frontangle, backangle;
    char comment[CANON_TOOL_ENTRY_LEN];

    if((scanned = sscanf(buffer, "%d %d %lf %lf %lf %lf %lf %d %[^\n]",
			 &toolno, &pocket, &zoffset, &xoffset, &diameter,
			 &frontangle, &backangle, &orientation, comment)) &&
       (scanned == 8 || scanned == 9)) {
	if(!random_toolchanger) {
	    fakepocket++;
	    if(fakepocket >= CANON_POCKETS_MAX) {
		printf("too many tools. skipping tool %d\n", toolno);
		return true;
	    }
	    if(fms) fms[fakepocket] = pocket;
	    pocket = fakepocket;
	}
	if (pocket < 0 || pocket >= CANON_POCKETS_MAX) {
	    printf("max pocket number is %d. skipping tool %d\n", CANON_POCKETS_MAX-1, toolno);
	    return true;
	} else {
	    /* lathe tool */
	    toolTable[pocket].toolno = toolno;
	    toolTable[pocket].offset.tran.z = zoffset;
	    toolTable[pocket].offset.tran.x = xoffset;
	    toolTable[pocket].diameter = diameter;

	    toolTable[pocket].frontangle = frontangle;
	    toolTable[pocket].backangle = backangle;
	    toolTable[pocket].orientation = orientation;
	    if(ttcomments && scanned == 9) strcpy(ttcomments[pocket], comment);
	    return true;
	}
    } else if ((scanned = sscanf(buffer, "%d %d %lf %lf %[^\n]",
				 &toolno, &pocket, &zoffset, &diameter, comment)) &&
	       (scanned == 4 || scanned == 5)) {
	if(!random_toolchanger) {
	    fakepocket++;
	    if(fakepocket >= CANON_POCKETS_MAX) {
		printf("too many tools. skipping tool %d\n", toolno);
		return true;
	    }
	    if(fms) fms[fakepocket] = pocket;
	    pocket = fakepocket;
	}
	if (pocket < 0 || pocket >= CANON_POCKETS_MAX) {
	    printf("max pocket number is %d. skipping tool %d\n", CANON_POCKETS_MAX-1, toolno);
	    return true;
	} else {
	    /* mill tool */
	    toolTable[pocket].toolno = toolno;
	    toolTable[pocket].offset.tran.z = zoffset;
	    toolTable[pocket].diameter = diameter;

	    // these aren't used on a mill
	    toolTable[pocket].frontangle = toolTable[pocket].backangle = 0.0;
	    toolTable[pocket].offset.tran.x = 0.0;
	    toolTable[pocket].orientation = 0;
	    if(ttcomments && scanned == 5) strcpy(ttcomments[pocket], comment);
	    return true;
	}
    }
    return false;
}

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
    char orig_line[CANON_TOOL_ENTRY_LEN];
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
        int toolno, orientation, valid = 1;
        EmcPose offset;  // tlo
        double diameter, frontangle, backangle;

        // for nonrandom machines, just read the tools into pockets 1..n
        // no matter their tool numbers.  NB leave the spindle pocket 0
        // unchanged/empty.

        if (NULL == fgets(buffer, CANON_TOOL_ENTRY_LEN, fp)) {
            break;
        }
        strcpy(orig_line, buffer);

        if(scan_old_style(buffer, toolTable, fms, ttcomments,
                                random_toolchanger, fakepocket)) continue;

        toolno = -1;
        diameter = frontangle = backangle = 0.0;
        orientation = 0;
        ZERO_EMC_POSE(offset);
        buff = strtok(buffer, ";");
        comment = strtok(NULL, "\n");

        token = strtok(buff, " ");
        while (token != NULL) {
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
        } else {
            fprintf(stderr, "Unrecognized line skipped: %s", orig_line);
        }
        if (!random_toolchanger && toolTable[0].toolno == toolTable[pocket].toolno) {
            toolTable[0] = toolTable[pocket];
        }
    }

    // close the file
    fclose(fp);

    return 0;
}
