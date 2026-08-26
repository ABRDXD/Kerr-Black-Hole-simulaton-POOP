#include "kerr/physics/StationaryObserver.hpp"

#include "kerr/numerics/Integrators.hpp"
#include "kerr/physics/Connection.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace kerr::physics {
namespace {

struct StationarySystem final {
    StationaryKinematics kinematics{};
    math::Matrix4d metric{};
    math::Matrix4<long double> generator{};
};

long double inner(const math::Matrix4d &metric, const math::Vector4d &a,
                  const math::Vector4d &b) noexcept {
    long double value{};
    for (std::size_t mu = 0; mu < 4; ++mu)
        for (std::size_t nu = 0; nu < 4; ++nu)
            value += static_cast<long double>(metric(mu, nu)) *
                     static_cast<long double>(a[mu]) * static_cast<long double>(b[nu]);
    return value;
}

StationarySystem stationary_system(const KerrParameters &parameters,
                                    const math::Vector4d &position) noexcept {
    StationarySystem result{};
    if (!numerics::finite(position))
        return result;
    const math::Vector3d spatial{position[1], position[2], position[3]};
    const auto metric = evaluate_metric(parameters, spatial);
    if (!metric.valid()) {
        result.kinematics.status = ObserverStatus::invalid_geometry;
        return result;
    }
    if (!(metric.covariant(0, 0) < 0.0)) {
        result.kinematics.status = ObserverStatus::stationary_unavailable;
        return result;
    }
    const auto partials = evaluate_metric_derivatives(parameters, spatial);
    const double rate = std::sqrt(-metric.covariant(0, 0));
    const math::Vector4d velocity{1.0 / rate, 0.0, 0.0, 0.0};
    math::Matrix4<long double> connection{};
    if (!partials.valid() || !numerics::finite(velocity) ||
        contract_connection(metric, partials, velocity, connection) != GeometryStatus::valid) {
        result.kinematics.status = ObserverStatus::numerical_failure;
        return result;
    }
    math::Vector4d acceleration{};
    for (std::size_t mu = 0; mu < 4; ++mu)
        acceleration[mu] = static_cast<double>(connection(mu, 0) *
                                               static_cast<long double>(velocity[0]));
    if (!numerics::finite(acceleration)) {
        result.kinematics.status = ObserverStatus::numerical_failure;
        return result;
    }
    const long double acceleration_squared = inner(metric.covariant, acceleration, acceleration);
    if (!(acceleration_squared >= 0.0L) || !std::isfinite(acceleration_squared) ||
        std::abs(inner(metric.covariant, velocity, velocity) + 1.0L) > 1.0e-9L) {
        result.kinematics.status = ObserverStatus::numerical_failure;
        return result;
    }
    std::array<long double, 4> lowered_velocity{}, lowered_acceleration{};
    for (std::size_t mu = 0; mu < 4; ++mu)
        for (std::size_t nu = 0; nu < 4; ++nu) {
            lowered_velocity[mu] += static_cast<long double>(metric.covariant(mu, nu)) *
                                    static_cast<long double>(velocity[nu]);
            lowered_acceleration[mu] += static_cast<long double>(metric.covariant(mu, nu)) *
                                        static_cast<long double>(acceleration[nu]);
        }
    // Signature (-,+,+,+): D e/dtau = u (a.e) - a (u.e).
    // Keep the full antisymmetric term, including for imperfect intermediate axes.
    for (std::size_t mu = 0; mu < 4; ++mu)
        for (std::size_t nu = 0; nu < 4; ++nu)
            result.generator(mu, nu) =
                (-connection(mu, nu) + static_cast<long double>(velocity[mu]) *
                                            lowered_acceleration[nu] -
                 static_cast<long double>(acceleration[mu]) * lowered_velocity[nu]) /
                static_cast<long double>(velocity[0]);
    const double magnitude = static_cast<double>(std::sqrt(acceleration_squared));
    if (!std::isfinite(magnitude) ||
        !std::all_of(result.generator.elements.begin(), result.generator.elements.end(),
                     [](long double value) { return std::isfinite(value); })) {
        result.generator = {};
        result.kinematics.status = ObserverStatus::numerical_failure;
        return result;
    }
    result.metric = metric.covariant;
    result.kinematics = {ObserverStatus::valid, velocity, acceleration, rate, magnitude};
    return result;
}

bool apply_generator(const math::Matrix4<long double> &generator, const StationaryAxes &axes,
                     StationaryAxes &derivative) noexcept {
    const StationaryAxes input = axes;
    derivative = {};
    if (!numerics::finite(input))
        return false;
    StationaryAxes result{};
    for (std::size_t leg = 0; leg < 3; ++leg)
        for (std::size_t mu = 0; mu < 4; ++mu) {
            long double sum{};
            for (std::size_t nu = 0; nu < 4; ++nu)
                sum += generator(mu, nu) * static_cast<long double>(input[4 * leg + nu]);
            result[4 * leg + mu] = static_cast<double>(sum);
        }
    if (!numerics::finite(result))
        return false;
    derivative = result;
    return true;
}

double frame_residual(const math::Matrix4d &metric, const ObserverEvent &observer) noexcept {
    long double residual{};
    for (std::size_t a = 0; a < 4; ++a)
        for (std::size_t b = a; b < 4; ++b) {
            const long double expected = a == b ? (a == 0 ? -1.0L : 1.0L) : 0.0L;
            residual = std::max(residual, std::abs(inner(metric, observer.tetrad[a],
                                                        observer.tetrad[b]) - expected));
        }
    return static_cast<double>(residual);
}

void validate_options(const StationaryOptions &options) {
    for (double positive : {options.relative_tolerance, options.axis_tolerance,
                            options.initial_step, options.minimum_step, options.maximum_step})
        if (!(positive > 0.0) || !std::isfinite(positive))
            throw std::invalid_argument{"Stationary tolerances and step bounds must be finite and positive"};
    if (options.minimum_step > options.initial_step || options.initial_step > options.maximum_step ||
        options.maximum_attempts == 0 || options.maximum_rejections == 0)
        throw std::invalid_argument{"Invalid stationary step bounds or work budgets"};
}

} // namespace

