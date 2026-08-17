#include "kerr/physics/RayTracer.hpp"
#include "fixtures/bound_equatorial_v1.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numbers>
#include <string_view>

namespace {
int checks{}, failures{};
void check(bool condition, std::string_view label, int line) {
    ++checks;
    if (!condition) {
        ++failures;
        std::cerr << "line " << line << ": " << label << '\n';
    }
}
#define CHECK(condition) check((condition), #condition, __LINE__)
void near(long double actual, long double expected, long double tolerance, int line) {
    ++checks;
    if (!std::isfinite(actual) || !std::isfinite(expected) ||
        std::abs(actual - expected) > tolerance) {
        ++failures;
        std::cerr << std::setprecision(18) << "line " << line << ": actual=" << actual
                  << " expected=" << expected << " tolerance=" << tolerance << '\n';
    }
}
#define NEAR(actual, expected, tolerance) \
    near(static_cast<long double>(actual), static_cast<long double>(expected), \
         static_cast<long double>(tolerance), __LINE__)
using namespace kerr::physics;
constexpr long double pi = std::numbers::pi_v<long double>;

// An independent equatorial, unit-mass (M=mu=1, Q=0) oracle, with no calls to
// KerrLab's metric, derivatives, initial-data factories, or numerical steppers.
// Schmidt (2002), arXiv:gr-qc/0202090v1, Eqs. (7), (9), (10), (12):
// https://arxiv.org/pdf/gr-qc/0202090v1
// Fujita & Hikida (2009), arXiv:0906.1420v2, Eqs. (19), (21), (22):
// https://arxiv.org/html/0906.1420v2
// These published equations are independently implemented; no source code copied.
struct Orbit {
    long double spin{}, p{}, eccentricity{}, direction{};
    long double pericenter{}, apocenter{}, energy{}, angular_momentum{}, inner_root{};
};

long double radial_equation(const Orbit &orbit, long double r) {
    const auto a = orbit.spin, e = orbit.energy;
    const auto v = orbit.angular_momentum - a * e;
    // R(r)/r^2, expanded independently from P^2-Delta*(r^2+(L-aE)^2).
    return (e * e - 1.0L) * r * r + 2.0L * r - a * a - 2.0L * a * e * v +
           (2.0L / r - 1.0L) * v * v;
}

Orbit make_orbit(long double spin, long double p, long double eccentricity,
                 long double direction) {
    Orbit orbit{spin, p, eccentricity, direction};
    orbit.pericenter = p / (1.0L + eccentricity);
    orbit.apocenter = p / (1.0L - eccentricity);
    // Schwarzschild bound-orbit constants seed a two-variable Newton solve of
    // R(r_peri)=R(r_apo)=0. The roots, not production orbit data, define the case.
    orbit.energy = std::sqrt(((p - 2.0L) * (p - 2.0L) -
                              4.0L * eccentricity * eccentricity) /
                             (p * (p - 3.0L - eccentricity * eccentricity)));
    long double v = direction * p / std::sqrt(p - 3.0L - eccentricity * eccentricity) -
                    spin * orbit.energy;
    for (int iteration = 0; iteration < 24; ++iteration) {
        orbit.angular_momentum = v + spin * orbit.energy;
        const auto rp = orbit.pericenter, ra = orbit.apocenter;
        const auto fp = radial_equation(orbit, rp), fa = radial_equation(orbit, ra);
        const auto ep = 2.0L * orbit.energy * rp * rp - 2.0L * spin * v;
        const auto ea = 2.0L * orbit.energy * ra * ra - 2.0L * spin * v;
        const auto vp = -2.0L * spin * orbit.energy + 2.0L * (2.0L / rp - 1.0L) * v;
        const auto va = -2.0L * spin * orbit.energy + 2.0L * (2.0L / ra - 1.0L) * v;
        const auto determinant = ep * va - ea * vp;
        const auto de = (-fp * va + fa * vp) / determinant;
        const auto dv = (-ep * fa + ea * fp) / determinant;
        orbit.energy += de;
        v += dv;
        if (std::abs(de) + std::abs(dv) < 8.0L * std::numeric_limits<long double>::epsilon())
            break;
    }
    orbit.angular_momentum = v + spin * orbit.energy;
    // R=(1-E^2)*r*(r_apo-r)*(r-r_peri)*(r-r_3); the fourth root is zero.
    orbit.inner_root = 2.0L / (1.0L - orbit.energy * orbit.energy) -
                       orbit.pericenter - orbit.apocenter;
    CHECK(orbit.energy > 0.0L && orbit.energy < 1.0L);
    CHECK(direction * orbit.angular_momentum > 0.0L);
    CHECK(orbit.inner_root > 0.0L && orbit.inner_root < orbit.pericenter);
    CHECK(orbit.pericenter > 1.0L + std::sqrt(1.0L - spin * spin));
    NEAR(radial_equation(orbit, orbit.pericenter), 0.0L, 1.0e-11L);
    NEAR(radial_equation(orbit, orbit.apocenter), 0.0L, 1.0e-11L);
    return orbit;
}

std::array<Orbit, 5> orbits() {
    return {make_orbit(0.0L, 10.0L, 0.3L, 1.0L),
            make_orbit(0.7L, 10.0L, 0.3L, 1.0L),
            make_orbit(-0.7L, 10.0L, 0.3L, -1.0L),
            make_orbit(0.7L, 12.0L, 0.2L, -1.0L),
            make_orbit(0.95L, 6.0L, 0.2L, 1.0L)};
}

long double radius(const Orbit &orbit, long double anomaly) {
    return (orbit.apocenter + orbit.pericenter) / 2.0L -
           (orbit.apocenter - orbit.pericenter) / 2.0L * std::cos(anomaly);
}

// r=c-h*cos(chi) removes both square-root turning-point singularities exactly:
// dr/sqrt(R)=dchi/sqrt((1-E^2)*r*(r-r_3)) on each oriented half-cycle.
// Components are derivatives of (tau,t_BL,phi_BL) with respect to chi.
std::array<long double, 3> quadrature_rhs(const Orbit &orbit, long double anomaly) {
    const auto r = radius(orbit, anomaly), a = orbit.spin;
    const auto delta = r * r - 2.0L * r + a * a;
    const auto v = orbit.angular_momentum - a * orbit.energy;
    const auto capital_p = orbit.energy * r * r - a * v;
    const auto denominator =
        std::sqrt((1.0L - orbit.energy * orbit.energy) * r * (r - orbit.inner_root));
    return {r * r / denominator,
            ((r * r + a * a) * capital_p / delta + a * v) / denominator,
            (a * capital_p / delta + v) / denominator};
}

std::array<long double, 3> quadrature(const Orbit &orbit, long double anomaly,
                                     std::size_t intervals) {
    std::array<long double, 3> sum{};
    const auto step = anomaly / static_cast<long double>(intervals);
    for (std::size_t i = 0; i <= intervals; ++i) {
        const auto value = quadrature_rhs(orbit, static_cast<long double>(i) * step);
        const auto weight = i == 0 || i == intervals ? 1.0L : (i % 2 == 0 ? 2.0L : 4.0L);
        for (std::size_t j = 0; j < sum.size(); ++j)
            sum[j] += weight * value[j];
    }
    for (auto &value : sum)
        value *= step / 3.0L;
    return sum;
}

struct OracleEvent {
    long double proper_time{}, unwrapped_azimuth{};
    GeodesicState state{};
};

OracleEvent oracle_event(const Orbit &orbit, long double anomaly, std::size_t cycles = 0,
                         std::size_t intervals = 4096) {
    auto integrals = quadrature(orbit, anomaly, intervals);
    const auto period = quadrature(orbit, 2.0L * pi, intervals);
    for (std::size_t i = 0; i < integrals.size(); ++i)
        integrals[i] += static_cast<long double>(cycles) * period[i];
    const auto r = radius(orbit, anomaly), a = orbit.spin;
    const auto delta = r * r - 2.0L * r + a * a;
    const auto root_separation = 2.0L * std::sqrt(1.0L - a * a);
    const auto horizon_plus = 1.0L + root_separation / 2.0L;
    const auto horizon_minus = 1.0L - root_separation / 2.0L;
    const auto log_plus = std::log((r - horizon_plus) / (orbit.pericenter - horizon_plus));
    const auto log_minus = std::log((r - horizon_minus) / (orbit.pericenter - horizon_minus));
    // Ingoing transform: dt_KS=dt_BL+2r/Delta dr, dpsi=dphi_BL+a/Delta dr,
    // x+iy=(r+ia)*exp(i*psi). Integration constants set KS t=psi=0 at periapsis.
    // Canonical one-form supplies p_r_KS=p_r_BL+(2rE-aL)/Delta.
    const auto time = integrals[1] +
                      2.0L * (horizon_plus * log_plus - horizon_minus * log_minus) /
                          root_separation;
    const auto psi = integrals[2] + a * (log_plus - log_minus) / root_separation;
    const auto c = std::cos(psi), s = std::sin(psi);
    const auto x = r * c - a * s, y = r * s + a * c;
    const auto radial_rate = (orbit.apocenter - orbit.pericenter) / 2.0L *
                             std::sin(anomaly) / quadrature_rhs(orbit, anomaly)[0];
    const auto radial_momentum =
        (r * r * radial_rate + 2.0L * r * orbit.energy - a * orbit.angular_momentum) / delta;
    const auto px = (x * radial_momentum - s * orbit.angular_momentum) / r;
    const auto py = (y * radial_momentum + c * orbit.angular_momentum) / r;
    return {integrals[0], psi,
            {static_cast<double>(time), static_cast<double>(x), static_cast<double>(y), 0.0,
             static_cast<double>(-orbit.energy), static_cast<double>(px),
             static_cast<double>(py), 0.0}};
}

double maximum_error(const GeodesicState &actual, const GeodesicState &expected,
                     std::size_t begin, std::size_t end) {
    double error{};
    for (auto i = begin; i < end; ++i)
        error = std::max(error, std::abs(actual[i] - expected[i]));
    return error;
}

void fixture_checks(const std::array<Orbit, 5> &cases) {
    for (std::size_t i = 0; i < cases.size(); ++i) {
        const auto &orbit = cases[i];
        const auto period = quadrature(orbit, 2.0L * pi, 4096);
        const std::array<long double, 12> actual{
            orbit.spin, orbit.p, orbit.eccentricity, orbit.direction, orbit.energy,
            orbit.angular_momentum, orbit.pericenter, orbit.apocenter, orbit.inner_root,
            period[0], period[1], period[2]};
        for (std::size_t j = 0; j < actual.size(); ++j)
            NEAR(actual[j], kerr::testfixtures::bound_equatorial_v1[i][j],
                 j < 9 ? 2.0e-13L : 2.0e-10L);
    }
}

void oracle_checks(const Orbit &orbit) {
    for (const auto anomaly : {0.0L, pi / 2.0L, pi, 3.0L * pi / 2.0L, 2.0L * pi}) {
        const auto coarse = oracle_event(orbit, anomaly, 0, 2048);
        const auto fine = oracle_event(orbit, anomaly, 0, 4096);
        NEAR(coarse.proper_time, fine.proper_time, 2.0e-10L);
        NEAR(coarse.unwrapped_azimuth, fine.unwrapped_azimuth, 2.0e-12L);
        for (std::size_t i = 0; i < 8; ++i)
            NEAR(coarse.state[i], fine.state[i], 3.0e-10L);
        // Both factored and unfactored radial equations must agree away from roots.
        const auto r = radius(orbit, anomaly);
        const auto factored = (1.0L - orbit.energy * orbit.energy) *
                              (orbit.apocenter - r) * (r - orbit.pericenter) *
                              (r - orbit.inner_root) / r;
        NEAR(radial_equation(orbit, r), factored, 2.0e-11L);
        const auto d = diagnose_timelike_geodesic(
            KerrParameters{1.0, static_cast<double>(orbit.spin)}, fine.state);
        CHECK(d.valid());
        NEAR(d.hamiltonian, -0.5L, 2.0e-14L);
        NEAR(d.energy, orbit.energy, 2.0e-15L);
        NEAR(d.angular_momentum, orbit.angular_momentum, 5.0e-15L);
        NEAR(d.carter, 0.0L, 1.0e-25L);
    }
}

void endpoint_tests(const Orbit &orbit) {
    const KerrParameters parameters{1.0, static_cast<double>(orbit.spin)};
    const auto initial = oracle_event(orbit, 0.0L).state;
    double worst_position{}, worst_momentum{};
    for (const auto anomaly : {pi / 2.0L, pi, 3.0L * pi / 2.0L, 2.0L * pi}) {
        const auto expected = oracle_event(orbit, anomaly);
        WorldlineOptions options{};
        options.proper_time_limit = static_cast<double>(expected.proper_time);
        options.stop_at_escape = false;
        const auto result = trace_timelike_worldline(parameters, initial, options);
        CHECK(result.termination == RayTermination::proper_time_limit);
        NEAR(result.proper_time, options.proper_time_limit, 0.0L);
        for (std::size_t i = 0; i < 8; ++i)
            NEAR(result.state[i], expected.state[i], i < 4 ? 2.0e-7L : 2.0e-8L);
        NEAR(result.diagnostics.radius, radius(orbit, anomaly), 5.0e-8L);
        CHECK(result.statistics.maximum_constraint < 2.0e-10);
        CHECK(result.statistics.maximum_lz_drift < 2.0e-9);
        worst_position = std::max(worst_position, maximum_error(result.state, expected.state, 0, 4));
        worst_momentum = std::max(worst_momentum, maximum_error(result.state, expected.state, 4, 8));
    }
    std::cout << "orbit a=" << static_cast<double>(orbit.spin)
              << " p=" << static_cast<double>(orbit.p)
              << " direction=" << static_cast<double>(orbit.direction)
              << " one-period max coordinate error=" << worst_position
              << " max momentum error=" << worst_momentum << '\n';
}

void stress_test(const Orbit &orbit) {
    const KerrParameters parameters{1.0, static_cast<double>(orbit.spin)};
    const auto initial = oracle_event(orbit, 0.0L).state;
    constexpr std::size_t cycles = 20;
    const auto expected = oracle_event(orbit, 0.0L, cycles);
    double last_tau = -1.0, last_time = -1.0, last_angle = std::atan2(initial[2], initial[1]);
    double total_angle{}, maximum_shell_error{};
    std::size_t samples{};
    WorldlineOptions options{};
    options.proper_time_limit = static_cast<double>(expected.proper_time);
    options.stop_at_escape = false;
    const auto result = trace_timelike_worldline(
        parameters, initial, options,
        [&](double tau, const GeodesicState &state, const GeodesicDiagnostics &d) {
            ++samples;
            CHECK(d.valid() && tau > last_tau && state[0] > last_time);
            CHECK(d.radius >= static_cast<double>(orbit.pericenter) - 3.0e-6 &&
                  d.radius <= static_cast<double>(orbit.apocenter) + 3.0e-6);
            NEAR(state[3], 0.0L, 1.0e-25L);
            NEAR(state[7], 0.0L, 1.0e-25L);
            maximum_shell_error = std::max(maximum_shell_error, std::abs(2.0 * d.hamiltonian + 1.0));
            const auto angle = std::atan2(state[2], state[1]);
            total_angle += std::remainder(angle - last_angle, 2.0 * std::numbers::pi);
            last_angle = angle;
            last_tau = tau;
            last_time = state[0];
            return true;
        });
    CHECK(result.termination == RayTermination::proper_time_limit);
    CHECK(samples == result.statistics.accepted_steps + 1);
    CHECK(result.statistics.accepted_steps > 10000);
    CHECK(result.statistics.accepted_steps + result.statistics.rejected_steps < 50000);
    CHECK(result.statistics.maximum_energy_drift == 0.0);
    CHECK(result.statistics.maximum_lz_drift < 2.0e-8);
    CHECK(result.statistics.maximum_carter_drift < 1.0e-25);
    CHECK(maximum_shell_error < 2.0e-9);
    NEAR(total_angle, expected.unwrapped_azimuth, 3.0e-6L);
    for (std::size_t i = 0; i < 8; ++i)
        NEAR(result.state[i], expected.state[i], i < 4 ? 3.0e-5L : 3.0e-6L);
    // The same Cartesian polar angle recurs in the radial chart shift at periapsis;
    // full-cycle increments of t_KS and psi equal those of BL t and phi.
    const auto reference_frequency = expected.unwrapped_azimuth /
                                     static_cast<long double>(expected.state[0]);
    NEAR(total_angle / result.state[0], reference_frequency, 5.0e-10L);
    // Long-run agreement must improve under a distinct maximum-step cap;
    // keeping the same source quadrature prevents a shared-integrator oracle.
    options.maximum_step = 0.25;
    const auto refined = trace_timelike_worldline(parameters, initial, options);
    CHECK(refined.termination == RayTermination::proper_time_limit);
    const auto coarse_error = maximum_error(result.state, expected.state, 0, 8);
    const auto refined_error = maximum_error(refined.state, expected.state, 0, 8);
    CHECK(refined_error < coarse_error * 0.2);
    CHECK(refined_error < 1.0e-6);
    std::cout << "stress cycles=" << cycles << " tau=" << result.proper_time
              << " steps=" << result.statistics.accepted_steps
              << " max coordinate error=" << maximum_error(result.state, expected.state, 0, 4)
              << " max momentum error=" << maximum_error(result.state, expected.state, 4, 8)
              << " phase error=" << std::abs(static_cast<long double>(total_angle) - expected.unwrapped_azimuth)
              << " max mass-shell error=" << maximum_shell_error
              << " half-step max endpoint error=" << refined_error << '\n';
}

void dump_oracle() {
    std::cout << "a,p,e,direction,E,Lz,r_peri,r_apo,r_inner,tau_period,t_period,phi_period\n"
              << std::setprecision(20);
    for (const auto &orbit : orbits()) {
        const auto period = quadrature(orbit, 2.0L * pi, 16384);
        std::cout << orbit.spin << ',' << orbit.p << ',' << orbit.eccentricity << ','
                  << orbit.direction << ',' << orbit.energy << ',' << orbit.angular_momentum << ','
                  << orbit.pericenter << ',' << orbit.apocenter << ',' << orbit.inner_root << ','
                  << period[0] << ',' << period[1] << ',' << period[2] << '\n';
    }
}
} // namespace

int main(int argc, char **argv) {
    if (argc == 2 && std::string_view{argv[1]} == "--dump-oracle") {
        dump_oracle();
        return failures == 0 ? 0 : 1;
    }
    const auto cases = orbits();
    fixture_checks(cases);
    for (const auto &orbit : cases) {
        oracle_checks(orbit);
        endpoint_tests(orbit);
    }
    stress_test(cases[1]);
    std::cout << (failures == 0 ? "PASS: " : "FAIL: ") << checks << " bound-orbit checks, "
              << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
