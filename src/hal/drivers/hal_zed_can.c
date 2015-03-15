/********************************************************************
* Description:  hal_zed_can.c
* Test driver for the ZedBoard CAN communication with 2FOC board
*
* \author Claudio Lorini (claudio.lorini@iit.it)
* License: GPL Version 2
* Copyright (c) 2014.
*
* Some code parts and profund inspiration taken from:
* - the hal_gpio.c by:
*   Author: Michael Haberler
* - the bcm2835 library by:
*   Author: Mike McCauley
* - the stepgen.c component:
*   Author: John Kasunich
*
********************************************************************/

/**
 \brief Test driver for the ZedBoard CAN communication with 2FOC boards
  using CAN8 interface board.

 \details
  The number FOC axis controlled is determined  by the insmod command 
  line parameter 'FOC_axis' passed from .hal configuration file. 
  It accepts a comma separated (no spaces) list of up to 8 numbers 
  indicating the CAN address of the FOC channel. 
  \note actually the FOC axis are configured all with address 3 each
  axis on a single CAN line in order to stress the system with maximum 
  CAN load.
    
  A second command line parameter "ctrl_type", selects between position 
  csp, traj-generator, velocity, current, torque control modes
   
  in order to adapt to different mechanical setup/encoder type the following 
  parameters are needed:
    PPR: pulses per revolution (for 2FOC is always 65536 all encoders are expanded to 16bit)
    Screw_ratio: lead screw ratio (mm/turns) such as 5mm/turn 
    Screw gear: number of teeths of screw pulley 
    Motor gear: number of teeths of motor pulley 

  To use this driver the 2FOC boards must be configured for speed control
  and CAN address 3 connecting each axis on a different CAN8 line in a 
  point2point configuration.
  2FOC periodic message is configured for: 
   - position(32bit)
   - velocity(16bit)
   - Iq(16bit)

 \par How to compile.
  In order to compile, this driver should be placed in the 
  /machinekit/src/hal/drivers/ directory
  in the the Makefile in the /machinekit/src directory add the following:
    ifeq ($(TARGET_PLATFORM), zedboard)
    obj-$(CONFIG_HAL_ZED_CAN) += hal_zed_can.o
    hal_zed_can-objs := hal/drivers/hal_zed_can.o
    endif
  in the '# Subdirectory: hal/drivers' section

  and add 
    ifeq ($(TARGET_PLATFORM),zedboard)
    $(RTLIBDIR)/hal_zedcan$(MODULE_EXT): $(addprefix $(OBJDIR)/,$(hal_zedcan-objs))
    endif
  in the '# build kernel RTAPI modules' section

  in the the Makefile.inc in the /machinekit/src directory add the following:
    CONFIG_HAL_ZED_CAN=m
  in the '# HAL drivers' section

 \par Revision history:
 \date 11.03.2014 started development from zeth.c files
 \date 21.03.2014 2FOC feedback, status and error parser
 \date 23.03.2014 extension to multichannel and parametrization
 \date 12.03.2015 ported to machinekit

 \version 00
*/

#include "rtapi.h"         // rtapi_print_msg()
#include "rtapi_bitops.h"  // RTAPI_BIT(n)    
#include "rtapi_app.h"     // 
#include "hal.h"

#if !defined(BUILD_SYS_USER_DSO) 
    #error "This driver is for usermode threads only"
#endif
#if !defined(TARGET_PLATFORM_ZEDBOARD)
    #error "This driver is for the Zedboard platform only"
#endif

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include "2FOC_status.h"

/** \brief maximum number of FOC channels */
#define MAX_FOC_CHAN 8

MODULE_AUTHOR("Claudio Lorini");
MODULE_DESCRIPTION("Driver CAN for Zedboard 2FOC board");
MODULE_LICENSE("GPL");

// Miscellaneous peripheral 
#define PMODA_OUTREG            0x04
#define PMODB_INREG             0x08
#define RESET_CAN_REG           0x00

