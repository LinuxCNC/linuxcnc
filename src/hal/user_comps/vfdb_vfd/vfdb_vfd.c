/*
  vfdb_vfd.c

  userspace HAL program to control a Delta VFD-B VFD

  Yishin Li, adapted from Michael Haberler's vfs11_vfd/.

  Copyright (C) 2007, 2008 Stephen Wille Padnos, Thoth Systems, Inc.
  Copyright (C) 2009,2010,2011,2012 Michael Haberler
  Copyright (C) 2013 Yishin Li

  Based on a work (test-modbus program, part of libmodbus) which is
  Copyright (C) 2001-2005 Stéphane Raimbault <stephane.raimbault@free.fr>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, version 2.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA.

  see 'man vfs11_vfd' and the VFS11 section in the Drivers manual.

  Add is-stopped pin John Thornton

 */


#ifndef ULAPI
#error This is intended as a userspace component only.
#endif

#ifdef DEBUG
#define DBG(fmt, ...)					\
        do {						\
            if (param.debug) printf(fmt,  ## __VA_ARGS__);	\
        } while(0)
#else
#define DBG(fmt, ...)
#endif

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <stdarg.h>

#include "rtapi.h"
#include "hal.h"
#include <modbus.h>
#include <modbus-tcp.h>
#include "inifile.h"

/*
 * VFS-11 parameters:
 *
 * There are dozens of parameters. Some can be stored permanently in EEPROM (setup parameters),
 * some in RAM (operating paramters), and some can be stored both in EEPROM and RAM. The manual
 * is a bit unclear which parameters are RAM/EEPROM/both.
 *
 * There are two communication protocols to talk to the VF-S11, a proprietary but documented
 * "Toshiba Inverter Protocol" (TIP), and a simple Modbus subset. TIP can set EEPROM and RAM
 * parameters and hence can be used for initial inverter configuration. Modbus control can only
 * set operating paramters in RAM. So any setup parameters which you'd like to change (like,
 * e.g. maximum output frequency) need to be set up differently, either through the operating
 * panel, or through a Windows program supplied by Toshiba named PCS001Z.
 *
 * Before using this driver, you need at least change the communication protocol from
 * TIP (default) to Modbus either way.
 *
 * Note from   TOSVERT VF-S11 Communications Function  Instruction Manual:
 *
 * The EEPROM life is 10,000 operations.
 * Do not write in the same parameter that has an EEPROM more than 10,000 times.
 * The communication commands (FA00, FA20, FA26), communication frequency command (FA01),
 * terminal output data (FA50) and analog output data (FA50) are stored in the RAMs only and no re-
 * strictions are placed on them.
 *
 * NB: "analog output data (FA50)" is obviously a typo in the manual, it's really FA51
 */

// command registers for DELTA VFD-B Inverter
#define REG_COMMAND1                    0x2000  // "Communication command" - start/stop, fwd/reverse, DC break, fault reset, panel override
#define REG_COMMAND2                    0xFA20
#define REG_COMMAND3                    0xFA26
#define REG_FREQUENCY                   0x2001  // Set frequency in 0.01Hz steps
#define REG_TERMINAL_OUTPUT             0xFA50
#define REG_ANALOG_OUTPUT               0xFA51
#define REG_UPPERLIMIT                  0x0100  // limit on output frequency in VFD

// bits in register FA00 - main command register
#define CMD_COMMAND_PRIORITY 	0x8000
#define CMD_FREQUENCY_PRIORITY	0x4000
#define CMD_FAULT_RESET		0x2000
#define CMD_EMERGENCY_STOP	0x1000
#define CMD_COAST_STOP		0x0800
#define CMD_RUN			0x0002
#define CMD_STOP               0x0001
#define CMD_REVERSE		0x0020
#define CMD_FORWARD            0x0010
#define CMD_JOG_RUN		0x0003
#define CMD_DC_BRAKE		0x0080
#define CMD_ACCEL_PATTERN_2	0x0040
#define CMD_DISABLE_PI_CONTROL	0x0020
#define CMD_SELECT_MOTOR1_2	0x0010
#define CMD_SPEED_PRESET1	0x0008
#define CMD_SPEED_PRESET2	0x0004
#define CMD_SPEED_PRESET3	0x0002
#define CMD_SPEED_PRESET4	0x0001

// status registers for DELTA VFD-B Inverter
#define SR_ERROR_CODE           0x2100                  //
#define SR_INV_OPSTATUS         0x2101                   //
#define SR_OUTPUT_FREQ          0x2103                   // 0.01Hz units
#define SR_TRIPCODE_PAST1       0x0608          // last 4 trips
#define SR_TRIPCODE_PAST2       0x0609
#define SR_TRIPCODE_PAST3       0x0610
#define SR_TRIPCODE_PAST4       0x0611
#define ST_EMERGENCY_STOPPED    0x0021          // EF1/ESTOP

#define SR_MOTOR_SPEED          0x210C          // RPM
#define SR_TORQUE_RATIO         0x210B          // %
#define SR_OUTPUT_CURRENT       0x2104          // output curr
#define SR_OUTPUT_VOLTAGE       0x2106          // %
#define SR_INVERTER_MODEL	0x0000
#define SR_RATED_CURRENT	0x0001		// 0.1A
#define SR_RATED_VOLTAGE	0x0102		// 0.1V
#define SR_EEPROM_VERSION	0x0006

/* There's an assumption in the gs2_vfd code, namely that the interesting registers
 * are contiguous and all of them can be read with a single read_holding_registers()
 * operation.
 *
 * However, the interesting VF-S11 registers are not contiguous, and must be read
 * one-by-one, because the Toshiba Modbus implementation only supports single-value
 * modbus_read_registers() queries, slowing things down considerably. It seems that
 * other VFD's have similar restrictions.
 *
 * Then, not all registers are equally important. We would like to read the
 * VFD status and actual frequency on every Modbus turnaround, but there is no need to
 * the read CPU version and inverter model more than once at startup, and the load factor etc 
 * every so often. 
 */
#define POLLCYCLES 	10      // read less important parameters only on every 10th transaction
#define MODBUS_MIN_OK	10      // assert the modbus-ok pin after 10 successful modbus transactions
#define MAX_RPM	        12000   // cap output RPM


/* HAL data struct */
typedef struct {
    hal_s32_t   *error_code;
    hal_s32_t 	*status;
    hal_float_t	*freq_cmd;	// frequency command
    hal_float_t	*freq_out;	// actual output frequency
    hal_float_t	*output_volt;	// output voltage
    hal_float_t	*RPM;
    hal_float_t *RPS;
    hal_float_t	*torque_ratio;
    hal_float_t	*output_current;
    hal_float_t *max_rpm;	// calculated based on VFD max frequency setup parameter
    hal_s32_t	*trip_code;
    hal_s32_t	*alarm_code;
    hal_bit_t	*at_speed;	// when drive freq_cmd == freq_out and running
    hal_bit_t	*is_stopped;	// when drive freq out is 0
    hal_bit_t	*estop;		// set estop bit in 0xFA00 - causes 'E trip'
    hal_bit_t	*is_e_stopped;	// true if emergency stop status set in 0xFD00
    hal_bit_t	*modbus_ok;	// the last MODBUS_OK transactions returned successfully
    hal_float_t	*speed_command;	// speed command input

    hal_bit_t	*spindle_on;	// spindle 1=on, 0=off
    hal_bit_t 	*DC_brake;	// setting this will turn off the spindle and engage the DC brake
    hal_bit_t	*spindle_fwd;	// direction, 0=fwd, 1=rev
    hal_bit_t 	*spindle_rev;	// on when in rev and running
    hal_bit_t	*err_reset;	// reset errors when 1  - set fault reset bit in 0xFA00
    hal_bit_t	*jog_mode;	// termed 'jog-run' in manual - might be useful for spindle positioning
    hal_s32_t	*errorcount;    // number of failed Modbus transactions - hints at logical errors

    hal_float_t	looptime;
    hal_float_t	speed_tolerance; 	
    hal_float_t	motor_nameplate_hz;	// speeds are scaled in Hz, not RPM
    hal_float_t	motor_nameplate_RPM;	// nameplate RPM at default Hz
    hal_float_t	rpm_limit;		// do-not-exceed output frequency
    hal_bit_t	*acc_dec_pattern;	// if set: choose ramp times as defined in F500+F501
    // if zero (default): choose ramp times as defined in ACC and DEC
    hal_bit_t	*enabled;		// if set: control VFD via Modbus commands, panel control disabled
    // if zero (default): manual control through panel enabled
    hal_float_t	*upper_limit_hz;		// VFD setup parameter - maximum output frequency in HZ

    hal_bit_t   *max_speed;             // 1: run as fast as possible, ignore unimportant registers
    // link this to spindle.orient-enable for better orient PID loop behaviour
} haldata_t;

// configuration and execution state
typedef struct params {
    int type;
    char *modname;
    int modbus_debug;
    int debug;
    int slave;
    int pollcycles; 
    char *device;
    int baud;
    int bits;
    char parity;
    int stopbits;
    int rts_mode;
    int serial_mode;
    struct timeval response_timeout;
    struct timeval byte_timeout;
    int tcp_portno;
    char *progname;
    char *section;
    FILE *fp;
    char *inifile;
    int reconnect_delay;
    modbus_t *ctx;
    haldata_t *haldata;
    int hal_comp_id;
    int read_initial_done;
    int old_err_reset; 
    uint16_t old_cmd1_reg;		// copy of last write to FA00 */
    int modbus_ok;
    uint16_t failed_reg;		// remember register for failed modbus transaction for debugging
    int	last_errno;
    char *tcp_destip;
    int report_device;
} params_type, *param_pointer;

#define TYPE_RTU 0
#define TYPE_TCP_SERVER 1
#define TYPE_TCP_CLIENT 2

// default options; read from inifile or command line
static params_type param = {
        .type = -1,
        .modname = NULL,
        .modbus_debug = 0,
        .debug = 0,
        .slave = 1,
        .pollcycles = POLLCYCLES,
        .device = "/dev/ttyS0",
        .baud = 19200,
        .bits = 8,
        .parity = 'E',
        .stopbits = 1,
        .serial_mode = -1,
        .rts_mode = -1,
        .response_timeout = { .tv_sec = 0, .tv_usec = 500000 },
        .byte_timeout = {.tv_sec = 0, .tv_usec = 500000},
        .tcp_portno = 1502, // MODBUS_TCP_DEFAULT_PORT (502) would require root privileges
        .progname = "vfs11_vfd",
        .section = "VFS11",
        .fp = NULL,
        .inifile = NULL,
        .reconnect_delay = 1,
        .ctx = NULL,
        .haldata = NULL,
        .hal_comp_id = -1,
        .read_initial_done = 0,
        .old_err_reset = 0,
        .old_cmd1_reg = 0,
        .modbus_ok = 0,    // set modbus-ok bit if last MODBUS_OK transactions went well
        .failed_reg =0,
        .last_errno = 0,
        .tcp_destip = "127.0.0.1",
        .report_device = 0,
};


static int connection_state;
enum connstate {NOT_CONNECTED, OPENING, CONNECTING, CONNECTED, RECOVER, DONE};

static char *option_string = "dhrmn:S:I:";
static struct option long_options[] = {
        {"debug", no_argument, 0, 'd'},
        {"help", no_argument, 0, 'h'},
        {"modbus-debug", no_argument, 0, 'm'},
        {"report-device", no_argument, 0, 'r'},
        {"ini", required_argument, 0, 'I'},     // default: getenv(INI_FILE_NAME)
        {"section", required_argument, 0, 'S'}, // default section = LIBMODBUS
        {"name", required_argument, 0, 'n'},    // vfs11_vfd
        {0,0,0,0}
};


void  windup(param_pointer p) 
{
    if (p->haldata && *(p->haldata->errorcount)) {
        fprintf(stderr,"%s: %d modbus errors\n",p->progname, *(p->haldata->errorcount));
        fprintf(stderr,"%s: last command register: 0x%.4x\n",p->progname, p->failed_reg);
        fprintf(stderr,"%s: last error: %s\n",p->progname, modbus_strerror(p->last_errno));
    }
    if (p->hal_comp_id >= 0)
        hal_exit(p->hal_comp_id);
    if (p->ctx)
        modbus_close(p->ctx);
}

static void toggle_modbus_debug(int sig)
{
    param.modbus_debug = !param.modbus_debug;
    modbus_set_debug(param.ctx, param.modbus_debug);
}

static void toggle_debug(int sig)
{
    param.debug = !param.debug;
}

static void quit(int sig) 
{
    if (param.debug)
        fprintf(stderr,"quit(connection_state=%d)\n",connection_state);

    switch (connection_state) {

    case CONNECTING:  
        // modbus_tcp_accept() or TCP modbus_connect()  were interrupted
        // these wont return to the main loop, so exit here
        windup(&param);
        exit(0);
        break;

    default:
        connection_state = DONE;
        break;
    }
}

enum kwdresult {NAME_NOT_FOUND, KEYWORD_INVALID, KEYWORD_FOUND};
#define MAX_KWD 10

int findkwd(param_pointer p, const char *name, int *result, const char *keyword, int value, ...)
{
    const char *word;
    va_list ap;
    const char *kwds[MAX_KWD], **s;
    int nargs = 0;

    if ((word = iniFind(p->fp, name, p->section)) == NULL)
        return NAME_NOT_FOUND;

    kwds[nargs++] = keyword;
    va_start(ap, value);

    while (keyword != NULL) {
        if (!strcasecmp(word, keyword)) {
            *result = value;
            va_end(ap);
            return KEYWORD_FOUND;
        }
        keyword = va_arg(ap, const char *);
        kwds[nargs++] = keyword;
        if (keyword)
            value = va_arg(ap, int);
    }  
    fprintf(stderr, "%s: %s:[%s]%s: found '%s' - not one of: ", 
            p->progname, p->inifile, p->section, name, word);
    for (s = kwds; *s; s++) 
        fprintf(stderr, "%s ", *s);
    fprintf(stderr, "\n");
    va_end(ap);
    return KEYWORD_INVALID;
}

int read_ini(param_pointer p)
{
    const char *s;
    double f;
    int value;

    if ((p->fp = fopen(p->inifile,"r")) != NULL) {
        if (!p->debug)
            iniFindInt(p->fp, "DEBUG", p->section, &p->debug);
        if (!p->modbus_debug)
            iniFindInt(p->fp, "MODBUS_DEBUG", p->section, &p->modbus_debug);
        iniFindInt(p->fp, "BITS", p->section, &p->bits);
        iniFindInt(p->fp, "BAUD", p->section, &p->baud);
        iniFindInt(p->fp, "STOPBITS", p->section, &p->stopbits);
        iniFindInt(p->fp, "TARGET", p->section, &p->slave);
        iniFindInt(p->fp, "POLLCYCLES", p->section, &p->pollcycles);
        iniFindInt(p->fp, "PORT", p->section, &p->tcp_portno);
        iniFindInt(p->fp, "RECONNECT_DELAY", p->section, &p->reconnect_delay);

        if ((s = iniFind(p->fp, "TCPDEST", p->section))) {
            p->tcp_destip = strdup(s);
        }
        if ((s = iniFind(p->fp, "DEVICE", p->section))) {
            p->device = strdup(s);
        }
        if (iniFindDouble(p->fp, "RESPONSE_TIMEOUT", p->section, &f)) {
            p->response_timeout.tv_sec = (int) f;
            p->response_timeout.tv_usec = (f-p->response_timeout.tv_sec) * 1000000;
        }
        if (iniFindDouble(p->fp, "BYTE_TIMEOUT", p->section, &f)) {
            p->byte_timeout.tv_sec = (int) f;
            p->byte_timeout.tv_usec = (f-p->byte_timeout.tv_sec) * 1000000;
        }
        value = p->parity;
        if (findkwd(p, "PARITY", &value,
                "even",'E',
                "odd", 'O',
                "none", 'N',
                NULL) == KEYWORD_INVALID)
            return -1;
        p->parity = value;

#ifdef MODBUS_RTU_RTS_UP	
        if (findkwd(p, "RTS_MODE", &p->rts_mode,
                "up", MODBUS_RTU_RTS_UP,
                "down", MODBUS_RTU_RTS_DOWN,
                "none", MODBUS_RTU_RTS_NONE,
                NULL) == KEYWORD_INVALID)
            return -1;
#else
        if (iniFind(p->fp, "RTS_MODE", p->section) != NULL) {
            fprintf(stderr,"%s: warning - the RTS_MODE feature is not available with the installed libmodbus version (%s).\n"
                    "to enable it, uninstall libmodbus-dev and rebuild with "
                    "libmodbus built http://github.com/stephane/libmodbus:master .\n",
                    LIBMODBUS_VERSION_STRING, p->progname);
        }
#endif
        if (findkwd(p,"SERIAL_MODE", &p->serial_mode,
                "rs232", MODBUS_RTU_RS232,
                "rs485", MODBUS_RTU_RS485,
                NULL) == KEYWORD_INVALID)
            return -1;

        if (findkwd(p, "TYPE", &p->type,
                "rtu", TYPE_RTU,
                "tcpserver", TYPE_TCP_SERVER,
                "tcpclient", TYPE_TCP_CLIENT,
                NULL) == NAME_NOT_FOUND) {
            fprintf(stderr, "%s: missing required TYPE in section %s\n",
                    p->progname, p->section);
            return -1;
        }
    } else {
        fprintf(stderr, "%s:cant open inifile '%s'\n",
                p->progname, p->inifile);
        return -1;
    }
    return 0;
}

void usage(int argc, char **argv) {
    printf("Usage:  %s [options]\n", argv[0]);
    printf("This is a userspace HAL program, typically loaded using the halcmd \"loadusr\" command:\n"
            "    loadusr vfs11_vfd [options]\n"
            "Options are:\n"
            "-I or --ini <inifile>\n"
            "    Use <inifile> (default: take ini filename from environment variable INI_FILE_NAME)\n"
            "-S or --section <section-name> (default 8)\n"
            "    Read parameters from <section_name> (default 'VFS11')\n"
            "-d or --debug\n"
            "    Turn on debugging messages. Toggled by USR1 signal.\n"
            "-m or --modbus-debug\n"
            "    Turn on modbus debugging.  This will cause all modbus messages\n"
            "    to be printed in hex on the terminal. Toggled by USR2 signal.\n"
            "-r or --report-device\n"
            "    Report device properties on console at startup\n");
}

int write_data(modbus_t *ctx, haldata_t *haldata, param_pointer p)
{
    hal_float_t hzcalc;
    int cmd1_reg;
    int freq_reg, freq_cap;

    if (!*(haldata->enabled)) {
        // send 0 to 0x2000 register - no bus control
        if (modbus_write_register(ctx, REG_COMMAND1, 0) < 0) {
            p->failed_reg = REG_COMMAND1;
            (*haldata->errorcount)++;
            p->last_errno = errno;
            return errno;
        }
        return 0;
    }

retry:
    // set frequency register
    hzcalc = haldata->motor_nameplate_hz / haldata->motor_nameplate_RPM;
    freq_reg =  (int)round(fabs((*(haldata->speed_command) * hzcalc * 100.0)));
    freq_cap =  (int)round(fabs((haldata->rpm_limit * hzcalc * 100)));

    // limit frequency to frequency set via max-rpm
    if (freq_reg > freq_cap)
        freq_reg = freq_cap;

    *(haldata->freq_cmd)  =  freq_reg / 100.0;

    // prepare command register
    //  force Modbus control - this disables the panel
//    cmd1_reg = (CMD_COMMAND_PRIORITY|CMD_FREQUENCY_PRIORITY);
    cmd1_reg = 0;
    if (*haldata->spindle_on){
        cmd1_reg |= (*haldata->jog_mode) ? CMD_JOG_RUN : CMD_RUN;
    } else {
        cmd1_reg |= CMD_STOP;
    }

//    // if 1, choose ramp times as per F500/F501
//    // fix for PID loops where long ramp times cause oscillation
//    if (haldata->acc_dec_pattern){
//        cmd1_reg|= CMD_ACCEL_PATTERN_2;
//    }

//    // rev follows fwd
//    // two bits for one direction is a mess in the first place
//    *(haldata->spindle_rev) = *(haldata->spindle_fwd) ? 0 : 1;
//    *(haldata->spindle_fwd) = *(haldata->spindle_rev) ? 0 : 1;

    if (*haldata->spindle_rev) {
        cmd1_reg |= CMD_REVERSE;
    }

    if (*haldata->spindle_fwd) {
        cmd1_reg |= CMD_FORWARD;
    }
    // TODO: implement REVERSE/FORWARD
    cmd1_reg |= CMD_FORWARD;

//    // DC brake - turn spindle_on off as well
//    if  (*(haldata->DC_brake)) {
//        cmd1_reg |= CMD_DC_BRAKE;  	// set DC brake bit
//        cmd1_reg &= ~(CMD_RUN | CMD_JOG_RUN);
//        *(haldata->spindle_on) = 0;
//        *(haldata->at_speed) = 0;
//    } else {
//        cmd1_reg &= ~CMD_DC_BRAKE;
//    }

//TODO: implement RESET command for VFD-B
//    // send CMD_FAULT_RESET and CMD_EMERGENCY_STOP only once so the poor thing comes back
//    // out of reset/estop status eventually
//    if (*(haldata->err_reset) && !(p->old_cmd1_reg  & CMD_FAULT_RESET ))	{ // not sent yet
//        cmd1_reg |= CMD_FAULT_RESET;		// fault reset bit = 1 -> clear fault
//        *(haldata->err_reset) = 0;
//    } else {
//        cmd1_reg &= ~CMD_FAULT_RESET;
//    }

//    if (*(haldata->estop) && !(p->old_cmd1_reg  & CMD_EMERGENCY_STOP )) {	// not sent yet)
//        cmd1_reg |= CMD_EMERGENCY_STOP;		// estop bit -> trip VFD into estop mode
//        *(haldata->estop) = 0;
//        *(haldata->spindle_on) = 0;
//        *(haldata->at_speed) = 0;
//    } else {
//        cmd1_reg &= ~CMD_EMERGENCY_STOP;
//    }

    DBG("write_data: cmd1_reg=0x%4.4X old cmd1_reg=0x%4.4X\n", cmd1_reg,p->old_cmd1_reg);
    printf("write_data: cmd1_reg=0x%4.4X old cmd1_reg=0x%4.4X\n", cmd1_reg, p->old_cmd1_reg);

    // cmd1_reg = 0x01;    // STOP
//    cmd1_reg = 0x0012;    // RUN
    if (modbus_write_register(ctx, REG_COMMAND1, cmd1_reg) < 0) {
        // modbus transaction timed out. This may happen if VFD is in E-Stop.
        // if VFD was in E-Stop, and a fault reset was sent, wait about 2 seconds for recovery
        // we must assume that any command and frequency values sent were cleared, so we restart
        // the operation.
        // note that sending the CMD_EMERGENCY_STOP bit in cmd1_reg causes an immediate reboot
        // without a Modbus reply (if the VFD actually was in e-stop) so we ignore this error.
        if (cmd1_reg & CMD_EMERGENCY_STOP) {
            sleep(2);
            goto retry;
        }
        p->failed_reg = REG_COMMAND1;
        (*haldata->errorcount)++;
        p->last_errno = errno;
        return errno;
    } 

    // remember so we can toggle fault/estop reset just once
    // otherwise the VFD keeps rebooting as long as the fault reset/estop reset bits are sent
    p->old_cmd1_reg = cmd1_reg;

    if ((modbus_write_register(ctx, REG_FREQUENCY, freq_reg)) < 0) {
        p->failed_reg = REG_FREQUENCY;
        (*haldata->errorcount)++;
        p->last_errno = errno;
        return errno;
    } 

//    if ((*(haldata->freq_cmd) > 0.01) && ((1.0 - *(haldata->freq_out) / *(haldata->freq_cmd))  < haldata->speed_tolerance)){
//        *(haldata->at_speed) = 1;
//    } else {
//        *(haldata->at_speed) = 0;
//    }
//
//    if (*(haldata->spindle_on) == 0){ // JET reset at-speed
//        *(haldata->at_speed) = 0;
//    }
    return 0;
}

#define GETREG(reg,into)					\
        do {							\
            curr_reg = reg;						\
            if (modbus_read_registers(ctx, reg, 1, into) != 1)	\
            goto failed;					\
        } while (0)


int read_initial(modbus_t *ctx, haldata_t *haldata, param_pointer p)
{
    uint16_t curr_reg, current, 
    voltage, model, eeprom, max_freq;

    GETREG(REG_UPPERLIMIT, &max_freq);
    *(haldata->upper_limit_hz) = (float)max_freq/100.0;
    *(haldata->max_rpm) = *(haldata->upper_limit_hz) * 
            haldata->motor_nameplate_RPM / haldata->motor_nameplate_hz;

    if (p->report_device) {
        GETREG(SR_RATED_CURRENT, &current);
        GETREG(SR_RATED_VOLTAGE, &voltage);
        GETREG(SR_INVERTER_MODEL, &model);
        GETREG(SR_EEPROM_VERSION, &eeprom);

        printf("%s: inverter model: %d/0x%4.4x\n",
                p->progname, model, model);
        printf("%s: maximum ratings: %.1fV %.1fA %.2fHz\n",
                p->progname, voltage/10.0, current/10.0, max_freq/100.0);
        printf("%s: versions: eeprom=%d/0x%4.4x\n",
                p->progname, eeprom, eeprom);
    }
    return 0;

    failed:
    p->failed_reg = curr_reg;
    p->last_errno = errno;
    (*haldata->errorcount)++;
    if (p->debug)
        fprintf(stderr, "%s: read_initial: modbus_read_registers(0x%4.4x): %s\n",
                p->progname, curr_reg, modbus_strerror(errno));
    return p->last_errno;
}

int read_data(modbus_t *ctx, haldata_t *haldata, param_pointer p)
{
    int retval;
    uint16_t curr_reg, val, status_reg, freq_reg;
    static int pollcount = 0;

    if (!p->read_initial_done) {
        if ((retval = read_initial(ctx, haldata, p)))
            return retval;
        else
            p->read_initial_done = 1;
    }

    GETREG(SR_ERROR_CODE, &curr_reg);
    *(haldata->error_code) = curr_reg;

    // we always at least read the main status register SR_INV_OPSTATUS
    // and current operating frequency SR_OP_FREQUENCY
    GETREG(SR_INV_OPSTATUS, &status_reg);
    *(haldata->status) = status_reg;

    GETREG(SR_OUTPUT_FREQ, &freq_reg);
    *(haldata->freq_out) = freq_reg * 0.01;

    DBG("read_data: status_reg=%4.4x freq_reg=%4.4x\n", status_reg, freq_reg);

    // JET if freq out is 0 then the drive is stopped
    *(haldata->is_stopped) = (freq_reg == 0);

    if (status_reg == ST_EMERGENCY_STOPPED) {	// set e-stop status.
        *(haldata->is_e_stopped) = 1;
    } else {
        *(haldata->is_e_stopped) = 0;
    }

    if ((pollcount == 0) && !*(haldata->max_speed)) {
        // less urgent registers
        GETREG(SR_MOTOR_SPEED, &val);
        *(haldata->RPM) = val;
        *(haldata->RPS) = val/60.0;

        GETREG(SR_TORQUE_RATIO, &val);
        *(haldata->torque_ratio) =  val;

        GETREG(SR_OUTPUT_CURRENT, &val);
        *(haldata->output_current) =  val * 0.1;

        GETREG(SR_OUTPUT_VOLTAGE, &val);
        *(haldata->output_volt) =  val * 0.1;

    } else {
        pollcount++;
    }

    if (pollcount >= p->pollcycles)
        pollcount = 0;

    p->last_errno = retval = 0;
    return 0;

    failed:
    p->failed_reg = curr_reg;
    p->last_errno = errno;
    (*haldata->errorcount)++;
    if (p->debug)
        fprintf(stderr, "%s: read_data: modbus_read_registers(0x%4.4x): %s\n",
                p->progname, curr_reg, modbus_strerror(errno));
    return p->last_errno;
}

#undef GETREG

#define PIN(x)					\
        do {						\
            status = (x);					\
            if ((status) != 0)				\
            return status;				\
        } while (0)

int hal_setup(int id, haldata_t *h, const char *name)
{
    int status;
    PIN(hal_pin_bit_newf(HAL_IN, &(h->acc_dec_pattern), id, "%s.acceleration-pattern", name));
    PIN(hal_pin_s32_newf(HAL_OUT, &(h->alarm_code), id, "%s.alarm-code", name));
    PIN(hal_pin_bit_newf(HAL_OUT, &(h->at_speed), id, "%s.at-speed", name));
    PIN(hal_pin_float_newf(HAL_OUT, &(h->output_current), id, "%s.output-current", name));
    PIN(hal_pin_bit_newf(HAL_IN, &(h->DC_brake), id, "%s.dc-brake", name));
    PIN(hal_pin_bit_newf(HAL_IN, &(h->enabled), id, "%s.enable", name));
    PIN(hal_pin_bit_newf(HAL_IN, &(h->err_reset), id, "%s.err-reset", name));
    PIN(hal_pin_bit_newf(HAL_IN, &(h->jog_mode), id, "%s.jog-mode", name));
    PIN(hal_pin_bit_newf(HAL_IN, &(h->estop), id, "%s.estop", name));
    PIN(hal_pin_float_newf(HAL_OUT, &(h->freq_cmd), id, "%s.frequency-command", name));
    PIN(hal_pin_float_newf(HAL_OUT, &(h->freq_out), id, "%s.frequency-out", name));
    PIN(hal_pin_float_newf(HAL_OUT, &(h->torque_ratio), id, "%s.inverter-load-percentage", name));
    PIN(hal_pin_bit_newf(HAL_OUT, &(h->is_e_stopped), id, "%s.is-e-stopped", name)); // JET
    PIN(hal_pin_bit_newf(HAL_OUT, &(h->is_stopped), id, "%s.is-stopped", name)); // JET
    PIN(hal_pin_float_newf(HAL_OUT, &(h->max_rpm), id, "%s.max-rpm", name));
    PIN(hal_pin_bit_newf(HAL_OUT, &(h->modbus_ok), id, "%s.modbus-ok", name)); // JET
    PIN(hal_pin_float_newf(HAL_OUT, &(h->RPM), id, "%s.motor-RPM", name));
    PIN(hal_pin_float_newf(HAL_OUT, &(h->RPS), id, "%s.motor-RPS", name));
    PIN(hal_pin_float_newf(HAL_OUT, &(h->output_volt), id, "%s.output-voltage", name));
    PIN(hal_pin_float_newf(HAL_IN, &(h->speed_command), id, "%s.speed-command", name));
    PIN(hal_pin_bit_newf(HAL_IN, &(h->spindle_fwd), id, "%s.spindle-fwd", name));
    PIN(hal_pin_bit_newf(HAL_IN, &(h->spindle_on), id, "%s.spindle-on", name));
    PIN(hal_pin_bit_newf(HAL_IN, &(h->spindle_rev), id, "%s.spindle-rev", name)); //JET
    PIN(hal_pin_s32_newf(HAL_OUT, &(h->error_code), id, "%s.error-code", name));
    PIN(hal_pin_s32_newf(HAL_OUT, &(h->status), id, "%s.status", name));
    PIN(hal_pin_s32_newf(HAL_OUT, &(h->trip_code), id, "%s.trip-code", name));
    PIN(hal_pin_bit_newf(HAL_IN, &(h->max_speed), id, "%s.max-speed", name));
    PIN(hal_pin_s32_newf(HAL_OUT, &(h->errorcount), id, "%s.error-count", name));
    PIN(hal_pin_float_newf(HAL_OUT, &(h->upper_limit_hz), id, "%s.frequency-limit", name));

    // the following limit must be set manually from the panel since its in EEPROM
    PIN(hal_param_float_newf(HAL_RW, &(h->looptime), id, "%s.loop-time", name));
    PIN(hal_param_float_newf(HAL_RW, &(h->motor_nameplate_hz), id, "%s.nameplate-HZ", name));
    PIN(hal_param_float_newf(HAL_RW, &(h->motor_nameplate_RPM), id, "%s.nameplate-RPM", name));
    PIN(hal_param_float_newf(HAL_RW, &(h->rpm_limit), id, "%s.rpm-limit", name));
    PIN(hal_param_float_newf(HAL_RW, &(h->speed_tolerance), id, "%s.tolerance", name));

    return 0;
}
#undef PIN

int set_defaults(param_pointer p)
{
    haldata_t *h = p->haldata;

    *(h->status) = 0;
    *(h->freq_cmd) = 0;
    *(h->freq_out) = 0;
    *(h->output_volt) = 0;
    *(h->RPM) = 0;
    *(h->torque_ratio) = 0;
    *(h->output_current) = 0;
    *(h->upper_limit_hz) = 0;
    *(h->trip_code) = 0;
    *(h->alarm_code) = 0;
    *(h->at_speed) = 0;
    *(h->is_stopped) = 0;
    *(h->estop) = 0;
    *(h->is_e_stopped) = 0;
    *(h->speed_command) = 0;
    *(h->modbus_ok) = 0;

    *(h->spindle_on) = 0;
    *(h->DC_brake) = 0;
    *(h->spindle_fwd) = 1;
    *(h->spindle_rev) = 0;
    *(h->err_reset) = 0;
    *(h->jog_mode) = 0;
    *(h->enabled) = 0;
    *(h->acc_dec_pattern) = 0;
    *(h->errorcount) = 0;
    *(h->max_speed) = 0;

    h->looptime = 0.1;
    h->speed_tolerance = 0.01;      // output frequency within 1% of target frequency
    h->motor_nameplate_hz = 50;	    // folks in The Colonies typically would use 60Hz and 1730 rpm
    h->motor_nameplate_RPM = 1410;
    h->rpm_limit = MAX_RPM;

    p->failed_reg = 0;
    return 0;
}

int main(int argc, char **argv)
{
    struct timespec loop_timespec, remaining;
    int opt;
    param_pointer p = &param;
    int retval = 0;
    retval = -1;
    p->progname = argv[0];
    connection_state = NOT_CONNECTED;
    p->inifile = getenv("INI_FILE_NAME");

    while ((opt = getopt_long(argc, argv, option_string, long_options, NULL)) != -1) {
        switch(opt) {
        case 'n':
            p->modname = strdup(optarg);
            break;
        case 'm':
            p->modbus_debug = 1;
            break;
        case 'd':
            p->debug = 1;
            break;
        case 'S':
            p->section = optarg;
            break;
        case 'I':
            p->inifile = optarg;
            break;
        case 'r':
            p->report_device = 1;
            break;
        case 'h':
        default:
            usage(argc, argv);
            exit(0);
        }
    }

    if (p->inifile) {
        if (read_ini(p))
            goto finish;
        if (!p->modname)
            p->modname = "vfs11_vfd";
    } else {
        fprintf(stderr, "%s: ERROR: no inifile - either use '--ini inifile' or set INI_FILE_NAME environment variable\n", p->progname);
        goto finish;
    }

    signal(SIGINT, quit);
    signal(SIGTERM, quit);
    signal(SIGUSR1, toggle_debug);
    signal(SIGUSR2, toggle_modbus_debug);

    // create HAL component 
    p->hal_comp_id = hal_init(p->modname);
    if ((p->hal_comp_id < 0) || (connection_state == DONE)) {
        fprintf(stderr, "%s: ERROR: hal_init(%s) failed: HAL error code=%d\n",
                p->progname, p->modname, p->hal_comp_id);
        retval = p->hal_comp_id;
        goto finish;
    }

    // grab some shmem to store the HAL data in
    p->haldata = (haldata_t *)hal_malloc(sizeof(haldata_t));
    if ((p->haldata == 0) || (connection_state == DONE)) {
        fprintf(stderr, "%s: ERROR: unable to allocate shared memory\n", p->modname);
        retval = -1;
        goto finish;
    }
    if (hal_setup(p->hal_comp_id,p->haldata, p->modname))
        goto finish;

    set_defaults(p);
    hal_ready(p->hal_comp_id);

    DBG("using libmodbus version %s\n", LIBMODBUS_VERSION_STRING);

    switch (p->type) {

    case TYPE_RTU:
        connection_state = OPENING;
        if ((p->ctx = modbus_new_rtu(p->device, p->baud, p->parity, p->bits, p->stopbits)) == NULL) {
            fprintf(stderr, "%s: ERROR: modbus_new_rtu(%s): %s\n",
                    p->progname, p->device, modbus_strerror(errno));
            goto finish;
        }
        DBG("device(%s) baud(%d) parity(%s) bits(%d) stopbits(%d)\n", p->device, p->baud, &(p->parity), p->bits, p->stopbits);
        if (modbus_set_slave(p->ctx, p->slave) < 0) {
            fprintf(stderr, "%s: ERROR: invalid slave number: %d\n", p->modname, p->slave);
            goto finish;
        }
        if ((retval = modbus_connect(p->ctx)) != 0) {
            fprintf(stderr, "%s: ERROR: couldn't open serial device: %s\n", p->modname, modbus_strerror(errno));
            goto finish;
        }
        DBG("%s: serial port %s connected\n", p->progname, p->device);
        break;

    default:
        fprintf(stderr, "%s: ERROR: invalid connection type %d\n",
                p->progname, p->type);
        goto finish;
    }

    modbus_set_debug(p->ctx, p->modbus_debug);
    if (modbus_set_slave(p->ctx, p->slave) < 0) {
        fprintf(stderr, "%s: ERROR: invalid slave number: %d\n", p->modname, p->slave);
        goto finish;
    }

    connection_state = CONNECTED;
    while (connection_state != DONE) {

        while (connection_state == CONNECTED) {
            if ((retval = read_data(p->ctx, p->haldata, p))) {
                p->modbus_ok = 0;
            } else {
                p->modbus_ok++;
            }
            if (p->modbus_ok > MODBUS_MIN_OK) {
                *(p->haldata->modbus_ok) = 1;
            } else {
                *(p->haldata->modbus_ok) = 0;
            }
            if ((retval = write_data(p->ctx, p->haldata, p))) {
                p->modbus_ok = 0;
                if ((retval == EBADF || retval == ECONNRESET || retval == EPIPE)) {
                    connection_state = RECOVER;
                }
            } else {
                p->modbus_ok++;
            }
            if (p->modbus_ok > MODBUS_MIN_OK) {
                *(p->haldata->modbus_ok) = 1;
            } else {
                *(p->haldata->modbus_ok) = 0;
            }
            /* don't want to scan too fast, and shouldn't delay more than a few seconds */
            if (p->haldata->looptime < 0.001) p->haldata->looptime = 0.001;
            if (p->haldata->looptime > 2.0) p->haldata->looptime = 2.0;
            loop_timespec.tv_sec = (time_t)(p->haldata->looptime);
            loop_timespec.tv_nsec = (long)((p->haldata->looptime - loop_timespec.tv_sec) * 1000000000l);
            if (!p->haldata->max_speed)
                nanosleep(&loop_timespec, &remaining);
        }

        switch (connection_state) {
        case DONE:
            // cleanup actions before exiting.
            modbus_flush(p->ctx);
            // clear the command register (control and frequency override) so panel operation gets reactivated
            if ((retval = modbus_write_register(p->ctx, REG_COMMAND1, 0)) != 1) {
                // not much we can do about it here if it goes wrong, so complain
                fprintf(stderr, "%s: failed to release VFD from bus control (write to register 0x%x): %s\n",
                        p->progname, REG_COMMAND1, modbus_strerror(errno));
            } else {
                DBG("%s: VFD released from bus control.\n", p->progname);
            }
            break;

        case RECOVER:
            DBG("recover\n");
            set_defaults(p);
            p->read_initial_done = 0;
            // reestablish connection to slave
            switch (p->type) {

            case TYPE_RTU:
                modbus_flush(p->ctx);
                modbus_close(p->ctx);
                while ((connection_state != CONNECTED) &&
                        (connection_state != DONE)) {
                    sleep(p->reconnect_delay);
                    if (!modbus_connect(p->ctx)) {
                        connection_state = CONNECTED;
                        DBG("rtu/tcpclient reconnect\n");
                    } else {
                        fprintf(stderr, "%s: recovery: modbus_connect(): %s\n",
                                p->progname, modbus_strerror(errno));
                    }
                }
                break;

            default:
                break;
            }
            break;
            default: ;
        }
    }
    retval = 0;	

    finish:
    windup(p);
    return retval;
}

