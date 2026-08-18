#include "kerr/physics/Observer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace kerr::physics {
namespace {

constexpr long double frame_tolerance = 1.0e-9L;
using Tetrad = std::array<math::Vector4d, 4>;

[[nodiscard]] bool finite(const math::Vector4d &vector) noexcept {
    return std::all_of(vector.begin(), vector.end(),
                       [](const double entry) { return std::isfinite(entry); });
}

[[nodiscard]] bool finite_event(const math::Vector4d &position, const double proper_time) noexcept {
    return finite(position) && std::isfinite(proper_time);
}

[[nodiscard]] long double inner(const math::Matrix4d &metric, const math::Vector4d &first,
                                const math::Vector4d &second) noexcept {
    long double result{};
    for (std::size_t mu = 0; mu < 4U; ++mu) {
        for (std::size_t nu = 0; nu < 4U; ++nu) {
            result += static_cast<long double>(metric(mu, nu)) *
                      static_cast<long double>(first[mu]) * static_cast<long double>(second[nu]);
        }
    }
    return result;
}

[[nodiscard]] ObserverEvaluation failure(const ObserverStatus status) noexcept {
    return {status, {}};
}

[[nodiscard]] ObserverStatus geometry_status(const MetricEvaluation &metric) noexcept {
    return metric.status == GeometryStatus::non_finite_input ? ObserverStatus::non_finite_input
                                                             : ObserverStatus::invalid_geometry;
}

[[nodiscard]] MetricEvaluation metric_at(const KerrParameters &parameters,
                                         const math::Vector4d &position) noexcept {
    return evaluate_metric(parameters, {position[1], position[2], position[3]});
}

[[nodiscard]] Tetrad eulerian_frame(const MetricEvaluation &metric) noexcept {
    Tetrad frame{};
    const long double profile = static_cast<long double>(metric.profile);
    const long double root = std::sqrt(1.0L + profile);
    // gamma_ij = delta_ij + F*l_i*l_j; symmetric inverse square root.
    const long double correction = -profile / (root * (root + 1.0L));
    frame[0][0] = static_cast<double>(root);
    for (std::size_t i = 1U; i < 4U; ++i) {
        const long double li = static_cast<long double>(metric.null_covector[i]);
        frame[0][i] = static_cast<double>(-profile * li / root);
        for (std::size_t j = 1U; j < 4U; ++j) {
            frame[i][j] = static_cast<double>(
                (i == j ? 1.0L : 0.0L) +
                correction * li * static_cast<long double>(metric.null_covector[j]));
        }
    }
    return frame;
}

[[nodiscard]] long double frame_determinant(const Tetrad &frame) noexcept {
    std::array<std::array<long double, 4>, 4> matrix{};
    for (std::size_t row = 0; row < 4U; ++row) {
        for (std::size_t column = 0; column < 4U; ++column) {
            matrix[row][column] = static_cast<long double>(frame[column][row]);
        }
    }
    long double determinant = 1.0L;
    for (std::size_t i = 0; i < 4U; ++i) {
        std::size_t pivot = i;
        for (std::size_t j = i + 1U; j < 4U; ++j) {
            if (std::abs(matrix[j][i]) > std::abs(matrix[pivot][i])) {
                pivot = j;
            }
        }
        if (matrix[pivot][i] == 0.0L) {
            return 0.0L;
        }
        if (pivot != i) {
            std::swap(matrix[pivot], matrix[i]);
            determinant = -determinant;
        }
        determinant *= matrix[i][i];
        for (std::size_t j = i + 1U; j < 4U; ++j) {
            const long double factor = matrix[j][i] / matrix[i][i];
            for (std::size_t k = i + 1U; k < 4U; ++k) {
                matrix[j][k] -= factor * matrix[i][k];
            }
        }
    }
    return determinant;
}

[[nodiscard]] ObserverStatus validate_frame(const MetricEvaluation &metric,
                                            const Tetrad &frame) noexcept {
    for (const auto &basis : frame) {
        if (!finite(basis)) {
            return ObserverStatus::non_finite_input;
        }
    }
    // The ingoing KS t-slices are spacelike; u^t>0 fixes time orientation even
    // in the ergoregion and within the future horizon.
    if (frame[0][0] <= 0.0) {
        return ObserverStatus::invalid_four_velocity;
    }
    for (std::size_t a = 0; a < 4U; ++a) {
        for (std::size_t b = a; b < 4U; ++b) {
            const long double expected = a == b ? (a == 0U ? -1.0L : 1.0L) : 0.0L;
            const long double value = inner(metric.covariant, frame[a], frame[b]);
            if (!std::isfinite(value) || std::abs(value - expected) > frame_tolerance) {
                return a == 0U && b == 0U ? ObserverStatus::invalid_four_velocity
                                          : ObserverStatus::invalid_tetrad;
            }
        }
    }
    const long double determinant = frame_determinant(frame);
    if (!std::isfinite(determinant) || std::abs(determinant - 1.0L) > 4.0L * frame_tolerance) {
        return ObserverStatus::invalid_tetrad;
    }
    return ObserverStatus::valid;
}

} // namespace

