#include <ruckig/block.hpp>
#include <ruckig/position.hpp>


namespace ruckig {

PositionThirdOrderStep1::PositionThirdOrderStep1(double p0, double v0, double a0, double pf, double vf, double af, double vMax, double vMin, double aMax, double aMin, double jMax): v0(v0), a0(a0), vf(vf), af(af), _vMax(vMax), _vMin(vMin), _aMax(aMax), _aMin(aMin), _jMax(jMax) {
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

void PositionThirdOrderStep1::time_all_vel(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, double jMax, bool) const {
    // ACC0_ACC1_VEL
    profile->t[0] = (-a0 + aMax)/jMax;
    profile->t[1] = (a0_a0/2 - aMax*aMax - jMax*(v0 - vMax))/(aMax*jMax);
    profile->t[2] = aMax/jMax;
    profile->t[3] = (3*(a0_p4*aMin - af_p4*aMax) + 8*aMax*aMin*(af_p3 - a0_p3 + 3*jMax*(a0*v0 - af*vf)) + 6*a0_a0*aMin*(aMax*aMax - 2*jMax*v0) - 6*af_af*aMax*(aMin*aMin - 2*jMax*vf) - 12*jMax*(aMax*aMin*(aMax*(v0 + vMax) - aMin*(vf + vMax) - 2*jMax*pd) + (aMin - aMax)*jMax*vMax*vMax + jMax*(aMax*vf_vf - aMin*v0_v0)))/(24*aMax*aMin*jMax_jMax*vMax);
    profile->t[4] = -aMin/jMax;
    profile->t[5] = -(af_af/2 - aMin*aMin - jMax*(vf - vMax))/(aMin*jMax);
    profile->t[6] = profile->t[4] + af/jMax;

    if (profile->check<ControlSigns::UDDU, ReachedLimits::ACC0_ACC1_VEL>(jMax, vMax, vMin, aMax, aMin)) {
        add_profile(profile);
        return;
    }

    // ACC1_VEL
    const double t_acc0 = std::sqrt(a0_a0/(2*jMax_jMax) + (vMax - v0)/jMax);

    profile->t[0] = t_acc0 - a0/jMax;
    profile->t[1] = 0;
    profile->t[2] = t_acc0;
    profile->t[3] = -(3*af_p4 - 8*aMin*(af_p3 - a0_p3) - 24*aMin*jMax*(a0*v0 - af*vf) + 6*af_af*(aMin*aMin - 2*jMax*vf) - 12*jMax*(2*aMin*jMax*pd + aMin*aMin*(vf + vMax) + jMax*(vMax*vMax - vf_vf) + aMin*t_acc0*(a0_a0 - 2*jMax*(v0 + vMax))))/(24*aMin*jMax_jMax*vMax);
    // profile->t[4] = -aMin/jMax;
    // profile->t[5] = -(af_af/2 - aMin*aMin + jMax*(vMax - vf))/(aMin*jMax);
    // profile->t[6] = profile->t[4] + af/jMax;

    if (profile->check<ControlSigns::UDDU, ReachedLimits::ACC1_VEL>(jMax, vMax, vMin, aMax, aMin)) {
        add_profile(profile);
        return;
    }

    // ACC0_VEL
    const double t_acc1 = std::sqrt(af_af/(2*jMax_jMax) + (vMax - vf)/jMax);

    profile->t[0] = (-a0 + aMax)/jMax;
    profile->t[1] = (a0_a0/2 - aMax*aMax - jMax*(v0 - vMax))/(aMax*jMax);
    profile->t[2] = aMax/jMax;
    profile->t[3] = (3*a0_p4 + 8*aMax*(af_p3 - a0_p3) + 24*aMax*jMax*(a0*v0 - af*vf) + 6*a0_a0*(aMax*aMax - 2*jMax*v0) - 12*jMax*(-2*aMax*jMax*pd + aMax*aMax*(v0 + vMax) + jMax*(vMax*vMax - v0_v0) + aMax*t_acc1*(-af_af + 2*(vf + vMax)*jMax)))/(24*aMax*jMax_jMax*vMax);
    profile->t[4] = t_acc1;
    profile->t[5] = 0;
    profile->t[6] = t_acc1 + af/jMax;

    if (profile->check<ControlSigns::UDDU, ReachedLimits::ACC0_VEL>(jMax, vMax, vMin, aMax, aMin)) {
        add_profile(profile);
        return;
    }

    // VEL
    // Solution 3/4
    profile->t[0] = t_acc0 - a0/jMax;
    profile->t[1] = 0;
    profile->t[2] = t_acc0;
    profile->t[3] = (af_p3 - a0_p3)/(3*jMax_jMax*vMax) + (a0*v0 - af*vf + (af_af*t_acc1 + a0_a0*t_acc0)/2)/(jMax*vMax) - (v0/vMax + 1.0)*t_acc0 - (vf/vMax + 1.0)*t_acc1 + pd/vMax;
    // profile->t[4] = t_acc1;
    // profile->t[5] = 0;
    // profile->t[6] = t_acc1 + af/jMax;

    if (profile->check<ControlSigns::UDDU, ReachedLimits::VEL>(jMax, vMax, vMin, aMax, aMin)) {
        add_profile(profile);
    }
}

void PositionThirdOrderStep1::time_acc0_acc1(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, double jMax, bool return_after_found) const {
    double h1 = (3*(af_p4*aMax - a0_p4*aMin) + aMax*aMin*(8*(a0_p3 - af_p3) + 3*aMax*aMin*(aMax - aMin) + 6*aMin*af_af - 6*aMax*a0_a0) + 12*jMax*(aMax*aMin*((aMax - 2*a0)*v0 - (aMin - 2*af)*vf) + aMin*a0_a0*v0 - aMax*af_af*vf))/(3*(aMax - aMin)*jMax_jMax) + 4*(aMax*vf_vf - aMin*v0_v0 - 2*aMin*aMax*pd)/(aMax - aMin);

    if (h1 >= 0) {
        h1 = std::sqrt(h1) / 2;
        const double h2 = a0_a0/(2*aMax*jMax) + (aMin - 2*aMax)/(2*jMax) - v0/aMax;
        const double h3 = -af_af/(2*aMin*jMax) - (aMax - 2*aMin)/(2*jMax) + vf/aMin;

        // UDDU: Solution 2
        if (h2 > h1/aMax && h3 > -h1/aMin) {
            profile->t[0] = (-a0 + aMax)/jMax;
            profile->t[1] = h2 - h1/aMax;
            profile->t[2] = aMax/jMax;
            profile->t[3] = 0;
            profile->t[4] = -aMin/jMax;
            profile->t[5] = h3 + h1/aMin;
            profile->t[6] = profile->t[4] + af/jMax;

            if (profile->check<ControlSigns::UDDU, ReachedLimits::ACC0_ACC1, true>(jMax, vMax, vMin, aMax, aMin)) {
                add_profile(profile);
                if (return_after_found) {
                    return;
                }
            }
        }

        // UDDU: Solution 1
        if (h2 > -h1/aMax && h3 > h1/aMin) {
            profile->t[0] = (-a0 + aMax)/jMax;
            profile->t[1] = h2 + h1/aMax;
            profile->t[2] = aMax/jMax;
            profile->t[3] = 0;
            profile->t[4] = -aMin/jMax;
            profile->t[5] = h3 - h1/aMin;
            profile->t[6] = profile->t[4] + af/jMax;

            if (profile->check<ControlSigns::UDDU, ReachedLimits::ACC0_ACC1, true>(jMax, vMax, vMin, aMax, aMin)) {
                add_profile(profile);
            }
        }
    }
}

void PositionThirdOrderStep1::time_all_none_acc0_acc1(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, double jMax, bool return_after_found) const {
    // NONE UDDU / UDUD Strategy: t7 == 0 (equals UDDU), this one is in particular prone to numerical issues
    const double h2_none = (a0_a0 - af_af)/(2*jMax) + (vf - v0);
    const double h2_h2 = h2_none*h2_none;
    const double t_min_none = (a0 - af)/jMax;
    const double t_max_none = (aMax - aMin)/jMax;

    std::array<double, 4> polynom_none;
    polynom_none[0] = 0;
    polynom_none[1] = -2*(a0_a0 + af_af - 2*jMax*(v0 + vf))/jMax_jMax;
    polynom_none[2] = 4*(a0_p3 - af_p3 + 3*jMax*(af*vf - a0*v0))/(3*jMax*jMax_jMax) - 4*pd/jMax;
    polynom_none[3] = -h2_h2/jMax_jMax;


    // ACC0
    const double h3_acc0 = (a0_a0 - af_af)/(2*aMax*jMax) + (vf - v0)/aMax;
    const double t_min_acc0 = (aMax - af)/jMax;
    const double t_max_acc0 = (aMax - aMin)/jMax;

    const double h0_acc0 = 3*(af_p4 - a0_p4) + 8*(a0_p3 - af_p3)*aMax + 24*aMax*jMax*(af*vf - a0*v0) - 6*a0_a0*(aMax*aMax - 2*jMax*v0) + 6*af_af*(aMax*aMax - 2*jMax*vf) + 12*jMax*(jMax*(vf_vf - v0_v0 - 2*aMax*pd) - aMax*aMax*(vf - v0));
    const double h2_acc0 = -af_af + aMax*aMax + 2*jMax*vf;

    std::array<double, 4> polynom_acc0;
    polynom_acc0[0] = -2*aMax/jMax;
    polynom_acc0[1] = h2_acc0/jMax_jMax;
    polynom_acc0[2] = 0;
    polynom_acc0[3] = h0_acc0/(12*jMax_jMax*jMax_jMax);


    // ACC1
    const double h3_acc1 = -(a0_a0 + af_af)/(2*jMax*aMin) + aMin/jMax + (vf - v0)/aMin;
    const double t_min_acc1 = (aMin - a0)/jMax;
    const double t_max_acc1 = (aMax - a0)/jMax;

    const double h0_acc1 = (a0_p4 - af_p4)/4 + 2*(af_p3 - a0_p3)*aMin/3 + (a0_a0 - af_af)*aMin*aMin/2 + jMax*(af_af*vf + a0_a0*v0 + 2*aMin*(jMax*pd - a0*v0 - af*vf) + aMin*aMin*(v0 + vf) + jMax*(v0_v0 - vf_vf));
    const double h2_acc1 = a0_a0 - a0*aMin + 2*jMax*v0;

    std::array<double, 4> polynom_acc1;
    polynom_acc1[0] = 2*(2*a0 - aMin)/jMax;
    polynom_acc1[1] = (5*a0_a0 + aMin*(aMin - 6*a0) + 2*jMax*v0)/jMax_jMax;
    polynom_acc1[2] = 2*(a0 - aMin)*h2_acc1/(jMax_jMax*jMax);
    polynom_acc1[3] = h0_acc1/(jMax_jMax*jMax_jMax);


    auto roots_none = roots::solve_quart_monic(polynom_none);
    auto roots_acc0 = roots::solve_quart_monic(polynom_acc0);
    auto roots_acc1 = roots::solve_quart_monic(polynom_acc1);

    for (double t: roots_none) {
        if (t < t_min_none || t > t_max_none) {
            continue;
        }

        // Single Newton-step (regarding pd)
        if (t > DBL_EPSILON) {
            const double h1 = jMax*t*t;
            const double orig = -h2_h2/(4*jMax*t) + h2_none*(af/jMax + t) + (4*a0_p3 + 2*af_p3 - 6*a0_a0*(af + 2*jMax*t) + 12*(af - a0)*jMax*v0 + 3*jMax_jMax*(-4*pd + (h1 + 8*v0)*t))/(12*jMax_jMax);
            const double deriv = h2_none + 2*v0 - a0_a0/jMax + h2_h2/(4*h1) + (3*h1)/4;

            t -= orig / deriv;
        }

        const double h0 = h2_none/(2*jMax*t);
        profile->t[0] = h0 + t/2 - a0/jMax;
        profile->t[1] = 0;
        profile->t[2] = t;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = -h0 + t/2 + af/jMax;

        if (profile->check<ControlSigns::UDDU, ReachedLimits::NONE>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile);
            if (return_after_found) {
                return;
            }
        }
    }

    for (double t: roots_acc0) {
        if (t < t_min_acc0 || t > t_max_acc0) {
            continue;
        }

        // Single Newton step (regarding pd)
        if (t > DBL_EPSILON) {
            const double h1 = jMax*t;
            const double orig = h0_acc0/(12*jMax_jMax*t) + t*(h2_acc0 + h1*(h1 - 2*aMax));
            const double deriv = 2*(h2_acc0 + h1*(2*h1 - 3*aMax));

            t -= orig / deriv;
        }

        profile->t[0] = (-a0 + aMax)/jMax;
        profile->t[1] = h3_acc0 - 2*t + jMax/aMax*t*t;
        profile->t[2] = t;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = (af - aMax)/jMax + t;

        if (profile->check<ControlSigns::UDDU, ReachedLimits::ACC0>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile);
            if (return_after_found) {
                return;
            }
        }
    }

