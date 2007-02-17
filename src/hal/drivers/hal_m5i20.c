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
 * This is the driver for the Mesa Electronics 5i20 board.
 * The board includes a user programable FPGA. This driver
 * is written for the 4 axis host based motion control FPGA
 * configuration. It includes 8 quadrature decoder channels, 4
 * PWM output channels, and 48 I/O channels.
 *
 * Installation of the driver (realtime only):
 *
 * insmod hal_m5i20 loadFpga=<1|0> dacRate=<Hz>
 *
 *
 * The following items are exported to the HAL. <boardId> is
 * the PCI board number and is formated as "%d". <channel> is
 * formated as "%02d".
 *
 * Encoders:
 *   Parameters:
 *	float	m5i20.<boardId>.enc-<channel>-scale
 *
 *   Pins:
 *	s32	m5i20.<boardId>.enc-<channel>-count
 *	s32	m5i20.<boardId>.enc-<channel>-cnt-latch
 *	float	m5i20.<boardId>.enc-<channel>-position
 *	float	m5i20.<boardId>.enc-<channel>-pos-latch
 *	bit	m5i20.<boardId>.enc-<channel>-index
 *	bit	m5i20.<boardId>.enc-<channel>-idx-latch
 *	bit	m5i20.<boardId>.enc-<channel>-latch-index
 *	bit	m5i20.<boardId>.enc-<channel>-reset-count
 *
 *   Functions:
 *	void    m5i20.<boardId>.encoder-read
 *
 *
 * DACs:
 *   Parameters:
 *	float	m5i20.<boardId>.dac-<channel>-offset
 *	float	m5i20.<boardId>.dac-<channel>-gain
 *	bit	m5i20.<boardId>.dac-<channel>-interlaced
 *
 *   Pins:
 *	bit	m5i20.<boardId>.dac-<channel>-enable
 *	float	m5i20.<boardId>.dac-<channel>-value
 *
 *   Functions:
 *	void    m5i20.<boardId>.dac-write
 *
 *
 * Digital In:
 *   Pins:
 *	bit	m5i20.<boardId>.in-<channel>
 *	bit	m5i20.<boardId>.in-<channel>-not
 *
 *   Functions:
 *	void    m5i20.<boardId>.digital-in-read
 *
 *
 * Digital Out:
 *   Parameters:
 *	bit	m5i20.<boardId>.out-<channel>-invert
 *
 *   Pins:
 *	bit	m5i20.<boardId>.out-<channel>
 *
 *   Functions:
 *	void    m5i20.<boardId>.digital-out-write
 *
 *
 * Miscellaneous:
 *   Parameters:
 *	u16	m5i20.<boardId>.watchdog-control
 *		    0x0001				// 1 = enabled.
 *		    0x0002				// Reset by DAC writes.
 *	u16	m5i20.<boardId>.watchdog-timeout	// In micro-seconds.
 *	u16	m5i20.<boardId>.led-view
 *
 *   Pins:
 *	bit	m5i20.<boardId>.estop-in
 *	bit	m5i20.<boardId>.estop-in-not
 *	bit	m5i20.<boardId>.watchdog-reset
 *
 *   Functions:
 *	void    m5i20.<boardId>.misc-update
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
 ******************************************************************************/

#ifndef RTAPI
#error This is a realtime component only!
#endif


#include <linux/pci.h>

#include "rtapi.h"			// RTAPI realtime OS API.
#include "rtapi_app.h"			// RTAPI realtime module decls.
#include "hal.h"			// HAL public API decls.
#include "plx9030.h"			// Hardware dependent defines.
#include "m5i20.h"			// Hardware dependent defines.


#ifndef MODULE
#define MODULE
#endif


