#include "kerr/numerics/Integrators.hpp"
#include "kerr/physics/RayTracer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numbers>
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
using kerr::math::Vector3d;
using kerr::math::Vector4d;

Vector4d normalized(const KerrParameters &parameters, Vector3d position, Vector4d vector) {
    const auto metric = evaluate_metric(parameters, position);
    CHECK(metric.valid());
    long double norm{};
    for (std::size_t i = 0; i < 4; ++i)
        for (std::size_t j = 0; j < 4; ++j)
            norm += static_cast<long double>(metric.covariant(i, j)) *
                    static_cast<long double>(vector[i]) * static_cast<long double>(vector[j]);
    CHECK(norm < 0.0L);
    for (double &component : vector)
        component /= static_cast<double>(std::sqrt(-norm));
    return vector;
}

GeodesicState radial_infall(const KerrParameters &parameters, double radius, double time = 0.0) {
    const double w = std::sqrt(2.0 * parameters.mass() / radius);
    const auto initial = make_timelike_state(parameters, {radius, 0.0, 0.0},
                                             {1.0 + w * w / (1.0 + w), -w, 0.0, 0.0}, time);
    CHECK(initial.valid());
    return initial.state;
}

double infall_proper_time(double mass, double initial_radius, double radius) {
    return 2.0 / (3.0 * std::sqrt(2.0 * mass)) *
           (std::pow(initial_radius, 1.5) - std::pow(radius, 1.5));
}

// Integrating dt/dr=-1/w-w/(1+w), w=sqrt(2M/r), is regular through r=2M.
double infall_coordinate_time(double mass, double initial_radius, double radius) {
    const auto primitive = [](double s) { return s * s * s / 3.0 + s - std::log1p(s); };
    return 4.0 * mass *
           (primitive(std::sqrt(initial_radius / (2.0 * mass))) -
            primitive(std::sqrt(radius / (2.0 * mass))));
}

void initial_data_tests() {
    const KerrParameters parameters{1.0, 0.7};
    for (const Vector3d position : std::array<Vector3d, 4>{
             {{9.0, 1.0, 3.0}, {0.0, 0.0, 4.0}, {0.0, 0.0, -4.0}, {1.2, 0.0, 0.2}}}) {
        // Unit normal to the spacelike t=constant slices remains valid inside the horizon.
        const auto metric = evaluate_metric(parameters, position);
        Vector4d velocity{};
        for (std::size_t i = 0; i < 4; ++i)
            velocity[i] = -metric.contravariant(i, 0) / std::sqrt(-metric.contravariant(0, 0));
        const auto initial = make_timelike_state(parameters, position, velocity, 12.5);
        CHECK(initial.valid());
        NEAR(initial.state[0], 12.5, 0.0);
        const auto diagnostics = diagnose_timelike_geodesic(parameters, initial.state);
        CHECK(diagnostics.valid());
        NEAR(diagnostics.hamiltonian, -0.5, 1.0e-14);
        CHECK(diagnostics.normalized_constraint < 1.0e-14);
        NEAR(diagnostics.coordinate_time_rate, velocity[0], 2.0e-14);
        const auto null_diagnostics = diagnose_null_geodesic(parameters, initial.state);
        const double additional = parameters.spin() * position.z / diagnostics.radius;
        NEAR(diagnostics.carter - null_diagnostics.carter, additional * additional, 2.0e-14);
        GeodesicState rhs{};
        CHECK(geodesic_rhs(parameters, initial.state, rhs) == GeometryStatus::valid);
        for (std::size_t i = 0; i < 4; ++i)
            NEAR(rhs[i], velocity[i], 2.0e-14);
    }
    const Vector3d position{10.0, 0.0, 0.0};
    auto velocity = normalized(parameters, position, {1.0, 0.1, 0.05, -0.02});
    for (double &component : velocity)
        component *= -1.0;
    CHECK(make_timelike_state(parameters, position, velocity).status ==
          TimelikeStateStatus::not_future_directed);
    CHECK(make_timelike_state(parameters, position, {1.0, 2.0, 0.0, 0.0}).status ==
          TimelikeStateStatus::not_timelike);
    CHECK(make_timelike_state(parameters, position, {1.0, 0.0, 0.0, 0.0}).status ==
          TimelikeStateStatus::not_normalized);
    CHECK(make_timelike_state(parameters, {}, {1.0, 0.0, 0.0, 0.0}).status ==
          TimelikeStateStatus::singular_geometry);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (const auto &failed : {make_timelike_state(parameters, position, {nan, 0.0, 0.0, 0.0}),
                               make_timelike_state(parameters, {nan, 0.0, 0.0}, velocity),
                               make_timelike_state(parameters, position, velocity, nan)}) {
        CHECK(failed.status == TimelikeStateStatus::non_finite_input);
        CHECK(num::finite(failed.state));
        for (double component : failed.state)
            NEAR(component, 0.0, 0.0);
    }
}