#define PMODJA1                 0
#define PMODJA2                 1
#define PMODJA3                 2
#define PMODJA4                 3
#define PMODACTIVE              1
#define PMODNOTACTIVE           0

inline void WR(unsigned int OutAddress, unsigned int Value) { *(volatile unsigned int *) OutAddress = Value; }
#define WriteReg(BaseAddr, RegOffset, Data) \
    WR((BaseAddr) + reg_map_ocores_can[RegOffset], (Data))

inline unsigned int RD(unsigned int InAddress) { return (*(volatile unsigned int *) InAddress); }
#define ReadReg(BaseAddr, RegOffset) \
    RD((BaseAddr) + reg_map_ocores_can[RegOffset])

// for Read Modify Write on misc pheripheral
unsigned char rmw;
unsigned int MISC_ADDR = 0x43c40000;
unsigned int map_MiscAddress;

// RT component ID
static int comp_id;

// 2FOC mirror data
typedef struct {
    //
    // pins
    //
    // image of setpoint
    hal_float_t *setpoint;
    // image of feedback
    hal_float_t *feedback;

    // image of status of 2FOC
    hal_u32_t   *focstatus;
    // image of error of 2FOC
    hal_u32_t   *focerror;

    //
    // params
    //

    // used for debug pourpouses
    hal_float_t dbg;
    
} FOC_data_t;

/** \brief Array of FOC_data structs in shared memory, 1 per configured axis */
static FOC_data_t * FOC_data_array;

// socket handles
int sock[MAX_FOC_CHAN];

// tx/rx frames
struct can_frame txframe[MAX_FOC_CHAN];
struct can_frame rxframe[MAX_FOC_CHAN];

/** \brief number of FOC channels configured */
static int num_chan = 0;

/** \brief Number and CAN address of active FOC axis */
int FOC_axis[] = { [0 ... MAX_FOC_CHAN-1] = -1 };
RTAPI_MP_ARRAY_INT(FOC_axis,MAX_FOC_CHAN,"CAN address for up to 8 FOC channels");

/** \brief Number of teeths of the motor pulley for each active axis */
int Motor_gear[] = { [0 ... MAX_FOC_CHAN-1] = 1 };
RTAPI_MP_ARRAY_INT(Motor_gear,MAX_FOC_CHAN,"Number of teeths of the motor pulley");
/** \brief Number of teeths of the screw pulley for each active axis */
int Screw_gear[] = { [0 ... MAX_FOC_CHAN-1] = 1 };
RTAPI_MP_ARRAY_INT(Screw_gear,MAX_FOC_CHAN,"Number of teeths of the screw pulley");

/** \brief Screw ratio for each active axis */
int Screw_ratio[] = { [0 ... MAX_FOC_CHAN-1] = 1 };
RTAPI_MP_ARRAY_INT(Screw_ratio,MAX_FOC_CHAN,"Screw_ratio for each axis");

/** \brief Screw ratio for each active axis */
int PPR[] = { [0 ... MAX_FOC_CHAN-1] = 65535 };
RTAPI_MP_ARRAY_INT(PPR,MAX_FOC_CHAN,"Encoder PPR for each axis");

/** \brief control type of 2FOC setpoint/feedback */
typedef enum CONTROL { CSP, TRAJGEN, VELOCITY, CURRENT, TORQUE, INVALID } CONTROL;
char *ctrl_type[MAX_FOC_CHAN];
RTAPI_MP_ARRAY_STRING(ctrl_type,MAX_FOC_CHAN,"Control type for up to 8 FOC channels");

// Check if the FOC axis is in 'operation enable' status
bool FOCAxisIsOperationEnable[MAX_FOC_CHAN]={false};

/**
 \brief Send setpoint to 2FOC
 \pre  */
