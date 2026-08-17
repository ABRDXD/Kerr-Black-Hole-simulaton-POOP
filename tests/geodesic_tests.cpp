#include "kerr/numerics/Integrators.hpp"
#include "kerr/numerics/PolynomialRoots.hpp"
#include "kerr/physics/RayTracer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
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
void near(double actual, double expected, double tolerance, int line) {
    ++checks;
    if (!std::isfinite(actual) || !std::isfinite(expected) ||
        std::abs(actual - expected) > tolerance) {
        ++failures;
        std::cerr << std::setprecision(17) << "line " << line << ": actual=" << actual
                  << " expected=" << expected << " tolerance=" << tolerance << '\n';
    }
}
#define NEAR(actual, expected, tolerance) near((actual), (expected), (tolerance), __LINE__)
using namespace kerr::physics;
namespace num = kerr::numerics;

GeodesicState schwarzschild_ray(double radius, double impact) {
    const double f = 2.0 / radius;
    const double radial = -std::sqrt(1.0 - (1.0 - f) * impact * impact / (radius * radius));
    return {0.0, radius, 0.0, 0.0, -1.0, (radial + f) / (1.0 - f), impact / radius, 0.0};
}

void derivative_tests() {
    for (double spin : {0.0, 0.83, -0.83, 0.999999, 1.0}) {
        const KerrParameters parameters{1.0, spin};
        const std::array<kerr::math::Vector3d, 7> points{{{9.0, -3.0, 2.0},
                                                          {2.0, 0.3, 0.2},
                                                          {0.0, 0.0, 1.4},
                                                          {1.2, 0.0, 0.0},
                                                          {0.3, 0.2, 0.1},
                                                          {20.0, 8.0, -4.0},
                                                          {0.0, 0.0, -2.0}}};
        for (const auto &point : points) {
            const auto ad = evaluate_metric_derivatives(parameters, point);
            CHECK(ad.valid());
            const std::array<double, 3> xyz{point.x, point.y, point.z};
            for (std::size_t axis = 0; axis < 3; ++axis) {
                const double h = 1.0e-5 * std::max(1.0, std::abs(xyz[axis]));
                std::array<kerr::math::Matrix4d, 4> samples{};
                constexpr std::array<double, 4> offsets{-2.0, -1.0, 1.0, 2.0};
                for (std::size_t sample = 0; sample < 4; ++sample) {
                    auto shifted = xyz;
                    shifted[axis] += offsets[sample] * h;
                    const auto metric =
                        evaluate_metric(parameters, {shifted[0], shifted[1], shifted[2]});
                    CHECK(metric.valid());
                    samples[sample] = metric.contravariant;
                }
                for (std::size_t i = 0; i < 16; ++i) {
                    const double fd = (samples[0].elements[i] - 8.0 * samples[1].elements[i] +
                                       8.0 * samples[2].elements[i] - samples[3].elements[i]) /
                                      (12.0 * h);
                    NEAR(ad.spatial[axis].elements[i], fd, 2.0e-8 * std::max(1.0, std::abs(fd)));
                }
            }
        }
    }
    const auto analytic = evaluate_metric_derivatives(KerrParameters{1.0, 0.0}, {3.0, 4.0, 0.0});
    NEAR(analytic.spatial[0](0, 0), 6.0 / 125.0, 1.0e-15);
    NEAR(analytic.spatial[1](0, 0), 8.0 / 125.0, 1.0e-15);
    NEAR(analytic.spatial[0](0, 1), 2.0 / 25.0 - 36.0 / 625.0, 1.0e-15);
    CHECK(!evaluate_metric_derivatives(KerrParameters{1.0, 0.5}, {0.0, 0.0, 0.0}).valid());
    CHECK(!evaluate_metric_derivatives(KerrParameters{1.0, 0.5},
                                       {std::numeric_limits<double>::quiet_NaN(), 0.0, 1.0})
               .valid());
}

