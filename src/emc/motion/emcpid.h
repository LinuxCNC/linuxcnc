#ifndef PID_H
#define PID_H

/*
   pid.h

   Decls for PID control law data structure and class

   Modification history:

   5-Jan-2004 MGS used this file to build a motion module for emc2.
   6-Jul-2001  FMP removed decls for the useless functions pidGetCycleTime(),
   pidGetGains().
    7-Nov-2000 WPS added lastSetPoint_was_set flag.
   13-Jul-2000 WPS added some hacks surrounded by THROWAWAY_CUMULATIVE_ERROR_ON_SIGN_CHANGES and SMOOTH_D_FACTOR
   13-Mar-2000 WPS added unused attribute to pid_h to avoid 'defined but not used' compiler warning.
   24-Feb-2000  FMP added deadband
   18-Aug-1998  FMP took out reverse gains, added bias
   5-Jun-1998   FMP added reverse gains
   14-Apr-1998  FMP added backlash
   18-Dec-1997  FMP took out C++ interface
   19-Nov-1997  FMP added ff2
   16-Oct-1997  FMP changed vf, af to ff0, ff1
   15-Aug-1997  FMP added maxError
   01-Aug-1997  FMP added lastOutput to PID_STRUCT; added pidReset()
   24-Apr-1997  FMP added pidIniLoad(); removed inifile
   17-Apr-1997  FMP split into C/C++ sections
*/

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused)) emcpid_h[] = "$Id$";

/* Uncomment these lines if you like these mathematically impure effects. */
/* #define  SMOOTH_D_FACTOR */
/* #define THROWAWAY_CUMULATIVE_ERROR_ON_SIGN_CHANGES */

typedef struct
{
  double p;                     /* proportional gain, volts/unit */
  double i;                     /* integral gain, volts/unit */
  double d;                     /* derivative gain, volts/unit per sec */
  double ff0;                   /* 0th order feedforward, volts/unit */
  double ff1;                   /* 1st order feedforward, volts/unit/sec */
  double ff2;                   /* 2nd order feedforward, volts/unit/sec^2 */
  double backlash;              /* backlash */
  double bias;                  /* bias for gravity comp, for example */
  double maxError;              /* ceiling for cumError */
  double deadband;              /* delta below which error is deemed to be 0 */
  double cycleTime;             /* copy of arg to setCycleTime() */
  double error;                 /* error this cycle */
  double lastError;             /* last error computed */
  double lastSetpoint;          /* last setpoint received */
  double lastOutput;            /* last value output */
  double lastSetpointDot;       /* delta setpoint / delta t */
  double cumError;              /* cumulative error, all cycles */
  int configured;               /* configure flags */
#ifdef SMOOTH_D_FACTOR
#define MAX_ERROR_WINDOW 4
  double oldErrors[MAX_ERROR_WINDOW]; /* This is a ring buffer that stores the
                                         last MAX_ERROR_WINDOW errors. */
  double errorWindowFactors[MAX_ERROR_WINDOW];/* Weights to use in smoothing
                                                 error factors. */
  int errorIndex; /* Index into the oldErrors ring buffer. */
#endif
  int lastSetpoint_was_set; /* true when lastSetpoint is valid. */

} PID_STRUCT;

extern int pidInit(PID_STRUCT * pid);
extern int pidReset(PID_STRUCT * pid);
extern int pidSetCycleTime(PID_STRUCT * pid, double seconds);
extern int pidSetGains(PID_STRUCT * pid, PID_STRUCT parameters);
extern double pidRunCycle(PID_STRUCT * pid, double sample, double setpoint);
extern int pidIniLoad(PID_STRUCT * pid, const char * filename);

#endif /* PID_H */
