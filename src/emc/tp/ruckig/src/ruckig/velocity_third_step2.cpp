#include <ruckig/block.hpp>
#include <ruckig/velocity.hpp>
#include <ruckig/profile.hpp>
#include <ruckig/roots.hpp>


namespace ruckig {

VelocityThirdOrderStep2::VelocityThirdOrderStep2(double tf, double v0, double a0, double vf, double af, double aMax, double aMin, double jMax): a0(a0), tf(tf), af(af), _aMax(aMax), _aMin(aMin), _jMax(jMax) {
    vd = vf - v0;
    ad = af - a0;
}

bool VelocityThirdOrderStep2::time_acc0(Profile& profile, double aMax, double aMin, double jMax) {
    // UD Solution 1/2
    {
        const double h1 = std::sqrt((-ad*ad + 2*jMax*((a0 + af)*tf - 2*vd))/(jMax*jMax) + tf*tf);

        profile.t[0] = ad/(2*jMax) + (tf - h1)/2;
        profile.t[1] = h1;
        profile.t[2] = tf - (profile.t[0] + h1);
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = 0;

        if (profile.check_for_velocity_with_timing<ControlSigns::UDDU, ReachedLimits::ACC0>(tf, jMax, aMax, aMin)) {
            profile.pf = profile.p.back();
            return true;
        }
    }

    // UU Solution
    {
        const double h1 = (-ad + jMax*tf);

        profile.t[0] = -ad*ad/(2*jMax*h1) + (vd - a0*tf)/h1;
        profile.t[1] = -ad/jMax + tf;
        profile.t[2] = 0;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = tf - (profile.t[0] + profile.t[1]);

        if (profile.check_for_velocity_with_timing<ControlSigns::UDDU, ReachedLimits::ACC0>(tf, jMax, aMax, aMin)) {
            profile.pf = profile.p.back();
            return true;
        }
    }

    // UU Solution - 2 step
    {
        profile.t[0] = 0;
        profile.t[1] = -ad/jMax + tf;
        profile.t[2] = 0;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = ad/jMax;

        if (profile.check_for_velocity_with_timing<ControlSigns::UDDU, ReachedLimits::ACC0>(tf, jMax, aMax, aMin)) {
            profile.pf = profile.p.back();
            return true;
        }
    }

    return false;
}

bool VelocityThirdOrderStep2::time_none(Profile& profile, double aMax, double aMin, double jMax) {
    if (std::abs(a0) < DBL_EPSILON && std::abs(af) < DBL_EPSILON && std::abs(vd) < DBL_EPSILON) {
        profile.t[0] = 0;
        profile.t[1] = tf;
        profile.t[2] = 0;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = 0;

        if (profile.check_for_velocity_with_timing<ControlSigns::UDDU, ReachedLimits::NONE>(tf, jMax, aMax, aMin)) {
            profile.pf = profile.p.back();
            return true;
        }
    }

    // UD Solution 1/2
    {
        const double h1 = 2*(af*tf - vd);

        profile.t[0] = h1/ad;
        profile.t[1] = tf - profile.t[0];
        profile.t[2] = 0;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = 0;

        const double jf = ad*ad/h1;

        if (std::abs(jf) < std::abs(jMax) + 1e-12 && profile.check_for_velocity_with_timing<ControlSigns::UDDU, ReachedLimits::NONE>(tf, jf, aMax, aMin)) {
            profile.pf = profile.p.back();
            return true;
        }
    }

    return false;
}

bool VelocityThirdOrderStep2::get_profile(Profile& profile) {
    // Test all cases to get ones that match
    // However we should guess which one is correct and try them first...
    if (vd > 0) {
        return check_all(profile, _aMax, _aMin, _jMax) || check_all(profile, _aMin, _aMax, -_jMax);
    }

    return check_all(profile, _aMin, _aMax, -_jMax) || check_all(profile, _aMax, _aMin, _jMax);
}

} // namespace ruckig
