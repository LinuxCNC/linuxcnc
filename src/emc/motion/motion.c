#define _XOPEN_SOURCE

#include <linux/types.h>
#include <stdarg.h>		/* va_list */
#include <float.h>		/* DBL_MIN */
#include <math.h>		/* fabs() */

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"		/* HAL public API decls */

#include "posemath.h"		/* PmPose, pmCartMag() */
#include "emcpos.h"
#include "emcmotcfg.h"
#include "emcmotglb.h"
#include "motion.h"
#include "mot_priv.h"
#include "emcpid.h"
#include "cubic.h"
#include "tc.h"
#include "tp.h"
#include "extintf.h"
#include "mmxavg.h"
#include "emcmotlog.h"

/* RTAPI shmem stuff */
static int key = 101;
static int shmem_mem;		/* the shared memory ID */
static EMCMOT_DEBUG localEmcmotDebug;
/* lpg changed period from 20000 to avoid flattening a P133                */
/* this is programmable in the .ini file, so faster boxes can use 16us     */
static int PERIOD = 48000;	/* fundamental period for timer interrupts */
static int PERIOD_NSEC = 48000000;

int debug_motion = 0;

/*
  Principles of communication:

  Data is copied in or out from the various types of comm mechanisms:
  mbuff mapped memory for Linux/RT-Linux, or OS shared memory for Unixes.

  emcmotStruct is ptr to this memory.

  emcmotCommand points to emcmotStruct->command,
  emcmotStatus points to emcmotStruct->status,
  emcmotError points to emcmotStruct->error, and
  emcmotLog points to emcmotStruct->log.
  emcmotComp[] points to emcmotStruct->comp[].
 */
EMCMOT_STRUCT *emcmotStruct;
/* ptrs to either buffered copies or direct memory for
   command and status */
EMCMOT_COMMAND *emcmotCommand;
EMCMOT_STATUS *emcmotStatus;
EMCMOT_STATUS *emcmotStatus;
EMCMOT_CONFIG *emcmotConfig;
EMCMOT_DEBUG *emcmotDebug;
EMCMOT_ERROR *emcmotError;	/* unused for RT_FIFO */
EMCMOT_LOG *emcmotLog;		/* unused for RT_FIFO */
EMCMOT_IO *emcmotIo;		/* new struct added 8/21/2001 JME */
EMCMOT_COMP *emcmotComp[EMCMOT_MAX_AXIS];	/* unused for RT_FIFO */
EMCMOT_LOG_STRUCT ls;

/* FIXME */
int EMCMOT_NO_FORWARD_KINEMATICS = 0;

/* the type of kinematics, from emcmot.h */
int kinType = 0;

void emcmot_config_change(void)
{
    if (emcmotConfig->head == emcmotConfig->tail) {
	emcmotConfig->config_num++;
	emcmotStatus->config_num = emcmotConfig->config_num;
	emcmotConfig->head++;
    }
}

void reportError(const char *fmt, ...)
{
    /* use the rtapi_snprintf function where vsprintf is called for. */
    va_list args;
    char error[EMCMOT_ERROR_LEN];

    va_start(args, fmt);
    rtapi_snprintf(error, EMCMOT_ERROR_LEN, fmt, args);
    va_end(args);

    emcmotErrorPut(emcmotError, error);
}

/* call this when setting the trajectory cycle time */
void setTrajCycleTime(double secs)
{
    static int t;

    /* make sure it's not zero */
    if (secs <= 0.0) {
	return;
    }

    emcmot_config_change();

    /* compute the interpolation rate as nearest integer to traj/servo */
    emcmotConfig->interpolationRate =
	(int) (secs / emcmotConfig->servoCycleTime + 0.5);

    /* set traj planner */
    tpSetCycleTime(&emcmotDebug->queue, secs);

    /* set the free planners, cubic interpolation rate and segment time */
    for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	tpSetCycleTime(&emcmotDebug->freeAxis[t], secs);
	cubicSetInterpolationRate(&emcmotDebug->cubic[t],
	    emcmotConfig->interpolationRate);
    }

    /* copy into status out */
    emcmotConfig->trajCycleTime = secs;
}

