#include <ruckig/block.hpp>
#include <ruckig/position.hpp>
#include <ruckig/profile.hpp>
#include <ruckig/roots.hpp>


namespace ruckig {

PositionSecondOrderStep2::PositionSecondOrderStep2(double tf, double p0, double v0, double pf, double vf, double vMax, double vMin, double aMax, double aMin): v0(v0), tf(tf), vf(vf), _vMax(vMax), _vMin(vMin), _aMax(aMax), _aMin(aMin) {
    pd = pf - p0;
    vd = vf - v0;
}

bool PositionSecondOrderStep2::time_acc0(Profile& profile, double vMax, double vMin, double aMax, double aMin) {
    // UD Solution 1/2
    {
        const double h1 = std::sqrt((2*aMax*(pd - tf*vf) - 2*aMin*(pd - tf*v0) + vd*vd)/(aMax*aMin) + tf*tf);

        profile.t[0] = (aMax*vd - aMax*aMin*(tf - h1))/(aMax*(aMax - aMin));
        profile.t[1] = h1;
        profile.t[2] = tf - (profile.t[0] + h1);
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = 0;

        if (profile.check_for_second_order_with_timing<ControlSigns::UDDU, ReachedLimits::ACC0>(tf, aMax, aMin, vMax, vMin)) {
            profile.pf = profile.p.back();
            return true;
        }
    }

    // UU Solution
    {
        const double h1 = (-vd + aMax*tf);

        profile.t[0] = -vd*vd/(2*aMax*h1) + (pd - v0*tf)/h1;
        profile.t[1] = -vd/aMax + tf;
        profile.t[2] = 0;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = tf - (profile.t[0] + profile.t[1]);

        if (profile.check_for_second_order_with_timing<ControlSigns::UDDU, ReachedLimits::ACC0>(tf, aMax, aMin, vMax, vMin)) {
            profile.pf = profile.p.back();
            return true;
        }
    }

    // UU Solution - 2 step
    {
        profile.t[0] = 0;
        profile.t[1] = -vd/aMax + tf;
        profile.t[2] = 0;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = vd/aMax;

        if (profile.check_for_second_order_with_timing<ControlSigns::UDDU, ReachedLimits::ACC0>(tf, aMax, aMin, vMax, vMin)) {
            profile.pf = profile.p.back();
            return true;
        }
    }

    return false;
}

bool PositionSecondOrderStep2::time_none(Profile& profile, double vMax, double vMin, double aMax, double aMin) {
    if (std::abs(v0) < DBL_EPSILON && std::abs(vf) < DBL_EPSILON && std::abs(pd) < DBL_EPSILON) {
        profile.t[0] = 0;
        profile.t[1] = tf;
        profile.t[2] = 0;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = 0;

        if (profile.check_for_second_order_with_timing<ControlSigns::UDDU, ReachedLimits::NONE>(tf, aMax, aMin, vMax, vMin)) {
            profile.pf = profile.p.back();
            return true;
        }
    }

    // UD Solution 1/2
    {
        const double h1 = 2*(vf*tf - pd);

        profile.t[0] = h1/vd;
        profile.t[1] = tf - profile.t[0];
        profile.t[2] = 0;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = 0;

        const double af = vd*vd/h1;

        if ((aMin - 1e-12 < af) && (af < aMax + 1e-12) && profile.check_for_second_order_with_timing<ControlSigns::UDDU, ReachedLimits::NONE>(tf, af, -af, vMax, vMin)) {
            profile.pf = profile.p.back();
            return true;
        }
    }

    return false;
}

bool PositionSecondOrderStep2::get_profile(Profile& profile) {
    // Test all cases to get ones that match
    // However we should guess which one is correct and try them first...
    if (pd > 0) {
        return check_all(profile, _vMax, _vMin, _aMax, _aMin) || check_all(profile, _vMin, _vMax, _aMin, _aMax);
    }

    return check_all(profile, _vMin, _vMax, _aMin, _aMax) || check_all(profile, _vMax, _vMin, _aMax, _aMin);
}

} // namespace ruckig
