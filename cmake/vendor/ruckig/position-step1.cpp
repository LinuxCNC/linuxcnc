#include "ruckig.hpp"
#include "roots.hpp"

namespace ruckig {

PositionStep1::PositionStep1(double p0, double v0, double a0, double pf, double vf, double af, double vMax, double vMin, double aMax, double aMin, double jMax): p0(p0), v0(v0), a0(a0), pf(pf), vf(vf), af(af), _vMax(vMax), _vMin(vMin), _aMax(aMax), _aMin(aMin), _jMax(jMax) {
    pd = pf - p0;

    v0_v0 = v0 * v0;
    vf_vf = vf * vf;

    a0_a0 = a0 * a0;
    af_af = af * af;

    a0_p3 = a0 * a0_a0;
    a0_p4 = a0_a0 * a0_a0;
    af_p3 = af * af_af;
    af_p4 = af_af * af_af;

    // max values needs to be invariant to plus minus sign change
    jMax_jMax = jMax * jMax;
}

void PositionStep1::time_all_vel(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax) {
    // ACC0_ACC1_VEL
    profile.t[0] = (-a0 + aMax)/jMax;
    profile.t[1] = (a0_a0/2 - aMax*aMax - jMax*(v0 - vMax))/(aMax*jMax);
    profile.t[2] = aMax/jMax;
    profile.t[3] = (3*(a0_p4*aMin - af_p4*aMax) + 8*aMax*aMin*(af_p3 - a0_p3 + 3*jMax*(a0*v0 - af*vf)) + 6*a0_a0*aMin*(aMax*aMax - 2*jMax*v0) - 6*af_af*aMax*(aMin*aMin - 2*jMax*vf) - 12*jMax*(aMax*aMin*(aMax*(v0 + vMax) - aMin*(vf + vMax) - 2*jMax*pd) + (aMin - aMax)*jMax*vMax*vMax + jMax*(aMax*vf_vf - aMin*v0_v0)))/(24*aMax*aMin*jMax_jMax*vMax);
    profile.t[4] = -aMin/jMax;
    profile.t[5] = -(af_af/2 - aMin*aMin - jMax*(vf - vMax))/(aMin*jMax);
    profile.t[6] = profile.t[4] + af/jMax;

    if (profile.check<JerkSigns::UDDU, Limits::ACC0_ACC1_VEL>(jMax, vMax, vMin, aMax, aMin)) {
        add_profile(profile, jMax);
        return;
    }

    // ACC1_VEL
    const double t_acc0 = std::sqrt(a0_a0/(2*jMax_jMax) + (vMax - v0)/jMax);

    profile.t[0] = t_acc0 - a0/jMax;
    profile.t[1] = 0;
    profile.t[2] = t_acc0;
    profile.t[3] = -(3*af_p4 - 8*aMin*(af_p3 - a0_p3) - 24*aMin*jMax*(a0*v0 - af*vf) + 6*af_af*(aMin*aMin - 2*jMax*vf) - 12*jMax*(2*aMin*jMax*pd + aMin*aMin*(vf + vMax) + jMax*(vMax*vMax - vf_vf) + aMin*t_acc0*(a0_a0 - 2*jMax*(v0 + vMax))))/(24*aMin*jMax_jMax*vMax);
    // profile.t[4] = -aMin/jMax;
    // profile.t[5] = -(af_af/2 - aMin*aMin + jMax*(vMax - vf))/(aMin*jMax);
    // profile.t[6] = profile.t[4] + af/jMax;

    if (profile.check<JerkSigns::UDDU, Limits::ACC1_VEL>(jMax, vMax, vMin, aMax, aMin)) {
        add_profile(profile, jMax);
        return;
    }

    // ACC0_VEL
    const double t_acc1 = std::sqrt(af_af/(2*jMax_jMax) + (vMax - vf)/jMax);

    profile.t[0] = (-a0 + aMax)/jMax;
    profile.t[1] = (a0_a0/2 - aMax*aMax - jMax*(v0 - vMax))/(aMax*jMax);
    profile.t[2] = aMax/jMax;
    profile.t[3] = (3*a0_p4 + 8*(af_p3 - a0_p3)*aMax + 24*aMax*jMax*(a0*v0 - af*vf) + 6*a0_a0*(aMax*aMax - 2*jMax*v0) - 12*jMax*(-2*aMax*jMax*pd + aMax*aMax*(v0 + vMax) + jMax*(vMax*vMax - v0_v0) + (2*(vf + vMax)*jMax - af_af)*aMax*t_acc1))/(24*aMax*jMax_jMax*vMax);
    profile.t[4] = t_acc1;
    profile.t[5] = 0;
    profile.t[6] = t_acc1 + af/jMax;

    if (profile.check<JerkSigns::UDDU, Limits::ACC0_VEL>(jMax, vMax, vMin, aMax, aMin)) {
        add_profile(profile, jMax);
        return;
    }

    // VEL
    // Solution 3/4
    profile.t[0] = t_acc0 - a0/jMax;
    profile.t[1] = 0;
    profile.t[2] = t_acc0;
    profile.t[3] = (af_p3 - a0_p3)/(3*jMax_jMax*vMax) + (a0*v0 - af*vf + (af_af*t_acc1 + a0_a0*t_acc0)/2)/(jMax*vMax) - (v0/vMax + 1.0)*t_acc0 - (vf/vMax + 1.0)*t_acc1 + pd/vMax;
    // profile.t[4] = t_acc1;
    // profile.t[5] = 0;
    // profile.t[6] = t_acc1 + af/jMax;

    if (profile.check<JerkSigns::UDDU, Limits::VEL>(jMax, vMax, vMin, aMax, aMin)) {
        add_profile(profile, jMax);
    }
}

void PositionStep1::time_acc0_acc1(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax) {
    double h1 = (3*(af_p4*aMax - a0_p4*aMin) + aMax*aMin*(8*(a0_p3 - af_p3) + 3*aMax*aMin*(aMax - aMin) + 6*aMin*af_af - 6*aMax*a0_a0) + 12*jMax*(aMax*aMin*((aMax - 2*a0)*v0 - (aMin - 2*af)*vf) + aMin*a0_a0*v0 - aMax*af_af*vf))/(3*(aMax - aMin)*jMax_jMax) + 4*(aMax*vf_vf - aMin*v0_v0 - 2*aMin*aMax*pd)/(aMax - aMin);

    if (h1 >= 0) {
        h1 = std::sqrt(h1) / 2;
        const double h2 = a0_a0/(2*aMax*jMax) + (aMin - 2*aMax)/(2*jMax) - v0/aMax;
        const double h3 = -af_af/(2*aMin*jMax) - (aMax - 2*aMin)/(2*jMax) + vf/aMin;

        // UDDU: Solution 2
        if (h2 > h1/aMax && h3 > -h1/aMin)  {
            profile.t[0] = (-a0 + aMax)/jMax;
            profile.t[1] = h2 - h1/aMax;
            profile.t[2] = aMax/jMax;
            profile.t[3] = 0;
            profile.t[4] = -aMin/jMax;
            profile.t[5] = h3 + h1/aMin;
            profile.t[6] = profile.t[4] + af/jMax;

            if (profile.check<JerkSigns::UDDU, Limits::ACC0_ACC1, true>(jMax, vMax, vMin, aMax, aMin)) {
                add_profile(profile, jMax);
            }
        }

        // UDDU: Solution 1
        if (h2 > -h1/aMax && h3 > h1/aMin) {
            profile.t[0] = (-a0 + aMax)/jMax;
            profile.t[1] = h2 + h1/aMax;
            profile.t[2] = aMax/jMax;
            profile.t[3] = 0;
            profile.t[4] = -aMin/jMax;
            profile.t[5] = h3 - h1/aMin;
            profile.t[6] = profile.t[4] + af/jMax;

            if (profile.check<JerkSigns::UDDU, Limits::ACC0_ACC1, true>(jMax, vMax, vMin, aMax, aMin)) {
                add_profile(profile, jMax);
            }
        }
    }
}

void PositionStep1::time_acc1(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax) {
    const double h3 = -(a0_a0 + af_af)/(2*jMax*aMin) + aMin/jMax + (vf - v0)/aMin;
    const double t_max = (-a0 + aMax)/jMax;

    // double h4 = a0_a0/(aMin*aMin) + h3*jMax/aMin;
    // if (a0*aMin > 0 && h4 > 0) {
    //     if (t_max < -(a0 - aMin*std::sqrt(h4))/jMax) {
    //         return;
    //     }
    // }

    const double h0 = (a0_p4 - af_p4)/4 + 2*(af_p3 - a0_p3)*aMin/3 + (a0_a0 - af_af)*aMin*aMin/2 + jMax*(af_af*vf + a0_a0*v0 + 2*aMin*(jMax*pd - a0*v0 - af*vf) + aMin*aMin*(v0 + vf) + jMax*(v0_v0 - vf_vf));
    const double h2 = a0_a0 - a0*aMin + 2*jMax*v0;

    std::array<double, 4> polynom;
    polynom[0] = (2*(2*a0 - aMin))/jMax;
    polynom[1] = (5*a0_a0 + aMin*(aMin - 6*a0) + 2*jMax*v0)/jMax_jMax;
    polynom[2] = 2*(a0 - aMin)*h2/(jMax_jMax*jMax);
    polynom[3] = h0/(jMax_jMax*jMax_jMax);

    const double t_min = -(a0 - aMin)/jMax;

    auto roots = Roots::solveQuartMonic(polynom);
    for (double t: roots) {
        if (t < t_min || t > t_max) {
            continue;
        }

        // Double Newton step (regarding pd)
        if (t > DBL_EPSILON) {
            const double h5 = a0_p3 + 2*jMax*a0*v0;
            double h1 = jMax*t;
            double orig = -(h0/2 + h1*(h5 + a0*(aMin - 2*h1)*(aMin - h1) + a0_a0*(5*h1/2 - 2*aMin) + aMin*aMin*h1/2 + jMax*(h1/2 - aMin)*(h1*t + 2*v0)))/(aMin*jMax_jMax);
            double deriv = (aMin - a0 - h1)*(h2 + h1*(4*a0 - aMin + 2*h1))/(aMin*jMax);
            t -= std::min(orig / deriv, t);

            h1 = jMax*t;
            orig = -(h0/2 + h1*(h5 + a0*(aMin - 2*h1)*(aMin - h1) + a0_a0*(5*h1/2 - 2*aMin) + aMin*aMin*h1/2 + jMax*(h1/2 - aMin)*(h1*t + 2*v0)))/(aMin*jMax_jMax);
            
            if (std::abs(orig) > 1e-9) {
                deriv = (aMin - a0 - h1)*(h2 + h1*(4*a0 - aMin + 2*h1))/(aMin*jMax);
                t -= orig / deriv;

                h1 = jMax*t;
                orig = -(h0/2 + h1*(h5 + a0*(aMin - 2*h1)*(aMin - h1) + a0_a0*(5*h1/2 - 2*aMin) + aMin*aMin*h1/2 + jMax*(h1/2 - aMin)*(h1*t + 2*v0)))/(aMin*jMax_jMax);
                
                if (std::abs(orig) > 1e-9) {
                    deriv = (aMin - a0 - h1)*(h2 + h1*(4*a0 - aMin + 2*h1))/(aMin*jMax);
                    t -= orig / deriv;
                }
            }
        }

        profile.t[0] = t;
        profile.t[1] = 0;
        profile.t[2] = (a0 - aMin)/jMax + t;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = h3 - (2*a0 + jMax*t)*t/aMin;
        profile.t[6] = (af - aMin)/jMax;

        if (profile.check<JerkSigns::UDDU, Limits::ACC1, true>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile, jMax);
        }
    }
}

