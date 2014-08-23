#include <linux/pci.h>
#include "rtapi.h"			// RTAPI realtime OS API.
#include "rtapi_app.h"			// RTAPI realtime module decls.
#include "hal.h"			// HAL public API decls.
#include "opto_ac5.h"			// Hardware dependent defines.

#ifndef MODULE
#define MODULE
#endif

#ifdef MODULE
// Module information.
MODULE_AUTHOR("Chris Morley");
MODULE_DESCRIPTION("Driver for opto22 pci AC5 for EMC HAL");
MODULE_LICENSE("GPL");
#endif // MODULE


/*************************************************************************
                          typedefs and defines
*************************************************************************/


// Vendor and device ID. "magic numbers"
#define opto22_VENDOR_ID			0x148a		// opto22 corp
#define opto22_pci_AC5_DEVICE_ID		0xac05		// AC5 relay board interface
#define MAX_BOARDS	4					// could be higher uses more memory

// OFFSETS FROM BASE ADDRESS FOR COMMANDS TO OPTO CARD
// for port 0
#define CONFIG_WRITE_OFFSET_0  0x00000004
#define CONFIG_READ_OFFSET_0   0x00000100
#define DATA_WRITE_OFFSET_0    0X00000008
#define DATA_READ_OFFSET_0     0X00000300

// for port 1
#define CONFIG_WRITE_OFFSET_1  0x00000014
#define CONFIG_READ_OFFSET_1   0x00000500
#define DATA_WRITE_OFFSET_1    0X00000018
#define DATA_READ_OFFSET_1     0X00000700

// These methods are used for initialization.
static int Device_Init(board_data_t *pboard);
static int Device_ExportPinsParametersFunctions(board_data_t *pboard, int comp_id, int boardId);
static int Device_ExportDigitalInPinsParametersFunctions(board_data_t *pboard, int comp_id, int boardId);
static int Device_ExportDigitalOutPinsParametersFunctions(board_data_t *pboard, int comp_id, int boardId);
// These methods are exported to the HAL.
static void Device_DigitalInRead(void *this, long period);
static void Device_DigitalOutWrite(void *this, long period);
/*************************************************************************
                                   Globals
*************************************************************************/


typedef struct {
	int comp_id;	/* HAL component ID */
	board_data_t *boards[MAX_BOARDS];
	int num_of_boards;
} Driver_t;

static Driver_t				driver;

// in case no portconfig is given when loading driver
// sets 12 inputs then 12 outputs per port and sets leds (bit 31,32) for output
// could define more variables so the ports of extra cards could be configured differently but
// I doubt if anyone will use more then one card

static unsigned long		portconfig0 = 0Xc0fff000;
RTAPI_MP_LONG(portconfig0, "port 0 i/o configuration number");
static unsigned long		portconfig1 = 0Xc0fff000;
RTAPI_MP_LONG(portconfig1, "port 1 i/o configuration number");

/******************************************************************************
                              Init and exit code
 ******************************************************************************/

