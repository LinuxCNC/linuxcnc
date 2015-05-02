
//
// Copyright (C) 2009 scotta at CNCZone.com, alan_3301 at CNCZone.com
//     2014 Benjamin Brockhaus
//     2015 Sebastian Kuzminsky <seb@highlab.com>
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


#ifndef ULAPI
#error This is intended as a userspace component only.
#endif

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <math.h>
#include "rtapi.h"
#include "hal.h"
#include "hy_modbus.h"


#define MODBUS_MIN_OK		10


// bits in Status Data
#define STATUS_SetF 		0x00
#define STATUS_OutF			0x01
#define STATUS_OutA 		0x02
#define STATUS_RoTT 		0x03
#define STATUS_DCV 			0x04
#define STATUS_ACV 			0x05
#define STATUS_Cont 		0x06
#define STATUS_Tmp 			0x07

// control commands CNTR
#define CONTROL_Run_Fwd		0x01
#define CONTROL_Run_Rev		0x11
#define CONTROL_Stop		0x08

// control responses CNST
#define CONTROL_Run			0x01
#define CONTROL_Jog			0x02
#define CONTROL_Command_rf	0x04
#define CONTROL_Running		0x08
#define CONTROL_Jogging		0x10
#define CONTROL_Running_rf	0x20
#define CONTROL_Bracking	0x40
#define CONTROL_Track_Start	0x80



int modbus_ok;						// set modbus-ok bit if last MODBUS_OK transactions went well
int debug;
int slave = 1;

/* HAL data struct */
typedef struct {
	// The HAL comp name will be set to <string>, and all pin and parameter
	// names will begin with <string>.  

	hal_bit_t	*enable;				// bit to enable this component
	
	hal_float_t	*Set_F;					// frequency command
	hal_float_t	*Out_F;					// actual output frequency
	hal_float_t	*Out_A;					// actual output amps
	hal_float_t	*RoTT;					// actual motor rmp (based on VFD parameters) 
	hal_float_t	*DCV;					// DC Volts (to be confirmed)
	hal_float_t	*ACV;					// AC Volts (to be confirmed)
	hal_float_t	*Cont;
	hal_float_t *Tmp;					// Temperature (to be confirmed)
	
	hal_bit_t *spindle_fwd;				// spindle forward input
	hal_bit_t *spindle_rev;				// spindle reverse input
	hal_bit_t *spindle_on;				// spinlde on input
	hal_float_t *CNTR;					// stores the status of the control request
	hal_float_t *CNST;					// stores the response of the control request
	
	hal_bit_t *CNST_Run;				// CNST Run bit
	hal_bit_t *CNST_Jog;				// CNST Jog bit	
	hal_bit_t *CNST_Command_rf;			// CNST Run reverse / forward bit	
	hal_bit_t *CNST_Running;			// CNST Running bit	
	hal_bit_t *CNST_Jogging;			// CNST Jogging bit	
	hal_bit_t *CNST_Running_rf;				// CNST Jog reverse / forward bit	
	hal_bit_t *CNST_Bracking;			// CNST bracking bit	
	hal_bit_t *CNST_Track_Start;		// CNST track start bit	

	hal_float_t *speed_command;			// spindle speed command from EMC
	hal_float_t	*freq_cmd;				// calculated frequency command

	hal_float_t *max_freq;				// PD005 Max Operating Freqency
	hal_float_t *freq_lower_limit;		// PD011 Freqency Lower Limit
	hal_float_t *rated_motor_voltage; 	// PD141 Rated Motor Voltage - as per motor name plate
	hal_float_t *rated_motor_current;	// PD142 Rated Motor Current - as per motor name plate
	hal_float_t *rated_motor_rev;		// PD144 Set value corresponds to RPM at 50Hz
	
	hal_bit_t	*modbus_ok;				// the last MODBUS_OK transactions returned successfully
	
	hal_float_t *max_rpm;				// calculated based on VFD max frequency setup parameter

	

	hal_float_t	retval;
	hal_s32_t	errorcount;
	hal_float_t	looptime;
	//hal_float_t	speed_tolerance;   		// unused?
	//hal_float_t	motor_nameplate_hz;		// speeds are scaled in Hz, not RPM
	//hal_float_t	motor_nameplate_RPM;	// nameplate RPM at default Hz
	//hal_float_t	rpm_limit;				// do-not-exceed output frequency
	//hal_bit_t	*acc_dec_pattern;		// if set: choose ramp times as defined in F500+F501
										// if zero (default): choose ramp times as defined in ACC and DEC
	//hal_float_t	upper_limit_hz;		    // VFD setup parameter - maximum output frequency in HZ

	//hal_bit_t	old_run;				// so we can detect changes in the run state
	//hal_bit_t	old_dir;
	//hal_bit_t	old_err_reset;
	//hal_u32_t	old_cmd1_reg;				// copy of last write to FA00
	//hal_u32_t	failed_reg;				// remember register for failed modbus transaction - aids debugging
} haldata_t;

