#include "kerr/app/ObserverSession.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace kerr::app {
namespace {
using namespace physics;
GeodesicDiagnostics diagnostics_at(const KerrParameters &parameters, const ObserverEvent &event) {
    const auto state = make_transport_state(parameters, event);
    if (!state.valid())
        throw std::invalid_argument{"Invalid observer frame"};
    GeodesicState geodesic{};
    std::copy_n(state.state.begin(), 8, geodesic.begin());
    const auto diagnostics = diagnose_timelike_geodesic(parameters, geodesic);
    if (!diagnostics.valid())
        throw std::invalid_argument{"Invalid observer diagnostics"};
    return diagnostics;
}
bool physical_stop(RayTermination termination) {
    return termination == RayTermination::horizon || termination == RayTermination::escape ||
           termination == RayTermination::disk ||
           termination == RayTermination::initially_inside_horizon;
}
} // namespace

ObserverSession::ObserverSession(physics::KerrParameters parameters, double initial_coordinate_time,
                                 SessionOptions options)
    : parameters_{parameters}, options_{std::move(options)},
      coordinate_time_{initial_coordinate_time} {
    if (!std::isfinite(coordinate_time_) || options_.maximum_observers == 0 ||
        options_.history_capacity < 2)
        throw std::invalid_argument{"Session needs finite time, positive observer capacity and at "
                                    "least two history samples"};
}

