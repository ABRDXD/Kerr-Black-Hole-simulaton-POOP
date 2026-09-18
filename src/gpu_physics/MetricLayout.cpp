#include "kerr/gpu_physics/MetricLayout.hpp"

#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>

namespace kerr::gpu_physics {
namespace {
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(std::string("Metric shader ABI mismatch: ") + message);
}
struct Type {
    std::uint32_t opcode{};
    std::vector<std::uint32_t> args;
};
struct Reflection {
    std::map<std::uint32_t, Type> types;
    std::map<std::uint32_t, std::uint32_t> constants;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> decorations;
    std::map<std::pair<std::uint32_t, std::uint32_t>, std::uint32_t> offsets;
    std::map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> variables;
    std::array<std::uint32_t, 3> local{};
    bool local_ids{};
    std::uint32_t entry{}, execution_entry{}, entries{}, execution_modes{};
    std::uint32_t decoration(std::uint32_t id, std::uint32_t kind) const {
        auto it = decorations.find({id, kind});
        require(it != decorations.end(), "missing decoration");
        return it->second;
    }
    const Type &type(std::uint32_t id, std::uint32_t opcode) const {
        auto it = types.find(id);
        require(it != types.end() && it->second.opcode == opcode, "unexpected type");
        return it->second;
    }
    void scalar(std::uint32_t id, bool integer) const {
        auto &t = type(id, integer ? 21U : 22U);
        require(t.args.size() == (integer ? 2U : 1U) && t.args[0] == 32, "scalar width");
        if (integer)
            require(t.args[1] == 0, "unsigned integer required");
    }
    void vector(std::uint32_t id, bool integer) const {
        auto &t = type(id, 23);
        require(t.args.size() == 2 && t.args[1] == 4, "vec4 required");
        scalar(t.args[0], integer);
    }
    void array(std::uint32_t id, std::uint32_t count) const {
        auto &t = type(id, 28);
        require(t.args.size() == 2, "array type");
        scalar(t.args[0], false);
        require(constants.contains(t.args[1]) && constants.at(t.args[1]) == count, "array length");
        require(decoration(id, 6) == 4, "scalar array stride");
    }
    void record(std::uint32_t id, bool output) const {
        auto &t = type(id, 30);
        const std::vector<std::uint32_t> expected =
            output ? std::vector<std::uint32_t>{0, 16, 32, 96, 160}
                   : std::vector<std::uint32_t>{0, 16, 32};
        require(t.args.size() == expected.size(), "record member count");
        for (std::size_t i = 0; i < expected.size(); ++i) {
            auto key = std::pair{id, static_cast<std::uint32_t>(i)};
            require(offsets.contains(key) && offsets.at(key) == expected[i],
                    "record member offset");
        }
        if (output) {
            vector(t.args[0], true);
            vector(t.args[1], false);
            array(t.args[2], 16);
            array(t.args[3], 16);
            array(t.args[4], 48);
        } else {
            vector(t.args[0], false);
            vector(t.args[1], false);
            vector(t.args[2], true);
        }
    }
};
} // namespace

void validate_metric_shader(std::span<const std::uint32_t> words) {
    require(words.size() >= 5 && words[0] == 0x07230203U && words[4] == 0, "SPIR-V header");
    Reflection r;
    for (std::size_t p = 5; p < words.size();) {
        auto count = static_cast<std::size_t>(words[p] >> 16);
        auto opcode = words[p] & 0xffffU;
        require(count > 0 && count <= words.size() - p, "truncated instruction");
        auto args = words.subspan(p + 1, count - 1);
        if (opcode >= 21 && opcode <= 32 && opcode != 31) {
            require(args.size() >= 1, "type instruction");
            r.types[args[0]] = {opcode, {args.begin() + 1, args.end()}};
        } else if (opcode == 43 && args.size() == 3) { // OpConstant, 32-bit literals used by ABI
            r.constants[args[1]] = args[2];
        } else if (opcode == 59) { // OpVariable
            require(args.size() >= 3, "variable instruction");
            r.variables[args[1]] = {args[0], args[2]};
        } else if (opcode == 71) { // OpDecorate
            require(args.size() >= 2, "decoration instruction");
            r.decorations[{args[0], args[1]}] = args.size() > 2 ? args[2] : 0;
        } else if (opcode == 72 && args.size() >= 3 && args[2] == 35) {
            require(args.size() == 4, "offset instruction");
            r.offsets[{args[0], args[1]}] = args[3];
        } else if (opcode == 15) { // OpEntryPoint, GLCompute main
            require(args.size() >= 4 && args[0] == 5 && args[2] == 0x6e69616dU && args[3] == 0,
                    "compute main entry point");
            r.entry = args[1];
            ++r.entries;
        } else if ((opcode == 16 || opcode == 331) && args.size() >= 2 &&
                   (args[1] == 17 || args[1] == 38)) {
            require(args.size() == 5, "local size instruction");
            require((opcode == 16 && args[1] == 17) || (opcode == 331 && args[1] == 38),
                    "local size mode");
            r.local = {args[2], args[3], args[4]};
            r.local_ids = opcode == 331;
            r.execution_entry = args[0];
            ++r.execution_modes;
        }
        p += count;
    }
    require(r.entries == 1 && r.execution_modes == 1 && r.entry == r.execution_entry,
            "entry point execution mode");
    if (r.local_ids)
        for (auto &value : r.local) {
            require(r.constants.contains(value), "constant local size");
            value = r.constants.at(value);
        }
    require(r.local == std::array<std::uint32_t, 3>{metric_workgroup_size, 1, 1}, "workgroup size");
    std::array<bool, 2> bindings{};
    for (const auto &[id, variable] : r.variables) {
        const auto [type_id, storage] = variable;
        require(storage != 0 && storage != 2 && storage != 9,
                "unexpected descriptor or push constants");
        if (storage != 12)
            continue; // StorageBuffer
        require(r.decoration(id, 34) == 0, "descriptor set");
        auto binding = r.decoration(id, 33);
        require(binding < 2 && !bindings[binding], "descriptor binding");
        bindings[binding] = true;
        auto &pointer = r.type(type_id, 32);
        require(pointer.args.size() == 2 && pointer.args[0] == 12, "storage pointer");
        auto block_id = pointer.args[1];
        auto &block = r.type(block_id, 30);
        require(r.decorations.contains({block_id, 2}) && block.args.size() == 1, "storage block");
        require(r.offsets.contains({block_id, 0}) && r.offsets.at({block_id, 0}) == 0,
                "block offset");
        auto &runtime = r.type(block.args[0], 29);
        require(runtime.args.size() == 1, "runtime array");
        require(r.decoration(block.args[0], 6) ==
                    (binding == 0 ? sizeof(MetricInput) : sizeof(MetricOutput)),
                "record stride");
        r.record(runtime.args[0], binding == 1);
    }
    require(bindings[0] && bindings[1], "two storage descriptors required");
}

