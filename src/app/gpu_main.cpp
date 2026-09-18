#include "kerr/gpu_physics/MetricLayout.hpp"
#include "kerr/physics/Geodesic.hpp"
#include "kerr/renderer/ComputeContext.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
std::vector<std::uint32_t> read_shader(const std::filesystem::path &path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream)
        throw std::runtime_error("Cannot open shader: " + path.string());
    auto size = stream.tellg();
    if (size < 20 || size > 16 * 1024 * 1024 || size % 4 != 0)
        throw std::runtime_error("Invalid SPIR-V file size");
    std::vector<std::uint32_t> words(static_cast<std::size_t>(size) / 4);
    stream.seekg(0);
    if (!stream.read(reinterpret_cast<char *>(words.data()), size))
        throw std::runtime_error("Cannot read shader");
    return words;
}
struct ErrorStats {
    std::size_t values{}, failures{};
    double max_absolute{}, max_fraction{};
    void compare(float actual_float, double expected, double absolute, double relative) {
        const double actual = static_cast<double>(actual_float);
        ++values;
        const double error = std::abs(actual - expected);
        const double fraction = error / (absolute + relative * std::abs(expected));
        if (!std::isfinite(actual) || !std::isfinite(expected) || !std::isfinite(fraction) ||
            fraction > 1)
            ++failures;
        max_absolute = std::max(max_absolute, error);
        max_fraction = std::max(max_fraction, fraction);
    }
};
} // namespace

