/******************************************************************************
 *
 * Copyright (C) 2007 Peter G. Vavaroutsos <pete AT vavaroutsos DOT com>
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU General
 * Public License as published by the Free Software Foundation.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
 * ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
 * TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
 * harming persons must have provisions for completely removing power
 * from all motors, etc, before persons enter any danger area.  All
 * machinery must be designed to comply with local and national safety
 * codes, and the authors of this software can not, and do not, take
 * any responsibility for such compliance.
 *
 * This code was written as part of the EMC project.  For more
 * information, go to www.linuxcnc.org.
 *
 ******************************************************************************/

#include <math.h>           // M_PI.
#include "emcIniFile.hh"


IniFile::StrIntPair         EmcIniFile::jointTypeMap[] = {
    {"LINEAR", EMC_LINEAR},
    {"ANGULAR", EMC_ANGULAR},
    { NULL, 0 },
};

EmcIniFile::ErrorCode
EmcIniFile::Find(EmcJointType *result,
                 const char *tag, const char *section, int num)
{
    return(IniFile::Find((int *)result, jointTypeMap, tag, section, num));
}


IniFile::StrIntPair         EmcIniFile::boolMap[] = {
    {"TRUE", 1},
    {"YES", 1},
    {"1", 1},
    {"FALSE", 0},
    {"NO", 0},
    {"0", 0},
    { NULL, 0 },
};

EmcIniFile::ErrorCode
EmcIniFile::Find(bool *result, const char *tag, const char *section, int num)
{
    ErrorCode               errCode;
    int                     value;

    if((errCode = IniFile::Find(&value, boolMap, tag,section,num)) == ERR_NONE){
        *result = (bool)value;
    }

    return(errCode);
}


// The next const struct holds pairs for linear units which are 
// valid under the [TRAJ] section. These are of the form {"name", value}.
// If the name "name" is encountered in the ini, the value will be used.
EmcIniFile::StrDoublePair   EmcIniFile::linearUnitsMap[] = {
    { "mm",         1.0 },
    { "metric",     1.0 },
    { "in",         1/25.4 },
    { "inch",       1/25.4 },
    { "imperial",   1/25.4 },
    { NULL,         0 },
};
    
EmcIniFile::ErrorCode
EmcIniFile::FindLinearUnits(EmcLinearUnits *result,
                 const char *tag, const char *section, int num)
{
    return(IniFile::Find((double *)result, linearUnitsMap, tag, section, num));
}


// The next const struct holds pairs for angular units which are 
// valid under the [TRAJ] section. These are of the form {"name", value}.
// If the name "name" is encountered in the ini, the value will be used.
EmcIniFile::StrDoublePair   EmcIniFile::angularUnitsMap[] = {
    { "deg",        1.0 },
    { "degree",     1.0 },
    { "grad",       0.9 },
    { "gon",        0.9 },
    { "rad",        M_PI / 180 },
    { "radian",     M_PI / 180 },
    { NULL,         0 },
};

EmcIniFile::ErrorCode
EmcIniFile::FindAngularUnits(EmcAngularUnits *result,
                 const char *tag, const char *section, int num)
{
    return(IniFile::Find((double *)result, angularUnitsMap, tag, section, num));
}
