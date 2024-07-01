#pragma once
#include "EasyVKStart.h"
#include <array>
#include <cstdint>
#include <functional>
#include <vector>
#include <vulkan/vulkan_core.h>

/**
 * @brief 定义vulkan命名空间，之后会把Vulkan中一些基本对象的封装写在其中
 *
 */
namespace vulkan {

constexpr VkExtent2D defaultWindowSize = {1280, 720};  // 全局常量用constexpr修饰定义在类外

#define DestroyHandleBy(Func)                                                                                          \
    if (handle)                                                                                                        \
    {                                                                                                                  \
        Func(GraphicsBase::Base().Device(), handle, nullptr);                                                          \
        handle = VK_NULL_HANDLE;                                                                                       \
    }

#define MoveHandle                                                                                                     \
    handle       = other.handle;                                                                                       \
    other.handle = VK_NULL_HANDLE;

#define DefineHandleTypeOperator                                                                                       \
    operator decltype(handle)() const                                                                                  \
    {                                                                                                                  \
        return handle;                                                                                                 \
    }
/*
别把operator decltype(handle)写成operator auto（除非你是对继承深恶痛绝的的程序员），
到占位类型的类型转换函数不能在派生类中被using重设其访问级别。
*/
#define DefineAddressFunction                                                                                          \
    const decltype(handle)* Address() const                                                                            \
    {                                                                                                                  \
        return &handle;                                                                                                \
    }
/*
你可以把const decltype(handle)*简化成auto
*/

#define ExecuteOnce(...)                                                                                               \
    {                                                                                                                  \
        static bool executed = false;                                                                                  \
        if (executed) return __VA_ARGS__;                                                                              \
        executed = true;                                                                                               \
    }

constexpr struct outStream_t
{
    static std::stringstream ss;
    struct returnedStream_t
    {
        returnedStream_t operator<<(const std::string& string) const
        {
            ss << string;
            return {};
        }
    };
    returnedStream_t operator<<(const std::string& string) const
    {
        ss.clear();
        ss << string;
        return {};
    }
} outStream;
inline std::stringstream outStream_t::ss;
/*
使用方法：outStream_t<<"hello "<<"world!";
1. outStream_t<<"hello "执行这句话的时候把"hello "送进ss，返回一个returnedStream_t空对象
2. returnedStream_t<<"world!"执行这句话时继续向ss写入"world!"
*/

// inline const auto& outStream = std::cout;  // 不是constexpr，因为std::cout具有外部链接
/*
constexpr 的值再编译阶段就要确定
而std::cout被申明为extern变量，std::cout再链接阶段才会被检查
*/

#ifndef NDEBUG
#    define ENABLE_DEBUG_MESSENGER true
#else
#    define ENABLE_DEBUG_MESSENGER false
#endif

#define VK_RESULT_THROW
#ifdef VK_RESULT_THROW
/**
 * @brief
 *
 */
class result_t {
    VkResult result;

public:
    static std::function<void(VkResult)> callback_throw;
    result_t(VkResult result) : result(result) {}
    result_t(result_t&& other) noexcept : result(other.result) { other.result = VK_SUCCESS; }
    result_t& operator=(const VkResult& _result)
    {
        result = _result;
        return *this;
    }
    ~result_t() noexcept(false)
    {
        if (static_cast<uint32_t>(result) < VK_RESULT_MAX_ENUM) { return; }
        if (callback_throw) { callback_throw(result); }
        throw result;
    }
    operator VkResult()  // result_t 到 VkResult的隐式转换
    {
        VkResult result = this->result;
        this->result    = VK_SUCCESS;
        return result;
    }
};
inline std::function<void(VkResult)> result_t::callback_throw = nullptr;

#elif defined VK_RESULT_NODISCARD
/**
 * @brief
 *
 */
struct [[nodiscard]] result_t
{
    VkResult result;
    result_t(VkResult result) : result(result) {}
    operator VkResult() const { return result; }
};
// 在本文件中关闭弃值提醒（因为我懒得做处理）
#    pragma warning(disable : 4834)
#    pragma warning(disable : 6031)

#else
using result_t = VkResult;
#endif

class GraphicsBase {
    /* 单例类
    定义移动构造器（删除也算进行了定义），且没有定义复制构造器、复制赋值、移动赋值时，
    上述四个函数将全部无法使用，由此构成单例类。

    单例类对象是静态的，未设定初始值亦无构造函数的成员会被零初始化
    */

private:
    static GraphicsBase singleton;  // 静态单例类变量

    // General Vulkan Setting
    uint32_t apiVersion = VK_API_VERSION_1_0;

    // Vulkan Instance and Surface
    VkInstance                      instance{nullptr};
    std::vector<const char*>        instanceLayers;
    std::vector<const char*>        instanceExtensions;
    VkDebugUtilsMessengerEXT        debugUtilsMessenger{nullptr};
    VkSurfaceKHR                    surface{nullptr};
    std::vector<VkSurfaceFormatKHR> availableSurfaceFormats;

    // Physical Device
    VkPhysicalDevice                   physicalDevice{nullptr};
    VkPhysicalDeviceProperties         physicalDeviceProperties{};
    VkPhysicalDeviceMemoryProperties   physicalDeviceMemoryProperties{};
    std::vector<VkPhysicalDevice>      availablePhysicalDevices;
    std::vector<const char*>           deviceExtensions;
    std::vector<std::function<void()>> callbacks_createDevice;
    std::vector<std::function<void()>> callbacks_destroyDevice;

    // Logical Device
    VkDevice device{nullptr};
    uint32_t queueFamilyIndex_graphics     = VK_QUEUE_FAMILY_IGNORED;
    uint32_t queueFamilyIndex_presentation = VK_QUEUE_FAMILY_IGNORED;
    uint32_t queueFamilyIndex_compute      = VK_QUEUE_FAMILY_IGNORED;
    /*
    有效的索引从0开始，因此使用特殊值VK_QUEUE_FAMILY_IGNORED
    （为UINT32_MAX）为队列族索引的默认值
    */
    VkQueue queue_graphics{nullptr};
    VkQueue queue_presentation{nullptr};
    VkQueue queue_compute{nullptr};

    // Swap Chain
    VkSwapchainKHR           swapchain{nullptr};
    std::vector<VkImage>     swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkSwapchainCreateInfoKHR swapchainCreateInfo{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
    };  // 保存交换链的创建信息以便重建交换链
    std::vector<std::function<void()>> callbacks_createSwapchain;
    std::vector<std::function<void()>> callbacks_destroySwapchain;

    // rendering loop
    uint32_t currentImageIndex = 0;

    // ANCHOR - Private Function

    GraphicsBase()                               = default;
    GraphicsBase(const GraphicsBase&)            = delete;
    GraphicsBase& operator=(const GraphicsBase&) = delete;
    GraphicsBase& operator=(GraphicsBase&&)      = delete;
    GraphicsBase(GraphicsBase&&)                 = delete;
    ~GraphicsBase()
    {
        if (instance == nullptr) { return; }

        if (device != nullptr)
        {
            WaitIdle();
            if (swapchain != nullptr)
            {
                for (auto& i : callbacks_destroySwapchain) { i(); }
                for (auto& i : swapchainImageViews)
                {
                    if (i != nullptr) { vkDestroyImageView(device, i, nullptr); }
                }
                vkDestroySwapchainKHR(device, swapchain, nullptr);
            }
            for (auto& i : callbacks_destroyDevice) { i(); }
            vkDestroyDevice(device, nullptr);
        }

        if (surface != nullptr) { vkDestroySurfaceKHR(instance, surface, nullptr); }

        if (debugUtilsMessenger != nullptr)
        {
            auto DestroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (DestroyDebugUtilsMessenger != nullptr)
            {
                DestroyDebugUtilsMessenger(instance, debugUtilsMessenger, nullptr);
            }
        }

        vkDestroyInstance(instance, nullptr);
    }

