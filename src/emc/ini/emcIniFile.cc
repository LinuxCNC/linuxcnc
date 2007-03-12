/******************************************************************************
 *
 * Copyright (C) 2007 Peter G. Vavaroutsos <pete AT vavaroutsos DOT com>
 *
 * $RCSfile$
 * $Author$
 * $Locker$
 * $Revision$
 * $State$
 * $Date$
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA
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

#include "emcIniFile.hh"


IniFile::ConversionEntry    EmcIniFile::axisTypeMap[] = {
    {"LINEAR", EMC_AXIS_LINEAR},
    {"ANGULAR", EMC_AXIS_ANGULAR},
};


EmcIniFile::ErrorCode
EmcIniFile::Find(EmcAxisType *result,
                 const char *tag, const char *section, int num)
{
    return(IniFile::Find((int *)result,
                         axisTypeMap, sizeof(axisTypeMap)/sizeof(*axisTypeMap),
                         tag, section, num));
}


IniFile::ConversionEntry    EmcIniFile::boolMap[] = {
    {"TRUE", 1},
    {"YES", 1},
    {"1", 1},
    {"FALSE", 0},
    {"NO", 0},
    {"0", 0},
};


EmcIniFile::ErrorCode
EmcIniFile::Find(bool *result, const char *tag, const char *section, int num)
{
    ErrorCode               errCode;
    int                     value;

    if((errCode = IniFile::Find(&value,
                                boolMap, sizeof(boolMap)/sizeof(*boolMap),
                                tag, section, num)) == ERR_NONE){
        *result = (bool)value;
    }

    return(errCode);
}
