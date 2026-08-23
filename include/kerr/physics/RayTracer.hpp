#pragma once

#include "kerr/physics/Geodesic.hpp"

#include <cstddef>
#include <functional>
#include <string_view>

namespace kerr::physics {

enum class RayTermination {
    affine_limit,
    proper_time_limit,
    coordinate_time_limit,
    horizon,
    escape,
    disk,
    initially_inside_horizon,
    work_limit,
    invalid_initial_state,
    singular_geometry,
    non_finite,
    minimum_step,
    rejection_limit,
    constraint_violation,
    frame_violation,
    event_failure,
    cancelled
};
[[nodiscard]] std::string_view termination_name(RayTermination reason) noexcept;

struct RayOptions final {
    double relative_tolerance{1.0e-10};
    double position_tolerance{1.0e-11};
    double momentum_tolerance{1.0e-12};
    double initial_step{0.02};
    double minimum_step{1.0e-12};
    double maximum_step{0.5};
    double affine_limit{200.0};
    double escape_radius{100.0};
    double event_affine_tolerance{1.0e-10};
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

// Same numerical/event controls as RayOptions, with a proper-time clock.
// Kept flat so both option types support designated aggregate initialization.
struct WorldlineOptions final {
    double relative_tolerance{1.0e-10};
    double position_tolerance{1.0e-11};
    double momentum_tolerance{1.0e-12};
    double initial_step{0.02};
    double minimum_step{1.0e-12};
    double maximum_step{0.5};
    double proper_time_limit{200.0};
    double escape_radius{100.0};
    double event_affine_tolerance{1.0e-10}; // Measured in proper time for worldlines.
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

struct RayStatistics final {
    std::size_t accepted_steps{}, rejected_steps{}, rhs_evaluations{};
    double maximum_constraint{}, maximum_energy_drift{}, maximum_lz_drift{}, maximum_carter_drift{};
    double smallest_step{}, largest_step{};
};

struct RayResult final {
    RayTermination termination{RayTermination::invalid_initial_state};
    GeodesicState state{}; // Last valid state; zero if initial input was invalid.
    double affine_parameter{};
    GeodesicDiagnostics diagnostics{};
    RayStatistics statistics{};
};

// Callback sees the initial point, accepted steps, and the located final event.
// Return false for bounded cancellation. Never called for rejected trials.
using RayObserver = std::function<bool(double, const GeodesicState &, const GeodesicDiagnostics &)>;

struct WorldlineResult final {
    RayTermination termination{RayTermination::invalid_initial_state};
    GeodesicState state{}; // Last valid event; coordinate clock is state[0].
    double proper_time{};  // Elapsed proper time from the supplied initial event.
    GeodesicDiagnostics diagnostics{};
    RayStatistics statistics{};
};

// First callback argument is elapsed proper time; the remaining contract is RayObserver's.
using WorldlineObserver = RayObserver;

// Invalid configuration throws std::invalid_argument. Runtime ray failures are
// typed results. Lambda advances positively; use negated p_mu for past rays.
[[nodiscard]] RayResult trace_null_ray(const KerrParameters &parameters,
                                       const GeodesicState &initial, const RayOptions &options = {},
                                       const RayObserver &observer = {});

// Unit rest mass and future-directed motion only. Uses the same adaptive solver,
// invariant gates and dense event location as null rays. Completion is reported
// as proper_time_limit. Clocks use ingoing Cartesian KS t and proper time tau;
// separate worldline clocks do not by themselves define observable time dilation.
// Also enforces |2H+1|<=min(constraint_limit,1e-7) at every accepted point/event,
// so a large momentum scale cannot hide loss of unit proper-time normalization.
[[nodiscard]] WorldlineResult trace_timelike_worldline(const KerrParameters &parameters,
                                                       const GeodesicState &initial,
                                                       const WorldlineOptions &options = {},
                                                       const WorldlineObserver &observer = {});

} // namespace kerr::physics