    /**
     * @brief 用于向instanceLayers或instanceExtensions容器中添加字符串指针，并确保不重复
     *
     * @param container
     * @param name
     */
    static void AddLayerOrExtension(std::vector<const char*>& container, const char* name)
    {
        for (auto& i : container)
        {
            if (!strcmp(name, i)) { return; }  // 如果层/扩展的名称已在容器中，直接返回
        }
        container.push_back(name);
    }

    /**
     * @brief 用于检查物理设备是否满足所需的队列族类型
     *
     * @return VkResult 将对应的队列族索引返回到queueFamilyIndices，执行成功时直接将索引写入相应成员变量
     */
    result_t GetQueueFamilyIndices(VkPhysicalDevice         physicalDevice,
                                   bool                     enableGraphicsQueue,
                                   bool                     enableComputeQueue,
                                   std::array<uint32_t, 3>& queueFamilyIndices)
    {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        if (queueFamilyCount == 0U) { return VK_RESULT_MAX_ENUM; }
        std::vector<VkQueueFamilyProperties> queueFamilyPropertieses(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyPropertieses.data());

        auto& [ig, ip, ic] = queueFamilyIndices;  // 分别对应图形、呈现、计算
        ig = ip = ic = VK_QUEUE_FAMILY_IGNORED;

        /*
        当图像或缓冲区要被另一个队列族的队列使用时，可能需要做资源的队列族所有权转移
        （所谓可能是因为并非所有硬件都有此要求），所以尽量使用相同的队列族来省去这一麻烦。
        而优先确保图形和计算的队列族相同

        因为无论程序多么复杂，资源的所有权在图形和呈现队列之间转移的情况至多只会在渲染循环
        中发生，但在图形和计算队列之间转移的情况可能发生在所有你想做间接渲染（此处姑且不解释）
        的场合，而且图形和计算队列不同的话，计算和图形命令也不得不在各自的命令缓冲区中完成
        （而不是在同一个命令缓冲区中）。
        即，图形和计算的列族不同，比图形和呈现的队列族不同更费事。
        */
        for (uint32_t i = 0; i < queueFamilyCount; i++)
        {
            auto supportGraphics = static_cast<VkBool32>(
                enableGraphicsQueue && ((queueFamilyPropertieses[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U));
            auto supportCompute = static_cast<VkBool32>(
                enableComputeQueue && ((queueFamilyPropertieses[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0U));

            VkBool32 supportPresentation = 0U;
            if (surface != nullptr)
            {
                if (result_t result =
                        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &supportPresentation))
                {
                    LOG(ERROR) << "Failed to determine if the queue family supports presentation!\nError code: "
                               << static_cast<int32_t>(result);
                    return result;
                }
            }

            /*
            只有在找到支持的队列族时，程序才会写[ig, ip, ic]
            下面的判断条件时用来尽可能地保证[ig, ip, ic]的值相同
            */

            // 若某队列族同时支持图形操作和计算
            if ((supportGraphics != 0U) && (supportCompute != 0U))
            {
                // 若需要呈现，最好是三个队列族索引全部相同
                if (supportPresentation != 0U)
                {
                    ig = ip = ic = i;
                    break;
                }

                // 除非ig和ic都已取得且相同，否则将它们的值覆写为i，以确保两个队列族索引相同
                if (ig != ic || ig == VK_QUEUE_FAMILY_IGNORED) { ig = ic = i; }
                // 如果不需要呈现，那么已经可以break了
                if (surface == nullptr) { break; }
            }
            // 若任何一个队列族索引可以被取得但尚未被取得，将其值覆写为i
            if ((supportGraphics != 0U) && ig == VK_QUEUE_FAMILY_IGNORED) { ig = i; }
            if ((supportPresentation != 0U) && ip == VK_QUEUE_FAMILY_IGNORED) { ip = i; }
            if ((supportCompute != 0U) && ic == VK_QUEUE_FAMILY_IGNORED) { ic = i; }
        }

        // 若任何需要被取得的队列族索引尚未被取得，则函数执行失败
        if (ig == VK_QUEUE_FAMILY_IGNORED && enableGraphicsQueue ||
            ip == VK_QUEUE_FAMILY_IGNORED && (surface != nullptr) ||
            ic == VK_QUEUE_FAMILY_IGNORED && enableComputeQueue)
        {
            return VK_RESULT_MAX_ENUM;
        }
        queueFamilyIndex_graphics     = ig;
        queueFamilyIndex_presentation = ip;
        queueFamilyIndex_compute      = ic;
        return VK_SUCCESS;
    }

    /**
     * @brief 该函数被CreateSwapchain(...)和RecreateSwapchain()调用
     */
    result_t CreateSwapchain_Internal()
    {
        if (result_t result = vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain))
        {
            LOG(ERROR) << "Failed to create a swapchain!\nError code: " << static_cast<int32_t>(result);
            return result;
        }

        // 获取交换链图像
        uint32_t swapchainImageCount = 0;
        if (result_t result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr))
        {
            LOG(ERROR) << "Failed to get the count of swapchain images!\nError code: " << static_cast<int32_t>(result);
            return result;
        }
        swapchainImages.resize(swapchainImageCount);
        if (result_t result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data()))
        {
            LOG(ERROR) << "Failed to get swapchain images!\nError code: " << static_cast<int32_t>(result);
            return result;
        }

        // 为交换链图像创建Image View
        swapchainImageViews.resize(swapchainImageCount, nullptr);
        VkImageViewCreateInfo imageViewCreateInfo = {
            .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format   = swapchainCreateInfo.imageFormat,
            //.components = {},//四个成员皆为VK_COMPONENT_SWIZZLE_IDENTITY
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };
        for (size_t i = 0; i < swapchainImageCount; i++)
        {
            imageViewCreateInfo.image = swapchainImages[i];
            if (result_t result = vkCreateImageView(device, &imageViewCreateInfo, nullptr, &swapchainImageViews[i]))
            {
                LOG(ERROR) << "Failed to create a swapchain image view!\nError code: " << static_cast<int32_t>(result);
                return result;
            }
        }
        return VK_SUCCESS;
    }

public:
    /**
     * @brief 静态函数，该函数用于访问单例
     *
     * @return graphicsBase&
     */
    static GraphicsBase& Base() { return singleton; }

    /**
     * @brief 删除GraphicsBase单例类的实例
     */
    void Terminate()
    {
        this->~GraphicsBase();
        instance       = VK_NULL_HANDLE;
        physicalDevice = VK_NULL_HANDLE;
        device         = VK_NULL_HANDLE;
        surface        = VK_NULL_HANDLE;
        swapchain      = VK_NULL_HANDLE;
        swapchainImages.resize(0);
        swapchainImageViews.resize(0);
        swapchainCreateInfo = {};
        debugUtilsMessenger = VK_NULL_HANDLE;
        /*
        对于需要在运行过程中切换图形API的情况，我更推荐你单独写一个专用的函数，销毁其他
        对象但保留Vulkan实例，原因在于创建Vulkan实例是初始化过程中最耗时的一步（Release
         Bulid可能消耗1.4秒上下），而保留Vulkan实例并不会耗费你太多内存（30到40MB），
         方便来回切换。
        */
    }

    // ANCHOR - Getter

    uint32_t ApiVersion() const { return apiVersion; }

    VkInstance                      Instance() const { return instance; }
    const std::vector<const char*>& InstanceLayers() const { return instanceLayers; }
    const std::vector<const char*>& InstanceExtensions() const { return instanceExtensions; }
    VkSurfaceKHR                    Surface() const { return surface; }
    const VkFormat& AvailableSurfaceFormat(uint32_t index) const { return availableSurfaceFormats[index].format; }
    const VkColorSpaceKHR& AvailableSurfaceColorSpace(uint32_t index) const
    {
        return availableSurfaceFormats[index].colorSpace;
    }
    uint32_t AvailableSurfaceFormatCount() const { return static_cast<uint32_t>(availableSurfaceFormats.size()); }

