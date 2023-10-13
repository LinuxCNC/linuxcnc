#ifndef SC_PLANNER_H
#define SC_PLANNER_H

#ifdef __cplusplus

//! Author  : SKynet Cyberdyne
//! Licence : MIT
//! Date    : 2023

#include <chrono>
#include "sc_engine.h"
#include "sc_optimizer.h"
//! return 0 = error
//! return 1 = ok
class sc_planner
{
public:
    sc_planner();

    sc_enum_program_status sc_enum_program_state=
            sc_enum_program_status::program_end;

    sc_enum_run_status sc_enum_run_state=
            sc_enum_run_status::run_check;

    sc_enum_pause_status sc_enum_pause_state=
            sc_enum_pause_status::pause_init;

    sc_enum_pause_resume_status sc_enum_pause_resume_state=
            sc_enum_pause_resume_status::pause_resume_init;

    sc_enum_stop_status sc_enum_stop_state=
            sc_enum_stop_status::stop_init;

    sc_enum_vm_interupt_status sc_enum_vm_interupt_state=
            sc_enum_vm_interupt_status::vm_interupt_init;

    V sc_set_a_dv(T acceleration, T delta_velocity);
    V sc_set_a_dv_gforce_vm(T acceleration, T delta_velocity, T gforce, T vm);
    V sc_print_a_dv();
    V sc_set_interval(T time);
    V sc_set_maxvel(T velocity_max);
    //! Input in [g].
    V sc_set_gforce(T gforce);
    V sc_set_adaptive_feed(T adaptive_feed);
    V sc_set_startline(UI startline);
    V sc_set_position(T position);
    V sc_set_state(sc_enum_program_status command);
    B sc_set_traject_stot();
    V sc_get_motionvec_nr_from_position(
            T &motionvec_nr,
            T &motionvec_nr_progress,
            T &motionvec_nr_dtg);

    B sc_check_motionvec_loaded();
    B sc_check_motionvec_startline();
    B sc_check_servo_cycletime();
    B sc_check_vm();

    V sc_get_program_state(sc_enum_program_status &state);
    V sc_print_program_state();
    std::string sc_get_program_state();
    B sc_get_run_cycle_state();
    B sc_get_pause_cycle_state();
    B sc_get_stop_cycle_state();

    V sc_reset();
    V sc_clear();

    V sc_add_line_motion(T vo,
                         T ve,
                         T acs,
                         T ace,
                         sc_pnt start,
                         sc_pnt end,
                         sc_type type, int gcode_line_nr); //! sc_G0, etc.

    V sc_add_arc_motion(T vo,
                        T ve,
                        T acs,
                        T ace,
                        sc_pnt start,
                        sc_pnt way,
                        sc_pnt end,
                        sc_type type, int gcode_line_nr);

    //! Add motion up to 9 axis.
    V sc_add_general_motion(T vo,
                            T ve,
                            T acs,
                            T ace,
                            sc_primitive_id id,
                            sc_type type,
                            sc_pnt start,
                            sc_pnt way,
                            sc_pnt end,
                            sc_dir dir_start,
                            sc_dir dir_end,
                            sc_ext ext_start,
                            sc_ext ext_end);

    //! Attached to thread.
    V sc_update();

    V sc_get_planner_results(T &position,
                             T &velocity,
                             T &acceleration,
                             UI &line_nr,
                             T &line_progress,
                             T &traject_progress,
                             B &finished);

    V sc_get_interpolation_results(
            sc_pnt &xyz,
            sc_dir &abc,
            sc_ext &uvw,
            T &curve_progress);

    //! Not ok.
    V sc_interpolate_c(T traject_progress,
                       sc_pnt &pnt,
                       sc_dir &dir,
                       sc_ext &ext,
                       T &curve_progress);

    V sc_interpolate_curve_c(UI block_nr, T curve_progress,
                             sc_pnt &pnt,
                             sc_dir &dir,
                             sc_ext &ext);

    UI sc_get_gcodeline_nr(UI blockvec_nr);

    //! State machine functions:
    V sc_run_state();
    //! Subs:
    B sc_run_flag=0; //! Check preconditions only once.
    V sc_run_check();
    V sc_run_init();
    V sc_run_cycle();
    V sc_run_finished();

    V sc_pause_state();
    //! Subs:
    V sc_pause_init();
    V sc_pause_cycle();

    V sc_pause_resume_state();
    //! Subs:
    V sc_pause_resume_init();
    V sc_pause_resume_cycle();

    V sc_stop_state();
    //! Subs:
    V sc_stop_init();
    V sc_stop_cycle();


    V sc_vm_interupt_state();
    //! Subs:
    V sc_vm_interupt_init();
    V sc_vm_interupt_cycle();

    V sc_program_end_state();
    V sc_error_state();

    T sc_performance();

    //! If you don't know the range, use 0 & INFINITY
    V sc_optimize(UI range_begin, UI range_end);

    V sc_print_blockvec();

    UI sc_blockvec_size();

private:

    sc_engine *engine = new sc_engine();
    sc_interpolate *interpolate= new sc_interpolate();
    sc_optimizer *optimizer=new sc_optimizer();
    std::vector<sc_block> blockvec; //! The gcode coordinates.
    std::vector<sc_period> motionvec; //! Derived lenghts from blockvec.
    std::vector<sc_period> pvec;

    sc_enum_program_status sc_last_program_state;

    UI sc_motionvec_nr=0;
    T sc_motionvec_nr_dtg=0;
    T sc_motionvec_nr_progress=0;

    T sc_ms=0;
    T sc_vm=0;
    T sc_gforce=0;
    T sc_adaptive_feed=1;

    T sc_stot=0;
    T sc_timer=0;
    T sc_interval=0;
    T sc_newpos=0;
    T sc_oldpos=0;
    T sc_pos=0;
    T sc_vel=0;
    T sc_acc=0;
};

//! Here it tells if this code is used in c, convert the class to a struct. This is handy!
#else
typedef struct sc_planner sc_planner;
#endif //! cplusplus

#endif //! sc_planner



























