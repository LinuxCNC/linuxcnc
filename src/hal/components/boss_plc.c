/******************************************************************************
 *
 * Copyright (C) 2007 Peter G. Vavaroutsos <pete AT vavaroutsos DOT com>
 *
 *
 * This module is a hard coded PLC for use on a Bridgeport Boss.
 *
 * Installation of the component (realtime only):
 *
 * insmod boss_plc count=<1>
 *
 *
 * The following items are exported to the HAL. <id> is
 * the component id number and is formatted as "%d".
 *
 *  Pins (former parameters):
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
 *        bit       boss_plc.<id>.mist-on-in
 *        bit       boss_plc.<id>.mist-on-out
 *        bit       boss_plc.<id>.flood-on-in
 *        bit       boss_plc.<id>.flood-on-out
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


#include <rtapi.h>              // RTAPI realtime OS API.
#include <rtapi_app.h>          // RTAPI realtime module decls.
#include <hal.h>                // HAL public API decls.


#define TRUE                    1
#define FALSE                   0
typedef int                     BOOL;


// Module information.
MODULE_AUTHOR("Pete Vavaroutsos");
MODULE_DESCRIPTION("Bridgeport BOSS PLC for EMC HAL");
MODULE_LICENSE("GPL");
static unsigned long                    count = 1;
RTAPI_MP_LONG(count, "Number of BOSS PLCs to instance");
static int                              debug = 0;
RTAPI_MP_INT(debug, "Enables optional params");


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

typedef void                    (*TIMER_ROUTINE)(void *pArgs);

typedef struct {
    // Private data.
    BOOL                        enabled;
    rtapi_u32                   nSec;
    rtapi_u32                   count;
    rtapi_u32                   timeout;
    TIMER_ROUTINE               pTimeout;
    void                        *pArgs;
    TimerMode                   mode;
} Timer;

static void Timer_Init(Timer *this);
static void Timer_Enable(Timer *this, TimerMode mode);
static void Timer_Disable(Timer *this);
static BOOL Timer_IsEnabled(Timer *this);
static void Timer_Update(Timer *this, long period);
static void Timer_SetTimeout(Timer *this, rtapi_u32 timeout);
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
    hal_real_t                  pPositionIn;
    hal_bool_t                  pJogEnIn;
    hal_bool_t                  pIn;
    hal_bool_t                  pPosOut;
    hal_bool_t                  pNegOut;

    hal_uint_t                  stateDebug;  // Only available when debug is on

    // Internal data.
    LimitState                  state;
    rtapi_real                  position;
    rtapi_bool                  limitPos;
    rtapi_bool                  limitNeg;
} Limit;

static int Limit_Export(Limit *this, int compId, int id, char axis);
static void Limit_Init(Limit *this);
static BOOL Limit_IsActive(Limit *this);
static void Limit_Refresh(Limit *this, rtapi_bool override);


/******************************************************************************
 * AMP OBJECT
 *
 * This object creates the amp fault signal from the enable and ready signal.
 *
 ******************************************************************************/

typedef struct {
    // Exported pins.
    hal_bool_t                  pEnableIn;
    hal_bool_t                  pReadyIn;
    hal_bool_t                  pFaultOut;

    // Internal data.
    Timer                       timer;
    rtapi_bool                  lastEnable;
} Amp;

static int Amp_Export(Amp *this, int compId, int id, char axis);
static void Amp_Init(Amp *this);
static void Amp_Refresh(Amp *this, long period, rtapi_u32 readyDelay);


/******************************************************************************
 * PLC OBJECT
 *
 * This object contains all the data for one PLC. A component object is
 * dynamically allocated in shmem for each PLC during initialization.
 *
 ******************************************************************************/

#define NUM_JOG_SEL             3
#define NUM_AXIS                4

