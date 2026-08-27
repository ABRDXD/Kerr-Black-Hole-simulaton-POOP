#pragma once

#include "kerr/physics/ObserverTracer.hpp"
#include "kerr/physics/StationaryObserver.hpp"

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace kerr::app {

struct ObserverId final {
    std::uint64_t generation{}, serial{};
    bool operator==(const ObserverId &) const = default;
};
enum class ObserverMotion { geodesic, stationary };
enum class ObserverLifecycle { active, terminated, failed };

struct ObserverSample final {
    physics::ObserverEvent event{};
    physics::GeodesicDiagnostics diagnostics{};
};
struct ObserverSnapshot final {
    ObserverId id{};
    std::string label;
    ObserverMotion motion{ObserverMotion::geodesic};
    ObserverLifecycle lifecycle{ObserverLifecycle::active};
    std::string termination{"active"};
    ObserverSample latest{};
    std::size_t samples_dropped{}, accepted_steps{}, rejected_steps{};
    double maximum_invariant_drift{}; // Geodesic lifetime E/Lz/Q, not reset per advance.
};
struct SessionSnapshot final {
    std::uint64_t generation{};
    double mass{}, spin{}, coordinate_time{}; // Requested slice; stopped observers retain older t.
    std::vector<ObserverSnapshot> observers;
};
struct SessionOptions final {
    std::size_t maximum_observers{16}, history_capacity{512};
    physics::ObserverTraceOptions geodesic{};
    physics::StationaryOptions stationary{};
};

// CPU, single-threaded owner. Snapshots/history are independent copies. No graphics state.
// All active observers advance to the same KS t slice; this is coordinate synchronization,
// not signal exchange. Runtime failure/event freezes only the affected observer.
class ObserverSession final {
  public:
    explicit ObserverSession(physics::KerrParameters parameters,
                             double initial_coordinate_time = 0.0, SessionOptions options = {});
    [[nodiscard]] ObserverId add(std::string label, const physics::ObserverEvent &initial,
                                 ObserverMotion motion = ObserverMotion::geodesic);
    void remove(ObserverId id);
    [[nodiscard]] SessionSnapshot snapshot() const;
    [[nodiscard]] std::vector<ObserverSample> history(ObserverId id) const;
    // Target is finite, absolute, and nondecreasing. Options' coordinate/proper-time limits
    // are owned by scheduling; remaining numerical controls apply. Invalid configuration
    // throws before committing any observer changes. Histories keep accepted finite samples.
    [[nodiscard]] SessionSnapshot advance_to(double coordinate_time);
    // Atomically replace immutable spacetime, clear all observers/histories, invalidate IDs.
    // Options persist; initial t defaults to zero. No previous observer can be resumed.
    void reset(physics::KerrParameters parameters, double initial_coordinate_time = 0.0);
    [[nodiscard]] const physics::KerrParameters &parameters() const noexcept { return parameters_; }

  private:
    struct Entry final {
        ObserverSnapshot view{};
        physics::GeodesicDiagnostics birth{};
        std::deque<ObserverSample> samples;
    };
    physics::KerrParameters parameters_;
    SessionOptions options_;
    double coordinate_time_{};
    std::uint64_t generation_{1}, next_serial_{1};
    std::vector<Entry> entries_;
    void append(Entry &entry, const ObserverSample &sample) const;
    [[nodiscard]] static double drift(const physics::GeodesicDiagnostics &value,
                                      const physics::GeodesicDiagnostics &initial) noexcept;
};

} // namespace kerr::app
