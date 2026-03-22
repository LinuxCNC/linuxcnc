/**
 * @file conf.c
 * @brief EtherCAT HAL configuration parser — main entry point.
 *
 * This file implements the @c lcec_conf userspace tool.  It:
 *  1. Initialises a HAL component and registers two output pins
 *     (@c lcec.conf.master-count and @c lcec.conf.slave-count).
 *  2. Reads and parses the EtherCAT bus topology from an XML configuration
 *     file supplied as the sole command-line argument.
 *  3. Serialises the parsed records into a POSIX shared-memory segment keyed
 *     by @ref LCEC_CONF_SHMEM_KEY so that the realtime @c lcec component can
 *     read them at startup.
 *  4. Calls @c hal_ready() and then waits for SIGINT or SIGTERM before
 *     cleaning up and exiting.
 *
 * The XML grammar handled here is:
 * @code
 *   <masters>
 *     <master idx="0" name="main" appTimePeriod="1000000" ...>
 *       <slave idx="0" type="EK1100" name="coupler"/>
 *       <slave idx="1" type="generic" vid="0x2" pid="0x3" configPdos="true">
 *         <dcConf assignActivate="0x300" sync0Cycle="*1" .../>
 *         <watchdog divider="1000" intervals="10"/>
 *         <sdoConfig idx="0x6040" subIdx="0x0">
 *           <sdoDataRaw data="06 00"/>
 *         </sdoConfig>
 *         <idnConfig drive="0" idn="32768" state="PREOP">
 *           <idnDataRaw data="01 00"/>
 *         </idnConfig>
 *         <syncManager idx="0" dir="out">
 *           <pdo idx="0x1600">
 *             <pdoEntry idx="0x6040" subIdx="0" bitLen="16" halType="u32" halPin="ctrl"/>
 *           </pdo>
 *         </syncManager>
 *         <modParam name="maxCurrent" value="1000"/>
 *       </slave>
 *     </master>
 *   </masters>
 * @endcode
 *
 * @copyright Copyright (C) 2012-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <expat.h>
#include <signal.h>
#include <sys/eventfd.h>

#include "rtapi.h"
#include "hal.h"

#include "lcec_compat.h"
#include "conf.h"
#include "conf_priv.h"

#include "devices/stmds5k.h"
#include "devices/el6900.h"
#include "devices/el1918_logic.h"
#include "devices/el5002.h"
#include "devices/el70x1.h"
#include "devices/el7411.h"

#include "classes/class_ax5.h"

/**
 * @brief Internal type tag for module parameter values.
 *
 * Used in @ref LCEC_CONF_MODPARAM_DESC_T to describe the expected value type
 * of a slave driver module parameter, so the parser knows how to convert the
 * XML string value before storing it in @ref LCEC_CONF_MODPARAM_VAL_T.
 */
typedef enum {
  MODPARAM_TYPE_BIT,     /**< Boolean (0/1, TRUE/FALSE). */
  MODPARAM_TYPE_U32,     /**< Unsigned 32-bit integer. */
  MODPARAM_TYPE_S32,     /**< Signed 32-bit integer. */
  MODPARAM_TYPE_FLOAT,   /**< Double-precision floating point. */
  MODPARAM_TYPE_STRING   /**< String (copied up to @ref LCEC_CONF_STR_MAXLEN). */
} LCEC_CONF_MODPARAM_TYPE_T;

/**
 * @brief Descriptor for a single module parameter accepted by a slave driver.
 *
 * Slave driver parameter tables are arrays of these descriptors, terminated
 * by an entry with @c name == NULL.  The @c id field is a driver-specific
 * integer constant (defined in the corresponding device header) that the
 * realtime component uses to dispatch the parameter value.
 */
typedef struct {
  const char *name;              /**< XML attribute name used to reference this parameter. */
  int id;                        /**< Driver-specific parameter identifier. */
  LCEC_CONF_MODPARAM_TYPE_T type; /**< Expected value type for parsing and validation. */
} LCEC_CONF_MODPARAM_DESC_T;

/**
 * @brief Maps an XML slave type name string to its enum value and parameter table.
 *
 * The global @c slaveTypes[] array is a NULL-terminated list of these entries.
 * The parser resolves the @c type attribute of a @c \<slave\> element by
 * searching this list.
 */
typedef struct {
  const char *name;                         /**< XML type attribute string (e.g. "EL1002"). */
  LCEC_SLAVE_TYPE_T type;                   /**< Corresponding @ref LCEC_SLAVE_TYPE_T enum value. */
  const LCEC_CONF_MODPARAM_DESC_T *modParams; /**< NULL-terminated parameter descriptor table,
                                               *   or NULL if this type has no module parameters. */
} LCEC_CONF_TYPELIST_T;

/**
 * @brief HAL pin data for the lcec_conf component.
 *
 * Holds pointers to the two HAL output pins that lcec_conf registers so that
 * other HAL components (or the user) can read the number of configured
 * masters and slaves.
 */
typedef struct {
  hal_u32_t *master_count;  /**< Pointer to the @c lcec.conf.master-count HAL pin. */
  hal_u32_t *slave_count;   /**< Pointer to the @c lcec.conf.slave-count HAL pin. */
} LCEC_CONF_HAL_T;

static const LCEC_CONF_MODPARAM_DESC_T slaveStMDS5kParams[] = {
  { "isMultiturn", LCEC_STMDS5K_PARAM_MULTITURN, MODPARAM_TYPE_BIT } ,
  { "extEnc", LCEC_STMDS5K_PARAM_EXTENC, MODPARAM_TYPE_U32 } ,
  { NULL }
};

static const LCEC_CONF_MODPARAM_DESC_T slaveEL6900Params[] = {
  { "fsoeSlaveIdx", LCEC_EL6900_PARAM_SLAVEID, MODPARAM_TYPE_U32 } ,
  { "stdInName", LCEC_EL6900_PARAM_STDIN_NAME, MODPARAM_TYPE_STRING } ,
  { "stdOutName", LCEC_EL6900_PARAM_STDOUT_NAME, MODPARAM_TYPE_STRING } ,
  { NULL }
};

static const LCEC_CONF_MODPARAM_DESC_T slaveEL1918_LOGICParams[] = {
  { "fsoeSlaveIdx", LCEC_EL1918_LOGIC_PARAM_SLAVEID, MODPARAM_TYPE_U32 } ,
  { "stdInName", LCEC_EL1918_LOGIC_PARAM_STDIN_NAME, MODPARAM_TYPE_STRING } ,
  { "stdOutName", LCEC_EL1918_LOGIC_PARAM_STDOUT_NAME, MODPARAM_TYPE_STRING } ,
  { NULL }
};

static const LCEC_CONF_MODPARAM_DESC_T slaveEL70x1Params[] = {
  { "maxCurrent", LCEC_EL70x1_PARAM_MAX_CURR, MODPARAM_TYPE_U32 } ,
  { "redCurrent", LCEC_EL70x1_PARAM_RED_CURR, MODPARAM_TYPE_U32 } ,
  { "nomVoltage", LCEC_EL70x1_PARAM_NOM_VOLT, MODPARAM_TYPE_U32 } ,
  { "coilRes", LCEC_EL70x1_PARAM_COIL_RES, MODPARAM_TYPE_U32 } ,
  { "motorEMF", LCEC_EL70x1_PARAM_MOTOR_EMF, MODPARAM_TYPE_U32 } ,
  { NULL }
};

static const LCEC_CONF_MODPARAM_DESC_T slaveEL7411Params[] = {

  { "dcLinkNominal", LCEC_EL7411_PARAM_DCLINK_NOM, MODPARAM_TYPE_U32 } ,
  { "dcLinkMin", LCEC_EL7411_PARAM_DCLINK_MIN, MODPARAM_TYPE_U32 } ,
  { "dcLinkMax", LCEC_EL7411_PARAM_DCLINK_MAX, MODPARAM_TYPE_U32 } ,
  { "maxCurrent", LCEC_EL7411_PARAM_MAX_CURR, MODPARAM_TYPE_U32 } ,
  { "ratedCurrent", LCEC_EL7411_PARAM_RATED_CURR, MODPARAM_TYPE_U32 } ,
  { "ratedVoltage", LCEC_EL7411_PARAM_RATED_VOLT, MODPARAM_TYPE_U32 } ,
  { "polePairs", LCEC_EL7411_PARAM_POLE_PAIRS, MODPARAM_TYPE_U32 } ,
  { "coilRes", LCEC_EL7411_PARAM_RESISTANCE, MODPARAM_TYPE_U32 } ,
  { "coilInd", LCEC_EL7411_PARAM_INDUCTANCE, MODPARAM_TYPE_U32 } ,
  { "torqueConst", LCEC_EL7411_PARAM_TOURQUE_CONST, MODPARAM_TYPE_U32 } ,
  { "voltageConst", LCEC_EL7411_PARAM_VOLTAGE_CONST, MODPARAM_TYPE_U32 } ,
  { "rotorInertia", LCEC_EL7411_PARAM_ROTOR_INERTIA, MODPARAM_TYPE_U32 } ,
  { "maxSpeed", LCEC_EL7411_PARAM_MAX_SPEED, MODPARAM_TYPE_U32 } ,
  { "ratedSpeed", LCEC_EL7411_PARAM_RATED_SPEED, MODPARAM_TYPE_U32 } ,
  { "thermalTimeConst", LCEC_EL7411_PARAM_TH_TIME_CONST, MODPARAM_TYPE_U32 } ,
  { "hallVoltage", LCEC_EL7411_PARAM_HALL_VOLT, MODPARAM_TYPE_U32 } ,
  { "hallAdjust", LCEC_EL7411_PARAM_HALL_ADJUST, MODPARAM_TYPE_S32 } ,
  { NULL }
};

