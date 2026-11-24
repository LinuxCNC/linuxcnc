//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//
//
// This is a driver for the Hostmot2 FanucSpindle module: the CNC side of the
// Fanuc alpha serial spindle link (JA7A/JA7B) to an alpha series spindle
// amplifier (SPM).
//
// The FPGA module does the line format (1.5 Mbit/s, 40-bit units with CRC-5,
// two 9-unit blocks per 2.000 ms frame, new word content every 4th frame).
// This driver runs the link start-up and the parameter download, and maps
// the 16 link words to HAL pins.
//
// Register map (one instance per link, instance stride 4, register stride 0x100):
//
//   +0..+7   TXD0..7  R/W  staging words: [15:0] = w(2j), [31:16] = w(2j+1)
//   +8..+15  RXD0..7  R    received block, same packing: a snapshot of the last good block,
//                          taken at each CTRL write, so all 16 words belong to one block
//   +16      TXHDRM   R/W  header unit on marker frames (every 4th frame), staged like TXD
//   +17      TXHDRN   R/W  header unit on the other frames, staged like TXD
//   +18      RXHDR    R    header unit of the last good received block
//   +19      CTRL     R/W  bit0 TX enable, bit1 RX enable, bit2 commit,
//                          bits 9:8 update ratio, bits 11:10 update phase,
//                          bit12 clear counters, bit13 invert RX, bit14 invert TX.
//                          A watchdog bite clears TX enable in the FPGA, so the amplifier
//                          stops the spindle when the host stops.
//   +20      TIMING   R/W  [11:0] frame length, [23:12] block 2 offset (bit times)
//   +21      STATUS   R    [7:0] good blocks, [15:8] bad units, [23:16] frames,
//                          bit24 new block, bit25 RX idle, bit26 commit pending,
//                          [30:27] commits taken (modulo 16)
//
// Words (link word numbers; see the Fanuc serial spindle section of hostmot2(9)):
//   CNC -> SPM: w6 speed command (Q14 of P4020), w7 parameter command,
//               w8 parameter value, w9 control signal 1 (G070/G071),
//               w10 control signal 2 (G072/G073), w11 data-select,
//               w12 mode word (bit 0 toggles), w13 mode word 2
//               w13 bit 8: a rising edge arms the SPM's one-rev synchronisation (index)
//   The speed command is Q14 of the SPM's parameter 4020 (maximum motor speed); set the
//   max-rpm parameter to that value.
//   SPM -> CNC: w4 position increment counted from the one-rev marker (after an arm),
//               w5 load meter, w6 speed meter, w7 parameter echo,
//               w9 status 1 (F045/F046), w10 status 2, w11 position increment,
//               w12 state word (high byte = alarm number, bit 3 initialised, bit 6 one-rev synced),
//               w15 data-select answer
//


#include <rtapi.h>
#include <rtapi_string.h>
#include <rtapi_math.h>
#include <hal.h>

#include "hostmot2.h"


#define FSPD_NUM_REGS 22

#define FSPD_REG_TXD   0
#define FSPD_REG_RXD   8
#define FSPD_REG_HDRM  16
#define FSPD_REG_HDRN  17
#define FSPD_REG_RXHDR 18
#define FSPD_REG_CTRL  19
#define FSPD_REG_TIMING 20
#define FSPD_REG_STATUS 21

#define FSPD_CTRL_TXEN   (1u << 0)
#define FSPD_CTRL_RXEN   (1u << 1)
#define FSPD_CTRL_COMMIT (1u << 2)
#define FSPD_CTRL_RATIO4 (2u << 8)
#define FSPD_CTRL_PHASE2 (2u << 10)
#define FSPD_CTRL_CLEAR  (1u << 12)

#define FSPD_STATUS_IDLE    (1u << 25)
#define FSPD_STATUS_PENDING (1u << 26)
#define FSPD_STATUS_COMMITS_SHIFT 27     // [30:27] commits taken, modulo 16
#define FSPD_COMMIT_RESEND 3            // periods without pending or a count before a resend

// ---- the module descriptor this driver expects
#define FSPD_MD_VERSION 0
#define FSPD_MD_INSTANCE_STRIDE 4
#define FSPD_MD_MULTIPLE_REGS 0x003FFFFF     // all 22 registers exist once per instance

// ---- fields of the STATUS register
#define FSPD_STATUS_GOOD_MASK 0xFFu          // [7:0] good blocks, modulo 256
#define FSPD_STATUS_BAD_SHIFT 8              // [15:8] receive errors, modulo 256
#define FSPD_STATUS_BAD_MASK 0xFFu
#define FSPD_STATUS_COMMITS_MASK 0xFu

// ---- header unit: one per block, its top byte says what kind of frame this is.
// Only the "update marker" is understood: the amplifier takes new word content on a marker frame.
// The other values are replayed as recorded from a 21-MB control.
#define FSPD_HDR(byte) (((rtapi_u32)(byte) << 24) | 0x00400000u)
#define FSPD_HDR_MARKER  FSPD_HDR(0x70)       // running: every 4th frame, new content
#define FSPD_HDR_REPEAT  FSPD_HDR(0x50)       // running: the three frames between markers
#define FSPD_HDR_PRE_A   FSPD_HDR(0x4F)       // the four header values of the first frames
#define FSPD_HDR_PRE_B   FSPD_HDR(0x46)
#define FSPD_HDR_PRE_C   FSPD_HDR(0xCF)
#define FSPD_HDR_PRE_D   FSPD_HDR(0xC6)
#define FSPD_HDR_LOOP_A  FSPD_HDR(0x48)       // loopback test and key, first part
#define FSPD_HDR_LOOP_B  FSPD_HDR(0x44)       // loopback test and key, second part
#define FSPD_HDR_START   FSPD_HDR(0x4B)       // first update of the link start

// ---- the 16 link words. Each direction carries 16 words of 16 bits per block.
// Fanuc's manuals do not number or name the words on the wire. Where the amplifier maintenance
// manual names the data a word carries (its check-board data list), that name is quoted; the
// other descriptions are this driver's own.
#define FSPD_WORD_BITS 16
#define FSPD_WORD_MASK 0xFFFFu