ObserverId ObserverSession::add(std::string label, const physics::ObserverEvent &initial,
                                ObserverMotion motion) {
    if (entries_.size() >= options_.maximum_observers)
        throw std::length_error{"Observer capacity reached"};
    if (label.empty() || initial.position[0] != coordinate_time_)
        throw std::invalid_argument{
            "Observer needs a label and must start on the session's current coordinate slice"};
    if (next_serial_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error{"Observer identifier exhausted"};
    if (motion == ObserverMotion::geodesic) {
        auto controls = options_.geodesic;
        controls.coordinate_time_limit = initial.position[0];
        controls.proper_time_limit = 1.0;
        const auto checked = trace_observer(parameters_, initial, controls);
        if (checked.termination != RayTermination::coordinate_time_limit)
            throw std::invalid_argument{"Invalid geodesic observer or initial stopping surface"};
    } else if (motion == ObserverMotion::stationary) {
        const auto checked = advance_stationary_observer(parameters_, initial, initial.position[0],
                                                         options_.stationary);
        if (checked.termination != StationaryTermination::coordinate_time_limit)
            throw std::invalid_argument{"Invalid stationary observer"};
    } else
        throw std::invalid_argument{"Unknown observer motion"};
    Entry entry{};
    entry.view.id = {generation_, next_serial_};
    entry.view.label = std::move(label);
    entry.view.motion = motion;
    entry.birth = diagnostics_at(parameters_, initial);
    append(entry, {initial, entry.birth});
    entries_.push_back(std::move(entry));
    ++next_serial_;
    return entries_.back().view.id;
}

void ObserverSession::remove(ObserverId id) {
    const auto found = std::find_if(entries_.begin(), entries_.end(),
                                    [&](const Entry &entry) { return entry.view.id == id; });
    if (found == entries_.end())
        throw std::out_of_range{"Unknown or stale observer ID"};
    entries_.erase(found);
}

SessionSnapshot ObserverSession::snapshot() const {
    SessionSnapshot result{
        generation_, parameters_.mass(), parameters_.spin(), coordinate_time_, {}};
    result.observers.reserve(entries_.size());
    for (const auto &entry : entries_)
        result.observers.push_back(entry.view);
    return result;
}

std::vector<ObserverSample> ObserverSession::history(ObserverId id) const {
    const auto found = std::find_if(entries_.begin(), entries_.end(),
                                    [&](const Entry &entry) { return entry.view.id == id; });
    if (found == entries_.end())
        throw std::out_of_range{"Unknown or stale observer ID"};
    return {found->samples.begin(), found->samples.end()};
}

void ObserverSession::append(Entry &entry, const ObserverSample &sample) const {
    if (!entry.samples.empty() && sample.event.position[0] == entry.view.latest.event.position[0] &&
        sample.event.proper_time == entry.view.latest.event.proper_time)
        return; // A restart's initial callback is not a new sample.
    entry.view.latest = sample;
    entry.samples.push_back(sample);
    if (entry.samples.size() > options_.history_capacity) {
        entry.samples.pop_front();
        ++entry.view.samples_dropped;
    }
}

double ObserverSession::drift(const physics::GeodesicDiagnostics &value,
                              const physics::GeodesicDiagnostics &initial) noexcept {
    const auto scaled = [](double current, double start) {
        return std::abs(current - start) / std::max(1.0, std::abs(start));
    };
    return std::max({scaled(value.energy, initial.energy),
                     scaled(value.angular_momentum, initial.angular_momentum),
                     scaled(value.carter, initial.carter)});
}

SessionSnapshot ObserverSession::advance_to(double target) {
    if (!std::isfinite(target) || target < coordinate_time_ ||
        !std::isfinite(target - coordinate_time_))
        throw std::invalid_argument{"Session coordinate target must be finite and nondecreasing"};
    if (target == coordinate_time_)
        return snapshot();
    auto candidates =
        entries_; // Configuration/exception failure leaves the entire session unchanged.
    for (auto &entry : candidates) {
        if (entry.view.lifecycle != ObserverLifecycle::active)
            continue;
        const auto initial = entry.view.latest.event;
        if (entry.view.motion == ObserverMotion::geodesic) {
            auto controls = options_.geodesic;
            controls.coordinate_time_limit = target;
            // In this chart u^t=gamma*sqrt(1+F)>=1, hence delta tau<=delta t.
            controls.proper_time_limit = std::nextafter(target - initial.position[0],
                                                        std::numeric_limits<double>::infinity());
            if (!std::isfinite(controls.proper_time_limit))
                throw std::invalid_argument{"Coordinate interval is too large"};
            bool lifetime_failure{};
            const auto result = trace_observer(
                parameters_, initial, controls,
                [&](double, const ObserverEvent &event, const GeodesicDiagnostics &diagnostics) {
                    const double invariant_error = drift(diagnostics, entry.birth);
                    if (invariant_error > controls.invariant_drift_limit) {
                        lifetime_failure = true;
                        return false;
                    }
                    entry.view.maximum_invariant_drift =
                        std::max(entry.view.maximum_invariant_drift, invariant_error);
                    append(entry, {event, diagnostics});
                    return true;
                });
            entry.view.accepted_steps += result.statistics.geodesic.accepted_steps;
            entry.view.rejected_steps += result.statistics.geodesic.rejected_steps;
            if (lifetime_failure) {
                entry.view.lifecycle = ObserverLifecycle::failed;
                entry.view.termination = "lifetime_invariant_violation";
            } else if (result.termination != RayTermination::coordinate_time_limit) {
                entry.view.lifecycle = physical_stop(result.termination)
                                           ? ObserverLifecycle::terminated
                                           : ObserverLifecycle::failed;
                entry.view.termination = std::string{termination_name(result.termination)};
            }
        } else {
            const auto result = advance_stationary_observer(
                parameters_, initial, target, options_.stationary, [&](const ObserverEvent &event) {
                    append(entry, {event, diagnostics_at(parameters_, event)});
                    return true;
                });
            entry.view.accepted_steps += result.statistics.accepted_steps;
            entry.view.rejected_steps += result.statistics.rejected_steps;
            if (result.termination != StationaryTermination::coordinate_time_limit) {
                entry.view.lifecycle = ObserverLifecycle::failed;
                entry.view.termination =
                    std::string{stationary_termination_name(result.termination)};
            }
        }
    }
    entries_ = std::move(candidates);
    coordinate_time_ = target;
    return snapshot();
}

void ObserverSession::reset(physics::KerrParameters parameters, double initial_coordinate_time) {
    if (!std::isfinite(initial_coordinate_time))
        throw std::invalid_argument{"Reset coordinate time must be finite"};
    if (generation_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error{"Session generation exhausted"};
    parameters_ = parameters;
    coordinate_time_ = initial_coordinate_time;
    entries_.clear();
    ++generation_;
    next_serial_ = 1;
}

} // namespace kerr::app
