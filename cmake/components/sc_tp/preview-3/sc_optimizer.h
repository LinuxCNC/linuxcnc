#ifndef SC_OPTIMIZER_H
#define SC_OPTIMIZER_H

//! Author  : SKynet Cyberdyne
//! Licence : MIT
//! Date    : 2023

#include <vector>
#include <iostream>
#include <cmath>
#include <Dense>
#include <Geometry>

#include "sc_struct.h"
#include "sc_arcs.h"
#include "sc_interpolate.h"
#include "sc_engine.h"
#include "sc_block.h"

//! Make conversion's easy:
#define to_radians  M_PI/180.0
#define to_degrees  (180.0/M_PI)

typedef void V ;
typedef bool B ;
typedef double T ;
typedef uint UI;

class sc_optimizer
{
public:

    //! Constructor.
    sc_optimizer();

    //! Set parameters.
    V sc_set_a_dv_gforce_velmax(T acceleration, T delta_v, T gforce_max, T velocity_max);

    //! The following functions are path rules. You can use them in sequence, or use a specific one.
    //! Eventually you could add more path rules to the sequence.

    //! Calculates block angles, then set's a corner ve.
    std::vector<sc_block> sc_optimize_block_angles_ve(std::vector<sc_block> blockvec);

    //! Calculates arc's vm trough a max defined gforce impact value in [g].
    std::vector<sc_block> sc_optimize_gforce_arcs(std::vector<sc_block> blockvec);

    //! Sets vo=0, ve=0 when motion is of type: G0
    std::vector<sc_block> sc_optimize_G0_ve(std::vector<sc_block> blockvec);

    //! Sets vo. ve when motions are of type: G0, G1, G2.
    std::vector<sc_block> sc_optimize_G123_ve_backward(std::vector<sc_block> blockvec);
    std::vector<sc_block> sc_optimize_G123_ve_forward(std::vector<sc_block> blockvec);

    //! Function that uses all above path rules.
    std::vector<sc_block> sc_optimize_all(std::vector<sc_block> blockvec);

    sc_block convert_cpp_to_c(sc_block in);
    sc_block convert_c_to_cpp(sc_block in);

    sc_block* sc_optimize_all(sc_block *blockvec,
                                int size,
                                T acceleration,
                                T delta_v,
                                T gforce_max,
                                T velocity_max);
    //! Print.
    V sc_print_blockvec(std::vector<sc_block> blockvec);

    //! Calculates rotational gforce impact in [g].
    //! We use this to set maxvel for arc's.
    V sc_get_gforce(T vel_mm_sec, T radius, T &gforce);
    V sc_set_gforce(T radius, T gforce, T &vel_mm_sec);

private:
    sc_engine *engine=new sc_engine();
    T a=0, dv=0, gforcemax=0, vm=0;

    //! Iterate over the gcode to get the corners.
    std::vector<sc_block> sc_get_blockangles(
            std::vector<sc_block> blockvec);

    //! Calculate end velocity's based on the given angles to next primitive.
    std::vector<sc_block> sc_get_corner_ve_blockangles(
            std::vector<sc_block> blockvec,
            T velmax);

    //! Set maxvel for arcs, depending on gforce.
    std::vector<sc_block> sc_get_velmax_gforce(std::vector<sc_block> blockvec,
                                               T velmax,
                                               T gforcemax);

    //! p1 = common point.
    V line_line_angle(sc_pnt p0,
                      sc_pnt p1,
                      sc_pnt p2,
                      T &angle_deg);

    //! p0 = line start.
    //! p1 = arc startpoint, common point.
    //! p2 = arc waypoint.
    //! p3 = arc endpoint.
    V line_arc_angle(sc_pnt p0,
                     sc_pnt p1,
                     sc_pnt p2,
                     sc_pnt p3,
                     T &angle_deg);

    //! p0 = arc start.
    //! p1 = arc waypoint.
    //! p2 = arc endpoint, common point.
    //! p3 = line endpoint.
    V arc_line_angle(sc_pnt p0,
                     sc_pnt p1,
                     sc_pnt p2,
                     sc_pnt p3,
                     T &angle_deg);

    //! p0 = arc start.
    //! p1 = arc waypoint.
    //! p2 = arc endpoint, common point.
    //! p3 = arc waypoint.
    //! p4 = arc endpoint.
    V arc_arc_angle(sc_pnt p0,
                    sc_pnt p1,
                    sc_pnt p2,
                    sc_pnt p3,
                    sc_pnt p4,
                    T &angle_deg);
};

#endif




