// words sent to the amplifier
enum {
    FSPD_TXW_0           = 0,    // a small constant in the recording; the amplifier does not read it
    FSPD_TXW_SPEED       = 6,    // "speed command data" in the amplifier manual
    FSPD_TXW_PARAM_CMD   = 7,    // parameter channel: command and index
    FSPD_TXW_PARAM_VALUE = 8,    // parameter channel: value
    FSPD_TXW_CONTROL1    = 9,    // "spindle control signal 1" in the manual (the control's G70/G71)
    FSPD_TXW_CONTROL2    = 10,   // "spindle control signal 2" (G72/G73)
    FSPD_TXW_SELECT      = 11,   // data-select
    FSPD_TXW_MODE        = 12,   // mode word, with the update toggle; the key during start-up
    FSPD_TXW_MODE2       = 13,   // update ratio and the one-rev synchronisation request
};

// words received from the amplifier
enum {
    FSPD_RXW_INDEX_INC   = 4,    // position increment counted from the one-rev signal
    FSPD_RXW_LOAD        = 5,    // "load data" in the manual
    FSPD_RXW_SPEED       = 6,    // "speed data" in the manual
    FSPD_RXW_PARAM_ECHO  = 7,    // parameter channel: echo and "download complete"
    FSPD_RXW_STATUS1     = 9,    // "spindle status signal 1" in the manual
    FSPD_RXW_STATUS2     = 10,   // "spindle status signal 2"
    FSPD_RXW_POS_INC     = 11,   // position increment
    FSPD_RXW_STATE       = 12,   // link state word
    FSPD_RXW_SELECT_ANS  = 15,   // answer to the data-select
};

// TX word 0 as recorded: 2 in most frames, 3 in the last of the first frames
#define FSPD_W0_NORMAL 2
#define FSPD_W0_PRE_LAST 3

// loopback test: the control fills the words with an alternating pattern and the amplifier
// sends it back. Words 0 and 1 are left out of the pattern by the control, and the amplifier
// does not return words 14 and 15, so the echo is checked on words 2 to 13.
#define FSPD_LOOP_EVEN 0x55AA                // pattern in even-numbered words
#define FSPD_LOOP_ODD  0xAA55                // pattern in odd-numbered words
#define FSPD_LOOP_FIRST_WORD 2
#define FSPD_LOOP_ECHO_END 14                // one past the last word the amplifier echoes

// the key that ends the loopback test, sent in the mode word
#define FSPD_KEY_WORD 0x6996

// mode word (FSPD_TXW_MODE)
#define FSPD_MODE_TOGGLE     0x0001          // flips at every content update
#define FSPD_MODE_ALWAYS     0x4000          // set in every frame once the link is started
#define FSPD_MODE_LINK_START 0x8000          // sent once: resets the amplifier's serial alarms

// mode word 2 (FSPD_TXW_MODE2)
#define FSPD_MODE2_RATIO4    0x0004          // new content every 4th frame
#define FSPD_MODE2_INDEX_ARM 0x0100          // rising edge: reference the position to the one-rev signal

// parameter channel
#define FSPD_PARAM_WRITE       0x4000        // TX command: write the value to this index
#define FSPD_PARAM_SELECT_MASK 0x01FF        // RX echo: the data-select the amplifier has taken
#define FSPD_PARAM_COMPLETE    0x8000        // RX echo: the download is complete

// data-select codes that read the amplifier's software identity
#define FSPD_SELECT_SERIES_HI 1              // two ASCII characters, e.g. "9D"
#define FSPD_SELECT_SERIES_LO 2              // two ASCII characters, e.g. "20"
#define FSPD_SELECT_EDITION   3              // two ASCII characters
#define FSPD_SELECT_MASK 0xFFu

// link state word (FSPD_RXW_STATE)
#define FSPD_STATE_TOGGLE 0x0001             // flips at every update the amplifier has taken
#define FSPD_STATE_INIT   0x0008             // the amplifier has started its link
#define FSPD_STATE_SYNCED 0x0040             // the position is referenced to the one-rev signal
#define FSPD_STATE_ALARM_SHIFT 8             // high byte: alarm number

// status signals 1 (FSPD_RXW_STATUS1)
#define FSPD_ALMA (1u << 0)                  // alarm
#define FSPD_SSTA (1u << 1)                  // zero speed
#define FSPD_SARA (1u << 3)                  // speed arrival

// speed command and speed meter: full scale is the amplifier's maximum speed
#define FSPD_SPEED_FULL_SCALE 16384.0
#define FSPD_DEFAULT_MAX_RPM 8000.0
#define FSPD_DEFAULT_COUNTS_PER_REV 4096.0
#define FSPD_DEFAULT_SELECT 4                // 376 parameter words
#define FSPD_DEFAULT_LAST_INDEX 375

// ---- start-up timing, in content updates (8 ms each), as recorded from the control
#define FSPD_PRE_A_END 2                     // updates 0..1: header PRE_A
#define FSPD_PRE_B_END 5                     // updates 2..4: header PRE_B
#define FSPD_PRE_C_END 6                     // update 5: header PRE_C
#define FSPD_PRE_STEPS 9                     // updates 6..8: header PRE_D
#define FSPD_LOOP_A_STEPS 2                  // loopback test: header LOOP_A
#define FSPD_LOOP_STEPS 3                    //   then one update with LOOP_B
#define FSPD_KEY_A_STEPS 3                   // key: header LOOP_A
#define FSPD_KEY_STEPS 6                     //   then three updates with LOOP_B
#define FSPD_START_STEPS 3                   // link start: three mode words
#define FSPD_SELECT_SETTLE_STEPS 2           // updates before an echo can belong to the new select
// download pacing: in every group of 4 updates the first 2 each write one parameter
#define FSPD_DL_GROUP 4
#define FSPD_DL_WRITES 2


