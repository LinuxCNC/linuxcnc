#include <ruckig/block.hpp>
#include <ruckig/velocity.hpp>
#include <ruckig/profile.hpp>
#include <ruckig/roots.hpp>


namespace ruckig {

VelocitySecondOrderStep2::VelocitySecondOrderStep2(double tf, double v0, double vf, double aMax, double aMin): tf(tf), _aMax(aMax), _aMin(aMin) {
    vd = vf - v0;
}

bool VelocitySecondOrderStep2::get_profile(Profile& profile) {
    const double af = vd / tf;

    profile.t[0] = 0;
    profile.t[1] = tf;
    profile.t[2] = 0;
    profile.t[3] = 0;
    profile.t[4] = 0;
    profile.t[5] = 0;
    profile.t[6] = 0;

    if (profile.check_for_second_order_velocity_with_timing<Profile::ControlSigns::UDDU, Profile::ReachedLimits::NONE>(tf, af, _aMax, _aMin)) {
        profile.pf = profile.p.back();
        return true;
    }

    return false;
}

} // namespace ruckig
