/********************************************************************
* Description: nurbs_math.hh
*   Math helpers for evaluating the NURBS curves of G5.2 and G6.2.
*   They are plain functions, not canonical commands; the canon
*   implementations use them to turn NURBS_G5_FEED/NURBS_G6_FEED
*   into line segments.
*
* License: GPL Version 2
********************************************************************/
#ifndef NURBS_MATH_HH
#define NURBS_MATH_HH

#include <vector>
#include "canon.hh" // NURBS_CONTROL_POINT, NURBS_G6_CONTROL_POINT, NURBS_PLANE_POINT

/* Additional functions needed to calculate nurbs G5 points */

std::vector<unsigned int> nurbs_G5_knot_vector_creator(unsigned int n, unsigned int k);

double nurbs_G5_Nmix(unsigned int i, unsigned int k, double u, const std::vector<unsigned int>& knot_vector);

double nurbs_G5_Rden(double u, unsigned int k,
                  const std::vector<NURBS_CONTROL_POINT>& nurbs_control_points,
                  const std::vector<unsigned int>& knot_vector);

NURBS_PLANE_POINT nurbs_G5_point(double u, unsigned int k, 
                  const std::vector<NURBS_CONTROL_POINT>& nurbs_control_points,
                  const std::vector<unsigned int>& knot_vector);

NURBS_PLANE_POINT nurbs_G5_tangent(double u, unsigned int k,
                  const std::vector<NURBS_CONTROL_POINT>& nurbs_control_points,
                  const std::vector<unsigned int>& knot_vector);

/* Additional functions needed to calculate nurbs points G_6_2*/

std::vector<double> nurbs_g6_knot_vector_creator(unsigned int n, unsigned int k, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points);

std::vector<double> nurbs_interval_span_knot_vector_creator(unsigned int n, unsigned int k, const std::vector<double>& knot_vector_);

double nurbs_lderv(double u, unsigned int k, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points, const std::vector<double>& knot_vector_);

double nurbs_Sa1_b1_length_(double a1, double b1, unsigned int k, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points, const std::vector<double>& knot_vector_);

std::vector<double> nurbs_lenght_vector_creator(unsigned int k, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points, const std::vector<double>& knot_vector_, const std::vector<double>& span_knot_vector);

double nurbs_lenght_tot(int j, const std::vector<double>& span_knot_vector, const std::vector<double>& lenght_vector);

double nurbs_lenght_l_u(double u, unsigned int k, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points, const std::vector<double>& knot_vector_, const std::vector<double>& span_knot_vector, const std::vector<double>& lenght_vector);

std::vector<double> nurbs_Du_span_knot_vector_creator(unsigned int k, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points, const std::vector<double>& knot_vector_, const std::vector<double>& span_knot_vector);

std::vector<double> nurbs_costant_crator(const std::vector<double>& span_knot_vector, const std::vector<double>& lenght_vector, const std::vector<double>& Du_span_knot_vector);

double nurbs_uj_l(double l, const std::vector<double>& span_knot_vector, const std::vector<double>& lenght_vector, const std::vector<double>& nurbs_costant);

/* Funzioni di supporto all'algoritmo di suddivisione */
std::vector<double> nurbs_G6_new_control_point_nurbs1(double u, unsigned int k, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points, const std::vector<double>& knot_vector_ );

std::vector<double> nurbs_G6_new_control_point_nurbs2(double u, unsigned int k, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points, const std::vector<double>& knot_vector_ );

std::vector<double> nurbs_G6_knot_vector_new_creator_sgment(unsigned int k, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points);
/* ... */

double nurbs_G6_Nmix(unsigned int i, unsigned int k, double u, const std::vector<double>& knot_vector_);

std::vector< std::vector<double> > nurbs_G6_Nmix_creator(double u, unsigned int k, double n, const std::vector<double>& knot_vector_);

double nurbs_G6_Rden(double u, unsigned int k, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points, const std::vector<double>& knot_vector_);

NURBS_PLANE_POINT nurbs_G6_point(double u, unsigned int k, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points, const std::vector<double>& knot_vector_);

double nurbs_G6_Rdenx(double u, unsigned int k, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points, const std::vector<double>& knot_vector_, const std::vector< std::vector<double> >& A6);

NURBS_PLANE_POINT nurbs_G6_pointx(double u, unsigned int k, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points, const std::vector<double>& knot_vector_, const std::vector< std::vector<double> >& A6);

NURBS_PLANE_POINT nurbs_G6_point_x(double u, unsigned int k, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points, const std::vector<double>& knot_vector_);

NURBS_PLANE_POINT nurbs_G6_tangent_x(double u, unsigned int k, const std::vector<NURBS_G6_CONTROL_POINT>& nurbs_control_points, const std::vector<double>& knot_vector_);

#endif
