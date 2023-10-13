#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <tuple>
#include <type_traits>

namespace ruckig {
 
template<class Vector>
std::string join(const Vector& array) {
    std::ostringstream ss;
    for (size_t i = 0; i < array.size(); ++i) {
        if (i) ss << ", ";
        ss << std::setprecision(16) << array[i];
    }
    return ss.str();
}

//! Integrate with constant jerk for duration t. Returns new position, new velocity, and new acceleration.
static inline std::tuple<double, double, double> integrate(double t, double p0, double v0, double a0, double j) {
    return std::make_tuple(
        p0 + t * (v0 + t * (a0 / 2 + t * j / 6)),
        v0 + t * (a0 + t * j / 2),
        a0 + t * j
    );
}

} // namespace ruckig
