/******************************************************************************
 *
 * Copyright (C) 2007 Peter G. Vavaroutsos <pete AT vavaroutsos DOT com>
 *
 * $RCSfile$
 * $Author$
 * $Locker$
 * $Revision$
 * $State$
 * $Date$
 *
 * This module is a hard coded PLC for use on a Bridgeport Boss.
 *
 * Installation of the driver (realtime only):
 *
 * insmod boss_plc count=<1>
 *
 *
 * The following items are exported to the HAL. <id> is
 * the component id number and is formated as "%d".
 *
 *  Parameters:
 *        u32       boss_plc.<id>.amp-ready-delay
 *        u32       boss_plc.<id>.brake-on-delay
 *        u32       boss_plc.<id>.brake-off-delay
 *        u32       boss_plc.<id>.spindle-lo-to-hi
 *        float     boss_plc.<id>.jog-scale-0
 *        float     boss_plc.<id>.jog-scale-1
 *        float     boss_plc.<id>.jog-scale-2
 *
 *  Pins:
 *        bit       boss_plc.<id>.cycle-start-in
 *        bit       boss_plc.<id>.cycle-hold-in
 *        bit       boss_plc.<id>.feed-hold-out
 *        float     boss_plc.<id>.adaptive-feed-in
 *        float     boss_plc.<id>.adaptive-feed-out
 *        bit       boss_plc.<id>.tool-change-in
 *        bit       boss_plc.<id>.tool-changed-out
 *        bit       boss_plc.<id>.wait-user-out
 *
 *        bit       boss_plc.<id>.limit-override-in
 *        bit       boss_plc.<id>.limit-active-out
 *        float     boss_plc.<id>.x-position-in
 *        bit       boss_plc.<id>.x-jog-en-in
 *        bit       boss_plc.<id>.x-limit-in
 *        bit       boss_plc.<id>.x-limit-pos-out
 *        bit       boss_plc.<id>.x-limit-neg-out
 *        float     boss_plc.<id>.y-position-in
 *        bit       boss_plc.<id>.y-jog-en-in
 *        bit       boss_plc.<id>.y-limit-in
 *        bit       boss_plc.<id>.y-limit-pos-out
 *        bit       boss_plc.<id>.y-limit-neg-out
 *        bit       boss_plc.<id>.z-jog-en-in
 *        bit       boss_plc.<id>.z-limit-pos-in
 *        bit       boss_plc.<id>.z-limit-neg-in
 *        bit       boss_plc.<id>.z-limit-pos-out
 *        bit       boss_plc.<id>.z-limit-neg-out
 *
 *        bit       boss_plc.<id>.x-amp-enable-in
 *        bit       boss_plc.<id>.x-amp-ready-in
 *        bit       boss_plc.<id>.x-amp-fault-out
 *        bit       boss_plc.<id>.y-amp-enable-in
 *        bit       boss_plc.<id>.y-amp-ready-in
 *        bit       boss_plc.<id>.y-amp-fault-out
 *        bit       boss_plc.<id>.z-amp-enable-in
 *        bit       boss_plc.<id>.z-amp-ready-in
 *        bit       boss_plc.<id>.z-amp-fault-out
 *        bit       boss_plc.<id>.a-amp-enable-in
 *        bit       boss_plc.<id>.a-amp-ready-in
 *        bit       boss_plc.<id>.a-amp-fault-out
 *
 *        float     boss_plc.<id>.spindle-speed-in
 *        bit       boss_plc.<id>.spindle-is-on-in
 *        bit       boss_plc.<id>.spindle-fwd-out
 *        bit       boss_plc.<id>.spindle-rev-out
 *        bit       boss_plc.<id>.spindle-inc-in
 *        bit       boss_plc.<id>.spindle-dec-in
 *        bit       boss_plc.<id>.spindle-inc-out
 *        bit       boss_plc.<id>.spindle-dec-out
 *        bit       boss_plc.<id>.brake-en-in
 *        bit       boss_plc.<id>.brake-en-out
 *
 *        bit       boss_plc.<id>.jog-sel-in-0
 *        bit       boss_plc.<id>.jog-sel-in-1
 *        bit       boss_plc.<id>.jog-sel-in-2
 *        bit       boss_plc.<id>.jog-scale-out
 *
 *   Functions:
 *        void      boss_plc.<id>.refresh
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


#include "rtapi.h"                      // RTAPI realtime OS API.
#include "rtapi_app.h"                  // RTAPI realtime module decls.
#include "hal.h"                        // HAL public API decls.


#define TRUE                            1
#define FALSE                           0
typedef int                             BOOL;


#ifndef MODULE
#define MODULE
#endif


#ifdef MODULE
// Module information.
MODULE_AUTHOR("Pete Vavaroutsos");
MODULE_DESCRIPTION("Bridgeport BOSS PLC for EMC HAL");
MODULE_LICENSE("GPL");
static unsigned long                    count = 1;
RTAPI_MP_LONG(count, "Number of BOSS PLCs to instance");
#endif // MODULE


/******************************************************************************
 * TIMER OBJECT
 *
 * This object implements a timer with mSec resolution.
 *
 ******************************************************************************/