    VkPhysicalDevice                        PhysicalDevice() const { return physicalDevice; }
    const VkPhysicalDeviceProperties&       PhysicalDeviceProperties() const { return physicalDeviceProperties; }
    const VkPhysicalDeviceMemoryProperties& PhysicalDeviceMemoryProperties() const
    {
        return physicalDeviceMemoryProperties;
    }
    VkPhysicalDevice AvailablePhysicalDevice(uint32_t index) const { return availablePhysicalDevices[index]; }
    uint32_t AvailablePhysicalDeviceCount() const { return static_cast<uint32_t>(availablePhysicalDevices.size()); }
    const std::vector<const char*>& DeviceExtensions() const { return deviceExtensions; }

    VkDevice Device() const { return device; }
    uint32_t QueueFamilyIndex_Graphics() const { return queueFamilyIndex_graphics; }
    uint32_t QueueFamilyIndex_Presentation() const { return queueFamilyIndex_presentation; }
    uint32_t QueueFamilyIndex_Compute() const { return queueFamilyIndex_compute; }
    VkQueue  Queue_Graphics() const { return queue_graphics; }
    VkQueue  Queue_Presentation() const { return queue_presentation; }
    VkQueue  Queue_Compute() const { return queue_compute; }

    VkSwapchainKHR Swapchain() const { return swapchain; }
    VkImage        SwapchainImage(uint32_t index) const { return swapchainImages[index]; }
    VkImageView    SwapchainImageView(uint32_t index) const { return swapchainImageViews[index]; }
    uint32_t       SwapchainImageCount() const { return static_cast<uint32_t>(swapchainImages.size()); }
    const VkSwapchainCreateInfoKHR& SwapchainCreateInfo() const { return swapchainCreateInfo; }

    uint32_t CurrentImageIndex() const { return currentImageIndex; }

    // ANCHOR - Setter

    void AddInstanceLayer(const char* layerName) { AddLayerOrExtension(instanceLayers, layerName); }
    void AddInstanceExtension(const char* extensionName) { AddLayerOrExtension(instanceExtensions, extensionName); }
    void InstanceLayers(const std::vector<const char*>& layerNames) { instanceLayers = layerNames; }
    void InstanceExtensions(const std::vector<const char*>& extensionNames) { instanceExtensions = extensionNames; }
    void Surface(VkSurfaceKHR surface)
    {
        if (this->surface == nullptr) { this->surface = surface; }
    }
    result_t SetSurfaceFormat(VkSurfaceFormatKHR surfaceFormat)
    {
        bool formatIsAvailable = false;
        if (surfaceFormat.format == 0)
        {
            // 如果格式未指定，只匹配色彩空间，图像格式有啥就用啥
            for (auto& i : availableSurfaceFormats)
            {
                if (i.colorSpace == surfaceFormat.colorSpace)
                {
                    swapchainCreateInfo.imageFormat     = i.format;
                    swapchainCreateInfo.imageColorSpace = i.colorSpace;
                    formatIsAvailable                   = true;
                    break;
                }
            }
        }
        else
        {
            // 否则匹配格式和色彩空间
            for (auto& i : availableSurfaceFormats)
            {
                if (i.format == surfaceFormat.format && i.colorSpace == surfaceFormat.colorSpace)
                {
                    swapchainCreateInfo.imageFormat     = i.format;
                    swapchainCreateInfo.imageColorSpace = i.colorSpace;
                    formatIsAvailable                   = true;
                    break;
                }
            }
        }
        // 如果没有符合的格式，恰好有个语义相符的错误代码
        if (!formatIsAvailable) { return VK_ERROR_FORMAT_NOT_SUPPORTED; }
        // 如果交换链已存在，调用RecreateSwapchain()重建交换链
        if (swapchain != nullptr) { return RecreateSwapchain(); }
        return VK_SUCCESS;
    }
    void AddDeviceExtension(const char* extensionName) { AddLayerOrExtension(deviceExtensions, extensionName); }
    void DeviceExtensions(const std::vector<const char*>& extensionNames) { deviceExtensions = extensionNames; }
    void AddCallback_CreateDevice(std::function<void()>& function) { callbacks_createDevice.push_back(function); }
    void AddCallback_DestroyDevice(std::function<void()>& function) { callbacks_destroyDevice.push_back(function); }
    void AddCallback_CreateSwapchain(std::function<void()>& function) { callbacks_createSwapchain.push_back(function); }
    void AddCallback_DestroySwapchain(std::function<void()>& function)
    {
        callbacks_destroySwapchain.push_back(function);
    }

    // ANCHOR - General Vulkan Setting

    /**
     * @brief 检查是否可以使用Vulkan的最新版本
     */
    result_t UseLatestApiVersion()
    {
        /*
        如果你想使用Vulkan的最新版本，须在创建Vulkan实例前，用vkEnumerateInstanceVersion(...)
        取得取得当前运行环境所支持的最新Vulkan版本，然而，Vulkan1.0版本中是不支持这个函数的，
        所以要先用vkGetInstanceProcAddr(...)尝试取得该函数的指针。
        */
        if (vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion") != nullptr)
        {
            return vkEnumerateInstanceVersion(&apiVersion);
        }
        return VK_SUCCESS;
    }

    // ANCHOR - Create Instance

    /**
     * @brief 创建Vulkan实例
     */
    result_t CreateInstance(VkInstanceCreateFlags flags = 0)
    {
        if constexpr (ENABLE_DEBUG_MESSENGER)
        {
            AddInstanceLayer("VK_LAYER_KHRONOS_validation");
            AddInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        VkApplicationInfo applicatianInfo = {
            .sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .apiVersion = apiVersion,
        };
        VkInstanceCreateInfo instanceCreateInfo = {
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .flags                   = flags,
            .pApplicationInfo        = &applicatianInfo,
            .enabledLayerCount       = static_cast<uint32_t>(instanceLayers.size()),
            .ppEnabledLayerNames     = instanceLayers.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(instanceExtensions.size()),
            .ppEnabledExtensionNames = instanceExtensions.data(),
        };
        if (result_t result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance))
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to create a vulkan instance!\nError code: "
                       << static_cast<int32_t>(result);
            return result;
        }

        // 成功创建Vulkan实例后，输出Vulkan版本
        LOG(INFO) << "[ graphicsBase ] INFO\nSuccessfully created a vulkan instance!\n";
        LOG(INFO) << "[ graphicsBase ] INFO\nVulkan API Version: " << VK_VERSION_MAJOR(apiVersion) << "."
                  << VK_VERSION_MINOR(apiVersion) << "." << VK_VERSION_PATCH(apiVersion) << "\n";

        if constexpr (ENABLE_DEBUG_MESSENGER) { CreateDebugMessenger(); }

        return VK_SUCCESS;
    }

    /**
     * @brief 将传入中不可用的层设置为nullptr，与原本保存的instanceLayers比对，确定哪些层和扩展不可用。
     *
     * @return VkResult 返回值则指示在获取可用层或扩展列表时是否发生错误。
     */
    result_t CheckInstanceLayers(std::vector<const char*>& layersToCheck)
    {
        uint32_t                       layerCount = 0;
        std::vector<VkLayerProperties> availableLayers;
        if (result_t result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr))
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to get the count of instance layers!";
            return result;
        }

        if (layerCount != 0U)
        {
            availableLayers.resize(layerCount);
            if (result_t result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()))
            {
                LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to enumerate instance layer properties!\nError code: "
                           << static_cast<int32_t>(result);
                return result;
            }

