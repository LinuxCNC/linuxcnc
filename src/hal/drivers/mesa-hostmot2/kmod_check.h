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
#ifndef HAL_HM2_KMOD_CHECK_H
#define HAL_HM2_KMOD_CHECK_H

/*
 * Check whether a kernel module is loaded (loadable or built-in).
 * Returns non-zero when present, zero when absent.
 *
 * Implemented via /sys/module/<name>, which the kernel populates for
 * both loadable modules (initstate "live") and built-in ones (no
 * initstate file). This catches the built-in case that /proc/modules
 * misses; on a custom kernel where the conflicting driver is compiled
 * in rather than modular, the check still fires.
 *
 * NOTE: pass the canonical underscore form of the module name. The
 * kernel exposes modules in /sys/module/ with underscores only, so
 * "spi-bcm2835" will not match "spi_bcm2835" on disk. modprobe/lsmod
 * canonicalize for you; this helper does not.
 */
int kernel_module_loaded(const char *name);

#endif
/* vim: ts=4
 */