static const LCEC_CONF_MODPARAM_DESC_T slaveAX5Params[] = {
  { "enableFB2", LCEC_AX5_PARAM_ENABLE_FB2, MODPARAM_TYPE_BIT } ,
  { "enableDiag", LCEC_AX5_PARAM_ENABLE_DIAG, MODPARAM_TYPE_BIT } ,
  { NULL }
};

static const LCEC_CONF_MODPARAM_DESC_T slaveEL5002Params[] = {
  { "ch0DisFrameErr", LCEC_EL5002_PARAM_CH_0 | LCEC_EL5002_PARAM_DIS_FRAME_ERR, MODPARAM_TYPE_BIT } ,
  { "ch0EnPwrFailChk", LCEC_EL5002_PARAM_CH_0 | LCEC_EL5002_PARAM_EN_PWR_FAIL_CHK, MODPARAM_TYPE_BIT } ,
  { "ch0EnInhibitTime", LCEC_EL5002_PARAM_CH_0 | LCEC_EL5002_PARAM_EN_INHIBIT_TIME, MODPARAM_TYPE_BIT } ,
  { "ch0Coding", LCEC_EL5002_PARAM_CH_0 | LCEC_EL5002_PARAM_CODING, MODPARAM_TYPE_U32 } ,
  { "ch0Baudrate", LCEC_EL5002_PARAM_CH_0 | LCEC_EL5002_PARAM_BAUDRATE, MODPARAM_TYPE_U32 } ,
  { "ch0ClkJitComp", LCEC_EL5002_PARAM_CH_0 | LCEC_EL5002_PARAM_CLK_JIT_COMP, MODPARAM_TYPE_U32 } ,
  { "ch0FrameType", LCEC_EL5002_PARAM_CH_0 | LCEC_EL5002_PARAM_FRAME_TYPE, MODPARAM_TYPE_U32 } ,
  { "ch0FrameSize", LCEC_EL5002_PARAM_CH_0 | LCEC_EL5002_PARAM_FRAME_SIZE, MODPARAM_TYPE_U32 } ,
  { "ch0DataLen", LCEC_EL5002_PARAM_CH_0 | LCEC_EL5002_PARAM_DATA_LEN, MODPARAM_TYPE_U32 } ,
  { "ch0MinInhibitTime", LCEC_EL5002_PARAM_CH_0 | LCEC_EL5002_PARAM_MIN_INHIBIT_TIME, MODPARAM_TYPE_U32 } ,
  { "ch0NoClkBursts", LCEC_EL5002_PARAM_CH_0 | LCEC_EL5002_PARAM_NO_CLK_BURSTS, MODPARAM_TYPE_U32 } ,
  { "ch1DisFrameErr", LCEC_EL5002_PARAM_CH_1 | LCEC_EL5002_PARAM_DIS_FRAME_ERR, MODPARAM_TYPE_BIT } ,
  { "ch1EnPwrFailChk", LCEC_EL5002_PARAM_CH_1 | LCEC_EL5002_PARAM_EN_PWR_FAIL_CHK, MODPARAM_TYPE_BIT } ,
  { "ch1EnInhibitTime", LCEC_EL5002_PARAM_CH_1 | LCEC_EL5002_PARAM_EN_INHIBIT_TIME, MODPARAM_TYPE_BIT } ,
  { "ch1Coding", LCEC_EL5002_PARAM_CH_1 | LCEC_EL5002_PARAM_CODING, MODPARAM_TYPE_U32 } ,
  { "ch1Baudrate", LCEC_EL5002_PARAM_CH_1 | LCEC_EL5002_PARAM_BAUDRATE, MODPARAM_TYPE_U32 } ,
  { "ch1ClkJitComp", LCEC_EL5002_PARAM_CH_1 | LCEC_EL5002_PARAM_CLK_JIT_COMP, MODPARAM_TYPE_U32 } ,
  { "ch1FrameType", LCEC_EL5002_PARAM_CH_1 | LCEC_EL5002_PARAM_FRAME_TYPE, MODPARAM_TYPE_U32 } ,
  { "ch1FrameSize", LCEC_EL5002_PARAM_CH_1 | LCEC_EL5002_PARAM_FRAME_SIZE, MODPARAM_TYPE_U32 } ,
  { "ch1DataLen", LCEC_EL5002_PARAM_CH_1 | LCEC_EL5002_PARAM_DATA_LEN, MODPARAM_TYPE_U32 } ,
  { "ch1MinInhibitTime", LCEC_EL5002_PARAM_CH_1 | LCEC_EL5002_PARAM_MIN_INHIBIT_TIME, MODPARAM_TYPE_U32 } ,
  { "ch1NoClkBursts", LCEC_EL5002_PARAM_CH_1 | LCEC_EL5002_PARAM_NO_CLK_BURSTS, MODPARAM_TYPE_U32 } ,
  { NULL }
};