int rtapi_app_main(void)
{
    int n;
    board_data_t *pboard;
    struct pci_dev *pDev;

    // Connect to the HAL.
    driver.comp_id = hal_init("opto_ac5");
    if (driver.comp_id < 0) {
	rtapi_print_msg(RTAPI_MSG_ERR, " ERROR OPTO_AC5--- hal_init() failed\n");
	return(-1);
    }	
    for ( n = 0 ; n < MAX_BOARDS ; n++ ) {
	driver.boards[n] = NULL;
    }
    pDev = NULL;
    for ( n = 0 ; n < MAX_BOARDS ; n++ ) 
    {
	// Find a M5I20 card.
	pDev = pci_get_device(opto22_VENDOR_ID, opto22_pci_AC5_DEVICE_ID, pDev);
	if ( pDev == NULL ) { /* no more boards */break;}
	

	/* Allocate HAL memory for the board */
	pboard = (board_data_t *)(hal_malloc(sizeof(board_data_t)));
	if ( pboard == NULL ) {
	    rtapi_print_msg(RTAPI_MSG_ERR, "ERROR OPTO_AC5--- hal_malloc() failed\n");
	    rtapi_app_exit();
	    return -1;
	}
	// save pointer
	driver.boards[n] = pboard;

	/* gather info about the board and save it */
	pboard->pci_dev = pDev;
	pboard->slot = PCI_SLOT(pDev->devfn);
	pboard->num = n;
	rtapi_print("INFO OPTO_AC5--- Board %d detected in PCI Slot: %2x\n", pboard->num, pboard->slot);
	/* region 0 is the 32 bit memory mapped I/O region */
	pboard->len = pci_resource_len(pDev, 0);
	pboard->base = ioremap_nocache(pci_resource_start(pDev, 0), pboard->len);
	if ( pboard->base == NULL ) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"ERROR OPTO_AC5---  could not find board %d PCI base address\n", pboard->num );
	    rtapi_app_exit();
	    return -1;
	} else {
	 	    rtapi_print(
		"INFO OPTO_AC5--- board %d mapped to %08lx, Len = %ld\n",
		pboard->num, (long)pboard->base,(long)pboard->len);
		}
	// Initialize device.
	if(Device_Init(pboard)){
	    hal_exit(driver.comp_id);
	    return(-1);
	}

	// Export pins, parameters, and functions.
	if(Device_ExportPinsParametersFunctions(pboard, driver.comp_id, n)){
	    hal_exit(driver.comp_id);
	    return(-1);
	}
	
    }
    
    if(n == 0){
	/* No cards detected */
	rtapi_print ("ERROR OPTO_AC5---  No opto PCI-AC5 card(s) detected\n");
	rtapi_app_exit();
	return(-1);
    	}
	
    hal_ready(driver.comp_id);
    return(0);
}

// here we turn the leds and outputs off and unmap the pci driver
void rtapi_app_exit(void)
{ 
  	int n;
	board_data_t 	*pDevice;

	pDevice = driver.boards[0];


    hal_exit(driver.comp_id);
    for ( n = 0; n < MAX_BOARDS; n++ ) 
	{
		if ( driver.boards[n] != NULL)
		 {
			rtapi_print ("INFO OPTO_AC5--- board %i driver removed.\n",n);
			writel(0XC0FFFFFF, driver.boards[n]->base + (DATA_WRITE_OFFSET_0));
			writel(0XC0FFFFFF, driver.boards[n]->base + (DATA_WRITE_OFFSET_1));
		   	// Unmap board memory
			if ( driver.boards[n]->base != NULL ) 
			{
				iounmap(driver.boards[n]->base);
	   		}
		}
   	}
}

/******************************************************************************
 * DEVICE OBJECT FUNCTION DEFINITIONS
 ******************************************************************************/

/*
 * LOCAL FUNCTIONS
 */
static int Device_Init(board_data_t *pboard)
{

 // Initialize hardware.
// portconfig[0 or 1] are either the default number from global or mapped from the command line 
// make sure last two bits are set for output so leds will work 

	if ((portconfig0 & 0x80000000) ==0) { 	portconfig0 |=0x80000000;	}
	if ((portconfig0 & 0x40000000) ==0) { 	portconfig0 |=0x40000000;	}
	if ((portconfig1 & 0x80000000) ==0) { 	portconfig1 |=0x80000000;	}
	if ((portconfig1 & 0x40000000) ==0) { 	portconfig1 |=0x40000000;	}
	writel(portconfig0, pboard->base + (CONFIG_WRITE_OFFSET_0));
	writel(portconfig1, pboard->base + (CONFIG_WRITE_OFFSET_1));

 // Initialize digital I/O structure mask.
    
	pboard->port[0].mask = portconfig0;
	pboard->port[1].mask = portconfig1;
    
    return(0);
}

static int Device_ExportPinsParametersFunctions(board_data_t *this, int componentId, int boardId)
{
    int					msgLevel, error=0;

// This function exports a lot of stuff, which results in a lot of
// logging if msg_level is at INFO or ALL. So we save the current value
// of msg_level and restore it later.  If you actually need to log this
// function's actions, change the second line below.

 	msgLevel = rtapi_get_msg_level();
   	rtapi_set_msg_level(RTAPI_MSG_WARN);

// Export digital I/O.

    if(!error) error = Device_ExportDigitalInPinsParametersFunctions(this, componentId, boardId);
    if(!error) error = Device_ExportDigitalOutPinsParametersFunctions(this, componentId, boardId);

// Restore saved message level.

    rtapi_set_msg_level(msgLevel);

    return(error);
}

