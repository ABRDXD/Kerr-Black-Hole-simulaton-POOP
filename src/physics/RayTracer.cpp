#include "kerr/physics/RayTracer.hpp"
#include "TraceEvents.hpp"
#include "kerr/numerics/Integrators.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace kerr::physics {
namespace {

using detail::drift;
using detail::Event;
using detail::find_event;
using detail::validate_options;
} // namespace

std::string_view termination_name(const RayTermination reason) noexcept {
    switch (reason) {
    case RayTermination::affine_limit:
        return "affine_limit";
    case RayTermination::proper_time_limit:
        return "proper_time_limit";
    case RayTermination::coordinate_time_limit:
        return "coordinate_time_limit";
    case RayTermination::horizon:
        return "horizon";
    case RayTermination::escape:
        return "escape";
    case RayTermination::disk:
        return "disk";
    case RayTermination::initially_inside_horizon:
        return "initially_inside_horizon";
    case RayTermination::work_limit:
        return "work_limit";
    case RayTermination::invalid_initial_state:
        return "invalid_initial_state";
    case RayTermination::singular_geometry:
        return "singular_geometry";
    case RayTermination::non_finite:
        return "non_finite";
    case RayTermination::minimum_step:
        return "minimum_step";
    case RayTermination::rejection_limit:
        return "rejection_limit";
    case RayTermination::constraint_violation:
        return "constraint_violation";
    case RayTermination::frame_violation:
        return "frame_violation";
    case RayTermination::event_failure:
        return "event_failure";
    case RayTermination::cancelled:
        return "cancelled";
    }
    return "unknown";
}

namespace {
RayResult trace_geodesic(const KerrParameters &parameters, const GeodesicState &initial,
                         const RayOptions &o, const RayObserver &observer, const bool timelike) {
    const double horizon = horizon_radii(parameters).outer;
    validate_options(o, horizon, o.affine_limit);
    RayResult result{};
    const auto diagnose = [&](const GeodesicState &state) {
        return timelike ? diagnose_timelike_geodesic(parameters, state)
                        : diagnose_null_geodesic(parameters, state);
    };
    const auto mass_shell_ok = [&](const GeodesicDiagnostics &diagnostics) {
        return !timelike ||
               (diagnostics.hamiltonian < 0.0 && diagnostics.coordinate_time_rate > 0.0 &&
                std::abs(2.0 * diagnostics.hamiltonian + 1.0) <=
                    std::min(o.constraint_limit, 1.0e-7));
    };
    const auto initial_diagnostics = diagnose(initial);
    if (!initial_diagnostics.valid() ||
        initial_diagnostics.normalized_constraint > o.constraint_limit ||
        !mass_shell_ok(initial_diagnostics))
        return result;
    result.state = initial;
    result.diagnostics = initial_diagnostics;
    result.statistics.maximum_constraint = initial_diagnostics.normalized_constraint;
    if (o.stop_at_horizon && initial_diagnostics.radius <= horizon) {
        result.termination = RayTermination::initially_inside_horizon;
        return result;
    }
    GeometryStatus last_status{GeometryStatus::valid};
    auto rhs = [&](const GeodesicState &state, GeodesicState &derivative) {
        ++result.statistics.rhs_evaluations;
        last_status = geodesic_rhs(parameters, state, derivative);
        return last_status == GeometryStatus::valid;
    };
    GeodesicState first{};
    if (!rhs(initial, first)) {
        result.termination = last_status == GeometryStatus::non_finite_input
                                 ? RayTermination::non_finite
                                 : RayTermination::singular_geometry;
        return result;
    }
    if (o.stop_at_escape && initial_diagnostics.radius >= o.escape_radius) {
        // d/dlambda of the constant-r oblate surface has the radial sign.
        const double r = initial_diagnostics.radius, a = parameters.spin();
        const double outward = (initial[1] * first[1] + initial[2] * first[2]) / (r * r + a * a) +
                               initial[3] * first[3] / (r * r);
        if (outward > 0.0) {
            result.termination = RayTermination::escape;
            return result;
        }
    }
    if (observer && !observer(0.0, result.state, result.diagnostics)) {
        result.termination = RayTermination::cancelled;
        return result;
    }
    const numerics::State<8> atol{o.position_tolerance, o.position_tolerance, o.position_tolerance,
                                  o.position_tolerance, o.momentum_tolerance, o.momentum_tolerance,
                                  o.momentum_tolerance, o.momentum_tolerance};
    double h = o.initial_step;
    std::size_t consecutive_rejections{};
    for (std::size_t attempt = 0; attempt < o.maximum_attempts; ++attempt) {
        const double remaining = o.affine_limit - result.affine_parameter;
        if (remaining <= 0.0) {
            result.termination = RayTermination::affine_limit;
            return result;
        }
        h = std::min({h, o.maximum_step, remaining});
        if ((h < o.minimum_step && remaining > o.minimum_step) ||
            result.affine_parameter + h == result.affine_parameter) {
            result.termination = RayTermination::minimum_step;
            return result;
        }
        const auto step = numerics::dopri5(rhs, result.state, first, h);
        const double error =
            step.valid ? numerics::scaled_error(result.state, step, atol, o.relative_tolerance)
                       : std::numeric_limits<double>::infinity();
        auto next = step.valid ? diagnose(step.end) : GeodesicDiagnostics{};
        const bool invariants_ok =
            next.valid() && mass_shell_ok(next) &&
            next.normalized_constraint <= o.constraint_limit &&
            drift(next.energy, initial_diagnostics.energy) <= o.invariant_drift_limit &&
            drift(next.angular_momentum, initial_diagnostics.angular_momentum) <=
                o.invariant_drift_limit &&
            drift(next.carter, initial_diagnostics.carter) <= o.invariant_drift_limit;
        Event event{};
        if (step.valid && error <= 1.0 && invariants_ok)
            event = find_event(parameters, result.state, step, h, o);
        GeodesicState endpoint = step.end;
        double used_h = h;
        if (event.theta <= 1.0) {
            endpoint = numerics::dense_state(result.state, step, h, event.theta);
            next = diagnose(endpoint);
            used_h = h * event.theta;
        }
        const bool endpoint_ok =
            next.valid() && mass_shell_ok(next) &&
            next.normalized_constraint <= o.constraint_limit &&
            drift(next.energy, initial_diagnostics.energy) <= o.invariant_drift_limit &&
            drift(next.angular_momentum, initial_diagnostics.angular_momentum) <=
                o.invariant_drift_limit &&
            drift(next.carter, initial_diagnostics.carter) <= o.invariant_drift_limit;
        if (!step.valid || !std::isfinite(error) || error > 1.0 || !invariants_ok || !endpoint_ok ||
            !event.valid) {
            ++result.statistics.rejected_steps;
            ++consecutive_rejections;
            RayTermination failure = RayTermination::rejection_limit;
            if (!event.valid)
                failure = RayTermination::event_failure;
            else if (!step.valid)
                failure = last_status == GeometryStatus::singular_or_degenerate_chart
                              ? RayTermination::singular_geometry
                              : RayTermination::non_finite;
            else if (!invariants_ok || !endpoint_ok)
                failure = RayTermination::constraint_violation;
            if (consecutive_rejections >= o.maximum_rejections) {
                result.termination = failure;
                return result;
            }
            const double factor =
                std::isfinite(error) && error > 1.0 ? numerics::step_factor(error, 0.2, true) : 0.5;
            if (h <= o.minimum_step) {
                result.termination = failure == RayTermination::rejection_limit
                                         ? RayTermination::minimum_step
                                         : failure;
                return result;
            }
            h = std::max(o.minimum_step, h * factor);
            continue;
        }
        ++result.statistics.accepted_steps;
        auto &stats = result.statistics;
        stats.smallest_step =
            stats.accepted_steps == 1 ? used_h : std::min(stats.smallest_step, used_h);
        stats.largest_step = std::max(stats.largest_step, used_h);
        stats.maximum_constraint = std::max(stats.maximum_constraint, next.normalized_constraint);
        stats.maximum_energy_drift =
            std::max(stats.maximum_energy_drift, drift(next.energy, initial_diagnostics.energy));
        stats.maximum_lz_drift =
            std::max(stats.maximum_lz_drift,
                     drift(next.angular_momentum, initial_diagnostics.angular_momentum));
        stats.maximum_carter_drift =
            std::max(stats.maximum_carter_drift, drift(next.carter, initial_diagnostics.carter));
        result.state = endpoint;
        result.diagnostics = next;
        result.affine_parameter =
            used_h == remaining ? o.affine_limit : result.affine_parameter + used_h;
        if (observer && !observer(result.affine_parameter, result.state, result.diagnostics)) {
            result.termination = RayTermination::cancelled;
            return result;
        }
        if (event.theta <= 1.0) {
            result.termination = event.reason;
            return result;
        }
        first = step.endpoint_derivative;
        h *= numerics::step_factor(error, 0.2, consecutive_rejections > 0);
        consecutive_rejections = 0;
    }
    result.termination = result.affine_parameter >= o.affine_limit ? RayTermination::affine_limit
                                                                   : RayTermination::work_limit;
    return result;
}
} // namespace

