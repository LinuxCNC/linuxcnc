/** @file el7342.c
 * @brief Driver for the Beckhoff EL7342 2-channel DC motor terminal.
 *
 * Implements two independent velocity-mode DC motor drive channels. Each
 * channel has its own encoder, velocity setpoint, and status/control PDOs.
 * Two selectable synchronous information words per channel allow monitoring
 * of motor voltage, current, duty cycle, velocity, temperature, and more.
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
#include "el7342.h"

/** @name Synchronous information channel selector constants
 *  Values written to dcm_sel_info1 / dcm_sel_info2 HAL pins to choose
 *  which quantity appears in dcm_raw_info1 / dcm_raw_info2.
 * @{
 */
#define INFO_SEL_STATUS_WORD   0   /**< Show the drive status word */
#define INFO_SEL_MOTOR_VOLT    1   /**< Show motor voltage */
#define INFO_SEL_MOTOR_CURR    2   /**< Show motor current */
#define INFO_SEL_CURR_LIMIT    3   /**< Show current limit */
#define INFO_SEL_CTRL_ERR      4   /**< Show controller error */
#define INFO_SEL_DUTY_CYCLE    5   /**< Show PWM duty cycle */
#define INFO_SEL_MOTOR_VELO    7   /**< Show motor velocity */
#define INFO_SEL_OVERLOAD_TIME 8   /**< Show overload time counter */
#define INFO_SEL_INT_TEMP      101 /**< Show internal temperature */
#define INFO_SEL_CTRL_VOLT     103 /**< Show controller supply voltage */
#define INFO_SEL_SUPP_VOLT     104 /**< Show power supply voltage */
#define INFO_SEL_DCM_SWORD     150 /**< Show DC motor status word */
#define INFO_SEL_DCM_STATE     151 /**< Show DC motor state */
/** @} */

/**
 * @brief Per-channel HAL data for one EL7342 DC motor channel.
 *
 * Holds HAL pin pointers, PDO offsets/bit positions, and internal state
 * for both the encoder sub-interface and the DC motor drive sub-interface
 * of one channel.
 */
