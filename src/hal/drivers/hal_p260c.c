/*
 HAL driver to talk to P260C I/O boards on an RS-485 chain.




*/

//#define NO_SERIAL 1
#define OPEN_ON_ERROR 1

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */

#include "hal.h"		/* HAL public API decls */

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <linux/serial.h>

#if !defined(BUILD_SYS_USER_DSO) 
#error "This driver is for usermode threads only"
#endif

#define MODNAME "hal_p260c"

/* module information */
MODULE_AUTHOR("Colin Whittaker for PMC Stone, Inc.");
MODULE_DESCRIPTION("Driver for P260C boards in a RS-485 string. Version 1.2");
MODULE_LICENSE("GPL");

#define MAX_BOARDS 16
#define INPUT_PINS 16
#define OUTPUT_PINS 16


//#define DEBUG_RX 1

typedef struct _board_s {
	// Board physical address setting
	u8   address;

	// HAL Pins
	hal_bit_t *input_pins[INPUT_PINS];
	hal_bit_t *output_pins[INPUT_PINS];
	// Status data
	int        invalid_timer;             // count since last error
	int        count_errors;              // count of errors per time if >X then comm_error
	hal_s32_t *invalidcnt;                // s32 total count of invalid reads
	hal_bit_t *comm_error;                // Currently in communication error
	hal_bit_t *permanent_error;           // Triggered permanet error ( must be reset )
	// Debug data
#ifdef DEBUG_RX
	hal_s32_t *writecnt;                  // s32 count of write calls
	hal_s32_t *readbytes;                 // s32 count of read bytes
	hal_s32_t *validcnt;                  // s32 count of valid read
	hal_s32_t *readbeforewritecount;
	hal_s32_t *readcount;
	hal_s32_t *read0;
	hal_s32_t *read1;
	hal_s32_t *read2;
	hal_s32_t *read3;
	hal_s32_t *maxreadtime;
#endif

	// Protocol data structures
	u16  output_bits;
	u8   output_data[4];

	u16  input_bits;
	u8   input_data[16];
	int  input_cnt;
	int  input_valid;

} board_t;

static const char *modname = MODNAME;

static char *addrs;
RTAPI_MP_STRING( addrs, "board addresses, comma separated.  0-F");

#ifndef NO_SERIAL
static char *tty;
RTAPI_MP_STRING( tty, "Serial port name, /dev/ttyUSB0");
static char *tty_debug;
RTAPI_MP_STRING( tty_debug, "Serial port name, /dev/ttyUSB1");
#endif

static int comp_id; 

unsigned long runtime;
unsigned long threadtime;

typedef struct _mod_status {
	hal_s32_t *maxruntime;
	hal_s32_t *minruntime;
	hal_s32_t *maxreadtime;
	hal_s32_t *maxwritetime;

	hal_s32_t *writecnt;                  // s32 count of write calls
	
	hal_bit_t *comm_error;               // Currently some board has a communication error
	hal_bit_t *permanent_error;          // Permanent error triggered by comm_error ( Must be reset )
	hal_bit_t *reset_permanent;          // Input bit to reset permanent error

	// Parameters
	hal_s32_t clear_comm_count;
	hal_s32_t set_perm_count;
	hal_s32_t min_tx_boards;
	hal_s32_t max_rx_wait;

	hal_bit_t debug_on_error;
} mod_status_t;

//#define ERROR_CLEAR_TIME     10  // 10 cycles without an error to clear the comm_error
//#define MAX_ERRORS_PER_TIME  5   // 5 errors without 10 clean, count as a comm_error

mod_status_t *mstat;

static int num_boards = 0;
board_t *boards;

static int sfd;
static int debug_fd;

static void serial_port_task( void *arg, long period );

#ifndef NO_SERIAL
static char *serialdev = "/dev/ttyUSB0";
static void closeserial( int fd );
static int openserial(char *devicename, int baud );
static struct termios oldterminfo;
#endif
//static int gpio48, gpio49;
//static int openGPIO( int number );