static char                     axisNames[NUM_AXIS] = {
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
    // Pins. (former parameters)
    hal_uint_t                  ampReadyDelay;
    hal_uint_t                  brakeOnDelay;
    hal_uint_t                  brakeOffDelay;
    hal_real_t                  spindleLoToHi;
    hal_real_t                  jogScale[NUM_JOG_SEL];

    // Pins.
    hal_bool_t                  pCycleStartIn;
    hal_bool_t                  pCycleHoldIn;
    hal_bool_t                  pFeedHoldOut;
    hal_real_t                  pAdaptiveFeedIn;
    hal_real_t                  pAdaptiveFeedOut;
    hal_bool_t                  pToolChangeIn;
    hal_bool_t                  pToolChangedOut;
    hal_bool_t                  pWaitUserOut;
    hal_bool_t                  pMistOnIn;
    hal_bool_t                  pMistOnOut;
    hal_bool_t                  pFloodOnIn;
    hal_bool_t                  pFloodOnOut;

    hal_bool_t                  pLimitOverrideIn;
    hal_bool_t                  pLimitActiveOut;
    Limit                       xLimit;
    Limit                       yLimit;
    hal_bool_t                  pZJogEnIn;
    hal_bool_t                  pZLimitPosIn;
    hal_bool_t                  pZLimitNegIn;
    hal_bool_t                  pZLimitPosOut;
    hal_bool_t                  pZLimitNegOut;

    Amp                         amps[NUM_AXIS];

    hal_real_t                  pSpindleSpeedIn;
    hal_bool_t                  pSpindleIsOnIn;
    hal_bool_t                  pSpindleFwdOut;
    hal_bool_t                  pSpindleRevOut;
    hal_bool_t                  pSpindleIncIn;
    hal_bool_t                  pSpindleDecIn;
    hal_bool_t                  pSpindleIncOut;
    hal_bool_t                  pSpindleDecOut;
    hal_bool_t                  pBrakeEnIn;
    hal_bool_t                  pBrakeEnOut;

    hal_bool_t                  pJogSelIn[NUM_JOG_SEL];
    hal_real_t                  pJogScaleOut;

    hal_uint_t                  spindleStateDebug;  // Only available when debug is on

    // Private data.
    SpindleState                spindleState;
    Timer                       spindleTimer;
    rtapi_real                  lastSpindleSpeed;
    rtapi_bool                  lastCycleStart;
} Plc;


// These methods are used for initialization.
static int Plc_Init(Plc *this);
static int Plc_Export(Plc *this, int compId, int id);
static int Plc_ExportFeed(Plc *this, int compId, int id);
static int Plc_ExportLimits(Plc *this, int compId, int id);
static int Plc_ExportAmps(Plc *this, int compId, int id);
static int Plc_ExportSpindle(Plc *this, int compId, int id);
static int Plc_ExportJog(Plc *this, int compId, int id);

// These methods are exported to the HAL.
static void Plc_Refresh(void *this, long period);

// Private helper methods.
static void Plc_RefreshFeed(Plc *this, long period);
static void Plc_RefreshLimits(Plc *this, long period);
static void Plc_RefreshAmps(Plc *this, long period);
static void Plc_RefreshSpindle(Plc *this, long period);
static void Plc_RefreshJog(Plc *this, long period);


/******************************************************************************
 * COMPONENT OBJECT
 *
 * This object contains all the data for this HAL component.
 *
 ******************************************************************************/

#define MAX_DEVICES             4

typedef struct {
    int                         id;             // HAL component ID.
    Plc                         *plcTable[MAX_DEVICES];
} Component;

static Component                component;


/******************************************************************************
 * INIT AND EXIT CODE
 ******************************************************************************/

int
rtapi_app_main(void)
{
    unsigned                    i;
    Plc                         *pComp;

    // Connect to the HAL.
    component.id = hal_init("boss_plc");
    if (component.id < 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "BOSS_PLC: ERROR: hal_init() failed\n");
        return(-1);
    }

    for(i = 0; i < MAX_DEVICES; i++){
        component.plcTable[i] = NULL;
    }

    if(count > MAX_DEVICES)
        count = MAX_DEVICES;

    for(i = 0; i < count; i++){
        // Allocate memory for device object.
        pComp = hal_malloc(sizeof(Plc));

        if (pComp == NULL) {
            rtapi_print_msg(RTAPI_MSG_ERR, "BOSS_PLC: ERROR: hal_malloc() failed\n");
            hal_exit(component.id);
            return(-1);
        }

        // Save pointer to device object.
        component.plcTable[i] = pComp;

        // Initialize device.
        if(Plc_Init(pComp)){
            hal_exit(component.id);
            return(-1);
        }

        // Export pins, parameters, and functions.
        if(Plc_Export(pComp, component.id, i)){
            hal_exit(component.id);
            return(-1);
        }
    }

    hal_ready(component.id);
    return(0);
}


