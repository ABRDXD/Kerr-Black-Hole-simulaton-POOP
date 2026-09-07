#pragma once
#include "kerr/renderer/ComputeContext.hpp"
#include "kerr/renderer/SurfaceSource.hpp"

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan_core.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

namespace kerr::renderer::detail {
inline void check(VkResult result, const char *operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + " failed (VkResult " +
                                 std::to_string(static_cast<int>(result)) + ")");
}
template <class T> T structure(VkStructureType type) {
    T value{};
    value.sType = type;
    return value;
}
template <class T, class Query> std::vector<T> enumerate(Query query, const char *label) {
    for (int retry = 0; retry < 4; ++retry) {
        std::uint32_t count{};
        check(query(&count, static_cast<T *>(nullptr)), label);
        std::vector<T> values(count);
        auto result = query(&count, values.data());
        if (result == VK_INCOMPLETE)
            continue;
        check(result, label);
        values.resize(count);
        return values;
    }
    throw std::runtime_error(std::string(label) +
                             ": enumeration changed repeatedly; retry the command");
}
struct VulkanContext {
#ifdef _WIN32
    HMODULE library{};
#else
    void *library{};
#endif
    VkInstance instance{};
    VkSurfaceKHR surface{};
    VkDebugUtilsMessengerEXT messenger{};
    VkPhysicalDevice physical{};
    VkDevice device{};
    VkQueue queue{};
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceMemoryProperties memory_properties{};
    std::vector<DeviceInfo> infos;
    std::size_t selected{};
    bool validation{};
    std::atomic<std::uint32_t> errors{};
    PFN_vkGetInstanceProcAddr get_instance{};
    PFN_vkGetDeviceProcAddr get_device{};
    PFN_vkDestroyInstance destroy_instance{};
    PFN_vkDestroySurfaceKHR destroy_surface{};
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger{};
    // clang-format off
#define KERR_DEVICE_FUNCTIONS(X) \
    X(DestroyDevice) \
    X(GetDeviceQueue) \
    X(DeviceWaitIdle) \
    X(CreateBuffer) \
    X(DestroyBuffer) \
    X(GetBufferMemoryRequirements) \
    X(AllocateMemory) \
    X(FreeMemory) \
    X(BindBufferMemory) \
    X(MapMemory) \
    X(UnmapMemory) \
    X(FlushMappedMemoryRanges) \
    X(InvalidateMappedMemoryRanges) \
    X(CreateDescriptorSetLayout) \
    X(DestroyDescriptorSetLayout) \
    X(CreatePipelineLayout) \
    X(DestroyPipelineLayout) \
    X(CreateShaderModule) \
    X(DestroyShaderModule) \
    X(CreateComputePipelines) \
    X(DestroyPipeline) \
    X(CreateDescriptorPool) \
    X(DestroyDescriptorPool) \
    X(AllocateDescriptorSets) \
    X(UpdateDescriptorSets) \
    X(CreateCommandPool) \
    X(DestroyCommandPool) \
    X(AllocateCommandBuffers) \
    X(BeginCommandBuffer) \
    X(EndCommandBuffer) \
    X(CmdBindPipeline) \
    X(CmdBindDescriptorSets) \
    X(CmdDispatch) \
    X(CmdPipelineBarrier2) \
    X(CreateFence) \
    X(DestroyFence) \
    X(QueueSubmit) \
    X(WaitForFences) \
    X(CreateQueryPool) \
    X(DestroyQueryPool) \
    X(CmdResetQueryPool) \
    X(CmdWriteTimestamp) \
    X(GetQueryPoolResults)
// clang-format on
#define DECLARE(name) PFN_vk##name name{};
    KERR_DEVICE_FUNCTIONS(DECLARE)
#undef DECLARE