void PositionStep1::time_acc0(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax) {
    const double h3 = (a0_a0 - af_af)/(2*aMax*jMax) + (vf - v0)/aMax;

    // if (h3 < (aMax*aMax - aMin*aMin)/(jMax*aMax)) {
    //     return;
    // }

    const double t_max = (aMax - aMin)/jMax;
    const double h0 = 3*(af_p4 - a0_p4) + 8*(a0_p3 - af_p3)*aMax + 24*aMax*jMax*(af*vf - a0*v0) - 6*a0_a0*(aMax*aMax - 2*jMax*v0) + 6*af_af*(aMax*aMax - 2*jMax*vf) + 12*jMax*(jMax*(vf_vf - v0_v0 - 2*aMax*pd) - aMax*aMax*(vf - v0));
    const double h2 = -af_af + aMax*aMax + 2*jMax*vf;

    std::array<double, 4> polynom;
    polynom[0] = (-2*aMax)/jMax;
    polynom[1] = h2/jMax_jMax;
    polynom[2] = 0;
    polynom[3] = h0/(12*jMax_jMax*jMax_jMax);

    const double t_min = (aMax - af)/jMax;

    auto roots = Roots::solveQuartMonic(polynom);
    for (double t: roots) {
        if (t < t_min || t > t_max) {
            continue;
        }

        // Single Newton step (regarding pd)
        if (t > DBL_EPSILON) {
            const double h1 = jMax*t;
            const double orig = h0/(24*aMax*jMax_jMax) + t*t*(h2 + h1*(h1 - 2*aMax))/(2*aMax);
            const double deriv = t*(h2 + h1*(2*h1 - 3*aMax))/aMax;

            t -= orig / deriv;
        }

        profile.t[0] = (-a0 + aMax)/jMax;
        profile.t[1] = h3 - 2*t + jMax/aMax*t*t;
        profile.t[2] = t;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = (af - aMax)/jMax + t;

        if (profile.check<JerkSigns::UDDU, Limits::ACC0>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile, jMax);
        }
    }
}