void
rtapi_app_exit(void)
{
    int                         i;
    Plc                         *pComp;

    hal_exit(component.id);

    for(i = 0; i < MAX_DEVICES; i++){
        if((pComp = component.plcTable[i]) != NULL){
            // TODO: Free device object when HAL supports free.
//            hal_free(pComp);
        }
    }
}


/******************************************************************************
 * PLC OBJECT FUNCTION DEFINITIONS
 ******************************************************************************/

/*
 * LOCAL FUNCTIONS
 */

static int
Plc_Init(Plc *this)
{
    int                         i;

    // Initialize variables.
    this->spindleState = SS_OFF;
    this->lastSpindleSpeed = 0.0;
    this->lastCycleStart = 1;

    // Initialize parameters.
    hal_set_real(this->jogScale[0], 0.0001);
    for(i = 1; i < NUM_JOG_SEL; i++){
        hal_set_real(this->jogScale[i], hal_get_real(this->jogScale[i-1]) * 10);
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
Plc_Export(Plc *this, int compId, int id)
{
    int                         msgLevel, error;

    // This function exports a lot of stuff, which results in a lot of
    // logging if msg_level is at INFO or ALL. So we save the current value
    // of msg_level and restore it later.  If you actually need to log this
    // function's actions, change the second line below.
    msgLevel = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    // Export pins and parameters.
    error = Plc_ExportFeed(this, compId, id);

    if(!error){
        error = Plc_ExportLimits(this, compId, id);
    }

    if(!error){
        error = Plc_ExportAmps(this, compId, id);
    }

    if(!error){
        error = Plc_ExportSpindle(this, compId, id);
    }
    if(!error){
        error = Plc_ExportJog(this, compId, id);
    }

    // Export functions.
    if(!error){
        error = hal_export_functf(Plc_Refresh, this, 1, 0, compId, "boss_plc.%d.refresh", id);
    }

    // Restore saved message level.
    rtapi_set_msg_level(msgLevel);

    return(error);
}


static int
Plc_ExportFeed(Plc *this, int compId, int id)
{
    int                         error;

    // Export pins.
    error = hal_pin_new_bool(compId, HAL_IN, &this->pCycleStartIn, 0,
			     "boss_plc.%d.cycle-start-in", id);

    if(!error){
        error = hal_pin_new_bool(compId, HAL_IN, &this->pCycleHoldIn, 0,
				 "boss_plc.%d.cycle-hold-in", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pFeedHoldOut, 0,
				 "boss_plc.%d.feed-hold-out", id);
    }

    if(!error){
        error = hal_pin_new_real(compId, HAL_IN, &this->pAdaptiveFeedIn, 1.0,
				   "boss_plc.%d.adaptive-feed-in", id);
    }

    if(!error){
        error = hal_pin_new_real(compId, HAL_OUT, &this->pAdaptiveFeedOut, 0.0,
				   "boss_plc.%d.adaptive-feed-out", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_IN, &this->pToolChangeIn, 0,
				 "boss_plc.%d.tool-change-in", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pToolChangedOut, 0,
				 "boss_plc.%d.tool-changed-out", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pWaitUserOut, 0,
				 "boss_plc.%d.wait-user-out", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_IN, &this->pMistOnIn, 0,
				 "boss_plc.%d.mist-on-in", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pMistOnOut, 0,
				 "boss_plc.%d.mist-on-out", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_IN, &this->pFloodOnIn, 0,
				 "boss_plc.%d.flood-on-in", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pFloodOnOut, 0,
				 "boss_plc.%d.flood-on-out", id);
    }

    return(error);
}


static int
Plc_ExportLimits(Plc *this, int compId, int id)
{
    int                         error;

    // Export pins.
    error = hal_pin_new_bool(compId, HAL_IN, &this->pLimitOverrideIn, 0,
			     "boss_plc.%d.limit-override-in", id);

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pLimitActiveOut, 0,
				 "boss_plc.%d.limit-active-out", id);
    }

    if(!error){
        error = Limit_Export(&this->xLimit, compId, id, axisNames[0]);
    }

    if(!error){
        error = Limit_Export(&this->yLimit, compId, id, axisNames[1]);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_IN, &this->pZLimitPosIn, 0,
				 "boss_plc.%d.%c-limit-pos-in", id, axisNames[2]);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_IN, &this->pZJogEnIn, 0,
				 "boss_plc.%d.%c-jog-en-in", id, axisNames[2]);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_IN, &this->pZLimitNegIn, 0,
				 "boss_plc.%d.%c-limit-neg-in", id, axisNames[2]);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pZLimitPosOut, 0,
				 "boss_plc.%d.%c-limit-pos-out", id, axisNames[2]);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pZLimitNegOut, 0,
				 "boss_plc.%d.%c-limit-neg-out", id, axisNames[2]);
    }

    // Export optional parameters.
    if(debug > 0){
        if(!error){
            error = hal_param_new_ui32(compId, HAL_RO, &this->xLimit.stateDebug, 0,
				       "boss_plc.%d.%c-limit-state", id, axisNames[0]);
        }

        if(!error){
            error = hal_param_new_ui32(compId, HAL_RO, &this->yLimit.stateDebug, 0,
				       "boss_plc.%d.%c-limit-state", id, axisNames[1]);
        }
    }

    return(error);
}


