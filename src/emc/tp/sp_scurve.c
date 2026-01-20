/*!
********************************************************************
* Description: sp_scurve.c
*\brief Discriminate-based scurve trajectory planning
*
*\author Derived from a work by 杨阳
*
* License: GPL Version 2
* System: Linux
*
* Copyright (c) 2004 All rights reserved.
*
* Last change:
********************************************************************/
#include "sp_scurve.h"
#include "rtapi_math.h"
#include "mot_priv.h" 
#ifndef __KERNEL__
#include <stdio.h>
#include <string.h>
#endif

unsigned getPhase(double v, double a, double j)  {
    if (!v) return 0;
    // Handle negative velocity
    //double v = this->v;
    //double a = this->a;
    if (v < 0) {
        v = -v;
        a = -a;
    }

    if (0 < a) {
    if (0 < j) return 1;
    if (!j) return 2;
    return 3;
    }

    if (!a) return 4;
    if (j < 0) return 5;
    if (!j) return 6;

    return 7;
}


/*
a cubic coefficient
b quadratic coefficient
c linear coefficient
d constant coefficient
num initial calculation value
*/
double solute( double a, double b, double c, double d, double num){
  double x ,x0 ,f ,f1;
  //int cnt = 0;
  x = num;
  do{
    x0 = x;
    f = (( a*x0 + b )*x0 + c )*x0 + d;
    f1 = ( 3*a*x0 + 2*b )*x0 + c;
    x = x0 - f / f1;
    //cnt++;
  } while ( fabs( x - x0 ) > 1e-6 );

  //printf( "the value is %.14f \r\n", x );
  return( x );
}


/* my own implementation for solving cubic equation
 * INPUT
 *   a, b, c, d: coefficients of cubic equation ax^3 + bx^2 + cx + d = 0
 * OUTPUT
 *   res: array to store real solutions
 *   len: pointer to store number of real solutions
 * RETURN
 *   number of real solutions
 */
int solve_cubic(double a, double b, double c, double d, double res[3], int* len) {
    // Define precision constants
    const double EPSILON = 1e-10;
    const double NEAR_ZERO = 1e-10;
    
    // Handle special cases (linear and quadratic equations)
    if (fabs(a) < EPSILON) {
        if (fabs(b) < EPSILON) {
            if (fabs(c) < EPSILON) {
                *len = 0;
                return 0;  // No solution
            }
            // Linear equation: cx + d = 0
            res[0] = -d / c;
            *len = 1;
            return 1;
        }
        // Quadratic equation: bx^2 + cx + d = 0
        double A = b;
        double B = c;
        double C = d;
        double disc = fma(B, B, -4.0*A*C);  // B*B - 4*A*C
        
        if (disc < 0) {
            *len = 0;
            return 0;  // No real solutions
        }
        
        double sqrt_disc = sqrt(disc);
        res[0] = (-B + sqrt_disc) / (2.0 * A);
        res[1] = (-B - sqrt_disc) / (2.0 * A);
        *len = 2;
        return 2;
    }
    
    // Normalize coefficients for better numerical stability
    double norm = fabs(a);
    double a_norm = a / norm;
    double b_norm = b / norm;
    double c_norm = c / norm;
    double d_norm = d / norm;
    
    // Calculate intermediate values using fma for better precision
    double A = fma(b_norm, b_norm, -3.0*a_norm*c_norm);
    double B = fma(b_norm, c_norm, -9.0*a_norm*d_norm);
    double C = fma(c_norm, c_norm, -3.0*b_norm*d_norm);
    double f = fma(B, B, -4.0*A*C);
    
    *len = 0;
    
    // Handle different cases based on discriminant
    if (fabs(A) < NEAR_ZERO && fabs(B) < NEAR_ZERO) {
        // Triple root case
        double root = -b_norm / (3.0 * a_norm);
        res[(*len)++] = root;
        res[(*len)++] = root;
        res[(*len)++] = root;
    }
    else if (fabs(f) < NEAR_ZERO) {
        // Double root case
        double K = B / A;
        res[(*len)++] = -b_norm/a_norm + K;
        res[(*len)++] = -K/2.0;
        res[(*len)++] = -K/2.0;
    }
    else if (f > NEAR_ZERO) {
        // One real root and two complex conjugate roots
        double Y1 = fma(A, b_norm, 3.0*a_norm*(-B + sqrt(f))/2.0);
        double Y2 = fma(A, b_norm, 3.0*a_norm*(-B - sqrt(f))/2.0);
        
        // Use more stable cube root calculation
        double Y1_value = (Y1 == 0.0) ? 0.0 : 
            (Y1 > 0.0 ? exp(log(Y1) / 3.0) : -exp(log(-Y1) / 3.0));
        double Y2_value = (Y2 == 0.0) ? 0.0 : 
            (Y2 > 0.0 ? exp(log(Y2) / 3.0) : -exp(log(-Y2) / 3.0));
        
        res[(*len)++] = (-b_norm - Y1_value - Y2_value) / (3.0 * a_norm);
        
        // Handle near-real complex roots
        double i_value = sqrt(3.0)/2.0 * (Y1_value - Y2_value) / (3.0 * a_norm);
        if (fabs(i_value) < 1e-1) {
            res[(*len)++] = (-b_norm + 0.5*(Y1_value + Y2_value)) / (3.0 * a_norm);
        }
    }
    else {
        // Three distinct real roots
        double T = (2.0*A*b_norm - 3.0*a_norm*B) / (2.0*A*sqrt(A));
        // Ensure T is within valid range for acos
        T = fmax(-1.0, fmin(1.0, T));
        double S = acos(T);
        
        // Pre-calculate common values
        double cos_S3 = cos(S/3.0);
        double sin_S3 = sin(S/3.0);
        double sqrt_A = sqrt(A);
        double sqrt_3 = sqrt(3.0);
        
        // Calculate roots
        res[(*len)++] = (-b_norm - 2.0*sqrt_A*cos_S3) / (3.0 * a_norm);
        res[(*len)++] = (-b_norm + sqrt_A*(cos_S3 + sqrt_3*sin_S3)) / (3.0 * a_norm);
        res[(*len)++] = (-b_norm + sqrt_A*(cos_S3 - sqrt_3*sin_S3)) / (3.0 * a_norm);
    }
    
    return *len;
}


/*
 Continuous form
 PT = P0 + V0T + 1/2A0T2 + 1/6JT3
 VT = PT' = V0 + A0T + 1/2 JT2
 AT = VT' = PT'' = A0 + JT

 Discrete time form (let T be 1, then T^2 == 1, T^3 == 1)
 PT = PT + VT + 1/2AT + 1/6J
 VT = VT + AT + 1/2JT
 AT = AT + JT
 */
