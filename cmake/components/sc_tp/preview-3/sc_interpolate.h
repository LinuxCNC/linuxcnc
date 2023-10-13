#ifndef SC_INTERPOLATE_H
#define SC_INTERPOLATE_H

//! Author  : SKynet Cyberdyne
//! Licence : MIT
//! Date    : 2023

#include "sc_struct.h"
#include "sc_block.h"
#include "sc_lines.h"
#include "sc_arcs.h"

class sc_interpolate
{
public:
    sc_interpolate();

    V interpolate_blockvec(std::vector<sc_block> blockvec,
                           T traject_progress, //! 0-1
                           sc_pnt &pnt,
                           sc_dir &dir,
                           sc_ext &ext,
                           T &curve_progress);
};

#endif
