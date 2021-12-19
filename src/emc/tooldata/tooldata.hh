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

#include "canon.hh"
#include "emc_nml.hh"

#ifdef CPLUSPLUS
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
                                 char * ttcomments[],
                                 char formatted_line[CANON_TOOL_ENTRY_LEN]
                                 );

void   tooldata_add_init(int nonrandom_start_idx);
int    tooldata_read_entry(const char *input_line,
                           char *ttcomments[]);

void   tooldata_set_db(tooldb_t mode);

//----------------------------------------------------------
int tooldata_load(const char *filename,
                  char *ttcomments[CANON_POCKETS_MAX]);

int tooldata_save(const char *filename,
                  char *ttcomments[CANON_POCKETS_MAX]);

//----------------------------------------------------------
//mmap specific
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
int   tooldata_db_getall(void);
#ifdef CPLUSPLUS
}

#endif
#endif
