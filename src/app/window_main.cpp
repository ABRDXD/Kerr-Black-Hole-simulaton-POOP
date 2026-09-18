#include "kerr/platform/Window.hpp"
#include "kerr/renderer/WindowRenderer.hpp"

#include <charconv>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
int integer(std::string_view text) {
    int value{};
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || value < 0)
        throw std::invalid_argument("Expected a nonnegative integer");
    return value;
}
std::vector<std::uint32_t> read_shader(const std::filesystem::path &path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Missing presentation shader: " + path.string());
    auto length = file.tellg();
    if (length < 20 || length > 16 * 1024 * 1024 || length % 4)
        throw std::runtime_error("Invalid shader file size");
    std::vector<std::uint32_t> result(static_cast<std::size_t>(length) / 4);
    file.seekg(0);
    if (!file.read(reinterpret_cast<char *>(result.data()), length))
        throw std::runtime_error("Shader read failed");
    return result;
}
} // namespace
int main(int argc, char **argv) {
    try {
        kerr::renderer::ComputeOptions options;
        bool smoke{};
        int width = 1001, height = 613, frames = 0;
        std::filesystem::path capture, shader_path = KERR_PRESENTATION_SHADER;
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg(argv[i]);
            if (arg == "--help") {
                std::cout << "KerrLab Vulkan presentation diagnostic (not a black-hole image)\n"
                             "--device N | --validation auto|on|off | --width N | --height N\n"
                             "--frames N (0=until closed) | --capture image.ppm | --shader PATH\n"
                             "--smoke: 24 frames, resize/minimize/restore and exact compute-image "
                             "captures\n"
                             "Escape closes the window. Minimized windows suspend GPU dispatch.\n";
                return 0;
            }
            if (arg == "--smoke") {
                smoke = true;
                continue;
            }
            if (++i == argc)
                throw std::invalid_argument("Missing value for " + std::string(arg));
            const std::string_view value(argv[i]);
            if (arg == "--device")
                options.device_index = integer(value);
            else if (arg == "--width")
                width = integer(value);
            else if (arg == "--height")
                height = integer(value);
            else if (arg == "--frames")
                frames = integer(value);
            else if (arg == "--capture")
                capture = value;
            else if (arg == "--shader")
                shader_path = value;
            else if (arg == "--validation") {
                if (value == "auto")
                    options.validation = kerr::renderer::ValidationMode::automatic;
                else if (value == "on")
                    options.validation = kerr::renderer::ValidationMode::required;
                else if (value == "off")
                    options.validation = kerr::renderer::ValidationMode::disabled;
                else
                    throw std::invalid_argument("--validation must be auto, on or off");
            } else
                throw std::invalid_argument("Unknown option: " + std::string(arg));
        }
        if (smoke) {
            frames = 24;
            if (capture.empty())
                capture = "out/window-smoke/final.ppm";
        }
        const auto shader = read_shader(shader_path);
        kerr::platform::Window window(width, height,
                                      "KerrLab | Vulkan presentation diagnostic | Esc to close");
        kerr::renderer::WindowRenderer renderer(window.surface_source(), options, shader);
        std::cout << "device=" << renderer.device().name
                  << " queue=" << renderer.device().compute_family << " validation="
                  << (renderer.validation_enabled() ? "enabled" : "disabled/unavailable")
                  << " presentation_fences=KHR_swapchain_maintenance1\n"
                  << std::flush;
        const auto start = std::chrono::steady_clock::now();
        auto minimized_at = start;
        bool resized{}, minimize_requested{}, minimize_observed{}, restored{}, second_resize{},
            initial_capture{}, resized_capture{};
        std::uint64_t title_frame = UINT64_MAX, frames_before_minimize{};
        while (!window.should_close()) {
            window.poll();
            auto statistics = renderer.statistics();
            const auto now = std::chrono::steady_clock::now();
            if (smoke && now - start > std::chrono::seconds(45))
                throw std::runtime_error(
                    "Window smoke test exceeded 45-second work deadline: presented=" +
                    std::to_string(statistics.presented) +
                    " acquire_retries=" + std::to_string(statistics.acquire_retries) +
                    " suspended=" + std::to_string(statistics.suspended) +
                    " recreations=" + std::to_string(statistics.recreations) +
                    " minimized=" + std::to_string(window.minimized()) +
                    " framebuffer=" + std::to_string(window.framebuffer_size()[0]) + "x" +
                    std::to_string(window.framebuffer_size()[1]) +
                    " validation_errors=" + std::to_string(statistics.validation_errors));
            if (smoke && statistics.presented >= 3 && !resized) {
                window.resize(817, 479);
                resized = true;
                window.poll();
            }
            if (smoke && resized && statistics.recreations >= 2 && !resized_capture) {
                // Capture only after drawing at least one frame into the replacement image.
                renderer.capture(capture.parent_path() / "resized.ppm");
                resized_capture = true;
            }
            if (smoke && statistics.presented >= 8 && !minimize_requested) {
                frames_before_minimize = statistics.presented;
                window.minimize();
                minimize_requested = true;
                minimized_at = now;
                window.poll();
            }
            if (smoke && minimize_requested && !restored) {
                if (window.minimized())
                    minimize_observed = true;
                if (now - minimized_at > std::chrono::milliseconds(250) && minimize_observed) {
                    if (renderer.statistics().presented != frames_before_minimize)
                        throw std::runtime_error("Frames were submitted while minimized");
                    window.restore();
                    window.resize(1023, 617);
                    restored = true;
                    second_resize = true;
                    window.poll();
                } else if (now - minimized_at > std::chrono::seconds(3))
                    throw std::runtime_error("Window did not enter minimized state");
            }
            const auto extent = window.framebuffer_size();
            const auto status = renderer.draw(extent[0], extent[1], 0);
            if (status != kerr::renderer::FrameStatus::presented) {
                window.wait(0.01);
                continue;
            }
            statistics = renderer.statistics();
            if (smoke)
                std::cout << "smoke_frame=" << statistics.presented << " elapsed_ms="
                          << std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - start)
                                 .count()
                          << '\n'
                          << std::flush;
            if (smoke && !initial_capture) {
                renderer.capture(capture.parent_path() / "initial.ppm");
                initial_capture = true;
            }
            if (statistics.presented / 30 != title_frame) {
                title_frame = statistics.presented / 30;
                window.title("KerrLab | Presentation diagnostic | " + renderer.device().name +
                             " | " + std::to_string(statistics.width) + " x " +
                             std::to_string(statistics.height) + " | Esc to close");
            }
            if (frames > 0 && statistics.presented >= static_cast<std::uint64_t>(frames))
                break;
        }
        if (!capture.empty())
            renderer.capture(capture);
        renderer.finish();
        const auto result = renderer.statistics();
        std::cout << "presented=" << result.presented << " recreations=" << result.recreations
                  << " suspended=" << result.suspended
                  << " acquire_retries=" << result.acquire_retries
                  << " final_extent=" << result.width << 'x' << result.height
                  << " validation_errors=" << result.validation_errors << '\n';
        if (result.validation_errors)
            return 1;
        if (smoke &&
            (!resized || !resized_capture || !minimize_observed || !restored || !second_resize ||
             result.recreations < 3 || !result.suspended || result.presented < 24))
            throw std::runtime_error(
                window.should_close()
                    ? "Window smoke cancelled: window closed before all 24 frames completed"
                    : "Window smoke lifecycle checks incomplete");
        std::cout << "PASS: Vulkan compute presentation"
                  << (smoke ? ", resize, minimize/restore, exact pixel readback" : "") << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "kerr_window: " << error.what() << '\n';
        return 1;
    }
}
