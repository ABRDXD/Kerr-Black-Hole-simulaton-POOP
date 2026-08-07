#pragma once

#include "kerr/math/Matrix4.hpp"
#include "kerr/math/Vector.hpp"

#include <cstdint>

namespace kerr::physics {

enum class OrbitSense : std::uint8_t {
    prograde,
    retrograde,
};

enum class GeometryStatus : std::uint8_t {
    valid,
    non_finite_input,
    singular_or_degenerate_chart,
};

class KerrParameters final {
public:
    // Construction is a configuration boundary. Invalid or non-finite values
    // throw std::invalid_argument rather than entering numerical hot paths.
    KerrParameters(double mass, double spin);

    [[nodiscard]] constexpr double mass() const noexcept { return mass_; }
    [[nodiscard]] constexpr double spin() const noexcept { return spin_; }
    [[nodiscard]] constexpr double dimensionless_spin() const noexcept {
        return spin_ / mass_;
    }

private:
    double mass_{};
    double spin_{};
};

struct RadiusEvaluation final {
    GeometryStatus status{GeometryStatus::non_finite_input};
    double radius{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return status == GeometryStatus::valid;
    }
};

struct MetricEvaluation final {
    GeometryStatus status{GeometryStatus::non_finite_input};
    double radius{};
    double profile{}; // F in g = eta + F l tensor l.
    math::Covector4d null_covector{};
    math::Matrix4d covariant{};
    math::Matrix4d contravariant{};

    [[nodiscard]] constexpr bool valid() const noexcept {
        return status == GeometryStatus::valid;
    }
};

struct HorizonRadii final {
    double inner{};
    double outer{};
};

[[nodiscard]] RadiusEvaluation kerr_schild_radius(
    const KerrParameters& parameters,
    math::Vector3d position) noexcept;

[[nodiscard]] MetricEvaluation evaluate_metric(
    const KerrParameters& parameters,
    math::Vector3d position) noexcept;

[[nodiscard]] HorizonRadii horizon_radii(const KerrParameters& parameters) noexcept;

[[nodiscard]] double outer_ergosphere_radius(
    const KerrParameters& parameters,
    double polar_angle) noexcept;

[[nodiscard]] double equatorial_isco_radius(
    const KerrParameters& parameters,
    OrbitSense sense) noexcept;

[[nodiscard]] double equatorial_photon_orbit_radius(
    const KerrParameters& parameters,
    OrbitSense sense) noexcept;

[[nodiscard]] double hamiltonian(
    const math::Matrix4d& inverse_metric,
    const math::Covector4d& covariant_momentum) noexcept;

[[nodiscard]] math::Vector4d raise_index(
    const math::Matrix4d& inverse_metric,
    const math::Covector4d& covector) noexcept;

} // namespace kerr::physics


// Frame-dragging frequency omega
