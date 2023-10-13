#ifndef SC_STRUCT_H
#define SC_STRUCT_H

//! Author  : SKynet Cyberdyne
//! Licence : MIT
//! Date    : 2023

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

//! Make conversion's easy:
#define to_radians  M_PI/180.0
#define to_degrees  (180.0/M_PI)
#define ns_to_ms    0.000001

typedef double T ;
typedef void V;
typedef unsigned int UI;
typedef int I;
typedef bool B;

struct sc_pnt {
    T x, y, z;
};

struct sc_dir {
    T a, b, c;
};

struct sc_ext {
    T u, v, w;
};

enum sc_primitive_id {
    sc_line,
    sc_arc,
};

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

//! Inherent numbers to canon motion type.
enum sc_type {
    sc_rapid=1,
    sc_linear=2,
    sc_circle=3,
    sc_G3
};
//! Used for c.
struct sc_block {

    enum sc_primitive_id primitive_id;
    enum sc_type type;

    int gcode_line_nr;

    struct sc_pnt pnt_s, pnt_e, pnt_w, pnt_c;
    struct sc_dir dir_s, dir_e;
    struct sc_ext ext_s, ext_e;

    //! The look ahead angle to next primitive,
    //! to calculate acceptable end velocity.
    T angle_end_deg;

    //! If arc's velmax is reduced by gforce impact value, this is maxvel.
    //! Otherwise the velmax is set to program velmax.
    T vo;
    T vm;
    T ve;

    //! Store the lenght for the scurve planner.
    T path_lenght;
};

T netto_difference_of_2_values(T a, T b);
T blocklenght(struct sc_block b);

enum sc_enum_program_status {
    program_run=0,
    program_pause=1,
    program_pause_resume=2,
    program_stop=3,
    program_vm_interupt=4,
    program_wait=5,
    program_end=6,
    program_error=7
};

enum sc_enum_run_status {
    run_check,
    run_init,
    run_cycle
};

enum sc_enum_pause_status {
    pause_init,
    pause_cycle
};

enum sc_enum_pause_resume_status {
    pause_resume_init,
    pause_resume_cycle
};

enum sc_enum_stop_status {
    stop_init,
    stop_cycle
};

enum sc_enum_vm_interupt_status {
    vm_interupt_init,
    vm_interupt_cycle
};


#endif