// control signal 1 (w9) bits
#define FSPD_TLMLA (1u << 0)
#define FSPD_TLMHA (1u << 1)
#define FSPD_SRVA  (1u << 4)
#define FSPD_SFRA  (1u << 5)
#define FSPD_ORCMA (1u << 6)
#define FSPD_MRDYA (1u << 7)
#define FSPD_ARSTA (1u << 8)
#define FSPD_ESPA  (1u << 9)

// update steps (one step = one 8 ms content update)
#define FSPD_INIT_STEPS 12
#define FSPD_SELECT_STEPS 20
#define FSPD_DONE_STEPS 125
#define FSPD_UNINIT_STEPS 4

#define FSPD_LINK_TIMEOUT_NS ((rtapi_s64)50000000)   // 50 ms without a good block: the link is lost


static inline void fspd_set_word(hm2_fspd_instance_t *inst, int w, rtapi_u16 v) {
    inst->tx[w] = v;
}

static inline void fspd_clear_words(hm2_fspd_instance_t *inst) {
    memset(inst->tx, 0, sizeof(inst->tx));
}

static inline rtapi_u16 fspd_param_word(const hm2_fspd_instance_t *inst, int n) {
    return (rtapi_u16)hal_get_uint(inst->hal.param.value[n]);
}


int hm2_fspd_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r;

    if (!hm2_md_is_consistent_or_complain(hm2, md_index, FSPD_MD_VERSION, FSPD_NUM_REGS, FSPD_MD_INSTANCE_STRIDE, FSPD_MD_MULTIPLE_REGS)) {
        HM2_ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }

    if (hm2->fspd.num_instances != 0) {
        HM2_ERR(
            "found duplicate Module Descriptor for %s (inconsistent firmware), not loading driver\n",
            hm2_get_general_function_name(md->gtag)
        );
        return -EINVAL;
    }

    if (hm2->config.num_fspds > md->instances) {
        HM2_ERR(
            "config.num_fspds=%d, but only %d are available, not loading driver\n",
            hm2->config.num_fspds,
            md->instances
        );
        return -EINVAL;
    }

    if (hm2->config.num_fspds == 0) {
        return 0;
    }

    if (hm2->config.num_fspds == -1) {
        hm2->fspd.num_instances = md->instances;
    } else {
        hm2->fspd.num_instances = hm2->config.num_fspds;
    }

    hm2->fspd.clock_freq = md->clock_freq;
    hm2->fspd.version = md->version;
    hm2->fspd.base_addr = md->base_address;
    hm2->fspd.register_stride = md->register_stride;
    hm2->fspd.instance_stride = md->instance_stride;

    hm2->fspd.instance = hal_malloc(hm2->fspd.num_instances * sizeof(*hm2->fspd.instance));
    if (hm2->fspd.instance == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }
    memset(hm2->fspd.instance, 0, hm2->fspd.num_instances * sizeof(*hm2->fspd.instance));

    // TRAM: read the received words and the status;
    // write the staging words, both headers and the control register.
    for (int i = 0; i < HM2_FSPD_NUM_WORD_REGS; i ++) {
        r = hm2_register_tram_read_region(hm2, md->base_address + (FSPD_REG_RXD + i) * md->register_stride,
                                          hm2->fspd.num_instances * sizeof(rtapi_u32), &hm2->fspd.rxd_reg[i]);
        if (r < 0) goto fail_tram;
        r = hm2_register_tram_write_region(hm2, md->base_address + (FSPD_REG_TXD + i) * md->register_stride,
                                           hm2->fspd.num_instances * sizeof(rtapi_u32), &hm2->fspd.txd_reg[i]);
        if (r < 0) goto fail_tram;
    }
    r = hm2_register_tram_read_region(hm2, md->base_address + FSPD_REG_STATUS * md->register_stride,
                                      hm2->fspd.num_instances * sizeof(rtapi_u32), &hm2->fspd.status_reg);
    if (r < 0) goto fail_tram;
    r = hm2_register_tram_write_region(hm2, md->base_address + FSPD_REG_HDRM * md->register_stride,
                                       hm2->fspd.num_instances * sizeof(rtapi_u32), &hm2->fspd.hdrm_reg);
    if (r < 0) goto fail_tram;
    r = hm2_register_tram_write_region(hm2, md->base_address + FSPD_REG_HDRN * md->register_stride,
                                       hm2->fspd.num_instances * sizeof(rtapi_u32), &hm2->fspd.hdrn_reg);
    if (r < 0) goto fail_tram;
    r = hm2_register_tram_write_region(hm2, md->base_address + FSPD_REG_CTRL * md->register_stride,
                                       hm2->fspd.num_instances * sizeof(rtapi_u32), &hm2->fspd.ctrl_reg);
    if (r < 0) goto fail_tram;

    //
    // Export to HAL.
    //

