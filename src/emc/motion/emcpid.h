// NOTE: emcpid.h is no longer used by emc2, the PID function is
// supplied by the HAL when needed.  Stepper based systems don't
// use PID at all.  This file will eventually be deleted, but is
// being kept for reference for now.
/********************************************************************
* Description: emcpid.h
*   Decls for PID control law data structure and class
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
* $Revision$
* $Author$
* $Date$
********************************************************************/
#ifndef PID_H
#define PID_H

/* Uncomment these lines if you like these mathematically impure effects. */
/* #define  SMOOTH_D_FACTOR */
/* #define THROWAWAY_CUMULATIVE_ERROR_ON_SIGN_CHANGES */

typedef struct {
    double p;			/* proportional gain, volts/unit */
    double i;			/* integral gain, volts/unit */
    double d;			/* derivative gain, volts/unit per sec */
    double ff0;			/* 0th order feedforward, volts/unit */
    double ff1;			/* 1st order feedforward, volts/unit/sec */
    double ff2;			/* 2nd order feedforward, volts/unit/sec^2 */
    double backlash;		/* backlash */
    double bias;		/* bias for gravity comp, for example */
    double maxError;		/* ceiling for cumError */
    double deadband;		/* delta below which error is deemed to be 0 */
    double cycleTime;		/* copy of arg to setCycleTime() */
    double error;		/* error this cycle */
    double lastError;		/* last error computed */
    double lastSetpoint;	/* last setpoint received */
    double lastOutput;		/* last value output */
    double lastSetpointDot;	/* delta setpoint / delta t */
    double cumError;		/* cumulative error, all cycles */
    int configured;		/* configure flags */
#ifdef SMOOTH_D_FACTOR
#define MAX_ERROR_WINDOW 4
    double oldErrors[MAX_ERROR_WINDOW];	/* This is a ring buffer that stores
					   the last MAX_ERROR_WINDOW errors. */
    double errorWindowFactors[MAX_ERROR_WINDOW];	/* Weights to use in
							   smoothing error
							   factors. */
    int errorIndex;		/* Index into the oldErrors ring buffer. */
#endif
    int lastSetpoint_was_set;	/* true when lastSetpoint is valid. */

} PID_STRUCT;

extern int pidInit(PID_STRUCT * pid);
extern int pidReset(PID_STRUCT * pid);
extern int pidSetCycleTime(PID_STRUCT * pid, double seconds);
extern int pidSetGains(PID_STRUCT * pid, PID_STRUCT parameters);
extern double pidRunCycle(PID_STRUCT * pid, double sample, double setpoint);
extern int pidIniLoad(PID_STRUCT * pid, const char *filename);

#endif /* PID_H */
