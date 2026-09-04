#include "kerr/app/ObserverSession.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace kerr::app;
using namespace kerr::physics;
std::size_t checks{}, failures{};
void check(bool condition, const char *label) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << label << '\n';
    }
}
template <typename Function> void throws(Function &&call, const char *label) {
    bool caught{};
    try {
        call();
    } catch (const std::exception &) {
        caught = true;
    }
    check(caught, label);
}
ObserverEvent circular(const KerrParameters &parameters) {
    const auto event = make_circular_observer(
        parameters, {0.0, std::hypot(10.0, parameters.spin()), 0.0, 0.0}, OrbitSense::prograde);
    if (!event.valid())
        throw std::runtime_error{"Bad test circular initial data"};
    return event.observer;
}
void clock_and_restart_tests() {
    const KerrParameters parameters{1.0, 0.0};
    const auto orbit = circular(parameters);
    const auto stationary = make_stationary_observer(parameters, orbit.position).observer;
    SessionOptions options{};
    options.history_capacity = 5;
    ObserverSession session{parameters, 0.0, options};
    const auto orbit_id = session.add("orbiter", orbit);
    const auto stationary_id = session.add("stationary", stationary, ObserverMotion::stationary);
    check(orbit_id != stationary_id, "independent IDs");
    const auto old_snapshot = session.snapshot();
    for (double time : {2.0, 4.0, 8.0, 16.0, 32.0}) {
        const auto result = session.advance_to(time);
        check(result.coordinate_time == time, "session coordinate slice");
        for (const auto &view : result.observers) {
            check(view.lifecycle == ObserverLifecycle::active, "observer remains active");
            check(std::abs(view.latest.event.position[0] - time) < 2.0e-11,
                  "observer reached coordinate target");
            check(validate_observer(parameters, view.latest.event) == ObserverStatus::valid,
                  "transported frame valid");
        }
        check(std::abs(result.observers[0].latest.event.proper_time - time * std::sqrt(0.7)) <
                  2.0e-8,
              "orbital proper clock");
        check(std::abs(result.observers[1].latest.event.proper_time - time * std::sqrt(0.8)) <
                  2.0e-13,
              "stationary proper clock");
    }
    check(old_snapshot.observers[0].latest.event.position[0] == 0.0,
          "snapshots do not alias mutable session");
    auto history = session.history(orbit_id);
    check(history.size() == 5, "bounded history capacity");
    check(session.snapshot().observers[0].samples_dropped > 0, "discard count visible");
    for (std::size_t i = 1; i < history.size(); ++i) {
        check(history[i].event.position[0] > history[i - 1].event.position[0],
              "history coordinate times strictly increase");
        check(history[i].event.proper_time > history[i - 1].event.proper_time,
              "history proper times strictly increase");
    }
    history[0].event.position[0] = -1.0;
    check(session.history(orbit_id)[0].event.position[0] > 0.0, "history copies isolated");
    const auto latest = session.snapshot().observers[0].latest.event;
    ObserverTraceOptions direct_options{};
    direct_options.coordinate_time_limit = 32.0;
    const auto direct = trace_observer(parameters, orbit, direct_options);
    check(direct.termination == RayTermination::coordinate_time_limit, "direct trace completes");
    for (std::size_t mu = 0; mu < 4; ++mu) {
        check(std::abs(latest.position[mu] - direct.observer.position[mu]) < 2.0e-8,
              "chunked position consistency");
        for (std::size_t leg = 0; leg < 4; ++leg)
            check(std::abs(latest.tetrad[leg][mu] - direct.observer.tetrad[leg][mu]) < 2.0e-9,
                  "chunked transported orientation consistency");
    }
    const auto samples_before = session.history(orbit_id).size();
    static_cast<void>(session.advance_to(32.0));
    check(session.history(orbit_id).size() == samples_before, "zero advance creates no duplicate");
    throws([&] { static_cast<void>(session.advance_to(31.0)); }, "backward advance rejected");
    throws([&] { static_cast<void>(session.advance_to(std::numeric_limits<double>::quiet_NaN())); },
           "non-finite advance rejected");
    check(session.snapshot().coordinate_time == 32.0, "invalid advance leaves session unchanged");
    throws([&] { static_cast<void>(session.add("wrong time", orbit)); },
           "off-slice addition rejected");
    session.remove(stationary_id);
    check(session.snapshot().observers.size() == 1, "remove only selected observer");
    throws([&] { static_cast<void>(session.history(stationary_id)); }, "removed ID rejected");
    session.reset(KerrParameters{2.0, -1.0}, 5.0);
    const auto reset = session.snapshot();
    check(reset.generation == 2 && reset.observers.empty() && reset.coordinate_time == 5.0,
          "reset clears observers and clock");
    check(reset.mass == 2.0 && reset.spin == -1.0, "reset replaces spacetime");
    throws([&] { static_cast<void>(session.history(orbit_id)); }, "old generation invalidated");
    auto new_event =
        make_stationary_observer(session.parameters(), {5.0, 20.0, 0.0, 0.0}, 7.0).observer;
    const auto new_id = session.add("new observer", new_event, ObserverMotion::stationary);
    check(new_id.generation != orbit_id.generation, "new generation cannot alias old ID");
    check(new_id.serial == 1, "serial resets within new generation");
    const auto restarted = session.advance_to(6.0);
    check(restarted.observers[0].latest.event.proper_time > 7.0,
          "proper clock offset survives scheduling");
    auto reset_orbit = make_circular_observer(session.parameters(),
                                              {6.0, std::hypot(20.0, -1.0), 0.0, 0.0},
                                              OrbitSense::retrograde, 11.0);
    check(reset_orbit.valid(), "changed mass and spin permit a new geodesic");
    const auto reset_orbit_id = session.add("reset geodesic", reset_orbit.observer);
    check(reset_orbit_id.generation == new_id.generation && reset_orbit_id.serial == 2,
          "reset generation identifies stationary and geodesic observers");
    const auto reset_advanced = session.advance_to(7.0);
    const auto &reset_view = reset_advanced.observers[1];
    check(reset_view.lifecycle == ObserverLifecycle::active &&
              std::abs(reset_view.latest.event.position[0] - 7.0) < 2.0e-11,
          "new geodesic advances in reset spacetime");
    check(std::abs(reset_view.latest.diagnostics.radius - 20.0) < 2.0e-8,
          "new geodesic uses the replacement mass and spin");
    check(reset_view.latest.event.proper_time > 11.0 &&
              validate_observer(session.parameters(), reset_view.latest.event) ==
                  ObserverStatus::valid,
          "reset geodesic retains its clock offset and valid orientation");
}
void short_slice_tests() {
    const KerrParameters parameters{1.0, 0.8};
    for (double start : {0.0, 1.0}) {
        auto event = circular(parameters);
        event.position[0] = start;
        ObserverSession session{parameters, start};
        const auto id = session.add("short slices", event);
        double target = start;
        for (std::size_t slice = 0; slice < 16; ++slice) {
            target += 1.0e-13;
            const auto result = session.advance_to(target);
            const auto &view = result.observers[0];
            check(view.lifecycle == ObserverLifecycle::active,
                  "sub-minimum-step coordinate slice remains active");
            check(std::abs(view.latest.event.position[0] - target) <=
                      2.0 * std::numeric_limits<double>::epsilon() * std::max(1.0, target),
                  "very short consecutive slice reaches the requested clock");
        }
        const auto history = session.history(id);
        check(history.size() == 17, "short slices keep one new accepted sample per advance");
        for (std::size_t i = 1; i < history.size(); ++i) {
            check(history[i].event.position[0] > history[i - 1].event.position[0] &&
                      history[i].event.proper_time > history[i - 1].event.proper_time,
                  "very short slice histories retain strict clock ordering");
        }
    }
}
void lifetime_invariant_tests() {
    const KerrParameters parameters{1.0, 0.8};
    SessionOptions options{};
    options.history_capacity = 512;
    options.geodesic.initial_step = 0.5;
    options.geodesic.maximum_step = 0.5;
    options.geodesic.invariant_drift_limit = 1.0e-12;
    ObserverSession session{parameters, 0.0, options};
    const auto id = session.add("lifetime gate", circular(parameters));
    static_cast<void>(session.add(
        "stationary peer", make_stationary_observer(parameters, {0.0, 12.0, 0.0, 0.0}).observer,
        ObserverMotion::stationary));
    const auto birth = session.snapshot().observers[0].latest.diagnostics;
    SessionSnapshot result{};
    for (std::size_t slice = 1; slice <= 2000; ++slice) {
        result = session.advance_to(static_cast<double>(slice));
        if (result.observers[0].lifecycle != ObserverLifecycle::active)
            break;
    }
    const auto &failed = result.observers[0];
    check(failed.lifecycle == ObserverLifecycle::failed &&
              failed.termination == "lifetime_invariant_violation",
          "joined traces enforce birth invariants instead of resetting the drift baseline");
    check(result.observers[1].lifecycle == ObserverLifecycle::active &&
              result.observers[1].latest.event.position[0] == result.coordinate_time,
          "lifetime invariant failure leaves stationary peer active");
    const auto history = session.history(id);
    check(!history.empty() && history.back().event.position == failed.latest.event.position &&
              history.back().event.tetrad == failed.latest.event.tetrad,
          "lifetime failure retains the last accepted history event");
    check(failed.maximum_invariant_drift <= options.geodesic.invariant_drift_limit,
          "lifetime diagnostic excludes rejected samples");
    for (const auto &sample : history) {
        const auto within_limit = [&](double value, double initial) {
            return std::abs(value - initial) / std::max(1.0, std::abs(initial)) <=
                   options.geodesic.invariant_drift_limit;
        };
        check(within_limit(sample.diagnostics.energy, birth.energy) &&
                  within_limit(sample.diagnostics.angular_momentum, birth.angular_momentum) &&
                  within_limit(sample.diagnostics.carter, birth.carter),
              "stored history never includes the lifetime-rejected event");
        check(validate_observer(parameters, sample.event) == ObserverStatus::valid,
              "lifetime failure history retains finite valid frames");
    }
    const auto resumed = session.advance_to(result.coordinate_time + 1.0);
    check(resumed.observers[0].latest.event.position == failed.latest.event.position &&
              resumed.observers[0].latest.event.tetrad == failed.latest.event.tetrad &&
              session.history(id).size() == history.size(),
          "lifetime failure freezes the observer and its history");
}
void isolated_failure_tests() {
    const KerrParameters parameters{1.0, 0.0};
    SessionOptions options{};
    options.history_capacity = 256;
    ObserverSession session{parameters, 0.0, options};
    const double w = std::sqrt(2.0 / 4.0);
    const auto falling =
        make_observer(parameters, {0.0, 4.0, 0.0, 0.0}, {1.0 + w * w / (1.0 + w), -w, 0.0, 0.0})
            .observer;
    const auto fall_id = session.add("infall", falling);
    static_cast<void>(session.add("circular", circular(parameters)));
    const auto first = session.advance_to(8.0);
    check(first.observers[0].lifecycle == ObserverLifecycle::terminated &&
              first.observers[0].termination == "horizon",
          "capture is physical termination");
    check(first.observers[0].latest.event.position[0] < 8.0,
          "captured observer retains event time");
    check(first.observers[1].lifecycle == ObserverLifecycle::active,
          "capture does not stop other observer");
    const auto old_samples = session.history(fall_id).size();
    const auto second = session.advance_to(10.0);
    check(second.observers[0].latest.event.position == first.observers[0].latest.event.position,
          "terminal observer stays frozen");
    check(session.history(fall_id).size() == old_samples,
          "frozen history receives no fake samples");
    check(std::abs(second.observers[1].latest.event.position[0] - 10.0) < 2.0e-11,
          "surviving observer continues");

    options.geodesic.maximum_attempts = 1;
    ObserverSession limited{parameters, 0.0, options};
    static_cast<void>(limited.add("limited", circular(parameters)));
    static_cast<void>(limited.add(
        "stationary", make_stationary_observer(parameters, {0.0, 10.0, 0.0, 0.0}).observer,
        ObserverMotion::stationary));
    const auto failure = limited.advance_to(5.0);
    check(failure.observers[0].lifecycle == ObserverLifecycle::failed &&
              failure.observers[0].termination == "work_limit",
          "per-observer work failure classified");
    check(failure.observers[1].lifecycle == ObserverLifecycle::active &&
              failure.observers[1].latest.event.position[0] == 5.0,
          "independent motion survives peer failure");
    check(validate_observer(parameters, failure.observers[0].latest.event) == ObserverStatus::valid,
          "failed observer retains finite valid frame");
}
void input_tests() {
    const KerrParameters parameters{1.0, 0.8};
    SessionOptions options{};
    options.maximum_observers = 1;
    ObserverSession session{parameters, 0.0, options};
    const auto event = circular(parameters);
    throws([&] { static_cast<void>(session.add("", event)); }, "empty label rejected");
    auto bad = event;
    bad.tetrad[2][1] += 0.3;
    throws([&] { static_cast<void>(session.add("bad", bad)); }, "corrupted axes rejected");
    check(session.snapshot().observers.empty(), "bad additions do not mutate session");
    static_cast<void>(session.add("valid", event));
    throws([&] { static_cast<void>(session.add("over capacity", event)); },
           "observer capacity bounded");
    throws([&] { session.reset(parameters, std::numeric_limits<double>::infinity()); },
           "invalid reset rejected");
    check(session.snapshot().generation == 1 && session.snapshot().observers.size() == 1,
          "invalid reset is atomic");
    options.history_capacity = 1;
    throws([&] { ObserverSession invalid{parameters, 0.0, options}; },
           "history capacity validated");
    options.history_capacity = 5;
    options.geodesic.relative_tolerance = -1.0;
    ObserverSession invalid_options{parameters, 0.0, options};
    throws([&] { static_cast<void>(invalid_options.add("invalid controls", event)); },
           "solver options validated on addition");
    check(invalid_options.snapshot().observers.empty(), "invalid controls preserve empty session");
}
} // namespace
int main() {
    try {
        clock_and_restart_tests();
        short_slice_tests();
        lifetime_invariant_tests();
        isolated_failure_tests();
        input_tests();
        std::cout << checks << " session checks, " << failures << " failures\n";
        return failures == 0 ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