typedef enum {
    TM_ONE_SHOT,
    TM_CONTINUOUS,
} TimerMode;

typedef void                            (*TIMER_ROUTINE)(void *pArgs);

typedef struct {
    // Private data.
    BOOL                                enabled;
    hal_u32_t                           nSec;
    hal_u32_t                           count;
    hal_u32_t                           timeout;
    TIMER_ROUTINE                       pTimeout;
    void                                *pArgs;
    TimerMode                           mode;
} Timer;

static void Timer_Init(Timer *this);
static void Timer_Enable(Timer *this, TimerMode mode);
static void Timer_Disable(Timer *this);
static BOOL Timer_IsEnabled(Timer *this);
static void Timer_Update(Timer *this, long period);
static void Timer_SetTimeout(Timer *this, hal_u32_t timeout);
#if 0
static void Timer_SetCallback(Timer *this, TIMER_ROUTINE pCallback, void *pArgs);
#endif


/******************************************************************************
 * LIMIT OBJECT
 *
 * This object converts a bi-directional limit switch into to limit signals.
 *
 ******************************************************************************/

typedef enum {
    LS_INIT,
    LS_ON_LIMIT,
    LS_NO_MOTION,
    LS_POS_MOTION,
    LS_NEG_MOTION,
} LimitState;

typedef struct {
    // Exported pins.
    hal_float_t                         *pPositionIn;
    hal_bit_t                           *pJogEnIn;
    hal_bit_t                           *pIn;
    hal_bit_t                           *pPosOut;
    hal_bit_t                           *pNegOut;

    // Internal data.
    LimitState                          state;
    hal_float_t                         position;
    hal_bit_t                           limitPos;
    hal_bit_t                           limitNeg;
} Limit;

static int Limit_Export(Limit *this, int compId, int id, char *name, char axis);
static void Limit_Init(Limit *this);
static BOOL Limit_IsActive(Limit *this);
static void Limit_Refresh(Limit *this, hal_bit_t override);


/******************************************************************************
 * AMP OBJECT
 *
 * This object creates the am fault signal from the enable and ready signal.
 *
 ******************************************************************************/

typedef struct {
    // Exported pins.
    hal_bit_t                           *pEnableIn;
    hal_bit_t                           *pReadyIn;
    hal_bit_t                           *pFaultOut;

    // Internal data.
    Timer                               timer;
    hal_bit_t                           lastEnable;
} Amp;

static int Amp_Export(Amp *this, int compId, int id, char *name, char axis);
static void Amp_Init(Amp *this);
static void Amp_Refresh(Amp *this, long period, hal_u32_t readyDelay);


/******************************************************************************
 * DEVICE OBJECT
 *
 * This object contains all the data for one PLC. A device object is
 * dynamically allocated in shmem for each PLC during initialization.
 *
 ******************************************************************************/

#define NUM_JOG_SEL                     3
#define NUM_AXIS                        4

