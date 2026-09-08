#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace kerr::renderer {

// Application work budget, independent of permissive driver limits.
inline constexpr std::uint32_t max_headless_workgroups = 16'384;

enum class ValidationMode { automatic, required, disabled };
struct ComputeOptions final {
    int device_index{-1}; // -1 prefers a suitable discrete GPU.
    ValidationMode validation{ValidationMode::automatic};
};
struct DeviceInfo final {
    std::uint32_t index{}, api_version{}, driver_version{}, vendor_id{}, device_id{};
    std::uint32_t compute_family{}, timestamp_bits{};
    bool discrete{}, float64{}, suitable{};
    std::string name, rejection;
};
struct DispatchResult final {
    std::vector<std::byte> output;
    double gpu_milliseconds{};
    bool timestamp_available{};
};

// Synchronous headless validation foundation. No physics dependency or native handles in this API.
class ComputeContext final {
  public:
    explicit ComputeContext(ComputeOptions options = {});
    ~ComputeContext();
    ComputeContext(const ComputeContext &) = delete;
    ComputeContext &operator=(const ComputeContext &) = delete;
    [[nodiscard]] const std::vector<DeviceInfo> &devices() const noexcept;
    [[nodiscard]] const DeviceInfo &selected_device() const noexcept;
    [[nodiscard]] bool validation_enabled() const noexcept;
    [[nodiscard]] std::uint32_t validation_errors() const noexcept;
    [[nodiscard]] DispatchResult dispatch(std::span<const std::uint32_t> spirv,
                                          std::span<const std::byte> input,
                                          std::size_t output_bytes, std::uint32_t workgroups);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace kerr::renderer