    for (double t: roots_acc1) {
        if (t < t_min_acc1 || t > t_max_acc1) {
            continue;
        }

        // Double Newton step (regarding pd)
        if (t > DBL_EPSILON) {
            const double h5 = a0_p3 + 2*jMax*a0*v0;
            double h1 = jMax*t;
            double orig = -(h0_acc1/2 + h1*(h5 + a0*(aMin - 2*h1)*(aMin - h1) + a0_a0*(5*h1/2 - 2*aMin) + aMin*aMin*h1/2 + jMax*(h1/2 - aMin)*(h1*t + 2*v0)))/jMax;
            double deriv = (aMin - a0 - h1)*(h2_acc1 + h1*(4*a0 - aMin + 2*h1));
            t -= std::min(orig / deriv, t);

            h1 = jMax*t;
            orig = -(h0_acc1/2 + h1*(h5 + a0*(aMin - 2*h1)*(aMin - h1) + a0_a0*(5*h1/2 - 2*aMin) + aMin*aMin*h1/2 + jMax*(h1/2 - aMin)*(h1*t + 2*v0)))/jMax;

            if (std::abs(orig) > 1e-9) {
                deriv = (aMin - a0 - h1)*(h2_acc1 + h1*(4*a0 - aMin + 2*h1));
                t -= orig / deriv;

                h1 = jMax*t;
                orig = -(h0_acc1/2 + h1*(h5 + a0*(aMin - 2*h1)*(aMin - h1) + a0_a0*(5*h1/2 - 2*aMin) + aMin*aMin*h1/2 + jMax*(h1/2 - aMin)*(h1*t + 2*v0)))/jMax;

                if (std::abs(orig) > 1e-9) {
                    deriv = (aMin - a0 - h1)*(h2_acc1 + h1*(4*a0 - aMin + 2*h1));
                    t -= orig / deriv;
                }
            }
        }

        profile->t[0] = t;
        profile->t[1] = 0;
        profile->t[2] = (a0 - aMin)/jMax + t;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = h3_acc1 - (2*a0 + jMax*t)*t/aMin;
        profile->t[6] = (af - aMin)/jMax;

        if (profile->check<ControlSigns::UDDU, ReachedLimits::ACC1, true>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile);
            if (return_after_found) {
                return;
            }
        }
    }
}


