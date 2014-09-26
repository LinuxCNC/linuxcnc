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

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

#include "simple_tp.h"
#include "rtapi_math.h"

static int debug1 = 1;

void simple_tp_setCycleTime(simple_tp_t *tp, double secs) {
    tp->cycle_time = secs;
}

void simple_tp_jog_cont(simple_tp_t *tp, double pos_cmd, double vel_cmd, double max_vel, double max_acc, double max_jerk) {
    tp->pos_cmd = pos_cmd;
    tp->vel_cmd = vel_cmd;
    tp->max_vel = max_vel * tp->cycle_time;
    tp->max_acc = max_acc * tp->cycle_time * tp->cycle_time;
    tp->jerk = max_jerk * tp->cycle_time * tp->cycle_time * tp->cycle_time;
    tp->target = pos_cmd - tp->curr_pos;
    tp->start_pos = tp->curr_pos;
    tp->accel_state = ACC_S0;
    tp->active = 1;
    printf("JOG -- %.4f %.4f | %.4f %.4f %.4f |  %.4f %.4f %.4f\n", pos_cmd, vel_cmd, max_vel, max_acc, max_jerk, tp->max_vel, tp->max_acc, tp->jerk);
}

void simple_tp_jog_abort(simple_tp_t *tp) {
    printf("JOG ABORT\n");
    tp->aborting = 1;
    tp->vel_cmd = 0.0;
}

double simple_tp_get_curr_vel(simple_tp_t *tp) {
    return tp->curr_vel/tp->cycle_time;
}

double calc_dist_s2_s6(simple_tp_t *tp) {
    double t, t1, vel, vel1;
    double dist;
    
    // AT = AT + JT
    // VT = V0 + A0T + 1/2 JT^2
    // PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
    // distance for S2
    t = ceil(tp->curr_acc/tp->jerk);
    dist = tp->curr_pos + tp->curr_vel * t + 0.5 * tp->curr_acc * t * t - 1.0/6.0 * tp->jerk * t * t * t;
    vel = tp->curr_vel + tp->curr_acc * t - 0.5 * tp->jerk * t * t;
    // distance for S3
    dist += vel;
    
    /* 
    0.5 * vel = vel + 0 * t1 - 0.5 * j * t1 * t1;
    t1 = sqrt(vel/j)
    */
    t = ceil(sqrt(vel/tp->jerk));
    // AT = AT + JT
    t1 = ceil(tp->max_acc / tp->jerk);   // max time for S4
    if (t > t1) {
	// S4 -> S5 -> S6
	dist += t1 * vel;    // dist of (S4 + S6)
	// calc decel.dist for ACC_S5
	// t: time for S5
	t = (vel - tp->jerk * t1 * t1) / tp->max_acc;
	vel1 = vel - 0.5 * tp->jerk * t1 * t1;
	// PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
	dist += (vel1 * t - 0.5 * tp->max_acc * t * t);
    } else {
	// S4 -> S6
	dist += t * vel;    // dist of (S4 + S6)
    }
    return dist;
}

double calc_dist_s3_s6(simple_tp_t *tp) {
    double t, t1, vel, vel1;
    double dist;
    
    // check if we will hit progress limit
    // refer to 2011-10-17 ysli design note
    // AT = AT + JT
    // VT = V0 + A0T + 1/2 JT^2
    // PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
    vel = tp->curr_vel;
    // distance for S3
    dist = tp->curr_pos + (vel);
    
    /* 
    0.5 * vel = vel + 0 * t1 - 0.5 * j * t1 * t1;
    t1 = sqrt(vel/j)
    */
    t = ceil(sqrt(vel/tp->jerk));
    // AT = AT + JT
    t1 = ceil(tp->max_acc / tp->jerk);   // max time for S4
    if (t > t1) {
	// S4 -> S5 -> S6
	dist += t1 * vel;    // dist of (S4 + S6)
	// calc decel.dist for ACC_S5
	// t: time for S5
	t = (vel - tp->jerk * t1 * t1) / tp->max_acc;
	vel1 = vel - 0.5 * tp->jerk * t1 * t1;
	// PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
	dist += (vel1 * t - 0.5 * tp->max_acc * t * t);
    } else {
	// S4 -> S6
	dist += t * vel;    // dist of (S4 + S6)
    }
    return dist;
}