void circular_tests() {
    for (double mass : {1.0, 2.3}) {
        for (double chi : {0.0, 0.8, -0.8, 0.999}) {
            const double spin = mass * chi, radius = 10.0 * mass;
            const KerrParameters parameters{mass, spin};
            for (double orientation : {-1.0, 1.0}) {
                // Independent circular BL relation; dr=0 means dt_KS=dt_BL and dphi_KS=dphi_BL.
                const double orbital_term =
                    orientation * spin * std::sqrt(mass) / std::pow(radius, 1.5);
                const double time_rate = (1.0 + orbital_term) /
                                         std::sqrt(1.0 - 3.0 * mass / radius + 2.0 * orbital_term);
                const double omega = orientation * std::sqrt(mass) /
                                     (std::pow(radius, 1.5) + orientation * spin * std::sqrt(mass));
                const auto initial = make_timelike_state(
                    parameters, {radius, spin, 0.0},
                    {time_rate, -spin * omega * time_rate, radius * omega * time_rate, 0.0});
                CHECK(initial.valid());
                WorldlineOptions options{};
                options.proper_time_limit = 2.0 * std::numbers::pi / std::abs(omega * time_rate);
                options.maximum_step = 0.5 * mass;
                options.stop_at_escape = false;
                double last_time = -1.0, last_tau = -1.0;
                const auto result = trace_timelike_worldline(
                    parameters, initial.state, options,
                    [&](double tau, const GeodesicState &state, const GeodesicDiagnostics &d) {
                        CHECK(tau > last_tau && state[0] > last_time);
                        CHECK(d.valid() && d.coordinate_time_rate > 0.0);
                        NEAR(d.radius, radius, 4.0e-7 * mass);
                        NEAR(state[0], time_rate * tau, 4.0e-7 * mass);
                        NEAR(d.hamiltonian, -0.5, 2.0e-9);
                        last_time = state[0];
                        last_tau = tau;
                        return true;
                    });
                CHECK(result.termination == RayTermination::proper_time_limit);
                NEAR(result.proper_time, options.proper_time_limit, 0.0);
                NEAR(result.state[1], radius, 4.0e-7 * mass);
                NEAR(result.state[2], spin, 4.0e-7 * mass);
                CHECK(result.statistics.maximum_energy_drift == 0.0);
                CHECK(result.statistics.maximum_lz_drift < 1.0e-8);
                CHECK(result.statistics.maximum_carter_drift < 1.0e-12);
            }
        }
    }
}