#ifdef DEBUG_BB_GPIO
#include "util_bb_gpio.c"
#else
void util_bb_init() {}
void write_gpio( int gpio, int on ) {}
void configure_output( int gpio ) {}
#endif

int rtapi_app_main(void) 
{
	int   i, j, retval;
	char *data, *token;
	char  name[HAL_NAME_LEN + 1];


	comp_id = hal_init(modname);
	if(comp_id < 0) 
	{
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_init() failed\n", modname);
		return -1;
	}

	// Allocate board structures.
	boards = hal_malloc( MAX_BOARDS * sizeof( board_t ) );
	if ( boards == NULL )
	{
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_malloc() failed\n", modname);
		hal_exit(comp_id);
		return -1;
	}
	memset( boards, 0, MAX_BOARDS * sizeof( board_t ) );
	// Allocate status structures.
	mstat = hal_malloc( sizeof( mod_status_t ) );
	if ( mstat == NULL )
	{
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: hal_malloc() failed\n", modname);
		hal_exit(comp_id);
		return -1;
	}
	memset( mstat, 0, sizeof( mod_status_t ) );

	// Parse parameters
	if ( addrs != NULL )
	{
		data = addrs;
		while((token = strtok(data, ",")) != NULL) 
		{
			int add = strtol(token, NULL, 16);

			if ( add < 0 || add > 15 )
			{
				rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: address %s = %x is not valid. Only 0-F\n", modname, token, add );
				hal_exit(comp_id);
				return -1;
			}
			boards[num_boards++].address = add;

			data = NULL;
		}
	}
	else
	{
		// No parameteres default to 1 boards address 0
		boards[0].address = 0;
		num_boards = 1;
	}

#ifndef NO_SERIAL
	if ( tty != NULL )
	{
		serialdev = tty;
	}
#endif


	// Open and configure the serial port
#ifdef NO_SERIAL
	sfd = 0;
#else
    sfd = openserial(serialdev,B3000000);
	if ( tty_debug != NULL )
	{
		debug_fd = openserial(tty_debug,B576000);
	}
	else
	{
		debug_fd = 0;
	}
#endif
	if ( sfd < 0 )
	{
		hal_exit(comp_id);
		return -1;
	}

	// Add pins
	for (i=0;i<num_boards;i++)
	{
		int add = boards[i].address;
		for (j=0;j<INPUT_PINS;j++)
		{
			retval = hal_pin_bit_newf(HAL_OUT, &(boards[i].input_pins[j]), comp_id, "%s.%d.pin-%02d-in", modname, add, j+1);
			if(retval < 0) 
			{
				rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin %d.%02d could not export pin, err: %d\n", modname, add, j+1, retval);
				hal_exit(comp_id);
				return -1;
			}

		}
		for (j=0;j<OUTPUT_PINS;j++)
		{
			retval = hal_pin_bit_newf(HAL_IN, &(boards[i].output_pins[j]), comp_id, "%s.%d.pin-%02d-out", modname, add, j+1);
			if(retval < 0) 
			{
				rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin %d.%02d could not export pin, err: %d\n", modname, add, j+1, retval);
				hal_exit(comp_id);
				return -1;
			}

		}

		retval = hal_pin_s32_newf(HAL_IN, &(boards[i].invalidcnt), comp_id, "%s.%d.rx_cnt_error", modname, add );
		if(retval < 0) 
		{
			rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin %d.serial_invalidcnt could not export pin, err: %d\n", modname, add, retval);
			hal_exit(comp_id);
			return -1;
		}
		retval = hal_pin_bit_newf(HAL_OUT, &(boards[i].comm_error), comp_id, "%s.%d.rx_comm_error", modname, add );
		if(retval < 0) 
		{
			rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin %d.comm_error could not export pin, err: %d\n", modname, add, retval);
			hal_exit(comp_id);
			return -1;
		}
		retval = hal_pin_bit_newf(HAL_OUT, &(boards[i].permanent_error), comp_id, "%s.%d.rx_perm_error", modname, add );
		if(retval < 0) 
		{
			rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin %d.permanent_error could not export pin, err: %d\n", modname, add, retval);
			hal_exit(comp_id);
			return -1;
		}

		// Debug
#ifdef DEBUG_RX
		hal_pin_s32_newf(HAL_IN, &(boards[i].writecnt), comp_id, "%s.%d.serial_writecnt", modname, add );
		hal_pin_s32_newf(HAL_IN, &(boards[i].readbytes), comp_id, "%s.%d.serial_readbytes", modname, add );
		hal_pin_s32_newf(HAL_IN, &(boards[i].validcnt), comp_id, "%s.%d.serial_validcnt", modname, add );
		hal_pin_s32_newf(HAL_IN, &(boards[i].readbeforewritecount), comp_id, "%s.%d.serial_readbeforewrcnt", modname, add );
		hal_pin_s32_newf(HAL_IN, &(boards[i].readcount), comp_id, "%s.%d.serial_readcnt", modname, add );
		hal_pin_s32_newf(HAL_IN, &(boards[i].read0), comp_id, "%s.%d.serial_read0", modname, add );
		hal_pin_s32_newf(HAL_IN, &(boards[i].read1), comp_id, "%s.%d.serial_read1", modname, add );
		hal_pin_s32_newf(HAL_IN, &(boards[i].read2), comp_id, "%s.%d.serial_read2", modname, add );
		hal_pin_s32_newf(HAL_IN, &(boards[i].read3), comp_id, "%s.%d.serial_read3", modname, add );
		hal_pin_s32_newf(HAL_IN, &(boards[i].maxreadtime), comp_id, "%s.%d.serial_maxreadtime", modname, add );
#endif                                       
	}
	retval = hal_pin_s32_newf(HAL_IN, &(mstat->maxreadtime), comp_id, "%s.sys_max_read", modname );
	if(retval < 0) 
	{
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin maxreadtime could not export pin, err: %d\n", modname, retval);
		hal_exit(comp_id);
		return -1;
	}
	retval = hal_pin_s32_newf(HAL_IN, &(mstat->maxwritetime), comp_id, "%s.sys_max_write", modname );
	if(retval < 0) 
	{
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin maxwritetime could not export pin, err: %d\n", modname, retval);
		hal_exit(comp_id);
		return -1;
	}
	retval = hal_pin_s32_newf(HAL_IN, &(mstat->writecnt), comp_id, "%s.sys_writecnt", modname );
	if(retval < 0) 
	{
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin writecnt could not export pin, err: %d\n", modname, retval);
		hal_exit(comp_id);
		return -1;
	}
	retval = hal_pin_bit_newf(HAL_OUT, &(mstat->comm_error), comp_id, "%s.rx_comm_error", modname );
	if(retval < 0) 
	{
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin comm_error could not export pin, err: %d\n", modname, retval);
		hal_exit(comp_id);
		return -1;
	}
	retval = hal_pin_bit_newf(HAL_OUT, &(mstat->permanent_error), comp_id, "%s.rx_perm_error", modname );
	if(retval < 0) 
	{
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin permanent_error could not export pin, err: %d\n", modname, retval);
		hal_exit(comp_id);
		return -1;
	}
	retval = hal_pin_bit_newf(HAL_IN, &(mstat->reset_permanent), comp_id, "%s.rx_reset_error", modname );
	if(retval < 0) 
	{
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: pin reset_permanent could not export pin, err: %d\n", modname, retval);
		hal_exit(comp_id);
		return -1;
	}

	// Parameters
	retval = hal_param_s32_newf(HAL_RW, &(mstat->clear_comm_count), comp_id, "%s.clear_comm_count", modname );
	if(retval < 0) 
	{
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: param clear_comm_count could not create, err: %d\n", modname, retval);
		hal_exit(comp_id);
		return -1;
	}
	retval = hal_param_s32_newf(HAL_RW, &(mstat->set_perm_count), comp_id, "%s.set_perm_count", modname );
	if(retval < 0) 
	{
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: param set_perm_count could not create, err: %d\n", modname, retval);
		hal_exit(comp_id);
		return -1;
	}
	retval = hal_param_s32_newf(HAL_RW, &(mstat->min_tx_boards), comp_id, "%s.minimum_tx", modname );
	if(retval < 0) 
	{
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: param minimum_tx could not create, err: %d\n", modname, retval);
		hal_exit(comp_id);
		return -1;
	}
	retval = hal_param_s32_newf(HAL_RW, &(mstat->max_rx_wait), comp_id, "%s.max_rx_wait", modname );
	if(retval < 0) 
	{
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: param minimum_tx could not create, err: %d\n", modname, retval);
		hal_exit(comp_id);
		return -1;
	}
	retval = hal_param_bit_newf(HAL_RW, &(mstat->debug_on_error), comp_id, "%s.debug_on_error", modname );
	if(retval < 0) 
	{
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: param debug_on_error could not create, err: %d\n", modname, retval);
		hal_exit(comp_id);
		return -1;
	}

	mstat->set_perm_count = 5;
	mstat->clear_comm_count = 10;
	mstat->min_tx_boards = 6;
	mstat->max_rx_wait = 5000000;
	mstat->debug_on_error = 0;

	// Debug
	util_bb_init();
	configure_output( 915 );
	configure_output( 923 );
//	gpio48 = openGPIO( 48 );
//	gpio49 = openGPIO( 49 );

	// Export the run thread function
	rtapi_snprintf( name, sizeof(name), "%s.refresh", modname );
	retval = hal_export_funct( name, serial_port_task, 0, 0, 0, comp_id);
	if(retval < 0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: refresh funct export failed\n", modname);
		hal_exit(comp_id);
		return -1;
	}

	rtapi_print_msg(RTAPI_MSG_INFO, "%s: installed driver\n", modname);

	hal_ready(comp_id);

	return 0;
}

