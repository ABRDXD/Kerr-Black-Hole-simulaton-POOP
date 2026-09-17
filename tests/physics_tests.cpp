#include "kerr/math/Matrix4.hpp"
#include "kerr/math/Vector.hpp"
#include "kerr/physics/KerrSpacetime.hpp"
#include "kerr/physics/Units.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace {

class TestSuite final {
public:
    void check(const bool condition, const std::string_view expression,
               const std::string_view file, const int line) {
        ++checks_;
        if (!condition) {
            ++failures_;
            std::cerr << file << ':' << line << ": CHECK failed: " << expression << '\n';
        }
    }

    void near(const double actual, const double expected, const double relative_tolerance,
              const double absolute_tolerance, const std::string_view label,
              const std::string_view file, const int line) {
        ++checks_;
        const double scale = std::max(std::abs(actual), std::abs(expected));
        const double tolerance = absolute_tolerance + relative_tolerance * scale;
        if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
            ++failures_;
            std::cerr << std::setprecision(17) << file << ':' << line << ": " << label
                      << " actual=" << actual << " expected=" << expected
                      << " tolerance=" << tolerance << '\n';
        }
    }

    template <typename Callable>
    void throws_invalid_argument(Callable&& callable, const std::string_view label,
                                 const std::string_view file, const int line) {
        ++checks_;
        try {
            std::forward<Callable>(callable)();
        } catch (const std::invalid_argument&) {
            return;
        } catch (const std::exception& error) {
            ++failures_;
            std::cerr << file << ':' << line << ": " << label
                      << " threw wrong exception: " << error.what() << '\n';
            return;
        }
        ++failures_;
        std::cerr << file << ':' << line << ": " << label
                  << " did not throw std::invalid_argument\n";
    }

    [[nodiscard]] int finish() const {
        if (failures_ == 0) {
            std::cout << "PASS: " << checks_ << " physics foundation checks\n";
            return 0;
        }
        std::cerr << "FAIL: " << failures_ << " of " << checks_ << " checks failed\n";
        return 1;
    }

private:
    int checks_{};
    int failures_{};
};

#define KERR_CHECK(suite, expression) \
    (suite).check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
#define KERR_NEAR(suite, actual, expected, relative_tolerance, absolute_tolerance, label) \
    (suite).near((actual), (expected), (relative_tolerance), (absolute_tolerance), \
                 (label), __FILE__, __LINE__)
#define KERR_THROWS_INVALID(suite, expression) \
    (suite).throws_invalid_argument([&]() { static_cast<void>(expression); }, #expression, \
                                    __FILE__, __LINE__)

[[nodiscard]] double determinant(kerr::math::Matrix4d matrix) {
    double value = 1.0;
    double sign = 1.0;
    for (std::size_t column = 0; column < 4U; ++column) {
        std::size_t pivot_row = column;
        double pivot_magnitude = std::abs(matrix(column, column));
        for (std::size_t row = column + 1U; row < 4U; ++row) {
            const double candidate = std::abs(matrix(row, column));
            if (candidate > pivot_magnitude) {
                pivot_magnitude = candidate;
                pivot_row = row;
            }
        }
        if (pivot_magnitude == 0.0) {
            return 0.0;
        }
        if (pivot_row != column) {
            for (std::size_t entry = 0; entry < 4U; ++entry) {
                std::swap(matrix(column, entry), matrix(pivot_row, entry));
            }
            sign = -sign;
        }
        const double pivot = matrix(column, column);
        value *= pivot;
        for (std::size_t row = column + 1U; row < 4U; ++row) {
            const double factor = matrix(row, column) / pivot;
            for (std::size_t entry = column + 1U; entry < 4U; ++entry) {
                matrix(row, entry) -= factor * matrix(column, entry);
            }
        }
    }
    return sign * value;
}

[[nodiscard]] kerr::math::Vector3d oblate_point(const double radius,
                                                 const double polar_angle,
                                                 const double azimuth,
                                                 const double spin) {
    const double sine_theta = std::sin(polar_angle);
    const double cosine_theta = std::cos(polar_angle);
    const double cosine_phi = std::cos(azimuth);
    const double sine_phi = std::sin(azimuth);
    return {
        (radius * cosine_phi - spin * sine_phi) * sine_theta,
        (radius * sine_phi + spin * cosine_phi) * sine_theta,
        radius * cosine_theta,
    };
}

