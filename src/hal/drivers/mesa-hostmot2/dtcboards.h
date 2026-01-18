/*
 * This is a component for RaspberryPi and other boards for linuxcnc.
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
#ifndef HAL_HM2_DTCBOARDS_H
#define HAL_HM2_DTCBOARDS_H

/*
 * Info about the hardware platform, see:
 *  https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#best-practices-for-revision-code-usage
 *  https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#check-raspberry-pi-model-and-cpu-across-distributions
 *
 * Reading /proc/device-tree/compatible should contain the relevant
 * information. We should get a buffer containing a string-list like:
 *   "raspberrypi,5-model-b\0brcm,bcm2712\0"
 * And yes, it has embedded NULs.
 *
 * The idea is to match one of the strings to assign the correct driver for the
 * specific board.
 */
#define DTC_BOARD_MAKE_RPI "raspberrypi"

#define DTC_BOARD_RPI_5CM "5-compute-module"
#define DTC_BOARD_RPI_5B "5-model-b"
#define DTC_BOARD_RPI_4CM "4-compute-module"
#define DTC_BOARD_RPI_4B "4-model-b"
#define DTC_BOARD_RPI_3CM "3-compute-module"
#define DTC_BOARD_RPI_3BP "3-model-b-plus"
#define DTC_BOARD_RPI_3AP "3-model-a-plus"
#define DTC_BOARD_RPI_3B "3-model-b"
#define DTC_BOARD_RPI_2B "2-model-b"
#define DTC_BOARD_RPI_CM "compute-module"
#define DTC_BOARD_RPI_BP "model-b-plus"
#define DTC_BOARD_RPI_AP "model-a-plus"
#define DTC_BOARD_RPI_BR2 "model-b-rev2"
#define DTC_BOARD_RPI_B "model-b"
#define DTC_BOARD_RPI_A "model-a"
#define DTC_BOARD_RPI_ZERO_2W "model-zero-2-w"
#define DTC_BOARD_RPI_ZERO_W "model-zero-w"
#define DTC_BOARD_RPI_ZERO "model-zero"

#define DTC_SOC_MAKE_BRCM "brcm"

#define DTC_SOC_MODEL_BCM2712 "bcm2712"
#define DTC_SOC_MODEL_BCM2711 "bcm2711"
#define DTC_SOC_MODEL_BCM2837 "bcm2837"
#define DTC_SOC_MODEL_BCM2836 "bcm2836"
#define DTC_SOC_MODEL_BCM2835 "bcm2835"

/* The device-tree compatible strings for the boards */
#define DTC_RPI_SOC_BCM2712 DTC_SOC_MAKE_RPI "," DTC_SOC_MODEL_BCM2712
#define DTC_RPI_MODEL_5CM DTC_BOARD_MAKE_RPI "," DTC_BOARD_RPI_5CM
#define DTC_RPI_MODEL_5B DTC_BOARD_MAKE_RPI "," DTC_BOARD_RPI_5B

#define DTC_RPI_SOC_BCM2711 DTC_SOC_MAKE_RPI "," DTC_SOC_MODEL_BCM2711
#define DTC_RPI_MODEL_4CM DTC_BOARD_MAKE_RPI "," DTC_BOARD_RPI_4CM
#define DTC_RPI_MODEL_4B DTC_BOARD_MAKE_RPI "," DTC_BOARD_RPI_4B

#define DTC_RPI_SOC_BCM2837 DTC_SOC_MAKE_BRCM "," DTC_SOC_MODEL_BCM2837
#define DTC_RPI_MODEL_3CM DTC_BOARD_MAKE_RPI "," DTC_BOARD_RPI_3CM
#define DTC_RPI_MODEL_3BP DTC_BOARD_MAKE_RPI "," DTC_BOARD_RPI_3BP
#define DTC_RPI_MODEL_3AP DTC_BOARD_MAKE_RPI "," DTC_BOARD_RPI_3AP
#define DTC_RPI_MODEL_3B DTC_BOARD_MAKE_RPI "," DTC_BOARD_RPI_3B
#define DTC_RPI_MODEL_ZERO_2W DTC_BOARD_MAKE_RPI "," DTC_BOARD_RPI_ZERO_2W

/* Older than a RPi3 (bcm2836 and bcm2835) is probably not a good idea to use. */

#endif
// vim: ts=4
