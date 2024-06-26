#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <functional>  //functional头文件用于资源管理
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>  //报错
#include <string>
#include <type_traits>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>  //GLFW库会自动包含Vulkan库的头文件
#include <glog/logging.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

// constants
static const uint32_t                 screen_width     = 1920;
static const uint32_t                 screen_height    = 1080;
static const std::vector<const char*> validationLayers = {"VK_LAYER_KHRONOS_validation"};
static const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

class HelloTriangleApplication {
public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    struct QueueFamilyIndices
    {
        int graphicsFamily = -1;
        int presentFamily  = -1;

        [[nodiscard]] bool isComplete() const
        {
            return graphicsFamily >= 0 && presentFamily >= 0;
        }
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR        capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;
    };

    GLFWwindow*              window;
    VkInstance               instance;
    VkDebugUtilsMessengerEXT callback;
    VkPhysicalDevice         physicalDevice = VK_NULL_HANDLE;  // vkinstance清除时自动清除
    VkSurfaceKHR             surface;
    VkDevice                 device;
    VkQueue                  graphicsQueue;  // 逻辑设备的队列会在逻辑设备清除时自动被清除
    VkQueue                  presentQueue;

    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // 阻止GLFW它创建OpenGL上下文
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);    // 禁止窗口大小改变
        window = glfwCreateWindow(screen_width, screen_height, "hello_triangle", nullptr, nullptr);
    }

    void initVulkan()
    {
        createInstance();
        setupDebugCallback();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
    }

    void mainLoop()
    {
        while (glfwWindowShouldClose(window) == 0)
        {
            glfwPollEvents();
        }
    }

    void cleanup()
    {
        // cleanup Vulkan
        vkDestroyDevice(device, nullptr);
        if (enableValidationLayers)  // 清除VK校验层
        {
            auto DestroyDebugUtilsMessengerEXT = [](VkInstance instance, VkDebugUtilsMessengerEXT callback,
                                                    const VkAllocationCallbacks* pAllocator) -> void {
                auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                    instance, "vkDestroyDebugUtilsMessengerEXT");
                if (func != nullptr) { func(instance, callback, pAllocator); }
            };
            DestroyDebugUtilsMessengerEXT(instance, callback, nullptr);
        }
        vkDestroySurfaceKHR(instance, surface, nullptr);  // GLFW并没有提供清除表面的函数
        vkDestroyInstance(instance, nullptr);             // 清除VK实例

        // cleanup GLFW
        glfwDestroyWindow(window);
        window = nullptr;
        glfwTerminate();
    }

    void createInstance()
    {
        // ANCHOR - 创建Vulkan实例
        VkApplicationInfo appInfo = {
            VK_STRUCTURE_TYPE_APPLICATION_INFO,  // sType
            nullptr,                             // pNext
            "Hello Triangle",                    // pApplicationName
            VK_MAKE_VERSION(1, 0, 0),            // applicationVersion
            "Vulkan Tutorial",                   // pEngineName
            VK_MAKE_VERSION(1, 0, 0),            // engineVersion
            VK_API_VERSION_1_0                   // apiVersion
        };

        // 检查对校验层的支持
        if (enableValidationLayers && !checkValidationLayerSupport())
        {
            LOG(ERROR) << "validation layers requested, but not available!";
            throw std::runtime_error("validation layers requested, but not available!");
        }

        // 打印Vulkan扩展的信息
        {
            uint32_t extensionCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> extensions(extensionCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
            LOG(INFO) << "available extensions:";
            for (const auto& extension : extensions)
            {
                LOG(INFO) << "\t" << extension.extensionName;
            }
        }

        /** NOTE - 全局扩展以及，校验层所需的扩展
        校验层功能：
            - 检测参数值是否合法
            - 追踪对象的创建和清除操作，发现资源泄漏问题
            - 追踪调用来自的线程，检测是否线程安全。
            - 将API调用和调用的参数写入日志
            - 追踪API调用进行分析和回放

        校验层只能用于安装了它们的系统，比如LunarG的校验层只可以在安装了Vulkan SDK的PC上使用。

        Vulkan可以使用两种不同类型的校验层：实例校验层和设备校验层。
        实例校验层只检查和全局Vulkan对象相关的调用，比如Vulkan实例。
        设备校验层只检查和特定GPU相关的调用。设备校验层现在已经不推荐使用。

        仅仅启用校验层并没有任何用处，我们不能得到任何有用的调试信息。
        为了获得调试信息，我们需要使用VK_EXT_debug_utils扩展，设置回调函数来接受调试信息。
        */
        auto extensions = getRequiredExtensions();

        VkInstanceCreateInfo createInfo = {
            VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,                                         // sType
            nullptr,                                                                        // pNext
            0,                                                                              // flags
            &appInfo,                                                                       // pApplicationInfo
            (enableValidationLayers) ? static_cast<uint32_t>(validationLayers.size()) : 0,  // enabledLayerCount
            (enableValidationLayers) ? validationLayers.data() : nullptr,                   // ppEnabledLayerNames
            static_cast<uint32_t>(extensions->size()),                                      // enabledExtensionCount
            extensions->data()                                                              // ppEnabledExtensionNames
        };  // 告诉Vulkan的驱动程序需要使用的全局扩展和校验层

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            LOG(ERROR) << "failed to create instance!";
            throw std::runtime_error("failed to create instance!");
        }
    }

    static bool checkValidationLayerSupport()
    {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers)
        {
            bool layerFound = false;
            for (const auto& layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }
            if (!layerFound) { return false; }
        }
        return true;
    }

    static std::shared_ptr<std::vector<const char*>> getRequiredExtensions()
    {
        uint32_t     glfwExtensionCount = 0;
        const char** glfwExtensions     = nullptr;
        glfwExtensions                  = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::shared_ptr<std::vector<const char*>> extensions(
            new std::vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount));

        if (enableValidationLayers) { extensions->push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME); }
        // VK_EXT_DEBUG_UTILS_EXTENSION_NAME等价于VK_EXT_debug_utils

        return extensions;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT             messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                        void*                                       pUserData)
    {
        /*
        函数使用VKAPI_ATTR和VKAPI_CALL定义，确保它可以被Vulkan库调用。
        函数的第一个参数指定了消息的级别，可以使用比较运算符来过滤处理一定级别以上的调试信息

        回调函数返回了一个布尔值，用来表示引发校验层处理的Vulkan API调用是否被中断。
        如果返回值为true，对应Vulkan API调用就会返回VK_ERROR_VALIDATION_FAILED_EXT错误代码。
        通常，只在测试校验层本身时会返回true，其余情况下，回调函数应该返回VK_FALSE。
        */

        LOG(ERROR) << "validation layer: " << pCallbackData->pMessage;
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    void setupDebugCallback()
    {
        if (!enableValidationLayers) { return; }

        VkDebugUtilsMessengerCreateInfoEXT createInfo{
            VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            nullptr,
            0,
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            debugCallback,
            nullptr};

        // 载入属于扩展的vkCreateDebugUtilsMessengerEXT函数
        auto CreateDebugUtilsMessengerEXT =
            [](VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
               const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pCallback) -> VkResult {
            auto func =
                (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
            if (func != nullptr) { return func(instance, pCreateInfo, pAllocator, pCallback); }
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        };

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &callback) != VK_SUCCESS)
        {
            LOG(ERROR) << "failed to set up debug callback! ";
            throw std::runtime_error("failed to set up debug callback!");
        }
    }

    void pickPhysicalDevice()
    {
        // Vulkan允许我们选择任意数量的显卡设备，并能够同时使用它们

        // 请求显卡列表
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0)
        {
            LOG(ERROR) << "failed to find GPUs with Vulkan support!";
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // ANCHOR - 检查并选定物理设备
        for (const auto& device : devices)
        {
            if (isDeviceSuitable(device))
            {
                physicalDevice = device;
                break;
            }
        }
        if (physicalDevice == VK_NULL_HANDLE)
        {
            LOG(ERROR) << "failed to find a suitable GPU!";
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    bool isDeviceSuitable(VkPhysicalDevice device)
    {
        bool extensionsSupported = checkDeviceExtensionSupport(device);

        // 检查物理设备是否满足交换链的所有要求
        bool swapChainAdequate = false;
        if (extensionsSupported)
        {
            auto swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate     = !swapChainSupport->formats.empty() && !swapChainSupport->presentModes.empty();
        }

        return findQueueFamilies(device).isComplete()  // 查找满足需求的队列族
               && extensionsSupported                  // 检查物理设备是否满足所有扩展
               && swapChainAdequate;                   // 检查物理设备是否满足交换链的所有要求
    }

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        // 找到支持VK_QUEUE_GRAPHICS_BIT的队列族
        int i = 0;
        for (const auto& queueFamily : queueFamilies)
        {
            if (queueFamily.queueCount > 0 && ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0))
            {
                indices.graphicsFamily = i;
            }

            // 检查物理设备是否具有呈现能力
            VkBool32 presentSupport = 0;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport) { indices.presentFamily = i; }
            // 实际上，读者可以显式地指定绘制和呈现队列族是同一个的物理设备来提高性能表现。

            if (indices.isComplete()) { break; }
            i++;
        }

        return indices;
    }

    void createLogicalDevice()
    {
        // ANCHOR - 创建逻辑设备
        // 创建图形队列、呈现队列
        QueueFamilyIndices                   indices             = findQueueFamilies(physicalDevice);
        std::set<int32_t>                    uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos(uniqueQueueFamilies.size());

        float queuePriority = 1.0F;
        for (int32_t queueFamily : uniqueQueueFamilies)
        {
            // Vulkan需要我们赋予队列一个0.0到1.0之间的浮点数作为优先级来控制指令缓冲的执行顺序。
            queueCreateInfos.push_back(VkDeviceQueueCreateInfo{
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,  //
                nullptr,                                     //
                0,                                           //
                static_cast<uint32_t>(queueFamily),          //
                1,                                           //
                &queuePriority                               //
            });
        }

        // 指定应用程序使用的设备特性
        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{
            VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,                                           //
            nullptr,                                                                        //
            0,                                                                              //
            static_cast<uint32_t>(queueCreateInfos.size()),                                 //
            queueCreateInfos.data(),                                                        //
            (enableValidationLayers) ? static_cast<uint32_t>(validationLayers.size()) : 0,  //
            (enableValidationLayers) ? validationLayers.data() : nullptr,                   //
            static_cast<uint32_t>(deviceExtensions.size()),                                 //
            deviceExtensions.data(),                                                        //
            &deviceFeatures                                                                 //
        };
        /*
        VK_KHR_swapchain就是一个设备特定扩展的例子，这一扩展使得我们可以将
        渲染的图像在窗口上显示出来。看起来似乎应该所有支持Vulkan的设备都应该支持这一扩展，
        然而，实际上有的Vulkan设备只支持计算指令
        */
        // 创建逻辑设备
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
        {
            LOG(ERROR) << "failed to create logical device!";
            throw std::runtime_error("failed to create logical device!");
        }  // 逻辑设备并不直接与Vulkan实例交互，所以创建逻辑设备时不需要使用Vulkan实例作为参数。

        // 获取队列句柄
        vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
        vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
    }

    void createSurface()
    {
        /**NOTE - 窗口表面
        为了将Vulkan渲染的图像显示在窗口上，我们需要使用WSI(Window System Integration)扩展。
        VK_KHR_surface扩展通过VkSurfaceKHR对象抽象出可供Vulkan渲染的表面。

        VK_KHR_surface是一个实例级别的扩展，它已经被包含在使用glfwGetRequiredInstance
        Extensions函数获取的扩展列表中。WSI扩展同样也被包含在函数获取的扩展列表中。

        由于窗口表面对物理设备的选择有一定影响，它的创建只能在Vulkan实例创建之后进行。

        尽管VkSurfaceKHR对象是平台无关的，但它的创建依赖窗口系统。
        */
        if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        {
            LOG(ERROR) << "failed to create window surface!";
            throw std::runtime_error("failed to create window surface!");
        }
    }

    static bool checkDeviceExtensionSupport(VkPhysicalDevice device)
    {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
        for (const auto& extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    std::shared_ptr<SwapChainSupportDetails> querySwapChainSupport(VkPhysicalDevice device)
    {
        std::shared_ptr<SwapChainSupportDetails> details(new SwapChainSupportDetails());

        // 查询基础表面特性
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &(details->capabilities));

        // 查询表面支持的格式
        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
        if (formatCount != 0)
        {
            details->formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details->formats.data());
        }

        // 查询支持的呈现模式
        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
        if (presentModeCount != 0)
        {
            details->presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details->presentModes.data());
        }

        return details;
    }

    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        /*
        对于颜色空间，如果SRGB被支持，我们就使用SRGB，使用它可以得到更加准确的颜色表示。
        直接使用SRGB颜色有很大挑战，所以我们使用RGB作为颜色格式，这一格式可以通过
        VK_FORMAT_B8G8R8A8_UNORM宏指定。
        */
        if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
        {
            return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        }

        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        /*
        呈现模式决定了什么条件下图像才会显示到屏幕。Vulkan提供了四种可用的呈现模式：

            VK_PRESENT_MODE_IMMEDIATE_KHR：
            应用程序提交的图像会被立即传输到屏幕上，可能会导致撕裂现象。

            VK_PRESENT_MODE_FIFO_KHR：
            交换链变成一个先进先出的队列，每次从队列头部取出一张图像进行显示，
            应用程序渲染的图像提交给交换链后，会被放在队列尾部。当队列为满时，
            应用程序需要进行等待。这一模式非常类似现在常用的垂直同步。
            刷新显示的时刻也被叫做垂直回扫。

            VK_PRESENT_MODE_FIFO_RELAXED_KHR：
            这一模式和上一模式的唯一区别是，如果应用程序延迟，导致交换链的队列在上一
            次垂直回扫时为空，那么，如果应用程序在下一次垂直回扫前提交图像，
            图像会立即被显示。这一模式可能会导致撕裂现象。

            VK_PRESENT_MODE_MAILBOX_KHR：
            这一模式是第二种模式的另一个变种。它不会在交换链的队列满时阻塞应用程序，
            队列中的图像会被直接替换为应用程序新提交的图像。这一模式可以用来实现三倍缓冲，
            避免撕裂现象的同时减小了延迟问题。

        上面四种呈现模式，只有VK_PRESENT_MODE_FIFO_KHR模式保证一定可用
        */
        VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto& availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) { return availablePresentMode; }
            if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) { bestMode = availablePresentMode; }
        }
        return bestMode;
    }

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
    {
        // 交换范围是交换链中图像的分辨率，它几乎总是和我们要显示图像的窗口的分辨率相同。
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }

        return {std::clamp(screen_width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
                std::clamp(screen_height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
    }

    void createSwapChain()
    {
        /**NOTE - 交换链
        Vulkan没有默认帧缓冲的概念，它需要一个能够缓冲渲染操作的组件。这一组件就是交换链。
        Vulkan的交换链必须显式地创建，不存在默认的交换链。
        交换链本质上一个包含了若干等待呈现的图像的队列。我们的应用程序从交换链获取一张图像，
        然后在图像上进行渲染操作，完成后，将图像返回到交换链的队列中。
        通常来说，交换链被用来同步图像呈现和屏幕刷新。

        并不是所有的显卡设备都具有可以直接将图像呈现到屏幕的能力。
        比如，被设计用于服务器的显卡是没有任何显示输出设备的。
        此外，由于图像呈现非常依赖窗口系统，以及和窗口系统有关的窗口表面，
        这些并非Vulkan核心的一部分。使用交换链，必须保证VK_KHR_swapchain设备扩展被启用。

        只检查交换链是否可用还不够，交换链可能与我们的窗口表面不兼容。
        创建交换链所要进行的设置要比Vulkan实例和设备创建多得多

        有三种最基本的属性，需要检查：
            基础表面特性(交换链的最小/最大图像数量，最小/最大图像宽度、高度)
            表面格式(像素格式，颜色空间)
            可用的呈现模式
        */

        auto               swapChainSupport = querySwapChainSupport(physicalDevice);
        VkSurfaceFormatKHR surfaceFormat    = chooseSwapSurfaceFormat(swapChainSupport->formats);
        VkPresentModeKHR   presentMode      = chooseSwapPresentMode(swapChainSupport->presentModes);
        VkExtent2D         extent           = chooseSwapExtent(swapChainSupport->capabilities);

        // 设置交换链的队列可以容纳的图像个数
        // 使用交换链支持的最小图像个数+1数量的图像来实现三倍缓冲
        // maxImageCount的值为0表明，只要内存可以满足，我们可以使用任意数量的图像。
        uint32_t imageCount = swapChainSupport->capabilities.minImageCount + 1;
        if (swapChainSupport->capabilities.maxImageCount > 0 &&
            imageCount > swapChainSupport->capabilities.maxImageCount)
        {
            // minImageCount == maxImageCount
            imageCount = swapChainSupport->capabilities.maxImageCount;
        }

        QueueFamilyIndices      indices            = findQueueFamilies(physicalDevice);
        std::array<uint32_t, 2> queueFamilyIndices = {static_cast<uint32_t>(indices.graphicsFamily),
                                                      static_cast<uint32_t>(indices.presentFamily)};

        VkSwapchainCreateInfoKHR createInfo{
            .sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface          = surface,
            .minImageCount    = imageCount,
            .imageFormat      = surfaceFormat.format,
            .imageColorSpace  = surfaceFormat.colorSpace,
            .imageExtent      = extent,
            .imageArrayLayers = 1,
            .imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .preTransform     = swapChainSupport->capabilities.currentTransform,
            .compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode      = presentMode,
            .clipped          = VK_TRUE,
            .oldSwapchain     = VK_NULL_HANDLE,
        };
        /**NOTE - VkSwapchainCreateInfoKHR
        imageArrayLayers成员变量用于指定每个图像所包含的层次。
        通常，来说它的值为1。但对于VR相关的应用程序来说，会使用更多的层次。

        imageUsage成员变量用于指定我们将在图像上进行怎样的操作。
        在本教程，我们在图像上进行绘制操作，也就是将图像作为一个颜色附着来使用。
        如果读者需要对图像进行后期处理之类的操作，可以使用VK_IMAGE_USAGE_
        TRANSFER_DST_BIT作为imageUsage成员变量的值，让交换链图像可以作为传输的目的图像。
        */
    }
};

int main(int argc, char** argv)
{
    // ANCHOR - init glog
    {
        google::InitGoogleLogging(*argv);
        FLAGS_minloglevel      = google::LogSeverity::GLOG_INFO;  // 设置最小日志级别
        FLAGS_colorlogtostderr = true;                            // 是否启用不同颜色显示
        FLAGS_colorlogtostdout = true;
        FLAGS_logtostderr      = true;
        FLAGS_logtostdout      = true;
    }

    try
    {
        HelloTriangleApplication app;
        app.run();
    }
    catch (const std::exception& e)
    {
        // 执行过程中，如果发生错误，会抛出一个带有错误描述信息的std::runtime_error异常，
        // 为了处理多种不同类型的异常，我们使用更加通用的std::exception来接受异常。
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
