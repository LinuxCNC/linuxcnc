//
//    Copyright (C) 2011 Sascha Ittner <sascha.ittner@modusoft.de>
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//
#ifndef _LCEC_CONF_H_
#define _LCEC_CONF_H_

#include "hal.h"
#include "ecrt.h"

#define LCEC_MODULE_NAME "lcec"

#define LCEC_CONF_SHMEM_KEY   0xACB572C7
#define LCEC_CONF_SHMEM_MAGIC 0x036ED5A3

#define LCEC_CONF_STR_MAXLEN 48

#define LCEC_CONF_SDO_COMPLETE_SUBIDX -1
#define LCEC_CONF_GENERIC_MAX_SUBPINS 32
#define LCEC_CONF_GENERIC_MAX_BITLEN  255

typedef enum {
  lcecConfTypeNone = 0,
  lcecConfTypeMasters,
  lcecConfTypeMaster,
  lcecConfTypeSlave,
  lcecConfTypeDcConf,
  lcecConfTypeWatchdog,
  lcecConfTypeSyncManager,
  lcecConfTypePdo,
  lcecConfTypePdoEntry,
  lcecConfTypeSdoConfig,
  lcecConfTypeSdoDataRaw,
  lcecConfTypeIdnConfig,
  lcecConfTypeIdnDataRaw,
  lcecConfTypeInitCmds,
  lcecConfTypeComplexEntry,
  lcecConfTypeModParam
} LCEC_CONF_TYPE_T;

typedef enum {
  lcecPdoEntTypeSimple,
  lcecPdoEntTypeFloatSigned,
  lcecPdoEntTypeFloatUnsigned,
  lcecPdoEntTypeComplex
} LCEC_PDOENT_TYPE_T;

typedef enum {
  lcecSlaveTypeInvalid,
  lcecSlaveTypeGeneric,
  lcecSlaveTypeAX5203,
  lcecSlaveTypeAX5206,
  lcecSlaveTypeEK1100,
  lcecSlaveTypeEK1101,
  lcecSlaveTypeEK1110,
  lcecSlaveTypeEK1122,
  lcecSlaveTypeEL1002,
  lcecSlaveTypeEL1004,
  lcecSlaveTypeEL1008,
  lcecSlaveTypeEL1012,
  lcecSlaveTypeEL1014,
  lcecSlaveTypeEL1018,
  lcecSlaveTypeEL1024,
  lcecSlaveTypeEL1034,
  lcecSlaveTypeEL1084,
  lcecSlaveTypeEL1088,
  lcecSlaveTypeEL1094,
  lcecSlaveTypeEL1098,
  lcecSlaveTypeEL1104,
  lcecSlaveTypeEL1114,
  lcecSlaveTypeEL1124,
  lcecSlaveTypeEL1134,
  lcecSlaveTypeEL1144,
  lcecSlaveTypeEL1252,
  lcecSlaveTypeEL1808,
  lcecSlaveTypeEL1809,
  lcecSlaveTypeEL1819,
  lcecSlaveTypeEL2002,
  lcecSlaveTypeEL2004,
  lcecSlaveTypeEL2008,
  lcecSlaveTypeEL2022,
  lcecSlaveTypeEL2024,
  lcecSlaveTypeEL2032,
  lcecSlaveTypeEL2034,
  lcecSlaveTypeEL2042,
  lcecSlaveTypeEL2084,
  lcecSlaveTypeEL2088,
  lcecSlaveTypeEL2124,
  lcecSlaveTypeEL2202,
  lcecSlaveTypeEL2612,
  lcecSlaveTypeEL2622,
  lcecSlaveTypeEL2634,
  lcecSlaveTypeEL2652,
  lcecSlaveTypeEL2808,
  lcecSlaveTypeEL2798,
  lcecSlaveTypeEL2809,
  lcecSlaveTypeEP2008,
  lcecSlaveTypeEP2028,
  lcecSlaveTypeEP2809,
  lcecSlaveTypeEL1859,
  lcecSlaveTypeEL3102,
  lcecSlaveTypeEL3112,
  lcecSlaveTypeEL3122,
  lcecSlaveTypeEL3142,
  lcecSlaveTypeEL3152,
  lcecSlaveTypeEL3162,
  lcecSlaveTypeEL3164,
  lcecSlaveTypeEL3255,
  lcecSlaveTypeEL4001,
  lcecSlaveTypeEL4011,
  lcecSlaveTypeEL4021,
  lcecSlaveTypeEL4031,
  lcecSlaveTypeEL4002,
  lcecSlaveTypeEL4012,
  lcecSlaveTypeEL4022,
  lcecSlaveTypeEL4032,
  lcecSlaveTypeEL4008,
  lcecSlaveTypeEL4018,
  lcecSlaveTypeEL4028,
  lcecSlaveTypeEL4038,
  lcecSlaveTypeEL4102,
  lcecSlaveTypeEL4112,
  lcecSlaveTypeEL4122,
  lcecSlaveTypeEL4132,
  lcecSlaveTypeEL4104,
  lcecSlaveTypeEL4134,
  lcecSlaveTypeEL5101,
  lcecSlaveTypeEL5151,
  lcecSlaveTypeEL5152,
  lcecSlaveTypeEL2521,
  lcecSlaveTypeEL7031,
  lcecSlaveTypeEL7041_0052,
  lcecSlaveTypeEL7041_1000,
  lcecSlaveTypeEL7201_9014,
  lcecSlaveTypeEL7211,
  lcecSlaveTypeEL7221,
  lcecSlaveTypeEL7342,
  lcecSlaveTypeEL7411,
  lcecSlaveTypeEL9505,
  lcecSlaveTypeEL9576,
  lcecSlaveTypeEL9508,
  lcecSlaveTypeEL9510,
  lcecSlaveTypeEL9512,
  lcecSlaveTypeEL9515,
  lcecSlaveTypeEL6900,
  lcecSlaveTypeEL1918_LOGIC,
  lcecSlaveTypeEL1904,
  lcecSlaveTypeEL2904,
  lcecSlaveTypeEM7004,
  lcecSlaveTypeStMDS5k,
  lcecSlaveTypeDeASDA,
  lcecSlaveTypeOmrG5_KNA5L,
  lcecSlaveTypeOmrG5_KN01L,
  lcecSlaveTypeOmrG5_KN02L,
  lcecSlaveTypeOmrG5_KN04L,
  lcecSlaveTypeOmrG5_KN01H,
  lcecSlaveTypeOmrG5_KN02H,
  lcecSlaveTypeOmrG5_KN04H,
  lcecSlaveTypeOmrG5_KN08H,
  lcecSlaveTypeOmrG5_KN10H,
  lcecSlaveTypeOmrG5_KN15H,
  lcecSlaveTypeOmrG5_KN20H,
  lcecSlaveTypeOmrG5_KN30H,
  lcecSlaveTypeOmrG5_KN50H,
  lcecSlaveTypeOmrG5_KN75H,
  lcecSlaveTypeOmrG5_KN150H,
  lcecSlaveTypeOmrG5_KN06F,
  lcecSlaveTypeOmrG5_KN10F,
  lcecSlaveTypeOmrG5_KN15F,
  lcecSlaveTypeOmrG5_KN20F,
  lcecSlaveTypeOmrG5_KN30F,
  lcecSlaveTypeOmrG5_KN50F,
  lcecSlaveTypeOmrG5_KN75F,
  lcecSlaveTypeOmrG5_KN150F,
  lcecSlaveTypePh3LM2RM
} LCEC_SLAVE_TYPE_T;

