#pragma once

#include "kerr/physics/Observer.hpp"

#include <array>

namespace kerr::physics {

// Geodesic (x^mu,p_mu), followed by three contravariant spatial tetrad legs.
// The temporal leg is always recovered as u^mu=g^{mu nu}p_nu.
using TransportState = std::array<double, 20>;

struct TransportEvaluation final {
    ObserverStatus status{ObserverStatus::non_finite_input};
    TransportState state{};
    [[nodiscard]] bool valid() const noexcept { return status == ObserverStatus::valid; }
};

[[nodiscard]] TransportEvaluation make_transport_state(const KerrParameters &parameters,
                                                       const ObserverEvent &observer) noexcept;

// Proper-time RHS for a freely falling observer and parallel transported axes.
// This is a numerical-system primitive, not an adaptive/event tracing driver.
// Acceleration requires Fermi-Walker transport, which is not implemented here.
// Output is zero on failure; input/output may alias.
[[nodiscard]] GeometryStatus parallel_transport_rhs(const KerrParameters &parameters,
                                                    const TransportState &state,
                                                    TransportState &derivative) noexcept;

// Preserve the transported orientation and validate it; never silently project.
[[nodiscard]] ObserverEvaluation transported_observer(const KerrParameters &parameters,
                                                      const TransportState &state,
                                                      double proper_time) noexcept;

} // namespace kerr::physics