static int done;
char *modname = "hy_vfd";

static struct option long_options[] = {
		{"bits", 1, 0, 'b'},
		{"device", 1, 0, 'd'},
		{"debug", 0, 0, 'g'},
		{"help", 0, 0, 'h'},
		{"name", 1, 0, 'n'},
		{"parity", 1, 0, 'p'},
		{"rate", 1, 0, 'r'},
		{"stopbits", 1, 0, 's'},
		{"target", 1, 0, 't'},
		{0,0,0,0}
};

static char *option_string = "b:d:hn:p:r:s:t:vg";

static char *bitstrings[] = {"5", "6", "7", "8", NULL};
static char *paritystrings[] = {"even", "odd", "none", NULL};
static char *ratestrings[] = {"110", "300", "600", "1200", "2400", "4800", "9600",
		"19200", "38400", "57600", "115200", NULL};
static char *stopstrings[] = {"1", "2", NULL};

int match_string(char *string, char **matches) {
	int len, which, match;
	which=0;
	match=-1;
	if ((matches==NULL) || (string==NULL)) return -1;
	len = strlen(string);
	while (matches[which] != NULL) {
		if ((!strncmp(string, matches[which], len)) && (len <= strlen(matches[which]))) {
			if (match>=0) return -1;        // multiple matches
			match=which;
		}
		++which;
	}
	return match;
}

void usage(int argc, char **argv) {
	printf("Usage:  %s [options]\n", argv[0]);
	printf(
			"This is a userspace HAL program, typically loaded using the halcmd \"loadusr\" command:\n"
			"    loadusr hy_vfd\n"
			"There are several command-line options.  Options that have a set list of possible values may\n"
			"    be set by using any number of characters that are unique.  For example, --rate 5 will use\n"
			"    a baud rate of 57600, since no other available baud rates start with \"5\"\n"
			"-b or --bits <n> (default 8)\n"
			"    Set number of data bits to <n>, where n must be from 5 to 8 inclusive\n"
			"-d or --device <path> (default /dev/ttyS0)\n"
			"    Set the name of the serial device node to use\n"
			"-g or --debug\n"
			"    Turn on debugging messages.  Debug mode will cause all modbus messages\n"
                        "    to be printed in hex on the terminal.\n"
			"-n or --name <string> (default hy_vfd)\n"
			"    Set the name of the HAL module.  The HAL comp name will be set to <string>, and all pin\n"
			"    and parameter names will begin with <string>.\n"
			"-p or --parity {even,odd,none} (default even)\n"
			"    Set serial parity to even, odd, or none.\n"
			"-r or --rate <n> (default 19200)\n"
			"    Set baud rate to <n>.  It is an error if the rate is not one of the following:\n"
			"    110, 300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200\n"
			"-s or --stopbits {1,2} (default 1)\n"
			"    Set serial stop bits to 1 or 2\n"
			"-t or --target <n> (default 1)\n"
			"    Set MODBUS target (slave) number.  This must match the device number you set on the VF-S11.\n"
        );
}