void PositionThirdOrderStep1::time_acc1_vel_two_step(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, double jMax) const {
    profile->t[0] = 0;
    profile->t[1] = 0;
    profile->t[2] = a0/jMax;
    profile->t[3] = -(3*af_p4 - 8*aMin*(af_p3 - a0_p3) - 24*aMin*jMax*(a0*v0 - af*vf) + 6*af_af*(aMin*aMin - 2*jMax*vf) - 12*jMax*(2*aMin*jMax*pd + aMin*aMin*(vf + vMax) + jMax*(vMax*vMax - vf_vf) + aMin*a0*(a0_a0 - 2*jMax*(v0 + vMax))/jMax))/(24*aMin*jMax_jMax*vMax);
    profile->t[4] = -aMin/jMax;
    profile->t[5] = -(af_af/2 - aMin*aMin + jMax*(vMax - vf))/(aMin*jMax);
    profile->t[6] = profile->t[4] + af/jMax;

    if (profile->check<ControlSigns::UDDU, ReachedLimits::ACC1_VEL>(jMax, vMax, vMin, aMax, aMin)) {
        add_profile(profile);
    }
}

void PositionThirdOrderStep1::time_acc0_two_step(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, double jMax) const {
    // Two step
    {
        profile->t[0] = 0;
        profile->t[1] = (af_af - a0_a0 + 2*jMax*(vf - v0))/(2*a0*jMax);
        profile->t[2] = (a0 - af)/jMax;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (profile->check<ControlSigns::UDDU, ReachedLimits::ACC0>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile);
            return;
        }
    }

    // Three step - Removed pf
    {
        profile->t[0] = (-a0 + aMax)/jMax;
        profile->t[1] = (a0_a0 + af_af - 2*aMax*aMax + 2*jMax*(vf - v0))/(2*aMax*jMax);
        profile->t[2] = (-af + aMax)/jMax;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (profile->check<ControlSigns::UDDU, ReachedLimits::ACC0>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile);
            return;
        }
    }

    // Three step - Removed aMax
    {
        const double h0 = 3*(af_af - a0_a0 + 2*jMax*(v0 + vf));
        const double h2 = a0_p3 + 2*af_p3 + 6*jMax_jMax*pd + 6*(af - a0)*jMax*vf - 3*a0*af_af;
        const double h1 = std::sqrt(2*(2*h2*h2 + h0*(a0_p4 - 6*a0_a0*(af_af + 2*jMax*vf) + 8*a0*(af_p3 + 3*jMax_jMax*pd + 3*af*jMax*vf) - 3*(af_p4 + 4*af_af*jMax*vf + 4*jMax_jMax*(vf_vf - v0_v0))))) * std::abs(jMax) / jMax;
        profile->t[0] = (4*af_p3 + 2*a0_p3 - 6*a0*af_af + 12*jMax_jMax*pd + 12*(af - a0)*jMax*vf + h1)/(2*jMax*h0);
        profile->t[1] = -h1/(jMax*h0);
        profile->t[2] = (-4*a0_p3 - 2*af_p3 + 6*a0_a0*af + 12*jMax_jMax*pd - 12*(af - a0)*jMax*v0 + h1)/(2*jMax*h0);
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (profile->check<ControlSigns::UDDU, ReachedLimits::ACC0>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile);
            return;
        }
    }

    // Three step - t=(aMax - aMin)/jMax
    {
        const double t = (aMax - aMin)/jMax;

        profile->t[0] = (-a0 + aMax)/jMax;
        profile->t[1] = (a0_a0 - af_af)/(2*aMax*jMax) + (vf - v0 + jMax*t*t)/aMax - 2*t;
        profile->t[2] = t;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = (af - aMin)/jMax;

        if (profile->check<ControlSigns::UDDU, ReachedLimits::ACC0>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile);
            return;
        }
    }
}