void rtapi_app_exit(void)
{
#ifndef NO_SERIAL
	closeserial( sfd );
#endif

	hal_exit(comp_id);
}

/***********************************************************************
Implementation
*/
#ifndef NO_SERIAL
static int openserial(char *devicename, int baud)
{
	int            fd = 0;
    struct termios options;

    if ((fd = open(devicename, O_RDWR | O_NONBLOCK | O_NOCTTY)) <0) {
		rtapi_print_msg(RTAPI_MSG_ERR, "%s: ERROR: Failed to open %s\n", modname, devicename);
    }

    if (tcgetattr(fd, &oldterminfo) == -1) {
       	rtapi_print_msg(RTAPI_MSG_ERR,"openserial(): tcgetattr()");
        return -1;
    }
    options = oldterminfo;
	rtapi_print_msg(RTAPI_MSG_INFO, "termios\niflag = %08X\noflag = %08X\ncflag = %08X\nlflag = %08X\n",
		options.c_iflag, options.c_oflag, options.c_cflag, options.c_lflag );
	
	// Ignore input break
	// Check parity
	options.c_iflag = IGNBRK;
	options.c_iflag |= IGNPAR;
	options.c_oflag = 0;

	// CLOCAL means don't allow
	// control of the port to be changed
	// CREAD says to enable the receiver
	// CS8 means 8-bits per work
	// PARENB is enable parity bit
	// so this disables the parity bit
	// CSTOPB means 2 stop bits
	// otherwise (in this case)
	// only one stop bit
	options.c_cflag = 0;
	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	options.c_cflag |= PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_lflag &= ~(ISIG|ICANON|ECHO|ECHOE|ECHOK|ECHONL|NOFLSH|TOSTOP|ECHOCTL|ECHOKE|FLUSHO);

	// set the read and write speed to
	// All speeds can be prefixed with B
	// as a settings.
	cfsetispeed(&options, baud );
	cfsetospeed(&options, baud );



	// Set the timeouts
	options.c_cc[VMIN] = 1;

	// The amount of time to wait
	// for the amount of data
	// specified by VMIN in tenths
	// of a second.
	options.c_cc[VTIME] = 0;

    if (tcflush(fd, TCIOFLUSH) == -1) {
       	rtapi_print_msg(RTAPI_MSG_ERR,"openserial(): tcflush()");
        return -1;
    }
    if (tcsetattr(fd, TCSANOW, &options) == -1) {
       	rtapi_print_msg(RTAPI_MSG_ERR,"initserial(): tcsetattr()");
        return -1;
    }

	return fd;
}
static void closeserial( int fd )
{
    tcsetattr(fd, TCSANOW, &oldterminfo);
	close(fd);
}

