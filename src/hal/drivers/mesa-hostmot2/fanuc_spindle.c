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
//                          bit12 clear counters, bit13 invert RX, bit14 invert TX
//   +20      TIMING   R/W  [11:0] frame length, [23:12] block 2 offset (bit times)
//   +21      STATUS   R    [7:0] good blocks, [15:8] bad units, [23:16] frames,
//                          bit24 new block, bit25 RX idle, bit26 commit pending,
//                          [30:27] commits taken (modulo 16)
//
// Words (true link numbering, see SPINDLE_LINK_PROTOCOL.md):
//   CNC -> SPM: w6 speed command (Q14 of P4020), w7 parameter command,
//               w8 parameter value, w9 control signal 1 (G070/G071),
//               w10 control signal 2 (G072/G073), w11 data-select,
//               w12 mode word (bit 0 toggles), w13 mode word 2
//   The speed command is Q14 of the SPM's parameter 4020 (maximum motor speed); set the
//   max-rpm parameter to that value. The CNC itself scales by 3741, equal to 4020 on Kira.
//               w13 bit 8: a rising edge arms the SPM's one-rev synchronisation (index)
//   SPM -> CNC: w4 position increment counted from the one-rev marker (after an arm),
//               w5 load meter, w6 speed meter, w7 parameter echo,
//               w9 status 1 (F045/F046), w10 status 2, w11 position increment,
//               w12 state word (high byte = alarm number, bit 3 initialised, bit 6 one-rev synced),
//               w15 data-select answer
//


#include <rtapi.h>
#include <rtapi_string.h>
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

#define FSPD_HDR_BASE 0x00400000u

// control signal 1 (w9) bits
#define FSPD_TLMLA (1u << 0)
#define FSPD_TLMHA (1u << 1)
#define FSPD_SRVA  (1u << 4)
#define FSPD_SFRA  (1u << 5)
#define FSPD_ORCMA (1u << 6)
#define FSPD_MRDYA (1u << 7)
#define FSPD_ARSTA (1u << 8)
#define FSPD_ESPA  (1u << 9)

// state numbers, also exported on the state pin
enum {
    FSPD_OFF = 0,
    FSPD_PRE,
    FSPD_LOOPBACK,
    FSPD_KEY,
    FSPD_START,
    FSPD_WAIT_INIT,
    FSPD_SELECT,
    FSPD_DOWNLOAD,
    FSPD_WAIT_DONE,
    FSPD_RUN,
    FSPD_FAULT
};

// fault codes, exported on the fault-code pin
enum {
    FSPD_FAULT_NONE = 0,
    FSPD_FAULT_LOOPBACK = 1,   // the SPM did not echo the loopback pattern (CNC alarm 750 class)
    FSPD_FAULT_NO_INIT = 2,    // no "initialised" bit after the link start (750 class)
    FSPD_FAULT_DOWNLOAD = 3,   // no "download complete" after the download (750 class)
    FSPD_FAULT_LINK = 4,       // no good blocks while running (749 class)
    FSPD_FAULT_SELECT = 5,     // the SPM did not echo a data-select (749 class on the 21i-B)
    FSPD_FAULT_SPM_RESET = 6,  // the SPM dropped its "initialised" bit while running (it restarted its link)
};

// update steps (one step = one 8 ms content update)
#define FSPD_INIT_STEPS 12
#define FSPD_SELECT_STEPS 20
#define FSPD_DONE_STEPS 125
#define FSPD_UNINIT_STEPS 4

// one-rev (index) synchronisation
enum {
    FSPD_IDX_FREE = 0,   // position = sum of w11, no marker reference
    FSPD_IDX_ARM,        // w13 bit 8 raised, waiting for the SPM to drop its "synced" bit (w12 bit 6)
    FSPD_IDX_WAIT,       // SPM waits for its one-rev marker; position held at 0, w4 counted
    FSPD_IDX_SYNCED      // position = sum of w4 since the marker
};
#define FSPD_LINK_TIMEOUT_NS 50000000LL


static void fspd_set_word(hm2_fspd_instance_t *inst, int w, rtapi_u16 v) {
    inst->tx[w] = v;
}

