/*
** This file is part of a refactor of internal  tool data management
** and incorporates work from removed files:
**     src/emc/ini/initool.cc
**     src/emc/rs274ngc/tool_parse.cc
**
** Copyright: 2021
** Author:    Dewey Garrett <dgarrett@panix.com>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "tooldata.hh"

#define UNEXPECTED_MSG fprintf(stderr,"UNEXPECTED %s %d\n",__FILE__,__LINE__);

static bool     is_random_toolchanger = 0;

void tooldata_init(bool random_toolchanger)
{
    is_random_toolchanger = random_toolchanger;
} // tooldata_init

struct CANON_TOOL_TABLE tooldata_entry_init()
{
    struct CANON_TOOL_TABLE tdata = CANON_TOOL_TABLE_INIT;
    return tdata;
} // tooldata_entry_init()


int tooldata_parse_line(const char *input_line, CANON_TOOL_TABLE *tdata,
                        bool *has_pocket)
{
    char work_line[CANON_TOOL_ENTRY_LEN];
    const char *token;
    char *buff, *comment;
    int valid=1;

    *tdata = tooldata_entry_init();
    tdata->pocketno = 0; // a line without a P word
    if (has_pocket) {*has_pocket = false;}

    if (input_line[0] == ';') {return 0;} //ignore leading ';'
    strncpy(work_line, input_line, sizeof(work_line)-1);
    work_line[sizeof(work_line)-1] = 0;

    char* saveptr;
    buff = strtok_r(work_line, ";", &saveptr);
    if (!buff || strlen(buff) <=1) {
        //fprintf(stderr,"skip blankline %s\n",__FILE__);
        return 0;
    }
    // an indented comment, or a line of blanks
    if (strspn(buff, " \t\r\n") == strlen(buff)) {return 0;}
    comment = strtok_r(NULL, "\n", &saveptr);

    token = strtok_r(buff, " ", &saveptr);
    while (token != NULL) {
        switch (toupper(token[0])) {
        case 'T':
            if (sscanf(&token[1], "%d", &tdata->toolno) != 1)
                valid = 0;
            break;
        case 'P':
            if (sscanf(&token[1], "%d", &tdata->pocketno) != 1) {
                valid = 0;
                break;
            }
            if (has_pocket) {*has_pocket = true;}
            break;
        case 'D':
            if (sscanf(&token[1], "%lf", &tdata->diameter) != 1)
                valid = 0;
            break;
        case 'X':
            if (sscanf(&token[1], "%lf", &tdata->offset.tran.x) != 1)
                valid = 0;
            break;
        case 'Y':
            if (sscanf(&token[1], "%lf", &tdata->offset.tran.y) != 1)
                valid = 0;
            break;
        case 'Z':
            if (sscanf(&token[1], "%lf", &tdata->offset.tran.z) != 1)
                valid = 0;
            break;
        case 'A':
            if (sscanf(&token[1], "%lf", &tdata->offset.a) != 1)
                valid = 0;
            break;
        case 'B':
            if (sscanf(&token[1], "%lf", &tdata->offset.b) != 1)
                valid = 0;
            break;
        case 'C':
            if (sscanf(&token[1], "%lf", &tdata->offset.c) != 1)
                valid = 0;
            break;
        case 'U':
            if (sscanf(&token[1], "%lf", &tdata->offset.u) != 1)
                valid = 0;
            break;
        case 'V':
            if (sscanf(&token[1], "%lf", &tdata->offset.v) != 1)
                valid = 0;
            break;
        case 'W':
            if (sscanf(&token[1], "%lf", &tdata->offset.w) != 1)
                valid = 0;
            break;
        case 'I':
            if (sscanf(&token[1], "%lf", &tdata->frontangle) != 1)
                valid = 0;
            break;
        case 'J':
            if (sscanf(&token[1], "%lf", &tdata->backangle) != 1)
                valid = 0;
            break;
        case 'Q':
            if (sscanf(&token[1], "%d", &tdata->orientation) != 1)
                valid = 0;
            break;
        default:
            if (strncmp(token, "\n", 1) != 0)
                valid = 0;
            break;
        }
        token = strtok_r(NULL, " ", &saveptr);
    } // while token

    if (!valid) {return -1;}

    if (comment) {
        strncpy(tdata->comment,comment,CANON_TOOL_COMMENT_SIZE-1);
        tdata->comment[CANON_TOOL_COMMENT_SIZE-1] = 0;
        if (strlen(comment) > (CANON_TOOL_COMMENT_SIZE-1) ) {
            fprintf(stderr,"%s():comment for toolno %d truncated to %d chars:\n   <%s>\n"
                   ,__FUNCTION__
                   ,tdata->toolno
                   ,CANON_TOOL_COMMENT_SIZE-1
                   ,tdata->comment
                   );
        }
    }
    return 1;
} // tooldata_parse_line()


void tooldata_format_toolline (int idx,
                               bool ignore_zero_values,
                               CANON_TOOL_TABLE tdata,
                               char formatted_line[CANON_TOOL_ENTRY_LEN]
                               )
{
    char tmp[CANON_TOOL_ENTRY_LEN-1] = {0};
    int space = sizeof(tmp);
    int len;
    len = snprintf(tmp,sizeof(tmp),"T%-3d P%-3d"
            ,tdata.toolno
            ,is_random_toolchanger ? idx : tdata.pocketno);
    strncat(formatted_line,tmp,space);
    space -= len;
// format zero float values as %.0f for brevity
#define F_ITEM(item,letter) if (!ignore_zero_values || tdata.item) { \
                                char local_tmp[64] = {}; \
                                if (tdata.item) { \
                                    len = snprintf(local_tmp,sizeof(local_tmp)," " letter "%+f", tdata.item); \
                                } else { \
                                    len = snprintf(local_tmp,sizeof(local_tmp)," " letter "%.0f",tdata.item); \
                                } \
                                strncat(formatted_line,local_tmp,space); \
                                space -= len; \
                            }
#define I_ITEM(item,letter) if (!ignore_zero_values || tdata.item) { \
                                len = snprintf(tmp,sizeof(tmp)," " letter "%d",tdata.item); \
                                strncat(formatted_line,tmp,space); \
                                space -= len; \
                            }

    F_ITEM(diameter,       "D");
    F_ITEM(offset.tran.x,  "X");
    F_ITEM(offset.tran.y,  "Y");
    F_ITEM(offset.tran.z,  "Z");
    F_ITEM(offset.a,       "A");
    F_ITEM(offset.b,       "B");
    F_ITEM(offset.c,       "C");
    F_ITEM(offset.u,       "U");
    F_ITEM(offset.v,       "V");
    F_ITEM(offset.w,       "W");
    F_ITEM(frontangle,     "I");
    F_ITEM(backangle,      "J");
    I_ITEM(orientation,    "Q");
#undef F_ITEM
#undef I_ITEM
    // Stop tracking len and space here for current tool table format
    if (tdata.comment[0]) {
        snprintf(tmp,space," ;%s\n",tdata.comment);
    } else {
        snprintf(tmp,space,"\n");
    }
    strncat(formatted_line,tmp,space);
    return;
} // tooldata_format_toolline()

