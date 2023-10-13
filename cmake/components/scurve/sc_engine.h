#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

//! Author  : SKynet Cyberdyne
//! Licence : MIT
//! Date    : 2023

/*  Usage example 0:

    set_a_dv(2,10);

    struct sc_period p={0};
    p.vo=0;
    p.ve=50;
    p.acs=0;
    p.ace=0;

    struct sc_period *pvec;
    size_t size;
    t1_t2_t3_pvec(p, &pvec, &size);

    rtapi_print_msg(RTAPI_MSG_ERR,"i: %f \n",pvec[0].ncs);
    rtapi_print_msg(RTAPI_MSG_ERR,"i: %f \n",pvec[1].ncs);
    rtapi_print_msg(RTAPI_MSG_ERR,"i: %f \n",pvec[2].ncs);

    free(pvec);
*/

/*  Usage example 1:

    set_a_dv(2,10);

    struct sc_period p={0};
    p.id=id_run;
    p.vo=0;
    T vm=10;
    p.ve=0;
    p.acs=0;
    p.ace=0;
    p.ncs=100;

    struct sc_period *pvec;
    size_t size;

    process_curve(p,vm,&pvec,&size); //! This is the most difficult function call.

    rtapi_print_msg(RTAPI_MSG_ERR,"runs, size of process curve periods: %zu \n",size);
    rtapi_print_msg(RTAPI_MSG_ERR,"curve ttot: %f \n",to_ttot_pvec(pvec,size));
    rtapi_print_msg(RTAPI_MSG_ERR,"curve stot: %f \n",to_stot_pvec(pvec,size));

    free(pvec);
*/

//! Make conversion's easy:
#define to_radians  M_PI/180.0
#define to_degrees  (180.0/M_PI)
#define ns_to_ms    0.000001

typedef double T;
typedef int I;
typedef uint UI;
typedef void V;

//! Enum to define curve periods.
enum sc_period_id {
    id_t1,
    id_t2,
    id_t3,
    id_t4,
    id_t5,
    id_t6,
    id_t7,
    id_pvec,
    id_run,
    id_pause,
    id_pause_resume,
    id_none
};

//! A period with it's values.
struct sc_period {
    //! Period type, t1,t2,t3, etc,
    enum sc_period_id id;
    //! Velocity start.
    T vo;
    //! Velocity end.
    T ve;
    //! Acceleration start.
    T acs;
    //! Acceleration end.
    T ace;
    //! Netto curve displacement.
    T ncs;
    //! Netto curve time.
    T nct;
};

struct sc_pnt {
    T x, y, z;
};

struct sc_direction {
    T a, b, c;
};

struct sc_ext {
    T u, v, w;
};

enum sc_primitive_id {
    sc_line,
    sc_arc,
};

enum sc_type {
    sc_G0,
    sc_G1,
    sc_G2,
    sc_G3
};

struct sc_vsa {
    T v, s, a;
};

//! Functions with "pid" are updated every servo cycle and
//! give new position based on given end point.

//! Set acceleration, delta-velocity.
//!
//!     Delta velocity represents the curve power.
//!
//!     The dv represents a velocity from
//!     acc=0 to max acc "as" back to acc=0;
//!
//!     Using a dv=10 is a curve from v=0 to v=10.
//!     When given a velocity from 0 to 15, a
//!     linear period of 5 is in the middle.
//!
I set_a_dv(T theA, T theDv);
//! Check if values are set.
I check_a_as_jm_dv();

//! Basic scurve functions