static void send_setpoint(void *arg, long period)
{
    int nbytes; 
    int n;
    __s16 setpoint;
    FOC_data_t *foctxdata = NULL;
    
    // point to first axis data
    foctxdata = arg;

    /* for each FOC axis */
    for (n = 0; n < num_chan; n++) {

        // giva a fake feedback to machinekit
        // *(foctxdata->feedback) = *(foctxdata->setpoint);
        
        // procede to periodic CAN data exchange only if FOC in 'enable operation'
        if( false != FOCAxisIsOperationEnable[n] ) {

            // prepare packet lenght for speed setpoint
            txframe[n].can_dlc = 2;
            // CAN address (cancellastarobafattaccazzo!)
            // speed reference
            txframe[n].can_id = 0x83FFFF04;
            // payload = speed in mm/sec
            setpoint = *(foctxdata->setpoint) * 100.0;
            memcpy(txframe[n].data, &setpoint, sizeof(__s16));
            // transmit!
            nbytes = write (sock[n], &txframe[n], sizeof (struct can_frame));
            if (sizeof (struct can_frame) != nbytes){
                rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_CAN: Error: CAN write on axis %d returned %d",n ,nbytes);
                hal_exit(comp_id);
                exit(-1);
            }
        }

        // move to next axis
        foctxdata++;
    }
}

/**
 \brief  Parse incoming CAN messages
 \params rxframe: incoming message
         n: CAN channel
         FOC_data_t: FOC data image to fill 
 \return 0 everything OK
        -1 */
int ParseMessage(struct can_frame *rxframe, int n, FOC_data_t *focrxdata)
{
    // parse packet type, mask away CAN address
    switch( ( rxframe->can_id & 0xFFFFFF) ) {
        
        case 0xFFFF81:
        {  
            // 0x81FFFF81, here it is a periodic! grab first (4 bytes) data (position).
            __s32 *pos = NULL;

            // FOC position feedback is 65536 postions per turn
            // for a direct drive screw with 1mm x turn 65536.0;
            // for a direct drive screw with 5mm x turn 65536.0/5.0;
            // for 46:22 pulleys and 5mm x turn 65535/(5*46/22) 
            pos = (__s32*) rxframe->data;
            // *(focrxdata->feedback) = (float) *pos / (65536.0/5.0); 
            *(focrxdata->feedback) = (float) *pos / ( (float)PPR[n] / ( (float)Screw_ratio[n]*((float)Motor_gear[n]/Screw_gear[n]))); 
        }
        break;

        case 0xFFFF82:
        {
            // 0x81FFFF82, here it is a status/error packet.
            hal_u32_t *stat = NULL, *err = NULL;
                        
            stat = (hal_u32_t*)  rxframe->data;
            err  = (hal_u32_t*) (rxframe->data+4);
            *(focrxdata->focstatus) = (hal_u32_t) *stat;
            *(focrxdata->focerror)  = (hal_u32_t) *err;
        }
        break;

        case 0xFFFFFE:
            // 0x8xFFFFFE: oh, it's just an ack to some boring command...
        break;

        case 0xFFFFFF:
            // 0x8xFFFFFF: ok, this is a command NACK, something serious has happened, 
            // like an set-point overrange or such...                        
            // \todo try to mend it.
            rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_CAN: Error: NACK (0x%X) from CAN%d",rxframe->can_id, n);
            // hal_exit(comp_id);
            // return (-1);
        break;

        default:
            // aliens incoming!
            rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_CAN: Error: unexpected message received 0x%X form CAN%d",rxframe->can_id, n);
            hal_exit(comp_id);
            return (-1);
        break;
    }

    return (0);
}

/**
 \brief Read feedback from 2FOC
 \pre  */
