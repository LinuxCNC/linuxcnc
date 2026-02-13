#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <ruckig/brake.hpp>
#include <ruckig/roots.hpp>
#include <ruckig/utils.hpp>


namespace ruckig {

//! Information about the position extrema
struct Bound {
    //! The extreme position
    double min, max;

    //! Time when the positions are reached
    double t_min, t_max;
};


//! @brief A single-dof kinematic profile with position, velocity, acceleration and jerk
//!
//! The class members are only available in the Ruckig Community Version.
class Profile {
    constexpr static double v_eps {1e-12};
    constexpr static double a_eps {1e-12};
    constexpr static double j_eps {1e-12};

    constexpr static double p_precision {1e-8};
    constexpr static double v_precision {1e-8};
    constexpr static double a_precision {1e-10};
    constexpr static double t_precision {1e-12};

    constexpr static double t_max {1e12};

public:
    std::array<double, 7> t, t_sum, j;
    std::array<double, 8> a, v, p;

    //! Brake sub-profiles
    BrakeProfile brake, accel;

    //! Target (final) kinematic state
    double pf, vf, af;

    enum class ReachedLimits { ACC0_ACC1_VEL, VEL, ACC0, ACC1, ACC0_ACC1, ACC0_VEL, ACC1_VEL, NONE } limits;
    enum class Direction { UP, DOWN } direction;
    enum class ControlSigns { UDDU, UDUD } control_signs;


    // For third-order velocity interface
    template<ControlSigns control_signs, ReachedLimits limits>
    bool check_for_velocity(double jf, double aMax, double aMin) {
        if (t[0] < 0) {
            return false;
        }

        t_sum[0] = t[0];
        for (size_t i = 0; i < 6; ++i) {
            if (t[i+1] < 0) {
                return false;
            }

            t_sum[i+1] = t_sum[i] + t[i+1];
        }

        if (limits == ReachedLimits::ACC0) {
            if (t[1] < std::numeric_limits<double>::epsilon()) {
                return false;
            }
        }

        if (t_sum.back() > t_max) { // For numerical reasons, is that needed?
            return false;
        }

        if (control_signs == ControlSigns::UDDU) {
            j = {(t[0] > 0 ? jf : 0), 0, (t[2] > 0 ? -jf : 0), 0, (t[4] > 0 ? -jf : 0), 0, (t[6] > 0 ? jf : 0)};
        } else {
            j = {(t[0] > 0 ? jf : 0), 0, (t[2] > 0 ? -jf : 0), 0, (t[4] > 0 ? jf : 0), 0, (t[6] > 0 ? -jf : 0)};
        }

        for (size_t i = 0; i < 7; ++i) {
            a[i+1] = a[i] + t[i] * j[i];
            v[i+1] = v[i] + t[i] * (a[i] + t[i] * j[i] / 2);
            p[i+1] = p[i] + t[i] * (v[i] + t[i] * (a[i] / 2 + t[i] * j[i] / 6));
        }

        this->control_signs = control_signs;
        this->limits = limits;

        direction = (aMax > 0) ? Profile::Direction::UP : Profile::Direction::DOWN;
        const double aUppLim = (direction == Profile::Direction::UP ? aMax : aMin) + a_eps;
        const double aLowLim = (direction == Profile::Direction::UP ? aMin : aMax) - a_eps;

        // Velocity limit can be broken in the beginning if both initial velocity and acceleration are too high
        // std::cout << std::setprecision(15) << "target: " << std::abs(p.back() - pf) << " " << std::abs(v.back() - vf) << " " << std::abs(a.back() - af) << " T: " << t_sum.back() << " " << to_string() << std::endl;
        return std::abs(v.back() - vf) < v_precision && std::abs(a.back() - af) < a_precision
            && a[1] >= aLowLim && a[3] >= aLowLim && a[5] >= aLowLim
            && a[1] <= aUppLim && a[3] <= aUppLim && a[5] <= aUppLim;
    }

    template<ControlSigns control_signs, ReachedLimits limits>
    inline bool check_for_velocity_with_timing(double, double jf, double aMax, double aMin) {
        // Time doesn't need to be checked as every profile has a: tf - ... equation
        return check_for_velocity<control_signs, limits>(jf, aMax, aMin); // && (std::abs(t_sum.back() - tf) < t_precision);
    }

    template<ControlSigns control_signs, ReachedLimits limits>
    inline bool check_for_velocity_with_timing(double tf, double jf, double aMax, double aMin, double jMax) {
        return (std::abs(jf) < std::abs(jMax) + j_eps) && check_for_velocity_with_timing<control_signs, limits>(tf, jf, aMax, aMin);
    }