void test_parameters(TestSuite& suite) {
    using kerr::physics::KerrParameters;
    const KerrParameters valid{2.0, -1.5};
    KERR_NEAR(suite, valid.dimensionless_spin(), -0.75, 0.0, 0.0,
              "dimensionless spin");
    KERR_THROWS_INVALID(suite, KerrParameters(0.0, 0.0));
    KERR_THROWS_INVALID(suite, KerrParameters(-1.0, 0.0));
    KERR_THROWS_INVALID(suite, KerrParameters(1.0, 1.0000001));
    KERR_THROWS_INVALID(
        suite, KerrParameters(std::numeric_limits<double>::infinity(), 0.0));
    KERR_THROWS_INVALID(
        suite, KerrParameters(1.0, std::numeric_limits<double>::quiet_NaN()));
}

void test_radius(TestSuite& suite) {
    using namespace kerr::physics;
    const KerrParameters parameters{1.0, 0.7};
    constexpr double expected_radius = 4.25;
    const auto point = oblate_point(expected_radius, 1.1, 0.4, parameters.spin());
    const auto radius = kerr_schild_radius(parameters, point);
    KERR_CHECK(suite, radius.valid());
    KERR_NEAR(suite, radius.radius, expected_radius, 2.0e-15, 2.0e-15,
              "oblate radius round trip");

    const double rho_squared = point.x * point.x + point.y * point.y + point.z * point.z;
    const double residual = std::pow(radius.radius, 4.0) -
                            radius.radius * radius.radius *
                                (rho_squared - parameters.spin() * parameters.spin()) -
                            parameters.spin() * parameters.spin() * point.z * point.z;
    KERR_NEAR(suite, residual, 0.0, 0.0, 2.0e-12,
              "Kerr-Schild radius polynomial residual");

    const double oblate_identity =
        (point.x * point.x + point.y * point.y) /
            (radius.radius * radius.radius + parameters.spin() * parameters.spin()) +
        point.z * point.z / (radius.radius * radius.radius);
    KERR_NEAR(suite, oblate_identity, 1.0, 2.0e-15, 2.0e-15,
              "oblate coordinate identity");

    const auto enormous =
        kerr_schild_radius(parameters, {1.0e200, -2.0e200, 3.0e200});
    KERR_CHECK(suite, enormous.valid());
    KERR_CHECK(suite, std::isfinite(enormous.radius));

    const auto origin = kerr_schild_radius(parameters, {0.0, 0.0, 0.0});
    KERR_CHECK(suite, !origin.valid());
    KERR_CHECK(suite, origin.status == GeometryStatus::singular_or_degenerate_chart);

    const auto non_finite = kerr_schild_radius(
        parameters, {std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0});
    KERR_CHECK(suite, non_finite.status == GeometryStatus::non_finite_input);
}