int setRTS(int fd, int level)
{
    int status;

    if (ioctl(fd, TIOCMGET, &status) == -1) {
        return 0;
    }
    if (level)
        status |= TIOCM_RTS;
    else
        status &= ~TIOCM_RTS;

    if (ioctl(fd, TIOCMSET, &status) == -1) {
        return 0;
    }
    return 1;
}
int setDTR(int fd, int level)
{
    int status;

    if (ioctl(fd, TIOCMGET, &status) == -1) {
        return 0;
    }
    if (level)
        status |= TIOCM_DTR;
    else
        status &= ~TIOCM_DTR;

    if (ioctl(fd, TIOCMSET, &status) == -1) {
        return 0;
    }
    return 1;
}

#endif

static void set_debug( int pin, int val )
{
	if ( pin == 0 )
	{
		write_gpio( 915, val );
		if ( debug_fd != 0 )
		{
			setDTR( debug_fd, val );
		}
	}
	else
	{
		write_gpio( 923, val );
		if ( debug_fd != 0 )
		{
			setRTS( debug_fd, val );
		}
	}
}
/***********************************************************************

 Protocol task functions

***/

/* static void flush_input( int board ) */
/* { */
/* 	unsigned char inp; */
/* 	int cnt = 0; */

/* 	// Flush the input data first */
/* 	while ( read( sfd, &inp, 1 ) > 0 && cnt++ < 1000 ) */
/* 	{ */
/* 		if ( debug_fd ) */
/* 		{ */
/* 			write( debug_fd, &inp, 1 ); */
/* 		} */
/* 	} */