int main(int argc, char **argv) {
    try {
        kerr::renderer::ComputeOptions options;
        bool probe{}, abi_test{};
        std::filesystem::path shader = KERR_METRIC_SHADER;
        std::filesystem::path csv;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg(argv[i]);
            if (arg == "--help") {
                std::cout
                    << "KerrLab headless Vulkan metric conformance v1\n"
                       "  --probe                     list capabilities and selected device\n"
                       "  --device N                  select enumeration index (default: "
                       "discrete)\n"
                       "  --validation auto|on|off    on requires Khronos validation layer\n"
                       "  --shader PATH               locally compiled metric_v1 SPIR-V\n"
                       "  --csv PATH                  write per-case numerical measurements\n"
                       "  --abi-test                  test reflection rejection without a GPU\n";
                return 0;
            }
            if (arg == "--probe") {
                probe = true;
                continue;
            }
            if (arg == "--abi-test") {
                abi_test = true;
                continue;
            }
            if (++i == argc)
                throw std::invalid_argument("Missing value for " + std::string(arg));
            std::string_view value(argv[i]);
            if (arg == "--device") {
                auto [end, error] = std::from_chars(value.data(), value.data() + value.size(),
                                                    options.device_index);
                if (error != std::errc{} || end != value.data() + value.size() ||
                    options.device_index < 0)
                    throw std::invalid_argument("--device requires a nonnegative integer");
            } else if (arg == "--validation") {
                if (value == "auto")
                    options.validation = kerr::renderer::ValidationMode::automatic;
                else if (value == "on")
                    options.validation = kerr::renderer::ValidationMode::required;
                else if (value == "off")
                    options.validation = kerr::renderer::ValidationMode::disabled;
                else
                    throw std::invalid_argument("--validation must be auto, on or off");
            } else if (arg == "--shader")
                shader = value;
            else if (arg == "--csv")
                csv = value;
            else
                throw std::invalid_argument("Unknown argument: " + std::string(arg));
        }
        std::vector<std::uint32_t> spirv;
        if (!probe || abi_test) {
            spirv = read_shader(shader);
            kerr::gpu_physics::validate_metric_shader(spirv);
        }
        if (abi_test) {
            std::size_t rejections{};
            auto reject = [&](const auto &mutated) {
                bool rejected = false;
                try {
                    kerr::gpu_physics::validate_metric_shader(mutated);
                } catch (const std::exception &) {
                    rejected = true;
                }
                if (!rejected)
                    throw std::runtime_error("ABI negative test accepted an incompatible shader");
                ++rejections;
            };
            auto truncated = spirv;
            truncated.resize(4);
            reject(truncated);
            auto corrupt = spirv;
            corrupt[5] = 0;
            reject(corrupt);
            std::vector<std::uint32_t> storage_arrays;
            for (std::size_t p = 5; p < spirv.size(); p += spirv[p] >> 16)
                if ((spirv[p] & 0xffffU) == 71 && (spirv[p] >> 16) == 4 && spirv[p + 2] == 6)
                    storage_arrays.push_back(spirv[p + 1]);
            for (std::size_t p = 5; p < spirv.size();) {
                auto count = spirv[p] >> 16, opcode = spirv[p] & 0xffffU;
                if ((opcode == 71 && count == 4 &&
                     (spirv[p + 2] == 6 || spirv[p + 2] == 33 || spirv[p + 2] == 34)) ||
                    (opcode == 72 && count == 5 && spirv[p + 3] == 35) ||
                    (opcode == 22 && count == 3) ||
                    (opcode == 21 && count == 4 && spirv[p + 3] == 0)) {
                    auto mutated = spirv;
                    mutated[p + count - 1] += 4;
                    reject(mutated);
                }
                if ((opcode == 28 && std::find(storage_arrays.begin(), storage_arrays.end(),
                                               spirv[p + 1]) != storage_arrays.end()) ||
                    opcode == 331 || (opcode == 16 && count == 6 && spirv[p + 2] == 17)) {
                    auto mutated = spirv;
                    mutated[p + count - 1] =
                        0; // Break array length or workgroup constant/reference.
                    reject(mutated);
                }
                p += count;
            }
            if (rejections < 10)
                throw std::runtime_error("Too few reflected layout decorations tested");
            std::cout << "PASS: shader ABI and " << rejections
                      << " incompatible/truncated shader rejection checks\n";
            return 0;
        }
        kerr::renderer::ComputeContext context(options);
        for (const auto &device : context.devices()) {
            auto version = device.api_version;
            std::cout << "device " << device.index << ": " << device.name << " | Vulkan "
                      << (version >> 22) << '.' << ((version >> 12) & 1023U) << '.'
                      << (version & 4095U) << " | driver_raw=" << device.driver_version
                      << " | vendor=" << device.vendor_id << " | FP64=" << device.float64
                      << " | compute_queue=" << device.compute_family
                      << " | timestamp_bits=" << device.timestamp_bits << " | "
                      << (device.suitable ? "suitable" : device.rejection) << '\n';
        }
        std::cout << "selected: " << context.selected_device().name << "\nvalidation: "
                  << (context.validation_enabled()
                          ? "enabled"
                          : "disabled/unavailable (no validation-clean claim)")
                  << '\n';
        if (probe)
            return context.validation_errors() ? 1 : 0;
        const auto cases = kerr::gpu_physics::metric_conformance_cases();
        std::vector<kerr::gpu_physics::MetricInput> inputs;
        for (const auto &fixture : cases)
            inputs.push_back(fixture.input);
        const auto dispatch =
            context.dispatch(spirv, std::as_bytes(std::span(inputs)),
                             cases.size() * sizeof(kerr::gpu_physics::MetricOutput),
                             static_cast<std::uint32_t>((cases.size() + 63) / 64));
        std::vector<kerr::gpu_physics::MetricOutput> outputs(cases.size());
        std::memcpy(outputs.data(), dispatch.output.data(), dispatch.output.size());
        // Test partial/exact workgroups, repeated resource lifetimes, and record ordering.
        for (std::size_t count : {1U, 63U, 64U, 65U}) {
            const auto subset =
                context.dispatch(spirv, std::as_bytes(std::span(inputs).first(count)),
                                 count * sizeof(kerr::gpu_physics::MetricOutput),
                                 static_cast<std::uint32_t>((count + 63) / 64));
            if (std::memcmp(subset.output.data(), outputs.data(), subset.output.size()) != 0)
                throw std::runtime_error("Repeated/partial workgroup output changed");
        }
        auto reversed_inputs = inputs;
        std::reverse(reversed_inputs.begin(), reversed_inputs.end());
        const auto reversed = context.dispatch(
            spirv, std::as_bytes(std::span(reversed_inputs)), outputs.size() * sizeof(outputs[0]),
            static_cast<std::uint32_t>((inputs.size() + 63) / 64));
        for (std::size_t i = 0; i < outputs.size(); ++i)
            if (std::memcmp(reversed.output.data() + i * sizeof(outputs[0]),
                            &outputs[outputs.size() - 1 - i], sizeof(outputs[0])) != 0)
                throw std::runtime_error("Reordered batch output mismatch");
        for (bool empty : {true, false}) {
            bool rejected = false;
            try {
                (void)context.dispatch(
                    spirv, empty ? std::span<const std::byte>{} : std::as_bytes(std::span(inputs)),
                    sizeof(outputs[0]), empty ? 1U : UINT32_MAX);
            } catch (const std::invalid_argument &) {
                rejected = true;
            }
            if (!rejected)
                throw std::runtime_error("Invalid dispatch was accepted");
        }
        std::ofstream report;
        if (!csv.empty()) {
            if (csv.has_parent_path())
                std::filesystem::create_directories(csv.parent_path());
            report.open(csv);
            if (!report)
                throw std::runtime_error("Cannot open CSV: " + csv.string());
            report << "id,name,status,expected_status,metric_max_abs,metric_max_tolerance_fraction,"
                      "partial_max_abs,partial_max_tolerance_fraction,failures\n";
            report << std::setprecision(12);
        }
        ErrorStats all_metric, all_partials;
        std::size_t failures{}, valid{};
        for (std::size_t i = 0; i < cases.size(); ++i) {
            const auto &fixture = cases[i];
            const auto &output = outputs[i];
            ErrorStats metric, partials;
            std::size_t case_failures{};
            if (output.meta[0] != 1 || output.meta[1] != i ||
                output.meta[2] != static_cast<std::uint32_t>(fixture.expected_status) ||
                output.meta[3] != 0)
                ++case_failures;
            if (fixture.expected_status == kerr::gpu_physics::MetricStatus::valid) {
                ++valid;
                const auto &p = fixture.input.position_mass;
                const kerr::physics::KerrParameters parameters(
                    static_cast<double>(p[3]), static_cast<double>(fixture.input.spin_reserved[0]));
                const kerr::math::Vector3d position{static_cast<double>(p[0]),
                                                    static_cast<double>(p[1]),
                                                    static_cast<double>(p[2])};
                auto reference = kerr::physics::evaluate_metric(parameters, position);
                auto derivatives = kerr::physics::evaluate_metric_derivatives(parameters, position);
                if (!reference.valid() || !derivatives.valid())
                    throw std::runtime_error("Invalid CPU fixture: " + fixture.name);
                metric.compare(output.scalars[0], reference.radius, 3e-5 * parameters.mass(), 3e-5);
                metric.compare(output.scalars[1], reference.profile, 3e-5, 3e-5);
                for (std::size_t j = 0; j < 16; ++j) {
                    metric.compare(output.covariant[j], reference.covariant.elements[j], 3e-5,
                                   3e-5);
                    metric.compare(output.contravariant[j], reference.contravariant.elements[j],
                                   3e-5, 3e-5);
                }
                for (std::size_t axis = 0; axis < 3; ++axis)
                    for (std::size_t j = 0; j < 16; ++j)
                        partials.compare(output.spatial_partials[axis * 16 + j],
                                         derivatives.spatial[axis].elements[j],
                                         3e-5 / parameters.mass(), 3e-4);
                for (std::size_t row = 0; row < 4; ++row)
                    for (std::size_t col = 0; col < 4; ++col) {
                        if (output.covariant[row * 4 + col] != output.covariant[col * 4 + row] ||
                            output.contravariant[row * 4 + col] !=
                                output.contravariant[col * 4 + row])
                            ++case_failures;
                        for (std::size_t axis = 0; axis < 3; ++axis)
                            if (output.spatial_partials[axis * 16 + row * 4 + col] !=
                                output.spatial_partials[axis * 16 + col * 4 + row])
                                ++case_failures;
                    }
            } else {
                for (float value : output.scalars)
                    if (value != 0)
                        ++case_failures;
                for (float value : output.covariant)
                    if (value != 0)
                        ++case_failures;
                for (float value : output.contravariant)
                    if (value != 0)
                        ++case_failures;
                for (float value : output.spatial_partials)
                    if (value != 0)
                        ++case_failures;
            }
            if (output.scalars[2] != 0 || output.scalars[3] != 0)
                ++case_failures;
            case_failures += metric.failures + partials.failures;
            failures += case_failures;
            if (case_failures)
                std::cerr << fixture.name << ": " << case_failures
                          << " failures; status=" << output.meta[2] << '\n';
            for (auto pair :
                 {std::pair{&all_metric, &metric}, std::pair{&all_partials, &partials}}) {
                pair.first->values += pair.second->values;
                pair.first->max_absolute =
                    std::max(pair.first->max_absolute, pair.second->max_absolute);
                pair.first->max_fraction =
                    std::max(pair.first->max_fraction, pair.second->max_fraction);
            }
            if (report)
                report << i << ',' << fixture.name << ',' << output.meta[2] << ','
                       << static_cast<std::uint32_t>(fixture.expected_status) << ','
                       << metric.max_absolute << ',' << metric.max_fraction << ','
                       << partials.max_absolute << ',' << partials.max_fraction << ','
                       << case_failures << '\n';
        }
        if (report.is_open()) {
            report.flush();
            if (!report)
                throw std::runtime_error("CSV write failed");
        }
        std::cout << std::setprecision(10) << "cases=" << cases.size() << " valid=" << valid
                  << " failures=" << failures
                  << " compared_values=" << all_metric.values + all_partials.values << '\n'
                  << "metric_max_abs=" << all_metric.max_absolute
                  << " max_tolerance_fraction=" << all_metric.max_fraction << '\n'
                  << "partial_max_abs=" << all_partials.max_absolute
                  << " max_tolerance_fraction=" << all_partials.max_fraction << '\n'
                  << "timestamp_available=" << dispatch.timestamp_available
                  << " dispatch_gpu_ms=" << dispatch.gpu_milliseconds
                  << " (single cold dispatch, not a throughput benchmark)\n"
                  << "validation_errors=" << context.validation_errors() << '\n';
        std::cout << "PASS: repeated batches (1/63/64/65 records), reordered batch, and dispatch "
                     "limit rejection\n";
        if (failures || context.validation_errors())
            return 1;
        std::cout << "PASS: GPU metric/derivative conformance v1\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "kerr_gpu: " << error.what() << '\n';
        return 1;
    }
}
