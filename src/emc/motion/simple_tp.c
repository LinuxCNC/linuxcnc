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

static inline double stp_clamp(double val, double limit)
{
    if (val > limit) return limit;
    if (val < -limit) return -limit;
    return val;
}

void simple_tp_update(simple_tp_t *tp, double period)
{
    double max_dv, tiny_dp, pos_err, vel_req;

    tp->active = 0;
    /* compute max change in velocity per servo period */
    max_dv = tp->max_acc * period;
    /* compute a tiny position range, to be treated as zero */
    tiny_dp = TINY_DP(tp->max_acc, period);
    /* calculate desired velocity */
    if (tp->enable) {
	/* planner enabled, request a velocity that tends to drive
	   pos_err to zero, but allows for stopping without position
	   overshoot */
	pos_err = tp->pos_cmd - tp->curr_pos;

	/* When jerk-limited, stopping distance is longer than for
	   bang-bang because deceleration takes time to ramp up.
	   Additionally, if curr_acc is in the same direction as
	   curr_vel (speeding up or not yet decelerating), we must
	   first ramp it to zero before building decel, which consumes
	   extra distance and velocity. */
	double extra_dist = 0.0;
	double extra_vel = 0.0;
	double k = 0.0;
	if (tp->max_jerk > 0.0) {
	    k = tp->max_acc * tp->max_acc / tp->max_jerk;

	    /* Compute residual acceleration that opposes stopping.
	       If curr_vel > 0 and curr_acc > 0: accelerating away from stop
	       If curr_vel < 0 and curr_acc < 0: accelerating away from stop
	       In both cases, |curr_acc| must be ramped to 0 first. */
	    double a_residual = 0.0;
	    if (tp->curr_vel > 0.0 && tp->curr_acc > 0.0) {
		a_residual = tp->curr_acc;
	    } else if (tp->curr_vel < 0.0 && tp->curr_acc < 0.0) {
		a_residual = -tp->curr_acc;
	    }
	    if (a_residual > 0.0) {
		/* Time to zero out residual: t_r = a_residual / max_jerk
		   Extra velocity gained: a_residual² / (2*max_jerk)
		   Extra distance: |curr_vel|*t_r + a_residual³/(6*j²) */
		double t_r = a_residual / tp->max_jerk;
		extra_vel = 0.5 * a_residual * t_r;
		extra_dist = fabs(tp->curr_vel) * t_r
		           + a_residual * t_r * t_r / 6.0;
	    }
	}
	/* Combined offset for quadratic solve (discrete-time + jerk).
	   From d_stop = v²/(2a) + v*a/j, solving for v gives:
	   v = -k + sqrt(k² + 2*a*d) where k = a²/j */
	double offset = max_dv + k + extra_vel;

	/* positive and negative errors require some sign flipping to
	   avoid sqrt(negative) */
	if (pos_err > tiny_dp) {
	    double d = pos_err - extra_dist;
	    if (d < tiny_dp) d = tiny_dp;
	    vel_req = -offset +
		       sqrt(2.0 * tp->max_acc * d + offset * offset);
	    /* mark planner as active */
	    tp->active = 1;
	} else if (pos_err < -tiny_dp) {
	    double d = -pos_err - extra_dist;
	    if (d < tiny_dp) d = tiny_dp;
	    vel_req =  offset -
		       sqrt(2.0 * tp->max_acc * d + offset * offset);
	    /* mark planner as active */
	    tp->active = 1;
	} else {
	    /* within 'tiny_dp' of desired pos, no need to move */
	    vel_req = 0.0;
	    tp->curr_acc = 0.0;
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

    /* compute desired acceleration */
    double acc_req = (vel_req - tp->curr_vel) / period;
    acc_req = stp_clamp(acc_req, tp->max_acc);

    if (tp->max_jerk > 0.0) {
	/* jerk-limited: ramp acceleration toward acc_req */
	double max_da = tp->max_jerk * period;
	double acc_err = acc_req - tp->curr_acc;
	tp->curr_acc += stp_clamp(acc_err, max_da);
	/* clamp acceleration to max_acc (safety) */
	tp->curr_acc = stp_clamp(tp->curr_acc, tp->max_acc);
    } else {
	/* no jerk limit: bang-bang acceleration (original behavior) */
	tp->curr_acc = acc_req;
    }

    /* update velocity from acceleration */
    tp->curr_vel += tp->curr_acc * period;
    /* clamp velocity */
    tp->curr_vel = stp_clamp(tp->curr_vel, tp->max_vel);

    /* check for still moving */
    if (tp->curr_vel != 0.0 || tp->curr_acc != 0.0) {
	tp->active = 1;
    }
    /* integrate velocity to get new position */
    tp->curr_pos += tp->curr_vel * period;
}
