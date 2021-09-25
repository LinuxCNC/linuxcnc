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
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include "rtapi_mutex.h"
#include "tooldata.hh"

#define UNEXPECTED_MSG fprintf(stderr,"UNEXPECTED %s %d\n",__FILE__,__LINE__);

#define TOOL_MMAP_FILENAME ".tool.mmap"
#define TOOL_MMAP_MODE     0600
#define TOOL_MMAP_CREATOR_OPEN_FLAGS  O_RDWR | O_CREAT | O_TRUNC
#define TOOL_MMAP_USER_OPEN_FLAGS     O_RDWR

static int           creator_fd;
static char          filename[LINELEN] = {};
static char*         tool_mmap_base = 0;
static EMC_TOOL_STAT const *toolstat;

typedef struct {
    rtapi_mutex_t   mutex;
    unsigned int    last_index;
    int             is_random_toolchanger;
} tooldata_header_t;

/* mmap region:
**   1) header
**   2) CANON_TOOL_TABLE items (howmany=CANON_POCKETS_MAX)
*/

//---------------------------------------------------------------------
#define TOOL_MMAP_HEADER_OFFSET 0
#define TOOL_MMAP_HEADER_SIZE sizeof(tooldata_header_t)

#define TOOL_MMAP_SIZE    TOOL_MMAP_HEADER_SIZE + \
                          CANON_POCKETS_MAX * sizeof(struct CANON_TOOL_TABLE)

#define TOOL_MMAP_STRIDE  sizeof(CANON_TOOL_TABLE)
//---------------------------------------------------------------------
#define HPTR()    (tooldata_header_t*)( tool_mmap_base \
                                      + TOOL_MMAP_HEADER_OFFSET)

#define TPTR(idx) (CANON_TOOL_TABLE*)( tool_mmap_base \
                                     + TOOL_MMAP_HEADER_OFFSET \
                                     + TOOL_MMAP_HEADER_SIZE \
                                     + idx * TOOL_MMAP_STRIDE)
//---------------------------------------------------------------------
/* Note: emccfg.h defaults (seconds)
**       DEFAULT_EMC_TASK_CYCLE_TIME 0.100 (.001 common)
**       DEFAULT_EMC_IO_CYCLE_TIME   0.100
*/

static char* tool_mmap_fname(void) {
    if (*filename) {return filename;}
    char* hdir = secure_getenv("HOME");
    if (!hdir) {hdir = (char*)"/tmp";}
    snprintf(filename,sizeof(filename),"%s/%s",hdir,TOOL_MMAP_FILENAME);
    return(filename);
}

static int tool_mmap_mutex_get()
{
    tooldata_header_t *hptr = HPTR();
    useconds_t waited_us  =      0;
    useconds_t delta_us   =    100;
    useconds_t maxwait_us = 10*1e6; //10seconds
    bool try_failed = 0;
    while ( rtapi_mutex_try(&(hptr->mutex)) ) { //true==failed
        usleep(delta_us); waited_us += delta_us;
        // fprintf(stderr,"!!!%5d tool_mmap_mutex_get(): waited_us=%d\n"
        //        ,getpid(),waited_us);
        if (waited_us > maxwait_us) break;
    }
    if (waited_us > maxwait_us) {
        UNEXPECTED_MSG;
        fprintf(stderr,"tool_mmap_mutex_get:waited_us=%d delta_us=%d maxwait_us=%d\n\n",
                waited_us,delta_us,maxwait_us);
        rtapi_mutex_give(&(hptr->mutex)); // continue without mutex
        try_failed = 1;
    }
    if (try_failed) {return -1;}

    return 0;
} // tool_mmap_mutex_get()

static void tool_mmap_mutex_give()
{
    tooldata_header_t *hptr = HPTR();
    rtapi_mutex_give(&(hptr->mutex));
} // tool_mmap_mutex_give()