int getTargetV(double distence, double v, double a, double period, double maxV, double maxA, double maxJ, double* req_v, double* req_a){
  if(distence == 0) return 0;
  double T1 = maxA / maxJ;
  double V1 = maxJ * T1 * T1 / 2;
  int phase = 0;
  //double tT = 0;
  
  if(V1 >= maxV / 2){ // Handle case where jerk is small, causing T2 and T6 segments to not exist
    T1 = sqrt(maxV / maxJ);
  }

  double S1 = maxJ * T1 * T1 * T1 / 6.0;
  double S2 = S1;
  double S3 = S2;
  double V2 = V1;
  double T2 = 0;
  double T3 = 0;
  double needTime = 0;

  if (distence < 0) {   // Due to symmetry, only consider one case
    distence = -distence;
    v = -v;
    a = -a;
  }
  
  double calc_v = 0;
  double calc_a = a;

  if(distence <= S1){  
    double t1;
    //rtai
    t1 = pow(6.0 * distence / maxJ, 1.0/3.0);
    //uspace
    //t1 = cbrt(6.0 * distence / maxJ) ;


    //if(t1 -period > 0)
    //  t1 = t1 - period; 
    

    calc_a = maxJ * t1;
    calc_v = (calc_a * t1) / 2;
    //tT = t1;
    
    phase = 1; 

    //double p = calc_v * period;
    //double cp = calc_v * t1 + maxJ * t1 * t1 * t1 / 6;
    //double cpNext = calc_v * (t1 - period) + maxJ * (t1 - period) * (t1 - period) * (t1 - period) / 6;
    //if(t1 - period > 0)
    //  calc_v = (distence - cpNext) / period;
    needTime = t1;
    //printf("S1 | D: %.10f | CP: %f | CNP: %f | P: %f | T: %f | J: %f | V: %f | calc_a: %f | RV: %f | t1: %f \n",  distence, cp, cpNext, p, period, maxJ, calc_v, calc_a, v, t1);
  }

  if(phase == 0){
    if(V1 < maxV / 2){ // T2 and T6 segments exist
      T2 = (maxV / maxA) - T1;
      S2 = S1 + V1 * T2 + maxJ * T1 * T2 * T2 / 2;
      V2 = V1 + maxA * T2;
      if(distence < S2){
        double t2;
        double A = maxJ * T1 / 2;
        double B = V1;
        double C = S1 - distence;
        double dt = sqrt(B * B - 4 * A * C);
        t2 = (-B + dt) / (2 * A);
        //printf("In S2 segment ");
        //t2 = t2 - period;
        //if(t2 < 0){
          //calc_v = V1;
        //  calc_a = maxJ * t2;
        //  calc_v = (calc_a * t2) / 2;
        //}else{
          //Vt = Vs + J * T1^2 - 0.5 * J * (t - 2 * T1) ^2
          calc_a = maxA;
          calc_v = V1 + maxA * t2;
        //}
        //tT = t2;
        phase = 2;

        //double p = calc_v * period;
        //printf("P: %f, Ov: %f Nv: %f Oa: %f, Na: %f ",  p * 1000, v, calc_v, a, calc_a);
        
        //double p = calc_v * period;
        //t2 = t2 - period;
        // S2 = S1 + V1 * T2 + maxJ * T1 * T2 * T2 / 2;
        //double cp = S1 + V1 * t2 + 0.5 * maxJ * T1 * t2 * t2;
        //double cpNext = S1 + V1 * (t2 - period) + 0.5 * maxJ * T1 * (t2 - period) *  (t2 - period);
        needTime = T1 + t2;
        //printf("S2 | D: %.10f | CP: %f | CNP: %f | P: %f | T: %f | J: %f | V: %f | calc_a: %f | RV: %f | t2: %f \n",  distence, cp, cpNext, p, period, maxJ, calc_v, calc_a, v, T1 + t2);
      }
   
    }
  }

  if(phase == 0){
    S3 = S2 + V2 * T1 + S1 * 2;
    T3 = T1;
    if(distence <= S3){ // Solving cubic equation is expensive 
      double A = - maxJ / 6;
      double B = maxJ * T1 / 2;
      double C = V2;
      double D = S2 - distence;
      double t3 = 0;//solute(A,B,C,D,T2);
      //double x1, x2, x3;
      //int n;
      //x1 = x2 = x3 = 0;
      //n = solve_cubic(A, B, C, D, &x1, &x2, &x3);
      //double temp;
      double xo[10];
      int len;
      int i = 0;
      solve_cubic(A, B, C, D, xo, &len);
      if(len > 0){
        t3 = xo[0];
        for (; i < len; i++)
        {
          if(i == 0)continue;
          t3 = fmax(xo[i], t3);
        }
      }
      //t3 = fmax(fmax(xo[0], x2), x3);
      if(t3 < 0){
        t3 = 0.001;
      }
      //if(x1>x2) temp=x1,x1=x2,x2=temp;
      //if(x2>x3) temp=x2,x2=x3,x3=temp;
      //if(x1>x2) temp=x1,x1=x2,x2=temp;
      //if(x1 > 0) t3 = x1;
      //else if(x2 > 0) t3 = x2;
      //else if(x3 > 0) t3 = x3;
      //else t3 = 0.001;
      //printf("S3 t3: %f n: %d x1: %f x2: %f x3: %f  A: %f B: %f C: %f D: %f\n" , t3, n, x1, x2, x3, A, B, C, D);
      //JT1 - J ( t - T1 )
      //t3 = t3 - period;
      //if(t3 <= 0 ){
      //  calc_a = maxA;
      //  calc_v = V1 + maxA *  (T2 + t3);
     // }else{
        calc_v = V2 + maxA * t3 - 0.5 * maxJ * t3 * t3;
        calc_a = maxA - maxJ * t3;
      //}
      //tT = t3;

      //double p = calc_v * period;
      //t3 = t3 - period;
      //double cp = 0;
      //if(t3 < 0){

      //}else{
      //cp = S2 + calc_v * t3 + 0.5 * maxJ * T1 * t3* t3 - maxJ * t3 * t3 * t3 / 6;
      //double cpNext = S2 + calc_v * (t3 - period) + 0.5 * maxJ * T1 * (t3 - period) * (t3 - period) - maxJ * (t3 - period) * (t3 - period) * (t3 - period) / 6;
         //(cp - distence) / period;
      //}
      //double rp =
      //printf("P: %f, Ov: %f Nv: %f Oa: %f, Na: %f ",  p * 1000, v, calc_v, a, calc_a);
      needTime = T1 + T2 + t3;
      //printf("S3 | D: %.10f | CP: %f | CNP: %f | P: %f | T: %f | J: %f | T1: %f | V: %f | calc_a: %f | RV: %f | t3: %f \n",  distence, cp, cpNext, p, period, maxJ, T1, calc_v, calc_a, v, T1 + T2 + t3);
      phase = 3;
    }
  }
  //printf("In constant velocity phase ");
  // Need to enter S3 early by one period

  //*req_v = calc_v;
  if(phase == 0)
    phase = 4;

  // PT = P0 + V0 * T + 0.5 * A0 * T^2 + J * T^3 / 6
  // VT = V0 + A0 * T + J * T^2 /2
  // AT = A0 + J * T
  // After discretization:
  // PT = PT + VT + 0.5 * AT + J / 6
  // VT = VT + AT + 0.5 * J * T
  // AT = AT + J * T

  if(phase == 4){//
    if(v < maxV){ // Need to accelerate to target speed
      double tt = fabs(a / maxJ);
      double ve = v + (a - maxJ * period) * tt - maxJ * tt * tt / 2;
      // a * t + 1/2 * j * t^2
      if(ve >=  maxV ){
        phase = 5;
        calc_a = a - maxJ * period;
      }
      else{
        phase = 7;
        calc_a = a + maxJ * period;
      }

      if(calc_a >= maxA){
        calc_a = maxA;
        phase = 6;
      }
      //double p = 0;
      // VT = V0 + A0 * T + J * T^2 /2
      // AT = A0 + J * T
      if(phase == 7){
        //p = v * period + 0.5 * calc_a * period * period + maxJ * period * period* period / 6;
        calc_v = v + calc_a * period + maxJ * period * period / 2;
      }
      else if(phase == 6){
        if(a != maxA){
          double J = (maxA - a) / period;
          calc_v = v + maxA * period + J * period * period / 2;
        }else
          calc_v = v + calc_a * period;
      }
      else if(phase == 5){ // Need compensation; after discretization each segment is s = v * t, so distance may be more reliable
        // V0 * T + 0.5 * A0 * T^2 + J * T^3 / 6
        //p = v * period + 0.5 * calc_a * period * period - maxJ * period * period* period / 6;
        calc_v = v + calc_a * period - maxJ * period * period / 2;
      }
      
      if(calc_v >= maxV){
        calc_v = maxV;
      }
      //printf("VE: %f, OV: %f NV: %f P: %f OA: %f, NA: %f ", ve, v, calc_v, p * 1000, a, calc_a);
    }else if(v > maxV){// Need to decelerate to target speed; triangular acceleration algorithm used here; entered when velocity is modified
      double tt = fabs(a) / maxJ;
      double ve = v + a * tt + maxJ * tt * tt / 2;
      if(ve <= maxV) {
        phase = 8;
        calc_a = a + maxJ * period;
      }
      else{
        phase = 9;
        calc_a = a - maxJ * period;
      }

      if(calc_a < -maxA){
        calc_a = -maxA;
      }
        
      calc_v = v + calc_a * period - maxJ * period * period / 2;
      if(calc_v < maxV){
        calc_v = maxV;
      }
    }else{ // Constant velocity motion, forced
      phase = 4;
      calc_v = maxV;
    }
  }else{ // This section is intended for optimizing the stopping segment, very important, may be used to smooth the curve
    //if(a)

    if(needTime == 0){ // Constant velocity or acceleration phase
      // Decelerate a few periods early
    }else if(needTime > T1 + T2 && needTime <= T1 + T2 +T3){ // Handle S3 segment

    }else if(needTime > T1 && needTime <= T1 + T2){ // Handle S2 segment

    }else if(needTime >= 0 && needTime <= T1){ // Handle S1 segment
      
    }


  }


  if(calc_v > maxV)
    calc_v = maxV;
  *req_a = calc_a;
  //*req_a = (calc_v - v) / period;

#define MAX_A_OVERRIDE 1.5
  double ra = (calc_v - v) / period;
  if(ra > maxA * MAX_A_OVERRIDE){ // This is not an ideal solution
    *req_a = maxA* MAX_A_OVERRIDE;
    calc_v = v + *req_a * period;
  }else if(ra < -maxA * MAX_A_OVERRIDE){
    *req_a = -maxA * MAX_A_OVERRIDE;
    calc_v = v + *req_a * period;
  }
#undef MAX_A_OVERRIDE

  *req_v = calc_v;
  //if(phase == 1 || phase == 2 || phase == 3)
  //  printf("S%d L: %.10f, ACC %f V: %f *req_v:  %f v: %f S3: %f A: %f CA: %f\n", phase, distence , *req_a, calc_v, *req_v, v, S3, (calc_v - v) / period, ov);
  return phase;
}


