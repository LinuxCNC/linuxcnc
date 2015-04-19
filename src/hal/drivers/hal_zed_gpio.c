/********************************************************************
* Description:  hal_zed_gpio.c
* Test driver for the ZedBoard GPIO pins for real time applications
*
* \author Claudio Lorini (claudio.lorini@iit.it)
* License: GPL Version 2
* Copyright (c) 2015.
*
* code derived from the hal_gpio.c by:
* Author: Michael Haberler
* License: GPL Version 2
* Copyright (c) 2012.
*
********************************************************************/

/**
 \brief Test driver for the ZedBoard GPIO pins for realtime applications.

 \details This driver profides a series of digital IO connected to the
   PMods connectors JA1, JB1, JC1, JD1, JE1 and Vita connector.
   
   In order to mantain rt performances the access to the peripheral is
   done as a memory access to the xilinx-gpio peripheral.
   Details of the pheripheral registers structure can be found in the 
   following document:
   ug585-Zynq-7000-TRM.pdf (pag.381, pag.1348)

 \par IO connection table:
    Pin assignment with CAN8 board (Vita)
    55,56,57,58,59,60,61,62  - Led CAN      (out)
    63,64,65,66,67,68,69,70  - DIP Switch 1 (in) 
    71,72,73,74,75,76,77,78  - J8 Outputs   (out)
    79,80,81,82,83,84,85     - J6 Inputs    (in)
    86,87,88,89,90,91,92     - J7 Inputs    (in)

 \par Revision history:
 \date  22.01.2014 started development from hal_gpio.c files
 \version 00
 \date  07.02.2014 compiled from source tree and loaded successfully
 \version 01
 \date  12.02.2014 Inputs working at last! 
 \version 02
 \date  20.03.2015 Moved to a true rt implementation
 \version 03

 \note
 \bug
 \warning 
 \todo
 \pre 
 \param 
 \return
*/

/**
\brief Zynq gpio peripheral register mapping from ug585-Zynq-7000-TRM.pdf 
*/

/** \brief gpio peripheral base address */
#define GPIO_BASE           0xE000A000 

/** \breief This registers enables software to change the value being output.  
Only data values with a corresponding deasserted mask bit will be changed.
0: pin value is updated
1: pin is masked */
#define MASK_DATA_0_LSW     0x00000000 
#define MASK_DATA_0_MSW     0x00000004 
#define MASK_DATA_1_LSW     0x00000008 
#define MASK_DATA_1_MSW     0x0000000C 
#define MASK_DATA_2_LSW     0x00000010 
#define MASK_DATA_2_MSW     0x00000014 
#define MASK_DATA_3_LSW     0x00000018 
#define MASK_DATA_3_MSW     0x0000001C 

/** \breief This register controls the value being output when the GPIO 
signal is configured as an output. All 32bits of this register are written 
at one time. */
#define DATA_0              0x00000040
#define DATA_1              0x00000044
#define DATA_2              0x00000048
#define DATA_3              0x0000004C

/** \breief This register enables software to observe the value on the device pin. 
If the GPIO signal is configured as an output, then this would normally reflect 
the value being driven on the output. Writes to this register are ignored.*/
#define DATA_0_RO           0x00000060 // 00..31 [8:7] cannot be used as inputs, will always return 0 when read.
#define DATA_1_RO           0x00000064 // 32..53 
#define DATA_2_RO           0x00000068 // 54..85
#define DATA_3_RO           0x0000006C // 86..117

/** \breief This register controls whether the IO pin is acting as an input 
or an output. Since the input logic is always enabled, this effectively 
enables/disables the output driver.
0: input
1: output */
#define DIRM_0              0x00000204 
#define DIRM_1              0x00000244 
#define DIRM_2              0x00000284
#define DIRM_3              0x000002C4

/** \breief When the IO is configured as an output, this controls whether 
the output is enabled or not. When the output is disabled, the pin is tri-stated.
0: disabled
1: enabled */
#define OEN_0               0x00000208
#define OEN_1               0x00000248
#define OEN_2               0x00000288
#define OEN_3               0x000002C8

/** \breief This register shows which bits are currently masked and which 
are un-masked/enabled. This register is read only, so masks cannot be 
changed here.
0: interrupt source enabled
1: interrupt source masked */
#define INT_MASK_0          0x0000020C 
#define INT_MASK_1          0x0000024C 
#define INT_MASK_2          0x0000028C
#define INT_MASK_3          0x000002CC 

/** \breief This register is used to enable or unmask a GPIO input for 
use as an interrupt source.Writing a 1 to any bit of this register enables/unmasks 
that signal for interrupts. */
#define INT_EN_0            0x00000210 
#define INT_EN_1            0x00000250
#define INT_EN_2            0x00000290
#define INT_EN_3            0x000002D0 

