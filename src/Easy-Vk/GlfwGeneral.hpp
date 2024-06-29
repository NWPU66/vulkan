#pragma once
#include "EasyVKStart.h"
#include "VKBase.h"

// GLFW
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
// #pragma comment(lib, "glfw3.lib")

// global variables
GLFWwindow*  pWindow     = nullptr;   // 窗口的指针，全局变量自动初始化为NULL
GLFWmonitor* pMonitor    = nullptr;   // 显示器信息的指针
const char*  windowTitle = "EasyVK";  // 窗口标题

/**
 * @brief 初始化Vulkan
 *
 * @return true
 * @return false
 */
bool InitializeVulkan(bool limitFrameRate = true)
{
    // 向GraphicsBase添加GLFW实例级扩展
    uint32_t     extensionCount = 0;
    const char** extensionNames = nullptr;
    extensionNames              = glfwGetRequiredInstanceExtensions(&extensionCount);
    if (extensionNames == nullptr)
    {
        LOG(ERROR) << "[ InitializeWindow ]\nVulkan is not available on this machine!";
        glfwTerminate();
        return false;
    }
    for (size_t i = 0; i < extensionCount; i++) { GraphicsBase::Base().AddInstanceExtension(extensionNames[i]); }
    // 向GraphicsBase添加GLFW设备级扩展
    GraphicsBase::Base().AddDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    // 创建Vulkan Instance
    GraphicsBase::Base().UseLatestApiVersion();
    if (GraphicsBase::Base().CreateInstance() != 0)
    {
        LOG(ERROR) << "[ InitializeWindow ]\nFailed to create a Vulkan instance!";
        return false;
    }

    // 创建Surface
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (VkResult result = glfwCreateWindowSurface(GraphicsBase::Base().Instance(), pWindow, nullptr, &surface))
    {
        LOG(ERROR) << "[ InitializeWindow ]\nFailed to create a window surface!";
        glfwTerminate();
        return false;
    }
    GraphicsBase::Base().Surface(surface);

    // 查找物理设备并创建逻辑设备
    if ((GraphicsBase::Base().GetPhysicalDevices() != 0)  // 获取物理设备，并使用列表中的第一个物理设备
        || (GraphicsBase::Base().DeterminePhysicalDevice(0, true, false) != 0)  // 暂时不需要计算用的队列
        || (GraphicsBase::Base().CreateDevice() != 0))                          // 创建逻辑设备
    {
        return false;
    }

    // 创建交换链
    if (GraphicsBase::Base().CreateSwapchain(limitFrameRate) != 0) { return false; }

    return true;
}

/**
 * @brief 初始化GLFW窗口
 *
 */
bool InitializeWindow(VkExtent2D size, bool fullScreen = false, bool isResizable = true, bool limitFrameRate = true)
{
    if (glfwInit() == 0)
    {
        LOG(ERROR) << "[ InitializeWindow ] ERROR\nFailed to initialize GLFW!";
        return false;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // 不需要OpenGL的API
    glfwWindowHint(GLFW_RESIZABLE, static_cast<int>(isResizable));

    // 全屏模式
    pMonitor                 = glfwGetPrimaryMonitor();     // 显示器信息
    const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);  // 显示器当前的视频模式
    // 创建GLFW窗口
    pWindow = (fullScreen) ? glfwCreateWindow(pMode->width, pMode->height, windowTitle, pMonitor, nullptr) :
                             glfwCreateWindow(size.width, size.height, windowTitle, nullptr, nullptr);
    /* glfwCreateWindow()
    第四个参数用于指定全屏模式的显示器，若为nullptr则使用窗口模式，
    第五个参数可传入一个其他窗口的指针，用于与其他窗口分享内容。
    */
    if (pWindow == nullptr)
    {
        LOG(ERROR) << "[ InitializeWindow ]\nFailed to create a glfw window!";
        glfwTerminate();
        return false;
    }

    // 初始化Vulkan
    if (!InitializeVulkan(limitFrameRate))
    {
        LOG(ERROR) << "[ InitializeWindow ]\nFailed to initialize Vulkan!";
        glfwTerminate();
        return false;
    }
    return true;
}

/**
 * @brief 终止窗口时，清理GLFW
 *
 */
void TerminateWindow()
{
    GraphicsBase::Base().WaitIdle();
    glfwTerminate();
}

/**
 * @brief 在窗口标题上显示帧率
 * @note 代码逻辑：记录时间点t0，若之后某次调用该函数时取得的时间t1已超过t0一秒，
 * 用t1与t0的差除以这中间经历的帧数，得到帧率，并将t1赋值给t0。
 */
void TitleFps()
{
    static double            time0 = glfwGetTime();
    static double            time1;
    static double            deltaTime;
    static int               deltaFrame = -1;
    static std::stringstream info;
    time1 = glfwGetTime();
    deltaFrame++;
    deltaTime = time1 - time0;
    if (deltaTime >= 1)
    {
        info.precision(1);
        info << windowTitle << "    " << std::fixed << deltaFrame / deltaTime << " FPS";
        glfwSetWindowTitle(pWindow, info.str().c_str());
        info.str("");  // 别忘了在设置完窗口标题后清空所用的stringstream
        time0      = time1;
        deltaFrame = 0;
    }
}

/**
 * @brief
 *
 */
void MakeWindowFullScreen()
{
    const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
    glfwSetWindowMonitor(pWindow, pMonitor, 0, 0, pMode->width, pMode->height, pMode->refreshRate);
}

/**
 * @brief
 *
 * @param position
 * @param size
 */
void MakeWindowWindowed(VkOffset2D position, VkExtent2D size)
{
    const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
    glfwSetWindowMonitor(pWindow, nullptr, position.x, position.y, size.width, size.height, pMode->refreshRate);
}