typedef struct {
  hal_bit_t *reset;               /**< IN: reset the encoder count to zero */
  hal_bit_t *ina;                 /**< OUT: encoder channel A state */
  hal_bit_t *inb;                 /**< OUT: encoder channel B state */
  hal_bit_t *inext;               /**< OUT: external latch input state */
  hal_bit_t *sync_err;            /**< OUT: encoder sync error flag */
  hal_bit_t *expol_stall;         /**< OUT: encoder extrapolation stall flag */
  hal_bit_t *count_overflow;      /**< OUT: encoder counter overflow flag */
  hal_bit_t *count_underflow;     /**< OUT: encoder counter underflow flag */
  hal_bit_t *tx_toggle;           /**< OUT: encoder TxPDO toggle (new data indicator) */
  hal_bit_t *set_raw_count;       /**< IO: write 1 to preset the encoder counter */
  hal_s32_t *set_raw_count_val;   /**< IN: preset value for the encoder counter */
  hal_bit_t *latch_ext_valid;     /**< OUT: external latch valid flag */
  hal_bit_t *ena_latch_ext_pos;   /**< IO: enable external latch on positive edge */
  hal_bit_t *ena_latch_ext_neg;   /**< IO: enable external latch on negative edge */
  hal_s32_t *raw_count;           /**< OUT: raw encoder counter value */
  hal_s32_t *raw_latch;           /**< OUT: raw encoder latch value */
  hal_s32_t *count;               /**< OUT: accumulated encoder count */
  hal_float_t *pos_scale;         /**< IO: encoder counts per user unit */
  hal_float_t *pos;               /**< OUT: position in user units */

  hal_bit_t *dcm_reset;           /**< IN: drive reset (clear faults) */
  hal_bit_t *dcm_reduce_torque;   /**< IN: activate reduced torque */
  hal_bit_t *dcm_enable;          /**< IN: enable the drive */
  hal_bit_t *dcm_absmode;         /**< IN: use absolute velocity mode */
  hal_float_t *dcm_value;         /**< IN: commanded velocity in user units */
  hal_float_t *dcm_scale;         /**< IO: velocity scale factor */
  hal_float_t *dcm_offset;        /**< IO: velocity offset */
  hal_float_t *dcm_min_dc;        /**< IO: minimum duty cycle */
  hal_float_t *dcm_max_dc;        /**< IO: maximum duty cycle */
  hal_float_t *dcm_curr_dc;       /**< OUT: current duty cycle */
  hal_s32_t *dcm_raw_val;         /**< OUT: raw 16-bit velocity setpoint */
  hal_bit_t *dcm_ready_to_enable; /**< OUT: drive ready-to-enable */
  hal_bit_t *dcm_ready;           /**< OUT: drive ready */
  hal_bit_t *dcm_warning;         /**< OUT: drive warning */
  hal_bit_t *dcm_error;           /**< OUT: drive error */
  hal_bit_t *dcm_move_pos;        /**< OUT: moving in positive direction */
  hal_bit_t *dcm_move_neg;        /**< OUT: moving in negative direction */
  hal_bit_t *dcm_torque_reduced;  /**< OUT: reduced torque active */
  hal_bit_t *dcm_din1;            /**< OUT: digital input 1 state */
  hal_bit_t *dcm_din2;            /**< OUT: digital input 2 state */
  hal_bit_t *dcm_sync_err;        /**< OUT: drive sync error flag */
  hal_bit_t *dcm_tx_toggle;       /**< OUT: drive TxPDO toggle */
  hal_s32_t *dcm_raw_info1;       /**< OUT: raw value from synchronous info channel 1 */
  hal_s32_t *dcm_raw_info2;       /**< OUT: raw value from synchronous info channel 2 */
  hal_u32_t *dcm_sel_info1;       /**< OUT: selected info channel 1 type (INFO_SEL_*) */
  hal_u32_t *dcm_sel_info2;       /**< OUT: selected info channel 2 type (INFO_SEL_*) */
  hal_float_t *dcm_velo_fb;       /**< OUT: velocity feedback (reserved) */
  hal_float_t *dcm_current_fb;    /**< OUT: current feedback (reserved) */

  unsigned int set_count_pdo_os;          /**< PDO byte offset: set counter command */
  unsigned int set_count_pdo_bp;          /**< Bit position: set counter command */
  unsigned int set_count_val_pdo_os;      /**< PDO byte offset: set counter value */
  unsigned int set_count_done_pdo_os;     /**< PDO byte offset: set counter done */
  unsigned int set_count_done_pdo_bp;     /**< Bit position: set counter done */
  unsigned int expol_stall_pdo_os;        /**< PDO byte offset: extrapolation stall */
  unsigned int expol_stall_pdo_bp;        /**< Bit position: extrapolation stall */
  unsigned int ina_pdo_os;               /**< PDO byte offset: encoder input A */
  unsigned int ina_pdo_bp;               /**< Bit position: encoder input A */
  unsigned int inb_pdo_os;               /**< PDO byte offset: encoder input B */
  unsigned int inb_pdo_bp;               /**< Bit position: encoder input B */
  unsigned int inext_pdo_os;             /**< PDO byte offset: external latch input */
  unsigned int inext_pdo_bp;             /**< Bit position: external latch input */
  unsigned int sync_err_pdo_os;          /**< PDO byte offset: encoder sync error */
  unsigned int sync_err_pdo_bp;          /**< Bit position: encoder sync error */
  unsigned int tx_toggle_pdo_os;         /**< PDO byte offset: encoder TxPDO toggle */
  unsigned int tx_toggle_pdo_bp;         /**< Bit position: encoder TxPDO toggle */
  unsigned int count_overflow_pdo_os;    /**< PDO byte offset: counter overflow */
  unsigned int count_overflow_pdo_bp;    /**< Bit position: counter overflow */
  unsigned int count_underflow_pdo_os;   /**< PDO byte offset: counter underflow */
  unsigned int count_underflow_pdo_bp;   /**< Bit position: counter underflow */
  unsigned int latch_ext_valid_pdo_os;   /**< PDO byte offset: external latch valid */
  unsigned int latch_ext_valid_pdo_bp;   /**< Bit position: external latch valid */
  unsigned int ena_latch_ext_pos_pdo_os; /**< PDO byte offset: enable latch on pos edge */
  unsigned int ena_latch_ext_pos_pdo_bp; /**< Bit position: enable latch on pos edge */
  unsigned int ena_latch_ext_neg_pdo_os; /**< PDO byte offset: enable latch on neg edge */
  unsigned int ena_latch_ext_neg_pdo_bp; /**< Bit position: enable latch on neg edge */
  unsigned int count_pdo_os;            /**< PDO byte offset: 16-bit counter value */
  unsigned int latch_pdo_os;            /**< PDO byte offset: 16-bit latch value */

  unsigned int dcm_ena_pdo_os;              /**< PDO byte offset: drive enable command */
  unsigned int dcm_ena_pdo_bp;              /**< Bit position: drive enable command */
  unsigned int dcm_reset_pdo_os;            /**< PDO byte offset: drive reset command */
  unsigned int dcm_reset_pdo_bp;            /**< Bit position: drive reset command */
  unsigned int dcm_reduce_torque_pdo_os;    /**< PDO byte offset: reduce torque command */
  unsigned int dcm_reduce_torque_pdo_bp;    /**< Bit position: reduce torque command */
  unsigned int dcm_velo_pdo_os;             /**< PDO byte offset: 16-bit velocity setpoint */
  unsigned int dcm_ready_to_enable_pdo_os;  /**< PDO byte offset: ready-to-enable status */
  unsigned int dcm_ready_to_enable_pdo_bp;  /**< Bit position: ready-to-enable status */
  unsigned int dcm_ready_pdo_os;            /**< PDO byte offset: drive ready */
  unsigned int dcm_ready_pdo_bp;            /**< Bit position: drive ready */
  unsigned int dcm_warning_pdo_os;          /**< PDO byte offset: drive warning */
  unsigned int dcm_warning_pdo_bp;          /**< Bit position: drive warning */
  unsigned int dcm_error_pdo_os;            /**< PDO byte offset: drive error */
  unsigned int dcm_error_pdo_bp;            /**< Bit position: drive error */
  unsigned int dcm_move_pos_pdo_os;         /**< PDO byte offset: moving positive */
  unsigned int dcm_move_pos_pdo_bp;         /**< Bit position: moving positive */
  unsigned int dcm_move_neg_pdo_os;         /**< PDO byte offset: moving negative */
  unsigned int dcm_move_neg_pdo_bp;         /**< Bit position: moving negative */
  unsigned int dcm_torque_reduced_pdo_os;   /**< PDO byte offset: torque reduced */
  unsigned int dcm_torque_reduced_pdo_bp;   /**< Bit position: torque reduced */
  unsigned int dcm_din1_pdo_os;             /**< PDO byte offset: digital input 1 */
  unsigned int dcm_din1_pdo_bp;             /**< Bit position: digital input 1 */
  unsigned int dcm_din2_pdo_os;             /**< PDO byte offset: digital input 2 */
  unsigned int dcm_din2_pdo_bp;             /**< Bit position: digital input 2 */
  unsigned int dcm_sync_err_pdo_os;         /**< PDO byte offset: drive sync error */
  unsigned int dcm_sync_err_pdo_bp;         /**< Bit position: drive sync error */
  unsigned int dcm_tx_toggle_pdo_os;        /**< PDO byte offset: drive TxPDO toggle */
  unsigned int dcm_tx_toggle_pdo_bp;        /**< Bit position: drive TxPDO toggle */
  unsigned int dcm_info1_pdo_os;            /**< PDO byte offset: synchronous info word 1 */
  unsigned int dcm_info2_pdo_os;            /**< PDO byte offset: synchronous info word 2 */

  int enc_do_init;              /**< Flag: 1 on first read after startup */
  int16_t enc_last_count;       /**< Previous raw encoder count for delta computation */
  double enc_old_scale;         /**< Previously applied encoder scale (change detection) */
  double enc_scale_recip;         /**< Reciprocal of current encoder scale factor */
  double dcm_old_scale;           /**< Previously applied drive scale (change detection) */
  double dcm_scale_recip;         /**< Reciprocal of current drive scale factor */

} lcec_el7342_chan_t;