// PT = P0 + V0 * T + 0.5 * A0 * T^2 + J * T^3 / 6
// VT = V0 + A0 * T + J * T^2 /2
// AT = A0 + J * T
int calcSCurve(double S, double Vc, double Ve, double Vm, double Ac, double Am, double Jm,double T,
               double n[8], double a[8], double v[8], double* tn, double* verr, double* J2, double* J4){
if(S < 0){
  Vc = -Vc;
  Ac = -Ac;
}

S = fabs(S);
double N1 = fabs(Vm - Vc) / (Jm * T * T); //fabs(Vm - Vs) / (Jm * T * T);
double N2 = fabs(Vm - Ve) / (Jm * T * T);

n[0] = floor(Am / (Jm * T));
n[1] = 0;
double rn2;
double rn6;

*J2 = *J4 = Jm;
n[1] = n[5] = 0;

if(N1 > n[0] * (n[0] + 1)){ // Constant acceleration phase exists
  n[1] = floor((N1 - n[0] * (n[0] + 1)) / n[0]);
}else{
  n[0] = n[2] = floor(sqrt(N1 + 1 / 4) - 1 / 2);
}

rn2 = n[2] = n[0];
rn6 = n[6] = n[4] = floor(Am / (Jm * T));

if(N2 > n[4] * (n[4] + 1)){ // Constant deceleration phase exists
  n[5] = floor((N2 - n[4] * (n[4] + 1)) / n[4]);
}else{
  n[4] = n[6] = floor(sqrt(N2 + 1 / 4) - 1 / 2);
}

double Y1 = N1 - n[0] * (n[0] + 1) - n[0] * n[1];
double Y2 = N2 - n[4] * (n[4] + 1) - n[4] * n[5];

//printf("n[0]: %f | n[0]: %f \n", n0, n1);
//printf("N1: %f | N2: %f \n", N1, N2);
//printf("Y1: %f | Y2: %f \n", Y1, Y2);
if(Y1 > 0){ // Residual velocity Y1 * Jm * T^2 exists; n2 needs to increase by one period to distribute residual velocity
  *J2 = -1 * (2 * Y1 + n[2] * (n[2] + 1)) * Jm /((n[2] + 1) * (n[2] + 2));
  rn2 = n[2] + 1;
}else
  *J2 = -(*J2);

if(Y2 > 0){ // Residual velocity Y2 * Jm * T^2 exists; n6 needs to increase by one period to distribute residual velocity
  *J4 = (2 * Y2 + n[6] * (n[6] + 1)) * Jm /((n[6] + 1) * (n[6] + 2));
  rn6 = n[6] + 1;
}

// Calculate constant velocity phase and residual distance, also calculate S values for each phase
//http://www.doc88.com/p-9119146814737.html
double S0;
double S1;
double S2;
double S3;
double S4;
double S5;
double S6;
S0 = S1 = S2 = S3 = S4 = S5 = S6 = 0;
//Tp = T
//S0 = ∑(V0 + ∑(k * Jm * Tp^2)) * Tp
//S0 = n0 * V0 * Tp + Tp * ∑∑(k * Jm * Tp^2))
//S0 =  n0 * V0 * Tp + Tp * Jm * Tp^2 * ∑∑k;
//S0 =  n0 * V0 * Tp + Tp * Jm * Tp^2 * n0 * (n0 + 1) * (n0 + 2) / 6;
//Vt1 = Vc + ∑k * Jm * Tp^2
//Vt1 = Vc + Jm * Tp^2 * n * (n + 1) / 2;
//At1 = n0 * Jm * Tp
S0 = n[0] * Vc * T + T * Jm * T * T * n[0] * (n[0] + 1) * (n[0] + 2) / 6.0;
double Vt1 = Vc + Jm * T * T * n[0] * (n[0] + 1) / 2.0;
double At1 = n[0] * Jm * T;

//S1 = ∑(Vt1 + n * At1 * T) * T
//S1 = n * (Vt1 + (n * (n + 1) / 2) * At1 * T) * T
//Vt2 = Vt1 + n1 * At1 * T
S1 = (n[1] * Vt1 + (n[1] * (n[1] + 1) / 2) * At1 * T) *T;
double Vt2 = Vt1 + n[1] * At1 * T;
double At2 = At1;

// May need to increase by one period
// S0 = ∑(V0 + ∑(k * Jm * Tp^2)) * Tp
// S2 = ∑(Vm - ∑(k * J1 * Tp^2)) * Tp
// S2 = ∑(Vm - ∑(k * J1 * Tp^2)) * Tp
// S2 = ∑(Vm -  J1 * Tp^2 * ∑k ) * Tp
// S2 = n2 * Vm * Tp - Tp * J1 * Tp^2 * ∑∑k
// ∑∑k = (n * (n + 1) * (2 * n + 1)) / (2 * 6) -
//       (2 * n2 + 1) * n * (n + 1) / (2 * 2) +
//       n2 * (n2 * n2 + n2) / 2

// S2 =  n2 * Vm * Tp - Tp * J1 * Tp^2 * rn2 * (rn2 + 1) * (rn2 + 2) / 6;
// S4 = n4 * Vm * T - T * Jm * T * T * n4 * (n4 + 1) * (n4 + 2) / 6;
// S6 =  rn6 * Ve * T + T * J4 * T * T * rn6 * (rn6 + 1) * (rn6 + 2) / 6;
double xgmxgmk = (rn2 * (rn2 + 1) * (2 * rn2 + 1)) / (2.0 * 6.0) -
             ((2 * rn2 + 1) * rn2 * (rn2 + 1)) / (2.0 * 2.0) +
              (rn2 * (rn2 * rn2 + rn2)) / 2.0;
S2 = rn2 * Vm * T - T * fabs(*J2) * T * T * xgmxgmk;

double Vt3 = Vm;
double At3 = 0;

// Acceleration phase distance
double Sa = S0 + S1 + S2;

//S4 = ∑(Vm - ∑(k * Jm * Tp^2)) * Tp
//S4 =  n4 * Vm * Tp - Tp * Jm * Tp^2 * n4 * (n4 + 1) * (n4 + 2) / 6;
//double x = n - (tp->n0 + tp->n1 + tp->n2 + tp->n3) ;
//cal_v = tp->vm - tp->jm * T * T * x * (x + 1) / 2;
S4 = n[4] * Vm * T - T * Jm * T * T * n[4] * (n[4] + 1) * (n[4] + 2) / 6.0;
double Vt5 = Vm - Jm * T * T * n[4] * (n[4] + 1) / 2;
double At5 = - n[4] * Jm * T;


//S5 = ∑(Vt5 - n * At5 * T) * T
//S5 = n * (Vt5 - (n * (n + 1) / 2) * At5 * T) * T
//Vt6 = Vt5 - n5 * At5 * T
//cal_v = tp->v5 + x * tp->a6 * T;
//S1 = (n1 * Vt1 + (n1 * (n1 + 1) / 2) * At1 * T) *T;
S5 = (n[5] * Vt5 + (n[5] * (n[5] + 1) / 2) * At5 * T) * T;
double Vt6 = Vt5 + n[5] * At5 * T;
double At6 = At5;

// May need to increase by one period
//S6 = ∑(Ve + ∑(k * j4 * Tp^2)) * Tp
//double x = tp->n0 + tp->n1 + tp->n2 + tp->n3 + tp->n4 + tp->n5 + tp->n6 - n;
//cal_v = tp->v7 + fabs(tp->j4) * T * T *  x * (x + 1) / 2.0;
xgmxgmk = (rn6 * (rn6 + 1) * (2 * rn6 + 1)) / (2.0 * 6.0) -
             ((2 * rn6 + 1) * rn6 * (rn6 + 1)) / (2.0 * 2.0) +
              (rn6 * (rn6 * rn6 + rn6)) / 2.0;
S6 =  rn6 * Ve * T + T * fabs(*J4) * T * T * xgmxgmk;

double Vt7 = Ve;
double At7 = 0;

double Sb = S4 + S5 + S6; // Deceleration distance
double Sc = S - Sa - Sb;  // Constant velocity phase distance
n[3] = floor(Sc / (Vm * T));
double RSc = n[3] * Vm * T;
double Serr = Sc - RSc;    // Residual length of constant velocity segment
double Verr = Serr / T;    // This velocity needs to be inserted at appropriate location

//if(Serr != 0){  // Not useful, poor effect
//double A = Am * T * T / 6.0;
//double B = Ve * T;
//double C = -(Am * T * T / 6.0 + (S6 + Serr));
//double B4AC = sqrt(B * B - 4 * A * C);
//double tmpN1 = ceil((-B + B4AC) / (2 * A));
//printf("ORG N6 : %f  Now N6: %f  \n", rn6, tmpN1);
//rn6 = tmpN1;
//
//printf("ORAG J4 : %f  \n", J4);
//double xxk = (rn6 * (rn6 + 1) * (2 * rn6 + 1)) / (2.0 * 6.0) -
//            ((2 * rn6 + 1) * rn6 * (rn6 + 1)) / (2.0 * 2.0) +
//            (rn6 * (rn6 * rn6 + rn6)) / 2.0;
//J4 = ((S6 + Serr) - rn6 * Ve * T) / (T * T * T * xxk);
//printf("Now J4 : %f \n", J4);
//}


Sb = S4 + S5 + S6; // Deceleration distance

double N = n[0] + n[1] + rn2 + n[3] + n[4] + n[5] + rn6;
  
  n[2] = rn2;
  n[6] = rn6;

  //printf("N: %f | n0: %f | n1 : %f  | n2 : %f  | n3 : %f | n4 : %f | n5 : %f | n6 : %f \n", N, n[0], n[1], n[2], n[3], n[4], n[5], n[6]);

  *tn = N;
  *verr = Verr;
  v[1] = Vt1;
  v[2] = Vt2;
  v[3] = Vt3;
  v[5] = Vt5;
  v[6] = Vt6;
  v[7] = Vt7;

  a[1] = At1;
  a[2] = At2;
  a[3] = At3;
  a[5] = At5;
  a[6] = At6;
  a[7] = At7;

  //printf("SA: %f | SB: %f | SC: %f \n", Sa, Sb, Sc);
  if(Sc < 0)
    return -1;
  return 0;
}  

