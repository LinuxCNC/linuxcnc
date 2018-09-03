/********************************************************************
* Description: simple_tp.c
*   A simple single axis trajectory planner.  See simple_tp.h for API.
*
* Author: jmkasunich
* License: GPL Version 2
* Created on:
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
********************************************************************/

#include "simple_tp.h"
#include "rtapi_math.h"

/***********************************************************************
*                  LOCAL VARIABLE DECLARATIONS                         *
************************************************************************/


/***********************************************************************
*                      LOCAL FUNCTION PROTOTYPES                       *
************************************************************************/



/***********************************************************************
*                        PUBLIC FUNCTION CODE                          *
************************************************************************/

void simple_tp_update(simple_tp_t *tp, double period)
{
    double max_dv, tiny_dp, pos_err, vel_req;

    tp->active = 0;
    /* compute max change in velocity per servo period */
    max_dv = tp->max_acc * period;
    /* compute a tiny position range, to be treated as zero */
    tiny_dp = max_dv * period * 0.001;
    /* calculate desired velocity */
    if (tp->enable) {
	/* planner enabled, request a velocity that tends to drive
	   pos_err to zero, but allows for stopping without position
	   overshoot */
	pos_err = tp->pos_cmd - tp->curr_pos;
	/* positive and negative errors require some sign flipping to
	   avoid rtapi_sqrt(negative) */
	if (pos_err > tiny_dp) {
	    vel_req = -max_dv +
		       rtapi_sqrt(2.0 * tp->max_acc * pos_err + max_dv * max_dv);
	    /* mark planner as active */
	    tp->active = 1;
	} else if (pos_err < -tiny_dp) {
	    vel_req =  max_dv -
		       rtapi_sqrt(-2.0 * tp->max_acc * pos_err + max_dv * max_dv);
	    /* mark planner as active */
	    tp->active = 1;
	} else {
	    /* within 'tiny_dp' of desired pos, no need to move */
	    vel_req = 0.0;
	}
    } else {
	/* planner disabled, request zero velocity */
	vel_req = 0.0;
	/* and set command to present position to avoid movement when
	   next enabled */
	tp->pos_cmd = tp->curr_pos;
    }
    /* limit velocity request */
    if (vel_req > tp->max_vel) {
        vel_req = tp->max_vel;
    } else if (vel_req < -tp->max_vel) {
	vel_req = -tp->max_vel;
    }
    /* ramp velocity toward request at accel limit */
    if (vel_req > tp->curr_vel + max_dv) {
	tp->curr_vel += max_dv;
    } else if (vel_req < tp->curr_vel - max_dv) {
	tp->curr_vel -= max_dv;
    } else {
	tp->curr_vel = vel_req;
    }
    /* check for still moving */
    if (tp->curr_vel != 0.0) {
	/* yes, mark planner active */
	tp->active = 1;
    }
    /* integrate velocity to get new position */
    tp->curr_pos += tp->curr_vel * period;
}
