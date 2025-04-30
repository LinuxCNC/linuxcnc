// Copyright (C) 2025 B Stultiens
// Parts from mesa_modbus.c.tmpl Copyright (C) 2023 Andy Pugh
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
//

/* A generic configurable Modbus component using Mesa PktUART interfaces */


#include "rtapi.h"
#include "rtapi_slab.h"
#include "rtapi_app.h"
#include "rtapi_string.h"
#include "rtapi_byteorder.h"
#include "rtapi_ctype.h"
#include "rtapi_math.h"
#include "hal.h"
#include "hostmot2-serial.h"

#include "hm2_modbus.h"

#if !defined(__KERNEL__)
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <endian.h>
static inline rtapi_u32 be32_to_cpu(rtapi_u32 v) { return be32toh(v); }
static inline rtapi_u16 be16_to_cpu(rtapi_u16 v) { return be16toh(v); }
#else
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/byteorder/generic.h>
#endif

// Define to compile in debug messages
#define DEBUG
#ifdef DEBUG
//#define DEBUG_STATE
#endif

/* module information */
MODULE_AUTHOR("B.Stultiens");
MODULE_DESCRIPTION("Modbus interface and control using Mesa PktUART");
MODULE_LICENSE("GPL");

// The number of instances we support
#define MAX_PORTS 8

// The number of consecutive send/receive errors before a command gets disabled
#define MAX_ERRORS	5

static const char *error_codes[] = {
	"Invalid exception code: 0",	// 0 - Not a valid error
	"Illegal Function",
	"Illegal Data Address",
	"Illegal Data Value",
	"Server Device Failure",
	"Acknowledge",
	"Server Device Busy",
	"Negative Acknowledge",			// 7 - Not defined in V1.1b3
	"Memory Parity Error",
	"Unknown exception code: 9",
	"Gateway Path Unavailable",
	"Gateway Failed to Respond",
	"Comm Timeout"					// 12 - Not defined in V1.1b3
};

// Byte-order markers for types
// The lower nibble will hold the HAL_XXX type enum value (except float/double
// distinction). The values are used as indices!
#define MBT_AB			0x00
#define MBT_BA			0x01
#define MBT_ABCD		0x02
#define MBT_BADC		0x03
#define MBT_CDAB		0x04
#define MBT_DCBA		0x05
#define MBT_ABCDEFGH	0x06
#define MBT_BADCFEHG	0x07
#define MBT_CDABGHEF	0x08
#define MBT_DCBAHGFE	0x09
#define MBT_EFGHABCD	0x0a
#define MBT_FEHGBADC	0x0b
#define MBT_GHEFCDAB	0x0c
#define MBT_HGFEDCBA	0x0d

#define MBT_U			0x00	// Which makes default U_AB zero
#define MBT_S			0x10
#define MBT_F			0x20

#define MBT_X_MASK		0x0f
#define MBT_T_MASK		0xf0

static inline bool mtypeiscompound(unsigned mtype) { return mtype >= MBT_ABCD; }
static inline bool mtypeisvalid(unsigned mtype)    { return (mtype & MBT_X_MASK) <= MBT_HGFEDCBA && (mtype & MBT_T_MASK) <= MBT_F; }
static inline unsigned mtypeformat(unsigned mtype) { return mtype & MBT_X_MASK; }
static inline unsigned mtypetype(unsigned mtype)   { return mtype & MBT_T_MASK; }
static inline unsigned mtypesize(unsigned mtype) {
	static const rtapi_u8 s[16] = {1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 4, 4, 4, 1, 1};
	return s[mtypeformat(mtype)];
}

// Supported Modbus commands
#define MBCMD_R_COILS      1
#define MBCMD_R_INPUTS     2
#define MBCMD_R_REGISTERS  3
#define MBCMD_R_INPUTREGS  4
#define MBCMD_W_COIL       5
#define MBCMD_W_REGISTER   6
#define MBCMD_W_COILS     15
#define MBCMD_W_REGISTERS 16

#define xstr(x) #x
#define str(x) xstr(x)

#define COMP_NAME "hm2_modbus"

// Maximum PDU data payload
#define MAX_MSG_LEN 252
// One byte device address, one byte command, two bytes CRC
#define MAX_PKT_LEN (1 + 1 + MAX_MSG_LEN + 2)

#ifndef DEBUG
#define MSG_DBG(fmt...)		do{}while(0)
#else
#define MSG_DBG(fmt...)		do { rtapi_print_msg(RTAPI_MSG_DBG,  fmt); } while(0)
#endif
#define MSG_INFO(fmt...)	do { rtapi_print_msg(RTAPI_MSG_INFO, fmt); } while(0)
#define MSG_ERR(fmt...)		do { rtapi_print_msg(RTAPI_MSG_ERR,  fmt); } while(0)
#define MSG_WARN(fmt...)	do { rtapi_print_msg(RTAPI_MSG_WARN, fmt); } while(0)

// State-machine states
enum {
	STATE_START = 0,
	STATE_WAIT_FOR_TIMEOUT,
	STATE_WAIT_FOR_SEND_COMPLETE,
	STATE_WAIT_FOR_DATA_FRAME,
	STATE_FETCH_DATA,
	STATE_FETCH_MORE_DATA,
	STATE_HANDLE_DATA,
	STATE_RESET_WAIT,
	STATE_LAST,
};

#ifdef DEBUG
static const char *state_names[] = {
	"STATE_START",
	"STATE_WAIT_FOR_TIMEOUT",
	"STATE_WAIT_FOR_SEND_COMPLETE",
	"STATE_WAIT_FOR_DATA_FRAME",
	"STATE_FETCH_DATA",
	"STATE_FETCH_MORE_DATA",
	"STATE_HANDLE_DATA",
	"STATE_RESET_WAIT",
	"STATE_LAST",
};
#endif

// Overlapping types to handle byte-ordering
typedef union {
	rtapi_u8	b[4];
	rtapi_u16	w[2];
	rtapi_u32	u;
	rtapi_s32	s;
	float		f;
} mb_types32_u;

typedef union {
	rtapi_u8	b[8];
	rtapi_u16	w[4];
	rtapi_u64	u;
	rtapi_s64	s;
	double		f;
} mb_types64_u;

typedef struct {
	hal_data_u	*pin;		// Modbus data pin
	hal_data_u	*offset;	// Pin offset input
	hal_float_t *scale;		// Pin scale input
	hal_float_t	*scaled;	// Pin scaled output
} mbt_pin_hal_t;

typedef struct {
	hal_bit_t *disabled;	// Command disable
	hal_bit_t *reset;		// Reset errors and re-enable on rising edge
	hal_u32_t *error;		// Command error counter
	hal_u32_t *errorcode;	// Last error code
} mbt_cmd_hal_t;

typedef struct {
	mbt_pin_hal_t *pins;	// All data pins
	mbt_cmd_hal_t *cmds;	// Per command pins
	hal_bit_t *suspend;		// Suspend running commands
	hal_bit_t *reset;		// Reset command errors and re-enable on rising edge
	hal_bit_t *fault;
	hal_u32_t *faultcmd;
	hal_u32_t *lasterror;
	hal_u32_t baudrate;	// RO
	hal_u32_t parity;	// RO
	hal_u32_t stopbits;	// RO
	hal_u32_t icdelay;	// RO Inter character delay
	hal_u32_t txdelay;	// RO Inter frame delay for packets sent
	hal_u32_t rxdelay;	// RO Inter frame delay for packet end detection in receive
	hal_u32_t drvdelay;	// RO Delay before sending data (in bit times)
} hm2_modbus_hal_t;

// The command structure and data buffer.
// Note: The buffer is wasted on init lines and delay commands. But wasting a
// few kilobytes is not fatal in modern times.
typedef struct {
	hm2_modbus_mbccb_cmds_t cmd;	// In host order
	hm2_modbus_mbccb_type_t *typeptr;	// The types for this command
	rtapi_s64	interval;			// The running interval of this command
	int pinref;		// What pin to start with
	bool disabled;	// Skipped if set
	bool prevreset;	// To track the rising edge
	int errors;		// Count the errors
	int datalen;	// Number of bytes in 'data' buffer
	rtapi_u8 data[MAX_PKT_LEN]; // PDU: 2-byte header, MAX_MSG_LEN payload, 2-byte CRC
} hm2_modbus_cmd_t;

static inline bool hastimesout(const hm2_modbus_cmd_t *cc)  { return 0 != (cc->cmd.flags & MBCCB_CMDF_TIMESOUT); }
static inline bool hasbcanswer(const hm2_modbus_cmd_t *cc)  { return 0 != (cc->cmd.flags & MBCCB_CMDF_BCANSWER); }
static inline bool hasnoanswer(const hm2_modbus_cmd_t *cc)  { return 0 != (cc->cmd.flags & MBCCB_CMDF_NOANSWER); }
static inline bool hasresend(const hm2_modbus_cmd_t *cc)    { return 0 != (cc->cmd.flags & MBCCB_CMDF_RESEND); }
static inline bool haspinscale(const hm2_modbus_mbccb_type_t *t)  { return 0 != (t->flags & MBCCB_PINF_SCALE); }
static inline bool haspinclamp(const hm2_modbus_mbccb_type_t *t)  { return 0 != (t->flags & MBCCB_PINF_CLAMP); }

typedef struct {
	char		name[HAL_NAME_LEN];		// What we call ourselves (hm2_modbus.X)
	char		uart[HAL_NAME_LEN];		// The PktUART we attached to (like hm2_5i25.Y.pktuart.Z)

	hm2_modbus_mbccb_header_t *mbccb;		// Modbus command control binary
	ssize_t		mbccbsize;					// Buffer/file size
	const hm2_modbus_mbccb_cmds_t *initptr;	// Pointer to mbccb init section
	hm2_modbus_mbccb_cmds_t *cmdsptr;		// Pointer to mbccb cmds section
	const rtapi_u8 *dataptr;				// Pointer to mbccb data section
	unsigned	ninit;		// Total number of inits
	unsigned	ncmds;		// Total number of commands
	unsigned	npins;		// Total number of pins

	hm2_modbus_hal_t *hal;	// HAL pins and params
	bool		prevreset;	// Last state of 'reset' hal pin

	hm2_modbus_cmd_t *_init;	// List of inits sent initially
	hm2_modbus_cmd_t *_cmds;	// List of commands sent in loop
	hm2_modbus_cmd_t *cmds;		// List of commands sent in loop *or* inits at startup
	unsigned cmdidx;			// Where are we in the list

	hm2_pktuart_config_t cfg_rx;	// Receiver config
	hm2_pktuart_config_t cfg_tx;	// Transmitter config

	int			state;			// State-machine state
	bool		suspended;		// Suspend operation when set
	bool		ignoredata;		// Ignore received packet if set
	unsigned	rxversion;		// The version of the PktUART RX channel
	unsigned	txversion;		// The version of the PktUART TX channel
	unsigned	maxicharbits;	// The max allowed inter-character delay (in bit-times)

	unsigned	frameidx;	// Which frame we are handling (should only ever be 0)
	rtapi_u32	fsizes[16];	// See HM2_PKTUART_RCR_* defines for bit-fields
	rtapi_u32	rxdata[256];	// 0x400 bytes, 0x100 32-bit words

	rtapi_s64	timeout;	// Timeout timer for commands
} hm2_modbus_inst_t;

typedef struct {
	int ninsts;
	hm2_modbus_inst_t *insts;
} hm2_modbus_t;


static int comp_id = -1;	// HAL component ID
static hm2_modbus_t mb;		// Our instances

// Forward declarations
static int parse_data_frame(hm2_modbus_inst_t *inst);
static int build_data_frame(hm2_modbus_inst_t *inst);
static rtapi_u16 crc_modbus(const rtapi_u8 *buffer, size_t len);


/*
 * The PktUART interfaces to be linked to the hm2_modbus module. This consists
 * of a list of HAL names like:
 *   ports="hm2_7i95.0.pktuart.0","hm2_5i25.0.pktuart.7"
 */
static char *ports[MAX_PORTS];
RTAPI_MP_ARRAY_STRING(ports, MAX_PORTS, "PktUART HAL names");

/*
 * The Modbus configuration and command structure files for each PktUART
 * interface to be read by the hm2_modbus module. This should be a list of
 * absolute path file names like:
 *   files="/usr/share/linuxcnc/modbus/spindle.mbccb","/home/test/xyz.mbccb"
 */
static char *mbccbs[MAX_PORTS];
RTAPI_MP_ARRAY_STRING(mbccbs, MAX_PORTS, "Binary Modbus configuration and command sequence absolute path file names");

/*
 * Set the message level for debugging purpose. This has the (side-)effect that
 * all modules within this process will start spitting out messages at the
 * requested level.
 * The upstream message level is not touched if debug == -1.
 */
static int debug = -1;
RTAPI_MP_INT(debug, "Set message level for debugging purpose [0...5] where 0=none and 5=all (default: -1; upstream defined)");


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static inline hm2_modbus_cmd_t *current_cmd(hm2_modbus_inst_t *inst)
{
	return &inst->cmds[inst->cmdidx];
}

