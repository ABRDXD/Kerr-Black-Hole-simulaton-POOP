#pragma once

#include "kerr/physics/Observer.hpp"

#include <array>
#include <cstddef>
#include <functional>
#include <string_view>

namespace kerr::physics {

// Three contravariant spatial tetrad legs; clocks and fixed position are analytic.
using StationaryAxes = std::array<double, 12>;

struct StationaryKinematics final {
    ObserverStatus status{ObserverStatus::non_finite_input};
    math::Vector4d velocity{}, acceleration{};
    double proper_time_rate{}; // d tau / d t = sqrt(-g_tt).
    double proper_acceleration{}; // sqrt(g(a,a)).
    [[nodiscard]] bool valid() const noexcept { return status == ObserverStatus::valid; }
};

[[nodiscard]] StationaryKinematics stationary_kinematics(const KerrParameters &parameters,
                                                         math::Vector4d position) noexcept;

// Coordinate-time derivative for fixed Cartesian coordinates. Uses the full
// Fermi-Walker generator -Gamma(u) + u*a_flat - a*u_flat, divided by u^t.
// Intermediate axes need only be finite; the adaptive driver validates frames.
// Output is cleared on failure and may alias input.
[[nodiscard]] ObserverStatus stationary_fermi_walker_rhs(const KerrParameters &parameters,
                                                         math::Vector4d position,
                                                         const StationaryAxes &axes,
                                                         StationaryAxes &derivative) noexcept;

enum class StationaryTermination {
    coordinate_time_limit,
    invalid_initial_state,
    stationary_unavailable,
    non_finite,
    frame_violation,
    work_limit,
    minimum_step,
    rejection_limit,
    cancelled,
};

[[nodiscard]] std::string_view stationary_termination_name(StationaryTermination reason) noexcept;

struct StationaryOptions final {
    double relative_tolerance{1.0e-10};
    double axis_tolerance{1.0e-12};
    double initial_step{0.02}; // All steps are measured in KS coordinate time.
    double minimum_step{1.0e-12};
    double maximum_step{0.5};
    std::size_t maximum_attempts{100000};
    std::size_t maximum_rejections{32};
};

struct StationaryStatistics final {
    std::size_t accepted_steps{}, rejected_steps{}, rhs_evaluations{};
    double maximum_frame_residual{}, smallest_step{}, largest_step{};
};

struct StationaryResult final {
    StationaryTermination termination{StationaryTermination::invalid_initial_state};
    ObserverStatus observer_status{ObserverStatus::non_finite_input};
    ObserverEvent observer{}; // Last valid event; zero for invalid initial input.
    StationaryKinematics kinematics{};
    StationaryStatistics statistics{};
};

using StationaryObserverCallback = std::function<bool(const ObserverEvent &)>;

// Advances only forward to an absolute KS coordinate time, including a no-op at
// the initial time. Initial temporal leg must equal the analytic stationary
// velocity returned by stationary_kinematics/make_stationary_observer. Spatial
// legs retain their supplied orientation; neither normalization nor projection
// is applied. Callbacks see initial and accepted frames only, and false cancels.
// Invalid numerical options or a backwards target throw std::invalid_argument.
[[nodiscard]] StationaryResult advance_stationary_observer(
    const KerrParameters &parameters, const ObserverEvent &initial, double coordinate_time,
    const StationaryOptions &options = {}, const StationaryObserverCallback &observer = {});

} // namespace kerr::physics