/* call this when setting the servo cycle time */
static void setServoCycleTime(double secs)
{
    static int t;

    /* make sure it's not zero */
    if (secs <= 0.0) {
	return;
    }

    emcmot_config_change();

    /* compute the interpolation rate as nearest integer to traj/servo */
    emcmotConfig->interpolationRate =
	(int) (emcmotConfig->trajCycleTime / secs + 0.5);

    /* set the cubic interpolation rate and PID cycle time */
    for (t = 0; t < EMCMOT_MAX_AXIS; t++) {
	cubicSetInterpolationRate(&emcmotDebug->cubic[t],
	    emcmotConfig->interpolationRate);
	cubicSetSegmentTime(&emcmotDebug->cubic[t], secs);
	pidSetCycleTime(&emcmotConfig->pid[t], secs);
    }

    /* copy into status out */
    emcmotConfig->servoCycleTime = secs;
}

/* register symbols to be modified by insmod
   see "Linux Device Drivers", Alessandro Rubini, p. 385
   (p.42-44 in 2nd edition) */
MODULE_PARM(PERIOD, "i");
MODULE_PARM(debug_motion, "i");
MODULE_PARM(SHMEM_BASE_ADDRESS, "l");
MODULE_PARM(SHMEM_KEY, "i");
MODULE_PARM(MOTION_IO_ADDRESS, "i");

MODULE_PARM(IO_BASE_ADDRESS, "l");
MODULE_PARM(FIND_IO_BASE_ADDRESS, "i");

MODULE_PARM(EMCMOT_TASK_PRIORITY, "i");
MODULE_PARM(EMCMOT_TASK_STACK_SIZE, "i");
MODULE_PARM(EMCMOT_NO_FORWARD_KINEMATICS, "i");

#ifdef MODULE_LICENSE
// The additional rights are you can do anything you want. -- meaning the code is public domain.
MODULE_LICENSE("GPL and additional rights");
#endif

/* rtapi stuff */
static int module;
static int emcmot_task;		/* the task ID */
static int emcmot_prio;
#define EMCMOT_TASK_STACKSIZE 8192

typedef void (*RTAPI_FUNC) (void *);