            for (auto& i : layersToCheck)
            {
                bool found = false;
                for (auto& j : availableLayers)
                {
                    if (strcmp(i, static_cast<char*>(j.layerName)) == 0)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found) { i = nullptr; }
            }
        }
        else { layersToCheck.resize(layersToCheck.size(), nullptr); }

        return VK_SUCCESS;
    }

    /**
     * @brief 将传入中不可用的扩展设置为nullptr，与原本保存的instanceExtensions比对，确定哪些层和扩展不可用。
     *
     * @return VkResult 返回值则指示在获取可用层或扩展列表时是否发生错误。
     */
    result_t CheckInstanceExtensions(std::vector<const char*>& extensionsToCheck, const char* layerName = nullptr) const
    {
        uint32_t                           extensionCount = 0;
        std::vector<VkExtensionProperties> availableExtensions;
        if (result_t result = vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, nullptr))
        {
            (layerName != nullptr) ?
                LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!\nLayer name:"
                           << layerName :
                LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to get the count of instance extensions!";
            return result;
        }

        if (extensionCount != 0U)
        {
            availableExtensions.resize(extensionCount);
            if (result_t result =
                    vkEnumerateInstanceExtensionProperties(layerName, &extensionCount, availableExtensions.data()))
            {
                LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to enumerate instance extension properties!\nError code: "
                           << static_cast<int32_t>(result);
                return result;
            }

            for (auto& i : extensionsToCheck)
            {
                bool found = false;
                for (auto& j : availableExtensions)
                {
                    if (strcmp(i, static_cast<char*>(j.extensionName)) == 0)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found) { i = nullptr; }
            }
        }
        else { extensionsToCheck.resize(extensionsToCheck.size(), nullptr); }

        return VK_SUCCESS;
    }

    /**
     * @brief Create a Debug Messenger object
     */
    result_t CreateDebugMessenger()
    {
        // 回调函数
        static PFN_vkDebugUtilsMessengerCallbackEXT DebugUtilsMessengerCallback =
            [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) -> VkBool32 {
            LOG(ERROR) << pCallbackData->pMessage;
            return VK_FALSE;
        };

        auto vkCreateDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (vkCreateDebugUtilsMessenger != nullptr)
        {
            VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity =
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = DebugUtilsMessengerCallback,
            };
            result_t result =
                vkCreateDebugUtilsMessenger(instance, &debugUtilsMessengerCreateInfo, nullptr, &debugUtilsMessenger);
            if (result != 0)
            {
                LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to create a debug messenger!\nError code: "
                           << static_cast<int32_t>(result);
            }
            return result;
        }
        LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to get the function pointer of vkCreateDebugUtilsMessengerEXT!";

        return VK_RESULT_MAX_ENUM;  // INT32_MAX，不会跟任何Vulkan函数的返回值重合
    }

    /**
     * @brief Get the Surface Formats
     */
    result_t GetSurfaceFormats()
    {
        uint32_t surfaceFormatCount = 0;
        if (result_t result =
                vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount, nullptr))
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to get the count of surface formats!\nError code: "
                       << static_cast<int32_t>(result);
            return result;
        }
        if (surfaceFormatCount == 0U)
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to find any supported surface format!";
            abort();
        }

        availableSurfaceFormats.resize(surfaceFormatCount);
        result_t result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &surfaceFormatCount,
                                                               availableSurfaceFormats.data());
        if (result != 0)
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to get surface formats!\nError code: "
                       << static_cast<int32_t>(result);
        }
        return result;
    }

    // ANCHOR - Search Physical Device and create Logical Device

    /**
     * @brief Get the Physical Devices object
     */
    result_t GetPhysicalDevices()
    {
        uint32_t deviceCount = 0;
        if (result_t result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr))
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to get the count of physical devices!\nError code: "
                       << static_cast<int32_t>(result);
            return result;
        }
        if (deviceCount == 0U)
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to find any physical device supports vulkan!";
            abort();
        }
        availablePhysicalDevices.resize(deviceCount);
        result_t result = vkEnumeratePhysicalDevices(instance, &deviceCount, availablePhysicalDevices.data());
        if (result != 0)
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: "
                       << static_cast<int32_t>(result);
        }
        return result;
    }

    /**
     * @brief 指定所用物理设备并调用GetQueueFamilyIndices(...)取得队列族索引
     *
     * REVIEW - 说实话，我没有看懂
     */
    result_t
    DeterminePhysicalDevice(uint32_t deviceIndex = 0, bool enableGraphicsQueue = true, bool enableComputeQueue = true)
    {
        // 定义一个特殊值用于标记一个队列族索引已被找过但未找到
        static constexpr uint32_t notFound = INT32_MAX;  //== VK_QUEUE_FAMILY_IGNORED & INT32_MAX

        struct queueFamilyIndexCombination  // 定义队列族索引组合的结构体
        {
            uint32_t graphics     = VK_QUEUE_FAMILY_IGNORED;
            uint32_t presentation = VK_QUEUE_FAMILY_IGNORED;
            uint32_t compute      = VK_QUEUE_FAMILY_IGNORED;
        };
        // queueFamilyIndexCombinations用于为各个物理设备保存一份队列族索引组合
        static std::vector<queueFamilyIndexCombination> queueFamilyIndexCombinations(availablePhysicalDevices.size());
        auto& [ig, ip, ic] = queueFamilyIndexCombinations[deviceIndex];

        // 如果有任何队列族索引已被找过但未找到，返回VK_RESULT_MAX_ENUM
        if ((ig == notFound && enableGraphicsQueue) || (ip == notFound && (surface != nullptr)) ||
            (ic == notFound && enableComputeQueue))
        {
            return VK_RESULT_MAX_ENUM;
        }

        // 如果有任何队列族索引应被获取但还未被找过
        if (ig == VK_QUEUE_FAMILY_IGNORED && enableGraphicsQueue || ip == VK_QUEUE_FAMILY_IGNORED && surface ||
            ic == VK_QUEUE_FAMILY_IGNORED && enableComputeQueue)
        {
            std::array<uint32_t, 3> indices = {};
            result_t result = GetQueueFamilyIndices(availablePhysicalDevices[deviceIndex], enableGraphicsQueue,
                                                    enableComputeQueue, indices);
            /*
            若GetQueueFamilyIndices(...)返回VK_SUCCESS或VK_RESULT_MAX_ENUM
            （vkGetPhysicalDeviceSurfaceSupportKHR(...)执行成功但没找齐所需队列族），
            说明对所需队列族索引已有结论，保存结果到queueFamilyIndexCombinations
            [deviceIndex]中相应变量应被获取的索引若仍为VK_QUEUE_FAMILY_IGNORED，
            说明未找到相应队列族，VK_QUEUE_FAMILY_IGNORED（~0u）与INT32_MAX
            做位与得到的数值等于notFound
            */
            if (result == VK_SUCCESS || result == VK_RESULT_MAX_ENUM)
            {
                if (enableGraphicsQueue) { ig = indices[0] & INT32_MAX; }
                if (surface != nullptr) { ip = indices[1] & INT32_MAX; }
                if (enableComputeQueue) { ic = indices[2] & INT32_MAX; }
            }
            if (result != 0) { return result; }  // 如果GetQueueFamilyIndices(...)执行失败，return
        }
        else
        {
            // 若以上两个if分支皆不执行，则说明所需的队列族索引皆已被获取，
            // 从queueFamilyIndexCombinations[deviceIndex]中取得索引
            queueFamilyIndex_graphics     = enableGraphicsQueue ? ig : VK_QUEUE_FAMILY_IGNORED;
            queueFamilyIndex_presentation = (surface != nullptr) ? ip : VK_QUEUE_FAMILY_IGNORED;
            queueFamilyIndex_compute      = enableComputeQueue ? ic : VK_QUEUE_FAMILY_IGNORED;
        }
        physicalDevice = availablePhysicalDevices[deviceIndex];
        return VK_SUCCESS;

        /**NOTE - 关于同时支持图形、计算和呈现地队列族
        因为严格按照Vulkan标准而言，无法断言是否一定能找到一个同时支持图形、计算、呈现的队列族。

        如果你程序的目标平台一定能跑DirectX12的图形程序（能装Win11的电脑皆在此列，
        嘛不是老古董电脑就没问题），那么你可以大胆假定一定会有这么一个队列族。
         */
    }

    /**
     * @brief 创建逻辑设备
     */
    result_t CreateDevice(VkDeviceCreateFlags flags = 0)
    {
        /**NOTE - 受保护的队列
        flags指定为VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT，说明该队列受保护。

        受保护的内存：Vulkan的设备内存对主机（指CPU一侧）可以是可见的，这就导致有能力
        的程序员可以通过一些手段，使得一个程序读写其他程序的设备内存，而受保护设备内存
        则被严格限制只能被设备写入，受保护队列则是能执行受保护操作的队列，这里头还有一
        些很麻烦的读写守则。

        受保护设备内存的主要应用场景是防止从内存中读取DRM内容（也就是防止盗版加密图像啦！
        DLsite和Fanza的新版电子书阅读器有这种特性）
        */

        // 填写队列信息
        float                                  queuePriority    = 1.0F;
        std::array<VkDeviceQueueCreateInfo, 3> queueCreateInfos = {};
        queueCreateInfos.fill(VkDeviceQueueCreateInfo{
            .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueCount       = 1,
            .pQueuePriorities = &queuePriority,
        });
        uint32_t queueCreateInfoCount = 0;
        if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED)
        {
            queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_graphics;
        }
        if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED &&
            queueFamilyIndex_presentation != queueFamilyIndex_graphics)
        {
            queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_presentation;
        }
        if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED &&
            queueFamilyIndex_compute != queueFamilyIndex_graphics &&
            queueFamilyIndex_compute != queueFamilyIndex_presentation)
        {
            queueCreateInfos[queueCreateInfoCount++].queueFamilyIndex = queueFamilyIndex_compute;
        }

        // 获取物理设备特性
        VkPhysicalDeviceFeatures physicalDeviceFeatures;  // 这里获取到的是Vulkan1.0的特性
        vkGetPhysicalDeviceFeatures(physicalDevice, &physicalDeviceFeatures);

        VkDeviceCreateInfo deviceCreateInfo = {
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .flags                   = flags,
            .queueCreateInfoCount    = queueCreateInfoCount,
            .pQueueCreateInfos       = queueCreateInfos.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures        = &physicalDeviceFeatures,
        };
        if (result_t result = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device))
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to create a vulkan logical device!";
            return result;
        }

        // 获取刚刚创建的设备队列
        if (queueFamilyIndex_graphics != VK_QUEUE_FAMILY_IGNORED)
        {
            vkGetDeviceQueue(device, queueFamilyIndex_graphics, 0, &queue_graphics);
        }
        if (queueFamilyIndex_presentation != VK_QUEUE_FAMILY_IGNORED)
        {
            vkGetDeviceQueue(device, queueFamilyIndex_presentation, 0, &queue_presentation);
        }
        if (queueFamilyIndex_compute != VK_QUEUE_FAMILY_IGNORED)
        {
            vkGetDeviceQueue(device, queueFamilyIndex_compute, 0, &queue_compute);
        }

        // 获取物理设备属性
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &physicalDeviceMemoryProperties);
        LOG(INFO) << "Physical Device Name: " << physicalDeviceProperties.deviceName;

        // TODO -  /*待Ch1-4填充*/

        return VK_SUCCESS;
    }

    result_t CheckDeviceExtensions(const std::vector<const char*>& extensionsToCheck,
                                   const char*                     layerName = nullptr) const
    {
        // TODO - /*待Ch1-3填充*/

        return VK_SUCCESS;
    }

    /**
     * @brief 该函数用于等待逻辑设备空闲
     */
    result_t WaitIdle() const
    {
        result_t result = vkDeviceWaitIdle(device);
        if (result != 0)
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to wait for the device to be idle!\nError code: "
                       << static_cast<int32_t>(result);
        }
        return result;
    }

    /**
     * @brief 该函数用于重建逻辑设备
     * @note System does not provide avability to change logical device
     */
    result_t RecreateDevice(VkDeviceCreateFlags flags = 0)
    {
        LOG(WARNING) << "System does not provide avability to change logical device";
        return VK_SUCCESS;
    }

    // ANCHOR - Create Swapchain

    /**
     * @brief 创建交换链
     */
    result_t CreateSwapchain(bool limitFrameRate = true, VkSwapchainCreateFlagsKHR flags = 0)
    {
        VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
        if (result_t result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities))
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: "
                       << static_cast<int32_t>(result);
            return result;
        }

        /* 配置交换链
        对于交换链图像的数量，尽量不要太少，避免阻塞（所谓阻塞，即当需要渲染一张新图像时，
        所有交换链图像不是正在被呈现引擎读取就是正在被渲染），但也不要太多，避免多余的显存开销。

        于是，如果容许的最大数量与最小数量不等，那么使用最小数量+1
        一般VkSurfaceCapabilitiesKHR::minImageCount会是2，多一张图像可以实现三重缓冲。
        */
        swapchainCreateInfo.minImageCount =
            surfaceCapabilities.minImageCount +
            static_cast<uint32_t>(surfaceCapabilities.maxImageCount > surfaceCapabilities.minImageCount);
        swapchainCreateInfo.imageExtent =
            (surfaceCapabilities.currentExtent.width == -1) ?
                VkExtent2D{glm::clamp(defaultWindowSize.width, surfaceCapabilities.minImageExtent.width,
                                      surfaceCapabilities.maxImageExtent.width),
                           glm::clamp(defaultWindowSize.height, surfaceCapabilities.minImageExtent.height,
                                      surfaceCapabilities.maxImageExtent.height)} :
                surfaceCapabilities.currentExtent;
        swapchainCreateInfo.imageArrayLayers = 1;
        swapchainCreateInfo.preTransform     = surfaceCapabilities.currentTransform;

        /* 处理交换链图像透明通道的方式
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
            表示不透明，每个像素的A通道值皆被视作1.f。
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR
            表示将颜色值视作预乘透明度（premultiplied alpha）形式。
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR
            表示将颜色值视作后乘透明度形式，或称直接透明度（straight alpha）形式。
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
            表示透明度的处理方式由应用程序的其他部分（Vulkan以外的部分）指定。

        在呈现时处理透明通道的方式未必能由Vulkan决定，而窗口系统可能会指定这一方式，
        这种情况下应当将swapchainCreateInfo.compositeAlpha设置为VK_COMPOSITE_ALPHA
        _INHERIT_BIT_KHR，该bit说明程序会用窗口系统相关的指令设定处理透明通道的方式，
        若程序没有执行这样的指令，该bit会确保使用程序的运行平台默认的透明度处理方式
        （通常为使用不透明背景）。

        因此，若surfaceCapabilities.supportedCompositeAlpha具有VK_COMPOSITE_ALPHA
        _INHERIT_BIT_KHR则优先将swapchainCreateInfo.compositeAlpha设置为VK_COMPOSITE
        _ALPHA_INHERIT_BIT_KHR，否则选择surfaceCapabilities.supportedCompositeAlpha
        中首个非0的bit
        */
        if (surfaceCapabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
        {
            swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        }
        else
        {
            for (size_t i = 0; i < 4; i++)
            {
                if ((surfaceCapabilities.supportedCompositeAlpha & (1 << i)) != 0U)
                {
                    swapchainCreateInfo.compositeAlpha =
                        VkCompositeAlphaFlagBitsKHR(surfaceCapabilities.supportedCompositeAlpha & (1 << i));
                    break;
                }
            }
        }

        /* 图像的用途
        图像必须被用作颜色附件（VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT），最好还能
        被用作数据传送的目标（VK_IMAGE_USAGE_TRANSFER_DST_BIT），这样就能用vkCmd
        ClearColorImage(...)清屏，还可以用vkCmdBlitImage(...)将图像直接搬运（而不必渲染）
        到屏幕上，此外还可能得被用作数据传送的来源（VK_IMAGE_USAGE_TRANSFER_SRC_BIT）
        ，比如实现窗口截屏。
        */
        swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        {
            swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        }
        if (surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        {
            swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        }
        else { LOG(WARNING) << "[ graphicsBase ] WARNING\nVK_IMAGE_USAGE_TRANSFER_DST_BIT isn't supported!"; }

        // 设置交换链的图像格式
        if (availableSurfaceFormats.empty())
        {
            if (result_t result = GetSurfaceFormats()) { return result; }
        }
        /*
        默认格式应当满足：RGBA四通道，每个通道8位的UNORM格式。8位表示每个通道有256个
        色阶，UNORM的U表示其底层数据是无符号整形，NORM表示在着色器中使用时，其数值
        会被转为[0, 1]区间内的小数（即被标准化）。这类格式最为普通，受到广泛支持且不会导致
        自动的色调映射/颜色校正。

        早在Vulkan1.0.14标准中便已明确：
        pSurfaceFormats must not contain an entry whose value for format is VK_FORMAT_UNDEFINED.
        */
        if (swapchainCreateInfo.imageFormat == 0)
        {
            if ((SetSurfaceFormat({VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}) != 0) &&
                (SetSurfaceFormat({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}) != 0))
            {
                // 如果找不到上述图像格式和色彩空间的组合，那只能有什么用什么，采用availableSurfaceFormats中的第一组
                swapchainCreateInfo.imageFormat     = availableSurfaceFormats[0].format;
                swapchainCreateInfo.imageColorSpace = availableSurfaceFormats[0].colorSpace;
                LOG(WARNING) << "[ graphicsBase ] WARNING\nFailed to select a four-component UNORM surface format!";
            }
        }

        /* 指定呈现模式
        ![](https://easyvulkan.github.io/_images/IMMEDIATE.png)
        ![](https://easyvulkan.github.io/_images/FIFO.png)
        ![](https://easyvulkan.github.io/_images/MAILBOX.png)

        在不需要限制帧率时应当选择VK_PRESENT_MODE_MAILBOX_KHR，
        需要限制帧率使其最大不超过屏幕刷新率时应选择VK_PRESENT_MODE_FIFO_KHR
        */
        uint32_t surfacePresentModeCount = 0;
        if (result_t result =
                vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &surfacePresentModeCount, nullptr))
        {
            LOG(WARNING) << "[ graphicsBase ] WARNING\nFailed to get the count of surface present modes!";
            return result;
        }
        if (surfacePresentModeCount == 0U)
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to find any surface present mode!";
            abort();
        }
        std::vector<VkPresentModeKHR> surfacePresentModes(surfacePresentModeCount);
        if (result_t result = vkGetPhysicalDeviceSurfacePresentModesKHR(
                physicalDevice, surface, &surfacePresentModeCount, surfacePresentModes.data()))
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to get surface present modes!\nError code: "
                       << static_cast<int32_t>(result);
            return result;
        }
        swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        if (!limitFrameRate)
        {
            for (size_t i = 0; i < surfacePresentModeCount; i++)
            {
                if (surfacePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    swapchainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                    break;
                }
            }
        }

        // 填写剩余的参数
        swapchainCreateInfo.surface          = surface;
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.clipped          = VK_TRUE;

        // 创建交换链
        if (result_t result = CreateSwapchain_Internal()) { return result; }
        // 执行回调函数
        for (auto& i : callbacks_createSwapchain) { i(); }

        return VK_SUCCESS;
    }

    /**
     * @brief 重建交换链
     */
    result_t RecreateSwapchain()
    {
        VkSurfaceCapabilitiesKHR surfaceCapabilities = {};
        if (result_t result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfaceCapabilities))
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: "
                       << static_cast<int32_t>(result);
            return result;
        }
        if (surfaceCapabilities.currentExtent.width == 0 || surfaceCapabilities.currentExtent.height == 0)
        {
            return VK_SUBOPTIMAL_KHR;
            /*
            如果窗口显示区域的长或宽为0（通常发生在最小化到任务栏窗口时），不重建交换链
            （若所创建图像的大小为0，特定显卡驱动可能报错），留待窗口大小非0时重建，函数
            执行视为不完全成功返回VK_SUBOPTIMAL_KHR
            */
        }
        swapchainCreateInfo.imageExtent  = surfaceCapabilities.currentExtent;
        swapchainCreateInfo.oldSwapchain = swapchain;  // 有利于重用一些资源

        /*
        在重建交换链前，须确保程序没有正在使用旧的交换链，在渲染循环（rendering loop）中，
        交换链的使用并没有明确的停滞时机，因此需要等待逻辑设备闲置，或者更精细点，等待图形
        和呈现队列闲置（交换链图像被图形队列写入，被呈现队列读取），这么一来计算队列就可以
        在重建交换链时继续其任务
        */
        result_t result = vkQueueWaitIdle(queue_graphics);
        // 仅在等待图形队列成功，且图形与呈现所用队列不同时等待呈现队列
        if ((result == VK_SUCCESS) && queue_graphics != queue_presentation)
        {
            result = vkQueueWaitIdle(queue_presentation);
        }
        if (result != VK_SUCCESS)
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to wait for the queue to be idle!\nError code: "
                       << static_cast<int32_t>(result);
            return result;
        }

        // TODO - /*待后续填充*/

        // 用vkDestroyImageView(...)销毁旧有的image view
        for (auto& i : swapchainImageViews)
        {
            if (i != nullptr) { vkDestroyImageView(device, i, nullptr); }
        }
        swapchainImageViews.resize(0);
        /*
        值得注意的是，这里只销毁了image view，而没销毁其对应的交换链图像，这是因为在销毁
        旧交换链时会被一并销毁交换链图像。而尽管图形和呈现队列都没有在使用旧图像，无法确
        保呈现引擎在CPU上做的操作或不会使用旧图像，因此销毁旧交换链的时机需要延后，之后
        会在Ch2-1 Rendering Loop中进行相应处理。
        */

        // 销毁旧交换链相关对象
        for (auto& i : callbacks_destroySwapchain) { i(); }
        for (auto& i : swapchainImageViews)
        {
            if (i != nullptr) { vkDestroyImageView(device, i, nullptr); }
        }
        swapchainImageViews.resize(0);

        // 创建新交换链及与之相关的对象
        if (result_t result = CreateSwapchain_Internal()) { return result; }
        for (auto& i : callbacks_createSwapchain) { i(); }
        return VK_SUCCESS;
    }

    // ANCHOR - rendering loop

    /**
     * @brief 用于获取交换链图像索引到currentImageIndex，
     * 以及在需要重建交换链时调用RecreateSwapchain()、重建交换链后销毁旧交换链
     */
    result_t SwapImage(VkSemaphore semaphore_imageIsAvailable)
    {
        // 销毁旧交换链（若存在）
        if ((swapchainCreateInfo.oldSwapchain != nullptr) && swapchainCreateInfo.oldSwapchain != swapchain)
        {
            vkDestroySwapchainKHR(device, swapchainCreateInfo.oldSwapchain, nullptr);
            swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
        }
        /*
        如之前在Ch1-4 创建交换链中所说，不能在重建交换链后立刻销毁旧交换链。SwapImage(...)
        中的逻辑是，若在当前帧重建交换链，那么在下一帧销毁交换链（这也是为什么重建交换链后
        使用了break来再次执行while，而非递归地调用SwapImage(...)）。
        */

        // 获取交换链图像索引
        while (VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, semaphore_imageIsAvailable,
                                                       VK_NULL_HANDLE, &currentImageIndex))
        {
            // 如果result的值为vk_success，会直接退出循环
            switch (result)
            {
                case VK_SUBOPTIMAL_KHR: {
                    /* REVIEW - 看不懂
                   返回VK_SUBOPTIMAL_KHR的情况下，信号量可能会被置位，这会导致验证层在第二次
                   调用vkAcquireNextImageKHR(...)时报错，但并不影响后续执行逻辑：因为是新创建的
                   交换链图像，不需要等呈现引擎把它吐出来，已经置位的信号量就这么保留置位即可
                   （要消掉这时的报错，代码改起来很烦，就不改了）。
                   */
                }
                case VK_ERROR_OUT_OF_DATE_KHR: {
                    if (VkResult result = RecreateSwapchain()) { return result; }
                    break;  // 注意重建交换链后仍需要获取图像，通过break递归，再次执行while的条件判定语句
                }
                default: {
                    LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to acquire the next image!\nError code: "
                               << static_cast<int32_t>(result);
                    return result;
                }
            }
        }
        return VK_SUCCESS;
    }

    /**
     * @brief 用于将命令缓冲区提交到用于图形的队列
     */
    result_t SubmitCommandBuffer_Graphics(VkSubmitInfo& submitInfo, VkFence fence = VK_NULL_HANDLE) const
    {
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkResult result  = vkQueueSubmit(queue_graphics, 1, &submitInfo, fence);
        if (result != 0)
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: "
                       << static_cast<int32_t>(result);
        }
        return result;
    }

    /**
     * @brief 用于在渲染循环中将命令缓冲区提交到图形队列的常见情形
     */
    result_t SubmitCommandBuffer_Graphics(
        VkCommandBuffer      commandBuffer,
        VkSemaphore          semaphore_imageIsAvailable    = VK_NULL_HANDLE,
        VkSemaphore          semaphore_renderingIsOver     = VK_NULL_HANDLE,
        VkFence              fence                         = VK_NULL_HANDLE,
        VkPipelineStageFlags waitDstStage_imageIsAvailable = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) const
    {
        VkSubmitInfo submitInfo = {
            .commandBufferCount = 1,
            .pCommandBuffers    = &commandBuffer,
        };
        if (semaphore_imageIsAvailable != nullptr)
        {
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores    = &semaphore_imageIsAvailable,
            submitInfo.pWaitDstStageMask  = &waitDstStage_imageIsAvailable;
        }
        if (semaphore_renderingIsOver != nullptr)
        {
            submitInfo.signalSemaphoreCount = 1, submitInfo.pSignalSemaphores = &semaphore_renderingIsOver;
        }
        return SubmitCommandBuffer_Graphics(submitInfo, fence);
    }

    /**
     * @brief 用于将命令缓冲区提交到用于图形的队列，且只使用栅栏的常见情形
     */
    result_t SubmitCommandBuffer_Graphics(VkCommandBuffer commandBuffer, VkFence fence = VK_NULL_HANDLE) const
    {
        VkSubmitInfo submitInfo = {
            .commandBufferCount = 1,
            .pCommandBuffers    = &commandBuffer,
        };
        return SubmitCommandBuffer_Graphics(submitInfo, fence);
    }

    /**
     * @brief 用于将命令缓冲区提交到用于计算的队列
     */
    result_t SubmitCommandBuffer_Compute(VkSubmitInfo& submitInfo, VkFence fence = VK_NULL_HANDLE) const
    {
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkResult result  = vkQueueSubmit(queue_compute, 1, &submitInfo, fence);
        if (result != 0)
        {
            LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to submit the command buffer!\nError code: "
                       << static_cast<int32_t>(result);
        }
        return result;
    }

    /**
     * @brief 用于将命令缓冲区提交到用于计算的队列，且只使用栅栏的常见情形
     */
    result_t SubmitCommandBuffer_Compute(VkCommandBuffer commandBuffer, VkFence fence = VK_NULL_HANDLE) const
    {
        VkSubmitInfo submitInfo = {.commandBufferCount = 1, .pCommandBuffers = &commandBuffer};
        return SubmitCommandBuffer_Compute(submitInfo, fence);
    }

    /*
    在渲染循环中将命令缓冲区提交到图形队列时，若不需要做深度或模板测试，最迟可以在
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT阶段等待获取到交换链图像，
    渲染结果在该阶段被写入到交换链图像。

    包含图形命令的命令缓冲区可能不带任何信号量（在渲染循环之外完全有理由这么做），
    而只包含数据转移命令的话通常也不会带任何信号量（就使用情形而言多是在“加载”这一环节中），
    数据转移命令可以被提交给图形或计算队列。
    */

    /**
     * @brief 用于在渲染循环中呈现图像的常见情形
     */
    result_t PresentImage(VkPresentInfoKHR& presentInfo)
    {
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        switch (VkResult result = vkQueuePresentKHR(queue_presentation, &presentInfo))
        {
            case VK_SUCCESS: {
                return VK_SUCCESS;
            }
            case VK_SUBOPTIMAL_KHR: {
            }
            case VK_ERROR_OUT_OF_DATE_KHR: {
                /*
                跟SwapImage(...)中的情形不同，PresentImage(...)在重建交换链后直接返回，这会导致必定丢1帧。
                要保留这1帧的话，得在重建交换链后再获取交换链图像、呈现图像，考虑到获取交换链图像时还要
                创建临时的同步对象，代码会写得比较麻烦，按下不表。
                */
                return RecreateSwapchain();
            }
            default: {
                LOG(ERROR) << "[ graphicsBase ] ERROR\nFailed to queue the image for presentation!\nError code: {}\n"
                           << static_cast<int32_t>(result);
                return result;
            }
        }
    }

    /**
     * @brief 用于在渲染循环中呈现图像的常见情形
     */
    result_t PresentImage(VkSemaphore semaphore_renderingIsOver = VK_NULL_HANDLE)
    {
        VkPresentInfoKHR presentInfo = {
            .swapchainCount = 1,
            .pSwapchains    = &swapchain,
            .pImageIndices  = &currentImageIndex,
        };
        if (semaphore_renderingIsOver != nullptr)
        {
            presentInfo.waitSemaphoreCount = 1;
            presentInfo.pWaitSemaphores    = &semaphore_renderingIsOver;
        }
        return PresentImage(presentInfo);
    }
};
inline GraphicsBase GraphicsBase::singleton;

