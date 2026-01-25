/********************************************************************
* Description: usrmotintf.h
*   Decls for interface functions (init, exit, read, write) for user
*   processes which communicate with the real-time motion controller
*   in emcmot.c
*
*   Derived from a work by Fred Proctor & Will Shackleford
*
* Author:
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
********************************************************************/
#ifndef USRMOTINTF_H
#define USRMOTINTF_H

struct emcmot_status_t;
struct emcmot_command_t;
struct emcmot_config_t;
struct emcmot_internal_t;
struct emcmot_error_t;
struct TP_STRUCT;

#ifdef __cplusplus
extern "C" {
#endif

/* usrmotIniLoad() loads params (SHMEM_KEY) from
   named INI file */
    extern int usrmotIniLoad(const char *file);

/* usrmotReadEmcmotStatus() gets the status info out of
   the emcmot controller and puts it in arg */
    extern int usrmotReadEmcmotStatus(emcmot_status_t * s);

/* usrmotReadEmcmotConfig() gets the config info out of
   the emcmot controller and puts it in arg */
    extern int usrmotReadEmcmotConfig(emcmot_config_t * s);

/* usrmotReadEmcmotInternal() gets the Internal info out of
   the emcmot controller and puts it in arg */
    extern int usrmotReadEmcmotInternal(emcmot_internal_t * s);

/* usrmotGetEmcmotInternal() returns a direct pointer to the internal
   structure in shared memory (for 9D planner optimization).
   Returns NULL if shared memory is not initialized. */
    extern emcmot_internal_t *usrmotGetEmcmotInternal(void);

/* usrmotGetEmcmotStatus() returns a direct pointer to the status
   structure in shared memory.
   Returns NULL if shared memory is not initialized. */
    extern emcmot_status_t *usrmotGetEmcmotStatus(void);

/* usrmotGetTPDataPtr() returns a direct pointer to the trajectory
   planner structure (coord_tp) in shared memory. This enables
   dual-layer architecture where userspace calls TP functions directly.
   Returns NULL if shared memory is not initialized.
   WARNING: This is the actual TP struct shared with RT - use carefully! */
    extern TP_STRUCT *usrmotGetTPDataPtr(void);

/* usrmotGetEmcmotConfig() returns a direct pointer to the config
   structure in shared memory (for TP global initialization).
   Returns NULL if shared memory is not initialized. */
    extern emcmot_config_t *usrmotGetEmcmotConfig(void);

/* usrmotReadEmcmotError() gets the earliest queued error string out of
   the emcmot controller and puts it in arg */
    extern int usrmotReadEmcmotError(char *e);

/* usrmotPrintEmcmotStatus() prints the status in s, using which
   arg to select sub-prints */
    extern void usrmotPrintEmcmotStatus(emcmot_status_t *s, int which);

/* usrmotPrintEmcmotConfig() prints the config in s, using which
   arg to select sub-prints */
    extern void usrmotPrintEmcmotConfig(emcmot_config_t s, int which);

/* values returned by usrmotWriteEmcmotCommand; negative values
   are all errors */
#define EMCMOT_COMM_OK 0	/* went through and honored */
#define EMCMOT_COMM_ERROR_CONNECT -1	/* can't even connect */
#define EMCMOT_COMM_ERROR_TIMEOUT -2	/* connected, but send timeout */
#define EMCMOT_COMM_ERROR_COMMAND -3	/* sent, but can't run command now */
#define EMCMOT_COMM_SPLIT_READ_TIMEOUT -4	/* can't read without split */
#define EMCMOT_COMM_INVALID_MOTION_ID -5 /* do not queue a motion id MOTION_INVALID_ID */

/* usrmotWriteEmcmotCommand() writes the command to the emcmot process.
   Return values are as per the #defines above */
    extern int usrmotWriteEmcmotCommand(emcmot_command_t * c);

/* usrmotInit() initializes communication with the emcmot process */
    extern int usrmotInit(const char *name);

/* usrmotExit() terminates communication with the emcmot process */
    extern int usrmotExit(void);

/* usrmotLoadComp() loads the compensation data in file into the joint */
    extern int usrmotLoadComp(int joint, const char *file, int type);

/* usrmotPrintComp() prints the joint compensation data for the specified joint */
    extern int usrmotPrintComp(int joint);

#ifdef __cplusplus
}
#endif
#endif				/* USRMOTINTF_H */