void integrator_tests() {
    auto exponential = [](const num::State<1> &y, num::State<1> &dy) {
        dy = y;
        return true;
    };
    auto fixed = [&](bool reference, double h) {
        num::State<1> y{1.0};
        const auto count = static_cast<std::size_t>(std::round(1.0 / h));
        for (std::size_t i = 0; i < count; ++i) {
            const auto step = reference ? num::extrapolated_midpoint8(exponential, y, y, h)
                                        : num::dopri5(exponential, y, y, h);
            CHECK(step.valid);
            y = step.end;
        }
        return std::abs(y[0] - std::exp(1.0));
    };
    const double coarse = fixed(false, 0.2), fine = fixed(false, 0.1);
    CHECK(coarse / fine > 24.0 && coarse / fine < 40.0);
    const double reference_coarse = fixed(true, 0.5), reference_fine = fixed(true, 0.25);
    CHECK(reference_coarse / reference_fine > 180.0 && reference_coarse / reference_fine < 400.0);
    auto step = num::dopri5(exponential, num::State<1>{1.0}, num::State<1>{1.0}, 0.1);
    NEAR(num::dense_state(num::State<1>{1.0}, step, 0.1, 0.5)[0], std::exp(0.05), 4.0e-9);
    NEAR(num::dense_state(num::State<1>{1.0}, step, 0.1, 0.0)[0], 1.0, 0.0);
    NEAR(num::dense_state(num::State<1>{1.0}, step, 0.1, 1.0)[0], step.end[0], 0.0);
    auto flat = [](const num::State<8> &y, num::State<8> &dy) {
        dy = {-y[4], y[5], y[6], y[7], 0.0, 0.0, 0.0, 0.0};
        return true;
    };
    num::State<8> straight{0.0, 1.0, 2.0, 3.0, -1.0, 0.6, 0.0, 0.8}, velocity{};
    flat(straight, velocity);
    const auto flat_step = num::dopri5(flat, straight, velocity, 13.0);
    CHECK(flat_step.valid);
    for (std::size_t i = 0; i < 8; ++i)
        NEAR(flat_step.end[i], straight[i] + 13.0 * velocity[i], 5.0e-15);
    auto bad = [](const num::State<1> &, num::State<1> &dy) {
        dy = {std::numeric_limits<double>::quiet_NaN()};
        return true;
    };
    CHECK(!num::dopri5(bad, num::State<1>{1.0}, num::State<1>{1.0}, 0.1).valid);
    // Same-sign endpoints with two crossings must still yield two roots.
    num::Polynomial polynomial{};
    polynomial[0] = 0.16L;
    polynomial[1] = -1.0L;
    polynomial[2] = 1.0L;
    const auto roots = num::unit_interval_roots(polynomial, 2);
    CHECK(roots.valid && roots.count == 2);
    NEAR(roots.values[0], 0.2, 1.0e-11);
    NEAR(roots.values[1], 0.8, 1.0e-11);
    num::Polynomial eight{};
    eight[0] = 1.0L;
    for (std::size_t k = 0; k < 8; ++k) {
        const long double root = static_cast<long double>(k + 1) / 9.0L;
        num::Polynomial next{};
        for (std::size_t j = 0; j <= k; ++j) {
            next[j] -= root * eight[j];
            next[j + 1] += eight[j];
        }
        eight = next;
    }
    const auto all = num::unit_interval_roots(eight, 8);
    CHECK(all.valid && all.count == 8);
    for (std::size_t k = 0; k < all.count; ++k)
        NEAR(all.values[k], static_cast<double>(k + 1) / 9.0, 1.0e-10);
}

void radial_tests() {
    const KerrParameters parameters{1.0, 0.0};
    RayOptions options{};
    options.escape_radius = 40.0;
    const auto inward = trace_null_ray(parameters, schwarzschild_ray(30.0, 0.0), options);
    CHECK(inward.termination == RayTermination::horizon);
    NEAR(inward.affine_parameter, 28.0, 2.0e-9);
    NEAR(inward.state[0], 28.0, 2.0e-9);
    NEAR(inward.state[1], 2.0, 2.0e-9);
    CHECK(inward.statistics.maximum_constraint < 1.0e-12);
    CHECK(inward.statistics.rhs_evaluations ==
          1 + 6 * (inward.statistics.accepted_steps + inward.statistics.rejected_steps));
    const double r = 8.0;
    const GeodesicState outward{0.0, r, 0.0, 0.0, -1.0, (r + 2.0) / (r - 2.0), 0.0, 0.0};
    const auto escaped = trace_null_ray(parameters, outward, options);
    CHECK(escaped.termination == RayTermination::escape);
    NEAR(escaped.affine_parameter, 32.0, 2.0e-7);
    NEAR(escaped.state[0], 32.0 + 4.0 * std::log(38.0 / 6.0), 3.0e-7);
    NEAR(escaped.diagnostics.radius, 40.0, 2.0e-9);
    options.stop_at_horizon = false;
    options.stop_at_escape = false;
    options.affine_limit = 28.5;
    const auto across = trace_null_ray(parameters, schwarzschild_ray(30.0, 0.0), options);
    CHECK(across.termination == RayTermination::affine_limit);
    NEAR(across.diagnostics.radius, 1.5, 3.0e-10);
    for (double offset : {-1.0e-3, -1.0e-6, 1.0e-6, 1.0e-3}) {
        options = {};
        options.escape_radius = 35.0;
        const auto critical =
            trace_null_ray(parameters, schwarzschild_ray(30.0, std::sqrt(27.0) + offset), options);
        CHECK(critical.termination ==
              (offset < 0.0 ? RayTermination::horizon : RayTermination::escape));
        CHECK(critical.statistics.maximum_constraint < options.constraint_limit);
    }
}