// Creates one pin or parameter; on failure reports it and leaves through fail0.
#define CHK(x) do { \
        int _rv = (x); \
        if (_rv < 0) { \
            r = _rv; \
            HM2_ERR("%s: failed to create pin or parameter (error=%d)\n", name, _rv); \
            goto fail0; \
        } \
    } while (0)

    for (int i = 0; i < hm2->fspd.num_instances; i ++) {
        hm2_fspd_instance_t *inst = &hm2->fspd.instance[i];
        char name[HAL_NAME_LEN + 1];

        rtapi_snprintf(name, sizeof(name), "%s.fspd.%02d", hm2->llio->name, i);

        CHK(hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.enable, 0, "%s.enable", name));
        CHK(hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.spindle_on, 0, "%s.spindle-on", name));
        CHK(hal_pin_new_real(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.speed_cmd, 0.0, "%s.speed-cmd-rpm", name));
        CHK(hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.estop_ok, 0, "%s.estop-ok", name));
        CHK(hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.machine_ready, 0, "%s.machine-ready", name));
        CHK(hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.alarm_reset, 0, "%s.alarm-reset", name));
        CHK(hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.orient, 0, "%s.orient", name));
        CHK(hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.torque_limit_low, 0, "%s.torque-limit-low", name));
        CHK(hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.torque_limit_high, 0, "%s.torque-limit-high", name));
        CHK(hal_pin_new_uint(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.control1_extra, 0, "%s.control1-extra", name));
        CHK(hal_pin_new_uint(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.control2, 0, "%s.control2", name));

        CHK(hal_pin_new_sint(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.state, FSPD_OFF, "%s.state", name));
        CHK(hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.link_ok, 0, "%s.link-ok", name));
        CHK(hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.fault, 0, "%s.fault", name));
        CHK(hal_pin_new_sint(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.fault_code, 0, "%s.fault-code", name));
        CHK(hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.alarm, 0, "%s.alarm", name));
        CHK(hal_pin_new_uint(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.alarm_code, 0, "%s.alarm-code", name));
        CHK(hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.speed_fb, 0.0, "%s.speed-fb-rpm", name));
        CHK(hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.at_speed, 0, "%s.at-speed", name));
        CHK(hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.zero_speed, 0, "%s.zero-speed", name));
        CHK(hal_pin_new_uint(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.load, 0, "%s.load-raw", name));
        CHK(hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.position, 0.0, "%s.position", name));
        CHK(hal_pin_new_sint(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.position_raw, 0, "%s.position-raw", name));
        CHK(hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.position_interpolated, 0.0, "%s.position-interpolated", name));
        CHK(hal_pin_new_bool(hm2->llio->comp_id, HAL_IO, &inst->hal.pin.index_enable, 0, "%s.index-enable", name));
        CHK(hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.index_synced, 0, "%s.index-synced", name));
        CHK(hal_pin_new_uint(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.status1, 0, "%s.status1", name));
        CHK(hal_pin_new_uint(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.status2, 0, "%s.status2", name));
        CHK(hal_pin_new_uint(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.state_word, 0, "%s.state-word", name));
        CHK(hal_pin_new_uint(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.rom_id, 0, "%s.rom-id", name));
        CHK(hal_pin_new_uint(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.rom_edition, 0, "%s.rom-edition", name));
        CHK(hal_pin_new_uint(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.good_blocks, 0, "%s.good-blocks", name));
        CHK(hal_pin_new_uint(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.bad_units, 0, "%s.bad-units", name));
        CHK(hal_pin_new_uint(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.resends, 0, "%s.resends", name));
        CHK(hal_pin_new_uint(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.download_index, 0, "%s.download-index", name));
        for (int n = 0; n < HM2_FSPD_NUM_WORDS; n ++) {
            CHK(hal_pin_new_uint(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.rx_word[n], 0, "%s.rx-w%02d", name, n));
        }

        CHK(hal_param_new_real(hm2->llio->comp_id, HAL_RW, &inst->hal.param.max_rpm, FSPD_DEFAULT_MAX_RPM, "%s.max-rpm", name));
        CHK(hal_param_new_real(hm2->llio->comp_id, HAL_RW, &inst->hal.param.counts_per_rev, FSPD_DEFAULT_COUNTS_PER_REV, "%s.counts-per-rev", name));
        CHK(hal_param_new_uint(hm2->llio->comp_id, HAL_RW, &inst->hal.param.data_select, FSPD_DEFAULT_SELECT, "%s.data-select", name));
        CHK(hal_param_new_uint(hm2->llio->comp_id, HAL_RW, &inst->hal.param.last_index, FSPD_DEFAULT_LAST_INDEX, "%s.last-index", name));
        for (int n = 0; n < HM2_FSPD_MAX_PARAMS; n ++) {
            CHK(hal_param_new_uint(hm2->llio->comp_id, HAL_RW, &inst->hal.param.value[n], 0, "%s.param-%03d", name, n));
        }

        inst->state = FSPD_OFF;
    }
#undef CHK

    return hm2->fspd.num_instances;

fail_tram:
    HM2_ERR("error registering tram regions for fspd (%d)\n", r);
fail0:
    hm2->fspd.num_instances = 0;
    return r;
}


void hm2_fspd_cleanup(hostmot2_t *hm2) {
    if (hm2->fspd.num_instances <= 0) return;
    // stop transmitting: the SPM then stops the spindle with its own serial link alarm.
    // hm2_unregister() has already forced a watchdog bite, which stops the module too; this covers
    // the case of firmware built without a watchdog
    for (int i = 0; i < hm2->fspd.num_instances; i ++) {
        const rtapi_u32 zero = 0;
        hm2->llio->write(hm2->llio,
                         hm2->fspd.base_addr + FSPD_REG_CTRL * hm2->fspd.register_stride + i * hm2->fspd.instance_stride,
                         &zero, sizeof(zero));
    }
}


static inline void fspd_enter(hm2_fspd_instance_t *inst, hm2_fspd_state_t state) {
    inst->state = state;
    inst->step = 0;
    if (state == FSPD_OFF) inst->commit_wait = false;
}

static inline void fspd_fault(hm2_fspd_instance_t *inst, hm2_fspd_fault_t code) {
    inst->fault_code = code;
    fspd_enter(inst, FSPD_FAULT);
}

static inline rtapi_u16 fspd_rx(const hostmot2_t *hm2, int i, int w) {
    // each register holds two words: the even-numbered one in the low half
    const rtapi_u32 v = hm2->fspd.rxd_reg[w / 2][i];
    return (w & 1) ? (rtapi_u16)(v >> FSPD_WORD_BITS) : (rtapi_u16)(v & FSPD_WORD_MASK);
}


