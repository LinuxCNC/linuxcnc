/******************************************************************************
 *
 * Copyright (C) 2005 Peter G. Vavaroutsos <pete AT vavaroutsos DOT com>
 *
 * $RCSfile$
 * $Author$
 * $Locker$
 * $Revision$
 * $State$
 * $Date$
 *
 * This is the driver for the Vital Systems MOTENC-100 board.
 * The board includes 8 quadrature decoders, 8 analog inputs,
 * 8 analog outputs, 68 digital inputs, 32 digital outputs,
 * programable timer interrupts, a watch dog timer, and a hardware
 * E-STOP circuit.
 *
 * Installation of the driver (realtime only):
 *
 * insmod hal_motenc
 *
 *
 * The following items are exported to the HAL. <boardId> is read
 * from the jumper settings on the MOTENC-100 board and is formated
 * as "%d". <channel> is formated as "%02d".
 *
 * Encoders:
 *   Parameters:
 *	float	motenc.<boardId>.enc-<channel>-scale
 *
 *   Pins:
 *	s32	motenc.<boardId>.enc-<channel>-count
 *	float	motenc.<boardId>.enc-<channel>-position
 *	bit	motenc.<boardId>.enc-<channel>-index
 *	bit	motenc.<boardId>.enc-<channel>-idx-latch
 *	bit	motenc.<boardId>.enc-<channel>-latch-index
 *	bit	motenc.<boardId>.enc-<channel>-reset-count
 *
 *   Functions:
 *	void    motenc.<boardId>.encoder_read
 *
 *
 * DACs:
 *   Parameters:
 *	float	motenc.<boardId>.dac-<channel>-offset
 *	float	motenc.<boardId>.dac-<channel>-gain
 *
 *   Pins:
 *	float	motenc.<boardId>.dac-<channel>-value
 *
 *   Functions:
 *	void    motenc.<boardId>.dac_write
 *
 *
 * ADC:
 *   Parameters:
 *	float	motenc.<boardId>.adc-<channel>-offset
 *	float	motenc.<boardId>.adc-<channel>-gain
 *
 *   Pins:
 *	float	motenc.<boardId>.adc-<channel>-value
 *
 *   Functions:
 *	void    motenc.<boardId>.adc_read
 *
 *
 * Digital In:
 *   Pins:
 *	bit	motenc.<boardId>.pin-<channel>-in
 *	bit	motenc.<boardId>.pin-<channel>-in-not
 *
 *   Functions:
 *	void    motenc.<boardId>.digital_in_read
 *
 *
 * Digital Out:
 *   Parameters:
 *	bit	motenc.<boardId>.pin-<channel>-out-invert
 *
 *   Pins:
 *	bit	motenc.<boardId>.pin-<channel>-out
 *
 *   Functions:
 *	void    motenc.<boardId>.digital_out_write
 *
 *
 * Miscellaneous:
 *   Parameters:
 *	u32	motenc.<boardId>.watchdog-control
 *		    MOTENC_WATCHDOG_CTL_8MS	    0x00000000
 *		    MOTENC_WATCHDOG_CTL_16MS	    0x00000001
 *		    MOTENC_WATCHDOG_CTL_ENABLE	    0x00000004
 *		    MOTENC_WATCHDOG_CTL_AUTO_RESET  0x00000010 // Reset by DAC writes.
 *
 *   Pins:
 *	bit	motenc.<boardId>.estop-in
 *	bit	motenc.<boardId>.estop-in-not
 *	bit	motenc.<boardId>.watchdog-reset
 *
 *   Functions:
 *	void    motenc.<boardId>.misc_update
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2.1 of the GNU General
 * Public License as published by the Free Software Foundation.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111 USA
 *
 * THE AUTHORS OF THIS LIBRARY ACCEPT ABSOLUTELY NO LIABILITY FOR
 * ANY HARM OR LOSS RESULTING FROM ITS USE.  IT IS _EXTREMELY_ UNWISE
 * TO RELY ON SOFTWARE ALONE FOR SAFETY.  Any machinery capable of
 * harming persons must have provisions for completely removing power
 * from all motors, etc, before persons enter any danger area.  All
 * machinery must be designed to comply with local and national safety
 * codes, and the authors of this software can not, and do not, take
 * any responsibility for such compliance.
 *
 * This code was written as part of the EMC HAL project.  For more
 * information, go to www.linuxcnc.org.
 *
 ******************************************************************************
 *
 * $Log$
 * Revision 1.2.2.3  2005/05/23 18:29:47  paul_c
 * Last set of joins before starting on the /src/emc directories
 *
 * Revision 1.3  2005/05/23 16:34:08  paul_c
 * One more join..
 *
 * Revision 1.2.2.2  2005/05/23 15:45:33  paul_c
 * This is the first stage of joining of the bdi-4 branch and HEAD. It *may* break both HEAD and the branch, but this will be fixed in later commits.
 * Note: Just the rtapi, hal, and libnml directories have been joined so far. A few minor changes need to be made in HEAD before a join can progress there..
 *
 * Revision 1.2  2005/05/23 04:22:30  jmkasunich
 * merged the last couple weeks of work from the kbuild-0-1 branch to HEAD.  HEAD should now compile under all BDIs, including BDI-4.20 (kernel 2.6)
 *
 * Revision 1.1.2.1  2005/05/08 15:40:34  jmkasunich
 * fixed some warning messages in hal_motenc.c - uninitialized variables
 *
 * Revision 1.1  2005/03/31 21:35:40  petev
 * Initial revision.
 *
 ******************************************************************************/