GeodesicState reference_endpoint(const KerrParameters &parameters, const GeodesicState &initial,
                                 double end, double step_size) {
    auto rhs = [&](const GeodesicState &state, GeodesicState &derivative) {
        return geodesic_rhs(parameters, state, derivative) == GeometryStatus::valid;
    };
    auto state = initial;
    double affine{};
    while (affine < end) {
        GeodesicState first{};
        CHECK(rhs(state, first));
        const double h = std::min(step_size, end - affine);
        const auto step = num::extrapolated_midpoint8(rhs, state, first, h);
        CHECK(step.valid);
        if (!step.valid)
            break;
        state = step.end;
        affine += h;
    }
    return state;
}

void kerr_tests() {
    for (double spin : {0.5, -0.5, 0.99}) {
        const KerrParameters parameters{1.0, spin};
        const auto initial = make_null_state(parameters, {10.0, 1.0, 3.0}, {-0.8, 0.4, -0.1});
        CHECK(initial.valid());
        const auto diagnostics = diagnose_null_geodesic(parameters, initial.state);
        CHECK(diagnostics.valid() && diagnostics.normalized_constraint < 1.0e-14);
        RayOptions options{};
        options.affine_limit = 12.0;
        options.stop_at_escape = false;
        const auto result = trace_null_ray(parameters, initial.state, options);
        CHECK(result.termination == RayTermination::affine_limit);
        CHECK(result.statistics.maximum_lz_drift < 1.0e-8);
        CHECK(result.statistics.maximum_carter_drift < 1.0e-8);
        const auto truth = reference_endpoint(parameters, initial.state, 12.0, 0.05);
        const auto finer = reference_endpoint(parameters, initial.state, 12.0, 0.025);
        for (std::size_t i = 0; i < 8; ++i) {
            NEAR(truth[i], finer[i], 2.0e-11);
            NEAR(result.state[i], finer[i], 2.0e-8);
        }
    }
    // Known circular photon orbits: independent angular velocity relation.
    for (double spin : {0.0, 0.5, -0.5, 0.99}) {
        const KerrParameters parameters{1.0, spin};
        for (const auto sense : {OrbitSense::prograde, OrbitSense::retrograde}) {
            const double radius = equatorial_photon_orbit_radius(parameters, sense);
            const double orientation =
                (spin < 0.0 ? -1.0 : 1.0) * (sense == OrbitSense::prograde ? 1.0 : -1.0);
            const double omega = orientation / (std::pow(radius, 1.5) + orientation * spin);
            const auto metric = evaluate_metric(parameters, {radius, spin, 0.0});
            const std::array<double, 4> velocity{1.0, -omega * spin, omega * radius, 0.0};
            GeodesicState initial{0.0, radius, spin, 0.0, 0.0, 0.0, 0.0, 0.0};
            for (std::size_t row = 0; row < 4; ++row)
                for (std::size_t col = 0; col < 4; ++col)
                    initial[row + 4] += metric.covariant(row, col) * velocity[col];
            RayOptions options{};
            options.affine_limit = 12.0;
            options.maximum_step = 0.1;
            const auto result = trace_null_ray(parameters, initial, options);
            CHECK(result.termination == RayTermination::affine_limit);
            NEAR(result.diagnostics.radius, radius, 2.0e-7);
            NEAR(result.state[1], radius * std::cos(omega * 12.0) - spin * std::sin(omega * 12.0),
                 3.0e-7);
            NEAR(result.state[2], radius * std::sin(omega * 12.0) + spin * std::cos(omega * 12.0),
                 3.0e-7);
        }
    }
    // Axis-regular Carter invariant equals L^2-Lz^2 in Schwarzschild.
    const KerrParameters schwarzschild{1.0, 0.0};
    const auto axis = make_null_state(schwarzschild, {0.0, 0.0, 8.0}, {0.3, 0.4, -1.0});
    NEAR(diagnose_null_geodesic(schwarzschild, axis.state).carter, 16.0, 1.0e-13);
}