int getNext(simple_tp_t *tp, double Vs, double Ve, double period){
#define PE tp->pos_cmd   
#define PN tp->curr_pos
#define T  period
#define Jm  tp->max_jerk
//#define Vc tp->curr_vel
#define Vc Vs
#define Ac tp->curr_acc
#define Am tp->max_acc
#define Vm tp->max_vel

tp->vm = tp->max_vel;
tp->use_trapezoid = 0;

double n[8];
double a[8];
double v[8];
double total_n;
double verr;
double J2;
double J4;
double S = fabs(tp->pos_cmd - tp->curr_pos);

double tn[8];
double ta[8];
double tv[8];
double ttotal_n;
double tverr;
double tJ2;
double tJ4;

int time = 0;
double TVmax = Vm;
double TVmax1 = fmax(Vc, Ve);
//double Vmax = Vm;
do{ // Use bisection method to find optimal Vmax
  int res = calcSCurve(S, Vc, Ve, TVmax, Ac, Am, Jm, T, tn, ta, tv, &ttotal_n, &tverr, &tJ2, &tJ4);
  if(res == 0){
    memcpy(n, tn, sizeof(double) * 8);
    memcpy(a, ta, sizeof(double) * 8);
    memcpy(v, tv, sizeof(double) * 8);
    total_n = ttotal_n;
    verr = tverr;
    J2 = tJ2;
    J4 = tJ4;
    break;
  }

  if(res == -1){
    TVmax = (TVmax + TVmax1) / 2;
    //printf("Change Vmax[%f] to %f", tp->vm, TVmax);
    tp->vm = TVmax;
  }

  if(time >= 5){
    tp->use_trapezoid = 1;
    return -1;
  }
  time++;
}while(true);


tp->total_n = total_n;
tp->curr_n = 1;
tp->n0 = n[0];
tp->n1 = n[1];
tp->n2 = n[2];
tp->n3 = n[3];
tp->n4 = n[4];
tp->n5 = n[5];
tp->n6 = n[6];
tp->verr = verr;
tp->vc = Vc;
tp->ve = Ve;
//tp->vm = tp->curr_max_vel;
tp->jm = Jm;
tp->j2 = J2;
tp->j4 = J4;
tp->v1 = v[1];
tp->v2 = v[2];
tp->v3 = v[3];
tp->v5 = v[5];
tp->v6 = v[6];
tp->v7 = v[7];

tp->a1 = a[1];
tp->a2 = a[2];
tp->a3 = a[3];
tp->a5 = a[5];
tp->a6 = a[6];
tp->a7 = a[7];
tp->fix_verr = 0;
tp->prograss = 0;

//printf("N: %f | n0: %f | n1 : %f  | n2 : %f  | n3 : %f | n4 : %f | n5 : %f | n6 : %f \n", total_n, n[0], n[1], n[2], n[3], n[4], n[5], n[6]);

#undef PE
#undef PN
#undef Vc
#undef Ac
#undef Am
#undef Vm
#undef T
#undef Jm
return 0;
}

