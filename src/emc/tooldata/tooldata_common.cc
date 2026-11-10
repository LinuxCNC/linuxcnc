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
static int      nonrandom_idx = 0; // 'fakepocket' counter
static bool     add_init_initialized = 0;
static tooldb_t db_mode = DB_NOTUSED;

void tooldata_init(bool random_toolchanger)
{
    is_random_toolchanger = random_toolchanger;
} // tooldata_init

struct CANON_TOOL_TABLE tooldata_entry_init()
{
    struct CANON_TOOL_TABLE tdata = CANON_TOOL_TABLE_INIT;
    return tdata;
} // tooldata_entry_init()

struct CANON_TOOL_TABLE tooldata_get_tool(int toolno)
{
    CANON_TOOL_TABLE tdata = tooldata_entry_init();
    int idx = tooldata_find_index_for_tool(toolno);
    if (idx < 0 || tooldata_get(&tdata,idx) != IDX_OK) {
        return tooldata_entry_init();
    }
    // a non-random changer finds tool 0 at the spindle's index, which may
    // hold the loaded tool's copy
    if (tdata.toolno != toolno) {
        tdata = tooldata_entry_init();
        tdata.toolno = 0;
        tdata.pocketno = 0;
    }
    return tdata;
} // tooldata_get_tool()

void tooldata_set_db(tooldb_t mode)
{
    db_mode = mode;
    return;
} //tooldata_set_db()

void tooldata_add_init(int nonrandom_start_idx)
{
    // initialize file-static vars need for tooldata_add_line()
    nonrandom_idx        = nonrandom_start_idx;
    add_init_initialized = 1;
    return;
} // tooldata_add_init()

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

int tooldata_read_entry(const char *input_line)
{
    CANON_TOOL_TABLE tdata;
    bool has_pocket;
    int idx = 0;

    if (!add_init_initialized) {
        fprintf(stderr,"!!! PROBLEM no init %s\n",__FILE__);
        return -1;
    }
    int parsed = tooldata_parse_line(input_line, &tdata, &has_pocket);
    if (parsed == 0) {return 0;}

    // The index the entry is stored at: the pocket on a random changer,
    // the next free one otherwise. A line that turns out to be unusable
    // takes its index all the same.
    if (has_pocket) {
        idx = tdata.pocketno; //random toolchanger
        if (!is_random_toolchanger) {
            if (   (nonrandom_idx <= 0)
                || (nonrandom_idx >= CANON_POCKETS_MAX) ) {
                printf("Out-of-range nonrandom_idx=%d. Skipping tool=%d\n",
                       nonrandom_idx,tdata.toolno);
                return -1;
            }
            idx = nonrandom_idx; // nonrandom toolchanger
            (nonrandom_idx)++;
        }
        if (idx < 0 || idx >= CANON_POCKETS_MAX) {
            printf("max pocket number is %d. skipping tool %d\n",
                   CANON_POCKETS_MAX - 1, tdata.toolno);
            return -1;
        }
    }
    if (parsed < 0) {return -1;}

    // verify no prior tool in pocket
    CANON_TOOL_TABLE prior = tooldata_entry_init();
    if (tooldata_get(&prior,idx) != IDX_OK) { UNEXPECTED_MSG; }
    if (is_random_toolchanger && prior.toolno != -1) {
        fprintf(stderr,"WARNING: Attempt to assign multiple toolno.s to pocket %d\n",tdata.pocketno);
        fprintf(stderr,"         %s %s()\n",__FILE__,__FUNCTION__);
        fprintf(stderr,"    WAS: pocket=%3d toolno=%3d\n",prior.pocketno,prior.toolno);
        fprintf(stderr,"     IS: pocket=%3d toolno=%3d\n",tdata.pocketno,tdata.toolno);
    }
    if (!tdata.comment[0]) {
        // a line without a comment leaves the one the index has
        memcpy(tdata.comment,prior.comment,sizeof(tdata.comment));
    }
    if (tooldata_put(tdata,idx) == IDX_FAIL) {
        UNEXPECTED_MSG;
    }
    return idx;
} // tooldata_read_entry()

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

