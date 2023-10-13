#include "brake.hpp"

namespace ruckig {

inline double v_at_t(double v0, double a0, double j, double t) {
    return v0 + t * (a0 + j * t / 2);
}

inline double v_at_a_zero(double v0, double a0, double j) {
    return v0 + (a0 * a0)/(2 * j);
}

void BrakeProfile::acceleration_brake(double v0, double a0, double vMax, double vMin, double aMax, double aMin, double jMax) {
    j[0] = -jMax;

    const double t_to_a_max = (a0 - aMax) / jMax;
    const double t_to_a_zero = a0 / jMax;

    const double v_at_a_max = v_at_t(v0, a0, -jMax, t_to_a_max);
    const double v_at_a_zero = v_at_t(v0, a0, -jMax, t_to_a_zero);

    if ((v_at_a_zero > vMax && jMax > 0) || (v_at_a_zero < vMax && jMax < 0)) {
        velocity_brake(v0, a0, vMax, vMin, aMax, aMin, jMax);
    
    } else if ((v_at_a_max < vMin && jMax > 0) || (v_at_a_max > vMin && jMax < 0)) {
        const double t_to_v_min = -(v_at_a_max - vMin)/aMax;
        const double t_to_v_max = -aMax/(2*jMax) - (v_at_a_max - vMax)/aMax;

        t[0] = t_to_a_max + eps;
        t[1] = std::max(std::min(t_to_v_min, t_to_v_max - eps), 0.0);

    } else {
        t[0] = t_to_a_max + eps;
    }
}

void BrakeProfile::velocity_brake(double v0, double a0, double vMax, double vMin, double, double aMin, double jMax) {
    j[0] = -jMax;
    const double t_to_a_min = (a0 - aMin)/jMax;
    const double t_to_v_max = a0/jMax + std::sqrt(a0*a0 + 2 * jMax * (v0 - vMax)) / std::abs(jMax);
    const double t_to_v_min = a0/jMax + std::sqrt(a0*a0 / 2 + jMax * (v0 - vMin)) / std::abs(jMax);
    const double t_min_to_v_max = std::min(t_to_v_max, t_to_v_min);

    if (t_to_a_min < t_min_to_v_max) {
        const double v_at_a_min = v_at_t(v0, a0, -jMax, t_to_a_min);
        const double t_to_v_max_with_constant = -(v_at_a_min - vMax)/aMin;
        const double t_to_v_min_with_constant = aMin/(2*jMax) - (v_at_a_min - vMin)/aMin;

        t[0] = t_to_a_min - eps;
        t[1] = std::max(std::min(t_to_v_max_with_constant, t_to_v_min_with_constant), 0.0);

    } else {
        t[0] = t_min_to_v_max - eps;
    }
}

void BrakeProfile::get_position_brake_trajectory(double v0, double a0, double vMax, double vMin, double aMax, double aMin, double jMax) {
    t[0] = 0.0;
    t[1] = 0.0;
    j[0] = 0.0;
    j[1] = 0.0;

    if (a0 > aMax) {
        acceleration_brake(v0, a0, vMax, vMin, aMax, aMin, jMax);

    } else if (a0 < aMin) {
        acceleration_brake(v0, a0, vMin, vMax, aMin, aMax, -jMax);

    } else if ((v0 > vMax && v_at_a_zero(v0, a0, -jMax) > vMin) || (a0 > 0 && v_at_a_zero(v0, a0, jMax) > vMax)) {
        velocity_brake(v0, a0, vMax, vMin, aMax, aMin, jMax);

    } else if ((v0 < vMin && v_at_a_zero(v0, a0, jMax) < vMax) || (a0 < 0 && v_at_a_zero(v0, a0, -jMax) < vMin)) {
        velocity_brake(v0, a0, vMin, vMax, aMin, aMax, -jMax);
    }
}

void BrakeProfile::get_velocity_brake_trajectory(double a0, double aMax, double aMin, double jMax) {
    t[0] = 0.0;
    t[1] = 0.0;
    j[0] = 0.0;
    j[1] = 0.0;

    if (a0 > aMax) {
        j[0] = -jMax;
        t[0] = (a0 - aMax)/jMax + eps;

    } else if (a0 < aMin) {
        j[0] = jMax;
        t[0] = -(a0 - aMin)/jMax + eps;
    }
}

} // namespace ruckig
