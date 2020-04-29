/******************************************************************************
 *
 * Copyright (C) 2003 John Kasunich <jmkasunich AT users DOT sourceforge DOT net>
 * Copyright (C) 2007 Peter G. Vavaroutsos <pete AT vavaroutsos DOT com>
 *
 *
 ******************************************************************************
 *
 * This file, 'pid.c', is a HAL component that provides Proportional/
 * Integeral/Derivative control loops.  It is a realtime component.
 *
 * It supports a maximum of 16 PID loops, as set by the insmod parameter
 * 'num_chan=' or the 'names=" specifier.  The 'num_chan=' and 'names='
 * specifiers are mutually exclusive.
 *
 * In this documentation, it is assumed that we are discussing position
 * loops.  However this component can be used to implement other loops
 * such as speed loops, torch height control, and others.
 *
 * Each loop has a number of pins and parameters
 * When the 'num_chan=' method is used, names begin with 'pid.x.', where
 * 'x' is the channel number.  Channel numbers  start at zero.
 * When the 'names=' method is used, names begin with the specified names,
 * e.g., for 'names=PID', the pin/parameter begin with "PID."
 *
 * The three most important pins are 'command', 'feedback', and
 * 'output'.  For a position loop, 'command' and 'feedback' are
 * in position units.  For a linear axis, this could be inches,
 * mm, metres, or whatever is relavent.  Likewise, for a angular
 * axis, it could be degrees, radians, etc.  The units of the
 * 'output' pin represent the change needed to make the feedback
 * match the command.  As such, for a position loop 'Output' is
 * a velocity, in inches/sec, mm/sec, degrees/sec, etc.
 *
 * Each loop has several other pins as well.  'error' is equal to
 * 'command' minus 'feedback'.  'enable' is a bit that enables
 * the loop.  If 'enable' is false, all integrators are reset,
 * and the output is forced to zero.  If 'enable' is true, the
 * loop operates normally.
 *
 * The PID gains, limits, and other 'tunable' features of the
 * loop are implemented as parameters.  These are as follows:
 *
 * Pgain        Proportional gain
 * Igain        Integral gain
 * Dgain        Derivative gain
 * bias         Constant offset on output
 * FF0          Zeroth order Feedforward gain
 * FF1          First order Feedforward gain
 * FF2          Second order Feedforward gain
 * deadband     Amount of error that will be ignored
 * maxerror     Limit on error
 * maxerrorI    Limit on error integrator
 * maxerrorD    Limit on error differentiator
 * maxcmdD      Limit on command differentiator
 * maxcmdDD     Limit on command 2nd derivative
 * maxoutput    Limit on output value
 *
 * All of the limits (max____) are implemented such that if the
 * parameter value is zero, there is no limit.
 *
 * A number of internal values which may be useful for testing
 * and tuning are also available as parameters.  To avoid cluttering
 * the parameter list, these are only exported if "debug=1" is
 * specified on the insmod command line.
 *
 * errorI       Integral of error
 * errorD       Derivative of error
 * commandD     Derivative of the command
 * commandDD    2nd derivative of the command
 *
 * The PID loop calculations are as follows (see the code for
 * all the nitty gritty details):
 *
 * error = command - feedback
 * if ( fabs(error) < deadband ) then error = 0
 * limit error to +/- maxerror
 * errorI += error * period
 * limit errorI to +/- maxerrorI
 * errorD = (error - previouserror) / period
 * limit errorD to +/- paxerrorD
 * commandD = (command - previouscommand) / period
 * limit commandD to +/- maxcmdD
 * commandDD = (commandD - previouscommandD) / period
 * limit commandDD to +/- maxcmdDD
 * output = bias + error * Pgain + errorI * Igain +
 *          errorD * Dgain + command * FF0 + commandD * FF1 +
 *          commandDD * FF2
 * limit output to +/- maxoutput
 *
 * This component exports one function called 'pid.x.do-pid-calcs'
 * for each PID loop.  This allows loops to be included in different
 * threads and execute at different rates.
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

#include "rtapi.h"                      // RTAPI realtime OS API.
#include "rtapi_app.h"                  // RTAPI realtime module decls.
#include "rtapi_math.h"
#include "hal.h"                        // HAL public API decls.


// Module information.
MODULE_AUTHOR("Pete Vavaroutsos");
MODULE_DESCRIPTION("Auto-Tune PID Loop Component for EMC HAL");
MODULE_LICENSE("GPL");
static int num_chan;                // Number of channels.
static int default_num_chan = 3;
static int howmany;
RTAPI_MP_INT(num_chan, "number of channels");
static int debug = 0;                   // Flag to export optional params.
RTAPI_MP_INT(debug, "enables optional params");

#define MAX_CHAN            16
static char *names[MAX_CHAN] = {0,};
RTAPI_MP_ARRAY_STRING(names, MAX_CHAN, "names of at_pid components");

#define PI                              3.141592653589


/******************************************************************************
 * PID OBJECT
 *
 * This structure contains the runtime data for a single PID loop.
 ******************************************************************************/

