
#ifndef SC_VECTOR_H
#define SC_VECTOR_H

//! Author  : SKynet Cyberdyne
//! Licence : MIT
//! Date    : 2023

#ifdef __cplusplus

#include <vector>
#include <iostream>
#include <cmath>
#include <Dense>
#include <Geometry>

#include "sc_struct.h"
#include "sc_optimizer.h"

class sc_vector
{
public:

    //! Constructor.
    sc_vector();

    V popfront();
    T traject_lenght();
    V interpolate_traject(T traject_progress, T traject_lenght, T &curve_progress, I &curve_nr);
    V optimize_gcode();
    std::vector<sc_block> pvec;
    std::vector<T> optimizedVec;

private:
};

//! Here it tells if this code is used in c, convert the class to a struct. This is handy!
#else
typedef struct sc_vector sc_vector;
#endif //! cplusplus

#endif




