double getNextPoint(simple_tp_t *tp, int n, double T, double* req_v, double* req_a){
  double cal_v;
  double cal_a;
  int phase = 0;
  if(n <= tp->n0){                                           // S0 segment
    cal_v = tp->vc + tp->jm * T * T * n * (n + 1) / 2;
    cal_a = n * tp->jm * T;
    phase = 0;
  }else if(tp->n1 != 0 && n <= tp->n0 + tp->n1){              // S1 segment
    cal_v = tp->v1 + (n - tp->n0) * tp->a1 * T;
    cal_a = tp->a1;
    phase = 1;
  }else if(n <= tp->n0 + tp->n1 + tp->n2){                    // S2 segment
    //Vm - ∑(k * J1 * Tp^2)
    double x = tp->n0 + tp->n1 + tp->n2 - n;
    cal_v = tp->v3 - fabs(tp->j2) * T * T * x * (x + 1) / 2;
    cal_a = tp->a3 - x * fabs(tp->j2) * T;
    phase = 2;
  }else if(n <= tp->n0 + tp->n1 + tp->n2 + tp->n3){           // S3 segment
    cal_v = tp->vm;
    cal_a = 0;
    phase = 3;
  }else if(n <= tp->n0 + tp->n1 + tp->n2 + tp->n3 + tp->n4){  // S4 segment
    double x = n - (tp->n0 + tp->n1 + tp->n2 + tp->n3) ;
    cal_v = tp->vm - tp->jm * T * T * x * (x + 1) / 2;
    cal_a = - x * tp->jm * T;
    phase = 4;
  }else if(tp->n5 != 0  &&n <= tp->n0 + tp->n1 + tp->n2 + tp->n3 + tp->n4 + tp->n5){
    phase = 5;
    double x = n - (tp->n0 + tp->n1 + tp->n2 + tp->n3 + tp->n4);
    cal_v = tp->v5 + x * tp->a6 * T;
    cal_a = tp->a6;
  }else if(n <= tp->n0 + tp->n1 + tp->n2 + tp->n3 + tp->n4 + tp->n5 + tp->n6){
    phase = 6;
    double x = tp->n0 + tp->n1 + tp->n2 + tp->n3 + tp->n4 + tp->n5 + tp->n6 - n;
    cal_v = tp->v7 + fabs(tp->j4) * T * T *  x * (x + 1) / 2.0;
    cal_a = tp->a6 - x * fabs(tp->j4) * T;
  }else{
    if( tp->n0 + tp->n1 + tp->n2 + tp->n3 + tp->n4 + tp->n5 + tp->n6 == n + 1 ){
      tp->curr_n++ ;
      phase = 7;
      cal_v = tp->ve;
      cal_a = tp->a7;
    }
    return 0;
  }
  if(phase > 3 && tp->fix_verr == 0 && cal_v < tp->verr){
    tp->fix_verr = 1;  // Insert additional residual velocity
    cal_v = tp->verr;
    //printf("ver: %f | acc: %f | n: %d | phase: %d | prograss: %.10f F\n", cal_v, cal_a, n, phase, tp->prograss);
  }else{
    tp->curr_n++ ;
    //printf("ver: %f | acc: %f | n: %d | phase: %d | prograss: %.10f TN: %d\n", cal_v, cal_a, n, phase, tp->prograss, tp->n0 + tp->n1 + tp->n2 + tp->n3 + tp->n4 + tp->n5 + tp->n6);
  }
  tp->prograss += cal_v * T;
  *req_v = cal_v;
  *req_a = cal_a;
  return 0;
}


// PT = P0 + V0 * T + 0.5 * A0 * T^2 + J * T^3 / 6
// VT = V0 + A0 * T + J * T^2 /2
// AT = A0 + J * T
double nextSpeed(double v, double a, double t, double targetV, double maxA, double maxJ, double* req_v, double* req_a, double* req_j) {
  //double maxJ = tp->max_jerk ;
  //double v = tp->curr_vel;
  //double a = tp->curr_acc;
  //double maxA = tp->max_acc;

  // Compute next acceleration
  double nextA = nextAccel(t, targetV, v, a, maxA, maxJ);

  // Compute next velocity
  // PT = P0 + V0 * T + 0.5 * A0 * T^2 + J * T^3 / 6
  // VT = V0 + A0 * T + J * T^2 /2
  // AT = A0 + J * T

  // VT - V0 = (A0 + A0 + J * T) * T / 2 = (A0 + AT) * T / 2
  // 2 * (VT - V0) / T = A0 + AT
  // AT = 2 * (VT - V0) / T - A0

  // VT - V0 = A0 * T + J * T^2 /2
  // VT - V0 = (A0 + AT) * T / 2

  //double deltaV = nextA * t;
  double deltaV = (a + nextA) * t / 2.0;
  if ((deltaV < 0 && targetV < v && v + deltaV < targetV) ||
      (0 < deltaV && v < targetV && targetV < v + deltaV)) {
    //nextA = (targetV - v) / t;
    nextA = 2.0 * (targetV - v) / t - a;
    if(nextA >= maxA){
      nextA = maxA;
      targetV = (a + nextA) * t / 2.0;
    }
    //printf("############################  FIXED nextA to be:  %.14f \n", nextA);
    v = targetV;
  } else {
    //printf("############################  USE nextA:  %.14f \n", nextA);
    v += deltaV;
  }

  // Compute jerk = delta accel / time
  *req_j = (nextA - a) / t;
  if(*req_j > maxJ){
    *req_j = maxJ;
    nextA = a + maxJ * t;
  } else if (*req_j < -maxJ) {
    *req_j = -maxJ;
    nextA = a - maxJ * t;
  }
  *req_a = nextA;
  *req_v = v;

  return v;
}

