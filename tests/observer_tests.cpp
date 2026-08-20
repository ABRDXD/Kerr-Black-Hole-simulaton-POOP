#include "kerr/physics/Observer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

using namespace kerr::physics;
using kerr::math::Vector3d;
using kerr::math::Vector4d;

class TestSuite final {
  public:
    void check(const bool condition, const std::string_view label, const int line) {
        ++checks_;
        if (!condition) {
            ++failures_;
            std::cerr << "observer_tests.cpp:" << line << ": CHECK " << label << '\n';
        }
    }
    void near(const double actual, const double expected, const double tolerance,
              const std::string_view label, const int line) {
        ++checks_;
        if (!std::isfinite(actual) || std::abs(actual - expected) > tolerance) {
            ++failures_;
            std::cerr << std::setprecision(17) << "observer_tests.cpp:" << line << ": " << label
                      << " actual=" << actual << " expected=" << expected
                      << " tolerance=" << tolerance << '\n';
        }
    }
    [[nodiscard]] int finish() const {
        std::cout << (failures_ == 0 ? "PASS: " : "FAIL: ") << checks_ << " observer checks, "
                  << failures_ << " failures\n";
        return failures_ == 0 ? 0 : 1;
    }

  private:
    int checks_{};
    int failures_{};
};

#define CHECK(expression) suite.check(static_cast<bool>(expression), #expression, __LINE__)
#define NEAR(actual, expected, tolerance)                                                          \
    suite.near((actual), (expected), (tolerance), #actual, __LINE__)

[[nodiscard]] Vector4d event_at(const double radius, const double theta, const double azimuth,
                                const double spin) {
    return {3.5, (radius * std::cos(azimuth) - spin * std::sin(azimuth)) * std::sin(theta),
            (radius * std::sin(azimuth) + spin * std::cos(azimuth)) * std::sin(theta),
            radius * std::cos(theta)};
}

[[nodiscard]] double inner(const kerr::math::Matrix4d &metric, const Vector4d &first,
                           const Vector4d &second) {
    long double value{};
    for (std::size_t i = 0; i < 4U; ++i) {
        for (std::size_t j = 0; j < 4U; ++j) {
            value += static_cast<long double>(metric(i, j)) * static_cast<long double>(first[i]) *
                     static_cast<long double>(second[j]);
        }
    }
    return static_cast<double>(value);
}

[[nodiscard]] double determinant(const ObserverEvent &observer) {
    // Independent permutation determinant, rather than the implementation's LU.
    std::array<std::size_t, 4> permutation{0U, 1U, 2U, 3U};
    long double determinant_value{};
    do {
        unsigned int inversions{};
        long double term = 1.0L;
        for (std::size_t i = 0; i < 4U; ++i) {
            term *= static_cast<long double>(observer.tetrad[i][permutation[i]]);
            for (std::size_t j = i + 1U; j < 4U; ++j) {
                if (permutation[i] > permutation[j]) {
                    ++inversions;
                }
            }
        }
        determinant_value += inversions % 2U == 0U ? term : -term;
    } while (std::next_permutation(permutation.begin(), permutation.end()));
    return static_cast<double>(determinant_value);
}

[[nodiscard]] Vector4d momentum(const GeodesicState &state) {
    return {state[4], state[5], state[6], state[7]};
}

[[nodiscard]] double contract(const Vector4d &covector, const Vector4d &vector) {
    long double sum{};
    for (std::size_t i = 0; i < 4U; ++i) {
        sum += static_cast<long double>(covector[i]) * static_cast<long double>(vector[i]);
    }
    return static_cast<double>(sum);
}

void check_frame(TestSuite &suite, const KerrParameters &parameters,
                 const ObserverEvaluation &result, const double tolerance = 2.0e-12) {
    CHECK(result.valid());
    if (!result.valid()) {
        return;
    }
    CHECK(validate_observer(parameters, result.observer) == ObserverStatus::valid);
    const auto &event = result.observer;
    const auto metric =
        evaluate_metric(parameters, {event.position[1], event.position[2], event.position[3]});
    CHECK(event.tetrad[0][0] > 0.0);
    for (std::size_t a = 0; a < 4U; ++a) {
        for (std::size_t b = 0; b < 4U; ++b) {
            NEAR(inner(metric.covariant, event.tetrad[a], event.tetrad[b]),
                 a == b ? (a == 0U ? -1.0 : 1.0) : 0.0, tolerance);
        }
    }
    NEAR(determinant(event), 1.0, tolerance * 4.0);
}

void check_zero_failure(TestSuite &suite, const ObserverEvaluation &result,
                        const ObserverStatus expected) {
    CHECK(result.status == expected);
    CHECK(!result.valid());
    for (const double value : result.observer.position) {
        NEAR(value, 0.0, 0.0);
    }
    NEAR(result.observer.proper_time, 0.0, 0.0);
    for (const auto &basis : result.observer.tetrad) {
        for (const double value : basis) {
            NEAR(value, 0.0, 0.0);
        }
    }
}

void test_eulerian_and_boosts(TestSuite &suite) {
    for (const double spin : {0.0, 0.83, -0.83, 0.999999, 1.0, -1.0}) {
        const KerrParameters parameters{1.0, spin};
        const double horizon = horizon_radii(parameters).outer;
        const std::array<Vector4d, 8> positions{Vector4d{0.0, 0.0, 0.0, 4.0},
                                                Vector4d{0.0, 0.0, 0.0, -4.0},
                                                Vector4d{0.0, 1.0e-14, -2.0e-14, 4.0},
                                                event_at(8.0, 0.8, 0.4, spin),
                                                event_at(horizon, 1.1, 0.2, spin),
                                                event_at(horizon * 0.85, 1.0, 0.2, spin),
                                                event_at(horizon * 1.000001, 1.5, -0.2, spin),
                                                Vector4d{0.0, 1.0e14, -2.0e14, 3.0e14}};
        for (const auto &position : positions) {
            const auto base = make_eulerian_observer(parameters, position, 7.25);
            check_frame(suite, parameters, base);
            if (!base.valid()) {
                continue;
            }
            CHECK(base.observer.position == position);
            NEAR(base.observer.proper_time, 7.25, 0.0);
            const auto metric =
                evaluate_metric(parameters, {position[1], position[2], position[3]});
            Vector4d normal_covector{};
            for (std::size_t mu = 0; mu < 4U; ++mu) {
                for (std::size_t nu = 0; nu < 4U; ++nu) {
                    normal_covector[mu] += metric.covariant(mu, nu) * base.observer.tetrad[0][nu];
                }
            }
            NEAR(normal_covector[0], -1.0 / std::sqrt(1.0 + metric.profile), 2.0e-14);
            for (std::size_t i = 1U; i < 4U; ++i) {
                NEAR(normal_covector[i], 0.0, 2.0e-14);
            }
            for (const Vector3d velocity : {Vector3d{0.0, 0.0, 0.0}, Vector3d{0.3, -0.4, 0.2},
                                            Vector3d{-0.8, 0.1, 0.2}, Vector3d{0.999, 0.0, 0.0}}) {
                const auto boosted =
                    make_local_velocity_observer(parameters, position, velocity, 7.25);
                check_frame(suite, parameters, boosted, 1.0e-10);
                if (!boosted.valid()) {
                    continue;
                }
                const double gamma =
                    1.0 / std::sqrt(1.0 - velocity.x * velocity.x - velocity.y * velocity.y -
                                    velocity.z * velocity.z);
                NEAR(-inner(metric.covariant, base.observer.tetrad[0], boosted.observer.tetrad[0]),
                     gamma, 2.0e-11);
                const std::array<double, 3> components{velocity.x, velocity.y, velocity.z};
                for (std::size_t i = 0; i < 3U; ++i) {
                    NEAR(inner(metric.covariant, base.observer.tetrad[i + 1U],
                               boosted.observer.tetrad[0]),
                         gamma * components[i], 2.0e-11);
                }
                const auto copied =
                    make_observer(parameters, position, boosted.observer.tetrad[0], 7.25);
                CHECK(copied.valid());
                CHECK(copied.observer.tetrad[0] == boosted.observer.tetrad[0]);
            }
        }
    }
}

void test_stationary(TestSuite &suite) {
    const KerrParameters schwarzschild{1.0, 0.0};
    const auto stationary = make_stationary_observer(schwarzschild, {9.0, 10.0, 0.0, 0.0}, 4.5);
    check_frame(suite, schwarzschild, stationary);
    NEAR(stationary.observer.tetrad[0][0], 1.0 / std::sqrt(0.8), 2.0e-15);
    NEAR(stationary.observer.proper_time, 4.5, 0.0);
    check_zero_failure(suite, make_stationary_observer(schwarzschild, {0.0, 2.0, 0.0, 0.0}),
                       ObserverStatus::stationary_unavailable);
    for (const double spin : {-0.8, 0.8}) {
        const KerrParameters parameters{1.0, spin};
        check_frame(suite, parameters, make_stationary_observer(parameters, {0.0, 2.1, spin, 0.0}));
        check_zero_failure(suite, make_stationary_observer(parameters, {0.0, 1.8, spin, 0.0}),
                           ObserverStatus::stationary_unavailable);
        check_frame(suite, parameters, make_eulerian_observer(parameters, {0.0, 1.8, spin, 0.0}));
        check_frame(suite, parameters, make_stationary_observer(parameters, {0.0, 0.0, 0.0, 1.8}));
    }
}

void test_camera_rays(TestSuite &suite) {
    for (const double spin : {0.0, 0.9, -0.9, 1.0}) {
        const KerrParameters parameters{1.0, spin};
        for (const auto &position : {Vector4d{5.0, 0.0, 0.0, 2.0}, Vector4d{5.0, 1.3, spin, 0.0},
                                     event_at(7.0, 0.9, 2.0, spin)}) {
            const auto observer =
                make_local_velocity_observer(parameters, position, {0.2, -0.4, 0.3}, 7.0);
            CHECK(observer.valid());
            for (const Vector3d direction :
                 {Vector3d{1.0, 0.0, 0.0}, Vector3d{-1.0, 0.0, 0.0}, Vector3d{0.0, 1.0, 0.0},
                  Vector3d{0.0, 0.0, 1.0}, Vector3d{1.0, -2.0, 3.0}}) {
                for (const bool past : {false, true}) {
                    constexpr double frequency = 2.7;
                    const auto ray =
                        make_camera_ray(parameters, observer.observer, direction, frequency, past);
                    CHECK(ray.valid());
                    if (!ray.valid()) {
                        continue;
                    }
                    const auto metric =
                        evaluate_metric(parameters, {position[1], position[2], position[3]});
                    const auto p = momentum(ray.state);
                    const auto tangent = raise_index(metric.contravariant, p);
                    CHECK(past ? tangent[0] < 0.0 : tangent[0] > 0.0);
                    NEAR(hamiltonian(metric.contravariant, p), 0.0, 3.0e-13);
                    const auto measured =
                        measured_frequency(parameters, observer.observer, p, past);
                    CHECK(measured.valid());
                    NEAR(measured.frequency, frequency, 3.0e-14);
                    const double magnitude = std::hypot(direction.x, direction.y, direction.z);
                    const std::array<double, 3> components{direction.x, direction.y, direction.z};
                    for (std::size_t i = 0; i < 3U; ++i) {
                        NEAR(contract(p, observer.observer.tetrad[i + 1U]) / frequency,
                             components[i] / magnitude, 2.0e-14);
                    }
                    for (std::size_t mu = 0; mu < 4U; ++mu) {
                        NEAR(ray.state[mu], position[mu], 0.0);
                    }
                    const auto reversed = make_camera_ray(
                        parameters, observer.observer, {-direction.x, -direction.y, -direction.z},
                        frequency, !past);
                    CHECK(reversed.valid());
                    for (std::size_t mu = 4U; mu < 8U; ++mu) {
                        NEAR(reversed.state[mu], -ray.state[mu], 0.0);
                    }
                }
            }
        }
    }
}

void test_aberration_and_doppler(TestSuite &suite) {
    const KerrParameters parameters{1.0, 0.6};
    for (const Vector4d position :
         {Vector4d{0.0, 1.0e16, 0.0, 0.0}, Vector4d{0.0, 1.7, 0.6, 0.0}}) {
        const auto base = make_eulerian_observer(parameters, position);
        constexpr double speed = 0.6;
        constexpr double gamma = 1.25;
        constexpr double frequency = 3.0;
        const auto moving = make_local_velocity_observer(parameters, position, {speed, 0.0, 0.0});
        CHECK(base.valid());
        CHECK(moving.valid());
        for (const double cosine : {-1.0, -0.4, 0.0, 0.7, 1.0}) {
            const Vector3d direction{cosine, std::sqrt(1.0 - cosine * cosine), 0.0};
            const auto ray = make_camera_ray(parameters, moving.observer, direction, frequency);
            CHECK(ray.valid());
            const auto base_frequency =
                measured_frequency(parameters, base.observer, momentum(ray.state));
            CHECK(base_frequency.valid());
            NEAR(base_frequency.frequency, gamma * frequency * (1.0 + speed * cosine), 2.0e-13);
            NEAR(contract(momentum(ray.state), base.observer.tetrad[1]) / base_frequency.frequency,
                 (cosine + speed) / (1.0 + speed * cosine), 5.0e-14);
            const auto rest_ray = make_camera_ray(parameters, base.observer, direction, frequency);
            const auto moving_frequency =
                measured_frequency(parameters, moving.observer, momentum(rest_ray.state));
            CHECK(moving_frequency.valid());
            NEAR(moving_frequency.frequency, gamma * frequency * (1.0 - speed * cosine), 2.0e-13);
            const auto past_ray =
                make_camera_ray(parameters, moving.observer, direction, frequency, true);
            const auto past_frequency =
                measured_frequency(parameters, base.observer, momentum(past_ray.state), true);
            CHECK(past_frequency.valid());
            NEAR(past_frequency.frequency, gamma * frequency * (1.0 - speed * cosine), 2.0e-13);
            if (position[1] > 1.0e15) {
                // An independent coordinate Minkowski comparison at finite far field.
                NEAR(-ray.state[4], gamma * frequency * (1.0 + speed * cosine), 2.0e-13);
                NEAR(ray.state[5], gamma * frequency * (cosine + speed), 2.0e-13);
            }
        }
    }
}

void test_circular_orbits(TestSuite &suite) {
    for (const double mass : {0.25, 1.0, 4.0}) {
        for (const double chi : {0.0, 0.5, -0.5, 0.99, -0.99, 1.0, -1.0}) {
            const KerrParameters parameters{mass, chi * mass};
            for (const auto sense : {OrbitSense::prograde, OrbitSense::retrograde}) {
                const double photon = equatorial_photon_orbit_radius(parameters, sense);
                for (const double radius : {photon * 1.05, 10.0 * mass}) {
                    const Vector4d position{2.5, radius, parameters.spin(), 0.0};
                    const auto observer = make_circular_observer(parameters, position, sense, 8.0);
                    check_frame(suite, parameters, observer, 3.0e-11);
                    if (!observer.valid()) {
                        continue;
                    }
                    const auto &velocity = observer.observer.tetrad[0];
                    const double sign =
                        (chi < 0.0 ? -1.0 : 1.0) * (sense == OrbitSense::prograde ? 1.0 : -1.0);
                    const double r = radius / mass;
                    const double correction = sign * chi / std::pow(r, 1.5);
                    const double denominator = std::sqrt(1.0 - 3.0 / r + 2.0 * correction);
                    const double omega = sign / (mass * (std::pow(r, 1.5) + sign * chi));
                    const double ut = (1.0 + correction) / denominator;
                    NEAR(velocity[0], ut, 2.0e-10);
                    NEAR(velocity[1] / velocity[0], -omega * position[2], 2.0e-12);
                    NEAR(velocity[2] / velocity[0], omega * position[1], 2.0e-12);
                    const auto metric =
                        evaluate_metric(parameters, {position[1], position[2], position[3]});
                    Vector4d p{};
                    for (std::size_t mu = 0; mu < 4U; ++mu) {
                        for (std::size_t nu = 0; nu < 4U; ++nu) {
                            p[mu] += metric.covariant(mu, nu) * velocity[nu];
                        }
                    }
                    NEAR(-p[0], (1.0 - 2.0 / r + correction) / denominator, 2.0e-10);
                    NEAR(position[1] * p[2] - position[2] * p[1],
                         sign * mass * std::sqrt(r) *
                             (1.0 - 2.0 * correction + chi * chi / (r * r)) / denominator,
                         2.0e-9);
                    // Differentiate the prescribed circular orbit independently,
                    // then compare Hamilton's dp/dtau to rotation of covariant p.
                    const GeodesicState state{position[0], position[1], position[2], 0.0,
                                              p[0],        p[1],        p[2],        p[3]};
                    GeodesicState derivative{};
                    CHECK(geodesic_rhs(parameters, state, derivative) == GeometryStatus::valid);
                    NEAR(derivative[4], 0.0, 2.0e-12);
                    NEAR(derivative[5], -omega * velocity[0] * p[2], 3.0e-9 / mass);
                    NEAR(derivative[6], omega * velocity[0] * p[1], 3.0e-9 / mass);
                    NEAR(derivative[7], 0.0, 2.0e-12);
                }
                check_zero_failure(
                    suite,
                    make_circular_observer(parameters, {0.0, photon * 0.99, parameters.spin(), 0.0},
                                           sense),
                    ObserverStatus::circular_orbit_unavailable);
            }
        }
    }
    const KerrParameters parameters{1.0, 0.9};
    check_zero_failure(
        suite, make_circular_observer(parameters, {0.0, 10.0, 0.0, 1.0e-15}, OrbitSense::prograde),
        ObserverStatus::circular_orbit_unavailable);
    check_zero_failure(
        suite,
        make_circular_observer(parameters, {0.0, 10.0, 0.0, 0.0}, static_cast<OrbitSense>(42)),
        ObserverStatus::circular_orbit_unavailable);
}

void test_failures(TestSuite &suite) {
    const KerrParameters parameters{1.0, 0.7};
    const Vector4d position{0.0, 10.0, 0.0, 0.0};
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    const auto base = make_eulerian_observer(parameters, position);
    CHECK(base.valid());
    check_zero_failure(suite, make_eulerian_observer(parameters, {nan, 10.0, 0.0, 0.0}),
                       ObserverStatus::non_finite_input);
    check_zero_failure(suite, make_eulerian_observer(parameters, position, infinity),
                       ObserverStatus::non_finite_input);
    check_zero_failure(suite, make_eulerian_observer(parameters, {0.0, 0.0, 0.0, 0.0}),
                       ObserverStatus::invalid_geometry);
    check_zero_failure(suite, make_observer(parameters, position, {nan, 0.0, 0.0, 0.0}),
                       ObserverStatus::non_finite_input);
    check_zero_failure(suite, make_observer(parameters, position, {}),
                       ObserverStatus::invalid_four_velocity);
    check_zero_failure(suite, make_observer(parameters, position, {1.0, 10.0, 0.0, 0.0}),
                       ObserverStatus::invalid_four_velocity);
    auto scaled = base.observer.tetrad[0];
    for (double &value : scaled) {
        value *= 2.0;
    }
    check_zero_failure(suite, make_observer(parameters, position, scaled),
                       ObserverStatus::invalid_four_velocity);
    for (double &value : scaled) {
        value *= -0.5;
    }
    check_zero_failure(suite, make_observer(parameters, position, scaled),
                       ObserverStatus::invalid_four_velocity);
    check_zero_failure(suite, make_local_velocity_observer(parameters, position, {1.0, 0.0, 0.0}),
                       ObserverStatus::invalid_four_velocity);
    check_zero_failure(suite, make_local_velocity_observer(parameters, position, {0.9, 0.9, 0.0}),
                       ObserverStatus::invalid_four_velocity);
    check_zero_failure(suite, make_local_velocity_observer(parameters, position, {nan, 0.0, 0.0}),
                       ObserverStatus::non_finite_input);

    auto corrupt = base.observer;
    corrupt.tetrad[1][1] += 0.01;
    CHECK(validate_observer(parameters, corrupt) == ObserverStatus::invalid_tetrad);
    CHECK(make_camera_ray(parameters, corrupt, {1.0, 0.0, 0.0}).status ==
          ObserverStatus::invalid_tetrad);
    corrupt = base.observer;
    std::swap(corrupt.tetrad[1], corrupt.tetrad[2]);
    CHECK(validate_observer(parameters, corrupt) == ObserverStatus::invalid_tetrad);
    corrupt = base.observer;
    corrupt.tetrad[2][2] = infinity;
    CHECK(validate_observer(parameters, corrupt) == ObserverStatus::non_finite_input);

    for (const auto &invalid_ray :
         {make_camera_ray(parameters, base.observer, {}),
          make_camera_ray(parameters, base.observer, {nan, 0.0, 0.0}),
          make_camera_ray(parameters, base.observer, {1.0, 0.0, 0.0}, 0.0),
          make_camera_ray(parameters, base.observer, {1.0, 0.0, 0.0}, -1.0),
          make_camera_ray(parameters, base.observer, {1.0, 0.0, 0.0}, infinity),
          make_camera_ray(parameters, base.observer, {1.0, 0.0, 0.0}, 1.0e308)}) {
        CHECK(!invalid_ray.valid());
        for (const double value : invalid_ray.state) {
            NEAR(value, 0.0, 0.0);
        }
    }
    CHECK(make_camera_ray(parameters, base.observer, {1.0e308, -1.0e308, 0.0}).valid());
    CHECK(make_camera_ray(parameters, base.observer, {1.0e-300, 0.0, 0.0}).valid());
    CHECK(make_camera_ray(parameters, base.observer, {}).status ==
          ObserverStatus::invalid_direction);
    CHECK(make_camera_ray(parameters, base.observer, {1.0, 0.0, 0.0}, -1.0).status ==
          ObserverStatus::invalid_frequency);
    const auto ray = make_camera_ray(parameters, base.observer, {1.0, 0.0, 0.0});
    const auto wrong_time =
        measured_frequency(parameters, base.observer, momentum(ray.state), true);
    CHECK(wrong_time.status == ObserverStatus::invalid_frequency);
    NEAR(wrong_time.frequency, 0.0, 0.0);
    const auto bad_frequency = measured_frequency(parameters, base.observer, {nan, 0.0, 0.0, 0.0});
    CHECK(bad_frequency.status == ObserverStatus::non_finite_input);
    NEAR(bad_frequency.frequency, 0.0, 0.0);
}

} // namespace

int main() {
    TestSuite suite;
    test_eulerian_and_boosts(suite);
    test_stationary(suite);
    test_camera_rays(suite);
    test_aberration_and_doppler(suite);
    test_circular_orbits(suite);
    test_failures(suite);
    return suite.finish();
}
