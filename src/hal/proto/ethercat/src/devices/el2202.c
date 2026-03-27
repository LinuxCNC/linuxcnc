/**
 * @file el2202.c
 * @brief HAL driver for the Beckhoff EL2202 2-channel fast digital output
 *        terminal with tri-state capability.
 *
 * The EL2202 provides two independent digital output channels.  Each channel
 * exposes two HAL pins:
 *  - @c dout-N   (GOMC_HAL_BIT, IN) — desired logic level of the output.
 *  - @c tristate-N (GOMC_HAL_BIT, IN) — when TRUE the output is placed in
 *    high-impedance (tri-state) mode regardless of @c dout-N.
 *
 * EtherCAT identifiers:
 *  - Vendor ID : 0x00000002 (Beckhoff)
 *  - Product code: 0x089A3052
 *
 * @see http://www.beckhoff.com/english.asp?EtherCAT/el2202.htm
 *
 * @copyright Copyright (C) 2015-2026 Claudio lorini <claudio.lorini@iit.it>
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
#include "el2202.h"

/* Master 0, Slave 2, "EL2202"
 * Vendor ID:       0x00000002
 * Product code:    0x089a3052
 * Revision number: 0x00100000
 */

/** @brief PDO entry definitions for channel 1 (index 0x7000): Output and TriState bits. */
ec_pdo_entry_info_t lcec_el2202_ch1_out[] = {
    {0x7000, 0x01, 1}, /* Output */
    {0x7000, 0x02, 1}, /* TriState */
};
/** @brief PDO entry definitions for channel 2 (index 0x7010): Output and TriState bits. */
ec_pdo_entry_info_t lcec_el2202_ch2_out[] = {
    {0x7010, 0x01, 1}, /* Output */
    {0x7010, 0x02, 1}, /* TriState */
};

/** @brief PDO mapping for the EL2202: one PDO per channel, each with Output and TriState entries. */
ec_pdo_info_t lcec_el2202_pdos[] = {
    {0x1600, 2, lcec_el2202_ch1_out}, /* Channel 1 */
    {0x1601, 2, lcec_el2202_ch2_out}, /* Channel 2 */
};

ec_sync_info_t lcec_el2202_syncs[] = {
    {0, EC_DIR_OUTPUT, 2, lcec_el2202_pdos, EC_WD_ENABLE},
    {0xff}
};

/**
 * @brief Per-channel HAL data for the EL2202.
 *
 * Each of the two channels has its own instance of this struct which holds
 * HAL pin pointers and the corresponding PDO byte offsets / bit positions.
 */
typedef struct {
  // data exposed as PIN to Linuxcnc/Machinekit
  gomc_hal_bit_t *out;           /**< HAL input pin: desired output logic level. */
  gomc_hal_bit_t *tristate;      /**< HAL input pin: TRUE to enable tri-state (high-Z) on this channel. */
  // OffSets and BitPositions used to access data in EC PDOs
  unsigned int out_offs;        /**< Byte offset of the output bit within the process data image. */
  unsigned int out_bitp;        /**< Bit position of the output bit within the process data byte. */
  unsigned int tristate_offs;   /**< Byte offset of the tri-state bit within the process data image. */
  unsigned int tristate_bitp;   /**< Bit position of the tri-state bit within the process data byte. */
} lcec_el2202_chan_t;

/**
 * @brief Top-level HAL data for the EL2202 slave.
 *
 * Contains one ::lcec_el2202_chan_t per physical output channel.
 */
typedef struct {
  lcec_el2202_chan_t chans[LCEC_EL2202_CHANS]; /**< Per-channel data array. */
} lcec_el2202_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_el2202_chan_t, out), "%s.%s.%s.dout-%d" },
  { GOMC_HAL_BIT, GOMC_HAL_IN, offsetof(lcec_el2202_chan_t, tristate), "%s.%s.%s.tristate-%d" },
  { GOMC_HAL_TYPE_UNSPECIFIED, GOMC_HAL_DIR_UNSPECIFIED, -1, NULL }
};

/**
 * @brief Periodic write callback — copies HAL pin values to the EtherCAT PDO.
 * @param slave  Pointer to the EtherCAT slave structure.
 * @param period Servo period in nanoseconds (unused, present for callback ABI).
 */
void lcec_el2202_write(struct lcec_slave *slave, long period);

/**
 * @brief Initialize the EL2202 slave: allocate HAL memory, register PDOs, and
 *        export HAL pins.
 *
 * Sets up the write callback, allocates and zeroes the per-slave HAL data
 * structure, registers the PDO entries for both channels, and exports the
 * @c dout-N and @c tristate-N HAL pins for each channel.
 *
 * @param comp_id        HAL component ID returned by hal_init().
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array; advanced
 *                       by the number of entries registered.
 * @return 0 on success, -EIO on memory allocation failure, or a negative HAL
 *         error code if pin export fails.
 */
int lcec_el2202_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  const cmod_env_t *env = master->env;

  lcec_el2202_data_t *hal_data;
  lcec_el2202_chan_t *chan;

  int i;
  int err;

  // initialize callbacks
  slave->proc_write = lcec_el2202_write;

  // alloc hal memory
  if ((hal_data = env->hal->malloc(env->hal->ctx, sizeof(lcec_el2202_data_t))) == NULL) {
    LCEC_ERR(master, "hal_malloc() for slave %s.%s failed", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el2202_data_t));
  slave->hal_data = hal_data;

  // initializer sync info
  slave->sync_info = lcec_el2202_syncs;

  // initialize pins
  for (i=0; i<LCEC_EL2202_CHANS; i++) {
    chan = &hal_data->chans[i];

    // initialize PDO entries     position      vend.id     prod.code   index              sindx  offset             bit pos
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (i << 4), 0x01, &chan->out_offs, &chan->out_bitp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (i << 4), 0x02, &chan->tristate_offs, &chan->tristate_bitp);

    // export pins
    if ((err = lcec_pin_newf_list(env, comp_id, chan, slave_pins, master->instance_name, master->name, slave->name, i)) != 0) {
      return err;
    }
  }

  return 0;
}

/**
 * @brief Write HAL pin values for all channels to the EtherCAT process data.
 *
 * Iterates over each channel and writes the @c out and @c tristate bit values
 * from HAL into the corresponding locations in the master process data image.
 *
 * @param slave  Pointer to the EtherCAT slave structure.
 * @param period Servo period in nanoseconds (unused).
 */
void lcec_el2202_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  uint8_t *pd = master->process_data;

  lcec_el2202_data_t *hal_data = (lcec_el2202_data_t *) slave->hal_data;
  lcec_el2202_chan_t *chan;

  int i;

  for (i=0; i<LCEC_EL2202_CHANS; i++) {
    chan = &hal_data->chans[i];

    // set output
    EC_WRITE_BIT(&pd[chan->out_offs], chan->out_bitp, *(chan->out));
    // set tristate
    EC_WRITE_BIT(&pd[chan->tristate_offs], chan->tristate_bitp, *(chan->tristate));
  }
}