/* #ifdef DEBUG_RX */
/* 	if ( boards[board].readbeforewritecount != NULL ) */
/* 	{ */
/* 		*(boards[board].readbeforewritecount) = cnt; */
/* 	} */
/* #endif */
/* 	if ( cnt ) */
/* 	{ */
/* 		set_debug( 0, 0 ); */
/* 		set_debug( 0, 1 ); */
/* 	} */
/* } */

static void wait_tx_empty()
{
	// Wait for write to be complete.
	tcdrain(sfd);
}

static void send_data( int board )
{
#ifdef DEBUG_RX
	/* flush_input( board ); */
#endif

	// Write protocol data
#ifndef NO_SERIAL
	write( sfd, boards[board].output_data, 4 );
#endif

#ifdef DEBUG_RX
	if ( boards[board].writecnt != NULL )
	{
		*(boards[board].writecnt) = *(boards[board].writecnt) + 1;
	}
#endif

}

static unsigned char nibble_xsum( unsigned char *data )
{
	unsigned char xsum;
	int i;

	xsum = 0;
	for ( i=0; i<4; i++ )
	{
		xsum = xsum ^ (data[i]>>4);
		xsum = xsum ^ (data[i] & 0xf );
	}

	return xsum;
}

static u8 validate_input_buffer( u8 *input_data, u16 *bits )
{
	u8 input_address = input_data[0] >> 3;

	if ( nibble_xsum( input_data ) == 0 )
	{
		if ( (input_data[0] & 0x7) == 2 )
		{
			if ( (input_data[3] & 0xF) == 3 )
			{
				*bits = (input_data[1]<<8) & 0xFF00;
				*bits |= (input_data[2] & 0xFF);

				return input_address;
			}
		}
	}
	return -1;
}

