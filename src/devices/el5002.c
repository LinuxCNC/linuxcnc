//
//    Copyright (C) 2023 Sascha Ittner <sascha.ittner@modusoft.de>
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
 * @file el5002.c
 * @brief Driver implementation for the Beckhoff EL5002 2-channel SSI encoder interface terminal.
 *
 * Provides HAL pins for two independent SSI absolute encoder channels. Each
 * channel exposes position (raw and scaled), error flags, and TxPDO status.
 * Channel configuration (baud rate, frame format, coding, etc.) is applied
 * during initialisation via CoE SDO writes driven by XML module parameters.
 */

#include "../lcec.h"
#include "el5002.h"

/**
 * @brief Per-channel HAL pins and PDO mapping data for one EL5002 SSI channel.
 */
typedef struct {
  hal_bit_t *reset;       /**< HAL IN: reset the relative position counter to zero. */
  hal_bit_t *abs_mode;    /**< HAL IN: when high, pos outputs the raw absolute count; otherwise outputs relative count. */
  hal_bit_t *err_data;    /**< HAL OUT: data error flag from the SSI frame (PDO 0x6000/0x01). */
  hal_bit_t *err_frame;   /**< HAL OUT: frame error flag from the SSI frame (PDO 0x6000/0x02). */
  hal_bit_t *err_power;   /**< HAL OUT: power-fail flag from the SSI frame (PDO 0x6000/0x03). */
  hal_bit_t *err_sync;    /**< HAL OUT: sync error flag (PDO 0x6000/0x0e). */
  hal_bit_t *tx_state;    /**< HAL OUT: TxPDO state bit indicating PDO validity (PDO 0x6000/0x0f). */
  hal_bit_t *tx_toggle;   /**< HAL OUT: TxPDO toggle bit; alternates each new PDO (PDO 0x6000/0x10). */
  hal_s32_t *raw_count;   /**< HAL OUT: raw 32-bit position value directly from the SSI frame. */
  hal_s32_t *count;       /**< HAL OUT: relative position count (zeroed on reset or initialisation). */
  hal_float_t *pos;       /**< HAL OUT: scaled position in user units (count * scale or raw_count * scale). */
  hal_float_t *pos_scale; /**< HAL IO: counts per user unit; reciprocal applied internally. */

  unsigned int err_data_os;   /**< Byte offset of err_data bit in process data image. */
  unsigned int err_data_bp;   /**< Bit position of err_data within the byte. */
  unsigned int err_frame_os;  /**< Byte offset of err_frame bit in process data image. */
  unsigned int err_frame_bp;  /**< Bit position of err_frame within the byte. */
  unsigned int err_power_os;  /**< Byte offset of err_power bit in process data image. */
  unsigned int err_power_bp;  /**< Bit position of err_power within the byte. */
  unsigned int err_sync_os;   /**< Byte offset of err_sync bit in process data image. */
  unsigned int err_sync_bp;   /**< Bit position of err_sync within the byte. */
  unsigned int tx_state_os;   /**< Byte offset of tx_state bit in process data image. */
  unsigned int tx_state_bp;   /**< Bit position of tx_state within the byte. */
  unsigned int tx_toggle_os;  /**< Byte offset of tx_toggle bit in process data image. */
  unsigned int tx_toggle_bp;  /**< Bit position of tx_toggle within the byte. */
  unsigned int count_pdo_os;  /**< Byte offset of the 32-bit counter value in process data image. */

  int do_init;          /**< Non-zero until the first valid read; triggers counter zeroing. */
  int32_t last_count;   /**< Previous raw counter value used to compute relative delta. */
  double old_scale;     /**< Last observed pos_scale value; used to detect pin changes. */
  double scale;         /**< Cached reciprocal of pos_scale (1.0 / pos_scale). */
} lcec_el5002_chan_t;

/**
 * @brief Top-level HAL data structure for the EL5002 slave.
 */
typedef struct {
  lcec_el5002_chan_t chans[LCEC_EL5002_CHANS]; /**< Per-channel state for each SSI encoder. */
  int last_operational; /**< Tracks whether the slave was operational on the previous cycle. */
} lcec_el5002_data_t;

