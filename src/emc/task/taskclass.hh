/*    This is a component of LinuxCNC
 *    Copyright 2011 Michael Haberler <git@mah.priv.at>
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#ifndef TASKCLASS_HH
#define TASKCLASS_HH

#include "emc.hh"
#include "inifile.hh"
#include "hal.hh"
#include "tooldata.hh"

#define UNEXPECTED_MSG fprintf(stderr,"UNEXPECTED %s %d\n",__FILE__,__LINE__);

struct iocontrol_str {
    hal_pin<bool> user_enable_out;     /* output, TRUE when EMC wants stop */
    hal_pin<bool> emc_enable_in;        /* input, TRUE on any external stop */
    hal_pin<bool> user_request_enable; /* output, used to reset ENABLE latch */
    hal_pin<bool> coolant_mist;        /* coolant mist output pin */
    hal_pin<bool> coolant_flood;       /* coolant flood output pin */

    // the following pins are needed for toolchanging
    //tool-prepare
    hal_pin<bool> tool_prepare;        /* output, pin that notifies HAL it needs to prepare a tool */
    hal_pin<int32_t> tool_prep_pocket; /* output, pin that holds the pocketno for the tool table entry matching the tool to be prepared,
                                               only valid when tool-prepare=TRUE */
    hal_pin<int32_t> tool_from_pocket; /* output, pin indicating pocket current load tool retrieved from*/
    hal_pin<int32_t> tool_prep_index;  /* output, pin for internal index (idx) of prepped tool above */
    hal_pin<int32_t> tool_prep_number; /* output, pin that holds the tool number to be prepared, only valid when tool-prepare=TRUE */
    hal_pin<int32_t> tool_number;      /* output, pin that holds the tool number currently in the spindle */
    hal_pin<bool> tool_prepared;        /* input, pin that notifies that the tool has been prepared */
    //tool-change
    hal_pin<bool> tool_change;         /* output, notifies a tool-change should happen (emc should be in the tool-change position) */
    hal_pin<bool> tool_changed;         /* input, notifies tool has been changed */

    // note: spindle control has been moved to motion
};                        //pointer to the HAL-struct

class Task {
public:
    Task(EMC_IO_STAT &emcioStatus_in);
    virtual ~Task();

    virtual int emcIoInit();
    virtual int emcIoAbort(EMC_ABORT reason);
    virtual int emcAuxEstopOn();
    virtual int emcAuxEstopOff();
    virtual int emcCoolantMistOn();
    virtual int emcCoolantMistOff();
    virtual int emcCoolantFloodOn();
    virtual int emcCoolantFloodOff();
    virtual int emcToolSetOffset(int pocket, int toolno, EmcPose offset, double diameter,
				 double frontangle, double backangle, int orientation);
    virtual int emcToolPrepare(int tool);
    virtual int emcToolLoad();
    virtual int emcToolLoadToolTable(const char *file);
    virtual int emcToolUnload();
    virtual int emcToolSetNumber(int number);

    int iocontrol_hal_init(void);
    void reload_tool_number(int toolno);
    void load_tool(int idx);
    void run();
    int read_tool_inputs(void);
    void hal_init_pins(void);

    EMC_IO_STAT &emcioStatus;
    int random_toolchanger {0};
    iocontrol_str iocontrol_data;
    hal_comp iocontrol;
    const char *ini_filename;
    const char *tooltable_filename {};
    char db_program[LINELEN] {};
    tooldb_t db_mode {tooldb_t::DB_NOTUSED};
    int tool_status;
};

extern Task *task_methods;

#endif
