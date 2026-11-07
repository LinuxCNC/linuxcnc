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
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h> // write(2),lseek(2)
#include <fcntl.h> // open(2)
#include <sys/mman.h>
#include <sys/stat.h> // fstat(2)
#include <string.h>
#include <errno.h>
#include "config.h"
#include <rtapi_mutex.h>
#include "tooldata.hh"

#define UNEXPECTED_MSG fprintf(stderr,"UNEXPECTED %s %d\n",__FILE__,__LINE__);

#define TOOL_MMAP_FILENAME ".tool.mmap"
#define TOOL_MMAP_MODE     0600
// O_NOFOLLOW: refuse to follow a symlink at the mmap path. Without it a
// pre-placed symlink is followed and the target truncated.
#define TOOL_MMAP_CREATOR_OPEN_FLAGS  O_RDWR | O_CREAT | O_TRUNC | O_NOFOLLOW
#define TOOL_MMAP_USER_OPEN_FLAGS     O_RDWR

static int           creator_fd;
// The descriptor actually backing the mapping, and whether THIS process
// created the file. tool_mmap_user() previously dropped its fd on the
// floor: it was neither closed nor recorded, so it leaked for the life of
// the process and tool_mmap_close() had nothing to close but creator_fd --
// which a user process never sets, so it is 0, i.e. stdin.
static int           mmap_fd = -1;
static bool          mmap_is_creator = false;
static char          filename[LINELEN] = {};
static char*         tool_mmap_base = NULL;
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
#define HPTR()    reinterpret_cast<tooldata_header_t*>( tool_mmap_base \
                                      + TOOL_MMAP_HEADER_OFFSET)

#define TPTR(idx) reinterpret_cast<CANON_TOOL_TABLE*>( tool_mmap_base \
                                     + TOOL_MMAP_HEADER_OFFSET \
                                     + TOOL_MMAP_HEADER_SIZE \
                                     + idx * TOOL_MMAP_STRIDE)
//---------------------------------------------------------------------
/* Note: emccfg.h defaults (seconds)
**       DEFAULT_EMC_TASK_CYCLE_TIME 0.100 (.001 common)
**       DEFAULT_EMC_IO_CYCLE_TIME   0.100
*/

void tool_mmap_set_fname(const char* fname) {
    // Let a standalone user of the tooldata mmap (sai/rs274) choose a private
    // file. Without this every process uses $HOME/.tool.mmap, so an offline
    // parse truncates the tool table of a session that is already running.
    snprintf(filename,sizeof(filename),"%s",fname);
}

static char* tool_mmap_fname(void) {
    if (*filename) {return filename;}
    char* hdir = secure_getenv("HOME");
    if (!hdir) { hdir = (char *) EMC2_TMP_DIR; }
    snprintf(filename,sizeof(filename),"%s/%s",hdir,TOOL_MMAP_FILENAME);
    return(filename);
}

// Every entry point below answers "no data" when nothing is mapped - a
// process that never created or attached the mmap (gcode.parse with no
// LinuxCNC running) - rather than dereferencing a null base.
static int tool_mmap_mutex_get()
{
    if (!tool_mmap_base) { return -1; }
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
    if (!tool_mmap_base) { return; }
    tooldata_header_t *hptr = HPTR();
    rtapi_mutex_give(&(hptr->mutex));
} // tool_mmap_mutex_give()

bool tool_mmap_is_random_toolchanger(void)
{
    bool ans = 0;
    if (!tool_mmap_base) { return ans; }
    tool_mmap_mutex_get();
    tooldata_header_t *hptr = HPTR();
    ans = hptr->is_random_toolchanger;
    tool_mmap_mutex_give();
    return ans;
}