void PositionStep1::time_none(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax) {
    // UDDU
    {
        if (std::abs(v0) < DBL_EPSILON && std::abs(a0) < DBL_EPSILON && std::abs(vf) < DBL_EPSILON && std::abs(af) < DBL_EPSILON) {
            profile.t[0] = std::cbrt(pd/(2*jMax));
            profile.t[1] = 0;
            profile.t[2] = 2*profile.t[0];
            profile.t[3] = 0;
            profile.t[4] = 0;
            profile.t[5] = 0;
            profile.t[6] = profile.t[0];

            if (profile.check<JerkSigns::UDDU, Limits::NONE>(jMax, vMax, vMin, aMax, aMin)) {
                add_profile(profile, jMax);
            }
            return;
        }

        if (std::abs(a0 - af) < DBL_EPSILON && std::abs(v0 + vf) < DBL_EPSILON && std::abs(p0 - pf) < DBL_EPSILON) {
            const double h1 = std::sqrt(a0_a0 - 2*jMax*v0);

            // Solution 3
            {
                profile.t[0] = -(a0 + h1)/jMax;
                profile.t[1] = 0;
                profile.t[2] = profile.t[0];
                profile.t[3] = 0;
                profile.t[4] = 0;
                profile.t[5] = 0;
                profile.t[6] = 0;

                if (profile.check<JerkSigns::UDDU, Limits::NONE>(jMax, vMax, vMin, aMax, aMin)) {
                    add_profile(profile, jMax);
                }
            }

            // Solution 4
            {
                profile.t[0] = -(a0 - h1)/jMax;
                profile.t[1] = 0;
                profile.t[2] = profile.t[0];
                profile.t[3] = 0;
                profile.t[4] = 0;
                profile.t[5] = 0;
                profile.t[6] = 0;

                if (profile.check<JerkSigns::UDDU, Limits::NONE>(jMax, vMax, vMin, aMax, aMin)) {
                    add_profile(profile, jMax);
                }
            }

            return;
        }
    }

    // UDDU / UDUD modern, this one is in particular prone to numerical issues
    {
        const double h2 = (a0_a0 - af_af)/(2*jMax) + (vf - v0);
        const double h2_h2 = h2*h2;

        // UDUD Strategy: t7 == 0 (equals UDDU)
        std::array<double, 4> polynom;
        polynom[0] = 0;
        polynom[1] = (-2*(a0_a0 + af_af - 2*jMax*(v0 + vf)))/jMax_jMax;
        polynom[2] = 4*(a0_p3 - af_p3 + 3*jMax*(af*vf - a0*v0))/(3*jMax*jMax_jMax) - 4*pd/jMax;
        polynom[3] = -h2_h2/jMax_jMax;

        const double t_min = (a0 - af)/jMax;
        const double t_max = (aMax - aMin)/jMax;

        // const double h01 = a0_a0/jMax - 2*(v0 - vMax);
        // const double h02 = Sqrt((h01 - h2)*h01) * Abs(jMax)/jMax;
        // const double t_min_vmax = Sqrt((2*h01 - h2 - 2*h02)/jMax);
        // t_min = std::max(t_min, t_min_vmax);

        auto roots = Roots::solveQuartMonic(polynom);
        for (double t: roots) {
            if (t < t_min || t > t_max) {
                continue;
            }

            // Single Newton-step (regarding pd)
            if (t > DBL_EPSILON) {
                const double h1 = jMax*t*t;
                const double orig = -h2_h2/(4*jMax*t) + h2*(af/jMax + t) + (4*a0_p3 + 2*af_p3 - 6*a0_a0*(af + 2*jMax*t) + 12*(af - a0)*jMax*v0 + 3*jMax_jMax*(-4*pd + (h1 + 8*v0)*t))/(12*jMax_jMax);
                const double deriv = h2 + 2*v0 - a0_a0/jMax + h2_h2/(4*h1) + (3*h1)/4;

                t -= orig / deriv;
            }

            const double h0 = h2/(2*jMax*t);
            profile.t[0] = h0 + t/2 - a0/jMax;
            profile.t[1] = 0;
            profile.t[2] = t;
            profile.t[3] = 0;
            profile.t[4] = 0;
            profile.t[5] = 0;
            profile.t[6] = -h0 + t/2 + af/jMax;

            if (profile.check<JerkSigns::UDDU, Limits::NONE>(jMax, vMax, vMin, aMax, aMin)) {
                add_profile(profile, jMax);
            }
        }
    }
}


