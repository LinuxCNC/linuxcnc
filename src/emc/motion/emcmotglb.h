#ifndef EMCMOTGLB_H
#define EMCMOTGLB_H

/*
  declarations for globals whose default values are found in emcmotcfg.h

  Modification history:

  21-Jan-2004  P.C. Moved across from the original EMC source tree.
  */

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__ ((unused)) emcmotglb_h[] =
    "$Id$";

#ifdef __cplusplus
extern "C" {
#endif

#define EMCMOT_INIFILE_LEN 256
    extern char EMCMOT_INIFILE[EMCMOT_INIFILE_LEN];

    extern unsigned int SHMEM_KEY;

    extern double EMCMOT_COMM_TIMEOUT;	/* seconds until timeout */
    extern double EMCMOT_COMM_WAIT;	/* seconds to delay between tries */

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
}				/* matches extern "C" at top */
#endif
#endif				/* EMCMOTGLB_H */
