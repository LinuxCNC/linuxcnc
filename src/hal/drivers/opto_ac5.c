#include <rtapi.h> // RTAPI realtime OS API.
#include <rtapi_app.h> // RTAPI realtime module decls.
#include <rtapi_errno.h>
#include <hal.h> // HAL public API decls.
#include "opto_ac5.h" // Hardware dependent defines.

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
#define opto22_VENDOR_ID 0x148a // opto22 corp
#define opto22_pci_AC5_DEVICE_ID 0xac05 // AC5 relay board interface
#define MAX_BOARDS 4 // could be higher uses more memory

// OFFSETS FROM BASE ADDRESS FOR COMMANDS TO OPTO CARD
// for port 0
#define CONFIG_WRITE_OFFSET_0 0x00000004
#define CONFIG_READ_OFFSET_0 0x00000100
#define DATA_WRITE_OFFSET_0 0X00000008
#define DATA_READ_OFFSET_0 0X00000300

// for port 1
#define CONFIG_WRITE_OFFSET_1 0x00000014
#define CONFIG_READ_OFFSET_1 0x00000500
#define DATA_WRITE_OFFSET_1 0X00000018
#define DATA_READ_OFFSET_1 0X00000700

/* 32 bit register access.  readl()/writel() in a kernel (RTAI) build, a
 * volatile access to the BAR mapped by rtapi_pci_ioremap_bar() in uspace. */
#ifdef __KERNEL__
#define ac5_read(b, off) readl((b)->base + (off))
#define ac5_write(b, off, v) writel((v), (b)->base + (off))
#else
static inline rtapi_u32 ac5_read(board_data_t *b, unsigned long off)
{
    return *(volatile rtapi_u32 *)((volatile char *)b->base + off);
}
static inline void ac5_write(board_data_t *b, unsigned long off, rtapi_u32 v)
{
    *(volatile rtapi_u32 *)((volatile char *)b->base + off) = v;
}
#endif

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
    int comp_id; /* HAL component ID */
    board_data_t *boards[MAX_BOARDS];
    int num_of_boards;
} Driver_t;

static Driver_t driver;

// in case no portconfig is given when loading driver
// sets 12 inputs then 12 outputs per port and sets leds (bit 31,32) for output
// could define more variables so the ports of extra cards could be configured differently but
// I doubt if anyone will use more then one card

static unsigned long portconfig0 = 0Xc0fff000;
RTAPI_MP_LONG(portconfig0, "port 0 i/o configuration number");
static unsigned long portconfig1 = 0Xc0fff000;
RTAPI_MP_LONG(portconfig1, "port 1 i/o configuration number");

/******************************************************************************
                              PCI driver
 ******************************************************************************/

static int failed_errno = 0; /* errno of the last failed probe */

/* The uspace rtapi_pci matches subvendor/subdevice exactly (no PCI_ANY_ID
 * wildcard), so list the subsystem IDs the card reports (lspci -nn). */
static struct rtapi_pci_device_id opto_ac5_pci_tbl[] = {
    {
        .vendor = opto22_VENDOR_ID,
        .device = opto22_pci_AC5_DEVICE_ID,
        .subvendor = opto22_VENDOR_ID,
        .subdevice = opto22_pci_AC5_DEVICE_ID,
    },
    {0,},
};

static int opto_ac5_pci_probe(struct rtapi_pci_dev *dev, const struct rtapi_pci_device_id *id)
{
    board_data_t *pboard;
    int n = driver.num_of_boards;

    (void)id;
    if (n >= MAX_BOARDS) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "ERROR OPTO_AC5--- skipping board at %s, this driver handles %d boards\n",
                rtapi_pci_name(dev), MAX_BOARDS);
        return -EINVAL;
    }
    if (rtapi_pci_enable_device(dev)) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "ERROR OPTO_AC5--- failed to enable PCI device %s\n", rtapi_pci_name(dev));
        return failed_errno = -ENODEV;
    }

    /* Allocate HAL memory for the board */
    pboard = (board_data_t *)(hal_malloc(sizeof(board_data_t)));
    if (pboard == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ERROR OPTO_AC5--- hal_malloc() failed\n");
        return failed_errno = -ENOMEM;
    }
    pboard->pci_dev = dev;
    pboard->num = n;

    /* region 0 is the 32 bit memory mapped I/O region */
    pboard->len = rtapi_pci_resource_len(dev, 0);
    pboard->base = rtapi_pci_ioremap_bar(dev, 0);
    if (pboard->base == NULL) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                "ERROR OPTO_AC5--- could not map board %d (%s) PCI region 0\n", n, rtapi_pci_name(dev));
        return failed_errno = -ENODEV;
    }
    rtapi_print("INFO OPTO_AC5--- board %d at %s mapped, Len = %ld\n",
            n, rtapi_pci_name(dev), (long)pboard->len);

    // Initialize device.
    if (Device_Init(pboard))
        return failed_errno = -EINVAL;

    // Export pins, parameters, and functions.
    if (Device_ExportPinsParametersFunctions(pboard, driver.comp_id, n))
        return failed_errno = -EINVAL;

    driver.boards[n] = pboard;
    driver.num_of_boards++;
    rtapi_pci_set_drvdata(dev, pboard);
    return 0;
}

