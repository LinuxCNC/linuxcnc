#ifndef SC_ARCS_H
#define SC_ARCS_H

//! Author  : SKynet Cyberdyne
//! Licence : MIT
//! Date    : 2023

#include "sc_struct.h"

#ifdef __cplusplus

#include <vector>
#include <iostream>
#include <Dense>
#include <Geometry>

using namespace Eigen;

class sc_arcs
{
public:
    sc_arcs();

    void sc_interpolate_arc(sc_pnt p0,
                            sc_pnt p1,     //! Waypoint.
                            sc_pnt p2,
                            T progress,    //! 0-1.
                            sc_pnt &pi);   //! Interpolated point.

    T sc_arc_lenght(sc_pnt p0,
                    sc_pnt p1,  //! Waypoint.
                    sc_pnt p2);

    V sc_arc_radius(sc_pnt p0,
                    sc_pnt p1,  //! Waypoint.
                    sc_pnt p2,
                    T &radius);

    //! If points are colinear, output is xy plane, type clockwise g2.
    V sc_arc_get_mid_waypoint(sc_pnt p0, //! Start.
                          sc_pnt p1, //! Center.
                          sc_pnt p2, //! End.
                          sc_pnt &pi);

private:
    sc_pnt sc_rotate_point_around_line(sc_pnt thePointToRotate,
                                       T theta,sc_pnt theLineP1,
                                       sc_pnt theLineP2);

    //! Keeping as is from original github code example.
    struct sc_arc {
        std::vector<sc_pnt> pntVec;
        float radius=0;
        float diameter=0;
        float arcLenght=0;
        float arcAngleRad=0; //! Radians
        float arcCircumFence=0;
        bool arcAngleNegative=0; //! When sign of the angle < 0, set true.
        sc_pnt center;
        sc_pnt pointOnArcAxis;
    };

    sc_arc sc_arc_points(Eigen::Vector3d p1,
                         Eigen::Vector3d p2,
                         Eigen::Vector3d p3,
                         T division);
};
#else
typedef struct sc_arcs sc_arcs;
#endif //! cplusplus

#endif