    inline void set_boundary_for_velocity(double p0_new, double v0_new, double a0_new, double vf_new, double af_new) {
        a[0] = a0_new;
        v[0] = v0_new;
        p[0] = p0_new;
        af = af_new;
        vf = vf_new;
    }


    // For second-order velocity interface
    template<ControlSigns control_signs, ReachedLimits limits>
    bool check_for_second_order_velocity(double aUp) {
        // ReachedLimits::ACC0
        if (t[1] < 0.0) {
            return false;
        }

        t_sum = {0, t[1], t[1], t[1], t[1], t[1], t[1]};
        if (t_sum.back() > t_max) { // For numerical reasons, is that needed?
            return false;
        }

        j = {0, 0, 0, 0, 0, 0, 0};
        a = {0, (t[1] > 0) ? aUp : 0, 0, 0, 0, 0, 0, af};
        for (size_t i = 0; i < 7; ++i) {
            v[i+1] = v[i] + t[i] * a[i];
            p[i+1] = p[i] + t[i] * (v[i] + t[i] * a[i] / 2);
        }

        this->control_signs = control_signs;
        this->limits = limits;

        direction = (aUp > 0) ? Profile::Direction::UP : Profile::Direction::DOWN;

        // Velocity limit can be broken in the beginning if both initial velocity and acceleration are too high
        // std::cout << std::setprecision(15) << "target: " << std::abs(p.back() - pf) << " " << std::abs(v.back() - vf) << " " << std::abs(a.back() - af) << " T: " << t_sum.back() << " " << to_string() << std::endl;
        return std::abs(v.back() - vf) < v_precision;
    }

    template<ControlSigns control_signs, ReachedLimits limits>
    inline bool check_for_second_order_velocity_with_timing(double, double aUp) {
        // Time doesn't need to be checked as every profile has a: tf - ... equation
        return check_for_second_order_velocity<control_signs, limits>(aUp); // && (std::abs(t_sum.back() - tf) < t_precision);
    }

    template<ControlSigns control_signs, ReachedLimits limits>
    inline bool check_for_second_order_velocity_with_timing(double tf, double aUp, double aMax, double aMin) {
        return (aMin - a_eps < aUp) && (aUp < aMax + a_eps) && check_for_second_order_velocity_with_timing<control_signs, limits>(tf, aUp);
    }