ObserverStatus validate_observer(const KerrParameters &parameters,
                                 const ObserverEvent &observer) noexcept {
    if (!finite_event(observer.position, observer.proper_time)) {
        return ObserverStatus::non_finite_input;
    }
    const auto metric = metric_at(parameters, observer.position);
    if (!metric.valid()) {
        return geometry_status(metric);
    }
    return validate_frame(metric, observer.tetrad);
}

ObserverEvaluation make_observer(const KerrParameters &parameters, const math::Vector4d position,
                                 const math::Vector4d unit_future_velocity,
                                 const double proper_time) noexcept {
    if (!finite_event(position, proper_time) || !finite(unit_future_velocity)) {
        return failure(ObserverStatus::non_finite_input);
    }
    const auto metric = metric_at(parameters, position);
    if (!metric.valid()) {
        return failure(geometry_status(metric));
    }
    const long double norm = inner(metric.covariant, unit_future_velocity, unit_future_velocity);
    if (unit_future_velocity[0] <= 0.0 || !std::isfinite(norm) ||
        std::abs(norm + 1.0L) > frame_tolerance) {
        return failure(ObserverStatus::invalid_four_velocity);
    }
    const Tetrad base = eulerian_frame(metric);
    const long double gamma = static_cast<long double>(unit_future_velocity[0]) /
                              std::sqrt(1.0L + static_cast<long double>(metric.profile));
    std::array<long double, 3> momentum{};
    for (std::size_t i = 0; i < 3U; ++i) {
        momentum[i] = inner(metric.covariant, base[i + 1U], unit_future_velocity);
    }
    ObserverEvent event{position, proper_time, {}};
    event.tetrad[0] = unit_future_velocity;
    for (std::size_t i = 0; i < 3U; ++i) {
        for (std::size_t mu = 0; mu < 4U; ++mu) {
            long double value = static_cast<long double>(base[i + 1U][mu]) +
                                momentum[i] * static_cast<long double>(base[0][mu]);
            for (std::size_t j = 0; j < 3U; ++j) {
                value += momentum[i] * momentum[j] / (gamma + 1.0L) *
                         static_cast<long double>(base[j + 1U][mu]);
            }
            event.tetrad[i + 1U][mu] = static_cast<double>(value);
        }
    }
    if (validate_frame(metric, event.tetrad) != ObserverStatus::valid) {
        return failure(ObserverStatus::numerical_failure);
    }
    return {ObserverStatus::valid, event};
}

ObserverEvaluation make_eulerian_observer(const KerrParameters &parameters,
                                          const math::Vector4d position,
                                          const double proper_time) noexcept {
    if (!finite_event(position, proper_time)) {
        return failure(ObserverStatus::non_finite_input);
    }
    const auto metric = metric_at(parameters, position);
    if (!metric.valid()) {
        return failure(geometry_status(metric));
    }
    const ObserverEvent event{position, proper_time, eulerian_frame(metric)};
    if (validate_frame(metric, event.tetrad) != ObserverStatus::valid) {
        return failure(ObserverStatus::numerical_failure);
    }
    return {ObserverStatus::valid, event};
}