StationaryKinematics stationary_kinematics(const KerrParameters &parameters,
                                          const math::Vector4d position) noexcept {
    return stationary_system(parameters, position).kinematics;
}

ObserverStatus stationary_fermi_walker_rhs(const KerrParameters &parameters,
                                           const math::Vector4d position,
                                           const StationaryAxes &axes,
                                           StationaryAxes &derivative) noexcept {
    const StationaryAxes input = axes;
    derivative = {};
    if (!numerics::finite(input))
        return ObserverStatus::non_finite_input;
    const auto system = stationary_system(parameters, position);
    if (!system.kinematics.valid())
        return system.kinematics.status;
    return apply_generator(system.generator, input, derivative) ? ObserverStatus::valid
                                                               : ObserverStatus::numerical_failure;
}

std::string_view stationary_termination_name(const StationaryTermination reason) noexcept {
    switch (reason) {
    case StationaryTermination::coordinate_time_limit: return "coordinate_time_limit";
    case StationaryTermination::invalid_initial_state: return "invalid_initial_state";
    case StationaryTermination::stationary_unavailable: return "stationary_unavailable";
    case StationaryTermination::non_finite: return "non_finite";
    case StationaryTermination::frame_violation: return "frame_violation";
    case StationaryTermination::work_limit: return "work_limit";
    case StationaryTermination::minimum_step: return "minimum_step";
    case StationaryTermination::rejection_limit: return "rejection_limit";
    case StationaryTermination::cancelled: return "cancelled";
    }
    return "unknown";
}