double calc_dist_s4_s6(simple_tp_t *tp) {
    double t, t1, vel, vel1;
    double dist;
    
    // check if we will hit progress limit
    // refer to 2011-10-17 ysli design note
    // AT = AT + JT
    // VT = V0 + A0T + 1/2 JT^2
    // PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
    vel = tp->curr_vel;
    // distance for S3
    dist = tp->curr_pos;
    
    /* 
    0.5 * vel = vel + 0 * t1 - 0.5 * j * t1 * t1;
    t1 = sqrt(vel/j)
    */
    t = ceil(sqrt(vel/tp->jerk));
    // AT = AT + JT
    t1 = ceil(tp->max_acc / tp->jerk);   // max time for S4
    if (t > t1) {
	// S4 -> S5 -> S6
	dist += t1 * vel;    // dist of (S4 + S6)
	// calc decel.dist for ACC_S5
	// t: time for S5
	t = (vel - tp->jerk * t1 * t1) / tp->max_acc;
	vel1 = vel - 0.5 * tp->jerk * t1 * t1;
	// PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
	dist += (vel1 * t - 0.5 * tp->max_acc * t * t);
    } else {
	// S4 -> S6
	dist += t * vel;    // dist of (S4 + S6)
    }
    return dist;
}

double calc_dist_s5_s6(simple_tp_t *tp) {
    double t, t1, vel, vel1;
    double dist;

    // should we stay in S4 and keep decel?
    // calculate dist for S6 -> S4 -> (maybe S5) -> S6
    t = - tp->curr_acc / tp->jerk;
    // target dist after switching to S6 (jerk is positive for S6)
    dist = tp->curr_pos + tp->curr_vel * t 
	   + 0.5 * tp->curr_acc * t * t
	   + 1.0 / 6.0 * tp->jerk * t * t * t;
    // VT = V0 + A0T + 1/2JT2
    // obtain vel for S6 -> S3
    vel = tp->curr_vel + tp->curr_acc * t + 0.5 * tp->jerk * t * t;
    
    if (vel > 0) {    
	/* 
	0.5 * vel = vel + 0 * t1 - 0.5 * j * t1 * t1;
	t1 = sqrt(vel/j)
	*/
	t = ceil(sqrt(vel/tp->jerk));
	// AT = AT + JT
	t1 = ceil(tp->max_acc / tp->jerk);   // max time for S4
	if (t > t1) {
	    // S4 -> S5 -> S6
	    dist += t1 * vel;    // dist of (S4 + S6)
	    // calc decel.dist for ACC_S5
	    // t: time for S5
	    t = (vel - tp->jerk * t1 * t1) / tp->max_acc;
	    vel1 = vel - 0.5 * tp->jerk * t1 * t1;
	    // PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
	    dist += (vel1 * t - 0.5 * tp->max_acc * t * t);
	} else {
	    // S4 -> S6
	    dist += t * vel;    // dist of (S4 + S6)
	}
    }
    return dist;
}

#define DEBUG_AVP(state) \
  do { \
   if (debug1) \
   printf("%s | ACC %.4f%+.4f | VEL %.4f%+.4f | POS %.4f%+.4f\n", state,  \
     tp->curr_acc/tp->cycle_time/tp->cycle_time - da/tp->cycle_time/tp->cycle_time, da/tp->cycle_time/tp->cycle_time, \
     tp->curr_vel/tp->cycle_time - dv/tp->cycle_time, dv/tp->cycle_time, \
     tp->curr_pos - dp, dp); \
 } while (0);


/*
 Continuous form
 PT = P0 + V0T + 1/2A0T2 + 1/6JT3
 VT = V0 + A0T + 1/2 JT2
 AT = A0 + JT

 Discrete time form (讓 T 成為 1, 那麼 T^2 == 1, T^3 == 1)
 PT = PT + VT + 1/2AT + 1/6J
 VT = VT + AT + 1/2JT
 AT = AT + JT
 */

