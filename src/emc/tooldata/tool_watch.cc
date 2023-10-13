/*
** base on example by Jeff Epler:
** https://emc-users.narkive.com/44bLaS4j/controlling-linuxcnc-externally-with-c-program
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
#include "emc.hh"
#include "emc_nml.hh"
#include <unistd.h>
#include "config.h"

#define DATA_FMT "%8d %8d %6d %6d %8.3f %8.3f %8.3f %8.3f\n"
#define HDR_FMT  "%8s %8s %6s %6s %8s %8s %8s %8s\n"
#define HDR_INTERVAL 20

static void header() {
    fprintf(stderr, HDR_FMT,
    "PrepIDX", "Spindle","Tool","Pocket","Diam","X-off","Y-off","Z-off");
}

int main(int argc, char **argv) {
    int hdr_ct     = 0;
    int invalid_ct = 0;
    int peek_ct    = 0;
    RCS_STAT_CHANNEL *stat = 0;
    char nmlfile[512];

    if(argc == 2) {
        snprintf(nmlfile,sizeof(nmlfile),"%s",argv[1]);
    } else {
        snprintf(nmlfile,sizeof(nmlfile),"%s",EMC2_DEFAULT_NMLFILE);
    }
    fprintf(stderr,"%s Using: %s\n",argv[0],(char*)nmlfile);
    stat = new RCS_STAT_CHANNEL(emcFormat, "emcStatus", "xemc", nmlfile);

#ifdef TOOL_NML
  fprintf(stderr,"tool_watch: TOOLTABLE:nml\n");
#else
  fprintf(stderr,"tool_watch: TOOLTABLE:mmap\n");
#endif
    fprintf(stderr,"%8ld EMC_STAT\n",sizeof(EMC_STAT));
    fprintf(stderr,"%8ld   EMC_IO_STAT\n",sizeof(EMC_IO_STAT));
    fprintf(stderr,"%8ld    EMC_TOOL_STAT\n",sizeof(EMC_TOOL_STAT));
    fprintf(stderr,"%8ld     EMC_COOLANT_STAT\n",sizeof(EMC_COOLANT_STAT));
    fprintf(stderr,"%8ld     EMC_AUX_STAT\n",sizeof(EMC_AUX_STAT));
    fprintf(stderr,"%8ld   EMC_TASK_STAT\n",sizeof(EMC_TASK_STAT));
    fprintf(stderr,"%8ld   EMC_MOTION_STAT\n",sizeof(EMC_MOTION_STAT));
    fprintf(stderr,"%8ld     EMC_TRAJ_STAT\n",sizeof(EMC_TRAJ_STAT));
    fprintf(stderr,"%8ld     EMC_JOINT_STAT\n",sizeof(EMC_JOINT_STAT));
    fprintf(stderr,"%8ld     EMC_AXIS_STAT\n",sizeof(EMC_AXIS_STAT));
    fprintf(stderr,"%8ld     EMC_SPINDLE_STAT\n",sizeof(EMC_SPINDLE_STAT));
    fprintf(stderr,"\n");

    header();hdr_ct=1;
    while(1) {
        if(!stat->valid()) {
            invalid_ct++;
            if (invalid_ct > 2) break;
            continue;
        }
        if(stat->peek() != EMC_STAT_TYPE) {
            peek_ct++;
            if (peek_ct > 2) break;
            continue;
        }
        EMC_STAT *emcStatus = static_cast<EMC_STAT*>(stat->get_address());
        fprintf(stderr, DATA_FMT
               , emcStatus->io.tool.pocketPrepped // idx
               , emcStatus->io.tool.toolInSpindle // toolno
#ifdef TOOL_NML //{
               , emcStatus->io.tool.toolTable[0].toolno
               , emcStatus->io.tool.toolTable[0].pocketno
               , emcStatus->io.tool.toolTable[0].diameter
               , emcStatus->io.tool.toolTable[0].offset.tran.x
               , emcStatus->io.tool.toolTable[0].offset.tran.y
               , emcStatus->io.tool.toolTable[0].offset.tran.z
#else //}{
               , emcStatus->io.tool.toolTableCurrent.toolno
               , emcStatus->io.tool.toolTableCurrent.pocketno
               , emcStatus->io.tool.toolTableCurrent.diameter
               , emcStatus->io.tool.toolTableCurrent.offset.tran.x
               , emcStatus->io.tool.toolTableCurrent.offset.tran.y
               , emcStatus->io.tool.toolTableCurrent.offset.tran.z
#endif //}
        );
        usleep(1000*1000);
        if (++hdr_ct % HDR_INTERVAL == 0) {header();}
    }
    return 0;
} // main()