bool tool_mmap_is_random_toolchanger(void)
{
    bool ans = 0;
    tool_mmap_mutex_get();
    tooldata_header_t *hptr = HPTR();
    ans = hptr->is_random_toolchanger;
    tool_mmap_mutex_give();
    return ans;
}

//typ creator: emc/ioControl.cc, sai/driver.cc
//    (first applicable process started in linuxcnc script)
int tool_mmap_creator(EMC_TOOL_STAT const * ptr,int random_toolchanger)
{
    static int inited=0;

    if (inited) {
        fprintf(stderr,"Error: tool_mmap_creator already called BYE\n");
        exit(EXIT_FAILURE);
    }
    toolstat = ptr; //note NULL for sai
    creator_fd = open(tool_mmap_fname(),
                     TOOL_MMAP_CREATOR_OPEN_FLAGS,TOOL_MMAP_MODE);
    if (!creator_fd) {
        perror("tool_mmap_creator(): file open fail");
        exit(EXIT_FAILURE);
    }
    if (lseek(creator_fd, TOOL_MMAP_SIZE, SEEK_SET) == -1) {
        close(creator_fd);
        perror("tool_mmap_creator() lseek fail");
        exit(EXIT_FAILURE);
    }
    if (write(creator_fd, "\0", 1) < 0) {
        close(creator_fd);
        perror("tool_mmap_creator(): file tail write fail");
        exit(EXIT_FAILURE);
    }
    tool_mmap_base = (char*)mmap(0, TOOL_MMAP_SIZE, PROT_READ | PROT_WRITE,
                                 MAP_SHARED, creator_fd, 0);
    if (tool_mmap_base == MAP_FAILED) {
        close(creator_fd);
        perror("tool_mmap_creator(): mmap fail");
        exit(EXIT_FAILURE);
    }

    tooldata_header_t *hptr = HPTR();
    hptr->is_random_toolchanger = random_toolchanger;
    hptr->last_index = 0;

    inited = 1;
    tool_mmap_mutex_give(); return 0;
} // tool_mmap_creator();

//typ: milltask, guis (emcmodule,emcsh,...), halui
int tool_mmap_user()
{
    int fd = open(tool_mmap_fname(),
                  TOOL_MMAP_USER_OPEN_FLAGS, TOOL_MMAP_MODE);

    if (fd < 0) {
        perror("tool_mmap_user(): file open fail");
        /*
        ** For the LinuxCNC application, tool_mmap_creator()
        ** should start first and create the mmap file needed here.
        ** Related designer applications may use emcmodule.cc
        ** which calls this tool_mmap_user() but need to
        ** continue execution if no mmap file is open.
        ** So print message and return fail indicator.
        */
        fprintf(stderr,"tool_mmap_user(): no mmap file,continuing\n");
        tool_mmap_base = (char*)0;
        return(-1);
    }
    tool_mmap_base = (char*)mmap(0, TOOL_MMAP_SIZE, PROT_READ|PROT_WRITE,
                                 MAP_SHARED, fd, 0);

    if (tool_mmap_base == MAP_FAILED) {
        close(fd);
        perror("tool_mmap_user(): mmap fail");
        exit(EXIT_FAILURE);
    }
    return 0;
} //tool_mmap_user()

void tool_mmap_close()
{
    if (!tool_mmap_base) { return; }
    // flush mmapped file to filesystem
    if (msync(tool_mmap_base, TOOL_MMAP_SIZE, MS_SYNC) == -1) {
        perror("tool_mmap_close(): msync fail");
    }
    if (munmap(tool_mmap_base, TOOL_MMAP_SIZE) < 0) {
        close(creator_fd);
        perror("tool_mmap_close(): munmapfail");
        exit(EXIT_FAILURE);
    }
    if( unlink(tool_mmap_fname() )) {
        perror("tool_mmap_close(): unlink fail");
    }
    close(creator_fd);
} //tool_mmap_close()

