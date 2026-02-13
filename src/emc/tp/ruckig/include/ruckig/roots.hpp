#pragma once

#include <array>
#include <algorithm>
#include <cfloat>
#include <cmath>


namespace ruckig {

template<typename T>
inline T pow2(T v) {
    return v * v;
}


namespace roots {

// Use own set class on stack for real-time capability
template<typename T, size_t N>
class Set {
protected:
    using Container = typename std::array<T, N>;
    using iterator = typename Container::iterator;

    Container data;
    size_t size {0};

public:
    // Sort when accessing the elements (range is [begin, begin+size), size <= N; pragma silences false positive from inlined std::sort)
    const iterator begin() {
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
        std::sort(data.begin(), data.begin() + size);
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
        return data.begin();
    }

    const iterator end() {
        return data.begin() + size;
    }

    void insert(T value) {
        data[size] = value;
        ++size;
    }
};


// Set that only inserts positive values
template<typename T, size_t N>
class PositiveSet: public Set<T, N> {
public:
    void insert(T value) {
        if (value >= 0) {
            Set<T, N>::insert(value);
        }
    }
};


//! Calculate all roots of a*x^3 + b*x^2 + c*x + d = 0
inline PositiveSet<double, 3> solve_cubic(double a, double b, double c, double d) {
    PositiveSet<double, 3> roots;

    if (std::abs(d) < DBL_EPSILON) {
        // First solution is x = 0
        roots.insert(0.0);

        // Converting to a quadratic equation
        d = c;
        c = b;
        b = a;
        a = 0.0;
    }

    if (std::abs(a) < DBL_EPSILON) {
        if (std::abs(b) < DBL_EPSILON) {
            // Linear equation
            if (std::abs(c) > DBL_EPSILON) {
                roots.insert(-d / c);
            }

        } else {
            // Quadratic equation
            const double discriminant = c * c - 4 * b * d;
            if (discriminant >= 0) {
                const double inv2b = 1.0 / (2 * b);
                const double y = std::sqrt(discriminant);
                roots.insert((-c + y) * inv2b);
                roots.insert((-c - y) * inv2b);
            }
        }

    } else {
        // Cubic equation
        const double inva = 1.0 / a;
        const double invaa = inva * inva;
        const double bb = b * b;
        const double bover3a = b * inva / 3;
        const double p = (a * c - bb / 3) * invaa;
        const double halfq = (2 * bb * b - 9 * a * b * c + 27 * a * a * d) / 54 * invaa * inva;
        const double yy = p * p * p / 27 + halfq * halfq;

        constexpr double cos120 = -0.50;
        constexpr double sin120 = 0.866025403784438646764;

        if (yy > DBL_EPSILON) {
            // Sqrt is positive: one real solution
            const double y = std::sqrt(yy);
            const double uuu = -halfq + y;
            const double vvv = -halfq - y;
            const double www = std::abs(uuu) > std::abs(vvv) ? uuu : vvv;
            const double w = std::cbrt(www);
            roots.insert(w - p / (3 * w) - bover3a);
        } else if (yy < -DBL_EPSILON) {
            // Sqrt is negative: three real solutions
            const double x = -halfq;
            const double y = std::sqrt(-yy);
            double theta;
            double r;

            // Convert to polar form
            if (std::abs(x) > DBL_EPSILON) {
                theta = (x > 0.0) ? std::atan(y / x) : (std::atan(y / x) + M_PI);
                r = std::sqrt(x * x - yy);
            } else {
                // Vertical line
                theta = M_PI / 2;
                r = y;
            }
            // Calculate cube root
            theta /= 3;
            r = 2 * std::cbrt(r);
            // Convert to complex coordinate
            const double ux = std::cos(theta) * r;
            const double uyi = std::sin(theta) * r;

            roots.insert(ux - bover3a);
            roots.insert(ux * cos120 - uyi * sin120 - bover3a);
            roots.insert(ux * cos120 + uyi * sin120 - bover3a);
        } else {
            // Sqrt is zero: two real solutions
            const double www = -halfq;
            const double w = 2 * std::cbrt(www);

            roots.insert(w - bover3a);
            roots.insert(w * cos120 - bover3a);
        }
    }
    return roots;
}

// Solve resolvent eqaution of corresponding Quartic equation
// The input x must be of length 3
// Number of zeros are returned
inline int solve_resolvent(std::array<double, 3>& x, double a, double b, double c) {
    constexpr double cos120 = -0.50;
    constexpr double sin120 = 0.866025403784438646764;

    a /= 3;
    const double a2 = a * a;
    double q = a2 - b / 3;
    const double r = (a * (2 * a2 - b) + c) / 2;
    const double r2 = r * r;
    const double q3 = q * q * q;

    if (r2 < q3) {
        const double qsqrt = std::sqrt(q);
        const double t = std::min(std::max(r / (q * qsqrt), -1.0), 1.0);
        q = -2 * qsqrt;

        const double theta = std::acos(t) / 3;
        const double ux = std::cos(theta) * q;
        const double uyi = std::sin(theta) * q;
        x[0] = ux - a;
        x[1] = ux * cos120 - uyi * sin120 - a;
        x[2] = ux * cos120 + uyi * sin120 - a;
        return 3;

    } else {
        double A = -std::cbrt(std::abs(r) + std::sqrt(r2 - q3));
        if (r < 0.0) {
            A = -A;
        }
        const double B = (0.0 == A ? 0.0 : q / A);

        x[0] = (A + B) - a;
        x[1] = -(A + B) / 2 - a;
        x[2] = std::sqrt(3) * (A - B) / 2;
        if (std::abs(x[2]) < DBL_EPSILON) {
            x[2] = x[1];
            return 2;
        }

        return 1;
    }
}

//! Calculate all roots of the monic quartic equation: x^4 + a*x^3 + b*x^2 + c*x + d = 0
inline PositiveSet<double, 4> solve_quart_monic(double a, double b, double c, double d) {
    PositiveSet<double, 4> roots;

    if (std::abs(d) < DBL_EPSILON) {
        if (std::abs(c) < DBL_EPSILON) {
            roots.insert(0.0);

            const double D = a * a - 4 * b;
            if (std::abs(D) < DBL_EPSILON) {
                roots.insert(-a / 2);
            } else if (D > 0.0) {
                const double sqrtD = std::sqrt(D);
                roots.insert((-a - sqrtD) / 2);
                roots.insert((-a + sqrtD) / 2);
            }
            return roots;
        }

        if (std::abs(a) < DBL_EPSILON && std::abs(b) < DBL_EPSILON) {
            roots.insert(0.0);
            roots.insert(-std::cbrt(c));
            return roots;
        }
    }

    const double a3 = -b;
    const double b3 = a * c - 4 * d;
    const double c3 = -a * a * d - c * c + 4 * b * d;

    std::array<double, 3> x3;
    const int number_zeroes = solve_resolvent(x3, a3, b3, c3);

    double y = x3[0];
    // Choosing Y with maximal absolute value.
    if (number_zeroes != 1) {
        if (std::abs(x3[1]) > std::abs(y)) {
            y = x3[1];
        }
        if (std::abs(x3[2]) > std::abs(y)) {
            y = x3[2];
        }
    }

    double q1, q2, p1, p2;

    double D = y * y - 4 * d;
    if (std::abs(D) < DBL_EPSILON) {
        q1 = q2 = y / 2;
        D = a * a - 4 * (b - y);
        if (std::abs(D) < DBL_EPSILON) {
            p1 = p2 = a / 2;
        } else {
            const double sqrtD = std::sqrt(D);
            p1 = (a + sqrtD) / 2;
            p2 = (a - sqrtD) / 2;
        }
    } else {
        const double sqrtD = std::sqrt(D);
        q1 = (y + sqrtD) / 2;
        q2 = (y - sqrtD) / 2;
        p1 = (a * q1 - c) / (q1 - q2);
        p2 = (c - a * q2) / (q1 - q2);
    }

    constexpr double eps {16 * DBL_EPSILON};

    D = p1 * p1 - 4 * q1;
    if (std::abs(D) < eps) {
        roots.insert(-p1 / 2);
    } else if (D > 0.0) {
        const double sqrtD = std::sqrt(D);
        roots.insert((-p1 - sqrtD) / 2);
        roots.insert((-p1 + sqrtD) / 2);
    }

    D = p2 * p2 - 4 * q2;
    if (std::abs(D) < eps) {
        roots.insert(-p2 / 2);
    } else if (D > 0.0) {
        const double sqrtD = std::sqrt(D);
        roots.insert((-p2 - sqrtD) / 2);
        roots.insert((-p2 + sqrtD) / 2);
    }

    return roots;
}

//! Calculate the quartic equation: x^4 + b*x^3 + c*x^2 + d*x + e = 0
inline PositiveSet<double, 4> solve_quart_monic(const std::array<double, 4>& polynom) {
    return solve_quart_monic(polynom[0], polynom[1], polynom[2], polynom[3]);
}


//! Evaluate a polynomial of order N at x (specialization for N == 0)
template<size_t N>
inline typename std::enable_if<N == 0, double>::type poly_eval(const std::array<double, N>&, double) {
    return 0.0;
}

//! Evaluate a polynomial of order N at x
template<size_t N>
inline typename std::enable_if<(N > 0), double>::type poly_eval(const std::array<double, N>& p, double x) {
    double retVal = 0.0;

    if (std::abs(x) < DBL_EPSILON) {
        retVal = p[N - 1];
    } else if (x == 1.0) {
        for (int i = N - 1; i >= 0; i--) {
            retVal += p[i];
        }
    } else {
        double xn = 1.0;

        for (int i = N - 1; i >= 0; i--) {
            retVal += p[i] * xn;
            xn *= x;
        }
    }

    return retVal;
}

// Calculate the derivative poly coefficients of a given poly
template<size_t N>
inline std::array<double, N-1> poly_derivative(const std::array<double, N>& coeffs) {
    std::array<double, N-1> deriv;
    for (size_t i = 0; i < N - 1; ++i) {
        deriv[i] = (N - 1 - i) * coeffs[i];
    }
    return deriv;
}

template<size_t N>
inline std::array<double, N-1> poly_monic_derivative(const std::array<double, N>& monic_coeffs) {
    std::array<double, N-1> deriv;
    deriv[0] = 1.0;
    for (size_t i = 1; i < N - 1; ++i) {
        deriv[i] = (N - 1 - i) * monic_coeffs[i] / (N - 1);
    }
    return deriv;
}

// Safe Newton Method
constexpr double tolerance {1e-14};

// Calculate a single zero of polynom p(x) inside [lbound, ubound]
// Requirements: p(lbound)*p(ubound) < 0, lbound < ubound
template<size_t N, size_t maxIts = 128>
inline double shrink_interval(const std::array<double, N>& p, double l, double h) {
    const double fl = poly_eval(p, l);
    const double fh = poly_eval(p, h);
    if (fl == 0.0) {
        return l;
    }
    if (fh == 0.0) {
        return h;
    }
    if (fl > 0.0) {
        std::swap(l, h);
    }

    double rts = (l + h) / 2;
    double dxold = std::abs(h - l);
    double dx = dxold;
    const auto deriv = poly_derivative(p);
    double f = poly_eval(p, rts);
    double df = poly_eval(deriv, rts);
    double temp;
    for (size_t j = 0; j < maxIts; j++) {
        if ((((rts - h) * df - f) * ((rts - l) * df - f) > 0.0) || (std::abs(2 * f) > std::abs(dxold * df))) {
            dxold = dx;
            dx = (h - l) / 2;
            rts = l + dx;
            if (l == rts) {
                break;
            }
        } else {
            dxold = dx;
            dx = f / df;
            temp = rts;
            rts -= dx;
            if (temp == rts) {
                break;
            }
        }

        if (std::abs(dx) < tolerance) {
            break;
        }

        f = poly_eval(p, rts);
        df = poly_eval(deriv, rts);
        if (f < 0.0) {
            l = rts;
        } else {
            h = rts;
        }
    }

    return rts;
}

} // namespace roots

} // namespace ruckig