static const LCEC_CONF_TYPELIST_T slaveTypes[] = {
  // bus coupler
  { "EK1100", lcecSlaveTypeEK1100, NULL },
  { "EK1101", lcecSlaveTypeEK1101, NULL },
  { "EK1110", lcecSlaveTypeEK1110, NULL },
  { "EK1122", lcecSlaveTypeEK1122, NULL },

  // generic device
  { "generic", lcecSlaveTypeGeneric, NULL },

  // AX5000 servo drives
  { "AX5101", lcecSlaveTypeAX5101, slaveAX5Params },
  { "AX5103", lcecSlaveTypeAX5103, slaveAX5Params },
  { "AX5106", lcecSlaveTypeAX5106, slaveAX5Params },
  { "AX5112", lcecSlaveTypeAX5112, slaveAX5Params },
  { "AX5118", lcecSlaveTypeAX5118, slaveAX5Params },
  { "AX5203", lcecSlaveTypeAX5203, slaveAX5Params },
  { "AX5206", lcecSlaveTypeAX5206, slaveAX5Params },

  // digital in
  { "EL1002", lcecSlaveTypeEL1002, NULL },
  { "EL1004", lcecSlaveTypeEL1004, NULL },
  { "EL1008", lcecSlaveTypeEL1008, NULL },
  { "EL1012", lcecSlaveTypeEL1012, NULL },
  { "EL1014", lcecSlaveTypeEL1014, NULL },
  { "EL1018", lcecSlaveTypeEL1018, NULL },
  { "EL1024", lcecSlaveTypeEL1024, NULL },
  { "EL1034", lcecSlaveTypeEL1034, NULL },
  { "EL1084", lcecSlaveTypeEL1084, NULL },
  { "EL1088", lcecSlaveTypeEL1088, NULL },
  { "EL1094", lcecSlaveTypeEL1094, NULL },
  { "EL1098", lcecSlaveTypeEL1098, NULL },
  { "EL1104", lcecSlaveTypeEL1104, NULL },
  { "EL1114", lcecSlaveTypeEL1114, NULL },
  { "EL1124", lcecSlaveTypeEL1124, NULL },
  { "EL1134", lcecSlaveTypeEL1134, NULL },
  { "EL1144", lcecSlaveTypeEL1144, NULL },
  { "EL1252", lcecSlaveTypeEL1252, NULL },
  { "EL1808", lcecSlaveTypeEL1808, NULL },
  { "EL1809", lcecSlaveTypeEL1809, NULL },
  { "EL1819", lcecSlaveTypeEL1819, NULL },

  // digital out
  { "EL2002", lcecSlaveTypeEL2002, NULL },
  { "EL2004", lcecSlaveTypeEL2004, NULL },
  { "EL2008", lcecSlaveTypeEL2008, NULL },
  { "EL2022", lcecSlaveTypeEL2022, NULL },
  { "EL2024", lcecSlaveTypeEL2024, NULL },
  { "EL2032", lcecSlaveTypeEL2032, NULL },
  { "EL2034", lcecSlaveTypeEL2034, NULL },
  { "EL2042", lcecSlaveTypeEL2042, NULL },
  { "EL2084", lcecSlaveTypeEL2084, NULL },
  { "EL2088", lcecSlaveTypeEL2088, NULL },
  { "EL2124", lcecSlaveTypeEL2124, NULL },
  { "EL2202", lcecSlaveTypeEL2202, NULL },
  { "EL2612", lcecSlaveTypeEL2612, NULL },
  { "EL2622", lcecSlaveTypeEL2622, NULL },
  { "EL2634", lcecSlaveTypeEL2634, NULL },
  { "EL2652", lcecSlaveTypeEL2652, NULL },
  { "EL2808", lcecSlaveTypeEL2808, NULL },
  { "EL2798", lcecSlaveTypeEL2798, NULL },
  { "EL2809", lcecSlaveTypeEL2809, NULL },

  { "EP2008", lcecSlaveTypeEP2008, NULL },
  { "EP2028", lcecSlaveTypeEP2028, NULL },
  { "EP2809", lcecSlaveTypeEP2809, NULL },

  // digital in(out
  { "EL1859", lcecSlaveTypeEL1859, NULL },

  // analog in, 2ch, 16 bits
  { "EL3102", lcecSlaveTypeEL3102, NULL },
  { "EL3112", lcecSlaveTypeEL3112, NULL },
  { "EL3122", lcecSlaveTypeEL3122, NULL },
  { "EL3142", lcecSlaveTypeEL3142, NULL },
  { "EL3152", lcecSlaveTypeEL3152, NULL },
  { "EL3162", lcecSlaveTypeEL3162, NULL },

  // analog in, 4ch, 16 bits
  { "EL3164", lcecSlaveTypeEL3164, NULL },
  { "EL3204", lcecSlaveTypeEL3204, NULL },

  // analog in, 5ch, 16 bits
  { "EL3255", lcecSlaveTypeEL3255, NULL },

  // analog in, 4ch, 16 bits
  { "EL3314", lcecSlaveTypeEL3314, NULL },

  // analog in, 4ch, 16 bits
  { "EM3712", lcecSlaveTypeEM3712, NULL },

  // analog out, 1ch, 12 bits
  { "EL4001", lcecSlaveTypeEL4001, NULL },
  { "EL4011", lcecSlaveTypeEL4011, NULL },
  { "EL4021", lcecSlaveTypeEL4021, NULL },
  { "EL4031", lcecSlaveTypeEL4031, NULL },

  // analog out, 2ch, 12 bits
  { "EL4002", lcecSlaveTypeEL4002, NULL },
  { "EL4012", lcecSlaveTypeEL4012, NULL },
  { "EL4022", lcecSlaveTypeEL4022, NULL },
  { "EL4032", lcecSlaveTypeEL4032, NULL },

  // analog out, 2ch, 16 bits
  { "EL4102", lcecSlaveTypeEL4102, NULL },
  { "EL4112", lcecSlaveTypeEL4112, NULL },
  { "EL4122", lcecSlaveTypeEL4122, NULL },
  { "EL4132", lcecSlaveTypeEL4132, NULL },

  // analog out, 4ch, 16 bits
  { "EL4104", lcecSlaveTypeEL4104, NULL },
  { "EL4134", lcecSlaveTypeEL4134, NULL },

  // analog out, 8ch, 12 bits
  { "EL4008", lcecSlaveTypeEL4008, NULL },
  { "EL4018", lcecSlaveTypeEL4018, NULL },
  { "EL4028", lcecSlaveTypeEL4028, NULL },
  { "EL4038", lcecSlaveTypeEL4038, NULL },

  // encoder inputs
  { "EL5002", lcecSlaveTypeEL5002, slaveEL5002Params },
  { "EL5021", lcecSlaveTypeEL5021, NULL },
  { "EL5032", lcecSlaveTypeEL5032, NULL },
  { "EL5101", lcecSlaveTypeEL5101, NULL },
  { "EL5151", lcecSlaveTypeEL5151, NULL },
  { "EL5122", lcecSlaveTypeEL5122, NULL },
  { "EL5152", lcecSlaveTypeEL5152, NULL },

  // pulse train (stepper) output
  { "EL2521", lcecSlaveTypeEL2521, NULL },

  // stepper
  { "EL7031", lcecSlaveTypeEL7031, slaveEL70x1Params },
  { "EL7041-0052", lcecSlaveTypeEL7041_0052, slaveEL70x1Params },
  { "EL7041-1000", lcecSlaveTypeEL7041_1000, NULL },

  // ac servo
  { "EL7201-9014", lcecSlaveTypeEL7201_9014, NULL },
  { "EL7211", lcecSlaveTypeEL7211, NULL },
  { "EL7221", lcecSlaveTypeEL7221, NULL },

  // dc servo
  { "EL7342", lcecSlaveTypeEL7342, NULL },

  // BLDC
  { "EL7411", lcecSlaveTypeEL7411, slaveEL7411Params },

  // power suppply
  { "EL9505", lcecSlaveTypeEL9505, NULL },
  { "EL9508", lcecSlaveTypeEL9508, NULL },
  { "EL9510", lcecSlaveTypeEL9510, NULL },
  { "EL9512", lcecSlaveTypeEL9512, NULL },
  { "EL9515", lcecSlaveTypeEL9515, NULL },
  { "EL9576", lcecSlaveTypeEL9576, NULL },

  // FSoE devices
  { "EL6900", lcecSlaveTypeEL6900, slaveEL6900Params },
  { "EL1918_LOGIC", lcecSlaveTypeEL1918_LOGIC, slaveEL1918_LOGICParams },
  { "EL1904", lcecSlaveTypeEL1904, NULL },
  { "EL2904", lcecSlaveTypeEL2904, NULL },
  { "AX5805", lcecSlaveTypeAX5805, NULL },

  // multi axis interface
  { "EM7004", lcecSlaveTypeEM7004, NULL },

  // stoeber MDS5000 series
  { "StMDS5k", lcecSlaveTypeStMDS5k, slaveStMDS5kParams },

  // Delta ASDA series
  { "DeASDA", lcecSlaveTypeDeASDA, NULL },

  // Delta MS/MH300 series
  { "DeMS300", lcecSlaveTypeDeMS300, NULL },

  // Omron G5 series
  { "R88D-KNA5L-ECT", lcecSlaveTypeOmrG5_KNA5L, NULL },
  { "R88D-KN01L-ECT", lcecSlaveTypeOmrG5_KN01L, NULL },
  { "R88D-KN02L-ECT", lcecSlaveTypeOmrG5_KN02L, NULL },
  { "R88D-KN04L-ECT", lcecSlaveTypeOmrG5_KN04L, NULL },
  { "R88D-KN01H-ECT", lcecSlaveTypeOmrG5_KN01H, NULL },
  { "R88D-KN02H-ECT", lcecSlaveTypeOmrG5_KN02H, NULL },
  { "R88D-KN04H-ECT", lcecSlaveTypeOmrG5_KN04H, NULL },
  { "R88D-KN08H-ECT", lcecSlaveTypeOmrG5_KN08H, NULL },
  { "R88D-KN10H-ECT", lcecSlaveTypeOmrG5_KN10H, NULL },
  { "R88D-KN15H-ECT", lcecSlaveTypeOmrG5_KN15H, NULL },
  { "R88D-KN20H-ECT", lcecSlaveTypeOmrG5_KN20H, NULL },
  { "R88D-KN30H-ECT", lcecSlaveTypeOmrG5_KN30H, NULL },
  { "R88D-KN50H-ECT", lcecSlaveTypeOmrG5_KN50H, NULL },
  { "R88D-KN75H-ECT", lcecSlaveTypeOmrG5_KN75H, NULL },
  { "R88D-KN150H-ECT", lcecSlaveTypeOmrG5_KN150H, NULL },
  { "R88D-KN06F-ECT", lcecSlaveTypeOmrG5_KN06F, NULL },
  { "R88D-KN10F-ECT", lcecSlaveTypeOmrG5_KN10F, NULL },
  { "R88D-KN15F-ECT", lcecSlaveTypeOmrG5_KN15F, NULL },
  { "R88D-KN20F-ECT", lcecSlaveTypeOmrG5_KN20F, NULL },
  { "R88D-KN30F-ECT", lcecSlaveTypeOmrG5_KN30F, NULL },
  { "R88D-KN50F-ECT", lcecSlaveTypeOmrG5_KN50F, NULL },
  { "R88D-KN75F-ECT", lcecSlaveTypeOmrG5_KN75F, NULL },
  { "R88D-KN150F-ECT", lcecSlaveTypeOmrG5_KN150F, NULL },

  // modusoft PH3LM2RM converter
  { "Ph3LM2RM", lcecSlaveTypePh3LM2RM, NULL },

  { NULL }
};

static int hal_comp_id;       // HAL component identifier returned by hal_init()
static LCEC_CONF_HAL_T *conf_hal_data;  // HAL pin data block in shared memory
static int shmem_id;          // rtapi shared-memory segment identifier

static int exitEvent;         // eventfd file descriptor used to wake main() on signal

/**
 * @brief Full XML parser state for the top-level conf.c XML parse pass.
 *
 * The @c xml member MUST be first so that expat callbacks can safely cast
 * between @ref LCEC_CONF_XML_INST_T* and @ref LCEC_CONF_XML_STATE_T*.
 * The remaining fields track the objects currently being built.
 */
typedef struct {
  LCEC_CONF_XML_INST_T xml;               /**< Base expat instance; MUST be first. */

  LCEC_CONF_MASTER_T *currMaster;         /**< Master record currently being populated. */
  const LCEC_CONF_TYPELIST_T *currSlaveType; /**< Type-list entry for the current slave
                                              *   (provides its modParam descriptor table). */
  LCEC_CONF_SLAVE_T *currSlave;           /**< Slave record currently being populated. */
  LCEC_CONF_SYNCMANAGER_T *currSyncManager; /**< Sync-manager record currently being populated. */
  LCEC_CONF_PDO_T *currPdo;               /**< PDO record currently being populated. */
  LCEC_CONF_SDOCONF_T *currSdoConf;       /**< SDO config record currently being populated. */
  LCEC_CONF_IDNCONF_T *currIdnConf;       /**< IDN config record currently being populated. */
  LCEC_CONF_PDOENTRY_T *currPdoEntry;     /**< PDO entry record currently being populated. */
  uint8_t currComplexBitOffset;           /**< Running bit offset within the current complex entry. */

  LCEC_CONF_OUTBUF_T outputBuf;           /**< Dynamic output buffer accumulating all records. */
} LCEC_CONF_XML_STATE_T;

static void parseMasterAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseSlaveAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseDcConfAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseWatchdogAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseSdoConfigAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseIdnConfigAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseDataRawAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseInitCmdsAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseSyncManagerAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parsePdoAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parsePdoEntryAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseComplexEntryAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);
static void parseModParamAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr);