typedef enum {
    STATE_PID,
    STATE_TUNE_IDLE,
    STATE_TUNE_START,
    STATE_TUNE_POS,
    STATE_TUNE_NEG,
    STATE_TUNE_ABORT,
} State;


// Values for tune-type parameter.
typedef enum {
    TYPE_PID,
    TYPE_PI_FF1,
} Mode;


typedef struct {
    // Pins. (former params)
    hal_float_t         *deadband;
    hal_float_t         *maxError;       // Limit for error.
    hal_float_t         *maxErrorI;      // Limit for integrated error.
    hal_float_t         *maxErrorD;      // Limit for differentiated error.
    hal_float_t         *maxCmdD;        // Limit for differentiated cmd.
    hal_float_t         *maxCmdDd;       // Limit for 2nd derivative of cmd.

    hal_float_t         *bias;           // Steady state offset.
    hal_float_t         *pGain;          // Proportional gain.
    hal_float_t         *iGain;          // Integral gain.
    hal_float_t         *dGain;          // Derivative gain.
    hal_float_t         *ff0Gain;        // Feedforward proportional.
    hal_float_t         *ff1Gain;        // Feedforward derivative.
    hal_float_t         *ff2Gain;        // Feedforward 2nd derivative.
    hal_float_t         *maxOutput;      // Limit for PID output.
    hal_float_t         *tuneEffort;     // Control effort for limit cycle.
    hal_u32_t           *tuneCycles;
    hal_u32_t           *tuneType;

    hal_float_t         *errorI;         // Integrated error.
    hal_float_t         *errorD;         // Differentiated error.
    hal_float_t         *cmdD;           // Differentiated command.
    hal_float_t         *cmdDd;          // 2nd derivative of command.
    hal_float_t         *ultimateGain;   // Calc by auto-tune from limit cycle.
    hal_float_t         *ultimatePeriod; // Calc by auto-tune from limit cycle.

    // Pins.
    hal_bit_t           *pEnable;
    hal_float_t         *pCommand;      // Commanded value.
    hal_float_t         *pFeedback;     // Feedback value.
    hal_float_t         *pError;        // Command - feedback.
    hal_float_t         *pOutput;       // The output value.
    hal_bit_t           *pTuneMode;     // 0=PID, 1=tune.
    hal_bit_t           *pTuneStart;    // Set to 1 to start an auto-tune cycle.
                                        // Clears automatically when the cycle
                                        // has finished.

    // Private data.
    hal_float_t         prevError;      // previous error for differentiator.
    hal_float_t         prevCmd;        // previous command for differentiator.
    hal_float_t         limitState;     // +1 or -1 if in limit, else 0.
    State               state;
    hal_u32_t           cycleCount;
    hal_u32_t           cyclePeriod;
    hal_float_t         cycleAmplitude;
    hal_float_t         totalTime;
    hal_float_t         avgAmplitude;
} Pid;


// These methods are used for initialization.
static int Pid_Init(Pid *this);
static int Pid_Export(Pid *this, int compId, char* prefix);
static void Pid_AutoTune(Pid *this, long period);
static void Pid_CycleEnd(Pid *this);

// These methods are exported to the HAL.
static void Pid_Refresh(void *this, long period);

/******************************************************************************
 * COMPONENT OBJECT
 *
 * This object contains all the data for this HAL component.
 *
 ******************************************************************************/


typedef struct {
    int                         id;     // HAL component ID.
    Pid                         *pidTable;
} Component;

static Component                component;


/******************************************************************************
 * INIT AND EXIT CODE
 ******************************************************************************/

