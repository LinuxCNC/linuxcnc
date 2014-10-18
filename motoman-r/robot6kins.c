/********************************************************************
* Description: robot6kins.c
*   Kinematics for  6 axis ABB,Fanuc,Kuka type robot.
*   This serial arm robot has the following DH parameters:
*   a1,a2,a3, d1,d2,d4,d6
*
* Author: Rudy du Preez (SA-CNC-CLUB)
* License: GPL Version 2
*    
********************************************************************/

#include "kinematics.h"
#include "hal.h"
#include "rtapi.h"
#include "rtapi_math.h"


struct haldata {
    hal_float_t *DH_a1;
    hal_float_t *DH_a2;
    hal_float_t *DH_a3;
    hal_float_t *DH_d1;
    hal_float_t *DH_d2;
    hal_float_t *DH_d4;
    hal_float_t *DH_d6;
    hal_bit_t *tmode;
    hal_bit_t *soffs;
} *haldata;

double Ao = 0;
double Bo = 0;
double Co = 0;

double aw = 0;
double bw = 0;
double cw = 0;
double To[4][4];
double Tt[4][4];
double Tw[4][4];

int elbup = 1;
int wrflp = 0;
int wsing = 0;

double eps = 0.0001;

//=======================================================================
int fwABC2(double ux, double uy, double uz, double vx, double vy, 
           double vz, double wz)
{
// to Craig: p40 RPY(c,b,a) => (Rz,Ry,Rz)

    bw = atan2(-uz, sqrt(ux*ux + uy*uy) );
    if (fabs(fabs(bw) - M_PI/2.0) > eps) {
       aw = atan2(vz, wz);
       cw = atan2(uy, ux);
    } else if (bw > 0.0) {
       aw = atan2(vx, vy);
       bw = M_PI/2.0;
       cw = 0;
    } else {
       aw = -atan2(vx, vy);
       bw = -M_PI/2.0;
       cw = 0;
    }
    if (aw < 0) {aw = aw + 2*M_PI;}

    return 0;
}
//=======================================================================   
static void MatMult(double A[][4], double B[][4], double C[][4])
{
  int i, j, k;
  
  for (i=0; i<=3; ++i){
    for (j=0; j<=3; ++j){
      C[i][j] = 0;
      for (k=0; k<=3; ++k){
        C[i][j] = C[i][j] + A[i][k]*B[k][j];
      }
    }
  }
}
//=====================================================================
static void PoseMat(double X, double Y, double Z,
                    double A, double B, double C, double T[][4])
{
double cr, sr, cp, sp, cy, sy;
  
       sr = sin(A); cr = cos(A);
       sp = sin(B); cp = cos(B);
       sy = sin(C); cy = cos(C);
    
       T[0][0] = cp*cy;
       T[1][0] = cp*sy;
       T[2][0] = -sp;
       T[3][0] = 0;

       T[0][1] = sr*sp*cy - cr*sy;
       T[1][1] = sr*sp*sy + cr*cy;
       T[2][1] = sr*cp;
       T[3][1] = 0;

       T[0][2] = cr*sp*cy + sr*sy;
       T[1][2] = cr*sp*sy - sr*cy;
       T[2][2] = cr*cp;
       T[3][2] = 0;

       T[0][3] = X;
       T[1][3] = Y;
       T[2][3] = Z;
       T[3][3] = 1; 
}
//========================================================================
int kinematicsForward(const double *joint,
		      EmcPose * pos,
		      const KINEMATICS_FORWARD_FLAGS * fflags,
		      KINEMATICS_INVERSE_FLAGS * iflags)
{
    double a1 = *(haldata->DH_a1);
    double a2 = *(haldata->DH_a2);
    double a3 = *(haldata->DH_a3);
    double d1 = *(haldata->DH_d1);
    double d2 = *(haldata->DH_d2);
    double d4 = *(haldata->DH_d4);
    double d6 = *(haldata->DH_d6);
    int tmode = *(haldata->tmode);
    int soffs = *(haldata->soffs);

    double th1 = joint[0]*M_PI/180;
    double th2 = joint[1]*M_PI/180;
    double th3 = joint[2]*M_PI/180;
    double th4 = joint[3]*M_PI/180;
    double th5 = joint[4]*M_PI/180;
    double th6 = joint[5]*M_PI/180;

    double c1, s1, c2, s2, c3, s3, c4, s4, c5, s5, c6, s6, c23, s23;
    double u511, u512, u521, u522, u411, u412, u413, u421, u422, u423;
    double u311, u312, u313, u314, u321, u322, u323, u324;
    double u211, u212, u213, u214, u221, u222, u223, u224, v114;
    double ux, uy, uz, vx, vy, vz, wx, wy, wz, qx, qy, qz;

    c1 = cos(th1);  s1 = sin(th1);
    c2 = cos(th2);  s2 = sin(th2);
    c3 = cos(th3);  s3 = sin(th3);
    c4 = cos(th4);  s4 = sin(th4);
    c5 = cos(th5);  s5 = sin(th5);
    c6 = cos(th6);  s6 = sin(th6);
    c23 = cos(th2+th3);  s23 = sin(th2+th3);

    u511 = c5*c6; u512 = -c5*s6;
    u521 = s5*c6; u522 = -s5*s6;

    u411 = c4*u511 - s4*s6; 
    u412 = c4*u512 - s4*c6; 
    u413 = c4*s5;
    u421 = s4*u511 + c4*s6;
    u422 = s4*u512 + c4*c6; 
    u423 = s4*s5;

    u311 = c3*u411 - s3*u521; 
    u312 = c3*u412 - s3*u522;
    u313 = c3*u413 + s3*c5;
    u314 = s3*d4 + c3*a3;

    u321 = s3*u411 + c3*u521; 
    u322 = s3*u412 + c3*u522;
    u323 = s3*u413 - c3*c5;
    u324 = -c3*d4 + s3*a3;

    u211 = c23*u411 - s23*u521; 
    u212 = c23*u412 - s23*u522;
    u213 = c23*u413 + s23*c5;
    u214 = s23*d4 + c23*a3 + c2*a2;

    u221 = s23*u411 + c23*u521; 
    u222 = s23*u412 + c23*u522;
    u223 = s23*u413 - c23*c5; 
    u224 = -c23*d4 + s23*a3 + s2*a2;
    v114 = -s23*d4 + c23*a3 + c2*a2;

    ux = c1*u211 + s1*u421;
    uy = s1*u211 - c1*u421;
    uz = u221;
    vx = c1*u212 + s1*u422;
    vy = s1*u212 - c1*u422;
    vz = u222;
    wx = c1*u213 + s1*u423;
    wy = s1*u213 - c1*u423;
    wz = u223;
    qx = c1*u214 + s1*d2 + c1*a1 + d6*wx;
    qy = s1*u214 - c1*d2 + s1*a1 + d6*wy;
    qz = u224 + d1 + d6*wz;

     fwABC2(ux, uy, uz, vx, vy, vz, wz);

    if (v114 > 0) {elbup = 1;} else {elbup = 0;}
    if (s5 >= 0)  {wrflp = 0;} else {wrflp = 1;}
    if (fabs(s5) < eps)   {wsing = 1;} else {wsing = 0;}

    if (soffs) { 
//   
      To[0][0] = ux; To[0][1] = vx; To[0][2] = wx; To[0][3] = qx;
      To[1][0] = uy; To[1][1] = vy; To[1][2] = wy; To[1][3] = qy;
      To[2][0] = uz; To[2][1] = vz; To[2][2] = wz; To[2][3] = qz;
      To[3][0] =  0; To[3][1] =  0; To[3][2] =  0; To[3][3] =  1;

      Ao = aw; Bo = bw; Co = cw;
    }

    if (tmode) {
      PoseMat(qx-To[0][3], qy-To[1][3], qz-To[2][3],
               aw-Ao, bw-Bo, cw-Co, Tt);
      MatMult(To, Tt, Tw);
      ux = Tw[0][0]; vx = Tw[0][1]; wx = Tw[0][2]; qx = Tw[0][3];
      uy = Tw[1][0]; vy = Tw[1][1]; wy = Tw[1][2]; qy = Tw[1][3];
      uz = Tw[2][0]; vz = Tw[2][1]; wz = Tw[2][2]; qz = Tw[2][3];
    } 

    fwABC2(ux, uy, uz, vx, vy, vz, wz);

    pos->tran.x = qx; 
    pos->tran.y = qy;
    pos->tran.z = qz;
    pos->a = aw/M_PI*180;
    pos->b = bw/M_PI*180;
    pos->c = cw/M_PI*180;

    return 0;
}

