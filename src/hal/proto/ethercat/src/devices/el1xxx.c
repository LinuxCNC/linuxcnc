/**
 * @file el1xxx.c
 * @brief Driver implementation for Beckhoff EL1xxx digital input terminals.
 *
 * Handles all EL1xxx-family single-bit digital input terminals. Each channel
 * maps to a PDO entry at index 0x6000 + (channel << 4), subindex 0x01, and
 * exposes `din-<n>` and `din-<n>-not` HAL pins.
 *
 * @see el1xxx.h for supported device list and product IDs.
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

#include "../lcec.h"
#include "el1xxx.h"

/**
 * @brief Per-channel HAL data for an EL1xxx digital input terminal.
 */
typedef struct {
  hal_bit_t *in;      /**< HAL output pin: current digital input state. */
  hal_bit_t *in_not;  /**< HAL output pin: inverted digital input state. */
  unsigned int pdo_os; /**< Byte offset of this channel's PDO entry in the process data image. */
  unsigned int pdo_bp; /**< Bit position within the byte at pdo_os. */
} lcec_el1xxx_pin_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_OUT, offsetof(lcec_el1xxx_pin_t, in), "%s.%s.%s.din-%d" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el1xxx_pin_t, in_not), "%s.%s.%s.din-%d-not" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

/**
 * @brief EtherCAT cyclic read callback — updates all digital input HAL pins.
 *
 * @param slave  Pointer to the lcec slave structure.
 * @param period Servo period in nanoseconds (unused).
 */
void lcec_el1xxx_read(struct lcec_slave *slave, long period);

/**
 * @brief Initialize an EL1xxx digital input slave.
 *
 * Allocates HAL memory for all channels, registers PDO entries, and exports
 * `din-<n>` and `din-<n>-not` HAL pins for each input channel.
 *
 * @param comp_id        HAL component ID.
 * @param slave          Pointer to the lcec slave structure.
 * @param pdo_entry_regs Pointer to PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el1xxx_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el1xxx_pin_t *hal_data;
  lcec_el1xxx_pin_t *pin;
  int i;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_el1xxx_read;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el1xxx_pin_t) * slave->pdo_entry_count)) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el1xxx_pin_t) * slave->pdo_entry_count);
  slave->hal_data = hal_data;

  // initialize pins
  for (i=0, pin=hal_data; i<slave->pdo_entry_count; i++, pin++) {
    // initialize POD entry
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x01, &pin->pdo_os, &pin->pdo_bp);

    // export pins
    if ((err = lcec_pin_newf_list(comp_id, pin, slave_pins, master->instance_name, master->name, slave->name, i)) != 0) {
      return err;
    }
  }

  return 0;
}

void lcec_el1xxx_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el1xxx_pin_t *hal_data = (lcec_el1xxx_pin_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  lcec_el1xxx_pin_t *pin;
  int i, s;

  // wait for slave to be operational
  if (!slave->state.operational) {
    return;
  }

  // check inputs
  for (i=0, pin=hal_data; i<slave->pdo_entry_count; i++, pin++) {
    s = EC_READ_BIT(&pd[pin->pdo_os], pin->pdo_bp);
    *(pin->in) = s;
    *(pin->in_not) = !s;
  }
}