typedef struct {
  uint32_t magic;
  size_t length;
} LCEC_CONF_HEADER_T;

typedef struct {
  LCEC_CONF_TYPE_T confType;
  int index;
  uint32_t appTimePeriod;
  int refClockSyncCycles;
  char name[LCEC_CONF_STR_MAXLEN];
} LCEC_CONF_MASTER_T;

typedef struct {
  LCEC_CONF_TYPE_T confType;
  int index;
  LCEC_SLAVE_TYPE_T type;
  uint32_t vid;
  uint32_t pid;
  int configPdos;
  unsigned int syncManagerCount;
  unsigned int pdoCount;
  unsigned int pdoEntryCount;
  unsigned int pdoMappingCount;
  size_t sdoConfigLength;
  size_t idnConfigLength;
  unsigned int modParamCount;
  char name[LCEC_CONF_STR_MAXLEN];
} LCEC_CONF_SLAVE_T;

typedef struct {
  LCEC_CONF_TYPE_T confType;
  uint16_t assignActivate;
  uint32_t sync0Cycle;
  int32_t sync0Shift;
  uint32_t sync1Cycle;
  int32_t sync1Shift;
} LCEC_CONF_DC_T;

typedef struct {
  LCEC_CONF_TYPE_T confType;
  uint16_t divider;
  uint16_t intervals;
} LCEC_CONF_WATCHDOG_T;

typedef struct {
  LCEC_CONF_TYPE_T confType;
  uint8_t index;
  ec_direction_t dir;
  unsigned int pdoCount;
} LCEC_CONF_SYNCMANAGER_T;

typedef struct {
  LCEC_CONF_TYPE_T confType;
  uint16_t index;
  unsigned int pdoEntryCount;
} LCEC_CONF_PDO_T;

typedef struct {
  LCEC_CONF_TYPE_T confType;
  uint16_t index;
  uint8_t subindex;
  uint8_t bitLength;
  LCEC_PDOENT_TYPE_T subType;
  hal_type_t halType;
  hal_float_t floatScale;
  hal_float_t floatOffset;
  char halPin[LCEC_CONF_STR_MAXLEN];
} LCEC_CONF_PDOENTRY_T;

typedef struct {
  LCEC_CONF_TYPE_T confType;
  uint8_t bitOffset;
  uint8_t bitLength;
  LCEC_PDOENT_TYPE_T subType;
  hal_type_t halType;
  hal_float_t floatScale;
  hal_float_t floatOffset;
  char halPin[LCEC_CONF_STR_MAXLEN];
} LCEC_CONF_COMPLEXENTRY_T;

typedef struct {
  LCEC_CONF_TYPE_T confType;
} LCEC_CONF_NULL_T;

typedef struct {
  LCEC_CONF_TYPE_T confType;
  uint16_t index;
  int16_t subindex;
  size_t length;
  uint8_t data[];
} LCEC_CONF_SDOCONF_T;

typedef struct {
  LCEC_CONF_TYPE_T confType;
  uint8_t drive;
  uint16_t idn;
  ec_al_state_t state;
  size_t length;
  uint8_t data[];
} LCEC_CONF_IDNCONF_T;

typedef union {
  hal_bit_t bit;
  hal_s32_t s32;
  hal_u32_t u32;
  hal_float_t flt;
} LCEC_CONF_MODPARAM_VAL_T;

typedef struct {
  LCEC_CONF_TYPE_T confType;
  int id;
  LCEC_CONF_MODPARAM_VAL_T value;
} LCEC_CONF_MODPARAM_T;

#endif