//============================================================================
int kinematicsInverse(const EmcPose * pos,
		      double *joint,
		      const KINEMATICS_INVERSE_FLAGS * iflags,
		      KINEMATICS_FORWARD_FLAGS * fflags)
{      
    double a1 = *(haldata->DH_a1);
    double a2 = *(haldata->DH_a2);
    double a3 = *(haldata->DH_a3);
    double d1 = *(haldata->DH_d1);
    double d2 = *(haldata->DH_d2);
    double d4 = *(haldata->DH_d4);
    double d6 = *(haldata->DH_d6);
    int tmode = *(haldata->tmode);

    double Xt = pos->tran.x;
    double Yt = pos->tran.y;
    double Zt = pos->tran.z;
    double At = pos->a/180*M_PI;
    double Bt = pos->b/180*M_PI;
    double Ct = pos->c/180*M_PI;

    double th1, th2, th3, th4, th5, th6;
    double ux, uy, uz, vx, vy, vz, wx, wy, wz, px, py, pz;
    double r, k1, k2, qx, qy, qz;
    double c1, s1, c2, s2, c3, s3, c4, s4, c5, s5, c23, s23;
    double v114, v124, v214, v224, v113, v313, v323;
    double v111, v131, v311, v331, v411, v431;
    static double th4old = 0.0;
// 
    int n1 = 1; // shoulder on the right
    int n2 = 1; // elbow up
    int n4 = 1; // wrist not flipped

    joint[6] = To[0][3];  joint[7] = To[1][3]; joint[8] = To[2][3]; 

    if (tmode) {
//               tool coordinates
       PoseMat(Xt-To[0][3], Yt-To[1][3], Zt-To[2][3],
               At-Ao, Bt-Bo, Ct-Co, Tt);
       MatMult(To, Tt, Tw);
    } else {
//               world coordinates
       PoseMat(Xt, Yt, Zt, At, Bt, Ct, Tw);
    }

      ux = Tw[0][0]; vx = Tw[0][1]; wx = Tw[0][2]; qx = Tw[0][3];
      uy = Tw[1][0]; vy = Tw[1][1]; wy = Tw[1][2]; qy = Tw[1][3];
      uz = Tw[2][0]; vz = Tw[2][1]; wz = Tw[2][2]; qz = Tw[2][3];

/*  wrist position --------------------------------------------------*/
    px = qx - d6*wx;
    py = qy - d6*wy;
    pz = qz - d6*wz;

/*  solve for th1 ---------------------------------------------------*/
    r = sqrt(px*px + py*py);
    if (r < d2) {
/*      'ERROR:--------- point not reachable' */
        return 1;
    }
    k1 = atan2(py, px);
    k2 = asin(d2/r);
    if (n1 == 1) { th1 = k1 + k2;}
    else { th1 = k1 - k2 + M_PI;}
    c1 = cos(th1);  s1 = sin(th1);

/*  solve for th2 ---------------------------------------------------*/
    v114 = px*c1 + py*s1 - a1;
    v124 = pz - d1;
    r = sqrt(v114*v114 + v124*v124);
    k1 = (a2*a2 - d4*d4 - a3*a3 + v114*v114 + v124*v124)/(2*a2*r);
    if (abs(k1) > 1) {
/*       'ERROR:--------- point not reachable'; */
         return 2;
    }
    k2 = acos(k1);
    if (elbup == 1) {n2 = 1;}
    else {n2 = -1;}
    th2 = atan2(v124, v114) + n2*k2;
    c2 = cos(th2); s2 = sin(th2);

/*  solve for  th3  -----------------------------------------------*/
    v214 =  c2*v114 + s2*v124 - a2;
    v224 = -s2*v114 + c2*v124;
    th3 = -atan2(a3, d4) + atan2(v214, -v224);
    c3 = cos(th3); s3 = sin(th3);

/*  solve for  th4  -----------------------------------------------*/
    c23 = cos(th2+th3); s23 = sin(th2+th3);
    v113 = c1*wx + s1*wy;
    v313 = c23*v113 + s23*wz;
    v323 = s1*wx - c1*wy;

    if ((fabs(v323) < eps) && (fabs(v313) < eps)){ th4 = 0;}
    else {th4 = atan2(n4*v323, n4*v313);}
//         take care of singularities and map for continuity   
    if ((fabs(v323) < eps) && (v313 < eps)) {th4 = th4old;}
    if ((v323 > eps) && (v313 < eps)) {th4 = th4 - 2*M_PI;}
    if ((fabs(v113) < eps) && (fabs(v313) < eps) &&
        (fabs(v323) < eps) ) {th4 = th4old;}
    th4old = th4;
//  
    c4 = cos(th4); s4 = sin(th4);

/*  solve for  th5  -------------------------------------------------*/
    k1 = c4*v313 + s4*v323;
    k2 = s23*v113 - c23*wz;
    th5 = atan2(k1, k2);
//  
    c5 = cos(th5); s5 = sin(th5);

/*  solve for th6  --------------------------------------------------*/
    v111 = c1*ux + s1*uy;
    v131 = s1*ux - c1*uy;
    v311 = c23*v111 + s23*uz;
    v331 = s23*v111 - c23*uz;
    v411 =  c4*v311 + s4*v131;
    v431 = -s4*v311 + c4*v131;
// 
    k1 = v431;  
    k2 = c5*v411 - s5*v331;
    th6 = atan2(k1, k2);
//  
/* convert to degrees ------------------------------------------------*/
   joint[0] = th1*180/PM_PI;
   joint[1] = th2*180/PM_PI;
   joint[2] = th3*180/PM_PI;
   joint[3] = th4*180/PM_PI;
   joint[4] = th5*180/PM_PI;
   joint[5] = th6*180/PM_PI;

   return 0;
}
//=======================================================================
KINEMATICS_TYPE kinematicsType()
{
    return KINEMATICS_BOTH;
}

