#ifndef EMCMOTGLB_H
#define EMCMOTGLB_H

/*
  declarations for globals whose default values are found in emcmotcfg.h

  Modification history:

  13-Mar-2000 WPS added unused attribute to emcmotglb_h to avoid 'defined but not used' compiler warning.
  28-Jul-1999  FMP added NUM_AXES
  8-Jun-1999  FMP took out SM_CYCLE_TIME decl since we're now using
  servo cycle time / 2 for this
  18-Aug-1998  FMP added BIAS, MAX_ERROR
  6-Aug-1998  FMP added SM_CYCLE_TIME decl
  8-Jul-1998  FMP added EMCMOT_INIFILE decls
  5-Jun-1998  FMP added BACKLASH
  1-Apr-1998  FMP added EMCMOT_COMM_TIMEOUT, EMCMOT_COMM_WAIT
  26-Mar-1998  FMP added ifdef __cplusplus
  6-Jan-1998  FMP added PID gains, input/output offsets, base address/key
  */

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__((unused)) emcmotglb_h[] = "$Id$";

#ifdef __cplusplus
extern "C" {
#endif

#define EMCMOT_INIFILE_LEN 256
extern char EMCMOT_INIFILE[EMCMOT_INIFILE_LEN];

// extern unsigned long int SHMEM_BASE_ADDRESS;
extern unsigned int SHMEM_KEY;

extern double EMCMOT_COMM_TIMEOUT; /* seconds until timeout */
extern double EMCMOT_COMM_WAIT; /* seconds to delay between tries */

extern int NUM_AXES;

extern double TRAJ_CYCLE_TIME;
extern double SERVO_CYCLE_TIME;

extern double VELOCITY;
extern double ACCELERATION;

extern double MAX_LIMIT;
extern double MIN_LIMIT;

extern double MAX_OUTPUT;
extern double MIN_OUTPUT;

extern int TC_QUEUE_SIZE;

extern int MMXAVG_SIZE;

#if 0 
  // Moved to emcmotDebug struct
extern double tMmxavgSpace[];
extern double sMmxavgSpace[];
extern double nMmxavgSpace[];
#endif

extern double MAX_FERROR;

extern double P_GAIN;
extern double I_GAIN;
extern double D_GAIN;
extern double FF0_GAIN;
extern double FF1_GAIN;
extern double FF2_GAIN;
extern double BACKLASH;
extern double BIAS;
extern double MAX_ERROR;

extern double INPUT_SCALE;
extern double INPUT_OFFSET;
extern double OUTPUT_SCALE;
extern double OUTPUT_OFFSET;

#ifdef __cplusplus
} /* matches extern "C" at top */
#endif

#endif /* EMCMOTGLB_H */