/** @brief State-transition table for the top-level XML configuration parser. */
static const LCEC_CONF_XML_HANLDER_T xml_states[] = {
  { "masters", lcecConfTypeNone, lcecConfTypeMasters, NULL, NULL },
  { "master", lcecConfTypeMasters, lcecConfTypeMaster, parseMasterAttrs, NULL },
  { "slave", lcecConfTypeMaster, lcecConfTypeSlave, parseSlaveAttrs, NULL },
  { "dcConf", lcecConfTypeSlave, lcecConfTypeDcConf, parseDcConfAttrs, NULL },
  { "watchdog", lcecConfTypeSlave, lcecConfTypeWatchdog, parseWatchdogAttrs, NULL },
  { "sdoConfig", lcecConfTypeSlave, lcecConfTypeSdoConfig, parseSdoConfigAttrs, NULL },
  { "sdoDataRaw", lcecConfTypeSdoConfig, lcecConfTypeSdoDataRaw, parseDataRawAttrs, NULL },
  { "idnConfig", lcecConfTypeSlave, lcecConfTypeIdnConfig, parseIdnConfigAttrs, NULL },
  { "idnDataRaw", lcecConfTypeIdnConfig, lcecConfTypeIdnDataRaw, parseDataRawAttrs, NULL },
  { "initCmds", lcecConfTypeSlave, lcecConfTypeInitCmds, parseInitCmdsAttrs, NULL },
  { "syncManager", lcecConfTypeSlave, lcecConfTypeSyncManager, parseSyncManagerAttrs, NULL },
  { "pdo", lcecConfTypeSyncManager, lcecConfTypePdo, parsePdoAttrs, NULL },
  { "pdoEntry", lcecConfTypePdo, lcecConfTypePdoEntry, parsePdoEntryAttrs, NULL },
  { "complexEntry", lcecConfTypePdoEntry, lcecConfTypeComplexEntry, parseComplexEntryAttrs, NULL },
  { "modParam", lcecConfTypeSlave, lcecConfTypeModParam, parseModParamAttrs, NULL },
  { "NULL", -1, -1, NULL, NULL }
};

static int parseSyncCycle(LCEC_CONF_XML_STATE_T *state, const char *nptr);

/**
 * @brief Signal handler for SIGINT and SIGTERM.
 *
 * Writes a non-zero 64-bit value to the @c exitEvent eventfd so that
 * the blocking @c read() in @c main() returns and the process can shut
 * down cleanly.
 *
 * @param sig  Signal number (unused; handler handles both SIGINT and SIGTERM).
 */
static void exitHandler(int sig) {
  uint64_t u = 1;
  if (write(exitEvent, &u, sizeof(uint64_t)) < 0) {
    fprintf(stderr, "%s: ERROR: error writing exit event\n", modname);
  }
}

/**
 * @brief Program entry point for the lcec_conf configuration tool.
 *
 * Performs the following steps:
 *  -# Initialises the HAL component and allocates the @ref LCEC_CONF_HAL_T
 *     pin block in HAL shared memory.
 *  -# Registers the @c lcec.conf.master-count and @c lcec.conf.slave-count
 *     HAL output pins.
 *  -# Creates an eventfd and installs @ref exitHandler for SIGINT/SIGTERM.
 *  -# Opens the XML configuration file named on the command line.
 *  -# Parses the file using expat and the @c xml_states transition table,
 *     accumulating configuration records in @c state.outputBuf.
 *  -# Appends a @ref LCEC_CONF_NULL_T end-of-stream sentinel.
 *  -# Creates a shared-memory segment and writes the @ref LCEC_CONF_HEADER_T
 *     followed by the serialised records.
 *  -# Calls @c hal_ready() and blocks on the eventfd until a signal arrives.
 *  -# Cleans up (shared memory, parser, file, HAL) and returns.
 *
 * @param argc  Argument count; must be exactly 2.
 * @param argv  Argument vector; @c argv[1] must be the XML file path.
 * @return 0 on success, 1 on any error.
 */
int main(int argc, char **argv) {
  int ret = 1;
  char *filename;
  int done;
  char buffer[BUFFSIZE];
  FILE *file;
  LCEC_CONF_NULL_T *end;
  void *shmem_ptr;
  LCEC_CONF_HEADER_T *header;
  uint64_t u;
  LCEC_CONF_XML_STATE_T state;

  // initialize component
  hal_comp_id = hal_init(modname);
  if (hal_comp_id < 1) {
    fprintf(stderr, "%s: ERROR: hal_init failed\n", modname);
    goto fail0;
  }

  // allocate hal memory
  conf_hal_data = hal_malloc(sizeof(LCEC_CONF_HAL_T));
  if (conf_hal_data == NULL) {
    fprintf(stderr, "%s: ERROR: unable to allocate HAL shared memory\n", modname);
    goto fail1;
  }

  // register pins
  if (hal_pin_u32_newf(HAL_OUT, &(conf_hal_data->master_count), hal_comp_id, "%s.conf.master-count", LCEC_MODULE_NAME) != 0) {
    fprintf(stderr, "%s: ERROR: unable to register pin %s.conf.master-count\n", modname, LCEC_MODULE_NAME);
    goto fail1;
  }
  if (hal_pin_u32_newf(HAL_OUT, &(conf_hal_data->slave_count), hal_comp_id, "%s.conf.slave-count", LCEC_MODULE_NAME) != 0) {
    fprintf(stderr, "%s: ERROR: unable to register pin %s.conf.slave-count\n", modname, LCEC_MODULE_NAME);
    goto fail1;
  }
  *(conf_hal_data->master_count) = 0;
  *(conf_hal_data->slave_count) = 0;

  // initialize signal handling
  exitEvent = eventfd(0, 0);
  if (exitEvent == -1) {
    fprintf(stderr, "%s: ERROR: unable to create exit event\n", modname);
    goto fail1;
  }
  signal(SIGINT, exitHandler);
  signal(SIGTERM, exitHandler);

  // get config file name
  if (argc != 2) {
    fprintf(stderr, "%s: ERROR: invalid arguments\n", modname);
    goto fail2;
  }
  filename = argv[1];

  // open file
  file = fopen(filename, "r");
  if (file == NULL) {
    fprintf(stderr, "%s: ERROR: unable to open config file %s\n", modname, filename);
    goto fail2;
  }

  // create xml parser
  memset(&state, 0, sizeof(state));
  if (initXmlInst((LCEC_CONF_XML_INST_T *) &state, xml_states)) {
    fprintf(stderr, "%s: ERROR: Couldn't allocate memory for parser\n", modname);
    goto fail3;
  }

  initOutputBuffer(&state.outputBuf);
  for (done=0; !done;) {
    // read block
    int len = fread(buffer, 1, BUFFSIZE, file);
    if (ferror(file)) {
      fprintf(stderr, "%s: ERROR: Couldn't read from file %s\n", modname, filename);
      goto fail4;
    }

    // check for EOF
    done = feof(file);

    // parse current block
    if (!XML_Parse(state.xml.parser, buffer, len, done)) {
      fprintf(stderr, "%s: ERROR: Parse error at line %u: %s\n", modname,
        (unsigned int)XML_GetCurrentLineNumber(state.xml.parser),
        XML_ErrorString(XML_GetErrorCode(state.xml.parser)));
      goto fail4;
    }
  }

  // set end marker
  end = addOutputBuffer(&state.outputBuf, sizeof(LCEC_CONF_NULL_T));
  if (end == NULL) {
      goto fail4;
  }
  end->confType = lcecConfTypeNone;

  // setup shared mem for config
  shmem_id = rtapi_shmem_new(LCEC_CONF_SHMEM_KEY, hal_comp_id, sizeof(LCEC_CONF_HEADER_T) + state.outputBuf.len);
  if ( shmem_id < 0 ) {
    fprintf(stderr, "%s: ERROR: couldn't allocate user/RT shared memory\n", modname);
    goto fail4;
  }
  if (lcec_rtapi_shmem_getptr(shmem_id, &shmem_ptr) < 0) {
    fprintf(stderr, "%s: ERROR: couldn't map user/RT shared memory\n", modname);
    goto fail5;
  }

  // setup header
  header = shmem_ptr;
  shmem_ptr += sizeof(LCEC_CONF_HEADER_T);
  header->magic = LCEC_CONF_SHMEM_MAGIC;
  header->length = state.outputBuf.len;

  // copy data and free buffer
  copyFreeOutputBuffer(&state.outputBuf, shmem_ptr);

  // everything is fine
  ret = 0;
  hal_ready(hal_comp_id);

  // wait for SIGTERM
  if (read(exitEvent, &u, sizeof(uint64_t)) < 0) {
    fprintf(stderr, "%s: ERROR: error reading exit event\n", modname);
  }

fail5:
  rtapi_shmem_delete(shmem_id, hal_comp_id);
fail4:
  copyFreeOutputBuffer(&state.outputBuf, NULL);
  XML_ParserFree(state.xml.parser);
fail3:
  fclose(file);
fail2:
  close(exitEvent);
fail1:
  hal_exit(hal_comp_id);
fail0:
  return ret;
}