/**
 * @brief
 *
 */
class fence {
    VkFence handle = VK_NULL_HANDLE;

public:
    // fence() = default;
    fence(VkFenceCreateInfo& createInfo) { Create(createInfo); }
    // 默认构造器创建未置位的栅栏
    fence(VkFenceCreateFlags flags = 0) { Create(flags); }
    fence(fence&& other) noexcept { MoveHandle; }
    ~fence() { DestroyHandleBy(vkDestroyFence); }

    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;

    // Const Function
    result_t Wait() const
    {
        VkResult result = vkWaitForFences(GraphicsBase::Base().Device(), 1, &handle, false, UINT64_MAX);
        if (result != 0)
        {
            LOG(ERROR) << "[ fence ] ERROR\nFailed to wait for the fence!\nError code: "
                       << static_cast<int32_t>(result);
        }
        return result;
    }
    result_t Reset() const
    {
        VkResult result = vkResetFences(GraphicsBase::Base().Device(), 1, &handle);
        if (result != 0)
        {
            LOG(ERROR) << "[ fence ] ERROR\nFailed to reset the fence!\nError code: " << static_cast<int32_t>(result);
        }
        return result;
    }
    // 因为“等待后立刻重置”的情形经常出现，定义此函数
    result_t WaitAndReset() const
    {
        VkResult result = Wait();
        (result != 0) || ((result = Reset()) != 0);
        /*
        检擦result的结果，如果是成功（0），则fence需要手动重置
        否则，不需要重置
        */
        return result;
    }
    result_t Status() const
    {
        VkResult result = vkGetFenceStatus(GraphicsBase::Base().Device(), handle);
        if (result < 0)  // vkGetFenceStatus(...)成功时有两种结果，所以不能仅仅判断result是否非0
        {
            LOG(ERROR) << "[ fence ] ERROR\nFailed to get the status of the fence!\nError code: "
                       << static_cast<int32_t>(result);
        }
        return result;
    }

