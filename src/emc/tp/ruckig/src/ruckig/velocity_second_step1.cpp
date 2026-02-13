#include <ruckig/block.hpp>
#include <ruckig/velocity.hpp>


namespace ruckig {

VelocitySecondOrderStep1::VelocitySecondOrderStep1(double v0, double vf, double aMax, double aMin): _aMax(aMax), _aMin(aMin) {
    vd = vf - v0;
}

bool VelocitySecondOrderStep1::get_profile(const Profile& input, Block& block) {
    auto& p = block.p_min;
    p.set_boundary(input);

    const double af = (vd > 0) ? _aMax : _aMin;
    p.t[0] = 0;
    p.t[1] = vd / af;
    p.t[2] = 0;
    p.t[3] = 0;
    p.t[4] = 0;
    p.t[5] = 0;
    p.t[6] = 0;

    if (p.check_for_second_order_velocity<Profile::ControlSigns::UDDU, Profile::ReachedLimits::ACC0>(af)) {
        block.t_min = p.t_sum.back() + p.brake.duration + p.accel.duration;
        return true;
    }
    return false;
}

} // namespace ruckig