#ifdef MODULE
// Module information.
MODULE_AUTHOR("Pete Vavaroutsos");
MODULE_DESCRIPTION("Driver for Mesa Electronics 5i20 for EMC HAL");
MODULE_LICENSE("GPL");
static unsigned long			loadFpga = 1;
RTAPI_MP_LONG(loadFpga, "Set to have FPGA configuration loaded");
static unsigned long			dacRate = 32000;// PWM rate in Hz. 1 Hz to 32 KHz.
RTAPI_MP_LONG(dacRate, "DAC PWM rate");
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
    hal_s32_t				*pCountLatch;
    hal_float_t				*pPosition;	// Scaled position (floating point).
    hal_float_t				*pPositionLatch;
    hal_bit_t				*pIndex;	// Current state of index.
    hal_bit_t				*pIndexLatch;
    hal_bit_t				*pLatchIndex;	// Setting this pin enables the index
							// latch and clears IndexLatch. When the
							// next index pulse is seen, CountLatch
							// is updated and IndexLatch is set. This
							// pin is self clearing.
    hal_bit_t				*pResetCount;	// Setting this pin causes Count to be reset.
							// This pin is self clearing.

    // Parameters.
    hal_float_t				scale;		// Scaling factor for position.
} EncoderPinsParams;

typedef struct {
    // Pins.
    hal_bit_t				*pEnable;	// Controls HW ENA pin too.
    hal_float_t				*pValue;	// Desired value.

    // Parameters.
    hal_float_t				offset;
    hal_float_t				gain;
    hal_bit_t				interlaced;	// For analog out.
} DacPinsParams;

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
    hal_u32_t				watchdogTimeout;
    hal_u32_t				ledView;
} MiscPinsParams;

#define WDT_CONTROL_ENABLE		0x0001
#define WDT_CONTROL_AUTO_RESET		0x0002

typedef struct {
    M5i20HostMotRegMap			*pCard16;
    M5i20HostMotRegMap			*pCard32;
    Plx9030LocalRegMap			*pBridgeIc;

    hal_bit_t				lastDacInterlaced[M5I20_NUM_PWM_CHANNELS];

    // Exported to HAL.
    EncoderPinsParams			encoder[M5I20_NUM_ENCODER_CHANNELS];
    DacPinsParams			dac[M5I20_NUM_PWM_CHANNELS];
    DigitalInPinsParams			in[M5I20_NUM_DIGITAL_INPUTS];
    DigitalOutPinsParams		out[M5I20_NUM_DIGITAL_OUTPUTS];
    MiscPinsParams			misc;
} Device;


// These methods are used for initialization.
static int Device_Init(Device *this, M5i20HostMotRegMap *pCard16, M5i20HostMotRegMap *pCard32,
			Plx9030LocalRegMap *pBridgeIc);
static int Device_LoadFpga(Device *this);
static void Device_Delay100us(void);
static int Device_ExportPinsParametersFunctions(Device *this, int componentId, int boardId);
static int Device_ExportEncoderPinsParametersFunctions(Device *this, int componentId, int boardId);
static int Device_ExportDacPinsParametersFunctions(Device *this, int componentId, int boardId);
static int Device_ExportDigitalInPinsParametersFunctions(Device *this, int componentId, int boardId);
static int Device_ExportDigitalOutPinsParametersFunctions(Device *this, int componentId, int boardId);
static int Device_ExportMiscPinsParametersFunctions(Device *this, int componentId, int boardId);

// These methods are exported to the HAL.
static void Device_EncoderRead(void *this, long period);
static void Device_DacWrite(void *this, long period);
static void Device_DigitalInRead(void *this, long period);
static void Device_DigitalOutWrite(void *this, long period);
static void Device_MiscUpdate(void *this, long period);

// Private helper methods.
static void Device_WdtReset(Device *this);


/******************************************************************************
 * DRIVER OBJECT
 *
 * This object contains all the data for this HAL component.
 *
 ******************************************************************************/

#define MAX_DEVICES			4		// Would be nice to have board id bits.

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
    M5i20HostMotRegMap			*pCard16 = NULL;
    M5i20HostMotRegMap			*pCard32;
    Plx9030LocalRegMap			*pBridgeIc;
    Device				*pDevice;

    // Connect to the HAL.
    driver.componentId = hal_init("hal_m5i20");
    if (driver.componentId < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, "M5I20: ERROR: hal_init() failed\n");
	return(-1);
    }

    for(i = 0; i < MAX_DEVICES; i++){
	driver.deviceTable[i] = NULL;
    }

    i = 0;

    // TODO: check subsys values after programming into serial prom.
    // Find a M5I20 card.
