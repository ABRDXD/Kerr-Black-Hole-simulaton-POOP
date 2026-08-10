#pragma once

#include "kerr/physics/Geodesic.hpp"

namespace kerr::physics {

// Levi-Civita connection contracted with a contravariant tangent:
// connection(mu,b)=Gamma^mu_ab tangent^a. Uses partial g=-g(partial g^-1)g.
// Output is finite on success and zero on failure.
[[nodiscard]] GeometryStatus contract_connection(const MetricEvaluation &metric,
                                                 const MetricDerivatives &inverse_partials,
                                                 const math::Vector4d &tangent,
                                                 math::Matrix4<long double> &connection) noexcept;

} // namespace kerr::physics