/**
 * @brief Start-element callback for the @c \<master\> XML element.
 *
 * Allocates a @ref LCEC_CONF_MASTER_T record, populates it from the
 * attributes listed below, and updates @c state->currMaster.
 * The global @c conf_hal_data->master_count HAL pin is incremented.
 *
 * Recognised attributes:
 *  - @c idx              — EtherCAT master index (integer, default 0).
 *  - @c name             — Human-readable name (string, default = decimal idx).
 *  - @c appTimePeriod    — Application time period in ns (uint32).
 *  - @c refClockSyncCycles — DC reference-clock sync cycle count (int).
 *  - @c refClockSlaveIdx — Bus position of DC reference slave, or -1 (int).
 *  - @c interface        — (EC_USPACE_MASTER only) Primary NIC name, REQUIRED.
 *  - @c backupInterface  — (EC_USPACE_MASTER only) Backup NIC name, optional.
 *  - @c transportType    — (EC_USPACE_MASTER only) "raw", "xdp-skb", "xdp-native".
 *  - @c debugLevel       — (EC_USPACE_MASTER only) Debug verbosity (uint, default 0).
 *  - @c runOnCpu         — (EC_USPACE_MASTER only) CPU affinity, or -1 (int).
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_XML_STATE_T*.
 * @param next  State to transition to (unused here).
 * @param attr  Null-terminated attribute name/value pair array.
 */
static void parseMasterAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  LCEC_CONF_MASTER_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_MASTER_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypeMaster;
  p->refClockSlaveIdx = -1;
#ifdef EC_USPACE_MASTER
  p->transportType = 0;         // EC_TRANSPORT_RAW
  p->interface[0] = 0;
  p->backupInterface[0] = 0;
  p->debugLevel = 0;
  p->runOnCpu = -1;             // no CPU binding
#endif
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse index
    if (strcmp(name, "idx") == 0) {
      p->index = atoi(val);
      continue;
    }

    // parse name
    if (strcmp(name, "name") == 0) {
      strncpy(p->name, val, LCEC_CONF_STR_MAXLEN);
      p->name[LCEC_CONF_STR_MAXLEN - 1] = 0;
      continue;
    }

    // parse appTimePeriod
    if (strcmp(name, "appTimePeriod") == 0) {
      p->appTimePeriod = atol(val);
      continue;
    }

    // parse refClockSyncCycles
    if (strcmp(name, "refClockSyncCycles") == 0) {
      p->refClockSyncCycles = atoll(val);
      continue;
    }

    // parse refClockSlaveIdx
    if (strcmp(name, "refClockSlaveIdx") == 0) {
      p->refClockSlaveIdx = atoi(val);
      continue;
    }

#ifdef EC_USPACE_MASTER
    // parse interface (required)
    if (strcmp(name, "interface") == 0) {
      strncpy(p->interface, val, LCEC_CONF_STR_MAXLEN);
      p->interface[LCEC_CONF_STR_MAXLEN - 1] = 0;
      continue;
    }

    // parse backupInterface (optional)
    if (strcmp(name, "backupInterface") == 0) {
      strncpy(p->backupInterface, val, LCEC_CONF_STR_MAXLEN);
      p->backupInterface[LCEC_CONF_STR_MAXLEN - 1] = 0;
      continue;
    }

    // parse transportType (optional, default: "raw")
    if (strcmp(name, "transportType") == 0) {
      if (strcmp(val, "raw") == 0) {
        p->transportType = EC_TRANSPORT_RAW;
      } else if (strcmp(val, "xdp-skb") == 0) {
        p->transportType = EC_TRANSPORT_XDP_SKB;
      } else if (strcmp(val, "xdp-native") == 0) {
        p->transportType = EC_TRANSPORT_XDP_NATIVE;
      } else {
        fprintf(stderr, "%s: ERROR: Unknown transportType '%s'"
            " (valid values: raw, xdp-skb, xdp-native)\n",
            modname, val);
        XML_StopParser(inst->parser, 0);
        return;
      }
      continue;
    }

    // parse debugLevel (optional, default: 0)
    if (strcmp(name, "debugLevel") == 0) {
      p->debugLevel = atoi(val);
      continue;
    }

    // parse runOnCpu (optional, default: -1)
    if (strcmp(name, "runOnCpu") == 0) {
      p->runOnCpu = atoi(val);
      continue;
    }
#endif

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid master attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

#ifdef EC_USPACE_MASTER
  if (p->interface[0] == 0) {
    fprintf(stderr, "%s: ERROR: Master %d requires 'interface' attribute\n",
        modname, p->index);
    XML_StopParser(inst->parser, 0);
    return;
  }
#endif

  // set default name
  if (p->name[0] == 0) {
    snprintf(p->name, LCEC_CONF_STR_MAXLEN, "%d", p->index);
  }

  (*(conf_hal_data->master_count))++;
  state->currMaster = p;
}

/**
 * @brief Start-element callback for the @c \<slave\> XML element.
 *
 * Allocates a @ref LCEC_CONF_SLAVE_T record, resolves the mandatory @c type
 * attribute against @c slaveTypes[], and populates remaining fields.
 * The global @c conf_hal_data->slave_count HAL pin is incremented.
 * Updates @c state->currSlave and @c state->currSlaveType.
 *
 * Recognised attributes:
 *  - @c type      — Slave type name (REQUIRED); must match an entry in @c slaveTypes[].
 *  - @c idx       — EtherCAT bus position (integer, default 0).
 *  - @c name      — Human-readable name (string, default = decimal idx).
 *  - @c vid       — Vendor ID in hex (generic slaves only).
 *  - @c pid       — Product code in hex (generic slaves only).
 *  - @c configPdos — "true"/"false"; whether to configure PDOs (generic only).
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_XML_STATE_T*.
 * @param next  State to transition to (unused here).
 * @param attr  Null-terminated attribute name/value pair array.
 */