#if 0
    while((i < MAX_DEVICES) && ((pDev = pci_find_subsys(M5I20_VENDOR_ID, M5I20_DEVICE_ID,
			M5I20_SUBSYS_VENDOR_ID, M5I20_SUBSYS_DEVICE_ID, pDev)) != NULL)){
#else
    while((i < MAX_DEVICES) && ((pDev = pci_find_device(M5I20_VENDOR_ID, M5I20_DEVICE_ID, pDev)) != NULL)){
#endif

	// Allocate memory for device object.
	pDevice = hal_malloc(sizeof(Device));

	if (pDevice == NULL) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "M5I20: ERROR: hal_malloc() failed\n");
	    hal_exit(driver.componentId);
	    return(-1);
	}

	// Save pointer to device object.
	driver.deviceTable[i] = pDevice;

	// Map card into memory.
	rtapi_print_msg(RTAPI_MSG_INFO, "M5I20: Card detected in Slot: %2x\n", PCI_SLOT(pDev->devfn));
	pCard16 = (M5i20HostMotRegMap *)ioremap_nocache(pci_resource_start(pDev, 4), pci_resource_len(pDev, 4));
	rtapi_print_msg(RTAPI_MSG_INFO, "M5I20: Card address @ %lx, Len = %ld\n", (long)pCard16, (long)pci_resource_len(pDev, 4));
	pCard32 = (M5i20HostMotRegMap *)ioremap_nocache(pci_resource_start(pDev, 5), pci_resource_len(pDev, 5));
	rtapi_print_msg(RTAPI_MSG_INFO, "M5I20: Card address @ %lx, Len = %ld\n", (long)pCard32, (long)pci_resource_len(pDev, 5));
	pBridgeIc = (Plx9030LocalRegMap *)ioremap_nocache(pci_resource_start(pDev, 0), pci_resource_len(pDev, 0));
	rtapi_print_msg(RTAPI_MSG_INFO, "M5I20: Card address @ %lx, Len = %ld\n", (long)pBridgeIc, (long)pci_resource_len(pDev, 0));

	// Initialize device.
	if(Device_Init(pDevice, pCard16, pCard32, pBridgeIc)){
	    hal_exit(driver.componentId);
	    return(-1);
	}

	// Export pins, parameters, and functions.
	if(Device_ExportPinsParametersFunctions(pDevice, driver.componentId, i++)){
	    hal_exit(driver.componentId);
	    return(-1);
	}
    }

    if(pCard16 == NULL){
	// No card present.
	rtapi_print_msg(RTAPI_MSG_WARN, "M5I20: **** No M5I20 card detected ****\n");
	hal_exit(driver.componentId);
	return(-1);
    }

    hal_ready(driver.componentId);
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
	    iounmap((void *)(pDevice->pCard16));
	    iounmap((void *)(pDevice->pCard32));
	    iounmap((void *)(pDevice->pBridgeIc));

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
Device_Init(Device *this, M5i20HostMotRegMap *pCard16, M5i20HostMotRegMap *pCard32,
		Plx9030LocalRegMap *pBridgeIc)
{
    int					i;

    this->pCard16 = pCard16;
    this->pCard32 = pCard32;
    this->pBridgeIc = pBridgeIc;

    // Download FPGA configuration.
    if(loadFpga && ((i = Device_LoadFpga(this)) != 0)){
	return(i);
    }

    // Initialize hardware.
    pCard16->mode = 0;
    pCard16->ledView = 0;
    pCard16->pwmRate = dacRate * 65536 / 1000 * 1024 / 33000;
    pCard16->wdTimeout = 16000;
    pCard16->control = M5I20_CONTROL_RESET_WDT | M5I20_CONTROL_CLEAR_IRQ
	    | M5I20_CONTROL_CLEAR_PWMS | M5I20_CONTROL_CLEAR_ENCODERS;

    // Initialize PWM channels.
    for(i = 0; i < M5I20_NUM_PWM_CHANNELS; i++){
	this->lastDacInterlaced[i] = 1;
	pCard16->pwmControl[i] = M5I20_PWM_CTL_SIGNED | M5I20_PWM_CTL_INTERLACED;
    }

    // Initialize primary encoders.
    for(i = 0; i < M5I20_NUM_PRIMARY_ENCODERS; i++){
	pCard16->encoderControl[i] = M5I20_ENC_CTL_COUNT_QUADRATURE
	    | M5I20_ENC_CTL_INDEX_ACTIVE_HI | M5I20_ENC_CTL_QUAD_FILTER_4MHZ
	    | M5I20_ENC_CTL_LATCH_ON_READ | M5I20_ENC_CTL_LOCAL_CLEAR;
    }

    // Initialize secondary encoders.
    for(i = M5I20_MAX_PRIMARY_ENCODERS;
	i < M5I20_MAX_PRIMARY_ENCODERS + M5I20_NUM_SECONDARY_ENCODERS; i++){
	pCard16->encoderControl[i] = M5I20_ENC_CTL_COUNT_QUADRATURE
	    | M5I20_ENC_CTL_INDEX_ACTIVE_HI | M5I20_ENC_CTL_QUAD_FILTER_4MHZ
	    | M5I20_ENC_CTL_LATCH_ON_READ | M5I20_ENC_CTL_LOCAL_CLEAR;
    }

    // Initialize digital I/O.
    for(i = 0; i < M5I20_NUM_DIGITAL_IO_PORTS; i++){
	pCard32->digitalIo[i].data = M5I20_DIGITAL_OUT;
	pCard32->digitalIo[i].direction = M5I20_DIGITAL_OUT;
    }

    pCard16->mode = M5I20_MODE_PWM_ENABLE | M5I20_MODE_COUNTER_ENABLE;
    
    return(0);
}