static void fspd_clear_words(hm2_fspd_instance_t *inst) {
    int w;
    for (w = 0; w < 16; w ++) inst->tx[w] = 0;
}

static rtapi_u16 fspd_param_word(hm2_fspd_instance_t *inst, int n) {
    return (rtapi_u16)hal_get_uint(inst->hal.param.value[n]);
}


int hm2_fspd_parse_md(hostmot2_t *hm2, int md_index) {
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    int r, i, n;

    if (!hm2_md_is_consistent_or_complain(hm2, md_index, 0, FSPD_NUM_REGS, 4, 0x003FFFFF)) {
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

    // TRAM: read the received words, the received header and the status;
    // write the staging words, both headers and the control register.
    for (i = 0; i < 8; i ++) {
        r = hm2_register_tram_read_region(hm2, md->base_address + (FSPD_REG_RXD + i) * md->register_stride,
                                          hm2->fspd.num_instances * sizeof(rtapi_u32), &hm2->fspd.rxd_reg[i]);
        if (r < 0) goto fail_tram;
        r = hm2_register_tram_write_region(hm2, md->base_address + (FSPD_REG_TXD + i) * md->register_stride,
                                           hm2->fspd.num_instances * sizeof(rtapi_u32), &hm2->fspd.txd_reg[i]);
        if (r < 0) goto fail_tram;
    }
    r = hm2_register_tram_read_region(hm2, md->base_address + FSPD_REG_RXHDR * md->register_stride,
                                      hm2->fspd.num_instances * sizeof(rtapi_u32), &hm2->fspd.rxhdr_reg);
    if (r < 0) goto fail_tram;
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

    for (i = 0; i < hm2->fspd.num_instances; i ++) {
        hm2_fspd_instance_t *inst = &hm2->fspd.instance[i];
        char name[HAL_NAME_LEN + 1];

        rtapi_snprintf(name, sizeof(name), "%s.fspd.%02d", hm2->llio->name, i);

        r = 0;
        r += hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.enable, 0, "%s.enable", name);
        r += hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.spindle_on, 0, "%s.spindle-on", name);
        r += hal_pin_new_real(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.speed_cmd, 0.0, "%s.speed-cmd-rpm", name);
        r += hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.estop_ok, 0, "%s.estop-ok", name);
        r += hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.machine_ready, 0, "%s.machine-ready", name);
        r += hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.alarm_reset, 0, "%s.alarm-reset", name);
        r += hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.orient, 0, "%s.orient", name);
        r += hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.torque_limit_low, 0, "%s.torque-limit-low", name);
        r += hal_pin_new_bool(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.torque_limit_high, 0, "%s.torque-limit-high", name);
        r += hal_pin_new_ui32(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.control1_extra, 0, "%s.control1-extra", name);
        r += hal_pin_new_ui32(hm2->llio->comp_id, HAL_IN, &inst->hal.pin.control2, 0, "%s.control2", name);

        r += hal_pin_new_si32(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.state, FSPD_OFF, "%s.state", name);
        r += hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.link_ok, 0, "%s.link-ok", name);
        r += hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.fault, 0, "%s.fault", name);
        r += hal_pin_new_si32(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.fault_code, 0, "%s.fault-code", name);
        r += hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.alarm, 0, "%s.alarm", name);
        r += hal_pin_new_ui32(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.alarm_code, 0, "%s.alarm-code", name);
        r += hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.speed_fb, 0.0, "%s.speed-fb-rpm", name);
        r += hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.at_speed, 0, "%s.at-speed", name);
        r += hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.zero_speed, 0, "%s.zero-speed", name);
        r += hal_pin_new_ui32(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.load, 0, "%s.load-raw", name);
        r += hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.position, 0.0, "%s.position", name);
        r += hal_pin_new_si32(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.position_raw, 0, "%s.position-raw", name);
        r += hal_pin_new_real(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.position_interpolated, 0.0, "%s.position-interpolated", name);
        r += hal_pin_new_bool(hm2->llio->comp_id, HAL_IO, &inst->hal.pin.index_enable, 0, "%s.index-enable", name);
        r += hal_pin_new_bool(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.index_synced, 0, "%s.index-synced", name);
        r += hal_pin_new_ui32(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.status1, 0, "%s.status1", name);
        r += hal_pin_new_ui32(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.status2, 0, "%s.status2", name);
        r += hal_pin_new_ui32(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.state_word, 0, "%s.state-word", name);
        r += hal_pin_new_ui32(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.rom_id, 0, "%s.rom-id", name);
        r += hal_pin_new_ui32(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.rom_edition, 0, "%s.rom-edition", name);
        r += hal_pin_new_ui32(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.good_blocks, 0, "%s.good-blocks", name);
        r += hal_pin_new_ui32(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.bad_units, 0, "%s.bad-units", name);
        r += hal_pin_new_ui32(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.resends, 0, "%s.resends", name);
        r += hal_pin_new_ui32(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.download_index, 0, "%s.download-index", name);
        for (n = 0; n < 16; n ++) {
            r += hal_pin_new_ui32(hm2->llio->comp_id, HAL_OUT, &inst->hal.pin.rx_word[n], 0, "%s.rx-w%02d", name, n);
        }

        r += hal_param_new_real(hm2->llio->comp_id, HAL_RW, &inst->hal.param.max_rpm, 8000.0, "%s.max-rpm", name);
        r += hal_param_new_real(hm2->llio->comp_id, HAL_RW, &inst->hal.param.counts_per_rev, 4096.0, "%s.counts-per-rev", name);
        r += hal_param_new_ui32(hm2->llio->comp_id, HAL_RW, &inst->hal.param.data_select, 4, "%s.data-select", name);
        r += hal_param_new_ui32(hm2->llio->comp_id, HAL_RW, &inst->hal.param.last_index, 375, "%s.last-index", name);
        for (n = 0; n < HM2_FSPD_MAX_PARAMS; n ++) {
            r += hal_param_new_ui32(hm2->llio->comp_id, HAL_RW, &inst->hal.param.value[n], 0, "%s.param-%03d", name, n);
        }
        if (r < 0) {
            HM2_ERR("error adding pins or parameters for %s, aborting\n", name);
            r = -EINVAL;
            goto fail0;
        }

        inst->state = FSPD_OFF;
    }

    return hm2->fspd.num_instances;

