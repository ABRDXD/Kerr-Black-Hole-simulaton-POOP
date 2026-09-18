#pragma once

#include "kerr/renderer/ComputeContext.hpp"
#include "kerr/renderer/SurfaceSource.hpp"
#include <array>
#include <filesystem>

namespace kerr::renderer {
enum class FrameStatus { presented, suspended, retry };
struct WindowStatistics final {
    std::uint64_t presented{}, recreations{}, suspended{}, acquire_retries{};
    std::uint32_t width{}, height{}, validation_errors{};
};
class WindowRenderer final {
  public:
    WindowRenderer(const SurfaceSource &source, ComputeOptions options,
                   std::span<const std::uint32_t> shader);
    ~WindowRenderer();
    WindowRenderer(const WindowRenderer &) = delete;
    WindowRenderer &operator=(const WindowRenderer &) = delete;
    [[nodiscard]] FrameStatus draw(std::uint32_t width, std::uint32_t height,
                                   std::uint32_t phase = 0);
    // Explicit diagnostic capture only: synchronous readback of the compute image.
    // Writes a binary PPM and checks every pixel against the integer CPU oracle.
    void capture(const std::filesystem::path &path);
    void finish();
    [[nodiscard]] WindowStatistics statistics() const;
    [[nodiscard]] const DeviceInfo &device() const;
    [[nodiscard]] bool validation_enabled() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace kerr::renderer