    // For third-order position interface
    template<ControlSigns control_signs, ReachedLimits limits, bool set_limits = false>
    bool check(double jf, double vMax, double vMin, double aMax, double aMin) {
        if (t[0] < 0) {
            return false;
        }

        t_sum[0] = t[0];
        for (size_t i = 0; i < 6; ++i) {
            if (t[i+1] < 0) {
                return false;
            }

            t_sum[i+1] = t_sum[i] + t[i+1];
        }

        if (limits == ReachedLimits::ACC0_ACC1_VEL || limits == ReachedLimits::ACC0_VEL || limits == ReachedLimits::ACC1_VEL || limits == ReachedLimits::VEL) {
            if (t[3] < std::numeric_limits<double>::epsilon()) {
                return false;
            }
        }

        if (limits == ReachedLimits::ACC0 || limits == ReachedLimits::ACC0_ACC1) {
            if (t[1] < std::numeric_limits<double>::epsilon()) {
                return false;
            }
        }

        if (limits == ReachedLimits::ACC1 || limits == ReachedLimits::ACC0_ACC1) {
            if (t[5] < std::numeric_limits<double>::epsilon()) {
                return false;
            }
        }

        if (t_sum.back() > t_max) { // For numerical reasons, is that needed?
            return false;
        }

        if (control_signs == ControlSigns::UDDU) {
            j = {(t[0] > 0 ? jf : 0), 0, (t[2] > 0 ? -jf : 0), 0, (t[4] > 0 ? -jf : 0), 0, (t[6] > 0 ? jf : 0)};
        } else {
            j = {(t[0] > 0 ? jf : 0), 0, (t[2] > 0 ? -jf : 0), 0, (t[4] > 0 ? jf : 0), 0, (t[6] > 0 ? -jf : 0)};
        }

        direction = (vMax > 0) ? Profile::Direction::UP : Profile::Direction::DOWN;
        const double vUppLim = (direction == Profile::Direction::UP ? vMax : vMin) + v_eps;
        const double vLowLim = (direction == Profile::Direction::UP ? vMin : vMax) - v_eps;

        for (size_t i = 0; i < 7; ++i) {
            a[i+1] = a[i] + t[i] * j[i];
            v[i+1] = v[i] + t[i] * (a[i] + t[i] * j[i] / 2);
            p[i+1] = p[i] + t[i] * (v[i] + t[i] * (a[i] / 2 + t[i] * j[i] / 6));

            if (limits == ReachedLimits::ACC0_ACC1_VEL || limits == ReachedLimits::ACC0_ACC1 || limits == ReachedLimits::ACC0_VEL || limits == ReachedLimits::ACC1_VEL || limits == ReachedLimits::VEL) {
                if (i == 2) {
                    a[3] = 0.0;
                }
            }

            if (set_limits) {
                if (limits == ReachedLimits::ACC1) {
                    if (i == 2) {
                        a[3] = aMin;
                    }
                }

                if (limits == ReachedLimits::ACC0_ACC1) {
                    if (i == 0) {
                        a[1] = aMax;
                    }

                    if (i == 4) {
                        a[5] = aMin;
                    }
                }
            }

            if (i > 1 && a[i+1] * a[i] < -std::numeric_limits<double>::epsilon()) {
                const double v_a_zero = v[i] - (a[i] * a[i]) / (2 * j[i]);
                if (v_a_zero > vUppLim || v_a_zero < vLowLim) {
                    return false;
                }
            }
        }

        this->control_signs = control_signs;
        this->limits = limits;

        const double aUppLim = (direction == Profile::Direction::UP ? aMax : aMin) + a_eps;
        const double aLowLim = (direction == Profile::Direction::UP ? aMin : aMax) - a_eps;

        // Velocity limit can be broken in the beginning if both initial velocity and acceleration are too high
        // std::cout << std::setprecision(16) << "target: " << std::abs(p.back() - pf) << " " << std::abs(v.back() - vf) << " " << std::abs(a.back() - af) << " T: " << t_sum.back() << " " << to_string() << std::endl;
        return std::abs(p.back() - pf) < p_precision && std::abs(v.back() - vf) < v_precision && std::abs(a.back() - af) < a_precision
            && a[1] >= aLowLim && a[3] >= aLowLim && a[5] >= aLowLim
            && a[1] <= aUppLim && a[3] <= aUppLim && a[5] <= aUppLim
            && v[3] <= vUppLim && v[4] <= vUppLim && v[5] <= vUppLim && v[6] <= vUppLim
            && v[3] >= vLowLim && v[4] >= vLowLim && v[5] >= vLowLim && v[6] >= vLowLim;
    }

    template<ControlSigns control_signs, ReachedLimits limits>
    inline bool check_with_timing(double, double jf, double vMax, double vMin, double aMax, double aMin) {
        // Time doesn't need to be checked as every profile has a: tf - ... equation
        return check<control_signs, limits>(jf, vMax, vMin, aMax, aMin); // && (std::abs(t_sum.back() - tf) < t_precision);
    }

    template<ControlSigns control_signs, ReachedLimits limits>
    inline bool check_with_timing(double tf, double jf, double vMax, double vMin, double aMax, double aMin, double jMax) {
        return (std::abs(jf) < std::abs(jMax) + j_eps) && check_with_timing<control_signs, limits>(tf, jf, vMax, vMin, aMax, aMin);
    }

    inline void set_boundary(const Profile& profile) {
        a[0] = profile.a[0];
        v[0] = profile.v[0];
        p[0] = profile.p[0];
        af = profile.af;
        vf = profile.vf;
        pf = profile.pf;
        brake = profile.brake;
        accel = profile.accel;
    }

    inline void set_boundary(double p0_new, double v0_new, double a0_new, double pf_new, double vf_new, double af_new) {
        a[0] = a0_new;
        v[0] = v0_new;
        p[0] = p0_new;
        af = af_new;
        vf = vf_new;
        pf = pf_new;
    }