/**
 * @brief Top-level HAL data for the EL7342 terminal.
 */
typedef struct {
  lcec_el7342_chan_t chans[LCEC_EL7342_CHANS]; /**< Per-channel data (2 channels) */
  int last_operational;                        /**< 1 when slave was operational on previous cycle */
} lcec_el7342_data_t;

static const lcec_pindesc_t slave_pins[] = {
  // encoder pins
  { HAL_BIT, HAL_IN, offsetof(lcec_el7342_chan_t, reset), "%s.%s.%s.enc-%d-reset" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, ina), "%s.%s.%s.enc-%d-ina" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, inb), "%s.%s.%s.enc-%d-inb" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, inext), "%s.%s.%s.enc-%d-inext" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, sync_err), "%s.%s.%s.enc-%d-sync-error" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, expol_stall), "%s.%s.%s.enc-%d-expol-stall" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, tx_toggle), "%s.%s.%s.enc-%d-tx-toggle" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, count_overflow), "%s.%s.%s.enc-%d-count-overflow" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, count_underflow), "%s.%s.%s.enc-%d-count-underflow" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, latch_ext_valid), "%s.%s.%s.enc-%d-latch-ext-valid" },
  { HAL_BIT, HAL_IO, offsetof(lcec_el7342_chan_t, set_raw_count), "%s.%s.%s.enc-%d-set-raw-count" },
  { HAL_BIT, HAL_IO, offsetof(lcec_el7342_chan_t, ena_latch_ext_pos), "%s.%s.%s.enc-%d-index-ext-pos-enable" },
  { HAL_BIT, HAL_IO, offsetof(lcec_el7342_chan_t, ena_latch_ext_neg), "%s.%s.%s.enc-%d-index-ext-neg-enable" },
  { HAL_S32, HAL_IN, offsetof(lcec_el7342_chan_t, set_raw_count_val), "%s.%s.%s.enc-%d-set-raw-count-val" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el7342_chan_t, raw_count), "%s.%s.%s.enc-%d-raw-count" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el7342_chan_t, count), "%s.%s.%s.enc-%d-count" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el7342_chan_t, raw_latch), "%s.%s.%s.enc-%d-raw-latch" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el7342_chan_t, pos), "%s.%s.%s.enc-%d-pos" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el7342_chan_t, pos_scale), "%s.%s.%s.enc-%d-pos-scale" },

  // encoder pins
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el7342_chan_t, dcm_scale), "%s.%s.%s.srv-%d-scale" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el7342_chan_t, dcm_offset), "%s.%s.%s.srv-%d-offset" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el7342_chan_t, dcm_min_dc), "%s.%s.%s.srv-%d-min-dc" },
  { HAL_FLOAT, HAL_IO, offsetof(lcec_el7342_chan_t, dcm_max_dc), "%s.%s.%s.srv-%d-max-dc" },
  { HAL_FLOAT, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_curr_dc), "%s.%s.%s.srv-%d-curr-dc" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el7342_chan_t, dcm_enable), "%s.%s.%s.srv-%d-enable" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el7342_chan_t, dcm_absmode), "%s.%s.%s.srv-%d-absmode" },
  { HAL_FLOAT, HAL_IN, offsetof(lcec_el7342_chan_t, dcm_value), "%s.%s.%s.srv-%d-cmd" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_raw_val), "%s.%s.%s.srv-%d-raw-cmd" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el7342_chan_t, dcm_reset), "%s.%s.%s.srv-%d-reset" },
  { HAL_BIT, HAL_IN, offsetof(lcec_el7342_chan_t, dcm_reduce_torque), "%s.%s.%s.srv-%d-reduce-torque" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_ready_to_enable), "%s.%s.%s.srv-%d-ready-to-enable" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_ready), "%s.%s.%s.srv-%d-ready" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_warning), "%s.%s.%s.srv-%d-warning" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_error), "%s.%s.%s.srv-%d-error" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_move_pos), "%s.%s.%s.srv-%d-move-pos" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_move_neg), "%s.%s.%s.srv-%d-move-neg" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_torque_reduced), "%s.%s.%s.srv-%d-torque-reduced" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_din1), "%s.%s.%s.srv-%d-din1" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_din2), "%s.%s.%s.srv-%d-din2" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_sync_err), "%s.%s.%s.srv-%d-sync-error" },
  { HAL_BIT, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_tx_toggle), "%s.%s.%s.srv-%d-tx-toggle" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_raw_info1), "%s.%s.%s.srv-%d-raw-info1" },
  { HAL_S32, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_raw_info2), "%s.%s.%s.srv-%d-raw-info2" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_sel_info1), "%s.%s.%s.srv-%d-sel-info1" },
  { HAL_U32, HAL_OUT, offsetof(lcec_el7342_chan_t, dcm_sel_info2), "%s.%s.%s.srv-%d-sel-info2" },

  { HAL_TYPE_UNSPECIFIED, HAL_DIR_UNSPECIFIED, -1, NULL }
};

