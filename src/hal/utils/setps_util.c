/*
 * Common set_p and set_s callback for consistent value parsing
 *
 * Copyright (c) 2026  B.Stultiens
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <locale.h>

#include "setps_util.h"

//
// Common callback parsing the string argument as a value. The actual prototype
// is:
//     int setps_common_cb(hal_query_t *q, const char *value)
//
// It expects the q->name to be set to allow error signalling of the HAL_PORT
// type that is not allowed in signals.
//
// Returns zero (0) on success or an appropriate negative errno value on error:
//   -EINVAL  - unsupported/illegal value
//   -ERANGE  - value out of range
//   -EBADF   - setting of q->pp.type/q->sig.type not supported
//   -ENOMEM  - failed to allocate memory for locale switching
//
int setps_common_cb(hal_query_t *q, void *arg)
{
    const char *value = (const char *)arg;
    hal_query_value_u *qvp = HAL_QTYPE_SIGNAL == q->qtype ? &q->sig.value : &q->pp.value;
    hal_type_t type = HAL_QTYPE_SIGNAL == q->qtype ? q->sig.type : q->pp.type;
    char *eptr;
    static const struct {
        const char *name;
        bool value;
    } boolvals[] = {
        {"TRUE",  1}, {"FALSE", 0},
        {"1",     1}, {"0",     0},
        {"ON",    1}, {"OFF",   0},
        {"YES",   1}, {"NO",    0},
        {NULL, 0}
    };
    switch(type) {
    case HAL_BOOL:
        for(int i = 0; boolvals[i].name; i++) {
            if(!strcasecmp(boolvals[i].name, value)) {
                qvp->b = boolvals[i].value;
                return 0;
            }
        }
        return -EINVAL;
    case HAL_S32:
    case HAL_SINT:
        errno = 0;
        qvp->s = strtoll(value, &eptr, 0);
        if(HAL_S32 == type && (qvp->s < RTAPI_INT32_MIN || qvp->s > RTAPI_INT32_MAX))
            return -ERANGE;
        if(0 != errno)
            return -errno;
        if(value == eptr || (('\0' != *eptr) && !isspace((unsigned char)*eptr)))
            return -EINVAL;  // No data in string or trailing junk
        break;
    case HAL_PORT:
        if(HAL_QTYPE_SIGNAL != q->qtype) {
            // This is a setp call, don't allow port
            return -EBADF;
        }
        /* Fallthrough */
    case HAL_U32:
    case HAL_UINT:
        errno = 0;
        qvp->u = strtoull(value, &eptr, 0);
        if((HAL_U32 == type || HAL_PORT == type) && qvp->u > RTAPI_UINT32_MAX)
            return -ERANGE;
        if(0 != errno)
            return -errno;
        if(value == eptr || (('\0' != *eptr) && !isspace((unsigned char)*eptr)))
            return -EINVAL;  // No data in string or trailing junk
        break;
    case HAL_REAL: {
        // Switch locale so we always use decimal point
        locale_t olc = uselocale((locale_t)0);
        locale_t nlc = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
        if((locale_t)0 == nlc)
            return -ENOMEM;
        uselocale(nlc);
        errno = 0;
        qvp->r = strtod(value, &eptr);
        int errnosave = errno;
        uselocale(olc);
        freelocale(nlc);
        if(0 != errnosave)
            return -errnosave;
        if(value == eptr || (('\0' != *eptr) && !isspace((unsigned char)*eptr)))
            return -EINVAL;  // No data in string or trailing junk
        break; }
    default:
        return -EBADF;
    }
    return 0;
}
