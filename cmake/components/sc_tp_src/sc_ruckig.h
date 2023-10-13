#ifndef SC_RUCKIG_H
#define SC_RUCKIG_H

#define to_override ((r.feed_overide/100)+1)
#define time_interval 0.01

#ifdef __cplusplus

//! LibRuckig scurve lib
//! https://github.com/pantor/ruckig, extra info: https://github.com/pantor/ruckig/issues/64
//! Examples
//! https://docs.ruckig.com/pages.html
#include "ruckig.hpp"

typedef double T;
typedef bool B;
typedef void V;

enum sc_ruckig_state {
    sc_ruckig_none,
    sc_ruckig_finished,
    sc_ruckig_run,
    sc_ruckig_stop,
    sc_ruckig_pause,
    sc_ruckig_pause_resume,
    sc_ruckig_wait
};

class sc_ruckig
{
public:
    sc_ruckig();

    V set_a_jm(T maxacc, T jerkmax);

    V set_interval(T value);

    V set_vm(T maxvel);

    V set_fo(T value);

    V set_af(T value);

    V set_target(T tarpos, T tarvel, T taracc);

    V set_all_and_run(T a, T jm, T interval, T vm, T fo, T af, T curpos, T curvel, T curacc,
                      T tarpos, T tarvel, T taracc);

    T get_a();

    T get_jm();

    T get_vm();

    V run(T tarpos, T tarvel, T taracc);

    V stop();

    V pause();

    V pause_resume();

    //! For each cycle, incpos is the increment value to be added to your current position.
    //! Add this function to a update thread.
    V update(T &incpos, T &mempos, T &newpos, T &newvel, T &newacc);

    sc_ruckig_state get_state();

    V set_wait_state();

    V set_mem_pos(T value);

    V run();

    V print_result(ruckig::Result r);

private:
    ruckig::InputParameter<1> myIn;
    ruckig::OutputParameter<1> myOut;

    std::array<double, 1> myVel, myAcc, myPos;

    // ruckig::Ruckig<1> myOtg {0.001};
    ruckig::Result myResult;

    T myDuration=0;
    T myAtTime=0;

    T myOldPos=0;
    T myMemPos=0;

    T myMaxVelocity=0;
    T myFeedOveride=1; //! Ratio 0 to 1. Ratio 0 to 1.2 etc.
    T myAdaptiveFeed=1; //! Ratio -1 to 1.

    T myInterval=0.001;

    sc_ruckig_state myState=sc_ruckig_none;

    V set_current(T curpos, T curvel, T curacc);
};

//! Here it tells if this code is used in c, convert the class to a struct. This is handy!
#else
typedef struct sc_ruckig sc_ruckig;
#endif //! cplusplus

#endif // SC_RUCKIG_H














