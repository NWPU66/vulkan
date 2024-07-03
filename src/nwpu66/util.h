#pragma once
#include <cstdint>

#include <memory>
#include <string>
#include <fstream>

#include <GLFW/glfw3.h>
#include <glog/logging.h>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

/**
 * @brief
 */
inline std::string vulkan_apiVersion(uint32_t apiVersion)
{
    uint32_t majorVersion = (apiVersion >> 22) & 0x3FF;
    uint32_t minorVersion = (apiVersion >> 12) & 0x3FF;
    uint32_t patchVersion = apiVersion & 0xFFF;
    return std::to_string(majorVersion) + "." + std::to_string(minorVersion) + "." + std::to_string(patchVersion);
}

/**
 * @brief
 *
 */
class Result_T {
private:
    VkBool32 result_code = VK_RESULT_MAX_ENUM;

    void check_success() const
    {
        if (is_error())
        {
            LOG(ERROR) << "Vulkan API error! error code: " << result_code;
            throw std::runtime_error("Vulkan API error");
        }
    }

public:
    Result_T();
    explicit Result_T(VkBool32 _result_code) : result_code(_result_code) { check_success(); }

    VkBool32 get_result_code() const { return result_code; }
    bool     is_success() const { return result_code == VK_SUCCESS; }
    bool     is_error() const { return result_code != VK_SUCCESS; }

    Result_T& operator=(VkBool32 _result_code)
    {
        result_code = _result_code;
        check_success();
        return *this;
    }
};

/**
 * @brief 打印Vulkan支持的实例级层和扩展
 *
 */
void print_instanceLevel_layer_extension()
{
    uint32_t                           layer_count     = 0;
    uint32_t                           extension_count = 0;
    std::vector<VkExtensionProperties> extension_properties;
    std::vector<VkLayerProperties>     layer_properties;
    Result_T(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr));
    extension_properties.resize(extension_count);
    Result_T(vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extension_properties.data()));
    Result_T(vkEnumerateInstanceLayerProperties(&layer_count, nullptr));
    layer_properties.resize(layer_count);
    Result_T(vkEnumerateInstanceLayerProperties(&layer_count, layer_properties.data()));

    LOG(INFO) << "Instance Level Layer and Extension:";
    LOG(INFO) << "Instance Level Layer Count: " << layer_count;
    for (const auto& layer_property : layer_properties)
    {
        LOG(INFO) << "  Layer Name: " << layer_property.layerName;
        // LOG(INFO) << "  Spec Version: " << layer_property.specVersion;
    }
    LOG(INFO) << "Instance Level Extension Count: " << extension_count;
    for (const auto& extension_property : extension_properties)
    {
        LOG(INFO) << "  Extension Name: " << extension_property.extensionName;
    }
}

/**
 * @brief 获取GLFW扩展
 *
 */
inline std::shared_ptr<std::vector<const char*>> get_glfw_extension()
{
    uint32_t     glfw_extension_count = 0;
    const char** glfw_extensions      = nullptr;
    glfw_extensions                   = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    std::shared_ptr<std::vector<const char*>> extensions(
        new std::vector<const char*>(glfw_extensions, glfw_extensions + glfw_extension_count));

    return extensions;
}

/**
 * @brief 查询vulkan api的版本
 *
 */
inline void basic_vulkan_info()
{
    /*
    实例级：vkEnumerateInstanceVersion
    设备级：vkGetPhysicalDeviceProperties or vkGetPhysicalDeviceProperties2
    */
    uint32_t instanceLevel_apiVersion = 0;
    vkEnumerateInstanceVersion(&instanceLevel_apiVersion);
    LOG(INFO) << "instanceLevel_apiVersion: " << vulkan_apiVersion(instanceLevel_apiVersion);
    print_instanceLevel_layer_extension();
}

/**
 * @brief
 *
 */
