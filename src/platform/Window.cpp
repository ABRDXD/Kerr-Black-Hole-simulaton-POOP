#include "kerr/platform/Window.hpp"

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace kerr::platform {
namespace {
bool active_window{}; // GLFW is process-global; this adapter deliberately supports one owner.
std::string glfw_error(const char *operation) {
    const char *description{};
    const int code = glfwGetError(&description);
    return std::string(operation) + ": GLFW " + std::to_string(code) + " " +
           (description ? description : "");
}
} // namespace
struct Window::Impl {
    GLFWwindow *window{};
    bool initialized{};
    ~Impl() {
        if (window)
            glfwDestroyWindow(window);
        if (initialized) {
            glfwTerminate();
            active_window = false;
        }
    }
};
Window::Window(int width, int height, const std::string &title) : impl_(std::make_unique<Impl>()) {
    if (width < 1 || height < 1 || width > 8192 || height > 8192)
        throw std::invalid_argument("Window dimensions must be in [1,8192]");
    if (active_window)
        throw std::runtime_error("Only one GLFW window owner is supported");
    if (!glfwInit())
        throw std::runtime_error(glfw_error("glfwInit"));
    impl_->initialized = true;
    active_window = true;
    if (!glfwVulkanSupported())
        throw std::runtime_error("GLFW cannot find a Vulkan loader/driver");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    impl_->window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!impl_->window)
        throw std::runtime_error(glfw_error("glfwCreateWindow"));
    glfwSetKeyCallback(impl_->window, [](GLFWwindow *window, int key, int, int action, int) {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);
    });
}
Window::~Window() = default;
renderer::SurfaceSource Window::surface_source() {
    std::uint32_t count{};
    const auto extensions = glfwGetRequiredInstanceExtensions(&count);
    if (!extensions || !count)
        throw std::runtime_error(glfw_error("glfwGetRequiredInstanceExtensions"));
    renderer::SurfaceSource source;
    source.poll_events = [] { glfwPollEvents(); };
    for (std::uint32_t i = 0; i < count; ++i)
        source.instance_extensions.emplace_back(extensions[i]);
    source.create = [this](std::uintptr_t bits) {
        VkSurfaceKHR surface{};
        auto result = glfwCreateWindowSurface(reinterpret_cast<VkInstance>(bits), impl_->window,
                                              nullptr, &surface);
        if (result != VK_SUCCESS)
            throw std::runtime_error("glfwCreateWindowSurface failed (VkResult " +
                                     std::to_string(result) + ")");
        std::uint64_t handle{};
        static_assert(sizeof(surface) <= sizeof(handle));
        std::memcpy(&handle, &surface, sizeof(surface));
        return handle;
    };
    return source;
}
std::array<std::uint32_t, 2> Window::framebuffer_size() const {
    if (minimized())
        return {0, 0};
    int width{}, height{};
    glfwGetFramebufferSize(impl_->window, &width, &height);
    return {static_cast<std::uint32_t>(std::max(width, 0)),
            static_cast<std::uint32_t>(std::max(height, 0))};
}
bool Window::should_close() const {
    return glfwWindowShouldClose(impl_->window) != 0;
}
bool Window::minimized() const {
    return glfwGetWindowAttrib(impl_->window, GLFW_ICONIFIED) != 0;
}
void Window::poll() {
    glfwPollEvents();
}
void Window::wait(double seconds) {
    glfwWaitEventsTimeout(seconds);
}
void Window::title(const std::string &text) {
    glfwSetWindowTitle(impl_->window, text.c_str());
}
void Window::resize(int width, int height) {
    glfwSetWindowSize(impl_->window, width, height);
}
void Window::minimize() {
    glfwIconifyWindow(impl_->window);
}
void Window::restore() {
    glfwRestoreWindow(impl_->window);
}
} // namespace kerr::platform