double getStoppingDist(simple_tp_t *tp) {
    double maxJ = tp->max_jerk ;
    double v = tp->curr_vel;
    double a = tp->curr_acc;
    double maxA = tp->max_acc;
    //double phase;
    return stoppingDist(v, a, maxA, maxJ/*, &phase*/);
}

double stoppingDist(double v, double a, double maxA, double maxJ/*, int* phase*/) {
    // Already stopped
    //*phase = 0;
    if (fabs(v) < 0.0001) return 0;
    // Handle negative velocity
    if (v < 0) {
    v = -v;
    a = -a;
    }

    double d = 0;

    // Compute distance and velocity change to accel = 0
    if (0 < a) {
      // Compute distance to decrease accel to zero
      // distance(double t, double v, double a, double j)
      // distance => v * t + 1/2 * a * t^2 + 1/6 * j * t^3
      // velocity => a * t + 1/2 * j * t^2
      double t = a / maxJ;
      d += sc_distance(t, v, a, -maxJ);
      v += velocity(t, a, -maxJ);
      a = 0;
      //if(*phase == 0) *phase = 3;
    }

    // Compute max deccel
    // PT = P0 + V0 * T + 0.5 * A0 * T^2 + J * T^3 / 6
    // VT = V0 + A0 * T + J * T^2 /2
    // AT = A0 + J * T
    // 
    // Calculate maxDeccel from both sides
    // At target velocity, both velocity and acceleration are 0
    // So VT = 0 + 0 * T1 + J * T1^2 / 2
    // Also because Amax = J * T1, VT = J * J * T1 * T1 / (2 * J) = Amax^2 / (2 * J)
    // Therefore Amax^2 = 2 * J * VT
    // From another perspective VT = v + a * T2 - J * T2^2 / 2
    // Also because Amax = a - J * T2, VT = v + (a + a - J * T2) * T2 / 2 = v + (a + Amax) * T2 / 2
    // T2 = (a - Amax) / J
    // Amax^2 / (2 * J) = v + (a + Amax) * T2 / 2 = v + (a^2 - Amax^2) / (2 * J)
    // Amax^2 = 2 * J * v + a^2 - Amax^2
    // 2 * Amax^2 = 2 * J * v + a^2
    // Amax^2 = v * J + 0.5 * a * a
    double maxDeccel = -sqrt(v * maxJ + 0.5 * a * a);
    if (maxDeccel < -maxA) maxDeccel = -maxA;
    //double maxDeccel = -fabs(maxA);

    // Compute distance and velocity change to max deccel
    // a * t + 1/2 * j * t^2
    if (maxDeccel < a) {
        double t = (a - maxDeccel) / maxJ;
        d += sc_distance(t, v, a, -maxJ);
        v += velocity(t, a, -maxJ);
        a = maxDeccel;
    }

    // Compute velocity change over remaining accel
    // VT = J * J * T1 * T1 / (2 * J) = Amax^2 / (2 * J)
    double deltaV = 0.5 * a * a / maxJ;

    // Compute constant deccel period
    // P = v * t + 1/2 * a * t^2 + 1/6 * j * t^3
    // VT = V0 + A0 * T + J * T^2 /2
    if (deltaV < v) {
      // distance => v * t + 1/2 * a * t^2 + 1/6 * j * t^3
      // velocity => a * t + 1/2 * j * t^2
        double t = (v - deltaV) / -a;
        d += sc_distance(t, v, a, 0);
        v += velocity(t, a, 0);
        //if(*phase == 0) *phase = 6;
    }

    // Compute distance to zero vel
    d += sc_distance(-a / maxJ, v, a, maxJ);
    //if(*phase == 0) *phase = 7;

    return d;
}

double finishWithSpeedDist(double v, double ve, double a, double maxA, double maxJ/*, int* phase*/) {
    // Handle negative velocity: transform to positive domain
    if (v < 0) {
        v = -v;
        a = -a;
        ve = -ve;
    }

    // Velocity difference too small, no accel/decel needed
    if (fabs(v - ve) < 0.0001 && fabs(a) < 0.0001) {
        return 0;
    }

    double d = 0;

    // ========== Acceleration case (ve > v) ==========
    if (ve > v) {
        // Phase 1: If a < 0 (decelerating), first bring a to 0
        if (a < 0) {
            double t = -a / maxJ;
            d += sc_distance(t, v, a, maxJ);
            v += velocity(t, a, maxJ);
            a = 0;
        }

        // Compute required max acceleration
        // Amax^2 = (ve - v) * maxJ + 0.5 * a * a
        double sqrt_arg = (ve - v) * maxJ + 0.5 * a * a;
        if (sqrt_arg < 0) sqrt_arg = 0;
        double maxAccel = sqrt(sqrt_arg);
        if (maxAccel > maxA) maxAccel = maxA;

        // Phase 2: Increase a from current value to maxAccel
        if (maxAccel > a) {
            double t = (maxAccel - a) / maxJ;
            d += sc_distance(t, v, a, maxJ);
            v += velocity(t, a, maxJ);
            a = maxAccel;
        }

        // Compute velocity when entering decel phase
        double deltaV = ve - 0.5 * a * a / maxJ;

        // Phase 3: Constant acceleration phase (if needed)
        if (v < deltaV && a > 0.0001) {
            double t = (deltaV - v) / a;
            d += sc_distance(t, v, a, 0);
            v += velocity(t, a, 0);
        }

        // Phase 4: Decrease a from maxAccel to 0, velocity reaches ve
        if (a > 0.0001) {
            double t = a / maxJ;
            d += sc_distance(t, v, a, -maxJ);
        }

        return d;
    }

    // ========== Deceleration case (v > ve) ==========

    // Phase 1: If a > 0 (accelerating), first bring a to 0
    if (a > 0) {
        double t = a / maxJ;
        d += sc_distance(t, v, a, -maxJ);
        v += velocity(t, a, -maxJ);
        a = 0;
    }

    // Compute required max deceleration
    // Amax^2 = (v - ve) * maxJ + 0.5 * a * a
    double sqrt_arg = (v - ve) * maxJ + 0.5 * a * a;
    if (sqrt_arg < 0) sqrt_arg = 0;
    double maxDeccel = -sqrt(sqrt_arg);
    if (maxDeccel < -maxA) maxDeccel = -maxA;

    // Phase 2: Decrease a from current value to maxDeccel
    if (maxDeccel < a) {
        double t = (a - maxDeccel) / maxJ;
        d += sc_distance(t, v, a, -maxJ);
        v += velocity(t, a, -maxJ);
        a = maxDeccel;
    }

    // Compute velocity when entering decel end phase
    // VT = Ve + Amax^2 / (2 * J)
    double deltaV = ve + 0.5 * a * a / maxJ;

    // Phase 3: Constant deceleration phase (if needed)
    if (deltaV < v && a < -0.0001) {
        double t = (v - deltaV) / (-a);
        d += sc_distance(t, v, a, 0);
        v += velocity(t, a, 0);
    }

    // Phase 4: Increase a from maxDeccel to 0, velocity reaches ve
    if (a < -0.0001) {
        double t = (-a) / maxJ;
        d += sc_distance(t, v, a, maxJ);
    }

    return d;
}

