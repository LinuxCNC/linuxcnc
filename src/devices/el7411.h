//
//    Copyright (C) 2018 Sascha Ittner <sascha.ittner@modusoft.de>
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
/**
 * @file el7411.h
 * @brief Driver header for the Beckhoff EL7411 BLDC motor terminal.
 *
 * The EL7411 drives brushless DC (BLDC) motors using Hall-sensor commutation.
 * It shares the EL7211 CiA-402 PDO structure (6 PDO entries) but requires
 * additional SDO configuration for motor parameters (current, voltage, speed,
 * inductance, pole pairs, thermal time constant, Hall sensor voltage, etc.).
 */
#ifndef _LCEC_EL7411_H_
#define _LCEC_EL7411_H_

#include "../lcec.h"
#include "el7211.h"

#define LCEC_EL7411_VID   LCEC_BECKHOFF_VID  /**< Beckhoff vendor ID */
#define LCEC_EL7411_PID   0x1Cf33052         /**< EL7411 product ID */

#define LCEC_EL7411_PDOS  LCEC_EL7211_PDOS   /**< PDO count (same as EL7211) */

/** @name EL7411 module parameter IDs
 *  Used in the XML configuration to tune motor electrical parameters via SDO.
 * @{ */
#define LCEC_EL7411_PARAM_DCLINK_NOM     1  /**< Nominal DC-link voltage (mV, SDO 0x8010:19) */
#define LCEC_EL7411_PARAM_DCLINK_MIN     2  /**< Minimum DC-link voltage (mV, SDO 0x8010:1A) */
#define LCEC_EL7411_PARAM_DCLINK_MAX     3  /**< Maximum DC-link voltage (mV, SDO 0x8010:1B) */
#define LCEC_EL7411_PARAM_MAX_CURR       4  /**< Maximum coil current (mA, SDO 0x8011:11) */
#define LCEC_EL7411_PARAM_RATED_CURR     5  /**< Rated coil current (mA, SDO 0x8011:12) */
#define LCEC_EL7411_PARAM_RATED_VOLT     6  /**< Rated motor voltage (mV, SDO 0x8011:2F) */
#define LCEC_EL7411_PARAM_POLE_PAIRS     7  /**< Number of pole pairs (SDO 0x8011:13) */
#define LCEC_EL7411_PARAM_RESISTANCE     8  /**< Coil resistance (mΩ, SDO 0x8011:30) */
#define LCEC_EL7411_PARAM_INDUCTANCE     9  /**< Coil inductance (µH, SDO 0x8011:19) */
#define LCEC_EL7411_PARAM_TOURQUE_CONST  10 /**< Torque constant (µNm/A, SDO 0x8011:16) */
#define LCEC_EL7411_PARAM_VOLTAGE_CONST  11 /**< Back-EMF voltage constant (µVs/rad, SDO 0x8011:31) */
#define LCEC_EL7411_PARAM_ROTOR_INERTIA  12 /**< Rotor moment of inertia (µkg·m², SDO 0x8011:18) */
#define LCEC_EL7411_PARAM_MAX_SPEED      13 /**< Maximum speed (rpm, SDO 0x8011:1B) */
#define LCEC_EL7411_PARAM_RATED_SPEED    14 /**< Rated speed (rpm, SDO 0x8011:2E) */
#define LCEC_EL7411_PARAM_TH_TIME_CONST  15 /**< Thermal time constant (ms, SDO 0x8011:2D) */
#define LCEC_EL7411_PARAM_HALL_VOLT      16 /**< Hall sensor supply voltage (mV, SDO 0x800A:11) */
#define LCEC_EL7411_PARAM_HALL_ADJUST    17 /**< Hall sensor position adjustment (°, SDO 0x800A:13) */
/** @} */

/**
 * @brief Initialise the EL7411 BLDC motor terminal.
 *
 * Configures Hall-sensor commutation mode and motor electrical parameters via
 * SDOs from the slave module params, then delegates to lcec_el7211_init() for
 * standard CiA-402 PDO mapping and HAL pin export.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el7411_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs);

#endif