static void read_feedback(void *arg, long period)
{
    int nbytes=0;
    int n;
    FOC_data_t *focrxdata = NULL;

    // point to first axis data
    focrxdata = arg;

rmw = RD(map_MiscAddress + PMODA_OUTREG);
rmw = rmw | 0x01;
rmw = rmw & 0xFD;
WR(map_MiscAddress + PMODA_OUTREG, rmw);

    // for each FOC axis
    for (n = 0; n < num_chan; n++) {

        // procede to periodic CAN data exchange only if FOC in 'enable operation'
        if( false != FOCAxisIsOperationEnable[n] ) {
            // parse all messages (eventually) in the rx queue 
            do {
                // read data (hopefully not-blocking)
                nbytes = read (sock[n], &rxframe[n], sizeof (struct can_frame));
                if (nbytes >= 0) {
                    // parse packet type, mask away CAN address
                    if ( 0 != ParseMessage(&(rxframe[n]), n, focrxdata) ){
                        //exit(-1);
                    }
                }
            } while ( 0 < nbytes  );
        }
        // move to next axis
        focrxdata++;
    }

rmw = rmw & 0xFE;
WR(map_MiscAddress + PMODA_OUTREG, rmw);

}

/**
 \brief Setup CAN communication with 2FOC board
 \details 
 \pre 
 \return 
   0: everything ok.
   ....: failed to initialize communication */
static int setup_CAN(int n)
{
    int i;
    struct ifreq ifr;
    struct sockaddr_can addr;
    /* This is the RX buffer len in bytes.
    Adjust it in order to let the network layer to
    store the required amount of data to avoid
    packet loss (it must be able to keep at least one
    whole packet) and not so much large to fuck up
    realtime motor control.
    */
    int buflen = 32;
    struct timeval tv;

    // Open a socket of type CAN
    if ((sock[n] = socket (PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: Error while opening socket %d", n);
        hal_exit(comp_id);
        return -1;
    }

    /** \todo error mamagement a palla */
    //
    sprintf(ifr.ifr_name,"can%d",n); 
    //
    ioctl (sock[n], SIOCGIFINDEX, &ifr);
    //
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
   
    rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_CAN: %s at index %d\n", ifr.ifr_name, ifr.ifr_ifindex);

    // Bind the socket
    if (bind (sock[n], (struct sockaddr *) &addr, sizeof (addr)) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: Error in binding socket");
        hal_exit(comp_id);
        return -1;
    }

    /* set socket option for RX buffer size */
    if(-1 == setsockopt(sock[n], SOL_SOCKET, SO_RCVBUF, &buflen, sizeof(buflen))) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: Error receive buffer size");
        hal_exit(comp_id);
        return -1;
    }
      
    /* sets the socket read timout to zero */
    memset((char*)&tv,0,sizeof(tv));
    if(-1 == setsockopt(sock[n], SOL_SOCKET, SO_RCVTIMEO,(char *)&tv,sizeof(tv))) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: Error: cannot set socket RX timeout");
        hal_exit(comp_id);
        return -1;
    }  

    // make the socket read non-blocking 
    if(0 != fcntl(sock[n], F_SETFL, fcntl(sock[n], F_GETFL, 0) | O_NONBLOCK)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: Error: cannot set socket in non-blocking mode");
        hal_exit(comp_id);
        return -1;
    }

    // Fill the frame with data 8 means extended frame
    txframe[n].can_id = 0x83FFFF02;

    // 4 bytes
    txframe[n].can_dlc = 8;
    for (i = 0; i < txframe[n].can_dlc; i++) {
        txframe[n].data[i] = 0;
    }
    return 0;
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
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: can't open %s: %d - %s\n",
          path, errno, strerror(errno));
        hal_exit(comp_id);
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
 \brief   Parse the control type configuration passed to the module 
 \details Valid configurations are:
    'c' for CSP,      Contimuous Setpoint Position
    'j' for TRAJGEN,  Position control with trajectory generator
    'v' for VELOCITY, Velocity control 
    'i' for CURRENT,  Current (torque) control 
    't' for TORQUE,   Torque control with sensor feedback
 \todo all.  
 \return 
    MODE:    detected mode 
    INVALID: mode not set or invalid */