void PositionStep1::time_acc1_vel_two_step(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax) {
    profile.t[0] = 0;
    profile.t[1] = 0;
    profile.t[2] = a0/jMax;
    profile.t[3] = -(3*af_p4 - 8*aMin*(af_p3 - a0_p3) - 24*aMin*jMax*(a0*v0 - af*vf) + 6*af_af*(aMin*aMin - 2*jMax*vf) - 12*jMax*(2*aMin*jMax*pd + aMin*aMin*(vf + vMax) + jMax*(vMax*vMax - vf_vf) + aMin*a0*(a0_a0 - 2*jMax*(v0 + vMax))/jMax))/(24*aMin*jMax_jMax*vMax);
    profile.t[4] = -aMin/jMax;
    profile.t[5] = -(af_af/2 - aMin*aMin + jMax*(vMax - vf))/(aMin*jMax);
    profile.t[6] = profile.t[4] + af/jMax;

    if (profile.check<JerkSigns::UDDU, Limits::ACC1_VEL>(jMax, vMax, vMin, aMax, aMin)) {
        add_profile(profile, jMax);
    }
}

void PositionStep1::time_acc0_two_step(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax) {
    // Two step
    {
        profile.t[0] = 0;
        profile.t[1] = (af_af - a0_a0 + 2*jMax*(vf - v0))/(2*a0*jMax);
        profile.t[2] = (a0 - af)/jMax;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = 0;

        if (profile.check<JerkSigns::UDDU, Limits::ACC0>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile, jMax);
            return;
        }
    }

    // Three step - Removed pf
    {
        profile.t[0] = (-a0 + aMax)/jMax;
        profile.t[1] = (a0_a0 + af_af - 2*aMax*aMax + 2*jMax*(vf - v0))/(2*aMax*jMax);
        profile.t[2] = (-af + aMax)/jMax;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = 0;

        if (profile.check<JerkSigns::UDDU, Limits::ACC0>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile, jMax);
            return;
        }
    }

    // Three step - Removed aMax
    {
        const double h0 = 3*(af_af - a0_a0 + 2*jMax*(v0 + vf));
        const double h2 = a0_p3 + 2*af_p3 + 6*jMax_jMax*pd + 6*(af - a0)*jMax*vf - 3*a0*af_af;
        const double h1 = std::sqrt(2*(2*h2*h2 + h0*(a0_p4 - 6*a0_a0*(af_af + 2*jMax*vf) + 8*a0*(af_p3 + 3*jMax_jMax*pd + 3*af*jMax*vf) - 3*(af_p4 + 4*af_af*jMax*vf + 4*jMax_jMax*(vf_vf - v0_v0))))) * Abs(jMax) / jMax;
        profile.t[0] = (4*af_p3 + 2*a0_p3 - 6*a0*af_af + 12*jMax_jMax*pd + 12*(af - a0)*jMax*vf + h1)/(2*jMax*h0);
        profile.t[1] = -h1/(jMax*h0);
        profile.t[2] = (-4*a0_p3 - 2*af_p3 + 6*a0_a0*af + 12*jMax_jMax*pd - 12*(af - a0)*jMax*v0 + h1)/(2*jMax*h0);
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = 0;

        if (profile.check<JerkSigns::UDDU, Limits::ACC0>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile, jMax);
            return;
        }
    }
}

