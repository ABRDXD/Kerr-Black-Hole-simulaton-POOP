#pragma once

#include "kerr/numerics/Integrators.hpp"
#include "kerr/numerics/PolynomialRoots.hpp"
#include "kerr/physics/RayTracer.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>

namespace kerr::physics::detail {
template <typename Options> void validate_options(const Options &o, double horizon, double limit) {
    for (double value : {o.relative_tolerance, o.position_tolerance, o.momentum_tolerance,
                         o.initial_step, o.minimum_step, o.maximum_step, limit,
                         o.event_affine_tolerance, o.constraint_limit, o.invariant_drift_limit})
        if (!std::isfinite(value) || value <= 0.0)
            throw std::invalid_argument{
                "Ray tolerances, steps and limits must be positive and finite"};
    if (o.minimum_step > o.initial_step || o.initial_step > o.maximum_step ||
        o.maximum_attempts == 0 || o.maximum_rejections == 0)
        throw std::invalid_argument{"Inconsistent ray step or work limits"};
    if (o.stop_at_escape && (!std::isfinite(o.escape_radius) || o.escape_radius <= horizon))
        throw std::invalid_argument{"Escape radius must be finite and outside the horizon"};
    if (o.stop_at_disk &&
        (!std::isfinite(o.disk_inner_radius) || !std::isfinite(o.disk_outer_radius) ||
         o.disk_inner_radius <= horizon || o.disk_outer_radius <= o.disk_inner_radius))
        throw std::invalid_argument{"Disk annulus must lie outside the horizon"};
}

using DensePolynomials = std::array<numerics::Polynomial, 3>;
template <std::size_t N>
DensePolynomials position_polynomials(const numerics::State<N> &state,
                                      const numerics::Step<N> &step, double h) {
    DensePolynomials result{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        result[axis][0] = static_cast<long double>(state[axis + 1]);
        for (std::size_t power = 0; power < 4; ++power)
            result[axis][power + 1] =
                static_cast<long double>(h) * static_cast<long double>(step.dense[power][axis + 1]);
    }
    return result;
}

inline numerics::Polynomial radial_surface(const DensePolynomials &position, double radius,
                                           double spin) {
    numerics::Polynomial result{};
    result[0] = -1.0L;
    const long double r = static_cast<long double>(radius), a = static_cast<long double>(spin);
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const long double denominator = axis == 2 ? r * r : r * r + a * a;
        for (std::size_t j = 0; j <= 4; ++j)
            for (std::size_t k = 0; k <= 4; ++k)
                result[j + k] += position[axis][j] * position[axis][k] / denominator;
    }
    return result;
}

struct Event {
    bool valid{true};
    double theta{2.0};
    RayTermination reason{RayTermination::affine_limit};
};

template <std::size_t N, typename Options>
Event find_event(const KerrParameters &parameters, const numerics::State<N> &start,
                 const numerics::Step<N> &step, double h, const Options &o,
                 std::optional<double> coordinate_time_limit = {}) {
    Event event{};
    const auto position = position_polynomials(start, step, h);
    const double tolerance = std::clamp(o.event_affine_tolerance / h, 2.0e-15, 1.0e-5);
    auto consider = [&](const numerics::Polynomial &polynomial, std::size_t degree,
                        RayTermination reason, int direction) {
        // Clock synchronization needs nearly machine-precision location on the
        // dense curve, independent of the looser physical-event time tolerance.
        // A boosted observer can reach a nearby clock target at a small step
        // fraction. Scale its root tolerance by the initial linear estimate so
        // a fixed theta error cannot overwhelm that short clock interval.
        const double clock_fraction = reason == RayTermination::coordinate_time_limit
            ? static_cast<double>(std::min(1.0L, std::abs(polynomial[0] / polynomial[1])))
            : 1.0;
        const double root_tolerance = reason == RayTermination::coordinate_time_limit
            ? std::max(std::numeric_limits<double>::denorm_min(),
                       4.0 * std::numeric_limits<double>::epsilon() * clock_fraction)
            : tolerance;
        const auto roots = numerics::unit_interval_roots(polynomial, degree, root_tolerance);
        if (!roots.valid) {
            event.valid = false;
            return;
        }
        numerics::Polynomial derivative{};
        for (std::size_t i = 1; i <= degree; ++i)
            derivative[i - 1] = static_cast<long double>(i) * polynomial[i];
        for (std::size_t i = 0; i < roots.count; ++i) {
            const double theta = roots.values[i];
            if (theta <= 0.0 || theta >= event.theta)
                continue;
            const long double slope =
                numerics::polynomial_value(derivative, degree - 1, static_cast<long double>(theta));
            if (slope == 0.0L || (direction < 0 && slope >= 0.0L) ||
                (direction > 0 && slope <= 0.0L))
                continue;
            if (reason == RayTermination::disk) {
                const auto state = numerics::dense_state(start, step, h, theta);
                const auto radius = kerr_schild_radius(parameters, {state[1], state[2], state[3]});
                if (!radius.valid()) {
                    event.valid = false;
                    return;
                }
                if (radius.radius < o.disk_inner_radius || radius.radius > o.disk_outer_radius)
                    continue;
            }
            event.theta = theta;
            event.reason = reason;
        }
    };
    if (o.stop_at_horizon)
        consider(radial_surface(position, horizon_radii(parameters).outer, parameters.spin()), 8,
                 RayTermination::horizon, -1);
    if (o.stop_at_escape)
        consider(radial_surface(position, o.escape_radius, parameters.spin()), 8,
                 RayTermination::escape, 1);
    if (o.stop_at_disk)
        consider(position[2], 4, RayTermination::disk, 0);
    if (coordinate_time_limit) {
        numerics::Polynomial clock{};
        clock[0] =
            static_cast<long double>(start[0]) - static_cast<long double>(*coordinate_time_limit);
        for (std::size_t power = 0; power < 4; ++power)
            clock[power + 1] =
                static_cast<long double>(h) * static_cast<long double>(step.dense[power][0]);
        consider(clock, 4, RayTermination::coordinate_time_limit, 1);
    }
    return event;
}

inline double drift(double current, double initial) {
    return std::abs(current - initial) / std::max(1.0, std::abs(initial));
}
} // namespace kerr::physics::detail
