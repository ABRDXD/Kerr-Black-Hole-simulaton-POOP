#include "kerr/numerics/Integrators.hpp"
#include "kerr/physics/ObserverTracer.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {
using namespace kerr::physics;
namespace num = kerr::numerics;
std::size_t checks{}, failures{};
void check(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
double dot(const kerr::math::Matrix4d &metric, const kerr::math::Vector4d &a,
           const kerr::math::Vector4d &b) {
    long double value{};
    for (std::size_t mu = 0; mu < 4; ++mu)
        for (std::size_t nu = 0; nu < 4; ++nu)
            value += static_cast<long double>(metric(mu, nu)) * static_cast<long double>(a[mu]) *
                     static_cast<long double>(b[nu]);
    return static_cast<double>(value);
}
double distance(const TransportState &a, const TransportState &b) {
    double result{};
    for (std::size_t i = 0; i < a.size(); ++i)
        result = std::max(result, std::abs(a[i] - b[i]));
    return result;
}
ObserverEvent radial(const KerrParameters &parameters, double radius, double clock = 0.0) {
    const double w = std::sqrt(2.0 * parameters.mass() / radius);
    const auto observer = make_observer(parameters, {0.0, radius, 0.0, 0.0},
                                        {1.0 + w * w / (1.0 + w), -w, 0.0, 0.0}, clock);
    if (!observer.valid())
        throw std::runtime_error{"Invalid radial test observer"};
    return observer.observer;
}
TransportState reference(const KerrParameters &parameters, TransportState state, double duration,
                         double maximum_step) {
    const auto rhs = [&](const TransportState &s, TransportState &d) {
        return parallel_transport_rhs(parameters, s, d) == GeometryStatus::valid;
    };
    const auto count = static_cast<std::size_t>(std::ceil(duration / maximum_step));
    const double h = duration / static_cast<double>(count);
    for (std::size_t i = 0; i < count; ++i) {
        TransportState first{};
        if (!rhs(state, first))
            throw std::runtime_error{"Invalid reference derivative"};
        const auto step = num::extrapolated_midpoint8(rhs, state, first, h);
        if (!step.valid)
            throw std::runtime_error{"Invalid reference step"};
        state = step.end;
    }
    return state;
}

void holonomy() {
    const KerrParameters parameters{1.0, 0.0};
    constexpr double r = 10.0, f = 1.0 - 2.0 / r;
    auto initial =
        make_circular_observer(parameters, {0.0, r, 0.0, 0.0}, OrbitSense::prograde).observer;
    auto &frame = initial.tetrad;
    const auto metric = evaluate_metric(parameters, {r, 0.0, 0.0});
    frame[1] = {2.0 / r / std::sqrt(f), std::sqrt(f), 0.0, 0.0};
    const kerr::math::Vector4d azimuth{0.0, 0.0, 1.0, 0.0};
    const double projection = dot(metric.covariant, frame[0], azimuth);
    for (std::size_t mu = 0; mu < 4; ++mu)
        frame[2][mu] =
            (azimuth[mu] + projection * frame[0][mu]) / std::sqrt(1.0 + projection * projection);
    frame[3] = {0.0, 0.0, 0.0, 1.0};
    ObserverTraceOptions options{};
    options.proper_time_limit = 2.0 * std::numbers::pi * r * std::sqrt(r - 3.0);
    options.relative_tolerance = 1.0e-12;
    options.position_tolerance = options.momentum_tolerance = options.frame_tolerance = 1.0e-13;
    std::size_t callbacks{};
    double previous_tau = -1.0, previous_t = -1.0;
    const auto result = trace_observer(
        parameters, initial, options,
        [&](double tau, const ObserverEvent &observer, const GeodesicDiagnostics &d) {
            ++callbacks;
            check(validate_observer(parameters, observer) == ObserverStatus::valid,
                  "every callback frame validates without projection");
            check(tau > previous_tau && observer.position[0] > previous_t && d.valid(),
                  "both callback clocks increase");
            previous_tau = tau;
            previous_t = observer.position[0];
            return true;
        });
    check(result.termination == RayTermination::proper_time_limit,
          "adaptive circular period finishes");
    check(callbacks == result.statistics.geodesic.accepted_steps + 1, "callbacks exclude trials");
    check(std::abs(result.state[1] - r) < 3.0e-8 && std::abs(result.state[2]) < 3.0e-8,
          "adaptive circular orbit closes");
    const double phase = 2.0 * std::numbers::pi * std::sqrt(1.0 - 3.0 / r);
    check(std::abs(dot(metric.covariant, frame[1], result.observer.tetrad[1]) - std::cos(phase)) <
              3.0e-9,
          "adaptive analytic holonomy cosine");
    check(std::abs(dot(metric.covariant, frame[2], result.observer.tetrad[1]) + std::sin(phase)) <
              3.0e-9,
          "adaptive analytic holonomy sine and orientation");
    check(result.statistics.maximum_frame_error < 1.0e-9 &&
              result.statistics.maximum_mass_shell_error < 1.0e-9,
          "absolute frame and mass-shell gates");
    const auto reset =
        make_circular_observer(parameters, result.observer.position, OrbitSense::prograde);
    double orientation_change{};
    for (std::size_t mu = 0; mu < 4; ++mu)
        orientation_change +=
            std::abs(reset.observer.tetrad[1][mu] - result.observer.tetrad[1][mu]);
    check(orientation_change > 0.5,
          "transport preserves gyroscope rotation rather than resetting axes");
}

void events_and_clocks() {
    const KerrParameters parameters{1.0, 0.0};
    auto initial = radial(parameters, 8.0, 17.0);
    ObserverTraceOptions options{};
    options.maximum_step = 1.0;
    const auto horizon = trace_observer(parameters, initial, options);
    const double expected_tau = (std::pow(8.0, 1.5) - std::pow(2.0, 1.5)) / (1.5 * std::sqrt(2.0));
    check(horizon.termination == RayTermination::horizon, "adaptive dense future horizon");
    check(std::abs(horizon.diagnostics.radius - 2.0) < 2.0e-9,
          "dense horizon lies on oblate surface");
    check(std::abs(horizon.proper_time - expected_tau) < 3.0e-8,
          "analytic infall horizon proper clock");
    check(std::abs(horizon.observer.proper_time - 17.0 - expected_tau) < 3.0e-8,
          "absolute observer clock preserves initial offset");
    check(validate_observer(parameters, horizon.observer) == ObserverStatus::valid,
          "dense horizon frame validates");
    options.stop_at_horizon = false;
    options.proper_time_limit = expected_tau + 0.3;
    const auto interior = trace_observer(parameters, initial, options);
    check(interior.termination == RayTermination::proper_time_limit &&
              interior.diagnostics.radius < 2.0,
          "adaptive orientation crosses the future horizon");

    const auto circle =
        make_circular_observer(parameters, {7.0, 10.0, 0.0, 0.0}, OrbitSense::prograde, 3.0)
            .observer;
    options = {};
    options.coordinate_time_limit = 22.123456789;
    const auto target = trace_observer(parameters, circle, options);
    const double analytic_tau =
        (*options.coordinate_time_limit - circle.position[0]) / circle.tetrad[0][0];
    check(target.termination == RayTermination::coordinate_time_limit,
          "absolute coordinate time event");
    check(std::abs(target.state[0] - *options.coordinate_time_limit) < 5.0e-13,
          "dense coordinate time root reaches requested slice");
    check(std::abs(target.proper_time - analytic_tau) < 1.0e-9,
          "circular coordinate target yields independent analytic proper time");
    check(std::abs(target.observer.proper_time - 3.0 - analytic_tau) < 1.0e-9,
          "absolute proper clock at coordinate slice");
    options.coordinate_time_limit = circle.position[0];
    const auto immediate = trace_observer(parameters, circle, options);
    check(immediate.termination == RayTermination::coordinate_time_limit &&
              immediate.statistics.geodesic.accepted_steps == 0 && immediate.proper_time == 0.0,
          "equal coordinate target is an immediate event");
    options.coordinate_time_limit = 100.0;
    check(trace_observer(parameters, initial, options).termination == RayTermination::horizon,
          "earlier horizon precedes coordinate target");
    options.coordinate_time_limit = 2.0;
    check(trace_observer(parameters, initial, options).termination ==
              RayTermination::coordinate_time_limit,
          "earlier coordinate target precedes horizon");

    const auto disk_initial =
        make_local_velocity_observer(parameters, {0.0, 9.0, 0.0, 0.7}, {0.05, 0.15, -0.2}).observer;
    options = {};
    options.stop_at_disk = true;
    const auto disk = trace_observer(parameters, disk_initial, options);
    check(disk.termination == RayTermination::disk && std::abs(disk.state[3]) < 1.0e-10,
          "transported thin disk event");
    check(validate_observer(parameters, disk.observer) == ObserverStatus::valid,
          "dense disk frame validates");
    const auto outward =
        make_local_velocity_observer(parameters, {0.0, 8.0, 0.0, 0.0}, {0.6, 0.0, 0.0}).observer;
    options = {};
    options.escape_radius = 9.0;
    const auto escape = trace_observer(parameters, outward, options);
    check(escape.termination == RayTermination::escape &&
              std::abs(escape.diagnostics.radius - 9.0) < 1.0e-9,
          "transported outward escape event");
}

void reference_and_restart() {
    for (double spin : {-0.9, 0.0, 0.99}) {
        const KerrParameters parameters{1.0, spin};
        const auto initial =
            make_local_velocity_observer(parameters, {4.0, 7.0, 1.0, 2.0}, {0.1, 0.2, -0.05}, 3.0)
                .observer;
        const auto packed = make_transport_state(parameters, initial);
        const auto truth = reference(parameters, packed.state, 12.0, 0.1);
        const auto refined = reference(parameters, packed.state, 12.0, 0.05);
        check(distance(truth, refined) < 3.0e-11, "independent midpoint8 step refinement");
        ObserverTraceOptions options{};
        options.proper_time_limit = 12.0;
        options.maximum_step = 2.0;
        const auto coarse = trace_observer(parameters, initial, options);
        options.relative_tolerance = 1.0e-12;
        options.position_tolerance = options.momentum_tolerance = options.frame_tolerance = 1.0e-14;
        const auto fine = trace_observer(parameters, initial, options);
        check(coarse.termination == RayTermination::proper_time_limit &&
                  fine.termination == RayTermination::proper_time_limit,
              "inclined adaptive traces finish");
        check(distance(coarse.state, refined) < 2.0e-8,
              "all twenty adaptive components agree with reference");
        check(distance(fine.state, refined) < distance(coarse.state, refined) * 0.15,
              "tolerance refinement improves full coupled state");
        options.proper_time_limit = 5.0;
        const auto first = trace_observer(parameters, initial, options);
        options.proper_time_limit = 7.0;
        const auto second = trace_observer(parameters, first.observer, options);
        check(second.termination == RayTermination::proper_time_limit,
              "orientation resumes from accepted frame");
        check(distance(second.state, fine.state) < 2.0e-10,
              "split trace and uninterrupted orientation agree");
        check(second.observer.proper_time == 15.0,
              "restart preserves absolute proper-clock offset");
    }
}

void coordinate_target_precision() {
    const KerrParameters parameters{1.0, 0.0};
    for (double offset : {0.0, 7.0, 1.0e12}) {
        const auto initial =
            make_circular_observer(parameters, {offset, 10.0, 0.0, 0.0},
                                   OrbitSense::prograde).observer;
        const double target = offset == 0.0 ? 1.0e-20 :
            std::nextafter(offset, std::numeric_limits<double>::infinity());
        ObserverTraceOptions options{};
        options.coordinate_time_limit = target;
        const auto result = trace_observer(parameters, initial, options);
        const double duration = (target - offset) / initial.tetrad[0][0];
        const double error = std::abs(result.proper_time - duration) / duration;
        check(result.termination == RayTermination::coordinate_time_limit,
              "nearby coordinate target terminates on its requested slice");
        check(error < 1.0e-12,
              "nearby coordinate target evolves the consistent proper-time interval");
        const double ulp = std::nextafter(target, std::numeric_limits<double>::infinity()) - target;
        check(std::abs(result.observer.position[0] - target) <= 8.0 * ulp,
              "coordinate root error scales with the target clock precision");
        check(result.statistics.geodesic.accepted_steps == 1 &&
                  validate_observer(parameters, result.observer) == ObserverStatus::valid,
              "short coordinate event accepts one finite frame even below minimum step");
    }
    const auto boosted =
        make_local_velocity_observer(parameters, {0.0, 10.0, 0.0, 0.0}, {0.9999, 0.0, 0.0});
    check(boosted.valid(), "boosted clock precision initial frame validates");
    for (double target : {1.0e-20, 1.0e-200, 1.0e-300}) {
        ObserverTraceOptions options{};
        options.coordinate_time_limit = target;
        const auto result = trace_observer(parameters, boosted.observer, options);
        // Over these intervals any change in the initial velocity is far below
        // binary64 resolution. This checks the evolved interval, not just t.
        const double duration = target / boosted.observer.tetrad[0][0];
        const double ulp = std::nextafter(target, std::numeric_limits<double>::infinity()) - target;
        check(result.termination == RayTermination::coordinate_time_limit &&
                  std::abs(result.proper_time - duration) / duration < 1.0e-13,
              "boosted tiny clock event evolves the correct proper-time interval");
        check(std::abs(result.observer.position[0] - target) <= 8.0 * ulp,
              "boosted clock root retains relative precision at tiny targets");
    }
    auto large =
        make_circular_observer(parameters, {1.0e20, 10.0, 0.0, 0.0},
                               OrbitSense::prograde).observer;
    ObserverTraceOptions options{};
    options.coordinate_time_limit = std::nextafter(large.position[0],
                                                   std::numeric_limits<double>::infinity());
    const auto stagnant = trace_observer(parameters, large, options);
    check(stagnant.termination == RayTermination::minimum_step &&
              stagnant.observer.position == large.position && stagnant.proper_time == 0.0,
          "unresolvable large clock offset fails without snapping time to target");
}

void failures_and_bounds() {
    const KerrParameters parameters{1.0, 0.0};
    const auto initial = radial(parameters, 8.0);
    auto invalid = initial;
    invalid.tetrad[1][1] *= 1.1;
    const auto corrupted = trace_observer(parameters, invalid);
    check(corrupted.termination == RayTermination::invalid_initial_state &&
              num::finite(corrupted.state),
          "corrupted initial orientation fails with finite state");
    invalid = initial;
    invalid.position[2] = std::numeric_limits<double>::infinity();
    check(trace_observer(parameters, invalid).termination == RayTermination::invalid_initial_state,
          "nonfinite initial data rejected");
    std::size_t callbacks{};
    const auto cancelled = trace_observer(
        parameters, initial, {}, [&](double, const ObserverEvent &, const GeodesicDiagnostics &) {
            return ++callbacks < 4;
        });
    check(cancelled.termination == RayTermination::cancelled && callbacks == 4 &&
              cancelled.statistics.geodesic.accepted_steps == 3 && num::finite(cancelled.state),
          "bounded cancellation retains accepted finite event");
    ObserverTraceOptions options{};
    options.maximum_attempts = 1;
    const auto limited = trace_observer(parameters, initial, options);
    check(limited.termination == RayTermination::work_limit &&
              limited.statistics.geodesic.accepted_steps == 1 &&
              limited.statistics.geodesic.rhs_evaluations <= 7,
          "bounded work budget");
    options = {};
    options.relative_tolerance = 1.0e-15;
    options.position_tolerance = options.momentum_tolerance = options.frame_tolerance = 1.0e-16;
    options.initial_step = options.minimum_step = options.maximum_step = 0.5;
    check(trace_observer(parameters, initial, options).termination == RayTermination::minimum_step,
          "minimum step failure retains last accepted event");
    options.minimum_step = 1.0e-12;
    options.maximum_rejections = 1;
    check(trace_observer(parameters, initial, options).termination ==
              RayTermination::rejection_limit,
          "consecutive rejection budget");
    options = {};
    options.relative_tolerance = 0.1;
    options.position_tolerance = options.momentum_tolerance = options.frame_tolerance = 0.1;
    options.initial_step = options.minimum_step = options.maximum_step = 2.0;
    options.frame_error_limit = 1.0e-12;
    const auto circle =
        make_circular_observer(parameters, {0.0, 10.0, 0.0, 0.0}, OrbitSense::prograde).observer;
    const auto frame_failed = trace_observer(parameters, circle, options);
    check(frame_failed.termination == RayTermination::frame_violation &&
              frame_failed.statistics.geodesic.accepted_steps == 0,
          "absolute frame gate rejects otherwise permissive local error");
    for (int kind = 0; kind < 5; ++kind) {
        options = {};
        if (kind == 0)
            options.frame_tolerance = 0.0;
        if (kind == 1)
            options.frame_error_limit = 1.0e-8;
        if (kind == 2)
            options.coordinate_time_limit = -1.0;
        if (kind == 3)
            options.coordinate_time_limit = std::numeric_limits<double>::quiet_NaN();
        if (kind == 4)
            options.proper_time_limit = std::numeric_limits<double>::infinity();
        bool threw{};
        try {
            static_cast<void>(trace_observer(parameters, initial, options));
        } catch (const std::invalid_argument &) {
            threw = true;
        }
        check(threw, "invalid transport numerical options throw");
    }
    options = {};
    options.proper_time_limit = 0.001;
    invalid = initial;
    invalid.proper_time = 1.0e100;
    const auto stagnant = trace_observer(parameters, invalid, options);
    check(stagnant.termination == RayTermination::minimum_step && stagnant.proper_time == 0.0,
          "absolute clock stagnation cannot publish a duplicate clock");
    check(termination_name(RayTermination::coordinate_time_limit) == "coordinate_time_limit" &&
              termination_name(RayTermination::frame_violation) == "frame_violation",
          "typed transport reasons");
}
} // namespace

int main() {
    holonomy();
    events_and_clocks();
    reference_and_restart();
    coordinate_target_precision();
    failures_and_bounds();
    std::cout << (failures == 0 ? "PASS: " : "FAIL: ") << checks << " adaptive transport checks, "
              << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
