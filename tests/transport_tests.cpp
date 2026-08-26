#include "kerr/numerics/Integrators.hpp"
#include "kerr/physics/Transport.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {
using namespace kerr::physics;
std::size_t checks{}, failures{};
void check(bool condition, const char *message) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}
double dot(const kerr::math::Matrix4d &metric, const kerr::math::Vector4d &a,
           const kerr::math::Vector4d &b) {
    long double value{};
    for (std::size_t mu = 0; mu < 4; ++mu)
        for (std::size_t nu = 0; nu < 4; ++nu)
            value += static_cast<long double>(metric(mu, nu)) * static_cast<long double>(a[mu]) *
                     static_cast<long double>(b[nu]);
    return static_cast<double>(value);
}
TransportState integrate(const KerrParameters &parameters, TransportState state, double duration,
                         double maximum_step) {
    const auto rhs = [&](const TransportState &x, TransportState &dx) {
        return parallel_transport_rhs(parameters, x, dx) == GeometryStatus::valid;
    };
    const auto count = static_cast<std::size_t>(std::ceil(std::abs(duration) / maximum_step));
    const double h = duration / static_cast<double>(count);
    for (std::size_t i = 0; i < count; ++i) {
        TransportState first{};
        if (!rhs(state, first))
            throw std::runtime_error{"Invalid transport derivative"};
        const auto step = kerr::numerics::extrapolated_midpoint8(rhs, state, first, h);
        if (!step.valid)
            throw std::runtime_error{"Invalid transport step"};
        state = step.end;
    }
    return state;
}
double distance(const TransportState &a, const TransportState &b) {
    double result{};
    for (std::size_t i = 0; i < a.size(); ++i)
        result = std::max(result, std::abs(a[i] - b[i]));
    return result;
}
void test_circular_holonomy() {
    const KerrParameters parameters{1.0, 0.0};
    constexpr double r = 10.0, f = 1.0 - 2.0 / r;
    auto observer = make_circular_observer(parameters, {0.0, r, 0.0, 0.0}, OrbitSense::prograde);
    check(observer.valid(), "circular transport observer");
    if (!observer.valid())
        return;
    auto &frame = observer.observer.tetrad;
    const auto metric = evaluate_metric(parameters, {r, 0.0, 0.0});
    frame[1] = {2.0 / r / std::sqrt(f), std::sqrt(f), 0.0, 0.0};
    const kerr::math::Vector4d azimuth{0.0, 0.0, 1.0, 0.0};
    const double projection = dot(metric.covariant, frame[0], azimuth);
    for (std::size_t mu = 0; mu < 4; ++mu)
        frame[2][mu] =
            (azimuth[mu] + projection * frame[0][mu]) / std::sqrt(1.0 + projection * projection);
    frame[3] = {0.0, 0.0, 0.0, 1.0};
    const auto initial = make_transport_state(parameters, observer.observer);
    check(initial.valid(), "radial-oriented frame");
    if (!initial.valid())
        return;
    const double period = 2.0 * std::numbers::pi * r * std::sqrt(r - 3.0);
    const auto final = integrate(parameters, initial.state, period, 0.5);
    const auto refined = integrate(parameters, initial.state, period, 0.25);
    check(distance(final, refined) < 2.0e-10, "transport step refinement");
    const auto transported = transported_observer(parameters, final, period);
    check(transported.valid(), "circular transported frame stays orthonormal");
    if (!transported.valid())
        return;
    check(std::abs(final[1] - r) < 2.0e-9 && std::abs(final[2]) < 2.0e-9,
          "full circular orbit endpoint");
    const double phase = 2.0 * std::numbers::pi * std::sqrt(1.0 - 3.0 / r);
    check(std::abs(dot(metric.covariant, frame[1], transported.observer.tetrad[1]) -
                   std::cos(phase)) < 2.0e-9,
          "independent analytic Schwarzschild gyroscope holonomy cosine");
    check(std::abs(dot(metric.covariant, frame[2], transported.observer.tetrad[1]) +
                   std::sin(phase)) < 2.0e-9,
          "independent analytic Schwarzschild gyroscope holonomy orientation");
}
void test_kerr_and_horizon() {
    for (double spin : {-0.9, 0.0, 0.99}) {
        const KerrParameters parameters{1.0, spin};
        const auto observer =
            make_local_velocity_observer(parameters, {4.0, 7.0, 1.0, 2.0}, {0.1, 0.2, -0.05}, 3.0);
        check(observer.valid(), "Kerr observer for transport");
        const auto initial = make_transport_state(parameters, observer.observer);
        check(initial.valid(), "Kerr transport initial state");
        const auto final = integrate(parameters, initial.state, 8.0, 0.2);
        const auto result = transported_observer(parameters, final, 11.0);
        check(result.valid(), "generic Kerr transported orthonormality");
        check(result.observer.proper_time == 11.0 && result.observer.position[0] > 4.0,
              "separate proper and coordinate clocks");
        const auto reversed = integrate(parameters, final, -8.0, 0.2);
        check(distance(initial.state, reversed) < 3.0e-11,
              "coupled frame forward reverse integration");
        TransportState derivative{};
        check(parallel_transport_rhs(parameters, initial.state, derivative) ==
                  GeometryStatus::valid,
              "finite transport RHS");
        auto aliased = initial.state;
        check(parallel_transport_rhs(parameters, aliased, aliased) == GeometryStatus::valid &&
                  distance(aliased, derivative) == 0.0,
              "aliased transport RHS");
    }
    const KerrParameters parameters{1.0, 0.0};
    constexpr double r0 = 4.0, target = 1.7;
    const double w = std::sqrt(2.0 / r0);
    const auto observer =
        make_observer(parameters, {0.0, r0, 0.0, 0.0}, {1.0 + w * w / (1.0 + w), -w, 0.0, 0.0});
    const auto initial = make_transport_state(parameters, observer.observer);
    check(initial.valid(), "radial freefall frame");
    const double duration =
        2.0 * (std::pow(r0, 1.5) - std::pow(target, 1.5)) / (3.0 * std::sqrt(2.0));
    const auto final = integrate(parameters, initial.state, duration, 0.025);
    check(std::abs(final[1] - target) < 3.0e-11, "transport smooth across horizon");
    check(transported_observer(parameters, final, duration).valid(),
          "interior frame orthonormality");
}
void test_failure() {
    const KerrParameters parameters{1.0, 0.8};
    TransportState invalid{}, derivative{};
    derivative.fill(1.0);
    check(parallel_transport_rhs(parameters, invalid, derivative) ==
              GeometryStatus::singular_or_degenerate_chart,
          "singular transport geometry classified");
    check(std::all_of(derivative.begin(), derivative.end(), [](double x) { return x == 0.0; }),
          "failed derivative cleared");
    invalid[17] = std::numeric_limits<double>::quiet_NaN();
    check(parallel_transport_rhs(parameters, invalid, derivative) ==
              GeometryStatus::non_finite_input,
          "non-finite orientation rejected");
    check(!transported_observer(parameters, invalid, 0.0).valid(), "non-finite unpack rejected");
    check(!make_transport_state(parameters, {}).valid(), "invalid frame rejected");
    const auto observer = make_eulerian_observer(parameters, {0.0, 5.0, 1.0, 0.0});
    auto corrupted = make_transport_state(parameters, observer.observer).state;
    corrupted[8] += 0.1;
    check(!transported_observer(parameters, corrupted, 0.0).valid(),
          "corrupted transported frame rejected without projection");
}
} // namespace

int main() {
    try {
        test_circular_holonomy();
        test_kerr_and_horizon();
        test_failure();
        std::cout << checks << " transport checks, " << failures << " failures\n";
        return failures == 0 ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
