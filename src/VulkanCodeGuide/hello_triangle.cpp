#include <cstddef>
#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <chrono>  //使用计时函数
#include <fstream>
#include <functional>  //functional头文件用于资源管理
#include <glm/ext/matrix_transform.hpp>
#include <glm/fwd.hpp>
#include <glm/trigonometric.hpp>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>  //报错
#include <string>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#define GLM_FORCE_RADIANS  // 函数使用弧度作为参数的单位
#define STB_IMAGE_IMPLEMENTATION
#include <GLFW/glfw3.h>  //GLFW库会自动包含Vulkan库的头文件
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glog/logging.h>
#include <stb_image.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

// constants
static const uint32_t    screen_width         = 800;
static const uint32_t    screen_height        = 600;
static const std::string shader_root          = "./shader/";
static const int32_t     MAX_FRAMES_IN_FLIGHT = 2;
#ifdef NDEBUG
static const bool enableValidationLayers = false;
#else
static const bool enableValidationLayers = true;
#endif
static const std::vector<const char*> validationLayers                  = {"VK_LAYER_KHRONOS_validation"};
static const std::vector<const char*> deviceExtensions                  = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
static const bool                     print_avalibale_vulkan_extensions = false;

// vertex inputs
struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;
};
static const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},  //
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},   //
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},    //
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},   //
};  // 交叉顶点属性(interleaving vertex attributes)
static const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};  // 顶点索引
/**FIXME -
这里绑定的时候把pos设置成3分量，而color是2分量
但是vertex shader读的时候，pos读两个分量，color读三个分量
colord的最后一个分量按照默认值填充（0填充）
 */

/**
 * @brief
 *
 */
struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