void PositionStep1::time_vel_two_step(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax) {
    const double h1 = std::sqrt(af_af/(2*jMax_jMax) + (vMax - vf)/jMax);

    // Four step
    {
        // Solution 3/4
        profile.t[0] = -a0/jMax;
        profile.t[1] = 0;
        profile.t[2] = 0;
        profile.t[3] = (af_p3 - a0_p3)/(3*jMax_jMax*vMax) + (a0*v0 - af*vf + (af_af*h1)/2)/(jMax*vMax) - (vf/vMax + 1.0)*h1 + pd/vMax;
        profile.t[4] = h1;
        profile.t[5] = 0;
        profile.t[6] = h1 + af/jMax;

        if (profile.check<JerkSigns::UDDU, Limits::VEL>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile, jMax);
            return;
        }
    }


    // Four step
    {
        profile.t[0] = 0;
        profile.t[1] = 0;
        profile.t[2] = a0/jMax;
        profile.t[3] = (af_p3 - a0_p3)/(3*jMax_jMax*vMax) + (a0*v0 - af*vf + (af_af*h1 + a0_p3/jMax)/2)/(jMax*vMax) - (v0/vMax + 1.0)*a0/jMax - (vf/vMax + 1.0)*h1 + pd/vMax;
        profile.t[4] = h1;
        profile.t[5] = 0;
        profile.t[6] = h1 + af/jMax;

        if (profile.check<JerkSigns::UDDU, Limits::VEL>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile, jMax);
            return;
        }
    }
}