// PT = P0 + V0 * T + 0.5 * A0 * T^2 + J * T^3 / 6
// VT = V0 + A0 * T + J * T^2 /2
// AT = A0 + J * T
/*
double nextAccel(double t, double targetV, double v, double a, double maxA,
                        double maxJ) {
  int increasing = v < targetV;
  double deltaA = acceleration(t, maxJ);

  if (increasing && a < -deltaA)
    return a + deltaA; // negative accel, increasing speed

  if (!increasing && deltaA < a)
    return a - deltaA; // positive accel, decreasing speed

  // VT = V0 + A0 * T + J * T^2 /2
  // When reaching target velocity, acceleration is 0
  // So we calculate in reverse
  // V0 = VT + 0 * T - J * T^2 / 2
  // J * T^2 = 2 * (VT - V0)
  // J * J * T^2 = 2 * (VT - V0) * J
  // A = 0 + J * T
  // A^2 = 2 * (VT - V0) * J

  // VT - V0 = A0 * T + J * T^2 /2
  // VT - V0 = (A0 + AT) * T / 2

  // PT = P0 + V0 * T + 0.5 * A0 * T^2 + J * T^3 / 6
  // VT = V0 + A0 * T + J * T^2 /2
  // AT = A0 + J * T
  double deltaV = fabs(targetV - v);
  double targetA = sqrt(2 * deltaV * maxJ);
  //double targetA = fabs((((targetV - v) * 2) / t) - a);

  if (maxA < targetA) targetA = maxA;

  if (increasing) {
    if (targetA < a + deltaA) {
      printf("return targetA:  %.14f a: %.14f targetV: %.14f v: %.14f maxA: %.14f", targetA, a, targetV, v, maxA);
      printf("\n");
      return targetA;
    }
    return a + deltaA;

  } else {
    if (a - deltaA < -targetA) {
      printf("return targetA:  %.14f a: %.14f targetV: %.14f v: %.14f maxA: %.14f", -targetA, a, targetV, v, maxA);
      printf("\n");
      return -targetA;
    }
    return a - deltaA;
  }
}
*/
double nextAccel(double t, double targetV, double v, double a, double maxA,
                        double maxJ) {
  double max_da, tiny_da, vel_err, acc_req;
  max_da = acceleration(t, maxJ);
  tiny_da = max_da * t * 0.001;
  vel_err = targetV - v;
  if (vel_err > tiny_da){
    acc_req = -max_da +
              sqrt(2.0 * maxJ * vel_err + max_da * max_da);
  }else if (vel_err < -tiny_da){
    acc_req = max_da -
              sqrt(-2.0 * maxJ * vel_err + max_da * max_da);
  }else{
    /* within 'tiny_da' of desired pos, no need to move */
    acc_req = 0.0;
  }
  /* limit velocity request */
  if (acc_req > maxA){
    acc_req = maxA;
  }else if (acc_req < -maxA){
    acc_req = -maxA;
  }
  /* ramp velocity toward request at accel limit */
  if (acc_req > a + max_da){
    return a + max_da;
  }else if (acc_req < a - max_da){
    return a - max_da;
  }else{
    return acc_req;
  }
}

double sc_distance(double t, double v, double a, double j) {
  // v * t + 1/2 * a * t^2 + 1/6 * j * t^3
  return t * (v + t * (0.5 * a + 1.0 / 6.0 * j * t));
}


double velocity(double t, double a, double j) {
  // a * t + 1/2 * j * t^2
  return t * (a + 0.5 * j * t);
}


double acceleration(double t, double j) {return j * t;}

#include "tp_types.h"

// Optimized efficient calculation version
double calculate_t2_optimized(double Ve, double distence, double maxA, double maxJ) {
    // 1. Fast path: check simple cases
    if (fabs(maxA) < 1e-10 || fabs(maxJ) < 1e-10) {
        return 0.0;
    }

    // 2. Pre-calculate and reuse common values
    const double maxA_inv = 1.0 / maxA;           // Avoid division
    const double maxJ_inv = 1.0 / maxJ;           // Avoid division
    const double maxA_squared = maxA * maxA;      // Reuse
    //const double maxJ_squared = maxJ * maxJ;      // Reuse

    // 3. Use more efficient expressions
    // Original: 2.0*distence*maxA + Ve*Ve + pow(maxA,4.0)/(4.0*maxJ*maxJ) - (maxA*maxA*Ve)/maxJ
    // Optimized: use pre-calculated values and multiplication instead of division
    const double term1 = 2.0 * distence * maxA;
    const double term2 = Ve * Ve;
    const double term3 = (maxA_squared * maxA_squared) * (0.25 * maxJ_inv * maxJ_inv);
    const double term4 = maxA_squared * Ve * maxJ_inv;

    // 4. Use fma to optimize multiply-add operations
    double sqrt_term = fma(term1, 1.0, term2);
    sqrt_term = fma(term3, 1.0, sqrt_term);
    sqrt_term = fma(-term4, 1.0, sqrt_term);

    // 5. Quick check if square root calculation is needed
    if (sqrt_term <= 0.0) {
        return 0.0;
    }

    // 6. Use more efficient square root calculation
    const double sqrt_result = sqrt(sqrt_term);

    // 7. Optimize final calculation
    // Original: -(2.0*Ve ± 2.0*sqrt_result + (3.0*maxA_squared)/maxJ)/(2.0*maxA)
    // Optimized: use pre-calculated reciprocal to avoid division
    const double common_term = 3.0 * maxA_squared * maxJ_inv;
    const double factor = 0.5 * maxA_inv;  // 1/(2*maxA)

    // 8. Use fma to optimize final calculation
    double t2_1 = fma(-(2.0 * Ve - 2.0 * sqrt_result + common_term), factor, 0.0);
    double t2_2 = fma(-(2.0 * Ve + 2.0 * sqrt_result + common_term), factor, 0.0);

    // 9. Quickly select valid solution
    return (t2_1 > 0.0) ? t2_1 : ((t2_2 > 0.0) ? t2_2 : 0.0);
}