#ifndef RTAPI
#error This is a realtime component only!
#endif


#include <linux/pci.h>

#include "rtapi.h"			// RTAPI realtime OS API.
#include "rtapi_app.h"			// RTAPI realtime module decls.
#include "hal.h"			// HAL public API decls.
#include "motenc.h"			// Hardware dependent defines.


#ifndef MODULE
#define MODULE
#endif


#ifdef MODULE
// Module information.
MODULE_AUTHOR("Pete Vavaroutsos");
MODULE_DESCRIPTION("Driver for Vital Systems MOTENC-100 for EMC HAL");
#ifdef MODULE_LICENSE
MODULE_LICENSE("GPL");
#endif // MODULE_LICENSE
static long				period = 0;// Thread period (0 = no thread).
MODULE_PARM(period, "l");
MODULE_PARM_DESC(period, "thread period (nsecs)");
#endif // MODULE


/******************************************************************************
 * DEVICE OBJECT
 *
 * This object contains all the data for one card. A device object is
 * dynamically allocated in shmem for each card during initialization.
 *
 ******************************************************************************/


typedef struct {
    // Pins.
    hal_s32_t				*pCount;	// Captured binary count value.
    hal_float_t				*pPosition;	// Scaled position (floating point).
    hal_bit_t				*pIndex;	// Current state of index.
    hal_bit_t				*pIndexLatch;
    hal_bit_t				*pLatchIndex;	// Setting this pin enables the index
							// latch. When the next index pulse is
							// seen, Count is cleared and IndexLatch
							// is set. Clearing this pin resets the
							// index latch. Use this feature at your
							// own risk as the PID loop may get upset.
    hal_bit_t				*pResetCount;	// Setting this pin causes Count to be reset.
							// This pin is self clearing.

    // Parameters.
    hal_float_t				scale;		// Scaling factor for position.
} EncoderPinsParams;

typedef struct {
    // Pins.
    hal_float_t				*pValue;	// Desired value.

    // Parameters.
    hal_float_t				offset;
    hal_float_t				gain;
} DacPinsParams;

typedef struct {
    // Pins.
    hal_float_t				*pValue;	// Converted value.

    // Parameters.
    hal_float_t				offset;
    hal_float_t				gain;
} AdcPinsParams;

typedef struct {
    // Pins.
    hal_bit_t				*pValue;
    hal_bit_t				*pValueNot;
} DigitalInPinsParams;

typedef struct {
    // Pins.
    hal_bit_t				*pValue;

    // Parameters.
    hal_bit_t				invert;
} DigitalOutPinsParams;

typedef struct {
    // Pins.
    hal_bit_t				*pEstopIn;
    hal_bit_t				*pEstopInNot;
    hal_bit_t				*pWatchdogReset;// This pin is self clearing.

    // Parameters.
    hal_u32_t				watchdogControl;
} MiscPinsParams;