// we check each ports mask (port[portnum].mask) against a mask bit to see which of the 24 points of i/o are inputs (a false bit is an input)
// then export HAL pins mapped to the proper io structure
// HAL pin numbers represent position in an opto22 relay rack (eg. pin 00 is position 0)
// this way you can configure the i/o in any way for all the 24 points of each port

static int Device_ExportDigitalInPinsParametersFunctions(board_data_t *this, int comp_id, int boardId)
{
    int					halError=0, channel,mask,portnum=0;
    char				name[HAL_NAME_LEN + 1];

    // Export pins and parameters.
	while (portnum<2)
		{
		mask=1;
   		 for(channel = 0; channel < 24; channel++)
		   {
			if ((this->port[portnum].mask & mask)==0)//physical input?
			{
			// Pins.
			if((halError = hal_pin_bit_newf(HAL_OUT, &(this->port[portnum].io[channel].pValue),
			  comp_id, "opto-ac5.%d.port%d.in-%02d", boardId, portnum, channel)) != 0)
			    break;

			if((halError = hal_pin_bit_newf(HAL_OUT, &(this->port[portnum].io[channel].pValueNot),
			  comp_id, "opto-ac5.%d.port%d.in-%02d-not", boardId, portnum, channel)) != 0)
			    break;

			// Init pin.
			*(this->port[portnum].io[channel].pValue) = 0;
			*(this->port[portnum].io[channel].pValueNot) = 1;
			}
			mask <<=1;
		   }

		   portnum ++;	
	 	}

    // Export functions.
    if(!halError){
	rtapi_snprintf(name, sizeof(name), "opto-ac5.%d.digital-read", boardId);
	halError = hal_export_funct(name, Device_DigitalInRead, this, 0, 0, comp_id);
    }

    if(halError){
	rtapi_print_msg(RTAPI_MSG_ERR, "ERROR OPTO22_AC5---exporting digital inputs to HAL failed\n");
	return(-1);
    }

    return(0);
}


// we check each ports mask (port[portnum].mask) against a mask bit to see which of the 24 points of i/o are outputs (a true bit is an output)
// then export HAL pins mapped to the proper io structure
// this way you can configure the i/o in any way for all the 24 points of each port
// HAL pin numbers represent position in an opto22 relay rack (eg. pin 00 is position 0)
// the LEDS are bits 31 and 32 of the portmask but are mapped to 24 and 25 of the i0 structure

static int Device_ExportDigitalOutPinsParametersFunctions(board_data_t *this, int comp_id, int boardId)
{
    int					halError=0, channel,mask,portnum=0;
    char				name[HAL_NAME_LEN + 1];

    // Export pins and parameters.
    
	while (portnum<2)
		{
		mask=1;
   		 for(channel = 0; channel < 24; channel++)
		   {
			if ((this->port[portnum].mask & mask)!=0)//phyical output?
			{
			// Pins.
			if((halError = hal_pin_bit_newf(HAL_IN, &(this->port[portnum].io[channel].pValue),
			  comp_id, "opto-ac5.%d.port%d.out-%02d", boardId, portnum, channel)) != 0)
			    break;

			// Parameters.
			if((halError = hal_param_bit_newf(HAL_RW, &(this->port[portnum].io[channel].invert),
			  comp_id, "opto-ac5.%d.port%d.out-%02d-invert", boardId, portnum, channel)) != 0)
			    break;

			// Init pin.
			*(this->port[portnum].io[channel].pValue) = 0;
			this->port[portnum].io[channel].invert = 0;
		   	}
			mask <<=1;
		   }
		   portnum++;
		}
		// led Pins.
		portnum=0;
		for(channel = 0; channel < 2; channel++)
		{
			if((halError = hal_pin_bit_newf(HAL_IN, &(this->port[portnum].io[24].pValue),
			  comp_id, "opto-ac5.%d.led%d", boardId, channel+portnum)) != 0)
			    break;

			if((halError = hal_pin_bit_newf(HAL_IN, &(this->port[portnum].io[25].pValue),
			  comp_id, "opto-ac5.%d.led%d", boardId, channel+portnum+1)) != 0)
			    break;
			portnum++;
		}
    // Export functions.
    if(!halError){
	rtapi_snprintf(name, sizeof(name), "opto-ac5.%d.digital-write", boardId);
	halError = hal_export_funct(name, Device_DigitalOutWrite, this, 0, 0, comp_id);
    }

    if(halError){
	rtapi_print_msg(RTAPI_MSG_ERR, "ERROR OPTO_AC5---exporting digital outputs to HAL failed\n");
	return(-1);
    }

    return(0);
}