// FPGA configuration.
#include "m5i20_HM5-4E.h"

/*
 * Function: Device_LoadFpga
 * Purpose: Initialize the FPGA with the contents from compiled in array.
 * Used by: Local functions.
 * Returns: Error code.
 * Notes:
 *   If DONE never goes high, but INIT goes low - this means the FPGA got a valid 
 *   synchonization word (5599AA66) but encounterd a CRC error later in the stream.
 *
 *   If DONE never goes high and INIT stays high, most likely the FPGA never saw 
 *   the syncronization word.
 *
 */
static int
Device_LoadFpga(Device *this)
{
    M5i20HostMotRegMap			*pCard16 = this->pCard16;
    Plx9030LocalRegMap			*pBridgeIc = this->pBridgeIc;
    hal_u32_t				i;

    // Configure the GPIO.
    pBridgeIc->gpioc &= M5I20_PLX_GPIOC_AND_MASK;
    pBridgeIc->gpioc |= M5I20_PLX_GPIOC_OR_MASK;

    // Turn on LED.
    pBridgeIc->gpioc &= ~M5I20_PLX_GPIOC_LED_OFF;

    // Reset the FPGA
    pBridgeIc->gpioc &= ~M5I20_PLX_GPIOC_CFG_NPROGRAM;
    pBridgeIc->gpioc |= M5I20_PLX_GPIOC_CFG_NWRITE;

    if(pBridgeIc->gpioc & M5I20_PLX_GPIOC_CFG_DONE){
	// Note that if we see DONE at the start of programming, it's most likely due
	// to an attempt to access the FPGA at the wrong I/O location.
	rtapi_print_msg(RTAPI_MSG_ERR, "M5I20: ERROR: FPGA busy at start of programming\n");
	return(1);
    }

    // Enable programming.
    pBridgeIc->gpioc |= M5I20_PLX_GPIOC_CFG_NPROGRAM;
    pBridgeIc->gpioc &= ~M5I20_PLX_GPIOC_CFG_NWRITE;

    // Delay for at least 100 uS. to allow the FPGA to finish its reset sequencing.
    Device_Delay100us();

    // Program the fpga.
    for(i = 0; i < sizeof(fpgaConfig); i++){

	// Write byte to FPGA.
	pCard16->fpgaCfgData = fpgaConfig[i];
    }

    // Wait for completion of programming.
    Device_Delay100us();

    // Check for completion.
    if(!(pBridgeIc->gpioc & M5I20_PLX_GPIOC_CFG_DONE)){
	rtapi_print_msg(RTAPI_MSG_ERR, "M5I20: ERROR: FPGA programming not completed\n");
	return(2);
    }

    // Send configuration completion clocks (6 should be enough, but we send
    // lots for good measure).
    pBridgeIc->gpioc |= M5I20_PLX_GPIOC_CFG_NWRITE;

    for(i = 0; i < 24 ; i++){
	pCard16->fpgaCfgData = 0xFF;
    }

    // Turn off LED.
    pBridgeIc->gpioc |= M5I20_PLX_GPIOC_LED_OFF;

    return(0);
}