typedef struct {
    MotencRegMap			*pCard;
    int					adcState;
    hal_u32_t				watchdogControl;// Shadow HW register.

    // Exported to HAL.
    EncoderPinsParams			encoder[MOTENC_NUM_ENCODER_CHANNELS];
    DacPinsParams			dac[MOTENC_NUM_DAC_CHANNELS];
    AdcPinsParams			adc[MOTENC_NUM_ADC_CHANNELS];
    DigitalInPinsParams			in[MOTENC_NUM_DIGITAL_INPUTS];
    DigitalOutPinsParams		out[MOTENC_NUM_DIGITAL_OUTPUTS];
    MiscPinsParams			misc;
} Device;


// These methods are used for initialization.
static int Device_Init(Device *this, MotencRegMap *pCard);
static int Device_ExportPinsParametersFunctions(Device *this, int componentId);
static int Device_ExportEncoderPinsParametersFunctions(Device *this, int componentId, int boardId);
static int Device_ExportDacPinsParametersFunctions(Device *this, int componentId, int boardId);
static int Device_ExportAdcPinsParametersFunctions(Device *this, int componentId, int boardId);
static int Device_ExportDigitalInPinsParametersFunctions(Device *this, int componentId, int boardId);
static int Device_ExportDigitalOutPinsParametersFunctions(Device *this, int componentId, int boardId);
static int Device_ExportMiscPinsParametersFunctions(Device *this, int componentId, int boardId);

// These methods are exported to the HAL.
static void Device_EncoderRead(void *this, long period);
static void Device_DacWrite(void *this, long period);
static void Device_AdcRead(void *this, long period);
static void Device_DigitalInRead(void *this, long period);
static void Device_DigitalOutWrite(void *this, long period);
static void Device_MiscUpdate(void *this, long period);

// Private helper methods.
static int Device_AdcRead4(Device *this, int startChannel);


/******************************************************************************
 * DRIVER OBJECT
 *
 * This object contains all the data for this HAL component.
 *
 ******************************************************************************/

#define MAX_DEVICES			4		// Since there are 2 board id bits.

typedef struct {
    int					componentId;	// HAL component ID.
    Device				*deviceTable[MAX_DEVICES];
} Driver;

static Driver				driver;


/******************************************************************************
 * INIT AND EXIT CODE
 ******************************************************************************/

int
rtapi_app_main(void)
{
    int					i;
    struct pci_dev			*pDev = NULL;
    MotencRegMap			*pCard = NULL;
    Device				*pDevice;

    // Connect to the HAL.
    driver.componentId = hal_init("hal_motenc");
    if (driver.componentId < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "MOTENC: ERROR: hal_init() failed\n");
	return(-1);
    }

    for(i = 0; i < MAX_DEVICES; i++){
	driver.deviceTable[i] = NULL;
    }

    i = 0;

    // Find a MOTENC-100 card.
    while((i < MAX_DEVICES) && ((pDev = pci_find_device(MOTENC_VENDOR_ID, MOTENC_DEVICE_ID, pDev)) != NULL)){

	// Allocate memory for device object.
	pDevice = hal_malloc(sizeof(Device));

	if (pDevice == 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "MOTENC: ERROR: hal_malloc() failed\n");
	    hal_exit(driver.componentId);
	    return(-1);
	}

	// Save pointer to device object.
	driver.deviceTable[i++] = pDevice;

	// Map card into memory.
	pCard = (MotencRegMap *)ioremap_nocache(pci_resource_start(pDev, 2), pci_resource_len(pDev, 2));
	rtapi_print_msg(RTAPI_MSG_INFO, "MOTENC: Card address @ %x, Len = %d\n", (int)pCard, (int)pci_resource_len(pDev, 2));
	rtapi_print_msg(RTAPI_MSG_INFO, "MOTENC: Card detected in Slot: %2x\n", PCI_SLOT(pDev->devfn));

	// Initialize device.
	Device_Init(pDevice, pCard);

	// Export pins, parameters, and functions.
	if(Device_ExportPinsParametersFunctions(pDevice, driver.componentId)){
	    hal_exit(driver.componentId);
	    return(-1);
	}
    }

    if(pCard == NULL){
	// No card present.
	rtapi_print_msg(RTAPI_MSG_WARN, "MOTENC: **** No MOTENC card detected ****\n");
	hal_exit(driver.componentId);
	return -1;
    }

    // Was 'period' specified in the insmod command?
    if (period > 0) {

	// Create a thread.
	if (hal_create_thread("motenc.thread", period, 1) < 0){
	    rtapi_print_msg(RTAPI_MSG_ERR, "MOTENC: ERROR: hal_create_thread() failed\n");
	    hal_exit(driver.componentId);
	    return(-1);
	} else {
	    rtapi_print_msg(RTAPI_MSG_INFO, "MOTENC: created %d uS thread\n", period / 1000);
	}
    }

    return(0);
}


