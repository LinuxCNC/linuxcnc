#ifndef MMXAVG_H
#define MMXAVG_H

/*
  mmxavg.h

  Min-max-average code

Modifications:

  5-Jan-2004 MGS used this file to build a motion module for emc2.
  13-Mar-2000 WPS added unused attribute to mmxavg_h to avoid 'defined but not used' compiler warning.
  */

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__ ((unused)) mmxavg_h[] =
    "$Id$";

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
