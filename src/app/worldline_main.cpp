#include "kerr/physics/Observer.hpp"
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
        << "KerrLab CPU timelike worldline tracer\n"
           "Usage: kerr_worldline [--scenario orbit|infall|pair] [--spin CHI]\n"
           "                      [--radius R] [--proper-time TAU] [--retrograde]\n"
           "                      [--tolerance RTOL] [--csv PATH]\n"
           "Units M=1, G=c=1. Clocks start at tau=t=0 in ingoing Kerr-Schild coordinates.\n"
           "Orbit: equatorial circular geodesic; unstable circular orbits are allowed.\n"
           "Infall: Schwarzschild radial fall from rest at infinity (spin fixed to zero).\n"
           "Pair: independent prograde/retrograde worldlines at the same initial radius,\n"
           "      each stopped at its own proper-time limit. No signal comparison is implied.\n"
           "CSV contains accepted states and final event, without transported orientation.\n";
}
} // namespace

int main(int argc, char **argv) {
    try {
        std::string scenario = "orbit", csv_path;
        double spin = 0.8, radius = 10.0;
        bool retrograde{};
        WorldlineOptions options{};
        options.proper_time_limit = 100.0;
        options.stop_at_escape = false;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--help" || argument == "-h") {
                help();
                return 0;
            }
            if (argument == "--retrograde") {
                retrograde = true;
                continue;
            }
            if (i + 1 >= argc)
                throw std::invalid_argument{"Missing value for " + argument};
            const std::string value = argv[++i];
            if (argument == "--scenario")
                scenario = value;
            else if (argument == "--spin")
                spin = number(value);
            else if (argument == "--radius")
                radius = number(value);
            else if (argument == "--proper-time")
                options.proper_time_limit = number(value);
            else if (argument == "--tolerance")
                options.relative_tolerance = number(value);
            else if (argument == "--csv")
                csv_path = value;
            else
                throw std::invalid_argument{"Unknown option: " + argument};
        }
        if (scenario != "orbit" && scenario != "infall" && scenario != "pair")
            throw std::invalid_argument{"Unknown scenario: " + scenario};
        if (radius <= 0.0)
            throw std::invalid_argument{"Radius must be positive"};
        if (std::abs(spin) > 1.0)
            throw std::invalid_argument{"Spin must be in [-1,1]"};
        const KerrParameters parameters{1.0, scenario == "infall" ? 0.0 : spin};
        if (radius <= horizon_radii(parameters).outer)
            throw std::invalid_argument{"These examples start outside the outer horizon"};
        std::ofstream csv;
        if (!csv_path.empty()) {
            const std::filesystem::path path{csv_path};
            if (path.has_parent_path())
                std::filesystem::create_directories(path.parent_path());
            csv.open(path);
            if (!csv)
                throw std::runtime_error{"Could not open CSV: " + csv_path};
            csv << "observer,tau,t,x,y,z,p_t,p_x,p_y,p_z,r,H,normalized_constraint,E,Lz,Q,dt_dtau\n"
                << std::setprecision(17);
        }
        bool success = true;
        const int count = scenario == "pair" ? 2 : 1;
        for (int i = 0; i < count; ++i) {
            const auto sense = (scenario == "pair" ? i == 1 : retrograde) ? OrbitSense::retrograde
                                                                          : OrbitSense::prograde;
            const std::string label =
                scenario == "infall" ? "infall"
                                     : (sense == OrbitSense::prograde ? "prograde" : "retrograde");
            const double x = std::hypot(radius, parameters.spin());
            ObserverEvaluation observer{};
            if (scenario == "infall") {
                const double w = std::sqrt(2.0 / radius);
                observer = make_observer(parameters, {0.0, radius, 0.0, 0.0},
                                         {1.0 + w * w / (1.0 + w), -w, 0.0, 0.0});
            } else
                observer = make_circular_observer(parameters, {0.0, x, 0.0, 0.0}, sense);
            if (!observer.valid())
                throw std::invalid_argument{"No physical observer for the requested orbit"};
            const auto &event = observer.observer;
            const auto initial = make_timelike_state(
                parameters, {event.position[1], event.position[2], event.position[3]},
                event.tetrad[0], event.position[0]);
            if (!initial.valid())
                throw std::runtime_error{"Could not construct timelike initial data"};
            const auto result = trace_timelike_worldline(
                parameters, initial.state, options,
                [&](double tau, const GeodesicState &state,
                    const GeodesicDiagnostics &diagnostics) {
                    if (csv.is_open()) {
                        csv << label << ',' << tau;
                        for (double component : state)
                            csv << ',' << component;
                        csv << ',' << diagnostics.radius << ',' << diagnostics.hamiltonian << ','
                            << diagnostics.normalized_constraint << ',' << diagnostics.energy << ','
                            << diagnostics.angular_momentum << ',' << diagnostics.carter << ','
                            << diagnostics.coordinate_time_rate << '\n';
                        if (!csv)
                            throw std::runtime_error{"Failed while writing CSV"};
                    }
                    return true;
                });
            std::cout << std::setprecision(12) << "Observer: " << label
                      << "  spin a/M: " << parameters.dimensionless_spin()
                      << "\nTermination: " << termination_name(result.termination)
                      << "\nProper time: " << result.proper_time
                      << "  coordinate time: " << result.state[0]
                      << "  final r/M: " << result.diagnostics.radius
                      << "\nAccepted/rejected steps: " << result.statistics.accepted_steps << '/'
                      << result.statistics.rejected_steps
                      << "\nMaximum normalized timelike constraint: "
                      << result.statistics.maximum_constraint
                      << "\nMaximum scaled drift E/Lz/Q: " << result.statistics.maximum_energy_drift
                      << " / " << result.statistics.maximum_lz_drift << " / "
                      << result.statistics.maximum_carter_drift << '\n';
            success = success && (result.termination == RayTermination::proper_time_limit ||
                                  result.termination == RayTermination::horizon);
        }
        if (csv.is_open()) {
            csv.flush();
            if (!csv)
                throw std::runtime_error{"Failed to flush CSV"};
            std::cout << "Trajectories: " << csv_path << '\n';
        }
        return success ? 0 : 2;
    } catch (const std::exception &error) {
        std::cerr << "KerrLab: " << error.what() << '\n';
        return 1;
    }
}