// Why the creator could not open the mmap file `fname`, after open() failed
// with `open_errno`: the error, then what lstat() finds at that path.
static void tool_mmap_open_diagnosis(const char *fname,int open_errno,
                                     char *why,size_t whylen)
{
    int n = snprintf(why,whylen,"open '%s' fail: %s",fname,strerror(open_errno));
    if (n < 0 || (size_t)n >= whylen) { return; }
    char *rest = why + n;
    size_t restlen = whylen - n;
    struct stat sb;
    if (0 == lstat(fname, &sb)) {
        unsigned mode = (unsigned)sb.st_mode & ~S_IFMT;
        if (S_ISREG(sb.st_mode)) {
            if (access(fname, R_OK | W_OK) != 0) {
                snprintf(rest,restlen,"; file exists but no read/write access (mode: %03o)",mode);
            } else {
                snprintf(rest,restlen,"; unknown issue, file exists with mode %03o",mode);
            }
        } else if (S_ISDIR(sb.st_mode)) {
            snprintf(rest,restlen,"; file is directory");
        } else if (S_ISLNK(sb.st_mode)) {
            snprintf(rest,restlen,"; file is symbolic link");
        } else {
            snprintf(rest,restlen,"; file is other");
        }
    } else if (ENOENT != errno) {
        snprintf(rest,restlen,"; lstat fail after open fail: %s",strerror(errno));
    }
}

// Create the mmap file and map it. -1 with the reason in `why` on failure,
// with nothing left open or mapped.
static int tool_mmap_create(EMC_TOOL_STAT const * ptr,int random_toolchanger,
                            char *why,size_t whylen)
{
    char * fname = tool_mmap_fname();
    int fd = open(fname,
                  TOOL_MMAP_CREATOR_OPEN_FLAGS,TOOL_MMAP_MODE);
    if (fd < 0) {
        tool_mmap_open_diagnosis(fname,errno,why,whylen);
        return -1;
    }
    if (lseek(fd, TOOL_MMAP_SIZE, SEEK_SET) == -1) {
        snprintf(why,whylen,"lseek '%s' fail: %s",fname,strerror(errno));
        close(fd);
        return -1;
    }
    if (write(fd, "\0", 1) < 0) {
        snprintf(why,whylen,"file tail write '%s' fail: %s",fname,strerror(errno));
        close(fd);
        return -1;
    }
    char *base = (char*)mmap(NULL, TOOL_MMAP_SIZE, PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, 0);
    if (base == MAP_FAILED) {
        snprintf(why,whylen,"mmap '%s' fail: %s",fname,strerror(errno));
        close(fd);
        return -1;
    }

    toolstat = ptr; //note NULL for sai
    creator_fd = fd;
    tool_mmap_base = base;
    tooldata_header_t *hptr = HPTR();
    hptr->is_random_toolchanger = random_toolchanger;
    hptr->last_index = 0;

    mmap_fd = creator_fd;
    mmap_is_creator = true;
    tool_mmap_mutex_give(); return 0;
} // tool_mmap_create()

//typ creator: emc/ioControl.cc, sai/driver.cc
//    (first applicable process started in linuxcnc script)
int tool_mmap_creator(EMC_TOOL_STAT const * ptr,int random_toolchanger)
{
    static int inited=0;
    char why[LINELEN] = {};

    if (inited) {
        fprintf(stderr,"Error: tool_mmap_creator already called BYE\n");
        exit(EXIT_FAILURE);
    }
    if (tool_mmap_create(ptr,random_toolchanger,why,sizeof(why))) {
        fprintf(stderr,"tool_mmap_creator(): error: %s\n",why);
        exit(EXIT_FAILURE);
    }
    inited = 1;
    return 0;
} // tool_mmap_creator();

// For a library user (the gcode Python module) that must not exit: create
// the mmap at the name set by tool_mmap_set_fname(), or answer -1 with the
// reason. May be called again once tool_mmap_close() dropped the last one.
int tool_mmap_creator_try(EMC_TOOL_STAT const * ptr,int random_toolchanger,
                          char *why,size_t whylen)
{
    if (tool_mmap_base) {
        snprintf(why,whylen,"a tool mmap is already open");
        return -1;
    }
    return tool_mmap_create(ptr,random_toolchanger,why,whylen);
} // tool_mmap_creator_try()