static int
Plc_ExportAmps(Plc *this, int compId, int id)
{
    int                         error, i;
    Amp                         *pAmp;

    error = hal_pin_new_ui32(compId, HAL_IO, &this->ampReadyDelay, 50,
			     "boss_plc.%d.amp-ready-delay", id);

    pAmp = this->amps;
    for(i = 0; i < NUM_AXIS && !error; i++, pAmp++){
        error = Amp_Export(pAmp, compId, id, axisNames[i]);
    }

    return(error);
}


static int
Plc_ExportSpindle(Plc *this, int compId, int id)
{
    int                         error;

    // Export parameters.
    error = hal_pin_new_ui32(compId, HAL_IO, &this->brakeOnDelay, 300,
			     "boss_plc.%d.brake-on-delay", id);

    if(!error){
        error = hal_pin_new_ui32(compId, HAL_IO, &this->brakeOffDelay, 500,
				 "boss_plc.%d.brake-off-delay", id);
    }

    if(!error){
        error = hal_pin_new_real(compId, HAL_IO, &this->spindleLoToHi, 500,
				   "boss_plc.%d.spindle-lo-to-hi", id);
    }

    // Export optional parameters.
    if(debug > 0){
        if(!error){
            error = hal_param_new_ui32(compId, HAL_RO, &this->spindleStateDebug, 0,
				       "boss_plc.%d.spindle-state", id);
        }
    }

    // Export pins.
    if(!error){
        error = hal_pin_new_real(compId, HAL_IN, &this->pSpindleSpeedIn, 0.0,
				   "boss_plc.%d.spindle-speed-in", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_IN, &this->pSpindleIsOnIn, 0,
				 "boss_plc.%d.spindle-is-on-in", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pSpindleFwdOut, 0,
				 "boss_plc.%d.spindle-fwd-out", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pSpindleRevOut, 0,
				 "boss_plc.%d.spindle-rev-out", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_IN, &this->pSpindleIncIn, 0,
				 "boss_plc.%d.spindle-inc-in", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_IN, &this->pSpindleDecIn, 0,
				 "boss_plc.%d.spindle-dec-in", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pSpindleIncOut, 0,
				 "boss_plc.%d.spindle-inc-out", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pSpindleDecOut, 0,
				 "boss_plc.%d.spindle-dec-out", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_IN, &this->pBrakeEnIn, 0,
				 "boss_plc.%d.brake-en-in", id);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pBrakeEnOut, 0,
				 "boss_plc.%d.brake-en-out", id);
    }

    return(error);
}


static int
Plc_ExportJog(Plc *this, int compId, int id)
{
    int                         error, i;

    // Export parameters.
    for(i = 0, error = 0; i < NUM_JOG_SEL && !error; i++){
        error = hal_pin_new_real(compId, HAL_IO, &this->jogScale[i], 0.0,
				   "boss_plc.%d.jog-scale-%d", id, i);
    }

    if(!error){
        for(i = 0; i < NUM_JOG_SEL && !error; i++){
            error = hal_pin_new_bool(compId, HAL_IN, &this->pJogSelIn[i], 0,
				     "boss_plc.%d.jog-sel-in-%d", id, i);
        }
    }

    // Export pins.
    if(!error){
        error = hal_pin_new_real(compId, HAL_OUT, &this->pJogScaleOut, 0.0,
				   "boss_plc.%d.jog-scale-out", id);
    }

    return(error);
}


