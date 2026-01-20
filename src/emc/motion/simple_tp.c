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
#include "sp_scurve.h"
#include "mot_priv.h"

void simple_scurve_tp_update(simple_tp_t *tp, double period);

void simple_tp_update(simple_tp_t *tp, double period)
{
    if(tp->max_jerk <= 0.0001 || GET_TRAJ_PLANNER_TYPE() == 0){
        return simple_tp_update_normal(tp, period);
    }
	
    simple_scurve_tp_update(tp,period);
    tp->last_pos_cmd = tp->pos_cmd;
	return;
}

void simple_tp_update_normal(simple_tp_t *tp, double period)
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
	/* positive and negative errors require some sign flipping to
	   avoid sqrt(negative) */
	if (pos_err > tiny_dp) {
	    vel_req = -max_dv +
		       sqrt(2.0 * tp->max_acc * pos_err + max_dv * max_dv);
	    /* mark planner as active */
	    tp->active = 1;
	} else if (pos_err < -tiny_dp) {
	    vel_req =  max_dv -
		       sqrt(-2.0 * tp->max_acc * pos_err + max_dv * max_dv);
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

void simple_scurve_tp_update(simple_tp_t *tp, double period){
    double tiny_dp, pos_err, vel_req;
    double acc_req;//, vel_acc;
    int abort;

    #define T  period
    #define J  tp->max_jerk
    #define Vc tp->curr_vel
    #define Ac tp->curr_acc
    #define Am tp->max_acc
    #define Vm tp->max_vel

    int dir = 0;
    tp->active = 0;
    double curr_acc = Ac;
    double curr_vel = Vc;
    double curr_jerk = tp->curr_jerk;

    /* calculate desired velocity */
    tiny_dp = TINY_DP(tp->max_acc, period);
    acc_req = 0;
    
    /* planner enabled, request a velocity that tends to drive
    pos_err to zero, but allows for stopping without position
    overshoot */
    pos_err = tp->pos_cmd - tp->curr_pos;
    
    if (tp->enable) {
        /* positive and negative errors require some sign flipping to
        avoid sqrt(negative) */
        if (fabs(pos_err) > tiny_dp) {
            abort = 0;
            tp->active = 1;
        } else {
            abort = 1;
        }
    } else {
        /* planner disabled, request zero velocity */
        if (fabs(Vc) < tiny_dp){
            tp->enable = 0;
            simple_tp_update_normal(tp, T);
            return;
        }
        abort = 1;
    }

    if(fabs(tp->last_pos_cmd - tp->pos_cmd) > tiny_dp) {
        tp->status = 0;
        tp->last_move_length = 0;
    }

    if (pos_err > tiny_dp)  dir = 1;
    else if (pos_err < -tiny_dp)  dir = -1;
    else dir = 0;

    if(dir != 0){
        double decLen =  stoppingDist(Vc, Ac, Am, J) ;
        nextSpeed(Vc, Ac, period, Vm * dir , Am, tp->max_jerk, &vel_req, &acc_req, &tp->curr_jerk);
        double moveL = sc_distance(T, curr_vel, curr_acc, curr_jerk);
        double decLen2 = stoppingDist(vel_req, acc_req, Am, J);
       
        if(abort == 1 ){
            nextSpeed(Vc, Ac, period, 0 , Am, tp->max_jerk, &vel_req, &acc_req, &tp->curr_jerk);

        }else if(tp->status == 2){
            simple_tp_update_normal(tp, T);
            return;
        }else if(fabs(fabs(pos_err) - decLen) <= 0.000001 || fabs(pos_err) - moveL * dir <= decLen2 || fabs(pos_err) <= decLen || tp->status == 1){
            nextSpeed(Vc, Ac, T, 0 * dir , Am, tp->max_jerk, &vel_req, &acc_req, &tp->curr_jerk);
            double move = sc_distance(T, curr_vel, curr_acc, tp->curr_jerk);
            double error = fabs(pos_err) - decLen;

            if (error > 0 && (error < tp->last_move_length * dir && error > move * dir))
            {   
                tp->curr_acc = curr_acc;
                tp->curr_vel = curr_vel;
                tp->curr_jerk = curr_jerk;
                tp->curr_pos += error * dir;
                return;
            }else if(error < 0){
            }
            
            if (fabs(move) > fabs(pos_err))
            {
                tp->status = 2;
                simple_tp_update_normal(tp, T);
                return;
            }
            else if (vel_req == 0 && fabs(pos_err) > tiny_dp)
            {
                tp->status = 2;
                simple_tp_update_normal(tp, T);
                return;
            }
            else if (fabs(vel_req) < tiny_dp)
            {
                tp->status = 2;
                simple_tp_update_normal(tp, T); 
                return;
            }
        }else{
            nextSpeed(Vc, Ac, period, Vm * dir , Am, tp->max_jerk, &vel_req, &acc_req, &tp->curr_jerk);
            double move = sc_distance(T, curr_vel, curr_acc, tp->curr_jerk);
            double error = fabs(pos_err) - decLen;
            if (error > 0 && (error < tp->last_move_length * dir && error > move * dir))
            {
                tp->curr_acc = curr_acc;
                tp->curr_vel = curr_vel;
                tp->curr_jerk = curr_jerk;
                tp->curr_pos += error * dir;
                return;
            }else if(error < 0){
            }
        } 
    }else{
        vel_req = 0.0;
        tp->curr_jerk = 0;
    }
    
    if (vel_req > tp->max_vel) {
        vel_req = tp->max_vel;
    } else if (vel_req < -tp->max_vel) {
        vel_req = -tp->max_vel;
    }

    tp->curr_acc = acc_req;
    tp->curr_vel = vel_req;


    if (tp->curr_vel != 0.0){
	    tp->active = 1;         
    } else {
        tp->curr_vel = 0;
	    tp->active = 0;
    }
    tp->last_move_length = curr_vel * period + 0.5 * curr_acc * period * period + (tp->curr_jerk * period * period * period) / 6;
    tp->curr_pos += tp->last_move_length;
#undef Vc
#undef Ac
#undef Am
#undef Vm
#undef T
#undef J
}