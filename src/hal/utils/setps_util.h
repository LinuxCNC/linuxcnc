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
#ifndef __HAL_UTILS_SETPS_UTIL_H
#define __HAL_UTILS_SETPS_UTIL_H

#include <hal.h>

#ifdef __cplusplus
extern "C" {
#endif

int setps_common_cb(hal_query_t *q, void *arg);

#ifdef __cplusplus
}
#endif

#endif
