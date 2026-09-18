#include "kerr/app/ObserverSession.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
double number(const std::string &text) {
    std::size_t consumed{};
    const double value = std::stod(text, &consumed);
    if (consumed != text.size() || !std::isfinite(value))
        throw std::invalid_argument{"Expected a finite number: " + text};
    return value;
}
void help() {
    std::cout
        << "KerrLab synchronized CPU observer session\n"
           "Usage: kerr_session [--spin CHI] [--radius R] [--time T] [--interval DT] [--csv PATH]\n"
           "M=1, G=c=1; all observers begin at KS t=0, proper time=0.\n"
           "Prograde and retrograde circular geodesics, a stationary observer with Fermi-Walker\n"
           "axes, and a freely falling Eulerian initial observer advance to shared KS t slices.\n"
           "The freefall example starts with the slice-normal velocity, not rest at infinity.\n"
           "Capture freezes that observer while the others continue. Clock comparisons use\n"
           "coordinate simultaneity; no signal exchange is simulated.\n"
           "CSV exports each observer's latest 4096 accepted samples and all transported axes.\n";
}
} // namespace

int main(int argc, char **argv) {
    try {
        using namespace kerr::physics;
        using namespace kerr::app;
        double spin = 0.8, radius = 10.0, duration = 100.0, interval = 5.0;
        std::string csv_path;
        for (int i = 1; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--help" || argument == "-h") {
                help();
                return 0;
            }
            if (i + 1 >= argc)
                throw std::invalid_argument{"Missing value for " + argument};
            const std::string value = argv[++i];
            if (argument == "--spin")
                spin = number(value);
            else if (argument == "--radius")
                radius = number(value);
            else if (argument == "--time")
                duration = number(value);
            else if (argument == "--interval")
                interval = number(value);
            else if (argument == "--csv")
                csv_path = value;
            else
                throw std::invalid_argument{"Unknown option: " + argument};
        }
        if (radius <= 0.0 || duration <= 0.0 || interval <= 0.0 || duration / interval > 100000.0)
            throw std::invalid_argument{
                "Positive radius/time/interval and at most 100000 output intervals are required"};
        const KerrParameters parameters{1.0, spin};
        const kerr::math::Vector4d position{0.0, std::hypot(radius, spin), 0.0, 0.0};
        SessionOptions options{};
        options.history_capacity = 4096;
        options.geodesic.stop_at_escape = false;
        ObserverSession session{parameters, 0.0, options};
        const auto add = [&](const std::string &label, const ObserverEvaluation &event,
                             ObserverMotion motion) {
            if (!event.valid())
                throw std::invalid_argument{"No physical initial observer: " + label};
            static_cast<void>(session.add(label, event.observer, motion));
        };
        add("prograde", make_circular_observer(parameters, position, OrbitSense::prograde),
            ObserverMotion::geodesic);
        add("retrograde", make_circular_observer(parameters, position, OrbitSense::retrograde),
            ObserverMotion::geodesic);
        add("stationary", make_stationary_observer(parameters, position),
            ObserverMotion::stationary);
        add("freefall", make_eulerian_observer(parameters, position), ObserverMotion::geodesic);
        SessionSnapshot result = session.snapshot();
        while (result.coordinate_time < duration) {
            const double next = std::min(duration, result.coordinate_time + interval);
            if (!(next > result.coordinate_time))
                throw std::invalid_argument{
                    "Coordinate interval cannot advance floating-point time"};
            result = session.advance_to(next);
        }
        bool success = true;
        std::cout << std::setprecision(12)
                  << "Session KS coordinate slice: " << result.coordinate_time
                  << "  spin a/M: " << spin << '\n';
        for (const auto &observer : result.observers) {
            const auto &sample = observer.latest;
            std::cout << observer.label << ": " << observer.termination
                      << "  tau=" << sample.event.proper_time << "  t=" << sample.event.position[0]
                      << "  r=" << sample.diagnostics.radius
                      << "  accepted/rejected=" << observer.accepted_steps << '/'
                      << observer.rejected_steps
                      << "  discarded old samples=" << observer.samples_dropped << '\n';
            success = success && observer.lifecycle != ObserverLifecycle::failed;
        }
        if (!csv_path.empty()) {
            const std::filesystem::path path{csv_path};
            if (path.has_parent_path())
                std::filesystem::create_directories(path.parent_path());
            std::ofstream file{path};
            if (!file)
                throw std::runtime_error{"Could not open CSV"};
            file << "generation,id,observer,tau,t,x,y,z,r,H,normalized_constraint,E,Lz,Q";
            for (std::size_t leg = 0; leg < 4; ++leg)
                for (std::size_t mu = 0; mu < 4; ++mu)
                    file << ",e" << leg << '_' << mu;
            file << '\n' << std::setprecision(17);
            for (const auto &observer : result.observers)
                for (const auto &sample : session.history(observer.id)) {
                    file << observer.id.generation << ',' << observer.id.serial << ','
                         << observer.label << ',' << sample.event.proper_time;
                    for (double value : sample.event.position)
                        file << ',' << value;
                    const auto &d = sample.diagnostics;
                    file << ',' << d.radius << ',' << d.hamiltonian << ','
                         << d.normalized_constraint << ',' << d.energy << ',' << d.angular_momentum
                         << ',' << d.carter;
                    for (const auto &leg : sample.event.tetrad)
                        for (double value : leg)
                            file << ',' << value;
                    file << '\n';
                }
            file.flush();
            if (!file)
                throw std::runtime_error{"Failed to write CSV"};
            std::cout << "Observer histories: " << csv_path << '\n';
        }
        return success ? 0 : 2;
    } catch (const std::exception &error) {
        std::cerr << "KerrLab: " << error.what() << '\n';
        return 1;
    }
}

// refactor: polish session_main.cpp implementation details
