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
 *        u32       boss_plc.<id>.brake-on-delay;
 *        u32       boss_plc.<id>.brake-off-delay;
 *
 *  Pins:
 *        bit       boss_plc.<id>.cycle-start-in;
 *        bit       boss_plc.<id>.cycle-hold-in;
 *        bit       boss_plc.<id>.feed-hold-out;
 *
 *        float     boss_plc.<id>.rapid-override-in;
 *        float     boss_plc.<id>.feed-override-in;
 *        u32       boss_plc.<id>.motion-type-in;
 *        float     boss_plc.<id>.adaptive-feed-out;
 *
 *        bit       boss_plc.<id>.limit-overridein;
 *        float     boss_plc.<id>.xposition-in;
 *        bit       boss_plc.<id>.xjog-en-in;
 *        bit       boss_plc.<id>.xlimit-in;
 *        bit       boss_plc.<id>.xlimit-pos-out;
 *        bit       boss_plc.<id>.xlimit-neg-out;
 *        float     boss_plc.<id>.yposition-in;
 *        bit       boss_plc.<id>.yjog-en-in;
 *        bit       boss_plc.<id>.ylimit-in;
 *        bit       boss_plc.<id>.ylimit-pos-out;
 *        bit       boss_plc.<id>.ylimit-neg-out;
 *        bit       boss_plc.<id>.zjog-en-in;
 *        bit       boss_plc.<id>.zlimit-pos-in;
 *        bit       boss_plc.<id>.zlimit-neg-in;
 *        bit       boss_plc.<id>.zlimit-pos-out;
 *        bit       boss_plc.<id>.zlimit-neg-out;
 *
 *        bit       boss_plc.<id>.spindle-fwd-in;
 *        bit       boss_plc.<id>.spindle-rev-in;
 *        bit       boss_plc.<id>.spindle-fwd-out;
 *        bit       boss_plc.<id>.spindle-rev-out;
 *        bit       boss_plc.<id>.spindle-stat-in;
 *        bit       boss_plc.<id>.spindle-inc-in;
 *        bit       boss_plc.<id>.spindle-dec-in;
 *        bit       boss_plc.<id>.spindle-inc-out;
 *        bit       boss_plc.<id>.spindle-dec-out;
 *        bit       boss_plc.<id>.brake-en-in;
 *        bit       boss_plc.<id>.brake-en-out;
 *
 *   Functions:
 *        void      boss_plc.<id>.read
 *        void      boss_plc.<id>.update
 *        void      boss_plc.<id>.write
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

typedef void                            (*TIMER_ROUTINE)(void *pArgs);

typedef struct {
    // Private data.
    BOOL                                enabled;
    hal_u32_t                           nSec;
    hal_u32_t                           count;
    hal_u32_t                           timeout;
    TIMER_ROUTINE                       pTimeout;
    void                                *pArgs;
} Timer;

static void Timer_Init(Timer *this);
static void Timer_Enable(Timer *this);
static void Timer_Disable(Timer *this);
static BOOL Timer_IsEnabled(Timer *this);
static void Timer_Update(Timer *this, long period);
static void Timer_SetTimeout(Timer *this, hal_u32_t timeout);
static void Timer_SetCallback(Timer *this, TIMER_ROUTINE pCallback, void *pArgs);


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

static void Limit_Init(Limit *this);
static void Limit_Refresh(Limit *this, hal_bit_t override);


/******************************************************************************
 * DEVICE OBJECT
 *
 * This object contains all the data for one PLC. A device object is
 * dynamically allocated in shmem for each PLC during initialization.
 *
 ******************************************************************************/

typedef struct {
    // Parameters.
    hal_u32_t                           brakeOnDelay;
    hal_u32_t                           brakeOffDelay;

    // Pins.
    hal_bit_t                           *pCycleStartIn;
    hal_bit_t                           *pCycleHoldIn;
    hal_bit_t                           *pFeedHoldOut;
    hal_float_t                         *pRapidOverrideIn;
    hal_float_t                         *pFeedOverrideIn;
    hal_u32_t                           *pMotionTypeIn;
    hal_float_t                         *pAdaptiveFeedOut;

    hal_bit_t                           *pLimitOverrideIn;
    Limit                               xLimit;
    Limit                               yLimit;
    hal_bit_t                           *pZJogEnIn;
    hal_bit_t                           *pZLimitPosIn;
    hal_bit_t                           *pZLimitNegIn;
    hal_bit_t                           *pZLimitPosOut;
    hal_bit_t                           *pZLimitNegOut;

    hal_bit_t                           *pSpindleFwdIn;
    hal_bit_t                           *pSpindleRevIn;
    hal_bit_t                           *pSpindleFwdOut;
    hal_bit_t                           *pSpindleRevOut;
    hal_bit_t                           *pSpindleIsOnIn;
    hal_bit_t                           *pSpindleIncIn;
    hal_bit_t                           *pSpindleDecIn;
    hal_bit_t                           *pSpindleIncOut;
    hal_bit_t                           *pSpindleDecOut;
    hal_bit_t                           *pBrakeEnIn;
    hal_bit_t                           *pBrakeEnOut;

    // Private data.
    Timer                               brakeOnTimer;
    Timer                               brakeOffTimer;
    hal_bit_t                           delayedBrakeEn;
} Device;


