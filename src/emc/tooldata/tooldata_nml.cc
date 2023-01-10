/*
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

#define UNEXPECTED_MSG fprintf(stderr,"UNEXPECTED %s %d\n",__FILE__,__LINE__);

#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include "tooldata.hh"

static CANON_TOOL_TABLE *the_table;

int tool_nml_register(CANON_TOOL_TABLE *tblptr)
{
    if (!tblptr) {
        fprintf(stderr,"%5d!!!PROBLEM tool_nml_register()\n",getpid());
        return -1;
    }
    the_table = tblptr;
    return 0;
} //tool_nml_register

void tooldata_last_index_set(int idx)
{
    return; //not used with nml
} //tooldata_last_index_set()

int tooldata_last_index_get(void)
{
    return CANON_POCKETS_MAX -1;
} // tooldata_last_index_get()


toolidx_t tooldata_put(CANON_TOOL_TABLE from,int idx)
{
    if (!the_table) {
        fprintf(stderr,"%5d!!!tool_nml_put() NOT INITIALIZED\n",getpid());
        return IDX_FAIL;
    }
    if (idx < 0 || idx >= CANON_POCKETS_MAX) {
        UNEXPECTED_MSG;
        return IDX_FAIL;
    }
    *(the_table + idx) = from;
    return IDX_OK;
} // tooldata_put()

void tooldata_reset()
{
    CANON_TOOL_TABLE initdata = tooldata_entry_init();
    int idx;
    for (idx = 0; idx < CANON_POCKETS_MAX; idx++) {
        *(the_table + idx) = initdata;
    }
} // tooldata_reset()

toolidx_t tooldata_get(CANON_TOOL_TABLE* pdata,int idx)
{
    if (idx < 0 || idx >= CANON_POCKETS_MAX) {
        fprintf(stderr,"!!!%5d PROBLEM tooldata_get(): idx=%d, maxallowed=%d\n",
                getpid(),idx,CANON_POCKETS_MAX-1);
        return IDX_FAIL;
    }

    if (!the_table) {
        fprintf(stderr,"!!!%5d PROBLEM tooldata_get(): idx=%d the_table=%p\n",
                getpid(),idx,the_table);
        return IDX_FAIL;
    }

    *pdata = *(struct CANON_TOOL_TABLE*)(the_table + idx);
    return IDX_OK;
} // tooldata_get()

int tooldata_find_index_for_tool(int toolno)
{
    int foundidx = -1;
    if (toolno == -1) {return -1;}
    for(int idx = 0;idx < CANON_POCKETS_MAX;idx++){
        if ((the_table+idx)->toolno == toolno) {
            foundidx = idx;
            if (foundidx==0) continue;
            break;
        }
    }
    return foundidx;
} //tooldata_find_index_for_tool()