    // Non-const Function
    result_t Create(VkFenceCreateInfo& createInfo)
    {
        createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkResult result  = vkCreateFence(GraphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result != 0)
        {
            LOG(ERROR) << "[ fence ] ERROR\nFailed to create a fence!\nError code: " << static_cast<int32_t>(result);
        }
        return result;
    }
    result_t Create(VkFenceCreateFlags flags = 0)
    {
        VkFenceCreateInfo createInfo = {.flags = flags};
        return Create(createInfo);
    }
};

/**
 * @brief
 *
 */
class semaphore {
    VkSemaphore handle = VK_NULL_HANDLE;

public:
    // semaphore() = default;
    semaphore(VkSemaphoreCreateInfo& createInfo) { Create(createInfo); }
    // 默认构造器创建未置位的信号量
    semaphore(/*VkSemaphoreCreateFlags flags*/) { Create(); }
    semaphore(semaphore&& other) noexcept { MoveHandle; }
    ~semaphore() { DestroyHandleBy(vkDestroySemaphore); }

    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;

    // Non-const Function
    result_t Create(VkSemaphoreCreateInfo& createInfo)
    {
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkResult result  = vkCreateSemaphore(GraphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result != 0)
        {
            LOG(ERROR) << "[ semaphore ] ERROR\nFailed to create a semaphore!\nError code: "
                       << static_cast<int32_t>(result);
        }
        return result;
    }
    result_t Create(/*VkSemaphoreCreateFlags flags*/)
    {
        VkSemaphoreCreateInfo createInfo = {};
        return Create(createInfo);
    }
};