int findSCurveVSpeedWithEndSpeed(double distence, double Ve, double maxA, double maxJ, double* req_v){
  // Solve cubic equation: 2 * ve * t + j * t^3 = D
  //    j*t^3 + 2 * ve * t - D = 0
  //    A*t^3 + B*t^2 + C*t^1 + D = 0
  if(fabs(Ve) <= TP_VEL_EPSILON)
    return findSCurveVSpeed(distence, maxA, maxJ, req_v);

  distence = distence / 2.0;
  double Vs = 0;
  double A = maxJ;
  double B = 0;
  double C = 2 * Ve;
  double D = - distence;
  double t1 = 0;//solute(A,B,C,D,T2);
  double t2 = 0;
  //double x1, x2, x3;
  //int n;
  int res = 1;
  //x1 = x2 = x3 = 0;
  //n = solve_cubic(A, B, C, D, &x1, &x2, &x3);
  //double temp;

  double xo[10];
  int len;
  solve_cubic(A, B, C, D, xo, &len);
  t1 = xo[0];
  int i = 0;
  for (; i < len; i++)
  {
    if(i == 0)continue;
    t1 = fmax(xo[i], t1);
  }

  //t1 = fmax(fmax(x1, x2), x3);
  //if(x1>x2) temp=x1,x1=x2,x2=temp;
  //if(x2>x3) temp=x2,x2=x3,x3=temp;
  //if(x1>x2) temp=x1,x1=x2,x2=temp;
  //if(x1 > 0) t1 = x1;
  //else if(x2 > 0) t1 = x2;
  //else if(x3 > 0) t1 = x3;
  //else {
  if(t1 < 0){
    t1 = 0.001;
    //printf("Cannot find t1 %f*t^3 + %f*t - %f = 0 \n", A, C, distence);
    res = -1;
  }

  //printf("[%f*t^3 + %f*t - %f = 0] t1 = %f\n", A, C, distence, t1);
  double a1;
  a1 = maxJ * t1;
  if(a1 < maxA){ // S2 segment does not exist
        //jerk*t1^2 + jerk*t2*t1 + ve
        Vs = maxJ * t1 * t1  + Ve;
  }else{
      // t2_1 = -(2*ve - 2*(2*D*a2 + ve^2 + a2^4/(4*jerk^2) - (a2^2*ve)/jerk)^(1/2) + (3*a2^2)/jerk)/(2*a2)
      // t2_2 = -(2*ve + 2*(2*D*a2 + ve^2 + a2^4/(4*jerk^2) - (a2^2*ve)/jerk)^(1/2) + (3*a2^2)/jerk)/(2*a2)
      //double t2_1 = -(2*Ve-2*sqrt(2*distence*maxA+Ve*Ve+pow(maxA,4)/(4*maxJ*maxJ)-(maxA*maxA*Ve)/maxJ)+(3*maxA*maxA)/maxJ)/(2*maxA);
      //double t2_2 = -(2*Ve+2*sqrt(2*distence*maxA+Ve*Ve+pow(maxA,4)/(4*maxJ*maxJ)-(maxA*maxA*Ve)/maxJ)+(3*maxA*maxA)/maxJ)/(2*maxA);
      //if(t2_1 > 0)
      //    t2 = t2_1;
      //else
      //    t2 = t2_2;
      t2 = calculate_t2_optimized(Ve, distence, maxA, maxJ);
      if(t2 <= 0){ // S2 segment does not exist, S1 => S3
          //printf("Cannot find t2 t2_1: %f t2_2: %f\n", t2_1, t2_2);
          Vs = maxJ * t1 * t1  + Ve;
          res = -2;
      }else{
          //vs = jerk*t1^2 + jerk*t2*t1 + ve
          t1 = maxA / maxJ;
          Vs = maxJ * t1 * t1 + maxJ * t2 * t1 + Ve;
      }
      //printf("t2_1: %f t2_2: %f\n", t2_1, t2_2);
  }
  *req_v = Vs;
  return res;
}

int findSCurveVSpeed(double distence,/* double maxV,*/ double maxA, double maxJ, double* req_v){
    double t1, t2;
    double a1;
    double Vs = 0;
    distence = distence / 2.0;

    //rtai
    t1 = pow(distence / maxJ, 1.0/3.0);
    //uspace rt-preempt
    //t1 = cbrt(distence/ maxJ);
    a1 = maxJ * t1;
    if(a1 < maxA){ // S2 segment does not exist, S1 => S3
        Vs = maxJ * t1 * t1;
    }else{
        //double t2_1 = (2* sqrt((maxA*(8*distence + pow(maxA, 3)/pow(maxJ, 2)))/4) - (3 * pow(maxA, 2))/ maxJ)/(2 * maxA);
        //double t2_2 = - (2 * sqrt((maxA*(8*distence + pow(maxA, 3)/pow(maxJ, 2)))/4) + (3*pow(maxA, 2))/maxJ)/(2 * maxA);
        //if(t2_1 > 0)
        //    t2 = t2_1;
        //else
        //    t2 = t2_2;
        t2 = calculate_t2_optimized(0.0, distence, maxA, maxJ);
        if(t2 <= 0){ // S2 segment does not exist, S1 => S3
            //printf("Cannot find t2 t2_1: %f t2_2: %f\n", t2_1, t2_2);
            Vs = maxJ * t1 * t1;
        }else{
          t1 = maxA / maxJ;
          Vs = maxJ * t1 * t1 + maxJ * t2 * t1;
        }
    }
    *req_v = Vs;
    return 1;
}

double calcDecelerateTimes(double v, double amax, double jerk, double* t1, double* t2){
  v = fabs(v);
  double T1 = amax / jerk;
  double T2 = 0;
  double V1 = jerk * T1 * T1 / 2;
  if(V1 >= v / 2){ // Handle case where jerk is small, causing T2 and T6 segments to not exist
    T1 = sqrt(v / jerk);
  }
  if(V1 < v / 2){
     T2 = (v / amax) - T1;
  }
  if(t1 != NULL)
    *t1 = T1;
  if(t2 != NULL)
    *t2 = T2;
  return T1 * 2 + T2;
}

double calcSCurveSpeedWithT(double amax, double jerk, double T) {
    // Parameter validation
    if (T <= 0.0 || jerk <= 0.0 || amax <= 0.0) {
        return 0.0;
    }

    // Use more stable calculation method
    const double T1 = fmin(amax / jerk, T / 2.0);  // Avoid T2 being negative
    const double T2 = T - 2.0 * T1;

    // Use fma to optimize multiply-add operations, improving numerical stability
    return fma(jerk * T1, T1 + T2, 0.0);
}

/**
 * Conservative S-curve reachable velocity calculation (for lookahead optimization)
 *
 * Unlike findSCurveVSpeedWithEndSpeed which assumes decel starts from a=0,
 * this function considers worst case: decel starting from a=maxA state.
 *
 * This ensures lookahead-computed velocities can smoothly decel to target
 * regardless of current acc state, avoiding acc discontinuities.
 *
 * @param distance   Available decel distance
 * @param Ve         Target end velocity
 * @param maxA       Maximum acceleration
 * @param maxJ       Maximum jerk
 * @param req_v      [output] Computed conservative reachable velocity
 * @return           1 success, -1 failure
 */
int findSCurveVSpeedConservative(double distance, double Ve, double maxA, double maxJ, double* req_v) {
    if (distance <= 0 || maxA <= 0 || maxJ <= 0) {
        *req_v = Ve;
        return -1;
    }

    // Extra distance needed to bring a from maxA to 0 (velocity still increasing during this)
    // t = maxA / maxJ, v_increase = 0.5 * maxA^2 / maxJ
    double v_increase = 0.5 * maxA * maxA / maxJ;
    double d_extra = maxA * maxA / (2.0 * maxJ);

    // Effective decel distance = total - extra
    double effective_distance = distance - d_extra;

    if (effective_distance <= 0) {
        *req_v = Ve;
        return -1;
    }

    double vs_from_zero;
    int res = findSCurveVSpeedWithEndSpeed(effective_distance, Ve, maxA, maxJ, &vs_from_zero);

    if (res != 1) {
        *req_v = Ve;
        return -1;
    }

    // Conservative estimate: subtract velocity increase during acc ramp-down
    *req_v = fmax(vs_from_zero - v_increase * 0.5, Ve);

    return 1;
}