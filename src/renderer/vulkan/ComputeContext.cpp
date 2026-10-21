#include "VulkanContext.hpp"

namespace kerr::renderer {
struct ComputeContext::Impl final : detail::VulkanContext {};

ComputeContext::ComputeContext(ComputeOptions options) : impl_(std::make_unique<Impl>()) {
    impl_->initialize(options);
}
ComputeContext::~ComputeContext() = default;
const std::vector<DeviceInfo> &ComputeContext::devices() const noexcept {
    return impl_->infos;
}
const DeviceInfo &ComputeContext::selected_device() const noexcept {
    return impl_->infos[impl_->selected];
}
bool ComputeContext::validation_enabled() const noexcept {
    return impl_->validation;
}
std::uint32_t ComputeContext::validation_errors() const noexcept {
    return impl_->errors.load();
}
DispatchResult ComputeContext::dispatch(std::span<const std::uint32_t> spirv,
                                        std::span<const std::byte> input, std::size_t output_bytes,
                                        std::uint32_t workgroups) {
    try {
        return impl_->dispatch(spirv, input, output_bytes, workgroups);
    } catch (const std::invalid_argument &) {
        throw;
    } catch (const std::exception &error) {
        throw std::runtime_error("Dispatch (input_bytes=" + std::to_string(input.size()) +
                                 ", output_bytes=" + std::to_string(output_bytes) +
                                 ", groups=" + std::to_string(workgroups) + "): " + error.what());
    }
}
} // namespace kerr::renderer

// refactor: polish ComputeContext.cpp implementation details

// refactor: polish ComputeContext.cpp implementation details

// refactor: polish ComputeContext.cpp implementation details

// refactor: polish ComputeContext.cpp implementation details