    // For second-order position interface
    template<ControlSigns control_signs, ReachedLimits limits>
    bool check_for_second_order(double aUp, double aDown, double vMax, double vMin) {
        if (t[0] < 0) {
            return false;
        }

        t_sum[0] = t[0];
        for (size_t i = 0; i < 6; ++i) {
            if (t[i+1] < 0) {
                return false;
            }

            t_sum[i+1] = t_sum[i] + t[i+1];
        }

        if (t_sum.back() > t_max) { // For numerical reasons, is that needed?
            return false;
        }

        j = {0, 0, 0, 0, 0, 0, 0};
        if (control_signs == ControlSigns::UDDU) {
            a = {(t[0] > 0 ? aUp : 0), 0, (t[2] > 0 ? aDown : 0), 0, (t[4] > 0 ? aDown : 0), 0, (t[6] > 0 ? aUp : 0), af};
        } else {
            a = {(t[0] > 0 ? aUp : 0), 0, (t[2] > 0 ? aDown : 0), 0, (t[4] > 0 ? aUp : 0), 0, (t[6] > 0 ? aDown : 0), af};
        }

        direction = (vMax > 0) ? Profile::Direction::UP : Profile::Direction::DOWN;
        const double vUppLim = (direction == Profile::Direction::UP ? vMax : vMin) + v_eps;
        const double vLowLim = (direction == Profile::Direction::UP ? vMin : vMax) - v_eps;

        for (size_t i = 0; i < 7; ++i) {
            v[i+1] = v[i] + t[i] * a[i];
            p[i+1] = p[i] + t[i] * (v[i] + t[i] * a[i] / 2);
        }

        this->control_signs = control_signs;
        this->limits = limits;

        // Velocity limit can be broken in the beginning if both initial velocity and acceleration are too high
        // std::cout << std::setprecision(16) << "target: " << std::abs(p.back() - pf) << " " << std::abs(v.back() - vf) << " " << std::abs(a.back() - af) << " T: " << t_sum.back() << " " << to_string() << std::endl;
        return std::abs(p.back() - pf) < p_precision && std::abs(v.back() - vf) < v_precision
            && v[2] <= vUppLim && v[3] <= vUppLim && v[4] <= vUppLim && v[5] <= vUppLim && v[6] <= vUppLim
            && v[2] >= vLowLim && v[3] >= vLowLim && v[4] >= vLowLim && v[5] >= vLowLim && v[6] >= vLowLim;
    }

    template<ControlSigns control_signs, ReachedLimits limits>
    inline bool check_for_second_order_with_timing(double, double aUp, double aDown, double vMax, double vMin) {
        // Time doesn't need to be checked as every profile has a: tf - ... equation
        return check_for_second_order<control_signs, limits>(aUp, aDown, vMax, vMin); // && (std::abs(t_sum.back() - tf) < t_precision);
    }

    template<ControlSigns control_signs, ReachedLimits limits>
    inline bool check_for_second_order_with_timing(double tf, double aUp, double aDown, double vMax, double vMin, double aMax, double aMin) {
        return (aMin - a_eps < aUp) && (aUp < aMax + a_eps) && (aMin - a_eps < aDown) && (aDown < aMax + a_eps) && check_for_second_order_with_timing<control_signs, limits>(tf, aUp, aDown, vMax, vMin);
    }


    // For first-order position interface
    template<ControlSigns control_signs, ReachedLimits limits>
    bool check_for_first_order(double vUp) {
        // ReachedLimits::VEL
        if (t[3] < 0.0) {
            return false;
        }

        t_sum = {0, 0, 0, t[3], t[3], t[3], t[3]};
        if (t_sum.back() > t_max) { // For numerical reasons, is that needed?
            return false;
        }

        j = {0, 0, 0, 0, 0, 0, 0};
        a = {0, 0, 0, 0, 0, 0, 0, af};
        v = {0, 0, 0, t[3] > 0 ? vUp : 0, 0, 0, 0, vf};
        for (size_t i = 0; i < 7; ++i) {
            p[i+1] = p[i] + t[i] * (v[i] + t[i] * a[i] / 2);
        }

        this->control_signs = control_signs;
        this->limits = limits;

        direction = (vUp > 0) ? Profile::Direction::UP : Profile::Direction::DOWN;

        return std::abs(p.back() - pf) < p_precision;
    }

    template<ControlSigns control_signs, ReachedLimits limits>
    inline bool check_for_first_order_with_timing(double, double vUp) {
        // Time doesn't need to be checked as every profile has a: tf - ... equation
        return check_for_first_order<control_signs, limits>(vUp); // && (std::abs(t_sum.back() - tf) < t_precision);
    }

    template<ControlSigns control_signs, ReachedLimits limits>
    inline bool check_for_first_order_with_timing(double tf, double vUp, double vMax, double vMin) {
        return (vMin - v_eps < vUp) && (vUp < vMax + v_eps) && check_for_first_order_with_timing<control_signs, limits>(tf, vUp);
    }



