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

#include <string.h>

#include "linuxcnc-ui-private.h"


int lui_error(lui_t *lui, lui_error_t *error_type, const char **error_msg) {
    NMLTYPE type;

    lui->error_string[0] = (char)0;

    if (lui == NULL || lui->error_nml == NULL || lui->error_nml->valid() == 0) {
        return -1;
    }

    type = lui->error_nml->read();
    switch (type) {
        case -1:
            // error reading channel
            return -1;
            break;

        case 0:
            *error_type = lui_no_error;
            break;

        case EMC_OPERATOR_ERROR_TYPE:
            strncpy(lui->error_string, ((EMC_OPERATOR_ERROR *)(lui->error_nml->get_address()))->error, LINELEN - 1);
            lui->error_string[LINELEN - 1] = (char)0;
            *error_type = lui_operator_error;
            *error_msg = lui->error_string;
            break;

        case EMC_OPERATOR_TEXT_TYPE:
            strncpy(lui->error_string, ((EMC_OPERATOR_TEXT *)(lui->error_nml->get_address()))->text, LINELEN - 1);
            lui->error_string[LINELEN - 1] = (char)0;
            *error_type = lui_operator_text;
            *error_msg = lui->error_string;
            break;

        case EMC_OPERATOR_DISPLAY_TYPE:
            strncpy(lui->error_string, ((EMC_OPERATOR_DISPLAY *)(lui->error_nml->get_address()))->display, LINELEN - 1);
            lui->error_string[LINELEN - 1] = (char)0;
            *error_type = lui_operator_display;
            *error_msg = lui->error_string;
            break;

        case NML_ERROR_TYPE:
            strncpy(lui->error_string, ((NML_ERROR *)(lui->error_nml->get_address()))->error, NML_ERROR_LEN - 1);
            lui->error_string[NML_ERROR_LEN - 1] = (char)0;
            *error_type = lui_nml_error;
            *error_msg = lui->error_string;
            break;

        case NML_TEXT_TYPE:
            strncpy(lui->error_string, ((NML_TEXT *)(lui->error_nml->get_address()))->text, NML_ERROR_LEN - 1);
            lui->error_string[NML_ERROR_LEN - 1] = (char)0;
            *error_type = lui_nml_text;
            *error_msg = lui->error_string;
            break;

        case NML_DISPLAY_TYPE:
            strncpy(lui->error_string, ((NML_DISPLAY *)(lui->error_nml->get_address()))->display, NML_ERROR_LEN - 1);
            lui->error_string[NML_ERROR_LEN - 1] = (char)0;
            *error_type = lui_nml_display;
            *error_msg = lui->error_string;
            break;

        default:
            printf("lui: unrecognized error type %ld", type);
            *error_type = lui_unknown_error;
            *error_msg = "";
            break;
    }

    return 0;
}
