#include "kerr/physics/Transport.hpp"
#include "kerr/physics/Connection.hpp"

#include <algorithm>
#include <cmath>

namespace kerr::physics {

TransportEvaluation make_transport_state(const KerrParameters &parameters,
                                         const ObserverEvent &observer) noexcept {
    TransportEvaluation result{};
    result.status = validate_observer(parameters, observer);
    if (!result.valid())
        return result;
    const auto metric = evaluate_metric(
        parameters, {observer.position[1], observer.position[2], observer.position[3]});
    for (std::size_t mu = 0; mu < 4; ++mu) {
        result.state[mu] = observer.position[mu];
        long double momentum{};
        for (std::size_t nu = 0; nu < 4; ++nu)
            momentum += static_cast<long double>(metric.covariant(mu, nu)) *
                        static_cast<long double>(observer.tetrad[0][nu]);
        result.state[mu + 4] = static_cast<double>(momentum);
        for (std::size_t leg = 0; leg < 3; ++leg)
            result.state[8 + 4 * leg + mu] = observer.tetrad[leg + 1][mu];
    }
    for (double value : result.state)
        if (!std::isfinite(value))
            return {ObserverStatus::numerical_failure, {}};
    return result;
}

GeometryStatus parallel_transport_rhs(const KerrParameters &parameters, const TransportState &state,
                                      TransportState &derivative) noexcept {
    const TransportState input = state;
    derivative = {};
    for (double value : input)
        if (!std::isfinite(value))
            return GeometryStatus::non_finite_input;
    GeodesicState geodesic{}, rhs{};
    std::copy_n(input.begin(), 8, geodesic.begin());
    const auto status = geodesic_rhs(parameters, geodesic, rhs);
    if (status != GeometryStatus::valid)
        return status;
    const math::Vector3d position{input[1], input[2], input[3]};
    const auto metric = evaluate_metric(parameters, position);
    const auto inverse_partials = evaluate_metric_derivatives(parameters, position);
    // Connection contracted with u; de_i^mu/dtau=-Gamma^mu_ab u^a e_i^b.
    math::Matrix4<long double> connection{};
    const auto connection_status =
        contract_connection(metric, inverse_partials, {rhs[0], rhs[1], rhs[2], rhs[3]}, connection);
    if (connection_status != GeometryStatus::valid)
        return connection_status;
    TransportState result{};
    std::copy(rhs.begin(), rhs.end(), result.begin());
    for (std::size_t leg = 0; leg < 3; ++leg)
        for (std::size_t mu = 0; mu < 4; ++mu) {
            long double value{};
            for (std::size_t b = 0; b < 4; ++b)
                value -= connection(mu, b) * static_cast<long double>(input[8 + 4 * leg + b]);
            result[8 + 4 * leg + mu] = static_cast<double>(value);
        }
    for (double value : result)
        if (!std::isfinite(value))
            return GeometryStatus::non_finite_input;
    derivative = result;
    return GeometryStatus::valid;
}

ObserverEvaluation transported_observer(const KerrParameters &parameters,
                                        const TransportState &state, double proper_time) noexcept {
    ObserverEvaluation result{};
    for (double value : state)
        if (!std::isfinite(value))
            return result;
    const auto metric = evaluate_metric(parameters, {state[1], state[2], state[3]});
    if (!metric.valid()) {
        result.status = ObserverStatus::invalid_geometry;
        return result;
    }
    ObserverEvent observer{};
    std::copy_n(state.begin(), 4, observer.position.begin());
    observer.proper_time = proper_time;
    observer.tetrad[0] =
        raise_index(metric.contravariant, {state[4], state[5], state[6], state[7]});
    for (std::size_t leg = 0; leg < 3; ++leg)
        std::copy_n(state.begin() + static_cast<std::ptrdiff_t>(8 + 4 * leg), 4,
                    observer.tetrad[leg + 1].begin());
    result.status = validate_observer(parameters, observer);
    if (result.valid())
        result.observer = observer;
    return result;
}

} // namespace kerr::physics