int
rtapi_app_main(void)
{
    int                         retval,n,i;
    Pid                         *pComp;

    if(num_chan && names[0]) {
        rtapi_print_msg(RTAPI_MSG_ERR,"num_chan= and names= are mutually exclusive\n");
        return -EINVAL;
    }
    if(!num_chan && !names[0]) num_chan = default_num_chan;

    if(num_chan) {
        howmany = num_chan;
    } else {
        howmany = 0;
        for (i = 0; i < MAX_CHAN; i++) {
            if ( (names[i] == NULL) || (*names[i] == 0) ){
                break;
            }
            howmany = i + 1;
        }
    }

    // Check number of channels.
    if((howmany <= 0) || (howmany > MAX_CHAN)){
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "AT_PID: ERROR: invalid number of channels: %d\n", howmany);
        return(-1);
    }

    // Connect to the HAL.
    component.id = hal_init("at_pid");
    if(component.id < 0){
        rtapi_print_msg(RTAPI_MSG_ERR, "PID: ERROR: hal_init() failed\n");
        return(-1);
    }

    // Allocate memory for pid objects.
    component.pidTable = hal_malloc(howmany * sizeof(*pComp));

    if(component.pidTable == NULL){
        rtapi_print_msg(RTAPI_MSG_ERR, "PID: ERROR: hal_malloc() failed\n");
        hal_exit(component.id);
        return(-1);
    }

    pComp = component.pidTable;
    i = 0; // for names= items
    for(n = 0; n < howmany; n++, pComp++){

        // Export pins, parameters, and functions.
        if(num_chan) {
            char buf[HAL_NAME_LEN + 1];
            // note name is pid not at_pid
            rtapi_snprintf(buf, sizeof(buf), "pid.%d", n);
	    retval = Pid_Export(pComp, component.id, buf);
        } else {
	    retval = Pid_Export(pComp, component.id, names[i++]);
        }
	if (retval != 0) {
	    rtapi_print_msg(RTAPI_MSG_ERR,
		"AT_PID: ERROR: at_pid %d var export failed\n", n);
	    hal_exit(component.id);
	    return -1;
	}

        // Initialize pid.
        if(Pid_Init(pComp)){
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
    Pid                         *pComp;

    hal_exit(component.id);

    if((pComp = component.pidTable) != NULL){
        // TODO: Free pid objects when HAL supports free.
//        hal_free(pComp);
    }
}


/******************************************************************************
 * PID OBJECT FUNCTION DEFINITIONS
 ******************************************************************************/

/*
 * LOCAL FUNCTIONS
 */

static int
Pid_Init(Pid *this)
{
    // Init all structure members.
    *(this->deadband) = 0;
    *(this->maxError) = 0;
    *(this->maxErrorI) = 0;
    *(this->maxErrorD) = 0;
    *(this->maxCmdD) = 0;
    *(this->maxCmdDd) = 0;
    *(this->errorI) = 0;
    this->prevError = 0;
    *(this->errorD) = 0;
    this->prevCmd = 0;
    this->limitState = 0;
    *(this->cmdD) = 0;
    *(this->cmdDd) = 0;
    *(this->bias) = 0;
    *(this->pGain) = 1;
    *(this->iGain) = 0;
    *(this->dGain) = 0;
    *(this->ff0Gain) = 0;
    *(this->ff1Gain) = 0;
    *(this->ff2Gain) = 0;
    *(this->maxOutput) = 0;
    this->state = STATE_PID;
    *(this->tuneCycles) = 50;
    *(this->tuneEffort) = 0.5;
    *(this->tuneType) = TYPE_PID;

    return(0);
}


static int
Pid_Export(Pid *this, int compId,char* prefix)
{
    int                         error, msg;
    char                        buf[HAL_NAME_LEN + 1];

    // This function exports a lot of stuff, which results in a lot of
    // logging if msg_level is at INFO or ALL. So we save the current value
    // of msg_level and restore it later.  If you actually need to log this
    // function's actions, change the second line below.
    msg = rtapi_get_msg_level();
    rtapi_set_msg_level(RTAPI_MSG_WARN);

    // Export pins.
    error = hal_pin_bit_newf(HAL_IN, &(this->pEnable), compId, "%s.enable", prefix);

    if(!error){
        error = hal_pin_float_newf(HAL_IN, &(this->pCommand), compId, "%s.command", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_IN, &(this->pFeedback), compId, "%s.feedback", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_OUT, &(this->pError), compId, "%s.error", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_OUT, &(this->pOutput), compId, "%s.output", prefix);
    }

    if(!error){
        error = hal_pin_bit_newf(HAL_IN, &(this->pTuneMode), compId, "%s.tune-mode", prefix);
    }

    if(!error){
        error = hal_pin_bit_newf(HAL_IO, &(this->pTuneStart), compId, "%s.tune-start", prefix);
    }

    // Export pins. (former parameters).
    if(!error){
        error = hal_pin_float_newf(HAL_IO, &(this->deadband), compId, "%s.deadband", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_IO, &(this->maxError), compId, "%s.maxerror", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_IO, &(this->maxErrorI), compId, "%s.maxerrorI", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_IO, &(this->maxErrorD), compId, "%s.maxerrorD", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_IO, &(this->maxCmdD), compId, "%s.maxcmdD", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_IO, &(this->maxCmdDd), compId, "%s.maxcmdDD", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_IO, &(this->bias), compId, "%s.bias", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_IO, &(this->pGain), compId, "%s.Pgain", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_IO, &(this->iGain), compId, "%s.Igain", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_IO, &(this->dGain), compId, "%s.Dgain", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_IO, &(this->ff0Gain), compId, "%s.FF0", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_IO, &(this->ff1Gain), compId, "%s.FF1", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_IO, &(this->ff2Gain), compId, "%s.FF2", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_IO, &(this->maxOutput), compId, "%s.maxoutput", prefix);
    }

    if(!error){
        error = hal_pin_float_newf(HAL_IO, &(this->tuneEffort), compId, "%s.tune-effort", prefix);
    }

    if(!error){
        error = hal_pin_u32_newf(HAL_IO, &(this->tuneCycles), compId, "%s.tune-cycles", prefix);
    }

    if(!error){
        error = hal_pin_u32_newf(HAL_IO, &(this->tuneType), compId, "%s.tune-type", prefix);
    }

    // Export optional parameters.
    if(debug > 0){
        if(!error){
            error = hal_pin_float_newf(HAL_OUT, &(this->errorI), compId, "%s.errorI", prefix);
        }

        if(!error){
            error = hal_pin_float_newf(HAL_OUT, &(this->errorD), compId, "%s.errorD", prefix);
        }

        if(!error){
            error = hal_pin_float_newf(HAL_OUT, &(this->cmdD), compId, "%s.commandD", prefix);
        }

        if(!error){
            error = hal_pin_float_newf(HAL_OUT, &(this->cmdDd), compId, "%s.commandDD", prefix);
        }

        if(!error){
            error = hal_pin_float_newf(HAL_OUT, &(this->ultimateGain), compId, "%s.ultimate-gain", prefix);
        }

        if(!error){
            error = hal_pin_float_newf(HAL_OUT, &(this->ultimatePeriod), compId, "%s.ultimate-period", prefix);
        }
    } else {
  	this->errorI = (hal_float_t *) hal_malloc(sizeof(hal_float_t));
  	this->errorD = (hal_float_t *) hal_malloc(sizeof(hal_float_t));
  	this->cmdD   = (hal_float_t *) hal_malloc(sizeof(hal_float_t));
  	this->cmdDd  = (hal_float_t *) hal_malloc(sizeof(hal_float_t));
  	this->ultimateGain = (hal_float_t *) hal_malloc(sizeof(hal_float_t));
  	this->ultimatePeriod = (hal_float_t *) hal_malloc(sizeof(hal_float_t));
    }

    // Export functions.
    if(!error){
        rtapi_snprintf(buf, sizeof(buf), "%s.do-pid-calcs", prefix);
        error = hal_export_funct(buf, Pid_Refresh, this, 1, 0, compId);
    }

    if(!error){
        *this->pEnable = 0;
        *this->pCommand = 0;
        *this->pFeedback = 0;
        *this->pError = 0;
        *this->pOutput = 0;
        *this->pTuneMode = 0;
        *this->pTuneStart = 0;
    }

    // Restore saved message level.
    rtapi_set_msg_level(msg);
    return(error);
}


/*
 * Perform an auto-tune operation. Sets up a limit cycle using the specified
 * tune effort. Averages the amplitude and period over the specified number of
 * cycles. This characterizes the process and determines the ultimate gain
 * and period, which are then used to calculate PID.
 *
 * CO(t) = P * [ e(t) + 1/Ti * (f e(t)dt) - Td * (d/dt PV(t)) ]
 *
 * Pu = 4/PI * tuneEffort / responseAmplitude
 * Ti = 0.5 * responsePeriod
 * Td = 0.125 * responsePeriod
 *
 * P = 0.6 * Pu
 * I = P * 1/Ti
 * D = P * Td
 */
static void
Pid_AutoTune(Pid *this, long period)
{
    hal_float_t                 error;

    // Calculate the error.
    error = *this->pCommand - *this->pFeedback;
    *this->pError = error;

    // Check if enabled and if still in tune mode.
    if(!*this->pEnable || !*this->pTuneMode){
        this->state = STATE_TUNE_ABORT;
    }

    switch(this->state){
    case STATE_TUNE_IDLE:
        // Wait for tune start command.
        if(*this->pTuneStart)
            this->state = STATE_TUNE_START;
        break;

    case STATE_TUNE_START:
        // Initialize tuning variables and start limit cycle.
        this->state = STATE_TUNE_POS;
        this->cycleCount = 0;
        this->cyclePeriod = 0;
        this->cycleAmplitude = 0;
        this->totalTime = 0;
        this->avgAmplitude = 0;
        *(this->ultimateGain) = 0;
        *(this->ultimatePeriod) = 0;
        *this->pOutput = *(this->bias) + fabs(*(this->tuneEffort));
        break;

    case STATE_TUNE_POS:
    case STATE_TUNE_NEG:
        this->cyclePeriod += period;

        if(error < 0.0){
            // Check amplitude.
            if(-error > this->cycleAmplitude)
                this->cycleAmplitude = -error;

            // Check for end of cycle.
            if(this->state == STATE_TUNE_POS){
                this->state = STATE_TUNE_NEG;
                Pid_CycleEnd(this);
            }

            // Update output so user can ramp effort until movement occurs.
            *this->pOutput = *(this->bias) - fabs(*(this->tuneEffort));
        }else{
            // Check amplitude.
            if(error > this->cycleAmplitude)
                this->cycleAmplitude = error;

            // Check for end of cycle.
            if(this->state == STATE_TUNE_NEG){
                this->state = STATE_TUNE_POS;
                Pid_CycleEnd(this);
            }

            // Update output so user can ramp effort until movement occurs.
            *this->pOutput = *(this->bias) + fabs(*(this->tuneEffort));
        }

        // Check if the last cycle just ended. This is really the number
        // of half cycles.
        if(this->cycleCount < *(this->tuneCycles))
            break;

        // Calculate PID.
        *(this->ultimateGain) = (4.0 * fabs(*(this->tuneEffort)))/(PI * this->avgAmplitude);
        *(this->ultimatePeriod) = 2.0 * this->totalTime / *(this->tuneCycles);
        *(this->ff0Gain) = 0;
        *(this->ff2Gain) = 0;

        if(*(this->tuneType) == TYPE_PID){
            // PID.
            *(this->pGain) = 0.6 * *(this->ultimateGain);
            *(this->iGain) = *(this->pGain) / (*(this->ultimatePeriod) / 2.0);
            *(this->dGain) = *(this->pGain) * (*(this->ultimatePeriod) / 8.0);
            *(this->ff1Gain) = 0;
        }else{
            // PI FF1.
            *(this->pGain) = 0.45 * *(this->ultimateGain);
            *(this->iGain) = *(this->pGain) / (*(this->ultimatePeriod) / 1.2);
            *(this->dGain) = 0;

            // Scaling must be set so PID output is in user units per second.
            *(this->ff1Gain) = 1;
        }

        // Fall through.

    case STATE_TUNE_ABORT:
    default:
        // Force output to zero.
        *this->pOutput = 0;

        // Abort any tuning cycle in progress.
        *this->pTuneStart = 0;
        this->state = (*this->pTuneMode)? STATE_TUNE_IDLE: STATE_PID;
    }
}


static void
Pid_CycleEnd(Pid *this)
{
    this->cycleCount++;
    this->avgAmplitude += this->cycleAmplitude / *(this->tuneCycles);
    this->cycleAmplitude = 0;
    this->totalTime += this->cyclePeriod * 0.000000001;
    this->cyclePeriod = 0;
}


/*
 * HAL EXPORTED FUNCTIONS
 */

static void
Pid_Refresh(void *arg, long periodNs)
{
    Pid                         *this = (Pid *)arg;
    hal_float_t                 period, periodRecip;
    hal_float_t                 prevCmdD, error, output;

    if(this->state != STATE_PID){
        Pid_AutoTune(this, periodNs);
        return;
    }

    // Calculate the error.
    error = *this->pCommand - *this->pFeedback;

    // Store error to error pin.
    *this->pError = error;

    // Check for tuning mode request.
    if(*this->pTuneMode){
        *(this->errorI) = 0;
        this->prevError = 0;
        *(this->errorD) = 0;
        this->prevCmd = 0;
        this->limitState = 0;
        *(this->cmdD) = 0;
        *(this->cmdDd) = 0;

        // Force output to zero.
        *(this->pOutput) = 0;

        // Switch to tuning mode.
        this->state = STATE_TUNE_IDLE;

        return;
    }

    // Precalculate some timing constants.
    period = periodNs * 0.000000001;
    periodRecip = 1.0 / period;

    // Apply error limits.
    if(*(this->maxError) != 0.0){
        if(error > *(this->maxError)){
            error = *(this->maxError);
        }else if(error < -*(this->maxError)){
            error = -*(this->maxError);
        }
    }

    // Apply the deadband.
    if(error > *(this->deadband)){
        error -= *(this->deadband);
    }else if(error < -*(this->deadband)){
        error += *(this->deadband);
    }else{
        error = 0;
    }

    // Calculate derivative term.
    *(this->errorD) = (error - this->prevError) * periodRecip;
    this->prevError = error;

    // Apply derivative limits.
    if(*(this->maxErrorD) != 0.0){
        if(*(this->errorD) > *(this->maxErrorD)){
            *(this->errorD) = *(this->maxErrorD);
        }else if(*(this->errorD) < -*(this->maxErrorD)){
            *(this->errorD) = -*(this->maxErrorD);
        }
    }

    // Calculate derivative of command.
    // Save old value for 2nd derivative calc later.
    prevCmdD = *(this->cmdD);
    *(this->cmdD) = (*this->pCommand - this->prevCmd) * periodRecip;
    this->prevCmd = *this->pCommand;

    // Apply derivative limits.
    if(*(this->maxCmdD) != 0.0){
        if(*(this->cmdD) > *(this->maxCmdD)){
            *(this->cmdD) = *(this->maxCmdD);
        }else if(*(this->cmdD) < -*(this->maxCmdD)){
            *(this->cmdD) = -*(this->maxCmdD);
        }
    }

    // Calculate 2nd derivative of command.
    *(this->cmdDd) = (*(this->cmdD) - prevCmdD) * periodRecip;

    // Apply 2nd derivative limits.
    if(*(this->maxCmdDd) != 0.0){
        if(*(this->cmdDd) > *(this->maxCmdDd)){
            *(this->cmdDd) = *(this->maxCmdDd);
        }else if(*(this->cmdDd) < -*(this->maxCmdDd)){
            *(this->cmdDd) = -*(this->maxCmdDd);
        }
    }

    // Check if enabled.
    if(!*this->pEnable){
        // Reset integrator.
        *(this->errorI) = 0;

        // Force output to zero.
        *this->pOutput = 0;
        this->limitState = 0;

        return;
    }

    // If output is in limit, don't let integrator wind up.
    if(error * this->limitState <= 0.0){
        // Compute integral term.
        *(this->errorI) += error * period;
    }

    // Apply integrator limits.
    if(*(this->maxErrorI) != 0.0){
        if(*(this->errorI) > *(this->maxErrorI)){
            *(this->errorI) = *(this->maxErrorI);
        }else if(*(this->errorI) < -*(this->maxErrorI)){
            *(this->errorI) = -*(this->maxErrorI);
        }
    }

    // Calculate the output value.
    output =
        *(this->bias) + *(this->pGain) * error + *(this->iGain) * *(this->errorI) +
        *(this->dGain) * *(this->errorD);
    output += *this->pCommand * *(this->ff0Gain) + *(this->cmdD) * *(this->ff1Gain) +
        *(this->cmdDd) * *(this->ff2Gain);

    // Apply output limits.
    if(*(this->maxOutput) != 0.0){
        if(output > *(this->maxOutput)){
            output = *(this->maxOutput);
            this->limitState = 1;
        }else if(output < -*(this->maxOutput)){
            output = -*(this->maxOutput);
            this->limitState = -1;
        }else{
            this->limitState = 0;
        }
    }

    // Write final output value to output pin.
    *this->pOutput = output;
}
