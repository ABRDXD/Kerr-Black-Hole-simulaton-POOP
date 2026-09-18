#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace kerr::gpu_physics {

inline constexpr std::uint32_t metric_schema_version = 1;
inline constexpr std::uint32_t metric_workgroup_size = 64;

enum class MetricStatus : std::uint32_t {
    valid = 0,
    invalid_input = 1,
    degenerate_chart = 2,
    unsupported_version = 3,
    numerical_failure = 4,
};

// std430, all matrices row-major scalar arrays. See GPU_PHYSICS.md.
struct alignas(16) MetricInput final {
    std::array<float, 4> position_mass{}; // x,y,z,M
    std::array<float, 4> spin_reserved{}; // a,0,0,0
    std::array<std::uint32_t, 4> meta{metric_schema_version, 0, 0, 0};
};

struct alignas(16) MetricOutput final {
    std::array<std::uint32_t, 4> meta{}; // version, id, status, reserved
    std::array<float, 4> scalars{};      // r,F,0,0
    std::array<float, 16> covariant{};
    std::array<float, 16> contravariant{};
    std::array<float, 48> spatial_partials{}; // axis*16 + row*4 + column
};

static_assert(sizeof(float) == 4 && sizeof(std::uint32_t) == 4);
static_assert(std::numeric_limits<float>::is_iec559);
static_assert(std::is_trivially_copyable_v<MetricInput> && std::is_standard_layout_v<MetricInput>);
static_assert(std::is_trivially_copyable_v<MetricOutput> &&
              std::is_standard_layout_v<MetricOutput>);
static_assert(sizeof(MetricInput) == 48 && alignof(MetricInput) == 16);
static_assert(offsetof(MetricInput, spin_reserved) == 16 && offsetof(MetricInput, meta) == 32);
static_assert(sizeof(MetricOutput) == 352 && alignof(MetricOutput) == 16);
static_assert(offsetof(MetricOutput, scalars) == 16);
static_assert(offsetof(MetricOutput, covariant) == 32);
static_assert(offsetof(MetricOutput, contravariant) == 96);
static_assert(offsetof(MetricOutput, spatial_partials) == 160);

// Narrow ABI reflection for our trusted, locally compiled shader; not a general SPIR-V validator.
// Throws on descriptor, scalar type, offset, stride, array length, or workgroup mismatch.
void validate_metric_shader(std::span<const std::uint32_t> words);

struct MetricCase final {
    std::string name;
    MetricInput input;
    MetricStatus expected_status{MetricStatus::valid};
};
[[nodiscard]] std::vector<MetricCase> metric_conformance_cases();

} // namespace kerr::gpu_physics
