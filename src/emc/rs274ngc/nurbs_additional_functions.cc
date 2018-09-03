/********************************************************************
 *
 * Author: Manfredi Leto (Xemet)
 * License: GPL Version 2
 * System: Linux
 *    
 * Copyright (c) 2009 All rights reserved.
 *
 ********************************************************************/

/* Those functions are needed to calculate NURBS points */

#include "rtapi_math.h"
#include <algorithm>
#include "canon.hh"

static void unit(PLANE_POINT &p) {
    double h = rtapi_hypot(p.X, p.Y);
    if(h != 0) { p.X/=h; p.Y/=h; }
}

std::vector<unsigned int> knot_vector_creator(unsigned int n, unsigned int k) {
    
    unsigned int i;
    std::vector<unsigned int> knot_vector;
    for (i=0; i<=n+k; i++) {
	if (i < k)  
            knot_vector.push_back(0);
        else if (i >= k && i <= n)
            knot_vector.push_back(i - k + 1);
        else
            knot_vector.push_back(n - k + 2);
    }
    return knot_vector;
 
}

double Nmix(unsigned int i, unsigned int k, double u, 
                    std::vector<unsigned int> knot_vector) {

    if (k == 1){
        if ((u >= knot_vector[i]) && (u <= knot_vector[i+1])) {
            return 1;
        } else {
            return 0;}
    } else if (k > 1) {
        if ((knot_vector[i+k-1]-knot_vector[i] == 0) && 
            (knot_vector[i+k]-knot_vector[i+1] != 0)) {
            return ((knot_vector[i+k] - u)*Nmix(i+1,k-1,u,knot_vector))/
                    (knot_vector[i+k]-knot_vector[i+1]);
        } else if ((knot_vector[i+k]-knot_vector[i+1] == 0) && 
            (knot_vector[i+k-1]-knot_vector[i] != 0)) {
            return ((u - knot_vector[i])*Nmix(i,k-1,u,knot_vector))/
                    (knot_vector[i+k-1]-knot_vector[i]);
        } else if ((knot_vector[i+k-1]-knot_vector[i] == 0) && 
            (knot_vector[i+k]-knot_vector[i+1] == 0)) {
            return 0;
        } else {
            return ((u - knot_vector[i])*Nmix(i,k-1,u,knot_vector))/
                    (knot_vector[i+k-1]-knot_vector[i]) + ((knot_vector[i+k] - u)*
                    Nmix(i+1,k-1,u,knot_vector))/(knot_vector[i+k]-knot_vector[i+1]);
        }
    }
    else return -1;
}



double Rden(double u, unsigned int k,
                  std::vector<CONTROL_POINT> nurbs_control_points,
                  std::vector<unsigned int> knot_vector) {

    unsigned int i;
    double d = 0.0;   
    for (i=0; i<(nurbs_control_points.size()); i++)
        d = d + Nmix(i,k,u,knot_vector)*nurbs_control_points[i].W;
    return d;
}

PLANE_POINT nurbs_point(double u, unsigned int k, 
                  std::vector<CONTROL_POINT> nurbs_control_points,
                  std::vector<unsigned int> knot_vector) {

    unsigned int i;
    PLANE_POINT point;
    point.X = 0;
    point.Y = 0;
    for (i=0; i<(nurbs_control_points.size()); i++) {
        point.X = point.X + nurbs_control_points[i].X*Nmix(i,k,u,knot_vector)
	*nurbs_control_points[i].W/Rden(u,k,nurbs_control_points,knot_vector);
        point.Y = point.Y + nurbs_control_points[i].Y*Nmix(i,k,u,knot_vector)
	*nurbs_control_points[i].W/Rden(u,k,nurbs_control_points,knot_vector);
    }
    return point;
}

#define DU (1e-5)
PLANE_POINT nurbs_tangent(double u, unsigned int k,
                  std::vector<CONTROL_POINT> nurbs_control_points,
                  std::vector<unsigned int> knot_vector) {
    unsigned int n = nurbs_control_points.size() - 1;
    double umax = n - k + 2;
    double ulo = std::max(0.0, u-DU), uhi = std::min(umax, u+DU);
    PLANE_POINT P1 = nurbs_point(ulo, k, nurbs_control_points, knot_vector);
    PLANE_POINT P3 = nurbs_point(uhi, k, nurbs_control_points, knot_vector);
    PLANE_POINT r = {(P3.X - P1.X) / (uhi-ulo), (P3.Y - P1.Y) / (uhi-ulo)};
    unit(r);
    return r;
}