StationaryResult advance_stationary_observer(const KerrParameters &parameters,
                                              const ObserverEvent &initial,
                                              const double coordinate_time,
                                              const StationaryOptions &options,
                                              const StationaryObserverCallback &observer) {
    validate_options(options);
    StationaryResult result{};
    const auto system = stationary_system(parameters, initial.position);
    result.observer_status = system.kinematics.status;
    if (!system.kinematics.valid()) {
        if (result.observer_status == ObserverStatus::stationary_unavailable)
            result.termination = StationaryTermination::stationary_unavailable;
        return result;
    }
    result.observer_status = validate_observer(parameters, initial);
    if (result.observer_status != ObserverStatus::valid)
        return result;
    if (initial.tetrad[0] != system.kinematics.velocity) {
        result.observer_status = ObserverStatus::invalid_four_velocity;
        return result;
    }
    result.observer = initial;
    result.kinematics = system.kinematics;
    result.statistics.maximum_frame_residual = frame_residual(system.metric, initial);
    if (!std::isfinite(coordinate_time)) {
        result.termination = StationaryTermination::non_finite;
        result.observer_status = ObserverStatus::non_finite_input;
        return result;
    }
    if (coordinate_time < initial.position[0])
        throw std::invalid_argument{"Stationary target coordinate time precedes the initial event"};
    const long double duration = static_cast<long double>(coordinate_time) -
                                 static_cast<long double>(initial.position[0]);
    if (!std::isfinite(static_cast<double>(duration))) {
        result.termination = StationaryTermination::non_finite;
        result.observer_status = ObserverStatus::numerical_failure;
        return result;
    }
    if (observer && !observer(result.observer)) {
        result.termination = StationaryTermination::cancelled;
        return result;
    }
    if (duration == 0.0L) {
        result.termination = StationaryTermination::coordinate_time_limit;
        return result;
    }

    StationaryAxes axes{}, first{}, atol{};
    for (std::size_t leg = 0; leg < 3; ++leg)
        std::copy(initial.tetrad[leg + 1].begin(), initial.tetrad[leg + 1].end(),
                  axes.begin() + static_cast<std::ptrdiff_t>(4 * leg));
    atol.fill(options.axis_tolerance);
    const auto rhs = [&](const StationaryAxes &state, StationaryAxes &derivative) {
        return apply_generator(system.generator, state, derivative);
    };
    ++result.statistics.rhs_evaluations;
    if (!rhs(axes, first)) {
        result.termination = StationaryTermination::non_finite;
        result.observer_status = ObserverStatus::numerical_failure;
        return result;
    }
    long double elapsed{};
    double step_size = options.initial_step;
    std::size_t consecutive_rejections{};
    for (std::size_t attempt = 0; attempt < options.maximum_attempts; ++attempt) {
        const long double remaining = duration - elapsed;
        const double h = std::min(step_size, static_cast<double>(remaining));
        const bool final_step = static_cast<long double>(step_size) >= remaining;
        const long double next_elapsed = final_step ? duration : elapsed + static_cast<long double>(h);
        const double next_time = final_step ? coordinate_time :
            static_cast<double>(static_cast<long double>(initial.position[0]) + next_elapsed);
        if (!(h > 0.0) || next_elapsed == elapsed || next_time <= result.observer.position[0]) {
            result.termination = StationaryTermination::minimum_step;
            return result;
        }
        const auto trial = numerics::dopri5(rhs, axes, first, h);
        result.statistics.rhs_evaluations += trial.evaluations;
        const double error = trial.valid ? numerics::scaled_error(axes, trial, atol,
                                                                  options.relative_tolerance)
                                          : std::numeric_limits<double>::infinity();
        ObserverEvent candidate = initial;
        ObserverStatus trial_status = ObserverStatus::numerical_failure;
        if (trial.valid && error <= 1.0) {
            candidate.position[0] = next_time;
            candidate.proper_time = static_cast<double>(
                static_cast<long double>(initial.proper_time) +
                next_elapsed * static_cast<long double>(system.kinematics.proper_time_rate));
            for (std::size_t leg = 0; leg < 3; ++leg)
                std::copy_n(trial.end.begin() + static_cast<std::ptrdiff_t>(4 * leg), 4,
                            candidate.tetrad[leg + 1].begin());
            trial_status = validate_observer(parameters, candidate);
            if (trial_status == ObserverStatus::valid &&
                candidate.proper_time <= result.observer.proper_time) {
                result.termination = StationaryTermination::minimum_step;
                return result;
            }
        }
        if (trial.valid && error <= 1.0 && trial_status == ObserverStatus::valid) {
            result.observer = candidate;
            result.observer_status = ObserverStatus::valid;
            axes = trial.end;
            first = trial.endpoint_derivative;
            elapsed = next_elapsed;
            auto &stats = result.statistics;
            ++stats.accepted_steps;
            stats.smallest_step = stats.accepted_steps == 1 ? h : std::min(stats.smallest_step, h);
            stats.largest_step = std::max(stats.largest_step, h);
            stats.maximum_frame_residual =
                std::max(stats.maximum_frame_residual, frame_residual(system.metric, candidate));
            if (observer && !observer(candidate)) {
                result.termination = StationaryTermination::cancelled;
                return result;
            }
            if (final_step) {
                result.termination = StationaryTermination::coordinate_time_limit;
                return result;
            }
            step_size = std::clamp(h * numerics::step_factor(error, 0.2, consecutive_rejections > 0),
                                   options.minimum_step, options.maximum_step);
            consecutive_rejections = 0;
        } else {
            ++result.statistics.rejected_steps;
            ++consecutive_rejections;
            const bool frame_failed = trial.valid && error <= 1.0 &&
                                       trial_status != ObserverStatus::valid;
            result.observer_status = frame_failed ? trial_status :
                (trial.valid ? ObserverStatus::valid : ObserverStatus::numerical_failure);
            const bool nonfinite_trial = !trial.valid ||
                (frame_failed && trial_status == ObserverStatus::non_finite_input);
            if (consecutive_rejections >= options.maximum_rejections) {
                result.termination = nonfinite_trial ? StationaryTermination::non_finite :
                    (frame_failed ? StationaryTermination::frame_violation
                                  : StationaryTermination::rejection_limit);
                return result;
            }
            if (h <= options.minimum_step) {
                result.termination = nonfinite_trial ? StationaryTermination::non_finite :
                    (frame_failed ? StationaryTermination::frame_violation
                                  : StationaryTermination::minimum_step);
                return result;
            }
            const double factor = frame_failed || !trial.valid ? 0.5 :
                numerics::step_factor(error, 0.2, true);
            step_size = std::max(options.minimum_step, h * factor);
        }
    }
    result.termination = StationaryTermination::work_limit;
    return result;
}

} // namespace kerr::physics
