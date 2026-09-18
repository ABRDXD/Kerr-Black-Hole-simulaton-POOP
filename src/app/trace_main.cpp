#include "kerr/physics/RayTracer.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
using namespace kerr::physics;
double number(const std::string &text) {
    std::size_t consumed{};
    const double value = std::stod(text, &consumed);
    if (consumed != text.size() || !std::isfinite(value))
        throw std::invalid_argument{"Expected a finite number: " + text};
    return value;
}
void help() {
    std::cout
        << "KerrLab CPU null-geodesic tracer\n"
           "Usage: kerr_trace [--scenario "
           "capture|escape|critical-capture|critical-escape|kerr|disk]\n"
           "                  [--spin CHI] [--tolerance RTOL] [--affine-limit LAMBDA] [--csv "
           "PATH]\n"
           "Units: M=1, G=c=1. Future-directed rays in ingoing Cartesian Kerr-Schild.\n"
           "CSV records accepted states and the located final event; no renderer is included.\n";
}
} // namespace

int main(int argc, char **argv) {
    try {
        std::string scenario = "kerr", csv_path;
        double spin = 0.8;
        RayOptions options{};
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--help" || argument == "-h") {
                help();
                return 0;
            }
            if (i + 1 >= argc)
                throw std::invalid_argument{"Missing value for " + argument};
            const std::string value = argv[++i];
            if (argument == "--scenario")
                scenario = value;
            else if (argument == "--spin")
                spin = number(value);
            else if (argument == "--csv")
                csv_path = value;
            else if (argument == "--tolerance")
                options.relative_tolerance = number(value);
            else if (argument == "--affine-limit")
                options.affine_limit = number(value);
            else
                throw std::invalid_argument{"Unknown option: " + argument};
        }
        const bool schwarzschild = scenario == "capture" || scenario == "escape" ||
                                   scenario == "critical-capture" || scenario == "critical-escape";
        const KerrParameters parameters{1.0, schwarzschild ? 0.0 : spin};
        GeodesicState initial{};
        if (schwarzschild) {
            double impact = scenario == "capture" ? 4.0 : 6.0;
            if (scenario == "critical-capture")
                impact = std::sqrt(27.0) - 1.0e-6;
            if (scenario == "critical-escape")
                impact = std::sqrt(27.0) + 1.0e-6;
            constexpr double radius = 30.0, f = 2.0 / radius;
            initial = {0.0,
                       radius,
                       0.0,
                       0.0,
                       -1.0,
                       (-std::sqrt(1.0 - (1.0 - f) * impact * impact / (radius * radius)) + f) /
                           (1.0 - f),
                       impact / radius,
                       0.0};
            options.escape_radius = 35.0;
        } else {
            NullStateEvaluation generated{};
            if (scenario == "disk") {
                generated = make_null_state(parameters, {12.0, 0.0, 2.0}, {0.0, 0.2, -1.0});
                options.stop_at_disk = true;
            } else if (scenario == "kerr") {
                generated = make_null_state(parameters, {10.0, 1.0, 3.0}, {-0.8, 0.4, -0.1});
                options.escape_radius = 35.0;
            } else
                throw std::invalid_argument{"Unknown scenario: " + scenario};
            if (!generated.valid())
                throw std::runtime_error{"Could not construct null initial data"};
            initial = generated.state;
        }
        std::ofstream csv;
        if (!csv_path.empty()) {
            const std::filesystem::path path{csv_path};
            if (path.has_parent_path())
                std::filesystem::create_directories(path.parent_path());
            csv.open(path);
            if (!csv)
                throw std::runtime_error{"Could not open CSV: " + csv_path};
            csv << "lambda,t,x,y,z,p_t,p_x,p_y,p_z,r,H,normalized_constraint,E,Lz,Q\n"
                << std::setprecision(17);
        }
        const auto result = trace_null_ray(
            parameters, initial, options,
            [&](double affine, const GeodesicState &state, const GeodesicDiagnostics &diagnostics) {
                if (csv.is_open()) {
                    csv << affine;
                    for (double component : state)
                        csv << ',' << component;
                    csv << ',' << diagnostics.radius << ',' << diagnostics.hamiltonian << ','
                        << diagnostics.normalized_constraint << ',' << diagnostics.energy << ','
                        << diagnostics.angular_momentum << ',' << diagnostics.carter << '\n';
                    if (!csv)
                        throw std::runtime_error{"Failed while writing CSV"};
                }
                return true;
            });
        if (csv.is_open()) {
            csv.flush();
            if (!csv)
                throw std::runtime_error{"Failed to flush CSV"};
        }
        std::cout << std::setprecision(12) << "Scenario: " << scenario
                  << "  spin a/M: " << parameters.dimensionless_spin()
                  << "\nTermination: " << termination_name(result.termination)
                  << "\nAffine parameter: " << result.affine_parameter
                  << "  final r/M: " << result.diagnostics.radius
                  << "\nAccepted/rejected steps: " << result.statistics.accepted_steps << '/'
                  << result.statistics.rejected_steps
                  << "  derivative evaluations: " << result.statistics.rhs_evaluations
                  << "\nMaximum normalized null constraint: "
                  << result.statistics.maximum_constraint
                  << "\nMaximum scaled drift E/Lz/Q: " << result.statistics.maximum_energy_drift
                  << " / " << result.statistics.maximum_lz_drift << " / "
                  << result.statistics.maximum_carter_drift << '\n';
        if (!csv_path.empty())
            std::cout << "Trajectory: " << csv_path << '\n';
        switch (result.termination) {
        case RayTermination::horizon:
        case RayTermination::escape:
        case RayTermination::disk:
        case RayTermination::affine_limit:
            return 0;
        default:
            return 2;
        }
    } catch (const std::exception &error) {
        std::cerr << "KerrLab: " << error.what() << '\n';
        return 1;
    }
}

// refactor: polish trace_main.cpp implementation details
