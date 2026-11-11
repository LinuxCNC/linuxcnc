/*
** This file is part of a refactor of internal  tool data management
** and incorporates work from removed files:
**     src/emc/ini/initool.hh
**     src/emc/rs274ngc/tool_parse.h
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
#ifndef TOOL_DATA_H
#define TOOL_DATA_H

#include "nml_intf/canon.hh"
#include "nml_intf/emc_nml.hh"

#ifdef __cplusplus
extern"C" {
#endif

typedef enum {
    IDX_OK = 0,
    IDX_NEW,
    IDX_FAIL,
} toolidx_t;

typedef enum {
    DB_NOTUSED=0,  // equivalent to not specifying in inifile
    DB_ACTIVE,
} tooldb_t;

typedef enum {
    SPINDLE_LOAD,
    SPINDLE_UNLOAD,
    TOOL_OFFSET,
} tool_notify_t;
//----------------------------------------------------------
// tooldata_*(): access to internal tool table data:
struct    CANON_TOOL_TABLE tooldata_entry_init(void);
toolidx_t tooldata_put(struct CANON_TOOL_TABLE tdata,int idx);
toolidx_t tooldata_get(CANON_TOOL_TABLE* pdata,int idx);

void   tooldata_init(bool random_tool_changer);
void   tooldata_reset(void);
void   tooldata_last_index_set(int idx);
int    tooldata_last_index_get(void);
int    tooldata_find_index_for_tool(int toolno);

// ignore_zero_values:1 for file writes
//                   :0 for use with tooldata_db_notify()
void   tooldata_format_toolline (int idx,
                                 bool ignore_zero_values,
                                 CANON_TOOL_TABLE tdata,
                                 char formatted_line[CANON_TOOL_ENTRY_LEN]
                                 );

// Parse one line of a tool table file into *tdata, without storing it.
// Returns 1 for an entry, 0 for a line that holds none (blank or comment)
// and -1 for a line that is not understood. pocketno is the P word, 0 if
// there is none; *has_pocket (may be NULL) tells the two apart, and is set
// as soon as a P word is read, also when the line is rejected later.
int    tooldata_parse_line(const char *input_line, CANON_TOOL_TABLE *tdata,
                           bool *has_pocket);

//----------------------------------------------------------
//mmap specific
void tool_mmap_set_fname(const char* fname);
int  tool_mmap_creator(EMC_TOOL_STAT const  *ptr,int random_toolchanger);
int  tool_mmap_user(void);
void tool_mmap_close(void);
bool tool_mmap_is_random_toolchanger(void);

//----------------------------------------------------------
//nml specific
int tool_nml_register(CANON_TOOL_TABLE *tblptr);

//----------------------------------------------------------
// database interface

// tool table file for tool in spindle
// (preserved on normal exit for random_toolchanger)
#define DB_SPINDLE_SAVE "./db_spindle.tbl"

int   tooldata_db_init(char db_find_progname[],int random_toolchanger);
int   tooldata_db_notify(tool_notify_t ntype,
                         int toolno,
                         int pocketno,
                         CANON_TOOL_TABLE tdata);
// Ask the database program for all tools: `reset` is called once the
// request is sent, then `line` for every tool line; a negative return from
// it ends the reading with an error.
int   tooldata_db_getall(void (*reset)(void *arg),
                         int (*line)(const char *toolline, void *arg),
                         void *arg);
#ifdef __cplusplus
}

#endif
#endif