fail_tram:
    HM2_ERR("error registering tram regions for fspd (%d)\n", r);
fail0:
    hm2->fspd.num_instances = 0;
    return r;
}


void hm2_fspd_cleanup(hostmot2_t *hm2) {
    int i;
    if (hm2->fspd.num_instances <= 0) return;
    // stop transmitting: the SPM then stops the spindle with its own link alarm (AL-52)
    for (i = 0; i < hm2->fspd.num_instances; i ++) {
        rtapi_u32 zero = 0;
        hm2->llio->write(hm2->llio,
                         hm2->fspd.base_addr + FSPD_REG_CTRL * hm2->fspd.register_stride + i * hm2->fspd.instance_stride,
                         &zero, sizeof(zero));
    }
}


static void fspd_enter(hm2_fspd_instance_t *inst, int state) {
    inst->state = state;
    inst->step = 0;
    if (state == FSPD_OFF) inst->commit_wait = 0;
}

static void fspd_fault(hm2_fspd_instance_t *inst, int code) {
    inst->fault_code = code;
    fspd_enter(inst, FSPD_FAULT);
}

static rtapi_u16 fspd_rx(hostmot2_t *hm2, int i, int w) {
    rtapi_u32 v = hm2->fspd.rxd_reg[w / 2][i];
    return (w & 1) ? (rtapi_u16)(v >> 16) : (rtapi_u16)(v & 0xFFFF);
}