    // Secondary features
    static void check_position_extremum(double t_ext, double t_sum, double t, double p, double v, double a, double j, Bound& ext) {
        if (0 < t_ext && t_ext < t) {
            double p_ext, a_ext;
            std::tie(p_ext, std::ignore, a_ext) = integrate(t_ext, p, v, a, j);
            if (a_ext > 0 && p_ext < ext.min) {
                ext.min = p_ext;
                ext.t_min = t_sum + t_ext;
            } else if (a_ext < 0 && p_ext > ext.max) {
                ext.max = p_ext;
                ext.t_max = t_sum + t_ext;
            }
        }
    }

    static void check_step_for_position_extremum(double t_sum, double t, double p, double v, double a, double j, Bound& ext) {
        if (p < ext.min) {
            ext.min = p;
            ext.t_min = t_sum;
        }
        if (p > ext.max) {
            ext.max = p;
            ext.t_max = t_sum;
        }

        if (j != 0) {
            const double D = a * a - 2 * j * v;
            if (std::abs(D) < std::numeric_limits<double>::epsilon()) {
                check_position_extremum(-a / j, t_sum, t, p, v, a, j, ext);

            } else if (D > 0.0) {
                const double D_sqrt = std::sqrt(D);
                check_position_extremum((-a - D_sqrt) / j, t_sum, t, p, v, a, j, ext);
                check_position_extremum((-a + D_sqrt) / j, t_sum, t, p, v, a, j, ext);
            }
        }
    }

    Bound get_position_extrema() const {
        Bound extrema;
        extrema.min = std::numeric_limits<double>::infinity();
        extrema.max = -std::numeric_limits<double>::infinity();

        if (brake.duration > 0.0) {
            if (brake.t[0] > 0.0) {
                check_step_for_position_extremum(0.0, brake.t[0], brake.p[0], brake.v[0], brake.a[0], brake.j[0], extrema);

                if (brake.t[1] > 0.0) {
                    check_step_for_position_extremum(brake.t[0], brake.t[1], brake.p[1], brake.v[1], brake.a[1], brake.j[1], extrema);
                }
            }
        }

        double t_current_sum {0.0};
        for (size_t i = 0; i < 7; ++i) {
            if (i > 0) {
                t_current_sum = t_sum[i - 1];
            }
            check_step_for_position_extremum(t_current_sum + brake.duration, t[i], p[i], v[i], a[i], j[i], extrema);
        }

        if (pf < extrema.min) {
            extrema.min = pf;
            extrema.t_min = t_sum.back() + brake.duration;
        }
        if (pf > extrema.max) {
            extrema.max = pf;
            extrema.t_max = t_sum.back() + brake.duration;
        }

        return extrema;
    }

    bool get_first_state_at_position(double pt, double& time, double time_after=0.0) const {
        double t_cum = 0.0;

        for (size_t i = 0; i < 7; ++i) {
            if (t[i] == 0.0) {
                continue;
            }

            if (std::abs(p[i] - pt) < DBL_EPSILON && t_cum >= time_after) {
                time = t_cum;
                return true;
            }

            for (const double _t: roots::solve_cubic(j[i]/6, a[i]/2, v[i], p[i] - pt)) {
                if (0 < _t && time_after - t_cum <= _t && _t <= t[i]) {
                    time = _t + t_cum;
                    return true;
                }
            }

            t_cum += t[i];
        }

        if ((t[6] > 0.0 || t_sum.back() == 0.0) && std::abs(pf - pt) < 1e-9 && t_sum.back() >= time_after) {
            time = t_sum.back();
            return true;
        }

        return false;
    }

    std::string to_string() const {
        std::string result;
        switch (direction) {
            case Direction::UP: result += "UP_"; break;
            case Direction::DOWN: result += "DOWN_"; break;
        }
        switch (limits) {
            case ReachedLimits::ACC0_ACC1_VEL: result += "ACC0_ACC1_VEL"; break;
            case ReachedLimits::VEL: result += "VEL"; break;
            case ReachedLimits::ACC0: result += "ACC0"; break;
            case ReachedLimits::ACC1: result += "ACC1"; break;
            case ReachedLimits::ACC0_ACC1: result += "ACC0_ACC1"; break;
            case ReachedLimits::ACC0_VEL: result += "ACC0_VEL"; break;
            case ReachedLimits::ACC1_VEL: result += "ACC1_VEL"; break;
            case ReachedLimits::NONE: result += "NONE"; break;
        }
        switch (control_signs) {
            case ControlSigns::UDDU: result += "_UDDU"; break;
            case ControlSigns::UDUD: result += "_UDUD"; break;
        }
        return result;
    }
};

} // namespace ruckig