// here we read inputs from board
// we read the current data (variable 'pins') of the first port. Then for each of the 24 points
// we compare to the mask of the first port to see which of the 24 io points are inputs (the bits that are false)
// if it is an input then check 'pins' against the mask to see if input bit is true
// update the HAL pin and not-pin accoringly. shift the mask then do the next point (of 24 io points)
// after all pins done-increase 'portnum' to 1 set offset to the offset for port1
// then do it all again on the second port

static void
Device_DigitalInRead(void *arg, long period)
{
    board_data_t			*pboard = (board_data_t *)arg;
    DigitalPinsParams			*pDigital;
    int					i, portnum=0;
    unsigned long			pins, offset=DATA_READ_OFFSET_0, mask;

    // For each port.
	while (portnum<2)
		{
			mask=1;
			pDigital = &pboard->port[portnum].io[0];

			// Read digital I/O register.
			pins=	readl (pboard->base + (offset));
   			for(i = 0; i < 24; i++,pDigital++)
			{
				if ((pboard->port[portnum].mask & mask) ==0) // is it an input bit ?
				{
				    if ((pins & mask) !=0){	*(pDigital->pValue) =0;
					}else{	*(pDigital->pValue) = 1;	}
					// Update not pin.
				    *(pDigital->pValueNot) = !*(pDigital->pValue);
				}
	  		 	 mask <<=1;// shift mask
			}
			portnum++;
			offset=DATA_READ_OFFSET_1;// change to offset for port1
 		}
}

// here we output data and update LEDS
// we look at the mask of the first port to see which of the 24 io points are outputs (the bits that are true)
// then check the OUT HAL pins to see if output should be on and if it should - OR the bit (using 'mask') to the variable 'pins' 
// then set 'mask' to the last bit (32) and check the HAL led pin to see if true- OR the bit to 'pins' if it is
// set 'mask to second to second to last bit (31) do the same
// have to remember that a 1 sent to the hardware turns an output OFF
// finally write the variable 'pins' to the boards first port
// reset offset to the next port offset, increase portnum for port 1
// then do it all again on the second port

static void
Device_DigitalOutWrite(void *arg, long period)
{
    board_data_t			*pboard = (board_data_t *)arg;
    DigitalPinsParams			*pDigital;
    int					i, j, portnum=0;
    unsigned long			pins, offset=DATA_WRITE_OFFSET_0,mask;

    // For each port.
	while (portnum<2)
		{
			pDigital = &pboard->port[portnum].io[0];
			mask=1;
			pins=0;

   			// For each pin.
			for(j = 0; j < 24; j++, pDigital++)
			{
				if ((pboard->port[portnum].mask & mask) !=0) //is it an output?
				{
			 	   // add mask to pins if HAL pin + invert =true.
				    if( (!*(pDigital->pValue) && !(pDigital->invert) ) ||
				       ( *(pDigital->pValue) &&  (pDigital->invert) ))
					 {	pins |= mask;	    }
				}
	   			 mask <<=1; // shift mask
				
			}

			// CHECK LED PINS
			pDigital = &pboard->port[portnum].io[23];//one before what we want to check
			for (i = 0;i < 2;i++)
				{
			 		mask=1<<(31-i);
					pDigital++;
				
					if ( *(pDigital->pValue) ==0 ) {	pins |= mask;	    }	
				}
			// Write digital I/O register.
			writel(pins,pboard->base + (offset));
			portnum ++;
			offset=DATA_WRITE_OFFSET_1; // set to port1 offset
   		 }
}