/*
 * HAL EXPORTED FUNCTIONS
 */

static void
Plc_Refresh(void *arg, long period)
{
    Plc                         *this = (Plc *)arg;

    Plc_RefreshFeed(this, period);
    Plc_RefreshLimits(this, period);
    Plc_RefreshAmps(this, period);
    Plc_RefreshSpindle(this, period);
    Plc_RefreshJog(this, period);

    if(debug > 0) {
        hal_set_ui32(this->spindleStateDebug, this->spindleState);
        hal_set_ui32(this->xLimit.stateDebug, this->xLimit.state);
        hal_set_ui32(this->yLimit.stateDebug, this->yLimit.state);
    }
}


static void
Plc_RefreshFeed(Plc *this, long period)
{
    (void)period;
    BOOL                        riseCycleStart;

    riseCycleStart = !this->lastCycleStart && hal_get_bool(this->pCycleStartIn);
    this->lastCycleStart = hal_get_bool(this->pCycleStartIn);

    // Condition feed hold so machine waits for cycle start and spindle to be
    // running if it is enabled.
    hal_set_bool(this->pFeedHoldOut, hal_get_bool(this->pCycleHoldIn)
                            || (hal_get_real(this->pSpindleSpeedIn) && !hal_get_bool(this->pSpindleIsOnIn))
                            || (hal_get_bool(this->pSpindleIsOnIn)
                                && (this->lastSpindleSpeed != hal_get_real(this->pSpindleSpeedIn)))
                            || (hal_get_bool(this->pFeedHoldOut) && !riseCycleStart));
    this->lastSpindleSpeed = hal_get_real(this->pSpindleSpeedIn);

    // Limit rapid/feed to 1% when limits are being overridden.
    if(hal_get_bool(this->pLimitOverrideIn) && (hal_get_real(this->pAdaptiveFeedIn) > 0.01))
        hal_set_real(this->pAdaptiveFeedOut, 0.01);
    else
        hal_set_real(this->pAdaptiveFeedOut, hal_get_real(this->pAdaptiveFeedIn));

    // Wait for cycle start to acknowledge tool change.
    hal_set_bool(this->pToolChangedOut, (hal_get_bool(this->pToolChangeIn) && riseCycleStart)
                            || (hal_get_bool(this->pToolChangedOut) && hal_get_bool(this->pToolChangeIn)));

    // Indicates waiting for user to press cycle start.
    hal_set_bool(this->pWaitUserOut, hal_get_bool(this->pFeedHoldOut)
                            || (hal_get_bool(this->pToolChangeIn) && !hal_get_bool(this->pToolChangedOut)));

    // Turn coolant off during tool changes.
    hal_set_bool(this->pMistOnOut, hal_get_bool(this->pMistOnIn) && !hal_get_bool(this->pToolChangeIn));
    hal_set_bool(this->pFloodOnOut, hal_get_bool(this->pFloodOnIn) && !hal_get_bool(this->pToolChangeIn));
}


static void
Plc_RefreshLimits(Plc *this, long period)
{
    (void)period;
    Limit_Refresh(&this->xLimit, hal_get_bool(this->pLimitOverrideIn));
    Limit_Refresh(&this->yLimit, hal_get_bool(this->pLimitOverrideIn));

    // Condition Z limits with override in manual mode.
    hal_set_bool(this->pZLimitPosOut, hal_get_bool(this->pZLimitPosIn)
                            && !(hal_get_bool(this->pZJogEnIn) && hal_get_bool(this->pLimitOverrideIn)));
    hal_set_bool(this->pZLimitNegOut, hal_get_bool(this->pZLimitNegIn)
                            && !(hal_get_bool(this->pZJogEnIn) && hal_get_bool(this->pLimitOverrideIn)));

    // Generate limit active signal for pilot lamp.
    hal_set_bool(this->pLimitActiveOut, Limit_IsActive(&this->xLimit)
                            || Limit_IsActive(&this->yLimit)
                            || hal_get_bool(this->pZLimitPosIn) || hal_get_bool(this->pZLimitNegIn));
}


static void
Plc_RefreshAmps(Plc *this, long period)
{
    int                         i;
    Amp                         *pAmp;

    pAmp = this->amps;
    for(i = 0; i < NUM_AXIS; i++, pAmp++){
        Amp_Refresh(pAmp, period, hal_get_ui32(this->ampReadyDelay));
    }
}

