/******************************************************************************
 *
 * Copyright (C) 2005 Peter G. Vavaroutsos <pete AT vavaroutsos DOT com>
 * License: GPL Version 2
 *
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
 * from the jumper settings on the MOTENC-100 board and is formatted
 * as "%d". <channel> is formatted as "%02d".
 *
 * Encoders:
 *   Parameters:
 *	float	motenc.<boardId>.enc-<channel>-scale
 *
 *   Pins:
 *	s32	motenc.<boardId>.enc-<channel>-count
 *	float	motenc.<boardId>.enc-<channel>-position
 *	bit	motenc.<boardId>.enc-<channel>-index
 *	bit	motenc.<boardId>.enc-<channel>-index-enable
 *	bit	motenc.<boardId>.enc-<channel>-reset
 *
 *   Functions:
 *	void    motenc.<boardId>.encoder-read
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
 *	void    motenc.<boardId>.dac-write
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
 *	void    motenc.<boardId>.adc-read
 *
 *
 * Digital In:
 *   Pins:
 *	bit	motenc.<boardId>.pin-<channel>-in
 *	bit	motenc.<boardId>.pin-<channel>-in-not
 *
 *   Functions:
 *	void    motenc.<boardId>.digital-in-read
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
 *	void    motenc.<boardId>.digital-out-write
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
 *	void    motenc.<boardId>.misc-update
 *
 ******************************************************************************
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General
 * Public License as published by the Free Software Foundation.
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
 ******************************************************************************/

#include <linux/pci.h>

#include "rtapi.h"			// RTAPI realtime OS API.
#include "rtapi_app.h"			// RTAPI realtime module decls.
#include "hal.h"			// HAL public API decls.
#include "motenc.h"			// Hardware dependent defines.

// Module information.
MODULE_AUTHOR("Pete Vavaroutsos");
MODULE_DESCRIPTION("Driver for Vital Systems MOTENC-100 for EMC HAL");
MODULE_LICENSE("GPL");


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
    hal_bit_t				*pIndexEnable;	// Setting this pin causes the count
							// to be cleared on the next index pulse.
							// Use this feature at your own risk as the PID loop
							// may get upset. This pin is self clearing.
    hal_bit_t				*pReset;	// Setting this pin causes Count to be reset.

    // Parameters.
    hal_float_t				scale;		// Scaling factor for position.

    // Private data.
    double				oldScale;	// Stored scale value.
    double				scaleRecip;	// Reciprocal value used for scaling.
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
    // Private data.
    MotencRegMap			*pCard;
    int					boardType;
    char				*pTypeName;
    int					boardID;
    int					numFpga;
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
    unsigned char			idPresent[MAX_DEVICES];
} Driver;

static Driver				driver;


/******************************************************************************
 * INIT AND EXIT CODE
 ******************************************************************************/

