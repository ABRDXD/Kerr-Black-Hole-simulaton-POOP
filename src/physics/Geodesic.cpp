#include "kerr/physics/Geodesic.hpp"
#include "kerr/math/Dual.hpp"

#include <algorithm>
#include <cmath>

namespace kerr::physics {
namespace {
bool finite(const GeodesicState &state) noexcept {
    for (double component : state)
        if (!std::isfinite(component))
            return false;
    return true;
}
} // namespace

MetricDerivatives evaluate_metric_derivatives(const KerrParameters &parameters,
                                              const math::Vector3d position) noexcept {
    MetricDerivatives result{};
    const auto radius = kerr_schild_radius(parameters, position);
    if (!radius.valid()) {
        result.status = radius.status;
        return result;
    }

    using D = math::Dual<3>;
    // Scale is frozen at this evaluation point. The expression is homogeneous,
    // so differentiating at any fixed positive scale gives the same partials.
    const double scale = std::max({std::abs(position.x), std::abs(position.y), std::abs(position.z),
                                   std::abs(parameters.spin())});
    const D divisor{static_cast<long double>(scale)};
    const D x = D::variable(static_cast<long double>(position.x), 0) / divisor;
    const D y = D::variable(static_cast<long double>(position.y), 1) / divisor;
    const D z = D::variable(static_cast<long double>(position.z), 2) / divisor;
    const D a = D{static_cast<long double>(parameters.spin())} / divisor;
    const D q = x * x + y * y + z * z - a * a;
    const D discriminant = math::sqrt(q * q + D{4.0L} * a * a * z * z);
    const D r2 = q.value >= 0.0L ? D{0.5L} * (q + discriminant)
                                 : D{2.0L} * a * a * z * z / (discriminant - q);
    const D r = math::sqrt(r2);
    const D coupled = a * z / r2;
    const D profile = (D{2.0L * static_cast<long double>(parameters.mass())} / divisor) /
                      (r * (D{1.0L} + coupled * coupled));
    const std::array<D, 4> raised_null{D{-1.0L}, (r * x + a * y) / (r2 + a * a),
                                       (r * y - a * x) / (r2 + a * a), z / r};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t col = row; col < 4; ++col) {
            const D update = D{-1.0L} * profile * raised_null[row] * raised_null[col];
            for (std::size_t axis = 0; axis < 3; ++axis) {
                const double partial = static_cast<double>(update.gradient[axis]);
                if (!std::isfinite(partial)) {
                    result = {};
                    result.status = GeometryStatus::singular_or_degenerate_chart;
                    return result;
                }
                result.spatial[axis](row, col) = partial;
                result.spatial[axis](col, row) = partial;
            }
        }
    }
    result.status = GeometryStatus::valid;
    return result;
}

GeometryStatus geodesic_rhs(const KerrParameters &parameters, const GeodesicState &state,
                            GeodesicState &derivative) noexcept {
    derivative = {};
    if (!finite(state))
        return GeometryStatus::non_finite_input;
    const math::Vector3d position{state[1], state[2], state[3]};
    const auto metric = evaluate_metric(parameters, position);
    if (!metric.valid())
        return metric.status;
    const auto partials = evaluate_metric_derivatives(parameters, position);
    if (!partials.valid())
        return partials.status;
    GeodesicState candidate{};
    for (std::size_t row = 0; row < 4; ++row) {
        long double velocity{};
        for (std::size_t col = 0; col < 4; ++col)
            velocity += static_cast<long double>(metric.contravariant(row, col)) *
                        static_cast<long double>(state[col + 4]);
        candidate[row] = static_cast<double>(velocity);
    }
    // Stationarity implies dp_t/dlambda = 0 exactly.
    for (std::size_t axis = 0; axis < 3; ++axis) {
        long double force{};
        for (std::size_t row = 0; row < 4; ++row)
            for (std::size_t col = 0; col < 4; ++col)
                force -= 0.5L * static_cast<long double>(partials.spatial[axis](row, col)) *
                         static_cast<long double>(state[row + 4]) *
                         static_cast<long double>(state[col + 4]);
        candidate[axis + 5] = static_cast<double>(force);
    }
    if (!finite(candidate))
        return GeometryStatus::non_finite_input;
    derivative = candidate;
    return GeometryStatus::valid;
}