#include "rtapi.h"		/* RTAPI realtime OS API */
#include "rtapi_app.h"		/* RTAPI realtime module decls */
#include "hal.h"


EXPORT_SYMBOL(kinematicsType);
EXPORT_SYMBOL(kinematicsInverse);
EXPORT_SYMBOL(kinematicsForward);
MODULE_LICENSE("GPL");

int comp_id;
int rtapi_app_main(void) {
    int res = 0;
    comp_id = hal_init("robot6kins");
    if(comp_id < 0) return comp_id;

    haldata = hal_malloc(sizeof(struct haldata));

    if((res = hal_pin_float_new("robot6kins.DH-a1", HAL_IO, &(haldata->DH_a1), comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("robot6kins.DH-a2", HAL_IO, &(haldata->DH_a2), comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("robot6kins.DH-a3", HAL_IN, &(haldata->DH_a3), comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("robot6kins.DH-d1", HAL_IO, &(haldata->DH_d1), comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("robot6kins.DH-d2", HAL_IO, &(haldata->DH_d2), comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("robot6kins.DH-d4", HAL_IN, &(haldata->DH_d4), comp_id)) < 0) goto error;
    if((res = hal_pin_float_new("robot6kins.DH-d6", HAL_IN, &(haldata->DH_d6), comp_id)) < 0) goto error;
    if((res = hal_pin_bit_new("robot6kins.soffs", HAL_IN, &(haldata->soffs), comp_id)) < 0) goto error;
    if((res = hal_pin_bit_new("robot6kins.tmode", HAL_IN, &(haldata->tmode), comp_id)) < 0) goto error;
    hal_ready(comp_id);
    return 0;

error:
    hal_exit(comp_id);
    return res;

}

void rtapi_app_exit(void) { hal_exit(comp_id); }
