#include "kerr/numerics/Integrators.hpp"
#include "kerr/physics/StationaryObserver.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace kerr::physics;
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

StationaryAxes axes_of(const ObserverEvent &observer) {
    StationaryAxes axes{};
    for (std::size_t leg = 0; leg < 3; ++leg)
        std::copy(observer.tetrad[leg + 1].begin(), observer.tetrad[leg + 1].end(),
                  axes.begin() + static_cast<std::ptrdiff_t>(4 * leg));
    return axes;
}

double distance(const StationaryAxes &a, const StationaryAxes &b) {
    double result{};
    for (std::size_t i = 0; i < a.size(); ++i)
        result = std::max(result, std::abs(a[i] - b[i]));
    return result;
}

StationaryAxes reference(const KerrParameters &parameters, const ObserverEvent &initial,
                         double duration, double maximum_step) {
    const auto rhs = [&](const StationaryAxes &state, StationaryAxes &derivative) {
        return stationary_fermi_walker_rhs(parameters, initial.position, state, derivative) ==
               ObserverStatus::valid;
    };
    auto axes = axes_of(initial);
    const auto count = static_cast<std::size_t>(std::ceil(duration / maximum_step));
    const double h = duration / static_cast<double>(count);
    for (std::size_t i = 0; i < count; ++i) {
        StationaryAxes first{};
        if (!rhs(axes, first))
            throw std::runtime_error{"Invalid stationary reference RHS"};
        const auto step = kerr::numerics::extrapolated_midpoint8(rhs, axes, first, h);
        if (!step.valid)
            throw std::runtime_error{"Invalid stationary reference step"};
        axes = step.end;
    }
    return axes;
}

void test_schwarzschild() {
    for (double mass : {0.5, 1.0, 2.3}) {
        const KerrParameters parameters{mass, 0.0};
        for (double radius : {2.1 * mass, 4.0 * mass, 20.0 * mass}) {
            const auto initial = make_stationary_observer(parameters, {3.0, radius, 0.0, 0.0}, 7.0);
            check(initial.valid(), "Schwarzschild stationary frame");
            const auto kinematics = stationary_kinematics(parameters, initial.observer.position);
            check(kinematics.valid(), "stationary acceleration is finite");
            const double rate = std::sqrt(1.0 - 2.0 * mass / radius);
            const double acceleration = mass / (radius * radius * rate);
            check(std::abs(kinematics.proper_acceleration - acceleration) <
                      3.0e-13 * std::max(1.0, acceleration),
                  "independent Schwarzschild proper acceleration");
            const auto metric = evaluate_metric(parameters, {radius, 0.0, 0.0});
            check(std::abs(dot(metric.covariant, kinematics.acceleration, kinematics.velocity)) <
                      3.0e-13 * std::max(1.0, acceleration),
                  "four-acceleration is orthogonal to velocity");
            const double target = 3.0 + 60.0 * mass;
            const auto result = advance_stationary_observer(parameters, initial.observer, target);
            check(result.termination == StationaryTermination::coordinate_time_limit,
                  "Schwarzschild stationary evolution completes");
            check(result.observer.position == kerr::math::Vector4d{target, radius, 0.0, 0.0},
                  "fixed position and exact requested coordinate time");
            check(std::abs(result.observer.proper_time - (7.0 + (target - 3.0) * rate)) < 2.0e-13,
                  "analytic stationary proper clock with initial offset");
            check(distance(axes_of(initial.observer), axes_of(result.observer)) < 2.0e-12,
                  "Schwarzschild stationary axes have no precession");
            check(result.statistics.maximum_frame_residual < 2.0e-12,
                  "Schwarzschild frame residual");
        }
    }
}