void event_and_failure_tests() {
    const KerrParameters parameters{1.0, 0.7};
    const auto initial = make_null_state(parameters, {12.0, 0.0, 2.0}, {0.0, 0.2, -1.0});
    RayOptions options{};
    options.stop_at_disk = true;
    const auto hit = trace_null_ray(parameters, initial.state, options);
    CHECK(hit.termination == RayTermination::disk);
    NEAR(hit.state[3], 0.0, 2.0e-9);
    CHECK(hit.diagnostics.radius >= 6.0 && hit.diagnostics.radius <= 30.0);
    auto refined = options;
    refined.maximum_step = 0.025;
    refined.relative_tolerance = 1.0e-12;
    const auto hit_refined = trace_null_ray(parameters, initial.state, refined);
    CHECK(hit_refined.termination == RayTermination::disk);
    NEAR(hit.affine_parameter, hit_refined.affine_parameter, 1.0e-8);
    auto invalid = initial.state;
    invalid[5] = std::numeric_limits<double>::infinity();
    const auto failed = trace_null_ray(parameters, invalid);
    CHECK(failed.termination == RayTermination::invalid_initial_state);
    CHECK(num::finite(failed.state));
    CHECK(trace_null_ray(parameters, {}).termination == RayTermination::invalid_initial_state);
    GeodesicState derivative{};
    CHECK(geodesic_rhs(parameters, invalid, derivative) == GeometryStatus::non_finite_input);
    CHECK(num::finite(derivative));
    options = {};
    options.maximum_attempts = 1;
    CHECK(trace_null_ray(parameters, initial.state, options).termination ==
          RayTermination::work_limit);
    options = {};
    CHECK(trace_null_ray(
              parameters, initial.state, options,
              [](double, const GeodesicState &, const GeodesicDiagnostics &) { return false; })
              .termination == RayTermination::cancelled);
    options.relative_tolerance = -1.0;
    bool threw = false;
    try {
        static_cast<void>(trace_null_ray(parameters, initial.state, options));
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    CHECK(threw);
    options = {};
    options.relative_tolerance = 1.0e-15;
    options.position_tolerance = 1.0e-16;
    options.momentum_tolerance = 1.0e-16;
    options.initial_step = 0.5;
    options.minimum_step = 0.5;
    options.maximum_step = 0.5;
    const auto minimum = trace_null_ray(parameters, initial.state, options);
    CHECK(minimum.termination == RayTermination::minimum_step);
    CHECK(num::finite(minimum.state));
    options.minimum_step = 1.0e-12;
    options.maximum_rejections = 1;
    CHECK(trace_null_ray(parameters, initial.state, options).termination ==
          RayTermination::rejection_limit);
    // Time reversal without event boundaries must recover the starting event.
    options = {};
    options.stop_at_horizon = false;
    options.stop_at_escape = false;
    options.affine_limit = 2.0;
    const auto forward = trace_null_ray(parameters, initial.state, options);
    auto reversed = forward.state;
    for (std::size_t i = 4; i < 8; ++i)
        reversed[i] = -reversed[i];
    const auto backward = trace_null_ray(parameters, reversed, options);
    CHECK(backward.termination == RayTermination::affine_limit);
    for (std::size_t i = 0; i < 4; ++i)
        NEAR(backward.state[i], initial.state[i], 1.0e-8);
}
} // namespace

int main() {
    derivative_tests();
    integrator_tests();
    radial_tests();
    kerr_tests();
    event_and_failure_tests();
    std::cout << (failures == 0 ? "PASS: " : "FAIL: ") << checks << " geodesic checks, " << failures
              << " failures\n";
    return failures == 0 ? 0 : 1;
}

// Null condition check
