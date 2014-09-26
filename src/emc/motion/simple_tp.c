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

#include "simple_tp.h"
#include "rtapi_math.h"

void simple_tp_setCycleTime(simple_tp_t *tp, double secs) {
    tp->cycle_time = secs;
}

void simple_tp_jog_cont(simple_tp_t *tp, double pos_cmd, double vel_cmd, double max_vel, double max_acc, double max_jerk) {
    tp->pos_cmd = pos_cmd;
    if (pos_cmd > tp->curr_pos) {
	tp->dir = 1;
    } else {
	tp->dir = -1;
    }
    tp->vel_cmd = vel_cmd;
    tp->max_vel = max_vel * tp->cycle_time;
    tp->max_acc = max_acc * tp->cycle_time * tp->cycle_time;
    tp->jerk = max_jerk * tp->cycle_time * tp->cycle_time * tp->cycle_time;
    tp->target = pos_cmd - tp->curr_pos;
    tp->start_pos = tp->curr_pos;
    tp->accel_state = ACC_S3;
    tp->active = 1;
    tp->feed_override = 1.0;
    printf("JOG -- %.4f %.4f | %.4f %.4f %.4f |  %.4f %.4f %.4f\n", pos_cmd, vel_cmd, max_vel, max_acc, max_jerk, tp->max_vel, tp->max_acc, tp->jerk);
}