// here we turn the leds and outputs off and unmap the board
static void opto_ac5_pci_remove(struct rtapi_pci_dev *dev)
{
    int n;

    for (n = 0; n < MAX_BOARDS; n++) {
        board_data_t *pboard = driver.boards[n];
        if (pboard == NULL || pboard->pci_dev != dev)
            continue;
        rtapi_print("INFO OPTO_AC5--- board %i driver removed.\n", n);
        ac5_write(pboard, DATA_WRITE_OFFSET_0, 0XC0FFFFFF);
        ac5_write(pboard, DATA_WRITE_OFFSET_1, 0XC0FFFFFF);
        rtapi_iounmap(pboard->base);
        pboard->base = NULL;
        driver.boards[n] = NULL;
        rtapi_pci_disable_device(dev);
        rtapi_pci_set_drvdata(dev, NULL);
    }
}

static struct rtapi_pci_driver opto_ac5_pci_driver = {
    .name = "opto_ac5",
    .id_table = opto_ac5_pci_tbl,
    .probe = opto_ac5_pci_probe,
    .remove = opto_ac5_pci_remove,
};

/******************************************************************************
                              Init and exit code
 ******************************************************************************/

int rtapi_app_main(void)
{
    int n, r;

    // Connect to the HAL.
    driver.comp_id = hal_init("opto_ac5");
    if (driver.comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, " ERROR OPTO_AC5--- hal_init() failed\n");
        return(-1);
    }
    for (n = 0; n < MAX_BOARDS; n++) {
        driver.boards[n] = NULL;
    }
    driver.num_of_boards = 0;

    r = rtapi_pci_register_driver(&opto_ac5_pci_driver);
    if (r != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ERROR OPTO_AC5--- error registering PCI driver\n");
        hal_exit(driver.comp_id);
        return r;
    }
    if (failed_errno || driver.num_of_boards == 0) {
        if (driver.num_of_boards == 0)
            rtapi_print_msg(RTAPI_MSG_ERR, "ERROR OPTO_AC5--- No opto PCI-AC5 card(s) detected\n");
        rtapi_pci_unregister_driver(&opto_ac5_pci_driver);
        hal_exit(driver.comp_id);
        return failed_errno ? failed_errno : -ENODEV;
    }

    hal_ready(driver.comp_id);
    return(0);
}

void rtapi_app_exit(void)
{
    rtapi_pci_unregister_driver(&opto_ac5_pci_driver);
    hal_exit(driver.comp_id);
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

    if ((portconfig0 & 0x80000000) == 0) {
        portconfig0 |= 0x80000000;
    }
    if ((portconfig0 & 0x40000000) == 0) {
        portconfig0 |= 0x40000000;
    }
    if ((portconfig1 & 0x80000000) == 0) {
        portconfig1 |= 0x80000000;
    }
    if ((portconfig1 & 0x40000000) == 0) {
        portconfig1 |= 0x40000000;
    }
    ac5_write(pboard, CONFIG_WRITE_OFFSET_0, portconfig0);
    ac5_write(pboard, CONFIG_WRITE_OFFSET_1, portconfig1);

    // Initialize digital I/O structure mask.

    pboard->port[0].mask = portconfig0;
    pboard->port[1].mask = portconfig1;

    return(0);
}