int write_data(modbus_param_t *mb_param, modbus_data_t *mb_data, haldata_t *haldata)
{
	int retval;
	int CNTR, old_CNTR;
	int CNST;
	hal_float_t hzcalc;
	int freq_comp;
	int freq, old_freq;
	
	// calculate and set frequency register, limit the freqency (upper and lower to VFD set parameters
	mb_data->function = WRITE_FREQ_DATA;
	mb_data->parameter = 0x00;
	
	if ((*(haldata->spindle_fwd) && !*(haldata->spindle_rev)) && *(haldata->spindle_on)) {
		freq_comp = 1;
	} else if ((*(haldata->spindle_rev) && !*(haldata->spindle_fwd)) && *(haldata->spindle_on)) {
		freq_comp = -1;
	} else {
		freq_comp = 0;	
	}

	hzcalc = 50 / *(haldata->rated_motor_rev);
	freq =  abs((int)((*(haldata->speed_command)+freq_comp)*hzcalc*100));
	
	// limit the frequency to the max and min as setup in the VFD
	if (freq > *(haldata->max_freq)*100)
		freq = *(haldata->max_freq)*100;
	if (freq < *(haldata->freq_lower_limit)*100)
		freq = *(haldata->freq_lower_limit)*100;
	
	old_freq = *(haldata->freq_cmd);
	
	if (freq != old_freq) {
		// commanded frequency has changed
		mb_data->data = freq;
		if ((retval = hy_modbus(mb_param, mb_data)) != 0)
			goto failed;
		*(haldata->freq_cmd)  =  freq; 
	}
	
	
	
	// update the control register
	mb_data->function = WRITE_CONTROL_DATA;
	mb_data->parameter = 0x00;
	
	if ((*(haldata->spindle_fwd) && !*(haldata->spindle_rev)) && *(haldata->spindle_on)) {
		CNTR = CONTROL_Run_Fwd;
	} else if ((*(haldata->spindle_rev) && !*(haldata->spindle_fwd)) && (*haldata->spindle_on)) {
		CNTR = CONTROL_Run_Rev;
	} else {
		CNTR = CONTROL_Stop;	
	}
	
	old_CNTR = *(haldata->CNTR);
	mb_data->data = old_CNTR;
	if ((retval = hy_modbus(mb_param, mb_data)) != 0)
		goto failed;
	if (CNTR != old_CNTR) {
		// CNTR register needs to be updated
		mb_data->data = CNTR;
		*(haldata->CNTR) = CNTR;
	}

	
	CNST = mb_data->ret_data;
	*(haldata->CNST) = CNST;
	
	if ((CNST & CONTROL_Run) != 0) {
		*(haldata->CNST_Run) = TRUE;
	} else {
		*(haldata->CNST_Run) = FALSE;
	}
	
	if ((CNST & CONTROL_Jog) != 0) {
		*(haldata->CNST_Jog) = TRUE;
	} else {
		*(haldata->CNST_Jog) = FALSE;
	}
	
	if ((CNST & CONTROL_Command_rf) != 0) {
		*(haldata->CNST_Command_rf) = TRUE;
	} else {
		*(haldata->CNST_Command_rf) = FALSE;
	}
	
	if ((CNST & CONTROL_Running) != 0) {
		*(haldata->CNST_Running) = TRUE;
	} else {
		*(haldata->CNST_Running) = FALSE;
	}
	
	if ((CNST & CONTROL_Jogging) != 0) {
		*(haldata->CNST_Jogging) = TRUE;
	} else {
		*(haldata->CNST_Jogging) = FALSE;
	}
	
	if ((CNST & CONTROL_Running_rf) != 0) {
		*(haldata->CNST_Running_rf) = TRUE;
	} else {
		*(haldata->CNST_Running_rf) = FALSE;	
	}
	
	
	if ((CNST & CONTROL_Bracking) != 0) {
		*(haldata->CNST_Bracking) = TRUE;
	} else {
		*(haldata->CNST_Bracking) = FALSE;
	}
	
	if ((CNST & CONTROL_Track_Start) != 0) {
		*(haldata->CNST_Track_Start) = TRUE;
	} else {
		*(haldata->CNST_Track_Start) = FALSE;	
	}
	
	retval = 0;
	haldata->retval = retval;
	return retval;

	failed:
	if (mb_param->debug) {
		printf("write_data: FAILED\n");
	}
	haldata->retval = retval;
	haldata->errorcount++;
	retval = -1;
	return retval;	
}