// These methods are used for initialization.
static int Device_Init(Device *this);
static int Device_ExportPinsParametersFunctions(Device *this, int componentId, int boardId);

// These methods are exported to the HAL.
static void Device_Refresh(void *this, long period);

// Private helper methods.
static void Device_RefreshSpindle(Device *this, long period);
static void Device_RefreshFeed(Device *this, long period);
static void Device_RefreshLimits(Device *this, long period);
static void Device_BrakeOnTimeout(void *this);
static void Device_BrakeOffTimeout(void *this);


/******************************************************************************
 * DRIVER OBJECT
 *
 * This object contains all the data for this HAL component.
 *
 ******************************************************************************/

#define MAX_DEVICES                     4

typedef struct {
    int                                 componentId;        // HAL component ID.
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
    driver.componentId = hal_init("boss_plc");
    if (driver.componentId < 0) {
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
            hal_exit(driver.componentId);
            return(-1);
        }

        // Save pointer to device object.
        driver.deviceTable[i] = pDevice;

        // Initialize device.
        if(Device_Init(pDevice)){
            hal_exit(driver.componentId);
            return(-1);
        }

        // Export pins, parameters, and functions.
        if(Device_ExportPinsParametersFunctions(pDevice, driver.componentId, i)){
            hal_exit(driver.componentId);
            return(-1);
        }
    }

    hal_ready(driver.componentId);
    return(0);
}


void
rtapi_app_exit(void)
{
    int                                 i;
    Device                              *pDevice;

    hal_exit(driver.componentId);

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
    Timer_Init(&this->brakeOnTimer);
    Timer_SetCallback(&this->brakeOnTimer, Device_BrakeOnTimeout, this);
    Timer_SetTimeout(&this->brakeOnTimer, this->brakeOnDelay);

    Timer_Init(&this->brakeOffTimer);
    Timer_SetCallback(&this->brakeOffTimer, Device_BrakeOffTimeout, this);
    Timer_SetTimeout(&this->brakeOffTimer, this->brakeOffDelay);

    Limit_Init(&this->xLimit);
    Limit_Init(&this->yLimit);

    return(0);
}


