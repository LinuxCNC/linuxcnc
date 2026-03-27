/**
 * @file conf.h
 * @brief Configuration data structures for the EtherCAT HAL driver.
 *
 * This header defines the binary layout of the EtherCAT bus topology
 * configuration.  Every record begins with a @ref LCEC_CONF_TYPE_T
 * discriminator so the reader can walk the flat buffer without additional
 * framing.
 *
 * @copyright Copyright (C) 2011-2026 Sascha Ittner <sascha.ittner@modusoft.de>
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

#ifndef _LCEC_CONF_H_
#define _LCEC_CONF_H_

#include "launcher/pkg/cmodule/gomc_hal.h"

#include "ecrt.h"

/** @brief Legacy module name constant.
 *  Pin and function names now use the per-instance name from cmod New(). */
#define LCEC_MODULE_NAME "ethercat"



/** @brief Maximum length (including NUL terminator) for name/interface strings
 *         stored inside configuration records. */
#define LCEC_CONF_STR_MAXLEN 48

/** @brief Sentinel value for @ref LCEC_CONF_SDOCONF_T::subindex that requests
 *         a complete-access SDO transfer (all sub-indices in one telegram). */
#define LCEC_CONF_SDO_COMPLETE_SUBIDX -1

/** @brief Maximum number of sub-pins allowed inside a single complex PDO entry. */
#define LCEC_CONF_GENERIC_MAX_SUBPINS 32

/** @brief Maximum bit length of a single PDO entry for generic slaves. */
#define LCEC_CONF_GENERIC_MAX_BITLEN  255

/**
 * @brief Discriminator tag present at the start of every configuration record.
 *
 * The flat shared-memory buffer is a sequence of variable-length records.
 * Each record's first field is one of these values so the reader knows how to
 * interpret and size the following bytes.
 */
typedef enum {
  lcecConfTypeNone = 0,      /**< End-of-list sentinel / uninitialized. */
  lcecConfTypeMasters,       /**< Root @c \<masters\> container (no data payload). */
  lcecConfTypeMaster,        /**< @ref LCEC_CONF_MASTER_T record. */
  lcecConfTypeSlave,         /**< @ref LCEC_CONF_SLAVE_T record. */
  lcecConfTypeDcConf,        /**< @ref LCEC_CONF_DC_T distributed-clock record. */
  lcecConfTypeWatchdog,      /**< @ref LCEC_CONF_WATCHDOG_T record. */
  lcecConfTypeSyncManager,   /**< @ref LCEC_CONF_SYNCMANAGER_T record. */
  lcecConfTypePdo,           /**< @ref LCEC_CONF_PDO_T record. */
  lcecConfTypePdoEntry,      /**< @ref LCEC_CONF_PDOENTRY_T record. */
  lcecConfTypeSdoConfig,     /**< @ref LCEC_CONF_SDOCONF_T record (CoE SDO). */
  lcecConfTypeSdoDataRaw,    /**< Raw SDO data bytes appended after an SDO record. */
  lcecConfTypeIdnConfig,     /**< @ref LCEC_CONF_IDNCONF_T record (SoE IDN). */
  lcecConfTypeIdnDataRaw,    /**< Raw IDN data bytes appended after an IDN record. */
  lcecConfTypeInitCmds,      /**< Init-command block loaded from a separate XML file. */
  lcecConfTypeComplexEntry,  /**< @ref LCEC_CONF_COMPLEXENTRY_T sub-field record. */
  lcecConfTypeModParam       /**< @ref LCEC_CONF_MODPARAM_T module parameter record. */
} LCEC_CONF_TYPE_T;

/**
 * @brief Sub-type that controls how a PDO entry's raw integer bits are
 *        converted to or from the HAL pin value.
 */
typedef enum {
  lcecPdoEntTypeSimple,         /**< Direct integer cast; no conversion. */
  lcecPdoEntTypeFloatSigned,    /**< Treat raw bits as a signed integer, then apply
                                 *   scale/offset to produce a HAL float. */
  lcecPdoEntTypeFloatUnsigned,  /**< Treat raw bits as an unsigned integer, then apply
                                 *   scale/offset to produce a HAL float. */
  lcecPdoEntTypeComplex,        /**< Entry contains multiple sub-fields (@ref LCEC_CONF_COMPLEXENTRY_T).
                                 *   No direct HAL pin is created for the entry itself. */
  lcecPdoEntTypeFloatIeee       /**< Raw bits are already IEEE-754 single-precision; mapped
                                 *   directly to a HAL float without scaling. */
} LCEC_PDOENT_TYPE_T;

