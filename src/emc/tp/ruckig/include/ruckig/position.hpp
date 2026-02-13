#pragma once

#include <array>
#include <optional>


namespace ruckig {

//! Mathematical equations for Step 1 in third-order position interface: Extremal profiles
class PositionThirdOrderStep1 {
    using ReachedLimits = Profile::ReachedLimits;
    using ControlSigns = Profile::ControlSigns;

    const double v0, a0;
    const double vf, af;
    const double _vMax, _vMin, _aMax, _aMin, _jMax;

    // Pre-calculated expressions
    double pd;
    double v0_v0, vf_vf;
    double a0_a0, a0_p3, a0_p4;
    double af_af, af_p3, af_p4;
    double jMax_jMax;

    // Max 5 valid profiles + 1 spare for numerical issues
    using ProfileIter = std::array<Profile, 6>::iterator;
    std::array<Profile, 6> valid_profiles;

    void time_all_vel(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, double jMax, bool return_after_found) const;
    void time_acc0_acc1(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, double jMax, bool return_after_found) const;
    void time_all_none_acc0_acc1(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, double jMax, bool return_after_found) const;

    // Only for numerical issues, always return_after_found
    void time_acc1_vel_two_step(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, double jMax) const;
    void time_acc0_two_step(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, double jMax) const;
    void time_vel_two_step(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, double jMax) const;
    void time_none_two_step(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, double jMax) const;

    // Only for zero-limits case
    bool time_all_single_step(Profile* profile, double vMax, double vMin, double aMax, double aMin, double jMax) const;

    inline void add_profile(ProfileIter& profile) const {
        const auto prev_profile = profile;
        ++profile;
        profile->set_boundary(*prev_profile);
    }

public:
    explicit PositionThirdOrderStep1(double p0, double v0, double a0, double pf, double vf, double af, double vMax, double vMin, double aMax, double aMin, double jMax);

    bool get_profile(const Profile& input, Block& block);
};


//! Mathematical equations for Step 2 in third-order position interface: Time synchronization
class PositionThirdOrderStep2 {
    using ReachedLimits = Profile::ReachedLimits;
    using ControlSigns = Profile::ControlSigns;

    const double v0, a0;
    const double tf, vf, af;
    const double _vMax, _vMin, _aMax, _aMin, _jMax;

    // Pre-calculated expressions
    double pd;
    double tf_tf, tf_p3, tf_p4;
    double vd, vd_vd;
    double ad, ad_ad;
    double v0_v0, vf_vf;
    double a0_a0, a0_p3, a0_p4, a0_p5, a0_p6;
    double af_af, af_p3, af_p4, af_p5, af_p6;
    double jMax_jMax;
    double g1, g2;

    bool time_acc0_acc1_vel(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax);
    bool time_acc1_vel(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax);
    bool time_acc0_vel(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax);
    bool time_vel(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax);
    bool time_acc0_acc1(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax);
    bool time_acc1(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax);
    bool time_acc0(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax);
    bool time_none(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax);
    bool time_none_smooth(Profile& profile, double vMax, double vMin, double aMax, double aMin, double jMax);

public:
    explicit PositionThirdOrderStep2(double tf, double p0, double v0, double a0, double pf, double vf, double af, double vMax, double vMin, double aMax, double aMin, double jMax);

    bool get_profile(Profile& profile);
};


//! Mathematical equations for Step 1 in second-order position interface: Extremal profiles
class PositionSecondOrderStep1 {
    using ReachedLimits = Profile::ReachedLimits;
    using ControlSigns = Profile::ControlSigns;

    const double v0, vf;
    const double _vMax, _vMin, _aMax, _aMin;

    // Pre-calculated expressions
    double pd;

    // Max 3 valid profiles
    using ProfileIter = std::array<Profile, 3>::iterator;
    std::array<Profile, 3> valid_profiles;

    void time_acc0(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, bool return_after_found) const;
    void time_none(ProfileIter& profile, double vMax, double vMin, double aMax, double aMin, bool return_after_found) const;

    // Only for zero-limits case
    bool time_all_single_step(Profile* profile, double vMax, double vMin, double aMax, double aMin) const;

    inline void add_profile(ProfileIter& profile) const {
        const auto prev_profile = profile;
        ++profile;
        profile->set_boundary(*prev_profile);
    }

public:
    explicit PositionSecondOrderStep1(double p0, double v0, double pf, double vf, double vMax, double vMin, double aMax, double aMin);

    bool get_profile(const Profile& input, Block& block);
};


//! Mathematical equations for Step 2 in second-order position interface: Time synchronization
class PositionSecondOrderStep2 {
    using ReachedLimits = Profile::ReachedLimits;
    using ControlSigns = Profile::ControlSigns;

    const double v0, tf, vf;
    const double _vMax, _vMin, _aMax, _aMin;

    // Pre-calculated expressions
    double pd, vd;

    bool time_acc0(Profile& profile, double vMax, double vMin, double aMax, double aMin);
    bool time_none(Profile& profile, double vMax, double vMin, double aMax, double aMin);

    inline bool check_all(Profile& profile, double vMax, double vMin, double aMax, double aMin) {
        return time_acc0(profile, vMax, vMin, aMax, aMin) || time_none(profile, vMax, vMin, aMax, aMin);
    }

public:
    explicit PositionSecondOrderStep2(double tf, double p0, double v0, double pf, double vf, double vMax, double vMin, double aMax, double aMin);

    bool get_profile(Profile& profile);
};


//! Mathematical equations for Step 1 in first-order position interface: Extremal profiles
class PositionFirstOrderStep1 {
    const double _vMax, _vMin;
    double pd; // Pre-calculated expressions

public:
    explicit PositionFirstOrderStep1(double p0, double pf, double vMax, double vMin);

    bool get_profile(const Profile& input, Block& block);
};


//! Mathematical equations for Step 2 in first-order position interface: Time synchronization
class PositionFirstOrderStep2 {
    const double tf, _vMax, _vMin;
    double pd; // Pre-calculated expressions

public:
    explicit PositionFirstOrderStep2(double tf, double p0, double pf, double vMax, double vMin);

    bool get_profile(Profile& profile);
};

} // namespace ruckig
