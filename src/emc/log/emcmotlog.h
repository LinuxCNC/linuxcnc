#ifndef EMCMOTLOG_H
#define EMCMOTLOG_H

/*
  Data logging declarations

  Modification history:

  21-Jan-2004  P.C. Moved across from the original EMC source tree.
  */

#include "posemath.h"		/* PmCartesian */

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__ ((unused)) emcmotlog_h[] =
    "$Id$";

/* max log points allowed */
/* Note: this will strongly affect the requirements for shared memory */
#define EMCMOT_LOG_MAX 10000

/* types of logged data */
enum EMCMOT_LOG_TYPE_ENUM {
    EMCMOT_LOG_TYPE_AXIS_POS = 1,	/* single axis cmd/actual pos */
    EMCMOT_LOG_TYPE_ALL_INPOS = 2,	/* all axes actual input pos */
    EMCMOT_LOG_TYPE_ALL_OUTPOS = 3,	/* all axes commanded output pos */
    EMCMOT_LOG_TYPE_CMD = 4,	/* command type and num */
    EMCMOT_LOG_TYPE_AXIS_VEL = 5,	/* single axis cmd/actual vel */
    EMCMOT_LOG_TYPE_ALL_FERROR = 6,	/* all axes following error */
    EMCMOT_LOG_TYPE_TRAJ_POS = 7,	/* Cartesian position at traj rate */
    EMCMOT_LOG_TYPE_TRAJ_VEL = 8,	/* Cartesian vel diffs at traj rate */
    EMCMOT_LOG_TYPE_TRAJ_ACC = 9,	/* Cartesian accel diffs at traj rate 
					 */
    EMCMOT_LOG_TYPE_POS_VOLTAGE = 10	/* all axes measured pos and output V 
					 */
};

enum EMCLOG_TRIGGER_TYPE {
    EMCLOG_MANUAL_TRIGGER,
    EMCLOG_DELTA_TRIGGER,
    EMCLOG_OVER_TRIGGER,
    EMCLOG_UNDER_TRIGGER
};

enum EMCLOG_TRIGGER_VAR {
    EMCLOG_TRIGGER_ON_FERROR,
    EMCLOG_TRIGGER_ON_VOLT,
    EMCLOG_TRIGGER_ON_POS,
    EMCLOG_TRIGGER_ON_VEL
};

/* various loggable structs */

/* logs commanded and actual position, single axis, per cycle */
typedef struct {
    double time;
    double output;
    double input;
} EMCMOT_LOG_AXIS_POS_STRUCT;

/* logs actual input position, all axes, per cycle */
#define EMCMOT_LOG_NUM_AXES 3	/* how many are logged */
typedef struct {
    double time;
    double input[EMCMOT_LOG_NUM_AXES];
} EMCMOT_LOG_ALL_INPOS_STRUCT;

/* logs commanded output position, all axes, per cycle */
typedef struct {
    double time;
    double output[EMCMOT_LOG_NUM_AXES];
} EMCMOT_LOG_ALL_OUTPOS_STRUCT;

/* logs commands, per new command */
typedef struct {
    double time;
    int command;
    int commandNum;
} EMCMOT_LOG_CMD_STRUCT;

/* logs axis cmd and actual vel */
typedef struct {
    double time;
    double cmdVel;
    double actVel;
} EMCMOT_LOG_AXIS_VEL_STRUCT;

/* logs all axes' following error */
typedef struct {
    double time;
    double ferror[EMCMOT_LOG_NUM_AXES];
} EMCMOT_LOG_ALL_FERROR_STRUCT;

/* logs Cartesian position at trajectory rate */
typedef struct {
    double time;
    PmCartesian pos;		/* calculated Cartesian position */
} EMCMOT_LOG_TRAJ_POS_STRUCT;

/* logs Cartesian velocity diffs at trajectory rate */
typedef struct {
    double time;
    PmCartesian vel;		/* differenced Cartesian velocity */
    double mag;
} EMCMOT_LOG_TRAJ_VEL_STRUCT;

/* logs Cartesian acceleration diffs at trajectory rate */
typedef struct {
    double time;
    PmCartesian acc;		/* differenced Cartesian acceleration */
    double mag;
} EMCMOT_LOG_TRAJ_ACC_STRUCT;

/* logs measured position and resulting output voltage */
typedef struct {
    double time;
    double pos;
    double voltage;
} EMCMOT_LOG_POS_VOLTAGE_STRUCT;

/* full emcmot_log_struct_t union */
typedef struct {
    int type;
    union {
	EMCMOT_LOG_AXIS_POS_STRUCT axisPos;
	EMCMOT_LOG_ALL_INPOS_STRUCT allInpos;
	EMCMOT_LOG_ALL_OUTPOS_STRUCT allOutpos;
	EMCMOT_LOG_CMD_STRUCT cmd;
	EMCMOT_LOG_AXIS_VEL_STRUCT axisVel;
	EMCMOT_LOG_ALL_FERROR_STRUCT allFerror;
	EMCMOT_LOG_TRAJ_POS_STRUCT trajPos;
	EMCMOT_LOG_TRAJ_VEL_STRUCT trajVel;
	EMCMOT_LOG_TRAJ_ACC_STRUCT trajAcc;
	EMCMOT_LOG_POS_VOLTAGE_STRUCT posVoltage;
    } item;
} emcmot_log_struct_t;

/* full log, with header and union of types */
typedef struct {
    int type;			/* type of data logged, as in enum above */
    int size;			/* elements of log[] array */
    int start;			/* index of start */
    int end;			/* index of end */
    int howmany;		/* how many in log */
    emcmot_log_struct_t log[EMCMOT_LOG_MAX];
} emcmot_log_t;

extern int emcmotLogInit(emcmot_log_t * log, int type, int size);
extern int emcmotLogAdd(emcmot_log_t * log, emcmot_log_struct_t val);
extern int emcmotLogGet(emcmot_log_t * log, emcmot_log_struct_t * val);

#endif /* EMCMOTLOG_H */
