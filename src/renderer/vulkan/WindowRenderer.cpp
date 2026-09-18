#include "kerr/renderer/WindowRenderer.hpp"
#include "VulkanContext.hpp"

#include <chrono>
#include <fstream>

namespace kerr::renderer {
using detail::check;
using detail::enumerate;
using detail::structure;

struct WindowRenderer::Impl {
    std::unique_ptr<detail::VulkanContext> context{std::make_unique<detail::VulkanContext>()};
    WindowStatistics stats{};
    VkDescriptorSetLayout descriptor_layout{};
    VkPipelineLayout pipeline_layout{};
    VkShaderModule shader{};
    VkPipeline pipeline{};
    VkCommandPool command_pool{};
    VkCommandBuffer command{};
    VkFence render_fence{}, present_fence{};
    VkSemaphore acquired{}, rendered{};
    bool acquire_pending{}, render_pending{}, present_pending{}, dirty{true}, finished{};
    std::uint32_t last_phase{};
    std::function<void()> poll_events;
    // clang-format off
#define WINDOW_FUNCTIONS(X) \
    X(CreateSwapchainKHR) X(DestroySwapchainKHR) X(GetSwapchainImagesKHR) \
    X(AcquireNextImageKHR) X(QueuePresentKHR) X(CreateSemaphore) X(DestroySemaphore) \
    X(ResetFences) X(ResetCommandPool) X(CreateImage) X(DestroyImage) \
    X(GetImageMemoryRequirements) X(BindImageMemory) X(CreateImageView) X(DestroyImageView) \
    X(CmdPushConstants) X(CmdBlitImage) X(CmdCopyImageToBuffer)
#define DECLARE(name) PFN_vk##name name{};
    WINDOW_FUNCTIONS(DECLARE)
#undef DECLARE
    // clang-format on
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR surface_capabilities{};
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR surface_formats{};
    PFN_vkGetPhysicalDeviceFormatProperties format_properties{};

    struct Swapchain final {
        Impl &owner;
        VkSwapchainKHR handle{};
        std::vector<VkImage> images;
        VkExtent2D extent{};
        VkFormat format{};
        VkImage compute_image{};
        VkDeviceMemory memory{};
        VkImageView view{};
        VkDescriptorPool descriptors{};
        VkDescriptorSet descriptor{};
        bool image_initialized{};
        explicit Swapchain(Impl &o) : owner(o) {}
        ~Swapchain() {
            auto &c = *owner.context;
            if (descriptors)
                c.DestroyDescriptorPool(c.device, descriptors, nullptr);
            if (view)
                owner.DestroyImageView(c.device, view, nullptr);
            if (compute_image)
                owner.DestroyImage(c.device, compute_image, nullptr);
            if (memory)
                c.FreeMemory(c.device, memory, nullptr);
            if (handle)
                owner.DestroySwapchainKHR(c.device, handle, nullptr);
        }
    };
    std::unique_ptr<Swapchain> swapchain;

    template <class T> T device_function(const char *name) {
        auto result = reinterpret_cast<T>(context->get_device(context->device, name));
        if (!result)
            throw std::runtime_error(std::string("Missing presentation function: ") + name);
        return result;
    }
    void initialize(const SurfaceSource &source, ComputeOptions options,
                    std::span<const std::uint32_t> spirv) {
        if (spirv.size() < 5 || spirv[0] != 0x07230203U)
            throw std::invalid_argument("Invalid presentation SPIR-V");
        context->initialize(options, &source);
        poll_events = source.poll_events;
        auto &c = *context;
        if (c.properties.limits.maxComputeWorkGroupInvocations < 256 ||
            c.properties.limits.maxComputeWorkGroupSize[0] < 16 ||
            c.properties.limits.maxComputeWorkGroupSize[1] < 16)
            throw std::runtime_error("Presentation shader requires 16x16 compute workgroups");
        // clang-format off
#define LOAD(name) name=device_function<PFN_vk##name>("vk" #name);
        WINDOW_FUNCTIONS(LOAD)
#undef LOAD
        // clang-format on
        surface_capabilities = c.instance_function<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
            "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        surface_formats = c.instance_function<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
            "vkGetPhysicalDeviceSurfaceFormatsKHR");
        format_properties = c.instance_function<PFN_vkGetPhysicalDeviceFormatProperties>(
            "vkGetPhysicalDeviceFormatProperties");
        VkFormatProperties rgba_properties{};
        format_properties(c.physical, VK_FORMAT_R8G8B8A8_UNORM, &rgba_properties);
        constexpr auto needed = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT |
                                VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
        if ((rgba_properties.optimalTilingFeatures & needed) != needed)
            throw std::runtime_error("RGBA8 storage/blit-source image support required");

        VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                             VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        auto descriptor_info = structure<VkDescriptorSetLayoutCreateInfo>(
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
        descriptor_info.bindingCount = 1;
        descriptor_info.pBindings = &binding;
        check(c.CreateDescriptorSetLayout(c.device, &descriptor_info, nullptr, &descriptor_layout),
              "vkCreateDescriptorSetLayout");
        VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, 16};
        auto layout_info =
            structure<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = &descriptor_layout;
        layout_info.pushConstantRangeCount = 1;
        layout_info.pPushConstantRanges = &push;
        check(c.CreatePipelineLayout(c.device, &layout_info, nullptr, &pipeline_layout),
              "vkCreatePipelineLayout");
        auto shader_info =
            structure<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
        shader_info.codeSize = spirv.size_bytes();
        shader_info.pCode = spirv.data();
        check(c.CreateShaderModule(c.device, &shader_info, nullptr, &shader),
              "vkCreateShaderModule");
        auto pipeline_info =
            structure<VkComputePipelineCreateInfo>(VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
        pipeline_info.stage = structure<VkPipelineShaderStageCreateInfo>(
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
        pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline_info.stage.module = shader;
        pipeline_info.stage.pName = "main";
        pipeline_info.layout = pipeline_layout;
        check(c.CreateComputePipelines(c.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                       &pipeline),
              "vkCreateComputePipelines");
        auto pool_info =
            structure<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
        pool_info.queueFamilyIndex = c.infos[c.selected].compute_family;
        check(c.CreateCommandPool(c.device, &pool_info, nullptr, &command_pool),
              "vkCreateCommandPool");
        auto allocation =
            structure<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
        allocation.commandPool = command_pool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocation.commandBufferCount = 1;
        check(c.AllocateCommandBuffers(c.device, &allocation, &command),
              "vkAllocateCommandBuffers");
        auto fence_info = structure<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
        check(c.CreateFence(c.device, &fence_info, nullptr, &render_fence),
              "vkCreateFence(render)");
        check(c.CreateFence(c.device, &fence_info, nullptr, &present_fence),
              "vkCreateFence(present)");
        auto semaphore_info =
            structure<VkSemaphoreCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
        check(CreateSemaphore(c.device, &semaphore_info, nullptr, &acquired),
              "vkCreateSemaphore(acquire)");
        check(CreateSemaphore(c.device, &semaphore_info, nullptr, &rendered),
              "vkCreateSemaphore(present)");
    }
    void wait_fence(VkFence fence, const char *label) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        for (;;) {
            const auto result =
                context->WaitForFences(context->device, 1, &fence, VK_TRUE, 1'000'000ULL);
            if (result != VK_TIMEOUT) {
                check(result, label);
                return;
            }
            if (std::chrono::steady_clock::now() >= deadline)
                check(result, label);
            if (poll_events)
                poll_events();
        }
    }
    void wait_idle() {
        auto &c = *context;
        // If recording failed after acquisition, consume its semaphore before teardown.
        if (acquire_pending) {
            check(ResetFences(c.device, 1, &render_fence), "vkResetFences(abandoned acquisition)");
            auto submit = structure<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
            const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            submit.waitSemaphoreCount = 1;
            submit.pWaitSemaphores = &acquired;
            submit.pWaitDstStageMask = &wait_stage;
            check(c.QueueSubmit(c.queue, 1, &submit, render_fence),
                  "vkQueueSubmit(abandoned acquisition)");
            acquire_pending = false;
            render_pending = true;
        }
        if (render_pending) {
            wait_fence(render_fence, "Wait render fence (30s)");
            render_pending = false;
        }
        if (present_pending) {
            wait_fence(present_fence, "Wait presentation fence (30s)");
            present_pending = false;
        }
    }
    void recreate(std::uint32_t width, std::uint32_t height) {
        auto &c = *context;
        wait_idle();
        VkSurfaceCapabilitiesKHR caps{};
        check(surface_capabilities(c.physical, c.surface, &caps),
              "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        const auto formats = enumerate<VkSurfaceFormatKHR>(
            [&](auto *n, auto *values) {
                return surface_formats(c.physical, c.surface, n, values);
            },
            "vkGetPhysicalDeviceSurfaceFormatsKHR");
        VkFormat format = VK_FORMAT_UNDEFINED;
        for (const auto desired : {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM}) {
            for (const auto &available : formats) {
                if (available.colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ||
                    (available.format != desired && available.format != VK_FORMAT_UNDEFINED))
                    continue;
                VkFormatProperties support{};
                format_properties(c.physical, desired, &support);
                if (support.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) {
                    format = desired;
                    break;
                }
            }
            if (format != VK_FORMAT_UNDEFINED)
                break;
        }
        if (format == VK_FORMAT_UNDEFINED)
            throw std::runtime_error("Surface needs an RGBA8/BGRA8 UNORM blit destination with "
                                     "sRGB nonlinear color space");
        if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
            throw std::runtime_error("Swapchain does not support transfer destinations");
        VkExtent2D extent = caps.currentExtent;
        if (extent.width == UINT32_MAX) {
            extent.width = std::clamp(width, caps.minImageExtent.width, caps.maxImageExtent.width);
            extent.height =
                std::clamp(height, caps.minImageExtent.height, caps.maxImageExtent.height);
        }
        if (!extent.width || !extent.height) {
            dirty = true;
            return;
        }
        if (extent.width > 8192 || extent.height > 8192 ||
            std::uint64_t{extent.width} * extent.height > 8'388'608)
            throw std::runtime_error(
                "Diagnostic presentation budget exceeded (max 8192 per axis / 8388608 pixels)");
        if ((extent.width + 15) / 16 > c.properties.limits.maxComputeWorkGroupCount[0] ||
            (extent.height + 15) / 16 > c.properties.limits.maxComputeWorkGroupCount[1])
            throw std::runtime_error("Presentation extent exceeds compute device limits");
        if (caps.minImageCount > 8)
            throw std::runtime_error("Surface requires more than the 8-image diagnostic budget");
        auto count = std::min(caps.minImageCount + 1, 8U);
        if (caps.maxImageCount)
            count = std::min(count, caps.maxImageCount);
        VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        for (auto candidate :
             {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
              VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
            if (caps.supportedCompositeAlpha & candidate) {
                alpha = candidate;
                break;
            }
        auto candidate = std::make_unique<Swapchain>(*this);
        candidate->extent = extent;
        candidate->format = format;
        auto info =
            structure<VkSwapchainCreateInfoKHR>(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
        info.surface = c.surface;
        info.minImageCount = count;
        info.imageFormat = format;
        info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        info.imageExtent = extent;
        info.imageArrayLayers = 1;
        info.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        info.preTransform = caps.currentTransform;
        info.compositeAlpha = alpha;
        info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        info.clipped = VK_TRUE;
        info.oldSwapchain = swapchain ? swapchain->handle : VK_NULL_HANDLE;
        check(CreateSwapchainKHR(c.device, &info, nullptr, &candidate->handle),
              "vkCreateSwapchainKHR");
        candidate->images = enumerate<VkImage>(
            [&](auto *n, auto *values) {
                return GetSwapchainImagesKHR(c.device, candidate->handle, n, values);
            },
            "vkGetSwapchainImagesKHR");
        auto image_info = structure<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
        image_info.extent = {extent.width, extent.height, 1};
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        check(CreateImage(c.device, &image_info, nullptr, &candidate->compute_image),
              "vkCreateImage");
        VkMemoryRequirements requirements{};
        GetImageMemoryRequirements(c.device, candidate->compute_image, &requirements);
        std::uint32_t memory_type = UINT32_MAX;
        for (std::uint32_t i = 0; i < c.memory_properties.memoryTypeCount; ++i)
            if (requirements.memoryTypeBits & (1U << i)) {
                memory_type = i;
                if (c.memory_properties.memoryTypes[i].propertyFlags &
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
                    break;
            }
        if (memory_type == UINT32_MAX)
            throw std::runtime_error("No compatible image memory type");
        auto memory_info = structure<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        memory_info.allocationSize = requirements.size;
        memory_info.memoryTypeIndex = memory_type;
        check(c.AllocateMemory(c.device, &memory_info, nullptr, &candidate->memory),
              "vkAllocateMemory(image)");
        check(BindImageMemory(c.device, candidate->compute_image, candidate->memory, 0),
              "vkBindImageMemory");
        auto view = structure<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
        view.image = candidate->compute_image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = VK_FORMAT_R8G8B8A8_UNORM;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        check(CreateImageView(c.device, &view, nullptr, &candidate->view), "vkCreateImageView");
        VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1};
        auto pool =
            structure<VkDescriptorPoolCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
        pool.maxSets = 1;
        pool.poolSizeCount = 1;
        pool.pPoolSizes = &pool_size;
        check(c.CreateDescriptorPool(c.device, &pool, nullptr, &candidate->descriptors),
              "vkCreateDescriptorPool");
        auto set =
            structure<VkDescriptorSetAllocateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
        set.descriptorPool = candidate->descriptors;
        set.descriptorSetCount = 1;
        set.pSetLayouts = &descriptor_layout;
        check(c.AllocateDescriptorSets(c.device, &set, &candidate->descriptor),
              "vkAllocateDescriptorSets");
        VkDescriptorImageInfo image_descriptor{VK_NULL_HANDLE, candidate->view,
                                               VK_IMAGE_LAYOUT_GENERAL};
        auto write = structure<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
        write.dstSet = candidate->descriptor;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        write.pImageInfo = &image_descriptor;
        c.UpdateDescriptorSets(c.device, 1, &write, 0, nullptr);
        // Presentation fences were waited above; old resources are no longer used by WSI.
        swapchain = std::move(candidate);
        stats.width = extent.width;
        stats.height = extent.height;
        ++stats.recreations;
        dirty = false;
    }
    VkImageMemoryBarrier2 image_barrier(VkImage image, VkImageLayout old_layout,
                                        VkImageLayout new_layout, VkPipelineStageFlags2 src_stage,
                                        VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage,
                                        VkAccessFlags2 dst_access) {
        auto b = structure<VkImageMemoryBarrier2>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2);
        b.srcStageMask = src_stage;
        b.srcAccessMask = src_access;
        b.dstStageMask = dst_stage;
        b.dstAccessMask = dst_access;
        b.oldLayout = old_layout;
        b.newLayout = new_layout;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = image;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        return b;
    }
    void barriers(std::span<const VkImageMemoryBarrier2> images) {
        auto dependency = structure<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
        dependency.imageMemoryBarrierCount = static_cast<std::uint32_t>(images.size());
        dependency.pImageMemoryBarriers = images.data();
        context->CmdPipelineBarrier2(command, &dependency);
    }
    void begin_commands() {
        check(ResetCommandPool(context->device, command_pool, 0), "vkResetCommandPool");
        auto begin =
            structure<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(context->BeginCommandBuffer(command, &begin), "vkBeginCommandBuffer");
    }
    FrameStatus draw(std::uint32_t width, std::uint32_t height, std::uint32_t phase) {
        if (finished)
            throw std::logic_error("Renderer already finished");
        if (!width || !height) {
            ++stats.suspended;
            dirty = true;
            return FrameStatus::suspended;
        }
        auto &c = *context;
        wait_idle();
        if (dirty || !swapchain || width != stats.width || height != stats.height)
            recreate(width, height);
        if (dirty) {
            ++stats.suspended;
            return FrameStatus::suspended;
        }
        std::uint32_t index{};
        auto result = AcquireNextImageKHR(c.device, swapchain->handle, 100'000'000ULL, acquired,
                                          VK_NULL_HANDLE, &index);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            dirty = true;
            ++stats.acquire_retries;
            return FrameStatus::retry;
        }
        if (result == VK_TIMEOUT || result == VK_NOT_READY) {
            ++stats.acquire_retries;
            return FrameStatus::retry;
        }
        if (result != VK_SUBOPTIMAL_KHR)
            check(result, "vkAcquireNextImageKHR");
        acquire_pending = true;
        const bool suboptimal = result == VK_SUBOPTIMAL_KHR;
        begin_commands();
        const auto image = swapchain->compute_image;
        const std::array start{image_barrier(
            image,
            swapchain->image_initialized ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_NONE, 0,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT)};
        barriers(start);
        c.CmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        c.CmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1,
                                &swapchain->descriptor, 0, nullptr);
        const std::array<std::uint32_t, 4> parameters{stats.width, stats.height, phase, 0};
        static_assert(sizeof(parameters) == 16);
        CmdPushConstants(command, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                         sizeof(parameters), parameters.data());
        c.CmdDispatch(command, (stats.width + 15) / 16, (stats.height + 15) / 16, 1);
        const std::array before_copy{
            image_barrier(image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_ACCESS_2_SHADER_WRITE_BIT,
                          VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT),
            image_barrier(swapchain->images.at(index), VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, 0,
                          VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT)};
        barriers(before_copy);
        VkImageBlit region{};
        region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.dstSubresource = region.srcSubresource;
        region.srcOffsets[1] = {static_cast<std::int32_t>(stats.width),
                                static_cast<std::int32_t>(stats.height), 1};
        region.dstOffsets[1] = region.srcOffsets[1];
        CmdBlitImage(command, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchain->images[index],
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);
        const std::array after_copy{
            image_barrier(swapchain->images[index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, 0),
            image_barrier(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                          VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                          VK_PIPELINE_STAGE_2_NONE, 0)};
        barriers(after_copy);
        check(c.EndCommandBuffer(command), "vkEndCommandBuffer");
        check(ResetFences(c.device, 1, &render_fence), "vkResetFences(render)");
        auto submit = structure<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
        const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &acquired;
        submit.pWaitDstStageMask = &wait_stage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &rendered;
        check(c.QueueSubmit(c.queue, 1, &submit, render_fence), "vkQueueSubmit(presentation)");
        acquire_pending = false;
        render_pending = true;
        check(ResetFences(c.device, 1, &present_fence), "vkResetFences(present)");
        auto fence_info = structure<VkSwapchainPresentFenceInfoKHR>(
            VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR);
        fence_info.swapchainCount = 1;
        fence_info.pFences = &present_fence;
        auto present = structure<VkPresentInfoKHR>(VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
        present.pNext = &fence_info;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &rendered;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain->handle;
        present.pImageIndices = &index;
        result = QueuePresentKHR(c.queue, &present);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR &&
            result != VK_ERROR_OUT_OF_DATE_KHR)
            check(result, "vkQueuePresentKHR");
        present_pending = true;
        dirty = suboptimal || result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR;
        swapchain->image_initialized = true;
        last_phase = phase;
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
            return FrameStatus::retry;
        ++stats.presented;
        return FrameStatus::presented;
    }
    void capture(const std::filesystem::path &path) {
        if (finished || !swapchain || !swapchain->image_initialized)
            throw std::logic_error("No rendered image available for capture");
        wait_idle();
        auto &c = *context;
        detail::VulkanContext::Buffer staging(c);
        const auto bytes = std::size_t{stats.width} * stats.height * 4;
        staging.create(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        begin_commands();
        const std::array before{image_barrier(
            swapchain->compute_image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT)};
        barriers(before);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {stats.width, stats.height, 1};
        CmdCopyImageToBuffer(command, swapchain->compute_image,
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, staging.buffer, 1, &copy);
        auto host = structure<VkMemoryBarrier2>(VK_STRUCTURE_TYPE_MEMORY_BARRIER_2);
        host.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        host.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        host.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
        host.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
        auto dependency = structure<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
        dependency.memoryBarrierCount = 1;
        dependency.pMemoryBarriers = &host;
        c.CmdPipelineBarrier2(command, &dependency);
        const std::array after{
            image_barrier(swapchain->compute_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_NONE, 0)};
        barriers(after);
        check(c.EndCommandBuffer(command), "vkEndCommandBuffer(capture)");
        check(ResetFences(c.device, 1, &render_fence), "vkResetFences(capture)");
        auto submit = structure<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        check(c.QueueSubmit(c.queue, 1, &submit, render_fence), "vkQueueSubmit(capture)");
        render_pending = true;
        // Staging must outlive queued work even if waiting reports an error.
        try {
            wait_idle();
        } catch (...) {
            (void)c.DeviceWaitIdle(c.device);
            throw;
        }
        staging.synchronize(true);
        const auto *pixels = static_cast<const std::uint8_t *>(staging.mapped);
        for (std::uint32_t y = 0; y < stats.height; ++y)
            for (std::uint32_t x = 0; x < stats.width; ++x) {
                std::array<std::uint32_t, 4> expected{
                    x * 255 / std::max(stats.width - 1, 1U),
                    y * 255 / std::max(stats.height - 1, 1U),
                    ((x / 32 + y / 32 + last_phase) & 1U) ? 144U : 48U, 255};
                if (!x || !y || x + 1 == stats.width || y + 1 == stats.height)
                    expected = {255, 255, 255, 255};
                const auto offset = (std::size_t{y} * stats.width + x) * 4;
                for (std::size_t channel = 0; channel < 4; ++channel)
                    if (pixels[offset + channel] != expected[channel])
                        throw std::runtime_error("Diagnostic pixel mismatch at " +
                                                 std::to_string(x) + "," + std::to_string(y));
            }
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary);
        if (!out)
            throw std::runtime_error("Cannot open diagnostic capture: " + path.string());
        out << "P6\n" << stats.width << ' ' << stats.height << "\n255\n";
        for (std::size_t i = 0; i < bytes; i += 4)
            out.write(reinterpret_cast<const char *>(pixels + i), 3);
        out.flush();
        if (!out)
            throw std::runtime_error("Diagnostic capture write failed");
        std::cout << "capture=" << path.string()
                  << " oracle_pixels=" << std::uint64_t{stats.width} * stats.height
                  << " exact_rgba8=pass\n";
    }
    void release_resources() noexcept {
        auto &c = *context;
        swapchain.reset();
        if (command_pool)
            c.DestroyCommandPool(c.device, command_pool, nullptr);
        if (acquired)
            DestroySemaphore(c.device, acquired, nullptr);
        if (rendered)
            DestroySemaphore(c.device, rendered, nullptr);
        if (render_fence)
            c.DestroyFence(c.device, render_fence, nullptr);
        if (present_fence)
            c.DestroyFence(c.device, present_fence, nullptr);
        if (pipeline)
            c.DestroyPipeline(c.device, pipeline, nullptr);
        if (shader)
            c.DestroyShaderModule(c.device, shader, nullptr);
        if (pipeline_layout)
            c.DestroyPipelineLayout(c.device, pipeline_layout, nullptr);
        if (descriptor_layout)
            c.DestroyDescriptorSetLayout(c.device, descriptor_layout, nullptr);
        command_pool = {};
        acquired = {};
        rendered = {};
        render_fence = {};
        present_fence = {};
        pipeline = {};
        shader = {};
        pipeline_layout = {};
        descriptor_layout = {};
    }
    void finish() {
        if (finished)
            return;
        wait_idle();
        release_resources();
        finished = true;
    }
    ~Impl() {
        try {
            finish();
        } catch (const std::exception &e) {
            std::cerr << "Presentation teardown: " << e.what() << '\n';
            if (context->device && context->DeviceWaitIdle)
                (void)context->DeviceWaitIdle(context->device);
            release_resources();
        }
    }
};
#undef WINDOW_FUNCTIONS
WindowRenderer::WindowRenderer(const SurfaceSource &source, ComputeOptions options,
                               std::span<const std::uint32_t> shader)
    : impl_(std::make_unique<Impl>()) {
    impl_->initialize(source, options, shader);
}
WindowRenderer::~WindowRenderer() = default;
FrameStatus WindowRenderer::draw(std::uint32_t width, std::uint32_t height, std::uint32_t phase) {
    return impl_->draw(width, height, phase);
}
void WindowRenderer::capture(const std::filesystem::path &path) {
    impl_->capture(path);
}
void WindowRenderer::finish() {
    impl_->finish();
}
WindowStatistics WindowRenderer::statistics() const {
    auto result = impl_->stats;
    result.validation_errors = impl_->context->errors.load();
    return result;
}
const DeviceInfo &WindowRenderer::device() const {
    return impl_->context->infos[impl_->context->selected];
}
bool WindowRenderer::validation_enabled() const {
    return impl_->context->validation;
}
} // namespace kerr::renderer
