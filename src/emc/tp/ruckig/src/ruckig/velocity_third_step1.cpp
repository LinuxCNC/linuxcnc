#include <ruckig/block.hpp>
#include <ruckig/velocity.hpp>


namespace ruckig {

VelocityThirdOrderStep1::VelocityThirdOrderStep1(double v0, double a0, double vf, double af, double aMax, double aMin, double jMax): a0(a0), af(af), _aMax(aMax), _aMin(aMin), _jMax(jMax) {
    vd = vf - v0;
}

void VelocityThirdOrderStep1::time_acc0(ProfileIter& profile, double aMax, double aMin, double jMax, bool) const {
    profile->t[0] = (-a0 + aMax)/jMax;
    profile->t[1] = (a0*a0 + af*af)/(2*aMax*jMax) - aMax/jMax + vd/aMax;
    profile->t[2] = (-af + aMax)/jMax;
    profile->t[3] = 0;
    profile->t[4] = 0;
    profile->t[5] = 0;
    profile->t[6] = 0;

    if (profile->check_for_velocity<ControlSigns::UDDU, ReachedLimits::ACC0>(jMax, aMax, aMin)) {
        add_profile(profile);
    }
}

void VelocityThirdOrderStep1::time_none(ProfileIter& profile, double aMax, double aMin, double jMax, bool return_after_found) const {
    double h1 = (a0*a0 + af*af)/2 + jMax*vd;
    if (h1 >= 0.0) {
        h1 = std::sqrt(h1);

        // Solution 1
        {
            profile->t[0] = -(a0 + h1)/jMax;
            profile->t[1] = 0;
            profile->t[2] = -(af + h1)/jMax;
            profile->t[3] = 0;
            profile->t[4] = 0;
            profile->t[5] = 0;
            profile->t[6] = 0;

            if (profile->check_for_velocity<ControlSigns::UDDU, ReachedLimits::NONE>(jMax, aMax, aMin)) {
                add_profile(profile);
                if (return_after_found) {
                    return;
                }
            }
        }

        // Solution 2
        {
            profile->t[0] = (-a0 + h1)/jMax;
            profile->t[1] = 0;
            profile->t[2] = (-af + h1)/jMax;
            profile->t[3] = 0;
            profile->t[4] = 0;
            profile->t[5] = 0;
            profile->t[6] = 0;

            if (profile->check_for_velocity<ControlSigns::UDDU, ReachedLimits::NONE>(jMax, aMax, aMin)) {
                add_profile(profile);
            }
        }
    }
}

bool VelocityThirdOrderStep1::time_all_single_step(Profile* profile, double aMax, double aMin, double /* jMax */) const {
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
        profile->t[3] = vd / a0;
        if (profile->check_for_velocity<ControlSigns::UDDU, ReachedLimits::NONE>(0.0, aMax, aMin)) {
            return true;
        }

    } else if (std::abs(vd) < DBL_EPSILON) {
        if (profile->check_for_velocity<ControlSigns::UDDU, ReachedLimits::NONE>(0.0, aMax, aMin)) {
            return true;
        }
    }

    return false;
}


bool VelocityThirdOrderStep1::get_profile(const Profile& input, Block& block) {
    // Zero-limits special case
    if (_jMax == 0.0) {
        auto& p = block.p_min;
        p.set_boundary(input);

        if (time_all_single_step(&p, _aMax, _aMin, _jMax)) {
            block.t_min = p.t_sum.back() + p.brake.duration + p.accel.duration;
            if (std::abs(a0) > DBL_EPSILON) {
                block.a = Block::Interval(block.t_min, std::numeric_limits<double>::infinity());
            }
            return true;
        }
        return false;
    }

    const ProfileIter start = valid_profiles.begin();
    ProfileIter profile = start;
    profile->set_boundary(input);

    if (std::abs(af) < DBL_EPSILON) {
        // There is no blocked interval when af==0, so return after first found profile
        const double aMax = (vd >= 0) ? _aMax : _aMin;
        const double aMin = (vd >= 0) ? _aMin : _aMax;
        const double jMax = (vd >= 0) ? _jMax : -_jMax;

        time_none(profile, aMax, aMin, jMax, true);
        if (profile > start) { goto return_block; }
        time_acc0(profile, aMax, aMin, jMax, true);
        if (profile > start) { goto return_block; }

        time_none(profile, aMin, aMax, -jMax, true);
        if (profile > start) { goto return_block; }
        time_acc0(profile, aMin, aMax, -jMax, true);

    } else {
        time_none(profile, _aMax, _aMin, _jMax, false);
        time_none(profile, _aMin, _aMax, -_jMax, false);
        time_acc0(profile, _aMax, _aMin, _jMax, false);
        time_acc0(profile, _aMin, _aMax, -_jMax, false);
    }

return_block:
    return Block::calculate_block(block, valid_profiles, std::distance(start, profile));
}

} // namespace ruckig
