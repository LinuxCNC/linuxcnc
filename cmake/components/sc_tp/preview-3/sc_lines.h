#ifndef SC_LINES_H
#define SC_LINES_H

//! Author  : SKynet Cyberdyne
//! Licence : MIT
//! Date    : 2023

#include "sc_struct.h"

class sc_lines
{
public:
    sc_lines();

    V sc_interpolate_lin(sc_pnt p0,
                         sc_pnt p1,
                         T progress, //! 0-1
                         sc_pnt &pi);

    V sc_interpolate_dir(sc_dir d0,
                         sc_dir d1,
                         T progress,
                         sc_dir &di);

    V sc_interpolate_ext(sc_ext e0,
                         sc_ext e1,
                         T progress,
                         sc_ext &ei);

    V sc_interpolate_lenght(T start,
                          T end,
                          T progress,
                          T &li);

    T sc_line_lenght(sc_pnt p0, sc_pnt p1);

    V sc_interpolate_lenght(sc_pnt start,
                          sc_pnt end,
                          T progress,
                          sc_pnt &pnt);
};

#endif