// Called once per content update while the link runs: prepares the words
// the FPGA will send for the next 4 frames.
static void fspd_step(hostmot2_t *hm2, int i) {
    hm2_fspd_instance_t *inst = &hm2->fspd.instance[i];
    rtapi_u32 hdr = FSPD_HDR_REPEAT;
    rtapi_u32 hdrm = FSPD_HDR_MARKER;
    bool running = false;

    switch (inst->state) {

    case FSPD_PRE:
        // what a Fanuc 21-MB control sends before the loopback test (headers and w0 as recorded)
        fspd_clear_words(inst);
        if (inst->step < FSPD_PRE_A_END) {
            hdr = hdrm = FSPD_HDR_PRE_A;
        } else if (inst->step < FSPD_PRE_B_END) {
            hdr = hdrm = FSPD_HDR_PRE_B;
            fspd_set_word(inst, FSPD_TXW_0, FSPD_W0_NORMAL);
        } else if (inst->step < FSPD_PRE_C_END) {
            hdr = hdrm = FSPD_HDR_PRE_C;
        } else {
            hdr = hdrm = FSPD_HDR_PRE_D;
            fspd_set_word(inst, FSPD_TXW_0, FSPD_W0_PRE_LAST);
        }
        if (++inst->step >= FSPD_PRE_STEPS) fspd_enter(inst, FSPD_LOOPBACK);
        break;

    case FSPD_LOOPBACK:
        // Recorded CNC timing, kept exactly: header LOOP_A for two updates, then LOOP_B for one.
        // An SPM was seen to drop the start-up if the master lingers here, so the
        // sequence does not wait for the echo. The echo is watched in the background instead (the
        // SPM echoes only part of the words during LOOP_A and all of w2..w13 after LOOP_B).
        hdr = hdrm = (inst->step < FSPD_LOOP_A_STEPS) ? FSPD_HDR_LOOP_A : FSPD_HDR_LOOP_B;
        fspd_clear_words(inst);
        fspd_set_word(inst, FSPD_TXW_0, FSPD_W0_NORMAL);
        for (int w = FSPD_LOOP_FIRST_WORD; w < HM2_FSPD_NUM_WORDS; w ++) {
            fspd_set_word(inst, w, (w & 1) ? FSPD_LOOP_ODD : FSPD_LOOP_EVEN);
        }
        if (++inst->step >= FSPD_LOOP_STEPS) fspd_enter(inst, FSPD_KEY);
        break;

    case FSPD_KEY:
        // recorded: header LOOP_A for three updates, then LOOP_B for three
        hdr = hdrm = (inst->step < FSPD_KEY_A_STEPS) ? FSPD_HDR_LOOP_A : FSPD_HDR_LOOP_B;
        fspd_clear_words(inst);
        fspd_set_word(inst, FSPD_TXW_0, FSPD_W0_NORMAL);
        fspd_set_word(inst, FSPD_TXW_MODE, FSPD_KEY_WORD);
        if (++inst->step >= FSPD_KEY_STEPS) {
            if (inst->echo_seen) {
                fspd_enter(inst, FSPD_START);
            } else {
                fspd_fault(inst, FSPD_FAULT_LOOPBACK);
            }
        }
        break;

    case FSPD_START:
        // three mode words as recorded: plain, with the toggle, then the link start bit once
        fspd_clear_words(inst);
        fspd_set_word(inst, FSPD_TXW_0, FSPD_W0_NORMAL);
        if (inst->step == 0) {
            hdr = hdrm = FSPD_HDR_START;
            inst->mode = FSPD_MODE_ALWAYS;
        } else if (inst->step == 1) {
            inst->mode = FSPD_MODE_ALWAYS | FSPD_MODE_TOGGLE;
        } else {
            inst->mode = FSPD_MODE_ALWAYS | FSPD_MODE_LINK_START;
        }
        fspd_set_word(inst, FSPD_TXW_MODE, inst->mode);
        if (++inst->step >= FSPD_START_STEPS) {
            inst->mode = FSPD_MODE_ALWAYS;
            fspd_enter(inst, FSPD_WAIT_INIT);
        }
        break;

    default:
        running = true;
        break;
    }

    // The states above each fill one whole update; a state entered in this call starts with the next one.
    if (running && inst->state >= FSPD_WAIT_INIT && inst->state <= FSPD_RUN) {
        const rtapi_u16 rxstate = fspd_rx(hm2, i, FSPD_RXW_STATE);

        inst->mode ^= FSPD_MODE_TOGGLE;
        fspd_set_word(inst, FSPD_TXW_MODE, inst->mode & ~FSPD_MODE_LINK_START);
        fspd_set_word(inst, FSPD_TXW_0, FSPD_W0_NORMAL);

        switch (inst->state) {

        case FSPD_WAIT_INIT:
            if (rxstate & FSPD_STATE_INIT) {
                fspd_enter(inst, FSPD_SELECT);
            } else if (++inst->step >= FSPD_INIT_STEPS) {
                fspd_fault(inst, FSPD_FAULT_NO_INIT);
            }
            break;

        case FSPD_SELECT: {
            // Data-select 1, 2, 3 read the ROM ID ("9D", "20", "11"), then the configured select
            // (4 on the 21-MB: parameter range 0x177). Each select is held until the SPM echoes it in its
            // w7 low bits. The SPM fixes its parameter range at the first parameter command, so the
            // final select must be acknowledged before the download starts.
            const bool final = (inst->sel > FSPD_SELECT_EDITION);
            const rtapi_u32 sel = final ? (hal_get_uint(inst->hal.param.data_select) & FSPD_SELECT_MASK)
                                        : (inst->sel ? inst->sel : FSPD_SELECT_SERIES_HI);
            fspd_set_word(inst, FSPD_TXW_SELECT, sel);
            if (final) fspd_set_word(inst, FSPD_TXW_MODE2, FSPD_MODE2_RATIO4);
            if (inst->step >= FSPD_SELECT_SETTLE_STEPS && (fspd_rx(hm2, i, FSPD_RXW_PARAM_ECHO) & FSPD_PARAM_SELECT_MASK) == sel) {
                const rtapi_u16 ans = fspd_rx(hm2, i, FSPD_RXW_SELECT_ANS);
                if (!final) {
                    // selects 1 and 2 return the software series, two ASCII characters each;
                    // select 3 returns the edition. They are read once per link start.
                    if (sel == FSPD_SELECT_SERIES_HI) hal_set_uint(inst->hal.pin.rom_id, (rtapi_u32)ans << FSPD_WORD_BITS);
                    if (sel == FSPD_SELECT_SERIES_LO) hal_set_uint(inst->hal.pin.rom_id, hal_get_uint(inst->hal.pin.rom_id) | ans);
                    if (sel == FSPD_SELECT_EDITION) hal_set_uint(inst->hal.pin.rom_edition, ans);
                    inst->sel = sel + 1;
                    inst->step = 0;
                } else {
                    inst->index = 0;
                    fspd_enter(inst, FSPD_DOWNLOAD);
                }
                break;
            }
            if (++inst->step >= FSPD_SELECT_STEPS) fspd_fault(inst, FSPD_FAULT_SELECT);
            break;
        }

        case FSPD_DOWNLOAD: {
            rtapi_u32 last = hal_get_uint(inst->hal.param.last_index);
            if (last >= HM2_FSPD_MAX_PARAMS) last = HM2_FSPD_MAX_PARAMS - 1;
            fspd_set_word(inst, FSPD_TXW_SELECT, hal_get_uint(inst->hal.param.data_select) & FSPD_SELECT_MASK);
            fspd_set_word(inst, FSPD_TXW_MODE2, FSPD_MODE2_RATIO4);
            // Strictly in sequence (a skipped index needs an SPM power cycle), in the CNC's own
            // pacing: two updates each write a parameter, then two updates repeat the last index
            // without the write bit.
            if ((inst->step % FSPD_DL_GROUP) < FSPD_DL_WRITES) {
                fspd_set_word(inst, FSPD_TXW_PARAM_CMD, FSPD_PARAM_WRITE | inst->index);
                fspd_set_word(inst, FSPD_TXW_PARAM_VALUE, fspd_param_word(inst, inst->index));
                inst->dl_last = inst->index;
                if (inst->index >= last) {
                    inst->index = last;
                    fspd_enter(inst, FSPD_WAIT_DONE);
                    break;
                }
                inst->index ++;
            } else {
                fspd_set_word(inst, FSPD_TXW_PARAM_CMD, inst->dl_last);
            }
            inst->step ++;
            break;
        }

        case FSPD_WAIT_DONE:
            fspd_set_word(inst, FSPD_TXW_PARAM_CMD, inst->index);    // last index, no write bit: idle
            fspd_set_word(inst, FSPD_TXW_PARAM_VALUE, 0);
            if (fspd_rx(hm2, i, FSPD_RXW_PARAM_ECHO) & FSPD_PARAM_COMPLETE) {
                fspd_enter(inst, FSPD_RUN);
            } else if (++inst->step >= FSPD_DONE_STEPS) {
                fspd_fault(inst, FSPD_FAULT_DOWNLOAD);
            }
            break;

        case FSPD_RUN: {
            // the SPM restarted its link (power cycle, sequence error): it loops our words back
            // and its "initialised" bit is gone
            if (!(rxstate & FSPD_STATE_INIT)) {
                if (++inst->step >= FSPD_UNINIT_STEPS) {
                    fspd_fault(inst, FSPD_FAULT_SPM_RESET);
                    break;
                }
            } else {
                inst->step = 0;
            }

            // Scale and limit the speed in floating point: an out-of-range conversion to an
            // integer is undefined.
            const double max = fmax(hal_get_real(inst->hal.param.max_rpm), 1.0);
            const double cmd = hal_get_real(inst->hal.pin.speed_cmd);
            const double rpm = isnan(cmd) ? 0.0 : cmd;
            rtapi_u32 q = (rtapi_u32)(fmin(fabs(rpm) * FSPD_SPEED_FULL_SCALE / max, FSPD_SPEED_FULL_SCALE) + 0.5);
            rtapi_u32 c1 = hal_get_uint(inst->hal.pin.control1_extra) & FSPD_WORD_MASK;

            if (hal_get_bool(inst->hal.pin.estop_ok)) c1 |= FSPD_ESPA;
            if (hal_get_bool(inst->hal.pin.machine_ready)) c1 |= FSPD_MRDYA;
            if (hal_get_bool(inst->hal.pin.alarm_reset)) c1 |= FSPD_ARSTA;
            if (hal_get_bool(inst->hal.pin.orient)) c1 |= FSPD_ORCMA;
            if (hal_get_bool(inst->hal.pin.torque_limit_low)) c1 |= FSPD_TLMLA;
            if (hal_get_bool(inst->hal.pin.torque_limit_high)) c1 |= FSPD_TLMHA;
            // like the CNC: w6 is the magnitude, the direction rides SFRA/SRVA,
            // and a zero speed clears both run bits
            if (hal_get_bool(inst->hal.pin.spindle_on) && q != 0) {
                c1 |= (rpm < 0) ? FSPD_SRVA : FSPD_SFRA;
            } else {
                q = 0;
            }

            // The words sent to the amplifier while running. The mode word and word 0 were set
            // above. The remaining words stay 0, which is what the control sends in speed mode:
            //   1          not used by the control
            //   2, 3       "move command" in the amplifier manual, read by the amplifier only in its own
            //              position modes (rigid tapping, Cs contour), which this driver does not use
            //   4, 5       read by the amplifier for orientation to an externally set stop
            //              position (5 is that position); 0 here
            //   14, 15     position command of the spindle synchronisation modes, not used here
            fspd_set_word(inst, FSPD_TXW_SPEED, (rtapi_u16)q);       // magnitude only; the direction is in control 1
            fspd_set_word(inst, FSPD_TXW_PARAM_CMD, inst->index);    // parameter channel idle: last index, no command
            fspd_set_word(inst, FSPD_TXW_PARAM_VALUE, 0);
            fspd_set_word(inst, FSPD_TXW_CONTROL1, (rtapi_u16)c1);
            fspd_set_word(inst, FSPD_TXW_CONTROL2, (rtapi_u16)(hal_get_uint(inst->hal.pin.control2) & FSPD_WORD_MASK));
            // the data-select stays at the value used for the download
            fspd_set_word(inst, FSPD_TXW_SELECT, hal_get_uint(inst->hal.param.data_select) & FSPD_SELECT_MASK);
            // a rising edge of the arm bit starts the one-rev synchronisation (see process_tram_read)
            const bool arm = (inst->idx_state == FSPD_IDX_ARM || inst->idx_state == FSPD_IDX_WAIT);
            fspd_set_word(inst, FSPD_TXW_MODE2, FSPD_MODE2_RATIO4 | (arm ? FSPD_MODE2_INDEX_ARM : 0));
            break;
        }

        default:
            break;
        }
    }

    inst->hdr = hdr;
    inst->hdrm = hdrm;
}