static ec_pdo_entry_info_t lcec_el7342_channel1_enc_out[] = {
    {0x0000, 0x00,  1}, // Gap
    {0x7000, 0x02,  1}, // Enable latch extern on positive edge
    {0x7000, 0x03,  1}, // Set counter
    {0x7000, 0x04,  1}, // Enable latch extern on negative edge
    {0x0000, 0x00,  4}, // Gap
    {0x0000, 0x00,  8}, // Gap
    {0x7000, 0x11, 16}  // Set counter value
};

static ec_pdo_entry_info_t lcec_el7342_channel2_enc_out[] = {
    {0x0000, 0x00,  1}, // Gap
    {0x7010, 0x02,  1}, // Enable latch extern on positive edge
    {0x7010, 0x03,  1}, // Set counter
    {0x7010, 0x04,  1}, // Enable latch extern on negative edge
    {0x0000, 0x00,  4}, // Gap
    {0x0000, 0x00,  8}, // Gap
    {0x7010, 0x11, 16}  // Set counter value
};

static ec_pdo_entry_info_t lcec_el7342_channel1_dcm_out[] = {
    {0x7020, 0x01,  1}, // Enable
    {0x7020, 0x02,  1}, // Reset
    {0x7020, 0x03,  1}, // Reduce torque
    {0x0000, 0x00,  5}, // Gap
    {0x0000, 0x00,  8}  // Gap
};

static ec_pdo_entry_info_t lcec_el7342_channel1_vel_out[] = {
    {0x7020, 0x21, 16}, // Velocity
};

static ec_pdo_entry_info_t lcec_el7342_channel2_dcm_out[] = {
    {0x7030, 0x01,  1}, // Enable
    {0x7030, 0x02,  1}, // Reset
    {0x7030, 0x03,  1}, // Reduce torque
    {0x0000, 0x00,  5}, // Gap
    {0x0000, 0x00,  8}  // Gap
};

static ec_pdo_entry_info_t lcec_el7342_channel2_vel_out[] = {
    {0x7030, 0x21, 16}  // Velocity
};

static ec_pdo_entry_info_t lcec_el7342_channel1_enc_in[] = {
    {0x0000, 0x00,  1}, // Gap
    {0x6000, 0x02,  1}, // Latch extern valid
    {0x6000, 0x03,  1}, // Set counter done
    {0x6000, 0x04,  1}, // Counter underflow
    {0x6000, 0x05,  1}, // Counter overflow
    {0x0000, 0x00,  2}, // Gap
    {0x6000, 0x08,  1}, // Extrapolation stall
    {0x6000, 0x09,  1}, // Status of input A
    {0x6000, 0x0a,  1}, // Status of input B
    {0x0000, 0x00,  1}, // Gap
    {0x0000, 0x00,  1}, // Gap
    {0x6000, 0x0d,  1}, // Status of extern latch
    {0x1c32, 0x20,  1}, // Sync error
    {0x0000, 0x00,  1}, // Gap
    {0x1800, 0x09,  1}, // TxPDO Toggle
    {0x6000, 0x11, 16}, // Counter value
    {0x6000, 0x12, 16}  // Latch value
};

static ec_pdo_entry_info_t lcec_el7342_channel2_enc_in[] = {
    {0x0000, 0x00,  1}, // Gap
    {0x6010, 0x02,  1}, // Latch extern valid
    {0x6010, 0x03,  1}, // Set counter done
    {0x6010, 0x04,  1}, // Counter underflow
    {0x6010, 0x05,  1}, // Counter overflow
    {0x0000, 0x00,  2}, // Gap
    {0x6010, 0x08,  1}, // Extrapolation stall
    {0x6010, 0x09,  1}, // Status of input A
    {0x6010, 0x0a,  1}, // Status of input B
    {0x0000, 0x00,  1}, // Gap
    {0x0000, 0x00,  1}, // Gap
    {0x6010, 0x0d,  1}, // Status of extern latch
    {0x1c32, 0x20,  1}, // Sync error
    {0x0000, 0x00,  1}, // Gap
    {0x1803, 0x09,  1}, // TxPDO Toggle
    {0x6010, 0x11, 16}, // Counter value
    {0x6010, 0x12, 16}  // Latch value
};

static ec_pdo_entry_info_t lcec_el7342_channel1_dcm_in[] = {
    {0x6020, 0x01,  1}, // Ready to enable
    {0x6020, 0x02,  1}, // Ready
    {0x6020, 0x03,  1}, // Warning
    {0x6020, 0x04,  1}, // Error
    {0x6020, 0x05,  1}, // Moving positive
    {0x6020, 0x06,  1}, // Moving negative
    {0x6020, 0x07,  1}, // Torque reduced
    {0x0000, 0x00,  1}, // Gap
    {0x0000, 0x00,  3}, // Gap
    {0x6020, 0x0c,  1}, // Digital input 1
    {0x6020, 0x0d,  1}, // Digital input 2
    {0x1c32, 0x20,  1}, // Sync error
    {0x0000, 0x00,  1}, // Gap
    {0x1806, 0x09,  1}  // TxPDO Toggle
};