//! Period t1.
I t1(T vo, T acs, T ace, struct sc_period *p);
I t1_i(struct sc_period p, T ti, struct sc_vsa *vsa);
I t1_ve(T vo, T ve, T acs, struct sc_period *p);
I t1_pid(T vo, T acs, T interval, struct sc_period *p);
//! Period t2.
I t2(T vo, T ve, T a, struct sc_period *p);
I t2_i(struct sc_period p, T ti, struct sc_vsa *vsa);
I t2_pid(T vo, T a, T interval, struct sc_period *p);
//! Period t3.
I t3(T vo, T acs, T ace, struct sc_period *p);
I t3_i(struct sc_period p, T ti, struct sc_vsa *vsa);
I t3_pid(T vo, T acs, T interval, struct sc_period *p);
//! Period t4.
I t4(T vo, T s, struct sc_period *p);
I t4_i(struct sc_period p, T ti, struct sc_vsa *vsa);
I t4_pid(T vo, T a, T interval, struct sc_period *p);
//! A straight line with a start curve using acs.
I t4_acs(struct sc_period p, struct sc_period **pvec, size_t *size);
//! A straight line with a end curve using ace.
I t4_ace(struct sc_period p, struct sc_period **pvec, size_t *size);
//! Period t5.
I t5(T vo, T acs, T ace, struct sc_period *p);
I t5_i(struct sc_period p, T ti, struct sc_vsa *vsa);
I t5_ve(T vo, T ve, T acs, struct sc_period *p);
I t5_pid(T vo, T acs, T interval, struct sc_period *p);
//! Period t6.
I t6(T vo, T ve, T a, struct sc_period *p);
I t6_i(struct sc_period p, T ti, struct sc_vsa *vsa);
I t6_pid(T vo, T a, T interval, struct sc_period *p);
//! Period t7.
I t7(T vo, T acs, T ace, struct sc_period *p);
I t7_i(struct sc_period p, T ti, struct sc_vsa *vsa);
I t7_pid(T vo, T acs, T interval, struct sc_period *p);

//! Calculate curve displacement for periods:
V t1_t2_t3_s(T vo, T ve, T *s);
V t5_t6_t7_s(T vo, T ve, T *s);

//! Calculate scurves, this is used by process_curve().
I t1_t2_t3_pvec(struct sc_period p, struct sc_period **pvec, size_t *size);
I t7_t1_t2_t3_t5_pvec(struct sc_period p, struct sc_period **pvec, size_t *size);
I t5_t6_t7_pvec(struct sc_period p, struct sc_period **pvec, size_t *size);
I t3_t5_t6_t7_t1_pvec(struct sc_period p, struct sc_period **pvec, size_t *size);

//! Added for pid control, to get the minimal stop curve lenght.
I t3_t5_t6_t7_pid(struct sc_period p, struct sc_period **pvec, size_t *size);

//! Interpolate one scurve. Vsa is in order: velocity, displacement, acceleration.
I interpolate_period(T at_time,
                     struct sc_period p,
                     struct sc_vsa *vsa);

//! Interpolate a traject of multiple scurves.
//! Returns 1 if succes. Returns 0 if at_time>curve_time.
I interpolate_periods(T at_time,
                      struct sc_period pvec[],
                      size_t size,
                      struct sc_vsa *vsa, I *finished);

//! From traject progress to local curve progress. 0-1.
//! Returns 1 if position fits into the scurve, else returns 0.
I curve_progress(struct sc_period pvec[],
                 size_t size,
                 T position,
                 T *curve_progress,
                 T *curve_dtg,
                 UI *curve_nr);

//! Process a full scurve using simple input.
I process_curve_simple(enum sc_period_id id,
                 T vo,
                 T ve,
                 T acs,
                 T ace,
                 T ncs,
                 T vm,
                 struct sc_period **pvec, size_t size);

//! Process a full scurve given period values and vm.
I process_curve(struct sc_period p,
                T vm,
                struct sc_period **pvec,
                size_t *size);

//! Helper functions:

//! Returns 0 on success, -1 on failure.
int append_to_pvec(struct sc_period **pvec, size_t *size, struct sc_period* vec_1, size_t size_1);
//! Total traject time.
T to_ttot_pvec(struct sc_period pvec[], size_t size);
//! Total displacement traject.
T to_stot_pvec(struct sc_period pvec[], size_t size);
//! Calculates netto distance between positive and negative values.
T netto_difference_of_2_values(T a, T b);
//! Does what you think.
I is_inbetween_2_values(T a, T b, T value);
//! Get the middle velocity as result.
T to_vh_acc(T vo, T ve);
T to_vh_dcc(T vo, T ve);

V cleanup_periods(struct sc_period **pvec, size_t size);






