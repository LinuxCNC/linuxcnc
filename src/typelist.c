/**
 * @file typelist.c
 * @brief Global slave-type registry for the LinuxCNC EtherCAT HAL driver.
 *
 * Defines the @c typelist[] array which is the single authoritative table
 * mapping every supported EtherCAT slave model to its driver callbacks and
 * identity information.  The driver's slave-creation path (in @c slave.c)
 * searches this table by @c LCEC_SLAVE_TYPE_T enum value to locate the
 * vendor ID, product code, expected PDO entry count, and the
 * @c proc_preinit / @c proc_init function pointers for the device.
 *
 * Adding support for a new device requires:
 *   1. Implementing a device driver in @c src/devices/ that exports a
 *      @c proc_init function (and optionally a @c proc_preinit function for
 *      devices with dynamic PDO counts).
 *   2. Adding a new enumerator to @c LCEC_SLAVE_TYPE_T in @c lcec.h.
 *   3. Adding a row to @c typelist[] below, grouped by device category.
 *
 * The array is terminated by a sentinel entry whose @c type field equals
 * @c lcecSlaveTypeInvalid.
 *
 * @copyright Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de>
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
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "priv.h"

#include "devices/generic.h"
#include "devices/ek1100.h"
#include "devices/ax5100.h"
#include "devices/ax5200.h"
#include "devices/el1xxx.h"
#include "devices/el1252.h"
#include "devices/el1859.h"
#include "devices/el2xxx.h"
#include "devices/el2202.h"
#include "devices/el31x2.h"
#include "devices/el31x4.h"
#include "devices/el32x4.h"
#include "devices/el3255.h"
#include "devices/el3314.h"
#include "devices/em3712.h"
#include "devices/el40x1.h"
#include "devices/el40x2.h"
#include "devices/el40x8.h"
#include "devices/el41x2.h"
#include "devices/el41x4.h"
#include "devices/el5002.h"
#include "devices/el5021.h"
#include "devices/el5032.h"
#include "devices/el5101.h"
#include "devices/el5151.h"
#include "devices/el5122.h"
#include "devices/el5152.h"
#include "devices/el2521.h"
#include "devices/el6900.h"
#include "devices/el1918_logic.h"
#include "devices/el1904.h"
#include "devices/el2904.h"
#include "devices/ax5805.h"
#include "devices/el7041_1000.h"
#include "devices/el70x1.h"
#include "devices/el7211.h"
#include "devices/el7342.h"
#include "devices/el7411.h"
#include "devices/el95xx.h"
#include "devices/em7004.h"
#include "devices/stmds5k.h"
#include "devices/deasda.h"
#include "devices/dems300.h"
#include "devices/omrg5.h"
#include "devices/ph3lm2rm.h"

/**
 * @brief Global slave-type registry.
 *
 * Each entry describes one supported EtherCAT slave model.  Fields:
 *   - @c type            – unique @c LCEC_SLAVE_TYPE_T enum value used in XML config.
 *   - @c vid             – EtherCAT vendor ID (must match the physical device).
 *   - @c pid             – EtherCAT product code (must match the physical device).
 *   - @c pdo_entry_count – number of PDO entries expected from this slave;
 *                          0 if the driver determines the count dynamically via @c proc_preinit.
 *   - @c is_fsoe_logic   – non-zero if the slave is an FSoE logic/gateway device.
 *   - @c proc_preinit    – optional callback invoked before PDO domain creation
 *                          to query or set the PDO count; NULL for most devices.
 *   - @c proc_init       – mandatory callback that creates HAL pins, registers PDO
 *                          entries, and sets up real-time read/write callbacks.
 *
 * The array is terminated by a sentinel whose @c type field is
 * @c lcecSlaveTypeInvalid.
 */