static inline bool handling_inits(const hm2_modbus_inst_t *inst)
{
	return inst->cmds == inst->_init;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static unsigned calc_icdelay(unsigned baudrate, unsigned parity, unsigned stopbits, unsigned icdelay)
{
	if(icdelay) {
		return icdelay;	// Manual setup
	} else if(baudrate <= 19200) {
		unsigned bits = 8 + (parity ? 1 : 0) + stopbits;
		return (15 * bits + 14) / 10;	// 1.5 char delay
	}
	// Faster than 19k2
	unsigned bits = (75 * baudrate + 99999) / 100000;	// 750 us
	return bits > 0xff ? 0xff : bits;
}

//
// A V3+ RX has inter-character delay measurement
// t1.5: see section 2.5.5.1
// of "MODBUS over serial line specification and implementation guide V1.02"
//
static void setup_icdelay(hm2_modbus_inst_t *inst, unsigned baudrate, unsigned parity, unsigned stopbits, unsigned icdelay)
{
	if(inst->rxversion >= 3) {
		inst->maxicharbits = calc_icdelay(baudrate, parity, stopbits, icdelay);
	} else {
		inst->maxicharbits = 0;
	}
	inst->hal->icdelay = inst->maxicharbits;
}

//
// Calculate the inter-frame delay time:
// - 3.5 chars if baudrate <= 19200
// - 1750 microseconds if baudrate > 19200
//
static unsigned calc_ifdelay(hm2_modbus_inst_t *inst, unsigned baudrate, unsigned parity, unsigned stopbits)
{
	if(baudrate > 582000) {
		MSG_WARN("%s: warning: Baudrate > 582000 will make inter-frame timer overflow. Setting to maximum.\n", inst->name);
		return 1020;
	}

	// calculation works for baudrates less than ~24 Mbit/s
	if(baudrate <= 19200)
		return (175u * baudrate + 99999u) / 100000u;
	unsigned bits = 1 + 8 + (parity ? 1 : 0) + (stopbits > 1 ? 2 : 1);
	return (bits * 35 + 9) / 10;	// Bit-times * 3.5 rounded up
}

//
// Send a communication parameter change
//
static int send_comms_change(hm2_modbus_inst_t *inst)
{
	int r;
	hm2_modbus_cmd_t *cc = current_cmd(inst);
	unsigned baudrate = cc->cmd.ibaudrate;

	// Set new baudrate
	inst->cfg_rx.baudrate = inst->cfg_tx.baudrate = baudrate;

	// Clear out previous flags
	inst->cfg_rx.flags &= ~(HM2_PKTUART_CONFIG_PARITYEN | HM2_PKTUART_CONFIG_PARITYODD | HM2_PKTUART_CONFIG_STOPBITS2);
	inst->cfg_tx.flags &= ~(HM2_PKTUART_CONFIG_PARITYEN | HM2_PKTUART_CONFIG_PARITYODD | HM2_PKTUART_CONFIG_STOPBITS2);
	// Set the new flags
	unsigned parity = 0;
	if(cc->cmd.flags & MBCCB_CMDF_PARITYEN) {
		inst->cfg_rx.flags |= HM2_PKTUART_CONFIG_PARITYEN;
		inst->cfg_tx.flags |= HM2_PKTUART_CONFIG_PARITYEN;
		parity |= 2;
	}
	if(cc->cmd.flags & MBCCB_CMDF_PARITYODD) {
		inst->cfg_rx.flags |= HM2_PKTUART_CONFIG_PARITYODD;
		inst->cfg_tx.flags |= HM2_PKTUART_CONFIG_PARITYODD;
		parity |= 1;
	}
	unsigned stopbits = 1;
	if(cc->cmd.flags & MBCCB_CMDF_STOPBITS2) {
		inst->cfg_rx.flags |= HM2_PKTUART_CONFIG_STOPBITS2;
		inst->cfg_tx.flags |= HM2_PKTUART_CONFIG_STOPBITS2;
		stopbits = 2;
	}

	// Re-examine the delay parameters
	if(cc->cmd.irxdelay)
		inst->cfg_rx.ifdelay = cc->cmd.irxdelay;
	else
		inst->cfg_rx.ifdelay = calc_ifdelay(inst, baudrate, parity, stopbits) - 1;
	if(cc->cmd.itxdelay)
		inst->cfg_tx.ifdelay = cc->cmd.itxdelay;
	else
		inst->cfg_tx.ifdelay = calc_ifdelay(inst, baudrate, parity, stopbits) + 1;

	inst->cfg_tx.drivedelay = cc->cmd.idrvdelay ? cc->cmd.idrvdelay : 1;

	// Expose to HAL
	inst->hal->baudrate = baudrate;
	inst->hal->parity   = parity;
	inst->hal->stopbits = stopbits;
	inst->hal->rxdelay  = inst->cfg_rx.ifdelay;
	inst->hal->txdelay  = inst->cfg_tx.ifdelay;
	inst->hal->drvdelay = inst->cfg_tx.drivedelay;
	// Redo the inter-character delay settings
	setup_icdelay(inst, baudrate, parity, stopbits, cc->cmd.iicdelay);

	// Queue a comms change
	if((r = hm2_pktuart_config(inst->uart, &inst->cfg_rx, &inst->cfg_tx, 1)) < 0) {
		MSG_ERR("%s: error: PktUART setup problem: error=%d\n", inst->name, r);
	}
	return r;
}

static void set_error(hm2_modbus_inst_t *inst, int errcode)
{
	if(handling_inits(inst)) {
		// No individual pins for init commands, use global
		*(inst->hal->fault) = 1;
		*(inst->hal->faultcmd) = inst->cmdidx;
		*(inst->hal->lasterror) = errcode;
		return;
	}
	hm2_modbus_cmd_t *cc = current_cmd(inst);
	if(++cc->errors >= MAX_ERRORS || cc->disabled) {
		cc->disabled = 1;
		*(inst->hal->cmds[inst->cmdidx].disabled)  = cc->disabled;
		*(inst->hal->cmds[inst->cmdidx].errorcode) = errcode;
	}
	*(inst->hal->cmds[inst->cmdidx].error) = cc->errors;
}

//
// Send a Modbus packet after attaching the CRC
//
static int send_modbus_pkt(hm2_modbus_inst_t *inst)
{
	hm2_modbus_cmd_t *cc = current_cmd(inst);

	// The +2 is because the buffer contains the header and the message data.
	// There must be room for the CRC. The buffer has the room allocated, now
	// it is checked to be available.
	if(cc->datalen >= MAX_MSG_LEN + 2) {
		MSG_ERR("%s: error: Data buffer overflow in send_modbus_pkt(), channel %d\n", inst->name, inst->cmdidx);
		return -EMSGSIZE;
	}

	// Append the CRC to the packet
	rtapi_u16 checksum = crc_modbus(cc->data, cc->datalen);
	cc->data[cc->datalen++] = checksum & 0xff;
	cc->data[cc->datalen++] = (checksum >> 8) & 0xff;

#ifdef DEBUG
	MSG_DBG("Sending cmd=%u to '%s' %i bytes", inst->cmdidx, inst->uart, cc->datalen);
	for(int i = 0; i < cc->datalen; i++) MSG_DBG(" 0x%02x", cc->data[i]);
	MSG_DBG("\n");
#endif

	rtapi_u8  frames = 1;
	rtapi_u16 fsizes[1] = { cc->datalen };
	return hm2_pktuart_send(inst->uart, cc->data, &frames, fsizes);
}

//
// Write invalid command data on the current channel to force re-send on next
// round
//
static inline void force_resend(hm2_modbus_inst_t *inst)
{
	hm2_modbus_cmd_t *cc = current_cmd(inst);
	cc->data[1] = 0xff;
	// If this was a 'once' command, make sure it gets resend asap in the next
	// round
	if(cc->cmd.cinterval == 0xffffffff)
		cc->interval = -1;
}

//
// Advance to the next command in the list
// A switch to the normal command list is made when it is the end of initlist.
// Return 0 on a normal switch. Return 1 when the list repeats.
//
static inline int next_command(hm2_modbus_inst_t *inst)
{
	// While running the init list
	if(handling_inits(inst)) {
		inst->cmdidx++;
		if(inst->cmdidx >= inst->ninit) {
			// The inits have have ended, move to commands
			inst->cmds = inst->_cmds;
			inst->cmdidx = 0;
			return 1;
		}
		return 0;
	}

	// Find the next non-disabled command
	do {
		inst->cmdidx++;

		// Reset the individual commands on request.
		// Using modulo index ensures we also reset index zero when we wrap.
		unsigned i = inst->cmdidx % inst->ncmds;
		bool b = !!*(inst->hal->cmds[i].reset);
		if(inst->cmds[i].prevreset != b) {
			inst->cmds[i].prevreset = !inst->cmds[i].prevreset;
			if(inst->cmds[i].prevreset) {
				inst->cmds[i].disabled = 0;
				inst->cmds[i].errors   = 0;
				*(inst->hal->cmds[i].disabled)  = 0;
				*(inst->hal->cmds[i].error)     = 0;
				*(inst->hal->cmds[i].errorcode) = 0;
			}
		}
	} while(inst->cmdidx < inst->ncmds && inst->cmds[inst->cmdidx].disabled);

	if(inst->cmdidx >= inst->ncmds) {
		// The cycle has ended, reset the command counter
		inst->cmdidx = 0;
		return 1;
	}
	return 0;
}

//
// Change state in the state machine
// Performs:
// - old state exit actions
// - change state
// - new state entry actions
//
static inline void set_state(hm2_modbus_inst_t *inst, int newstate)
{
#ifdef DEBUG_STATE
	if(newstate < 0 || newstate >= STATE_LAST || inst->state < 0 || inst->state > STATE_LAST) {
		MSG_ERR("%s: error: Invalid state detected in set_state() state=%d, newstate=%d\n", inst->name, inst->state, newstate);
		newstate = STATE_START;
	} else {
		MSG_DBG("set_state(): %s(%d) => %s(%d)\n", state_names[inst->state], inst->state, state_names[newstate], newstate);
	}
#endif

	// Exit actions
	switch(inst->state) {
	case STATE_START:
		// Exiting the START state means we are running a command and need to
		// setup the timeout. If this is no command, then the timeout setting
		// will be overridden locally in the code after the state change.
		inst->timeout = (rtapi_s64)inst->cmds[inst->cmdidx].cmd.ctimeout * 1000;
		break;
	default:
		break;
	}

	inst->state = newstate;	// Switch over


	// Entry actions
	switch(newstate) {
	case STATE_RESET_WAIT:
		// Reset wait uses a different timeout
		inst->timeout = 25000000;	// 25 milliseconds
		break;
	case STATE_START:
		if(*(inst->hal->suspend)) {
			inst->suspended = 1;
		}
		inst->ignoredata = 0;	// Re-enable data handling
		next_command(inst);
		break;
	default:
		break;
	}
}

//
// Perform a queued reset.
// The PktUART RX and TX are cleared and reset in the next period.
//
static inline void queue_reset(hm2_modbus_inst_t *inst)
{
	hm2_pktuart_queue_reset(inst->uart);
	set_state(inst, STATE_RESET_WAIT);
}

static void do_timeout(hm2_modbus_inst_t *inst)
{
	if(inst->timeout < 0) {
		if(hastimesout(current_cmd(inst))) {
			// This command is allowed to time out
			set_state(inst, STATE_START);
			return;
		}
		MSG_DBG("Timeout reset cmd=%u %s(%d)\n", inst->cmdidx, state_names[inst->state], inst->state);
		force_resend(inst);
		queue_reset(inst);
		*(inst->hal->lasterror) = ETIMEDOUT;
		*(inst->hal->fault) = 1;
		*(inst->hal->faultcmd) = inst->cmdidx;
		set_error(inst, ETIMEDOUT);
		set_state(inst, STATE_START);
	}
}

//
// Write flush sets all command data to the current values.
// Only Modbus write function are pre-calculated.
//
static void write_flush(hm2_modbus_inst_t *inst)
{
	unsigned oldidx = inst->cmdidx;	// Save the position
	for(unsigned i = 0; i < inst->ncmds; i++) {
		inst->cmdidx = i;	// This sets the current command index
		hm2_modbus_cmd_t *cc = current_cmd(inst);	// so this is the one we want
		switch(cc->cmd.func) {
		case MBCMD_W_COIL:
		case MBCMD_W_COILS:
		case MBCMD_W_REGISTER:
		case MBCMD_W_REGISTERS:
			break;	// Only write functions shall be built
		default:
			continue;
		}
		// Pre build the frame, even when disabled. When the WFLUSH flag is
		// set, then we still want timed out writes that are disabled, and
		// re-enabled due to a reset, to adhere to the flush setting when they
		// suddenly no longer timeout.
		if(!(cc->cmd.flags & MBCCB_CMDF_WFLUSH))	// Must have flag set
			continue;
		int r = build_data_frame(inst);
		if(r < 0) {
			// We cannot recover from data frames that cannot be build. They
			// would always result in the same error.
			if(!cc->disabled) {
				MSG_ERR("%s: error: Build data frame failed in write_flush for command %d, disabling\n", inst->name, inst->cmdidx);
				cc->disabled = 1;
			}
			set_error(inst, -r);
		}
	}
	inst->cmdidx = oldidx;	// Restore position
}

//
// The main process Modbus state-machine.
//
// It is essentially a synchronous communication machine that:
//   a) sends a command
//   b) wait for send complete
//   c) wait for answer
//   d) read and handle reply
// That way we can assure no wrong replies being attached to a sent command.
//
#ifdef DEBUG_STATE
static rtapi_u32 oldtx;
static rtapi_u32 oldrx;
static int oldst = -1;
#endif
static void process(void *arg, long period)
{
	hm2_modbus_inst_t *inst = (hm2_modbus_inst_t *)arg;

	if(inst->suspended) {
		if(!*(inst->hal->suspend)) {
			inst->suspended = 0;
			// When coming out of suspend, see if we want to suppress writes
			// because our local data will most likely not match the pin data.
			write_flush(inst);
		} else {
			return;
		}
	}

	int r;
	rtapi_u32 frsize;

	rtapi_u32 rxstatus = hm2_pktuart_get_rx_status(inst->uart);
	rtapi_u32 txstatus = hm2_pktuart_get_tx_status(inst->uart);

	if(inst->cmds == inst->_cmds) {
		// Only count timeout when running the command list
		for(unsigned i = 0; i < inst->ncmds; i++) {
			inst->cmds[i].interval -= period;	// Keep counting nanoseconds
		}
	}

	inst->timeout  -= period;	// Current timeout count

	// Re-enable all commands on rising edge of global reset pin
	bool b = !!*(inst->hal->reset);	// Make sure its value is bool-worthy
	if(inst->prevreset != b) {
		inst->prevreset = !inst->prevreset;
		if(inst->prevreset) {
			for(unsigned i = 0; i < inst->ncmds; i++) {
				inst->cmds[i].disabled = 0;
				inst->cmds[i].errors = 0;
				*(inst->hal->cmds[i].disabled)  = 0;
				*(inst->hal->cmds[i].error)     = 0;
				*(inst->hal->cmds[i].errorcode) = 0;
			}
		}
	}

	switch(inst->state) {
	case STATE_START:
#ifdef DEBUG_STATE
		{
			if(oldst != inst->state || oldrx != rxstatus || oldtx != txstatus) {
				MSG_DBG("START txstatus=0x%08x rxstatus=0x%08x\n", txstatus, rxstatus);
				oldrx = rxstatus;
				oldtx = txstatus;
				oldst = inst->state;
			}
		}
#endif
		// Check for received data
		// FIXME: This data would be out-of-band. It should never happen,
		//        unless there is more than one master on the bus.
		if(rxstatus & HM2_PKTUART_RXMODE_HASDATA) {
			inst->ignoredata = 1;
			set_state(inst, STATE_WAIT_FOR_DATA_FRAME);
			MSG_WARN("%s: warning: Out-of-band data received. Might not belong to current command %d\n", inst->name, inst->cmdidx);
			// Jump to the next state. There is no point in delaying queueing
			// the necessary read(s).
			goto wait_for_data_frame;
		}

		// Are we handling init commands?
		if(handling_inits(inst)) {
			// Yes, prepare and send
retry_next_init:
			hm2_modbus_cmd_t *cc = current_cmd(inst);
			if(0 == cc->cmd.func) {
				// Special meta command
				if(0 == cc->cmd.imetacmd) {			// This is a delay command
					// The timeout will be set in set_state()
					set_state(inst, STATE_WAIT_FOR_TIMEOUT);
				} else if(1 == cc->cmd.imetacmd) {	// This is a comms change
					send_comms_change(inst);
					next_command(inst);	// Advance because we stay in STATE_START
				} else {
					MSG_ERR("%s: error: init command %u uses unknown meta-command %u\n", inst->name, inst->cmdidx, cc->cmd.imetacmd);
					next_command(inst);
				}
				break;	// Meta commands will always need the next round
			} else {
				const rtapi_u8 *dptr = inst->dataptr + cc->cmd.cdataptr;
				memcpy(cc->data, dptr + 1, *dptr);	// Packet is prepared as data
				cc->datalen = *dptr;
				if((r = send_modbus_pkt(inst)) < 0) {	// This will attach CRC
					set_error(inst, -r);
					// Failure means we just move to the next
					MSG_ERR("%s: error: Failed to send init command %u\n", inst->name, inst->cmdidx);
					if(!next_command(inst))	// Until we wrap the command list
						goto retry_next_init;

					// We get to do commands next time around because
					// next_command() switches pointers on end of inits.
					break;
				}
				set_state(inst, STATE_WAIT_FOR_SEND_COMPLETE);
			}
			break;	// Init command sent
		}

		// Handling normal commands loop

		if(!inst->cmdidx) {
			// We are at the start of a cycle. If the first command message is
			// disabled, then try the next.
			if(inst->cmds[inst->cmdidx].disabled) {
				// If the next command is again the first, then we have no
				// messages we can send.
				if(next_command(inst)) {
					*(inst->hal->lasterror) = ENODATA;
					*(inst->hal->fault) = 1;
					*(inst->hal->faultcmd) = 0;
					break;
				}
			}
		}

		// No incoming data, we are free to send.
		// Cycle through the commands until one is found to have data that
		// needs to be sent.
		do {
			hm2_modbus_cmd_t *cc = current_cmd(inst);
			if(0 == cc->cmd.func) {
				// This is a delay command
				// The timeout will be set in set_state()
				set_state(inst, STATE_WAIT_FOR_TIMEOUT);
				break;
			}

			// The command may not yet be 'ripe', try next
			if(cc->interval >= 0)
				continue;

			// Reset the command's interval timer. The 'once' commands will
			// repeat every 292 years or so. If the command fails and needs a
			// resend, then the interval is patched (see force_resend()).
			// Reset to asap if the interval is shorter than the previous
			// experienced delay.
			if(cc->cmd.cinterval == 0xffffffff)
				cc->interval = RTAPI_INT64_MAX;
			else if((cc->interval += cc->cmd.cinterval * 1000) < 0)
				cc->interval = -1;

			if((r = build_data_frame(inst)) < 0) {
				// We cannot recover from data frames that cannot be build. The
				// next round would end in the same error. Therefore, disabling
				// the command is our only option.
				MSG_ERR("%s: error: Build data frame failed command %d, disabling\n", inst->name, inst->cmdidx);
				cc->disabled = 1;
				set_error(inst, -r);
				continue; // Just try next command
			}
			if(r || hasresend(cc)) { // if data has changed or forced
				if((r = send_modbus_pkt(inst)) < 0) {
					if(r == -EMSGSIZE) {
						// We cannot recover if the error occurred because the
						// buffer overflowed (no room for CRC). Resetting would
						// just give us an infinite stream of errors because it
						// would overflow in the next round too.
						// We disable this particular command so it will not
						// give problems again. It will probably cause havoc in
						// the application, but that should be OK.
						MSG_ERR("%s: error: Command %d disabled\n", inst->name, inst->cmdidx);
						cc->disabled = 1;
						set_error(inst, EMSGSIZE);
						continue; // Just try next command
					}
					MSG_ERR("%s: error: Send PDU failed (error %d) command %d, resetting\n", inst->name, r, inst->cmdidx);
					force_resend(inst);
					queue_reset(inst);
					break;
				} else {
					set_state(inst, STATE_WAIT_FOR_SEND_COMPLETE);
					break;
				}
			}
			// Nothing to do for this command, try next
		} while(!next_command(inst));	// Until we wrap the command list

		break;

	case STATE_WAIT_FOR_TIMEOUT:
#ifdef DEBUG_STATE
		{
			if(oldst != inst->state || oldrx != rxstatus || oldtx != txstatus) {
				MSG_DBG("WAIT_FOR_TIMEOUT RX 0x%08x TX 0x%08x\n", txstatus, rxstatus);
				oldrx = rxstatus;
				oldtx = txstatus;
				oldst = inst->state;
			}
		}
#endif
		// Check for received data
		// FIXME: This data would be out-of-band. It should never happen,
		//        unless there is more than one master on the bus.
		if(rxstatus & HM2_PKTUART_RXMODE_HASDATA) {
			inst->ignoredata = 1;
			set_state(inst, STATE_WAIT_FOR_DATA_FRAME);
			MSG_WARN("%s: warning: Out-of-band data received in WAIT_FOR_TIMEOUT in command %d\n", inst->name, inst->cmdidx);
			// Jump to the next state. There is no point in delaying queueing
			// the necessary read(s).
			goto wait_for_data_frame;
		}

		if(inst->timeout < 0) {
			// Timeout of delay, proceed to next command
			set_state(inst, STATE_START);
			*(inst->hal->fault) = 0;
		}
		break;

	case STATE_WAIT_FOR_SEND_COMPLETE:
#ifdef DEBUG_STATE
		{
			if(oldst != inst->state || oldrx != rxstatus || oldtx != txstatus) {
				MSG_DBG("WAIT_FOR_SEND_COMPLETE txstatus=0x%08x rxstatus=0x%08x\n", txstatus, rxstatus);
				oldrx = rxstatus;
				oldtx = txstatus;
				oldst = inst->state;
			}
		}
#endif
		if(!(txstatus & HM2_PKTUART_TXMODE_TXBUSY)) {
			const hm2_modbus_cmd_t *cc = current_cmd(inst);
			if((!cc->cmd.mbid && !hasbcanswer(cc)) || (cc->cmd.mbid && hasnoanswer(cc))) {
				// Broadcasts have no reply. Unless the equipment is badly
				// behaved and we have set flag to handle the case. Or,
				// non-broadcasts which are marked to produce no answer.
				set_state(inst, STATE_START);
			} else {
				set_state(inst, STATE_WAIT_FOR_DATA_FRAME);
				// No need to loop. Can test rxstatus immediately
				goto wait_for_data_frame;
			}
		} else {
			do_timeout(inst);
		}
		break;

	case STATE_WAIT_FOR_DATA_FRAME:
wait_for_data_frame:
#ifdef DEBUG_STATE
		{
			if(oldst != inst->state || oldrx != rxstatus || oldtx != txstatus) {
				MSG_DBG("WAIT_FOR_DATA_FRAME rxstatus=0x%08x\n", rxstatus);
				oldrx = rxstatus;
				oldtx = txstatus;
				oldst = inst->state;
			}
		}
#endif
		if(!HM2_PKTUART_RXMODE_NFRAMES_VAL(rxstatus)) {
			// No data yet, continue waiting.
			do_timeout(inst);
			break;
		}
		// Got frame(s) in receive buffer
		inst->frameidx = 0;
		memset(inst->fsizes, 0, sizeof(inst->fsizes));
		// Get the frame size(s)
		if((r = hm2_pktuart_queue_get_frame_sizes(inst->uart, inst->fsizes)) < 0)
			MSG_ERR("%s: error: hm2_pktuart_queue_get_frame_sizes() returned an error: %d\n", inst->name, r);
		set_state(inst, STATE_FETCH_DATA);
		break;

	case STATE_FETCH_DATA:
	case STATE_FETCH_MORE_DATA:
fetch_more_data:
#ifdef DEBUG_STATE
		MSG_DBG("FETCH_(MORE_)DATA Index %u Frames 0x%04x 0x%04x 0x%04x 0x%04x\n",
			inst->frameidx, inst->fsizes[0], inst->fsizes[1], inst->fsizes[2], inst->fsizes[3]);
#endif
		frsize = inst->fsizes[inst->frameidx];
		if(frsize & (HM2_PKTUART_RCR_ERROROVERRUN | HM2_PKTUART_RCR_ERRORSTARTBIT | HM2_PKTUART_RCR_ERRORPARITY)) {
			const char *eor = (frsize & HM2_PKTUART_RCR_ERROROVERRUN)  ? " OVERRUN"  : "";
			const char *esb = (frsize & HM2_PKTUART_RCR_ERRORSTARTBIT) ? " STARTBIT" : "";
			const char *epa = (frsize & HM2_PKTUART_RCR_ERRORPARITY)   ? " PARITY"   : "";
			MSG_WARN("%s: warning: reply to command %u had an error%s%s%s set in fsize (0x%04x), dropping\n",
					inst->name, inst->cmdidx, eor, esb, epa,
					frsize & (HM2_PKTUART_RCR_ERROROVERRUN | HM2_PKTUART_RCR_ERRORSTARTBIT | HM2_PKTUART_RCR_ERRORPARITY));
			set_error(inst, EIO);
			force_resend(inst);
			queue_reset(inst);
		}
		if(HM2_PKTUART_RCR_NBYTES_VAL(frsize) > MAX_PKT_LEN) { // indicates an overrun
			MSG_WARN("%s: warning: reply to command %u had packet size overrun (%u > %u), dropping\n",
					inst->name, inst->cmdidx,
					HM2_PKTUART_RCR_NBYTES_VAL(frsize), MAX_PKT_LEN);
			set_error(inst, EOVERFLOW);
			force_resend(inst);
			queue_reset(inst);
			break;
		}
		if(inst->maxicharbits && HM2_PKTUART_RCR_ICHARBITS_VAL(frsize) > inst->maxicharbits) {
			MSG_WARN("%s: warning: reply to command %u had too long inter-character delay (%u > %u), dropping\n",
					inst->name, inst->cmdidx,
					HM2_PKTUART_RCR_ICHARBITS_VAL(frsize), inst->maxicharbits);
			set_error(inst, ENOMSG);
			force_resend(inst);
			break;
		}
		inst->rxdata[0] = 0;	// This will fail the parse packet if the read did not resolve
		r = hm2_pktuart_queue_read_data(inst->uart, inst->rxdata, HM2_PKTUART_RCR_NBYTES_VAL(frsize));
		if(r < 0)
			MSG_ERR("%s: error: hm2_pktuart_queue_read_data() returned an error: %d\n", inst->name, r);	// What to do...
		// The above queued read doesn't resolve until the next cycle.
		set_state(inst, STATE_HANDLE_DATA);
		break;

	case STATE_HANDLE_DATA:
#ifdef DEBUG_STATE
		MSG_DBG("HANDLE_DATA\n");
#endif
		if(!inst->ignoredata) {
			if(parse_data_frame(inst) >= 0) {
				current_cmd(inst)->errors = 0;
			}
		}
		if(inst->frameidx < 16-1 && HM2_PKTUART_RCR_NBYTES_VAL(inst->fsizes[++(inst->frameidx)]) > 0) {
			// FIXME: We should never get multiple replies. Only one
			// outstanding packet should be waiting for a reply.
			MSG_WARN("%s: warning: Multiple data packets as reply? Maybe out-of-band? Command %d\n", inst->name, inst->cmdidx);
			inst->ignoredata = 1;
			set_state(inst, STATE_FETCH_MORE_DATA);
			// We do not need to wait for the next round. We can stuff a read
			// immediately when we know that there is more data to fetch.
			// Anyway, this scenario should not happen.
			goto fetch_more_data;
		} else {
			// We have received all frames from which we got frame size. If
			// more (new) data is pending, then that will also be out-of-band.
			// We can go to the start state and sort it out there.
			set_state(inst, STATE_START);
			*(inst->hal->fault) = 0;
		}
		break;

	case STATE_RESET_WAIT:
		if(inst->timeout < 0)
			set_state(inst, STATE_START);
		break;

	default:
		MSG_ERR("%s: error: Unknown state (%d) in process(), setting START state\n", inst->name, inst->state);
		*(inst->hal->fault) = 1;
		*(inst->hal->lasterror) = EINVAL;
		*(inst->hal->faultcmd) = inst->cmdidx;
		set_state(inst, STATE_START);
		break;
	}
}

//
// Build a frame one byte at a time.
// The ch_append8() returns:
// *  0        On success
// * -EMSGSIZE On failure
// * +1        On data change
//
static inline int ch_append8(hm2_modbus_cmd_t *cc, rtapi_u8 v)
{
	int r = 0;
	// The +2 is from the header (address and command), already written to the
	// buffer. The CRC is not counted while building the frame. The data buffer
	// has four extra bytes allocated to store header and CRC.
	if(cc->datalen >= MAX_MSG_LEN + 2)
		return -EMSGSIZE;
	if(cc->data[cc->datalen] != v) r = 1; // flag data changed
	cc->data[cc->datalen] = v;
	cc->datalen++;
	return r;
}

// Abort when -1 is returned and continue on 0 and +1
// Positive returns are summed. Negative drops out of the function.
#define CHK_RV(x)	do { \
						int rv = (x); \
						if(rv < 0) \
							return rv; \
						r += rv; \
					} while(0)
