#pragma once

#include "kerr/physics/KerrSpacetime.hpp"

#include <array>

namespace kerr::physics {

// (t,x,y,z,p_t,p_x,p_y,p_z). Momenta are covariant, lambda is affine.
using GeodesicState = std::array<double, 8>;

struct MetricDerivatives final {
    GeometryStatus status{GeometryStatus::non_finite_input};
    // Spatial partials of g^{mu nu}; the time partial is identically zero.
    std::array<math::Matrix4d, 3> spatial{};
    [[nodiscard]] bool valid() const noexcept { return status == GeometryStatus::valid; }
};

struct GeodesicDiagnostics final {
    GeometryStatus status{GeometryStatus::non_finite_input};
    double hamiltonian{};
    // |g^ab p_a p_b + mu^2| / (mu^2 + sum |g^ab p_a p_b|).
    // mu^2=0 for null diagnostics; mu^2=1 for unit-rest-mass timelike diagnostics.
    double normalized_constraint{};
    double energy{};
    double angular_momentum{};
    double carter{}; // Carter constant Q, continuous on the rotation axis.
    double radius{};
    double coordinate_time_rate{}; // dt/dlambda, or dt/dtau for unit timelike momentum.
    [[nodiscard]] bool valid() const noexcept { return status == GeometryStatus::valid; }
};

[[nodiscard]] MetricDerivatives evaluate_metric_derivatives(const KerrParameters &parameters,
                                                            math::Vector3d position) noexcept;

// On failure output is zeroed. No invalid derivative escapes this boundary.
[[nodiscard]] GeometryStatus geodesic_rhs(const KerrParameters &parameters,
                                          const GeodesicState &state,
                                          GeodesicState &derivative) noexcept;

[[nodiscard]] GeodesicDiagnostics diagnose_null_geodesic(const KerrParameters &parameters,
                                                         const GeodesicState &state) noexcept;

// Includes +a^2 cos(theta)^2 in Q and the H=-1/2 mass-shell residual.
[[nodiscard]] GeodesicDiagnostics diagnose_timelike_geodesic(const KerrParameters &parameters,
                                                             const GeodesicState &state) noexcept;

struct NullStateEvaluation final {
    GeometryStatus status{GeometryStatus::non_finite_input};
    GeodesicState state{};
    [[nodiscard]] bool valid() const noexcept { return status == GeometryStatus::valid; }
};

// Set spatial COVARIANT momentum and solve H=0 for the future-directed root.
// This is a scientific initial-data helper, not an observer/camera direction.
[[nodiscard]] NullStateEvaluation make_null_state(const KerrParameters &parameters,
                                                  math::Vector3d position,
                                                  math::Vector3d spatial_momentum,
                                                  bool past_directed = false) noexcept;

enum class TimelikeStateStatus {
    valid,
    non_finite_input,
    singular_geometry,
    not_future_directed,
    not_timelike,
    not_normalized
};

struct TimelikeStateEvaluation final {
    TimelikeStateStatus status{TimelikeStateStatus::non_finite_input};
    GeodesicState state{}; // Finite zero state on failure.
    [[nodiscard]] bool valid() const noexcept { return status == TimelikeStateStatus::valid; }
};

// Lower an already unit-normalized CONTRAVARIANT four-velocity (t,x,y,z).
// Requires u^t>0 (the ingoing KS time gradient is timelike), g(u,u)<0,
// and |g(u,u)+1|<=1e-10. No silent normalization or momentum projection.
// Stored momentum is per unit rest mass; the affine parameter is proper time.
[[nodiscard]] TimelikeStateEvaluation make_timelike_state(const KerrParameters &parameters,
                                                          math::Vector3d position,
                                                          math::Vector4d four_velocity,
                                                          double coordinate_time = 0.0) noexcept;

} // namespace kerr::physics

// Conserved quantity invariant assertions
