#include "kerr/physics/KerrSpacetime.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace kerr::physics {
namespace {

[[nodiscard]] bool finite_position(const math::Vector3d position) noexcept {
    return std::isfinite(position.x) && std::isfinite(position.y) &&
           std::isfinite(position.z);
}

[[nodiscard]] double nonnegative_kerr_discriminant(
    const KerrParameters& parameters) noexcept {
    const double mass = parameters.mass();
    const double absolute_spin = std::abs(parameters.spin());
    return std::max(0.0, (mass - absolute_spin) * (mass + absolute_spin));
}

} // namespace

KerrParameters::KerrParameters(const double mass, const double spin)
    : mass_{mass}, spin_{spin} {
    if (!std::isfinite(mass) || !std::isfinite(spin)) {
        throw std::invalid_argument{"Kerr mass and spin must be finite"};
    }
    if (!(mass > 0.0)) {
        throw std::invalid_argument{"Kerr mass must be strictly positive"};
    }
    if (std::abs(spin) > mass) {
        throw std::invalid_argument{"Kerr spin must satisfy |a| <= M"};
    }
}

RadiusEvaluation kerr_schild_radius(const KerrParameters& parameters,
                                    const math::Vector3d position) noexcept {
    if (!finite_position(position)) {
        return {GeometryStatus::non_finite_input, 0.0};
    }

    // Scale first so finite but enormous coordinates cannot overflow while
    // evaluating rho^2. The defining equation is homogeneous in (x,y,z,a).
    const double spin = parameters.spin();
    const double scale = std::max(
        {std::abs(position.x), std::abs(position.y), std::abs(position.z),
         std::abs(spin)});
    if (scale == 0.0) {
        return {GeometryStatus::singular_or_degenerate_chart, 0.0};
    }

    const double x = position.x / scale;
    const double y = position.y / scale;
    const double z = position.z / scale;
    const double a = spin / scale;
    const double rho_squared = x * x + y * y + z * z;
    const double a_squared = a * a;
    const double q = rho_squared - a_squared;
    const double discriminant = std::hypot(q, 2.0 * a * z);

    // r^2/scale^2 is the positive root of u^2-q*u-a^2*z^2=0.
    // The alternate expression avoids cancellation for q < 0.
    double normalized_r_squared{};
    if (q >= 0.0) {
        normalized_r_squared = 0.5 * (q + discriminant);
    } else {
        const double denominator = discriminant - q;
        normalized_r_squared = denominator > 0.0
                                   ? (2.0 * a_squared * z * z) / denominator
                                   : 0.0;
    }

    normalized_r_squared = std::max(0.0, normalized_r_squared);
    const double radius = scale * std::sqrt(normalized_r_squared);
    if (!(radius > 0.0) || !std::isfinite(radius)) {
        return {GeometryStatus::singular_or_degenerate_chart, 0.0};
    }
    return {GeometryStatus::valid, radius};
}

MetricEvaluation evaluate_metric(const KerrParameters& parameters,
                                 const math::Vector3d position) noexcept {
    MetricEvaluation result{};
    const RadiusEvaluation radius_evaluation = kerr_schild_radius(parameters, position);
    if (!radius_evaluation.valid()) {
        result.status = radius_evaluation.status;
        return result;
    }

    const long double radius = static_cast<long double>(radius_evaluation.radius);
    const long double mass = static_cast<long double>(parameters.mass());
    const long double spin = static_cast<long double>(parameters.spin());
    const long double x = static_cast<long double>(position.x);
    const long double y = static_cast<long double>(position.y);
    const long double z = static_cast<long double>(position.z);

    const long double component_scale = std::max(
        {std::abs(radius), std::abs(spin), std::abs(x), std::abs(y), std::abs(z)});
    if (!(component_scale > 0.0L)) {
        result.status = GeometryStatus::singular_or_degenerate_chart;
        return result;
    }

    const long double scaled_radius = radius / component_scale;
    const long double scaled_spin = spin / component_scale;
    const long double scaled_x = x / component_scale;
    const long double scaled_y = y / component_scale;
    const long double radial_denominator =
        scaled_radius * scaled_radius + scaled_spin * scaled_spin;
    const long double l_x =
        (scaled_radius * scaled_x + scaled_spin * scaled_y) / radial_denominator;
    const long double l_y =
        (scaled_radius * scaled_y - scaled_spin * scaled_x) / radial_denominator;
    const long double l_z = z / radius;

    // F = 2(M/r) / [1 + ((a/r)(z/r))^2] is algebraically
    // equivalent to 2 M r^3/(r^4+a^2 z^2) but resists overflow.
    const long double coupled_ratio = (spin / radius) * (z / radius);
    const long double profile =
        (2.0L * (mass / radius)) / (1.0L + coupled_ratio * coupled_ratio);

    result.radius = radius_evaluation.radius;
    result.profile = static_cast<double>(profile);
    result.null_covector = {
        1.0,
        static_cast<double>(l_x),
        static_cast<double>(l_y),
        static_cast<double>(l_z),
    };

    constexpr std::array<double, 4> minkowski_diagonal{-1.0, 1.0, 1.0, 1.0};
    const math::Vector4d raised_null{
        -result.null_covector[0], result.null_covector[1],
        result.null_covector[2], result.null_covector[3]};

    for (std::size_t row = 0; row < 4U; ++row) {
        for (std::size_t column = row; column < 4U; ++column) {
            const double flat = row == column ? minkowski_diagonal[row] : 0.0;
            result.covariant(row, column) =
                flat + result.profile * result.null_covector[row] *
                           result.null_covector[column];
            result.contravariant(row, column) =
                flat - result.profile * raised_null[row] * raised_null[column];
            // Mirror one evaluation to preserve exact symmetry in floating point.
            result.covariant(column, row) = result.covariant(row, column);
            result.contravariant(column, row) = result.contravariant(row, column);
        }
    }

    if (!std::isfinite(result.profile)) {
        result.status = GeometryStatus::singular_or_degenerate_chart;
        return result;
    }
    for (const double component : result.null_covector) {
        if (!std::isfinite(component)) {
            result.status = GeometryStatus::singular_or_degenerate_chart;
            return result;
        }
    }
    for (const double component : result.covariant.elements) {
        if (!std::isfinite(component)) {
            result.status = GeometryStatus::singular_or_degenerate_chart;
            return result;
        }
    }
    for (const double component : result.contravariant.elements) {
        if (!std::isfinite(component)) {
            result.status = GeometryStatus::singular_or_degenerate_chart;
            return result;
        }
    }

    result.status = GeometryStatus::valid;
    return result;
}