static void
Device_Delay100us(void)
{
    long int				maxDelay = rtapi_delay_max();
    long int				ns = 100000;
    long int				delay;

    while(ns){
	delay = (ns > maxDelay)? maxDelay: ns;
	ns -= delay;
	rtapi_delay(delay);
    }
}


static int
Device_ExportPinsParametersFunctions(Device *this, int componentId, int boardId)
{
    int					msgLevel, error;

    // This function exports a lot of stuff, which results in a lot of
    // logging if msg_level is at INFO or ALL. So we save the current value
    // of msg_level and restore it later.  If you actually need to log this
    // function's actions, change the second line below.
    msgLevel = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    // Export encoders.
    error = Device_ExportEncoderPinsParametersFunctions(this, componentId, boardId);

    // Export DACs.
    if(!error) error = Device_ExportDacPinsParametersFunctions(this, componentId, boardId);

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
    int					halError=0, channel;
    char				name[HAL_NAME_LEN + 2];

    // Export pins and parameters.
    for(channel = 0; channel < M5I20_NUM_ENCODER_CHANNELS; channel++){
	// Pins.
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.enc-%02d-count", boardId, channel);
	if((halError = hal_pin_s32_new(name, HAL_OUT, &(this->encoder[channel].pCount), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.enc-%02d-cnt-latch", boardId, channel);
	if((halError = hal_pin_s32_new(name, HAL_OUT, &(this->encoder[channel].pCountLatch), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.enc-%02d-position", boardId, channel);
	if((halError = hal_pin_float_new(name, HAL_OUT, &(this->encoder[channel].pPosition), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.enc-%02d-pos-latch", boardId, channel);
	if((halError = hal_pin_float_new(name, HAL_OUT, &(this->encoder[channel].pPositionLatch), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.enc-%02d-index", boardId, channel);
	if((halError = hal_pin_bit_new(name, HAL_OUT, &(this->encoder[channel].pIndex), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.enc-%02d-idx-latch", boardId, channel);
	if((halError = hal_pin_bit_new(name, HAL_OUT, &(this->encoder[channel].pIndexLatch), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.enc-%02d-latch-index", boardId, channel);
	if((halError = hal_pin_bit_new(name, HAL_IO, &(this->encoder[channel].pLatchIndex), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.enc-%02d-reset-count", boardId, channel);
	if((halError = hal_pin_bit_new(name, HAL_IO, &(this->encoder[channel].pResetCount), componentId)) != 0)
	    break;

	// Parameters.
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.enc-%02d-scale", boardId, channel);
	if((halError = hal_param_float_new(name, HAL_RW, &(this->encoder[channel].scale), componentId)) != 0)
	    break;

	// Init encoder.
	*(this->encoder[channel].pCount) = 0;
	*(this->encoder[channel].pCountLatch) = 0;
	*(this->encoder[channel].pPosition) = 0.0;
	*(this->encoder[channel].pPositionLatch) = 0.0;
	*(this->encoder[channel].pIndex) = 0;
	*(this->encoder[channel].pIndexLatch) = 0;
	*(this->encoder[channel].pLatchIndex) = 0;
	*(this->encoder[channel].pResetCount) = 0;
	this->encoder[channel].scale = 1.0;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.encoder-read", boardId);
	halError = hal_export_funct(name, Device_EncoderRead, this, 1, 0, componentId);
    }

    if(halError){
	rtapi_print_msg(RTAPI_MSG_ERR, "M5I20: ERROR: export encoder failed\n");
	return(-1);
    }

    return(0);
}


static int
Device_ExportDacPinsParametersFunctions(Device *this, int componentId, int boardId)
{
    int					halError=0, channel;
    char				name[HAL_NAME_LEN + 2];

    // Export pins and parameters.
    for(channel = 0; channel < M5I20_NUM_PWM_CHANNELS; channel++){
	// Pins.
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.dac-%02d-enable", boardId, channel);
	if((halError = hal_pin_bit_new(name, HAL_IN, &(this->dac[channel].pEnable), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.dac-%02d-value", boardId, channel);
	if((halError = hal_pin_float_new(name, HAL_IN, &(this->dac[channel].pValue), componentId)) != 0)
	    break;

	// Parameters.
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.dac-%02d-offset", boardId, channel);
	if((halError = hal_param_float_new(name, HAL_RW, &(this->dac[channel].offset), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.dac-%02d-gain", boardId, channel);
	if((halError = hal_param_float_new(name, HAL_RW, &(this->dac[channel].gain), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.dac-%02d-interlaced", boardId, channel);
	if((halError = hal_param_bit_new(name, HAL_RW, &(this->dac[channel].interlaced), componentId)) != 0)
	    break;

	// Init DAC.
	*(this->dac[channel].pEnable) = 0;
	*(this->dac[channel].pValue) = 0.0;
	this->dac[channel].offset = 0.0;
	this->dac[channel].gain = 1.0;
	this->dac[channel].interlaced = 1;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.dac-write", boardId);
	halError = hal_export_funct(name, Device_DacWrite, this, 1, 0, componentId);
    }

    if(halError){
	rtapi_print_msg(RTAPI_MSG_ERR, "M5I20: ERROR: export DAC failed\n");
	return(-1);
    }

    return(0);
}


static int
Device_ExportDigitalInPinsParametersFunctions(Device *this, int componentId, int boardId)
{
    int					halError=0, channel;
    char				name[HAL_NAME_LEN + 2];

    // Export pins and parameters.
    for(channel = 0; channel < M5I20_NUM_DIGITAL_INPUTS; channel++){
	// Pins.
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.in-%02d", boardId, channel);
	if((halError = hal_pin_bit_new(name, HAL_OUT, &(this->in[channel].pValue), componentId)) != 0)
	    break;

	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.in-%02d-not", boardId, channel);
	if((halError = hal_pin_bit_new(name, HAL_OUT, &(this->in[channel].pValueNot), componentId)) != 0)
	    break;

	// Init pin.
	*(this->in[channel].pValue) = 0;
	*(this->in[channel].pValueNot) = 1;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.digital-in-read", boardId);
	halError = hal_export_funct(name, Device_DigitalInRead, this, 0, 0, componentId);
    }

    if(halError){
	rtapi_print_msg(RTAPI_MSG_ERR, "M5I20: ERROR: export digital in failed\n");
	return(-1);
    }

    return(0);
}


static int
Device_ExportDigitalOutPinsParametersFunctions(Device *this, int componentId, int boardId)
{
    int					halError=0, channel;
    char				name[HAL_NAME_LEN + 2];

    // Export pins and parameters.
    for(channel = 0; channel < M5I20_NUM_DIGITAL_OUTPUTS; channel++){
	// Pins.
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.out-%02d", boardId, channel);
	if((halError = hal_pin_bit_new(name, HAL_IN, &(this->out[channel].pValue), componentId)) != 0)
	    break;

	// Parameters.
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.out-%02d-invert", boardId, channel);
	if((halError = hal_param_bit_new(name, HAL_RW, &(this->out[channel].invert), componentId)) != 0)
	    break;

	// Init pin.
	*(this->out[channel].pValue) = 0;
	this->out[channel].invert = 1;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.digital-out-write", boardId);
	halError = hal_export_funct(name, Device_DigitalOutWrite, this, 0, 0, componentId);
    }

    if(halError){
	rtapi_print_msg(RTAPI_MSG_ERR, "M5I20: ERROR: export digital out failed\n");
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
    rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.estop-in", boardId);
    halError = hal_pin_bit_new(name, HAL_OUT, &(this->misc.pEstopIn), componentId);

    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.estop-in-not", boardId);
	halError = hal_pin_bit_new(name, HAL_OUT, &(this->misc.pEstopInNot), componentId);
    }

    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.watchdog-reset", boardId);
	halError = hal_pin_bit_new(name, HAL_IO, &(this->misc.pWatchdogReset), componentId);
    }

    // Export Parameters.
    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.watchdog-control", boardId);
	halError = hal_param_u32_new(name, HAL_RW, &(this->misc.watchdogControl), componentId);
    }

    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.watchdog-timeout", boardId);
	halError = hal_param_u32_new(name, HAL_RW, &(this->misc.watchdogTimeout), componentId);
    }

    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.led-view", boardId);
	halError = hal_param_u32_new(name, HAL_RW, &(this->misc.ledView), componentId);
    }

    // Init pins.
    if(!halError){
	*(this->misc.pEstopIn) = 0;
	*(this->misc.pEstopInNot) = 1;
	*(this->misc.pWatchdogReset) = 0;
	this->misc.watchdogControl = 0;
	this->misc.watchdogTimeout = 16000;
	this->misc.ledView = 0;
    }

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, HAL_NAME_LEN, "m5i20.%d.misc-update", boardId);
	halError = hal_export_funct(name, Device_MiscUpdate, this, 0, 0, componentId);
    }

    if(halError){
	rtapi_print_msg(RTAPI_MSG_ERR, "M5I20: ERROR: export miscellaneous failed\n");
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
    M5i20HostMotRegMap			*pCard16 = this->pCard16;
    M5i20HostMotRegMap			*pCard32 = this->pCard32;
    EncoderPinsParams			*pEncoder;
    int					i, j;
    __u16				status;

    pEncoder = &this->encoder[0];

    // For each encoder.
    for(i = 0, j = 0; j < M5I20_NUM_ENCODER_CHANNELS; j++, pEncoder++){

	// Check reset pin.
	if(*(pEncoder->pResetCount)){
	    // Clear pin.
	    *(pEncoder->pResetCount) = 0;

	    // Reset encoder.
	    pCard16->encoderControl[i] |= M5I20_ENC_CTL_LOCAL_CLEAR;
	}

	// Check index latch pin.
	if(*(pEncoder->pLatchIndex)){
	    // Clear pins.
	    *(pEncoder->pLatchIndex) = 0;
	    *(pEncoder->pIndexLatch) = 0;

	    // Enable index latch.
	    pCard16->encoderControl[i] &= ~M5I20_ENC_CTL_LATCH_ON_READ;
	    pCard16->encoderControl[i] |= M5I20_ENC_CTL_LATCH_ON_INDEX | M5I20_ENC_CTL_LATCH_ONCE;
	}

	// Read encoder status.
	status = pCard16->encoderControl[i];

	// Read encoder count.
	*(pEncoder->pCount) = pCard32->encoderCount[i];

	// Scale count to make floating point position.
	if(pEncoder->scale) *(pEncoder->pPosition) = *(pEncoder->pCount) / pEncoder->scale;

	// Update index and index latched pins.
	*(pEncoder->pIndex) = (status & M5I20_ENC_CTL_INDEX)? 1: 0;

	if((*(pEncoder->pIndexLatch) == 0) && !(status & M5I20_ENC_CTL_LATCH_ON_INDEX)){
	    *(pEncoder->pIndexLatch) = 1;
	    *(pEncoder->pCountLatch) = *(pEncoder->pCount);
	    *(pEncoder->pPositionLatch) = *(pEncoder->pPosition);

	    pCard16->encoderControl[i] |= M5I20_ENC_CTL_LATCH_ON_READ;
	}

	// Loop house keeping.
	if(++i == M5I20_NUM_PRIMARY_ENCODERS) i = M5I20_MAX_PRIMARY_ENCODERS;
    }
}


static void
Device_DacWrite(void *arg, long period)
{
    Device				*this = (Device *)arg;
    M5i20HostMotRegMap			*pCard16 = this->pCard16;
    DacPinsParams			*pDac;
    int					i;
    hal_float_t				volts;
    __s16				dacCount;

    if(this->misc.watchdogControl & WDT_CONTROL_AUTO_RESET){
	// Reset the watchdog timer.
	Device_WdtReset(this);
    }

    pDac = &this->dac[0];

    // For each DAC.
    for(i = 0; i < M5I20_NUM_PWM_CHANNELS; i++, pDac++){

	// Check for mode change.
	if(pDac->interlaced != this->lastDacInterlaced[i]){
	    if((this->lastDacInterlaced[i] = pDac->interlaced) == 0){
		pCard16->pwmControl[i] &= ~M5I20_PWM_CTL_INTERLACED;
	    }else{
		pCard16->pwmControl[i] |= M5I20_PWM_CTL_INTERLACED;
	    }
	}

	// Calculate hardware register value.
	volts = (*(pDac->pValue) - pDac->offset) * pDac->gain;

	// Truncate volts to DAC limits.
	if(volts > M5I20_DAC_VOLTS_MAX){
	    volts = M5I20_DAC_VOLTS_MAX;
	}else if(volts < M5I20_DAC_VOLTS_MIN){
	    volts = M5I20_DAC_VOLTS_MIN;
	}

	// Transform volts to counts.
	dacCount = (__s16)(volts * M5I20_DAC_SCALE_MULTIPLY /
				M5I20_DAC_SCALE_DIVIDE);

	// Write DAC.
	pCard16->pwmValue[i] = dacCount;

	// Check enable pin.
	if(*(pDac->pEnable)){
	    pCard16->pwmControl[i] |= M5I20_PWM_CTL_ENABLE;
	}else{
	    pCard16->pwmControl[i] &= ~M5I20_PWM_CTL_ENABLE;
	}
    }
}


static void
Device_DigitalInRead(void *arg, long period)
{
    Device				*this = (Device *)arg;
    M5i20HostMotRegMap			*pCard32 = this->pCard32;
    DigitalInPinsParams			*pDigitalIn;
    int					i, j;
    hal_u32_t				pins;

    pDigitalIn = &this->in[0];

    // For each port.
    for(i = 0; i < M5I20_NUM_DIGITAL_IO_PORTS; i++){

	// Read digital I/O register.
	pins = pCard32->digitalIo[i].data >> M5I20_DIGITAL_IN_SHFT;

	// For each pin.
	for(j = 0; j < 16; j++, pDigitalIn++){

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
    M5i20HostMotRegMap			*pCard32 = this->pCard32;
    DigitalOutPinsParams		*pDigitalOut;
    int					i, j;
    hal_u32_t				pins, mask;

    pDigitalOut = &this->out[0];

    // For each port.
    for(i = 0; i < M5I20_NUM_DIGITAL_IO_PORTS; i++){

	pins = 0;
	mask = 1;

	// For each pin.
	for(j = 0; j < 8; j++, pDigitalOut++){

	    // Build hardware register value.
	    if(( *(pDigitalOut->pValue) && !(pDigitalOut->invert) ) ||
	       (!*(pDigitalOut->pValue) &&  (pDigitalOut->invert) )) {
		pins |= mask;
	    }

	    mask <<=1;
	}

	// Write digital I/O register.
	pCard32->digitalIo[i].data = pins << M5I20_DIGITAL_OUT_SHFT;
    }
}


static void
Device_MiscUpdate(void *arg, long period)
{
    Device				*this = (Device *)arg;
    M5i20HostMotRegMap			*pCard16 = this->pCard16;

    // Write watchdog timer configuration to hardware.
    if(this->misc.watchdogControl & WDT_CONTROL_ENABLE){
	pCard16->wdTimeout = this->misc.watchdogTimeout & 0x0000FFFF;
	pCard16->mode |= M5I20_MODE_STOP_ON_WDT;
    }else{
	pCard16->mode &= ~M5I20_MODE_STOP_ON_WDT;
    }

    // Check watchdog timer reset pin.
    if(*(this->misc.pWatchdogReset)){
	// Clear pin.
	*(this->misc.pWatchdogReset) = 0;

	// Reset the watchdog timer.
	Device_WdtReset(this);
    }

    // Update E-STOP pin.
    *(this->misc.pEstopIn) = ((pCard16->mode & M5I20_MODE_STOP_ON_WDT)
				&& (pCard16->wdTimer == 0))? 1: 0;
    *(this->misc.pEstopInNot) = !*(this->misc.pEstopIn);

    // Write LED view channel to hardware.
    pCard16->ledView = this->misc.ledView & 0x0000FFFF;
}


/*
 * PRIVATE HELPER FUNCTIONS
 */

static void
Device_WdtReset(Device *this)
{
    M5i20HostMotRegMap			*pCard16 = this->pCard16;

    // Check if watchdog timer is enabled.
    if(!(pCard16->mode & M5I20_MODE_STOP_ON_WDT))
	return;

    // Check for timeout.
    if(pCard16->wdTimer == 0){
	rtapi_print_msg(RTAPI_MSG_ERR, "M5I20: ERROR: watchdog timeout\n");

	// Reset the watchdog timer.
	pCard16->control = M5I20_CONTROL_RESET_WDT;

	// For error recovery.
	pCard16->mode |= M5I20_MODE_PWM_ENABLE;
    }else{
	// Reset the watchdog timer.
	pCard16->control = M5I20_CONTROL_RESET_WDT;
    }
}
