#pragma once

#include "kerr/physics/Geodesic.hpp"

#include <array>
#include <cstdint>

namespace kerr::physics {

enum class ObserverStatus : std::uint8_t {
    valid,
    non_finite_input,
    invalid_geometry,
    invalid_four_velocity,
    stationary_unavailable,
    circular_orbit_unavailable,
    invalid_tetrad,
    invalid_direction,
    invalid_frequency,
    numerical_failure,
};

// Cartesian Kerr-Schild coordinates. The first basis vector is the future unit
// four-velocity; the remaining vectors form a right-handed local spatial frame.
struct ObserverEvent final {
    math::Vector4d position{};
    double proper_time{};
    std::array<math::Vector4d, 4> tetrad{};
};

struct ObserverEvaluation final {
    ObserverStatus status{ObserverStatus::non_finite_input};
    ObserverEvent observer{};
    [[nodiscard]] bool valid() const noexcept { return status == ObserverStatus::valid; }
};

// Strictly validates a unit future-timelike velocity; never silently normalizes
// an arbitrary supplied four-velocity. The spatial basis is the rotation-free
// boost of the symmetric Cartesian Eulerian frame, not an orbit-transport rule.
[[nodiscard]] ObserverEvaluation make_observer(const KerrParameters &parameters,
                                               math::Vector4d position,
                                               math::Vector4d unit_future_velocity,
                                               double proper_time = 0.0) noexcept;

[[nodiscard]] ObserverEvaluation make_eulerian_observer(const KerrParameters &parameters,
                                                        math::Vector4d position,
                                                        double proper_time = 0.0) noexcept;

// Velocity components are physical velocities in the Eulerian spatial frame.
[[nodiscard]] ObserverEvaluation make_local_velocity_observer(const KerrParameters &parameters,
                                                              math::Vector4d position,
                                                              math::Vector3d local_velocity,
                                                              double proper_time = 0.0) noexcept;

// Fixed Cartesian coordinates: supported only where g_tt < 0.
[[nodiscard]] ObserverEvaluation make_stationary_observer(const KerrParameters &parameters,
                                                          math::Vector4d position,
                                                          double proper_time = 0.0) noexcept;

// Requires z=0 and r strictly above the selected equatorial photon orbit.
// Includes unstable circular timelike orbits below the ISCO. Sense is relative
// to signed spin; at zero spin prograde means increasing coordinate azimuth.
[[nodiscard]] ObserverEvaluation make_circular_observer(const KerrParameters &parameters,
                                                        math::Vector4d position, OrbitSense sense,
                                                        double proper_time = 0.0) noexcept;

// Checks finite data, future orientation, absolute orthonormal residual <=1e-9,
// and positive tetrad determinant. Extremely ill-conditioned frames fail.
[[nodiscard]] ObserverStatus validate_observer(const KerrParameters &parameters,
                                               const ObserverEvent &observer) noexcept;

struct CameraRayEvaluation final {
    ObserverStatus status{ObserverStatus::non_finite_input};
    GeodesicState state{};
    [[nodiscard]] bool valid() const noexcept { return status == ObserverStatus::valid; }
};

// Nonzero local direction is normalized. Future k=nu*(u+n); past k=nu*(-u+n).
// Thus the same direction is spatial propagation in either time orientation;
// negating a whole ray requires negating direction and reversing orientation.
// Frequency must be positive. Failure results contain only finite zero data.
[[nodiscard]] CameraRayEvaluation make_camera_ray(const KerrParameters &parameters,
                                                  const ObserverEvent &observer,
                                                  math::Vector3d direction, double frequency = 1.0,
                                                  bool past_directed = false) noexcept;

struct FrequencyEvaluation final {
    ObserverStatus status{ObserverStatus::non_finite_input};
    double frequency{};
    [[nodiscard]] bool valid() const noexcept { return status == ObserverStatus::valid; }
};

// Physical energy/frequency = -p_mu u^mu for a future momentum, +p_mu u^mu
// for a past momentum. Does not assert that an arbitrary supplied p is null.
[[nodiscard]] FrequencyEvaluation measured_frequency(const KerrParameters &parameters,
                                                     const ObserverEvent &observer,
                                                     math::Covector4d covariant_momentum,
                                                     bool past_directed = false) noexcept;

} // namespace kerr::physics

// refactor: polish Observer.hpp implementation details

// refactor: polish Observer.hpp implementation details

// refactor: polish Observer.hpp implementation details

// refactor: polish Observer.hpp implementation details

// refactor: polish Observer.hpp implementation details