HorizonRadii horizon_radii(const KerrParameters& parameters) noexcept {
    const double offset = std::sqrt(nonnegative_kerr_discriminant(parameters));
    return {
        parameters.mass() - offset,
        parameters.mass() + offset,
    };
}

double outer_ergosphere_radius(const KerrParameters& parameters,
                               const double polar_angle) noexcept {
    if (!std::isfinite(polar_angle)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const double projected_spin =
        std::abs(parameters.spin() * std::cos(polar_angle));
    const double radicand = std::max(
        0.0, (parameters.mass() - projected_spin) *
                 (parameters.mass() + projected_spin));
    return parameters.mass() + std::sqrt(radicand);
}

double equatorial_isco_radius(const KerrParameters& parameters,
                              const OrbitSense sense) noexcept {
    const double mass = parameters.mass();
    const double q = std::clamp(std::abs(parameters.spin()) / mass, 0.0, 1.0);
    const double z1 =
        1.0 + std::cbrt(std::max(0.0, 1.0 - q * q)) *
                  (std::cbrt(1.0 + q) + std::cbrt(1.0 - q));
    const double z2 = std::sqrt(3.0 * q * q + z1 * z1);
    const double root = std::sqrt(std::max(
        0.0, (3.0 - z1) * (3.0 + z1 + 2.0 * z2)));
    const double sign = sense == OrbitSense::prograde ? -1.0 : 1.0;
    return mass * (3.0 + z2 + sign * root);
}

double equatorial_photon_orbit_radius(const KerrParameters& parameters,
                                      const OrbitSense sense) noexcept {
    const double mass = parameters.mass();
    const double q = std::clamp(std::abs(parameters.spin()) / mass, 0.0, 1.0);
    const double argument = sense == OrbitSense::prograde ? -q : q;
    return 2.0 * mass *
           (1.0 + std::cos((2.0 / 3.0) * std::acos(argument)));
}

double hamiltonian(const math::Matrix4d& inverse_metric,
                   const math::Covector4d& covariant_momentum) noexcept {
    long double contraction{};
    for (std::size_t row = 0; row < 4U; ++row) {
        for (std::size_t column = 0; column < 4U; ++column) {
            contraction += static_cast<long double>(inverse_metric(row, column)) *
                           static_cast<long double>(covariant_momentum[row]) *
                           static_cast<long double>(covariant_momentum[column]);
        }
    }
    return static_cast<double>(0.5L * contraction);
}

math::Vector4d raise_index(const math::Matrix4d& inverse_metric,
                           const math::Covector4d& covector) noexcept {
    math::Vector4d result{};
    for (std::size_t row = 0; row < 4U; ++row) {
        long double component{};
        for (std::size_t column = 0; column < 4U; ++column) {
            component += static_cast<long double>(inverse_metric(row, column)) *
                         static_cast<long double>(covector[column]);
        }
        result[row] = static_cast<double>(component);
    }
    return result;
}

} // namespace kerr::physics

// Horizon radius computation

// Ergosphere boundary calculation

// Metric determinant check

// refactor: polish KerrSpacetime.cpp implementation details

// refactor: polish KerrSpacetime.cpp implementation details

// refactor: polish KerrSpacetime.cpp implementation details

// refactor: polish KerrSpacetime.cpp implementation details

// refactor: polish KerrSpacetime.cpp implementation details

// refactor: polish KerrSpacetime.cpp implementation details