void print_avaliable_physicalDevice(VkInstance instance)
{
    uint32_t                      physical_device_count = 0;
    std::vector<VkPhysicalDevice> physical_devices;
    Result_T(vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr));
    physical_devices.resize(physical_device_count);
    Result_T(vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data()));

    // 打印设备名字
    for (const auto& physical_device : physical_devices)
    {
        VkPhysicalDeviceProperties physical_device_properties;
        vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);
        LOG(INFO) << "Device Name: " << physical_device_properties.deviceName;
        LOG(INFO) << "Device Type: " << physical_device_properties.deviceType;
        LOG(INFO) << "Device ID: " << physical_device_properties.deviceID;
    }
}

/**
 * @brief
 *
 */
VkPhysicalDevice find_named_physicalDevice(VkInstance instance, const std::string& physicalDevice_name)
{
    uint32_t                      physicalDevice_count = 0;
    std::vector<VkPhysicalDevice> physicalDevices;
    Result_T(vkEnumeratePhysicalDevices(instance, &physicalDevice_count, nullptr));
    physicalDevices.resize(physicalDevice_count);
    Result_T(vkEnumeratePhysicalDevices(instance, &physicalDevice_count, physicalDevices.data()));

    // 打印设备名字
    VkPhysicalDevice found_physicalDevice = nullptr;
    for (const auto& physicalDevice : physicalDevices)
    {
        VkPhysicalDeviceProperties physicalDevice_properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDevice_properties);
        LOG(INFO) << "Device Name: " << physicalDevice_properties.deviceName;
        LOG(INFO) << "Device Type: " << physicalDevice_properties.deviceType;
        LOG(INFO) << "Device ID: " << physicalDevice_properties.deviceID;

        if (physicalDevice_properties.deviceName == physicalDevice_name) { found_physicalDevice = physicalDevice; }
    }

    return found_physicalDevice;
}

void print_avaliable_surface_detail(const VkSurfaceCapabilitiesKHR&        surface_capabilities,
                                    const std::vector<VkSurfaceFormatKHR>& surface_formats,
                                    const std::vector<VkPresentModeKHR>&   surface_presentModes)
{
    LOG(INFO) << "Surface Capabilities: ";
    LOG(INFO) << "  minImageCount: " << surface_capabilities.minImageCount;
    LOG(INFO) << "  maxImageCount: " << surface_capabilities.maxImageCount;
    LOG(INFO) << "  currentExtent: " << surface_capabilities.currentExtent.width << ", "
              << surface_capabilities.currentExtent.height;
    LOG(INFO) << "  minImageExtent: " << surface_capabilities.minImageExtent.width << ", "
              << surface_capabilities.minImageExtent.height;
    LOG(INFO) << "  maxImageExtent: " << surface_capabilities.maxImageExtent.width << ", "
              << surface_capabilities.maxImageExtent.height;
    LOG(INFO) << "  maxImageArrayLayers: " << surface_capabilities.maxImageArrayLayers;
    LOG(INFO) << "  supportedTransforms: " << surface_capabilities.supportedTransforms;
    LOG(INFO) << "  currentTransform: " << surface_capabilities.currentTransform;
    LOG(INFO) << "  supportedCompositeAlpha: " << surface_capabilities.supportedCompositeAlpha;
    LOG(INFO) << "  supportedUsageFlags: " << surface_capabilities.supportedUsageFlags;

    LOG(INFO) << "Surface Formats: ";
    for (const auto& surface_format : surface_formats)
    {
        LOG(INFO) << "  format: " << surface_format.format << "   " << "  colorSpace: " << surface_format.colorSpace;
    }

    LOG(INFO) << "Surface Present Modes: ";
    for (const auto& surface_presentMode : surface_presentModes)
    {
        LOG(INFO) << "  presentMode: " << surface_presentMode;
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT             messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                    void*                                       pUserData)
{
    LOG(ERROR) << "validation layer: " << pCallbackData->pMessage;
    return VK_FALSE;
}

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