/** \breief This register is used to disable or mask a GPIO input for use 
as an interrupt source. Writing a 1 to any bit of this register disables/masks 
that signal for interrupts. */
#define INT_DIS_0           0x00000214 
#define INT_DIS_1           0x00000254
#define INT_DIS_2           0x00000294
#define INT_DIS_3           0x000002D4

/** \breief This registers shows if an interrupt event has occurred or not. 
Writing a 1 to a bit in this register clears the interrupt status for that bit. */
#define INT_STAT_0          0x00000218
#define INT_STAT_1          0x00000258
#define INT_STAT_2          0x00000298
#define INT_STAT_3          0x000002D8

/** \breief This register controls whether the interrupt is edge sensitive 
or level sensitive.
0: level-sensitive
1: edge-sensitive */
#define INT_TYPE_0          0x0000021C
#define INT_TYPE_1          0x0000025C
#define INT_TYPE_2          0x0000029C
#define INT_TYPE_3          0x000002DC

/** \breief This register controls whether the interrupt is active-low or 
active high (or falling-edge sensitive or rising-edge sensitive).
0: active low or falling edge
1: active high or rising edge */
#define INT_POLARITY_0      0x00000220 
#define INT_POLARITY_1      0x00000260
#define INT_POLARITY_2      0x000002A0
#define INT_POLARITY_3      0x000002E0 

/** \breief If INT_TYPE is set to edge sensitive, then this register enables an 
interrupt event on both rising and falling edges. This register is ignored if 
INT_TYPE is set to level sensitive.
0: trigger on single edge, using configured interrupt polarity
1: trigger on both edges */
#define INT_ANY_0           0x00000224 
#define INT_ANY_1           0x00000264
#define INT_ANY_2           0x000002A4
#define INT_ANY_3           0x000002E4

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "rtapi.h"
#include "rtapi_bitops.h"
#include "rtapi_app.h"
#include "hal.h"

#if !defined(BUILD_SYS_USER_DSO) 
    #error "This driver is for usermode threads only"
#endif
#if !defined(TARGET_PLATFORM_ZEDBOARD)
    #error "This driver is for the Zedboard platform only"
#endif

MODULE_AUTHOR("Claudio Lorini");
MODULE_DESCRIPTION("Driver for Zedboard GPIOs");
MODULE_LICENSE("GPL");

// RT component ID
static int comp_id;

// array of bits, used for input and output
hal_bit_t **oport_data;
hal_bit_t **iport_data;

// number of available gp in
static int ngpi = 7;
// number of available gp out
static int ngpo = 8;

// pointer to start of GPIO pheriferal registers
void *base;

// file descriptor for mem access
int fd;

/**
 \brief Write IO function exported to hal
 \pre ngpio and base must be initialized */
static void write_port(void *arg, long period)
{
    int n;
    unsigned *RDATA_2 = base + DATA_2;

    // *((unsigned *)(base + DATA_2)) = 6;

    // J8 Outputs from 71 to 78 are on DATA_2 reg.
    for (n = 0; n < ngpo; n++) {
        if (0 == *(oport_data[n])) { 
            RTAPI_BIT_CLEAR(RDATA_2, n+17);
        }
        else {
            RTAPI_BIT_SET(RDATA_2, n+17);
        }
    }
}

/**
 \brief Read IO function exported to hal
 \pre ngpio must be initialized  */
static void read_port(void *arg, long period)
{
    int n=0;
    unsigned *RDATA_2_RO = base + DATA_2_RO;

    // regvalue = *((unsigned *)(base + DATA_2_RO));

    for (n = 0; n < ngpi; n++) {
        *iport_data[n] = RTAPI_BIT_TEST(RDATA_2_RO, n+25);
    }
}

/**
 \brief configrure GPIOs
 \pre ngpio must be initialized first*/
static void setup_gpio_access()
{     
    // set DATA_2 as 8 outputs J8 + 8 out LED
    *((unsigned *)(base + DIRM_2)) = 0x01FE01FE;
    // enable output drivers 
    *((unsigned *)(base + OEN_2))  = 0x01FE01FE;

    // enable update outputs 
    *((unsigned *)(base + MASK_DATA_2_LSW)) = 0x0;
    *((unsigned *)(base + MASK_DATA_2_MSW)) = 0x0;
}

/**
 \brief   Determine Zynq revision
 \details Parse data in /proc/cpuinfo for 'Revision'
 \return 
    -1: unable to parse /proc/cpuinfo
    -1: unable to parse a version number
    nnnn: the one and only revision */