    template <class T> T instance_function(const char *name) {
        auto function = reinterpret_cast<T>(get_instance(instance, name));
        if (!function)
            throw std::runtime_error(std::string("Vulkan entry point unavailable: ") + name);
        return function;
    }
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT,
                   const VkDebugUtilsMessengerCallbackDataEXT *data, void *user) {
        auto *self = static_cast<VulkanContext *>(user);
        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
            ++self->errors;
        std::cerr << "Vulkan validation: "
                  << (data && data->pMessage ? data->pMessage : "(no message)") << '\n';
        return VK_FALSE;
    }
    void initialize(ComputeOptions options, const SurfaceSource *source = nullptr) {
#ifdef _WIN32
        library = LoadLibraryExW(L"vulkan-1.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (library)
            get_instance = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                GetProcAddress(library, "vkGetInstanceProcAddr"));
#else
        library = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
        if (library)
            get_instance = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                dlsym(library, "vkGetInstanceProcAddr"));
#endif
        if (!get_instance)
            throw std::runtime_error(
                "Vulkan loader unavailable. Install a Vulkan-capable GPU driver.");
        auto version_function = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
            get_instance(nullptr, "vkEnumerateInstanceVersion"));
        std::uint32_t version = VK_API_VERSION_1_0;
        if (version_function)
            check(version_function(&version), "vkEnumerateInstanceVersion");
        if (version < VK_API_VERSION_1_3)
            throw std::runtime_error("Vulkan 1.3 loader required. Update the GPU driver/runtime.");
        auto list_layers = instance_function<PFN_vkEnumerateInstanceLayerProperties>(
            "vkEnumerateInstanceLayerProperties");
        auto layers =
            enumerate<VkLayerProperties>(list_layers, "vkEnumerateInstanceLayerProperties");
        const char *layer_name = "VK_LAYER_KHRONOS_validation";
        bool layer_present = std::any_of(layers.begin(), layers.end(), [&](const auto &l) {
            return std::strcmp(l.layerName, layer_name) == 0;
        });
        if (options.validation == ValidationMode::required && !layer_present)
            throw std::runtime_error("VK_LAYER_KHRONOS_validation is required but unavailable. "
                                     "Install the Vulkan SDK or configure VK_LAYER_PATH.");
        validation = options.validation != ValidationMode::disabled && layer_present;
        auto list_extensions = instance_function<PFN_vkEnumerateInstanceExtensionProperties>(
            "vkEnumerateInstanceExtensionProperties");
        auto extensions = enumerate<VkExtensionProperties>(
            [&](auto *count, auto *values) { return list_extensions(nullptr, count, values); },
            "vkEnumerateInstanceExtensionProperties");
        const char *debug_extension = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
        bool debug_present = std::any_of(extensions.begin(), extensions.end(), [&](const auto &e) {
            return std::strcmp(e.extensionName, debug_extension) == 0;
        });
        if (validation && !debug_present)
            throw std::runtime_error(
                "Validation requires VK_EXT_debug_utils to count and report errors.");
        std::vector<const char *> required_extensions;
        if (source) {
            if (!source->create)
                throw std::invalid_argument("Missing window surface factory");
            for (const auto &name : source->instance_extensions)
                required_extensions.push_back(name.c_str());
            required_extensions.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
            required_extensions.push_back(VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);
        }
        if (validation)
            required_extensions.push_back(debug_extension);
        for (const auto *name : required_extensions)
            if (std::none_of(extensions.begin(), extensions.end(), [&](const auto &e) {
                    return std::strcmp(e.extensionName, name) == 0;
                }))
                throw std::runtime_error(std::string("Required instance extension unavailable: ") +
                                         name);
        auto debug = structure<VkDebugUtilsMessengerCreateInfoEXT>(
            VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT);
        debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug.pfnUserCallback = debug_callback;
        debug.pUserData = this;
        auto app = structure<VkApplicationInfo>(VK_STRUCTURE_TYPE_APPLICATION_INFO);
        app.pApplicationName = "KerrLab headless conformance";
        app.apiVersion = VK_API_VERSION_1_3;
        auto create = structure<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
        create.pApplicationInfo = &app;
        create.enabledExtensionCount = static_cast<std::uint32_t>(required_extensions.size());
        create.ppEnabledExtensionNames = required_extensions.data();
        if (validation) {
            create.enabledLayerCount = 1;
            create.ppEnabledLayerNames = &layer_name;
            create.pNext = &debug;
        }
        auto create_instance = instance_function<PFN_vkCreateInstance>("vkCreateInstance");
        check(create_instance(&create, nullptr, &instance), "vkCreateInstance");
        destroy_instance = instance_function<PFN_vkDestroyInstance>("vkDestroyInstance");
        if (source) {
            destroy_surface = instance_function<PFN_vkDestroySurfaceKHR>("vkDestroySurfaceKHR");
            const auto bits = source->create(reinterpret_cast<std::uintptr_t>(instance));
            static_assert(sizeof(surface) <= sizeof(bits));
            std::memcpy(&surface, &bits, sizeof(surface));
            if (!surface)
                throw std::runtime_error("Window surface factory returned a null surface");
        }
        if (validation) {
            destroy_messenger = instance_function<PFN_vkDestroyDebugUtilsMessengerEXT>(
                "vkDestroyDebugUtilsMessengerEXT");
            auto create_debug = instance_function<PFN_vkCreateDebugUtilsMessengerEXT>(
                "vkCreateDebugUtilsMessengerEXT");
            check(create_debug(instance, &debug, nullptr, &messenger),
                  "vkCreateDebugUtilsMessengerEXT");
        }
        auto list_devices =
            instance_function<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
        auto devices = enumerate<VkPhysicalDevice>(
            [&](auto *count, auto *values) { return list_devices(instance, count, values); },
            "vkEnumeratePhysicalDevices");
        auto get_properties =
            instance_function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties");
        auto get_features =
            instance_function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2");
        auto get_families = instance_function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
            "vkGetPhysicalDeviceQueueFamilyProperties");
        PFN_vkGetPhysicalDeviceSurfaceSupportKHR surface_support{};
        if (source)
            surface_support = instance_function<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
                "vkGetPhysicalDeviceSurfaceSupportKHR");
        auto list_device_extensions = instance_function<PFN_vkEnumerateDeviceExtensionProperties>(
            "vkEnumerateDeviceExtensionProperties");
        for (std::size_t i = 0; i < devices.size(); ++i) {
            VkPhysicalDeviceProperties p{};
            get_properties(devices[i], &p);
            auto f13 = structure<VkPhysicalDeviceVulkan13Features>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
            auto maintenance = structure<VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR>(
                VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR);
            if (source)
                f13.pNext = &maintenance;
            auto features =
                structure<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
            features.pNext = &f13;
            get_features(devices[i], &features);
            std::uint32_t count{};
            get_families(devices[i], &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            get_families(devices[i], &count, families.data());
            DeviceInfo info{};
            info.index = static_cast<std::uint32_t>(i);
            info.name = p.deviceName;
            info.api_version = p.apiVersion;
            info.driver_version = p.driverVersion;
            info.vendor_id = p.vendorID;
            info.device_id = p.deviceID;
            info.discrete = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            info.float64 = features.features.shaderFloat64 != VK_FALSE;
            bool compute = false;
            for (std::uint32_t j = 0; j < count; ++j) {
                if (source) {
                    VkBool32 supports_present{};
                    check(surface_support(devices[i], j, surface, &supports_present),
                          "vkGetPhysicalDeviceSurfaceSupportKHR");
                    if (!supports_present || !(families[j].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                        continue;
                }
                if (families[j].queueCount && (families[j].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
                    info.compute_family = j;
                    info.timestamp_bits = families[j].timestampValidBits;
                    compute = true;
                    if (source || !(families[j].queueFlags & VK_QUEUE_GRAPHICS_BIT))
                        break;
                }
            }
            if (p.apiVersion < VK_API_VERSION_1_3)
                info.rejection = "Vulkan 1.3 device required";
            else if (!compute)
                info.rejection =
                    source ? "no combined graphics/compute/present queue" : "no compute queue";
            else if (!f13.synchronization2)
                info.rejection = "synchronization2 unavailable";
            else if (p.limits.maxComputeWorkGroupInvocations < 64 ||
                     p.limits.maxComputeWorkGroupSize[0] < 64)
                info.rejection = "64-invocation workgroup unsupported";
            else if (p.limits.maxPerStageDescriptorStorageBuffers < 2)
                info.rejection = "two storage buffers required";
            if (source && info.rejection.empty()) {
                const auto device_extensions = enumerate<VkExtensionProperties>(
                    [&](auto *n, auto *values) {
                        return list_device_extensions(devices[i], nullptr, n, values);
                    },
                    "vkEnumerateDeviceExtensionProperties");
                for (const char *name : {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                         VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME})
                    if (std::none_of(
                            device_extensions.begin(), device_extensions.end(),
                            [&](const auto &e) { return std::strcmp(e.extensionName, name) == 0; }))
                        info.rejection =
                            std::string("required device extension unavailable: ") + name;
                if (!maintenance.swapchainMaintenance1)
                    info.rejection = "swapchainMaintenance1 feature required for safe presentation "
                                     "resource lifetime";
            }
            info.suitable = info.rejection.empty();
            infos.push_back(std::move(info));
        }
        if (options.device_index < -1)
            throw std::invalid_argument(
                "Device index must be nonnegative, or -1 for automatic selection");
        bool found = false;
        for (std::size_t i = 0; i < infos.size(); ++i) {
            if (options.device_index >= 0 && i != static_cast<std::size_t>(options.device_index))
                continue;
            if (infos[i].suitable && (!found || infos[i].discrete)) {
                selected = i;
                found = true;
            }
        }
        if (!found) {
            std::string detail;
            for (const auto &info : infos)
                detail += "\n  " + std::to_string(info.index) + ": " + info.name +
                          (info.suitable ? " (suitable)" : " (" + info.rejection + ")");
            throw std::runtime_error("No suitable selected Vulkan compute device. Check --device "
                                     "and update the driver." +
                                     detail);
        }
        physical = devices[selected];
        get_properties(physical, &properties);
        auto get_memory = instance_function<PFN_vkGetPhysicalDeviceMemoryProperties>(
            "vkGetPhysicalDeviceMemoryProperties");
        get_memory(physical, &memory_properties);
        float priority = 1.0F;
        auto queue_info =
            structure<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
        queue_info.queueFamilyIndex = infos[selected].compute_family;
        queue_info.queueCount = 1;
        queue_info.pQueuePriorities = &priority;
        auto requested = structure<VkPhysicalDeviceVulkan13Features>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
        requested.synchronization2 = VK_TRUE;
        auto requested_maintenance = structure<VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR);
        requested_maintenance.swapchainMaintenance1 = VK_TRUE;
        if (source)
            requested.pNext = &requested_maintenance;
        const std::array<const char *, 2> window_extensions{
            VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME};
        auto device_info = structure<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
        device_info.pNext = &requested;
        device_info.queueCreateInfoCount = 1;
        device_info.pQueueCreateInfos = &queue_info;
        if (source) {
            device_info.enabledExtensionCount =
                static_cast<std::uint32_t>(window_extensions.size());
            device_info.ppEnabledExtensionNames = window_extensions.data();
        }
        auto create_device = instance_function<PFN_vkCreateDevice>("vkCreateDevice");
        get_device = instance_function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
        check(create_device(physical, &device_info, nullptr, &device), "vkCreateDevice");
#define LOAD(name)                                                                                 \
    name = reinterpret_cast<PFN_vk##name>(get_device(device, "vk" #name));                         \
    if (!name)                                                                                     \
        throw std::runtime_error("Device entry point unavailable: vk" #name);
        KERR_DEVICE_FUNCTIONS(LOAD)
#undef LOAD
        GetDeviceQueue(device, infos[selected].compute_family, 0, &queue);
    }
    ~VulkanContext() {
        if (device && DeviceWaitIdle)
            (void)DeviceWaitIdle(device);
        if (device && DestroyDevice)
            DestroyDevice(device, nullptr);
        if (surface && destroy_surface)
            destroy_surface(instance, surface, nullptr);
        if (messenger && destroy_messenger)
            destroy_messenger(instance, messenger, nullptr);
        if (instance && destroy_instance)
            destroy_instance(instance, nullptr);
#ifdef _WIN32
        if (library)
            FreeLibrary(library);
#else
        if (library)
            dlclose(library);
#endif
    }

    struct Buffer final {
        VulkanContext &owner;
        VkBuffer buffer{};
        VkDeviceMemory memory{};
        void *mapped{};
        bool coherent{};
        explicit Buffer(VulkanContext &o) : owner(o) {}
        void create(std::size_t size,
                    VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
            auto info = structure<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
            info.size = size;
            info.usage = usage;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            check(owner.CreateBuffer(owner.device, &info, nullptr, &buffer), "vkCreateBuffer");
            VkMemoryRequirements requirements{};
            owner.GetBufferMemoryRequirements(owner.device, buffer, &requirements);
            std::uint32_t chosen = UINT32_MAX;
            for (std::uint32_t i = 0; i < owner.memory_properties.memoryTypeCount; ++i) {
                auto flags = owner.memory_properties.memoryTypes[i].propertyFlags;
                if ((requirements.memoryTypeBits & (1U << i)) &&
                    (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
                    chosen = i;
                    if (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
                        break;
                }
            }
            if (chosen == UINT32_MAX)
                throw std::runtime_error("No host-visible storage-buffer memory. A staging-buffer "
                                         "path is not implemented yet.");
            coherent = (owner.memory_properties.memoryTypes[chosen].propertyFlags &
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
            auto allocation =
                structure<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = chosen;
            check(owner.AllocateMemory(owner.device, &allocation, nullptr, &memory),
                  "vkAllocateMemory");
            check(owner.BindBufferMemory(owner.device, buffer, memory, 0), "vkBindBufferMemory");
            check(owner.MapMemory(owner.device, memory, 0, VK_WHOLE_SIZE, 0, &mapped),
                  "vkMapMemory");
        }
        void synchronize(bool read) {
            if (coherent)
                return;
            auto range = structure<VkMappedMemoryRange>(VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE);
            range.memory = memory;
            range.size = VK_WHOLE_SIZE;
            if (read)
                check(owner.InvalidateMappedMemoryRanges(owner.device, 1, &range),
                      "vkInvalidateMappedMemoryRanges");
            else
                check(owner.FlushMappedMemoryRanges(owner.device, 1, &range),
                      "vkFlushMappedMemoryRanges");
        }
        ~Buffer() {
            if (mapped)
                owner.UnmapMemory(owner.device, memory);
            if (buffer)
                owner.DestroyBuffer(owner.device, buffer, nullptr);
            if (memory)
                owner.FreeMemory(owner.device, memory, nullptr);
        }
    };
    struct DispatchResources final {
        VulkanContext &o;
        VkDescriptorSetLayout descriptor_layout{};
        VkPipelineLayout pipeline_layout{};
        VkShaderModule shader{};
        VkPipeline pipeline{};
        VkDescriptorPool descriptor_pool{};
        VkCommandPool command_pool{};
        VkFence fence{};
        VkQueryPool queries{};
        bool submitted{};
        explicit DispatchResources(VulkanContext &owner) : o(owner) {}
        ~DispatchResources() {
            // On timeout/error, keep resources alive until the driver releases outstanding work.
            if (submitted)
                (void)o.DeviceWaitIdle(o.device);
            if (fence)
                o.DestroyFence(o.device, fence, nullptr);
            if (queries)
                o.DestroyQueryPool(o.device, queries, nullptr);
            if (command_pool)
                o.DestroyCommandPool(o.device, command_pool, nullptr);
            if (descriptor_pool)
                o.DestroyDescriptorPool(o.device, descriptor_pool, nullptr);
            if (pipeline)
                o.DestroyPipeline(o.device, pipeline, nullptr);
            if (shader)
                o.DestroyShaderModule(o.device, shader, nullptr);
            if (pipeline_layout)
                o.DestroyPipelineLayout(o.device, pipeline_layout, nullptr);
            if (descriptor_layout)
                o.DestroyDescriptorSetLayout(o.device, descriptor_layout, nullptr);
        }
    };
    DispatchResult dispatch(std::span<const std::uint32_t> spirv, std::span<const std::byte> input,
                            std::size_t output_bytes, std::uint32_t workgroups) {
        if (spirv.size() < 5 || spirv[0] != 0x07230203U)
            throw std::invalid_argument("Invalid SPIR-V header");
        if (input.empty() || !output_bytes || !workgroups)
            throw std::invalid_argument(
                "Compute dispatch requires nonempty buffers and workgroups");
        if (workgroups > max_headless_workgroups)
            throw std::invalid_argument(
                "Compute dispatch exceeds the headless work budget (16384 groups)");
        if (input.size() > properties.limits.maxStorageBufferRange ||
            output_bytes > properties.limits.maxStorageBufferRange ||
            workgroups > properties.limits.maxComputeWorkGroupCount[0])
            throw std::invalid_argument("Compute dispatch exceeds selected device limits");
        Buffer in(*this), out(*this);
        in.create(input.size());
        out.create(output_bytes);
        std::memcpy(in.mapped, input.data(), input.size());
        std::memset(out.mapped, 0xcd, output_bytes); // Missing writes cannot silently look valid.
        in.synchronize(false);
        out.synchronize(false);
        DispatchResources resources(*this); // Destroy before the buffers.
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
        for (std::uint32_t i = 0; i < 2; ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        auto layout = structure<VkDescriptorSetLayoutCreateInfo>(
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
        layout.bindingCount = 2;
        layout.pBindings = bindings.data();
        check(CreateDescriptorSetLayout(device, &layout, nullptr, &resources.descriptor_layout),
              "vkCreateDescriptorSetLayout");
        auto pipeline_layout =
            structure<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
        pipeline_layout.setLayoutCount = 1;
        pipeline_layout.pSetLayouts = &resources.descriptor_layout;
        check(CreatePipelineLayout(device, &pipeline_layout, nullptr, &resources.pipeline_layout),
              "vkCreatePipelineLayout");
        auto shader_info =
            structure<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
        shader_info.codeSize = spirv.size_bytes();
        shader_info.pCode = spirv.data();
        check(CreateShaderModule(device, &shader_info, nullptr, &resources.shader),
              "vkCreateShaderModule");
        auto pipeline_info =
            structure<VkComputePipelineCreateInfo>(VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
        pipeline_info.stage = structure<VkPipelineShaderStageCreateInfo>(
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
        pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        pipeline_info.stage.module = resources.shader;
        pipeline_info.stage.pName = "main";
        pipeline_info.layout = resources.pipeline_layout;
        check(CreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
                                     &resources.pipeline),
              "vkCreateComputePipelines");
        VkDescriptorPoolSize pool_size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2};
        auto pool =
            structure<VkDescriptorPoolCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
        pool.maxSets = 1;
        pool.poolSizeCount = 1;
        pool.pPoolSizes = &pool_size;
        check(CreateDescriptorPool(device, &pool, nullptr, &resources.descriptor_pool),
              "vkCreateDescriptorPool");
        auto descriptor_allocation =
            structure<VkDescriptorSetAllocateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
        descriptor_allocation.descriptorPool = resources.descriptor_pool;
        descriptor_allocation.descriptorSetCount = 1;
        descriptor_allocation.pSetLayouts = &resources.descriptor_layout;
        VkDescriptorSet descriptor{};
        check(AllocateDescriptorSets(device, &descriptor_allocation, &descriptor),
              "vkAllocateDescriptorSets");
        std::array<VkDescriptorBufferInfo, 2> buffer_info{
            {{in.buffer, 0, input.size()}, {out.buffer, 0, output_bytes}}};
        std::array<VkWriteDescriptorSet, 2> writes{};
        for (std::uint32_t i = 0; i < 2; ++i) {
            writes[i] = structure<VkWriteDescriptorSet>(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
            writes[i].dstSet = descriptor;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].pBufferInfo = &buffer_info[i];
        }
        UpdateDescriptorSets(device, 2, writes.data(), 0, nullptr);
        auto command_pool =
            structure<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
        command_pool.queueFamilyIndex = infos[selected].compute_family;
        command_pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        check(CreateCommandPool(device, &command_pool, nullptr, &resources.command_pool),
              "vkCreateCommandPool");
        auto commands =
            structure<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
        commands.commandPool = resources.command_pool;
        commands.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commands.commandBufferCount = 1;
        VkCommandBuffer command{};
        check(AllocateCommandBuffers(device, &commands, &command), "vkAllocateCommandBuffers");
        auto begin =
            structure<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(BeginCommandBuffer(command, &begin), "vkBeginCommandBuffer");
        if (infos[selected].timestamp_bits) {
            auto queries =
                structure<VkQueryPoolCreateInfo>(VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO);
            queries.queryType = VK_QUERY_TYPE_TIMESTAMP;
            queries.queryCount = 2;
            check(CreateQueryPool(device, &queries, nullptr, &resources.queries),
                  "vkCreateQueryPool");
            CmdResetQueryPool(command, resources.queries, 0, 2);
            CmdWriteTimestamp(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, resources.queries, 0);
        }
        CmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, resources.pipeline);
        CmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, resources.pipeline_layout, 0,
                              1, &descriptor, 0, nullptr);
        CmdDispatch(command, workgroups, 1, 1);
        auto barrier = structure<VkMemoryBarrier2>(VK_STRUCTURE_TYPE_MEMORY_BARRIER_2);
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
        auto dependency = structure<VkDependencyInfo>(VK_STRUCTURE_TYPE_DEPENDENCY_INFO);
        dependency.memoryBarrierCount = 1;
        dependency.pMemoryBarriers = &barrier;
        CmdPipelineBarrier2(command, &dependency);
        if (resources.queries)
            CmdWriteTimestamp(command, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, resources.queries, 1);
        check(EndCommandBuffer(command), "vkEndCommandBuffer");
        auto fence_info = structure<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
        check(CreateFence(device, &fence_info, nullptr, &resources.fence), "vkCreateFence");
        auto submission = structure<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
        submission.commandBufferCount = 1;
        submission.pCommandBuffers = &command;
        check(QueueSubmit(queue, 1, &submission, resources.fence), "vkQueueSubmit");
        resources.submitted = true;
        check(WaitForFences(device, 1, &resources.fence, VK_TRUE, 30'000'000'000ULL),
              "vkWaitForFences (30 second deadline)");
        resources.submitted = false;
        out.synchronize(true);
        DispatchResult result;
        result.output.resize(output_bytes);
        std::memcpy(result.output.data(), out.mapped, output_bytes);
        if (resources.queries) {
            std::array<std::uint64_t, 2> timestamps{};
            check(GetQueryPoolResults(device, resources.queries, 0, 2, sizeof(timestamps),
                                      timestamps.data(), sizeof(std::uint64_t),
                                      VK_QUERY_RESULT_64_BIT),
                  "vkGetQueryPoolResults");
            auto bits = infos[selected].timestamp_bits;
            const auto mask = bits == 64 ? UINT64_MAX : ((std::uint64_t{1} << bits) - 1);
            result.gpu_milliseconds = static_cast<double>((timestamps[1] - timestamps[0]) & mask) *
                                      static_cast<double>(properties.limits.timestampPeriod) / 1e6;
            result.timestamp_available = true;
        }
        return result;
    }
};
#undef KERR_DEVICE_FUNCTIONS
} // namespace kerr::renderer::detail