//typ: milltask, guis (emcmodule,emcsh,...), halui
int tool_mmap_user()
{
    static bool warned = false;
    char why[LINELEN] = {};
    char * fname = tool_mmap_fname();
    int fd = open(fname,
                  TOOL_MMAP_USER_OPEN_FLAGS, TOOL_MMAP_MODE);
    struct stat st;

    if (fd < 0) {
        snprintf(why, sizeof(why), "open: %s", strerror(errno));
    } else if (fstat(fd, &st) < 0) {
        snprintf(why, sizeof(why), "fstat: %s", strerror(errno));
    } else if ((size_t)st.st_size < (size_t)(TOOL_MMAP_SIZE)) {
        // The creator truncates the file before extending it: mapping
        // it while short would SIGBUS on first access.
        snprintf(why, sizeof(why), "size %zu < %zu",
                 (size_t)st.st_size, (size_t)(TOOL_MMAP_SIZE));
    }
    if (*why) {
        if (fd >= 0) { close(fd); }
        /*
        ** For the LinuxCNC application, tool_mmap_creator()
        ** should start first and create the mmap file needed here.
        ** Related designer applications may use emcmodule.cc
        ** which calls this tool_mmap_user() but need to
        ** continue execution if no mmap file is open.
        ** So print message and return fail indicator.
        */
        // callers may retry (emcmodule poll()), so warn only once
        if (!warned) {
            warned = true;
            fprintf(stderr,"tool_mmap_user(): note: tool mmap not available: %s: %s\n",
                    fname, why);
        }
        tool_mmap_base = (char*)NULL;
        return(-1);
    }
    tool_mmap_base = (char*)mmap(NULL, TOOL_MMAP_SIZE, PROT_READ|PROT_WRITE,
                                 MAP_SHARED, fd, 0);

    if (tool_mmap_base == MAP_FAILED) {
        close(fd);
        fprintf(stderr, "tool_mmap_user(): error: mmap '%s' fail: %s\n", fname, strerror(errno));
        exit(EXIT_FAILURE);
    }
    mmap_fd = fd;
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
        // NO exit() HERE. This function is usable as an atexit() handler,
        // and calling exit() from within one is undefined behaviour.
        // Report and FALL THROUGH: a failed munmap is exactly when the
        // unlink and close below matter most, so returning here would leave
        // the file on disk and the descriptor open.
        perror("tool_mmap_close(): munmap fail");
    }
    // Only the creator removes the file. A user process unlinking it would
    // delete the running session's tool table out from under io.
    if (mmap_is_creator && unlink(tool_mmap_fname())) {
        perror("tool_mmap_close(): unlink fail");
    }
    if (mmap_fd >= 0) {
        close(mmap_fd);
        mmap_fd = -1;
    }
    // Idempotent from here: a second call, or an atexit after an explicit
    // close, returns at the tool_mmap_base guard above.
    tool_mmap_base = nullptr;
    mmap_is_creator = false;
} //tool_mmap_close()

void tooldata_last_index_set(int idx)  //force last_index
{
    if (!tool_mmap_base) { return; }
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
    if (!tool_mmap_base) { return -1; }
    tool_mmap_mutex_get();
    tooldata_header_t *hptr = HPTR();
    tool_mmap_mutex_give(); return hptr->last_index;
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
    if (!tool_mmap_base) { return; }
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
        return IDX_FAIL;
    }
    if (idx < 0 || idx >= CANON_POCKETS_MAX) {
        // ui programs may query for nonexistent idx values
        // and must handle this error
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
    if (!tool_mmap_base) { return -1; } // not found: nothing is loaded
    tooldata_header_t *hptr = HPTR();
    tool_mmap_mutex_get();
    int idx;

    if (toolno == -1) {tool_mmap_mutex_give(); return -1;}

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