static int zynq_revision()
{
    char *path = "/proc/cpuinfo",  *s, line[1024];
    int rev = -1;
    // parse /proc/cpuinfo for the line: Revision
    char *rev_line = "Revision";
    FILE *f = fopen(path,"r");
  
    if (!f) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: can't open %s: %d - %s\n",
        path, errno, strerror(errno));
        return -1;
    }
  
    while (fgets(line, sizeof(line), f)) {
        if (!strncmp(line, rev_line, strlen(rev_line))) {
            s = strchr(line, ':');
            if (s && 1 == sscanf(s, ":%d", &rev)) {
                fclose(f);
                return rev;
            }
        }
    }
    fclose(f);
    return -1;
}

/**
 \brief   Determine Zedboard FPGA HW revision
 \details The FPGA can contain different resources, a version register determine 
   the available resources
 \todo    Do register read for FPGA versioning  
 \return 
    01: the one and only revision */
static int zb_revision()
{
    return 01;
}

/**
 \brief main realtime task */
int rtapi_app_main(void)
{
    // zynq and FPGA code revision
    int rev, zrev;
    // save messaging level 
    static int msg_level;
    int n, retval = 0;
    
    // save message level on entering 
    msg_level = rtapi_get_msg_level();
    
    /* force messaging level in:
    RTAPI_MSG_NONE,
    RTAPI_MSG_ERR,
    RTAPI_MSG_WARN,
    RTAPI_MSG_INFO,
    RTAPI_MSG_DBG,
    RTAPI_MSG_ALL 
    rtapi_set_msg_level(RTAPI_MSG_ALL); */

    // check Zynq revision 
    if ((zrev = zynq_revision()) < 0) {
        // unable to determine zynq revision 
        return -1;
    }
    // notify zynq revision
    rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_GPIO: Zynq Revision %d \n", zrev);

    // check Zedboard FPGA hardware revision 
    rev = zb_revision();
  
    // do revision specific configuration
    switch (rev) {
        case 01:
            rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_GPIO: Zedboard FPGA Revision 01\n");
            ngpi = 7;
            ngpo = 8;
        break;

        default:
            rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: FPGA revision %d not (yet) supported\n", rev);
            return -1;
        break;
    }

    // Open /dev/mem file
    fd = open ("/dev/mem", O_RDWR);
    if (fd < 1) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: Unable to open /dev/mem. Quitting.\n");
        return -1;
    }

    // mmap the device into memory 
    {
        unsigned page_addr, page_offset;
        unsigned page_size=sysconf(_SC_PAGESIZE);

        page_addr = (GPIO_BASE & (~(page_size-1)));
        page_offset = GPIO_BASE - page_addr;
        if (0 != page_offset){
            rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: Pheripheral not aligned to page start! \n");
            return -1;
        }

        base = mmap(NULL, page_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, page_addr);
    }

    // allocate space for IO port data
    iport_data = hal_malloc(ngpi * sizeof(void *));
    oport_data = hal_malloc(ngpo * sizeof(void *));
    if (( 0 == iport_data ) || ( 0 == iport_data )){
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: hal_malloc() failed\n");
        return -1;
    }

    // export and configure gpios
    setup_gpio_access();

    // try to init the component
    comp_id = hal_init("hal_zed_gpio");
    if (comp_id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: hal_init() failed\n");
        return -1;
    }

    // make available all gpios in hal
    for (n = 0; n < ngpo; n++) {
        if ( (retval = hal_pin_bit_newf(HAL_IN, &oport_data[n], comp_id, "hal_zed_gpio.pin-%02d-out", n) ) < 0) {
            break;
        }
    }
    for (n = 0; n < ngpi; n++) {
        if ( (retval = hal_pin_bit_newf(HAL_OUT, &iport_data[n], comp_id, "hal_zed_gpio.pin-%02d-in", n) ) < 0) {
            break;
        }
    }
    // check for failed gpio pin mapping
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: pin %d export failed with err=%i\n", n,retval);
        hal_exit(comp_id);
        return -1;
    }

    // export the read_port and write_port functions as hal_zed_gpio.read and hal_zed_gpio.write in hal
    retval = hal_export_funct("hal_zed_gpio.write", write_port, 0, 0, 0, comp_id);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: write funct export failed\n");
        hal_exit(comp_id);
        return -1;
    }
    retval = hal_export_funct("hal_zed_gpio.read", read_port, 0, 0, 0, comp_id);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_GPIO: ERROR: read funct export failed\n");
        hal_exit(comp_id);
        return -1;
    }

    // all operations succeded
    rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_GPIO: driver installed successfully.\n");
    hal_ready(comp_id);

    // return to previous mesaging level
    // rtapi_set_msg_level(msg_level);

    return 0;
}

/** 
 \brief Exit component */
void rtapi_app_exit(void)
{    
    //close /dev/mem
    close(fd);

    // notify clean termination
    rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_GPIO: component terminated successfully \n");

    hal_exit(comp_id);
}