void freefall_tests() {
    for (double mass : {1.0, 2.3}) {
        const KerrParameters parameters{mass, 0.0};
        const double initial_radius = 8.0 * mass, clock_offset = 13.0 * mass;
        const auto initial = radial_infall(parameters, initial_radius, clock_offset);
        WorldlineOptions options{};
        const auto captured = trace_timelike_worldline(parameters, initial, options);
        CHECK(captured.termination == RayTermination::horizon);
        NEAR(captured.diagnostics.radius, 2.0 * mass, 2.0e-9 * mass);
        NEAR(captured.proper_time, infall_proper_time(mass, initial_radius, 2.0 * mass),
             2.0e-8 * mass);
        NEAR(captured.state[0],
             clock_offset + infall_coordinate_time(mass, initial_radius, 2.0 * mass),
             4.0e-8 * mass);
        NEAR(captured.diagnostics.coordinate_time_rate, 1.5, 2.0e-8);
        options.stop_at_horizon = false;
        options.proper_time_limit = infall_proper_time(mass, initial_radius, 1.5 * mass);
        std::size_t samples{};
        const auto across = trace_timelike_worldline(
            parameters, initial, options,
            [&](double tau, const GeodesicState &state, const GeodesicDiagnostics &d) {
                ++samples;
                const double radius = std::pow(
                    std::pow(initial_radius, 1.5) - 1.5 * std::sqrt(2.0 * mass) * tau, 2.0 / 3.0);
                NEAR(d.radius, radius, 3.0e-8 * mass);
                NEAR(state[0], clock_offset + infall_coordinate_time(mass, initial_radius, radius),
                     5.0e-8 * mass);
                NEAR(d.energy, 1.0, 2.0e-15);
                NEAR(d.hamiltonian, -0.5, 3.0e-10);
                return true;
            });
        CHECK(across.termination == RayTermination::proper_time_limit);
        NEAR(across.diagnostics.radius, 1.5 * mass, 3.0e-8 * mass);
        CHECK(samples == across.statistics.accepted_steps + 1);
        CHECK(across.statistics.rhs_evaluations ==
              1 + 6 * (across.statistics.accepted_steps + across.statistics.rejected_steps));
        CHECK(trace_timelike_worldline(parameters, across.state).termination ==
              RayTermination::initially_inside_horizon);
        // Continuing from an interior event owns a new elapsed proper-time clock.
        options.proper_time_limit = 0.05 * mass;
        const auto inside = trace_timelike_worldline(parameters, across.state, options);
        CHECK(inside.termination == RayTermination::proper_time_limit);
        CHECK(inside.state[0] > across.state[0]);
        CHECK(inside.diagnostics.radius < across.diagnostics.radius);
    }
}

GeodesicState reference_endpoint(const KerrParameters &parameters, GeodesicState state, double end,
                                 double step_size) {
    const auto rhs = [&](const GeodesicState &y, GeodesicState &dy) {
        return geodesic_rhs(parameters, y, dy) == GeometryStatus::valid;
    };
    const auto count = static_cast<std::size_t>(std::round(end / step_size));
    for (std::size_t i = 0; i < count; ++i) {
        GeodesicState derivative{};
        CHECK(rhs(state, derivative));
        const auto step = num::extrapolated_midpoint8(rhs, state, derivative, step_size);
        CHECK(step.valid);
        if (!step.valid)
            break;
        state = step.end;
    }
    return state;
}

void inclined_reference_tests() {
    for (double spin : {0.0, 0.8, -0.8, 0.999}) {
        const KerrParameters parameters{1.0, spin};
        const Vector3d position{9.0, 1.0, 3.0};
        const auto velocity = normalized(parameters, position, {1.0, -0.08, 0.12, -0.03});
        const auto initial = make_timelike_state(parameters, position, velocity);
        CHECK(initial.valid());
        WorldlineOptions options{};
        options.proper_time_limit = 15.0;
        const auto result = trace_timelike_worldline(parameters, initial.state, options);
        CHECK(result.termination == RayTermination::proper_time_limit);
        const auto reference = reference_endpoint(parameters, initial.state, 15.0, 0.1);
        const auto refined = reference_endpoint(parameters, initial.state, 15.0, 0.05);
        for (std::size_t i = 0; i < 8; ++i) {
            NEAR(reference[i], refined[i], 2.0e-11);
            NEAR(result.state[i], refined[i], 2.0e-8);
        }
        CHECK(result.statistics.maximum_constraint < 1.0e-9);
        CHECK(result.statistics.maximum_carter_drift < 1.0e-8);
        CHECK(result.statistics.maximum_lz_drift < 1.0e-8);
    }
}

