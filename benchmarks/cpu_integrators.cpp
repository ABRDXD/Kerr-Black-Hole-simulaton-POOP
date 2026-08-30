#include "kerr/numerics/Integrators.hpp"
#include "kerr/physics/Geodesic.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace kerr::physics;
namespace num = kerr::numerics;
enum class Method { dopri5, midpoint8, rk4 };
std::string_view name(Method method) {
    switch (method) {
    case Method::dopri5:
        return "dopri5";
    case Method::midpoint8:
        return "midpoint8";
    case Method::rk4:
        return "rk4_fixed";
    }
    return "unknown";
}
struct Run {
    GeodesicState state{};
    std::size_t accepted{}, rejected{}, evaluations{};
    double max_constraint{};
};
Run integrate(const KerrParameters &parameters, const GeodesicState &initial, Method method,
              double end, double tolerance, double fixed_step) {
    Run run{initial};
    auto rhs = [&](const GeodesicState &state, GeodesicState &derivative) {
        ++run.evaluations;
        return geodesic_rhs(parameters, state, derivative) == GeometryStatus::valid;
    };
    double lambda{}, h = method == Method::rk4 ? fixed_step : 0.02;
    GeodesicState first{};
    if (!rhs(run.state, first))
        throw std::runtime_error{"Invalid benchmark initial state"};
    num::State<8> atol{};
    atol.fill(tolerance * 0.01);
    while (lambda < end) {
        if (run.accepted + run.rejected >= 100000 || h < 1.0e-14)
            throw std::runtime_error{"Benchmark exhausted work budget"};
        h = std::min(h, end - lambda);
        num::Step<8> step{};
        if (method == Method::rk4) {
            std::array<GeodesicState, 4> k{};
            k[0] = first;
            for (std::size_t stage = 1; stage < 4; ++stage) {
                GeodesicState intermediate{};
                const double fraction = stage == 3 ? 1.0 : 0.5;
                for (std::size_t i = 0; i < 8; ++i)
                    intermediate[i] = run.state[i] + fraction * h * k[stage - 1][i];
                if (!rhs(intermediate, k[stage]))
                    throw std::runtime_error{"Invalid RK4 stage"};
            }
            for (std::size_t i = 0; i < 8; ++i)
                step.end[i] =
                    run.state[i] + h * (k[0][i] + 2.0 * k[1][i] + 2.0 * k[2][i] + k[3][i]) / 6.0;
            step.valid = rhs(step.end, step.endpoint_derivative);
        } else if (method == Method::dopri5)
            step = num::dopri5(rhs, run.state, first, h);
        else
            step = num::extrapolated_midpoint8(rhs, run.state, first, h);
        const double error = step.valid ? num::scaled_error(run.state, step, atol, tolerance)
                                        : std::numeric_limits<double>::infinity();
        const double exponent = method == Method::midpoint8 ? 1.0 / 7.0 : 0.2;
        if (!step.valid || !std::isfinite(error) || error > 1.0) {
            ++run.rejected;
            if (method == Method::rk4)
                throw std::runtime_error{"RK4 failed"};
            h *= num::step_factor(error, exponent, true);
            continue;
        }
        run.state = step.end;
        first = step.endpoint_derivative;
        lambda += h;
        ++run.accepted;
        const auto diagnostics = diagnose_null_geodesic(parameters, run.state);
        if (!diagnostics.valid())
            throw std::runtime_error{"Invalid benchmark diagnostics"};
        run.max_constraint = std::max(run.max_constraint, diagnostics.normalized_constraint);
        h = method == Method::rk4 ? fixed_step
                                  : std::min(0.5, h * num::step_factor(error, exponent, false));
    }
    return run;
}
double distance(const GeodesicState &a, const GeodesicState &b) {
    double error{};
    for (std::size_t i = 0; i < 8; ++i)
        error = std::max(error, std::abs(a[i] - b[i]));
    return error;
}
} // namespace

int main(int argc, char **argv) {
    try {
        std::ofstream file;
        if (argc == 3 && std::string{argv[1]} == "--csv") {
            const std::filesystem::path path{argv[2]};
            if (path.has_parent_path())
                std::filesystem::create_directories(path.parent_path());
            file.open(path);
            if (!file)
                throw std::runtime_error{"Could not open benchmark CSV"};
        } else if (argc != 1)
            throw std::invalid_argument{"Usage: kerr_benchmark [--csv PATH]"};
        std::ostream &output = file.is_open() ? file : std::cout;
        output << "case,method,rtol,fixed_step,accepted,rejected,rhs_evaluations,median_ms,"
                  "endpoint_max_abs_error,max_constraint,reference_pair_error\n"
               << std::setprecision(12);
        struct Case {
            std::string_view label;
            double spin;
            kerr::math::Vector3d position, momentum;
            double end;
        };
        const std::array<Case, 3> cases{
            {{"weak", 0.0, {100.0, 1.0, 2.0}, {-0.8, 0.4, 0.1}, 12.0},
             {"kerr_strong", 0.8, {10.0, 1.0, 3.0}, {-0.8, 0.4, -0.1}, 12.0},
             {"near_extremal", 0.999, {5.0, 1.0, 0.7}, {-0.2, 0.8, 0.1}, 8.0}}};
        for (const auto &scenario : cases) {
            const KerrParameters parameters{1.0, scenario.spin};
            const auto initial = make_null_state(parameters, scenario.position, scenario.momentum);
            if (!initial.valid())
                throw std::runtime_error{"Invalid benchmark initial data"};
            // Two independent high-order tolerance levels bound reference sensitivity.
            const auto truth =
                integrate(parameters, initial.state, Method::midpoint8, scenario.end, 2.0e-14, 0.0);
            const auto check =
                integrate(parameters, initial.state, Method::midpoint8, scenario.end, 8.0e-15, 0.0);
            const double reference_error = distance(truth.state, check.state);
            if (reference_error > 2.0e-10)
                throw std::runtime_error{"Reference pair did not converge"};
            for (double tolerance : {1.0e-7, 1.0e-10}) {
                for (Method method : {Method::dopri5, Method::midpoint8, Method::rk4}) {
                    const double fixed_step = tolerance == 1.0e-7 ? 0.1 : 0.025;
                    static_cast<void>(integrate(parameters, initial.state, method, scenario.end,
                                                tolerance, fixed_step));
                    std::array<double, 7> timings{};
                    Run result{};
                    for (double &timing : timings) {
                        const auto start = std::chrono::steady_clock::now();
                        result = integrate(parameters, initial.state, method, scenario.end,
                                           tolerance, fixed_step);
                        timing = std::chrono::duration<double, std::milli>(
                                     std::chrono::steady_clock::now() - start)
                                     .count();
                    }
                    std::sort(timings.begin(), timings.end());
                    const double error = distance(result.state, check.state);
                    if (!std::isfinite(error) || error > 1.0e-3)
                        throw std::runtime_error{"Benchmark endpoint failed coarse sanity bound"};
                    output << scenario.label << ',' << name(method) << ','
                           << (method == Method::rk4 ? 0.0 : tolerance) << ','
                           << (method == Method::rk4 ? fixed_step : 0.0) << ',' << result.accepted
                           << ',' << result.rejected << ',' << result.evaluations << ','
                           << timings[3] << ',' << error << ',' << result.max_constraint << ','
                           << reference_error << '\n';
                }
            }
        }
        output.flush();
        if (!output)
            throw std::runtime_error{"Failed to write benchmark output"};
        if (file.is_open())
            std::cout << "Benchmark report: " << argv[2] << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
