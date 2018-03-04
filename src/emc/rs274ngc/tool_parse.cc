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
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "emcglb.h"
#include "emctool.h"
#include "tool_parse.h"


/********************************************************************
*
* Description: saveToolTable(const char *filename, CANON_TOOL_TABLE toolTable[])
*		Saves the tool table from toolTable[] array into file filename.
*		  Array is CANON_TOOL_MAX + 1 entries, since 0 is included.
*
* Return Value: Zero on success or -1 if file not found.
*
* Side Effects: Default setting used if the parameter not found in
*		the ini file.
*
* Called By: ioControl
*
********************************************************************/
int saveToolTable(const char *filename,
	CANON_TOOL_TABLE toolTable[],
	char *ttcomments[CANON_POCKETS_MAX],
	int random_toolchanger)
{
    int pocket;
    FILE *fp;
    const char *name;
    int start_pocket;

    // check filename
    if (filename[0] == 0) {
	name = tool_table_file;
    } else {
	// point to name provided
	name = filename;
    }

    // open tool table file
    if (NULL == (fp = fopen(name, "w"))) {
	// can't open file
	return -1;
    }

    if(random_toolchanger) {
        start_pocket = 0;
    } else {
        start_pocket = 1;
    }
    for (pocket = start_pocket; pocket < CANON_POCKETS_MAX; pocket++) {
        if (toolTable[pocket].toolno != -1) {
            fprintf(fp, "T%d P%d", toolTable[pocket].toolno, random_toolchanger? pocket: toolTable[pocket].pocketno);
            if (toolTable[pocket].diameter) fprintf(fp, " D%f", toolTable[pocket].diameter);
            if (toolTable[pocket].offset.tran.x) fprintf(fp, " X%+f", toolTable[pocket].offset.tran.x);
            if (toolTable[pocket].offset.tran.y) fprintf(fp, " Y%+f", toolTable[pocket].offset.tran.y);
            if (toolTable[pocket].offset.tran.z) fprintf(fp, " Z%+f", toolTable[pocket].offset.tran.z);
            if (toolTable[pocket].offset.a) fprintf(fp, " A%+f", toolTable[pocket].offset.a);
            if (toolTable[pocket].offset.b) fprintf(fp, " B%+f", toolTable[pocket].offset.b);
            if (toolTable[pocket].offset.c) fprintf(fp, " C%+f", toolTable[pocket].offset.c);
            if (toolTable[pocket].offset.u) fprintf(fp, " U%+f", toolTable[pocket].offset.u);
            if (toolTable[pocket].offset.v) fprintf(fp, " V%+f", toolTable[pocket].offset.v);
            if (toolTable[pocket].offset.w) fprintf(fp, " W%+f", toolTable[pocket].offset.w);
            if (toolTable[pocket].frontangle) fprintf(fp, " I%+f", toolTable[pocket].frontangle);
            if (toolTable[pocket].backangle) fprintf(fp, " J%+f", toolTable[pocket].backangle);
            if (toolTable[pocket].orientation) fprintf(fp, " Q%d", toolTable[pocket].orientation);
            fprintf(fp, " ;%s\n", ttcomments[pocket]);
        }
    }

    fclose(fp);
    return 0;
}

int loadToolTable(const char *filename,
			 CANON_TOOL_TABLE toolTable[],
			 char *ttcomments[],
			 int random_toolchanger)
{
    int fakepocket = 0;
    int realpocket = 0;
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
        toolTable[t].pocketno = -1;
        ZERO_EMC_POSE(toolTable[t].offset);
        toolTable[t].diameter = 0.0;
        toolTable[t].frontangle = 0.0;
        toolTable[t].backangle = 0.0;
        toolTable[t].orientation = 0;
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
                realpocket = pocket;
                if (!random_toolchanger) {
                    fakepocket++;
                    if (fakepocket >= CANON_POCKETS_MAX) {
                        printf("too many tools. skipping tool %d\n", toolno);
                        valid = 0;
                        break;
                    }
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
            toolTable[pocket].pocketno = realpocket;
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