static char                             axisNames[NUM_AXIS] = {
    'x', 'y', 'z', 'a'
};

typedef enum {
    SS_OFF,
    SS_WAIT_BRAKE_OFF,
    SS_WAIT_ON,
    SS_ON,
    SS_WAIT_OFF,
    SS_WAIT_BRAKE_ON,
} SpindleState;

typedef struct {
    // Parameters.
    hal_u32_t                           ampReadyDelay;
    hal_u32_t                           brakeOnDelay;
    hal_u32_t                           brakeOffDelay;
    hal_u32_t                           spindleLoToHi;
    hal_float_t                         jogScale[NUM_JOG_SEL];

    // Pins.
    hal_bit_t                           *pCycleStartIn;
    hal_bit_t                           *pCycleHoldIn;
    hal_bit_t                           *pFeedHoldOut;
    hal_float_t                         *pAdaptiveFeedIn;
    hal_float_t                         *pAdaptiveFeedOut;
    hal_bit_t                           *pToolChangeIn;
    hal_bit_t                           *pToolChangedOut;
    hal_bit_t                           *pWaitUserOut;

    hal_bit_t                           *pLimitOverrideIn;
    hal_bit_t                           *pLimitActiveOut;
    Limit                               xLimit;
    Limit                               yLimit;
    hal_bit_t                           *pZJogEnIn;
    hal_bit_t                           *pZLimitPosIn;
    hal_bit_t                           *pZLimitNegIn;
    hal_bit_t                           *pZLimitPosOut;
    hal_bit_t                           *pZLimitNegOut;

    Amp                                 amps[NUM_AXIS];

    hal_float_t                         *pSpindleSpeedIn;
    hal_bit_t                           *pSpindleIsOnIn;
    hal_bit_t                           *pSpindleFwdOut;
    hal_bit_t                           *pSpindleRevOut;
    hal_bit_t                           *pSpindleIncIn;
    hal_bit_t                           *pSpindleDecIn;
    hal_bit_t                           *pSpindleIncOut;
    hal_bit_t                           *pSpindleDecOut;
    hal_bit_t                           *pBrakeEnIn;
    hal_bit_t                           *pBrakeEnOut;

    hal_bit_t                           *pJogSelIn[NUM_JOG_SEL];
    hal_float_t                         *pJogScaleOut;

    // Private data.
    SpindleState                        spindleState;
    Timer                               spindleTimer;
    hal_bit_t                           lastCycleStart;
    BOOL                                riseCycleStart;
} Device;


// These methods are used for initialization.
static int Device_Init(Device *this);
static int Device_Export(Device *this, int compId, int id);
static int Device_ExportFeed(Device *this, int compId, int id, char *name);
static int Device_ExportLimits(Device *this, int compId, int id, char *name);
static int Device_ExportAmps(Device *this, int compId, int id, char *name);
static int Device_ExportSpindle(Device *this, int compId, int id, char *name);
static int Device_ExportJog(Device *this, int compId, int id, char *name);

// These methods are exported to the HAL.
static void Device_Refresh(void *this, long period);

// Private helper methods.
static void Device_RefreshFeed(Device *this, long period);
static void Device_RefreshLimits(Device *this, long period);
static void Device_RefreshAmps(Device *this, long period);
static void Device_RefreshSpindle(Device *this, long period);
static void Device_RefreshJog(Device *this, long period);


/******************************************************************************
 * DRIVER OBJECT
 *
 * This object contains all the data for this HAL component.
 *
 ******************************************************************************/

#define MAX_DEVICES                     4

typedef struct {
    int                                 compId;     // HAL component ID.
    Device                              *deviceTable[MAX_DEVICES];
} Driver;

static Driver                           driver;


/******************************************************************************
 * INIT AND EXIT CODE
 ******************************************************************************/