void test_kerr_precession() {
    // Kocherlakota et al. (2019), Eq. 38: Omega_p = -a M/[r^2(r-2M)] zhat,
    // per proper time. At fixed coordinates a constant orthonormal frame sees
    // this same signed rotation. KerrLab's equatorial third leg points along +z.
    for (double mass : {1.0, 2.3}) {
        for (double spin_ratio : {-0.9, 0.5, 1.0}) {
            const double spin = spin_ratio * mass, radius = 4.0 * mass;
            const KerrParameters parameters{mass, spin};
            const double x = std::hypot(radius, spin);
            const auto initial = make_stationary_observer(parameters, {4.0, x, 0.0, 0.0}, 9.0);
            check(initial.valid(), "Kerr equatorial stationary frame");
            const double duration = 150.0 * mass;
            StationaryOptions options{};
            options.initial_step = 1.0 * mass;
            options.maximum_step = 4.0 * mass;
            auto result = advance_stationary_observer(parameters, initial.observer, 4.0 + duration,
                                                      options);
            check(result.termination == StationaryTermination::coordinate_time_limit,
                  "Kerr stationary precession completes");
            check(validate_observer(parameters, result.observer) == ObserverStatus::valid,
                  "Kerr Fermi-Walker axes stay orthonormal");
            const double rate = std::sqrt(1.0 - 2.0 * mass / radius);
            const double phase = spin * mass / (radius * radius * (radius - 2.0 * mass)) *
                                 duration * rate;
            const auto metric = evaluate_metric(parameters, {x, 0.0, 0.0});
            check(std::abs(dot(metric.covariant, initial.observer.tetrad[1],
                               result.observer.tetrad[1]) - std::cos(phase)) < 3.0e-9,
                  "independent Kerr equatorial precession cosine");
            check(std::abs(dot(metric.covariant, initial.observer.tetrad[2],
                               result.observer.tetrad[1]) + std::sin(phase)) < 3.0e-9,
                  "independent signed Kerr equatorial precession sine");
            check(std::abs(dot(metric.covariant, initial.observer.tetrad[3],
                               result.observer.tetrad[3]) - 1.0) < 3.0e-12,
                  "equatorial precession preserves spin-axis leg");
            options.relative_tolerance = 1.0e-13;
            options.axis_tolerance = 1.0e-14;
            options.maximum_step = mass;
            const auto refined = advance_stationary_observer(parameters, initial.observer,
                                                              4.0 + duration, options);
            check(refined.termination == StationaryTermination::coordinate_time_limit,
                  "refined Kerr stationary evolution completes");
            check(distance(axes_of(result.observer), axes_of(refined.observer)) < 3.0e-9,
                  "Kerr adaptive endpoint converges under tolerance refinement");
        }
    }
}

void test_inclined_reference_and_callbacks() {
    for (double spin : {-0.8, 0.9}) {
        const KerrParameters parameters{1.0, spin};
        const auto initial = make_stationary_observer(parameters, {1.0, 3.0, -1.0, 2.0}, 5.0);
        const auto ref = reference(parameters, initial.observer, 20.0, 0.5);
        const auto fine = reference(parameters, initial.observer, 20.0, 0.25);
        check(distance(ref, fine) < 2.0e-12, "inclined eighth-order reference step refinement");
        std::size_t calls{};
        double previous_time = -1.0, previous_clock = -1.0;
        bool valid_callbacks = true;
        const auto result = advance_stationary_observer(
            parameters, initial.observer, 21.0, {}, [&](const ObserverEvent &event) {
                ++calls;
                valid_callbacks = valid_callbacks &&
                    validate_observer(parameters, event) == ObserverStatus::valid &&
                    event.position[0] > previous_time && event.proper_time > previous_clock;
                previous_time = event.position[0];
                previous_clock = event.proper_time;
                return true;
            });
        check(result.termination == StationaryTermination::coordinate_time_limit,
              "inclined stationary driver completes");
        check(valid_callbacks && calls == result.statistics.accepted_steps + 1,
              "callbacks contain only accepted finite frames with advancing clocks");
        check(distance(axes_of(result.observer), fine) < 2.0e-10,
              "inclined DOPRI5 agrees with independent eighth-order stepper");
        const auto segment = advance_stationary_observer(parameters, initial.observer, 11.0);
        const auto continued = advance_stationary_observer(parameters, segment.observer, 21.0);
        check(continued.termination == StationaryTermination::coordinate_time_limit &&
                  distance(axes_of(continued.observer), axes_of(result.observer)) < 2.0e-10 &&
                  std::abs(continued.observer.proper_time - result.observer.proper_time) < 1.0e-13,
              "stationary frame and clock continuation preserve history");
        StationaryAxes axes = axes_of(initial.observer), derivative{};
        check(stationary_fermi_walker_rhs(parameters, initial.observer.position, axes, derivative) ==
                  ObserverStatus::valid,
              "stationary RHS valid");
        check(stationary_fermi_walker_rhs(parameters, initial.observer.position, axes, axes) ==
                  ObserverStatus::valid && axes == derivative,
              "stationary RHS permits aliasing");
    }
}