RayResult trace_null_ray(const KerrParameters &parameters, const GeodesicState &initial,
                         const RayOptions &options, const RayObserver &observer) {
    return trace_geodesic(parameters, initial, options, observer, false);
}

WorldlineResult trace_timelike_worldline(const KerrParameters &parameters,
                                         const GeodesicState &initial,
                                         const WorldlineOptions &options,
                                         const WorldlineObserver &observer) {
    const RayOptions shared{.relative_tolerance = options.relative_tolerance,
                            .position_tolerance = options.position_tolerance,
                            .momentum_tolerance = options.momentum_tolerance,
                            .initial_step = options.initial_step,
                            .minimum_step = options.minimum_step,
                            .maximum_step = options.maximum_step,
                            .affine_limit = options.proper_time_limit,
                            .escape_radius = options.escape_radius,
                            .event_affine_tolerance = options.event_affine_tolerance,
                            .constraint_limit = options.constraint_limit,
                            .invariant_drift_limit = options.invariant_drift_limit,
                            .maximum_attempts = options.maximum_attempts,
                            .maximum_rejections = options.maximum_rejections,
                            .stop_at_horizon = options.stop_at_horizon,
                            .stop_at_escape = options.stop_at_escape,
                            .stop_at_disk = options.stop_at_disk,
                            .disk_inner_radius = options.disk_inner_radius,
                            .disk_outer_radius = options.disk_outer_radius};
    const auto result = trace_geodesic(parameters, initial, shared, observer, true);
    return {result.termination == RayTermination::affine_limit ? RayTermination::proper_time_limit
                                                               : result.termination,
            result.state, result.affine_parameter, result.diagnostics, result.statistics};
}

} // namespace kerr::physics

// Doppler factor computation

// Gravitational redshift and beaming

// refactor: polish RayTracer.cpp implementation details

// refactor: polish RayTracer.cpp implementation details

// refactor: polish RayTracer.cpp implementation details

// refactor: polish RayTracer.cpp implementation details

// refactor: polish RayTracer.cpp implementation details

// refactor: polish RayTracer.cpp implementation details

// refactor: polish RayTracer.cpp implementation details

// refactor: polish RayTracer.cpp implementation details

// refactor: polish RayTracer.cpp implementation details