static inline int ch_append16_sw(hm2_modbus_cmd_t *cc, rtapi_u16 v, bool reverse)
{
	int r = 0;
	if(reverse) {
		CHK_RV(ch_append8(cc, (rtapi_u8)(v & 0xFF)));
		CHK_RV(ch_append8(cc, (rtapi_u8)(v >> 8)));
	} else {
		CHK_RV(ch_append8(cc, (rtapi_u8)(v >> 8)));
		CHK_RV(ch_append8(cc, (rtapi_u8)(v & 0xFF)));
	}
	return r;
}

static inline int ch_append16(hm2_modbus_cmd_t *cc, rtapi_u16 v)
{
	return ch_append16_sw(cc, v, false);
}

//
// The byteswaps array indices MUST follow the MBT_xx, MBT_xxxx and
// MBT_xxxxxxxx endian defines.
//
typedef rtapi_u8 byteswaps_t[8];
static const byteswaps_t byteswaps[2+4+8] = {
#if RTAPI_BIG_ENDIAN
	// 2-byte/16-bit
	{0, 1, 0, 1, 0, 1, 0, 1},
	{1, 0, 1, 0, 1, 0, 1, 0},
	// 4-byte/32-bit
	{0, 1, 2, 3, 0, 1, 2, 3},
	{1, 0, 3, 2, 1, 0, 3, 2},
	{2, 3, 0, 1, 2, 3, 0, 1},
	{3, 2, 1, 0, 3, 2, 1, 0},
	// 8-byte/64-bit
	{0, 1, 2, 3, 4, 5, 6, 7},
	{1, 0, 3, 2, 5, 4, 7, 6},
	{2, 3, 0, 1, 6, 7, 4, 5},
	{3, 2, 1, 0, 7, 6, 5, 4},
	{4, 5, 6, 7, 0, 1, 2, 3},
	{5, 4, 7, 6, 1, 0, 3, 2},
	{6, 7, 4, 5, 2, 3, 0, 1},
	{7, 6, 5, 4, 3, 2, 1, 0},
#else
	// 2-byte/16-bit
	{1, 0, 1, 0, 1, 0, 1, 0},
	{0, 1, 0, 1, 0, 1, 0, 1},
	// 4-byte/32-bit
	{3, 2, 1, 0, 3, 2, 1, 0},
	{2, 3, 0, 1, 2, 3, 0, 1},
	{1, 0, 3, 2, 1, 0, 3, 2},
	{0, 1, 2, 3, 0, 1, 2, 3},
	// 8-byte/64-bit
	{7, 6, 5, 4, 3, 2, 1, 0},
	{6, 7, 4, 5, 2, 3, 0, 1},
	{5, 4, 7, 6, 1, 0, 3, 2},
	{4, 5, 6, 7, 0, 1, 2, 3},
	{3, 2, 1, 0, 7, 6, 5, 4},
	{2, 3, 0, 1, 6, 7, 4, 5},
	{1, 0, 3, 2, 5, 4, 7, 6},
	{0, 1, 2, 3, 4, 5, 6, 7},
#endif
};

static inline int ch_append32(hm2_modbus_cmd_t *cc, const mb_types32_u *v, unsigned tidx)
{
	int r = 0;
	unsigned idx = mtypeformat(cc->typeptr[tidx].mtype);
	if(idx < MBT_ABCD || idx > MBT_DCBA)
		return -EINVAL;
	const rtapi_u8 *bs = byteswaps[idx];
	for(unsigned i = 0; i < 4; i++)
		CHK_RV(ch_append8(cc, v->b[*bs++]));
	return r;
}

static inline int ch_append64(hm2_modbus_cmd_t *cc, const mb_types64_u *v, unsigned tidx)
{
	int r = 0;
	unsigned idx = mtypeformat(cc->typeptr[tidx].mtype);
	if(idx < MBT_ABCDEFGH || idx > MBT_HGFEDCBA)
		return -EINVAL;
	const rtapi_u8 *bs = byteswaps[idx];
	for(unsigned i = 0; i < 8; i++)
		CHK_RV(ch_append8(cc, v->b[*bs++]));
	return r;
}

static inline int ch_init(hm2_modbus_cmd_t *cc)
{
	int r = 0;
	cc->datalen = 0;
	CHK_RV(ch_append8(cc, cc->cmd.mbid));
	CHK_RV(ch_append8(cc, cc->cmd.func));
	return r;
}

static int map_u(hm2_modbus_cmd_t *cc, rtapi_u64 v, unsigned tidx)
{
	int r = 0;
	mb_types32_u v32;
	mb_types64_u v64;
	unsigned fmt = mtypeformat(cc->typeptr[tidx].mtype);
	switch(fmt) {
	case MBT_AB:
	case MBT_BA:
		if(haspinclamp(&cc->typeptr[tidx]) && v > RTAPI_UINT16_MAX) v = RTAPI_UINT16_MAX;
		CHK_RV(ch_append16_sw(cc, (rtapi_u16)v, fmt == MBT_BA));
		break;

	case MBT_ABCD:
	case MBT_BADC:
	case MBT_CDAB:
	case MBT_DCBA:
		if(haspinclamp(&cc->typeptr[tidx]) && v > RTAPI_UINT32_MAX) v = RTAPI_UINT32_MAX;
		v32.u = v;
		CHK_RV(ch_append32(cc, &v32, tidx));
		break;

	case MBT_ABCDEFGH:
	case MBT_BADCFEHG:
	case MBT_CDABGHEF:
	case MBT_DCBAHGFE:
	case MBT_EFGHABCD:
	case MBT_FEHGBADC:
	case MBT_GHEFCDAB:
	case MBT_HGFEDCBA:
		v64.u = v;
		CHK_RV(ch_append64(cc, &v64, tidx));
		break;
	}
	return r;
}

static int map_s(hm2_modbus_cmd_t *cc, rtapi_s64 v, unsigned tidx)
{
	int r = 0;
	mb_types32_u v32;
	mb_types64_u v64;
	unsigned fmt = mtypeformat(cc->typeptr[tidx].mtype);
	switch(fmt) {
	case MBT_AB:
	case MBT_BA:
		if(haspinclamp(&cc->typeptr[tidx]) && v > RTAPI_INT16_MAX) v = RTAPI_INT16_MAX;
		if(haspinclamp(&cc->typeptr[tidx]) && v < RTAPI_INT16_MIN) v = RTAPI_INT16_MIN;
		CHK_RV(ch_append16_sw(cc, (rtapi_s16)v, fmt == MBT_BA));
		break;

	case MBT_ABCD:
	case MBT_BADC:
	case MBT_CDAB:
	case MBT_DCBA:
		if(haspinclamp(&cc->typeptr[tidx]) && v > RTAPI_INT32_MAX) v = RTAPI_INT32_MAX;
		if(haspinclamp(&cc->typeptr[tidx]) && v < RTAPI_INT32_MIN) v = RTAPI_INT32_MIN;
		v32.s = (rtapi_s32)v;
		CHK_RV(ch_append32(cc, &v32, tidx));
		break;

	case MBT_ABCDEFGH:
	case MBT_BADCFEHG:
	case MBT_CDABGHEF:
	case MBT_DCBAHGFE:
	case MBT_EFGHABCD:
	case MBT_FEHGBADC:
	case MBT_GHEFCDAB:
	case MBT_HGFEDCBA:
		v64.s = v;
		CHK_RV(ch_append64(cc, &v64, tidx));
		break;
	}
	return r;
}