void hm2_fspd_process_tram_read(hostmot2_t *hm2, long period) {

    for (int i = 0; i < hm2->fspd.num_instances; i ++) {
        hm2_fspd_instance_t *inst = &hm2->fspd.instance[i];
        const rtapi_u32 status = hm2->fspd.status_reg[i];
        const rtapi_u32 good = status & FSPD_STATUS_GOOD_MASK;

        // received words
        for (int w = 0; w < HM2_FSPD_NUM_WORDS; w ++) {
            hal_set_uint(inst->hal.pin.rx_word[w], fspd_rx(hm2, i, w));
        }
        hal_set_uint(inst->hal.pin.good_blocks, good);
        hal_set_uint(inst->hal.pin.bad_units, (status >> FSPD_STATUS_BAD_SHIFT) & FSPD_STATUS_BAD_MASK);

        // link supervision: count time without a new good block
        if (good != inst->last_good) {
            inst->last_good = good;
            inst->silent_ns = 0;
        } else if (inst->silent_ns < FSPD_LINK_TIMEOUT_NS * 4) {
            inst->silent_ns += period;
        }

        // start and stop
        if (!hal_get_bool(inst->hal.pin.enable)) {
            if (inst->state != FSPD_OFF) {
                fspd_enter(inst, FSPD_OFF);
                inst->fault_code = FSPD_FAULT_NONE;
            }
        } else if (inst->state == FSPD_OFF) {
            fspd_enter(inst, FSPD_PRE);
            inst->fault_code = FSPD_FAULT_NONE;
            inst->sel = 0;
            inst->echo_seen = false;
            inst->idx_state = FSPD_IDX_FREE;
            inst->update_ns = 0;
            inst->since_flip_ns = 0;
            hal_set_uint(inst->hal.pin.rom_id, 0);
            hal_set_uint(inst->hal.pin.rom_edition, 0);
            inst->index = 0;
            inst->position = 0;
            inst->silent_ns = 0;
            fspd_clear_words(inst);
            inst->clear = true;
        }
        if (inst->state == FSPD_RUN && inst->silent_ns > FSPD_LINK_TIMEOUT_NS) {
            fspd_fault(inst, FSPD_FAULT_LINK);
        }
        // a rising edge of alarm-reset restarts a faulted link
        const bool rst = hal_get_bool(inst->hal.pin.alarm_reset);
        if (inst->state == FSPD_FAULT && rst && !inst->last_reset) {
            fspd_enter(inst, FSPD_OFF);     // restarts in the next period while enable is set
        }
        inst->last_reset = rst;

        // loopback echo: the SPM copies our pattern back until it has taken the key
        if (inst->state == FSPD_LOOPBACK || inst->state == FSPD_KEY) {
            bool ok = true;
            for (int w = FSPD_LOOP_FIRST_WORD; w < FSPD_LOOP_ECHO_END; w ++) {
                if (fspd_rx(hm2, i, w) != ((w & 1) ? FSPD_LOOP_ODD : FSPD_LOOP_EVEN)) ok = false;
            }
            if (ok) inst->echo_seen = true;
        }

        // One state machine step per content update. The FPGA counts the commits it has taken, so a
        // commit that never arrived (a lost write) is sent again with the same words instead of
        // moving on: the SPM does not survive a skipped download index.
        if (inst->state != FSPD_OFF && inst->state != FSPD_FAULT && !(status & FSPD_STATUS_PENDING)) {
            const rtapi_u32 cnt = (status >> FSPD_STATUS_COMMITS_SHIFT) & FSPD_STATUS_COMMITS_MASK;
            if (inst->commit_wait && cnt == inst->commit_cnt) {
                if (++inst->commit_age >= FSPD_COMMIT_RESEND) {
                    inst->commit = true;
                    inst->commit_age = 0;
                    hal_set_uint(inst->hal.pin.resends, hal_get_uint(inst->hal.pin.resends) + 1);
                }
            } else {
                fspd_step(hm2, i);
                inst->commit = true;
                inst->commit_wait = true;
                inst->commit_cnt = cnt;
                inst->commit_age = 0;
            }
        }

        // feedback from the SPM, once per SPM update (its state word toggle)
        const rtapi_u16 rxstate = fspd_rx(hm2, i, FSPD_RXW_STATE);
        const rtapi_u16 rxtoggle = rxstate & FSPD_STATE_TOGGLE;
        if (inst->state == FSPD_RUN) {
            const bool flip = (rxtoggle != inst->rx_toggle);
            const bool synced = (rxstate & FSPD_STATE_SYNCED) != 0;

            // Index: LinuxCNC sets index-enable; the SPM re-references its second increment channel
            // (w4) to the position coder's one-rev marker after a rising edge of our w13 bit 8, and
            // reports it in w12 bit 6. From then on the sum of w4 is the angle from the marker.
            if (hal_get_bool(inst->hal.pin.index_enable)) {
                if (inst->idx_state == FSPD_IDX_FREE || inst->idx_state == FSPD_IDX_SYNCED) {
                    inst->idx_state = FSPD_IDX_ARM;
                }
            } else if (inst->idx_state == FSPD_IDX_ARM || inst->idx_state == FSPD_IDX_WAIT) {
                inst->idx_state = FSPD_IDX_FREE;        // request withdrawn
            }
            if (inst->idx_state == FSPD_IDX_ARM && !synced) {
                inst->idx_state = FSPD_IDX_WAIT;
                inst->position = 0;
                inst->last_inc = 0;
            }

            // The SPM flips its toggle as soon as it gets a new update and builds the other words
            // a few passes later, so the increment read at a flip is that of the update before:
            // each increment is still counted exactly once.
            if (flip) {
                const bool from_marker = (inst->idx_state == FSPD_IDX_WAIT || inst->idx_state == FSPD_IDX_SYNCED);
                const int inc = (rtapi_s16)fspd_rx(hm2, i, from_marker ? FSPD_RXW_INDEX_INC : FSPD_RXW_POS_INC);
                inst->position += inc;
                inst->last_inc = inc;
                if (inst->since_flip_ns > 0) inst->update_ns = inst->since_flip_ns;
                inst->since_flip_ns = 0;
            }
            inst->since_flip_ns += period;

            if (inst->idx_state == FSPD_IDX_WAIT && synced) {
                inst->idx_state = FSPD_IDX_SYNCED;
                hal_set_bool(inst->hal.pin.index_enable, 0);
            }
        } else {
            inst->idx_state = FSPD_IDX_FREE;
            inst->last_inc = 0;
        }
        if (inst->state >= FSPD_WAIT_INIT && inst->state <= FSPD_RUN) {
            // the SPM's own words are valid from link start on (before that it echoes ours)
            hal_set_real(inst->hal.pin.speed_fb,
                         (rtapi_s16)fspd_rx(hm2, i, FSPD_RXW_SPEED) * hal_get_real(inst->hal.param.max_rpm) / FSPD_SPEED_FULL_SCALE);
            hal_set_bool(inst->hal.pin.alarm, (fspd_rx(hm2, i, FSPD_RXW_STATUS1) & FSPD_ALMA) != 0);
            hal_set_uint(inst->hal.pin.alarm_code, rxstate >> FSPD_STATE_ALARM_SHIFT);
            hal_set_bool(inst->hal.pin.zero_speed, (fspd_rx(hm2, i, FSPD_RXW_STATUS1) & FSPD_SSTA) != 0);
            hal_set_bool(inst->hal.pin.at_speed, (fspd_rx(hm2, i, FSPD_RXW_STATUS1) & FSPD_SARA) != 0);
            hal_set_uint(inst->hal.pin.load, fspd_rx(hm2, i, FSPD_RXW_LOAD));
            hal_set_uint(inst->hal.pin.status1, fspd_rx(hm2, i, FSPD_RXW_STATUS1));
            hal_set_uint(inst->hal.pin.status2, fspd_rx(hm2, i, FSPD_RXW_STATUS2));
        } else {
            hal_set_real(inst->hal.pin.speed_fb, 0.0);
            hal_set_bool(inst->hal.pin.alarm, 0);
            hal_set_uint(inst->hal.pin.alarm_code, 0);
            hal_set_bool(inst->hal.pin.zero_speed, 0);
            hal_set_bool(inst->hal.pin.at_speed, 0);
            hal_set_uint(inst->hal.pin.load, 0);
            hal_set_uint(inst->hal.pin.status1, 0);
            hal_set_uint(inst->hal.pin.status2, 0);
        }
        inst->rx_toggle = rxtoggle;
        hal_set_uint(inst->hal.pin.state_word, rxstate);

        const double cpr_param = hal_get_real(inst->hal.param.counts_per_rev);
        const double cpr = (cpr_param > 0) ? cpr_param : FSPD_DEFAULT_COUNTS_PER_REV;
        // straight-line extrapolation between the 8 ms updates, never further than one increment
        const double frac = (inst->update_ns > 0) ? fmin((double)inst->since_flip_ns / (double)inst->update_ns, 1.0) : 0.0;
        hal_set_sint(inst->hal.pin.position_raw, inst->position);
        hal_set_real(inst->hal.pin.position, inst->position / cpr);
        hal_set_real(inst->hal.pin.position_interpolated, (inst->position + inst->last_inc * frac) / cpr);
        hal_set_bool(inst->hal.pin.index_synced, inst->idx_state == FSPD_IDX_SYNCED);

        hal_set_sint(inst->hal.pin.state, inst->state);
        hal_set_bool(inst->hal.pin.link_ok, inst->state == FSPD_RUN);
        hal_set_bool(inst->hal.pin.fault, inst->state == FSPD_FAULT);
        hal_set_sint(inst->hal.pin.fault_code, inst->fault_code);
        hal_set_uint(inst->hal.pin.download_index, inst->index);
    }
}