void test_failures() {
    const KerrParameters parameters{1.0, 0.8};
    const auto initial = make_stationary_observer(parameters, {1.0, 4.0, 0.0, 0.0}, 2.0).observer;
    auto result = advance_stationary_observer(parameters, initial, 1.0);
    check(result.termination == StationaryTermination::coordinate_time_limit &&
              result.observer.tetrad == initial.tetrad && result.statistics.accepted_steps == 0,
          "stationary no-op retains exact initial orientation");
    result = advance_stationary_observer(parameters, initial, 10.0, {},
                                          [](const ObserverEvent &) { return false; });
    check(result.termination == StationaryTermination::cancelled &&
              result.observer.position == initial.position && result.statistics.rhs_evaluations == 0,
          "stationary cancellation at initial sample");
    std::size_t callbacks{};
    result = advance_stationary_observer(parameters, initial, 10.0, {},
                                          [&](const ObserverEvent &) { return ++callbacks < 4; });
    check(result.termination == StationaryTermination::cancelled &&
              result.statistics.accepted_steps == 3 &&
              validate_observer(parameters, result.observer) == ObserverStatus::valid,
          "stationary cancellation retains last accepted event");
    auto options = StationaryOptions{};
    options.maximum_attempts = 1;
    result = advance_stationary_observer(parameters, initial, 10.0, options);
    check(result.termination == StationaryTermination::work_limit &&
              result.statistics.accepted_steps == 1,
          "stationary attempt budget is bounded");
    auto invalid = initial;
    invalid.tetrad[1][1] *= 1.01;
    result = advance_stationary_observer(parameters, invalid, invalid.position[0]);
    check(result.termination == StationaryTermination::invalid_initial_state &&
              result.observer_status == ObserverStatus::invalid_tetrad &&
              result.observer.position == kerr::math::Vector4d{},
          "zero-time validation refuses bad frame without silent repair");
    invalid = initial;
    invalid.tetrad[0][0] *= 1.01;
    result = advance_stationary_observer(parameters, invalid, invalid.position[0]);
    check(result.observer_status == ObserverStatus::invalid_four_velocity,
          "zero-time validation refuses nonunit stationary velocity");
    invalid = make_eulerian_observer(parameters, initial.position).observer;
    result = advance_stationary_observer(parameters, invalid, 10.0);
    check(result.termination == StationaryTermination::invalid_initial_state &&
              result.observer_status == ObserverStatus::invalid_four_velocity,
          "moving observer cannot enter fixed-coordinate driver");
    invalid = make_eulerian_observer(parameters, {1.0, 1.9, 0.0, 0.0}).observer;
    result = advance_stationary_observer(parameters, invalid, 10.0);
    check(result.termination == StationaryTermination::stationary_unavailable,
          "stationary evolution refuses ergoregion");
    invalid = initial;
    invalid.position[2] = std::numeric_limits<double>::quiet_NaN();
    result = advance_stationary_observer(parameters, invalid, 10.0);
    check(result.termination == StationaryTermination::invalid_initial_state &&
              result.observer_status == ObserverStatus::non_finite_input &&
              kerr::numerics::finite(result.observer.position),
          "nonfinite initial data returns finite cleared event");
    result = advance_stationary_observer(parameters, initial, std::numeric_limits<double>::infinity());
    check(result.termination == StationaryTermination::non_finite &&
              result.observer.position == initial.position,
          "nonfinite target retains valid initial state");
    StationaryAxes axes = axes_of(initial), derivative{};
    axes[0] = std::numeric_limits<double>::infinity();
    derivative.fill(1.0);
    check(stationary_fermi_walker_rhs(parameters, initial.position, axes, derivative) ==
              ObserverStatus::non_finite_input && derivative == StationaryAxes{},
          "nonfinite stationary RHS input clears output");
    derivative.fill(1.0);
    check(stationary_fermi_walker_rhs(parameters, {0.0, 0.0, 0.0, 0.0},
                                      axes_of(initial), derivative) == ObserverStatus::invalid_geometry &&
              derivative == StationaryAxes{},
          "singular stationary geometry clears derivative");

    const auto near = make_stationary_observer(parameters,
        {0.0, std::hypot(2.1, parameters.spin()), 0.0, 0.0}).observer;
    options = {};
    options.relative_tolerance = 1.0e-20;
    options.axis_tolerance = 1.0e-20;
    options.initial_step = options.maximum_step = 100.0;
    options.maximum_rejections = 1;
    result = advance_stationary_observer(parameters, near, 200.0, options);
    check(result.termination == StationaryTermination::rejection_limit &&
              result.statistics.rejected_steps == 1 && result.observer.position == near.position,
          "stationary rejection budget retains initial state");
    options.maximum_rejections = 32;
    options.minimum_step = 100.0;
    result = advance_stationary_observer(parameters, near, 200.0, options);
    check(result.termination == StationaryTermination::minimum_step,
          "stationary minimum step failure");
    invalid = initial;
    invalid.position[0] = 1.0e20;
    result = advance_stationary_observer(parameters, invalid, std::nextafter(1.0e20, 2.0e20));
    check(result.termination == StationaryTermination::minimum_step,
          "coordinate clock stagnation terminates explicitly");
    invalid = initial;
    invalid.proper_time = 1.0e20;
    result = advance_stationary_observer(parameters, invalid, 2.0);
    check(result.termination == StationaryTermination::minimum_step,
          "proper clock stagnation terminates explicitly");

    bool threw{};
    try { (void)advance_stationary_observer(parameters, initial, 0.0); }
    catch (const std::invalid_argument &) { threw = true; }
    check(threw, "backward target rejected");
    threw = false;
    options = {};
    options.axis_tolerance = std::numeric_limits<double>::quiet_NaN();
    try { (void)advance_stationary_observer(parameters, initial, 10.0, options); }
    catch (const std::invalid_argument &) { threw = true; }
    check(threw, "invalid stationary options rejected");
}
} // namespace

int main() {
    try {
        test_schwarzschild();
        test_kerr_precession();
        test_inclined_reference_and_callbacks();
        test_failures();
    } catch (const std::exception &error) {
        ++failures;
        std::cerr << "Exception: " << error.what() << '\n';
    }
    std::cout << checks << " stationary checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