void
rtapi_app_exit(void)
{
    int					i;
    Device				*pDevice;

    hal_exit(driver.componentId);

    for(i = 0; i < MAX_DEVICES; i++){
	if((pDevice = driver.deviceTable[i]) != NULL){
	    // Unmap card.
	    iounmap((void *)(pDevice->pCard));

	    // TODO: Free device object when HAL supports free.
//	    hal_free(pDevice);
	}
    }
}


/******************************************************************************
 * DEVICE OBJECT FUNCTION DEFINITIONS
 ******************************************************************************/

/*
 * LOCAL FUNCTIONS
 */

static int
Device_Init(Device *this, MotencRegMap *pCard)
{
    int					i;

    this->pCard = pCard;
    this->adcState = 0;
    this->watchdogControl = 0;

    // Initialize hardware.
    for(i = 0; i < MOTENC_NUM_FPGA; i++){
	pCard->fpga[i].digitalIo = 0;
	pCard->fpga[i].statusControl = MOTENC_CONTROL_ENCODER_RESET;
    }

    for(i = 0; i < MOTENC_NUM_DAC_CHANNELS; i++){
	pCard->dac[i] = MOTENC_DAC_COUNT_ZERO;
    }

    pCard->timerIrqDisable = 1;
    pCard->watchdogControl = this->watchdogControl;

    return(0);
}


static int
Device_ExportPinsParametersFunctions(Device *this, int componentId)
{
    int					msgLevel, boardId, error;

    // This function exports a lot of stuff, which results in a lot of
    // logging if msg_level is at INFO or ALL. So we save the current value
    // of msg_level and restore it later.  If you actually need to log this
    // function's actions, change the second line below.
    msgLevel = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    // Read board id. The user sets this via jumpers on the card.
    boardId = (this->pCard->fpga[0].statusControl & MOTENC_STATUS_BOARD_ID) >> MOTENC_STATUS_BOARD_ID_SHFT;

    // Export encoders.
    error = Device_ExportEncoderPinsParametersFunctions(this, componentId, boardId);

    // Export DACs.
    if(!error) error = Device_ExportDacPinsParametersFunctions(this, componentId, boardId);

    // Export ADCs.
    if(!error) error = Device_ExportAdcPinsParametersFunctions(this, componentId, boardId);

    // Export digital I/O.
    if(!error) error = Device_ExportDigitalInPinsParametersFunctions(this, componentId, boardId);
    if(!error) error = Device_ExportDigitalOutPinsParametersFunctions(this, componentId, boardId);

    // Export miscellaneous.
    if(!error) error = Device_ExportMiscPinsParametersFunctions(this, componentId, boardId);

    // Restore saved message level.
    rtapi_set_msg_level(msgLevel);

    return(error);
}