static CONTROL parse_ctrl_type(const char *ctrl)
{
    // no parameters minimum damege.
    if(!ctrl || !*ctrl )             return TORQUE;
    if(*ctrl == 'c' || *ctrl == 'C') return CSP;
    if(*ctrl == 'j' || *ctrl == 'J') return TRAJGEN;
    if(*ctrl == 'v' || *ctrl == 'V') return VELOCITY;
    if(*ctrl == 'i' || *ctrl == 'I') return CURRENT;
    if(*ctrl == 't' || *ctrl == 'T') return TORQUE;
    return INVALID;
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
 \brief Export pins for component(s) 
 \param 
   num: component number 
   addr: pointer to array of the num^th FOC channel data 
*/
static int exportFOCaxis(int num, FOC_data_t * addr)
{    
    int retval = 0;

    //
    // PINS
    //

    // make available setpoint in hal
    if ( (retval = hal_pin_float_newf(HAL_IN, &(addr->setpoint), comp_id, "hal_zed_can.%d.setpoint", num) ) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: ERROR: pin setpoint export failed with err=%d", retval);
        hal_exit(comp_id);
        return -1;
    } 
    // make available position feedback in hal        
    if ( (retval = hal_pin_float_newf(HAL_OUT, &(addr->feedback), comp_id, "hal_zed_can.%d.feedback", num) ) < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: ERROR: pin feedback export failed with err=%d", retval);
        hal_exit(comp_id);
        return -1;
    }

    //
    // PARAMS
    //

    // instantiate a parameter
    retval = hal_param_float_newf(HAL_RW, &(addr->dbg), comp_id, "hal_zed_can.%d.debug", num);
    // check for failed debug space mapping
    if(retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: ERROR: param debug export failed with err=%d", retval);
        hal_exit(comp_id);
        return -1;
    }
    // instantiate parameter 
    retval = hal_pin_u32_newf(HAL_OUT, &(addr->focstatus), comp_id, "hal_zed_can.%d.status", num);
    // check for failed debug space mapping
    if(retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: ERROR: pin status export failed with err=%d" ,retval);
        hal_exit(comp_id);
        return -1;
    }
    // instantiate parameter 
    retval = hal_pin_u32_newf(HAL_OUT, &(addr->focerror), comp_id, "hal_zed_can.%d.error", num);
    // check for failed debug space mapping
    if(retval != 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: ERROR: param error export failed with err=%d",retval);
        hal_exit(comp_id);
        return -1;
    }
    // completed successfully
    rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_CAN: exportFOCaxis(%d) completed successfully.\n", num);

    return 0;
}

/**
 \brief do a programmable delay in a barbarous way... 
 \details 
 \params delaycycle: delay expiration value */
void dodelay(unsigned long delaycycles)
{
    int i;

    for(i=0; i<delaycycles ; ) {
        i++;
    }
}

/**
 \brief Make sure the periodic communication with 2FOC board is on
 \details 
 \pre 
 \return 
   0: everything ok.
   ....: failed to initialize periodic communication */
int setup_2FOC_periodic()
{
    int  nbytes;
    int n;
    // \todo use the global variable for status/error 
    FOC_data_t *focrxdata = NULL;

    // zero contents of databuffers
    for(n = 0; n < num_chan ; n++) {
        *(FOC_data_array[n].setpoint) = 0;
        *(FOC_data_array[n].feedback) = 0;
        *(FOC_data_array[n].focstatus) = 0;
        *(FOC_data_array[n].focerror)  = 0;
        FOC_data_array[n].dbg       = 0;
    }

    // for each FOC axis send 'shutdown', 'switch on' and 'enable operation' commands
    for (n = 0; n < num_chan; n++) {

        // 0 bytes payload
        txframe[n].can_dlc = 0;

        // send 'shutdown' command
        // all FOCs have adx 3
        txframe[n].can_id = 0x83FFFF11;
        nbytes = write (sock[n], &txframe[n], sizeof (struct can_frame));
        if (sizeof (struct can_frame) != nbytes){
            rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_CAN: Unable to send 'shut down' command to axis %d",n);
            hal_exit(comp_id);
            exit(-1);
        }
        dodelay(20000);

        // send 'switch on' command
        // txframe[n].can_id = 0x80FFFF0E | ((n+1) << 24);
        // all FOC has adx 3
        txframe[n].can_id = 0x83FFFF0E;
        nbytes = write (sock[n], &txframe[n], sizeof (struct can_frame));
        if (sizeof (struct can_frame) != nbytes){
            rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_CAN: Unable to send 'switch on' command to axis %d",n);
            hal_exit(comp_id);
            exit(-1);
        }
        dodelay(20000);

        // send 'enable operation' command
        // txframe[n].can_id = 0x80FFFF0F | ((n+1) << 24);
        txframe[n].can_id = 0x83FFFF0F;
        nbytes = write (sock[n], &txframe[n], sizeof (struct can_frame));
        if (sizeof (struct can_frame) != nbytes){
            rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_CAN: Unable to send 'enable operation' command to axis %d",n);
            hal_exit(comp_id);
            exit(-1);
        }     
        dodelay(20000);
    }
    rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_CAN: ShutDown,SwitchOn, EnableOperation sent to all channels.");

    
    // \todo Check anything possible before waiting for alignment
    // - errors of any kind
    // - board switched on and operationenabled
    // - correct configuration (speed loop)

    // point to first axis data
    focrxdata = FOC_data_array;

    // wait for completion of the rotor alignment (flag in status message)
    for (n = 0; n < num_chan; n++) {
        int nbytes=0;
    
        do {
            // read CAN packet
            nbytes = read (sock[n], &rxframe[n], sizeof (struct can_frame));
            if (nbytes >= 0) {
                // parse packet type
                if ( 0 != ParseMessage(&(rxframe[n]), n, focrxdata) ){
                    //exit(-1);
                }
            }
        }
        // rotor alignment completed 
        while ( 0 == (*(focrxdata->focstatus) & 0x00020000 ) );
  
        // \todo Check anything possible before living control to machinekit:
        // - errors of any kind
        // - correct configuration (speed loop)
        // - 2FOC firmware version
        // - board switched on and operationenabled
        rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_CAN: axis %d aligment completed.",n);

        // enable periodic rx & tx
        FOCAxisIsOperationEnable[n] = true;

        // move to next axis
        focrxdata++;
    }

    // everityng is ok!
    return 0;
}

/**
 \brief main realtime task */
int rtapi_app_main(void)
{
    // zynq and FPGA code revision
    int rev, zrev, n;
    // save messaging level 
    static int msg_level;
    int retval = 0;
    
    int fdmisc; 

    // save message level on entering 
    msg_level = rtapi_get_msg_level();
    
    /* setup messaging level in:
    RTAPI_MSG_NONE, RTAPI_MSG_ERR, RTAPI_MSG_WARN,
    RTAPI_MSG_INFO, RTAPI_MSG_DBG, RTAPI_MSG_ALL */
    rtapi_set_msg_level(RTAPI_MSG_ALL);

    // check Zynq revision 
    if ((zrev = zynq_revision()) < 0) {
        // unable to determine zynq revision 
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: ERROR: unable to determine zynq revision");
        hal_exit(comp_id);
        return -1;
    }
    // notify zynq revision
    rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_CAN: Zynq Revision %d \n", zrev);

    // check Zedboard FPGA hardware revision 
    rev = zb_revision();
  
    // do revision specific configuration
    switch (rev) {
        case 01:
            rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_CAN: Zedboard FPGA Revision 01\n");
        break;

        default:
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: ERROR: FPGA revision %d not (yet) supported\n", rev);
        return -1;
        break;
    }

    // open and map miscellaneous peripheral for PMOD IO access
    fdmisc = open("/dev/mem", O_RDWR);

    map_MiscAddress = (unsigned int) mmap(NULL, 100, PROT_READ | PROT_WRITE, MAP_SHARED, fdmisc, (off_t)MISC_ADDR );
    if (-1 == map_MiscAddress ) {
        fprintf(stderr, "MMap failed to map Miscellaneus peripheral\n");
        return 0;
    }
    printf("Map Misc peripheral: virtual 0x%x  real 0x%x \n", map_MiscAddress, MISC_ADDR);

    // parse module parameters in order to find the number
    // of configured FOC channels and their CAN address/configuration 
    for(n = 0; n < MAX_FOC_CHAN && FOC_axis[n] != -1 ; n++) {
        // check for a valid CAN address 
        if( (FOC_axis[n] <= 0) || ( FOC_axis[n] > 15) ) {
            rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: ERROR: bad CAN address '%i', axis %i", FOC_axis[n], n);
            hal_exit(comp_id);
        return -1;
        }
        // check the control mode configuration
        if( INVALID ==  parse_ctrl_type(ctrl_type[n])) {
            rtapi_print_msg(RTAPI_MSG_ERR,"HAL_ZED_CAN: ERROR: bad control type '%s' for axis %i ('c','j','v','i','t')",
              ctrl_type[n], n);
            hal_exit(comp_id);
            return -1;
        }
        // found a correctly configured channel 
        num_chan++;
        // notify 
        rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_CAN: FOC axis %d with CAN address %d.",n, FOC_axis[n] );
        rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_CAN: Motor gear %d, Screw gear %d, Screw ratio %d Encoder PPR %d.", Screw_gear[n], Motor_gear[n], Screw_ratio[n], PPR[n]);
    }
    if( (0 == num_chan) || (8 < num_chan) ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: ERROR: channels configured incorrectly.");
        hal_exit(comp_id);
        return -1;
    }
 
    // allocate shared memory for FOC_data of each axis
    FOC_data_array = hal_malloc(num_chan * sizeof(FOC_data_t));
    if ( 0 == FOC_data_array ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: ERROR: hal_malloc() failed\n");
        hal_exit(comp_id);
        return -1;
    }

    /** \note CAN8/2FOC connection is 1 axis for each CAN channel */
    // configure CAN communication
    for(n = 0; n < num_chan ; n++) {
        if ( 0 != setup_CAN(n) ) {
            hal_exit(comp_id);
            return -1;
        }
    }

    // try to init the component
    comp_id = hal_init("hal_zed_can");
    if( comp_id < 0 ) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: ERROR: hal_init() failed\n");
        hal_exit(comp_id);
        return -1;
    }

    /* Export the variables/parameters for each FOC axis */
    for (n = 0; n < num_chan; n++) {
        // export pins/params for the n^th component
        retval = exportFOCaxis(n, &(FOC_data_array[n]) );
        if(  retval < 0 ) {
            rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: ERROR: exportFOCaxis() failed");
            hal_exit(comp_id);
            return -1;
        }
    }

    // export the read_feedback and send_setpoint functions as hal_zed_can.send_setpoint and hal_zed_can.read_feedback in hal
    retval = hal_export_funct("hal_zed_can.send_setpoint", send_setpoint, FOC_data_array, 0, 0, comp_id);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: ERROR: write funct export failed\n");
        hal_exit(comp_id);
        return -1;
    }
    retval = hal_export_funct("hal_zed_can.read_feedback", read_feedback, FOC_data_array, 0, 0, comp_id);
    if (retval < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "HAL_ZED_CAN: ERROR: read funct export failed\n");
        hal_exit(comp_id);
        return -1;
    }

    // activate  periodic communication on all axis
    setup_2FOC_periodic();
    rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_CAN: FOC periodic data exchange active.");

    // all operations succeded
    rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_CAN: driver installed successfully.\n");
    hal_ready(comp_id);

    // return to previous mesaging level
    rtapi_set_msg_level(msg_level);

    return 0;
}

/** 
 \brief Exit component closing communication with EMS 
 \pre */
void rtapi_app_exit(void)
{ 
    int n;

    // notify clean termination
    rtapi_print_msg(RTAPI_MSG_INFO, "HAL_ZED_CAN: component terminated successfully \n");
   
    // close CAN sockets
    for(n = 0; n < num_chan ; n++) {
        close(sock[n]);
    }

    hal_exit(comp_id);
}