static const lcec_pindesc_t slave_pins[] = {
  { HAL_BIT, HAL_IN, offsetof(lcec_el5002_chan_t, reset), "%s.%s.%s.enc-%d-reset" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el5002_chan_t, abs_mode), "%s.%s.%s.enc-%d-abs-mode" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5002_chan_t, err_data), "%s.%s.%s.enc-%d-err-data" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5002_chan_t, err_frame), "%s.%s.%s.enc-%d-err-frame" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5002_chan_t, err_power), "%s.%s.%s.enc-%d-err-power" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5002_chan_t, err_sync), "%s.%s.%s.enc-%d-err-sync" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5002_chan_t, tx_state), "%s.%s.%s.enc-%d-tx-state" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el5002_chan_t, tx_toggle), "%s.%s.%s.enc-%d-tx-toggle" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el5002_chan_t, raw_count), "%s.%s.%s.enc-%d-raw-count" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el5002_chan_t, count), "%s.%s.%s.enc-%d-count" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el5002_chan_t, pos), "%s.%s.%s.enc-%d-pos" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el5002_chan_t, pos_scale), "%s.%s.%s.enc-%d-pos-scale" },
  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static ec_pdo_entry_info_t lcec_el5002_channel1_in[] = {
   {0x6000, 0x01,  1}, // Data error
   {0x6000, 0x02,  1}, // Frame error
   {0x6000, 0x03,  1}, // Power fail
   {0x6000, 0x04,  1}, // ?
   {0x0000, 0x00,  9}, // Gap
   {0x6000, 0x0e,  1}, // Sync error
   {0x6000, 0x0f,  1}, // TxPDO state
   {0x6000, 0x10,  1}, // TxPDO toggle
   {0x6000, 0x11, 32}, // counter
};

static ec_pdo_entry_info_t lcec_el5002_channel2_in[] = {
   {0x6010, 0x01,  1}, // Data error
   {0x6010, 0x02,  1}, // Frame error
   {0x6010, 0x03,  1}, // Power fail
   {0x6010, 0x04,  1}, // ?
   {0x0010, 0x00,  9}, // Gap
   {0x6010, 0x0e,  1}, // Sync error
   {0x6010, 0x0f,  1}, // TxPDO state
   {0x6010, 0x10,  1}, // TxPDO toggle
   {0x6010, 0x11, 32}, // counter
};

static ec_pdo_info_t lcec_el5002_pdos_in[] = {
    {0x1A00, 9, lcec_el5002_channel1_in},
    {0x1A01, 9, lcec_el5002_channel2_in}
};

static ec_sync_info_t lcec_el5002_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 0, NULL},
    {3, EC_DIR_INPUT,  2, lcec_el5002_pdos_in},
    {0xff}
};

void lcec_el5002_read(struct lcec_slave *slave, long period);

/**
 * @brief Initialise the EL5002 EtherCAT slave driver.
 *
 * Iterates over module parameters to apply per-channel SDO configuration
 * (disabling frame errors, power-fail checking, inhibit time, coding, baud
 * rate, clock-jitter compensation, frame type, frame size, data length,
 * minimum inhibit time, and number of clock bursts). Then allocates HAL
 * memory, sets up sync manager configuration, registers all PDO entries, and
 * exports HAL pins for both SSI encoder channels.
 *
 * @param comp_id        LinuxCNC HAL component ID.
 * @param slave          Pointer to the lcec slave descriptor.
 * @param pdo_entry_regs Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el5002_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_slave_modparam_t *p;
  lcec_el5002_data_t *hal_data;
  int i;
  lcec_el5002_chan_t *chan;
  int err;

  // set config patameters
  for (p = slave->modparams; p != NULL && p->id >= 0; p++) {
    // get channel offset
    i = (p->id & LCEC_EL5002_PARAM_CH_MASK) << 4;
    switch(p->id & LCEC_EL5002_PARAM_FNK_MASK) {
      case LCEC_EL5002_PARAM_DIS_FRAME_ERR:
        if (ecrt_slave_config_sdo8(slave->config, 0x8000 + i, 0x01, p->value.bit) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo DisFrameErr\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL5002_PARAM_EN_PWR_FAIL_CHK:
        if (ecrt_slave_config_sdo8(slave->config, 0x8000 + i, 0x02, p->value.bit) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo EnPwrFailChk\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL5002_PARAM_EN_INHIBIT_TIME:
        if (ecrt_slave_config_sdo8(slave->config, 0x8000 + i, 0x03, p->value.bit) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo EnInhibitTime\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL5002_PARAM_CODING:
        if (ecrt_slave_config_sdo8(slave->config, 0x8000 + i, 0x06, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo Coding\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL5002_PARAM_BAUDRATE:
        if (ecrt_slave_config_sdo8(slave->config, 0x8000 + i, 0x09, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo Baudrate\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL5002_PARAM_CLK_JIT_COMP:
        if (ecrt_slave_config_sdo8(slave->config, 0x8000 + i, 0x0c, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo ClkJitComp\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL5002_PARAM_FRAME_TYPE:
        if (ecrt_slave_config_sdo8(slave->config, 0x8000 + i, 0x0f, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo FrameType\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL5002_PARAM_FRAME_SIZE:
        if (ecrt_slave_config_sdo16(slave->config, 0x8000 + i, 0x11, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo FrameSize\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL5002_PARAM_DATA_LEN:
        if (ecrt_slave_config_sdo16(slave->config, 0x8000 + i, 0x12, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo DataLen\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL5002_PARAM_MIN_INHIBIT_TIME:
        if (ecrt_slave_config_sdo16(slave->config, 0x8000 + i, 0x13, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo MinInhibitTime\n", master->name, slave->name);
          return -1;
        }
        break;
      case LCEC_EL5002_PARAM_NO_CLK_BURSTS:
        if (ecrt_slave_config_sdo16(slave->config, 0x8000 + i, 0x14, p->value.u32) != 0) {
          rtapi_print_msg (RTAPI_MSG_ERR, LCEC_MSG_PFX "fail to configure slave %s.%s sdo NoClkBursts\n", master->name, slave->name);
          return -1;
        }
        break;
    }
  }

  // initialize callbacks
  slave->proc_read = lcec_el5002_read;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el5002_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el5002_data_t));
  slave->hal_data = hal_data;

  // initialize sync info
  slave->sync_info = lcec_el5002_syncs;

  // initialize global data
  hal_data->last_operational = 0;

  // initialize pins
  for (i=0; i<LCEC_EL5002_CHANS; i++) {
    chan = &hal_data->chans[i];

    // initialize POD entries
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x01, &chan->err_data_os, &chan->err_data_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x02, &chan->err_frame_os, &chan->err_frame_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x03, &chan->err_power_os, &chan->err_power_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x0e, &chan->err_sync_os, &chan->err_sync_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x0f, &chan->tx_state_os, &chan->tx_state_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x10, &chan->tx_toggle_os, &chan->tx_toggle_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x11, &chan->count_pdo_os, NULL);

    // export pins
    if ((err = lcec_pin_newf_list(chan, slave_pins, LCEC_MODULE_NAME, master->name, slave->name, i)) != 0) {
      return err;
    }

    // initialize pins
    *(chan->pos_scale) = 1.0;

    // initialize variables
    chan->do_init = 1;
    chan->last_count = 0;
    chan->old_scale = *(chan->pos_scale) + 1.0;
    chan->scale = 1.0;
  }

  return 0;
}

/**
 * @brief EtherCAT cyclic read callback for the EL5002.
 *
 * Called every EtherCAT cycle. Reads all PDO data for each channel: error
 * flags, TxPDO status, and the 32-bit raw position counter. Computes the
 * relative position delta and updates the scaled pos output. When abs_mode
 * is active, pos reflects the raw absolute counter directly. Counter is
 * zeroed when the reset pin is asserted or on first valid cycle after a
 * slave reconnect.
 *
 * @param slave  Pointer to the lcec slave descriptor.
 * @param period EtherCAT cycle period in nanoseconds (unused).
 */
