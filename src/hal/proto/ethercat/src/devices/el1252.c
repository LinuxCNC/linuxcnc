/**
 * @file el1252.c
 * @brief Driver implementation for Beckhoff EL1252 2-channel fast digital input with timestamp.
 *
 * LatchPosX records the time of the last positive edge (0→1) on input X.
 * LatchNegX records the time of the last negative edge (1→0) on input X.
 * StatusX.0 is set on a positive edge; StatusX.1 is set on a negative edge.
 * Capture bits are cleared when the corresponding latch register is read.
 *
 * @note Timestamp data is captured internally but not yet forwarded to HAL pins.
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

/**   \brief Linuxcnc and Machinekit HAL driver for Beckhoff EL1252
      2-channel fast digital input terminal with timestamp.

      \details LatchPosX is time of the last positive edge event (0->1) on input X.
      LatchNegX is time of the last negative edge event (1_>0) on input X.
      StatusX.0 (bit 0) is set when a positive edge event (0->1) is detected on the inputX.
      StatusX.1 (bit 1) is set when a negative edge event (1->0) is detected on the inputX.
      StatusX capture bits are cleared by reading the corrisponding latch register.

      \note This driver at the moment provides pins ONLY for input data,
      timestamping infos is not exposed to HAL.

      http://www.beckhoff.com/english.asp?EtherCAT/el1252.htm */

#include "../lcec.h"
#include "el1252.h"

/* Master 0, Slave 1, "EL1252"
 * Vendor ID:       0x00000002
 * Product code:    0x04e43052
 * Revision number: 0x00120000
 */

/*
SM0: PhysAddr 0x1000, DefaultSize    1, ControlRegister 0x22, Enable 1
  TxPDO 0x1a00 "Channel 1"
    PDO entry 0x6000:01,  1 bit, "Input"
  TxPDO 0x1a01 "Channel 2"
    PDO entry 0x6000:02,  1 bit, "Input"
    PDO entry 0x0000:00,  6 bit, ""
  TxPDO 0x1a02 "Reserved"
SM1: PhysAddr 0x09ae, DefaultSize    0, ControlRegister 0x00, Enable 4
  TxPDO 0x1a13 "Latch"
    PDO entry 0x1d09:ae,  8 bit, "Status1"
    PDO entry 0x1d09:af,  8 bit, "Status2"
    PDO entry 0x1d09:b0, 64 bit, "LatchPos1"
    PDO entry 0x1d09:b8, 64 bit, "LatchNeg1"
    PDO entry 0x1d09:c0, 64 bit, "LatchPos2"
    PDO entry 0x1d09:c8, 64 bit, "LatchNeg2"
SM2: PhysAddr 0x0910, DefaultSize    0, ControlRegister 0x00, Enable 4
*/

ec_pdo_entry_info_t lcec_el1252_entries[] = {
//   Index,  SIndx,Len
    {0x6000, 0x01, 1},  /* Input */
    {0x6000, 0x02, 1},  /* Input */
    {0x0000, 0x00, 6},  /* gap */
    {0x1d09, 0xae, 8},  /* Status1 */
    {0x1d09, 0xaf, 8},  /* Status2 */
    {0x1d09, 0xb0, 64}, /* LatchPos1 */
    {0x1d09, 0xb8, 64}, /* LatchNeg1 */
    {0x1d09, 0xc0, 64}, /* LatchPos2 */
    {0x1d09, 0xc8, 64}, /* LatchNeg2 */
};

ec_pdo_info_t lcec_el1252_pdos[] = {
//   Index,  n.PDOs, array of PDO entries
    {0x1a00, 1, lcec_el1252_entries + 0}, /* Channel 1 */
    {0x1a01, 2, lcec_el1252_entries + 1}, /* Channel 2 */
    {0x1a02, 0, NULL},                    /* Reserved */
    {0x1a13, 6, lcec_el1252_entries + 3}, /* Status and Latches */
};