static ec_pdo_entry_info_t lcec_el7342_channel1_dcm_sync_info[] = {
    {0x6020, 0x11,  16}, // Synchronous information 1
    {0x6020, 0x12,  16}  // Synchronous information 2
};

static ec_pdo_entry_info_t lcec_el7342_channel2_dcm_in[] = {
    {0x6030, 0x01,  1}, // Ready to enable
    {0x6030, 0x02,  1}, // Ready
    {0x6030, 0x03,  1}, // Warning
    {0x6030, 0x04,  1}, // Error
    {0x6030, 0x05,  1}, // Moving positive
    {0x6030, 0x06,  1}, // Moving negative
    {0x6030, 0x07,  1}, // Torque reduced
    {0x0000, 0x00,  1}, // Gap
    {0x0000, 0x00,  3}, // Gap
    {0x6030, 0x0c,  1}, // Digital input 1
    {0x6030, 0x0d,  1}, // Digital input 2
    {0x1c32, 0x20,  1}, // Sync error
    {0x0000, 0x00,  1}, // Gap
    {0x1808, 0x09,  1}  // TxPDO Toggle
};

static ec_pdo_entry_info_t lcec_el7342_channel2_dcm_sync_info[] = {
    {0x6030, 0x11,  16}, // Synchronous information 1
    {0x6030, 0x12,  16}  // Synchronous information 2
};

static ec_pdo_info_t lcec_el7342_pdos_out[] = {
    {0x1600,  7, lcec_el7342_channel1_enc_out},
    {0x1602,  7, lcec_el7342_channel2_enc_out},
    {0x1604,  5, lcec_el7342_channel1_dcm_out},
    {0x1606,  1, lcec_el7342_channel1_vel_out},
    {0x1607,  5, lcec_el7342_channel2_dcm_out},
    {0x1609,  1, lcec_el7342_channel2_vel_out}
};

static ec_pdo_info_t lcec_el7342_pdos_in[] = {
    {0x1a00, 17, lcec_el7342_channel1_enc_in},
    {0x1a03, 17, lcec_el7342_channel2_enc_in},
    {0x1a06, 14, lcec_el7342_channel1_dcm_in},
    {0x1a07,  2, lcec_el7342_channel1_dcm_sync_info},
    {0x1a08, 14, lcec_el7342_channel2_dcm_in},
    {0x1a09,  2, lcec_el7342_channel2_dcm_sync_info},
};

static ec_sync_info_t lcec_el7342_syncs[] = {
    {0, EC_DIR_OUTPUT, 0, NULL},
    {1, EC_DIR_INPUT,  0, NULL},
    {2, EC_DIR_OUTPUT, 6, lcec_el7342_pdos_out},
    {3, EC_DIR_INPUT,  6, lcec_el7342_pdos_in},
    {0xff}
};

void lcec_el7342_read(struct lcec_slave *slave, long period);
void lcec_el7342_write(struct lcec_slave *slave, long period);

/**
 * @brief Write a synchronous information selector value to the drive.
 *
 * Writes the requested info selector index to the raw PDO field so the
 * next cyclic read returns the chosen diagnostic value.
 *
 * @param chan     Pointer to the channel data.
 * @param raw_info Pointer to the raw info HAL pin to update.
 * @param sel_info Pointer to the selector HAL pin choosing which value to read.
 */
void lcec_el7342_set_info(lcec_el7342_chan_t *chan, hal_s32_t *raw_info, hal_u32_t *sel_info);

/**
 * @brief Initialise the EL7342 2-channel DC motor terminal.
 *
 * @param comp_id         HAL component ID.
 * @param slave           Pointer to the EtherCAT slave structure.
 * @param pdo_entry_regs  Pointer to the PDO entry registration array.
 * @return 0 on success, negative errno on failure.
 */