/**
 * @brief Enumeration of every supported EtherCAT slave device type.
 *
 * The configuration parser resolves the XML @c type attribute of a
 * @c \<slave\> element to one of these values.  The realtime component uses
 * the value to select the appropriate device driver.  @c lcecSlaveTypeGeneric
 * selects the fully user-configurable generic driver.
 */
typedef enum {
  lcecSlaveTypeInvalid,  /**< Not yet set; used as an error sentinel. */
  lcecSlaveTypeGeneric,  /**< Fully user-configurable generic slave driver. */
  lcecSlaveTypeAX5101,
  lcecSlaveTypeAX5103,
  lcecSlaveTypeAX5106,
  lcecSlaveTypeAX5112,
  lcecSlaveTypeAX5118,
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
  lcecSlaveTypeEL3204,
  lcecSlaveTypeEL3255,
  lcecSlaveTypeEL3314,
  lcecSlaveTypeEM3712,
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
  lcecSlaveTypeEL5002,
  lcecSlaveTypeEL5021,
  lcecSlaveTypeEL5032,
  lcecSlaveTypeEL5101,
  lcecSlaveTypeEL5151,
  lcecSlaveTypeEL5122,
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
  lcecSlaveTypeAX5805,
  lcecSlaveTypeEM7004,
  lcecSlaveTypeStMDS5k,
  lcecSlaveTypeDeASDA,
  lcecSlaveTypeDeMS300,
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
  lcecSlaveTypePh3LM2RM  /**< Modusoft PH3LM2RM three-phase converter. */
} LCEC_SLAVE_TYPE_T;



/**
 * @brief Configuration record for one EtherCAT master.
 *
 * Corresponds to a @c \<master\> XML element.  Master records appear first in
 * the shared-memory buffer, each immediately followed by the slave records
 * that belong to it.
 */
typedef struct {
  LCEC_CONF_TYPE_T confType;   /**< Must be @c lcecConfTypeMaster. */
  int index;                   /**< EtherCAT master index (passed to ecrt_request_master()). */
  uint32_t appTimePeriod;      /**< Application time period in nanoseconds; used to
                                *   derive DC sync cycle multiples when a @c '*' prefix
                                *   is used in the XML. */
  int refClockSyncCycles;      /**< Number of application cycles between DC reference-clock
                                *   synchronisation calls (0 = disabled). */
  int refClockSlaveIdx;        /**< EtherCAT position of the slave used as DC reference clock,
                                *   or -1 to use the first DC-capable slave. */
  char name[LCEC_CONF_STR_MAXLEN]; /**< Human-readable master name used as the HAL pin prefix.
                                    *   Defaults to the decimal string of @c index. */
#ifdef EC_USPACE_MASTER
  int transportType;                          // ec_transport_type_t, default: 0 (EC_TRANSPORT_RAW)
  char interface[LCEC_CONF_STR_MAXLEN];       // primary NIC — REQUIRED
  char backupInterface[LCEC_CONF_STR_MAXLEN]; // backup NIC or empty string
  unsigned int debugLevel;                    // default: 0
  int runOnCpu;                               // default: -1 (no binding)
#endif
} LCEC_CONF_MASTER_T;

/**
 * @brief Configuration record for one EtherCAT slave device.
 *
 * Corresponds to a @c \<slave\> XML element.  A slave record is always
 * preceded in the buffer by the @ref LCEC_CONF_MASTER_T record of its master.
 * The @c sdoConfigLength and @c idnConfigLength fields accumulate the total
 * bytes of SDO/IDN data appended after this record so the reader can advance
 * the buffer pointer correctly.
 */
typedef struct {
  LCEC_CONF_TYPE_T confType;       /**< Must be @c lcecConfTypeSlave. */
  int index;                       /**< EtherCAT slave position on the bus (0-based). */
  LCEC_SLAVE_TYPE_T type;          /**< Resolved slave type; selects the device driver. */
  uint32_t vid;                    /**< EtherCAT vendor ID (generic slaves only). */
  uint32_t pid;                    /**< EtherCAT product code (generic slaves only). */
  int configPdos;                  /**< Non-zero to configure PDOs during startup (generic only). */
  unsigned int syncManagerCount;   /**< Number of @ref LCEC_CONF_SYNCMANAGER_T records that follow. */
  unsigned int pdoCount;           /**< Total number of @ref LCEC_CONF_PDO_T records for this slave. */
  unsigned int pdoEntryCount;      /**< Total number of @ref LCEC_CONF_PDOENTRY_T records. */
  unsigned int pdoMappingCount;    /**< Number of PDO entries that have a non-empty @c halPin
                                    *   (i.e. are actually mapped to a HAL pin). */
  size_t sdoConfigLength;          /**< Cumulative byte size of all CoE SDO config data
                                    *   (including all @ref LCEC_CONF_SDOCONF_T headers). */
  size_t idnConfigLength;          /**< Cumulative byte size of all SoE IDN config data
                                    *   (including all @ref LCEC_CONF_IDNCONF_T headers). */
  unsigned int modParamCount;      /**< Number of @ref LCEC_CONF_MODPARAM_T records that follow. */
  char name[LCEC_CONF_STR_MAXLEN]; /**< Human-readable slave name used as the HAL pin prefix.
                                    *   Defaults to the decimal string of @c index. */
} LCEC_CONF_SLAVE_T;