void lcec_el5002_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el5002_data_t *hal_data = (lcec_el5002_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_el5002_chan_t *chan;
  int32_t raw_count, raw_delta;

  // wait for slave to be operational
  if (!slave->state.operational) {
    hal_data->last_operational = 0;
    return;
  }

  // check inputs
  for (i=0; i<LCEC_EL5002_CHANS; i++) {
    chan = &hal_data->chans[i];

    // check for change in scale value
    if (*(chan->pos_scale) != chan->old_scale) {
      // scale value has changed, test and update it
      if ((*(chan->pos_scale) < 1e-20) && (*(chan->pos_scale) > -1e-20)) {
        // value too small, divide by zero is a bad thing
        *(chan->pos_scale) = 1.0;
      }
      // save new scale to detect future changes
      chan->old_scale = *(chan->pos_scale);
      // we actually want the reciprocal
      chan->scale = 1.0 / *(chan->pos_scale);
    }

    // get bit states
    *(chan->err_data) = EC_READ_BIT(&pd[chan->err_data_os], chan->err_data_bp);
    *(chan->err_frame) = EC_READ_BIT(&pd[chan->err_frame_os], chan->err_frame_bp);
    *(chan->err_power) = EC_READ_BIT(&pd[chan->err_power_os], chan->err_power_bp);
    *(chan->err_sync) = EC_READ_BIT(&pd[chan->err_sync_os], chan->err_sync_bp);
    *(chan->tx_state) = EC_READ_BIT(&pd[chan->tx_state_os], chan->tx_state_bp);
    *(chan->tx_toggle) = EC_READ_BIT(&pd[chan->tx_toggle_os], chan->tx_toggle_bp);

    // read raw values
    raw_count = EC_READ_S32(&pd[chan->count_pdo_os]);

    // check for operational change of slave
    if (!hal_data->last_operational) {
      chan->last_count = raw_count;
    }

    // update raw values
    *(chan->raw_count) = raw_count;

    // handle initialization
    if (chan->do_init || *(chan->reset)) {
      chan->do_init = 0;
      chan->last_count = raw_count;
      *(chan->count) = 0;
    }

    // compute net counts
    raw_delta = raw_count - chan->last_count;
    chan->last_count = raw_count;
    *(chan->count) += raw_delta;

    // scale count to make floating point position
    if (*(chan->abs_mode)) {
      *(chan->pos) = *(chan->raw_count) * chan->scale;
    } else {
      *(chan->pos) = *(chan->count) * chan->scale;
    }
  }

  hal_data->last_operational = 1;
}