void simple_tp_update(simple_tp_t *tp, double period) {
    double t, t1, vel, vel1, req_vel, dist, ct;
    double da, dv, dp;
    int immediate_state;
    struct timeval tv;

    if ((tp->aborting) && (tp->curr_vel < 0.00001)) {
	tp->pos_cmd = tp->curr_pos;
	tp->curr_vel = 0;
	tp->curr_acc = 0;
	tp->vel_cmd = 0;
	tp->aborting = 0;
	tp->active = 0;
	tp->accel_state = ACC_IDLE;
        tp->s4 = 0;
    }

    gettimeofday(&tv, NULL);
    ct = tv.tv_sec + tv.tv_usec / 1000000.0;
    do {
	immediate_state = 0;
	
	switch (tp->accel_state) {
	case ACC_S0:
	    // acc > 0 
	    // jerk > 0 
	    // AT = AT + JT
	    // VT = VT + AT + 1/2JT
	    // PT = PT + VT + 1/2AT + 1/6JT

            if (tp->vel_cmd < 0.0001) {
		tp->accel_state = ACC_S4;
		DEBUG_AVP("ACC_S0");
		printf("ABORT -> VEL LIMIT -> ACC_S4\n");
		immediate_state = 1;
		break;
	    }

	    da = tp->jerk;
	    // check if we will acc limit; 
	    if ((tp->curr_acc + da) > tp->max_acc) {
		tp->curr_acc = tp->max_acc;
                dv = tp->curr_acc + 0.5 * tp->jerk;
	        tp->curr_vel += dv;
		dp = tp->curr_vel + 0.5 * tp->curr_acc + 1.0/6.0 * tp->jerk;
		tp->curr_pos += dp;

		tp->accel_state = ACC_S1;
		DEBUG_AVP("ACC_S0");
		printf("ACC LIMIT -> ACC_S1\n");
		break;
	    } else {
                tp->curr_acc += da;
	    }

            dv = tp->curr_acc + 0.5 * tp->jerk;
	    // check if we will hit velocity limit; 
	    // assume we start decel from here to "accel == 0"
	    //
	    // AT = A0 + JT (let AT = 0 to calculate T)
	    // VT = V0 + A0T + 1/2JT2
	    t = ceil(tp->curr_acc / tp->jerk);
	    vel = tp->vel_cmd * tp->cycle_time - tp->curr_acc * t + 0.5 * tp->jerk * t * t;
	    if ((tp->curr_vel + dv) > vel) {
		tp->curr_vel = vel;
                dp = tp->curr_vel + 0.5 * tp->curr_acc + 1.0/6.0 * tp->jerk;
	        tp->curr_pos += dp;
		
		tp->accel_state = ACC_S2;
		DEBUG_AVP("ACC_S0");
		printf("VEL LIMIT -> ACC_S2\n");
		break;
	    } else {
	        tp->curr_vel += dv;
	    }

            dp = tp->curr_vel + 0.5 * tp->curr_acc + 1.0/6.0 * tp->jerk;
	    tp->curr_pos += dp;
	    // check if we will hit progress limit
	    dist = calc_dist_s2_s6(tp);
	    if (dist > tp->pos_cmd) {
		tp->accel_state = ACC_S2;
		DEBUG_AVP("ACC_S0");
		printf("POS LIMIT -> ACC_S2\n");
		break;
	    }

            DEBUG_AVP("ACC_S0");
	    break;
	case ACC_S1:
	    // acc = max 
	    // jerk = 0 
	    // VT = VT + AT + 1/2JT
	    // PT = PT + VT + 1/2AT + 1/6JT

            if (tp->vel_cmd < 0.0001) {
		tp->accel_state = ACC_S4;
		DEBUG_AVP("ACC_S1");
		printf("ABORT -> VEL LIMIT -> ACC_S4\n");
		immediate_state = 1;
		break;
	    }

	    da = 0;
            dv = tp->curr_acc;
            // check if we will hit velocity limit; 
            // assume we start decel from here to "accel == 0"
            //
            // AT = A0 + JT (let AT = 0 to calculate T)
            // VT = V0 + A0T + 1/2JT2
            // t = ceil(tp->curr_acc / tp->jerk);
            t = tp->curr_acc / tp->jerk;
            vel = tp->vel_cmd * tp->cycle_time - tp->curr_acc * t + 0.5 * tp->jerk * t * t;
            if ((tp->curr_vel + dv) >= vel) {
		tp->curr_vel = vel;
		dp = tp->curr_vel + 0.5 * tp->curr_acc;
		tp->curr_pos += dp;

                tp->accel_state = ACC_S2;
		DEBUG_AVP("ACC_S1");
		printf("VEL LIMIT -> ACC_S2\n");
                break;
            } else {
		tp->curr_vel += dv;
	    }

	    dp = tp->curr_vel + 0.5 * tp->curr_acc;
	    tp->curr_pos += dp;
            // check if we will hit progress limit
	    dist = calc_dist_s2_s6(tp);
	    if (dist > tp->pos_cmd) {
		tp->accel_state = ACC_S2;
                DEBUG_AVP("ACC_S1");
		printf("POS LIMIT -> ACC_S2\n");
		break;
	    }

            DEBUG_AVP("ACC_S1");
	    break;
	case ACC_S2: 
	    // acc > 0 
	    // jerk < 0 
	    // AT = AT + JT
	    // VT = VT + AT + 1/2JT
	    // PT = PT + VT + 1/2AT + 1/6JT

            if (tp->vel_cmd < 0.0001) {
		tp->accel_state = ACC_S4;
		DEBUG_AVP("ACC_S2");
		printf("ABORT -> VEL LIMIT -> ACC_S4\n");
		immediate_state = 1;
		break;
	    }

	    da = -tp->jerk;
            if ((tp->curr_acc + da) < 0) {
		tp->curr_acc = 0.0;
                dv = tp->curr_acc + 0.5 * da;
		tp->curr_vel += dv;
		dp = tp->curr_vel + 0.5 * tp->curr_acc + 1.0/6.0 * da;
		tp->curr_pos += dp;

                tp->accel_state = ACC_S3;
                DEBUG_AVP("ACC_S2");
		printf("ACC LIMIT -> ACC_S3\n");
               break;
            } else {
	        tp->curr_acc += da;
	    }

            dv = tp->curr_acc + 0.5 * da;
            // check if we will hit velocity limit; 
            if ((tp->curr_vel + dv) > tp->vel_cmd) {
                tp->curr_vel = tp->vel_cmd;
		dp = tp->curr_vel + 0.5 * tp->curr_acc + 1.0/6.0 * da;
		tp->curr_pos += dp;

                tp->accel_state = ACC_S3;
                DEBUG_AVP("ACC_S2");
		printf("VEL LIMIT -> ACC_S3\n");
                break;
            } else {
		tp->curr_vel += dv;
	    }
	    
	    dp = tp->curr_vel + 0.5 * tp->curr_acc + 1.0/6.0 * da;
	    tp->curr_pos += dp;
	    dist = calc_dist_s3_s6(tp);
	    if (dist > tp->pos_cmd) {
		tp->accel_state = ACC_S3;
                DEBUG_AVP("ACC_S2");
		printf("POS LIMIT -> ACC_S3\n");
		break;
	    }

            DEBUG_AVP("ACC_S2");
	    break;
	case ACC_S3:
	    // acc = 0 
	    // jerk = 0 
	    // PT = PT + VT + 1/2AT + 1/6JT

            if (tp->vel_cmd < 0.0001) {
                tp->accel_state = ACC_S4;
                DEBUG_AVP("ACC_S3");
		printf("ABORT -> VEL LIMIT -> ACC_S4\n");
		immediate_state = 1;
                break;
            }

	    da = 0;
	    dv = 0;
	    tp->curr_acc = 0;

	    dist = calc_dist_s4_s6(tp);
	    if (dist > tp->pos_cmd) {
                dp = tp->curr_vel;
	        tp->curr_pos += dp;

		tp->accel_state = ACC_S4;
                DEBUG_AVP("ACC_S3");
		printf("POS LIMIT -> ACC_S4\n");
		break;
	    }

            dp = tp->curr_vel;
	    tp->curr_pos += dp;

            DEBUG_AVP("ACC_S3");
	    break;
	case ACC_S4:
	    // acc < 0 
	    // jerk < 0 
	    // AT = AT + JT
	    // VT = VT + AT + 1/2JT
	    // PT = PT + VT + 1/2AT + 1/6JT
	    if (tp->s4 == 0) {
		tp->s4 = 1;
	    }
	    
	    da = -tp->jerk;
	    if ((tp->curr_acc + da) < -tp->max_acc) {
		tp->curr_acc = -tp->max_acc;
		dv = tp->curr_acc + 0.5 * tp->jerk;
		tp->curr_vel += dv;
		dp = tp->curr_vel + 0.5 * tp->curr_acc + 1.0/6.0 * tp->jerk;
		tp->curr_pos += dp;
		
		tp->accel_state = ACC_S5;
                DEBUG_AVP("ACC_S4");
		printf("ACC LIMIT -> ACC_S5\n");
		break;
	    } else {
	        tp->curr_acc += da;
	    }
            dv = tp->curr_acc + 0.5 * tp->jerk;
	    tp->curr_vel += dv;


            if (tp->vel_cmd > 0.0) {
		dist = calc_dist_s5_s6(tp);
		printf("%.4f\n", dist);
		if (tp->target > (dist - (tp->curr_vel + 1.5 * tp->curr_acc - 2.1666667 * tp->jerk))) {
		    dp = tp->curr_vel + 0.5 * tp->curr_acc + 1.0/6.0 * tp->jerk;
		    tp->curr_pos += dp;

		    tp->accel_state = ACC_S6;
		    DEBUG_AVP("ACC_S4");
		    printf("POS LIMIT -> ACC_S6\n");
		    break;
		}
	    }

	    dp = tp->curr_vel + 0.5 * tp->curr_acc + 1.0/6.0 * tp->jerk;
	    tp->curr_pos += dp;

            DEBUG_AVP("ACC_S4");
	    break;
	case ACC_S5:
	    // acc < 0 
	    // jerk = 0 
	    // VT = VT + AT + 1/2JT
	    // PT = PT + VT + 1/2AT + 1/6JT
	    da = 0;
	    dv = tp->curr_acc;
	    tp->curr_vel += dv;

            dp = tp->curr_vel + 0.5 * tp->curr_acc;
	    tp->curr_pos += dp;
             // should we stay in S5 and keep decel?
            // calculate dist for S6 -> S4 -> (maybe S5) -> S6
            t = -tp->curr_acc / tp->jerk;
	    
	    // target dist after switching to S6 (jerk is positive for S6)
	    dist = tp->curr_pos + tp->curr_vel * t 
		   + 0.5 * tp->curr_acc * t * t
		   + 1.0 / 6.0 * tp->jerk * t * t * t;
	    // VT = V0 + A0T + 1/2JT2
	    // obtain vel for S6 -> S3
	    vel = tp->curr_vel + tp->curr_acc * t + 0.5 * tp->jerk * t * t;
	    
	    if (vel > 0) {
		/* S6 -> S3 -> S4 -> S5(maybe) -> S6 */
		/* 
		0.5 * vel = vel + 0 * t1 - 0.5 * j * t1 * t1;
		t1 = sqrt(vel/j)
		*/
		t = ceil(sqrt(vel/tp->jerk));
		// AT = AT + JT
		t1 = ceil(tp->max_acc / tp->jerk);   // max time for S4
		if (t > t1) {
		    // S4 -> S5 -> S6
		    dist += t1 * vel;    // dist of (S4 + S6)
		    // calc decel.dist for ACC_S5
		    // t: time for S5
		    t = (vel - tp->jerk * t1 * t1) / tp->max_acc;
		    vel1 = vel - 0.5 * tp->jerk * t1 * t1;
		    // PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
		    dist += (vel1 * t - 0.5 * tp->max_acc * t * t);
		} else {
		    // S4 -> S6
		    dist += t * vel;    // dist of (S4 + S6)
		}
	    }
	    
//	    printf("S5 dist = %.4f\n", dist);
	    // check if dist would be greater than tc_target at next cycle
	    if ((tp->pos_cmd > dist)) {
		tp->accel_state = ACC_S6;
                DEBUG_AVP("ACC_S5");
		printf("POS LIMIT -> ACC_S6\n");
		break;
	    }

	    
            DEBUG_AVP("ACC_S5");
	    break;
	case ACC_S6:
	    // acc < 0 
	    // jerk > 0 
	    // AT = AT + JT
	    // VT = VT + AT + 1/2JT
	    // PT = PT + VT + 1/2AT + 1/6JT
	    da = tp->jerk;
	    tp->curr_acc = tp->curr_acc + da;
	    if (tp->curr_acc > 0) {
		tp->accel_state = ACC_IDLE;
		tp->pos_cmd = tp->curr_pos;
		tp->vel_cmd = 0;
		tp->curr_vel = 0;
		tp->curr_acc = 0;
		break;
	    }
	    dv = tp->curr_acc + 0.5 * tp->jerk;
	    tp->curr_vel += dv;
	    dp = tp->curr_vel + 0.5 * tp->curr_acc + 1.0/6.0 * tp->jerk;
	    tp->curr_pos += dp;

            DEBUG_AVP("ACC_S6");
	    break;
	
	case ACC_IDLE:
	    break;
	}
    } while (immediate_state);
}
