#pragma once

#include <algorithm>
#include <limits>
#include <numeric>
#include <ruckig/optional.hpp>
#include <string>

#include <ruckig/profile.hpp>


namespace ruckig {

//! Which times are possible for synchronization?
class Block {
    template<size_t N>
    inline static void remove_profile(std::array<Profile, N>& valid_profiles, size_t& valid_profile_counter, size_t index) {
        for (size_t i = index; i < valid_profile_counter - 1; ++i) {
            valid_profiles[i] = valid_profiles[i + 1];
        }
        valid_profile_counter -= 1;
    }

public:
    struct Interval {
        double left, right; // [s]
        Profile profile; // Profile corresponding to right (end) time

        explicit Interval(double left, double right): left(left), right(right) { }

        explicit Interval(const Profile& profile_left, const Profile& profile_right) {
            const double left_duration = profile_left.t_sum.back() + profile_left.brake.duration + profile_left.accel.duration;
            const double right_duration = profile_right.t_sum.back() + profile_right.brake.duration + profile_right.accel.duration;
            if (left_duration < right_duration) {
                left = left_duration;
                right = right_duration;
                profile = profile_right;
            } else {
                left = right_duration;
                right = left_duration;
                profile = profile_left;
            }
        };
    };

    void set_min_profile(const Profile& profile) {
        p_min = profile;
        t_min = p_min.t_sum.back() + p_min.brake.duration + p_min.accel.duration;
        a = nullopt;
        b = nullopt;
    }

    Profile p_min; // Save min profile so that it doesn't need to be recalculated in Step2
    double t_min; // [s]

    // Max. 2 intervals can be blocked: called a and b with corresponding profiles, order does not matter
    Optional<Interval> a, b;

    template<size_t N, bool numerical_robust = true>
    static bool calculate_block(Block& block, std::array<Profile, N>& valid_profiles, size_t valid_profile_counter) {
        // std::cout << "---\n " << valid_profile_counter << std::endl;
        // for (size_t i = 0; i < valid_profile_counter; ++i) {
        //     std::cout << valid_profiles[i].t_sum.back() << " " << valid_profiles[i].to_string() << std::endl;
        // }

        if (valid_profile_counter == 1) {
            block.set_min_profile(valid_profiles[0]);
            return true;

        } else if (valid_profile_counter == 2) {
            if (std::abs(valid_profiles[0].t_sum.back() - valid_profiles[1].t_sum.back()) < 8 * std::numeric_limits<double>::epsilon()) {
                block.set_min_profile(valid_profiles[0]);
                return true;
            }

            if (numerical_robust) {
                const size_t idx_min = (valid_profiles[0].t_sum.back() < valid_profiles[1].t_sum.back()) ? 0 : 1;
                const size_t idx_else_1 = (idx_min + 1) % 2;

                block.set_min_profile(valid_profiles[idx_min]);
                block.a = Interval(valid_profiles[idx_min], valid_profiles[idx_else_1]);
                return true;
            }

        // Only happens due to numerical issues
        } else if (valid_profile_counter == 4) {
            // Find "identical" profiles
            if (std::abs(valid_profiles[0].t_sum.back() - valid_profiles[1].t_sum.back()) < 32 * std::numeric_limits<double>::epsilon() && valid_profiles[0].direction != valid_profiles[1].direction) {
                remove_profile<N>(valid_profiles, valid_profile_counter, 1);
            } else if (std::abs(valid_profiles[2].t_sum.back() - valid_profiles[3].t_sum.back()) < 256 * std::numeric_limits<double>::epsilon() && valid_profiles[2].direction != valid_profiles[3].direction) {
                remove_profile<N>(valid_profiles, valid_profile_counter, 3);
            } else if (std::abs(valid_profiles[0].t_sum.back() - valid_profiles[3].t_sum.back()) < 256 * std::numeric_limits<double>::epsilon() && valid_profiles[0].direction != valid_profiles[3].direction) {
                remove_profile<N>(valid_profiles, valid_profile_counter, 3);
            } else {
                return false;
            }

        } else if (valid_profile_counter % 2 == 0) {
            return false;
        }

        // Find index of fastest profile
        const auto idx_min_it = std::min_element(valid_profiles.cbegin(), valid_profiles.cbegin() + valid_profile_counter, [](const Profile& a, const Profile& b) { return a.t_sum.back() < b.t_sum.back(); });
        const size_t idx_min = std::distance(valid_profiles.cbegin(), idx_min_it);

        block.set_min_profile(valid_profiles[idx_min]);

        if (valid_profile_counter == 3) {
            const size_t idx_else_1 = (idx_min + 1) % 3;
            const size_t idx_else_2 = (idx_min + 2) % 3;

            block.a = Interval(valid_profiles[idx_else_1], valid_profiles[idx_else_2]);
            return true;

        } else if (valid_profile_counter == 5) {
            const size_t idx_else_1 = (idx_min + 1) % 5;
            const size_t idx_else_2 = (idx_min + 2) % 5;
            const size_t idx_else_3 = (idx_min + 3) % 5;
            const size_t idx_else_4 = (idx_min + 4) % 5;

            if (valid_profiles[idx_else_1].direction == valid_profiles[idx_else_2].direction) {
                block.a = Interval(valid_profiles[idx_else_1], valid_profiles[idx_else_2]);
                block.b = Interval(valid_profiles[idx_else_3], valid_profiles[idx_else_4]);
            } else {
                block.a = Interval(valid_profiles[idx_else_1], valid_profiles[idx_else_4]);
                block.b = Interval(valid_profiles[idx_else_2], valid_profiles[idx_else_3]);
            }
            return true;
        }

        return false;
    }

    inline bool is_blocked(double t) const {
        return (t < t_min) || (a && a->left < t && t < a->right) || (b && b->left < t && t < b->right);
    }

    const Profile& get_profile(double t) const {
        if (b && t >= b->right) {
            return b->profile;
        }
        if (a && t >= a->right) {
            return a->profile;
        }
        return p_min;
    }

    std::string to_string() const {
        std::string result = "[" + std::to_string(t_min) + " ";
        if (a) {
            result += std::to_string(a->left) + "] [" + std::to_string(a->right) + " ";
        }
        if (b) {
            result += std::to_string(b->left) + "] [" + std::to_string(b->right) + " ";
        }
        return result + "-";
    }
};

} // namespace ruckig