void PositionThirdOrderStep1::time_vel_two_step(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, double jMax) const {
    const double h1 = std::sqrt(af_af/(2*jMax_jMax) + (vMax - vf)/jMax);

    // Four step
    {
        // Solution 3/4
        profile->t[0] = -a0/jMax;
        profile->t[1] = 0;
        profile->t[2] = 0;
        profile->t[3] = (af_p3 - a0_p3)/(3*jMax_jMax*vMax) + (a0*v0 - af*vf + (af_af*h1)/2)/(jMax*vMax) - (vf/vMax + 1.0)*h1 + pd/vMax;
        profile->t[4] = h1;
        profile->t[5] = 0;
        profile->t[6] = h1 + af/jMax;

        if (profile->check<ControlSigns::UDDU, ReachedLimits::VEL>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile);
            return;
        }
    }


    // Four step
    {
        profile->t[0] = 0;
        profile->t[1] = 0;
        profile->t[2] = a0/jMax;
        profile->t[3] = (af_p3 - a0_p3)/(3*jMax_jMax*vMax) + (a0*v0 - af*vf + (af_af*h1 + a0_p3/jMax)/2)/(jMax*vMax) - (v0/vMax + 1.0)*a0/jMax - (vf/vMax + 1.0)*h1 + pd/vMax;
        profile->t[4] = h1;
        profile->t[5] = 0;
        profile->t[6] = h1 + af/jMax;

        if (profile->check<ControlSigns::UDDU, ReachedLimits::VEL>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile);
            return;
        }
    }
}

