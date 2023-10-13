#include "ruckig.hpp"
#include "roots.hpp"

namespace ruckig {

VelocityStep1::VelocityStep1(double v0, double a0, double vf, double af, double aMax, double aMin, double jMax): v0(v0), a0(a0), vf(vf), af(af), _aMax(aMax), _aMin(aMin), _jMax(jMax) { }

void VelocityStep1::time_acc0(Profile& profile, double aMax, double aMin, double jMax) {
    // UD
    {
        profile.t[0] = (-a0 + aMax)/jMax;
        profile.t[1] = (a0*a0 + af*af - 2*aMax*aMax + 2*jMax*(vf - v0))/(2*aMax*jMax);
        profile.t[2] = (-af + aMax)/jMax;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = 0;

        if (profile.check_for_velocity<JerkSigns::UDDU, Limits::ACC0>(jMax, aMax, aMin)) {
            add_profile(profile, jMax);
        }
    }

    // UU
    {
        profile.t[0] = (-a0 + aMax)/jMax;
        profile.t[1] = (a0*a0 - af*af + 2*jMax*(vf - v0))/(2*aMax*jMax);
        profile.t[2] = 0;
        profile.t[3] = 0;
        profile.t[4] = (af - aMax)/jMax;
        profile.t[5] = 0;
        profile.t[6] = 0;

        if (profile.check_for_velocity<JerkSigns::UDUD, Limits::ACC0>(jMax, aMax, aMin)) {
            add_profile(profile, jMax);
        }
    }
}

void VelocityStep1::time_none(Profile& profile, double aMax, double aMin, double jMax) {
    double h1 = Sqrt((a0*a0 + af*af)/2 + jMax*(vf - v0));

    // Solution 1
    {
        profile.t[0] = -(a0 + h1)/jMax;
        profile.t[1] = 0;
        profile.t[2] = -(af + h1)/jMax;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = 0;

        if (profile.check_for_velocity<JerkSigns::UDDU, Limits::NONE>(jMax, aMax, aMin)) {
            add_profile(profile, jMax);
        }
    }

    // Solution 2
    {
        profile.t[0] = (-a0 + h1)/jMax;
        profile.t[1] = 0;
        profile.t[2] = (-af + h1)/jMax;
        profile.t[3] = 0;
        profile.t[4] = 0;
        profile.t[5] = 0;
        profile.t[6] = 0;

        if (profile.check_for_velocity<JerkSigns::UDDU, Limits::NONE>(jMax, aMax, aMin)) {
            add_profile(profile, jMax);
        }
    }
}

bool VelocityStep1::get_profile(const Profile& input, Block& block) {
    Profile profile = input;
    valid_profile_counter = 0;

    if (std::abs(v0) < DBL_EPSILON && std::abs(vf) < DBL_EPSILON && std::abs(a0) < DBL_EPSILON && std::abs(af) < DBL_EPSILON) {
        time_none(profile, _aMax, _aMin, _jMax);

    } else {
        time_none(profile, _aMax, _aMin, _jMax);
        time_acc0(profile, _aMax, _aMin, _jMax);
        time_none(profile, _aMin, _aMax, -_jMax);
        time_acc0(profile, _aMin, _aMax, -_jMax);
    }

    return Block::calculate_block(block, valid_profiles, valid_profile_counter);
}

} // namespace ruckig