/**
 * @brief
 *
 */
class commandBuffer {
    friend class commandPool;  // 封装命令池的commandPool类负责分配和释放命令缓冲区，需要让其能访问私有成员handle
    VkCommandBuffer handle = VK_NULL_HANDLE;

public:
    commandBuffer() = default;
    commandBuffer(commandBuffer&& other) noexcept { MoveHandle; }
    // 因释放命令缓冲区的函数被我定义在封装命令池的commandPool类中，没析构器

    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;

    // Const Function
    // 这里没给inheritanceInfo设定默认参数，因为C++标准中规定对空指针解引用是未定义行为（尽管运行期不必发生，且至少MSVC编译器允许这种代码），而我又一定要传引用而非指针，因而形成了两个Begin(...)
    result_t Begin(VkCommandBufferUsageFlags usageFlags, VkCommandBufferInheritanceInfo& inheritanceInfo) const
    {
        inheritanceInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        VkCommandBufferBeginInfo beginInfo = {
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags            = usageFlags,
            .pInheritanceInfo = &inheritanceInfo,
        };
        VkResult result = vkBeginCommandBuffer(handle, &beginInfo);
        if (result != 0)
        {
            LOG(ERROR) << "[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: {} "
                       << static_cast<int32_t>(result);
        }
        return result;
    }
    result_t Begin(VkCommandBufferUsageFlags usageFlags = 0) const
    {
        VkCommandBufferBeginInfo beginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = usageFlags,
        };
        VkResult result = vkBeginCommandBuffer(handle, &beginInfo);
        if (result != 0)
        {
            LOG(ERROR) << "[ commandBuffer ] ERROR\nFailed to begin a command buffer!\nError code: {} "
                       << static_cast<int32_t>(result);
        }
        return result;
    }
    result_t End() const
    {
        VkResult result = vkEndCommandBuffer(handle);
        if (result != 0)
        {
            LOG(ERROR) << "[ commandBuffer ] ERROR\nFailed to end a command buffer!\nError code: {} "
                       << static_cast<int32_t>(result);
        }
        return result;
    }
};

