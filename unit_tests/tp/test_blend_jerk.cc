/**
 * test_blend_jerk.cc — Per-axis jerk from bezier blend curvature
 *
 * Self-contained: quintic Bezier + arc-length table, no external deps.
 * Simulates straight→blend→straight at constant velocity, computes
 * per-axis jerk via backward-difference (matching HAL ddt components).
 *
 * Build & run:
 *   g++ -std=c++20 -O2 test_blend_jerk.cc -lm -o test_blend_jerk && ./test_blend_jerk
 */

#include <cstdio>
#include <cmath>
#include <cstring>
#include <algorithm>

struct V3 { double x,y,z; };
static V3 operator+(V3 a, V3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
static V3 operator-(V3 a, V3 b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
static V3 operator*(double s, V3 a) { return {s*a.x, s*a.y, s*a.z}; }
static double mag(V3 a) { return sqrt(a.x*a.x + a.y*a.y + a.z*a.z); }

static V3 bezier5_eval(V3 const P[6], double t) {
    double s=1-t, s2=s*s, s3=s2*s, s4=s3*s, s5=s4*s;
    double t2=t*t, t3=t2*t, t4=t3*t, t5=t4*t;
    return { s5*P[0].x + 5*s4*t*P[1].x + 10*s3*t2*P[2].x + 10*s2*t3*P[3].x + 5*s*t4*P[4].x + t5*P[5].x,
             s5*P[0].y + 5*s4*t*P[1].y + 10*s3*t2*P[2].y + 10*s2*t3*P[3].y + 5*s*t4*P[4].y + t5*P[5].y,
             s5*P[0].z + 5*s4*t*P[1].z + 10*s3*t2*P[2].z + 10*s2*t3*P[3].z + 5*s*t4*P[4].z + t5*P[5].z };
}

static V3 bezier5_deriv(V3 const P[6], double t) {
    double s=1-t, s2=s*s, s3=s2*s, s4=s3*s;
    double t2=t*t, t3=t2*t, t4=t3*t;
    V3 D[5];
    for (int i=0;i<5;i++) D[i] = P[i+1] - P[i];
    return { 5*s4*D[0].x + 20*s3*t*D[1].x + 30*s2*t2*D[2].x + 20*s*t3*D[3].x + 5*t4*D[4].x,
             5*s4*D[0].y + 20*s3*t*D[1].y + 30*s2*t2*D[2].y + 20*s*t3*D[3].y + 5*t4*D[4].y,
             5*s4*D[0].z + 20*s3*t*D[1].z + 30*s2*t2*D[2].z + 20*s*t3*D[3].z + 5*t4*D[4].z };
}

static constexpr int ARC_N = 4096;

struct BlendCurve {
    V3 P[6];
    double s_table[ARC_N+1], t_table[ARC_N+1];
    double total_length, max_kappa, max_dkds;
};

static void build_arc_table(BlendCurve &c) {
    c.s_table[0] = 0; c.t_table[0] = 0;
    for (int i = 1; i <= ARC_N; i++) {
        double t = (double)i / ARC_N;
        c.t_table[i] = t;
        double t0 = c.t_table[i-1], tm = 0.5*(t0+t);
        double ds = (t-t0)/6.0 * (mag(bezier5_deriv(c.P,t0)) + 4*mag(bezier5_deriv(c.P,tm)) + mag(bezier5_deriv(c.P,t)));
        c.s_table[i] = c.s_table[i-1] + ds;
    }
    c.total_length = c.s_table[ARC_N];
}

static double arc_to_t(BlendCurve const &c, double s) {
    if (s <= 0) return 0;
    if (s >= c.total_length) return 1;
    int lo=0, hi=ARC_N;
    while (hi-lo > 1) { int mid=(lo+hi)/2; if (c.s_table[mid]<s) lo=mid; else hi=mid; }
    double frac = (s - c.s_table[lo]) / (c.s_table[hi] - c.s_table[lo]);
    return c.t_table[lo] + frac * (c.t_table[hi] - c.t_table[lo]);
}

static double curvature_at(V3 const P[6], double t) {
    V3 d1 = bezier5_deriv(P, t);
    double h = 1e-7;
    V3 d1p = bezier5_deriv(P, std::min(t+h, 1.0));
    V3 d1m = bezier5_deriv(P, std::max(t-h, 0.0));
    V3 d2 = (1.0/(2*h)) * (d1p - d1m);
    if (t < h || t > 1.0-h) d2 = (1.0/h) * (d1p - d1);  /* one-sided at endpoints */
    double cross = d1.x * d2.z - d1.z * d2.x;
    double m = mag(d1);
    return (m > 1e-12) ? fabs(cross) / (m*m*m) : 0;
}

static void compute_curvature_stats(BlendCurve &c) {
    c.max_kappa = 0; c.max_dkds = 0;
    double prev_k = curvature_at(c.P, 0);
    for (int i = 1; i <= ARC_N; i++) {
        double k = curvature_at(c.P, c.t_table[i]);
        if (k > c.max_kappa) c.max_kappa = k;
        double ds = c.s_table[i] - c.s_table[i-1];
        if (ds > 1e-12) { double dk = fabs(k - prev_k)/ds; if (dk > c.max_dkds) c.max_dkds = dk; }
        prev_k = k;
    }
}

static double acc_limit_vel(BlendCurve const &c, double a_max, double j_max) {
    double a_normal = sqrt(0.75) * a_max;
    double min_r = (c.max_kappa > 1e-8) ? 1.0/c.max_kappa : 1e12;
    double v = sqrt(a_normal * min_r);
    if (j_max > 0 && c.max_dkds > 1e-8) {
        double j_normal = sqrt(0.75) * j_max;
        double vj = cbrt(j_normal / c.max_dkds);
        if (vj < v) v = vj;
    }
    return v;
}

struct Seg { double x0,z0,x1,z1,ux,uz,len; };
static Seg make_seg(double x0,double z0,double x1,double z1) {
    double dx=x1-x0,dz=z1-z0,l=sqrt(dx*dx+dz*dz);
    return {x0,z0,x1,z1,dx/l,dz/l,l};
}

/* Sample position along straight→blend→straight path.
 * s: arc-length from start of straight_a.
 * straight_a extends 2mm before blend, straight_b extends 2mm after. */
struct Path {
    V3 a_start;    /* start of approach straight */
    V3 a_dir;      /* direction of approach */
    double a_len;  /* approach length (before blend) */
    BlendCurve blend;
    V3 b_dir;      /* direction of departure */
    double b_len;  /* departure length (after blend) */
    double total;
};

static V3 path_sample(Path const &p, double s) {
    if (s <= p.a_len) {
        /* On approach straight */
        return p.a_start + s * p.a_dir;
    }
    s -= p.a_len;
    if (s <= p.blend.total_length) {
        /* On blend */
        double t = arc_to_t(p.blend, s);
        return bezier5_eval(p.blend.P, t);
    }
    s -= p.blend.total_length;
    /* On departure straight */
    V3 blend_end = bezier5_eval(p.blend.P, 1.0);
    return blend_end + s * p.b_dir;
}

static void test_blend(const char *label, Seg &seg_a, Seg &seg_b,
                       double trim_frac, double velocity,
                       double j_limit, double a_limit)
{
    double trim = std::min(seg_a.len, seg_b.len) * trim_frac;
    if (trim < 1e-6) return;
    double alpha = trim / 2.0;

    BlendCurve blend;
    V3 start = {seg_a.x1 - trim*seg_a.ux, 0, seg_a.z1 - trim*seg_a.uz};
    V3 end   = {seg_b.x0 + trim*seg_b.ux, 0, seg_b.z0 + trim*seg_b.uz};
    V3 us = {seg_a.ux, 0, seg_a.uz}, ue = {seg_b.ux, 0, seg_b.uz};

    blend.P[0] = start;
    blend.P[1] = start + alpha * us;
    blend.P[2] = start + (2*alpha) * us;
    blend.P[3] = end - (2*alpha) * ue;
    blend.P[4] = end - alpha * ue;
    blend.P[5] = end;

    build_arc_table(blend);
    compute_curvature_stats(blend);
    double v_acc = acc_limit_vel(blend, a_limit, j_limit);
    double kink = acos(std::clamp(seg_a.ux*seg_b.ux + seg_a.uz*seg_b.uz, -1.0, 1.0));

    /* Build full path: 2mm straight → blend → 2mm straight */
    double approach = 2.0, depart = 2.0;
    Path path;
    path.a_start = start - approach * us;
    path.a_dir = us;
    path.a_len = approach;
    path.blend = blend;
    path.b_dir = ue;
    path.b_len = depart;
    path.total = approach + blend.total_length + depart;

    printf("─── %s  trim=%.3fmm  kink=%.1f° ───\n", label, trim, kink*180/M_PI);
    printf("  blend_len=%.4f  max_κ=%.3f  min_R=%.4f  max_dκ/ds=%.3f\n",
           blend.total_length, blend.max_kappa, 1.0/std::max(blend.max_kappa,1e-12), blend.max_dkds);
    printf("  v_acc_limit=%.2f\n", v_acc);

    double test_vels[] = { std::min(velocity, v_acc), velocity };
    const char *vlbl[] = { "capped", "raw" };
    int ntests = (velocity > v_acc + 0.1) ? 2 : 1;

    for (int vt = 0; vt < ntests; vt++) {
        double v = test_vels[vt];
        double dt = 0.001;
        int n = (int)(path.total / v / dt) + 5;
        if (n < 20) n = 20;
        if (n > 100000) n = 100000;

        double *xp = new double[n], *zp = new double[n];
        for (int i = 0; i < n; i++) {
            double s = v * i * dt;
            if (s > path.total) s = path.total;
            V3 pt = path_sample(path, s);
            xp[i] = pt.x; zp[i] = pt.z;
        }

        double max_jx=0, max_jz=0, max_ax=0, max_az=0;
        int mjx_i=0, mjz_i=0;
        for (int i = 3; i < n-1; i++) {  /* skip last sample (clamped) */
            double s = v * i * dt;
            if (s > path.total - v*dt*2) break;  /* stop before path end */
            double ax = (xp[i] - 2*xp[i-1] + xp[i-2]) / (dt*dt);
            double az = (zp[i] - 2*zp[i-1] + zp[i-2]) / (dt*dt);
            double jx = (xp[i] - 3*xp[i-1] + 3*xp[i-2] - xp[i-3]) / (dt*dt*dt);
            double jz = (zp[i] - 3*zp[i-1] + 3*zp[i-2] - zp[i-3]) / (dt*dt*dt);
            if (fabs(ax) > fabs(max_ax)) max_ax = ax;
            if (fabs(az) > fabs(max_az)) max_az = az;
            if (fabs(jx) > fabs(max_jx)) { max_jx = jx; mjx_i = i; }
            if (fabs(jz) > fabs(max_jz)) { max_jz = jz; mjz_i = i; }
        }

        /* Theoretical centripetal jerk at peak curvature (torsion term: v³κ²) */
        double v3k2 = v*v*v * blend.max_kappa * blend.max_kappa;

        printf("  [%s v=%.2f] max_acc: ax=%.1f az=%.1f  max_jerk: jx=%.0f(%d) jz=%.0f(%d)  "
               "jx/lim=%.2f jz/lim=%.2f  v³κ²=%.0f",
               vlbl[vt], v, max_ax, max_az, max_jx, mjx_i, max_jz, mjz_i,
               fabs(max_jx)/j_limit, fabs(max_jz)/j_limit, v3k2);
        if (fabs(max_jx) > j_limit || fabs(max_jz) > j_limit)
            printf("  *** OVER ***");
        printf("\n");

        delete[] xp; delete[] zp;
    }
    printf("\n");
}

int main()
{
    double j_limit = 50000.0, a_limit = 1000.0;

    Seg seg106 = make_seg(40.897,-15.094, 40.498,-16.063);
    Seg seg107 = make_seg(40.498,-16.063, 40.365,-16.581);
    Seg seg108 = make_seg(40.365,-16.581, 40.231,-18.000);
    Seg seg109 = make_seg(40.231,-18.000, 0.001,-18.000);

    printf("=== Blend Jerk Analysis (constant velocity, straight→blend→straight) ===\n");
    printf("Limits: j=%.0f a=%.0f  Seg: 106=%.3f 107=%.3f 108=%.3f 109=%.3f\n\n",
           j_limit, a_limit, seg106.len, seg107.len, seg108.len, seg109.len);

    printf("==== 107→108 (9° kink, near halscope spike location) ====\n");
    for (double tf : {0.15, 0.2, 0.3, 0.5})
        test_blend("107→108", seg107, seg108, tf, 26.0, j_limit, a_limit);

    printf("==== 108→109 (84.6° kink) ====\n");
    for (double tf : {0.05, 0.1, 0.15})
        test_blend("108→109", seg108, seg109, tf, 26.0, j_limit, a_limit);

    printf("==== 106→107 (8° kink) ====\n");
    for (double tf : {0.2, 0.3, 0.5})
        test_blend("106→107", seg106, seg107, tf, 26.0, j_limit, a_limit);

    return 0;
}
