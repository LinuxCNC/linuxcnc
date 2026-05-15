/*
 * Shared helper: check whether a kernel module is loaded.
 * Copyright (c) 2026 Luca Toniolo
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <unistd.h>

#include "kmod_check.h"

int kernel_module_loaded(const char *name)
{
    char path[256];
    snprintf(path, sizeof(path), "/sys/module/%s", name);
    return access(path, F_OK) == 0;
}