static int Device_ExportPinsParametersFunctions(board_data_t *this, int componentId, int boardId)
{
    int msgLevel, error = 0;

    // This function exports a lot of stuff, which results in a lot of
    // logging if msg_level is at INFO or ALL. So we save the current value
    // of msg_level and restore it later.  If you actually need to log this
    // function's actions, change the second line below.

    msgLevel = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    // Export digital I/O.

    if (!error) error = Device_ExportDigitalInPinsParametersFunctions(this, componentId, boardId);
    if (!error) error = Device_ExportDigitalOutPinsParametersFunctions(this, componentId, boardId);

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
    int halError = 0, channel, mask, portnum = 0;

    // Export pins and parameters.
    while (portnum < 2) {
        mask = 1;
        for (channel = 0; channel < 24; channel++) {
            if ((this->port[portnum].mask & mask) == 0) { // physical input?
                // Pins.
                if ((halError = hal_pin_new_bool(comp_id, HAL_OUT, &(this->port[portnum].io[channel].pValue),
                        0, "opto-ac5.%d.port%d.in-%02d", boardId, portnum, channel)) != 0)
                    break;

                if ((halError = hal_pin_new_bool(comp_id, HAL_OUT, &(this->port[portnum].io[channel].pValueNot),
                        1, "opto-ac5.%d.port%d.in-%02d-not", boardId, portnum, channel)) != 0)
                    break;
            }
            mask <<= 1;
        }

        portnum ++;
    }

    // Export functions.
    if (!halError) {
        halError = hal_export_functf(Device_DigitalInRead, this, 0, comp_id, "opto-ac5.%d.digital-read", boardId);
    }

    if (halError) {
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
    int halError = 0, channel, mask, portnum = 0;

    // Export pins and parameters.

    while (portnum < 2) {
        mask = 1;
        for (channel = 0; channel < 24; channel++) {
            if ((this->port[portnum].mask & mask) != 0) { // physical output?
                // Pins.
                if ((halError = hal_pin_new_bool(comp_id, HAL_IN, &(this->port[portnum].io[channel].pValue),
                        0, "opto-ac5.%d.port%d.out-%02d", boardId, portnum, channel)) != 0)
                    break;

                // Parameters.
                if ((halError = hal_param_new_bool(comp_id, HAL_RW, &(this->port[portnum].io[channel].invert),
                        0, "opto-ac5.%d.port%d.out-%02d-invert", boardId, portnum, channel)) != 0)
                    break;
            }
            mask <<= 1;
        }
        portnum++;
    }
    // led Pins.
    portnum = 0;
    for (channel = 0; channel < 2; channel++) {
        if ((halError = hal_pin_new_bool(comp_id, HAL_IN, &(this->port[portnum].io[24].pValue),
                0, "opto-ac5.%d.led%d", boardId, channel + portnum)) != 0)
            break;

        if ((halError = hal_pin_new_bool(comp_id, HAL_IN, &(this->port[portnum].io[25].pValue),
                0, "opto-ac5.%d.led%d", boardId, channel + portnum + 1)) != 0)
            break;
        portnum++;
    }
    // Export functions.
    if (!halError) {
        halError = hal_export_functf(Device_DigitalOutWrite, this, 0, comp_id, "opto-ac5.%d.digital-write", boardId);
    }

    if (halError) {
        rtapi_print_msg(RTAPI_MSG_ERR, "ERROR OPTO_AC5---exporting digital outputs to HAL failed\n");
        return(-1);
    }

    return(0);
}

// here we read inputs from board
// we read the current data (variable 'pins') of the first port. Then for each of the 24 points
// we compare to the mask of the first port to see which of the 24 io points are inputs (the bits that are false)
// if it is an input then check 'pins' against the mask to see if input bit is true
// update the HAL pin and not-pin accordingly. shift the mask then do the next point (of 24 io points)
// after all pins done-increase 'portnum' to 1 set offset to the offset for port1
// then do it all again on the second port

static void
Device_DigitalInRead(void *arg, long period)
{
    (void)period;
    board_data_t *pboard = (board_data_t *)arg;
    DigitalPinsParams *pDigital;
    int i, portnum = 0;
    unsigned long pins, offset = DATA_READ_OFFSET_0, mask;

    // For each port.
    while (portnum < 2) {
        mask = 1;
        pDigital = &pboard->port[portnum].io[0];

        // Read digital I/O register.
        pins = ac5_read(pboard, offset);
        for (i = 0; i < 24; i++, pDigital++) {
            if ((pboard->port[portnum].mask & mask) == 0) { // is it an input bit ?
                // A 0 read from the hardware means the input is on.
                rtapi_bool b = (pins & mask) == 0;
                hal_set_bool(pDigital->pValue, b);
                hal_set_bool(pDigital->pValueNot, !b);
            }
            mask <<= 1; // shift mask
        }
        portnum++;
        offset = DATA_READ_OFFSET_1; // change to offset for port1
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
    (void)period;
    board_data_t *pboard = (board_data_t *)arg;
    DigitalPinsParams *pDigital;
    int portnum = 0;
    unsigned int i, j;
    unsigned long pins, offset = DATA_WRITE_OFFSET_0, mask;

    // For each port.
    while (portnum < 2) {
        pDigital = &pboard->port[portnum].io[0];
        mask = 1;
        pins = 0;

        // For each pin.
        for (j = 0; j < 24; j++, pDigital++) {
            if ((pboard->port[portnum].mask & mask) != 0) { // is it an output?
                // A 1 written to the hardware turns the output off, so set
                // the bit when the pin is off, or on with invert set.
                rtapi_bool val = hal_get_bool(pDigital->pValue);
                rtapi_bool inv = hal_get_bool(pDigital->invert);
                if (val == inv) {
                    pins |= mask;
                }
            }
            mask <<= 1; // shift mask

        }

        // CHECK LED PINS
        pDigital = &pboard->port[portnum].io[23]; // one before what we want to check
        for (i = 0; i < 2; i++) {
            mask = (unsigned int) 1 << (31 - i);
            pDigital++;

            if (hal_get_bool(pDigital->pValue) == 0) {
                pins |= mask;
            }
        }
        // Write digital I/O register.
        ac5_write(pboard, offset, pins);
        portnum++;
        offset = DATA_WRITE_OFFSET_1; // set to port1 offset
    }
}
