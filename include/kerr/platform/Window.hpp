#pragma once

#include "kerr/renderer/SurfaceSource.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace kerr::platform {
// Main-thread, single-window platform adapter. No GLFW/native types escape.
class Window final {
  public:
    Window(int width, int height, const std::string &title);
    ~Window();
    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    [[nodiscard]] renderer::SurfaceSource surface_source();
    [[nodiscard]] std::array<std::uint32_t, 2> framebuffer_size() const;
    [[nodiscard]] bool should_close() const;
    [[nodiscard]] bool minimized() const;
    void poll();
    void wait(double seconds);
    void title(const std::string &text);
    void resize(int width, int height);
    void minimize();
    void restore();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace kerr::platform