static int map_f(hm2_modbus_cmd_t *cc, double v, unsigned tidx)
{
	int r = 0;
	mb_types32_u v32;
	mb_types64_u v64;
	rtapi_u16 w;
	unsigned fmt = mtypeformat(cc->typeptr[tidx].mtype);
	switch(fmt) {
	case MBT_AB:
	case MBT_BA:
		v64.f = v;
		// Demote double to half. Don't rely on compiler _Float16, do some
		// bit-fiddling to make it fit.
		// _Float16: sign(1) exponent( 5) mantissa(10) bias=15
		// (mask     0x8000  0x7c00       0x03ff)
		// double:   sign(1) exponent(11) mantissa(52) bias=1023
		w = v < 0 ? 0x8000 : 0;				// sign (b15)
		if(!(v64.u & 0x7ff0000000000000ul)) {
			// Zero or subnormal
			// Only copy the high mantissa bits
			w |= (v64.u >> (52 - 10)) & 0x03ff;
		} else if((v64.u & 0x7ff0000000000000ul) == 0x7ff0000000000000ul) {
			// Inf or NaN
			// Copy the lower mantissa bits and exponent all ones
			w |= 0x7c00 | (v64.u & 0x03ff);
		} else {
			if(v > +65504.0) v = +65504.0;	// Clamp instead of +/-inf
			if(v < -65504.0) v = -65504.0;
			w |= ((((v64.u >> 52) & 0x7ff) - 1023 + 15) << 10) & 0x7c00;	// exponent (b14..10)
			w |= (v64.u >> (52 - 10)) & 0x03ff;	// mantissa (b9..0)
		}
		CHK_RV(ch_append16_sw(cc, w, fmt == MBT_BA));
		break;

	case MBT_ABCD:
	case MBT_BADC:
	case MBT_CDAB:
	case MBT_DCBA:
		if(haspinclamp(&cc->typeptr[tidx]) && v > +FLT_MAX) v = +FLT_MAX;
		if(haspinclamp(&cc->typeptr[tidx]) && v < -FLT_MAX) v = -FLT_MAX;
		v32.f = (float)v;
		CHK_RV(ch_append32(cc, &v32, tidx));
		break;

	case MBT_ABCDEFGH:
	case MBT_BADCFEHG:
	case MBT_CDABGHEF:
	case MBT_DCBAHGFE:
	case MBT_EFGHABCD:
	case MBT_FEHGBADC:
	case MBT_GHEFCDAB:
	case MBT_HGFEDCBA:
		v64.f = v;
		CHK_RV(ch_append64(cc, &v64, tidx));
		break;
	}
	return r;
}

static inline rtapi_s64 map_us(rtapi_u64 v)
{
	return (rtapi_s64)(v & 0x7ffffffffffffffful);
}

static inline double map_uf(rtapi_u64 v)
{
	return (double)v;
}

static inline rtapi_u64 map_su(rtapi_s64 v)
{
	return v < 0 ? 0 : (rtapi_u64)v;
}

static inline double map_sf(rtapi_s64 v)
{
	return (double)v;
}

static inline rtapi_s64 map_fs(double v)
{
	if(v > (double)RTAPI_INT64_MAX) return RTAPI_INT64_MAX;
	if(v < (double)RTAPI_INT64_MIN) return RTAPI_INT64_MIN;
	return (rtapi_s64)v;
}

static inline rtapi_u64 map_fu(double v)
{
	if(v < 0.0) return 0;
	if(v > (double)RTAPI_UINT64_MAX) return RTAPI_UINT64_MAX;
	return (rtapi_u64)v;
}

static int build_data_frame(hm2_modbus_inst_t *inst)
{
	hm2_modbus_cmd_t *cc = current_cmd(inst);
	hm2_modbus_hal_t *hal = inst->hal;
	rtapi_u8 acc = 0;
	int r = 0;
	int p = cc->pinref;
	mb_types64_u val64;
	unsigned regpos = 0;

	MSG_DBG("Building PDU cmd=%u 0x%02x 0x%04x start pin %i\n", inst->cmdidx, cc->cmd.func, cc->cmd.caddr, p);

	if((r = ch_init(cc)) < 0) {
		MSG_ERR("%s: error: Failed to initialize frame, command %d\n", inst->name, inst->cmdidx);
		return r;
	}

	switch(cc->cmd.func) {
	case MBCMD_R_COILS: // Read coils
	case MBCMD_R_INPUTS: // Read discrete inputs
		r += 1; // trigger a read PDU every time
		CHK_RV(ch_append16(cc, cc->cmd.caddr));
		CHK_RV(ch_append16(cc, cc->cmd.cpincnt));
		break;
	case MBCMD_R_REGISTERS: // Read holding registers
	case MBCMD_R_INPUTREGS: // Read input registers
		r += 1; // trigger a read PDU every time
		CHK_RV(ch_append16(cc, cc->cmd.caddr));
		// Compound values are one, two or four registers large
		CHK_RV(ch_append16(cc, cc->cmd.cregcnt));
		break;
	case MBCMD_W_COIL: // Write single coil
		CHK_RV(ch_append16(cc, cc->cmd.caddr));
		CHK_RV(ch_append16(cc, hal->pins[p].pin->b ? 0xFF00 : 0x0000));
		break;
	case MBCMD_W_REGISTER: // Write single register
		// The target mtype can only be MBT_AB or MBT_BA (single reg write)
		CHK_RV(ch_append16(cc, cc->cmd.caddr));
		switch(cc->typeptr[0].htype) {
		case HAL_BIT:
			map_u(cc, hal->pins[p].pin->b ? 1 : 0, 0);
			break;
		case HAL_U32:
			switch(mtypetype(cc->typeptr[0].mtype)) {
			case MBT_U: map_u(cc, hal->pins[p].pin->u, 0); break;
			case MBT_S: map_s(cc, map_us(hal->pins[p].pin->u), 0); break;
			case MBT_F: map_f(cc, map_uf(hal->pins[p].pin->u), 0); break;
			}
			break;
		case HAL_U64:
			switch(mtypetype(cc->typeptr[0].mtype)) {
			case MBT_U: map_u(cc, hal->pins[p].pin->lu, 0); break;
			case MBT_S: map_s(cc, map_us(hal->pins[p].pin->lu), 0); break;
			case MBT_F: map_f(cc, map_uf(hal->pins[p].pin->lu), 0); break;
			}
			break;
		case HAL_S32:
			if(!haspinscale(&cc->typeptr[0])) {
				switch(mtypetype(cc->typeptr[0].mtype)) {
				case MBT_U: map_u(cc, map_su(hal->pins[p].pin->s), 0); break;
				case MBT_S: map_s(cc, hal->pins[p].pin->s, 0); break;
				case MBT_F: map_f(cc, map_sf(hal->pins[p].pin->s), 0); break;
				}
			} else {
				val64.f = (real_t)((rtapi_s64)hal->pins[p].pin->s - hal->pins[p].offset->s) * *(hal->pins[p].scale);
				switch(mtypetype(cc->typeptr[0].mtype)) {
				case MBT_U: map_u(cc, map_fu(val64.f), 0); break;
				case MBT_S: map_s(cc, map_fs(val64.f), 0); break;
				case MBT_F: map_f(cc, val64.f, 0); break;
				}
			}
			break;
		case HAL_S64:
			if(!haspinscale(&cc->typeptr[0])) {
				switch(mtypetype(cc->typeptr[0].mtype)) {
				case MBT_U: map_u(cc, map_su(hal->pins[p].pin->ls), 0); break;
				case MBT_S: map_s(cc, hal->pins[p].pin->ls, 0); break;
				case MBT_F: map_f(cc, map_sf(hal->pins[p].pin->ls), 0); break;
				}
			} else {
				val64.f = (real_t)(hal->pins[p].pin->ls - hal->pins[p].offset->ls) * *(hal->pins[p].scale);
				switch(mtypetype(cc->typeptr[0].mtype)) {
				case MBT_U: map_u(cc, map_fu(val64.f), 0); break;
				case MBT_S: map_s(cc, map_fs(val64.f), 0); break;
				case MBT_F: map_f(cc, val64.f, 0); break;
				}
			}
			break;
		case HAL_FLOAT:
			if(!haspinscale(&cc->typeptr[0])) {
				switch(mtypetype(cc->typeptr[0].mtype)) {
				case MBT_U: map_u(cc, map_fu(hal->pins[p].pin->f), 0); break;
				case MBT_S: map_s(cc, map_fs(hal->pins[p].pin->f), 0); break;
				case MBT_F: map_f(cc, hal->pins[p].pin->f, 0); break;
				}
			} else {
				val64.f = (hal->pins[p].pin->f - hal->pins[p].offset->f) * *(hal->pins[p].scale);
				switch(mtypetype(cc->typeptr[0].mtype)) {
				case MBT_U: map_u(cc, map_fu(val64.f), 0); break;
				case MBT_S: map_s(cc, map_fs(val64.f), 0); break;
				case MBT_F: map_f(cc, val64.f, 0); break;
				}
			}
			break;
		default:
			// Oops...
			break;
		}
		break;
	case MBCMD_W_COILS: // Write multiple coils
		CHK_RV(ch_append16(cc, cc->cmd.caddr));
		CHK_RV(ch_append16(cc, cc->cmd.cpincnt));
		CHK_RV(ch_append8(cc, (cc->cmd.cpincnt + 7) / 8));
		for(unsigned i = 0; i < cc->cmd.cpincnt; i++) {
			if(hal->pins[p++].pin->b)
				acc |= 1u << (i % 8);
			if((i % 8) == 7 || i == cc->cmd.cpincnt - 1u) { // time for the next byte
				CHK_RV(ch_append8(cc, acc));
				acc = 0;
			}
		}
		break;
	case MBCMD_W_REGISTERS: // write multiple holding registers
		CHK_RV(ch_append16(cc, cc->cmd.caddr));
		// Compound values are one, two or four registers large
		CHK_RV(ch_append16(cc, cc->cmd.cregcnt));
		CHK_RV(ch_append8(cc,  cc->cmd.cregcnt * 2));
		for(unsigned i = 0; i < cc->cmd.cpincnt; i++) {
			// Stuff empty space with zeros.
			// The device must allow writes at the address(es).
			while(regpos != cc->typeptr[i].regofs) {
				CHK_RV(ch_append16(cc, 0));
				regpos += 2;
			}
			switch(cc->typeptr[i].htype) {
			case HAL_BIT:
				map_u(cc, hal->pins[p].pin->b ? 1 : 0, i);
				break;
			case HAL_U32:
				switch(mtypetype(cc->typeptr[i].mtype)) {
				case MBT_U: map_u(cc, hal->pins[p].pin->u, i); break;
				case MBT_S: map_s(cc, map_us(hal->pins[p].pin->u), i); break;
				case MBT_F: map_f(cc, map_uf(hal->pins[p].pin->u), i); break;
				}
				break;
			case HAL_U64:
				switch(mtypetype(cc->typeptr[i].mtype)) {
				case MBT_U: map_u(cc, hal->pins[p].pin->lu, i); break;
				case MBT_S: map_s(cc, map_us(hal->pins[p].pin->lu), i); break;
				case MBT_F: map_f(cc, map_uf(hal->pins[p].pin->lu), i); break;
				}
				break;
			case HAL_S32:
				if(!haspinscale(&cc->typeptr[i])) {
					switch(mtypetype(cc->typeptr[i].mtype)) {
					case MBT_U: map_u(cc, map_su(hal->pins[p].pin->s), i); break;
					case MBT_S: map_s(cc, hal->pins[p].pin->s, i); break;
					case MBT_F: map_f(cc, map_sf(hal->pins[p].pin->s), i); break;
					}
				} else {
					val64.f = (real_t)((rtapi_s64)hal->pins[p].pin->s - hal->pins[p].offset->s) * *(hal->pins[p].scale);
					switch(mtypetype(cc->typeptr[i].mtype)) {
					case MBT_U: map_u(cc, map_fu(val64.f), i); break;
					case MBT_S: map_s(cc, map_fs(val64.f), i); break;
					case MBT_F: map_f(cc, val64.f, i); break;
					}
				}
				break;
			case HAL_S64:
				if(!haspinscale(&cc->typeptr[i])) {
					switch(mtypetype(cc->typeptr[i].mtype)) {
					case MBT_U: map_u(cc, map_su(hal->pins[p].pin->ls), i); break;
					case MBT_S: map_s(cc, hal->pins[p].pin->ls, i); break;
					case MBT_F: map_f(cc, map_sf(hal->pins[p].pin->ls), i); break;
					}
				} else {
					val64.f = (real_t)(hal->pins[p].pin->ls - hal->pins[p].offset->ls) * *(hal->pins[p].scale);
					switch(mtypetype(cc->typeptr[i].mtype)) {
					case MBT_U: map_u(cc, map_fu(val64.f), i); break;
					case MBT_S: map_s(cc, map_fs(val64.f), i); break;
					case MBT_F: map_f(cc, val64.f, i); break;
					}
				}
				break;
			case HAL_FLOAT:
				if(!haspinscale(&cc->typeptr[i])) {
					switch(mtypetype(cc->typeptr[i].mtype)) {
					case MBT_U: map_u(cc, map_fu(hal->pins[p].pin->f), i); break;
					case MBT_S: map_s(cc, map_fs(hal->pins[p].pin->f), i); break;
					case MBT_F: map_f(cc, hal->pins[p].pin->f, i); break;
					}
				} else {
					val64.f = (hal->pins[p].pin->f - hal->pins[p].offset->f) * *(hal->pins[p].scale);
					switch(mtypetype(cc->typeptr[i].mtype)) {
					case MBT_U: map_u(cc, map_fu(val64.f), i); break;
					case MBT_S: map_s(cc, map_fs(val64.f), i); break;
					case MBT_F: map_f(cc, val64.f, i); break;
					}
				}
				break;
			default:
				// Oops...
				break;
			}
			p++;
		}
		break;
	default:
		MSG_ERR("%s: error: Unknown Modbus function %u, channel %d\n", inst->name, cc->cmd.func, inst->cmdidx);
		return -EINVAL;
	}
	return r;
}
#undef CHK_RV

static int test_bytecount(const hm2_modbus_inst_t *inst, const rtapi_u8 *bytes, unsigned pkt_len, unsigned mini, unsigned maxi)
{
	if(bytes[2] < mini || bytes[2] > maxi) {
		MSG_ERR("%s: error: Invalid byte count %u in received PDU not in [%u, %u], cmd %u\n", inst->name, bytes[2], mini, maxi, bytes[1]);
		return -1;
	}
	// Byte count cannot be odd with words.
	if(mini > 1 && 0 != bytes[2] % mini) {
		MSG_ERR("%s: error: Invalid odd byte count %u in received PDU, expected mod %u, cmd %u\n", inst->name, bytes[2], mini, bytes[1]);
		return -1;
	}
	// The byte count must match the PDU's content size.
	// The -5 is subtracting the mbid, function code, CRC and the byte count byte.
	if(bytes[2] > pkt_len - 5) {
		MSG_ERR("%s: error: Byte count in received PDU too large (%u > %u), cmd %u\n", inst->name, bytes[2], pkt_len - 4, bytes[1]);
		return -1;
	}
	if(bytes[2] < pkt_len - 5) {
		MSG_ERR("%s: error: Byte count in received PDU too small (%u < %u), cmd %u\n", inst->name, bytes[2], pkt_len - 4, bytes[1]);
		return -1;
	}
	return 0;
}

static inline mb_types32_u get32(rtapi_u8 *b, unsigned mtype)
{
	mb_types32_u v;
	unsigned idx = mtypeformat(mtype);
	if(idx < MBT_ABCD || idx > MBT_DCBA)
		idx = MBT_ABCD;
	const rtapi_u8 *bs = byteswaps[idx];
	for(unsigned i = 0; i < 4; i++)
		v.b[i] = b[*bs++];
	return v;
}

static inline mb_types64_u get64(rtapi_u8 *b, unsigned mtype)
{
	mb_types64_u v;
	unsigned idx = mtypeformat(mtype);
	if(idx < MBT_ABCDEFGH || idx > MBT_HGFEDCBA)
		idx = MBT_ABCDEFGH;
	const rtapi_u8 *bs = byteswaps[idx];
	for(unsigned i = 0; i < 8; i++)
		v.b[i] = b[*bs++];
	return v;
}

static inline rtapi_u32 unmap32_uu(const hm2_modbus_cmd_t *cc, rtapi_u64 v, unsigned tidx)
{
	if(haspinclamp(&cc->typeptr[tidx]) && v > RTAPI_UINT32_MAX)
		return RTAPI_UINT32_MAX;
	return (rtapi_u32)v;
}

static inline rtapi_u32 unmap32_us(const hm2_modbus_cmd_t *cc, rtapi_s64 v, unsigned tidx)
{
	if(haspinclamp(&cc->typeptr[tidx])) {
		if(v > RTAPI_INT32_MAX) return RTAPI_INT32_MAX;
		if(v < 0) return 0;
	}
	return (rtapi_u32)v;
}

static inline rtapi_u32 unmap32_uf(const hm2_modbus_cmd_t *cc, double v, unsigned tidx)
{
	if(haspinclamp(&cc->typeptr[tidx])) {
		if(v > (double)RTAPI_INT32_MAX) return RTAPI_INT32_MAX;
		if(v < 0.0) return 0;
	}
	return (rtapi_u32)v;
}

static inline rtapi_s32 unmap32_su(const hm2_modbus_cmd_t *cc, rtapi_u64 v, unsigned tidx)
{
	if(haspinclamp(&cc->typeptr[tidx]) && v > (rtapi_u64)RTAPI_INT32_MAX)
		return RTAPI_INT32_MAX;
	return (rtapi_s32)v;
}