static void parseSlaveAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  LCEC_CONF_SLAVE_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_SLAVE_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  const LCEC_CONF_TYPELIST_T *slaveType = NULL;
  p->confType = lcecConfTypeSlave;
  p->type = lcecSlaveTypeInvalid;

  // pre parse slave type to avoid attribute ordering problems
  const char **iter = attr;
  while(*iter) {
    const char *name = *(iter++);
    const char *val = *(iter++);

    // parse slave type
    if (strcmp(name, "type") == 0) {
      for (slaveType = slaveTypes; slaveType->name != NULL; slaveType++) {
        if (strcmp(val, slaveType->name) == 0) {
          break;
        }
      }
      if (slaveType->name == NULL) {
        fprintf(stderr, "%s: ERROR: Invalid slave type %s\n", modname, val);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->type = slaveType->type;
      continue;
    }
  }

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // skip slave type (already parsed)
    if (strcmp(name, "type") == 0) {
      continue;
    }

    // parse index
    if (strcmp(name, "idx") == 0) {
      p->index = atoi(val);
      continue;
    }

    // parse name
    if (strcmp(name, "name") == 0) {
      strncpy(p->name, val, LCEC_CONF_STR_MAXLEN);
      p->name[LCEC_CONF_STR_MAXLEN - 1] = 0;
      continue;
    }

    // generic only attributes
    if (p->type == lcecSlaveTypeGeneric) {
      // parse vid (hex value)
      if (strcmp(name, "vid") == 0) {
        p->vid = strtol(val, NULL, 16);
        continue;
      }

      // parse pid (hex value)
      if (strcmp(name, "pid") == 0) {
        p->pid = strtol(val, NULL, 16);
        continue;
      }

      // parse configPdos
      if (strcmp(name, "configPdos") == 0) {
        p->configPdos = (strcasecmp(val, "true") == 0);
        continue;
      }
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid slave attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // set default name
  if (p->name[0] == 0) {
    snprintf(p->name, LCEC_CONF_STR_MAXLEN, "%d", p->index);
  }

  // type is required
  if (p->type == lcecSlaveTypeInvalid) {
    fprintf(stderr, "%s: ERROR: Slave has no type attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  (*(conf_hal_data->slave_count))++;
  state->currSlaveType = slaveType;
  state->currSlave = p;
}

/**
 * @brief Start-element callback for the @c \<dcConf\> XML element.
 *
 * Allocates a @ref LCEC_CONF_DC_T record for the current slave's
 * distributed-clock configuration.
 *
 * Recognised attributes:
 *  - @c assignActivate — DC activation word in hex (e.g. "0x0700").
 *  - @c sync0Cycle     — SYNC0 period in ns; prefix with @c '*' to multiply
 *                        by the master's appTimePeriod (e.g. "*2").
 *  - @c sync0Shift     — SYNC0 shift in ns (signed integer).
 *  - @c sync1Cycle     — SYNC1 period in ns (same @c '*' prefix supported).
 *  - @c sync1Shift     — SYNC1 shift in ns (signed integer).
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_XML_STATE_T*.
 * @param next  State to transition to (unused here).
 * @param attr  Null-terminated attribute name/value pair array.
 */
static void parseDcConfAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  LCEC_CONF_DC_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_DC_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypeDcConf;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse assignActivate (hex value)
    if (strcmp(name, "assignActivate") == 0) {
      p->assignActivate = strtol(val, NULL, 16);
      continue;
    }

    // parse sync0Cycle
    if (strcmp(name, "sync0Cycle") == 0) {
      p->sync0Cycle = parseSyncCycle(state, val);
      continue;
    }

    // parse sync0Shift
    if (strcmp(name, "sync0Shift") == 0) {
      p->sync0Shift = atoi(val);
      continue;
    }

    // parse sync1Cycle
    if (strcmp(name, "sync1Cycle") == 0) {
      p->sync1Cycle = parseSyncCycle(state, val);
      continue;
    }

    // parse sync1Shift
    if (strcmp(name, "sync1Shift") == 0) {
      p->sync1Shift = atoi(val);
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid dcConfig attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }
}

/**
 * @brief Start-element callback for the @c \<watchdog\> XML element.
 *
 * Allocates a @ref LCEC_CONF_WATCHDOG_T record for the current slave.
 *
 * Recognised attributes:
 *  - @c divider   — Watchdog divider value (uint16).
 *  - @c intervals — Watchdog interval count (uint16).
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_XML_STATE_T*.
 * @param next  State to transition to (unused here).
 * @param attr  Null-terminated attribute name/value pair array.
 */
static void parseWatchdogAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  LCEC_CONF_WATCHDOG_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_WATCHDOG_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypeWatchdog;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse divider
    if (strcmp(name, "divider") == 0) {
      p->divider = atoi(val);
      continue;
    }

    // parse intervals
    if (strcmp(name, "intervals") == 0) {
      p->intervals = atoi(val);
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid watchdog attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }
}

/**
 * @brief Start-element callback for the @c \<sdoConfig\> XML element.
 *
 * Allocates a @ref LCEC_CONF_SDOCONF_T record for a CoE SDO startup write.
 * Updates @c state->currSdoConf and increments the current slave's
 * @c sdoConfigLength by @c sizeof(LCEC_CONF_SDOCONF_T) (data bytes are added
 * later by @ref parseDataRawAttrs).
 *
 * Recognised attributes (both REQUIRED):
 *  - @c idx    — Object dictionary index in hex (0x0000–0xFFFE).
 *  - @c subIdx — Sub-index in hex (0x00–0xFE), or the string @c "complete"
 *                to request complete-access (@ref LCEC_CONF_SDO_COMPLETE_SUBIDX).
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_XML_STATE_T*.
 * @param next  State to transition to (unused here).
 * @param attr  Null-terminated attribute name/value pair array.
 */
static void parseSdoConfigAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  int tmp;
  LCEC_CONF_SDOCONF_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_SDOCONF_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypeSdoConfig;
  p->index = 0xffff;
  p->subindex = 0xff;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse index
    if (strcmp(name, "idx") == 0) {
      tmp = strtol(val, NULL, 16);
      if (tmp < 0 || tmp >= 0xffff) {
        fprintf(stderr, "%s: ERROR: Invalid sdoConfig idx %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->index = tmp;
      continue;
    }

    // parse subIdx
    if (strcmp(name, "subIdx") == 0) {
      if (strcasecmp(val, "complete") == 0) {
        p->subindex = LCEC_CONF_SDO_COMPLETE_SUBIDX;
        continue;
      }
      tmp = strtol(val, NULL, 16);
      if (tmp < 0 || tmp >= 0xff) {
        fprintf(stderr, "%s: ERROR: Invalid sdoConfig subIdx %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->subindex = tmp;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid sdoConfig attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // idx is required
  if (p->index == 0xffff) {
    fprintf(stderr, "%s: ERROR: sdoConfig has no idx attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // subIdx is required
  if (p->subindex == 0xff) {
    fprintf(stderr, "%s: ERROR: sdoConfig has no subIdx attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  state->currSdoConf = p;
  state->currSlave->sdoConfigLength += sizeof(LCEC_CONF_SDOCONF_T);
}

/**
 * @brief Start-element callback for the @c \<idnConfig\> XML element.
 *
 * Allocates a @ref LCEC_CONF_IDNCONF_T record for an SoE IDN startup write.
 * Updates @c state->currIdnConf and increments the current slave's
 * @c idnConfigLength by @c sizeof(LCEC_CONF_IDNCONF_T).
 *
 * Recognised attributes:
 *  - @c drive  — SoE drive number 0–7 (optional, default 0).
 *  - @c idn    — IDN value; may be decimal, or S/P notation (@c "S-0-32768",
 *                @c "P-3-0001"), REQUIRED.
 *  - @c state  — AL state string "PREOP" or "SAFEOP" (REQUIRED).
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_XML_STATE_T*.
 * @param next  State to transition to (unused here).
 * @param attr  Null-terminated attribute name/value pair array.
 */
static void parseIdnConfigAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  int tmp;
  LCEC_CONF_IDNCONF_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_IDNCONF_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypeIdnConfig;
  p->drive = 0;
  p->idn = 0xffff;
  p->state = 0;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse index
    if (strcmp(name, "drive") == 0) {
      tmp = atoi(val);
      if (tmp < 0 || tmp > 7) {
        fprintf(stderr, "%s: ERROR: Invalid idnConfig drive %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }

      p->drive = tmp;
      continue;
    }

    // parse idn
    if (strcmp(name, "idn") == 0) {
      char pfx = val[0];
      if (pfx == 0) {
        fprintf(stderr, "%s: ERROR: Missing idnConfig idn value\n", modname);
        XML_StopParser(inst->parser, 0);
        return;
      }

      pfx = toupper(pfx);

      tmp = 0xffff;
      if (pfx == 'S' || pfx == 'P') {
        int set;
        int block;
	if (sscanf(val, "%c-%d-%d", &pfx, &set, &block) == 3) {
          if (set < 0 || set >= (1 << 3)) {
            fprintf(stderr, "%s: ERROR: Invalid idnConfig idn set %d\n", modname, set);
            XML_StopParser(inst->parser, 0);
            return;
          }

          if (block < 0 || block >= (1 << 12)) {
            fprintf(stderr, "%s: ERROR: Invalid idnConfig idn block %d\n", modname, block);
            XML_StopParser(inst->parser, 0);
            return;
          }

          tmp = (set << 12) | block;
          if (pfx == 'P') {
            tmp |= (15 << 1);
          }
        }
      } else if (pfx >= '0' && pfx <= '9') {
        tmp = atoi(val);
      }

      if (tmp == 0xffff) {
        fprintf(stderr, "%s: ERROR: Invalid idnConfig idn value '%s'\n", modname, val);
        XML_StopParser(inst->parser, 0);
        return;
      }

      p->idn = tmp;
      continue;
    }

    // parse state
    if (strcmp(name, "drive") == 0) {
      if (strcmp(val, "PREOP") == 0) {
        p->state = EC_AL_STATE_PREOP;
      } else if (strcmp(val, "SAFEOP") == 0) {
        p->state = EC_AL_STATE_SAFEOP;
      } else {
        fprintf(stderr, "%s: ERROR: Invalid idnConfig state '%s'\n", modname, val);
        XML_StopParser(inst->parser, 0);
        return;
      }

      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid idnConfig attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // idn is required
  if (p->idn == 0xffff) {
    fprintf(stderr, "%s: ERROR: idnConfig has no idn attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // state is required
  if (p->state == 0) {
    fprintf(stderr, "%s: ERROR: idnConfig has no state attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  state->currIdnConf = p;
  state->currSlave->idnConfigLength += sizeof(LCEC_CONF_IDNCONF_T);
}

/**
 * @brief Start-element callback for @c \<sdoDataRaw\> and @c \<idnDataRaw\> elements.
 *
 * Decodes the @c data attribute (a hex-encoded byte string) and appends the
 * resulting bytes to the output buffer immediately after the active SDO or IDN
 * record.  Also updates the parent record's @c length field and the slave's
 * corresponding @c sdoConfigLength / @c idnConfigLength accumulator.
 *
 * Recognised attribute:
 *  - @c data — Hex-encoded payload bytes (e.g. "06 00 01").
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_XML_STATE_T*.
 * @param next  State to transition to (unused here).
 * @param attr  Null-terminated attribute name/value pair array.
 */
static void parseDataRawAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  int len;
  uint8_t *p;

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse data
    if (strcmp(name, "data") == 0) {
      len = parseHex(val, -1, NULL);
      if (len < 0) {
        fprintf(stderr, "%s: ERROR: Invalid dataRaw data\n", modname);
        XML_StopParser(inst->parser, 0);
        return;
      }
      if (len > 0) {
        p = (uint8_t *) addOutputBuffer(&state->outputBuf, len);
        if (p != NULL) {
          parseHex(val, -1, p);
          switch (inst->state) {
            case lcecConfTypeSdoConfig:
              state->currSdoConf->length += len;
              state->currSlave->sdoConfigLength += len;
              break;
            case lcecConfTypeIdnConfig:
              state->currIdnConf->length += len;
              state->currSlave->idnConfigLength += len;
              break;
          }
        }
      }
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid pdoEntry attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }
}

/**
 * @brief Start-element callback for the @c \<initCmds\> XML element.
 *
 * Delegates to @ref parseIcmds() to parse an ESI-style XML init-commands
 * file and append its CoE/SoE records to the shared output buffer.  The
 * @c filename attribute is REQUIRED.
 *
 * Recognised attributes:
 *  - @c filename — Path to the ESI-compatible XML file (REQUIRED).
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_XML_STATE_T*.
 * @param next  State to transition to (unused here).
 * @param attr  Null-terminated attribute name/value pair array.
 */
static void parseInitCmdsAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  const char *filename = NULL;

  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse filename
    if (strcmp(name, "filename") == 0) {
      filename = val;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid syncManager attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // filename is required
  if (filename == NULL || *filename == 0) {
    fprintf(stderr, "%s: ERROR: initCmds has no filename attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // try to parse initCmds
  if (parseIcmds(state->currSlave, &state->outputBuf, filename)) {
    XML_StopParser(inst->parser, 0);
    return;
  }
}

/**
 * @brief Start-element callback for the @c \<syncManager\> XML element.
 *
 * Only permitted on generic slaves (@c lcecSlaveTypeGeneric).  Allocates a
 * @ref LCEC_CONF_SYNCMANAGER_T record.  Increments the current slave's
 * @c syncManagerCount.  Updates @c state->currSyncManager.
 *
 * Recognised attributes (both REQUIRED):
 *  - @c idx — Sync manager index 0–3 (integer).
 *  - @c dir — Direction string "in" (input) or "out" (output).
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_XML_STATE_T*.
 * @param next  State to transition to (unused here).
 * @param attr  Null-terminated attribute name/value pair array.
 */
static void parseSyncManagerAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  int tmp;
  LCEC_CONF_SYNCMANAGER_T *p;

  // only allowed on generic slave
  if (state->currSlave->type != lcecSlaveTypeGeneric) {
    fprintf(stderr, "%s: ERROR: syncManager is only allowed on generic slaves\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_SYNCMANAGER_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypeSyncManager;
  p->index = 0xff;
  p->dir = EC_DIR_INVALID;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse index
    if (strcmp(name, "idx") == 0) {
      tmp = atoi(val);
      if (tmp < 0 || tmp >= EC_MAX_SYNC_MANAGERS) {
        fprintf(stderr, "%s: ERROR: Invalid syncManager idx %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->index = tmp;
      continue;
    }

    // parse dir
    if (strcmp(name, "dir") == 0) {
      if (strcasecmp(val, "in") == 0) {
        p->dir = EC_DIR_INPUT;
        continue;
      }
      if (strcasecmp(val, "out") == 0) {
        p->dir = EC_DIR_OUTPUT;
        continue;
      }
      fprintf(stderr, "%s: ERROR: Invalid syncManager dir %s\n", modname, val);
      XML_StopParser(inst->parser, 0);
      return;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid syncManager attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // idx is required
  if (p->index == 0xff) {
    fprintf(stderr, "%s: ERROR: syncManager has no idx attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // dir is required
  if (p->dir == EC_DIR_INVALID) {
    fprintf(stderr, "%s: ERROR: syncManager has no dir attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  (state->currSlave->syncManagerCount)++;
  state->currSyncManager = p;
}

/**
 * @brief Start-element callback for the @c \<pdo\> XML element.
 *
 * Allocates a @ref LCEC_CONF_PDO_T record assigned to the current sync manager.
 * Increments both the slave's @c pdoCount and the sync manager's @c pdoCount.
 * Updates @c state->currPdo.
 *
 * Recognised attributes:
 *  - @c idx — PDO index in hex (0x0000–0xFFFE), REQUIRED.
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_XML_STATE_T*.
 * @param next  State to transition to (unused here).
 * @param attr  Null-terminated attribute name/value pair array.
 */
static void parsePdoAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  int tmp;
  LCEC_CONF_PDO_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_PDO_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypePdo;
  p->index = 0xffff;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse index
    if (strcmp(name, "idx") == 0) {
      tmp = strtol(val, NULL, 16);
      if (tmp < 0 || tmp >= 0xffff) {
        fprintf(stderr, "%s: ERROR: Invalid pdo idx %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->index = tmp;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid pdo attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // idx is required
  if (p->index == 0xffff) {
    fprintf(stderr, "%s: ERROR: pdo has no idx attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  (state->currSlave->pdoCount)++;
  (state->currSyncManager->pdoCount)++;
  state->currPdo = p;
}

/**
 * @brief Start-element callback for the @c \<pdoEntry\> XML element.
 *
 * Allocates a @ref LCEC_CONF_PDOENTRY_T record.  Increments the parent slave's
 * @c pdoEntryCount and, if a @c halPin is given, @c pdoMappingCount.
 * Also increments the parent PDO's @c pdoEntryCount.
 * Updates @c state->currPdoEntry and resets @c state->currComplexBitOffset.
 *
 * Recognised attributes:
 *  - @c idx     — Object dictionary index in hex (0x0000–0xFFFE), REQUIRED.
 *  - @c subIdx  — Sub-index in hex (0x00–0xFE), REQUIRED.
 *  - @c bitLen  — Entry width in bits (1–255), REQUIRED.
 *  - @c halType — HAL type string: "bit", "s32", "u32", "float",
 *                 "float-unsigned", "complex", or "float-ieee".
 *  - @c scale   — Float scale factor (implies @c halType must be float).
 *  - @c offset  — Float offset (implies @c halType must be float).
 *  - @c halPin  — HAL pin name suffix; empty means entry is mapped but no pin created.
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_XML_STATE_T*.
 * @param next  State to transition to (unused here).
 * @param attr  Null-terminated attribute name/value pair array.
 */
static void parsePdoEntryAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  int tmp;
  int floatReq;
  LCEC_CONF_PDOENTRY_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_PDOENTRY_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  floatReq = 0;
  p->confType = lcecConfTypePdoEntry;
  p->index = 0xffff;
  p->subindex = 0xff;
  p->floatScale = 1.0;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse index
    if (strcmp(name, "idx") == 0) {
      tmp = strtol(val, NULL, 16);
      if (tmp < 0 || tmp >= 0xffff) {
        fprintf(stderr, "%s: ERROR: Invalid pdoEntry idx %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->index = tmp;
      continue;
    }

    // parse subIdx
    if (strcmp(name, "subIdx") == 0) {
      tmp = strtol(val, NULL, 16);
      if (tmp < 0 || tmp >= 0xff) {
        fprintf(stderr, "%s: ERROR: Invalid pdoEntry subIdx %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->subindex = tmp;
      continue;
    }

    // parse bitLen
    if (strcmp(name, "bitLen") == 0) {
      tmp = atoi(val);
      if (tmp <= 0 || tmp > LCEC_CONF_GENERIC_MAX_BITLEN) {
        fprintf(stderr, "%s: ERROR: Invalid pdoEntry bitLen %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->bitLength = tmp;
      continue;
    }

    // parse halType
    if (strcmp(name, "halType") == 0) {
      if (strcasecmp(val, "bit") == 0) {
        p->halType = HAL_BIT;
        continue;
      }
      if (strcasecmp(val, "s32") == 0) {
        p->subType = lcecPdoEntTypeSimple;
        p->halType = HAL_S32;
        continue;
      }
      if (strcasecmp(val, "u32") == 0) {
        p->subType = lcecPdoEntTypeSimple;
        p->halType = HAL_U32;
        continue;
      }
      if (strcasecmp(val, "float") == 0) {
        p->subType = lcecPdoEntTypeFloatSigned;
        p->halType = HAL_FLOAT;
        continue;
      }
      if (strcasecmp(val, "float-unsigned") == 0) {
        p->subType = lcecPdoEntTypeFloatUnsigned;
        p->halType = HAL_FLOAT;
        continue;
      }
      if (strcasecmp(val, "complex") == 0) {
        p->subType = lcecPdoEntTypeComplex;
        continue;
      }
      if (strcasecmp(val, "float-ieee") == 0) {
        p->subType = lcecPdoEntTypeFloatIeee;
        p->halType = HAL_FLOAT;
        continue;
      }
      fprintf(stderr, "%s: ERROR: Invalid pdoEntry halType %s\n", modname, val);
      XML_StopParser(inst->parser, 0);
      return;
    }

    // parse scale
    if (strcmp(name, "scale") == 0) {
      floatReq = 1;
      p->floatScale = atof(val);
      continue;
    }

    // parse offset
    if (strcmp(name, "offset") == 0) {
      floatReq = 1;
      p->floatOffset = atof(val);
      continue;
    }

    // parse halPin
    if (strcmp(name, "halPin") == 0) {
      strncpy(p->halPin, val, LCEC_CONF_STR_MAXLEN);
      p->halPin[LCEC_CONF_STR_MAXLEN - 1] = 0;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid pdoEntry attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // idx is required
  if (p->index == 0xffff) {
    fprintf(stderr, "%s: ERROR: pdoEntry has no idx attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // subIdx is required
  if (p->subindex == 0xff) {
    fprintf(stderr, "%s: ERROR: pdoEntry has no subIdx attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // bitLen is required
  if (p->bitLength == 0) {
    fprintf(stderr, "%s: ERROR: pdoEntry has no bitLen attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // pin name must not be given for complex subtype
  if (p->subType == lcecPdoEntTypeComplex && p->halPin[0] != 0) {
    fprintf(stderr, "%s: ERROR: pdoEntry has halPin attributes but pin type is 'complex'\n", modname);
    XML_StopParser(inst->parser, 0);
  }

  // check for float type if required
  if (floatReq && p->halType != HAL_FLOAT) {
    fprintf(stderr, "%s: ERROR: pdoEntry has scale/offset attributes but pin type is not 'float'\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  (state->currSlave->pdoEntryCount)++;
  if (p->halPin[0] != 0) {
    (state->currSlave->pdoMappingCount)++;
  }
  (state->currPdo->pdoEntryCount)++;
  state->currPdoEntry = p;
  state->currComplexBitOffset = 0;
}

/**
 * @brief Start-element callback for the @c \<complexEntry\> XML element.
 *
 * Allocates a @ref LCEC_CONF_COMPLEXENTRY_T sub-field record for a complex
 * PDO entry.  The @c bitOffset is set automatically from
 * @c state->currComplexBitOffset and advanced by @c bitLength after the
 * entry is processed.  If @c halPin is non-empty, increments the slave's
 * @c pdoMappingCount.
 *
 * Recognised attributes:
 *  - @c bitLen  — Sub-field width in bits (1–32); must not overflow the
 *                 parent entry's @c bitLen, REQUIRED.
 *  - @c halType — "bit", "s32", "u32", "float", "float-unsigned", "float-ieee".
 *  - @c scale   — Float scale factor (implies @c halType must be float).
 *  - @c offset  — Float offset (implies @c halType must be float).
 *  - @c halPin  — HAL pin name suffix; empty means sub-field has no pin.
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_XML_STATE_T*.
 * @param next  State to transition to (unused here).
 * @param attr  Null-terminated attribute name/value pair array.
 */
static void parseComplexEntryAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  int tmp;
  int floatReq;
  LCEC_CONF_COMPLEXENTRY_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_COMPLEXENTRY_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  floatReq = 0;
  p->confType = lcecConfTypeComplexEntry;
  p->bitOffset = state->currComplexBitOffset;
  p->floatScale = 1.0;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // parse bitLen
    if (strcmp(name, "bitLen") == 0) {
      tmp = atoi(val);
      if (tmp <= 0 || tmp > LCEC_CONF_GENERIC_MAX_SUBPINS) {
        fprintf(stderr, "%s: ERROR: Invalid complexEntry bitLen %d\n", modname, tmp);
        XML_StopParser(inst->parser, 0);
        return;
      }
      if ((state->currComplexBitOffset + tmp) > state->currPdoEntry->bitLength) {
        fprintf(stderr, "%s: ERROR: complexEntry bitLen sum exceeded pdoEntry bitLen %d\n", modname, state->currPdoEntry->bitLength);
        XML_StopParser(inst->parser, 0);
        return;
      }
      p->bitLength = tmp;
      continue;
    }

    // parse halType
    if (strcmp(name, "halType") == 0) {
      if (strcasecmp(val, "bit") == 0) {
        p->subType = lcecPdoEntTypeSimple;
        p->halType = HAL_BIT;
        continue;
      }
      if (strcasecmp(val, "s32") == 0) {
        p->subType = lcecPdoEntTypeSimple;
        p->halType = HAL_S32;
        continue;
      }
      if (strcasecmp(val, "u32") == 0) {
        p->subType = lcecPdoEntTypeSimple;
        p->halType = HAL_U32;
        continue;
      }
      if (strcasecmp(val, "float") == 0) {
        p->subType = lcecPdoEntTypeFloatSigned;
        p->halType = HAL_FLOAT;
        continue;
      }
      if (strcasecmp(val, "float-unsigned") == 0) {
        p->subType = lcecPdoEntTypeFloatUnsigned;
        p->halType = HAL_FLOAT;
        continue;
      }
      if (strcasecmp(val, "float-ieee") == 0) {
        p->subType = lcecPdoEntTypeFloatIeee;
        p->halType = HAL_FLOAT;
        continue;
      }
      fprintf(stderr, "%s: ERROR: Invalid complexEntry halType %s\n", modname, val);
      XML_StopParser(inst->parser, 0);
      return;
    }

    // parse scale
    if (strcmp(name, "scale") == 0) {
      floatReq = 1;
      p->floatScale = atof(val);
      continue;
    }

    // parse offset
    if (strcmp(name, "offset") == 0) {
      floatReq = 1;
      p->floatOffset = atof(val);
      continue;
    }

    // parse halPin
    if (strcmp(name, "halPin") == 0) {
      strncpy(p->halPin, val, LCEC_CONF_STR_MAXLEN);
      p->halPin[LCEC_CONF_STR_MAXLEN - 1] = 0;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid complexEntry attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // bitLen is required
  if (p->bitLength == 0) {
    fprintf(stderr, "%s: ERROR: complexEntry has no bitLen attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // check for float type if required
  if (floatReq && p->halType != HAL_FLOAT) {
    fprintf(stderr, "%s: ERROR: complexEntry has scale/offset attributes but pin type is not 'float'\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  if (p->halPin[0] != 0) {
    (state->currSlave->pdoMappingCount)++;
  }
  state->currComplexBitOffset += p->bitLength;
}

/**
 * @brief Start-element callback for the @c \<modParam\> XML element.
 *
 * Parses driver-specific module parameters for the current slave.  The
 * @c name attribute is resolved against the slave type's @c modParams table
 * to find the driver-specific parameter ID.  The @c value string is then
 * converted to the correct type (@ref LCEC_CONF_MODPARAM_TYPE_T) and stored.
 * Increments the slave's @c modParamCount on success.
 *
 * Recognised attributes (both REQUIRED):
 *  - @c name  — Parameter name; must match an entry in the slave type's descriptor table.
 *  - @c value — Parameter value as a string; parsed according to the declared type.
 *
 * @param inst  XML parse instance; cast to @ref LCEC_CONF_XML_STATE_T*.
 * @param next  State to transition to (unused here).
 * @param attr  Null-terminated attribute name/value pair array.
 */
static void parseModParamAttrs(LCEC_CONF_XML_INST_T *inst, int next, const char **attr) {
  LCEC_CONF_XML_STATE_T *state = (LCEC_CONF_XML_STATE_T *) inst;

  const char *pname, *pval;
  const LCEC_CONF_MODPARAM_DESC_T *modParams;

  if (state->currSlaveType->modParams == NULL) {
    fprintf(stderr, "%s: ERROR: modParam not allowed for this slave\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  LCEC_CONF_MODPARAM_T *p = addOutputBuffer(&state->outputBuf, sizeof(LCEC_CONF_MODPARAM_T));
  if (p == NULL) {
    XML_StopParser(inst->parser, 0);
    return;
  }

  p->confType = lcecConfTypeModParam;

  pname = NULL;
  pval = NULL;
  while (*attr) {
    const char *name = *(attr++);
    const char *val = *(attr++);

    // get name
    if (strcmp(name, "name") == 0) {
      pname = val;
      continue;
    }

    // get value
    if (strcmp(name, "value") == 0) {
      pval = val;
      continue;
    }

    // handle error
    fprintf(stderr, "%s: ERROR: Invalid modParam attribute %s\n", modname, name);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // name is required
  if (pname == NULL || pname[0] == 0) {
    fprintf(stderr, "%s: ERROR: modParam has no name attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // value is required
  if (pval == NULL || pval[0] == 0) {
    fprintf(stderr, "%s: ERROR: modParam has no value attribute\n", modname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // search for matching param name
  for (modParams = state->currSlaveType->modParams; modParams->name != NULL; modParams++) {
    if (strcmp(pname, modParams->name) == 0) {
      break;
    }
  }
  if (modParams->name == NULL) {
    fprintf(stderr, "%s: ERROR: Invalid modParam '%s'\n", modname, pname);
    XML_StopParser(inst->parser, 0);
    return;
  }

  // set id
  p->id = modParams->id;

  // try to parse value
  char *s = NULL;
  switch (modParams->type) {
    case MODPARAM_TYPE_BIT:
      if ((strcmp("1", pval) == 0) || (strcasecmp("TRUE", pval) == 0)) {
        p->value.bit = 1;
      } else if ((strcmp("0", pval) == 0) || (strcasecmp("FALSE", pval)) == 0) {
        p->value.bit = 0;
      } else {
        fprintf(stderr, "%s: ERROR: Invalid modParam bit value '%s' for param '%s'\n", modname, pval, pname);
        XML_StopParser(inst->parser, 0);
        return;
      }
      break;

    case MODPARAM_TYPE_U32:
      p->value.u32 = strtoul(pval, &s, 0);
      if (*s != 0) {
        fprintf(stderr, "%s: ERROR: Invalid modParam u32 value '%s' for param '%s'\n", modname, pval, pname);
        XML_StopParser(inst->parser, 0);
        return;
      }
      break;

    case MODPARAM_TYPE_S32:
      p->value.s32 = strtol(pval, &s, 0);
      if (*s != 0) {
        fprintf(stderr, "%s: ERROR: Invalid modParam s32 value '%s' for param '%s'\n", modname, pval, pname);
        XML_StopParser(inst->parser, 0);
        return;
      }
      break;

    case MODPARAM_TYPE_FLOAT:
      p->value.flt = strtod(pval, &s);
      if (*s != 0) {
        fprintf(stderr, "%s: ERROR: Invalid modParam float value '%s' for param '%s'\n", modname, pval, pname);
        XML_StopParser(inst->parser, 0);
        return;
      }
      break;

    case MODPARAM_TYPE_STRING:
      strncpy(p->value.str, pval, LCEC_CONF_STR_MAXLEN - 1);
      break;

    default:
      p->value.u32 = 0;
      break;
  }

  (state->currSlave->modParamCount)++;
}

/**
 * @brief Parse a DC sync-cycle value string into a nanosecond integer.
 *
 * If @p nptr starts with @c '*', the remainder is interpreted as a multiplier
 * applied to the current master's @c appTimePeriod (e.g. @c "*2" yields two
 * application periods).  Otherwise @p nptr is parsed as a plain decimal integer
 * (nanoseconds).
 *
 * @param state  Parser state providing access to the current master record.
 * @param nptr   String to parse; must not be NULL.
 * @return Sync-cycle value in nanoseconds.
 */
static int parseSyncCycle(LCEC_CONF_XML_STATE_T *state, const char *nptr) {
  // check for master period multiples
  if (*nptr == '*') {
    nptr++;
    return atoi(nptr) * state->currMaster->appTimePeriod;
  }

  // custom value
  return atoi(nptr);
}
