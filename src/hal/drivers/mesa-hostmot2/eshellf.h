/*
 * This is a component for hostmot2 board drivers
 * Copyright (c) 2013,2014,2020,2024 Michael Geszkiewicz <micges@wp.pl>,
 *    Jeff Epler <jepler@unpythonic.net>
 *    B.Stultiens <lcnc@vagrearg.org>
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
#ifndef HAL_HM2_ESHELLF_H
#define HAL_HM2_ESHELLF_H

int shell(char *command);
int eshellf(const char *errpfx, const char *fmt, ...);

#endif
/* vim: ts=4
 */