void failure_tests() {
    const KerrParameters parameters{1.0, 0.0};
    const auto initial = radial_infall(parameters, 8.0);
    auto invalid = initial;
    for (std::size_t i = 4; i < 8; ++i)
        invalid[i] *= -1.0;
    CHECK(trace_timelike_worldline(parameters, invalid).termination ==
          RayTermination::invalid_initial_state);
    invalid = initial;
    for (std::size_t i = 4; i < 8; ++i)
        invalid[i] *= 2.0;
    CHECK(trace_timelike_worldline(parameters, invalid).termination ==
          RayTermination::invalid_initial_state);
    CHECK(trace_timelike_worldline(parameters, {}).termination ==
          RayTermination::invalid_initial_state);
    invalid[3] = std::numeric_limits<double>::infinity();
    const auto failed = trace_timelike_worldline(parameters, invalid);
    CHECK(failed.termination == RayTermination::invalid_initial_state);
    CHECK(num::finite(failed.state));
    const auto null_state = make_null_state(parameters, {8.0, 0.0, 0.0}, {-1.0, 0.0, 0.0});
    CHECK(trace_timelike_worldline(parameters, null_state.state).termination ==
          RayTermination::invalid_initial_state);
    CHECK(trace_null_ray(parameters, initial).termination == RayTermination::invalid_initial_state);
    WorldlineOptions options{};
    std::size_t callbacks{};
    const auto cancelled =
        trace_timelike_worldline(parameters, initial, options,
                                 [&](double, const GeodesicState &, const GeodesicDiagnostics &) {
                                     return ++callbacks < 4;
                                 });
    CHECK(cancelled.termination == RayTermination::cancelled);
    CHECK(callbacks == 4 && cancelled.statistics.accepted_steps == 3);
    CHECK(num::finite(cancelled.state) && cancelled.proper_time > 0.0);
    options.maximum_attempts = 1;
    CHECK(trace_timelike_worldline(parameters, initial, options).termination ==
          RayTermination::work_limit);
    options = {};
    options.relative_tolerance = 1.0e-15;
    options.position_tolerance = 1.0e-16;
    options.momentum_tolerance = 1.0e-16;
    options.initial_step = options.minimum_step = options.maximum_step = 0.5;
    CHECK(trace_timelike_worldline(parameters, initial, options).termination ==
          RayTermination::minimum_step);
    options.minimum_step = 1.0e-12;
    options.maximum_rejections = 1;
    CHECK(trace_timelike_worldline(parameters, initial, options).termination ==
          RayTermination::rejection_limit);
    options = {};
    options.proper_time_limit = std::numeric_limits<double>::quiet_NaN();
    bool threw = false;
    try {
        static_cast<void>(trace_timelike_worldline(parameters, initial, options));
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    CHECK(threw);
    // A nearly null large momentum must not conceal an incorrect unit rest mass.
    auto boosted = make_null_state(parameters, {8.0, 0.0, 0.0}, {-1.0e5, 0.0, 0.0}).state;
    const auto boosted_diagnostics = diagnose_timelike_geodesic(parameters, boosted);
    CHECK(boosted_diagnostics.normalized_constraint < 1.0e-7);
    CHECK(trace_timelike_worldline(parameters, boosted).termination ==
          RayTermination::invalid_initial_state);
    CHECK(termination_name(RayTermination::proper_time_limit) == "proper_time_limit");
}
} // namespace

int main() {
    initial_data_tests();
    circular_tests();
    freefall_tests();
    inclined_reference_tests();
    failure_tests();
    std::cout << (failures == 0 ? "PASS: " : "FAIL: ") << checks << " timelike checks, " << failures
              << " failures\n";
    return failures == 0 ? 0 : 1;
}

// refactor: polish timelike_tests.cpp implementation details

// refactor: polish timelike_tests.cpp implementation details

// refactor: polish timelike_tests.cpp implementation details

// refactor: polish timelike_tests.cpp implementation details