ObserverEvaluation make_local_velocity_observer(const KerrParameters &parameters,
                                                const math::Vector4d position,
                                                const math::Vector3d local_velocity,
                                                const double proper_time) noexcept {
    const std::array<double, 3> velocity{local_velocity.x, local_velocity.y, local_velocity.z};
    for (const double component : velocity) {
        if (!std::isfinite(component)) {
            return failure(ObserverStatus::non_finite_input);
        }
    }
    const long double speed =
        std::hypot(static_cast<long double>(velocity[0]), static_cast<long double>(velocity[1]),
                   static_cast<long double>(velocity[2]));
    if (speed >= 1.0L) {
        return failure(ObserverStatus::invalid_four_velocity);
    }
    const auto base = make_eulerian_observer(parameters, position, proper_time);
    if (!base.valid()) {
        return base;
    }
    const long double gamma = 1.0L / std::sqrt((1.0L - speed) * (1.0L + speed));
    math::Vector4d four_velocity{};
    for (std::size_t mu = 0; mu < 4U; ++mu) {
        long double value = static_cast<long double>(base.observer.tetrad[0][mu]);
        for (std::size_t i = 0; i < 3U; ++i) {
            value += static_cast<long double>(velocity[i]) *
                     static_cast<long double>(base.observer.tetrad[i + 1U][mu]);
        }
        four_velocity[mu] = static_cast<double>(gamma * value);
    }
    return make_observer(parameters, position, four_velocity, proper_time);
}

ObserverEvaluation make_stationary_observer(const KerrParameters &parameters,
                                            const math::Vector4d position,
                                            const double proper_time) noexcept {
    if (!finite_event(position, proper_time)) {
        return failure(ObserverStatus::non_finite_input);
    }
    const auto metric = metric_at(parameters, position);
    if (!metric.valid()) {
        return failure(geometry_status(metric));
    }
    if (metric.covariant(0, 0) >= 0.0) {
        return failure(ObserverStatus::stationary_unavailable);
    }
    return make_observer(parameters, position,
                         {1.0 / std::sqrt(-metric.covariant(0, 0)), 0.0, 0.0, 0.0}, proper_time);
}

ObserverEvaluation make_circular_observer(const KerrParameters &parameters,
                                          const math::Vector4d position, const OrbitSense sense,
                                          const double proper_time) noexcept {
    if (!finite_event(position, proper_time)) {
        return failure(ObserverStatus::non_finite_input);
    }
    const auto metric = metric_at(parameters, position);
    if (!metric.valid()) {
        return failure(geometry_status(metric));
    }
    if ((sense != OrbitSense::prograde && sense != OrbitSense::retrograde) || position[3] != 0.0 ||
        metric.radius <= horizon_radii(parameters).outer ||
        metric.radius <= equatorial_photon_orbit_radius(parameters, sense)) {
        return failure(ObserverStatus::circular_orbit_unavailable);
    }
    const long double direction =
        (parameters.spin() < 0.0 ? -1.0L : 1.0L) * (sense == OrbitSense::prograde ? 1.0L : -1.0L);
    const long double radius = static_cast<long double>(metric.radius);
    const long double root_mass = std::sqrt(static_cast<long double>(parameters.mass()));
    // Bardeen-Press-Teukolsky (1972), Eq. 2.16. For dr=0 the radial BL->KS
    // shifts leave dphi/dt unchanged, and Cartesian rotation is (-Omega*y,Omega*x).
    const long double omega = direction * root_mass /
                              (radius * std::sqrt(radius) +
                               direction * static_cast<long double>(parameters.spin()) * root_mass);
    math::Vector4d velocity{
        1.0, static_cast<double>(-omega * static_cast<long double>(position[2])),
        static_cast<double>(omega * static_cast<long double>(position[1])), 0.0};
    const long double norm = inner(metric.covariant, velocity, velocity);
    if (!std::isfinite(norm) || norm >= 0.0L) {
        return failure(ObserverStatus::circular_orbit_unavailable);
    }
    for (double &component : velocity) {
        component = static_cast<double>(static_cast<long double>(component) / std::sqrt(-norm));
    }
    return make_observer(parameters, position, velocity, proper_time);
}