int lcec_el7342_init(int comp_id, struct lcec_slave *slave, ec_pdo_entry_reg_t **pdo_entry_regs) {
  lcec_master_t *master = slave->master;
  lcec_el7342_data_t *hal_data;
  int i;
  lcec_el7342_chan_t *chan;
  uint8_t info1_select, info2_select;
  int err;

  // initialize callbacks
  slave->proc_read = lcec_el7342_read;
  slave->proc_write = lcec_el7342_write;

  // alloc hal memory
  if ((hal_data = hal_malloc(sizeof(lcec_el7342_data_t))) == NULL) {
    rtapi_print_msg(RTAPI_MSG_ERR, LCEC_MSG_PFX "hal_malloc() for slave %s.%s failed\n", master->name, slave->name);
    return -EIO;
  }
  memset(hal_data, 0, sizeof(lcec_el7342_data_t));
  slave->hal_data = hal_data;

  // initialize sync info
  slave->sync_info = lcec_el7342_syncs;

  // initialize global data
  hal_data->last_operational = 0;

  // initialize pins
  for (i=0; i<LCEC_EL7342_CHANS; i++) {
    chan = &hal_data->chans[i];

    // read sdos
    // Info1 selector
    if (lcec_read_sdo(slave, 0x8022 + (i << 4), 0x11, &info1_select, 1)) {
      return -EIO;
    }
    // Info2 selector
    if (lcec_read_sdo(slave, 0x8022 + (i << 4), 0x19, &info2_select, 1)) {
      return -EIO;
    }

    // initialize POD entries
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x02, &chan->latch_ext_valid_pdo_os, &chan->latch_ext_valid_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x03, &chan->set_count_done_pdo_os, &chan->set_count_done_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x04, &chan->count_underflow_pdo_os, &chan->count_underflow_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x05, &chan->count_overflow_pdo_os, &chan->count_overflow_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x08, &chan->expol_stall_pdo_os, &chan->expol_stall_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x09, &chan->ina_pdo_os, &chan->ina_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x0a, &chan->inb_pdo_os, &chan->inb_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x0d, &chan->inext_pdo_os, &chan->inext_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x1c32           , 0x20, &chan->sync_err_pdo_os, &chan->sync_err_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x1800 + (i *  3), 0x09, &chan->tx_toggle_pdo_os, &chan->tx_toggle_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x11, &chan->count_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6000 + (i << 4), 0x12, &chan->latch_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (i << 4), 0x02, &chan->ena_latch_ext_pos_pdo_os, &chan->ena_latch_ext_pos_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (i << 4), 0x03, &chan->set_count_pdo_os, &chan->set_count_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (i << 4), 0x04, &chan->ena_latch_ext_neg_pdo_os, &chan->ena_latch_ext_neg_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7000 + (i << 4), 0x11, &chan->set_count_val_pdo_os, NULL);

    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6020 + (i << 4), 0x01, &chan->dcm_ready_to_enable_pdo_os, &chan->dcm_ready_to_enable_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6020 + (i << 4), 0x02, &chan->dcm_ready_pdo_os, &chan->dcm_ready_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6020 + (i << 4), 0x03, &chan->dcm_warning_pdo_os, &chan->dcm_warning_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6020 + (i << 4), 0x04, &chan->dcm_error_pdo_os, &chan->dcm_error_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6020 + (i << 4), 0x05, &chan->dcm_move_pos_pdo_os, &chan->dcm_move_pos_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6020 + (i << 4), 0x06, &chan->dcm_move_neg_pdo_os, &chan->dcm_move_neg_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6020 + (i << 4), 0x07, &chan->dcm_torque_reduced_pdo_os, &chan->dcm_torque_reduced_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6020 + (i << 4), 0x0c, &chan->dcm_din1_pdo_os, &chan->dcm_din1_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6020 + (i << 4), 0x0d, &chan->dcm_din2_pdo_os, &chan->dcm_din2_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x1c32           , 0x20, &chan->dcm_sync_err_pdo_os, &chan->dcm_sync_err_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x1806 + (i << 1), 0x09, &chan->dcm_tx_toggle_pdo_os, &chan->dcm_tx_toggle_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6020 + (i << 4), 0x11, &chan->dcm_info1_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x6020 + (i << 4), 0x12, &chan->dcm_info2_pdo_os, NULL);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7020 + (i << 4), 0x01, &chan->dcm_ena_pdo_os, &chan->dcm_ena_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7020 + (i << 4), 0x02, &chan->dcm_reset_pdo_os, &chan->dcm_reset_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7020 + (i << 4), 0x03, &chan->dcm_reduce_torque_pdo_os, &chan->dcm_reduce_torque_pdo_bp);
    LCEC_PDO_INIT(pdo_entry_regs, slave->index, slave->vid, slave->pid, 0x7020 + (i << 4), 0x21, &chan->dcm_velo_pdo_os, NULL);

    // export pins
    if ((err = lcec_pin_newf_list(comp_id, chan, slave_pins, master->instance_name, master->name, slave->name, i)) != 0) {
      return err;
    }
    if (info1_select == INFO_SEL_MOTOR_VELO || info2_select == INFO_SEL_MOTOR_VELO) {
      if ((err = lcec_pin_newf(comp_id, HAL_FLOAT, HAL_OUT, (void **) &(chan->dcm_velo_fb), "%s.%s.%s.srv-%d-velo-fb", master->instance_name, master->name, slave->name, i)) != 0) {
        return err;
      }
    }
    if (info1_select == INFO_SEL_MOTOR_CURR || info2_select == INFO_SEL_MOTOR_CURR) {
      if ((err = lcec_pin_newf(comp_id, HAL_FLOAT, HAL_OUT, (void **) &(chan->dcm_current_fb), "%s.%s.%s.srv-%d-current-fb", master->instance_name, master->name, slave->name, i)) != 0) {
        return err;
      }
    }

    // initialize pins
    *(chan->pos_scale) = 1.0;

    *(chan->dcm_scale) = 1.0;
    *(chan->dcm_min_dc) = -1.0;
    *(chan->dcm_max_dc) = 1.0;
    *(chan->dcm_sel_info1) = info1_select;
    *(chan->dcm_sel_info2) = info2_select;

    // initialize variables
    chan->enc_do_init = 1;
    chan->enc_last_count = 0;
    chan->enc_old_scale = *(chan->pos_scale) + 1.0;
    chan->enc_scale_recip = 1.0;

    chan->dcm_old_scale = *(chan->dcm_scale) + 1.0;
    chan->dcm_scale_recip = 1.0;
  }

  return 0;
}

/**
 * @brief Cyclic read: update encoder and drive status HAL pins for all channels.
 *
 * Iterates over both channels, reading encoder counts, latch values, status
 * bits, and synchronous info words. Computes accumulated position counts with
 * scale.
 *
 * @param slave  EtherCAT slave structure.
 * @param period Cycle period in nanoseconds (unused).
 */