/**
 * @brief Distributed-clock (DC) configuration for a slave.
 *
 * Corresponds to the @c \<dcConf\> XML element nested inside a @c \<slave\>.
 */
typedef struct {
  LCEC_CONF_TYPE_T confType;  /**< Must be @c lcecConfTypeDcConf. */
  uint16_t assignActivate;    /**< AssignActivate word passed to ecrt_slave_config_dc()
                               *   (hex value from XML, e.g. 0x0700). */
  uint32_t sync0Cycle;        /**< SYNC0 cycle time in nanoseconds. */
  int32_t sync0Shift;         /**< SYNC0 shift time in nanoseconds. */
  uint32_t sync1Cycle;        /**< SYNC1 cycle time in nanoseconds (0 = disabled). */
  int32_t sync1Shift;         /**< SYNC1 shift time in nanoseconds. */
} LCEC_CONF_DC_T;

/**
 * @brief Watchdog configuration for a slave.
 *
 * Corresponds to the @c \<watchdog\> XML element nested inside a @c \<slave\>.
 */
typedef struct {
  LCEC_CONF_TYPE_T confType;  /**< Must be @c lcecConfTypeWatchdog. */
  uint16_t divider;           /**< Watchdog divider value passed to ecrt_slave_config_watchdog(). */
  uint16_t intervals;         /**< Watchdog interval count. */
} LCEC_CONF_WATCHDOG_T;

/**
 * @brief Sync manager (SM) configuration record for a generic slave.
 *
 * Corresponds to the @c \<syncManager\> XML element.  Only valid for slaves
 * of type @c lcecSlaveTypeGeneric.
 */
typedef struct {
  LCEC_CONF_TYPE_T confType;   /**< Must be @c lcecConfTypeSyncManager. */
  uint8_t index;               /**< Sync manager index (0–3). */
  ec_direction_t dir;          /**< Direction: @c EC_DIR_INPUT or @c EC_DIR_OUTPUT. */
  unsigned int pdoCount;       /**< Number of @ref LCEC_CONF_PDO_T records assigned to this SM. */
} LCEC_CONF_SYNCMANAGER_T;

/**
 * @brief PDO (Process Data Object) assignment record.
 *
 * Corresponds to the @c \<pdo\> XML element nested inside a @c \<syncManager\>.
 */
typedef struct {
  LCEC_CONF_TYPE_T confType;     /**< Must be @c lcecConfTypePdo. */
  uint16_t index;                /**< PDO index (e.g. 0x1600 for the first RxPDO). */
  unsigned int pdoEntryCount;    /**< Number of @ref LCEC_CONF_PDOENTRY_T records in this PDO. */
} LCEC_CONF_PDO_T;

/**
 * @brief PDO entry mapping record.
 *
 * Corresponds to the @c \<pdoEntry\> XML element.  Describes one entry within
 * a PDO and the optional HAL pin to which it should be mapped.
 */
typedef struct {
  LCEC_CONF_TYPE_T confType;       /**< Must be @c lcecConfTypePdoEntry. */
  uint16_t index;                  /**< Object dictionary index of the mapped object. */
  uint8_t subindex;                /**< Sub-index of the mapped object. */
  uint8_t bitLength;               /**< Length of the entry in bits (1–255). */
  LCEC_PDOENT_TYPE_T subType;      /**< Conversion type for raw-to-HAL mapping. */
  gomc_hal_type_t halType;              /**< HAL data type of the pin to create. */
  gomc_hal_float_t floatScale;          /**< Scale factor applied when @c subType is float
                                    *   (raw × scale + offset → HAL float). */
  gomc_hal_float_t floatOffset;         /**< Offset applied after scaling for float conversion. */
  char halPin[LCEC_CONF_STR_MAXLEN]; /**< HAL pin name suffix, or empty string if the entry
                                      *   is mapped but no pin is desired. */
} LCEC_CONF_PDOENTRY_T;

/**
 * @brief Sub-field entry within a complex PDO entry.
 *
 * Corresponds to the @c \<complexEntry\> XML element nested inside a
 * @c \<pdoEntry\> that has @c halType="complex".  Multiple complex entries
 * carve up the parent entry's bit field and each gets its own HAL pin.
 */