int init_module(void)
{
    int axis;
    int t;
    PID_STRUCT pid;
    int retval;
    long period;

    module = rtapi_init("EMCMOT");
    if (module < 0) {
	rtapi_print("emcmot init: rtapi_init returned %d\n", module);
	return -1;
    }

    emcmotStruct = 0;
    emcmotDebug = 0;
    emcmotStatus = 0;
    emcmotCommand = 0;
    emcmotIo = 0;
    emcmotConfig = 0;

    /* record the kinematics type of the machine */
    kinType = kinematicsType();
    if (EMCMOT_NO_FORWARD_KINEMATICS) {
	kinType = KINEMATICS_INVERSE_ONLY;
    }

    /* allocate and initialize the shared memory structure */
    shmem_mem = rtapi_shmem_new(key, module, sizeof(EMCMOT_STRUCT));
    if (shmem_mem < 0) {
	rtapi_print("emcmot init: rtapi_shmem_new returned %d\n", shmem_mem);
	rtapi_exit(module);
	return -1;
    }
    retval = rtapi_shmem_getptr(shmem_mem, (void **) &emcmotStruct);
    if (retval != RTAPI_SUCCESS) {
	rtapi_print("emcmot init: rtapi_shmem_getptr returned %d\n", retval);
	rtapi_exit(module);
	return -1;
    }

    /* is timer started? if so, what period? */
    PERIOD_NSEC = PERIOD * 1000;	/* convert from msec to nsec */
    period = rtapi_clock_set_period(0);
    if (period == 0) {
	/* not running, start it */
	rtapi_print("emcmot init: starting timer with period %ld\n",
	    PERIOD_NSEC);
	period = rtapi_clock_set_period(PERIOD_NSEC);
	if (period < 0) {
	    rtapi_print
		("emcmot init: rtapi_clock_set_period failed with %ld\n",
		period);
	    rtapi_exit(module);
	    return -1;
	}
    }
    /* make sure period <= desired period (allow 1% roundoff error) */
    if (period > (PERIOD_NSEC + (PERIOD_NSEC / 100))) {
	/* timer period too long */
	rtapi_print("emcmot init: clock period too long: %ld\n", period);
	rtapi_exit(module);
	return -1;
    }
    rtapi_print("emcmot init: desired clock %ld, actual %ld\n", PERIOD_NSEC,
	period);

    /* we'll reference emcmotStruct directly */
    emcmotCommand = (EMCMOT_COMMAND *) & emcmotStruct->command;
    emcmotStatus = (EMCMOT_STATUS *) & emcmotStruct->status;
    emcmotConfig = (EMCMOT_CONFIG *) & emcmotStruct->config;
    if (debug_motion) {
	emcmotDebug = (EMCMOT_DEBUG *) & emcmotStruct->debug;
    } else {
	emcmotDebug = &localEmcmotDebug;
    }

    emcmotError = (EMCMOT_ERROR *) & emcmotStruct->error;
    emcmotIo = (EMCMOT_IO *) & emcmotStruct->io;	/* set address of
							   struct JE
							   8/21/2001 */
    emcmotLog = (EMCMOT_LOG *) & emcmotStruct->log;
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotComp[axis] = (EMCMOT_COMP *) & emcmotStruct->comp[axis];

	emcmotDebug->bcomp[axis] = 0;	/* backlash comp value */
	emcmotDebug->bcompdir[axis] = 0;	/* 0=none, 1=pos, -1=neg */
	emcmotDebug->bcompincr[axis] = 0;	/* incremental backlash comp */
	emcmotDebug->bac_done[axis] = 0;
	emcmotDebug->bac_d[axis] = 0;
	emcmotDebug->bac_di[axis] = 0;
	emcmotDebug->bac_D[axis] = 0;
	emcmotDebug->bac_halfD[axis] = 0;
	emcmotDebug->bac_incrincr[axis] = 0;
	emcmotDebug->bac_incr[axis] = 0;
    }

    /* zero shared memory */
    for (t = 0; t < sizeof(EMCMOT_STRUCT); t++) {
	((char *) emcmotStruct)[t] = 0;
    }

    /* init locals */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotDebug->maxLimitSwitchCount[axis] = 0;
	emcmotDebug->minLimitSwitchCount[axis] = 0;
	emcmotDebug->ampFaultCount[axis] = 0;
    }

    /* init compensation struct */
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotComp[axis]->total = 0;
	emcmotComp[axis]->alter = 0.0;
	/* leave out the avgint, nominal, forward, and reverse, since these
	   can't be zero and the total flag prevents their use anyway */
    }

    /* init error struct */
    emcmotErrorInit(emcmotError);

    /* init command struct */
    emcmotCommand->head = 0;
    emcmotCommand->command = 0;
    emcmotCommand->commandNum = 0;
    emcmotCommand->tail = 0;

    /* init status struct */

    emcmotStatus->head = 0;
    emcmotDebug->head = 0;
    emcmotConfig->head = 0;

    emcmotStatus->motionFlag = 0;
    SET_MOTION_ERROR_FLAG(0);
    SET_MOTION_COORD_FLAG(0);
    SET_MOTION_TELEOP_FLAG(0);
    emcmotDebug->split = 0;
    emcmotStatus->commandEcho = 0;
    emcmotStatus->commandNumEcho = 0;
    emcmotStatus->commandStatus = 0;
    emcmotStatus->heartbeat = 0;
    emcmotStatus->computeTime = 0.0;
    emcmotConfig->numAxes = EMCMOT_MAX_AXIS;
    emcmotConfig->trajCycleTime = TRAJ_CYCLE_TIME;
    emcmotConfig->servoCycleTime = SERVO_CYCLE_TIME;
    emcmotStatus->pos.tran.x = 0.0;
    emcmotStatus->pos.tran.y = 0.0;
    emcmotStatus->pos.tran.z = 0.0;
    emcmotStatus->actualPos.tran.x = 0.0;
    emcmotStatus->actualPos.tran.y = 0.0;
    emcmotStatus->actualPos.tran.z = 0.0;
    emcmotStatus->vel = VELOCITY;
    emcmotConfig->limitVel = VELOCITY;
    emcmotStatus->acc = ACCELERATION;
    emcmotStatus->qVscale = 1.0;
    emcmotStatus->id = 0;
    emcmotStatus->depth = 0;
    emcmotStatus->activeDepth = 0;
    emcmotStatus->paused = 0;
    emcmotStatus->overrideLimits = 0;
    SET_MOTION_INPOS_FLAG(1);
    emcmotStatus->logOpen = 0;
    emcmotStatus->logStarted = 0;
    emcmotStatus->logSize = 0;
    emcmotStatus->logSkip = 0;
    emcmotStatus->logPoints = 0;
    SET_MOTION_ENABLE_FLAG(0);
    emcmotConfig->kinematics_type = kinType;

    emcmotDebug->oldPos = emcmotStatus->pos;
    emcmotDebug->oldVel.tran.x = 0.0;
    emcmotDebug->oldVel.tran.y = 0.0;
    emcmotDebug->oldVel.tran.z = 0.0;

    emcmot_config_change();

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotConfig->homingVel[axis] = VELOCITY;
	emcmotConfig->homeOffset[axis] = 0.0;
	emcmotStatus->axisFlag[axis] = 0;
	emcmotConfig->maxLimit[axis] = MAX_LIMIT;
	emcmotConfig->minLimit[axis] = MIN_LIMIT;
	emcmotConfig->maxOutput[axis] = MAX_OUTPUT;
	emcmotConfig->minOutput[axis] = MIN_OUTPUT;
	emcmotConfig->minFerror[axis] = 0.0;	/* gives a true linear ferror 
						 */
	emcmotConfig->maxFerror[axis] = MAX_FERROR;
	emcmotDebug->ferrorCurrent[axis] = 0.0;
	emcmotDebug->ferrorHighMark[axis] = 0.0;
	emcmotStatus->outputScale[axis] = OUTPUT_SCALE;
	emcmotStatus->outputOffset[axis] = OUTPUT_OFFSET;
	emcmotStatus->inputScale[axis] = INPUT_SCALE;
	emcmotDebug->inverseInputScale[axis] = 1.0 / INPUT_SCALE;
	emcmotStatus->inputOffset[axis] = INPUT_OFFSET;
	emcmotDebug->inverseOutputScale[axis] = 1.0 / OUTPUT_SCALE;
	emcmotStatus->axVscale[axis] = 1.0;
	emcmotConfig->axisLimitVel[axis] = 1.0;
	emcmotDebug->bigVel[axis] = 1.0;
	SET_AXIS_ENABLE_FLAG(axis, 0);
	SET_AXIS_ACTIVE_FLAG(axis, 0);	/* default is not to use it; need an
					   explicit activate */
	SET_AXIS_NSL_FLAG(axis, 0);
	SET_AXIS_PSL_FLAG(axis, 0);
	SET_AXIS_NHL_FLAG(axis, 0);
	SET_AXIS_PHL_FLAG(axis, 0);
	SET_AXIS_INPOS_FLAG(axis, 1);
	SET_AXIS_HOMING_FLAG(axis, 0);
	SET_AXIS_HOMED_FLAG(axis, 0);
	SET_AXIS_FERROR_FLAG(axis, 0);
	SET_AXIS_FAULT_FLAG(axis, 0);
	SET_AXIS_ERROR_FLAG(axis, 0);
	emcmotConfig->axisPolarity[axis] = (EMCMOT_AXIS_FLAG) 0xFFFFFFFF;
	/* will read encoders directly, so don't set them here */
    }

    /* FIXME-- add emcmotError */

    /* init min-max-avg stats */
    mmxavgInit(&emcmotDebug->tMmxavg, emcmotDebug->tMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->sMmxavg, emcmotDebug->sMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->nMmxavg, emcmotDebug->nMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->yMmxavg, emcmotDebug->yMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->fMmxavg, emcmotDebug->fMmxavgSpace, MMXAVG_SIZE);
    mmxavgInit(&emcmotDebug->fyMmxavg, emcmotDebug->fyMmxavgSpace,
	MMXAVG_SIZE);
    emcmotDebug->tMin = 0.0;
    emcmotDebug->tMax = 0.0;
    emcmotDebug->tAvg = 0.0;
    emcmotDebug->sMin = 0.0;
    emcmotDebug->sMax = 0.0;
    emcmotDebug->sAvg = 0.0;
    emcmotDebug->nMin = 0.0;
    emcmotDebug->nMax = 0.0;
    emcmotDebug->nAvg = 0.0;
    emcmotDebug->yMin = 0.0;
    emcmotDebug->yMax = 0.0;
    emcmotDebug->yAvg = 0.0;
    emcmotDebug->fyMin = 0.0;
    emcmotDebug->fyMax = 0.0;
    emcmotDebug->fyAvg = 0.0;
    emcmotDebug->fMin = 0.0;
    emcmotDebug->fMax = 0.0;
    emcmotDebug->fAvg = 0.0;

    emcmotDebug->cur_time = emcmotDebug->last_time = 0.0;
    emcmotDebug->start_time = etime();
    emcmotDebug->running_time = 0.0;

    /* init motion emcmotDebug->queue */
    if (-1 == tpCreate(&emcmotDebug->queue, DEFAULT_TC_QUEUE_SIZE,
	    emcmotDebug->queueTcSpace)) {
	rtapi_print("can't create motion emcmotDebug->queue\n");
	return -1;
    }
    tpInit(&emcmotDebug->queue);
    tpSetCycleTime(&emcmotDebug->queue, emcmotConfig->trajCycleTime);
    tpSetPos(&emcmotDebug->queue, emcmotStatus->pos);
    tpSetVmax(&emcmotDebug->queue, emcmotStatus->vel);
    tpSetAmax(&emcmotDebug->queue, emcmotStatus->acc);

    /* init the axis components */
    pid.p = P_GAIN;
    pid.i = I_GAIN;
    pid.d = D_GAIN;
    pid.ff0 = FF0_GAIN;
    pid.ff1 = FF1_GAIN;
    pid.ff2 = FF2_GAIN;
    pid.backlash = BACKLASH;
    pid.bias = BIAS;
    pid.maxError = MAX_ERROR;

    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	if (-1 == tpCreate(&emcmotDebug->freeAxis[axis], FREE_AXIS_QUEUE_SIZE,
		emcmotDebug->freeAxisTcSpace[axis])) {
	    rtapi_print("can't create axis emcmotDebug->queue %d\n", axis);
	    return -1;
	}
	tpInit(&emcmotDebug->freeAxis[axis]);
	tpSetCycleTime(&emcmotDebug->freeAxis[axis],
	    emcmotConfig->trajCycleTime);
	/* emcmotDebug->freePose is inited to 0's in decl */
	tpSetPos(&emcmotDebug->freeAxis[axis], emcmotDebug->freePose);
	tpSetVmax(&emcmotDebug->freeAxis[axis], emcmotStatus->vel);
	tpSetAmax(&emcmotDebug->freeAxis[axis], emcmotStatus->acc);
	pidInit(&emcmotConfig->pid[axis]);
	pidSetGains(&emcmotConfig->pid[axis], pid);
	cubicInit(&emcmotDebug->cubic[axis]);
    }

    /* init the time and rate using functions to affect traj, the pids, and
       the cubics properly, since they're coupled */
    setTrajCycleTime(TRAJ_CYCLE_TIME);
    setServoCycleTime(SERVO_CYCLE_TIME);

    extEncoderSetIndexModel(EXT_ENCODER_INDEX_MODEL_MANUAL);
    for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	emcmotDebug->rawInput[axis] = 0.0;
	emcmotDebug->rawOutput[axis] = 0.0;
	emcmotDebug->coarseJointPos[axis] = 0.0;
	emcmotDebug->jointPos[axis] = 0.0;
	emcmotDebug->jointVel[axis] = 0.0;
	emcmotStatus->axisPos[axis] = 0.0;
	emcmotDebug->oldJointPos[axis] = 0.0;
	emcmotDebug->outJointPos[axis] = 0.0;

	emcmotDebug->homingPhase[axis] = 0;
	emcmotDebug->latchFlag[axis] = 0;
	emcmotDebug->saveLatch[axis] = 0.0;

	emcmotStatus->input[axis] = 0.0;
	emcmotDebug->oldInput[axis] = 0.0;
	emcmotDebug->oldInputValid[axis] = 0;
	emcmotStatus->output[axis] = 0.0;

	emcmotDebug->jointHome[axis] = 0.0;

	extAmpEnable(axis, !GET_AXIS_ENABLE_POLARITY(axis));
    }

    emcmotStatus->tail = 0;

    rtapi_print("emcmot: initializing emcmotTask\n");

    /* set the task priority to second lowest, since we only have one task */
    emcmot_prio = rtapi_prio_next_higher(rtapi_prio_lowest());

    /* create the timer task */
    /* the second arg is an abitrary int that is passed to the timer task on
       the first iterration */
    emcmot_task =
	rtapi_task_new(emcmotController, (void *) 0, emcmot_prio, module,
	EMCMOT_TASK_STACKSIZE, RTAPI_USES_FP);
    if (emcmot_task < 0) {
	/* See rtapi.h for the error codes returned */
	rtapi_print("emcmot init: rtapi_task_new returned %d\n", emcmot_task);
	rtapi_exit(module);
	return -1;
    }
    /* start the task running */
    retval = rtapi_task_start(emcmot_task, (int) (SERVO_CYCLE_TIME * 1.0e9));
    if (retval != RTAPI_SUCCESS) {
	rtapi_print("emcmot init: rtapi_task_start returned %d\n", retval);
	rtapi_exit(module);
	return -1;
    }
    rtapi_print("emcmot init: started timer task\n");
    rtapi_print("emcmot: init_module finished\n");

    return 0;
}