void simple_tp_jog_abort(simple_tp_t *tp) {
    printf("JOG ABORT\n");
    tp->aborting = 1;
    tp->feed_override = 0.0;
}

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
    double t, t1, vel, acc, v1, dist, req_vel;
    int immediate_state;
    static double ts, ti;
    static double k, s6_a, s6_v, s6_p, error_d, prev_s, prev_v;
    static double c1, c2, c3, c4, c5, c6;

    double pi = 3.14159265359;

    if ((tp->aborting) && (tp->curr_vel < 0.00001)) {
	tp->pos_cmd = tp->curr_pos;
	tp->curr_vel = 0;
	tp->curr_acc = 0;
	tp->vel_cmd = 0;
	tp->aborting = 0;
	tp->active = 0;
    }

    immediate_state = 0;
    do {
        switch (tp->accel_state) {
        case ACC_S0:
            printf("ACC_S0 -- %.4f %.4f %.4f\n", tp->curr_pos, tp->curr_vel, tp->curr_acc);
            // AT = AT + JT
            // VT = VT + AT + 1/2JT
            // PT = PT + VT + 1/2AT + 1/6JT
            tp->curr_acc += tp->jerk;
            tp->curr_vel += tp->curr_acc + 0.5 * tp->jerk;
            tp->curr_pos += tp->dir * tp->curr_vel + 0.5 * tp->curr_acc + 1.0/6.0 * tp->jerk;
            
            // check if we hit accel limit at next BP
            if ((tp->curr_acc + tp->jerk) >= tp->max_acc) {
                tp->curr_acc = tp->max_acc;
                tp->accel_state = ACC_S1;
                break;
            }
            
            // check if we will hit velocity limit; 
            // assume we start decel from here to "accel == 0"
            //
            // AT = A0 + JT (let AT = 0 to calculate T)
            // VT = V0 + A0T + 1/2JT2
            t = ceil(tp->curr_acc / tp->jerk);
            req_vel = tp->vel_cmd * tp->feed_override * tp->cycle_time;
            if (req_vel > tp->max_vel) {
                req_vel = tp->max_vel;
            }
            vel = req_vel - tp->curr_acc * t + 0.5 * tp->jerk * t * t;
            if (tp->curr_vel >= vel) {
                tp->accel_state = ACC_S2;
                break;
            }

            // check if we will hit progress limit
            // AT = AT + JT
            // VT = V0 + A0T + 1/2 JT^2
            // PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
            // distance for S2
            t = ceil(tp->curr_acc/tp->jerk);
            dist = tp->curr_pos + tp->curr_vel * t + 0.5 * tp->curr_acc * t * t
                   - 1.0/6.0 * tp->jerk * t * t * t;
            vel = tp->curr_vel + tp->curr_acc * t - 0.5 * tp->jerk * t * t;
            // distance for S3
            dist += (vel);
            
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
                v1 = vel - 0.5 * tp->jerk * t1 * t1;
                // PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
                dist += (v1 * t - 0.5 * tp->max_acc * t * t);
            } else {
                // S4 -> S6
                dist += t * vel;    // dist of (S4 + S6)
            }

            if (tp->dir == 1) {
		if (tp->target < dist) {
                    tp->accel_state = ACC_S2;
                    break;
                }
	    } else if (tp->dir == -1) {
                if (tp->target > dist) {
                    tp->accel_state = ACC_S2;
                    break;
                }
	    }

            break;
        
        case ACC_S1:
            printf("ACC_S1 -- %.4f %.4f %.4f\n", tp->curr_pos, tp->curr_vel, tp->curr_acc);
            // jerk is 0 at this state
            // VT = VT + AT + 1/2JT
            // PT = PT + VT + 1/2AT + 1/6JT
            tp->curr_vel = tp->curr_vel + tp->curr_acc;
            tp->curr_pos = tp->curr_pos + tp->dir * tp->curr_vel + 0.5 * tp->curr_acc;
            
            // check if we will hit velocity limit; 
            // assume we start decel from here to "accel == 0"
            //
            // AT = A0 + JT (let AT = 0 to calculate T)
            // VT = V0 + A0T + 1/2JT2
            // t = ceil(tp->curr_acc / tp->jerk);
            t = tp->curr_acc / tp->jerk;
            req_vel = tp->vel_cmd * tp->feed_override * tp->cycle_time;
            if (req_vel > tp->max_vel) {
                req_vel = tp->max_vel;
            }
            vel = req_vel - tp->curr_acc * t + 0.5 * tp->jerk * t * t;
            if (tp->curr_vel >= vel) {
                tp->accel_state = ACC_S2;
                break;
            }
            
            // check if we will hit progress limit
            // distance for S2
            t = ceil(tp->curr_acc/tp->jerk);
            dist = tp->curr_pos + tp->curr_vel * t + 0.5 * tp->curr_acc * t * t
                   - 1.0/6.0 * tp->jerk * t * t * t;
            vel = tp->curr_vel + tp->curr_acc * t - 0.5 * tp->jerk * t * t;
            // distance for S3
            dist += (vel);
            
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
                v1 = vel - 0.5 * tp->jerk * t1 * t1;
                // PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
                dist += (v1 * t - 0.5 * tp->max_acc * t * t);
            } else {
                // S4 -> S6
                dist += t * vel;    // dist of (S4 + S6)
            }

            if (tp->dir == 1) {
		if (tp->target < dist) {
                    tp->accel_state = ACC_S2;
                    break;
                }
	    } else if (tp->dir == -1) {
                if (tp->target > dist) {
                    tp->accel_state = ACC_S2;
                    break;
                }
	    }
            break;
      
        
        case ACC_S2: 
            printf("ACC_S2 -- %.4f %.4f %.4f\n", tp->curr_pos, tp->curr_vel, tp->curr_acc);
            // to DECELERATE to ACCEL==0
            
            // AT = AT + JT
            // VT = VT + AT + 1/2JT
            // PT = PT + VT + 1/2AT + 1/6JT
            tp->curr_acc = tp->curr_acc - tp->jerk;
            tp->curr_vel = tp->curr_vel + tp->curr_acc - 0.5 * tp->jerk;
            tp->curr_pos = tp->curr_pos + tp->dir * tp->curr_vel + 0.5 * tp->curr_acc - 1.0/6.0 * tp->jerk;

            // check if (accel <= 0) at next BP
            acc = tp->curr_acc - tp->jerk;
            if (acc <= 0) {
                tp->accel_state = ACC_S3;
                break;
            }

            // check if we will hit velocity limit at next BP
            req_vel = tp->vel_cmd * tp->feed_override * tp->cycle_time;
            if (req_vel > tp->max_vel) {
                req_vel = tp->max_vel;
            }
            // vel: velocity at next BP 
            vel = tp->curr_vel + tp->curr_acc - 1.5 * tp->jerk;
            if (vel > req_vel) {
                tp->curr_vel = req_vel;
                tp->accel_state = ACC_S3;
                break;
            } 
            
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
                v1 = vel - 0.5 * tp->jerk * t1 * t1;
                // PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
                dist += (v1 * t - 0.5 * tp->max_acc * t * t);
            } else {
                // S4 -> S6
                dist += t * vel;    // dist of (S4 + S6)
            }

            if (tp->dir == 1) {
		if (tp->target < dist) {
                    tp->accel_state = ACC_S3;
                    break;
                }
	    } else if (tp->dir == -1) {
                if (tp->target > dist) {
                    tp->accel_state = ACC_S3;
                    break;
                }
	    }

            break;
        
        case ACC_S3:
            printf("ACC_S3 -- %.4f %.4f %.4f\n", tp->curr_pos, tp->curr_vel, tp->curr_acc);
            // PT = PT + VT + 1/2AT + 1/6JT
            // , where (jerk == 0) and (accel == 0)
            tp->curr_acc = 0;
            tp->curr_pos = tp->curr_pos + tp->dir * tp->curr_vel;
            
            // check if we will hit progress limit
            // refer to 2011-10-17 ysli design note
            // AT = AT + JT
            // VT = V0 + A0T + 1/2 JT^2
            // PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
            /* 
            0.5 * vel = vel + 0 * t1 - 0.5 * j * t1 * t1;
            t1 = sqrt(vel/j)
            */
            vel = tp->curr_vel;
            // t = floor(sqrt(vel/tp->jerk) - 0.5);
            t = sqrt(vel/tp->jerk);
            // t = sqrt(vel/tp->jerk);
            // AT = AT + JT
            // t1 = floor(tp->max_acc / tp->jerk - 0.5);   // max time for S4
            t1 = tp->max_acc / tp->jerk;   // max time for S4
            // t1 = tp->max_acc / tp->jerk;   // max time for S4
            if (tp->dir == 1) {
		if (t > t1) {
		    // S4 -> S5 -> S6
		    dist = tp->curr_pos + t1 * vel;    // dist of (S4 + S6)
		    // calc decel.dist for ACC_S5
		    // t: time for S5
		    // t = floor((vel - tp->max_acc * t1) / tp->max_acc - 0.5);
		    t = (vel - tp->max_acc * t1) / tp->max_acc - 0.5;
		    // t = (vel - tp->max_acc * t1) / tp->max_acc;
		    v1 = vel - 0.5 * tp->jerk * t1 * t1;
		    // PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
		    dist += (v1 * t - 0.5 * tp->max_acc * t * t);
		} else {
		    // S4 -> S6
		    dist = tp->curr_pos + t * vel;    // dist of (S4 + S6)
		}

//                printf("%.4f %.4f %.4f %.4f\n", tp->curr_pos, tp->curr_vel, dist, vel);
		if ((tp->curr_vel != 0) && (tp->target < (dist - vel))) {
		    tp->accel_state = ACC_S4;
		    // blending at largest velocity for G64 w/o P<tolerance>
		    if (!tp->tolerance) {
			tp->tolerance = tp->target - tp->curr_pos; // tp->distance_to_go
		    } 
		    break;
		}
	    } else if (tp->dir == -1) {
		if (t > t1) {
		    // S4 -> S5 -> S6
		    dist = tp->start_pos - t1 * vel;    // dist of (S4 + S6)
		    // calc decel.dist for ACC_S5
		    // t: time for S5
		    // t = floor((vel - tp->max_acc * t1) / tp->max_acc - 0.5);
		    t = (vel - tp->max_acc * t1) / tp->max_acc - 0.5;
		    // t = (vel - tp->max_acc * t1) / tp->max_acc;
		    v1 = vel - 0.5 * tp->jerk * t1 * t1;
		    // PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
		    dist -= (v1 * t - 0.5 * tp->max_acc * t * t);
		} else {
		    // S4 -> S6
		    dist = tp->start_pos - t * vel;    // dist of (S4 + S6)
		}

//                printf("%.4f %.4f %.4f %.4f\n", tp->curr_pos, tp->curr_vel, dist, vel);
		// check if dist would be greater than tc_target at next cycle
		if ((tp->curr_vel != 0) && (-tp->target > (tp->curr_pos + dist - vel))) {
		    tp->accel_state = ACC_S4;
		    // blending at largest velocity for G64 w/o P<tolerance>
		    if (!tp->tolerance) {
			tp->tolerance = tp->target - tp->curr_pos; // tp->distance_to_go
		    } 
		    break;
		}
	    }


            // check for changes of feed_override and request-velocity
            req_vel = tp->vel_cmd * tp->feed_override  * tp->cycle_time;
            if (req_vel > tp->max_vel) {
                req_vel = tp->max_vel;
            }
	    if (tp->active == 0) {
		break;
	    }

            if ((tp->curr_vel + 1.5 * tp->jerk) < req_vel) {
                tp->accel_state = ACC_S0;
                break;
            } else if ((tp->curr_vel - 1.5 * tp->jerk) > req_vel) {
                tp->accel_state = ACC_S4;
                break;
            }
            tp->curr_vel = req_vel;
            break;


        case ACC_S4:
            // AT = AT + JT
            // VT = VT + AT + 1/2JT
            // PT = PT + VT + 1/2AT + 1/6JT

            printf("ACC 4 -- %.4f %.4f %.4f\n", tp->curr_pos, tp->curr_vel, tp->curr_acc);

            tp->curr_acc = tp->curr_acc - tp->jerk;
            tp->curr_vel = tp->curr_vel + tp->curr_acc - 0.5 * tp->jerk;
            if (tp->curr_vel <= 0) {
                tp->curr_vel = 0;
                tp->accel_state = ACC_S3;
                break;
            }
            tp->curr_pos = tp->curr_pos + tp->dir * tp->curr_vel + 0.5 * tp->curr_acc - 1.0/6.0 * tp->jerk;

            // (accel < 0) and (jerk < 0)
           
            //printf("ACC 4 -- %.4f %.4f %.4f\n", tp->curr_acc, tp->jerk, -tp->max_acc);
            // check if we hit accel limit at next BP
            if ((tp->curr_acc - tp->jerk) <= -tp->max_acc) {
                tp->curr_acc = -tp->max_acc;
                tp->accel_state = ACC_S5;
                break;
            }
            
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
                    v1 = vel - 0.5 * tp->jerk * t1 * t1;
                    // PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
                    dist += (v1 * t - 0.5 * tp->max_acc * t * t);
                } else {
                    // S4 -> S6
                    dist += t * vel;    // dist of (S4 + S6)
                }
            }

            // check if dist would be greater than tc_target at next cycle
            if (tp->target < (dist - (tp->curr_vel + 1.5 * tp->curr_acc - 2.1666667 * tp->jerk))) {
                tp->accel_state = ACC_S4;
//                DPS("should stay in S4 and keep decel\n");
                break;
            }
                    
            // check if we will approaching requested velocity
            // vel should not be greater than "request velocity" after
            // starting acceleration to "accel == 0".
            //
            // AT = A0 + JT (let AT = 0 to calculate T)
            // VT = V0 + A0T + 1/2JT2
            t = - tp->curr_acc / tp->jerk;
            req_vel = tp->vel_cmd * tp->feed_override * tp->cycle_time;
            if (req_vel > tp->max_vel) {
                req_vel = tp->max_vel;
            }
            if ((tp->curr_vel + tp->curr_acc * t + 0.5 * tp->jerk * t * t) <= req_vel) {
                if(tp->curr_pos/tp->target < 0.9){
                    tp->accel_state = ACC_S6;
                }
                else
                {
                    tp->accel_state = ACC_S7;
                    s6_v = tp->curr_vel;
                    s6_a = fabs(tp->curr_acc);
                    s6_p = tp->curr_pos;
                    ts = floor((2*s6_v)/s6_a);
                    k = s6_a*pi/(4*s6_v);
                    error_d = tp->target - tp->curr_pos - s6_v * s6_v / s6_a * (1-4/(pi*pi));
                    prev_s = 0;
                    prev_v = s6_v;
                    c1 = -s6_a/4;
                    c2 = s6_v+((error_d*s6_a)/(2*s6_v));
                    c3 = s6_a/(8*k*k);
                    c4 = 2*k;
                    c5 = -(error_d*s6_a)/(8*k*s6_v);
                    c6 = 4*k;
                    ti = 1;
                    break;
                }

            }

            break;
        
        case ACC_S5:
            printf("ACC_S5 -- %.4f %.4f %.4f\n", tp->curr_pos, tp->curr_vel, tp->curr_acc);
            // jerk is 0 at this state
            // accel < 0
            // VT = VT + AT + 1/2JT
            // PT = PT + VT + 1/2AT + 1/6JT
            tp->curr_vel = tp->curr_vel + tp->curr_acc;
            tp->curr_pos = tp->curr_pos + tp->dir * tp->curr_vel + 0.5 * tp->curr_acc;
            
            // should we stay in S5 and keep decel?
            // calculate dist for S6 -> S4 -> (maybe S5) -> S6
            t = - tp->curr_acc / tp->jerk;
	    
	    if (tp->dir == 1) {
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
			v1 = vel - 0.5 * tp->jerk * t1 * t1;
			// PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
			dist += (v1 * t - 0.5 * tp->max_acc * t * t);
		    } else {
			// S4 -> S6
			dist += t * vel;    // dist of (S4 + S6)
		    }
		}
		
		 printf("%.4f %.4f %.4f %.4f\n", tp->curr_pos, tp->curr_vel, dist, vel);
		// check if dist would be greater than tc_target at next cycle
		if ((tp->target < (dist - (tp->curr_vel + 1.5 * tp->curr_acc)))) {
		    tp->accel_state = ACC_S5;
    //                DPS("should stay in S5 and keep decel\n");
		    break;
		}
	    } else if (tp->dir == -1) {
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
			dist += t1 * vel * tp->dir;    // dist of (S4 + S6)
			// calc decel.dist for ACC_S5
			// t: time for S5
			t = (vel * tp->dir - tp->jerk * t1 * t1) / tp->max_acc;
			v1 = vel * tp->dir - 0.5 * tp->jerk * t1 * t1;
			// PT = P0 + V0T + 1/2A0T^2 + 1/6JT^3
			dist += (v1 * t - 0.5 * tp->max_acc * t * t);
		    } else {
			// S4 -> S6
			dist += t * vel * tp->dir;    // dist of (S4 + S6)
		    }
		}
		
		 printf("%.4f %.4f %.4f %.4f\n", tp->curr_pos, tp->curr_vel, dist, tp->target);
		// check if dist would be greater than tc_target at next cycle
		if ((tp->curr_pos < (dist + (tp->curr_vel + 1.5 * tp->curr_acc)))) {
		    tp->accel_state = ACC_S5;
    //                DPS("should stay in S5 and keep decel\n");
		    break;
		}
	    }

            // check if we will approaching requested velocity
            // vel should not be greater than "request velocity" after
            // starting acceleration to "accel == 0".
            //
            // AT = A0 + JT (let AT = 0 to calculate T)
            // VT = V0 + A0T + 1/2JT2
            // t: cycles for accel to decel to 0
            t = - tp->curr_acc / tp->jerk;
            req_vel = tp->vel_cmd * tp->feed_override * tp->cycle_time;
            if (req_vel > tp->max_vel) {
                req_vel = tp->max_vel;
            }
	    printf("ACC 5 DIV %.4f\n", tp->curr_pos/tp->target);
            if ((tp->curr_vel + tp->curr_acc * t + 0.5 * tp->jerk * t * t) <= req_vel) {
		if (tp->dir == 1) {
		    if(tp->curr_pos/tp->target < 0.9){
			tp->accel_state = ACC_S6;
		    }
		    else
		    {
			tp->accel_state = ACC_S7;
			s6_v = tp->curr_vel;
			s6_a = fabs(tp->curr_acc);
			s6_p = tp->curr_pos;
			ts = floor((2*s6_v)/s6_a);
			k = s6_a*pi/(4*s6_v);
			error_d = tp->target - tp->curr_pos - s6_v * s6_v / s6_a * (1-4/(pi*pi));
			prev_s = 0;
			prev_v = s6_v;
			c1 = -s6_a/4;
			c2 = s6_v+((error_d*s6_a)/(2*s6_v));
			c3 = s6_a/(8*k*k);
			c4 = 2*k;
			c5 = -(error_d*s6_a)/(8*k*s6_v);
			c6 = 4*k;
			ti = 1;
			break;
		    }
		} else if (tp->dir == -1) {
		    if(tp->curr_pos/tp->target < -0.9){
			tp->accel_state = ACC_S6;
		    }
		    else
		    {
			tp->accel_state = ACC_S7;
			s6_v = tp->curr_vel;
			s6_a = fabs(tp->curr_acc);
			s6_p = tp->curr_pos;
			ts = floor((2*s6_v)/s6_a);
			k = s6_a*pi/(4*s6_v);
			error_d = tp->curr_pos - tp->pos_cmd - s6_v * s6_v / s6_a * (1-4/(pi*pi));
			prev_s = 0;
			prev_v = s6_v;
			c1 = -s6_a/4;
			c2 = s6_v+((error_d*s6_a)/(2*s6_v));
			c3 = s6_a/(8*k*k);
			c4 = 2*k;
			c5 = -(error_d*s6_a)/(8*k*s6_v);
			c6 = 4*k;
			ti = 1;
			break;
		    }
		}
            }

            break;
        
        case ACC_S6:
            printf("ACC_S6 -- %.4f %.4f %.4f\n", tp->curr_pos, tp->curr_vel, tp->curr_acc);
            // AT = AT + JT
            // VT = VT + AT + 1/2JT
            // PT = PT + VT + 1/2AT + 1/6JT
            req_vel = tp->vel_cmd * tp->cycle_time;
            if (req_vel > tp->max_vel) {
                req_vel = tp->max_vel;
            }

            tp->curr_acc = tp->curr_acc + tp->jerk;
            tp->curr_vel = tp->curr_vel + tp->curr_acc + 0.5 * tp->jerk;

            dist = tp->dir * tp->curr_vel + 0.5 * tp->curr_acc + 1.0/6.0 * tp->jerk;
            tp->curr_pos = tp->curr_pos + dist;

            if (tp->curr_acc >= 0) {
                tp->accel_state = ACC_S3;
            }

            break;

        case ACC_S7:
            printf("ACC_S7 -- %.4f %.4f %.4f\n", tp->curr_pos, tp->curr_vel, tp->curr_acc);
            // decel to target position based on Jofey's algorithm

            if(ti <= ts){
                dist = c1*ti*ti + c2*ti + c3*cos(c4*ti) + c5*cos(c6*ti-0.5*pi) - c3;
                tp->curr_vel = tp->dir * dist - prev_s;
                tp->curr_acc = tp->curr_vel - prev_v;
                prev_s = dist;
                prev_v = tp->curr_vel;
                tp->curr_pos = s6_p + tp->dir * dist;
                ti = ti + 1;
            }
            else {
                tp->curr_vel = 0;
                tp->curr_acc = 0;
                tp->curr_pos = tp->pos_cmd;
		tp->active = 0;
                tp->accel_state = ACC_S3;
            }
            break;
        
        default:
         break;