int read_setup(modbus_param_t *mb_param, modbus_data_t *mb_data, haldata_t *haldata)
{
	int retval;
	
	/* can't do anything with a null HAL data block */
	if (haldata == NULL)
		return -1;
	/* but we can signal an error if the other params are null */
	if (mb_param==NULL) {
		haldata->errorcount++;
		return -1;
	}


	if (mb_param->debug) {
		printf("read_data: reading setup parameters:\n");
	}

	mb_data->function = FUNCTION_READ;
	mb_data->data = 0x0000;

	mb_data->parameter = 5; // PD005 Max Operating Freqency
	if ((retval = hy_modbus(mb_param, mb_data)) != 0)
		goto failed;		
	*(haldata->max_freq) = mb_data->ret_data * 0.01;
	
	mb_data->parameter = 11; // PD011 Frequency Lower Limit
	if ((retval = hy_modbus(mb_param, mb_data)) != 0)
		goto failed;		
	*(haldata->freq_lower_limit) = mb_data->ret_data * 0.01;

	mb_data->parameter = 141; // PD141 Rated Motor Voltage
	if ((retval = hy_modbus(mb_param, mb_data)) != 0)
		goto failed;		
	*(haldata->rated_motor_voltage) = mb_data->ret_data * 0.1;
	
	mb_data->parameter = 142; // PD142 Rated Motor Current
	if ((retval = hy_modbus(mb_param, mb_data)) != 0)
		goto failed;		
	*(haldata->rated_motor_current) = mb_data->ret_data * 0.1;
	
	mb_data->parameter = 144; // PD144 Rated Motor Rev
	if ((retval = hy_modbus(mb_param, mb_data)) != 0)
		goto failed;		
	*(haldata->rated_motor_rev) = mb_data->ret_data;
	
	if (mb_param->debug) {
		printf("read_data: read setup parameters - OK.\n");
	}
			
	retval = 0;
	haldata->retval = retval;
	return retval;

	failed:
	if (mb_param->debug) {
		printf("read_setup: FAILED\n");
	}
	haldata->retval = retval;
	haldata->errorcount++;
	retval = -1;
	return retval;	
}


int read_data(modbus_param_t *mb_param, modbus_data_t *mb_data, haldata_t *haldata)
{
	int retval;

	// Read the Status Data registers
	
	mb_data->function = READ_CONTROL_STATUS;
	mb_data->parameter = 0x00;

	mb_data->data = STATUS_SetF;
	if ((retval = hy_modbus(mb_param, mb_data)) != 0)
		goto failed;		
	*(haldata->Set_F) = mb_data->ret_data * 0.01;
	if (mb_param->debug) {
		printf("Set_F = [%.2X]", mb_data->ret_data);
		printf("\n");
	}
			
	mb_data->data = STATUS_OutF;
	if ((retval = hy_modbus(mb_param, mb_data)) != 0)
		goto failed;		
	*(haldata->Out_F) = mb_data->ret_data * 0.01;
	if (mb_param->debug) {
		printf("Out_F = [%.2X]", mb_data->ret_data);
		printf("\n");
	}
			
	mb_data->data = STATUS_OutA;
	if ((retval = hy_modbus(mb_param, mb_data)) != 0)
		goto failed;		
	*(haldata->Out_A) = mb_data->ret_data * 0.1;
	if (mb_param->debug) {
		printf("Out_A = [%.2X]", mb_data->ret_data);
		printf("\n");
	}
	
	mb_data->data = STATUS_RoTT;
	if ((retval = hy_modbus(mb_param, mb_data)) != 0)
		goto failed;		
	*(haldata->RoTT) = mb_data->ret_data;
	if (mb_param->debug) {
		printf("RoTT = [%.2X]", mb_data->ret_data);
		printf("\n");
	}
	
	mb_data->data = STATUS_DCV;
	if ((retval = hy_modbus(mb_param, mb_data)) != 0)
		goto failed;		
	*(haldata->DCV) = mb_data->ret_data;
	if (mb_param->debug) {
		printf("DCV = [%.2X]", mb_data->ret_data);
		printf("\n");
	}
	
	mb_data->data = STATUS_ACV;
	if ((retval = hy_modbus(mb_param, mb_data)) != 0)
		goto failed;		
	*(haldata->ACV) = mb_data->ret_data;
	if (mb_param->debug) {
		printf("ACV = [%.2X]", mb_data->ret_data);
		printf("\n");
	}
	
	mb_data->data = STATUS_Cont;
	if ((retval = hy_modbus(mb_param, mb_data)) != 0)
		goto failed;		
	*(haldata->Cont) = mb_data->ret_data;
	if (mb_param->debug) {
		printf("Cont = [%.2X]", mb_data->ret_data);
		printf("\n");
	}
	
	mb_data->data = STATUS_Tmp;
	if ((retval = hy_modbus(mb_param, mb_data)) != 0)
		goto failed;		
	*(haldata->Tmp) = mb_data->ret_data;
	if (mb_param->debug) {
		printf("Tmp = [%.2X]", mb_data->ret_data);
		printf("\n");
	}
	

	retval = 0;
	haldata->retval = retval;
	return retval;

	failed:
	if (mb_param->debug) {
		printf("read_data: FAILED\n");
	}
	haldata->retval = retval;
	haldata->errorcount++;
	retval = -1;
	return retval;
}