static int
Device_ExportPinsParametersFunctions(Device *this, int componentId, int id)
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
    rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.brake-on-delay", id);
    error = hal_param_u32_new(name, HAL_RW, &(this->brakeOnDelay), componentId);

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.brake-off-delay", id);
        error = hal_param_u32_new(name, HAL_RW, &(this->brakeOffDelay), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.cycle-start-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->pCycleStartIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.cycle-hold-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->pCycleHoldIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.feed-hold-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &(this->pFeedHoldOut), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.rapid-override-in", id);
        error = hal_pin_float_new(name, HAL_IN, &(this->pRapidOverrideIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.feed-override-in", id);
        error = hal_pin_float_new(name, HAL_IN, &(this->pFeedOverrideIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.motion-type-in", id);
        error = hal_pin_u32_new(name, HAL_IN, &(this->pMotionTypeIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.adaptive-feed-out", id);
        error = hal_pin_float_new(name, HAL_OUT, &(this->pAdaptiveFeedOut), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.limit-override-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->pLimitOverrideIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.xposition-in", id);
        error = hal_pin_float_new(name, HAL_IN, &(this->xLimit.pPositionIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.xjog-en-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->xLimit.pJogEnIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.xlimit-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->xLimit.pIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.xlimit-pos-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &(this->xLimit.pPosOut), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.xlimit-neg-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &(this->xLimit.pNegOut), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.yposition-in", id);
        error = hal_pin_float_new(name, HAL_IN, &(this->yLimit.pPositionIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.yjog-en-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->yLimit.pJogEnIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.ylimit-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->yLimit.pIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.ylimit-pos-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &(this->yLimit.pPosOut), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.ylimit-neg-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &(this->yLimit.pNegOut), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.zlimit-pos-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->pZLimitPosIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.zjog-en-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->pZJogEnIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.zlimit-neg-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->pZLimitNegIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.zlimit-pos-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &(this->pZLimitPosOut), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.zlimit-neg-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &(this->pZLimitNegOut), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-fwd-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->pSpindleFwdIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-rev-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->pSpindleRevIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-fwd-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &(this->pSpindleFwdOut), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-rev-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &(this->pSpindleRevOut), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-is-on-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->pSpindleIsOnIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-inc-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->pSpindleIncIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-dec-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->pSpindleDecIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-inc-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &(this->pSpindleIncOut), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.spindle-dec-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &(this->pSpindleDecOut), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.brake-en-in", id);
        error = hal_pin_bit_new(name, HAL_IN, &(this->pBrakeEnIn), componentId);
    }

    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.brake-en-out", id);
        error = hal_pin_bit_new(name, HAL_OUT, &(this->pBrakeEnOut), componentId);
    }

    // Export functions.
    if(!error){
        rtapi_snprintf(name, HAL_NAME_LEN, "boss_plc.%d.refresh", id);
        error = hal_export_funct(name, Device_Refresh, this, 1, 0, componentId);
    }

    // Restore saved message level.
    rtapi_set_msg_level(msgLevel);

    return(error);
}


/*
 * HAL EXPORTED FUNCTIONS
 */

static void
Device_Refresh(void *arg, long period)
{
    Device                              *this = (Device *)arg;

    Device_RefreshSpindle(this, period);
    Device_RefreshFeed(this, period);
    Device_RefreshLimits(this, period);
}


static void
Device_RefreshSpindle(Device *this, long period)
{
    // Update timers.
    Timer_Update(&this->brakeOnTimer, period);
    Timer_Update(&this->brakeOffTimer, period);

    // Generate a delayed brake enable.
    if(*this->pBrakeEnIn != *this->pBrakeEnOut){
        // Start the on or off timer.
        if(*this->pBrakeEnIn){
            Timer_Enable(&this->brakeOnTimer);
        }else{
            Timer_Enable(&this->brakeOffTimer);
        }
    }else{
        // If brake enable glitched on/off, ignore it as far as the brake is
        // concerned. However, on glitches will still stop the spindle.
        Timer_Disable(&this->brakeOnTimer);
        Timer_Disable(&this->brakeOffTimer);
    }

    // Brake turns off immediately, but on is delayed. This should never
    // happen, but make sure the brake is off if the spindle is on.
    if(!*this->pBrakeEnIn || *this->pSpindleIsOnIn)
        *this->pBrakeEnOut = 0;
    else
        *this->pBrakeEnOut = this->delayedBrakeEn;

    // Condition spindle control so spindle won't start until cycle start
    // is pressed, brake on timer is not active, and brake off timer has
    // expired. Spindle will stop immediately if the brake is enabled.
    *this->pSpindleFwdOut = (*this->pSpindleFwdIn && !*this->pSpindleRevIn
                                && !Timer_IsEnabled(&this->brakeOnTimer)
                                && !this->delayedBrakeEn
                                && *this->pCycleStartIn)
                            || (*this->pSpindleFwdOut && !*this->pBrakeEnIn);
    *this->pSpindleRevOut = (*this->pSpindleRevIn && !*this->pSpindleFwdIn
                                && !Timer_IsEnabled(&this->brakeOnTimer)
                                && !this->delayedBrakeEn
                                && *this->pCycleStartIn)
                            || (*this->pSpindleRevOut && !*this->pBrakeEnIn);

    // Condition spindle increase and decrease so they are disabled when
    // spindle is not running and both cannot be enabled at the same time.
    *this->pSpindleIncOut = *this->pSpindleIncIn && !*this->pSpindleDecIn
                            && *this->pSpindleIsOnIn;
    *this->pSpindleDecOut = *this->pSpindleDecIn && !*this->pSpindleIncIn
                            && *this->pSpindleIsOnIn;
}


static void
Device_RefreshFeed(Device *this, long period)
{
    // Condition feed hold so machine waits for cycle start and spindle to be
    // running if it is enabled.
    *this->pFeedHoldOut = *this->pCycleHoldIn
                            || ((*this->pSpindleFwdIn || *this->pSpindleRevIn)
                                && !*this->pSpindleIsOnIn)
                            || (*this->pFeedHoldOut && !*this->pCycleStartIn);

    // Rapid/feed override. Need motion type indicator to have
    // separate overrides.
    *this->pAdaptiveFeedOut = (*this->pMotionTypeIn)? *this->pFeedOverrideIn
                                : *this->pRapidOverrideIn;

    // Limit rapid/feed to 1% when limits are being overriden.
    if(*this->pLimitOverrideIn && (*this->pAdaptiveFeedOut > 0.01))
        *this->pAdaptiveFeedOut = 0.01;
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
}


static void
Device_BrakeOnTimeout(void *arg)
{
    Device                              *this = (Device *)arg;

    Timer_Disable(&this->brakeOnTimer);
    this->delayedBrakeEn = 1;
}


static void
Device_BrakeOffTimeout(void *arg)
{
    Device                              *this = (Device *)arg;

    Timer_Disable(&this->brakeOffTimer);
    this->delayedBrakeEn = 0;
}


/******************************************************************************
 * LIMIT OBJECT FUNCTION DEFINITIONS
 ******************************************************************************/

static void
Limit_Init(Limit *this)
{
    this->state = LS_INIT;
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
 * TIMER OBJECT FUNCTION DEFINITIONS
 ******************************************************************************/

static void
Timer_Init(Timer *this)
{
    this->enabled = FALSE;
    this->pTimeout = NULL;
}

static void
Timer_Enable(Timer *this)
{
    if(!this->enabled){
        this->enabled = TRUE;
        this->count = 0;
    }
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
        this->count -= this->timeout;
    }
}


static void
Timer_SetTimeout(Timer *this, hal_u32_t timeout)
{
    this->count = 0;
    this->timeout = timeout;
}


static void
Timer_SetCallback(Timer *this, TIMER_ROUTINE pCallback, void *pArgs)
{
    this->pTimeout = pCallback;
    this->pArgs = pArgs;
}
