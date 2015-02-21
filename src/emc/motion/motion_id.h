// spin out MOTION_ID related #defines to limit coupling between tp and motion


#ifndef MOTION_ID_H
#define MOTION_ID_H

#include "rtapi_limits.h"
// define a special value to denote an invalid motion ID
// NB: do not ever generate a motion id of  MOTION_INVALID_ID
// this should be really be tested for in command.c

#define MOTION_INVALID_ID INT_MIN
#define MOTION_ID_VALID(x) ((x) != MOTION_INVALID_ID)

#define MOTION_PAUSED_RETURN_MOVE (MOTION_INVALID_ID+10)
#define MOTION_PAUSED_JOG_MOVE (MOTION_INVALID_ID+11)

#endif	/* MOTION_ID_H */
