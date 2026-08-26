#pragma once

#include "kerr/physics/RayTracer.hpp"
#include "kerr/physics/Transport.hpp"

#include <optional>

namespace kerr::physics {

struct ObserverTraceOptions final {
    double relative_tolerance{1.0e-10};
    double position_tolerance{1.0e-11};
    double momentum_tolerance{1.0e-12};
    double frame_tolerance{1.0e-12};
    double frame_error_limit{1.0e-9};
    double initial_step{0.02};
    double minimum_step{1.0e-12};
    double maximum_step{0.5};
    double proper_time_limit{200.0}; // Elapsed duration, not an absolute clock reading.
    std::optional<double> coordinate_time_limit{}; // Absolute ingoing KS t.
    double escape_radius{100.0};
    double event_affine_tolerance{1.0e-10}; // Measured in proper time.
    double constraint_limit{1.0e-7};
    double invariant_drift_limit{1.0e-7};
    std::size_t maximum_attempts{100000};
    std::size_t maximum_rejections{32};
    bool stop_at_horizon{true};
    bool stop_at_escape{true};
    bool stop_at_disk{false};
    double disk_inner_radius{6.0};
    double disk_outer_radius{30.0};
};

struct ObserverTraceStatistics final {
    RayStatistics geodesic{};
    double maximum_frame_error{};      // Absolute maximum |g(e_a,e_b)-eta_ab|.
    double maximum_mass_shell_error{}; // Absolute |2H+1|.
};

struct ObserverTraceResult final {
    RayTermination termination{RayTermination::invalid_initial_state};
    TransportState state{};   // Last accepted state; finite zeros for invalid initial data.
    ObserverEvent observer{}; // Includes absolute proper-clock reading and transported axes.
    double proper_time{};     // Elapsed proper time from the initial event.
    GeodesicDiagnostics diagnostics{};
    ObserverTraceStatistics statistics{};
};

// Initial and accepted finite events only, including the final dense event.
// The first argument is elapsed proper time. Returning false cancels the trace.
using ObserverTraceCallback =
    std::function<bool(double, const ObserverEvent &, const GeodesicDiagnostics &)>;

// Coupled DOPRI5 evolution of a geodesic and its three parallel transported axes.
// Never renormalizes momenta or resets/projects axes. The error norm is the maximum
// of separate 8-component geodesic and 12-component frame RMS norms. Frame gates
// are absolute and cannot be relaxed above 1e-9. Invariants refer to this initial
// event; an owner joining traces must additionally monitor lifetime invariants.
// Configuration errors throw invalid_argument. Runtime failures retain the last
// valid event. Coordinate targets must be finite and no earlier than initial t;
// an equal target returns immediately. The earliest eligible event terminates.
[[nodiscard]] ObserverTraceResult trace_observer(const KerrParameters &parameters,
                                                 const ObserverEvent &initial,
                                                 const ObserverTraceOptions &options = {},
                                                 const ObserverTraceCallback &callback = {});

} // namespace kerr::physics