void PositionStep1::time_none_two_step(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax) {
    // Two step
    {
        const double h0 = std::sqrt((a0_a0 + af_af)/2 + jMax*(vf - v0)) * Abs(jMax) / jMax;
        profile.t[0] = (h0 - a0)/jMax;
        profile.t[1] = 0;
        profile.t[2] = (h0 - af)/jMax;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = 0;

        if (profile.check<JerkSigns::UDDU, Limits::NONE>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile, jMax);
            return;
        }
    }

    // Single step
    {
        profile.t[0] = (af - a0)/jMax;
        profile.t[1] = 0;
        profile.t[2] = 0;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = 0;

        if (profile.check<JerkSigns::UDDU, Limits::NONE>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile, jMax);
            return;
        }
    }
}


bool PositionStep1::get_profile(const Profile& input, Block& block) {
    Profile profile = input;
    valid_profile_counter = 0;

    if (std::abs(vf) < DBL_EPSILON && std::abs(af) < DBL_EPSILON) {
        const double vMax = (pd >= 0) ? _vMax : _vMin;
        const double vMin = (pd >= 0) ? _vMin : _vMax;
        const double aMax = (pd >= 0) ? _aMax : _aMin;
        const double aMin = (pd >= 0) ? _aMin : _aMax;
        const double jMax = (pd >= 0) ? _jMax : -_jMax;

        if (std::abs(v0) < DBL_EPSILON && std::abs(a0) < DBL_EPSILON && std::abs(pd) < DBL_EPSILON) {
            time_none(profile, vMax, vMin, aMax, aMin, jMax);

        } else {
            // There is no blocked interval when vf==0 && af==0, so return after first found profile
            time_all_vel(profile, vMax, vMin, aMax, aMin, jMax);
            if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }
            time_none(profile, vMax, vMin, aMax, aMin, jMax);
            if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }
            time_acc0(profile, vMax, vMin, aMax, aMin, jMax);
            if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }
            time_acc1(profile, vMax, vMin, aMax, aMin, jMax);
            if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }
            time_acc0_acc1(profile, vMax, vMin, aMax, aMin, jMax);
            if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }

            time_all_vel(profile, vMin, vMax, aMin, aMax, -jMax);
            if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }
            time_none(profile, vMin, vMax, aMin, aMax, -jMax);
            if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }
            time_acc0(profile, vMin, vMax, aMin, aMax, -jMax);
            if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }
            time_acc1(profile, vMin, vMax, aMin, aMax, -jMax);
            if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }
            time_acc0_acc1(profile, vMin, vMax, aMin, aMax, -jMax);
        }

    } else {
        time_all_vel(profile, _vMax, _vMin, _aMax, _aMin, _jMax);
        time_all_vel(profile, _vMin, _vMax, _aMin, _aMax, -_jMax);
        time_none(profile, _vMax, _vMin, _aMax, _aMin, _jMax);
        time_acc0(profile, _vMax, _vMin, _aMax, _aMin, _jMax);
        time_acc1(profile, _vMax, _vMin, _aMax, _aMin, _jMax);
        time_acc0_acc1(profile, _vMax, _vMin, _aMax, _aMin, _jMax);
        time_none(profile, _vMin, _vMax, _aMin, _aMax, -_jMax);
        time_acc0(profile, _vMin, _vMax, _aMin, _aMax, -_jMax);
        time_acc1(profile, _vMin, _vMax, _aMin, _aMax, -_jMax);
        time_acc0_acc1(profile, _vMin, _vMax, _aMin, _aMax, -_jMax);
    }

    if (valid_profile_counter == 0) {
        time_none_two_step(profile, _vMax, _vMin, _aMax, _aMin, _jMax);
        if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }
        time_none_two_step(profile, _vMin, _vMax, _aMin, _aMax, -_jMax);
        if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }
        time_acc0_two_step(profile, _vMax, _vMin, _aMax, _aMin, _jMax);
        if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }
        time_acc0_two_step(profile, _vMin, _vMax, _aMin, _aMax, -_jMax);
        if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }
        time_vel_two_step(profile, _vMax, _vMin, _aMax, _aMin, _jMax);
        if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }
        time_vel_two_step(profile, _vMin, _vMax, _aMin, _aMax, -_jMax);
        if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }
        time_acc1_vel_two_step(profile, _vMax, _vMin, _aMax, _aMin, _jMax);
        if (valid_profile_counter > 0) { return Block::calculate_block(block, valid_profiles, valid_profile_counter); }
        time_acc1_vel_two_step(profile, _vMin, _vMax, _aMin, _aMax, -_jMax);
    }

    return Block::calculate_block(block, valid_profiles, valid_profile_counter);
}

} // namespace ruckig