void test_metric(TestSuite& suite) {
    using namespace kerr::physics;

    const KerrParameters schwarzschild{1.0, 0.0};
    const auto axis_metric = evaluate_metric(schwarzschild, {10.0, 0.0, 0.0});
    KERR_CHECK(suite, axis_metric.valid());
    KERR_NEAR(suite, axis_metric.radius, 10.0, 0.0, 1.0e-14, "Schwarzschild r");
    KERR_NEAR(suite, axis_metric.profile, 0.2, 1.0e-15, 1.0e-15,
              "Schwarzschild Kerr-Schild profile");
    KERR_NEAR(suite, axis_metric.covariant(0, 0), -0.8, 1.0e-15, 1.0e-15,
              "Schwarzschild g_tt");
    KERR_NEAR(suite, axis_metric.covariant(0, 1), 0.2, 1.0e-15, 1.0e-15,
              "Schwarzschild g_tx");
    KERR_NEAR(suite, axis_metric.covariant(1, 1), 1.2, 1.0e-15, 1.0e-15,
              "Schwarzschild g_xx");

    const KerrParameters kerr_parameters{1.0, 0.83};
    const std::array<kerr::math::Vector3d, 4> points{
        oblate_point(8.0, 0.8, -0.3, kerr_parameters.spin()),
        oblate_point(2.0, 1.4, 2.2, kerr_parameters.spin()),
        oblate_point(1.7, 0.2, 1.0, kerr_parameters.spin()),
        oblate_point(100.0, 2.1, -2.0, kerr_parameters.spin()),
    };

    for (const auto point : points) {
        const auto metric = evaluate_metric(kerr_parameters, point);
        KERR_CHECK(suite, metric.valid());

        const double flat_null_norm =
            -metric.null_covector[0] * metric.null_covector[0] +
            metric.null_covector[1] * metric.null_covector[1] +
            metric.null_covector[2] * metric.null_covector[2] +
            metric.null_covector[3] * metric.null_covector[3];
        KERR_NEAR(suite, flat_null_norm, 0.0, 0.0, 4.0e-15,
                  "Kerr-Schild covector nullness");

        for (std::size_t row = 0; row < 4U; ++row) {
            for (std::size_t column = 0; column < 4U; ++column) {
                KERR_NEAR(suite, metric.covariant(row, column),
                          metric.covariant(column, row), 0.0, 0.0,
                          "covariant metric symmetry");
                KERR_NEAR(suite, metric.contravariant(row, column),
                          metric.contravariant(column, row), 0.0, 0.0,
                          "inverse metric symmetry");

                double product{};
                for (std::size_t contracted = 0; contracted < 4U; ++contracted) {
                    product += metric.covariant(row, contracted) *
                               metric.contravariant(contracted, column);
                }
                const double expected = row == column ? 1.0 : 0.0;
                KERR_NEAR(suite, product, expected, 2.0e-14, 2.0e-14,
                          "metric inverse product");
            }
        }

        KERR_NEAR(suite, determinant(metric.covariant), -1.0, 2.0e-13, 2.0e-13,
                  "Kerr-Schild metric determinant");
        KERR_NEAR(suite, hamiltonian(metric.contravariant, metric.null_covector),
                  0.0, 0.0, 4.0e-15, "principal null Hamiltonian");

        const auto raised = raise_index(metric.contravariant, metric.null_covector);
        KERR_NEAR(suite, raised[0], -1.0, 2.0e-14, 2.0e-14,
                  "raise Kerr-Schild null time component");
        for (std::size_t index = 1U; index < 4U; ++index) {
            KERR_NEAR(suite, raised[index], metric.null_covector[index], 2.0e-14,
                      2.0e-14, "raise Kerr-Schild null spatial component");
        }
    }

    const auto far_metric = evaluate_metric(kerr_parameters, {1.0e7, 0.0, 0.0});
    KERR_CHECK(suite, far_metric.valid());
    KERR_CHECK(suite, std::abs(far_metric.covariant(0, 0) + 1.0) < 3.0e-7);
    KERR_CHECK(suite, std::abs(far_metric.covariant(1, 1) - 1.0) < 3.0e-7);

    const auto invalid_metric = evaluate_metric(kerr_parameters, {0.0, 0.0, 0.0});
    KERR_CHECK(suite, !invalid_metric.valid());
}