typedef struct {
  LCEC_CONF_TYPE_T confType;       /**< Must be @c lcecConfTypeComplexEntry. */
  uint8_t bitOffset;               /**< Bit offset of this sub-field within the parent PDO entry. */
  uint8_t bitLength;               /**< Width of this sub-field in bits. */
  LCEC_PDOENT_TYPE_T subType;      /**< Conversion type for raw-to-HAL mapping. */
  gomc_hal_type_t halType;              /**< HAL data type of the pin to create. */
  gomc_hal_float_t floatScale;          /**< Scale factor (same semantics as @ref LCEC_CONF_PDOENTRY_T). */
  gomc_hal_float_t floatOffset;         /**< Offset (same semantics as @ref LCEC_CONF_PDOENTRY_T). */
  char halPin[LCEC_CONF_STR_MAXLEN]; /**< HAL pin name suffix for this sub-field. */
} LCEC_CONF_COMPLEXENTRY_T;

/**
 * @brief Minimal record used as an end-of-stream marker in the shared buffer.
 *
 * A @ref LCEC_CONF_NULL_T with @c confType == @c lcecConfTypeNone is written
 * after all real records.  The realtime component stops reading when it
 * encounters this sentinel.
 */
typedef struct {
  LCEC_CONF_TYPE_T confType;  /**< Must be @c lcecConfTypeNone to act as end marker. */
} LCEC_CONF_NULL_T;

/**
 * @brief CoE (CANopen over EtherCAT) SDO startup-configuration record.
 *
 * Corresponds to the @c \<sdoConfig\> XML element.  The raw data bytes
 * are stored in the flexible array member @c data immediately following
 * the struct in the shared-memory buffer.
 */
typedef struct {
  LCEC_CONF_TYPE_T confType;  /**< Must be @c lcecConfTypeSdoConfig. */
  uint16_t index;             /**< Object dictionary index of the SDO to write. */
  int16_t subindex;           /**< Sub-index, or @ref LCEC_CONF_SDO_COMPLETE_SUBIDX
                               *   for a complete-access transfer. */
  size_t length;              /**< Number of data bytes in @c data[]. */
  uint8_t data[];             /**< Flexible array: the raw SDO payload bytes. */
} LCEC_CONF_SDOCONF_T;

/**
 * @brief SoE (Servo over EtherCAT) IDN startup-configuration record.
 *
 * Corresponds to the @c \<idnConfig\> XML element.  The raw data bytes
 * are stored in the flexible array member @c data immediately following
 * the struct in the shared-memory buffer.
 */
typedef struct {
  LCEC_CONF_TYPE_T confType;  /**< Must be @c lcecConfTypeIdnConfig. */
  uint8_t drive;              /**< SoE drive number (0–7). */
  uint16_t idn;               /**< IDN number (16-bit, S/P notation resolved to raw). */
  ec_al_state_t state;        /**< AL state at which to apply this IDN write
                               *   (@c EC_AL_STATE_PREOP or @c EC_AL_STATE_SAFEOP). */
  size_t length;              /**< Number of data bytes in @c data[]. */
  uint8_t data[];             /**< Flexible array: the raw IDN payload bytes. */
} LCEC_CONF_IDNCONF_T;

/**
 * @brief Union holding the value of a single module parameter.
 *
 * The active member is determined by the parameter's declared type
 * (@ref LCEC_CONF_MODPARAM_TYPE_T in conf.c).
 */
typedef union {
  gomc_hal_bit_t bit;                   /**< Boolean (bit) value. */
  gomc_hal_s32_t s32;                   /**< Signed 32-bit integer value. */
  gomc_hal_u32_t u32;                   /**< Unsigned 32-bit integer value. */
  gomc_hal_float_t flt;                 /**< Floating-point value. */
  char str[LCEC_CONF_STR_MAXLEN];  /**< String value. */
} LCEC_CONF_MODPARAM_VAL_T;

/**
 * @brief Module (driver) parameter record for a slave device.
 *
 * Corresponds to the @c \<modParam\> XML element nested inside a @c \<slave\>.
 * Module parameters are driver-specific knobs that do not fit into the
 * standard PDO / SDO / IDN configuration.
 */
typedef struct {
  LCEC_CONF_TYPE_T confType;     /**< Must be @c lcecConfTypeModParam. */
  int id;                        /**< Driver-specific parameter identifier resolved from
                                  *   the parameter name by the configuration parser. */
  LCEC_CONF_MODPARAM_VAL_T value; /**< Parameter value; the active union member depends on
                                   *   the parameter's declared type. */
} LCEC_CONF_MODPARAM_T;

#endif