//
// Count RX valids.
//   gets called once per cycle for each board.
// 
static int read_counts( int board )
{
	int ret = 0;

	if ( mstat->debug_on_error )
	{
		if ( tty_debug != NULL && debug_fd != 0 )
		{
			closeserial(debug_fd);
			debug_fd = 0;
		}
	}


	if ( boards[board].input_valid )
	{
		// Good data
		if ( boards[board].invalid_timer >= mstat->clear_comm_count )
		{
			// Max out
			boards[board].invalid_timer = mstat->clear_comm_count;
			// Clear error counter
			boards[board].count_errors = 0;
			// Clear comm error
			if ( boards[board].comm_error != NULL )
			{
				*(boards[board].comm_error) = 0;
			}
		}
#ifdef DEBUG_RX
		if ( boards[board].validcnt != NULL )
		{
			*(boards[board].validcnt) = *(boards[board].validcnt) + 1;
		}
#endif
	}
	else
	{
		ret=1;
		if ( mstat->debug_on_error )
		{
			if ( tty_debug != NULL && debug_fd == 0 )
			{
				debug_fd = openserial(tty_debug,B576000);
				mstat->debug_on_error = 0;
			}
		}
		set_debug( 1, 1 );

		// new error
		boards[board].count_errors++;
		// reset timer
		boards[board].invalid_timer = 0;
		// Total Count
		if ( boards[board].invalidcnt != NULL )
		{
			*(boards[board].invalidcnt) = *(boards[board].invalidcnt) + 1;
		}
	}
	if ( boards[board].count_errors >= mstat->set_perm_count )
	{
		// Max out.
		boards[board].count_errors = mstat->set_perm_count;
		// Set comm error
		if ( boards[board].comm_error != NULL )
		{
			*(boards[board].comm_error) = 1;
		}
		// Go into permenant error
		if ( boards[board].permanent_error != NULL )
		{
			*(boards[board].permanent_error) = 1;
		}
	}

	boards[board].invalid_timer++;

	return ret;
}
//
// Read for a maximum of 8 bytes time and timeout
// 8 bytes at 3,000,000 baud is about 30 usec
// wait for 1/2 msec since the usb turn around is long.
//

unsigned long last_readtime;

static void read_all_data()
{
	unsigned long t0, t1;
	unsigned char inp=0;
	int           i, cnt=0;
	int           valid = 0;
	int           input_cnt = 0;
	int           input_idx = 0;
	u8            input_data[256];
	u8            input_address;
	u16           bits;

	t1 = t0 = rtapi_get_time();

#ifdef DEBUG_RX
	if ( boards[0].readcount != NULL )
	{
		*(boards[0].readcount) = 0;
	}
#endif

	while ( t1-t0 < mstat->max_rx_wait && input_idx < 255 )
	{
#ifndef NO_SERIAL
		cnt = read( sfd, &inp, 1 );
#endif
		if ( cnt > 0 )
		{
			set_debug( 0, 1 );
			input_data[input_idx++] = inp;
			input_cnt++;
			// Count all bytes in the first board
#ifdef DEBUG_RX
			if ( boards[0].readbytes != NULL )
			{
				*(boards[0].readbytes) = *(boards[0].readbytes) + 1;
			}
			if ( boards[0].readcount != NULL )
			{
				*(boards[0].readcount) = *(boards[0].readcount) + 1;
			}
#endif
			if ( input_cnt >= 4 )
			{
				if ( (input_address=validate_input_buffer( &input_data[input_idx - 4], &bits )) >= 0 )
				{
					// Find the board for this data
					for ( i=0;i<num_boards;i++)
					{
						if ( boards[i].address == input_address )
						{
							boards[i].input_bits  = bits;
							boards[i].input_valid = 1;
							boards[i].input_cnt = input_cnt;
							valid++;
							break;
						}
					}
					// Check for no boards...
					if ( i == num_boards)
					{
						// error, valid data for a board we don't have.
					}
					input_cnt = 0;
				}
			}

			// reset timeout
			t0 = rtapi_get_time();
			
			// Check max time between reads
			if ( last_readtime && *(mstat->maxreadtime) < (t0-last_readtime) )
			{
				*(mstat->maxreadtime) = (t0-last_readtime);
			}
			last_readtime = t0;
		}
		else // cnt > 0
		{
			// Are we done....
			if ( valid >= num_boards)
			{
				break;
			}
		}

		t1 = rtapi_get_time();
	}

	set_debug( 0, 0 );

	valid = 0;
	for ( i=0;i<num_boards;i++)
	{
		valid += read_counts( i );
	}
//	if ( valid )

	// Send data to debug
//	if (0)
	{
		if ( debug_fd != 0 )
		{
			write( debug_fd, input_data, input_idx );
		}
	}
}

