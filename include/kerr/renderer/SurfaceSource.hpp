#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace kerr::renderer {
// Platform-owned window must outlive the renderer. Opaque Vulkan handle bits keep
// platform and Vulkan headers out of the application/physics public interfaces.
struct SurfaceSource final {
    std::vector<std::string> instance_extensions;
    std::function<std::uint64_t(std::uintptr_t)> create;
    // Called on the window-owning thread between bounded GPU waits.
    std::function<void()> poll_events;
};
} // namespace kerr::renderer