static inline rtapi_s32 unmap32_ss(const hm2_modbus_cmd_t *cc, rtapi_s64 v, unsigned tidx)
{
	if(haspinclamp(&cc->typeptr[tidx])) {
		if(v > (rtapi_s64)RTAPI_INT32_MAX) return RTAPI_INT32_MAX;
		if(v < (rtapi_s64)RTAPI_INT32_MIN) return RTAPI_INT32_MIN;
	}
	return (rtapi_s32)v;
}

static inline rtapi_s32 unmap32_sf(const hm2_modbus_cmd_t *cc, double v, unsigned tidx)
{
	if(haspinclamp(&cc->typeptr[tidx])) {
		if(v > (double)RTAPI_INT32_MAX) return RTAPI_INT32_MAX;
		if(v < (double)RTAPI_INT32_MIN) return RTAPI_INT32_MIN;
	}
	return (rtapi_s32)v;
}

static inline rtapi_u64 unmap64_us(const hm2_modbus_cmd_t *cc, rtapi_s64 v, unsigned tidx)
{
	if(haspinclamp(&cc->typeptr[tidx]) && v < 0)
		return 0;
	return (rtapi_u64)v;
}

static inline rtapi_u64 unmap64_uf(const hm2_modbus_cmd_t *cc, double v, unsigned tidx)
{
	if(haspinclamp(&cc->typeptr[tidx])) {
		if(v > (double)RTAPI_INT64_MAX) return RTAPI_INT64_MAX;
		if(v < 0.0) return 0;
	}
	return (rtapi_u64)v;
}

static inline rtapi_s64 unmap64_su(const hm2_modbus_cmd_t *cc, rtapi_u64 v, unsigned tidx)
{
	if(haspinclamp(&cc->typeptr[tidx]) && v > RTAPI_INT64_MAX)
		return RTAPI_INT64_MAX;
	return (rtapi_s64)v;
}

static inline rtapi_s64 unmap64_sf(const hm2_modbus_cmd_t *cc, double v, unsigned tidx)
{
	if(haspinclamp(&cc->typeptr[tidx])) {
		if(v > (double)RTAPI_INT64_MAX) return RTAPI_INT64_MAX;
		if(v < (double)RTAPI_INT64_MIN) return RTAPI_INT64_MIN;
	}
	return (rtapi_s64)v;
}

static int parse_data_frame(hm2_modbus_inst_t *inst)
{
	hm2_modbus_cmd_t *cc = current_cmd(inst);
	hm2_modbus_hal_t *hal = inst->hal;
	rtapi_u32 *data = inst->rxdata;
	unsigned rxcount = HM2_PKTUART_RCR_NBYTES_VAL(inst->fsizes[inst->frameidx]);
	int w = 0;
	int b = 0;
	int p = cc->pinref;

	rtapi_u8 bytes[MAX_PKT_LEN] = {};
	rtapi_u16 checksum;
	mb_types32_u val32 = {};
	mb_types64_u val64;

	if(rxcount > sizeof(bytes)) {
		MSG_ERR("%s: error: Received PDU larger than buffer (%u > %zu), truncating\n", inst->name, rxcount, sizeof(bytes));
		set_error(inst, EFBIG);
		rxcount = sizeof(bytes);
	}

	// Worst case PDU size:
	// 1 byte slave address
	// 1 byte command
	// 1 byte exception code
	// 2 bytes CRC
	if(rxcount < 5) {
		MSG_ERR("%s: error: Received PDU too small, size=%u\n", inst->name, rxcount);
		set_error(inst, ERANGE);
		force_resend(inst);
		return -1;
	}

	MSG_DBG("Return PDU cmd=%u is", inst->cmdidx);
	for(unsigned i = 0; i < rxcount; i++) {
		bytes[i] = (data[w] >> b) & 0xFF;
		MSG_DBG(" %02x", bytes[i]);
		if((b += 8) >= 32) { b = 0; w++; }
	}
	MSG_DBG("\n");

	if(bytes[0] != cc->cmd.mbid) {
		MSG_ERR("%s: error: Modbus device address mismatch: got 0x%02x, expected 0x%02x\n", inst->name, bytes[0], cc->cmd.mbid);
		// FIXME: Actually don't know whether we should resend here...
		// It might be another master interfering with the reply
		force_resend(inst);
		return -1;
	}

	if((bytes[1] & 0x7f ) != cc->cmd.func) {
		MSG_ERR("%s: error: Call/response function number mismatch: got 0x%02x, expected 0x%02x)\n", inst->name, cc->cmd.func, bytes[1]);
		set_error(inst, ECHRNG);
		force_resend(inst);
		return -1;
	}

	checksum = crc_modbus(bytes, rxcount - 2);
	rtapi_u16 retcrc = ((rtapi_u16)bytes[rxcount - 1] << 8) | bytes[rxcount - 2];
	if(retcrc != checksum) {
		MSG_ERR("%s: error: Modbus checksum error: got 0x%04x, expected 0x%04x\n", inst->name, retcrc, checksum);
		set_error(inst, EBADE);
		force_resend(inst);
		return -1;
	}

	switch(bytes[1]) {
	case MBCMD_R_COILS: // read coils
	case MBCMD_R_INPUTS: // read inputs
		// The byte count cannot be zero and there is a hard limit on
		// coils/inputs/registers read in one pass.
		// 2000 bits  : n = 125 bytes (commands 1 and 2)
		if(test_bytecount(inst, bytes, rxcount, 1, 125) < 0) {
			set_error(inst, ERANGE);
			force_resend(inst);
			break;
		}
		w = 3;	// First data return byte {mbid, func, bytecount, ...}
		b = 0;
		for(unsigned i = 0; i < cc->cmd.cpincnt; i++) {
			hal->pins[p++].pin->b = !!(bytes[w] & (1u << b));
			if(++b >= 8) { b = 0; w++; }
		}
		break;
	case MBCMD_R_INPUTREGS: // read input registers
	case MBCMD_R_REGISTERS: // read holding registers
		// A set of data combination can be read. We know how many registers
		// should be returned.
		if(bytes[2] != cc->cmd.cregcnt * 2) {
			MSG_ERR("%s: error: Cmd response data length %u was expected to be %u\n",
					inst->name, (unsigned)bytes[2], cc->cmd.cregcnt * 2);
			set_error(inst, ERANGE);
			force_resend(inst);
			break;
		}

		for(int i = 0; i < cc->cmd.cpincnt; i++) {
			unsigned pos = 3 + cc->typeptr[i].regofs * 2;
			// 'pos' is offset in bytes[] array.
			// The 3 are the fields mbid, func and bytecount

			// Read bytes according to the size of the mtype
			switch(mtypeformat(cc->typeptr[i].mtype)) {
			case MBT_AB:	// Always sign-extended
				val64.s = 256 * (rtapi_s64)(rtapi_s8)bytes[pos] + bytes[pos+1];
				break;
			case MBT_BA:	// Always sign-extended
				val64.s = 256 * (rtapi_s64)(rtapi_s8)bytes[pos+1] + bytes[pos];
				break;

			case MBT_ABCD:
			case MBT_BADC:
			case MBT_CDAB:
			case MBT_DCBA:
				val32 = get32(bytes + pos, cc->typeptr[i].mtype);
				val64.s = val32.s;	// Sign-extend
				break;

			case MBT_ABCDEFGH:
			case MBT_BADCFEHG:
			case MBT_CDABGHEF:
			case MBT_DCBAHGFE:
			case MBT_EFGHABCD:
			case MBT_FEHGBADC:
			case MBT_GHEFCDAB:
			case MBT_HGFEDCBA:
				val64 = get64(bytes + pos, cc->typeptr[i].mtype);
				break;

			default:
				// This is inconsistent!
				MSG_ERR("%s: error: Invalid mtype %u for command %d\n", inst->name, mtypeformat(cc->typeptr[i].mtype), inst->cmdidx);
				cc->disabled = 1;
				set_error(inst, EINVAL);
				return -1;
			}
			// val64.s contains the 8 bytes sign extended if necessary
			// val32 contains the 4-byte sequence
			if(MBT_F == mtypetype(cc->typeptr[i].mtype)) {
				rtapi_u64 u;
				switch(mtypesize(cc->typeptr[i].mtype)) {
				case 1:
					// Promote half to double. Don't rely on compiler
					// _Float16, do some bit-fiddling to make it fit.
					// _Float16: sign(1) exponent( 5) mantissa(10) bias=15
					// (mask     0x8000  0x7c00       0x03ff)
					// double:   sign(1) exponent(11) mantissa(52) bias=1023
					u = val64.u & 0x8000 ? 0x8000000000000000ul : 0ul;	// Sign
					if(!(val64.u & 0x7c00)) {
						// Zero or subnormal
						// Keep exponent zero, only mantissa shifted
						u |= (rtapi_u64)(val64.u & 0x3ff) << (52 - 10);
					} else if((val64.u & 0x7c00) == 0x7c00) {
						// Inf or NaN
						u |= 0x7ff0000000000000ul;	// Exponent all ones
						u |= (rtapi_u64)(val64.u & 0x3ff);	// Mantissa --> NaN in lower bits
					} else {
						u |= (rtapi_u64)((((val64.u & 0x7c00) >> 10) - 15 + 1023) & 0x7ff) << 52;	// Exponent
						u |= (rtapi_u64)(val64.u & 0x3ff) << (52 - 10);	// Mantissa
					}
					val64.u = u;	// val64.f now contains half extended to double
					break;
				case 2:
					val64.f = val32.f;	// Promote float to double
					break;
				// case 4: has no special handling
				// 8-byte float already read into val64
				}
			}
			// val64.u is the unsigned value for MBT_U
			// val64.s is the signed value for MBT_S
			// val64.f is the (promoted) fp value for MBT_F

			switch(cc->typeptr[i].htype) {
			case HAL_BIT:
				hal->pins[p].pin->b = 0 != val64.u;	// Zero maps to false, anything else to true
				break;
			case HAL_U32:
				switch(mtypetype(cc->typeptr[i].mtype)) {
				case MBT_U:	hal->pins[p].pin->u = unmap32_uu(cc, val64.u, i); break;
				case MBT_S:	hal->pins[p].pin->u = unmap32_us(cc, val64.s, i); break;
				case MBT_F:	hal->pins[p].pin->u = unmap32_uf(cc, val64.f, i); break;
				}
				break;
			case HAL_S32:
				switch(mtypetype(cc->typeptr[i].mtype)) {
				case MBT_U:	hal->pins[p].pin->s = unmap32_su(cc, val64.u, i); break;
				case MBT_S:	hal->pins[p].pin->s = unmap32_ss(cc, val64.s, i); break;
				case MBT_F:	hal->pins[p].pin->s = unmap32_sf(cc, val64.f, i); break;
				}
				if(haspinscale(&cc->typeptr[i])) {
					switch(mtypetype(cc->typeptr[i].mtype)) {
					case MBT_U:	*(hal->pins[p].scaled) = (real_t)(rtapi_s64)(val64.u - hal->pins[p].offset->lu) * *(hal->pins[p].scale); break;
					case MBT_S:	*(hal->pins[p].scaled) = (real_t)(val64.s - hal->pins[p].offset->ls) * *(hal->pins[p].scale); break;
					case MBT_F:	*(hal->pins[p].scaled) = (val64.f - hal->pins[p].offset->f) * *(hal->pins[p].scale); break;
					}
				}
				break;
			case HAL_U64:
				switch(mtypetype(cc->typeptr[i].mtype)) {
				case MBT_U:	hal->pins[p].pin->lu = val64.u; break;
				case MBT_S:	hal->pins[p].pin->lu = unmap64_us(cc, val64.s, i); break;
				case MBT_F:	hal->pins[p].pin->lu = unmap64_uf(cc, val64.f, i); break;
				}
				break;
			case HAL_S64:
				switch(mtypetype(cc->typeptr[i].mtype)) {
				case MBT_U:	hal->pins[p].pin->ls = unmap64_su(cc, val64.u, i); break;
				case MBT_S:	hal->pins[p].pin->ls = val64.s; break;
				case MBT_F:	hal->pins[p].pin->ls = unmap64_sf(cc, val64.f, i); break;
				}
				if(haspinscale(&cc->typeptr[i])) {
					switch(mtypetype(cc->typeptr[i].mtype)) {
					case MBT_U:	*(hal->pins[p].scaled) = (real_t)(rtapi_s64)(val64.u - hal->pins[p].offset->lu) * *(hal->pins[p].scale); break;
					case MBT_S:	*(hal->pins[p].scaled) = (real_t)(val64.s - hal->pins[p].offset->ls) * *(hal->pins[p].scale); break;
					case MBT_F:	*(hal->pins[p].scaled) = (val64.f - hal->pins[p].offset->f) * *(hal->pins[p].scale); break;
					}
				}
				break;
			case HAL_FLOAT:
				switch(mtypetype(cc->typeptr[i].mtype)) {
				case MBT_U:	hal->pins[p].pin->f = val64.u; break;
				case MBT_S:	hal->pins[p].pin->f = val64.s; break;
				case MBT_F:	hal->pins[p].pin->f = val64.f; break;
				}
				if(haspinscale(&cc->typeptr[i])) {
					switch(mtypetype(cc->typeptr[i].mtype)) {
					case MBT_U:	*(hal->pins[p].scaled) = (real_t)(rtapi_s64)(val64.u - hal->pins[p].offset->lu) * *(hal->pins[p].scale); break;
					case MBT_S:	*(hal->pins[p].scaled) = (real_t)(val64.s - hal->pins[p].offset->ls) * *(hal->pins[p].scale); break;
					case MBT_F:	*(hal->pins[p].scaled) = (val64.f - hal->pins[p].offset->f) * *(hal->pins[p].scale); break;
					}
				}
				break;
			}
			p++;
		}
		break;
	// Nothing to do for write commands 5, 6, 15, 16 ??
	case MBCMD_W_COIL:
	case MBCMD_W_REGISTER:
		// The above two should be an echo of the PDU sent
	case MBCMD_W_COILS:
	case MBCMD_W_REGISTERS:
		// The above two should echo the first 4 bytes of the sent frame contents
		break;

	// The following are error codes
	case (128 + MBCMD_R_COILS):		//  1 + error bit
	case (128 + MBCMD_R_INPUTS):	//  2 + error bit
	case (128 + MBCMD_R_REGISTERS):	//  3 + error bit
	case (128 + MBCMD_R_INPUTREGS):	//  4 + error bit
	case (128 + MBCMD_W_COIL):		//  5 + error bit
	case (128 + MBCMD_W_REGISTER):	//  6 + error bit
	case (128 + MBCMD_W_COILS):		// 15 + error bit
	case (128 + MBCMD_W_REGISTERS):	// 16 + error bit
		force_resend(inst);
		set_error(inst, EBADMSG);
		if(bytes[2] >= (sizeof(error_codes) / sizeof(*error_codes))) {
			MSG_ERR("%s: error: Modbus error response cmd %u function %u with invalid/unknown error code %u\n", inst->name, inst->cmdidx, bytes[1] & 0x7f, bytes[2]);
		} else {
			MSG_ERR("%s: error: Modbus error response cmd %u function %u error '%s' (%u)\n", inst->name, inst->cmdidx, bytes[1] & 0x7f, error_codes[bytes[2]], bytes[2]);
		}
		return -1;
	default:
		MSG_ERR("%s: error: Unknown or unsupported Modbus function code: 0x%02x (mbid=%u, cmd=%u)\n", inst->name, bytes[1], bytes[0], inst->cmdidx);
		force_resend(inst);
		set_error(inst, EBADF);
		return -1;
	}
	return 0;
}