/**
 * @brief
 *
 */
class commandPool {
    VkCommandPool handle = VK_NULL_HANDLE;

public:
    commandPool() = default;
    commandPool(VkCommandPoolCreateInfo& createInfo) { Create(createInfo); }
    commandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0) { Create(queueFamilyIndex, flags); }
    commandPool(commandPool&& other) noexcept { MoveHandle; }
    ~commandPool() { DestroyHandleBy(vkDestroyCommandPool); }

    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;

    // Const Function
    result_t AllocateBuffers(arrayRef<VkCommandBuffer> buffers,
                             VkCommandBufferLevel      level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const
    {
        VkCommandBufferAllocateInfo allocateInfo = {.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                    .commandPool = handle,
                                                    .level       = level,
                                                    .commandBufferCount = static_cast<uint32_t>(buffers.Count())};
        VkResult result = vkAllocateCommandBuffers(GraphicsBase::Base().Device(), &allocateInfo, buffers.Pointer());
        if (result != 0)
        {
            LOG(ERROR) << "[ commandPool ] ERROR\nFailed to allocate command buffers!\nError code: {}\n"
                       << static_cast<int32_t>(result);
        }
        return result;
    }
    result_t AllocateBuffers(arrayRef<commandBuffer> buffers,
                             VkCommandBufferLevel    level = VK_COMMAND_BUFFER_LEVEL_PRIMARY) const
    {
        return AllocateBuffers({&buffers[0].handle, buffers.Count()}, level);
    }
    void FreeBuffers(arrayRef<VkCommandBuffer> buffers) const
    {
        vkFreeCommandBuffers(GraphicsBase::Base().Device(), handle, buffers.Count(), buffers.Pointer());
        memset(buffers.Pointer(), 0, buffers.Count() * sizeof(VkCommandBuffer));
    }
    void FreeBuffers(arrayRef<commandBuffer> buffers) const { FreeBuffers({&buffers[0].handle, buffers.Count()}); }
    /*
    arrayRef<commandBuffer> buffers这个类型的复制构造很省资源，各种构造都挺省的
    */

    // Non-const Function
    result_t Create(VkCommandPoolCreateInfo& createInfo)
    {
        createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        VkResult result  = vkCreateCommandPool(GraphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result != 0)
        {
            LOG(ERROR) << "[ commandPool ] ERROR\nFailed to create a command pool!\nError code: " << int32_t(result)
                       << "\n";
        }
        return result;
    }
    result_t Create(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0)
    {
        VkCommandPoolCreateInfo createInfo = {.flags = flags, .queueFamilyIndex = queueFamilyIndex};
        return Create(createInfo);
    }
};

/**
 * @brief
 *
 */
class renderPass {
    VkRenderPass handle = VK_NULL_HANDLE;

public:
    renderPass() = default;
    renderPass(VkRenderPassCreateInfo& createInfo) { Create(createInfo); }
    renderPass(renderPass&& other) noexcept { MoveHandle; }
    ~renderPass() { DestroyHandleBy(vkDestroyRenderPass); }

    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;

    // Const Function
    void CmdBegin(VkCommandBuffer        commandBuffer,
                  VkRenderPassBeginInfo& beginInfo,
                  VkSubpassContents      subpassContents = VK_SUBPASS_CONTENTS_INLINE) const
    {
        beginInfo.sType      = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        beginInfo.renderPass = handle;
        vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
    }
    void CmdBegin(VkCommandBuffer              commandBuffer,
                  VkFramebuffer                framebuffer,
                  VkRect2D                     renderArea,
                  arrayRef<const VkClearValue> clearValues     = {},
                  VkSubpassContents            subpassContents = VK_SUBPASS_CONTENTS_INLINE) const
    {
        VkRenderPassBeginInfo beginInfo = {
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass      = handle,
            .framebuffer     = framebuffer,
            .renderArea      = renderArea,
            .clearValueCount = uint32_t(clearValues.Count()),
            .pClearValues    = clearValues.Pointer(),
        };
        vkCmdBeginRenderPass(commandBuffer, &beginInfo, subpassContents);
    }
    void CmdNext(VkCommandBuffer commandBuffer, VkSubpassContents subpassContents = VK_SUBPASS_CONTENTS_INLINE) const
    {
        vkCmdNextSubpass(commandBuffer, subpassContents);
    }
    void CmdEnd(VkCommandBuffer commandBuffer) const { vkCmdEndRenderPass(commandBuffer); }

    // Non-const Function
    result_t Create(VkRenderPassCreateInfo& createInfo)
    {
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        VkResult result  = vkCreateRenderPass(GraphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result != 0)
        {
            LOG(ERROR) << "[ renderPass ] ERROR\nFailed to create a render pass!\nError code: {}\n"
                       << static_cast<int32_t>(result);
        }
        return result;
    }
};

/**
 * @brief
 *
 */
class framebuffer {
    VkFramebuffer handle = VK_NULL_HANDLE;

public:
    framebuffer() = default;
    framebuffer(VkFramebufferCreateInfo& createInfo) { Create(createInfo); }
    framebuffer(framebuffer&& other) noexcept { MoveHandle; }
    ~framebuffer() { DestroyHandleBy(vkDestroyFramebuffer); }

    // Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;

    // Non-const Function
    result_t Create(VkFramebufferCreateInfo& createInfo)
    {
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        VkResult result  = vkCreateFramebuffer(GraphicsBase::Base().Device(), &createInfo, nullptr, &handle);
        if (result != 0)
        {
            LOG(ERROR) << "[ framebuffer ] ERROR\nFailed to create a framebuffer!\nError code: {}\n",
                static_cast<int32_t>(result);
        }
        return result;
    }
};

};  // namespace vulkan