static void
Plc_RefreshSpindle(Plc *this, long period)
{
    Timer_Update(&this->spindleTimer, period);

    switch(this->spindleState){
    // Spindle is off, brake is on.
    case SS_OFF:
        if(!hal_get_bool(this->pBrakeEnIn)){
            this->spindleState = SS_WAIT_BRAKE_OFF;
            hal_set_bool(this->pBrakeEnOut, 0);
            Timer_SetTimeout(&this->spindleTimer, hal_get_ui32(this->brakeOffDelay));
            Timer_Enable(&this->spindleTimer, TM_ONE_SHOT);
        }
        break;

    // Spindle is off, brake has been turned off. Wait at least a brake off
    // delay before turning spindle on.
    case SS_WAIT_BRAKE_OFF:
        if(hal_get_bool(this->pBrakeEnIn)){
            this->spindleState = SS_OFF;
            hal_set_bool(this->pBrakeEnOut, 1);
            Timer_Disable(&this->spindleTimer);

        }else if((hal_get_real(this->pSpindleSpeedIn) != 0.0)
                 && !Timer_IsEnabled(&this->spindleTimer)){

            this->spindleState = SS_WAIT_ON;

            if(hal_get_real(this->pSpindleSpeedIn) > hal_get_real(this->spindleLoToHi)
              || (hal_get_real(this->pSpindleSpeedIn) < 0.0
                && hal_get_real(this->pSpindleSpeedIn) >= -hal_get_real(this->spindleLoToHi))){

                hal_set_bool(this->pSpindleFwdOut, 1);
            }else{
                hal_set_bool(this->pSpindleRevOut, 1);
            }
        }
        break;

    // Spindle has been turned on. Wait for confirmation that it is running.
    case SS_WAIT_ON:
        if(hal_get_bool(this->pSpindleIsOnIn)){
            this->spindleState = SS_ON;

        }else if(hal_get_real(this->pSpindleSpeedIn) == 0.0){
            this->spindleState = SS_WAIT_BRAKE_OFF;

            hal_set_bool(this->pSpindleFwdOut, 0);
            hal_set_bool(this->pSpindleRevOut, 0);
        }
        break;

    // Spindle is running.
    case SS_ON:
        if(hal_get_real(this->pSpindleSpeedIn) == 0.0){
            this->spindleState = SS_WAIT_OFF;

            hal_set_bool(this->pSpindleFwdOut, 0);
            hal_set_bool(this->pSpindleRevOut, 0);
        }
        break;

    // Spindle has been turned off. Wait for confirmation.
    case SS_WAIT_OFF:
        if(!hal_get_bool(this->pSpindleIsOnIn)){
            this->spindleState = SS_WAIT_BRAKE_ON;

            Timer_SetTimeout(&this->spindleTimer, hal_get_ui32(this->brakeOnDelay));
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

        hal_set_bool(this->pSpindleFwdOut, 0);
        hal_set_bool(this->pSpindleRevOut, 0);
    }

    // Condition spindle increase and decrease so they are disabled when
    // spindle is not running and both cannot be enabled at the same time.
    hal_set_bool(this->pSpindleIncOut, hal_get_bool(this->pSpindleIncIn) && !hal_get_bool(this->pSpindleDecIn)
                            && hal_get_bool(this->pSpindleIsOnIn));
    hal_set_bool(this->pSpindleDecOut, hal_get_bool(this->pSpindleDecIn) && !hal_get_bool(this->pSpindleIncIn)
                            && hal_get_bool(this->pSpindleIsOnIn));
}


static void
Plc_RefreshJog(Plc *this, long period)
{
    (void)period;
    int                         i;

    // Jog scale.
    for(i = 0; i < NUM_JOG_SEL; i++){
        if(hal_get_bool(this->pJogSelIn[i])){
            hal_set_real(this->pJogScaleOut, hal_get_real(this->jogScale[i]));
            break;
        }
    }
}


/******************************************************************************
 * LIMIT OBJECT FUNCTION DEFINITIONS
 ******************************************************************************/

static int
Limit_Export(Limit *this, int compId, int id, char axis)
{
    int                         error;

    error = hal_pin_new_real(compId, HAL_IN, &this->pPositionIn, 0.0,
			       "boss_plc.%d.%c-position-in", id, axis);

    if(!error){
        error = hal_pin_new_bool(compId, HAL_IN, &this->pJogEnIn, 0,
				 "boss_plc.%d.%c-jog-en-in", id, axis);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_IN, &this->pIn, 0,
				 "boss_plc.%d.%c-limit-in", id, axis);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pPosOut, 0,
				 "boss_plc.%d.%c-limit-pos-out", id, axis);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pNegOut, 0,
				 "boss_plc.%d.%c-limit-neg-out", id, axis);
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
    return hal_get_bool(this->pIn);
}

