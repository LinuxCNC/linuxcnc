#pragma once

#include <array>
#include <cmath>
#include <iostream>

#include <ruckig/utils.hpp>


namespace ruckig {

//! Calculates (pre- or post-) profile to get current or final state below the limits
class BrakeProfile {
    static constexpr double eps {2.2e-14};

    void acceleration_brake(double v0, double a0, double vMax, double vMin, double aMax, double aMin, double jMax);
    void velocity_brake(double v0, double a0, double vMax, double vMin, double aMax, double aMin, double jMax);

public:
    //! Overall duration
    double duration {0.0};

    //! Profile information for a two-step profile
    std::array<double, 2> t, j, a, v, p;

    //! Calculate brake trajectory for third-order position interface
    void get_position_brake_trajectory(double v0, double a0, double vMax, double vMin, double aMax, double aMin, double jMax);

    //! Calculate brake trajectory for second-order position interface
    void get_second_order_position_brake_trajectory(double v0, double vMax, double vMin, double aMax, double aMin);

    //! Calculate brake trajectory for third-order velocity interface
    void get_velocity_brake_trajectory(double a0, double aMax, double aMin, double jMax);

    //! Calculate brake trajectory for second-order velocity interface
    void get_second_order_velocity_brake_trajectory();

    //! Finalize third-order braking by integrating along kinematic state
    void finalize(double& ps, double& vs, double& as) {
        if (t[0] <= 0.0 && t[1] <= 0.0) {
            duration = 0.0;
            return;
        }

        duration = t[0];
        p[0] = ps;
        v[0] = vs;
        a[0] = as;
        std::tie(ps, vs, as) = integrate(t[0], ps, vs, as, j[0]);

        if (t[1] > 0.0) {
            duration += t[1];
            p[1] = ps;
            v[1] = vs;
            a[1] = as;
            std::tie(ps, vs, as) = integrate(t[1], ps, vs, as, j[1]);
        }
    }

    //! Finalize second-order braking by integrating along kinematic state
    void finalize_second_order(double& ps, double& vs, double& as) {
        if (t[0] <= 0.0) {
            duration = 0.0;
            return;
        }

        duration = t[0];
        p[0] = ps;
        v[0] = vs;
        std::tie(ps, vs, as) = integrate(t[0], ps, vs, a[0], 0.0);
    }
};

} // namespace ruckig