const lcec_typelist_t typelist[] = {
  // bus coupler
  { lcecSlaveTypeEK1100, LCEC_EK1100_VID, LCEC_EK1100_PID, LCEC_EK1100_PDOS, 0, NULL, NULL},
  { lcecSlaveTypeEK1101, LCEC_EK1101_VID, LCEC_EK1101_PID, LCEC_EK1101_PDOS, 0, NULL, NULL},
  { lcecSlaveTypeEK1110, LCEC_EK1110_VID, LCEC_EK1110_PID, LCEC_EK1110_PDOS, 0, NULL, NULL},
  { lcecSlaveTypeEK1122, LCEC_EK1122_VID, LCEC_EK1122_PID, LCEC_EK1122_PDOS, 0, NULL, NULL},

  // AX5000 servo drives
  { lcecSlaveTypeAX5101, LCEC_AX5100_VID, LCEC_AX5101_PID, 0, 0, lcec_ax5100_preinit, lcec_ax5100_init},
  { lcecSlaveTypeAX5103, LCEC_AX5100_VID, LCEC_AX5103_PID, 0, 0, lcec_ax5100_preinit, lcec_ax5100_init},
  { lcecSlaveTypeAX5106, LCEC_AX5100_VID, LCEC_AX5106_PID, 0, 0, lcec_ax5100_preinit, lcec_ax5100_init},
  { lcecSlaveTypeAX5112, LCEC_AX5100_VID, LCEC_AX5112_PID, 0, 0, lcec_ax5100_preinit, lcec_ax5100_init},
  { lcecSlaveTypeAX5118, LCEC_AX5100_VID, LCEC_AX5118_PID, 0, 0, lcec_ax5100_preinit, lcec_ax5100_init},
  { lcecSlaveTypeAX5203, LCEC_AX5200_VID, LCEC_AX5203_PID, 0, 0, lcec_ax5200_preinit, lcec_ax5200_init},
  { lcecSlaveTypeAX5206, LCEC_AX5200_VID, LCEC_AX5206_PID, 0, 0, lcec_ax5200_preinit, lcec_ax5200_init},

  // digital in
  { lcecSlaveTypeEL1002, LCEC_EL1xxx_VID, LCEC_EL1002_PID, LCEC_EL1002_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1004, LCEC_EL1xxx_VID, LCEC_EL1004_PID, LCEC_EL1004_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1008, LCEC_EL1xxx_VID, LCEC_EL1008_PID, LCEC_EL1008_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1012, LCEC_EL1xxx_VID, LCEC_EL1012_PID, LCEC_EL1012_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1014, LCEC_EL1xxx_VID, LCEC_EL1014_PID, LCEC_EL1014_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1018, LCEC_EL1xxx_VID, LCEC_EL1018_PID, LCEC_EL1018_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1024, LCEC_EL1xxx_VID, LCEC_EL1024_PID, LCEC_EL1024_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1034, LCEC_EL1xxx_VID, LCEC_EL1034_PID, LCEC_EL1034_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1084, LCEC_EL1xxx_VID, LCEC_EL1084_PID, LCEC_EL1084_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1088, LCEC_EL1xxx_VID, LCEC_EL1088_PID, LCEC_EL1088_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1094, LCEC_EL1xxx_VID, LCEC_EL1094_PID, LCEC_EL1094_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1098, LCEC_EL1xxx_VID, LCEC_EL1098_PID, LCEC_EL1098_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1104, LCEC_EL1xxx_VID, LCEC_EL1104_PID, LCEC_EL1104_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1114, LCEC_EL1xxx_VID, LCEC_EL1114_PID, LCEC_EL1114_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1124, LCEC_EL1xxx_VID, LCEC_EL1124_PID, LCEC_EL1124_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1134, LCEC_EL1xxx_VID, LCEC_EL1134_PID, LCEC_EL1134_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1144, LCEC_EL1xxx_VID, LCEC_EL1144_PID, LCEC_EL1144_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1252, LCEC_EL1252_VID, LCEC_EL1252_PID, LCEC_EL1252_PDOS, 0, NULL, lcec_el1252_init},  // 2 fast channels with timestamp
  { lcecSlaveTypeEL1808, LCEC_EL1xxx_VID, LCEC_EL1808_PID, LCEC_EL1808_PDOS, 0, NULL, lcec_el1xxx_init},
  { lcecSlaveTypeEL1809, LCEC_EL1xxx_VID, LCEC_EL1809_PID, LCEC_EL1809_PDOS, 0, NULL, lcec_el1xxx_init},
	{ lcecSlaveTypeEL1819, LCEC_EL1xxx_VID, LCEC_EL1819_PID, LCEC_EL1819_PDOS, 0, NULL, lcec_el1xxx_init},

  // digital out
  { lcecSlaveTypeEL2002, LCEC_EL2xxx_VID, LCEC_EL2002_PID, LCEC_EL2002_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2004, LCEC_EL2xxx_VID, LCEC_EL2004_PID, LCEC_EL2004_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2008, LCEC_EL2xxx_VID, LCEC_EL2008_PID, LCEC_EL2008_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2022, LCEC_EL2xxx_VID, LCEC_EL2022_PID, LCEC_EL2022_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2024, LCEC_EL2xxx_VID, LCEC_EL2024_PID, LCEC_EL2024_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2032, LCEC_EL2xxx_VID, LCEC_EL2032_PID, LCEC_EL2032_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2034, LCEC_EL2xxx_VID, LCEC_EL2034_PID, LCEC_EL2034_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2042, LCEC_EL2xxx_VID, LCEC_EL2042_PID, LCEC_EL2042_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2084, LCEC_EL2xxx_VID, LCEC_EL2084_PID, LCEC_EL2084_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2088, LCEC_EL2xxx_VID, LCEC_EL2088_PID, LCEC_EL2088_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2124, LCEC_EL2xxx_VID, LCEC_EL2124_PID, LCEC_EL2124_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2202, LCEC_EL2202_VID, LCEC_EL2202_PID, LCEC_EL2202_PDOS, 0, NULL, lcec_el2202_init}, // 2 fast channels with tristate
  { lcecSlaveTypeEL2612, LCEC_EL2xxx_VID, LCEC_EL2612_PID, LCEC_EL2612_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2622, LCEC_EL2xxx_VID, LCEC_EL2622_PID, LCEC_EL2622_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2634, LCEC_EL2xxx_VID, LCEC_EL2634_PID, LCEC_EL2634_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2652, LCEC_EL2xxx_VID, LCEC_EL2652_PID, LCEC_EL2652_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2808, LCEC_EL2xxx_VID, LCEC_EL2808_PID, LCEC_EL2808_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2798, LCEC_EL2xxx_VID, LCEC_EL2798_PID, LCEC_EL2798_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEL2809, LCEC_EL2xxx_VID, LCEC_EL2809_PID, LCEC_EL2809_PDOS, 0, NULL, lcec_el2xxx_init},

  { lcecSlaveTypeEP2008, LCEC_EL2xxx_VID, LCEC_EP2008_PID, LCEC_EP2008_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEP2028, LCEC_EL2xxx_VID, LCEC_EP2028_PID, LCEC_EP2028_PDOS, 0, NULL, lcec_el2xxx_init},
  { lcecSlaveTypeEP2809, LCEC_EL2xxx_VID, LCEC_EP2809_PID, LCEC_EP2809_PDOS, 0, NULL, lcec_el2xxx_init},

  // digital in/out
  { lcecSlaveTypeEL1859, LCEC_EL1859_VID, LCEC_EL1859_PID, LCEC_EL1859_PDOS, 0, NULL, lcec_el1859_init},

  // analog in, 2ch, 16 bits
  { lcecSlaveTypeEL3102, LCEC_EL31x2_VID, LCEC_EL3102_PID, LCEC_EL31x2_PDOS, 0, NULL, lcec_el31x2_init},
  { lcecSlaveTypeEL3112, LCEC_EL31x2_VID, LCEC_EL3112_PID, LCEC_EL31x2_PDOS, 0, NULL, lcec_el31x2_init},
  { lcecSlaveTypeEL3122, LCEC_EL31x2_VID, LCEC_EL3122_PID, LCEC_EL31x2_PDOS, 0, NULL, lcec_el31x2_init},
  { lcecSlaveTypeEL3142, LCEC_EL31x2_VID, LCEC_EL3142_PID, LCEC_EL31x2_PDOS, 0, NULL, lcec_el31x2_init},
  { lcecSlaveTypeEL3152, LCEC_EL31x2_VID, LCEC_EL3152_PID, LCEC_EL31x2_PDOS, 0, NULL, lcec_el31x2_init},
  { lcecSlaveTypeEL3162, LCEC_EL31x2_VID, LCEC_EL3162_PID, LCEC_EL31x2_PDOS, 0, NULL, lcec_el31x2_init},

  // analog in, 4ch, 16 bits
  { lcecSlaveTypeEL3164, LCEC_EL31x4_VID, LCEC_EL3164_PID, LCEC_EL31x4_PDOS, 0, NULL, lcec_el31x4_init},
  { lcecSlaveTypeEL3204, LCEC_EL32x4_VID, LCEC_EL3204_PID, LCEC_EL32x4_PDOS, 0, NULL, lcec_el32x4_init},

  // analog in, 5ch, 16 bits
  { lcecSlaveTypeEL3255, LCEC_EL3255_VID, LCEC_EL3255_PID, LCEC_EL3255_PDOS, 0, NULL, lcec_el3255_init},

  // analog in, 4ch, 16 bits (thermocouple)
  { lcecSlaveTypeEL3314, LCEC_EL3314_VID, LCEC_EL3314_PID, LCEC_EL3314_PDOS, 0, NULL, lcec_el3314_init},

  // analog in, 4ch, 16 bits (relative pressure)
  { lcecSlaveTypeEM3712, LCEC_EM3712_VID, LCEC_EM3712_PID, LCEC_EM3712_PDOS, 0, NULL, lcec_em3712_init},

  // analog out, 1ch, 12 bits
  { lcecSlaveTypeEL4001, LCEC_EL40x1_VID, LCEC_EL4001_PID, LCEC_EL40x1_PDOS, 0, NULL, lcec_el40x1_init},
  { lcecSlaveTypeEL4011, LCEC_EL40x1_VID, LCEC_EL4011_PID, LCEC_EL40x1_PDOS, 0, NULL, lcec_el40x1_init},
  { lcecSlaveTypeEL4021, LCEC_EL40x1_VID, LCEC_EL4021_PID, LCEC_EL40x1_PDOS, 0, NULL, lcec_el40x1_init},
  { lcecSlaveTypeEL4031, LCEC_EL40x1_VID, LCEC_EL4031_PID, LCEC_EL40x1_PDOS, 0, NULL, lcec_el40x1_init},

  // analog out, 2ch, 12 bits
  { lcecSlaveTypeEL4002, LCEC_EL40x2_VID, LCEC_EL4002_PID, LCEC_EL40x2_PDOS, 0, NULL, lcec_el40x2_init},
  { lcecSlaveTypeEL4012, LCEC_EL40x2_VID, LCEC_EL4012_PID, LCEC_EL40x2_PDOS, 0, NULL, lcec_el40x2_init},
  { lcecSlaveTypeEL4022, LCEC_EL40x2_VID, LCEC_EL4022_PID, LCEC_EL40x2_PDOS, 0, NULL, lcec_el40x2_init},
  { lcecSlaveTypeEL4032, LCEC_EL40x2_VID, LCEC_EL4032_PID, LCEC_EL40x2_PDOS, 0, NULL, lcec_el40x2_init},

  // analog out, 2ch, 16 bits
  { lcecSlaveTypeEL4102, LCEC_EL41x2_VID, LCEC_EL4102_PID, LCEC_EL41x2_PDOS, 0, NULL, lcec_el41x2_init},
  { lcecSlaveTypeEL4112, LCEC_EL41x2_VID, LCEC_EL4112_PID, LCEC_EL41x2_PDOS, 0, NULL, lcec_el41x2_init},
  { lcecSlaveTypeEL4122, LCEC_EL41x2_VID, LCEC_EL4122_PID, LCEC_EL41x2_PDOS, 0, NULL, lcec_el41x2_init},
  { lcecSlaveTypeEL4132, LCEC_EL41x2_VID, LCEC_EL4132_PID, LCEC_EL41x2_PDOS, 0, NULL, lcec_el41x2_init},

  // analog out, 4ch, 16 bits
  { lcecSlaveTypeEL4104, LCEC_EL41x4_VID, LCEC_EL4104_PID, LCEC_EL41x4_PDOS, 0, NULL, lcec_el41x4_init},
  { lcecSlaveTypeEL4134, LCEC_EL41x4_VID, LCEC_EL4134_PID, LCEC_EL41x4_PDOS, 0, NULL, lcec_el41x4_init},

  // analog out, 8ch, 12 bits
  { lcecSlaveTypeEL4008, LCEC_EL40x8_VID, LCEC_EL4008_PID, LCEC_EL40x8_PDOS, 0, NULL, lcec_el40x8_init},
  { lcecSlaveTypeEL4018, LCEC_EL40x8_VID, LCEC_EL4018_PID, LCEC_EL40x8_PDOS, 0, NULL, lcec_el40x8_init},
  { lcecSlaveTypeEL4028, LCEC_EL40x8_VID, LCEC_EL4028_PID, LCEC_EL40x8_PDOS, 0, NULL, lcec_el40x8_init},
  { lcecSlaveTypeEL4038, LCEC_EL40x8_VID, LCEC_EL4038_PID, LCEC_EL40x8_PDOS, 0, NULL, lcec_el40x8_init},

  // encoder inputs
  { lcecSlaveTypeEL5002, LCEC_EL5002_VID, LCEC_EL5002_PID, LCEC_EL5002_PDOS, 0, NULL, lcec_el5002_init},
  { lcecSlaveTypeEL5021, LCEC_EL5021_VID, LCEC_EL5021_PID, LCEC_EL5021_PDOS, 0, NULL, lcec_el5021_init},
  { lcecSlaveTypeEL5032, LCEC_EL5032_VID, LCEC_EL5032_PID, LCEC_EL5032_PDOS, 0, NULL, lcec_el5032_init},
  { lcecSlaveTypeEL5101, LCEC_EL5101_VID, LCEC_EL5101_PID, LCEC_EL5101_PDOS, 0, NULL, lcec_el5101_init},
  { lcecSlaveTypeEL5151, LCEC_EL5151_VID, LCEC_EL5151_PID, LCEC_EL5151_PDOS, 0, NULL, lcec_el5151_init},
  { lcecSlaveTypeEL5122, LCEC_EL5122_VID, LCEC_EL5122_PID, LCEC_EL5122_PDOS, 0, NULL, lcec_el5122_init},
  { lcecSlaveTypeEL5152, LCEC_EL5152_VID, LCEC_EL5152_PID, LCEC_EL5152_PDOS, 0, NULL, lcec_el5152_init},

  // pulse train (stepper) output
  { lcecSlaveTypeEL2521, LCEC_EL2521_VID, LCEC_EL2521_PID, LCEC_EL2521_PDOS, 0, NULL, lcec_el2521_init},

  // stepper
  { lcecSlaveTypeEL7031, LCEC_EL70x1_VID, LCEC_EL7031_PID, LCEC_EL70x1_PDOS, 0, NULL, lcec_el7031_init},
  { lcecSlaveTypeEL7041_0052, LCEC_EL70x1_VID, LCEC_EL7041_0052_PID, LCEC_EL70x1_PDOS, 0, NULL, lcec_el7041_0052_init},
  { lcecSlaveTypeEL7041_1000, LCEC_EL7041_1000_VID, LCEC_EL7041_1000_PID, LCEC_EL7041_1000_PDOS, 0, NULL, lcec_el7041_1000_init},

  // ac servo
  { lcecSlaveTypeEL7201_9014, LCEC_EL7211_VID, LCEC_EL7201_9014_PID, LCEC_EL7201_9014_PDOS, 0, NULL, lcec_el7201_9014_init},
  { lcecSlaveTypeEL7211, LCEC_EL7211_VID, LCEC_EL7211_PID, LCEC_EL7211_PDOS, 0, NULL, lcec_el7211_init},
  { lcecSlaveTypeEL7221, LCEC_EL7211_VID, LCEC_EL7221_PID, LCEC_EL7211_PDOS, 0, NULL, lcec_el7211_init},

  // dc servo
  { lcecSlaveTypeEL7342, LCEC_EL7342_VID, LCEC_EL7342_PID, LCEC_EL7342_PDOS, 0, NULL, lcec_el7342_init},

  // BLDC
  { lcecSlaveTypeEL7411, LCEC_EL7411_VID, LCEC_EL7411_PID, LCEC_EL7411_PDOS, 0, NULL, lcec_el7411_init},

  // power supply
  { lcecSlaveTypeEL9505, LCEC_EL95xx_VID, LCEC_EL9505_PID, LCEC_EL95xx_PDOS, 0, NULL, lcec_el95xx_init},
  { lcecSlaveTypeEL9508, LCEC_EL95xx_VID, LCEC_EL9508_PID, LCEC_EL95xx_PDOS, 0, NULL, lcec_el95xx_init},
  { lcecSlaveTypeEL9510, LCEC_EL95xx_VID, LCEC_EL9510_PID, LCEC_EL95xx_PDOS, 0, NULL, lcec_el95xx_init},
  { lcecSlaveTypeEL9512, LCEC_EL95xx_VID, LCEC_EL9512_PID, LCEC_EL95xx_PDOS, 0, NULL, lcec_el95xx_init},
  { lcecSlaveTypeEL9515, LCEC_EL95xx_VID, LCEC_EL9515_PID, LCEC_EL95xx_PDOS, 0, NULL, lcec_el95xx_init},
  { lcecSlaveTypeEL9576, LCEC_EL95xx_VID, LCEC_EL9576_PID, LCEC_EL95xx_PDOS, 0, NULL, lcec_el95xx_init},

  // FSoE devices
  { lcecSlaveTypeEL6900, LCEC_EL6900_VID, LCEC_EL6900_PID, 0, 1, lcec_el6900_preinit, lcec_el6900_init},
  { lcecSlaveTypeEL1918_LOGIC, LCEC_EL1918_LOGIC_VID, LCEC_EL1918_LOGIC_PID, 0, 1, lcec_el1918_logic_preinit, lcec_el1918_logic_init},
  { lcecSlaveTypeEL1904, LCEC_EL1904_VID, LCEC_EL1904_PID, LCEC_EL1904_PDOS, 0, lcec_el1904_preinit, lcec_el1904_init},
  { lcecSlaveTypeEL2904, LCEC_EL2904_VID, LCEC_EL2904_PID, LCEC_EL2904_PDOS, 0, lcec_el2904_preinit, lcec_el2904_init},
  { lcecSlaveTypeAX5805, LCEC_AX5805_VID, LCEC_AX5805_PID, 0, 0, lcec_ax5805_preinit, lcec_ax5805_init},

  // multi axis interface
  { lcecSlaveTypeEM7004, LCEC_EM7004_VID, LCEC_EM7004_PID, LCEC_EM7004_PDOS, 0, NULL, lcec_em7004_init},

  // stoeber MDS5000 series
  { lcecSlaveTypeStMDS5k, LCEC_STMDS5K_VID, LCEC_STMDS5K_PID, 0, 0, lcec_stmds5k_preinit, lcec_stmds5k_init},

  // Delta ASDA series
  { lcecSlaveTypeDeASDA, LCEC_DEASDA_VID, LCEC_DEASDA_PID, LCEC_DEASDA_PDOS, 0, NULL, lcec_deasda_init},

  // Delta MS/MH300 series
  { lcecSlaveTypeDeMS300, LCEC_DEMS300_VID, LCEC_DEMS300_PID, LCEC_DEMS300_PDOS, 0, NULL, lcec_dems300_init},

  // Omron G5 series
  { lcecSlaveTypeOmrG5_KNA5L,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KNA5L_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN01L,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN01L_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN02L,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN02L_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN04L,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN04L_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN01H,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN01H_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN02H,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN02H_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN04H,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN04H_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN08H,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN08H_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN10H,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN10H_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN15H,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN15H_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN20H,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN20H_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN30H,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN30H_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN50H,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN50H_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN75H,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN75H_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN150H, LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN150H_ECT_PID, LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN06F,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN06F_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN10F,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN10F_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN15F,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN15F_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN20F,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN20F_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN30F,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN30F_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN50F,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN50F_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN75F,  LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN75F_ECT_PID,  LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},
  { lcecSlaveTypeOmrG5_KN150F, LCEC_OMRG5_VID, LCEC_OMRG5_R88D_KN150F_ECT_PID, LCEC_OMRG5_PDOS, 0, NULL, lcec_omrg5_init},

  // modusoft PH3LM2RM converter
  { lcecSlaveTypePh3LM2RM, LCEC_PH3LM2RM_VID, LCEC_PH3LM2RM_PID, LCEC_PH3LM2RM_PDOS, 0, NULL, lcec_ph3lm2rm_init},

  { lcecSlaveTypeInvalid }
};
