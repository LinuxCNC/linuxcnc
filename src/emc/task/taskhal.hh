//    HAL pins exported by Task ("task.*"), taking over halui's pins.
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#ifndef TASKHAL_HH
#define TASKHAL_HH

#include "nml_intf/emc_nml.hh"

// Create the "task" HAL component and its pins, before the HAL files run:
// they may connect them. The axes and spindles come from the INI file.
int emcTaskHalInit(int joints);

// Copy status into the output pins, once per Task cycle.
void emcTaskHalUpdate(const EMC_STAT &stat);

void emcTaskHalExit();

#endif