void PositionThirdOrderStep1::time_none_two_step(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, double jMax) const {
    // Two step
    {
        const double h0 = std::sqrt((a0_a0 + af_af)/2 + jMax*(vf - v0)) * std::abs(jMax) / jMax;
        profile->t[0] = (h0 - a0)/jMax;
        profile->t[1] = 0;
        profile->t[2] = (h0 - af)/jMax;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (profile->check<ControlSigns::UDDU, ReachedLimits::NONE>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile);
            return;
        }
    }

    // Single step
    {
        profile->t[0] = (af - a0)/jMax;
        profile->t[1] = 0;
        profile->t[2] = 0;
        profile->t[3] = 0;
        profile->t[4] = 0;
        profile->t[5] = 0;
        profile->t[6] = 0;

        if (profile->check<ControlSigns::UDDU, ReachedLimits::NONE>(jMax, vMax, vMin, aMax, aMin)) {
            add_profile(profile);
            return;
        }
    }
}

bool PositionThirdOrderStep1::time_all_single_step(Profile* profile, double vMax, double vMin, double aMax, double aMin, double /* jMax */) const {
    if (std::abs(af - a0) > DBL_EPSILON) {
        return false;
    }

    profile->t[0] = 0;
    profile->t[1] = 0;
    profile->t[2] = 0;
    profile->t[3] = 0;
    profile->t[4] = 0;
    profile->t[5] = 0;
    profile->t[6] = 0;

    if (std::abs(a0) > DBL_EPSILON) {
        const double q = std::sqrt(2*a0*pd + v0_v0);

        // Solution 1
        profile->t[3] = (-v0 + q) / a0;
        if (profile->t[3] >= 0.0 && profile->check<ControlSigns::UDDU, ReachedLimits::NONE>(0.0, vMax, vMin, aMax, aMin)) {
            return true;
        }

        // Solution 2
        profile->t[3] = -(v0 + q) / a0;
        if (profile->t[3] >= 0.0 && profile->check<ControlSigns::UDDU, ReachedLimits::NONE>(0.0, vMax, vMin, aMax, aMin)) {
            return true;
        }

    } else if (std::abs(v0) > DBL_EPSILON) {
        profile->t[3] = pd / v0;
        if (profile->check<ControlSigns::UDDU, ReachedLimits::NONE>(0.0, vMax, vMin, aMax, aMin)) {
            return true;
        }

    } else if (std::abs(pd) < DBL_EPSILON) {
        if (profile->check<ControlSigns::UDDU, ReachedLimits::NONE>(0.0, vMax, vMin, aMax, aMin)) {
            return true;
        }
    }

    return false;
}


