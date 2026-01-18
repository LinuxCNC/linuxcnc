/*
 * Author: 杨阳
 * Date: 2020-06-05 22:32:44
 * LastEditors: 杨阳
 * LastEditTime: 2020-07-25 10:47:58
 * Contact: mika-net@outlook.com
 * Copyright (c) 2017-2020: 杨阳
 * FilePath: \pomelo\PROGRAM\linuxcnc\src\emc\tp\sp_scurve.h
 * 
 */
/********************************************************************
* Description: sp_scurve.h
*   Discriminate-based trajectory planning
*
*   Derived from a work by 杨阳
*
* Author: 杨阳
* License: GPL Version 2
* System: Linux
*    
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/
#ifndef SP_SCURVE_H
#define SP_SCURVE_H

#include "simple_tp.h"
#include "rtapi_math.h"

double nextAccel(
    double t, double targetV, double v, double a, double maxA, double maxJ);
double sc_distance(double t, double v, double a, double j);
double velocity(double t, double a, double j);
double acceleration(double t, double j);
unsigned getPhase(double v, double a, double j);
double nextSpeed(double v,
                 double a,
                 double t,
                 double targetV,
                 double maxA,
                 double maxJ,
                 double *req_v,
                 double *req_a,
                 double *req_j);
double getStoppingDist(simple_tp_t *tp);
double stoppingDist(double v, double a, double maxA, double maxJ);
double
finishWithSpeedDist(double v, double ve, double a, double maxA, double maxJ);
int getTargetV(double distence,
               double v,
               double a,
               double period,
               double maxV,
               double maxA,
               double maxJ,
               double *req_v,
               double *req_a);

int getNext(simple_tp_t *tp, double Vs, double Ve, double period);
double
getNextPoint(simple_tp_t *tp, int n, double T, double *req_v, double *req_a);
int findSCurveVSpeed(double distence,
                     /* double maxV, */ double maxA,
                     double maxJ,
                     double *req_v);
int findSCurveVSpeedWithEndSpeed(
    double distence, double Ve, double maxA, double maxJ, double *req_v);
double
calcDecelerateTimes(double v, double amax, double jerk, double *t1, double *t2);
double calcSCurveSpeedWithT(double amax, double jerk, double T);

#endif