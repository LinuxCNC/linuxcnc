/********************************************************************
* Description: mmxavg.h
*   Min-max-average code
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
#ifndef MMXAVG_H
#define MMXAVG_H

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct {
	double sum;		/* total */
	double *nums;		/* array for stored values */
	int index;		/* index into array for next spot */
	int in;			/* how many in array */
	int size;		/* size of array; 0 <= index < max */
	int inited;		/* non-zero means at least one value in */
	double min, max;	/* saved min, max values */
    } MMXAVG_STRUCT;

/* initializes vars */
    extern int mmxavgInit(MMXAVG_STRUCT * mmxavg, double *space, int size);

/* re-initializes vars, leaves space alone */
    extern int mmxavgReset(MMXAVG_STRUCT * mmxavg);

/* adds a point to the mmxavg structure */
    extern int mmxavgAdd(MMXAVG_STRUCT * mmxavg, double num);

/* returns the min */
    extern double mmxavgMin(MMXAVG_STRUCT * mmxavg);

/* returns the max */
    extern double mmxavgMax(MMXAVG_STRUCT * mmxavg);

/* returns the average */
    extern double mmxavgAvg(MMXAVG_STRUCT * mmxavg);

#ifdef __cplusplus
}
#endif
#endif				/* MMXAVG_H */
