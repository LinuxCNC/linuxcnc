#ifndef USRMOTINTF_H
#define USRMOTINTF_H

/*
   usrmotintf.h

   Decls for interface functions (init, exit, read, write) for user
   processes which communicate with the real-time motion controller
   in emcmot.c

   Modification history:

  21-Jan-2004  P.C. Moved across from the original EMC source tree.
*/

#include "motion.h"		/* EMCMOT_STATUS,CMD */

/* ident tag */
#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

static char __attribute__ ((unused)) usrmotintf_h[] =
    "$Id$";

#ifdef __cplusplus
extern "C" {
#if 0
}
#endif
#endif
/* usrmotIniLoad() loads params (SHMEM_KEY, SHMEM_BASE_ADDRESS) from
   named ini file */ extern int usrmotIniLoad(const char *file);

/* usrmotReadEmcmotStatus() gets the status info out of
   the emcmot controller and puts it in arg */
extern int usrmotReadEmcmotStatus(EMCMOT_STATUS * s);

/* usrmotReadEmcmotConfig() gets the config info out of
   the emcmot controller and puts it in arg */
extern int usrmotReadEmcmotConfig(EMCMOT_CONFIG * s);

/* usrmotReadEmcmotDebug() gets the debug info out of
   the emcmot controller and puts it in arg */
extern int usrmotReadEmcmotDebug(EMCMOT_DEBUG * s);

/* usrmotReadEmcmotError() gets the earliest queued error string out of
   the emcmot controller and puts it in arg */
extern int usrmotReadEmcmotError(char *e);

/* usrmotPrintEmcmotStatus() prints the status in s, using which
   arg to select sub-prints */
extern void usrmotPrintEmcmotStatus(EMCMOT_STATUS s, int which);

/* usrmotPrintEmcmotConfig() prints the config in s, using which
   arg to select sub-prints */
extern void usrmotPrintEmcmotConfig(EMCMOT_CONFIG s, int which);

/* usrmotPrintEmcmotDebug() prints the debug in s, using which
   arg to select sub-prints */
extern void usrmotPrintEmcmotDebug(EMCMOT_DEBUG s, int which);

/* values returned by usrmotWriteEmcmotCommand; negative values
   are all errors */
#define EMCMOT_COMM_OK 0	/* went through and honored */
#define EMCMOT_COMM_ERROR_CONNECT -1	/* can't even connect */
#define EMCMOT_COMM_ERROR_TIMEOUT -2	/* connected, but send timeout */
#define EMCMOT_COMM_ERROR_COMMAND -3	/* sent, but can't run command now */
#define EMCMOT_COMM_SPLIT_READ_TIMEOUT -4	/* can't read without split */

/* usrmotWriteEmcmotCommand() writes the command to the emcmot process.
   Return values are as per the #defines above */
extern int usrmotWriteEmcmotCommand(EMCMOT_COMMAND * c);

/* usrmotInit() initializes communication with the emcmot process */
extern int usrmotInit(void);

/* usrmotExit() terminates communication with the emcmot process */
extern int usrmotExit(void);

/* usrmotDumpLog() dumps the logged data (if active) from the usrmot
   process into the named filename */
extern int usrmotDumpLog(const char *filename, int include_header);

/* usrmotLoadComp() loads the compensation data in file into the axis */
extern int usrmotLoadComp(int axis, const char *file);

/* usrmotAlter() loads the alter value to modify the axis position */
extern int usrmotAlter(int axis, double alter);

/* usrmotQueryAlter() sets the alter ptr to the current alter value */
extern int usrmotQueryAlter(int axis, double *alter);

/* usrmotPrintComp() prints the axis compensation data for axis */
extern int usrmotSetIOWriteCount(unsigned short int count);
extern int usrmotSetIOReadCount(unsigned short int count);
extern int usrmotWriteIO(int index, unsigned char val);
extern unsigned char usrmotReadIO(int index);

#ifdef __cplusplus
}
#endif

#endif /* USRMOTINTF_H */