int
rtapi_app_main(void)
{
    int                                 i;
    Device                              *pDevice;

    // Connect to the HAL.
    driver.compId = hal_init("boss_plc");
    if (driver.compId < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "BOSS_PLC: ERROR: hal_init() failed\n");
        return(-1);
    }

    for(i = 0; i < MAX_DEVICES; i++){
        driver.deviceTable[i] = NULL;
    }

    if(count > MAX_DEVICES)
        count = MAX_DEVICES;

    for(i = 0; i < count; i++){
        // Allocate memory for device object.
        pDevice = hal_malloc(sizeof(Device));

        if (pDevice == NULL) {
            rtapi_print_msg(RTAPI_MSG_ERR, "BOSS_PLC: ERROR: hal_malloc() failed\n");
            hal_exit(driver.compId);
            return(-1);
        }

        // Save pointer to device object.
        driver.deviceTable[i] = pDevice;

        // Initialize device.
        if(Device_Init(pDevice)){
            hal_exit(driver.compId);
            return(-1);
        }

        // Export pins, parameters, and functions.
        if(Device_Export(pDevice, driver.compId, i)){
            hal_exit(driver.compId);
            return(-1);
        }
    }

    hal_ready(driver.compId);
    return(0);
}


void
rtapi_app_exit(void)
{
    int                                 i;
    Device                              *pDevice;

    hal_exit(driver.compId);

    for(i = 0; i < MAX_DEVICES; i++){
        if((pDevice = driver.deviceTable[i]) != NULL){
            // TODO: Free device object when HAL supports free.
//            hal_free(pDevice);
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
Device_Init(Device *this)
{
    int                                 i;

    // Initialize variables.
    this->spindleState = SS_OFF;
    this->lastCycleStart = 1;
    this->riseCycleStart = FALSE;

    // Initialize parameters.
    this->brakeOffDelay = 500;
    this->brakeOnDelay = 300;
    this->ampReadyDelay = 50;
    this->spindleLoToHi = 500;

    this->jogScale[0] = 0.0001;
    for(i = 1; i < NUM_JOG_SEL; i++){
        this->jogScale[i] = this->jogScale[i-1] * 10;
    }

    // Initialize timer.
    Timer_Init(&this->spindleTimer);

    // Initialize limits.
    Limit_Init(&this->xLimit);
    Limit_Init(&this->yLimit);

    // Initialize amps.
    for(i = 0; i < NUM_AXIS; i++){
        Amp_Init(&this->amps[i]);
    }

    return(0);
}


static int
Device_Export(Device *this, int compId, int id)
{
    int                                 msgLevel, error;
    char                                name[HAL_NAME_LEN + 2];

    // This function exports a lot of stuff, which results in a lot of
    // logging if msg_level is at INFO or ALL. So we save the current value
    // of msg_level and restore it later.  If you actually need to log this
    // function's actions, change the second line below.
    msgLevel = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    // Export pins and parameters.
    error = Device_ExportFeed(this, compId, id, name);

    if(!error){
        error = Device_ExportLimits(this, compId, id, name);
    }

    if(!error){
        error = Device_ExportAmps(this, compId, id, name);
    }

    if(!error){
        error = Device_ExportSpindle(this, compId, id, name);
    }
    if(!error){
        error = Device_ExportJog(this, compId, id, name);
    }

    // Export functions.
    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.refresh", id);
        error = hal_export_funct(name, Device_Refresh, this, 1, 0, compId);
    }

    // Restore saved message level.
    rtapi_set_msg_level(msgLevel);

    return(error);
}


static int
Device_ExportFeed(Device *this, int compId, int id, char *name)
{
    int                                 error;

    // Export pins.
    rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.cycle-start-in", id);
    error = hal_pin_bit_new(name, HAL_IN, &this->pCycleStartIn, compId);

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.cycle-hold-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &this->pCycleHoldIn, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.feed-hold-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &this->pFeedHoldOut, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.adaptive-feed-in", id);
        error = hal_pin_float_new(name, HAL_IN, &this->pAdaptiveFeedIn, compId);
    }

    if(!error){
        *this->pAdaptiveFeedIn = 1.0;
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.adaptive-feed-out", id);
        error = hal_pin_float_new(name, HAL_OUT, &this->pAdaptiveFeedOut, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.tool-change-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &this->pToolChangeIn, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.tool-changed-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &this->pToolChangedOut, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.wait-user-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &this->pWaitUserOut, compId);
    }

    return(error);
}


static int
Device_ExportLimits(Device *this, int compId, int id, char *name)
{
    int                                 error;

    // Export pins.
    rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.limit-override-in", id);
    error = hal_pin_bit_new(name, HAL_IN, &this->pLimitOverrideIn, compId);

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.limit-active-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &this->pLimitActiveOut, compId);
    }

    if(!error){
        error = Limit_Export(&this->xLimit, compId, id, name, axisNames[0]);
    }

    if(!error){
        error = Limit_Export(&this->yLimit, compId, id, name, axisNames[1]);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.%c-limit-pos-in", id, axisNames[2]);
        error = hal_pin_bit_new(name, HAL_IN, &this->pZLimitPosIn, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.%c-jog-en-in", id, axisNames[2]);
        error = hal_pin_bit_new(name, HAL_IN, &this->pZJogEnIn, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.%c-limit-neg-in", id, axisNames[2]);
        error = hal_pin_bit_new(name, HAL_IN, &this->pZLimitNegIn, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.%c-limit-pos-out", id, axisNames[2]);
        error = hal_pin_bit_new(name, HAL_OUT, &this->pZLimitPosOut, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.%c-limit-neg-out", id, axisNames[2]);
        error = hal_pin_bit_new(name, HAL_OUT, &this->pZLimitNegOut, compId);
    }

    return(error);
}


static int
Device_ExportAmps(Device *this, int compId, int id, char *name)
{
    int                                 error, i;
    Amp                                 *pAmp;

    pAmp = this->amps;
    for(i = 0, error = 0; i < NUM_AXIS && !error; i++, pAmp++){
        error = Amp_Export(pAmp, compId, id, name, axisNames[i]);
    }

    return(error);
}


static int
Device_ExportSpindle(Device *this, int compId, int id, char *name)
{
    int                                 error;

    // Export parameters.
    rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.brake-on-delay", id);
    error = hal_param_u32_new(name, HAL_RW, &this->brakeOnDelay, compId);

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.brake-off-delay", id);
        error = hal_param_u32_new(name, HAL_RW, &this->brakeOffDelay, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-lo-to-hi", id);
        error = hal_param_u32_new(name, HAL_RW, &this->spindleLoToHi, compId);
    }

    // Export pins.
    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-speed-in", id);
        error = hal_pin_float_new(name, HAL_IN, &this->pSpindleSpeedIn, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-is-on-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &this->pSpindleIsOnIn, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-fwd-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &this->pSpindleFwdOut, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-rev-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &this->pSpindleRevOut, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-inc-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &this->pSpindleIncIn, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-dec-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &this->pSpindleDecIn, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-inc-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &this->pSpindleIncOut, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-dec-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &this->pSpindleDecOut, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.brake-en-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &this->pBrakeEnIn, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.brake-en-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &this->pBrakeEnOut, compId);
    }

    return(error);
}


static int
Device_ExportJog(Device *this, int compId, int id, char *name)
{
    int                                 error, i;

    // Export parameters.
    for(i = 0, error = 0; i < NUM_JOG_SEL && !error; i++){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.jog-scale-%d-in", id, i);
        error = hal_param_float_new(name, HAL_RW, &this->jogScale[i], compId);
    }

    if(!error){
        for(i = 0; i < NUM_JOG_SEL && !error; i++){
            rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.jog-sel-in-%d", id, i);
            error = hal_pin_bit_new(name, HAL_IN, &this->pJogSelIn[i], compId);
        }
    }

    // Export pins.
    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.jog-scale-out", id);
        error = hal_pin_float_new(name, HAL_OUT, &this->pJogScaleOut, compId);
    }

    return(error);
}


/*
 * HAL EXPORTED FUNCTIONS
 */

static void
Device_Refresh(void *arg, long period)
{
    Device                              *this = (Device *)arg;

    Device_RefreshFeed(this, period);
    Device_RefreshLimits(this, period);
    Device_RefreshAmps(this, period);
    Device_RefreshSpindle(this, period);
    Device_RefreshJog(this, period);
}


static void
Device_RefreshFeed(Device *this, long period)
{
    this->riseCycleStart = (this->lastCycleStart==0) && (*this->pCycleStartIn==1);
    this->lastCycleStart = *this->pCycleStartIn;

    // Condition feed hold so machine waits for cycle start and spindle to be
    // running if it is enabled.
    *this->pFeedHoldOut = *this->pCycleHoldIn
                            || (*this->pSpindleSpeedIn && !*this->pSpindleIsOnIn)
                            || (*this->pFeedHoldOut && !this->riseCycleStart);

    // Limit rapid/feed to 1% when limits are being overriden.
    if(*this->pLimitOverrideIn && (*this->pAdaptiveFeedIn > 0.01))
        *this->pAdaptiveFeedOut = 0.01;
    else
        *this->pAdaptiveFeedOut = *this->pAdaptiveFeedIn;

    // Wait for cycle start to acknowledge tool change.
    *this->pToolChangedOut = (*this->pToolChangeIn && this->riseCycleStart)
                            || (*this->pToolChangedOut && *this->pToolChangeIn);

    // Indicates waiting for user to press cycle start.
    *this->pWaitUserOut = *this->pFeedHoldOut
                            || (*this->pToolChangeIn && !*this->pToolChangedOut);
}


static void
Device_RefreshLimits(Device *this, long period)
{
    Limit_Refresh(&this->xLimit, *this->pLimitOverrideIn);
    Limit_Refresh(&this->yLimit, *this->pLimitOverrideIn);

    // Condition Z limits with override in manual mode.
    *this->pZLimitPosOut = *this->pZLimitPosIn
                            && !(*this->pZJogEnIn && *this->pLimitOverrideIn);
    *this->pZLimitNegOut = *this->pZLimitNegIn
                            && !(*this->pZJogEnIn && *this->pLimitOverrideIn);

    // Generate limit active signal for pilot lamp.
    *this->pLimitActiveOut = Limit_IsActive(&this->xLimit)
                            || Limit_IsActive(&this->yLimit)
                            || *this->pZLimitPosIn || *this->pZLimitNegIn;
}


static void
Device_RefreshAmps(Device *this, long period)
{
    int                                 i;
    Amp                                 *pAmp;

    pAmp = this->amps;
    for(i = 0; i < NUM_AXIS; i++, pAmp++){
        Amp_Refresh(pAmp, period, this->ampReadyDelay);
    }
}

static void
Device_RefreshSpindle(Device *this, long period)
{
    switch(this->spindleState){
    // Spindle is off, brake is on.
    case SS_OFF:
        if(!*this->pBrakeEnIn){
            this->spindleState = SS_WAIT_BRAKE_OFF;
            *this->pBrakeEnOut = 0;
            Timer_SetTimeout(&this->spindleTimer, this->brakeOffDelay);
            Timer_Enable(&this->spindleTimer, TM_ONE_SHOT);
        }
        break;

    // Spindle is off, brake has been turned off. Wait at least a brake off
    // delay before turning spindle on.
    case SS_WAIT_BRAKE_OFF:
        if(*this->pBrakeEnIn){
            this->spindleState = SS_OFF;
            *this->pBrakeEnOut = 1;
            Timer_Disable(&this->spindleTimer);

        }else if((*this->pSpindleSpeedIn != 0.0)
                 && !Timer_IsEnabled(&this->spindleTimer)){

            this->spindleState = SS_WAIT_ON;

            *this->pSpindleFwdOut = *this->pSpindleSpeedIn > this->spindleLoToHi
                                    || (*this->pSpindleSpeedIn < 0.0
                                      && *this->pSpindleSpeedIn >= -this->spindleLoToHi);

            *this->pSpindleRevOut = *this->pSpindleSpeedIn < -this->spindleLoToHi
                                    || (*this->pSpindleSpeedIn > 0.0
                                      && *this->pSpindleSpeedIn <= this->spindleLoToHi);
        }
        break;

    // Spindle has been turned on. Wait for confirmation that it is running.
    case SS_WAIT_ON:
        if(*this->pSpindleIsOnIn){
            this->spindleState = SS_ON;

        }else if(*this->pSpindleSpeedIn == 0.0){
            this->spindleState = SS_WAIT_BRAKE_OFF;

            *this->pSpindleFwdOut = 0;
            *this->pSpindleRevOut = 0;
        }
        break;

    // Spindle is running.
    case SS_ON:
        if(*this->pSpindleSpeedIn == 0.0){
            this->spindleState = SS_WAIT_OFF;

            *this->pSpindleFwdOut = 0;
            *this->pSpindleRevOut = 0;
        }
        break;

    // Spindle has been turned off. Wait for confirmation.
    case SS_WAIT_OFF:
        if(!*this->pSpindleIsOnIn){
            this->spindleState = SS_WAIT_BRAKE_ON;

            Timer_SetTimeout(&this->spindleTimer, this->brakeOnDelay);
            Timer_Enable(&this->spindleTimer, TM_ONE_SHOT);
        }
        break;

    case SS_WAIT_BRAKE_ON:
        if(!Timer_IsEnabled(&this->spindleTimer)){
            this->spindleState = SS_WAIT_BRAKE_OFF;
        }
        break;

    default:
        this->spindleState = SS_WAIT_OFF;

        *this->pSpindleFwdOut = 0;
        *this->pSpindleRevOut = 0;
    }

    // Condition spindle increase and decrease so they are disabled when
    // spindle is not running and both cannot be enabled at the same time.
    *this->pSpindleIncOut = *this->pSpindleIncIn && !*this->pSpindleDecIn
                            && (this->spindleState == SS_ON);
    *this->pSpindleDecOut = *this->pSpindleDecIn && !*this->pSpindleIncIn
                            && (this->spindleState == SS_ON);
}


static void
Device_RefreshJog(Device *this, long period)
{
    int                                 i;

    // Jog scale.
    for(i = 0; i < NUM_JOG_SEL; i++){
        if(*this->pJogSelIn[i]){
            *this->pJogScaleOut = this->jogScale[i];
            break;
        }
    }
}


/******************************************************************************
 * LIMIT OBJECT FUNCTION DEFINITIONS
 ******************************************************************************/

static int
Limit_Export(Limit *this, int compId, int id, char *name, char axis)
{
    int                                 error;

    rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.%c-position-in", id, axis);
    error = hal_pin_float_new(name, HAL_IN, &this->pPositionIn, compId);

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.%c-jog-en-in", id, axis);
        error = hal_pin_bit_new(name, HAL_IN, &this->pJogEnIn, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.%c-limit-in", id, axis);
        error = hal_pin_bit_new(name, HAL_IN, &this->pIn, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.%c-limit-pos-out", id, axis);
        error = hal_pin_bit_new(name, HAL_OUT, &this->pPosOut, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.%c-limit-neg-out", id, axis);
        error = hal_pin_bit_new(name, HAL_OUT, &this->pNegOut, compId);
    }

    return(error);
}


static void
Limit_Init(Limit *this)
{
    this->state = LS_INIT;
}


static BOOL
Limit_IsActive(Limit *this)
{
    return(*this->pIn);
}

static void
Limit_Refresh(Limit *this, hal_bit_t override)
{
    switch(this->state){
    case LS_INIT:
    default:
        this->state = LS_ON_LIMIT;
        this->limitNeg = this->limitPos = 1;
        this->position = *this->pPositionIn;
        // Fall through.

    case LS_ON_LIMIT:
        if(*this->pIn == 0){
            this->limitNeg = this->limitPos = 0;

            if(*this->pPositionIn == this->position)
                this->state = LS_NO_MOTION;
            else if(*this->pPositionIn > this->position)
                this->state = LS_POS_MOTION;
            else if(*this->pPositionIn < this->position)
                this->state = LS_NEG_MOTION;
        }
        break;

    case LS_NO_MOTION:
        if(*this->pIn){
            this->state = LS_ON_LIMIT;
            this->limitNeg = this->limitPos = 1;
        }else if(*this->pPositionIn > this->position){
            this->state = LS_POS_MOTION;
        }else if(*this->pPositionIn < this->position){
            this->state = LS_NEG_MOTION;
        }
        break;

    case LS_POS_MOTION:
        if(*this->pIn){
            this->state = LS_ON_LIMIT;
            this->limitPos = 1;
        }else if(*this->pPositionIn == this->position){
            this->state = LS_NO_MOTION;
        }else if(*this->pPositionIn < this->position){
            this->state = LS_NEG_MOTION;
        }
        break;

    case LS_NEG_MOTION:
        if(*this->pIn){
            this->state = LS_ON_LIMIT;
            this->limitNeg = 1;
        }else if(*this->pPositionIn == this->position){
            this->state = LS_NO_MOTION;
        }else if(*this->pPositionIn > this->position){
            this->state = LS_POS_MOTION;
        }
        break;
    }

    this->position = *this->pPositionIn;

    // Condition limits with override in manual mode.
    *this->pPosOut = this->limitPos && !(*this->pJogEnIn && override);
    *this->pNegOut = this->limitNeg && !(*this->pJogEnIn && override);
}


/******************************************************************************
 * AMP OBJECT FUNCTION DEFINITIONS
 ******************************************************************************/

static int
Amp_Export(Amp *this, int compId, int id, char *name, char axis)
{
    int                                 error;

    rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.%c-enable-in", id, axis);
    error = hal_pin_bit_new(name, HAL_IN, &this->pEnableIn, compId);

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.%c-ready-in", id, axis);
        error = hal_pin_bit_new(name, HAL_IN, &this->pReadyIn, compId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.%c-fault-out", id, axis);
        error = hal_pin_bit_new(name, HAL_OUT, &this->pFaultOut, compId);
    }

    return(error);
}


static void
Amp_Init(Amp *this)
{
    // Initialize variables.
    this->lastEnable = 0;

    // Initialize timer.
    Timer_Init(&this->timer);
}


static void
Amp_Refresh(Amp *this, long period, hal_u32_t readyDelay)
{
    Timer_Update(&this->timer, period);

    if(*this->pEnableIn){
        if(!this->lastEnable){
            Timer_SetTimeout(&this->timer, readyDelay);
            Timer_Enable(&this->timer, TM_ONE_SHOT);
        }
    } else {
        Timer_Disable(&this->timer);
    }

    *this->pFaultOut = *this->pEnableIn && !*this->pReadyIn
                        && !Timer_IsEnabled(&this->timer);

    this->lastEnable = *this->pEnableIn;
}


/******************************************************************************
 * TIMER OBJECT FUNCTION DEFINITIONS
 ******************************************************************************/

static void
Timer_Init(Timer *this)
{
    this->enabled = FALSE;
    this->pTimeout = NULL;
}

static void
Timer_Enable(Timer *this, TimerMode mode)
{
    this->mode = mode;
    this->enabled = TRUE;
    this->count = 0;
}


static void
Timer_Disable(Timer *this)
{
    this->enabled = FALSE;
}


static BOOL
Timer_IsEnabled(Timer *this)
{
    return(this->enabled);
}


static void
Timer_Update(Timer *this, long period)
{
    if(!this->enabled)
        return;

    this->nSec += period;

    if(this->nSec > 1000000){
        this->count += this->nSec / 1000000;
        this->nSec %= 1000000;
    }

    if(this->count >= this->timeout){
        if(this->pTimeout != NULL)
            this->pTimeout(this->pArgs);

        if(this->mode != TM_CONTINUOUS){
            this->enabled = FALSE;
        }else{
            this->count -= this->timeout;
        }
    }
}


static void
Timer_SetTimeout(Timer *this, hal_u32_t timeout)
{
    this->count = 0;
    this->timeout = timeout;
}


#if 0
static void
Timer_SetCallback(Timer *this, TIMER_ROUTINE pCallback, void *pArgs)
{
    this->pTimeout = pCallback;
    this->pArgs = pArgs;
}
#endif
