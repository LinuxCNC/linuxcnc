#ifndef __HOSTMOT2_HM2_MODBUS_H
#define __HOSTMOT2_HM2_MODBUS_H

//
// Binary Modbus command config file format:
// - hm2_modbus_mbccb_header_t [1]
// - hm2_modbus_mbccb_cmd_t    [0..x]	Init data
// - hm2_modbus_mbccb_cmd_t    [1..y]	Command sequence
// - data table
//
// Filesize = sizeof(header) + header.initlen + header.cmdlen + header.datalen.
//
#define MBCCB_FORMAT_PARITYEN_BIT     0 // bits 0 Enable parity if set
#define MBCCB_FORMAT_PARITYODD_BIT    1 // bits 1 Odd parity if set
#define MBCCB_FORMAT_STOPBITS2_BIT    2 // bit  2 0=8x1 1=8x2
#define MBCCB_FORMAT_DUPLEX_BIT       3 // bit  3 Set for full-duplex (rx-mask off)
#define MBCCB_FORMAT_SUSPEND_BIT      4 // bit  4 Set if state-machine starts suspended
#define MBCCB_FORMAT_PARITYEN         (1u << MBCCB_FORMAT_PARITYEN_BIT)
#define MBCCB_FORMAT_PARITYODD        (1u << MBCCB_FORMAT_PARITYODD_BIT)
#define MBCCB_FORMAT_STOPBITS2        (1u << MBCCB_FORMAT_STOPBITS2_BIT)
#define MBCCB_FORMAT_DUPLEX           (1u << MBCCB_FORMAT_DUPLEX_BIT)
#define MBCCB_FORMAT_SUSPEND          (1u << MBCCB_FORMAT_SUSPEND_BIT)

// XXX: keep in sync with mesamodbus.py
// Max one minute delay between init commands (in microseconds)
#define MAXDELAY 60000000
// Max chars for a name
#define MAXPINNAME 32
//
// 16*4 byte structure
// All values in Big-Endian
// Must be 32-bit aligned and sizeof() % 4 == 0
typedef struct {
	uint8_t	sig[8];		// Signature and version {'M','e','s','a','M','B','0','1'}
	uint32_t	baudrate;
	uint16_t	format;		// Parity and stopbits
	uint16_t	txdelay;	// Tx inter-frame timeout (t3.5)
	uint16_t	rxdelay;	// Rx inter-frame timeout (t3.5)
	uint16_t	drvdelay;	// Delay from output enable to tx start
	uint16_t	icdelay;	// Rx inter-character timeout (t1.5)
	uint16_t	unused1;
	uint32_t	unused2[7];
	uint32_t	initlen;	// Length of init section
	uint32_t	cmdslen;	// Length of command section
	uint32_t	datalen;	// Length of data table
} hm2_modbus_mbccb_header_t;

// 32 byte structure
// All values in Big-Endian
typedef struct {
	uint8_t	mbid;	// Modbus device ID
	uint8_t	func;	// Function code, 0 for init
	uint16_t	flags;	// Mostly quirks to handle, see MBCCB_CMDF_* defines
	union {
		struct {	// Command fields
			uint16_t	caddr;		// Address
			uint16_t	cpincnt;		// Number of pins
			uint16_t	cregcnt;		// Number of registers
			uint16_t	unusedp1;	// cmds 0 (drvdly)
			uint32_t	unusedp2;	// cmds 0 (icdelay)
			uint32_t	ctypeptr;	// Type and address offset list
			uint32_t	cinterval;	// The interval to repeat this command
			uint32_t	ctimeout;	// Response timeout or delay in microseconds
		};
		struct {	// Init fields
			uint16_t	imetacmd;	// Meta command
			uint16_t	irxdelay;	// init comm change
			uint16_t	itxdelay;	// init comm change
			uint16_t	idrvdelay;	// init comm change
			uint32_t	iicdelay;	// init comm change (unusedp1)
			uint32_t	unusedi1;	// init 0 (typeptr)
			uint32_t	unusedi2;	// init 0 (interval)
			uint32_t	ibaudrate;	// init comm change
		};
	};
	uint32_t	cdataptr; // Pin names, packet data for init
} hm2_modbus_mbccb_cmds_t;

#define MBCCB_CMDF_TIMESOUT  0x0001	// Don't treat timeout as an error
#define MBCCB_CMDF_BCANSWER  0x0002	// Broadcasts will get an answer, ignore it
#define MBCCB_CMDF_NOANSWER  0x0004	// Don't expect an answer
#define MBCCB_CMDF_RESEND    0x0008	// Resend the write even if no pins are changed
#define MBCCB_CMDF_WFLUSH    0x0010	// Don't write initial pin values but flush the output
#define MBCCB_CMDF_DISABLED  0x0020	// Start this command in disabled mode and must be reset
#define MBCCB_CMDF_PARITYEN  0x0100	// Init-only parity change
#define MBCCB_CMDF_PARITYODD 0x0200	// Init-only parity change
#define MBCCB_CMDF_STOPBITS2 0x0400	// Init-only stopbits change
#define MBCCB_CMDF_INITMASK  0x0707	// sum of allowed flags in init
#define MBCCB_CMDF_MASK      0x003f	// sum of allowed normal command flags

#define MBCCB_PINF_SCALE	0x01	// Add scale/offset pins
#define MBCCB_PINF_CLAMP	0x02	// Clamp values to fit target
#define MBCCB_PINF_MASK		0x03	// sum of pin flags

// Type mapping for pins/PDU register data
// Only for R_INPUTREGS, R_REGISTERS, W_REGISTER and W_REGISTERS
typedef struct {
	uint8_t	mtype;	// Modbus type
	uint8_t	htype;	// HAL type
	uint8_t	flags;	// scale and clamp flags (MBCCB_PINF_*)
	uint8_t	regofs;	// PDU register offset for value
} hm2_modbus_mbccb_type_t;

#endif
// vim: ts=4