std::vector<MetricCase> metric_conformance_cases() {
    std::vector<MetricCase> cases;
    auto add = [&](std::string name, float mass, float spin, float x, float y, float z,
                   MetricStatus status = MetricStatus::valid) {
        MetricInput input;
        input.position_mass = {x, y, z, mass};
        input.spin_reserved[0] = spin;
        input.meta[1] = static_cast<std::uint32_t>(cases.size());
        cases.push_back({std::move(name), input, status});
    };
    for (double mass : {1e-6, 1.0, 1e6})
        for (double chi : {0.0, 0.8, -0.8, 0.999999, 1.0, -1.0}) {
            double horizon = 1.0 + std::sqrt(1.0 - chi * chi);
            const std::array<std::array<double, 3>, 11> points{
                {{10, 2, 3},
                 {0, 0, 2},
                 {0, 0, -2},
                 {1e-5, -1e-5, 2},
                 {5, -3, 0},
                 {std::sqrt(horizon * horizon + chi * chi), 0, 0},
                 {1.1, 0.2, 0.1},
                 {0.25, 0.1, 0.15},
                 {0.8, -0.7, -0.5},
                 {1e4, -2e4, 3e4},
                 {2, 1, -0.01}}};
            for (const auto &p : points)
                add("grid_" + std::to_string(cases.size()), static_cast<float>(mass),
                    static_cast<float>(chi * mass), static_cast<float>(p[0] * mass),
                    static_cast<float>(p[1] * mass), static_cast<float>(p[2] * mass));
        }
    std::uint32_t random_state = 0x4b455252U;
    auto random = [&]() {
        random_state ^= random_state << 13;
        random_state ^= random_state >> 17;
        random_state ^= random_state << 5;
        return static_cast<double>(random_state >> 8) / 16777216.0;
    };
    for (int i = 0; i < 513; ++i) {
        float spin = static_cast<float>(2 * random() - 1);
        float x = static_cast<float>(20 * random() - 10);
        float y = static_cast<float>(20 * random() - 10);
        float z = static_cast<float>(20 * random() - 10);
        add("sample_" + std::to_string(i), 1, spin, x, y, z);
    }
    add("origin", 1, 0, 0, 0, 0, MetricStatus::degenerate_chart);
    add("ring", 1, 1, 1, 0, 0, MetricStatus::degenerate_chart);
    add("disk_chart", 1, 1, 0.5F, 0, 0, MetricStatus::degenerate_chart);
    add("nan", 1, 0, std::numeric_limits<float>::quiet_NaN(), 0, 2, MetricStatus::invalid_input);
    add("infinity", 1, 0, 0, std::numeric_limits<float>::infinity(), 2,
        MetricStatus::invalid_input);
    add("zero_mass", 0, 0, 2, 0, 0, MetricStatus::invalid_input);
    add("negative_mass", -1, 0, 2, 0, 0, MetricStatus::invalid_input);
    add("overspin", 1, 2, 2, 0, 0, MetricStatus::invalid_input);
    add("bad_schema", 1, 0, 2, 0, 0, MetricStatus::unsupported_version);
    cases.back().input.meta[0] = 2;
    add("reserved_float", 1, 0, 2, 0, 0, MetricStatus::invalid_input);
    cases.back().input.spin_reserved[1] = 1;
    add("reserved_uint", 1, 0, 2, 0, 0, MetricStatus::invalid_input);
    cases.back().input.meta[2] = 1;
    add("overflow", std::numeric_limits<float>::max(), 0, 1e-10F, 0, 0,
        MetricStatus::numerical_failure);
    return cases;
}
} // namespace kerr::gpu_physics
