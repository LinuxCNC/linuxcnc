/*
 * This is a component for hostmot2 board identification
 * Copyright (c) 2024 B.Stultiens <lcnc@vagrearg.org>
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
#ifndef HAL_HM2_LLIO_INFO_H
#define HAL_HM2_LLIO_INFO_H

/*
 * Search the list of supported boards for the idrom.board_name and sets all
 * administrative parameters of the hm2_lowlevel_t structure.
 *
 * Returns the name of the board that can be used as a base name for
 * hm2_register().
 */
const char *set_llio_info_spi(hm2_lowlevel_io_t *llio, const hm2_idrom_t *idrom);

#endif
/* vim: ts=4
 */
