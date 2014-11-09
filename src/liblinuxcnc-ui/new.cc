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

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "linuxcnc-ui-private.h"

lui_t *lui_new(void) {
    lui_t *lui;
    lui = (lui_t*)calloc(1, sizeof(lui_t));
    if (lui == NULL) {
        return NULL;
    }

    lui->command_nml_receive_timeout.tv_sec = 5;
    lui->command_nml_receive_timeout.tv_usec = 0;

    lui->command_nml_done_timeout.tv_sec = 5;
    lui->command_nml_done_timeout.tv_usec = 0;

    lui->nml_serial_number = getpid() * 100;  // pick a number that's different from every other UI instance

    return lui;
}