void lcec_el7342_read(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el7342_data_t *hal_data = (lcec_el7342_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_el7342_chan_t *chan;
  int16_t raw_count, raw_latch, raw_delta;

  // wait for slave to be operational
  if (!slave->state.operational) {
    hal_data->last_operational = 0;
    return;
  }

  // check inputs
  for (i=0; i<LCEC_EL7342_CHANS; i++) {
    chan = &hal_data->chans[i];

    // check for change in scale value
    if (*(chan->pos_scale) != chan->enc_old_scale) {
      // scale value has changed, test and update it
      if ((*(chan->pos_scale) < 1e-20) && (*(chan->pos_scale) > -1e-20)) {
        // value too small, divide by zero is a bad thing
        *(chan->pos_scale) = 1.0;
      }
      // save new scale to detect future changes
      chan->enc_old_scale = *(chan->pos_scale);
      // we actually want the reciprocal
      chan->enc_scale_recip = 1.0 / *(chan->pos_scale);
    }

    // get bit states
    *(chan->ina) = EC_READ_BIT(&pd[chan->ina_pdo_os], chan->ina_pdo_bp);
    *(chan->inb) = EC_READ_BIT(&pd[chan->inb_pdo_os], chan->inb_pdo_bp);
    *(chan->inext) = EC_READ_BIT(&pd[chan->inext_pdo_os], chan->inext_pdo_bp);
    *(chan->sync_err) = EC_READ_BIT(&pd[chan->sync_err_pdo_os], chan->sync_err_pdo_bp);
    *(chan->expol_stall) = EC_READ_BIT(&pd[chan->expol_stall_pdo_os], chan->expol_stall_pdo_bp);
    *(chan->tx_toggle) = EC_READ_BIT(&pd[chan->tx_toggle_pdo_os], chan->tx_toggle_pdo_bp);
    *(chan->count_overflow) = EC_READ_BIT(&pd[chan->count_overflow_pdo_os], chan->count_overflow_pdo_bp);
    *(chan->count_underflow) = EC_READ_BIT(&pd[chan->count_underflow_pdo_os], chan->count_underflow_pdo_bp);
    *(chan->latch_ext_valid) = EC_READ_BIT(&pd[chan->latch_ext_valid_pdo_os], chan->latch_ext_valid_pdo_bp);

    *(chan->dcm_ready_to_enable) = EC_READ_BIT(&pd[chan->dcm_ready_to_enable_pdo_os], chan->dcm_ready_to_enable_pdo_bp);
    *(chan->dcm_ready) = EC_READ_BIT(&pd[chan->dcm_ready_pdo_os], chan->dcm_ready_pdo_bp);
    *(chan->dcm_warning) = EC_READ_BIT(&pd[chan->dcm_warning_pdo_os], chan->dcm_warning_pdo_bp);
    *(chan->dcm_error) = EC_READ_BIT(&pd[chan->dcm_error_pdo_os], chan->dcm_error_pdo_bp);
    *(chan->dcm_move_pos) = EC_READ_BIT(&pd[chan->dcm_move_pos_pdo_os], chan->dcm_move_pos_pdo_bp);
    *(chan->dcm_move_neg) = EC_READ_BIT(&pd[chan->dcm_move_neg_pdo_os], chan->dcm_move_neg_pdo_bp);
    *(chan->dcm_torque_reduced) = EC_READ_BIT(&pd[chan->dcm_torque_reduced_pdo_os], chan->dcm_torque_reduced_pdo_bp);
    *(chan->dcm_din1) = EC_READ_BIT(&pd[chan->dcm_din1_pdo_os], chan->dcm_din1_pdo_bp);
    *(chan->dcm_din2) = EC_READ_BIT(&pd[chan->dcm_din2_pdo_os], chan->dcm_din2_pdo_bp);
    *(chan->dcm_sync_err) = EC_READ_BIT(&pd[chan->dcm_sync_err_pdo_os], chan->dcm_sync_err_pdo_bp);
    *(chan->dcm_tx_toggle) = EC_READ_BIT(&pd[chan->dcm_tx_toggle_pdo_os], chan->dcm_tx_toggle_pdo_bp);

    // read raw values
    raw_count = EC_READ_S16(&pd[chan->count_pdo_os]);
    raw_latch = EC_READ_S16(&pd[chan->latch_pdo_os]);

    // read raw info values
    *(chan->dcm_raw_info1) = EC_READ_S16(&pd[chan->dcm_info1_pdo_os]);
    *(chan->dcm_raw_info2) = EC_READ_S16(&pd[chan->dcm_info2_pdo_os]);

    // dispatch info values
    lcec_el7342_set_info(chan, chan->dcm_raw_info1, chan->dcm_sel_info1);
    lcec_el7342_set_info(chan, chan->dcm_raw_info2, chan->dcm_sel_info2);

    // check for operational change of slave
    if (!hal_data->last_operational) {
      chan->enc_last_count = raw_count;
    }

    // check for counter set done
    if (EC_READ_BIT(&pd[chan->set_count_done_pdo_os], chan->set_count_done_pdo_bp)) {
      chan->enc_last_count = raw_count;
      *(chan->set_raw_count) = 0;
    }

    // update raw values
    if (! *(chan->set_raw_count)) {
      *(chan->raw_count) = raw_count;
    }

    // handle initialization
    if (chan->enc_do_init || *(chan->reset)) {
      chan->enc_do_init = 0;
      chan->enc_last_count = raw_count;
      *(chan->count) = 0;
    }

    // handle index
    if (*(chan->latch_ext_valid)) {
      *(chan->raw_latch) = raw_latch;
      chan->enc_last_count = raw_latch;
      *(chan->count) = 0;
      *(chan->ena_latch_ext_pos) = 0;
      *(chan->ena_latch_ext_neg) = 0;
    }

    // compute net counts
    raw_delta = raw_count - chan->enc_last_count;
    chan->enc_last_count = raw_count;
    *(chan->count) += raw_delta;

    // scale count to make floating point position
    *(chan->pos) = *(chan->count) * chan->enc_scale_recip;
  }

  hal_data->last_operational = 1;
}

/**
 * @brief Cyclic write: send velocity commands and control bits for all channels.
 *
 * Iterates over both channels, validates duty-cycle limits, applies scale and
 * offset, clamps the result, writes enable/reset/reduce-torque control bits
 * and the 16-bit velocity setpoint to the process data image, and triggers
 * synchronous info selector writes.
 *
 * @param slave  EtherCAT slave structure.
 * @param period Cycle period in nanoseconds (unused).
 */
void lcec_el7342_write(struct lcec_slave *slave, long period) {
  lcec_master_t *master = slave->master;
  lcec_el7342_data_t *hal_data = (lcec_el7342_data_t *) slave->hal_data;
  uint8_t *pd = master->process_data;
  int i;
  lcec_el7342_chan_t *chan;
  double tmpval, tmpdc, raw_val;

  // set outputs
  for (i=0; i<LCEC_EL7342_CHANS; i++) {
    chan = &hal_data->chans[i];

    // validate duty cycle limits, both limits must be between
    // 0.0 and 1.0 (inclusive) and max must be greater then min
    if (*(chan->dcm_max_dc) > 1.0) {
      *(chan->dcm_max_dc) = 1.0;
    }
    if (*(chan->dcm_min_dc) > *(chan->dcm_max_dc)) {
      *(chan->dcm_min_dc) = *(chan->dcm_max_dc);
    }
    if (*(chan->dcm_min_dc) < -1.0) {
      *(chan->dcm_min_dc) = -1.0;
    }
    if (*(chan->dcm_max_dc) < *(chan->dcm_min_dc)) {
      *(chan->dcm_max_dc) = *(chan->dcm_min_dc);
    }

    // do scale calcs only when scale changes
    if (*(chan->dcm_scale) != chan->dcm_old_scale) {
      // validate the new scale value
      if ((*(chan->dcm_scale) < 1e-20) && (*(chan->dcm_scale) > -1e-20)) {
        // value too small, divide by zero is a bad thing
        *(chan->dcm_scale) = 1.0;
      }
      // get ready to detect future scale changes
      chan->dcm_old_scale = *(chan->dcm_scale);
      // we will need the reciprocal
      chan->dcm_scale_recip = 1.0 / *(chan->dcm_scale);
    }

    // get command
    tmpval = *(chan->dcm_value);
    if (*(chan->dcm_absmode) && (tmpval < 0)) {
      tmpval = -tmpval;
    }

    // convert value command to duty cycle
    tmpdc = tmpval * chan->dcm_scale_recip + *(chan->dcm_offset);
    if (tmpdc < *(chan->dcm_min_dc)) {
      tmpdc = *(chan->dcm_min_dc);
    }
    if (tmpdc > *(chan->dcm_max_dc)) {
      tmpdc = *(chan->dcm_max_dc);
    }

    // set output values
    if (*(chan->dcm_enable) == 0) {
      raw_val = 0;
      *(chan->dcm_curr_dc) = 0;
    } else {
      raw_val = (double)0x7fff * tmpdc;
      if (raw_val > (double)0x7fff) {
        raw_val = (double)0x7fff;
      }
      if (raw_val < (double)-0x7fff) {
        raw_val = (double)-0x7fff;
      }
      *(chan->dcm_curr_dc) = tmpdc;
    }

    // update value
    *(chan->dcm_raw_val) = (int32_t)raw_val;

    // set output data
    EC_WRITE_BIT(&pd[chan->set_count_pdo_os], chan->set_count_pdo_bp, *(chan->set_raw_count));
    EC_WRITE_BIT(&pd[chan->ena_latch_ext_pos_pdo_os], chan->ena_latch_ext_pos_pdo_bp, *(chan->ena_latch_ext_pos));
    EC_WRITE_BIT(&pd[chan->ena_latch_ext_neg_pdo_os], chan->ena_latch_ext_neg_pdo_bp, *(chan->ena_latch_ext_neg));
    EC_WRITE_S16(&pd[chan->set_count_val_pdo_os], *(chan->set_raw_count_val));

    EC_WRITE_BIT(&pd[chan->dcm_ena_pdo_os], chan->dcm_ena_pdo_bp, *(chan->dcm_enable));
    EC_WRITE_BIT(&pd[chan->dcm_reset_pdo_os], chan->dcm_reset_pdo_bp, *(chan->dcm_reset));
    EC_WRITE_BIT(&pd[chan->dcm_reduce_torque_pdo_os], chan->dcm_reduce_torque_pdo_bp, *(chan->dcm_reduce_torque));
    EC_WRITE_S16(&pd[chan->dcm_velo_pdo_os], (int16_t)raw_val);
  }
}

/**
 * @brief Process a synchronous info word and update derived feedback pins.
 *
 * When the selector is INFO_SEL_MOTOR_VELO, converts the raw count to user
 * units/s.  When INFO_SEL_MOTOR_CURR, converts the raw count to amperes.
 *
 * @param chan     Pointer to the channel data.
 * @param raw_info Raw info word read from PDO.
 * @param sel_info Pointer to the selector HAL pin indicating which quantity is present.
 */
void lcec_el7342_set_info(lcec_el7342_chan_t *chan, hal_s32_t *raw_info, hal_u32_t *sel_info) {
  switch(*sel_info) {
    case INFO_SEL_MOTOR_VELO:
      *(chan->dcm_velo_fb) = (double) *raw_info * 0.0001 * chan->dcm_old_scale;
      break;

    case INFO_SEL_MOTOR_CURR:
      *(chan->dcm_current_fb) = (double) *raw_info * 0.001;
      break;
  }
}