static void
Limit_Refresh(Limit *this, rtapi_bool override)
{
    switch(this->state){
    case LS_INIT:
    default:
        this->state = LS_ON_LIMIT;
        this->limitNeg = this->limitPos = 1;
        this->position = hal_get_real(this->pPositionIn);
        // Fall through.

    case LS_ON_LIMIT:
        if(hal_get_bool(this->pIn) == 0){
            this->limitNeg = this->limitPos = 0;

            if(hal_get_real(this->pPositionIn) == this->position)
                this->state = LS_NO_MOTION;
            else if(hal_get_real(this->pPositionIn) > this->position)
                this->state = LS_POS_MOTION;
            else if(hal_get_real(this->pPositionIn) < this->position)
                this->state = LS_NEG_MOTION;
        }
        break;

    case LS_NO_MOTION:
        if(hal_get_bool(this->pIn)){
            this->state = LS_ON_LIMIT;
            this->limitNeg = this->limitPos = 1;
        }else if(hal_get_real(this->pPositionIn) > this->position){
            this->state = LS_POS_MOTION;
        }else if(hal_get_real(this->pPositionIn) < this->position){
            this->state = LS_NEG_MOTION;
        }
        break;

    case LS_POS_MOTION:
        if(hal_get_bool(this->pIn)){
            this->state = LS_ON_LIMIT;
            this->limitPos = 1;
        }else if(hal_get_real(this->pPositionIn) == this->position){
            this->state = LS_NO_MOTION;
        }else if(hal_get_real(this->pPositionIn) < this->position){
            this->state = LS_NEG_MOTION;
        }
        break;

    case LS_NEG_MOTION:
        if(hal_get_bool(this->pIn)){
            this->state = LS_ON_LIMIT;
            this->limitNeg = 1;
        }else if(hal_get_real(this->pPositionIn) == this->position){
            this->state = LS_NO_MOTION;
        }else if(hal_get_real(this->pPositionIn) > this->position){
            this->state = LS_POS_MOTION;
        }
        break;
    }

    this->position = hal_get_real(this->pPositionIn);

    // Condition limits with override in manual mode.
    hal_set_bool(this->pPosOut, this->limitPos && !(hal_get_bool(this->pJogEnIn) && override));
    hal_set_bool(this->pNegOut, this->limitNeg && !(hal_get_bool(this->pJogEnIn) && override));
}


/******************************************************************************
 * AMP OBJECT FUNCTION DEFINITIONS
 ******************************************************************************/

static int
Amp_Export(Amp *this, int compId, int id, char axis)
{
    int                         error;

    error = hal_pin_new_bool(compId, HAL_IN, &this->pEnableIn, 0,
			     "boss_plc.%d.%c-amp-enable-in", id, axis);

    if(!error){
        error = hal_pin_new_bool(compId, HAL_IN, &this->pReadyIn, 0,
				 "boss_plc.%d.%c-amp-ready-in", id, axis);
    }

    if(!error){
        error = hal_pin_new_bool(compId, HAL_OUT, &this->pFaultOut, 0,
				 "boss_plc.%d.%c-amp-fault-out", id, axis);
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
Amp_Refresh(Amp *this, long period, rtapi_u32 readyDelay)
{
    Timer_Update(&this->timer, period);

    if(hal_get_bool(this->pEnableIn)){
        if(!this->lastEnable){
            Timer_SetTimeout(&this->timer, readyDelay);
            Timer_Enable(&this->timer, TM_ONE_SHOT);
        }
    } else {
        Timer_Disable(&this->timer);
    }

    hal_set_bool(this->pFaultOut, hal_get_bool(this->pEnableIn) && !hal_get_bool(this->pReadyIn)
                        && !Timer_IsEnabled(&this->timer));

    this->lastEnable = hal_get_bool(this->pEnableIn);
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
Timer_SetTimeout(Timer *this, rtapi_u32 timeout)
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