void test_landmarks(TestSuite& suite) {
    using namespace kerr::physics;
    constexpr double pi = std::numbers::pi_v<double>;

    const KerrParameters schwarzschild{1.0, 0.0};
    const auto horizons = horizon_radii(schwarzschild);
    KERR_NEAR(suite, horizons.inner, 0.0, 0.0, 0.0, "Schwarzschild inner horizon");
    KERR_NEAR(suite, horizons.outer, 2.0, 0.0, 0.0, "Schwarzschild horizon");
    KERR_NEAR(suite, outer_ergosphere_radius(schwarzschild, 0.37), 2.0, 0.0, 0.0,
              "Schwarzschild stationary limit");
    KERR_NEAR(suite, equatorial_isco_radius(schwarzschild, OrbitSense::prograde),
              6.0, 0.0, 2.0e-15, "Schwarzschild ISCO");
    KERR_NEAR(suite,
              equatorial_photon_orbit_radius(schwarzschild, OrbitSense::retrograde),
              3.0, 2.0e-15, 2.0e-15, "Schwarzschild photon orbit");

    const KerrParameters moderate{1.0, 0.5};
    KERR_NEAR(suite, equatorial_isco_radius(moderate, OrbitSense::prograde),
              4.233002529530826, 3.0e-15, 3.0e-15, "a=0.5 prograde ISCO");
    KERR_NEAR(suite, equatorial_isco_radius(moderate, OrbitSense::retrograde),
              7.554584714512358, 3.0e-15, 3.0e-15, "a=0.5 retrograde ISCO");
    KERR_NEAR(suite,
              equatorial_photon_orbit_radius(moderate, OrbitSense::prograde),
              2.3472963553338606, 3.0e-15, 3.0e-15,
              "a=0.5 prograde photon orbit");
    KERR_NEAR(suite,
              equatorial_photon_orbit_radius(moderate, OrbitSense::retrograde),
              3.532088886237956, 3.0e-15, 3.0e-15,
              "a=0.5 retrograde photon orbit");
    KERR_NEAR(suite, outer_ergosphere_radius(moderate, pi / 2.0), 2.0,
              2.0e-15, 2.0e-15, "equatorial stationary limit");

    const KerrParameters extremal{2.0, 2.0};
    const auto extremal_horizons = horizon_radii(extremal);
    KERR_NEAR(suite, extremal_horizons.inner, 2.0, 0.0, 0.0,
              "extremal inner horizon");
    KERR_NEAR(suite, extremal_horizons.outer, 2.0, 0.0, 0.0,
              "extremal outer horizon");
    KERR_NEAR(suite, equatorial_isco_radius(extremal, OrbitSense::prograde),
              2.0, 0.0, 2.0e-14, "extremal prograde ISCO");
    KERR_NEAR(suite, equatorial_isco_radius(extremal, OrbitSense::retrograde),
              18.0, 0.0, 2.0e-14, "extremal retrograde ISCO");
    KERR_NEAR(suite,
              equatorial_photon_orbit_radius(extremal, OrbitSense::prograde),
              2.0, 0.0, 2.0e-14, "extremal prograde photon orbit");
    KERR_NEAR(suite,
              equatorial_photon_orbit_radius(extremal, OrbitSense::retrograde),
              8.0, 0.0, 2.0e-14, "extremal retrograde photon orbit");

    const KerrParameters negative_spin{1.0, -0.5};
    KERR_NEAR(suite,
              equatorial_isco_radius(negative_spin, OrbitSense::prograde),
              equatorial_isco_radius(moderate, OrbitSense::prograde), 0.0, 0.0,
              "relative prograde radius is sign independent");
}

void test_units(TestSuite& suite) {
    using namespace kerr::physics;
    const PhysicalScale solar_mass{1.0L};
    KERR_NEAR(suite, static_cast<double>(solar_mass.length_metres_per_m()),
              1476.6696910334392, 3.0e-15, 3.0e-12,
              "one-solar-mass geometrized length");
    KERR_NEAR(suite, static_cast<double>(solar_mass.time_seconds_per_m()),
              4.925639893961039e-6, 3.0e-15, 3.0e-18,
              "one-solar-mass geometrized time");

    const PhysicalScale sagittarius_a{4.297e6L};
    KERR_NEAR(suite, static_cast<double>(sagittarius_a.distance_metres(2.0L)),
              2.0 * 4.297e6 * 1476.6696910334392, 4.0e-15, 1.0e-3,
              "mass-scaled horizon display length");
    KERR_THROWS_INVALID(suite, PhysicalScale(0.0L));
    KERR_THROWS_INVALID(
        suite, PhysicalScale(std::numeric_limits<long double>::quiet_NaN()));
}

} // namespace

int main() {
    TestSuite suite;
    test_parameters(suite);
    test_radius(suite);
    test_metric(suite);
    test_landmarks(suite);
    test_units(suite);
    return suite.finish();
}


// test(numerics): verify quartic polynomial roots accuracy against Ferrari closed form

// Extremal Kerr limit

// refactor: polish physics_tests.cpp implementation details

// refactor: polish physics_tests.cpp implementation details

// refactor: polish physics_tests.cpp implementation details

// refactor: polish physics_tests.cpp implementation details

// refactor: polish physics_tests.cpp implementation details

// refactor: polish physics_tests.cpp implementation details

// refactor: polish physics_tests.cpp implementation details

// refactor: polish physics_tests.cpp implementation details