// get HAL pins and set data
static void set_output( int board )
{
	int i, bit;

	// Get bits from HAL pins
	bit = 1;
	for (i=0;i<OUTPUT_PINS;i++)
	{
		if ( *(boards[board].output_pins[i]) )
		{
			boards[board].output_bits |= bit;
		}
		else
		{
			boards[board].output_bits &= ~bit;
		}

		bit = bit<<1;
	}

	// If error, reset outputs
	if ( *mstat->permanent_error )
	{
		boards[board].output_bits = 0;
	}

	// Build protocol bytes
	boards[board].output_data[ 0 ] = (boards[board].address << 3);
	boards[board].output_data[ 0 ] |= 0x3;

	boards[board].output_data[ 1 ] = (boards[board].output_bits >> 8) & 0xFF;
	boards[board].output_data[ 2 ] = boards[board].output_bits & 0xFF;
	boards[board].output_data[ 3 ] = 0x3;
	boards[board].output_data[ 3 ] |= ( nibble_xsum( boards[board].output_data ) << 4 );
}

static void set_input( int board )
{
	int i, bit;

	// If error, clear inputs
	if ( *mstat->permanent_error )
	{
		boards[board].input_bits = 0;
	}

	bit = 1;
	for(i=0;i<INPUT_PINS;i++)
	{
		if ( boards[board].input_bits & bit )
		{
			*(boards[board].input_pins[i]) = 1;
		}
		else
		{
			*(boards[board].input_pins[i]) = 0;
		}

		bit = bit<<1;
	}
}

static void handle_errors( void )
{
	int i, err;
	// Handle error reset
	if ( *mstat->reset_permanent )
	{
		*mstat->reset_permanent = 0;

		if ( ! *mstat->comm_error )
		{
			*mstat->permanent_error = 0;
			for ( i=0;i<num_boards;i++)
			{
				*(boards[i].permanent_error) = 0;
			}
		}
	}
	// Check for errors
	err = 0;
	for ( i=0;i<num_boards;i++)
	{
		if ( *(boards[i].comm_error) )
		{
			err = 1;
		}
	}
	*mstat->comm_error = err;
	if ( err )
	{
		*mstat->permanent_error = 1;
		set_debug( 1, 1 );
	}
}

static void serial_port_task( void *arg, long period )
{
	unsigned long        t0, t1;
	int                  i;
	char dat[4];

	memset( dat,0, 4);

	t0 = rtapi_get_time();

	// Update max time between thread calls.
	if ( threadtime && *(mstat->maxwritetime) < (t0-threadtime) )
	{
		*(mstat->maxwritetime) = (t0-threadtime);
	}

	// Check to be sure we are at the 10msec time < 9.5msec wait for the next tick.
	if ( (t0-threadtime) < 9500000 )
	{
		return;
	}
	threadtime = t0;

	*mstat->writecnt += 1;

	// Start the transmit to the first board.
	set_debug( 1, 0 );
//	set_debug( 0, 1 );
        /* flush_input( 0 ); */
	set_debug( 0, 0 );

	// Send data to all boards
	for ( i=0;i<num_boards;i++)
	{
		// get pins from user
		set_output( i );

		set_debug( 0, 0 );
		send_data( i );
		set_debug( 0, 1 );

		// Make sure it goes before queuing the next one
		wait_tx_empty();
		set_debug( 0, 0 );

		// Reset receive data flags
		boards[i].input_valid = 0;
		boards[i].input_cnt = 0;
	}
	// Fill in minimum tx data with 0's
	for ( i=num_boards; i<mstat->min_tx_boards; i++)
	{
		set_debug( 0, 0 );
		write( sfd, dat, 4 );
		set_debug( 0, 1 );
		wait_tx_empty();
		set_debug( 0, 0 );
	}

	// Read data from all boards
	read_all_data();

	handle_errors();

	// Set pins to user
	for ( i=0;i<num_boards;i++)
	{
		set_input( i );
	}
	t1 = rtapi_get_time();
	runtime = t1 - t0;

}

