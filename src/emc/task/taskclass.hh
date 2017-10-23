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
#include "initool.hh"
#include "tool_parse.h"
#include "inifile.hh"

class Task {
public:
    Task();
    virtual ~Task();

    virtual int emcIoInit();
    virtual int emcIoHalt();
    virtual int emcIoAbort(int reason);
    virtual int emcToolStartChange();
    virtual int emcAuxEstopOn();
    virtual int emcAuxEstopOff();
    virtual int emcCoolantMistOn();
    virtual int emcCoolantMistOff();
    virtual int emcCoolantFloodOn();
    virtual int emcCoolantFloodOff();
    virtual int emcLubeOn();
    virtual int emcLubeOff();
    virtual int emcIoSetDebug(int debug);
    virtual int emcToolSetOffset(int pocket, int toolno, EmcPose offset, double diameter,
				 double frontangle, double backangle, int orientation);
    virtual int emcToolPrepare(int p, int tool);
    virtual int emcToolLoad();
    virtual int emcToolLoadToolTable(const char *file);
    virtual int emcToolUnload();
    virtual int emcToolSetNumber(int number);
    virtual int emcIoUpdate(EMC_IO_STAT * stat);

    virtual int emcIoPluginCall(int len, const char *msg);

    int use_iocontrol;
    int random_toolchanger;
    const char *ini_filename;
    const char *tooltable_filename;
private:

    char *ttcomments[CANON_POCKETS_MAX];
};

extern Task *task_methods;

#endif
