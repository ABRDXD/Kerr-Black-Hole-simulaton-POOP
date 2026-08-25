#include "kerr/physics/ObserverTracer.hpp"
#include "TraceEvents.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace kerr::physics {
namespace {

GeodesicState geodesic_part(const TransportState &state) {
    GeodesicState result{};
    std::copy_n(state.begin(), result.size(), result.begin());
    return result;
}

double frame_error(const KerrParameters &parameters, const ObserverEvent &observer) {
    const auto metric = evaluate_metric(
        parameters, {observer.position[1], observer.position[2], observer.position[3]});
    if (!metric.valid())
        return std::numeric_limits<double>::infinity();
    long double error{};
    for (std::size_t a = 0; a < 4; ++a)
        for (std::size_t b = a; b < 4; ++b) {
            long double product{};
            for (std::size_t mu = 0; mu < 4; ++mu)
                for (std::size_t nu = 0; nu < 4; ++nu)
                    product += static_cast<long double>(metric.covariant(mu, nu)) *
                               static_cast<long double>(observer.tetrad[a][mu]) *
                               static_cast<long double>(observer.tetrad[b][nu]);
            const long double expected = a == b ? (a == 0 ? -1.0L : 1.0L) : 0.0L;
            if (!std::isfinite(product))
                return std::numeric_limits<double>::infinity();
            error = std::max(error, std::abs(product - expected));
        }
    return static_cast<double>(error);
}

// Each subsystem retains its own dimension in the denominator. Adding axes
// must never dilute a geodesic error that the eight-component tracer rejects.
double coupled_error(const TransportState &initial, const numerics::Step<20> &step,
                     const ObserverTraceOptions &o) {
    long double geodesic{}, frame{};
    for (std::size_t i = 0; i < initial.size(); ++i) {
        const double atol =
            i < 4 ? o.position_tolerance : (i < 8 ? o.momentum_tolerance : o.frame_tolerance);
        const long double scale =
            static_cast<long double>(atol) +
            static_cast<long double>(o.relative_tolerance) *
                static_cast<long double>(std::max(std::abs(initial[i]), std::abs(step.end[i])));
        if (!(scale > 0.0L) || !std::isfinite(scale))
            return std::numeric_limits<double>::infinity();
        const long double error = static_cast<long double>(step.error[i]) / scale;
        (i < 8 ? geodesic : frame) += error * error;
    }
    return static_cast<double>(std::sqrt(std::max(geodesic / 8.0L, frame / 12.0L)));
}

} // namespace