namespace {
GeodesicDiagnostics diagnose_geodesic(const KerrParameters &parameters, const GeodesicState &state,
                                      const long double rest_mass_squared) noexcept {
    GeodesicDiagnostics result{};
    if (!finite(state))
        return result;
    const auto metric = evaluate_metric(parameters, {state[1], state[2], state[3]});
    if (!metric.valid()) {
        result.status = metric.status;
        return result;
    }
    long double contraction{}, magnitude{};
    for (std::size_t row = 0; row < 4; ++row) {
        for (std::size_t col = 0; col < 4; ++col) {
            const long double term = static_cast<long double>(metric.contravariant(row, col)) *
                                     static_cast<long double>(state[row + 4]) *
                                     static_cast<long double>(state[col + 4]);
            contraction += term;
            magnitude += std::abs(term);
        }
    }
    if (!(magnitude > 0.0L))
        return result; // A zero four-momentum is not a ray.
    const long double x = static_cast<long double>(state[1]),
                      y = static_cast<long double>(state[2]),
                      z = static_cast<long double>(state[3]);
    const long double px = static_cast<long double>(state[5]),
                      py = static_cast<long double>(state[6]),
                      pz = static_cast<long double>(state[7]),
                      energy = -static_cast<long double>(state[4]);
    const long double r = static_cast<long double>(metric.radius),
                      a = static_cast<long double>(parameters.spin());
    const long double ratio = a / r;
    const long double shape = 1.0L + ratio * ratio;
    // Algebraic elimination of sin(theta) denominators from the BL expression
    // Q=p_theta^2+cos(theta)^2[Lz^2/sin(theta)^2+a^2(mu^2-E^2)].
    const long double carter =
        z * z * shape * (px * px + py * py) - 2.0L * z * (x * px + y * py) * pz +
        (x * x + y * y) / shape * pz * pz - ratio * ratio * z * z * energy * energy +
        rest_mass_squared * ratio * ratio * z * z;
    result.hamiltonian = static_cast<double>(0.5L * contraction);
    result.normalized_constraint = static_cast<double>(std::abs(contraction + rest_mass_squared) /
                                                       (magnitude + rest_mass_squared));
    result.energy = static_cast<double>(energy);
    result.angular_momentum = static_cast<double>(x * py - y * px);
    result.carter = static_cast<double>(carter);
    result.radius = metric.radius;
    long double time_rate{};
    for (std::size_t col = 0; col < 4; ++col)
        time_rate += static_cast<long double>(metric.contravariant(0, col)) *
                     static_cast<long double>(state[col + 4]);
    result.coordinate_time_rate = static_cast<double>(time_rate);
    if (!std::isfinite(result.hamiltonian) || !std::isfinite(result.normalized_constraint) ||
        !std::isfinite(result.angular_momentum) || !std::isfinite(result.carter) ||
        !std::isfinite(result.coordinate_time_rate))
        return {};
    result.status = GeometryStatus::valid;
    return result;
}
} // namespace

GeodesicDiagnostics diagnose_null_geodesic(const KerrParameters &parameters,
                                           const GeodesicState &state) noexcept {
    return diagnose_geodesic(parameters, state, 0.0L);
}

GeodesicDiagnostics diagnose_timelike_geodesic(const KerrParameters &parameters,
                                               const GeodesicState &state) noexcept {
    return diagnose_geodesic(parameters, state, 1.0L);
}