//
// Adapted from: MODBUS over serial line specification and implementation guide V1.02
// Crc poly x^16 + x^15 + x^2 + 1
// hex 0x8005 (reversed: 0xA001)
//
static rtapi_u16 crc_modbus(const rtapi_u8 *buffer, size_t len)
{
	static const rtapi_u8 crctabhi[256] = { // Table of CRC values for high–order byte
		0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41, 0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40,
		0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40, 0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41,
		0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40, 0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41,
		0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41, 0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40,
		0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40, 0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41,
		0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41, 0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40,
		0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41, 0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40,
		0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40, 0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41,
		0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40, 0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41,
		0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41, 0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40,
		0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41, 0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40,
		0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40, 0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41,
		0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41, 0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40,
		0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40, 0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41,
		0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40, 0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41,
		0x00, 0xc1, 0x81, 0x40, 0x01, 0xc0, 0x80, 0x41, 0x01, 0xc0, 0x80, 0x41, 0x00, 0xc1, 0x81, 0x40
	};

	static const rtapi_u8 crctablo[256] = { // Table of CRC values for low–order byte
		0x00, 0xc0, 0xc1, 0x01, 0xc3, 0x03, 0x02, 0xc2, 0xc6, 0x06, 0x07, 0xc7, 0x05, 0xc5, 0xc4, 0x04,
		0xcc, 0x0c, 0x0d, 0xcd, 0x0f, 0xcf, 0xce, 0x0e, 0x0a, 0xca, 0xcb, 0x0b, 0xc9, 0x09, 0x08, 0xc8,
		0xd8, 0x18, 0x19, 0xd9, 0x1b, 0xdb, 0xda, 0x1a, 0x1e, 0xde, 0xdf, 0x1f, 0xdd, 0x1d, 0x1c, 0xdc,
		0x14, 0xd4, 0xd5, 0x15, 0xd7, 0x17, 0x16, 0xd6, 0xd2, 0x12, 0x13, 0xd3, 0x11, 0xd1, 0xd0, 0x10,
		0xf0, 0x30, 0x31, 0xf1, 0x33, 0xf3, 0xf2, 0x32, 0x36, 0xf6, 0xf7, 0x37, 0xf5, 0x35, 0x34, 0xf4,
		0x3c, 0xfc, 0xfd, 0x3d, 0xff, 0x3f, 0x3e, 0xfe, 0xfa, 0x3a, 0x3b, 0xfb, 0x39, 0xf9, 0xf8, 0x38,
		0x28, 0xe8, 0xe9, 0x29, 0xeb, 0x2b, 0x2a, 0xea, 0xee, 0x2e, 0x2f, 0xef, 0x2d, 0xed, 0xec, 0x2c,
		0xe4, 0x24, 0x25, 0xe5, 0x27, 0xe7, 0xe6, 0x26, 0x22, 0xe2, 0xe3, 0x23, 0xe1, 0x21, 0x20, 0xe0,
		0xa0, 0x60, 0x61, 0xa1, 0x63, 0xa3, 0xa2, 0x62, 0x66, 0xa6, 0xa7, 0x67, 0xa5, 0x65, 0x64, 0xa4,
		0x6c, 0xac, 0xad, 0x6d, 0xaf, 0x6f, 0x6e, 0xae, 0xaa, 0x6a, 0x6b, 0xab, 0x69, 0xa9, 0xa8, 0x68,
		0x78, 0xb8, 0xb9, 0x79, 0xbb, 0x7b, 0x7a, 0xba, 0xbe, 0x7e, 0x7f, 0xbf, 0x7d, 0xbd, 0xbc, 0x7c,
		0xb4, 0x74, 0x75, 0xb5, 0x77, 0xb7, 0xb6, 0x76, 0x72, 0xb2, 0xb3, 0x73, 0xb1, 0x71, 0x70, 0xb0,
		0x50, 0x90, 0x91, 0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54,
		0x9c, 0x5c, 0x5d, 0x9d, 0x5f, 0x9f, 0x9e, 0x5e, 0x5a, 0x9a, 0x9b, 0x5b, 0x99, 0x59, 0x58, 0x98,
		0x88, 0x48, 0x49, 0x89, 0x4b, 0x8b, 0x8a, 0x4a, 0x4e, 0x8e, 0x8f, 0x4f, 0x8d, 0x4d, 0x4c, 0x8c,
		0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80, 0x40
	};

	rtapi_u8 crch = 0xff;
	rtapi_u8 crcl = 0xff;
	while(len--) {
		unsigned idx = crcl ^ *buffer++;
		crcl = crch ^ crctabhi[idx];
		crch = crctablo[idx];
	}
	return ((rtapi_u16)crch << 8) | crcl;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*                     Mbccb file read and validation                      */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#if !defined(__KERNEL__)
// Userspace file read
static ssize_t read_mbccb(const hm2_modbus_inst_t *inst, const char *fname, hm2_modbus_mbccb_header_t **pmbccb)
{
	if(!pmbccb)
		return -EINVAL;

	*pmbccb = NULL;

	// Open the file
	int fd = open(fname, O_RDONLY);
	if(fd < 0) {
		ssize_t rv = -errno;
		MSG_ERR("%s: error: Failed to open '%s' for reading (error %d)\n", inst->name, fname, errno);
		return rv;
	}

	// Get the file size
	struct stat sb;
	if(fstat(fd, &sb) < 0) {
		ssize_t rv = -errno;
		MSG_ERR("%s: error: Failed to fstat '%s' (error %d)\n", inst->name, fname, errno);
		close(fd);
		return rv;
	}

	// Allocate memory
	*pmbccb = rtapi_kzalloc(sb.st_size, RTAPI_GFP_KERNEL);
	if(!*pmbccb) {
		MSG_ERR("%s: error: Failed to allocate %zd bytes memory for mbccb buffer\n", inst->name, (ssize_t)sb.st_size);
		close(fd);
		return -ENOMEM;
	}

	// Read the entire file
retry_read:
	ssize_t err = read(fd, *pmbccb, sb.st_size);
	if(err < 0) {
		ssize_t rv = -errno;
		if(errno == EINTR)
			goto retry_read;	// Interrupted syscall
		MSG_ERR("%s: error: Failed to read from '%s' (error %d)\n", inst->name, fname, errno);
		rtapi_kfree(*pmbccb);
		*pmbccb = NULL;
		close(fd);
		return rv;
	}
	if(err != (ssize_t)sb.st_size) {
		MSG_ERR("%s: error: Read %zd bytes instead of %zd bytes from '%s', aborting\n", inst->name, err, (ssize_t)sb.st_size, fname);
		rtapi_kfree(*pmbccb);
		*pmbccb = NULL;
		close(fd);
		return -EIO;
	}
	close(fd);
	return err;	// Read it all, return the size
}

#else

// In-kernel file read
static ssize_t read_mbccb(const hm2_modbus_inst_t *inst, const char *fname, hm2_modbus_mbccb_header_t **pmbccb)
{
	if(!pmbccb)
		return -EINVAL;

	struct file *fp;
	*pmbccb = NULL;

	// Open the file
	fp = filp_open(fname, O_RDONLY, 0);
	if(IS_ERR(fp)) {
		MSG_ERR("%s: error: Failed to open '%s' for reading (error %d)\n", inst->name, fname, (int)PTR_ERR(fp));
		// FIXME: is it necessary to check negative?
		return PTR_ERR(fp) < 0 ? PTR_ERR(fp) : -PTR_ERR(fp);
	}

	ssize_t fsize = fp->f_inode->i_size;	// File's inode file size

	// Allocate memory
	*pmbccb = rtapi_kzalloc(fsize, RTAPI_GFP_KERNEL);
	if(!*pmbccb) {
		MSG_ERR("%s: error: Failed to allocate %zd bytes memory for mbccb buffer\n", inst->name, fsize);
		filp_close(fp, NULL);
		return -ENOMEM;
	}

	// Read the entire file
	ssize_t err = kernel_read(fp, *pmbccb, fsize, NULL);
	if(err < 0) {
		MSG_ERR("%s: error: Failed to read from '%s' (error %zd)\n", inst->name, fname, err);
		rtapi_kfree(*pmbccb);
		*pmbccb = NULL;
		filp_close(fp, NULL);
		return err;
	}
	if(err != fsize) {
		MSG_ERR("%s: error: Read %zd bytes instead of %zd bytes from '%s', aborting\n", inst->name, err, fsize, fname);
		rtapi_kfree(*pmbccb);
		*pmbccb = NULL;
		filp_close(fp, NULL);
		return -EIO;	// Assume IO error if the sizes do not match
	}
	filp_close(fp, NULL);
	return err;	// Read it all, return the size
}
#endif


static int check_htype(unsigned type)
{
	switch(type) {
	case HAL_BIT:	// not valid in register read/write
	case HAL_U32:
	case HAL_S32:
	case HAL_U64:
	case HAL_S64:
	case HAL_FLOAT:
		return 0;
	}
	return -1;
}
//
// Load a binary Modbus command control structure and verify its content.
// Returns 0 on success and -errno on failure. A message has been printed in
// case of error.
//
static int load_mbccb(hm2_modbus_inst_t *inst, const char *fname)
{
	int rv = -EINVAL;

	hm2_modbus_mbccb_header_t *mbccb;
	ssize_t mbccblen = read_mbccb(inst, fname, &mbccb);
	if(mbccblen < 0)
		return (int)mbccblen;

	if(mbccblen < (ssize_t)sizeof(*mbccb)) {
		MSG_ERR("%s: error: Mbccb file too small (only read %zd bytes)\n", inst->name, mbccblen);
		goto errout;
	}

	// Done reading, now test format
	static const rtapi_u8 signature[8] = {'M','e','s','a','M','B','0','1'};
	if(memcmp(signature, mbccb->sig, sizeof(signature))) {
		MSG_ERR("%s: error: Invalid signature in mbccb file: '", inst->name);
		for(unsigned i = 0; i < sizeof(mbccb->sig); i++)
			MSG_ERR("%c", isprint(mbccb->sig[i]) ? mbccb->sig[i] : '?');
		MSG_ERR("', expected 'MesaMB01'\n");
		goto errout;
	}

	// Make header's byte sex native from big-endian
	// Skip the signature words and byte values
	mbccb->baudrate = be32_to_cpu(mbccb->baudrate);
	mbccb->format   = be16_to_cpu(mbccb->format);
	mbccb->txdelay  = be16_to_cpu(mbccb->txdelay);
	mbccb->rxdelay  = be16_to_cpu(mbccb->rxdelay);
	mbccb->drvdelay = be16_to_cpu(mbccb->drvdelay);
	mbccb->icdelay  = be16_to_cpu(mbccb->icdelay);
	//for(unsigned i = 0; i < sizeof(mbccb->unused)/sizeof(mbccb->unused[0]); i++)
	//	mbccb->unused[i] = be32_to_cpu(mbccb->unused[i]);
	mbccb->initlen  = be32_to_cpu(mbccb->initlen);
	mbccb->cmdslen  = be32_to_cpu(mbccb->cmdslen);
	mbccb->datalen  = be32_to_cpu(mbccb->datalen);

	if(mbccb->baudrate < 1200) {
		MSG_WARN("%s: warning: Mbccb baudrate %u slower than 1200 baud. May fail to setup\n", inst->name, mbccb->baudrate);
	} else if(mbccb->baudrate > 460800) {
		MSG_WARN("%s: warning: Mbccb baudrate %u faster than 460800 baud. May fail to setup\n", inst->name, mbccb->baudrate);
	}

	if((mbccb->format & MBCCB_FORMAT_PARITYODD) && !(mbccb->format & MBCCB_FORMAT_PARITYEN)) {
		MSG_ERR("%s: error: Mbccb parity Odd set while parity disabled\n", inst->name);
		goto errout;
	}

	if(mbccb->rxdelay > 0x3fc) {
		MSG_ERR("%s: error: Mbccb rxdelay %u out of range [0..1020]\n", inst->name, mbccb->rxdelay);
		goto errout;
	}

	if(mbccb->txdelay > 0x3fc) {
		MSG_ERR("%s: error: Mbccb txdelay %u out of range [0..1020]\n", inst->name, mbccb->txdelay);
		goto errout;
	}

	if(mbccb->drvdelay > 0x1f) {
		MSG_ERR("%s: error: Mbccb drivedelay %u out of range [0..31]\n", inst->name, mbccb->drvdelay);
		goto errout;
	}

	if(mbccb->icdelay > 0x3fc) {
		MSG_ERR("%s: error: Mbccb icdelay %u out of range [0..255]\n", inst->name, mbccb->icdelay);
		goto errout;
	}

	// Must have whole init records
	if(0 != mbccb->initlen % sizeof(hm2_modbus_mbccb_cmds_t)) {
		MSG_ERR("%s: error: Mbccb init section has invalid size %zu\n", inst->name, (size_t)mbccb->initlen);
		goto errout;
	}
	// Must have whole cmds records
	if(0 != mbccb->cmdslen % sizeof(hm2_modbus_mbccb_cmds_t)) {
		MSG_ERR("%s: error: Mbccb commands section has invalid size %zu\n", inst->name, (size_t)mbccb->cmdslen);
		goto errout;
	}
	// Must have data in the data segment
	if(!mbccb->datalen) {
		MSG_ERR("%s: error: Mbccb datalen is zero, expected at least some pin name data\n", inst->name);
		goto errout;
	}
	// All lengths plus header must be file size
	ssize_t len = sizeof(*mbccb) + mbccb->initlen + mbccb->cmdslen + mbccb->datalen;
	if(mbccblen != len) {
		MSG_ERR("%s: error: Mbccb size mismatch. Read %zd and calculated %zd\n", inst->name, mbccblen, len);
		goto errout;
	}

	// Calculate pointers and sizes
	size_t ofs = sizeof(hm2_modbus_mbccb_header_t);
	hm2_modbus_mbccb_cmds_t *initptr = (hm2_modbus_mbccb_cmds_t *)((rtapi_u8 *)mbccb + ofs);
	ofs += mbccb->initlen;
	hm2_modbus_mbccb_cmds_t *cmdsptr = (hm2_modbus_mbccb_cmds_t *)((rtapi_u8 *)mbccb + ofs);
	ofs += mbccb->cmdslen;
	const rtapi_u8 *dataptr = ((rtapi_u8 *)mbccb + ofs);
	unsigned ninit = mbccb->initlen / sizeof(hm2_modbus_mbccb_cmds_t);
	unsigned ncmds = mbccb->cmdslen / sizeof(hm2_modbus_mbccb_cmds_t);

	// There must be something to do
	// You can still have 'nothing' if you only have one delay command.
	if(!ncmds) {
		MSG_ERR("%s: error: Mbccb has no commands\n", inst->name);
		goto errout;
	}

	// Check the data segment fragments to add up to the reported length
	// Note: zero-length segments are allowed (for alignment) and have one
	// length byte being zero.
	rtapi_u32 dl = 0;
	for(const rtapi_u8 *dptr = dataptr; dptr < dataptr + mbccb->datalen; dptr += *dptr + 1) {
		dl += *dptr + 1;	// Also count the length byte
	}
	if(dl != mbccb->datalen) {
		MSG_ERR("%s: error: Mbccb data segment size mismatch. Read %u and calculated %u\n", inst->name, mbccb->datalen, dl);
		goto errout;
	}

	const rtapi_u8 *dataptrend = dataptr + mbccb->datalen;

	// Check all init data packets
	for(unsigned i = 0; i < ninit; i++) {
		initptr[i].flags    = be16_to_cpu(initptr[i].flags);
		initptr[i].caddr    = be16_to_cpu(initptr[i].caddr);
		initptr[i].cpincnt  = be16_to_cpu(initptr[i].cpincnt);
		initptr[i].cregcnt  = be16_to_cpu(initptr[i].cregcnt);
		initptr[i].unusedp1 = be16_to_cpu(initptr[i].unusedp1);
		initptr[i].unusedp2 = be32_to_cpu(initptr[i].unusedp2);
		initptr[i].ctypeptr = be32_to_cpu(initptr[i].ctypeptr);
		initptr[i].cinterval= be32_to_cpu(initptr[i].cinterval);
		initptr[i].ctimeout = be32_to_cpu(initptr[i].ctimeout);
		initptr[i].cdataptr = be32_to_cpu(initptr[i].cdataptr);

		if(!initptr[i].func && initptr[i].caddr > 1) {
			MSG_ERR("%s: error: Mbccb init %u unknown meta-command %u\n", inst->name, i, initptr[i].caddr);
			goto errout;
		}

		// Flags
		if(initptr[i].flags & ~MBCCB_CMDF_INITMASK) {
			MSG_ERR("%s: error: Mbccb init %u has invalid flags set (flags=0x%04x, allowed=0x%04x)\n",
						inst->name, i, initptr[i].flags, MBCCB_CMDF_INITMASK);
			goto errout;
		}

		const rtapi_u8 *dp = dataptr + initptr[i].cdataptr;
		if(initptr[i].cdataptr >= mbccb->datalen) {
			MSG_ERR("%s: error: Mbccb init %u data size mismatch. Read %u is beyond segment size %u\n",
						inst->name, i, initptr[i].cdataptr, mbccb->datalen);
			goto errout;
		}

		if(initptr[i].cdataptr) {
			// Check init's data length
			// Length must be larger or equal smallest packet and it must be within
			// the file boundary.
			if(dp + *dp >= dataptrend) {
				MSG_ERR("%s: error: Mbccb init %u data outside data segment\n", inst->name, i);
				goto errout;
			}
			if(*dp < 6) {	// Packet: mbid(1), func(1), addr(2), value_or_count(2), ...
				MSG_ERR("%s: error: Mbccb init %u data packet size less than 6\n", inst->name, i);
				goto errout;
			}
		} else if(initptr[i].func || initptr[i].caddr > 1) {
			MSG_ERR("%s: error: Mbccb init %u has no data packet and invalid meta-command\n", inst->name, i);
			goto errout;
		}

		// Can never be more than MAXDELAY
		if(initptr[i].ctimeout > MAXDELAY) {
			MSG_ERR("%s: error: Mbccb init %u timeout larger than %u microseconds\n", inst->name, i, (unsigned)MAXDELAY);
			goto errout;
		}

		// Support only a strict set of command functions
		switch(initptr[i].func) {
		case 0: // The delay command
			if(0 != initptr[i].mbid) {
				MSG_ERR("%s: error: Mbccb init %u mbid not zero for delay command\n", inst->name, i);
				goto errout;
			}
			break;
		case MBCMD_R_COILS:
		case MBCMD_R_INPUTS:
		case MBCMD_R_REGISTERS:
		case MBCMD_R_INPUTREGS:
		case MBCMD_W_COIL:
		case MBCMD_W_REGISTER:
		case MBCMD_W_COILS:
		case MBCMD_W_REGISTERS:
			break;
		default:
			MSG_ERR("%s: error: Mbccb init %u has unsupported Modbus command function %u\n", inst->name, i, initptr[i].func);
			goto errout;
		}
	}

	unsigned npins = 0;

	// Check that all pins names are valid and in bounds
	for(unsigned c = 0; c < ncmds; c++) {
		cmdsptr[c].flags    = be16_to_cpu(cmdsptr[c].flags);
		cmdsptr[c].caddr    = be16_to_cpu(cmdsptr[c].caddr);
		cmdsptr[c].cpincnt  = be16_to_cpu(cmdsptr[c].cpincnt);
		cmdsptr[c].cregcnt  = be16_to_cpu(cmdsptr[c].cregcnt);
		cmdsptr[c].unusedp1 = be16_to_cpu(cmdsptr[c].unusedp1);
		cmdsptr[c].unusedp2 = be32_to_cpu(cmdsptr[c].unusedp2);
		cmdsptr[c].ctypeptr = be32_to_cpu(cmdsptr[c].ctypeptr);
		cmdsptr[c].cinterval= be32_to_cpu(cmdsptr[c].cinterval);
		cmdsptr[c].ctimeout = be32_to_cpu(cmdsptr[c].ctimeout);
		cmdsptr[c].cdataptr = be32_to_cpu(cmdsptr[c].cdataptr);

		// Data pointer can never be beyond data segment
		if(cmdsptr[c].cdataptr >= mbccb->datalen) {
			MSG_ERR("%s: error: Mbccb cmds %u cmds' datasize mismatch. Read %u is beyond segment size %u\n",
						inst->name, c, cmdsptr[c].cdataptr, mbccb->datalen);
			goto errout;
		}

		// Type pointer can never be beyond data segment
		if(cmdsptr[c].ctypeptr >= mbccb->datalen) {
			MSG_ERR("%s: error: Mbccb cmds %u types' datasize mismatch. Read %u is beyond segment size %u\n",
						inst->name, c, cmdsptr[c].ctypeptr, mbccb->datalen);
			goto errout;
		}

		// Type pointer /content/ must be % 4 aligned
		if(cmdsptr[c].ctypeptr && 0 != (cmdsptr[c].ctypeptr + 1) % 4) {
			MSG_ERR("%s: error: Mbccb cmds %u types' data alignment mismatch %u %% 4 != 0\n",
						inst->name, c, cmdsptr[c].ctypeptr + 1);
			goto errout;
		}

		// Flags
		if(cmdsptr[c].flags & ~MBCCB_CMDF_MASK) {
			MSG_WARN("%s: warning: Mbccb cmds %u has extra flags set (flags=0x%04x, allowed=0x%04x)\n",
						inst->name, c, cmdsptr[c].flags, MBCCB_CMDF_MASK);
		}
		cmdsptr[c].flags &= MBCCB_CMDF_MASK;

		// Can never be more than MAXDELAY
		if(cmdsptr[c].ctimeout > MAXDELAY) {
			MSG_ERR("%s: error: Mbccb cmds %u timeout larger than %u microseconds\n", inst->name, c, (unsigned)MAXDELAY);
			goto errout;
		}

		const hm2_modbus_mbccb_type_t *xtype;
		unsigned xtypelen;
		unsigned xregofs;

		// Support a strict set of command functions with type restrictions
		switch(cmdsptr[c].func) {
		case 0: // The delay command
			if(cmdsptr[c].mbid || cmdsptr[c].cpincnt || cmdsptr[c].caddr) {
				MSG_ERR("%s: error: Mbccb cmds %u mbid, pin count or address not zero for delay command\n", inst->name, c);
				goto errout;
			}
			continue;	// No pins, need to move to the next command
		case MBCMD_R_COILS:
		case MBCMD_W_COILS:
		case MBCMD_W_COIL:
		case MBCMD_R_INPUTS:
			if(cmdsptr[c].cpincnt < 1 || cmdsptr[c].cpincnt > 2000) {
				MSG_ERR("%s: error: Mbccb cmds %u bit function %u with invalid %u pins\n",
						inst->name, c, cmdsptr[c].func, cmdsptr[c].cpincnt);
				goto errout;
			}
			// These are implicit HAL_BIT
			break;
		case MBCMD_R_REGISTERS:
		case MBCMD_R_INPUTREGS:
		case MBCMD_W_REGISTERS:
		case MBCMD_W_REGISTER:
			// For these functions any type <=> any type is allowed
			// except W_REGISTER requiring word size
			if(!cmdsptr[c].ctypeptr) {
				MSG_ERR("%s: error: Mbccb cmds %u missing type info, typeptr is zero\n",
							inst->name, c);
				goto errout;
			}

			xtypelen = *(dataptr + cmdsptr[c].ctypeptr);
			xtype    = (hm2_modbus_mbccb_type_t *)(dataptr + cmdsptr[c].ctypeptr + 1);
			if(xtypelen % sizeof(*xtype)) {
				MSG_ERR("%s: error: Mbccb cmds %u has invalid types modulo length (%u %% %zu != 0)\n",
						inst->name, c, xtypelen, sizeof(*xtype));
				goto errout;
			}
			if(xtypelen / sizeof(*xtype) != cmdsptr[c].cpincnt) {
				MSG_ERR("%s: error: Mbccb cmds %u has %zu types with %u pins\n",
						inst->name, c, xtypelen / sizeof(*xtype), cmdsptr[c].cpincnt);
				goto errout;
			}
			if(cmdsptr[c].func == MBCMD_W_REGISTER) {
				if(cmdsptr[c].cpincnt != 1) {
					MSG_ERR("%s: error: Mbccb cmds %u has pin count %u != 1\n", inst->name, c, cmdsptr[c].cpincnt);
					goto errout;
				}
				if(cmdsptr[c].cregcnt != 1) {
					MSG_ERR("%s: error: Mbccb cmds %u has register count %u != 1\n", inst->name, c, cmdsptr[c].cregcnt);
					goto errout;
				}
				if(mtypesize(xtype->mtype) != 1) { // One word
					MSG_ERR("%s: error: Mbccb cmds %u has too large Modbus type for function W_REGISTER\n", inst->name, c);
					goto errout;
				}
			} else {
				if(cmdsptr[c].cpincnt < 1 || cmdsptr[c].cpincnt > 125) {
					MSG_ERR("%s: error: Mbccb cmds %u has pin count %u out of range [1..125]\n", inst->name, c, cmdsptr[c].cpincnt);
					goto errout;
				}
				if(cmdsptr[c].cregcnt < 1 || cmdsptr[c].cregcnt > 125) {
					MSG_ERR("%s: error: Mbccb cmds %u has register count %u out of range [1..125]\n", inst->name, c, cmdsptr[c].cregcnt);
					goto errout;
				}
			}
			// Check types and offsets for each pin
			if(0 != xtype[0].regofs) {
				MSG_ERR("%s: error: Mbccb cmds %u has invalid first xtype reg offset %u\n", inst->name, c, xtype[0].regofs);
				goto errout;
			}
			xregofs = 0;
			for(unsigned x = 0; x < cmdsptr[c].cpincnt; x++) {
				if(!mtypeisvalid(xtype[x].mtype)) {
					MSG_ERR("%s: error: Mbccb cmds %u, pin %u has invalid modbustype %u\n", inst->name, c, x, xtype[x].mtype);
					goto errout;
				}
				if(check_htype(xtype[x].htype) < 0) {
					MSG_ERR("%s: error: Mbccb cmds %u, pin %u has invalid haltype %u\n", inst->name, c, x, xtype[x].htype);
					goto errout;
				}
				if(xtype[x].htype & ~MBCCB_PINF_MASK) {
					MSG_ERR("%s: error: Mbccb cmds %u, pin %u has invalid flags 0x%02x\n", inst->name, c, x, xtype[x].flags);
					goto errout;
				}
				unsigned mbs = mtypesize(xtype[x].mtype);
				if(xtype[x].regofs < xregofs || xtype[x].regofs > 125-mbs) {
					MSG_ERR("%s: error: Mbccb cmds %u, pin %u has invalid register offset 0x%02x\n", inst->name, c, x, xtype[x].regofs);
					goto errout;
				}
				xregofs = xtype[x].regofs + mbs;
			}
			if(xregofs > 125+1) {	// Plus one because this would point to the /next/ offset
				MSG_ERR("%s: error: Mbccb cmds %u xtypes adds up to too many registers %u out of range [1..125]\n",
						inst->name, c, xregofs);
				goto errout;
			}
			// FIXME:
			// Should check more modbustype <=> haltype compatibility?
			// I think we currently implement conversion of any-to-any.
			break;

		default:
			MSG_ERR("%s: error: Mbccb cmds %u has unsupported Modbus command function %u\n", inst->name, c, cmdsptr[c].func);
			goto errout;
		}

		// Check each pin
		// Strings are pascal strings with 'leading byte == length' and must
		// additionally be NUL terminated. The NUL is counted in the length.
		const rtapi_u8 *dp = dataptr + cmdsptr[c].cdataptr;
		for(unsigned p = 0; p < cmdsptr[c].cpincnt; p++) {
			if(dp + *dp >= dataptrend) {
				MSG_ERR("%s: error: Mbccb pin %u:%u outside data segment\n", inst->name, c, p);
				goto errout;
			}
			if(*dp < 2) {	// Should contain at least one char and the terminating zero
				MSG_ERR("%s: error: Mbccb pin %u:%u is empty\n", inst->name, c, p);
				goto errout;
			}
			if(*dp > MAXPINNAME) {
				MSG_ERR("%s: error: Mbccb pin %u:%u is too long (%u > %u)\n", inst->name, c, p, *dp, (unsigned)MAXPINNAME);
				goto errout;
			}
			if(dp[*dp]) {	// End of string
				MSG_ERR("%s: error: Mbccb pin %u:%u not NUL terminated\n", inst->name, c, p);
				goto errout;
			}
			// Check all chars, exclude terminating NUL char: [a-z0-9.-]
			for(unsigned ch = *dp - 1; ch > 0; ch--) {
				if(!((dp[ch] >= 'a' && dp[ch] <= 'z') || (dp[ch] >= '0' && dp[ch] <= '9') || dp[ch] == '.' || dp[ch] == '-')) {
					MSG_ERR("%s: error: Mbccb Invalid character(s) '%c' in pin %u:%u\n", inst->name, isprint(dp[ch]) ? dp[ch] : '?', c, p);
					goto errout;
				}
			}

			npins++;		// Count the pins
			dp += *dp + 1;	// Next data fragment
		}
	}

	// Copy validated data refs into the instance
	inst->mbccb = mbccb;
	inst->initptr = initptr;
	inst->cmdsptr = cmdsptr;
	inst->dataptr = dataptr;
	inst->ninit = ninit;
	inst->ncmds = ncmds;
	inst->npins = npins;
	inst->mbccbsize = mbccblen;

	return 0;	// Success

errout:
	rtapi_kfree(mbccb);
	return rv;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*                        Main entry and exit point                        */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static void cleanup()
{
	if(comp_id >= 0)
		hal_exit(comp_id);

	if(mb.insts) {
		for(int i = 0; i < mb.ninsts; i++) {
			if(mb.insts[i].cmds)
				rtapi_kfree(mb.insts[i].cmds);
			if(mb.insts[i].mbccb)
				rtapi_kfree(mb.insts[i].mbccb);
		}
		rtapi_kfree(mb.insts);
	}
}

int rtapi_app_main(void)
{
	int retval;

	// Only touch the message level if requested
	if(debug >= 0)
		rtapi_set_msg_level(debug);

	if(!ports[0]) {
		MSG_ERR(COMP_NAME": The component requires at least one valid pktuart port, eg ports=\"hm2_5i25.0.pktuart.7\"\n");
		return -EINVAL;
	}

	comp_id = hal_init(COMP_NAME);
	if(comp_id < 0) {
		MSG_ERR(COMP_NAME": hal_init() failed\n");
		return comp_id;
	}

	// Count the instances.
	for(mb.ninsts = 0; mb.ninsts < MAX_PORTS && ports[mb.ninsts]; mb.ninsts++) {}
	// Allocate memory for the instances
	if(!(mb.insts = (hm2_modbus_inst_t *)rtapi_kzalloc(mb.ninsts * sizeof(*mb.insts), RTAPI_GFP_KERNEL))) {
		MSG_ERR(COMP_NAME": Allocate instance memory failed\n");
		hal_exit(comp_id);
		return -ENOMEM;
	}

	// Parse the config string and assign to instances
	for(int i = 0; i < mb.ninsts; i++) {
		hm2_modbus_inst_t *inst = &mb.insts[i];

		rtapi_snprintf(inst->name, sizeof(inst->name), COMP_NAME".%d", i);
		rtapi_strlcpy(inst->uart, ports[i], sizeof(inst->uart)-1);

		if(!mbccbs[i]) {
			MSG_ERR("%s: error: Missing mbccb file path for instance %d in 'mbccbs' argument\n", inst->name, i);
			retval = -EINVAL;
			goto errout;
		}
		if('/' != mbccbs[i][0]) {
			MSG_WARN("%s: warning: The 'mbccb' file path '%s' for instance %d in 'mbccbs' argument is not absolute\n", inst->name, mbccbs[i], i);
		}

		if((retval = load_mbccb(inst, mbccbs[i])) < 0) {
			// Messages printed in load function
			goto errout;
		}

		// All pointers and counts have been setup in load_mbccb()

		// Allocate HAL memory
		if(!(inst->hal =  (hm2_modbus_hal_t *)hal_malloc(sizeof(*inst->hal)))) {
			MSG_ERR("%s: error: Failed to allocate HAL memory\n", inst->name);
			retval = -ENOMEM;
			goto errout;
		}
		if(!(inst->hal->pins = (mbt_pin_hal_t *)hal_malloc(inst->npins * sizeof(*inst->hal->pins)))) {
			MSG_ERR("%s: error: Failed to allocate HAL pins memory\n", inst->name);
			retval = -ENOMEM;
			goto errout;
		}
		if(!(inst->hal->cmds = (mbt_cmd_hal_t *)hal_malloc(inst->ncmds * sizeof(*inst->hal->cmds)))) {
			MSG_ERR("%s: error: Failed to allocate HAL cmds memory\n", inst->name);
			retval = -ENOMEM;
			goto errout;
		}

		if(inst->ninit > 0) {
			// Allocate inits memory
			if(!(inst->_init = rtapi_kzalloc(inst->ninit * sizeof(*inst->_init), RTAPI_GFP_KERNEL))) {
				MSG_ERR("%s: error: Failed to allocate init commands memory\n", inst->name);
				retval = -ENOMEM;
				goto errout;
			}
		}

		// Allocate commands memory
		if(!(inst->_cmds = rtapi_kzalloc(inst->ncmds * sizeof(*inst->_cmds), RTAPI_GFP_KERNEL))) {
			MSG_ERR("%s: error: Failed to allocate commands memory\n", inst->name);
			retval = -ENOMEM;
			goto errout;
		}

		// Copy the init command control structure data
		for(unsigned i = 0; i < inst->ninit; i++) {
			inst->_init[i].cmd = inst->initptr[i];
		}
		// Copy the loop command control structure data
		for(unsigned i = 0; i < inst->ncmds; i++) {
			inst->_cmds[i].cmd = inst->cmdsptr[i];
			if(inst->cmdsptr[i].ctypeptr)
				inst->_cmds[i].typeptr = (hm2_modbus_mbccb_type_t *)(inst->dataptr + inst->cmdsptr[i].ctypeptr + 1);
		}

		// Setup to start sending init or command packets
		inst->cmds = inst->ninit ? inst->_init : inst->_cmds;

		// Export the HAL process function
		if((retval = hal_export_functf(process, inst, 1, 0, comp_id, COMP_NAME".%d.process", i)) < 0) {
			MSG_ERR("%s: error: Function export failed\n", inst->name);
			goto errout;
		}

#define CHECK(x) do { \
					retval = (x); \
					if(retval < 0) { \
						MSG_ERR("%s: error: Failed to create pin or parameter\n", inst->name); \
						goto errout; \
					} \
				} while(0)
		CHECK(hal_param_u32_newf(HAL_RO, &(inst->hal->baudrate), comp_id, "%s.baudrate", inst->name));
		CHECK(hal_param_u32_newf(HAL_RO, &(inst->hal->parity),   comp_id, "%s.parity", inst->name));
		CHECK(hal_param_u32_newf(HAL_RO, &(inst->hal->stopbits), comp_id, "%s.stopbits", inst->name));
		CHECK(hal_param_u32_newf(HAL_RO, &(inst->hal->icdelay),  comp_id, "%s.icdelay", inst->name));
		CHECK(hal_param_u32_newf(HAL_RO, &(inst->hal->txdelay),  comp_id, "%s.txdelay", inst->name));
		CHECK(hal_param_u32_newf(HAL_RO, &(inst->hal->rxdelay),  comp_id, "%s.rxdelay", inst->name));
		CHECK(hal_param_u32_newf(HAL_RO, &(inst->hal->drvdelay), comp_id, "%s.drivedelay", inst->name));

		CHECK(hal_pin_bit_newf(HAL_IN,  &(inst->hal->suspend),   comp_id, "%s.suspend", inst->name));
		CHECK(hal_pin_bit_newf(HAL_IN,  &(inst->hal->reset),     comp_id, "%s.reset", inst->name));
		CHECK(hal_pin_bit_newf(HAL_OUT, &(inst->hal->fault),     comp_id, "%s.fault", inst->name));
		CHECK(hal_pin_u32_newf(HAL_OUT, &(inst->hal->faultcmd),  comp_id, "%s.fault-command", inst->name));
		CHECK(hal_pin_u32_newf(HAL_OUT, &(inst->hal->lasterror), comp_id, "%s.last-error-code", inst->name));

		inst->hal->baudrate = inst->cfg_rx.baudrate = inst->cfg_tx.baudrate = inst->mbccb->baudrate;
		unsigned parity = 0;
		if(inst->mbccb->format & MBCCB_FORMAT_PARITYEN) {
			inst->cfg_rx.flags |= HM2_PKTUART_CONFIG_PARITYEN;
			inst->cfg_tx.flags |= HM2_PKTUART_CONFIG_PARITYEN;
			parity |= 2;
		}
		if(inst->mbccb->format & MBCCB_FORMAT_PARITYODD) {
			inst->cfg_rx.flags |= HM2_PKTUART_CONFIG_PARITYODD;
			inst->cfg_tx.flags |= HM2_PKTUART_CONFIG_PARITYODD;
			parity |= 1;
		}
		unsigned stopbits = 1;
		if(inst->mbccb->format & MBCCB_FORMAT_STOPBITS2) {
			inst->cfg_rx.flags |= HM2_PKTUART_CONFIG_STOPBITS2;
			inst->cfg_tx.flags |= HM2_PKTUART_CONFIG_STOPBITS2;
			stopbits = 2;
		}
		inst->hal->parity   = parity;
		inst->hal->stopbits = stopbits;
		if(!inst->mbccb->rxdelay)	// Auto
			inst->hal->rxdelay = inst->cfg_rx.ifdelay = calc_ifdelay(inst, inst->mbccb->baudrate, parity, stopbits) - 1;
		else	// Manual
			inst->hal->rxdelay = inst->cfg_rx.ifdelay = inst->mbccb->rxdelay;

		if(!inst->mbccb->txdelay)	// Auto
			inst->hal->txdelay = inst->cfg_tx.ifdelay = calc_ifdelay(inst, inst->mbccb->baudrate, parity, stopbits) + 1;
		else	// Manual
			inst->hal->txdelay = inst->cfg_tx.ifdelay = inst->mbccb->txdelay;

		if(!inst->mbccb->drvdelay)	// Auto
			inst->hal->drvdelay = inst->cfg_tx.drivedelay = 1;
		else	// Manual
			inst->hal->drvdelay = inst->cfg_tx.drivedelay = inst->mbccb->drvdelay;

		inst->cfg_rx.filterrate = 0;	// Zero means 2 times baudrate
		inst->cfg_rx.flags |= HM2_PKTUART_CONFIG_RXEN;
		if(!(inst->mbccb->format & MBCCB_FORMAT_DUPLEX))
			inst->cfg_rx.flags |= HM2_PKTUART_CONFIG_RXMASKEN;	// Set rx masking if half-duplex
		// FIXME: 'Drive Auto' has precedence over 'Drive Enable'. It should
		// work for both 2-wire and 4-wire setups. We could remove the enable.
		inst->cfg_tx.flags |= HM2_PKTUART_CONFIG_DRIVEEN | HM2_PKTUART_CONFIG_DRIVEAUTO;

		if((retval = hm2_pktuart_get_version(inst->uart)) < 0) {
			MSG_ERR("%s: error: Cannot get PktUART version (error=%d)\n", inst->name, retval);
			goto errout;
		}
		inst->rxversion = (retval >> 4) & 0x0f;
		inst->txversion = retval & 0x0f;

		// Older versions have no 2 stopbits, a bug in inter-frame length, no
		// extended inter-frame length setting and no max. inter-character
		// timing measurement.
		if(inst->rxversion < 3 || inst->txversion < 3) {
			if(inst->rxversion < 2 || inst->txversion < 2) {
				MSG_ERR("%s: error: The driver does not support PktUART versions before 2 (Rx=%u Tx=%u), aborting.\n",
						inst->name, inst->rxversion, inst->txversion);
				goto errout;
			}
			MSG_WARN("%s: warning: PktUART version is less than 3 (Rx=%u Tx=%u). Please consider upgrading.\n",
					inst->name, inst->rxversion, inst->txversion);
			if(stopbits > 1) {
				MSG_WARN("%s: warning: Old PktUART cannot set two stop-bits. Setting to one.\n", inst->name);
				stopbits = 1;
			}
			if(inst->txversion < 3 && inst->cfg_tx.ifdelay > 0xff) {
				MSG_WARN("%s: warning: Old PktUART cannot set txdelay to 0x%04x. Clamping to 0xff.\n",
						inst->name, inst->cfg_tx.ifdelay);
				inst->cfg_tx.ifdelay = 0xff;
			}
			if(inst->rxversion < 3 && inst->cfg_rx.ifdelay > 0xff) {
				MSG_WARN("%s: warning: Old PktUART cannot set rxdelay to 0x%04x. Clamping to 0xff.\n",
						inst->name, inst->cfg_rx.ifdelay);
				inst->cfg_rx.ifdelay = 0xff;
			}
			if(inst->rxversion < 3 && inst->mbccb->icdelay != 0) {
				MSG_WARN("%s: warning: Old PktUART cannot set icdelay, disabling.\n", inst->name);
				inst->mbccb->icdelay = 0;
			}
		}

		setup_icdelay(inst, inst->mbccb->baudrate, parity, stopbits, inst->mbccb->icdelay);

#ifdef DEBUG
		MSG_INFO("%s: inst->name     : %s\n", inst->name, inst->name);
		MSG_INFO("%s: inst->uart     : %s\n", inst->name, inst->uart);
		MSG_INFO("%s: inst->mbccbsize: %zd\n", inst->name, inst->mbccbsize);
		MSG_INFO("%s: inst->ninit    : %u\n", inst->name, inst->ninit);
		MSG_INFO("%s: inst->ncmds    : %u\n", inst->name, inst->ncmds);
		MSG_INFO("%s: inst->npins    : %u\n", inst->name, inst->npins);
		MSG_INFO("%s: inst->icdelay  : %u\n", inst->name, inst->maxicharbits);
		MSG_INFO("%s: inst->rxdelay  : %u\n", inst->name, inst->cfg_rx.ifdelay);
		MSG_INFO("%s: inst->txdelay  : %u\n", inst->name, inst->cfg_tx.ifdelay);
		MSG_INFO("%s: inst->drvdelay : %u\n", inst->name, inst->cfg_tx.drivedelay);
#endif

		MSG_INFO("%s: PktUART serial configured to 8%c%c@%d\n",
					inst->name,
					parity ? (parity == 2 ? 'E' : 'O') : 'N',
					stopbits > 1 ? '2' : '1',
					inst->cfg_rx.baudrate);

		*(inst->hal->fault)     = 0;
		*(inst->hal->faultcmd)  = 0;
		*(inst->hal->lasterror) = 0;
		*(inst->hal->suspend)   = 0 != (inst->mbccb->format & MBCCB_FORMAT_SUSPEND);
		// Start with internal suspended bit set. The HAL pin will dictate to
		// move to the active state in the process() function. This enables to
		// use the WFLUSH setting each time we come out of suspend and,
		// especially, the first process() invocation will honor the WFLUSH if
		// we do not start suspended because the HAL pin will be different from
		// the current 'suspended' setting.
		inst->suspended = 1;

		unsigned p = 0;
#define CPTR(x)	((const char *)((x) + 1))
		for(unsigned c = 0; c < inst->ncmds; c++) {
			// First create command status pins
			CHECK(hal_pin_bit_newf(HAL_OUT, &(inst->hal->cmds[c].disabled),
					comp_id, "%s.command.%02d.disabled", inst->name, c));
			CHECK(hal_pin_u32_newf(HAL_OUT, &(inst->hal->cmds[c].error),
					comp_id, "%s.command.%02d.errors", inst->name, c));
			CHECK(hal_pin_u32_newf(HAL_OUT, &(inst->hal->cmds[c].errorcode),
					comp_id, "%s.command.%02d.error-code", inst->name, c));
			CHECK(hal_pin_bit_newf(HAL_IN, &(inst->hal->cmds[c].reset),
					comp_id, "%s.command.%02d.reset", inst->name, c));

			// Now create the pins associated with the command
			hm2_modbus_cmd_t *cc = &inst->_cmds[c];
			int dir = HAL_IN;
			const rtapi_u8 *dptr = inst->dataptr + cc->cmd.cdataptr;
			cc->pinref = p;
			for(int j = 0; j < cc->cmd.cpincnt; j++) {
				switch(cc->cmd.func) {
				case MBCMD_R_COILS:
				case MBCMD_R_INPUTS:
					dir = HAL_OUT;
					/* Fallthrough */
				case MBCMD_W_COIL:
				case MBCMD_W_COILS:
					CHECK(hal_pin_bit_newf(dir, (hal_bit_t**)&(inst->hal->pins[p++]),
							comp_id, "%s.%s", inst->name, CPTR(dptr)));
					break;

				case MBCMD_R_INPUTREGS:
				case MBCMD_R_REGISTERS:
					dir = HAL_OUT;
					/* Fallthrough */
				case MBCMD_W_REGISTER:	// This has guaranteed pincnt == 1
				case MBCMD_W_REGISTERS:
					switch(cc->typeptr[j].htype) {
					default:
					case HAL_BIT:
						CHECK(hal_pin_bit_newf(dir, (hal_bit_t**)&(inst->hal->pins[p++]),
								comp_id, "%s.%s", inst->name, CPTR(dptr)));
						break;

					case HAL_U32:
						CHECK(hal_pin_u32_newf(dir, (hal_u32_t**)&(inst->hal->pins[p++]),
								comp_id, "%s.%s", inst->name, CPTR(dptr)));
						break;

					case HAL_U64:
						CHECK(hal_pin_u64_newf(dir, (hal_u64_t**)&(inst->hal->pins[p++]),
								comp_id, "%s.%s", inst->name, CPTR(dptr)));
						break;

					case HAL_S32:
						CHECK(hal_pin_s32_newf(dir, (hal_s32_t**)&(inst->hal->pins[p]),
								comp_id, "%s.%s", inst->name, CPTR(dptr)));
						if(haspinscale(&cc->typeptr[j])) {
							CHECK(hal_pin_float_newf(HAL_IN, &(inst->hal->pins[p].scale),
									comp_id, "%s.%s.scale", inst->name, CPTR(dptr)));
							*(inst->hal->pins[p].scale) = 1.0;
							if(HAL_OUT == dir) {
								CHECK(hal_pin_float_newf(HAL_OUT, &(inst->hal->pins[p].scaled),
										comp_id, "%s.%s.scaled", inst->name, CPTR(dptr)));
								switch(mtypetype(cc->typeptr[j].mtype)) {
								case MBT_U:
									CHECK(hal_pin_u64_newf(HAL_IN, (hal_u64_t**)&(inst->hal->pins[p].offset),
											comp_id, "%s.%s.offset", inst->name, CPTR(dptr)));
									break;
								case MBT_S:
									CHECK(hal_pin_s64_newf(HAL_IN, (hal_s64_t**)&(inst->hal->pins[p].offset),
											comp_id, "%s.%s.offset", inst->name, CPTR(dptr)));
									break;
								case MBT_F:
									CHECK(hal_pin_float_newf(HAL_IN, (hal_float_t**)&(inst->hal->pins[p].offset),
											comp_id, "%s.%s.offset", inst->name, CPTR(dptr)));
									break;
								}
							} else {
								CHECK(hal_pin_s32_newf(HAL_IN, (hal_s32_t**)&(inst->hal->pins[p].offset),
										comp_id, "%s.%s.offset", inst->name, CPTR(dptr)));
							}
						}
						p++;
						break;
					case HAL_S64:
						CHECK(hal_pin_s64_newf(dir, (hal_s64_t**)&(inst->hal->pins[p]),
								comp_id, "%s.%s", inst->name, CPTR(dptr)));
						if(haspinscale(&cc->typeptr[j])) {
							CHECK(hal_pin_float_newf(HAL_IN, &(inst->hal->pins[p].scale),
									comp_id, "%s.%s.scale", inst->name, CPTR(dptr)));
							*(inst->hal->pins[p].scale) = 1.0;
							if(HAL_OUT == dir) {
								CHECK(hal_pin_float_newf(HAL_OUT, &(inst->hal->pins[p].scaled),
										comp_id, "%s.%s.scaled", inst->name, CPTR(dptr)));
								switch(mtypetype(cc->typeptr[j].mtype)) {
								case MBT_U:
									CHECK(hal_pin_u64_newf(HAL_IN, (hal_u64_t**)&(inst->hal->pins[p].offset),
											comp_id, "%s.%s.offset", inst->name, CPTR(dptr)));
									break;
								case MBT_S:
									CHECK(hal_pin_s64_newf(HAL_IN, (hal_s64_t**)&(inst->hal->pins[p].offset),
											comp_id, "%s.%s.offset", inst->name, CPTR(dptr)));
									break;
								case MBT_F:
									CHECK(hal_pin_float_newf(HAL_IN, (hal_float_t**)&(inst->hal->pins[p].offset),
											comp_id, "%s.%s.offset", inst->name, CPTR(dptr)));
									break;
								}
							} else {
								CHECK(hal_pin_s64_newf(HAL_IN, (hal_s64_t**)&(inst->hal->pins[p].offset),
										comp_id, "%s.%s.offset", inst->name, CPTR(dptr)));
							}
						}
						p++;
						break;
					case HAL_FLOAT:
						CHECK(hal_pin_float_newf(dir, (hal_float_t**)&(inst->hal->pins[p]),
								comp_id, "%s.%s", inst->name, CPTR(dptr)));
						if(haspinscale(&cc->typeptr[j])) {
							CHECK(hal_pin_float_newf(HAL_IN, &(inst->hal->pins[p].scale),
									comp_id, "%s.%s.scale", inst->name, CPTR(dptr)));
							*(inst->hal->pins[p].scale) = 1.0;
							if(HAL_OUT == dir) {
								CHECK(hal_pin_float_newf(HAL_OUT, &(inst->hal->pins[p].scaled),
										comp_id, "%s.%s.scaled", inst->name, CPTR(dptr)));
								switch(mtypetype(cc->typeptr[j].mtype)) {
								case MBT_U:
									CHECK(hal_pin_u64_newf(HAL_IN, (hal_u64_t**)&(inst->hal->pins[p].offset),
											comp_id, "%s.%s.offset", inst->name, CPTR(dptr)));
									break;
								case MBT_S:
									CHECK(hal_pin_s64_newf(HAL_IN, (hal_s64_t**)&(inst->hal->pins[p].offset),
											comp_id, "%s.%s.offset", inst->name, CPTR(dptr)));
									break;
								case MBT_F:
									CHECK(hal_pin_float_newf(HAL_IN, (hal_float_t**)&(inst->hal->pins[p].offset),
											comp_id, "%s.%s.offset", inst->name, CPTR(dptr)));
									break;
								}
							} else {
								CHECK(hal_pin_float_newf(HAL_IN, (hal_float_t**)&(inst->hal->pins[p].offset),
										comp_id, "%s.%s.offset", inst->name, CPTR(dptr)));
							}
							inst->hal->pins[p].offset->f = 0.0;
						}
						p++;
						break;
					}
					break;
				}
				dptr += *dptr + 1;	// Add length byte to get to next data record/pin name
			}
		}
#undef CPTR
#undef CHECK

		inst->state = STATE_START;

		// Configure PktUART immediately and flush the fifos
		inst->cfg_rx.flags |= HM2_PKTUART_CONFIG_FLUSH;
		inst->cfg_tx.flags |= HM2_PKTUART_CONFIG_FLUSH;
		if((retval = hm2_pktuart_config(inst->uart, &inst->cfg_rx, &inst->cfg_tx, 0)) < 0) {
			MSG_ERR("%s: error: PktUART setup problem: error=%d\n", inst->name, retval);
			goto errout;
		}
		inst->cfg_rx.flags &= ~HM2_PKTUART_CONFIG_FLUSH;
		inst->cfg_tx.flags &= ~HM2_PKTUART_CONFIG_FLUSH;
	}
	hal_ready(comp_id);
	return 0;

errout:
	cleanup();
	return retval;
}

void rtapi_app_exit(void)
{
	cleanup();
}
// vim: syn=c ts=4