void tooldata_last_index_set(int idx)  //force last_index
{
    tool_mmap_mutex_get();
    tooldata_header_t *hptr = HPTR();
    if (idx < 0 || idx >= CANON_POCKETS_MAX) {
        UNEXPECTED_MSG;
        idx = 0;
        fprintf(stderr,"!!!continuing using idx=%d\n",idx);
    }
    hptr->last_index = idx;
    tool_mmap_mutex_give(); return;
} //tooldata_last_index_set()

int tooldata_last_index_get(void)
{
    tool_mmap_mutex_get();
    tooldata_header_t *hptr = HPTR();
    if (tool_mmap_base) {
        tool_mmap_mutex_give(); return hptr->last_index;
    } else {
        tool_mmap_mutex_give(); return -1;
    }
} // tooldata_last_index_get()

toolidx_t tooldata_put(struct CANON_TOOL_TABLE tdata,int idx)
{
    toolidx_t ret;
    if (!tool_mmap_base) {
        fprintf(stderr,"%5d tooldata_put() no tool_mmap_base\n",getpid());
        return IDX_FAIL;
    }

    if (idx < 0 ||idx >= CANON_POCKETS_MAX) {
        UNEXPECTED_MSG;
        return IDX_FAIL;
    }
    if (tool_mmap_mutex_get()) {
        fprintf(stderr,"!!!%5d PROBLEM: tooldata_put(): mutex get fail\n",getpid());
        fprintf(stderr,"!!!continuing without mutex\n");
        return IDX_FAIL;
    }

    tooldata_header_t *hptr = HPTR();
    if (idx > (int)(hptr->last_index) ) {  // extend known indices
        hptr->last_index = idx;
        ret = IDX_NEW;
    } else {
        ret = IDX_OK;
    }
    CANON_TOOL_TABLE *tptr = TPTR(idx);
    *tptr = tdata;

    if (idx==0 && toolstat) { //note sai does not use toolTableCurrent
       *(struct CANON_TOOL_TABLE*)(&toolstat->toolTableCurrent) = tdata;
    }
    tool_mmap_mutex_give(); return ret;
} // tooldata_put()

void tooldata_reset()
{
    CANON_TOOL_TABLE initdata = tooldata_entry_init();
    tool_mmap_mutex_get();
    int idx;
    for (idx = 0; idx < CANON_POCKETS_MAX; idx++) {
        CANON_TOOL_TABLE *tptr = TPTR(idx);
        *tptr = initdata;
    }
    tool_mmap_mutex_give(); return;
} // tooldata_reset()

toolidx_t tooldata_get(CANON_TOOL_TABLE* pdata, int idx)
{
    if (!tool_mmap_base) {
        fprintf(stderr,"%5d tooldata_get() not mmapped BYE\n", getpid() );
        exit(EXIT_FAILURE);
    }
    if (idx < 0 || idx >= CANON_POCKETS_MAX) {
        UNEXPECTED_MSG;
        return IDX_FAIL;
    }

    if (tool_mmap_mutex_get()) {
        UNEXPECTED_MSG;
        fprintf(stderr,"!!!continuing without mutex\n");
    }
    *pdata = *TPTR(idx);

    tool_mmap_mutex_give(); return IDX_OK;
} // tooldata_get()

int tooldata_find_index_for_tool(int toolno)
{
    tooldata_header_t *hptr = HPTR();
    tool_mmap_mutex_get();
    int idx;

    if (!hptr->is_random_toolchanger && toolno == 0) {
        tool_mmap_mutex_give(); return 0;
    }

    int foundidx = -1;
    for (idx = 0; idx <= (int)hptr->last_index; idx++) { //note <=
        CANON_TOOL_TABLE *tptr = TPTR(idx);
        if (tptr->toolno == toolno) {
            foundidx = idx;
            if (foundidx==0) continue;
            break;
        }
    }
    tool_mmap_mutex_give();
    return foundidx;
} // tooldata_find_index_for_tool()