NullStateEvaluation make_null_state(const KerrParameters &parameters, const math::Vector3d position,
                                    const math::Vector3d spatial_momentum,
                                    const bool past_directed) noexcept {
    NullStateEvaluation result{};
    const auto metric = evaluate_metric(parameters, position);
    if (!metric.valid()) {
        result.status = metric.status;
        return result;
    }
    const std::array<long double, 3> p{static_cast<long double>(spatial_momentum.x),
                                       static_cast<long double>(spatial_momentum.y),
                                       static_cast<long double>(spatial_momentum.z)};
    for (long double component : p)
        if (!std::isfinite(component))
            return result;
    const long double norm = std::hypot(p[0], p[1], p[2]);
    if (!(norm > 0.0L))
        return result;
    const long double f = static_cast<long double>(metric.profile);
    long double along{};
    for (std::size_t i = 0; i < 3; ++i)
        along += static_cast<long double>(metric.null_covector[i + 1]) * p[i];
    // g^tt p_t^2 + 2 g^ti p_i p_t + g^ij p_i p_j=0.
    // The negative quadratic root gives dt/dlambda>0 in this ingoing chart.
    const long double radicand = (1.0L + f) * norm * norm - f * along * along;
    if (!(radicand > 0.0L) || !std::isfinite(radicand))
        return result;
    long double pt = (f * along - std::sqrt(radicand)) / (1.0L + f);
    // Stable alternative when the numerator suffers cancellation.
    if (f * along > 0.0L)
        pt = -(norm * norm - f * along * along) / (f * along + std::sqrt(radicand));
    const double orientation = past_directed ? -1.0 : 1.0;
    result.state = {0.0,
                    position.x,
                    position.y,
                    position.z,
                    orientation * static_cast<double>(pt),
                    orientation * spatial_momentum.x,
                    orientation * spatial_momentum.y,
                    orientation * spatial_momentum.z};
    if (!finite(result.state))
        return {};
    result.status = GeometryStatus::valid;
    return result;
}

TimelikeStateEvaluation make_timelike_state(const KerrParameters &parameters,
                                            const math::Vector3d position,
                                            const math::Vector4d four_velocity,
                                            const double coordinate_time) noexcept {
    TimelikeStateEvaluation result{};
    if (!std::isfinite(coordinate_time))
        return result;
    for (double component : four_velocity)
        if (!std::isfinite(component))
            return result;
    const auto metric = evaluate_metric(parameters, position);
    if (!metric.valid()) {
        if (metric.status == GeometryStatus::singular_or_degenerate_chart)
            result.status = TimelikeStateStatus::singular_geometry;
        return result;
    }
    if (four_velocity[0] <= 0.0) {
        result.status = TimelikeStateStatus::not_future_directed;
        return result;
    }
    long double norm{};
    GeodesicState candidate{coordinate_time, position.x, position.y, position.z};
    for (std::size_t row = 0; row < 4; ++row) {
        long double momentum{};
        for (std::size_t col = 0; col < 4; ++col) {
            momentum += static_cast<long double>(metric.covariant(row, col)) *
                        static_cast<long double>(four_velocity[col]);
            norm += static_cast<long double>(metric.covariant(row, col)) *
                    static_cast<long double>(four_velocity[row]) *
                    static_cast<long double>(four_velocity[col]);
        }
        candidate[row + 4] = static_cast<double>(momentum);
    }
    if (!std::isfinite(norm) || !finite(candidate))
        return result;
    if (norm >= 0.0L) {
        result.status = TimelikeStateStatus::not_timelike;
        return result;
    }
    if (std::abs(norm + 1.0L) > 1.0e-10L) {
        result.status = TimelikeStateStatus::not_normalized;
        return result;
    }
    result.state = candidate;
    result.status = TimelikeStateStatus::valid;
    return result;
}

} // namespace kerr::physics

// Carter constant of motion Q

// Guard against polar axis theta = 0 singularity

// Horizon crossing termination check

// Escape radius condition

// refactor: polish Geodesic.cpp implementation details

// refactor: polish Geodesic.cpp implementation details

// refactor: polish Geodesic.cpp implementation details

// refactor: polish Geodesic.cpp implementation details

// refactor: polish Geodesic.cpp implementation details

// refactor: polish Geodesic.cpp implementation details

// refactor: polish Geodesic.cpp implementation details
