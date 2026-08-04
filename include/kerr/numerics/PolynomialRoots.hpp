#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace kerr::numerics {

using Polynomial = std::array<long double, 9>;
struct PolynomialRoots final {
    std::array<double, 8> values{};
    std::size_t count{};
    bool valid{true};
};

[[nodiscard]] inline long double polynomial_value(const Polynomial &p, std::size_t degree,
                                                  long double x) noexcept {
    long double result = p[degree];
    while (degree > 0)
        result = p[--degree] + x * result;
    return result;
}

// Isolate real roots on [0,1] by recursively finding derivative roots. Every
// resulting open interval is monotone. This catches multiple crossings inside
// one accepted dense step, unlike endpoint-only sign checks.
[[nodiscard]] inline PolynomialRoots
unit_interval_roots(Polynomial p, std::size_t degree, double theta_tolerance = 1.0e-12) noexcept {
    PolynomialRoots result{};
    if (degree > 8 || !std::isfinite(theta_tolerance) || theta_tolerance <= 0.0) {
        result.valid = false;
        return result;
    }
    while (degree > 0 && p[degree] == 0.0L)
        --degree;
    long double scale{};
    for (std::size_t i = 0; i <= degree; ++i) {
        if (!std::isfinite(p[i])) {
            result.valid = false;
            return result;
        }
        scale = std::max(scale, std::abs(p[i]));
    }
    if (degree == 0 || scale == 0.0L)
        return result; // Includes identically zero.
    for (std::size_t i = 0; i <= degree; ++i)
        p[i] /= scale;
    if (degree == 1) {
        const long double root = -p[0] / p[1];
        if (root >= 0.0L && root <= 1.0L)
            result.values[result.count++] = static_cast<double>(root);
        return result;
    }
    Polynomial derivative{};
    for (std::size_t i = 1; i <= degree; ++i)
        derivative[i - 1] = static_cast<long double>(i) * p[i];
    const auto extrema = unit_interval_roots(derivative, degree - 1, theta_tolerance * 0.25);
    if (!extrema.valid) {
        result.valid = false;
        return result;
    }
    std::array<double, 10> knots{};
    std::size_t knot_count = 1;
    for (std::size_t i = 0; i < extrema.count; ++i)
        if (extrema.values[i] > 0.0 && extrema.values[i] < 1.0)
            knots[knot_count++] = extrema.values[i];
    knots[knot_count++] = 1.0;
    auto append = [&](double root) {
        if (result.count > 0 && std::abs(root - result.values[result.count - 1]) <= theta_tolerance)
            return;
        if (result.count < result.values.size())
            result.values[result.count++] = root;
        else
            result.valid = false;
    };
    for (std::size_t interval = 1; interval < knot_count; ++interval) {
        double left = knots[interval - 1], right = knots[interval];
        long double fl = polynomial_value(p, degree, static_cast<long double>(left));
        const long double fr = polynomial_value(p, degree, static_cast<long double>(right));
        if (fl == 0.0L)
            append(left);
        if ((fl < 0.0L && fr > 0.0L) || (fl > 0.0L && fr < 0.0L)) {
            for (std::size_t iteration = 0; iteration < 80 && right - left > theta_tolerance;
                 ++iteration) {
                const double middle = left + 0.5 * (right - left);
                if (middle == left || middle == right)
                    break;
                const long double fm =
                    polynomial_value(p, degree, static_cast<long double>(middle));
                if (fm == 0.0L) {
                    left = middle;
                    right = middle;
                    break;
                }
                if ((fl < 0.0L) == (fm < 0.0L)) {
                    left = middle;
                    fl = fm;
                } else
                    right = middle;
            }
            append(left + 0.5 * (right - left));
        }
        if (interval + 1 == knot_count && fr == 0.0L)
            append(right);
    }
    return result;
}

} // namespace kerr::numerics