CameraRayEvaluation make_camera_ray(const KerrParameters &parameters, const ObserverEvent &observer,
                                    const math::Vector3d direction, const double frequency,
                                    const bool past_directed) noexcept {
    const auto status = validate_observer(parameters, observer);
    if (status != ObserverStatus::valid) {
        return {status, {}};
    }
    if (!std::isfinite(direction.x) || !std::isfinite(direction.y) || !std::isfinite(direction.z) ||
        !std::isfinite(frequency)) {
        return {ObserverStatus::non_finite_input, {}};
    }
    if (frequency <= 0.0) {
        return {ObserverStatus::invalid_frequency, {}};
    }
    const double scale =
        std::max({std::abs(direction.x), std::abs(direction.y), std::abs(direction.z)});
    if (scale == 0.0) {
        return {ObserverStatus::invalid_direction, {}};
    }
    std::array<long double, 3> unit{static_cast<long double>(direction.x / scale),
                                    static_cast<long double>(direction.y / scale),
                                    static_cast<long double>(direction.z / scale)};
    const long double length = std::hypot(unit[0], unit[1], unit[2]);
    for (auto &entry : unit) {
        entry /= length;
    }
    std::array<long double, 4> tangent{};
    for (std::size_t mu = 0; mu < 4U; ++mu) {
        tangent[mu] =
            (past_directed ? -1.0L : 1.0L) * static_cast<long double>(observer.tetrad[0][mu]);
        for (std::size_t i = 0; i < 3U; ++i) {
            tangent[mu] += unit[i] * static_cast<long double>(observer.tetrad[i + 1U][mu]);
        }
        tangent[mu] *= static_cast<long double>(frequency);
    }
    const auto metric = metric_at(parameters, observer.position);
    GeodesicState state{};
    for (std::size_t mu = 0; mu < 4U; ++mu) {
        long double covariant{};
        for (std::size_t nu = 0; nu < 4U; ++nu) {
            covariant += static_cast<long double>(metric.covariant(mu, nu)) * tangent[nu];
        }
        state[mu] = observer.position[mu];
        state[mu + 4U] = static_cast<double>(covariant);
        if (!std::isfinite(state[mu + 4U])) {
            return {ObserverStatus::numerical_failure, {}};
        }
    }
    const auto diagnostic = diagnose_null_geodesic(parameters, state);
    if (!diagnostic.valid() || diagnostic.normalized_constraint > 4.0e-9) {
        return {ObserverStatus::numerical_failure, {}};
    }
    return {ObserverStatus::valid, state};
}

FrequencyEvaluation measured_frequency(const KerrParameters &parameters,
                                       const ObserverEvent &observer,
                                       const math::Covector4d covariant_momentum,
                                       const bool past_directed) noexcept {
    const auto status = validate_observer(parameters, observer);
    if (status != ObserverStatus::valid) {
        return {status, 0.0};
    }
    if (!finite(covariant_momentum)) {
        return {ObserverStatus::non_finite_input, 0.0};
    }
    long double contraction{};
    for (std::size_t mu = 0; mu < 4U; ++mu) {
        contraction += static_cast<long double>(covariant_momentum[mu]) *
                       static_cast<long double>(observer.tetrad[0][mu]);
    }
    const double frequency = static_cast<double>((past_directed ? 1.0L : -1.0L) * contraction);
    if (!std::isfinite(frequency)) {
        return {ObserverStatus::numerical_failure, 0.0};
    }
    if (frequency <= 0.0) {
        return {ObserverStatus::invalid_frequency, 0.0};
    }
    return {ObserverStatus::valid, frequency};
}

} // namespace kerr::physics
