/********************************************************************
* Description: emcmotglb.h
*   Declarations for globals whose default values are found in emcmotcfg.h
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
#ifndef EMCMOTGLB_H
#define EMCMOTGLB_H

#ifdef __cplusplus
extern "C" {
#endif

#define EMCMOT_INIFILE_LEN 256

/* FIXME - eventually want to convert all of these to lowercase,
   uppercase is for constants only
*/

/* FIXME - want to move some of these out of here completely...
   too many globals - put in emcmotXXX structs instead, perhaps?
*/

    extern char EMCMOT_INIFILE[EMCMOT_INIFILE_LEN];

    extern unsigned int SHMEM_KEY;

    extern double EMCMOT_COMM_TIMEOUT;	/* seconds until timeout */
    extern double EMCMOT_COMM_WAIT;	/* seconds to delay between tries */

    extern int num_axes;

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

#if 0
    extern double P_GAIN;
    extern double I_GAIN;
    extern double D_GAIN;
    extern double FF0_GAIN;
    extern double FF1_GAIN;
    extern double FF2_GAIN;
    extern double BIAS;
    extern double MAX_ERROR;
#endif
    extern double BACKLASH;

#if 0
    extern double INPUT_SCALE;
    extern double INPUT_OFFSET;
    extern double OUTPUT_SCALE;
    extern double OUTPUT_OFFSET;
#endif

#ifdef __cplusplus
}				/* matches extern "C" at top */
#endif
#endif				/* EMCMOTGLB_H */
