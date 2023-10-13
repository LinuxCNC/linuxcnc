#ifndef SC_ENGINE_H
#define SC_ENGINE_H

//! Author  : SKynet Cyberdyne
//! Licence : MIT
//! Date    : 2023

#include <iostream>
#include <cmath>
#include <vector>
#include "sc_struct.h"

//! Make conversion's easy:
#define to_radians  M_PI/180.0
#define to_degrees  (180.0/M_PI)
#define ns_to_ms    0.000001

typedef double T;
typedef bool B;
typedef int I;
typedef uint UI;
typedef void V;

//! Scurve back end.
class sc_engine {
public:
    sc_engine(){};

    struct sc_motion {
        std::vector<sc_period> pvec;
    };

    enum sc_status {
        Error=0,
        Ok=1,
        Busy=2,
        Curve_finished=3,
        Traject_Finished=4
    };

    V sc_set_a_dv(T theA, T theDv);

    V interpolate_period(T at_time,
                            sc_period p,
                            T &pos,
                            T &vel,
                            T &acc);

    V interpolate_periods(T at_time, //! Uses the time of the motionvec.
                             std::vector<sc_period> pvec,
                             T &pos,
                             T &vel,
                             T &acc,
                             bool &finished);

    B process_curve( sc_period_id id,
                     T vo,
                     T ve,
                     T acs,
                     T ace,
                     T ncs,
                     T vm,
                     std::vector<sc_period> &pvec);

    B process_curve(sc_period p, T vm, std::vector<sc_period> &pvec);

    //! From traject progress to local curve progress. 0-1.
    V curve_progress(std::vector<sc_period> pvec,
                     T position,
                     T &curve_progress, T &curve_dtg, UI &curve_nr);

    T as=0;
    T a=0;
    T dv=0;
    T jm=0;
    T ct=0;

    V t5_t6_t7(T vo, T ve, T &s);
    V t1_t2_t3(T vo, T ve, T &s);

    inline I t1_t2_t3(sc_period p, std::vector<sc_period> &pvec);

    inline I t7_t1_t2_t3_t5(sc_period p, std::vector<sc_period> &pvec);

    inline I t5_t6_t7(sc_period p, std::vector<sc_period> &pvec);

    inline I t3_t5_t6_t7_t1(sc_period p, std::vector<sc_period> &pvec);
    //! Added for pid control, to get the minimal stop curve lenght.
    //! Inline can not be used. Input ace=0.
    I t3_t5_t6_t7_pid(sc_period p, std::vector<sc_period> &pvec);

    inline I t4_acs(sc_period p, std::vector<sc_period> &pvec);

    inline I t4_ace(sc_period p, std::vector<sc_period> &pvec);

    inline I t1(T vo, T acs, T ace, sc_period &p);
    //! Added for pid control, updates result by time interval. If p.nct=0, curve is finished.
    I t1_pid(T vo, T acs, T interval, sc_period &p);

    inline I t1_ve(T vo, T ve, T acs, sc_period &p);

    inline I t1_i(sc_period p, T ti, T &vi, T &si, T &ai);

    inline I t2(T vo, T ve, T a, sc_period &p);

    I t2_pid(T vo, T a, T interval, sc_period &p);

    inline I t2_i(sc_period p, T ti, T &vi, T &si, T &ai);

    inline I t3(T vo, T acs, T ace, sc_period &p);

    I t3_pid(T vo, T acs, T interval, sc_period &p);

    inline I t3_i(sc_period p, T ti, T &vi, T &si, T &ai);

    inline I t4(T vo, T s, sc_period &p);

    I t4_pid(T vo, T a, T interval, sc_period &p);

    inline I t4_i(sc_period p, T ti, T &vi, T &si, T &ai);

    inline I t5(T vo, T acs, T ace, sc_period &p);

    I t5_pid(T vo, T acs, T interval, sc_period &p);

    inline I t5_ve(T vo, T ve, T acs, sc_period &p);

    inline I t5_i(sc_period p, T ti, T &vi, T &si, T &ai);

    inline I t6(T vo, T ve, T a, sc_period &p);

    I t6_pid(T vo, T a, T interval, sc_period &p);

    inline I t6_i(sc_period p, T ti, T &vi, T &si, T &ai);

    inline I t7(T vo, T acs, T ace, sc_period &p);

    I t7_pid(T vo, T acs, T interval, sc_period &p);

    inline I t7_i(sc_period p, T ti, T &vi, T &si, T &ai);

    inline T to_vh_acc(T vo, T ve);

    inline T to_vh_dcc(T vo, T ve);

    T to_stot_pvec(std::vector<sc_period> pvec);

    T to_ttot_pvec(std::vector<sc_period> pvec);

    T netto_difference_of_2_values(T a, T b);

    B is_inbetween_2_values(T a, T b, T value);
};

#endif





























