//
//    Copyright (C) 2015 Claudio lorini <claudio.lorini@iit.it>
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

/**   \brief Linuxcnc and Machinekit HAL driver for Beckhoff EL2202
      2-channel fast digital output terminal with tri-state.
      \details Voltage on Output terminal is controlled by the Output hal pin,
      if the Tristate hal pin is activated the output value is placed in high 
      impedence status. 
      http://www.beckhoff.com/english.asp?EtherCAT/el2202.htm%20 */

#include "lcec.h"
#include "lcec_el2202.h"

/* Master 0, Slave 2, "EL2202"
 * Vendor ID:       0x00000002
 * Product code:    0x089a3052
 * Revision number: 0x00100000
 */

/** \brief channels PDOs Index, SubIndex, size in bit */
ec_pdo_entry_info_t lcec_el2202_ch1_out[] = {
    {0x7000, 0x01, 1}, /* Output */
    {0x7000, 0x02, 1}, /* TriState */
};
ec_pdo_entry_info_t lcec_el2202_ch2_out[] = {
    {0x7010, 0x01, 1}, /* Output */
    {0x7010, 0x02, 1}, /* TriState */
};

/** \brief PDOs of the EL2202 */
ec_pdo_info_t lcec_el2202_pdos[] = {
    {0x1600, 2, lcec_el2202_ch1_out}, /* Channel 1 */
    {0x1601, 2, lcec_el2202_ch2_out}, /* Channel 2 */
};

ec_sync_info_t lcec_el2202_syncs[] = {
    {0, EC_DIR_OUTPUT, 2, lcec_el2202_pdos, EC_WD_ENABLE},
    {0xff}
};

/** \brief data structure of each channel */
typedef struct {
  // data exposed as PIN to Linuxcnc/Machinekit
  hal_bit_t *out;
  hal_bit_t *tristate;
  // OffSets and BitPositions used to access data in EC PDOs
  unsigned int out_offs;
  unsigned int out_bitp;
  unsigned int tristate_offs;
  unsigned int tristate_bitp;
} lcec_el2202_chan_t;

/** \brief complete data structure for EL2202 */
typedef struct {
  lcec_el2202_chan_t chans[LCEC_EL2202_CHANS];
} lcec_el2202_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_IN, offsetof(lcec_el2202_chan_t, out), "%s.%s.%s.dout-%d" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el2202_chan_t, tristate), "%s.%s.%s.tristate-%d" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

/** \brief callback for periodic IO data access*/ 
void lcec_el2202_write(struct lcec_slave *slave, long period);

int lcec_el2202_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t *pdo_entry_regs) {
  lcec_master_t *master = slave->master;

  lcec_el2202_data_t *hal_data;
  lcec_el2202_chan_t *chan;

  int i;
  int err;

  // initialize callbacks
  slave->proc_write = lcec_el2202_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el2202_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
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
    if ((err = lcec_pin_newf_list(chan, slave_pins, LCEC_MODULE_NAME, master->name, slave->name, i)) != 0) {
      return err;
    }
  }

  return 0;
}

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