int main(int argc, char **argv)
{
	int retval;
	modbus_param_t mb_param;
	modbus_data_t mb_data;
	haldata_t *haldata;
	int hal_comp_id;
	struct timespec loop_timespec, remaining;
	int baud, bits, stopbits;
	char *device, *parity, *endarg;
	int opt;
	int argindex, argvalue;
	done = 0;

		// assume that nothing is specified on the command line
	baud = 19200;
	bits = 8;
	stopbits = 1;
	debug = FALSE;
	device = "/dev/ttyS0";
	parity = "even";
	slave = 1;

	
	// process command line options
	while ((opt=getopt_long(argc, argv, option_string, long_options, NULL)) != -1) {
		switch(opt) {
		case 'b':   // serial data bits, probably should be 8 (and defaults to 8)
			argindex=match_string(optarg, bitstrings);
			if (argindex<0) {
				printf("hy_vfd: ERROR: invalid number of bits: %s\n", optarg);
				retval = -1;
				goto out_noclose;
			}
			bits = atoi(bitstrings[argindex]);
			break;
		case 'd':   // device name, default /dev/ttyS0
			// could check the device name here, but we'll leave it to the library open
			if (strlen(optarg) > FILENAME_MAX) {
				printf("hy_vfd: ERROR: device node name is too long: %s\n", optarg);
				retval = -1;
				goto out_noclose;
			}
			device = strdup(optarg);
			break;
		case 'g':
			debug = 1;
			break;
		case 'n':   // module base name
			if (strlen(optarg) > HAL_NAME_LEN-20) {
				printf("hy_vfd: ERROR: HAL module name too long: %s\n", optarg);
				retval = -1;
				goto out_noclose;
			}
			modname = strdup(optarg);
			break;
		case 'p':   // parity, should be a string like "even", "odd", or "none"
			argindex=match_string(optarg, paritystrings);
			if (argindex<0) {
				printf("hy_vfd: ERROR: invalid parity: %s\n", optarg);
				retval = -1;
				goto out_noclose;
			}
			parity = paritystrings[argindex];
			break;
		case 'r':   // Baud rate, 19200 default
			argindex=match_string(optarg, ratestrings);
			if (argindex<0) {
				printf("hy_vfd: ERROR: invalid baud rate: %s\n", optarg);
				retval = -1;
				goto out_noclose;
			}
			baud = atoi(ratestrings[argindex]);
			break;
		case 's':   // stop bits, defaults to 1
			argindex=match_string(optarg, stopstrings);
			if (argindex<0) {
				printf("hy_vfd: ERROR: invalid number of stop bits: %s\n", optarg);
				retval = -1;
				goto out_noclose;
			}
			stopbits = atoi(stopstrings[argindex]);
			break;
		case 't':   // target number (MODBUS ID), default 1
			argvalue = strtol(optarg, &endarg, 10);
			if ((*endarg != '\0') || (argvalue < 1) || (argvalue > 254)) {
				printf("hy_vfd: ERROR: invalid slave number: %s\n", optarg);
				retval = -1;
				goto out_noclose;
			}
			slave = argvalue;
			break;
		case 'h':
		default:
			usage(argc, argv);
			exit(0);
			break;
		}
	}
	
	if (debug) {
		printf("%s: device='%s', baud=%d, bits=%d, parity='%s', stopbits=%d, address=%d, debug=%d, PID=%d\n",
				modname,device, baud, bits, parity, stopbits, slave, debug, getpid());
	}
	
	
	/* Assume 19.2k E-8-1 serial settings, device 1 */
	modbus_init(&mb_param, device, baud, parity, bits, stopbits);
	mb_param.debug = debug;
	mb_param.print_errors = 1;
	
	/* the open has got to work, or we're out of business */
	if (((retval = modbus_connect(&mb_param))!=0) || done) {
		printf("%s: ERROR: couldn't open serial device\n", modname);
		goto out_noclose;
	}
	
	
	/* create HAL component */
	hal_comp_id = hal_init(modname);
	if ((hal_comp_id < 0) || done) {
		printf("%s: ERROR: hal_init failed\n", modname);
		retval = hal_comp_id;
		goto out_close;
	} 
	
	/* grab some shmem to store the HAL data in */
	haldata = (haldata_t *)hal_malloc(sizeof(haldata_t));

	if ((haldata == 0) || done) {
		printf("%s: ERROR: unable to allocate shared memory\n", modname);
		retval = -1;
		goto out_close;
	} 

	retval = hal_pin_bit_newf(HAL_IN, &(haldata->enable), hal_comp_id, "%s.enable", modname);
	if (retval!=0) goto out_closeHAL;
	
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->Set_F), hal_comp_id, "%s.SetF", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->Out_F), hal_comp_id, "%s.OutF", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->Out_A), hal_comp_id, "%s.OutA", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->RoTT), hal_comp_id, "%s.Rott", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->DCV), hal_comp_id, "%s.DCV", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->ACV), hal_comp_id, "%s.ACV", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->Cont), hal_comp_id, "%s.Cont", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->Tmp), hal_comp_id, "%s.Tmp", modname);
	if (retval!=0) goto out_closeHAL;
	
	retval = hal_pin_bit_newf(HAL_IN, &(haldata->spindle_fwd), hal_comp_id, "%s.spindle-fwd", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_bit_newf(HAL_IN, &(haldata->spindle_rev), hal_comp_id, "%s.spindle-rev", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_bit_newf(HAL_IN, &(haldata->spindle_on), hal_comp_id, "%s.spindle-on", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->CNTR), hal_comp_id, "%s.CNTR", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->CNST), hal_comp_id, "%s.CNST", modname);
	if (retval!=0) goto out_closeHAL;
	
	retval = hal_pin_bit_newf(HAL_OUT, &(haldata->CNST_Run), hal_comp_id, "%s.CNST-run", modname); 
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_bit_newf(HAL_OUT, &(haldata->CNST_Jog), hal_comp_id, "%s.CNST-jog", modname); 
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_bit_newf(HAL_OUT, &(haldata->CNST_Command_rf), hal_comp_id, "%s.CNST-command-rf", modname); 
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_bit_newf(HAL_OUT, &(haldata->CNST_Running), hal_comp_id, "%s.CNST-running", modname); 
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_bit_newf(HAL_OUT, &(haldata->CNST_Jogging), hal_comp_id, "%s.CNST-jogging", modname); 
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_bit_newf(HAL_OUT, &(haldata->CNST_Running_rf), hal_comp_id, "%s.CNST-running-rf", modname); 
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_bit_newf(HAL_OUT, &(haldata->CNST_Bracking), hal_comp_id, "%s.CNST-bracking", modname); 
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_bit_newf(HAL_OUT, &(haldata->CNST_Track_Start), hal_comp_id, "%s.CNST-track-start", modname); 
	if (retval!=0) goto out_closeHAL;
	
	retval = hal_pin_float_newf(HAL_IN, &(haldata->speed_command), hal_comp_id, "%s.speed-command", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->freq_cmd), hal_comp_id, "%s.frequency-command", modname);
	if (retval!=0) goto out_closeHAL;
	
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->max_freq), hal_comp_id, "%s.max-freq", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->freq_lower_limit), hal_comp_id, "%s.freq-lower-limit", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->rated_motor_voltage), hal_comp_id, "%s.rated-motor-voltage", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->rated_motor_current), hal_comp_id, "%s.rated-motor-current", modname);
	if (retval!=0) goto out_closeHAL;
	retval = hal_pin_float_newf(HAL_OUT, &(haldata->rated_motor_rev), hal_comp_id, "%s.rated-motor-rev", modname);
	if (retval!=0) goto out_closeHAL;

	retval = hal_pin_bit_newf(HAL_OUT, &(haldata->modbus_ok), hal_comp_id, "%s.modbus-ok", modname); 
	if (retval!=0) goto out_closeHAL;
	
	retval = hal_param_s32_newf(HAL_RW, &(haldata->errorcount), hal_comp_id, "%s.error-count", modname);
	if (retval!=0) goto out_closeHAL;

	retval = hal_param_float_newf(HAL_RW, &(haldata->retval), hal_comp_id, "%s.retval", modname);
	if (retval!=0) goto out_closeHAL;

	/* make default data match what we expect to use */

	*(haldata->enable) = 0;
	
	*(haldata->Set_F) = 0;
	*(haldata->Out_F) = 0;
	*(haldata->Out_A) = 0;
	*(haldata->RoTT) = 0;
	*(haldata->DCV) = 0;
	*(haldata->ACV) = 0;
	*(haldata->Cont) = 0;
	*(haldata->Tmp) = 0;
	
	*(haldata->spindle_fwd) = 0;
	*(haldata->spindle_rev) = 0;
	*(haldata->spindle_on) = 0;
	*(haldata->freq_cmd) = 0;
	*(haldata->CNTR) = 0;
	*(haldata->CNST) = 0;
	
	*(haldata->max_freq) = 0;
	*(haldata->freq_lower_limit) = 0;
	*(haldata->rated_motor_voltage) = 0;
	*(haldata->rated_motor_current) = 0;
	*(haldata->rated_motor_rev) = 0;

	*(haldata->modbus_ok) = 0;

	mb_data.slave = slave;
	haldata->errorcount = 0;
	haldata->looptime = 0.1;

	
	//haldata->speed_tolerance = 0.01;  	//output frequency within 1% of target frequency
	//haldata->motor_nameplate_hz = 50;	// folks in The Colonies typically would use 60Hz and 1730 rpm
	//haldata->motor_nameplate_RPM = 1410;
	//haldata->rpm_limit = MAX_RPM;
	//haldata->acc_dec_pattern = 0;

	//haldata->old_run = -1;		// make sure the initial value gets output
	//haldata->old_dir = -1;
	//haldata->old_err_reset = -1;
	//haldata->failed_reg = 0;

	hal_ready(hal_comp_id);
	mb_data.slave = slave;
	
	// wait until EMC and AXIS is ready, ie enable bit is set
	while (!*(haldata->enable)){
		// do nothing until enabled
	}
	
	// read the VFD setup parameters
	retval = read_setup(&mb_param, &mb_data, haldata);
	// here's the meat of the program.  loop until done (which may be never)
	while (done==0) {
		
		if (*(haldata->enable)) {	
			// Read inputs
			if (read_data(&mb_param, &mb_data, haldata) < 0) {
				modbus_ok = 0;
			} else {
				modbus_ok++;
			}
			
			// Set outputs
			if (write_data(&mb_param, &mb_data, haldata) < 0) {
				modbus_ok = 0;
			} else {
				modbus_ok++;
			}
		}
		

		
		if (modbus_ok > MODBUS_MIN_OK) {
			*(haldata->modbus_ok) = 1;
		} else {
			*(haldata->modbus_ok) = 0;
		}

	    
		// don't want to scan too fast, and shouldn't delay more than a few seconds
		if (haldata->looptime < 0.001) haldata->looptime = 0.001;
		if (haldata->looptime > 1.0) haldata->looptime = 1.0;
		loop_timespec.tv_sec = (time_t)(haldata->looptime);
		loop_timespec.tv_nsec = (long)((haldata->looptime - loop_timespec.tv_sec) * 1000000000l);
		nanosleep(&loop_timespec, &remaining);
	
	}

	retval = 0;	/* if we get here, then everything is fine, so just clean up and exit */

	out_closeHAL:
	hal_exit(hal_comp_id);
	out_close:
	modbus_close(&mb_param);
	out_noclose:
	return retval;
}