static std::shared_ptr<std::vector<char>> readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    // 使用ate模式，从文件尾部开始读取可以根据读取位置确定文件的大小，分配足够的数组空间
    if (!file.is_open()) { throw std::runtime_error("failed to open file!"); }

    size_t                             fileSize = static_cast<size_t>(file.tellg());
    std::shared_ptr<std::vector<char>> buffer(new std::vector<char>(fileSize));
    file.seekg(0);
    file.read(buffer->data(), fileSize);
    file.close();
    return buffer;
}

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

        [[nodiscard]] bool isComplete() const { return graphicsFamily >= 0 && presentFamily >= 0; }
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR        capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR>   presentModes;
    };

    // create instance
    GLFWwindow*              window;
    VkInstance               instance;
    VkDebugUtilsMessengerEXT callback;

    // create logical device
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;  // vkinstance清除时自动清除
    VkSurfaceKHR     surface;
    VkDevice         device;
    VkQueue          graphicsQueue;  // 逻辑设备的队列会在逻辑设备清除时自动被清除
    VkQueue          presentQueue;

    // create swap chain
    VkSwapchainKHR           swapChain;
    std::vector<VkImage>     swapChainImages;  // 图像由交换链负责创建，并在交换链清除时自动清除
    VkFormat                 swapChainImageFormat;
    VkExtent2D               swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;

    // create render pipline
    VkRenderPass                 renderPass;
    VkDescriptorSetLayout        descriptorSetLayout;
    VkPipelineLayout             pipelineLayout;
    VkPipeline                   graphicsPipeline;
    std::vector<VkFramebuffer>   swapChainFramebuffers;
    VkCommandPool                commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    // render and present
    std::vector<VkSemaphore> imageAvailableSemaphores;  // 为每一帧创建信号量
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;  // CPU和GPU的同步，防止有超过最大并行数帧的指令同时被提交执行
    size_t         currentFrame       = 0;  // 当前帧应该使用的信号量
    bool           framebufferResized = false;
    VkBuffer       vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkBuffer       indexBuffer;
    VkDeviceMemory indexBufferMemory;

    std::vector<VkBuffer>       uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;

    VkDescriptorPool             descriptorPool;
    std::vector<VkDescriptorSet> descriptorSets;

    VkImage        textureImage;
    VkDeviceMemory textureImageMemory;

    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // 阻止GLFW它创建OpenGL上下文
        // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);    // 禁止窗口大小改变
        window = glfwCreateWindow(screen_width, screen_height, "hello_triangle", nullptr, nullptr);

        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
    }

    void initVulkan()
    {
        createInstance();
        setupDebugCallback();

        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();

        createSwapChain();
        createImageViews();

        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createTextureImage();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffer();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();

        createSemaphores();
    }

    void mainLoop()
    {
        while (glfwWindowShouldClose(window) == 0)
        {
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(device);
        /*
        drawFrame函数中的操作是异步执行的。这意味着我们关闭应用程序窗口跳出主循环时，
        绘制操作和呈现操作可能仍在继续执行，这与我们紧接着进行的清除操作也是冲突的。

        我们应该等待逻辑设备的操作结束执行才能销毁窗口，
        我们可以使用vkQueueWaitIdle函数等待一个特定指令队列结束执行。
        */
    }

    void cleanup()
    {
        // ANCHOR -  cleanup Vulkan
        vkDestroyBuffer(device, vertexBuffer, nullptr);  // 清除缓冲区
        vkFreeMemory(device, vertexBufferMemory, nullptr);
        vkDestroyBuffer(device, indexBuffer, nullptr);
        vkFreeMemory(device, indexBufferMemory, nullptr);
        // 清除所有信号量
        for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }

        cleanupSwapChain();  // 清理交换链

        vkDestroyImage(device, textureImage, nullptr);
        vkFreeMemory(device, textureImageMemory, nullptr);

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);

        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        for (size_t i = 0; i < swapChainImages.size(); i++)
        {
            vkDestroyBuffer(device, uniformBuffers[i], nullptr);
            vkFreeMemory(device, uniformBuffersMemory[i], nullptr);
        }

        vkDestroyCommandPool(device, commandPool, nullptr);

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

        // ANCHOR -  cleanup GLFW
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
        if (print_avalibale_vulkan_extensions)
        {
            uint32_t extensionCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> extensions(extensionCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
            LOG(INFO) << "available extensions:";
            for (const auto& extension : extensions) { LOG(INFO) << "\t" << extension.extensionName; }
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
        /**FIXME - 错题本
        std::vector<> _vector(count);
        构造函数会分配count个空间，并填充count个默认构造的元素进去
        */

        float   queuePriority = 1.0F;
        int32_t i             = 0;
        for (int32_t queueFamily : uniqueQueueFamilies)
        {
            // Vulkan需要我们赋予队列一个0.0到1.0之间的浮点数作为优先级来控制指令缓冲的执行顺序。
            queueCreateInfos[i++] = VkDeviceQueueCreateInfo{
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,  //
                nullptr,                                     //
                0,                                           //
                static_cast<uint32_t>(queueFamily),          //
                1,                                           //
                &queuePriority                               //
            };
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
            &deviceFeatures,                                                                //
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
        for (const auto& extension : availableExtensions) { requiredExtensions.erase(extension.extensionName); }

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

        int width  = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        return {
            std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
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
        const bool              uniqueFamily       = (indices.graphicsFamily == indices.presentFamily);

        VkSwapchainCreateInfoKHR createInfo{
            .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext                 = nullptr,
            .flags                 = 0,
            .surface               = surface,
            .minImageCount         = imageCount,
            .imageFormat           = surfaceFormat.format,
            .imageColorSpace       = surfaceFormat.colorSpace,
            .imageExtent           = extent,
            .imageArrayLayers      = 1,  // 每个图像所包含的层次，VR相应用程序会有更多的层次
            .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode      = (uniqueFamily) ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
            .queueFamilyIndexCount = (uniqueFamily) ? 0U : 2U,
            .pQueueFamilyIndices   = (uniqueFamily) ? nullptr : queueFamilyIndices.data(),
            .preTransform          = swapChainSupport->capabilities.currentTransform,
            .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,  // 忽略alpha通道
            .presentMode           = presentMode,
            .clipped               = VK_TRUE,  // 不关心被窗口系统中的其它窗口遮挡的像素的颜色
            .oldSwapchain          = VK_NULL_HANDLE,  // 改变窗口大小后交换链需要重建
        };
        /**NOTE - VkSwapchainCreateInfoKHR
        imageUsage成员变量用于指定我们将在图像上进行怎样的操作。
        在本教程，我们在图像上进行绘制操作，也就是将图像作为一个颜色附着来使用。
        如果读者需要对图像进行后期处理之类的操作，可以使用VK_IMAGE_USAGE_
        TRANSFER_DST_BIT作为imageUsage成员变量的值，让交换链图像可以作为传输的目的图像。

        我们通过图形队列在交换链图像上进行绘制操作，然后将图像提交给呈现队列来显示。
        有两种控制在多个队列访问图像的方式：
            VK_SHARING_MODE_EXCLUSIVE：
            一张图像同一时间只能被一个队列族所拥有，在另一队列族使用它之前，
            必须显式地改变图像所有权。这一模式下性能表现最佳。
            VK_SHARING_MODE_CONCURRENT：
            图像可以在多个队列族间使用，不需要显式地改变图像所有权。

        如果图形和呈现不是同一个队列族，我们使用协同模式来避免处理图像所有权问题。
        协同模式需要我们使用queueFamilyIndexCount和pQueueFamilyIndices来指定
        共享所有权的队列族。如果图形队列族和呈现队列族是同一个队列族(大部分情况下都是这样)，
        我们就不能使用协同模式，协同模式需要我们指定至少两个不同的队列族。
        */

        // ANCHOR - 创建交换链
        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
        {
            LOG(ERROR) << "failed to create swap chain!";
            throw std::runtime_error("failed to create swap chain!");
        }

        // 获取交换链图像
        // Vulkan的具体实现可能会创建比imageCount最小交换链图像数量更多的交换链图像
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());
        // 设置的交换链图像格式和范围
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent      = extent;
    }

    void createImageViews()
    {
        /* 图像视图
        使用任何VkImage对象，包括处于交换链中的，处于渲染管线中的，都需要我们创建一个
        VkImageView对象来绑定访问它。图像视图描述了访问图像的方式，以及图像的哪一部分
        可以被访问。比如，图像可以被图像视图描述为一个没有细化级别的二维深度纹理，进而
        可以在其上进行与二维深度纹理相关的操作。
        */
        swapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++)
        {
            VkImageViewCreateInfo createInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .image = swapChainImages[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,  // 指定图像被看作一维、二维、三维纹理还是立方体贴图
                .format     = swapChainImageFormat,
                .components = VkComponentMapping{.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                 .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                 .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                                 .a = VK_COMPONENT_SWIZZLE_IDENTITY},
                .subresourceRange =
                    VkImageSubresourceRange{
                        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel   = 0,
                        .levelCount     = 1,
                        .baseArrayLayer = 0,
                        .layerCount     = 1,
                    },
            };
            /*
            components成员变量用于进行图像颜色通道的映射。
            比如，对于单色纹理，我们可以将所有颜色通道映射到红色通道。
            我们也可以直接将颜色通道的值映射为常数0或1。在这里，我们只使用默认的映射

            subresourceRange成员变量用于指定图像的用途和图像的哪一部分可以被访问。
            在这里，我们的图像被用作渲染目标，并且没有细分级别，只存在一个图层

            如果读者在编写VR一类的应用程序，可能会使用支持多个层次的交换链。
            这时，读者应该为每个图像创建多个图像视图，分别用来访问左眼和右眼两个不同的图层
            */

            if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
            {
                LOG(ERROR) << "failed to create image views!";
                throw std::runtime_error("failed to create image views!");
            }
        }
    }

    VkShaderModule createShaderModule(const std::shared_ptr<std::vector<char>>& code)
    {
        /*
        但需要注意一点，我们需要先将存储字节码的数组指针转换为const uint32_t*变量类型，
        来匹配结构体中的字节码指针的变量类型。

        此外，我们指定的指针指向的地址应该符合uint32_t变量类型的内存对齐
        方式。我们这里使用的std::vector，它的默认分配器分配的内存的地址符合这一要求。
        */
        VkShaderModuleCreateInfo createInfo{
            .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .pNext    = nullptr,
            .flags    = 0,
            .codeSize = code->size(),
            .pCode    = reinterpret_cast<const uint32_t*>(code->data()),
        };
        VkShaderModule shaderModule = nullptr;
        if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            LOG(ERROR) << "failed to create shader module!";
            throw std::runtime_error("failed to create shader module!");
        }
        return shaderModule;
    }

    void createRenderPass()
    {
        /**NOTE - 定义渲染流程
        在进行管线创建之前，我们还需要设置用于渲染的帧缓冲附着。
        我们需要指定使用的颜色和深度缓冲，以及采样数，渲染操作如何处理缓冲的内容。
        所有这些信息被Vulkan包装为一个渲染流程对象。
         */
        VkAttachmentDescription colorAttachment{
            .flags         = 0,
            .format        = swapChainImageFormat,
            .samples       = VK_SAMPLE_COUNT_1_BIT,  // 我们没有使用多重采样，所以将采样数设置为1
            .loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR,   // 指定在渲染之前对附着中的数据进行的操作
            .storeOp       = VK_ATTACHMENT_STORE_OP_STORE,  // 指定在渲染之后对附着中的数据进行的操作
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,        // 指定渲染流程开始前的图像布局方式
            .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,  // 指定渲染流程结束后的图像布局方式
        };  // finalLayout设置为VK_IMAGE_LAYOUT_PRESENT_SRC_KHR，使渲染后的图像可以被交换链呈现。

        /* 子流程和附着引用
        一个渲染流程可以包含多个子流程。子流程依赖于上一流程处理后的帧缓冲内容。
        比如，许多叠加的后期处理效果就是在上一次的处理结果上进行的。
        我们将多个子流程组成一个渲染流程后，Vulkan可以对其进行一定程度的优化。

        每个子流程可以引用一个或多个附着，这些引用的附着是通过VkAttachmentReference结构体指定的
        */
        VkAttachmentReference colorAttachmentRef{
            .attachment = 0,  // 指定要引用的附着在附着描述结构体数组中的索引
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            // 指定进行子流程时引用的附着使用的布局方式，一般而言，这个值性能表现最佳
        };
        VkSubpassDescription subpass{
            .flags             = 0,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,  // 显式指定这是一个图形渲染的子流程
            .inputAttachmentCount    = 0,
            .pInputAttachments       = nullptr,  // 被着色器读取的附着
            .colorAttachmentCount    = 1,
            .pColorAttachments       = &colorAttachmentRef,
            .pResolveAttachments     = nullptr,  // 用于多重采样的颜色附着
            .pDepthStencilAttachment = nullptr,  // 用于深度和模板数据的附着
            .preserveAttachmentCount = 0,
            .pPreserveAttachments    = nullptr,  // 没有被这一子流程使用，但需要保留数据的附着
        };
        /*
        这里设置的颜色附着在数组中的索引会被片段着色器使用，
        对应我们在片段着色器中使用的 layout(location = 0) out vec4 outColor语句。
        */

        /* 配置子流程依赖
        渲染流程的子流程会自动进行图像布局变换。这一变换过程由子流程的依赖所决定。
        子流程的依赖包括子流程之间的内存和执行的依赖关系。虽然我们现在只使用了一个
        子流程，但子流程执行之前和子流程执行之后的操作也被算作隐含的子流程。

        在渲染流程开始和结束时会自动进行图像布局变换，但在渲染流程开始时进行的自动变换
        的时机和我们的需求不符，变换发生在管线开始时，但那时我们可能还没有获取到交换链
        图像。有两种方式可以解决这个问题。一个是设置imageAvailableSemaphore信号量的
        waitStages为VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT（一定要在管线开始的这个阶段，
        等到imageAvailableSemaphore这个信号量），确保渲染流程在我们获取交换链
        图像之前不会开始
        一个是设置渲染流程等待VK_PIPELINE_STAGE_COLOR_ATTACHMENT _OUTPUT_BIT管线阶段。

        srcSubpass和dstSubpass成员变量用于指定被依赖的子流程的索引和依赖被依赖的子流程的索引。
        VK_SUBPASS_EXTERNAL用来指定我们之前提到的隐含的子流程，对srcSubpass成员变量使用
        表示渲染流程开始前的子流程，对dstSubpass成员使用表示渲染流程结束后的子流程。这里
        使用的索引0是我们之前创建的子流程的索引。
        为了避免出现循环依赖，我们给dstSubpass设置的值必须始终大于srcSubpass。

        我们需要等待交换链结束对图像的读取才能对图像进行访问操作，也就是等待颜色附着输出这一管线阶段。
        // FIXME - 这里完全看不懂
        */
        VkSubpassDependency dependency{
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = 0,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0,
        };

        // 创建渲染流程
        VkRenderPassCreateInfo renderPassInfo{
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .attachmentCount = 1,
            .pAttachments    = &colorAttachment,
            .subpassCount    = 1,
            .pSubpasses      = &subpass,
            .dependencyCount = 1,
            .pDependencies   = &dependency,
        };
        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
        {
            LOG(ERROR) << "failed to create render pass!";
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void createGraphicsPipeline()
    {
        /**NOTE - 创建图形管线
        现在，让我们回忆之下我们为了创建图形管线而创建的对象：
            着色器阶段：定义了着色器模块用于图形管线哪一可编程阶段
            固定功能状态：定义了图形管线的固定功能阶段使用的状态信息，比如输入装配，视口，光栅化，颜色混合
            管线布局：定义了被着色器使用，在渲染时可以被动态修改的uniform变量
            渲染流程：定义了被管线使用的附着附着的用途
         */

        // 创建着色器阶段
        //  着色器模块对象只在管线创建时需要，所以，作为一个局部变量定义在createGraphicsPipeline函数中
        VkShaderModule vertShaderModule = createShaderModule(readFile(shader_root + "hello_triangle_vert.spv"));
        VkShaderModule fragShaderModule = createShaderModule(readFile(shader_root + "hello_triangle_frag.spv"));
        // LINK - 修改这里，来使用你自己的着色器

        // VkShaderModule对象只是一个对着色器字节码的包装。还需要指定它们在管线哪一阶段被使用。
        std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
            // vertex shader
            VkPipelineShaderStageCreateInfo{
                .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext               = nullptr,
                .flags               = 0,
                .stage               = VK_SHADER_STAGE_VERTEX_BIT,
                .module              = vertShaderModule,
                .pName               = "main",
                .pSpecializationInfo = nullptr,
            },
            // fragment shader
            VkPipelineShaderStageCreateInfo{
                .sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext               = nullptr,
                .flags               = 0,
                .stage               = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module              = fragShaderModule,
                .pName               = "main",   // 指定阶段调用的着色器函数
                .pSpecializationInfo = nullptr,  // 指定着色器用到的常量
            },
        };
        /*
        pName：我们可以通过使用不同pName在同一份着色器代码中实现所有需要的着色器，
        比如在同一份代码中实现多个片段着色器，然后通过不同的pName调用它们。

        pSpecializationInfo：我们可以对同一个着色器模块对象指定不同的着色器常量用于管线创建，
        这使得编译器可以根据指定的着色器常量来消除一些条件分支，
        这比在渲染时，使用变量配置着色器带来的效率要高得多。
        */

        // ANCHOR - 定义图形管线的固定功能状态

        // 顶点输入：描述传递给顶点着色器的顶点数据格式
        //        绑定：数据之间的间距和数据是按逐顶点的方式还是按逐实例的方式进行组织
        //        属性描述：传递给顶点着色器的属性类型，用于将属性绑定到顶点着色器中的变量
        VkVertexInputBindingDescription vertexBindingDescription{
            .binding   = 0,
            .stride    = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
        std::array<VkVertexInputAttributeDescription, 2> vertexAttributeDescriptions = {
            VkVertexInputAttributeDescription{
                .location = 0,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32_SFLOAT,
                .offset   = offsetof(Vertex, pos),
            },
            VkVertexInputAttributeDescription{
                .location = 1,
                .binding  = 0,
                .format   = VK_FORMAT_R32G32B32_SFLOAT,
                .offset   = offsetof(Vertex, color),
            },
        };
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{
            .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .pNext                           = nullptr,
            .flags                           = 0,
            .vertexBindingDescriptionCount   = 1,
            .pVertexBindingDescriptions      = &vertexBindingDescription,
            .vertexAttributeDescriptionCount = 2,
            .pVertexAttributeDescriptions    = vertexAttributeDescriptions.data(),
        };

        /* 输入装配
        顶点数据定义了哪种类型的几何图元，以及是否启用几何图元重启。

        一般而言，我们会通过索引缓冲来更好地复用顶点缓冲中的顶点数据。
        如果将primitiveRestartEnable成员变量的值设置为VK_TRUE，那么如果使用带有_STRIP
        结尾的图元类型，可以通过一个特殊索引值0xFFFF或0xFFFFFFFF达到重启图元的目
        的(从特殊索引值之后的索引重置为图元的第一个顶点)。
         */
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        };

        /* 视口和裁剪
        视口用于描述被用来输出渲染结果的帧缓冲区域。一般设置为(0，0)到(width，height)

        需要注意，交换链图像的大小可能与窗口大小不同。
        交换链图像在之后会被用作帧缓冲，所以这里我们设置视口大小为交换链图像的大小。

        minDepth和maxDepth：值必须在[0.0f，1.0f]，特别的，minDepth的值可以大于maxDepth的值。
        */
        VkViewport viewport{
            .x        = 0.0F,
            .y        = 0.0F,
            .width    = static_cast<float>(swapChainExtent.width),
            .height   = static_cast<float>(swapChainExtent.height),
            .minDepth = 0.0F,  // 指定帧缓冲使用的深度值的范围
            .maxDepth = 1.0F,  // 指定帧缓冲使用的深度值的范围
        };

        /* 裁剪矩形
        视口定义了图像到帧缓冲的映射关系，裁剪矩形定义了哪一区域的像素实际被存储在帧缓存。
        任何位于裁剪矩形外的像素都会被光栅化程序丢弃。
        ![](https://zeromake.github.io/VulkanTutorialCN/img/f14-1.jpg)
        */
        VkRect2D scissor{
            .offset = VkOffset2D{0, 0},
            .extent = swapChainExtent,
        };

        // 填写视口状态
        VkPipelineViewportStateCreateInfo viewportState{
            .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .pNext         = nullptr,
            .flags         = 0,
            .viewportCount = 1,
            .pViewports    = &viewport,
            .scissorCount  = 1,
            .pScissors     = &scissor,
        };

        /* 光栅化程序的配置
        depthClampEnable：VK_TRUE表示在近平面和远平面外的片段会被截断为在近平面和远平面上，
        而不是直接丢弃这些片段。这对于阴影贴图的生成很有用。使用这一设置需要开启相应的GPU特性。

        rasterizerDiscardEnable：
        成员变量设置为VK_TRUE表示所有几何图元都不能通过光栅化阶段。

        polygonMode：使用除了VK_POLYGON_MODE_FILL外的模式，需要启用相应的GPU特性。

        lineWidth：线宽的最大值依赖于硬件，使用大于1.0f的线宽，需要启用相应的GPU特性

        depthBiasEnable：光栅化程序可以添加一个常量值或是一个基于片段所处线段的斜率
        得到的变量值到深度值上。这对于阴影贴图会很有用
        */
        VkPipelineRasterizationStateCreateInfo rasterizer{
            .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .depthClampEnable        = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,                 // 禁止一切片段输出到帧缓冲
            .polygonMode             = VK_POLYGON_MODE_FILL,     // 指定几何图元生成片段的方式
            .cullMode                = VK_CULL_MODE_NONE,        // 表面剔除类型
            .frontFace               = VK_FRONT_FACE_CLOCKWISE,  // 指定顶点顺序的朝向
            .depthBiasEnable         = VK_FALSE,
            .depthBiasConstantFactor = 0.0F,
            .depthBiasClamp          = 0.0F,
            .depthBiasSlopeFactor    = 0.0F,
            .lineWidth = 1.0F,  // 指定光栅化后的线段宽度，它以线宽所占的片段数目为单位
        };

        /* 多重采样
        多重采样是一种组合多个不同多边形产生的片段的颜色来决定最终的像素颜色的技术，
        它可以一定程度上减少多边形边缘的走样现象。对于一个像素只被一个多边形产生的片段覆盖，
        只会对覆盖它的这个片段执行一次片段着色器，使用多重采样进行反走样的代价要比使用更高
        的分辨率渲染，然后缩小图像达到反走样的代价小得多。使用多重采样需要启用相应的GPU特性。
        */
        VkPipelineMultisampleStateCreateInfo multisampling{
            .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext                 = nullptr,
            .flags                 = 0,
            .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable   = VK_FALSE,
            .minSampleShading      = 1.0f,
            .pSampleMask           = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable      = VK_FALSE,
        };  // 这里先禁用多重采样

        /* 深度和模板测试
        暂时不使用深度和模板测试
        */

        /* 色彩混合
        片段着色器返回的片段颜色需要和原来帧缓冲中对应像素的颜色进行混合。

        VkPipelineColorBlendStateCreateInfo结构体使用了一个
        VkPipelineColorBlendAttachmentState结构体数组指针来指定每个帧缓冲的颜色混合设置，
        还提供了用于设置全局混合常量的成员变量。

        如果想要使用第二种混合方式(位运算)，那么就需要将logicOpEnable成员变量设置为 VK_TRUE。
        然后使用logicOp成员变量指定要使用的位运算。需要注意，这样设置后会自动禁用第一种混合方式，
        就跟对每个绑定的帧缓冲设置blendEnable成员变量为VK_FALSE一样。
        colorWriteMask成员变量的设置在第二种混合方式下仍然起作用，可以决定哪些颜色通道能够
        被写入帧缓冲。我们也可以禁用所有这两种混合模式，这种情况下，片段颜色会直接覆盖原来
        帧缓冲中存储的颜色值。
        */
        VkPipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable         = VK_FALSE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
            .colorBlendOp        = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp        = VK_BLEND_OP_ADD,
            .colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT,
        };
        VkPipelineColorBlendStateCreateInfo colorBlending{
            .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .pNext           = nullptr,
            .flags           = 0,
            .logicOpEnable   = VK_FALSE,
            .logicOp         = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments    = &colorBlendAttachment,
            .blendConstants  = {0.0F, 0.0F, 0.0F, 0.0F},
        };  // 这里两种方式都禁用了

        /* 动态状态
        只有非常有限的管线状态可以在不重建管线的情况下进行动态修改。这包括视口大小，线宽和混合常量

        这样设置后会导致我们之前对这里使用的动态状态的设置被忽略掉，需要我们在进行绘制时重新指定它们的值。
        */
        std::array<VkDynamicState, 2> dynamicStates = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_LINE_WIDTH,
        };
        VkPipelineDynamicStateCreateInfo dynamicState{
            .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .pNext             = nullptr,
            .flags             = 0,
            .dynamicStateCount = 2,
            .pDynamicStates    = dynamicStates.data(),
        };  // 创建图形管线那里设置了nullptr，所以不启用

        /* 管线布局
        我们可以在着色器中使用uniform变量，它可以在管线建立后动态地被应用程序修改，
        实现对着色器进行一定程度的动态配置。uniform变量经常被用来传递变换矩阵给顶点着色器，
        以及传递纹理采样器句柄给片段着色器。

        我们在着色器中使用的uniform变量需要在管线创建时使用VkPipelineLayout对象定义。
        */
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .pNext                  = nullptr,
            .flags                  = 0,
            .setLayoutCount         = 1,
            .pSetLayouts            = &descriptorSetLayout,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges    = nullptr,
        };  // 创建一个空管线布局
        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            LOG(ERROR) << "failed to create pipeline layout!";
            throw std::runtime_error("failed to create pipeline layout!");
        }

        /**ANCHOR - 开始创建图形管线
        basePipelineHandle和basePipelineIndex：用于以一个创建好的图形管线为基础创建
        一个新的图形管线。当要创建一个和已有管线大量设置相同的管线时，使用它的代价要
        比直接创建小，并且，对于从同一个管线衍生出的两个管线，在它们之间进行管线切换
        操作的效率也要高很多。
        我们可以使用basePipelineHandle来指定已经创建好的管线，或是使用basePipelineIndex
        来指定将要创建的管线作为基础管线，用于衍生新的管线。

        这两个成员变量的设置只有在VkGraphicsPipelineCreateInfo结构体的flags成员变量
        使用了VK_PIPELINE_CREATE_DERIVATIVE_BIT标记的情况下才会起效。
         */
        VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext               = nullptr,
            .flags               = 0,
            .stageCount          = 2,
            .pStages             = shaderStages.data(),
            .pVertexInputState   = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pTessellationState  = nullptr,
            .pViewportState      = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState   = &multisampling,
            .pDepthStencilState  = nullptr,
            .pColorBlendState    = &colorBlending,
            .pDynamicState       = nullptr,
            .layout              = pipelineLayout,
            .renderPass          = renderPass,
            .subpass             = 0,  // 这个是subpass的索引
            .basePipelineHandle  = VK_NULL_HANDLE,
            .basePipelineIndex   = -1,
        };
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) !=
            VK_SUCCESS)
        {
            LOG(ERROR) << "failed to create graphics pipeline!";
            throw std::runtime_error("failed to create graphics pipeline!");
        }
        /*
        vkCreateGraphicsPipelines函数被设计成一次调用可以通过多个
        VkGraphicsPipelineCreateInfo结构体数据创建多个VkPipeline对象。

        vkCreateGraphicsPipelines函数第二个参数可以用来引用一个可选的VkPipelineCache对象。
        通过它可以将管线创建相关的数据进行缓存在多个vkCreateGraphicsPipelines函数调用中使用，
        甚至可以将缓存存入文件，在多个程序间使用。使用它可以加速之后的管线创建。
        */

        // cleanup
        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
    }

    void createFramebuffers()
    {
        /**NOTE - 帧缓冲
        我们在创建渲染流程对象时指定使用的附着需要绑定在帧缓冲对象上使用。
        帧缓冲对象引用了用于表示附着的VkImageView对象。对于我们的程序，
        我们只使用了一个颜色附着。但这并不意味着我们只需要使用一张图像，
        每个附着对应的图像个数依赖于交换链用于呈现操作的图像个数。我们需要
        为交换链中的每个图像创建对应的帧缓冲，在渲染时，渲染到对应的帧缓冲上。
         */

        swapChainFramebuffers.resize(swapChainImageViews.size());
        // 数组大小：swapChainFramebuffers==swapChainImageViews==swapChainImages

        for (size_t i = 0; i < swapChainImageViews.size(); i++)
        {
            std::array<VkImageView, 1> attachments = {swapChainImageViews[i]};

            VkFramebufferCreateInfo framebufferInfo{
                .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                .pNext           = nullptr,
                .flags           = 0,
                .renderPass      = renderPass,
                .attachmentCount = attachments.size(),  // 指定附着个数
                .pAttachments    = attachments.data(),  // 描述附着信息的pAttachment数组
                .width           = swapChainExtent.width,
                .height          = swapChainExtent.height,
                .layers          = 1,  // 指定图像层数
            };
            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
            {
                LOG(ERROR) << "failed to create framebuffer!";
                throw std::runtime_error("failed to create framebuffer!");
            }
            /*
            首先指定帧缓冲需要兼容的渲染流程对象。之后的渲染操作，我们可以使用与这个指定的
            渲染流程对象相兼容的其它渲染流程对象。一般来说，使用相同数量，相同类型附着的
            渲染流程对象是相兼容的。
            */
        }
    }

    void createCommandPool()
    {
        /**NOTE - 指令缓冲
        Vulkan下的指令，比如绘制指令和内存传输指令并不是直接通过函数调用执行的。
        我们需要将所有要执行的操作记录在一个指令缓冲对象，然后提交给可以执行这些
        操作的队列才能执行。这使得我们可以在程序初始化时就准备好所有要指定的指令序列，
        在渲染时直接提交执行。也使得多线程提交指令变得更加容易。
         */

        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        /* 指令池
        每个指令池对象分配的指令缓冲对象只能提交给一个特定类型的队列。

        有两种用于指令池对象创建的标记，可以提供有用的信息给Vulkan的驱动程序进行一定优化处理：
            VK_COMMAND_POOL_CREATE_TRANSIENT_BIT：
            使用它分配的指令缓冲对象被频繁用来记录新的指令
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT：
            指令缓冲对象之间相互独立，不会被一起重置。
        */
        VkCommandPoolCreateInfo poolInfo{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,  // 指令池标记
            .queueFamilyIndex = static_cast<uint32_t>(queueFamilyIndices.graphicsFamily),
        };
        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
        {
            LOG(ERROR) << "failed to create command pool!";
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createCommandBuffers()
    {
        commandBuffers.resize(swapChainFramebuffers.size());

        /* 指令缓冲
        level成员变量用于指定分配的指令缓冲对象是主要指令缓冲对象还是辅助指令缓冲对象：
            VK_COMMAND_BUFFER_LEVEL_PRIMARY：
            可以被提交到队列进行执行，但不能被其它指令缓冲对象调用。
            VK_COMMAND_BUFFER_LEVEL_SECONDARY：
            不能直接被提交到队列进行执行，但可以被主要指令缓冲对象调用执行。

        在这里，我们没有使用辅助指令缓冲对象，但辅助治理给缓冲对象的好处是显而易见的，我们可以把一些常用的指令存储在辅助指令缓冲对象，然后在主要指令缓冲对象中调用执行
        */
        VkCommandBufferAllocateInfo allocInfo{
            .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .pNext              = nullptr,
            .commandPool        = commandPool,
            .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = static_cast<uint32_t>(commandBuffers.size()),
        };
        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        {
            LOG(ERROR) << "failed to allocate command buffers!";
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void recordCommandBuffer(VkCommandBuffer commandBuffer, const uint32_t imageIndex)
    {
        /* 记录指令到指令缓冲
               flags成员变量用于指定我们将要怎样使用指令缓冲。它的值可以是下面这些：
                   VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT：
                   指令缓冲在执行一次后，就被用来记录新的指令。
                   VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT：
                   这是一个只在一个渲染流程内使用的辅助指令缓冲。
                   VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT：
                   在指令缓冲等待执行时，仍然可以提交这一指令缓冲。

               在这里，我们使用了VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT标记，
               这使得我们可以在上一帧还未结束渲染时，提交下一帧的渲染指令。pInheritanceInfo成员
               变量只用于辅助指令缓冲，可以用它来指定从调用它的主要指令缓冲继承的状态。

               指令缓冲对象记录指令后，调用vkBeginCommandBuffer函数会重置指令缓冲对象。
               */

        // 启动指令缓冲
        VkCommandBufferBeginInfo beginInfo{
            .sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .pNext            = nullptr,
            .flags            = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
            .pInheritanceInfo = nullptr,
        };
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            LOG(ERROR) << "failed to begin recording command buffer!";
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        // 启动渲染流程
        VkClearValue          clearColor = {0.0F, 0.0F, 0.0F, 1.0F};
        VkRenderPassBeginInfo renderPassInfo{
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext           = nullptr,
            .renderPass      = renderPass,
            .framebuffer     = swapChainFramebuffers[imageIndex],
            .renderArea      = VkRect2D{.offset = {.x = 0, .y = 0}, .extent = swapChainExtent},
            .clearValueCount = 1,
            .pClearValues    = &clearColor,
        };
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        /*
        最后一个参数是用来指定渲染流程如何提供绘制指令的标记：
            VK_SUBPASS_CONTENTS_INLINE：
            所有要执行的指令都在主要指令缓冲中，没有辅助指令缓冲需要执行。
            VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS：
            有来自辅助指令缓冲的指令需要执行。
        */

        /* 绑定图形管线
        vkCmdBindPipeline函数的第二个参数用于指定管线对象是图形管线还是计算管线。
        */
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

        /* 绘制
        我们使用vkCmdDraw函数来提交绘制操作到指令缓冲，
        它的第一个参数是记录有要执行的指令的指令缓冲对象，它的剩余参数依次是：
            vertexCount：尽管这里我们没有使用顶点缓冲，但仍然需要指定三个顶点用于三角形的绘制。
            instanceCount：用于实例渲染，为1时表示不进行实例渲染。
            firstVertex：用于定义着色器变量gl_VertexIndex的值。
            firstInstance：用于定义着色器变量gl_InstanceIndex的值。
        */
        std::array<VkBuffer, 1>     vertexBuffers = {vertexBuffer};
        std::array<VkDeviceSize, 1> offsets       = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers.data(), offsets.data());
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
                                &descriptorSets[imageIndex], 0, nullptr);
        // vkCmdDraw(commandBuffer, vertices.size(), 1, 0, 0);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

        // 结束渲染流程，并提交指令
        vkCmdEndRenderPass(commandBuffer);
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            LOG(ERROR) << "failed to record command buffer!";
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void drawFrame()
    {
        /**NOTE - 渲染与呈现
        我们编写的drawFrame函数用于执行下面的操作：
            从交换链获取一张图像
            对帧缓冲附着执行指令缓冲中的渲染指令
            返回渲染后的图像到交换链进行呈现操作

        上面这些操作每一个都是通过一个函数调用设置的,但每个操作的实际执行却是异步进行的。
        函数调用会在操作实际结束前返回，并且操作的实际执行顺序也是不确定的。

        有两种用于同步交换链事件的方式：栅栏(fence)和信号量(semaphore)。

        栅栏(fence)和信号量(semaphore)的不同之处是，我们可以通过调用vkWaitForFences
        函数查询栅栏(fence)的状态，但不能查询信号量(semaphore)的状态。通常，我们使用
        栅栏(fence)来对应用程序本身和渲染操作进行同步。使用信号量(semaphore)来对一个
        指令队列内的操作或多个不同指令队列的操作进行同步。这里，我们想要通过指令队列
        中的绘制操作和呈现操作，显然，使用信号量(semaphore)更加合适。
        */

        /*
        vkWaitForFences函数可以用来等待一组栅栏(fence)中的一个或全部栅栏(fence)发出信号。
        VK_TRUE：指定它等待所有在数组中指定的栅栏(fence)。

        和信号量不同，等待栅栏发出信号后，需要调用vkResetFences函数手动将fence重置为未发出信号的状态。
        */
        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
        /* 不再此出重置fence
        假如重置fence后，重建了交换链，那么在下一个循环中vkWaitForFences()函数永远等不到singled的fence
        */

        // 从交换链获取图像
        uint32_t imageIndex = 0;
        VkResult result     = vkAcquireNextImageKHR(device, swapChain, std::numeric_limits<uint64_t>::max(),
                                                    imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        /* 检查交换链是否需要重建
            VK_ERROR_OUT_OF_DATE_KHR：交换链不能继续使用。通常发生在窗口大小改变后。
            VK_SUBOPTIMAL_KHR：交换链仍然可以使用，但表面属性已经不能准确匹配。
        */
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
        {
            framebufferResized = false;
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS)
        {
            LOG(ERROR) << "failed to acquire swap chain image!";
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // 渲染指令
        vkResetCommandBuffer(commandBuffers[imageIndex], /*VkCommandBufferResetFlagBits*/ 0);
        recordCommandBuffer(commandBuffers[imageIndex], imageIndex);
        updateUniformBuffer(imageIndex);

        // 提交指令
        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        /*
        如果在获取交换链不可继续使用后，立即跳出这一帧的渲染，会导致我们使用的栅栏
        (fence)处于我们不能确定得状态。所以，我们应该在重建交换链时，重置栅栏(fence)对象
        */
        std::array<VkSemaphore, 1>          waitSemaphores   = {imageAvailableSemaphores[currentFrame]};
        std::array<VkPipelineStageFlags, 1> waitStages       = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        std::array<VkSemaphore, 1>          signalSemaphores = {renderFinishedSemaphores[currentFrame]};
        VkSubmitInfo                        submitInfo{
                                   .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                                   .pNext                = nullptr,
                                   .waitSemaphoreCount   = 1,
                                   .pWaitSemaphores      = waitSemaphores.data(),
                                   .pWaitDstStageMask    = waitStages.data(),
                                   .commandBufferCount   = 1,  // 可以同时大批量提交数据
                                   .pCommandBuffers      = &commandBuffers[imageIndex],
                                   .signalSemaphoreCount = 1,
                                   .pSignalSemaphores    = &renderFinishedSemaphores[currentFrame],
        };
        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
        {
            LOG(ERROR) << "failed to submit draw command buffer!";
            throw std::runtime_error("failed to submit draw command buffer!");
        }
        /*
        这里，我们需要写入颜色数据到图像，所以我们指定等待图像管线到达可以写入颜色附着的管线阶段。
        waitStages数组中的条目和pWaitSemaphores中相同索引的信号量相对应。

        vkQueueSubmit函数的最后一个参数是一个可选的栅栏对象，
        可以用它同步提交的指令缓冲执行结束后要进行的操作。
        */

        // 呈现
        VkPresentInfoKHR presentInfo{
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .pNext              = nullptr,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &renderFinishedSemaphores[currentFrame],
            .swapchainCount     = 1,
            .pSwapchains        = &swapChain,
            .pImageIndices      = &imageIndex,
            .pResults           = nullptr,
        };
        result = vkQueuePresentKHR(presentQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) { recreateSwapChain(); }
        else if (result != VK_SUCCESS)
        {
            LOG(ERROR) << "failed to present swap chain image!";
            throw std::runtime_error("failed to present swap chain image!");
        }
        /*
        我们可以通过pResults成员变量获取每个交换链的呈现操作是否成功的信息。
        在这里，由于我们只使用了一个交换链，可以直接使用呈现函数的返回值来
        判断呈现操作是否成功，没有必要使用pResults。
        */

        /**NOTE -
        我们的应用程序的内存使用量一直在慢慢增加。这是由于我们的drawFrame函数以很快
        地速度提交指令，但却没有在下一次指令提交时检查上一次提交的指令是否已经执行结束。
        也就是说CPU提交指令快过GPU对指令的处理速度，造成GPU需要处理的指令大量堆积。
        更糟糕的是这种情况下，我们实际上对多个帧同时使用了相同的
        imageAvailableSemaphore和renderFinishedSemaphore信号量。

        最简单的解决上面这一问题的方法是使用vkQueueWaitIdle函数来等待上一次提交的指令
        结束执行，再提交下一帧的指令：
            vkQueueWaitIdle(presentQueue);
         */
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void createSemaphores()
    {
        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        // 创建信号量
        VkSemaphoreCreateInfo semaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
        };
        /*
        栅栏(fence)对象在创建后是未发出信号的状态。这就意味着如果我们没有在vkWaitForFences
        函数调用之前发出栅栏(fence)信号，vkWaitForFences函数调用将会一直处于等待状态。
        */
        VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = nullptr,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
            {
                LOG(ERROR) << "failed to create semaphores and fences!";
                throw std::runtime_error("failed to create semaphores and fences!");
            }
        }
    }

    void cleanupSwapChain()
    {
        // 清除帧缓冲对象
        for (auto& framebuffer : swapChainFramebuffers) { vkDestroyFramebuffer(device, framebuffer, nullptr); }

        /*
        对于指令池对象，我们不需要重建，
        只需要调用vkFreeCommandBuffers函数清除它分配的指令缓冲对象即可。
        */
        vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);
        // 清除图像视图
        for (auto& imageView : swapChainImageViews) { vkDestroyImageView(device, imageView, nullptr); }
        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }

    void recreateSwapChain()
    {
        int width  = 0;
        int height = 0;
        while (width == 0 || height == 0)
        {
            glfwGetFramebufferSize(window, &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(device);

        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandBuffers();
        /** NOTE - 重建交换链
        至此，我们就完成了交换链重建的所有工作！但是，我们使用的这一重建方法需要等待正在
        执行的所有设备操作结束才能进行。实际上，是可以在渲染操作执行，原来的交换链仍在使
        用时重建新的交换链，只需要在创建新的交换链时使用VkSwapchainCreateInfoKHR结构
        体的oldSwapChain成员变量引用原来的交换链即可。之后，在旧的交换链结束使用时就可
        以清除它。
        */
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
    {
        auto* app               = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    void createVertexBuffer()
    {
        // VkBufferCreateInfo bufferInfo{
        //     .sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        //     .pNext                 = nullptr,
        //     .flags                 = 0,
        //     .size                  = sizeof(vertices[0]) * vertices.size(),
        //     .usage                 = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        //     .sharingMode           = VK_SHARING_MODE_EXCLUSIVE,
        //     .queueFamilyIndexCount = 0,
        //     .pQueueFamilyIndices   = nullptr,
        // };
        // if (vkCreateBuffer(device, &bufferInfo, nullptr, &vertexBuffer) != VK_SUCCESS)
        // {
        //     LOG(ERROR) << "failed to create vertex buffer!";
        //     throw std::runtime_error("failed to create vertex buffer!");
        // }

        // // 分配缓冲内存前，我们需要获取缓冲的内存需求
        // VkMemoryRequirements memRequirements;
        // vkGetBufferMemoryRequirements(device, vertexBuffer, &memRequirements);
        // // 分配
        // VkMemoryAllocateInfo allocInfo{
        //     .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        //     .pNext           = nullptr,
        //     .allocationSize  = memRequirements.size,
        //     .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        //                                                                           VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
        // };
        // if (vkAllocateMemory(device, &allocInfo, nullptr, &vertexBufferMemory) != VK_SUCCESS)
        // {
        //     LOG(ERROR) << "failed to allocate vertex buffer memory!";
        //     throw std::runtime_error("failed to allocate vertex buffer memory!");
        // };
        // /* 把申请到的GPU内存绑定到缓冲对象上
        // 第四个参数是偏移值。这里我们将内存用作顶点缓冲，可以将其设置为0。
        // 偏移值需要满足能够被memRequirements.alignment整除。
        // */
        // vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        VkBuffer       stagingBuffer       = nullptr;
        VkDeviceMemory stagingBufferMemory = nullptr;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
                     stagingBufferMemory);

        // 将顶点数据复制到缓冲中
        void* data = nullptr;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(device, stagingBufferMemory);
        /*
        现在可以使用memcpy将顶点数据复制到映射后的内存，然后调用vkUnmapMemory函数来
        结束内存映射。然而，驱动程序可能并不会立即复制数据到缓冲关联的内存中去，这是由于
        现代处理器都存在缓存这一设计，写入内存的数据并不一定在多个核心同时可见，有下面两
        种方法可以保证数据被立即复制到缓冲关联的内存中去：

        （处理器在读取或写入数据时，通常会首先访问Cache，而不是直接访问较慢的主内存。
        由于每个处理器核心可能都有自己的Cache，当一个核心写入数据到它的Cache时，其他核心
        并不一定能立即看到这些变化，因为它们访问的可能是自己的Cache，而不是共享的主内存。
        因此，写入内存的数据并不一定在多个核心同时可见。这种现象被称为缓存一致性问题。）

            1. 使用带有VK_MEMORY_PROPERTY_HOST_COHERENT_BIT属性的内存类型，
            保证内存可见的一致性

            2. 当你写入数据到映射的内存后，调用vkFlushMappedMemoryRanges函数可以手动
            将缓存中的数据刷新到设备内存中。类似地，在读取映射的内存数据前，调用vkInvalid
            ateMappedMemoryRanges函数可以确保读取到最新的数据。这种方法需要手动控制
            缓存的刷新和无效化，但可以在某些情况下提供更好的性能。
        */

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);
        copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
        /*
        vertexBuffer现在关联的内存是设备所有的，不能vkMapMemory函数对它关联的内存进行映射。
        我们只能通过stagingBuffer来向vertexBuffer复制数据。我们需要使用标记指明我们使用缓冲
        进行传输操作。
         */

        // 清除我们使用的缓冲对象和它关联的内存对象
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        /*
        函数返回的VkPhysicalDeviceMemoryProperties结构体包含了memoryTypes和memoryHeaps
        两个数组成员变量。memoryHeaps数组成员变量中的每个元素是一种内存来源，比如显存以及
        显存用尽后的位于主内存种的交换空间
        */

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if (((typeFilter & (1 << i)) != 0U) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
            /*
            我们需要位域满足VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT(用于从CPU写入数据)
            和VK_MEMORY_PROPERTY_HOST_COHERENT_BIT(后面介绍原因)的内存类型。

            由于我们不只一个需要的内存属性，所以仅仅检测位与运算的结果是否非0是不够的，
            还需要检测它是否与我们需要的属性的位域完全相同。
            */
        }
        LOG(ERROR) << "failed to find suitable memory type!";
        throw std::runtime_error("failed to find suitable memory type!");
    }

    void createBuffer(VkDeviceSize          size,
                      VkBufferUsageFlags    usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer&             buffer,
                      VkDeviceMemory&       bufferMemory)
    {
        VkBufferCreateInfo bufferInfo{
            .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size        = size,
            .usage       = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = memRequirements.size,
            .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties),
        };
        if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        vkBindBufferMemory(device, buffer, bufferMemory, 0);
    }

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
    {
        // VkCommandBufferAllocateInfo allocInfo{
        //     .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        //     .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        //     .commandPool        = commandPool,
        //     .commandBufferCount = 1,
        // };
        // VkCommandBuffer commandBuffer = nullptr;
        // vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        // VkCommandBufferBeginInfo beginInfo{
        //     .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        //     .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        // };
        // /*
        // 我们可以使用VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT标记告诉
        // 驱动程序我们如何使用这个指令缓冲，来让驱动程序进行更好的优化。
        //  */
        // vkBeginCommandBuffer(commandBuffer, &beginInfo);

        // VkBufferCopy copyRegion{
        //     .srcOffset = 0,
        //     .dstOffset = 0,
        //     .size      = size,
        // };
        // vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        // vkEndCommandBuffer(commandBuffer);

        // VkSubmitInfo submitInfo{
        //     .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        //     .commandBufferCount = 1,
        //     .pCommandBuffers    = &commandBuffer,
        // };

        // vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        // vkQueueWaitIdle(graphicsQueue);

        // vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);

        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferCopy copyRegion = {};
        copyRegion.size         = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    void createIndexBuffer()
    {
        VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        VkBuffer       stagingBuffer       = nullptr;
        VkDeviceMemory stagingBufferMemory = nullptr;
        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
                     stagingBufferMemory);

        void* data = nullptr;
        vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(device, stagingBufferMemory);

        createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

        copyBuffer(stagingBuffer, indexBuffer, bufferSize);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void createDescriptorSetLayout()
    {
        VkDescriptorSetLayoutBinding uboLayoutBinding{
            .binding            = 0,
            .descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount    = 1,
            .stageFlags         = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr,
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo{
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 1,
            .pBindings    = &uboLayoutBinding,
        };
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    void createUniformBuffer()
    {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers.resize(swapChainImages.size());
        uniformBuffersMemory.resize(swapChainImages.size());

        for (size_t i = 0; i < swapChainImages.size(); i++)
        {
            createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i],
                         uniformBuffersMemory[i]);
        }
    }

    void updateUniformBuffer(uint32_t currentImage)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto  currentTime = std::chrono::high_resolution_clock::now();
        float time        = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{
            .model = glm::rotate(glm::mat4(1), time * glm::radians(90.0f), glm::vec3(0, 0, 1)),
            .view  = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            .proj  = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f,
                                      10.0f),
        };
        ubo.proj[1][1] *= -1;  // GLM库最初是为OpenGL设计的，它的裁剪坐标的Y轴和Vulkan是相反的

        void* data = nullptr;
        vkMapMemory(device, uniformBuffersMemory[currentImage], 0, sizeof(ubo), 0, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(device, uniformBuffersMemory[currentImage]);
    }

    void createDescriptorPool()
    {
        VkDescriptorPoolSize poolSize{
            .type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = static_cast<uint32_t>(swapChainImages.size()),
        };

        VkDescriptorPoolCreateInfo poolInfo{
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .poolSizeCount = 1,
            .pPoolSizes    = &poolSize,
            .maxSets       = static_cast<uint32_t>(swapChainImages.size()),
        };
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void createDescriptorSets()
    {
        std::vector<VkDescriptorSetLayout> layouts(swapChainImages.size(), descriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = descriptorPool,
            .descriptorSetCount = static_cast<uint32_t>(swapChainImages.size()),
            .pSetLayouts        = layouts.data(),
        };
        descriptorSets.resize(swapChainImages.size());
        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        // 描述符集对象创建后，还需要进行一定地配置
        for (size_t i = 0; i < swapChainImages.size(); i++)
        {
            // 通过VkDescriptorBufferInfo结构体来配置描述符引用的缓冲对象
            VkDescriptorBufferInfo bufferInfo{
                .buffer = uniformBuffers[i],
                .offset = 0,
                .range  = sizeof(UniformBufferObject),
            };

            VkWriteDescriptorSet descriptorWrite{
                .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet           = descriptorSets[i],
                .dstBinding       = 0,
                .dstArrayElement  = 0,
                .descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount  = 1,
                .pBufferInfo      = &bufferInfo,
                .pImageInfo       = nullptr,  // Optional
                .pTexelBufferView = nullptr,  // Optional
            };
            vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        }
    }

    void createTextureImage()
    {
        /*
        最常用的变换图像布局的方式是使用管线屏障(pipeline barrier)。管线屏障(pipeline
        barrier)主要被用来同步资源访问，比如保证图像在被读取之前数据被写入。它也可以被用来变换图像布局。在本章节，我们使用它进行图像布局变换。如果队列的所有模式为VK_SHARING_MODE_EXCLUSIVE，管线屏障(pipeline
        barrier)还可以被用来传递队列所有权。
         */
        int          texWidth, texHeight, texChannels;
        stbi_uc*     pixels    = stbi_load("textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        VkDeviceSize imageSize = texWidth * texHeight * 4;

        if (pixels == nullptr) { throw std::runtime_error("failed to load texture image!"); }

        VkBuffer       stagingBuffer       = nullptr;
        VkDeviceMemory stagingBufferMemory = nullptr;
        createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
                     stagingBufferMemory);

        void* data = nullptr;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(device, stagingBufferMemory);

        stbi_image_free(pixels);

        // // 创建纹理图像
        // VkImageCreateInfo imageInfo{
        //     .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        //     .imageType     = VK_IMAGE_TYPE_2D,
        //     .extent.width  = static_cast<uint32_t>(texWidth),
        //     .extent.height = static_cast<uint32_t>(texHeight),
        //     .extent.depth  = 1,
        //     .mipLevels     = 1,
        //     .arrayLayers   = 1,
        //     .format        = VK_FORMAT_R8G8B8A8_UNORM,
        //     .tiling        = VK_IMAGE_TILING_OPTIMAL,
        //     .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        //     .usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        //     .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        //     .samples       = VK_SAMPLE_COUNT_1_BIT,
        //     .flags         = 0,  // Optional
        // };
        // /**
        // 我们是将图像对象作为传输数据的接收方，将纹理数据从缓冲对象传输到图像对象，
        // 所以我们不需要保留图像对象第一次变换时的纹理数据，使用VK_IMAGE_LAYOUT_
        // UNDEFINED更好。

        // 有许多用于稀疏图像的优化标记可以使用。稀疏图像是一种离散存储图像数据的方法。
        // 比如，我们可以使用稀疏图像来存储体素地形，避免为"空气"部分分配内存。在这里，
        // 我们没有使用flags标记，将其设置为默认值0。
        //  */
        // if (vkCreateImage(device, &imageInfo, nullptr, &textureImage) != VK_SUCCESS)
        // {
        //     throw std::runtime_error("failed to create image!");
        // }

        // // 为图像对象分配内存
        // VkMemoryRequirements memRequirements;
        // vkGetImageMemoryRequirements(device, textureImage, &memRequirements);

        // VkMemoryAllocateInfo allocInfo{
        //     .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        //     .allocationSize  = memRequirements.size,
        //     .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
        // };
        // if (vkAllocateMemory(device, &allocInfo, nullptr, &textureImageMemory) != VK_SUCCESS)
        // {
        //     throw std::runtime_error("failed to allocate image memory!");
        // }

        // vkBindImageMemory(device, textureImage, textureImageMemory, 0);

        createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    textureImage, textureImageMemory);

        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth),
                          static_cast<uint32_t>(texHeight));

        transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
    }

    void createImage(uint32_t              width,
                     uint32_t              height,
                     VkFormat              format,
                     VkImageTiling         tiling,
                     VkImageUsageFlags     usage,
                     VkMemoryPropertyFlags properties,
                     VkImage&              image,
                     VkDeviceMemory&       imageMemory)
    {
        VkImageCreateInfo imageInfo = {};
        imageInfo.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType         = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width      = width;
        imageInfo.extent.height     = height;
        imageInfo.extent.depth      = 1;
        imageInfo.mipLevels         = 1;
        imageInfo.arrayLayers       = 1;
        imageInfo.format            = format;
        imageInfo.tiling            = tiling;
        imageInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage             = usage;
        imageInfo.samples           = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo = {};
        allocInfo.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize       = memRequirements.size;
        allocInfo.memoryTypeIndex      = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(device, image, imageMemory, 0);
    }

    VkCommandBuffer beginSingleTimeCommands()
    {
        VkCommandBufferAllocateInfo allocInfo = {};
        allocInfo.sType                       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level                       = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool                 = commandPool;
        allocInfo.commandBufferCount          = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType                    = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void endSingleTimeCommands(VkCommandBuffer commandBuffer)
    {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo       = {};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &commandBuffer;

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
    }

    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
    {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region{
            .bufferOffset                    = 0,
            .bufferRowLength                 = 0,
            .bufferImageHeight               = 0,
            .imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .imageSubresource.mipLevel       = 0,
            .imageSubresource.baseArrayLayer = 0,
            .imageSubresource.layerCount     = 1,
            .imageOffset                     = {0, 0, 0},
            .imageExtent                     = {width, height, 1},
        };
        vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        endSingleTimeCommands(commandBuffer);
    }

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        /**
        通过图像内存屏障(image memory barrier)我们可以对图像布局进行变换。管线屏障(pipeline
        barrier)主要被用来同步资源访问，比如保证图像在被读取之前数据被写入。
        它也可以被用来变换图像布局。在本章节，我们使用它进行图像布局变换。
        如果队列的所有模式为VK_SHARING_MODE_EXCLUSIVE，管线屏障(pipeline
        barrier)还可以被用来传递队列所有权。对于缓冲对象也有一个可以实现同样效果的缓冲内存屏障(buffer memory barrier)。
         */

        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkImageMemoryBarrier barrier{
            .sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout                       = oldLayout,
            .newLayout                       = newLayout,
            .srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED,
            .image                           = image,
            .subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel   = 0,
            .subresourceRange.levelCount     = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount     = 1,
            .srcAccessMask                   = 0,  // TODO
            .dstAccessMask                   = 0,  // TODO
        };

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

            sourceStage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else { throw std::invalid_argument("unsupported layout transition!"); }
        /*
        传输的写入操作必须在管线传输阶段进行。这里因为我们的写入操作不需要等待任何对象，
        我们可以指定一个空的的访问掩码，使用VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT指定最早
        出现的管线阶段。需要注意VK_PIPELINE_STAGE_TRANSFER_BIT并非图形和计算管线中真
        实存在的管线阶段，它实际上是一个伪阶段，出现在传输操作发生时。

        图像数据需要在片段着色器读取，所以我们指定了片段着色器管线阶段的读取访问掩码。
        */

        vkCmdPipelineBarrier(commandBuffer, 0 /* TODO */, 0 /* TODO */, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        endSingleTimeCommands(commandBuffer);
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
        std::shared_ptr<HelloTriangleApplication> app(new HelloTriangleApplication());
        app->run();
    }
    catch (const std::exception& e)
    {
        // 执行过程中，如果发生错误，会抛出一个带有错误描述信息的std::runtime_error异常，
        // 为了处理多种不同类型的异常，我们使用更加通用的std::exception来接受异常。
        LOG(ERROR) << e.what();
        throw std::runtime_error(e.what());
        return EXIT_FAILURE;
    }

    google::ShutdownGoogleLogging();
    return EXIT_SUCCESS;
}