// Called once per content update while the link runs: prepares the words
// the FPGA will send for the next 4 frames.
static void fspd_step(hostmot2_t *hm2, int i) {
    hm2_fspd_instance_t *inst = &hm2->fspd.instance[i];
    rtapi_u32 hdr = FSPD_HDR_BASE | 0x50000000u;
    rtapi_u32 hdrm = FSPD_HDR_BASE | 0x70000000u;
    int running = 0;
    int w;

    switch (inst->state) {

    case FSPD_PRE:
        // what Kira's 21-MB sends before the loopback test (headers and w0 as recorded)
        fspd_clear_words(inst);
        if (inst->step < 2) {
            hdr = hdrm = FSPD_HDR_BASE | 0x4F000000u;
        } else if (inst->step < 5) {
            hdr = hdrm = FSPD_HDR_BASE | 0x46000000u;
            fspd_set_word(inst, 0, 2);
        } else if (inst->step < 6) {
            hdr = hdrm = FSPD_HDR_BASE | 0xCF000000u;
        } else {
            hdr = hdrm = FSPD_HDR_BASE | 0xC6000000u;
            fspd_set_word(inst, 0, 3);
        }
        if (++inst->step >= 9) fspd_enter(inst, FSPD_LOOPBACK);
        break;

    case FSPD_LOOPBACK:
        // Recorded CNC timing, kept exactly: header 0x48 for two updates, then 0x44 for one.
        // The real SPM drops the start-up if the master lingers here (bench, 2026-11-13), so the
        // sequence does not wait for the echo. The echo is watched in the background instead (the
        // SPM echoes only part of the words during 0x48 and all of w2..w13 after 0x44).
        hdr = hdrm = FSPD_HDR_BASE | ((inst->step < 2) ? 0x48000000u : 0x44000000u);
        fspd_set_word(inst, 0, 2);
        fspd_set_word(inst, 1, 0);
        for (w = 2; w < 16; w ++) {
            fspd_set_word(inst, w, (w & 1) ? 0xAA55 : 0x55AA);
        }
        if (++inst->step >= 3) fspd_enter(inst, FSPD_KEY);
        break;

    case FSPD_KEY:
        // recorded: header 0x48 for three updates, then 0x44 for three
        hdr = hdrm = FSPD_HDR_BASE | ((inst->step < 3) ? 0x48000000u : 0x44000000u);
        fspd_clear_words(inst);
        fspd_set_word(inst, 0, 2);
        fspd_set_word(inst, 12, 0x6996);
        if (++inst->step >= 6) {
            if (inst->echo_seen) {
                fspd_enter(inst, FSPD_START);
            } else {
                fspd_fault(inst, FSPD_FAULT_LOOPBACK);
            }
        }
        break;

    case FSPD_START:
        fspd_clear_words(inst);
        fspd_set_word(inst, 0, 2);
        if (inst->step == 0) {
            hdr = hdrm = FSPD_HDR_BASE | 0x4B000000u;
            inst->mode = 0x4000;
        } else if (inst->step == 1) {
            inst->mode = 0x4001;
        } else {
            inst->mode = 0xC000;        // bit 15 once: link start, resets AL-24/25
        }
        fspd_set_word(inst, 12, inst->mode);
        if (++inst->step >= 3) {
            inst->mode = 0x4000;
            fspd_enter(inst, FSPD_WAIT_INIT);
        }
        break;

    default:
        running = 1;
        break;
    }

    // The states above each fill one whole update; a state entered in this call starts with the next one.
    if (running && inst->state >= FSPD_WAIT_INIT && inst->state <= FSPD_RUN) {
        rtapi_u16 rxstate = fspd_rx(hm2, i, 12);

        inst->mode ^= 1;
        fspd_set_word(inst, 12, inst->mode & 0x7FFF);
        fspd_set_word(inst, 0, 2);

        switch (inst->state) {

        case FSPD_WAIT_INIT:
            if (rxstate & 0x0008) {
                fspd_enter(inst, FSPD_SELECT);
            } else if (++inst->step >= FSPD_INIT_STEPS) {
                fspd_fault(inst, FSPD_FAULT_NO_INIT);
            }
            break;

        case FSPD_SELECT: {
            // Data-select 1, 2, 3 read the ROM ID ("9D", "20", "11"), then the configured select
            // (4 on Kira: parameter range 0x177). Each select is held until the SPM echoes it in its
            // w7 low bits. The SPM fixes its parameter range at the first parameter command, so the
            // final select must be acknowledged before the download starts.
            rtapi_u32 dsel = hal_get_uint(inst->hal.param.data_select) & 0xFF;
            rtapi_u32 sel = inst->sel ? inst->sel : 1;
            int final = (inst->sel > 3);
            if (final) sel = dsel;
            fspd_set_word(inst, 11, sel);
            if (final) fspd_set_word(inst, 13, 0x0004);
            if (inst->step >= 2 && (fspd_rx(hm2, i, 7) & 0x1FF) == sel) {
                rtapi_u16 ans = fspd_rx(hm2, i, 15);
                if (!final) {
                    if (sel == 1) inst->rom_id = (rtapi_u32)ans << 16;
                    if (sel == 2) inst->rom_id |= ans;
                    if (sel == 3) inst->rom_edition = ans;
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
            fspd_set_word(inst, 11, hal_get_uint(inst->hal.param.data_select) & 0xFF);
            fspd_set_word(inst, 13, 0x0004);
            // Strictly in sequence (a skipped index needs an SPM power cycle), in the CNC's own
            // pacing: two updates each write a parameter, then two updates repeat the last index
            // without the write bit.
            if ((inst->step & 3) < 2) {
                fspd_set_word(inst, 7, 0x4000 | inst->index);
                fspd_set_word(inst, 8, fspd_param_word(inst, inst->index));
                inst->dl_last = inst->index;
                if (inst->index >= last) {
                    inst->index = last;
                    fspd_enter(inst, FSPD_WAIT_DONE);
                    break;
                }
                inst->index ++;
            } else {
                fspd_set_word(inst, 7, inst->dl_last);
            }
            inst->step ++;
            break;
        }

        case FSPD_WAIT_DONE:
            fspd_set_word(inst, 7, inst->index);    // last index, no write bit: idle
            fspd_set_word(inst, 8, 0);
            if (fspd_rx(hm2, i, 7) & 0x8000) {
                fspd_enter(inst, FSPD_RUN);
            } else if (++inst->step >= FSPD_DONE_STEPS) {
                fspd_fault(inst, FSPD_FAULT_DOWNLOAD);
            }
            break;

        case FSPD_RUN: {
            double rpm = hal_get_real(inst->hal.pin.speed_cmd);
            double max = hal_get_real(inst->hal.param.max_rpm);
            rtapi_u32 c1 = hal_get_uint(inst->hal.pin.control1_extra) & 0xFFFF;
            rtapi_u32 q;

            if (max < 1.0) max = 1.0;
            if (!(rpm == rpm)) rpm = 0;     // NaN
            // the SPM restarted its link (power cycle, sequence error): it loops our words back
            // and its "initialised" bit is gone
            if (!(rxstate & 0x0008)) {
                if (++inst->step >= FSPD_UNINIT_STEPS) {
                    fspd_fault(inst, FSPD_FAULT_SPM_RESET);
                    break;
                }
            } else {
                inst->step = 0;
            }
            if (rpm < 0) {
                q = (rtapi_u32)(-rpm * 16384.0 / max + 0.5);
            } else {
                q = (rtapi_u32)(rpm * 16384.0 / max + 0.5);
            }
            if (q > 0x4000) q = 0x4000;

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

            fspd_set_word(inst, 6, (rtapi_u16)q);
            fspd_set_word(inst, 7, inst->index);
            fspd_set_word(inst, 8, 0);
            fspd_set_word(inst, 9, (rtapi_u16)c1);
            fspd_set_word(inst, 10, (rtapi_u16)(hal_get_uint(inst->hal.pin.control2) & 0xFFFF));
            fspd_set_word(inst, 11, hal_get_uint(inst->hal.param.data_select) & 0xFF);
            // w13 bit 8: a rising edge arms the SPM's one-rev synchronisation (see process_tram_read)
            fspd_set_word(inst, 13, 0x0004 | ((inst->idx_state == FSPD_IDX_ARM || inst->idx_state == FSPD_IDX_WAIT) ? 0x0100 : 0));
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
    int i, w;

    for (i = 0; i < hm2->fspd.num_instances; i ++) {
        hm2_fspd_instance_t *inst = &hm2->fspd.instance[i];
        rtapi_u32 status = hm2->fspd.status_reg[i];
        rtapi_u32 good = status & 0xFF;
        rtapi_u16 rxstate, rxtoggle;

        // received words
        for (w = 0; w < 16; w ++) {
            hal_set_uint(inst->hal.pin.rx_word[w], fspd_rx(hm2, i, w));
        }
        hal_set_uint(inst->hal.pin.good_blocks, good);
        hal_set_uint(inst->hal.pin.bad_units, (status >> 8) & 0xFF);

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
            inst->echo_seen = 0;
            inst->idx_state = FSPD_IDX_FREE;
            inst->update_ns = 0;
            inst->since_flip_ns = 0;
            inst->rom_id = 0;
            inst->rom_edition = 0;
            inst->index = 0;
            inst->position = 0;
            inst->silent_ns = 0;
            fspd_clear_words(inst);
            inst->clear = 1;
        }
        if (inst->state == FSPD_RUN && inst->silent_ns > FSPD_LINK_TIMEOUT_NS) {
            fspd_fault(inst, FSPD_FAULT_LINK);
        }
        // a rising edge of alarm-reset restarts a faulted link
        {
            int rst = hal_get_bool(inst->hal.pin.alarm_reset);
            if (inst->state == FSPD_FAULT && rst && !inst->last_reset) {
                fspd_enter(inst, FSPD_OFF);     // restarts in the next period while enable is set
            }
            inst->last_reset = rst;
        }

        // loopback echo: the SPM copies our pattern back until it has taken the key
        if (inst->state == FSPD_LOOPBACK || inst->state == FSPD_KEY) {
            int ok = 1;
            for (w = 2; w < 14; w ++) {
                if (fspd_rx(hm2, i, w) != ((w & 1) ? 0xAA55 : 0x55AA)) ok = 0;
            }
            if (ok) inst->echo_seen = 1;
        }

        // One state machine step per content update. The FPGA counts the commits it has taken, so a
        // commit that never arrived (a lost write) is sent again with the same words instead of
        // moving on: the SPM does not survive a skipped download index.
        if (inst->state != FSPD_OFF && inst->state != FSPD_FAULT && !(status & FSPD_STATUS_PENDING)) {
            rtapi_u32 cnt = (status >> FSPD_STATUS_COMMITS_SHIFT) & 0xF;
            if (inst->commit_wait && cnt == inst->commit_cnt) {
                if (++inst->commit_age >= FSPD_COMMIT_RESEND) {
                    inst->commit = 1;
                    inst->commit_age = 0;
                    hal_set_uint(inst->hal.pin.resends, hal_get_uint(inst->hal.pin.resends) + 1);
                }
            } else {
                fspd_step(hm2, i);
                inst->commit = 1;
                inst->commit_wait = 1;
                inst->commit_cnt = cnt;
                inst->commit_age = 0;
            }
        }

        // feedback from the SPM, once per SPM update (its state word toggle)
        rxstate = fspd_rx(hm2, i, 12);
        rxtoggle = rxstate & 1;
        if (inst->state == FSPD_RUN) {
            int flip = (rxtoggle != inst->rx_toggle);
            int synced = (rxstate & 0x0040) != 0;
            rtapi_s32 inc = 0;

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
                if (inst->idx_state == FSPD_IDX_WAIT || inst->idx_state == FSPD_IDX_SYNCED) {
                    inc = (rtapi_s16)fspd_rx(hm2, i, 4);
                } else {
                    inc = (rtapi_s16)fspd_rx(hm2, i, 11);
                }
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
                         (rtapi_s16)fspd_rx(hm2, i, 6) * hal_get_real(inst->hal.param.max_rpm) / 16384.0);
            hal_set_bool(inst->hal.pin.alarm, (fspd_rx(hm2, i, 9) & 1) != 0);
            hal_set_uint(inst->hal.pin.alarm_code, rxstate >> 8);
            hal_set_bool(inst->hal.pin.zero_speed, (fspd_rx(hm2, i, 9) & 0x02) != 0);
            hal_set_bool(inst->hal.pin.at_speed, (fspd_rx(hm2, i, 9) & 0x08) != 0);
            hal_set_uint(inst->hal.pin.load, fspd_rx(hm2, i, 5));
            hal_set_uint(inst->hal.pin.status1, fspd_rx(hm2, i, 9));
            hal_set_uint(inst->hal.pin.status2, fspd_rx(hm2, i, 10));
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

        {
            double cpr = hal_get_real(inst->hal.param.counts_per_rev);
            if (cpr <= 0) cpr = 4096.0;
            hal_set_sint(inst->hal.pin.position_raw, (rtapi_s32)inst->position);
            hal_set_real(inst->hal.pin.position, inst->position / cpr);
            // straight-line extrapolation between the 8 ms updates, never further than one increment
            {
                double f = 0.0;
                if (inst->update_ns > 0) f = (double)inst->since_flip_ns / (double)inst->update_ns;
                if (f > 1.0) f = 1.0;
                hal_set_real(inst->hal.pin.position_interpolated, (inst->position + inst->last_inc * f) / cpr);
            }
            hal_set_bool(inst->hal.pin.index_synced, inst->idx_state == FSPD_IDX_SYNCED);
        }

        hal_set_sint(inst->hal.pin.state, inst->state);
        hal_set_bool(inst->hal.pin.link_ok, inst->state == FSPD_RUN);
        hal_set_bool(inst->hal.pin.fault, inst->state == FSPD_FAULT);
        hal_set_sint(inst->hal.pin.fault_code, inst->fault_code);
        hal_set_uint(inst->hal.pin.rom_id, inst->rom_id);
        hal_set_uint(inst->hal.pin.rom_edition, inst->rom_edition);
        hal_set_uint(inst->hal.pin.download_index, inst->index);
    }
}


void hm2_fspd_prepare_tram_write(hostmot2_t *hm2) {
    int i, j;

    for (i = 0; i < hm2->fspd.num_instances; i ++) {
        hm2_fspd_instance_t *inst = &hm2->fspd.instance[i];
        rtapi_u32 ctrl = 0;

        for (j = 0; j < 8; j ++) {
            hm2->fspd.txd_reg[j][i] = inst->tx[2 * j] | ((rtapi_u32)inst->tx[2 * j + 1] << 16);
        }
        hm2->fspd.hdrm_reg[i] = inst->hdrm ? inst->hdrm : (FSPD_HDR_BASE | 0x70000000u);
        hm2->fspd.hdrn_reg[i] = inst->hdr ? inst->hdr : (FSPD_HDR_BASE | 0x50000000u);

        if (inst->state != FSPD_OFF && inst->state != FSPD_FAULT) {
            ctrl = FSPD_CTRL_TXEN | FSPD_CTRL_RXEN | FSPD_CTRL_RATIO4 | FSPD_CTRL_PHASE2;
            if (inst->commit) ctrl |= FSPD_CTRL_COMMIT;
        }
        if (inst->clear) {
            ctrl |= FSPD_CTRL_CLEAR;
            inst->clear = 0;
        }
        inst->commit = 0;
        hm2->fspd.ctrl_reg[i] = ctrl;
    }
}


void hm2_fspd_print_module(hostmot2_t *hm2) {
    int i;
    if (hm2->fspd.num_instances <= 0) return;
    HM2_PRINT("Fanuc serial spindle links: %d\n", hm2->fspd.num_instances);
    HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n", hm2->fspd.clock_freq, hm2_hz_to_mhz(hm2->fspd.clock_freq));
    HM2_PRINT("    version: %d\n", hm2->fspd.version);
    HM2_PRINT("    base_addr: 0x%04X\n", hm2->fspd.base_addr);
    for (i = 0; i < hm2->fspd.num_instances; i ++) {
        HM2_PRINT("    instance %d: state %d\n", i, hm2->fspd.instance[i].state);
    }
}