static int
Device_ExportEncoderPinsParametersFunctions(Device *this, int componentId, int boardId)
{
    int					halError, channel;
    char				name[HAL_NAME_LEN + 2];

    // Export pins and parameters.
    halError = 0;
    for(channel = 0; channel < MOTENC_NUM_ENCODER_CHANNELS; channel++){
	// Pins.
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.enc-%02d-count", boardId, channel);
	if((halError = hal_pin_s32_new(name, HAL_WR, &(this->encoder[channel].pCount), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.enc-%02d-position", boardId, channel);
	if((halError = hal_pin_float_new(name, HAL_WR, &(this->encoder[channel].pPosition), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.enc-%02d-index", boardId, channel);
	if((halError = hal_pin_bit_new(name, HAL_WR, &(this->encoder[channel].pIndex), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.enc-%02d-idx-latch", boardId, channel);
	if((halError = hal_pin_bit_new(name, HAL_WR, &(this->encoder[channel].pIndexLatch), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.enc-%02d-latch-index", boardId, channel);
	if((halError = hal_pin_bit_new(name, HAL_RD, &(this->encoder[channel].pLatchIndex), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.enc-%02d-reset-count", boardId, channel);
	if((halError = hal_pin_bit_new(name, HAL_RD_WR, &(this->encoder[channel].pResetCount), componentId)) != 0)
	    break;

	// Parameters.
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.enc-%02d-scale", boardId, channel);
	if((halError = hal_param_float_new(name, HAL_WR, &(this->encoder[channel].scale), componentId)) != 0)
	    break;

	// Init encoder.
	*(this->encoder[channel].pCount) = 0;
	*(this->encoder[channel].pPosition) = 0.0;
	*(this->encoder[channel].pIndex) = 0;
	*(this->encoder[channel].pIndexLatch) = 0;
	*(this->encoder[channel].pLatchIndex) = 0;
	*(this->encoder[channel].pResetCount) = 0;
	this->encoder[channel].scale = 1.0;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.encoder_read", boardId);
	halError = hal_export_funct(name, Device_EncoderRead, this, 1, 0, componentId);
    }

    if(halError){
	rtapi_print_msg(RTAPI_MSG_ERR, "MOTENC: ERROR: export encoder failed\n");
	return(-1);
    }

    return(0);
}


static int
Device_ExportDacPinsParametersFunctions(Device *this, int componentId, int boardId)
{
    int					halError, channel;
    char				name[HAL_NAME_LEN + 2];

    // Export pins and parameters.
    halError = 0;
    for(channel = 0; channel < MOTENC_NUM_DAC_CHANNELS; channel++){
	// Pins.
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.dac-%02d-value", boardId, channel);
	if((halError = hal_pin_float_new(name, HAL_RD, &(this->dac[channel].pValue), componentId)) != 0)
	    break;

	// Parameters.
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.dac-%02d-offset", boardId, channel);
	if((halError = hal_param_float_new(name, HAL_WR, &(this->dac[channel].offset), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.dac-%02d-gain", boardId, channel);
	if((halError = hal_param_float_new(name, HAL_WR, &(this->dac[channel].gain), componentId)) != 0)
	    break;

	// Init DAC.
	*(this->dac[channel].pValue) = 0.0;
	this->dac[channel].offset = 0.0;
	this->dac[channel].gain = 1.0;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.dac_write", boardId);
	halError = hal_export_funct(name, Device_DacWrite, this, 1, 0, componentId);
    }

    if(halError){
	rtapi_print_msg(RTAPI_MSG_ERR, "MOTENC: ERROR: export DAC failed\n");
	return(-1);
    }

    return(0);
}


static int
Device_ExportAdcPinsParametersFunctions(Device *this, int componentId, int boardId)
{
    int					halError, channel;
    char				name[HAL_NAME_LEN + 2];

    // Export pins and parameters.
    halError = 0;
    for(channel = 0; channel < MOTENC_NUM_ADC_CHANNELS; channel++){
	// Pins.
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.adc-%02d-value", boardId, channel);
	if((halError = hal_pin_float_new(name, HAL_WR, &(this->adc[channel].pValue), componentId)) != 0)
	    break;

	// Parameters.
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.adc-%02d-offset", boardId, channel);
	if((halError = hal_param_float_new(name, HAL_WR, &(this->adc[channel].offset), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.adc-%02d-gain", boardId, channel);
	if((halError = hal_param_float_new(name, HAL_WR, &(this->adc[channel].gain), componentId)) != 0)
	    break;

	// Init ADC.
	*(this->adc[channel].pValue) = 0.0;
	this->adc[channel].offset = 0.0;
	this->adc[channel].gain = 1.0;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.adc_read", boardId);
	halError = hal_export_funct(name, Device_AdcRead, this, 1, 0, componentId);
    }

    if(halError){
	rtapi_print_msg(RTAPI_MSG_ERR, "MOTENC: ERROR: export ADC failed\n");
	return(-1);
    }

    return(0);
}


static int
Device_ExportDigitalInPinsParametersFunctions(Device *this, int componentId, int boardId)
{
    int					halError, channel;
    char				name[HAL_NAME_LEN + 2];

    // Export pins and parameters.
    halError = 0;
    for(channel = 0; channel < MOTENC_NUM_DIGITAL_INPUTS; channel++){
	// Pins.
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.pin-%02d-in", boardId, channel);
	if((halError = hal_pin_bit_new(name, HAL_WR, &(this->in[channel].pValue), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.pin-%02d-in-not", boardId, channel);
	if((halError = hal_pin_bit_new(name, HAL_WR, &(this->in[channel].pValueNot), componentId)) != 0)
	    break;

	// Init pin.
	*(this->in[channel].pValue) = 0;
	*(this->in[channel].pValueNot) = 1;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.digital_in_read", boardId);
	halError = hal_export_funct(name, Device_DigitalInRead, this, 0, 0, componentId);
    }

    if(halError){
	rtapi_print_msg(RTAPI_MSG_ERR, "MOTENC: ERROR: export digital in failed\n");
	return(-1);
    }

    return(0);
}


static int
Device_ExportDigitalOutPinsParametersFunctions(Device *this, int componentId, int boardId)
{
    int					halError, channel;
    char				name[HAL_NAME_LEN + 2];

    // Export pins and parameters.
    halError = 0;
    for(channel = 0; channel < MOTENC_NUM_DIGITAL_OUTPUTS; channel++){
	// Pins.
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.pin-%02d-out", boardId, channel);
	if((halError = hal_pin_bit_new(name, HAL_RD, &(this->out[channel].pValue), componentId)) != 0)
	    break;

	// Parameters.
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.pin-%02d-out-invert", boardId, channel);
	if((halError = hal_param_bit_new(name, HAL_WR, &(this->out[channel].invert), componentId)) != 0)
	    break;

	// Init pin.
	*(this->out[channel].pValue) = 0;
	this->out[channel].invert = 1;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.digital_out_write", boardId);
	halError = hal_export_funct(name, Device_DigitalOutWrite, this, 0, 0, componentId);
    }

    if(halError){
	rtapi_print_msg(RTAPI_MSG_ERR, "MOTENC: ERROR: export digital out failed\n");
	return(-1);
    }

    return(0);
}


static int
Device_ExportMiscPinsParametersFunctions(Device *this, int componentId, int boardId)
{
    int					halError;
    char				name[HAL_NAME_LEN + 2];

    // Export Pins.
    rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.estop-in", boardId);
    halError = hal_pin_bit_new(name, HAL_WR, &(this->misc.pEstopIn), componentId);

    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.estop-in-not", boardId);
	halError = hal_pin_bit_new(name, HAL_WR, &(this->misc.pEstopInNot), componentId);
    }

    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.watchdog-reset", boardId);
	halError = hal_pin_bit_new(name, HAL_RD_WR, &(this->misc.pWatchdogReset), componentId);
    }

    // Export Parameters.
    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.watchdog-control", boardId);
	halError = hal_param_u32_new(name, HAL_WR, &(this->misc.watchdogControl), componentId);
    }

    // Init pins.
    if(!halError){
	*(this->misc.pEstopIn) = 0;
	*(this->misc.pEstopInNot) = 1;
	*(this->misc.pWatchdogReset) = 0;
	this->misc.watchdogControl = this->watchdogControl;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "motenc.%d.misc_update", boardId);
	halError = hal_export_funct(name, Device_MiscUpdate, this, 0, 0, componentId);
    }

    if(halError){
	rtapi_print_msg(RTAPI_MSG_ERR, "MOTENC: ERROR: export miscellaneous failed\n");
	return(-1);
    }

    return(0);
}


/*
 * HAL EXPORTED FUNCTIONS
 */

static void
Device_EncoderRead(void *arg, long period)
{
    Device				*this = (Device *)arg;
    MotencRegMap			*pCard = this->pCard;
    EncoderPinsParams			*pEncoder;
    int					i, j;
    hal_u32_t				status;

    pEncoder = &this->encoder[0];

    // For each FPGA.
    for(i = 0; i < MOTENC_NUM_FPGA; i++){

	// For each encoder.
	for(j = 0; j < MOTENC_FPGA_NUM_ENCODER_CHANNELS; j++, pEncoder++){

	    // Check reset pin.
	    if(*(pEncoder->pResetCount)){
		// Clear pin.
		*(pEncoder->pResetCount) = 0;

		// Reset encoder.
		pCard->fpga[i].statusControl = 1 << (MOTENC_CONTROL_ENCODER_RESET_SHFT + j);
	    }

	    // Write index latch with pin value.
	    pCard->fpga[i].encoderCount[j] = *(pEncoder->pLatchIndex);

	    // Update index and index latched pins.
	    status = pCard->fpga[i].statusControl;
	    *(pEncoder->pIndex) = (status >> (MOTENC_STATUS_INDEX_SHFT + j)) & 1;
	    *(pEncoder->pIndexLatch) = (status >> (MOTENC_STATUS_INDEX_LATCH_SHFT + j)) & 1;

	    // Read encoder counts.
	    *(pEncoder->pCount) = pCard->fpga[i].encoderCount[j];

	    // Scale count to make floating point position.
	    *(pEncoder->pPosition) = *(pEncoder->pCount) * pEncoder->scale;
	}
    }
}


static void
Device_DacWrite(void *arg, long period)
{
    Device				*this = (Device *)arg;
    MotencRegMap			*pCard = this->pCard;
    DacPinsParams			*pDac;
    int					i;
    hal_float_t				volts;
    hal_u32_t				dacCount;

    pDac = &this->dac[0];

    // For each DAC.
    for(i = 0; i < MOTENC_NUM_DAC_CHANNELS; i++, pDac++){

	// Calculate hardware register value.
	volts = (*(pDac->pValue) - pDac->offset) * pDac->gain;

	// Truncate volts to DAC limits.
	if(volts > MOTENC_DAC_VOLTS_MAX){
	    volts = MOTENC_DAC_VOLTS_MAX;
	}else if(volts < MOTENC_DAC_VOLTS_MIN){
	    volts = MOTENC_DAC_VOLTS_MIN;
	}

	// Transform volts to counts.
	dacCount = (hal_u32_t)(volts * MOTENC_DAC_SCALE_MULTIPLY /
				MOTENC_DAC_SCALE_DIVIDE + MOTENC_DAC_COUNT_ZERO);

	// Write DAC.
	pCard->dac[i] = dacCount;
    }
}


static void
Device_AdcRead(void *arg, long period)
{
    Device				*this = (Device *)arg;
    MotencRegMap			*pCard = this->pCard;

    switch(this->adcState){
    // Start conversion on first 4 channels.
    case 0:
	this->adcState++;
	pCard->adcDataCommand = MOTENC_ADC_COMMAND_CHN_0_1_2_3;
	pCard->adcStartConversion = 1;
	break;
    
    // Wait for first conversion, start conversion on second 4 channels.
    case 1:
	if(Device_AdcRead4(this, 0)){
	    this->adcState++;

	    // Start next conversion.
	    pCard->adcDataCommand = MOTENC_ADC_COMMAND_CHN_4_5_6_7;
	    pCard->adcStartConversion = 1;
	}
	break;

    // Wait for second conversion, start conversion on fisrt 4 channels.
    case 2:
	if(Device_AdcRead4(this, 4)){
	    this->adcState = 1;

	    // Start next conversion.
	    pCard->adcDataCommand = MOTENC_ADC_COMMAND_CHN_0_1_2_3;
	    pCard->adcStartConversion = 1;
	}
	break;

    default:
	this->adcState = 0;
    }
}


static int
Device_AdcRead4(Device *this, int startChannel)
{
    MotencRegMap			*pCard = this->pCard;
    AdcPinsParams			*pAdc;
    int					i;
    hal_s32_t				adcCount;
    hal_float_t				volts;

    if(pCard->fpga[0].statusControl & MOTENC_STATUS_ADC_DONE)
	return(0);

    pAdc = &this->adc[startChannel];

    // Get conversion results.
    for(i = 0; i < 4; i++, pAdc++){
	adcCount = pCard->adcDataCommand;

	// Sign extend result.
	if(adcCount & MOTENC_ADC_SIGN_BIT){
	    adcCount |= MOTENC_ADC_SIGN_EXTEND;
	}

	// Transform count to volts.
	volts = adcCount * MOTENC_ADC_SCALE_MULTIPLY / MOTENC_ADC_SCALE_DIVIDE;

	// Scale and offset volts.
	volts = volts * pAdc->gain - pAdc->offset;

	// Update pin.
	*(pAdc->pValue) = volts;
    }

    return(1);
}


static void
Device_DigitalInRead(void *arg, long period)
{
    Device				*this = (Device *)arg;
    MotencRegMap			*pCard = this->pCard;
    DigitalInPinsParams			*pDigitalIn;
    int					i, j, n;
    hal_u32_t				pins;

    pDigitalIn = &this->in[0];

    // For each FPGA.
    for(i = 0; i < MOTENC_NUM_FPGA; i++){

	// Read digital I/O register.
	pins = pCard->fpga[i].digitalIo >> MOTENC_DIGITAL_IN_SHFT;

	// For each pin.
	for(j = 0; j < 16; j++, pDigitalIn++){

	    // Update pins.
	    *(pDigitalIn->pValue) = pins & 1;
	    *(pDigitalIn->pValueNot) = !*(pDigitalIn->pValue);

	    pins >>= 1;
	}

	// Read status register.
	pins = pCard->fpga[i].statusControl >> MOTENC_STATUS_DIGITAL_IN_SHFT;

	// First FPGA only has 16 inputs in the status register. The other 4 are
	// used for special purpose inputs like board id, ADC done, and E-STOP.
	n = (i)? 20: 16;

	// For each pin.
	for(j = 0; j < n; j++, pDigitalIn++){

	    // Update pins.
	    *(pDigitalIn->pValue) = pins & 1;
	    *(pDigitalIn->pValueNot) = !*(pDigitalIn->pValue);

	    pins >>= 1;
	}
    }
}


static void
Device_DigitalOutWrite(void *arg, long period)
{
    Device				*this = (Device *)arg;
    MotencRegMap			*pCard = this->pCard;
    DigitalOutPinsParams		*pDigitalOut;
    int					i, j;
    hal_u32_t				pins, mask;

    pDigitalOut = &this->out[0];

    // For each FPGA.
    for(i = 0; i < MOTENC_NUM_FPGA; i++){

	pins = 0;
	mask = 1;

	// For each pin.
	for(j = 0; j < MOTENC_FPGA_NUM_DIGITAL_OUTPUTS; j++, pDigitalOut++){

	    // Build hardware register value.
	    if(*(pDigitalOut->pValue) != pDigitalOut->invert){
		pins |= mask;
	    }

	    mask <<=1;
	}

	// Write digital I/O register.
	pCard->fpga[i].digitalIo = pins << MOTENC_DIGITAL_OUT_SHFT;
    }
}


static void
Device_MiscUpdate(void *arg, long period)
{
    Device				*this = (Device *)arg;
    MotencRegMap			*pCard = this->pCard;

    // Check watchdog control parameter.
    if(this->watchdogControl != this->misc.watchdogControl){
	// Update shadow register.
	this->watchdogControl = this->misc.watchdogControl;

	// Write hardware.
	pCard->watchdogControl = this->watchdogControl;
    }

    // Check watchdog reset pin.
    if(*(this->misc.pWatchdogReset)){
	// Clear pin.
	*(this->misc.pWatchdogReset) = 0;

	// Reset the watchdog timer.
	pCard->watchdogReset = MOTENC_WATCHDOG_RESET_VALUE;
    }

    // Update E-STOP pin.
    *(this->misc.pEstopIn) = (pCard->fpga[0].statusControl & MOTENC_STATUS_ESTOP)? 1: 0;
    *(this->misc.pEstopInNot) = !*(this->misc.pEstopIn);
}
