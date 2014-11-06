//
// Copyright (C) 2014 Sebastian Kuzminsky
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Library General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public
// License along with this library; if not, write to the
// Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
// Boston, MA  02110-1301, USA.
//

#include "linuxcnc-ui-private.h"


int lui_coolant_mist_on(lui_t *lui) {
    EMC_COOLANT_MIST_ON msg;
    return lui_send_nml_command_and_wait(lui, msg);
}

int lui_coolant_mist_off(lui_t *lui) {
    EMC_COOLANT_MIST_OFF msg;
    return lui_send_nml_command_and_wait(lui, msg);
}

int lui_coolant_flood_on(lui_t *lui) {
    EMC_COOLANT_FLOOD_ON msg;
    return lui_send_nml_command_and_wait(lui, msg);
}

int lui_coolant_flood_off(lui_t *lui) {
    EMC_COOLANT_FLOOD_OFF msg;
    return lui_send_nml_command_and_wait(lui, msg);
}
