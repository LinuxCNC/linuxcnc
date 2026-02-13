#include <ruckig/block.hpp>
#include <ruckig/position.hpp>
#include <ruckig/profile.hpp>
#include <ruckig/roots.hpp>


namespace ruckig {

PositionFirstOrderStep2::PositionFirstOrderStep2(double tf, double p0, double pf, double vMax, double vMin): tf(tf), _vMax(vMax), _vMin(vMin) {
    pd = pf - p0;
}

bool PositionFirstOrderStep2::get_profile(Profile& profile) {
    const double vf = pd / tf;

    profile.t[0] = 0;
    profile.t[1] = 0;
    profile.t[2] = 0;
    profile.t[3] = tf;
    profile.t[4] = 0;
    profile.t[5] = 0;
    profile.t[6] = 0;

    return profile.check_for_first_order_with_timing<Profile::ControlSigns::UDDU, Profile::ReachedLimits::NONE>(tf, vf, _vMax, _vMin);
}

} // namespace ruckig