void cleanup_module(void)
{
    int axis;
    int retval;

    rtapi_print("emcmot: cleanup started.\n");

    retval = rtapi_task_pause(emcmot_task);
    if (retval != RTAPI_SUCCESS) {
	rtapi_print("emcmot exit: rtapi_task_pause returned %d\n", retval);
    }
    /* Remove the task from the list */
    retval = rtapi_task_delete(emcmot_task);
    if (retval != RTAPI_SUCCESS) {
	rtapi_print("emcmot exit: rtapi_task_delete returned %d\n", retval);
    }

    /* WPS these were moved from above to avoid a possible mutex problem. */
    /* There is no point in clearing the trajectory queue since the planner
       should be dead by now anyway. */
    if (emcmotStruct != 0 && emcmotDebug != 0 && emcmotConfig != 0) {
	rtapi_print("emcmot: disabling amps\n");
	for (axis = 0; axis < EMCMOT_MAX_AXIS; axis++) {
	    extAmpEnable(axis, !GET_AXIS_ENABLE_POLARITY(axis));
	}
    }

    /* free shared memory */
    retval = rtapi_shmem_delete(shmem_mem, module);
    if (retval != RTAPI_SUCCESS) {
	rtapi_print("emcmot exit: rtapi_shmem_delete returned %d\n", retval);
    }

    rtapi_print("emcmot: cleanup finished.\n");

    /* Clean up and exit */
    rtapi_exit(module);
}