ObserverTraceResult trace_observer(const KerrParameters &parameters, const ObserverEvent &initial,
                                   const ObserverTraceOptions &o,
                                   const ObserverTraceCallback &callback) {
    const double horizon = horizon_radii(parameters).outer;
    detail::validate_options(o, horizon, o.proper_time_limit);
    if (!std::isfinite(o.frame_tolerance) || o.frame_tolerance <= 0.0 ||
        !std::isfinite(o.frame_error_limit) || o.frame_error_limit <= 0.0 ||
        o.frame_error_limit > 1.0e-9)
        throw std::invalid_argument{
            "Frame tolerance must be positive and finite; frame error limit must be in (0,1e-9]"};
    if (o.coordinate_time_limit && !std::isfinite(*o.coordinate_time_limit))
        throw std::invalid_argument{"Coordinate time target must be finite"};

    ObserverTraceResult result{};
    const auto packed = make_transport_state(parameters, initial);
    if (!packed.valid())
        return result;
    if (o.coordinate_time_limit && *o.coordinate_time_limit < initial.position[0])
        throw std::invalid_argument{"Coordinate time target precedes the initial observer event"};
    const auto initial_diagnostics =
        diagnose_timelike_geodesic(parameters, geodesic_part(packed.state));
    const auto invariants_ok = [&](const GeodesicDiagnostics &d) {
        return d.valid() && d.hamiltonian < 0.0 && d.coordinate_time_rate > 0.0 &&
               std::abs(2.0 * d.hamiltonian + 1.0) <= std::min(o.constraint_limit, 1.0e-7) &&
               d.normalized_constraint <= o.constraint_limit &&
               detail::drift(d.energy, initial_diagnostics.energy) <= o.invariant_drift_limit &&
               detail::drift(d.angular_momentum, initial_diagnostics.angular_momentum) <=
                   o.invariant_drift_limit &&
               detail::drift(d.carter, initial_diagnostics.carter) <= o.invariant_drift_limit;
    };
    const auto initial_frame = transported_observer(parameters, packed.state, initial.proper_time);
    const double initial_frame_error = initial_frame.valid()
                                           ? frame_error(parameters, initial_frame.observer)
                                           : std::numeric_limits<double>::infinity();
    if (!invariants_ok(initial_diagnostics) || initial_frame_error > o.frame_error_limit)
        return result;
    result.state = packed.state;
    result.observer = initial_frame.observer;
    result.diagnostics = initial_diagnostics;
    auto &stats = result.statistics.geodesic;
    stats.maximum_constraint = initial_diagnostics.normalized_constraint;
    result.statistics.maximum_frame_error = initial_frame_error;
    result.statistics.maximum_mass_shell_error =
        std::abs(2.0 * initial_diagnostics.hamiltonian + 1.0);
    if (o.stop_at_horizon && initial_diagnostics.radius <= horizon) {
        result.termination = RayTermination::initially_inside_horizon;
        return result;
    }
    if (callback && !callback(0.0, result.observer, result.diagnostics)) {
        result.termination = RayTermination::cancelled;
        return result;
    }
    if (o.coordinate_time_limit && *o.coordinate_time_limit == initial.position[0]) {
        result.termination = RayTermination::coordinate_time_limit;
        return result;
    }

    GeometryStatus last_status{GeometryStatus::valid};
    const auto rhs = [&](const TransportState &state, TransportState &derivative) {
        ++stats.rhs_evaluations;
        last_status = parallel_transport_rhs(parameters, state, derivative);
        return last_status == GeometryStatus::valid;
    };
    TransportState first{};
    if (!rhs(result.state, first)) {
        result.termination = last_status == GeometryStatus::non_finite_input
                                 ? RayTermination::non_finite
                                 : RayTermination::singular_geometry;
        return result;
    }
    if (o.stop_at_escape && initial_diagnostics.radius >= o.escape_radius) {
        const double r = initial_diagnostics.radius, a = parameters.spin();
        const double outward =
            (result.state[1] * first[1] + result.state[2] * first[2]) / (r * r + a * a) +
            result.state[3] * first[3] / (r * r);
        if (outward > 0.0) {
            result.termination = RayTermination::escape;
            return result;
        }
    }

    double h = o.initial_step;
    std::size_t rejections{};
    for (std::size_t attempt = 0; attempt < o.maximum_attempts; ++attempt) {
        const double remaining = o.proper_time_limit - result.proper_time;
        if (remaining <= 0.0) {
            result.termination = RayTermination::proper_time_limit;
            return result;
        }
        const double coordinate_remaining = o.coordinate_time_limit
            ? *o.coordinate_time_limit - result.state[0]
            : std::numeric_limits<double>::infinity();
        if (coordinate_remaining <= 0.0) {
            result.termination = coordinate_remaining == 0.0
                ? RayTermination::coordinate_time_limit : RayTermination::event_failure;
            return result;
        }
        // Future unit timelike tangents have u^t >= 1 in this chart. Limiting
        // the proper-time trial by the remaining coordinate interval keeps a
        // nearby clock root resolvable even when the normal step is much larger.
        // As for a final proper-time step, this may be shorter than minimum_step.
        h = std::min({h, o.maximum_step, remaining, coordinate_remaining});
        if ((h < o.minimum_step && remaining > o.minimum_step &&
             coordinate_remaining > o.minimum_step) ||
            result.proper_time + h == result.proper_time) {
            result.termination = RayTermination::minimum_step;
            return result;
        }
        const auto step = numerics::dopri5(rhs, result.state, first, h);
        const double error = step.valid ? coupled_error(result.state, step, o)
                                        : std::numeric_limits<double>::infinity();
        auto next = step.valid ? diagnose_timelike_geodesic(parameters, geodesic_part(step.end))
                               : GeodesicDiagnostics{};
        const bool full_invariants_ok = invariants_ok(next);
        auto frame = step.valid ? transported_observer(parameters, step.end,
                                                       initial.proper_time + result.proper_time + h)
                                : ObserverEvaluation{};
        double next_frame_error = frame.valid() ? frame_error(parameters, frame.observer)
                                                : std::numeric_limits<double>::infinity();
        const bool full_frame_ok = next_frame_error <= o.frame_error_limit;
        detail::Event event{};
        if (step.valid && error <= 1.0 && full_invariants_ok && full_frame_ok)
            event =
                detail::find_event(parameters, result.state, step, h, o, o.coordinate_time_limit);
        TransportState endpoint = step.end;
        double used_h = h;
        if (event.theta <= 1.0) {
            endpoint = numerics::dense_state(result.state, step, h, event.theta);
            used_h = h * event.theta;
            next = diagnose_timelike_geodesic(parameters, geodesic_part(endpoint));
            frame = transported_observer(parameters, endpoint,
                                         initial.proper_time + result.proper_time + used_h);
            next_frame_error = frame.valid() ? frame_error(parameters, frame.observer)
                                             : std::numeric_limits<double>::infinity();
        }
        const bool endpoint_invariants_ok = invariants_ok(next);
        const bool endpoint_frame_ok = next_frame_error <= o.frame_error_limit;
        if (!step.valid || !std::isfinite(error) || error > 1.0 || !full_invariants_ok ||
            !endpoint_invariants_ok || !full_frame_ok || !endpoint_frame_ok || !event.valid) {
            ++stats.rejected_steps;
            ++rejections;
            RayTermination failure = RayTermination::rejection_limit;
            if (!event.valid)
                failure = RayTermination::event_failure;
            else if (!step.valid)
                failure = last_status == GeometryStatus::singular_or_degenerate_chart
                              ? RayTermination::singular_geometry
                              : RayTermination::non_finite;
            else if (!full_invariants_ok || !endpoint_invariants_ok)
                failure = RayTermination::constraint_violation;
            else if (!full_frame_ok || !endpoint_frame_ok)
                failure = RayTermination::frame_violation;
            if (rejections >= o.maximum_rejections || h <= o.minimum_step) {
                result.termination =
                    h <= o.minimum_step && failure == RayTermination::rejection_limit
                        ? RayTermination::minimum_step
                        : failure;
                return result;
            }
            h = std::max(o.minimum_step, h * (std::isfinite(error) && error > 1.0
                                                  ? numerics::step_factor(error, 0.2, true)
                                                  : 0.5));
            continue;
        }
        const double elapsed =
            used_h == remaining ? o.proper_time_limit : result.proper_time + used_h;
        const double absolute_time = initial.proper_time + elapsed;
        if (!(endpoint[0] > result.state[0]) || !(elapsed > result.proper_time) ||
            !(absolute_time > result.observer.proper_time)) {
            result.termination = RayTermination::minimum_step;
            return result;
        }
        ++stats.accepted_steps;
        stats.smallest_step =
            stats.accepted_steps == 1 ? used_h : std::min(stats.smallest_step, used_h);
        stats.largest_step = std::max(stats.largest_step, used_h);
        stats.maximum_constraint = std::max(stats.maximum_constraint, next.normalized_constraint);
        stats.maximum_energy_drift = std::max(
            stats.maximum_energy_drift, detail::drift(next.energy, initial_diagnostics.energy));
        stats.maximum_lz_drift =
            std::max(stats.maximum_lz_drift,
                     detail::drift(next.angular_momentum, initial_diagnostics.angular_momentum));
        stats.maximum_carter_drift = std::max(
            stats.maximum_carter_drift, detail::drift(next.carter, initial_diagnostics.carter));
        result.statistics.maximum_frame_error =
            std::max(result.statistics.maximum_frame_error, next_frame_error);
        result.statistics.maximum_mass_shell_error = std::max(
            result.statistics.maximum_mass_shell_error, std::abs(2.0 * next.hamiltonian + 1.0));
        result.state = endpoint;
        result.observer = frame.observer;
        result.observer.proper_time = absolute_time;
        result.proper_time = elapsed;
        result.diagnostics = next;
        if (callback && !callback(result.proper_time, result.observer, result.diagnostics)) {
            result.termination = RayTermination::cancelled;
            return result;
        }
        if (event.theta <= 1.0) {
            result.termination = event.reason;
            return result;
        }
        first = step.endpoint_derivative;
        h *= numerics::step_factor(error, 0.2, rejections > 0);
        rejections = 0;
    }
    result.termination = result.proper_time >= o.proper_time_limit
                             ? RayTermination::proper_time_limit
                             : RayTermination::work_limit;
    return result;
}

} // namespace kerr::physics
