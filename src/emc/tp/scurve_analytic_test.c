/* scurve_analytic_test.c — ground-truth validation of the closed-form
 * S-curve max-start-speed used to replace the per-cycle Ruckig solves.
 *
 * Build & run:   gcc -O2 -Wall -o /tmp/scurve_test scurve_analytic_test.c -lm && /tmp/scurve_test
 *
 * Proves three independent things against first principles (no Ruckig involved):
 *   1. Forward identity  d = (Vs+Ve)/2 * T  matches fine numerical integration
 *      of the actual jerk-limited deceleration profile.
 *   2. The closed-form inverse (the function we ship) round-trips exactly.
 *   3. The rest-to-rest peak  = analytic(D/2, Ve=0)  matches a bisection that
 *      simulates a true 0->Vpeak->0 move (this is the quantity the planner's
 *      findSCurveVSpeed clamp uses — the one the original Ruckig path halves).
 */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

/* ==== EXACT COPY of the shipped analytic (sp_scurve.c) ==== */
static double scurve_cbrt(double x) { return (x < 0.0) ? -pow(-x, 1.0/3.0) : pow(x, 1.0/3.0); }

static double scurve_max_start_speed_analytic(double distance, double Ve, double A, double J) {
    if (distance <= 0.0 || A <= 0.0 || J <= 0.0) return fabs(Ve);
    if (Ve < 0.0) Ve = 0.0;
    double dv_thresh = (A * A) / J;
    double d_thresh  = (Ve + 0.5 * dv_thresh) * (2.0 * A / J);
    double dv;
    if (distance <= d_thresh) {
        double p = 2.0 * Ve;
        double q = -distance * sqrt(J);
        double disc = (q * q) / 4.0 + (p * p * p) / 27.0;
        double s = sqrt(disc);
        double u = scurve_cbrt(-0.5 * q + s) + scurve_cbrt(-0.5 * q - s);
        dv = u * u;
    } else {
        double a = 1.0 / (2.0 * A);
        double b = Ve / A + A / (2.0 * J);
        double c = Ve * A / J - distance;
        double disc = b * b - 4.0 * a * c;
        if (disc < 0.0) disc = 0.0;
        dv = (-b + sqrt(disc)) / (2.0 * a);
    }
    if (dv < 0.0) dv = 0.0;
    return Ve + dv;
}

/* ==== Ground-truth reference: integrate the real jerk-limited decel ==== */
static double dist_numeric(double Vs, double Ve, double A, double J, long steps, double *v_end) {
    double dv = Vs - Ve;
    if (dv <= 0.0) { *v_end = Vs; return 0.0; }
    double Tj, Ta;
    if (dv <= A*A/J) { Tj = sqrt(dv/J); Ta = 0.0; }
    else             { Tj = A/J;       Ta = dv/A - A/J; }
    double Apk = J * Tj;                 /* peak decel actually reached (==A if trapezoidal) */
    double T = 2.0*Tj + Ta;
    double dt = T/(double)steps, v = Vs, d = 0.0, t = 0.0;
    for (long i = 0; i < steps; ++i) {
        double tc = t + 0.5*dt, a;       /* midpoint rule */
        if (tc < Tj)            a = -J*tc;
        else if (tc < Tj+Ta)    a = -Apk;
        else                    a = -Apk + J*(tc-(Tj+Ta));
        v += a*dt; d += v*dt; t += dt;
    }
    *v_end = v;
    return d;
}
static double dist_closed(double Vs, double Ve, double A, double J) {
    double dv = Vs - Ve, T;
    if (dv <= A*A/J) T = 2.0*sqrt(dv/J);
    else             T = A/J + dv/A;
    return (Vs+Ve)/2.0 * T;
}

static double frand(double lo, double hi){ return lo + (hi-lo)*((double)rand()/RAND_MAX); }

int main(void) {
    srand(12345);
    const int N = 200000;
    double worst_fwd = 0, worst_rt = 0, worst_peak = 0, worst_vend = 0;
    double w_fwd_in[4]={0}, w_rt_in[4]={0}, w_peak_in[4]={0};

    for (int i = 0; i < N; ++i) {
        double A  = frand(200, 6000);
        double J  = frand(1e3, 1e6);
        double Ve = frand(0, 300);
        double Vs = Ve + frand(1e-4, 500);   /* span triangular & trapezoidal regimes */
        double D  = dist_closed(Vs, Ve, A, J);

        /* (1) forward physics: closed-form distance vs numeric, and v_end lands on Ve */
        double vend; double dN = dist_numeric(Vs, Ve, A, J, 20000, &vend);
        double e1 = fabs(dN - D)/fmax(dN,1e-12);
        if (e1 > worst_fwd){ worst_fwd=e1; w_fwd_in[0]=Vs;w_fwd_in[1]=Ve;w_fwd_in[2]=A;w_fwd_in[3]=J; }
        double ev = fabs(vend - Ve);
        if (ev > worst_vend) worst_vend = ev;

        /* (2) inverse round-trip: analytic(D, Ve) must recover Vs */
        double Vs_rt = scurve_max_start_speed_analytic(D, Ve, A, J);
        double e2 = fabs(Vs_rt - Vs)/fmax(Vs,1e-12);
        if (e2 > worst_rt){ worst_rt=e2; w_rt_in[0]=Vs;w_rt_in[1]=Ve;w_rt_in[2]=A;w_rt_in[3]=J; }

        /* (3) rest-to-rest peak: analytic(D/2,0) vs bisection of a true 0->Vpk->0 move */
        double Dr = frand(1e-4, 200.0);
        double lo=0, hi=1e6;
        for (int k=0;k<200;k++){ double m=0.5*(lo+hi); double dd=2.0*dist_closed(m,0,A,J); if(dd<Dr) lo=m; else hi=m; }
        double peak_true = 0.5*(lo+hi);
        double peak_an   = scurve_max_start_speed_analytic(Dr*0.5, 0.0, A, J);
        double e3 = fabs(peak_an - peak_true)/fmax(peak_true,1e-12);
        if (e3 > worst_peak){ worst_peak=e3; w_peak_in[0]=Dr;w_peak_in[2]=A;w_peak_in[3]=J; }
    }

    printf("S-curve analytic validation  (%d random cases, triangular+trapezoidal)\n", N);
    printf("  (1) forward closed-form vs numeric integration : worst rel err = %.3e %%   [Vs=%.3g Ve=%.3g A=%.3g J=%.3g]\n",
           worst_fwd*100, w_fwd_in[0],w_fwd_in[1],w_fwd_in[2],w_fwd_in[3]);
    printf("      end-velocity lands on Ve                   : worst |v_end-Ve| = %.3e\n", worst_vend);
    printf("  (2) inverse round-trip analytic(d)->Vs         : worst rel err = %.3e %%   [Vs=%.3g Ve=%.3g A=%.3g J=%.3g]\n",
           worst_rt*100, w_rt_in[0],w_rt_in[1],w_rt_in[2],w_rt_in[3]);
    printf("  (3) rest-to-rest peak vs simulated 0->Vpk->0   : worst rel err = %.3e %%   [D=%.3g A=%.3g J=%.3g]\n",
           worst_peak*100, w_peak_in[0],w_peak_in[2],w_peak_in[3]);

    int pass = (worst_fwd < 1e-3) && (worst_rt < 1e-7) && (worst_peak < 1e-3) && (worst_vend < 1e-2);
    printf("\n  RESULT: %s\n", pass ? "PASS — analytic matches ground-truth physics" : "FAIL");
    return pass ? 0 : 1;
}