ec_sync_info_t lcec_el1252_syncs[] = {
//  Indx, SM direction, n.PDOs, array of PDOs, WD mode
    {0, EC_DIR_INPUT, 3, lcec_el1252_pdos + 0, EC_WD_DISABLE},
    {1, EC_DIR_INPUT, 1, lcec_el1252_pdos + 3, EC_WD_DISABLE},
    {2, EC_DIR_INPUT, 0, NULL, EC_WD_DISABLE},
    {0xff}
};

/** \brief data structure of one channel of the device */
typedef struct {
  // data exposed as PIN to Linuxcnc/Machinekit
  hal_bit_t *in;      /**< HAL output pin: current digital input state. */

  uint8_t    Status;     /**< Cached edge-detection status byte from the PDO. */
  uint64_t   LatchPos;   /**< Cached timestamp of last positive edge (0→1). */
  uint64_t   LatchNeg;   /**< Cached timestamp of last negative edge (1→0). */

  // OffSets and BitPositions used to access data in EC PDOs
  unsigned int in_offs;  /**< Byte offset of the input bit in the process data image. */
  unsigned int in_bitp;  /**< Bit position within the byte at in_offs. */

  unsigned int Status_offs;   /**< Byte offset of the Status byte in the process data image. */
  unsigned int LatchPos_offs; /**< Byte offset of the LatchPos timestamp in the process data image. */
  unsigned int LatchNeg_offs; /**< Byte offset of the LatchNeg timestamp in the process data image. */

} lcec_el1252_chan_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_OUT, offsetof(lcec_el1252_chan_t, in), "%s.%s.%s.din-%d" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

/** \brief complete data structure for EL1252 */
typedef struct {
  lcec_el1252_chan_t chans[LCEC_EL1252_CHANS]; /**< Per-channel HAL and PDO data. */
} lcec_el1252_data_t;

/** \brief callback for periodic IO data access*/
void lcec_el1252_read(struct lcec_slave *slave, long period);

/**
 * @brief Initialize the EL1252 fast digital input slave.
 *
 * Allocates HAL memory, sets up sync manager configuration, registers PDO
 * entries for input bits and latch/status data, and exports `din-<n>` pins.
 *
 * @param comp_id        HAL component ID.
 * @param slave          Pointer to the lcec slave structure.
 * @param pdo_entry_regs Pointer to PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el1252_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el1252_data_t *hal_data;
  int i;
  lcec_el1252_chan_t *chan;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_el1252_read;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el1252_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el1252_data_t));
  slave->hal_data = hal_data;

  // initializer sync info
  slave->sync_info = lcec_el1252_syncs;

  for (i=0; i<LCEC_EL1252_CHANS; i++) {
    chan = &hal_data->chans[i];

    // initialize PDO entries     position      vend.id     prod.code   index   sindx            offset                             bit pos
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000, 0x01 + i,        &hal_data->chans[i].in_offs,       &hal_data->chans[i].in_bitp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x1d09, 0xae + i,        &hal_data->chans[i].Status_offs,   NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x1d09, 0xb0 + (i << 4), &hal_data->chans[i].LatchPos_offs, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x1d09, 0xb8 + (i << 4), &hal_data->chans[i].LatchNeg_offs, NULL);

    // export pins
    if ((err = lcec_pin_newf_list(comp_id, chan, slave_pins, master->instance_name, master->name, slave->name, i)) != 0) {
      return err;
    }
  }

  return 0;
}

void lcec_el1252_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  uint8_t *pd = master->process_data;
  lcec_el1252_data_t *hal_data = (lcec_el1252_data_t *) slave->hal_data;
  int i;
  lcec_el1252_chan_t *chan;

  for (i=0; i<LCEC_EL1252_CHANS; i++) {
    chan = &hal_data->chans[i];

    // read inputs
    *(chan->in) = EC_READ_BIT(&pd[chan->in_offs], chan->in_bitp);

    /** TODO: do-something with timestamp data! */
    chan->Status = EC_READ_U8(&pd[chan->Status_offs]);
    chan->LatchPos = EC_READ_U8(&pd[chan->LatchPos_offs]);
    chan->LatchNeg = EC_READ_U8(&pd[chan->LatchNeg_offs]);
  }
}