int tooldata_load(const char *filename)
{
    FILE *fp;
    char input_line[CANON_TOOL_ENTRY_LEN];
    char orig_line[CANON_TOOL_ENTRY_LEN];

    if (db_mode == DB_ACTIVE) { //expect data loaded from db
         if (tooldata_db_getall()) {
            fprintf(stderr,
              "\ntooldata_load: Failed to load tooldata from database\n\n");
            db_mode = DB_NOTUSED;
            return -1;
         }
         return 0;
    }

    if(!filename) return -1;

    // clear out tool table
    // (Set vars to indicate no tool in pocket):
    tooldata_reset();

    // open tool table file
    if (NULL == (fp = fopen(filename, "r"))) {
        fprintf(stderr, "%s, %d: Failed to open tool table file '%s': %s\n",
                __FILE__, __LINE__, filename, strerror(errno));
        return -1;
    }

    // after initializing all available pockets,
    // subsequent read from filename will update the last_index
    // which becomes available by tooldata_last_index_get()
    tooldata_last_index_set(0); // (only mmap uses)

    const int nonrandom_start_idx = 1; // when reading file start at 0
    tooldata_add_init(nonrandom_start_idx);

    while (!feof(fp)) {
        // for nonrandom machines, just read the tools into pockets 1..n
        // no matter their tool numbers.  NB leave the spindle pocket 0
        // unchanged/empty.
        if (NULL == fgets(input_line, CANON_TOOL_ENTRY_LEN, fp)) {
            break;
        }
        strcpy(orig_line, input_line);

        // parse and store one line from tool table file
        int entry_idx = tooldata_read_entry(input_line);
        if (entry_idx <0) {
            printf("File: %s Unrecognized line skipped:\n    %s",filename, orig_line);
            continue;
        }

        if (!is_random_toolchanger) {
            CANON_TOOL_TABLE spindletool;
            if (tooldata_get(&spindletool,0) != IDX_OK) {
                continue;
            }
            CANON_TOOL_TABLE edata;  // just added
            if (tooldata_get(&edata,entry_idx) != IDX_OK) {
                UNEXPECTED_MSG;
            }
            if (!is_random_toolchanger && spindletool.toolno == edata.toolno) {
                spindletool = edata;
                tooldata_put(spindletool,0);
            }
        }
    } // while

    // close the file
    fclose(fp);

    return 0;
} // tooldata_load()

static void write_tool_line(FILE* fp,int idx)
{
    CANON_TOOL_TABLE tdata;
    if (tooldata_get(&tdata,idx) != IDX_OK) {return;}
    if (db_mode == DB_ACTIVE && idx != 0 ) {
        return;
    }

    if (tdata.toolno != -1) {
        char theline[CANON_TOOL_ENTRY_LEN] = {0};
        tooldata_format_toolline (idx,
                                  1, // ignore_zero_values
                                  tdata,theline);
        fprintf(fp,"%s",theline);
    }
    return;
} // write_tool_line()

int tooldata_save(const char *filename)
{
    int idx;
    FILE *fp;
    int start_idx;

    if (db_mode == DB_ACTIVE) {
        if (!is_random_toolchanger) {return 0;}
        filename = DB_SPINDLE_SAVE; //one entry tbl (nonran only)
    } else {
        // filename == NULL happens without ini entry [EMCIO]TOOL_TABLE
        // and a Task::emcToolSetOffset is performed.
        if (!filename) {
            fprintf(stderr, "%s: No filename. Are you missing INI-entry [EMCIO]TOOL_TABLE?\n",
                    __PRETTY_FUNCTION__);
            return -1;
        }
        if (filename[0] == 0) {
            UNEXPECTED_MSG;
        }
    }

    // open tool table file
    if (NULL == (fp = fopen(filename, "w"))) {
        fprintf(stderr, "%s, %d: Failed to open tool table file '%s': %s\n",
                __FILE__, __LINE__, filename, strerror(errno));
        return -1;
    }

    if (db_mode == DB_ACTIVE) {
        int spindle_idx = 0;
        write_tool_line(fp,spindle_idx);
    } else {
        start_idx = is_random_toolchanger ? 0 : 1;
        for (idx = start_idx; idx < CANON_POCKETS_MAX; idx++) {
            write_tool_line(fp,idx);
        }
    }
    fclose(fp);
    return 0;
} //tooldata_save()