void hm2_fspd_prepare_tram_write(hostmot2_t *hm2) {

    for (int i = 0; i < hm2->fspd.num_instances; i ++) {
        hm2_fspd_instance_t *inst = &hm2->fspd.instance[i];
        rtapi_u32 ctrl = 0;

        // two words per register: the even-numbered word in the low half
        for (int j = 0; j < HM2_FSPD_NUM_WORD_REGS; j ++) {
            hm2->fspd.txd_reg[j][i] = inst->tx[2 * j] | ((rtapi_u32)inst->tx[2 * j + 1] << FSPD_WORD_BITS);
        }
        hm2->fspd.hdrm_reg[i] = inst->hdrm ? inst->hdrm : FSPD_HDR_MARKER;
        hm2->fspd.hdrn_reg[i] = inst->hdr ? inst->hdr : FSPD_HDR_REPEAT;

        if (inst->state != FSPD_OFF && inst->state != FSPD_FAULT) {
            ctrl = FSPD_CTRL_TXEN | FSPD_CTRL_RXEN | FSPD_CTRL_RATIO4 | FSPD_CTRL_PHASE2;
            if (inst->commit) ctrl |= FSPD_CTRL_COMMIT;
        }
        if (inst->clear) {
            ctrl |= FSPD_CTRL_CLEAR;
            inst->clear = false;
        }
        inst->commit = false;
        hm2->fspd.ctrl_reg[i] = ctrl;
    }
}


void hm2_fspd_print_module(hostmot2_t *hm2) {
    if (hm2->fspd.num_instances <= 0) return;
    HM2_PRINT("Fanuc serial spindle links: %d\n", hm2->fspd.num_instances);
    HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n", hm2->fspd.clock_freq, hm2_hz_to_mhz(hm2->fspd.clock_freq));
    HM2_PRINT("    version: %d\n", hm2->fspd.version);
    HM2_PRINT("    base_addr: 0x%04X\n", hm2->fspd.base_addr);
    for (int i = 0; i < hm2->fspd.num_instances; i ++) {
        HM2_PRINT("    instance %d: state %d\n", i, hm2->fspd.instance[i].state);
    }
}