bool PositionThirdOrderStep1::get_profile(const Profile& input, Block& block) {
    // Zero-limits special case
    if (_jMax == 0.0 || _aMax == 0.0 || _aMin == 0.0) {
        auto& p = block.p_min;
        p.set_boundary(input);

        if (time_all_single_step(&p, _vMax, _vMin, _aMax, _aMin, _jMax)) {
            block.t_min = p.t_sum.back() + p.brake.duration + p.accel.duration;
            if (std::abs(v0) > DBL_EPSILON || std::abs(a0) > DBL_EPSILON) {
                block.a = Block::Interval(block.t_min, std::numeric_limits<double>::infinity());
            }
            return true;
        }
        return false;
    }

    const ProfileIter start = valid_profiles.begin();
    ProfileIter profile = start;
    profile->set_boundary(input);

    if (std::abs(vf) < DBL_EPSILON && std::abs(af) < DBL_EPSILON) {
        const double vMax = (pd >= 0) ? _vMax : _vMin;
        const double vMin = (pd >= 0) ? _vMin : _vMax;
        const double aMax = (pd >= 0) ? _aMax : _aMin;
        const double aMin = (pd >= 0) ? _aMin : _aMax;
        const double jMax = (pd >= 0) ? _jMax : -_jMax;

        if (std::abs(v0) < DBL_EPSILON && std::abs(a0) < DBL_EPSILON && std::abs(pd) < DBL_EPSILON) {
            time_all_none_acc0_acc1(profile, vMax, vMin, aMax, aMin, jMax, true);

        } else {
            // There is no blocked interval when vf==0 && af==0, so return after first found profile
            time_all_vel(profile, vMax, vMin, aMax, aMin, jMax, true);
            if (profile > start) { goto return_block; }
            time_all_none_acc0_acc1(profile, vMax, vMin, aMax, aMin, jMax, true);
            if (profile > start) { goto return_block; }
            time_acc0_acc1(profile, vMax, vMin, aMax, aMin, jMax, true);
            if (profile > start) { goto return_block; }

            time_all_vel(profile, vMin, vMax, aMin, aMax, -jMax, true);
            if (profile > start) { goto return_block; }
            time_all_none_acc0_acc1(profile, vMin, vMax, aMin, aMax, -jMax, true);
            if (profile > start) { goto return_block; }
            time_acc0_acc1(profile, vMin, vMax, aMin, aMax, -jMax, true);
        }

    } else {
        time_all_none_acc0_acc1(profile, _vMax, _vMin, _aMax, _aMin, _jMax, false);
        time_all_none_acc0_acc1(profile, _vMin, _vMax, _aMin, _aMax, -_jMax, false);
        time_acc0_acc1(profile, _vMax, _vMin, _aMax, _aMin, _jMax, false);
        time_acc0_acc1(profile, _vMin, _vMax, _aMin, _aMax, -_jMax, false);
        time_all_vel(profile, _vMax, _vMin, _aMax, _aMin, _jMax, false);
        time_all_vel(profile, _vMin, _vMax, _aMin, _aMax, -_jMax, false);
    }

    if (profile == start) {
        time_none_two_step(profile, _vMax, _vMin, _aMax, _aMin, _jMax);
        if (profile > start) { goto return_block; }
        time_none_two_step(profile, _vMin, _vMax, _aMin, _aMax, -_jMax);
        if (profile > start) { goto return_block; }
        time_acc0_two_step(profile, _vMax, _vMin, _aMax, _aMin, _jMax);
        if (profile > start) { goto return_block; }
        time_acc0_two_step(profile, _vMin, _vMax, _aMin, _aMax, -_jMax);
        if (profile > start) { goto return_block; }
        time_vel_two_step(profile, _vMax, _vMin, _aMax, _aMin, _jMax);
        if (profile > start) { goto return_block; }
        time_vel_two_step(profile, _vMin, _vMax, _aMin, _aMax, -_jMax);
        if (profile > start) { goto return_block; }
        time_acc1_vel_two_step(profile, _vMax, _vMin, _aMax, _aMin, _jMax);
        if (profile > start) { goto return_block; }
        time_acc1_vel_two_step(profile, _vMin, _vMax, _aMin, _aMax, -_jMax);
    }

return_block:
    return Block::calculate_block(block, valid_profiles, std::distance(start, profile));
}

} // namespace ruckig
