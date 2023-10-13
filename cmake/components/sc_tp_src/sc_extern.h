#include "sc_lines.h"
#include "sc_arcs.h"

//! C++ source and header file compiled with this project.
//! Function call to c++.
extern V sc_arc_get_mid_waypoint_c(struct sc_pnt start,
                                   struct sc_pnt center,
                                   struct sc_pnt end,
                                   struct sc_pnt *waypoint);

//! Interpolation.
extern T arc_lenght_c(struct sc_pnt start, struct sc_pnt way, struct sc_pnt end);
extern T line_lenght_c(struct sc_pnt start, struct sc_pnt end);
extern V interpolate_line_c(struct sc_pnt p0, struct sc_pnt p1, T progress, struct sc_pnt *pi);
extern V interpolate_dir_c(struct sc_dir p0, struct sc_dir p1, T progress, struct sc_dir *pi);
extern V interpolate_ext_c(struct sc_ext p0, struct sc_ext p1, T progress, struct sc_ext *pi);
extern V interpolate_arc_c(struct sc_pnt p0, struct sc_pnt p1, struct sc_pnt p2, T progress, struct sc_pnt *pi);

extern T blocklenght_c(struct sc_block b);

#include "sc_ruckig.h"
extern sc_ruckig* ruckig_init_ptr();
extern V ruckig_set_all_and_run(sc_ruckig *ptr, T interval,
                                    T a, T jm,
                                    T curpos, T curvel, T curacc,
                                    T tarpos, T tarvel, T taracc,
                                    T vm, T fo, T af);
extern V ruckig_stop(sc_ruckig *ptr);
extern V ruckig_pause(sc_ruckig *ptr);
extern V ruckig_pause_resume(sc_ruckig *ptr);
extern V ruckig_set_mempos(sc_ruckig *ptr, T value);
extern V ruckig_update(sc_ruckig *ptr, T *mempos, T *newvel, T *newacc);
//! Status callback.
extern enum sc_ruckig_state ruckig_get_state(sc_ruckig *ptr);

extern B ruckig_state_run(sc_ruckig *ptr);
extern B ruckig_state_stop(sc_ruckig *ptr);
extern B ruckig_state_pause(sc_ruckig *ptr);
extern B ruckig_state_pause_resume(sc_ruckig *ptr);
extern B ruckig_state_finished(sc_ruckig *ptr);

extern T ruckig_get_a(sc_ruckig *ptr);
extern T ruckig_get_jm(sc_ruckig *ptr);
extern T ruckig_get_vm(sc_ruckig *ptr);

extern V ruckig_set_wait_state(sc_ruckig *ptr);
extern V ruckig_run(sc_ruckig *ptr);

#include "sc_vector.h"
extern sc_vector* vector_init_ptr();
extern V vector_pushback(sc_vector *ptr, struct sc_block b);
extern V vector_insert(sc_vector *ptr, struct sc_block b, int index);
extern V vector_popfront(sc_vector *ptr);
extern V vector_popback(sc_vector *ptr);
extern V vector_clear(sc_vector *ptr);
extern int vector_size(sc_vector *ptr);
extern struct sc_block vector_at(sc_vector *ptr, int index);
extern struct sc_block vector_front(sc_vector *ptr);
extern struct sc_block vector_back(sc_vector *ptr);
extern T vector_traject_lenght(sc_vector *ptr);
extern V vector_interpolate_traject(sc_vector *ptr, T traject_progress, T traject_lenght, T *curve_progress, I *curve_nr);


extern int vector_optimized_size(sc_vector *ptr);
extern T vector_optimized_at(sc_vector *ptr, int index);
extern V vector_optimize_gcode(sc_vector *ptr);