int
rtapi_app_main(void)
{
    int					i, j;
    struct pci_dev			*pDev = NULL;
    MotencRegMap			*pCard = NULL;
    Device				*pDevice;

    // Connect to the HAL.
    driver.componentId = hal_init("hal_motenc");
    if (driver.componentId < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "MOTENC: ERROR: hal_init() failed\n");
	return(-EINVAL);
    }

    for(i = 0; i < MAX_DEVICES; i++){
	driver.deviceTable[i] = NULL;
	driver.idPresent[i] = 0;
    }

    i = 0;
    // Find a MOTENC card.
    while((i < MAX_DEVICES) && ((pDev = pci_get_device(MOTENC_VENDOR_ID, MOTENC_DEVICE_ID, pDev)) != NULL)){

	// Allocate memory for device object.
	pDevice = hal_malloc(sizeof(Device));

	if (pDevice == 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "MOTENC: ERROR: hal_malloc() failed\n");
	    hal_exit(driver.componentId);
	    return(-ENOMEM);
	}

	// Save pointer to device object.
	driver.deviceTable[i++] = pDevice;
	
	// Map card into memory.
	pCard = (MotencRegMap *)ioremap_nocache(pci_resource_start(pDev, 2), pci_resource_len(pDev, 2));
	rtapi_print_msg(RTAPI_MSG_INFO, "MOTENC: Card detected in slot %2x\n", PCI_SLOT(pDev->devfn));
	rtapi_print_msg(RTAPI_MSG_INFO, "MOTENC: Card address @ %p, Len = %d\n", pCard, (int)pci_resource_len(pDev, 2));

	// Initialize device.
	Device_Init(pDevice, pCard);
	rtapi_print_msg(RTAPI_MSG_INFO, "MOTENC: Card is %s, ID: %d\n", pDevice->pTypeName, pDevice->boardID);
        if ( pDevice->boardType == 0 ) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "MOTENC: ERROR, unknown card detected\n");
	    hal_exit(driver.componentId);
	    return(-ENODEV);
	}
	

	if ( driver.idPresent[pDevice->boardID] != 0 ) {
	    // duplicate ID... a strict driver would bail out, but
	    // we are nice, we try to find an unused ID
	    j = 0;
	    while ( driver.idPresent[j] != 0 ) {
		j++;
	        if ( j >= MAX_DEVICES ) {
		    rtapi_print_msg(RTAPI_MSG_ERR, "MOTENC: ERROR, duplicate ID, can't remap\n");
		    hal_exit(driver.componentId);
		    return(-EINVAL);
		}
	    }
	    pDevice->boardID = j;
	    rtapi_print_msg(RTAPI_MSG_WARN, "MOTENC: WARNING, duplicate ID, remapped to %d\n", j);
	}
	driver.idPresent[pDevice->boardID] = 1;
	
	// Export pins, parameters, and functions.
	if(Device_ExportPinsParametersFunctions(pDevice, driver.componentId)){
	    hal_exit(driver.componentId);
	    return(-EINVAL);
	}
    }

    if(pCard == NULL){
	// No card present.
	rtapi_print_msg(RTAPI_MSG_WARN, "MOTENC: **** No MOTENC card detected ****\n");
	hal_exit(driver.componentId);
	return -ENODEV;
    }

    hal_ready(driver.componentId);
    return(0);
}


