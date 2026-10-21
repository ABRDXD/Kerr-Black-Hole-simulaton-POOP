#include "kerr/physics/Connection.hpp"

#include <cmath>

namespace kerr::physics {

GeometryStatus contract_connection(const MetricEvaluation &metric,
                                   const MetricDerivatives &inverse_partials,
                                   const math::Vector4d &tangent,
                                   math::Matrix4<long double> &connection) noexcept {
    connection = {};
    if (!metric.valid())
        return metric.status;
    if (!inverse_partials.valid())
        return inverse_partials.status;
    for (double value : tangent)
        if (!std::isfinite(value))
            return GeometryStatus::non_finite_input;
    std::array<math::Matrix4<long double>, 4> partials{};
    for (std::size_t direction = 1; direction < 4; ++direction)
        for (std::size_t a = 0; a < 4; ++a)
            for (std::size_t b = 0; b < 4; ++b)
                for (std::size_t m = 0; m < 4; ++m)
                    for (std::size_t n = 0; n < 4; ++n)
                        partials[direction](a, b) -=
                            static_cast<long double>(metric.covariant(a, m)) *
                            static_cast<long double>(
                                inverse_partials.spatial[direction - 1](m, n)) *
                            static_cast<long double>(metric.covariant(n, b));
    math::Matrix4<long double> result{};
    for (std::size_t mu = 0; mu < 4; ++mu)
        for (std::size_t b = 0; b < 4; ++b)
            for (std::size_t a = 0; a < 4; ++a)
                for (std::size_t n = 0; n < 4; ++n)
                    result(mu, b) += 0.5L * static_cast<long double>(metric.contravariant(mu, n)) *
                                     (partials[a](n, b) + partials[b](n, a) - partials[n](a, b)) *
                                     static_cast<long double>(tangent[a]);
    for (long double value : result.elements)
        if (!std::isfinite(value))
            return GeometryStatus::non_finite_input;
    connection = result;
    return GeometryStatus::valid;
}

} // namespace kerr::physics