//            assert(0);
        } // switch (tp->accel_state)
    } while (immediate_state);

    if (tp->curr_vel != 0) {
	///* yes, mark planner active */
	tp->active = 1;
      //  printf("state=%d pos=%.2f vel=%.2f acc=%.2f\n", tp->accel_state, tp->curr_pos, tp->curr_vel/tp->cycle_time, tp->curr_acc/tp->cycle_time/tp->cycle_time);
    }
}

///*void old_simple_tp_update(simple_tp_t *tp, double period)
//{
    //double max_dv, tiny_dp, pos_err, vel_req;
    //double max_da;
    //double next_acc;
    //double acc_req;
    //double vel_err;
    //double tiny_dv;

    //tp->active = 0;
    //max_da = tp->max_jerk * period;
    //next_acc = fabs(tp->curr_acc) + max_da;
    //if (next_acc > tp->max_acc) {
        //next_acc = tp->max_acc;
    //}
    ///* compute max change in velocity per servo period */
    //max_dv = next_acc * period;
    ///* compute a tiny position range, to be treated as zero */
    //tiny_dp = max_dv * period * 0.001;
    ///* calculate desired velocity */
    //if (tp->enable) {
	///* planner enabled, request a velocity that tends to drive
	   //pos_err to zero, but allows for stopping without position
	   //overshoot */
	//pos_err = tp->pos_cmd - tp->curr_pos;
	///* positive and negative errors require some sign flipping to
	   //avoid sqrt(negative) */
	//if (pos_err > tiny_dp) {
	    //vel_req = -max_dv +
		       //sqrt(2.0 * next_acc * pos_err + max_dv * max_dv);
	    ////checked by curr_vel: /* mark planner as active */
	    ////checked by curr_vel: tp->active = 1;
	//} else if (pos_err < -tiny_dp) {
	    //vel_req =  max_dv -
		       //sqrt(-2.0 * next_acc * pos_err + max_dv * max_dv);
	    ////checked by curr_vel: /* mark planner as active */
	    ////checked by curr_vel: tp->active = 1;
	//} else {
	    ///* within 'tiny_dp' of desired pos, no need to move */
	    //vel_req = 0.0;
	//}
    //} else {
	///* planner disabled, request zero velocity */
	//vel_req = 0.0;
	///* and set command to present position to avoid movement when
	   //next enabled */
	//tp->pos_cmd = tp->curr_pos;
    //}

    ///* limit velocity request */
    //if (vel_req > tp->max_vel) {
        //vel_req = tp->max_vel;
    //} else if (vel_req < -tp->max_vel) {
	//vel_req = -tp->max_vel;
    //}
	
    ///* compute a tiny velocity range, to be treated as zero */
    //tiny_dv = max_da * period * 0.001;
    //vel_err = vel_req - tp->curr_vel;
    //if (vel_err > tiny_dv) {
        //acc_req = -max_da +
                   //sqrt(2.0 * tp->max_jerk * vel_err + max_da * max_da);
    //} else if (vel_err < -tiny_dv) { 
        //// vel_req <= 0
        //acc_req =  max_da -
                   //sqrt(-2.0 * tp->max_jerk * vel_err + max_da * max_da);
    //} else {
        //acc_req = 0;
    //}
    
    ///* limit accel request */
    //if (acc_req > tp->max_acc) {
        //acc_req = tp->max_acc;
    //} else if (acc_req < -tp->max_acc) {
        //acc_req = -tp->max_acc;
    //}

    ///* limit accel toward request at jerk limit */
    //if (acc_req > tp->curr_acc + max_da) {
        //tp->curr_acc += max_da;
    //} else if (acc_req < tp->curr_acc - max_da) {
        //tp->curr_acc -= max_da;
    //} else {
        //tp->curr_acc =  acc_req;
    //}

    //tp->curr_vel += (tp->curr_acc * period);

    ////orig: /* ramp velocity toward request at accel limit */
    ////orig: if (vel_req > tp->curr_vel + max_dv) {
    ////orig:     tp->curr_vel += max_dv;
    ////orig: } else if (vel_req < tp->curr_vel - max_dv) {
    ////orig:     tp->curr_vel -= max_dv;
    ////orig: } else {
    ////orig:     tp->curr_vel = vel_req;
    ////orig: }
    
    ///* check for still moving */
    //// if (tp->curr_vel != 0.0)
    //if (tp->curr_vel > tiny_dv) {
	///* yes, mark planner active */
	//tp->active = 1;
    //}
    ///* integrate velocity to get new position */
    //tp->curr_pos += tp->curr_vel * period;
//}
//*/