void
rtapi_app_exit(void)
{
    int					i, j;
    Device				*pDevice;

    hal_exit(driver.componentId);

    for(i = 0; i < MAX_DEVICES; i++){
	if((pDevice = driver.deviceTable[i]) != NULL){
	    // turn off digital outputs
	    for(j = 0; j < pDevice->numFpga; j++){
		pDevice->pCard->fpga[j].digitalIo = MOTENC_DIGITAL_OUT;
	    }
	    // set DAC outputs to zero volts
	    for(j = 0; j < MOTENC_NUM_DAC_CHANNELS; j++){
		pDevice->pCard->dac[j] = MOTENC_DAC_COUNT_ZERO;
	    }
	    
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
    int i, status;

    this->pCard = pCard;
    this->adcState = 0;
    this->watchdogControl = 0;

    // Identify type of board.
    status = pCard->fpga[0].boardVersion;
    if ( status == 0 ) {
	// MOTENC-100.
	this->boardType = 1;
	this->pTypeName = "MOTENC-100";
	this->numFpga = 2;
    } else if ( status == 1 ) {
	// MOTENC-Lite.
	this->boardType = 2;
	this->pTypeName = "MOTENC-Lite";
	this->numFpga = 1;
    } else {
	// No idea what it is.
	this->boardType = 0;
	this->pTypeName = "unknown";
	this->numFpga = 0;
	return -1;
    }

    // Extract board id from first FPGA. The user sets this via jumpers on the card.
    status = pCard->fpga[0].statusControl;
    this->boardID = (status & MOTENC_STATUS_BOARD_ID) >> MOTENC_STATUS_BOARD_ID_SHFT;
    
    // Initialize hardware.
    for(i = 0; i < this->numFpga; i++){
	pCard->fpga[i].digitalIo = MOTENC_DIGITAL_OUT;
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

    boardId = this->boardID;
    
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
    char				name[HAL_NAME_LEN + 1];

    // Export pins and parameters.
    halError = 0;
    for(channel = 0; channel < this->numFpga * MOTENC_FPGA_NUM_ENCODER_CHANNELS; channel++){
	// Pins.
	if((halError = hal_pin_s32_newf(HAL_OUT, &(this->encoder[channel].pCount),
	  componentId, "motenc.%d.enc-%02d-count", boardId, channel)) != 0)
	    break;

	if((halError = hal_pin_float_newf(HAL_OUT, &(this->encoder[channel].pPosition),
	  componentId, "motenc.%d.enc-%02d-position", boardId, channel)) != 0)
	    break;

	if((halError = hal_pin_bit_newf(HAL_OUT, &(this->encoder[channel].pIndex),
	  componentId, "motenc.%d.enc-%02d-index", boardId, channel)) != 0)
	    break;

	if((halError = hal_pin_bit_newf(HAL_IO, &(this->encoder[channel].pIndexEnable),
	  componentId, "motenc.%d.enc-%02d-index-enable", boardId, channel)) != 0)
	    break;

	if((halError = hal_pin_bit_newf(HAL_IN, &(this->encoder[channel].pReset),
	  componentId, "motenc.%d.enc-%02d-reset", boardId, channel)) != 0)
	    break;

	// Parameters.
	if((halError = hal_param_float_newf(HAL_RW, &(this->encoder[channel].scale),
	  componentId, "motenc.%d.enc-%02d-scale", boardId, channel)) != 0)
	    break;

	// Init encoder.
	*(this->encoder[channel].pCount) = 0;
	*(this->encoder[channel].pPosition) = 0.0;
	*(this->encoder[channel].pIndex) = 0;
	*(this->encoder[channel].pIndexEnable) = 0;
	*(this->encoder[channel].pReset) = 0;
	this->encoder[channel].scale = 1.0;
	this->encoder[channel].oldScale = 1.0;
	this->encoder[channel].scaleRecip = 1.0 / this->encoder[channel].scale;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, sizeof(name), "motenc.%d.encoder-read", boardId);
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
    char				name[HAL_NAME_LEN + 1];

    // Export pins and parameters.
    halError = 0;
    for(channel = 0; channel < MOTENC_NUM_DAC_CHANNELS; channel++){
	// Pins.
	if((halError = hal_pin_float_newf(HAL_IN, &(this->dac[channel].pValue),
	  componentId, "motenc.%d.dac-%02d-value", boardId, channel)) != 0)
	    break;

	// Parameters.
	if((halError = hal_param_float_newf(HAL_RW, &(this->dac[channel].offset),
	  componentId, "motenc.%d.dac-%02d-offset", boardId, channel)) != 0)
	    break;

	if((halError = hal_param_float_newf(HAL_RW, &(this->dac[channel].gain),
	  componentId, "motenc.%d.dac-%02d-gain", boardId, channel)) != 0)
	    break;

	// Init DAC.
	*(this->dac[channel].pValue) = 0.0;
	this->dac[channel].offset = 0.0;
	this->dac[channel].gain = 1.0;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, sizeof(name), "motenc.%d.dac-write", boardId);
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
    char				name[HAL_NAME_LEN + 1];

    // Export pins and parameters.
    halError = 0;
    for(channel = 0; channel < MOTENC_NUM_ADC_CHANNELS; channel++){
	// Pins.
	if((halError = hal_pin_float_newf(HAL_OUT, &(this->adc[channel].pValue),
	  componentId, "motenc.%d.adc-%02d-value", boardId, channel)) != 0)
	    break;

	// Parameters.
	if((halError = hal_param_float_newf(HAL_RW, &(this->adc[channel].offset),
	  componentId, "motenc.%d.adc-%02d-offset", boardId, channel)) != 0)
	    break;

	if((halError = hal_param_float_newf(HAL_RW, &(this->adc[channel].gain),
	  componentId, "motenc.%d.adc-%02d-gain", boardId, channel)) != 0)
	    break;

	// Init ADC.
	*(this->adc[channel].pValue) = 0.0;
	this->adc[channel].offset = 0.0;
	this->adc[channel].gain = 1.0;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, sizeof(name), "motenc.%d.adc-read", boardId);
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
    char				name[HAL_NAME_LEN + 1];

    // Export pins and parameters.
    halError = 0;
    for(channel = 0; channel < (this->numFpga * MOTENC_FPGA_NUM_DIGITAL_INPUTS - 4); channel++){
	// Pins.
	if((halError = hal_pin_bit_newf(HAL_OUT, &(this->in[channel].pValue),
	  componentId, "motenc.%d.in-%02d", boardId, channel)) != 0)
	    break;

	if((halError = hal_pin_bit_newf(HAL_OUT, &(this->in[channel].pValueNot),
	  componentId, "motenc.%d.in-%02d-not", boardId, channel)) != 0)
	    break;

	// Init pin.
	*(this->in[channel].pValue) = 0;
	*(this->in[channel].pValueNot) = 1;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, sizeof(name), "motenc.%d.digital-in-read", boardId);
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
    char				name[HAL_NAME_LEN + 1];

    // Export pins and parameters.
    halError = 0;
    for(channel = 0; channel < this->numFpga * MOTENC_FPGA_NUM_DIGITAL_OUTPUTS; channel++){
	// Pins.
	if((halError = hal_pin_bit_newf(HAL_IN, &(this->out[channel].pValue),
	  componentId, "motenc.%d.out-%02d", boardId, channel)) != 0)
	    break;

	// Parameters.
	if((halError = hal_param_bit_newf(HAL_RW, &(this->out[channel].invert),
	  componentId, "motenc.%d.out-%02d-invert", boardId, channel)) != 0)
	    break;

	// Init pin.
	*(this->out[channel].pValue) = 0;
	this->out[channel].invert = 0;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, sizeof(name), "motenc.%d.digital-out-write", boardId);
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
    char				name[HAL_NAME_LEN + 1];

    // Export Pins.
    halError = hal_pin_bit_newf(HAL_OUT, &(this->misc.pEstopIn), componentId,
				"motenc.%d.estop-in", boardId);

    if(!halError){
	halError = hal_pin_bit_newf(HAL_OUT, &(this->misc.pEstopInNot), componentId,
				    "motenc.%d.estop-in-not", boardId);
    }

    if(!halError){
	halError = hal_pin_bit_newf(HAL_IO, &(this->misc.pWatchdogReset), componentId,
				    "motenc.%d.watchdog-reset", boardId);
    }

    // Export Parameters.
    if(!halError){
	halError = hal_param_u32_newf(HAL_RW, &(this->misc.watchdogControl), componentId,
				      "motenc.%d.watchdog-control", boardId);
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
	rtapi_snprintf(name, sizeof(name), "motenc.%d.misc-update", boardId);
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
    for(i = 0; i < this->numFpga; i++){

	// read status register just once, before writing anything to the card
	status = pCard->fpga[i].statusControl;

	// For each encoder.
	for(j = 0; j < MOTENC_FPGA_NUM_ENCODER_CHANNELS; j++, pEncoder++){

	    // Check reset pin.
	    if(*(pEncoder->pReset)){
		// Reset encoder.
		pCard->fpga[i].statusControl = 1 << (MOTENC_CONTROL_ENCODER_RESET_SHFT + j);
	    }

	    // check state of hardware index pin
	    *(pEncoder->pIndex) = (status >> (MOTENC_STATUS_INDEX_SHFT + j)) & 1;

	    // check for index pulse detected
	    if((status >> (MOTENC_STATUS_INDEX_LATCH_SHFT + j)) & 1){
		// cancel index-enable
		*(pEncoder->pIndexEnable) = 0;
	    }

	    // Check for index enable request from HAL
	    if(*(pEncoder->pIndexEnable)){
		// tell hardware to latch on index
		pCard->fpga[i].encoderCount[j] = 1;
	    } else {
		// cancel hardware latch on index
		pCard->fpga[i].encoderCount[j] = 0;
	    }

	    // Read encoder counts.
	    *(pEncoder->pCount) = pCard->fpga[i].encoderCount[j];

	    // Check for change in scale value.
	    if ( pEncoder->scale != pEncoder->oldScale ) {
		// Get ready to detect future scale changes.
		pEncoder->oldScale = pEncoder->scale;

		// Validate the new scale value.
		if ((pEncoder->scale < 1e-20) && (pEncoder->scale > -1e-20)) {
		    // Value too small, divide by zero is a bad thing.
		    pEncoder->scale = 1.0;
		}

		// We will need the reciprocal.
		pEncoder->scaleRecip = 1.0 / pEncoder->scale;
	    }

	    // Scale count to make floating point position.
	    *(pEncoder->pPosition) = *(pEncoder->pCount) * pEncoder->scaleRecip;
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
    for(i = 0; i < this->numFpga; i++){

	// Read digital I/O register.
	pins = ~pCard->fpga[i].digitalIo >> MOTENC_DIGITAL_IN_SHFT;

	// For each pin.
	for(j = 0; j < 16; j++, pDigitalIn++){

	    // Update pins.
	    *(pDigitalIn->pValue) = pins & 1;
	    *(pDigitalIn->pValueNot) = !*(pDigitalIn->pValue);

	    pins >>= 1;
	}

	// Read status register.
	pins = ~pCard->fpga[i].statusControl >> MOTENC_STATUS_DIGITAL_IN_SHFT;

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
    for(i = 0; i < this->numFpga; i++){

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
	// invert for active low OPTO-22 modules
	pCard->fpga[i].digitalIo = ~pins << MOTENC_DIGITAL_OUT_SHFT;
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
