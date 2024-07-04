#include <array>
#include <cstdint>
#include <cstdlib>

#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glog/logging.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "util.h"

// ANCHOR - global variables
static const int                screen_width          = 1280;
static const int                screen_height         = 720;
static GLFWwindow*              window                = nullptr;
static const std::string        physicalDevice_name   = "NVIDIA GeForce RTX 3060 Laptop GPU";
static std::vector<const char*> instanceLevel_layer   = {};
static std::vector<const char*> deviceLevel_extension = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

// ANCHOR - Vulkan componment
VkInstance               instance       = nullptr;
VkDebugUtilsMessengerEXT debugMessenger = nullptr;
VkSurfaceKHR             surface        = nullptr;
VkPhysicalDevice         physicalDevice = nullptr;
struct QueueFamilies
{
    uint32_t graphics = std::numeric_limits<uint32_t>::max();
    uint32_t present  = std::numeric_limits<uint32_t>::max();
    uint32_t compute  = std::numeric_limits<uint32_t>::max();

    VkQueue graphics_handle = nullptr;
    VkQueue present_handle  = nullptr;
    VkQueue compute_handle  = nullptr;

    bool isComplete() const
    {
        return graphics != std::numeric_limits<uint32_t>::max()     //
               && present != std::numeric_limits<uint32_t>::max()   //
               && compute != std::numeric_limits<uint32_t>::max();  //
    }
} queueFamilies;
VkDevice                 device    = nullptr;
VkSwapchainKHR           swapChain = nullptr;
std::vector<VkImage>     swapChain_images;
std::vector<VkImageView> swapChain_imageViews;

// ANCHOR - vulkan pipeline object
VkCommandPool    commandPool    = nullptr;
VkCommandBuffer  commandBuffer  = nullptr;
VkRenderPass     renderPass     = nullptr;
VkPipelineLayout pipelineLayout = nullptr;
VkPipeline       pipeline       = nullptr;

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

    // ANCHOR - init glfw
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // 阻止GLFW它创建OpenGL上下文
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);    // 禁止窗口大小改变
        window = glfwCreateWindow(screen_width, screen_height, "hello_triangle", nullptr, nullptr);
        if (window == nullptr)
        {
            LOG(ERROR) << "Failed to create GLFW window";
            glfwTerminate();
            return EXIT_FAILURE;
        }
    }

    // SECTION - init vulkan componment
    {
        // ANCHOR -  create vulkan instance
        basic_vulkan_info();

        VkApplicationInfo appInfo{
            .sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext              = nullptr,
            .pApplicationName   = "vulkan app",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName        = "vulkan engine",
            .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion         = VK_API_VERSION_1_3,
        };

        // glfw extension and layer
        auto instanceLevel_extension = get_glfw_extension();
#ifdef NDEBUG
#else
        instanceLevel_extension->push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        instanceLevel_layer.push_back("VK_LAYER_KHRONOS_validation");
#endif

        // 创建vkinstance实例
        VkInstanceCreateInfo instance_createInfo{
            .sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .pApplicationInfo        = &appInfo,
            .enabledLayerCount       = static_cast<uint32_t>(instanceLevel_layer.size()),
            .ppEnabledLayerNames     = instanceLevel_layer.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(instanceLevel_extension->size()),
            .ppEnabledExtensionNames = instanceLevel_extension->data(),
        };
        Result_T(vkCreateInstance(&instance_createInfo, nullptr, &instance));

        // 配置校验层回调函数
#ifdef NDEBUG
#else
        VkDebugUtilsMessengerCreateInfoEXT debugManeger_createInfo{
            .sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
            .pNext           = nullptr,
            .flags           = 0,
            .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
            .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
            .pfnUserCallback = debugCallback,
            .pUserData       = nullptr,
        };
        auto vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        Result_T(vkCreateDebugUtilsMessengerEXT(instance, &debugManeger_createInfo, nullptr, &debugMessenger));
#endif

        // ANCHOR - 创建表面界面
        Result_T(glfwCreateWindowSurface(instance, window, nullptr, &surface));

        // ANCHOR - 挑选物理设备
        physicalDevice = find_named_physicalDevice(instance, physicalDevice_name);
        // 寻找队列族
        {
            uint32_t queueFamily_count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamily_count, nullptr);
            std::vector<VkQueueFamilyProperties> queueFamily_properties(queueFamily_count);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamily_count, queueFamily_properties.data());

            for (int i = 0; i < queueFamily_count; ++i)
            {
                // 检查图形与计算队列
                bool support_graphics = (queueFamily_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U;
                bool support_compute  = (queueFamily_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0U;
                // 检查设备的呈现能力
                VkBool32 support_presentation = VK_FALSE;
                Result_T(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &support_presentation));

                if (support_graphics && support_compute && support_presentation == VK_TRUE)
                {
                    queueFamilies.graphics = i;
                    queueFamilies.compute  = i;
                    queueFamilies.present  = i;
                    break;
                }
            }
        }
        // 检查设备级扩展
        {
            uint32_t extension_count = 0;
            Result_T(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extension_count, nullptr));
            std::vector<VkExtensionProperties> avaliable_extension(extension_count);
            Result_T(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extension_count,
                                                          avaliable_extension.data()));

            std::set<std::string> required_extension(deviceLevel_extension.begin(), deviceLevel_extension.end());
            for (const auto& extension : avaliable_extension) { required_extension.erase(extension.extensionName); }
            if (!required_extension.empty()) { LOG(ERROR) << "required device level extension not found"; }
        }

        // ANCHOR - 创建逻辑设备
        float              queuePriority      = 1.0F;
        std::set<uint32_t> unique_queueFamily = {
            queueFamilies.graphics,
            queueFamilies.compute,
            queueFamilies.present,
        };
        std::vector<VkDeviceQueueCreateInfo> deviceQueue_createInfos;
        for (const auto& queue_family : unique_queueFamily)
        {
            deviceQueue_createInfos.push_back(VkDeviceQueueCreateInfo{
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                // .queueFamilyIndex=,
                .queueCount       = 1,
                .pQueuePriorities = &queuePriority,
            });
        }

        VkDeviceCreateInfo device_createInfo{
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext                   = nullptr,
            .flags                   = 0,
            .queueCreateInfoCount    = static_cast<uint32_t>(deviceQueue_createInfos.size()),
            .pQueueCreateInfos       = deviceQueue_createInfos.data(),
            .enabledLayerCount       = static_cast<uint32_t>(instanceLevel_layer.size()),
            .ppEnabledLayerNames     = instanceLevel_layer.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(deviceLevel_extension.size()),
            .ppEnabledExtensionNames = deviceLevel_extension.data(),
        };
        Result_T(vkCreateDevice(physicalDevice, &device_createInfo, nullptr, &device));

        // 获取队列句柄
        vkGetDeviceQueue(device, queueFamilies.graphics, 0, &queueFamilies.graphics_handle);
        vkGetDeviceQueue(device, queueFamilies.compute, 0, &queueFamilies.compute_handle);
        vkGetDeviceQueue(device, queueFamilies.present, 0, &queueFamilies.present_handle);

        // ANCHOR - 创建交换链
        // 选择合适的交换链格式
        VkSurfaceCapabilitiesKHR        surface_capabilities;
        std::vector<VkSurfaceFormatKHR> surface_formats;
        std::vector<VkPresentModeKHR>   surface_presentModes;
        {
            // 基础表面特性
            Result_T(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surface_capabilities));

            // 表面支持的格式
            uint32_t format_count = 0;
            Result_T(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &format_count, nullptr));
            surface_formats.resize(format_count);
            Result_T(
                vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &format_count, surface_formats.data()));

            // 支持的呈现模式
            uint32_t presentMode_count = 0;
            Result_T(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentMode_count, nullptr));
            surface_presentModes.resize(presentMode_count);
            Result_T(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentMode_count,
                                                               surface_presentModes.data()));

            print_avaliable_surface_detail(surface_capabilities, surface_formats, surface_presentModes);
        }

        // 创建交换链
        std::set<uint32_t> unipue_queueFamily_set = {
            queueFamilies.graphics,
            queueFamilies.present,
            queueFamilies.compute,
        };
        std::vector<uint32_t> unipue_queueFamily(unipue_queueFamily_set.begin(), unipue_queueFamily_set.end());

        VkSwapchainCreateInfoKHR swapChain_createInfo{
            .sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .pNext                 = nullptr,
            .flags                 = 0,
            .surface               = surface,
            .minImageCount         = surface_capabilities.minImageCount + 1,
            .imageFormat           = VK_FORMAT_B8G8R8A8_UNORM,  // VK_FORMAT_R8G8B8A8_SRGB
            .imageColorSpace       = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            .imageExtent           = surface_capabilities.currentExtent,
            .imageArrayLayers      = 1,
            .imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = static_cast<uint32_t>(unipue_queueFamily.size()),
            .pQueueFamilyIndices   = unipue_queueFamily.data(),
            .preTransform          = surface_capabilities.currentTransform,
            .compositeAlpha        = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode           = VK_PRESENT_MODE_MAILBOX_KHR,
            .clipped               = VK_TRUE,
            .oldSwapchain          = nullptr,
        };
        Result_T(vkCreateSwapchainKHR(device, &swapChain_createInfo, nullptr, &swapChain));

        // 获取交换链图像，创建图像视图
        uint32_t imageCount = 0;
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
        swapChain_images.resize(imageCount);
        swapChain_imageViews.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChain_images.data());
        for (int i = 0; i < imageCount; i++)
        {
            VkImageViewCreateInfo imageView_createInfo{
                .sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .pNext    = nullptr,
                .flags    = 0,
                .image    = swapChain_images[i],
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format   = VK_FORMAT_B8G8R8A8_UNORM,
                .components =
                    VkComponentMapping{
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                        VK_COMPONENT_SWIZZLE_IDENTITY,
                    },
                .subresourceRange =
                    VkImageSubresourceRange{
                        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                        .baseMipLevel   = 0,
                        .levelCount     = 1,
                        .baseArrayLayer = 0,
                        .layerCount     = 1,
                    },
            };
            Result_T(vkCreateImageView(device, &imageView_createInfo, nullptr, &swapChain_imageViews[i]));
        }
        //~SECTION

        // SECTION - create pipeline
        {
            // ANCHOR - create commnad pool and buffer
            VkCommandPoolCreateInfo commandPool_createInfo{
                .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                .pNext            = nullptr,
                .flags            = 0,
                .queueFamilyIndex = queueFamilies.graphics,
            };
            Result_T(vkCreateCommandPool(device, &commandPool_createInfo, nullptr, &commandPool));
            VkCommandBufferAllocateInfo commandBuffer_allocateInfo{
                .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .pNext              = nullptr,
                .commandPool        = commandPool,
                .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1,
            };
            Result_T(vkAllocateCommandBuffers(device, &commandBuffer_allocateInfo, &commandBuffer));

            // ANCHOR - create render pass
            VkAttachmentDescription colorAttachment_description{
                .flags          = 0,
                .format         = VK_FORMAT_B8G8R8A8_UNORM,
                .samples        = VK_SAMPLE_COUNT_1_BIT,
                .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            };

            VkAttachmentReference colorAttachment_reference{
                .attachment = 0,
                .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            };
            VkSubpassDescription subpass_description{
                .flags                   = 0,
                .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
                .inputAttachmentCount    = 0,
                .pInputAttachments       = nullptr,
                .colorAttachmentCount    = 1,
                .pColorAttachments       = &colorAttachment_reference,
                .pResolveAttachments     = nullptr,
                .pDepthStencilAttachment = nullptr,
                .preserveAttachmentCount = 0,
                .pPreserveAttachments    = nullptr,
            };

            VkSubpassDependency subpass_dependency{
                .srcSubpass      = VK_SUBPASS_EXTERNAL,
                .dstSubpass      = 0,
                .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask   = 0,
                .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dependencyFlags = 0,
            };
            // 创建渲染流程
            VkRenderPassCreateInfo renderPass_createInfo{
                .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                .pNext           = nullptr,
                .flags           = 0,
                .attachmentCount = 1,
                .pAttachments    = &colorAttachment_description,
                .subpassCount    = 1,
                .pSubpasses      = &subpass_description,
                .dependencyCount = 1,
                .pDependencies   = &subpass_dependency,
            };
            Result_T(vkCreateRenderPass(device, &renderPass_createInfo, nullptr, &renderPass));

            // ANCHOR - create frame buffers

            // ANCHOR - create graphics pipeline
            // 创建着色器阶段
            std::function<VkShaderModule(std::shared_ptr<std::vector<char>>)> createShaderModule =
                [](std::shared_ptr<std::vector<char>> code) {
                    VkShaderModule           shaderModule = nullptr;
                    VkShaderModuleCreateInfo shaderModule_createInfo{
                        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                        .pNext    = nullptr,
                        .flags    = 0,
                        .codeSize = code->size(),
                        .pCode    = reinterpret_cast<const uint32_t*>(code->data()),
                    };
                    vkCreateShaderModule(device, &shaderModule_createInfo, nullptr, &shaderModule);
                    return shaderModule;
                };
            VkShaderModule vertexShader_module   = createShaderModule(readFile(""));
            VkShaderModule fragmentShader_module = createShaderModule(readFile(""));
            // LINK -
            std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages_createInfo{
                VkPipelineShaderStageCreateInfo{
                    .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .pNext  = nullptr,
                    .flags  = 0,
                    .stage  = VK_SHADER_STAGE_VERTEX_BIT,
                    .module = vertexShader_module,
                    .pName  = "main",
                },
                VkPipelineShaderStageCreateInfo{
                    .sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                    .pNext  = nullptr,
                    .flags  = 0,
                    .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
                    .module = fragmentShader_module,
                    .pName  = "main",
                },
            };

            // 顶点输入
            VkPipelineVertexInputStateCreateInfo vertexInputState_createInfo{
                .sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .pNext                           = nullptr,
                .flags                           = 0,
                .vertexBindingDescriptionCount   = 0,
                .pVertexBindingDescriptions      = nullptr,
                .vertexAttributeDescriptionCount = 0,
                .pVertexAttributeDescriptions    = nullptr,
            };

            // 输入装配
            VkPipelineInputAssemblyStateCreateInfo inputAssemblyState_createInfo{
                .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .pNext                  = nullptr,
                .flags                  = 0,
                .topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                .primitiveRestartEnable = VK_FALSE,
            };

            // 视口和裁剪
            VkViewport viewport{
                .x        = 0.0F,
                .y        = 0.0F,
                .width    = static_cast<float>(surface_capabilities.currentExtent.width),
                .height   = static_cast<float>(surface_capabilities.currentExtent.height),
                .minDepth = 0.0F,
                .maxDepth = 1.0F,
            };
            VkRect2D scissor{
                .offset = {0, 0},
                .extent = surface_capabilities.currentExtent,
            };
            VkPipelineViewportStateCreateInfo viewportState_createInfo{
                .sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                .pNext         = nullptr,
                .flags         = 0,
                .viewportCount = 1,
                .pViewports    = &viewport,
                .scissorCount  = 1,
                .pScissors     = &scissor,
            };

            // 光栅化器
            VkPipelineRasterizationStateCreateInfo rasterizationState_createInfo{
                .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .pNext                   = nullptr,
                .flags                   = 0,
                .depthClampEnable        = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode             = VK_POLYGON_MODE_FILL,
                .cullMode                = VK_CULL_MODE_BACK_BIT,
                .frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                .depthBiasEnable         = VK_FALSE,
                .depthBiasConstantFactor = 0.0F,
                .depthBiasClamp          = 0.0F,
                .depthBiasSlopeFactor    = 0.0F,
                .lineWidth               = 1.0F,
            };

            // 多重采样
            VkPipelineMultisampleStateCreateInfo multisampleState_createInfo{
                .sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
                .sampleShadingEnable   = VK_FALSE,  // 超采样模式
                .minSampleShading      = 0.0F,
                .pSampleMask           = nullptr,
                .alphaToCoverageEnable = VK_FALSE,
                .alphaToOneEnable      = VK_FALSE,
            };

            // 深度、模板缓冲
            VkPipelineDepthStencilStateCreateInfo depthStencilState_createInfo{
                .sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .pNext                 = nullptr,
                .flags                 = 0,
                .depthTestEnable       = VK_FALSE,
                .depthWriteEnable      = VK_FALSE,
                .depthCompareOp        = VK_COMPARE_OP_LESS,
                .depthBoundsTestEnable = VK_FALSE,
                .stencilTestEnable     = VK_FALSE,
                .front                 = {},
                .back                  = {},
                .minDepthBounds        = 0.0F,
                .maxDepthBounds        = 1.0F,
            };

            // 颜色混合
            VkPipelineColorBlendAttachmentState colorBlendAttachmentState{
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
            VkPipelineColorBlendStateCreateInfo colorBlendState_createInfo{
                .sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                .pNext           = nullptr,
                .flags           = 0,
                .logicOpEnable   = VK_FALSE,
                .logicOp         = VK_LOGIC_OP_COPY,
                .attachmentCount = 1,
                .pAttachments    = &colorBlendAttachmentState,
                .blendConstants  = {0.0F, 0.0F, 0.0F, 0.0F},
            };

            // 动态状态
            VkPipelineDynamicStateCreateInfo dynamicState_createInfo{
                .sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                .pNext             = nullptr,
                .flags             = 0,
                .dynamicStateCount = 0,
                .pDynamicStates    = nullptr,
            };

            // 管线布局
            VkPipelineLayoutCreateInfo pipelineLayout_createInfo{
                .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pNext                  = nullptr,
                .flags                  = 0,
                .setLayoutCount         = 0,
                .pSetLayouts            = nullptr,
                .pushConstantRangeCount = 0,
                .pPushConstantRanges    = nullptr,
            };
            Result_T(vkCreatePipelineLayout(device, &pipelineLayout_createInfo, nullptr, &pipelineLayout));

            VkGraphicsPipelineCreateInfo graphicsPipeline_createInfo{
                .sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                .pNext               = nullptr,
                .flags               = 0,
                .stageCount          = shaderStages_createInfo.size(),
                .pStages             = shaderStages_createInfo.data(),
                .pVertexInputState   = &vertexInputState_createInfo,
                .pInputAssemblyState = &inputAssemblyState_createInfo,
                .pTessellationState  = nullptr,
                .pViewportState      = &viewportState_createInfo,
                .pRasterizationState = &rasterizationState_createInfo,
                .pMultisampleState   = &multisampleState_createInfo,
                .pDepthStencilState  = &depthStencilState_createInfo,
                .pColorBlendState    = &colorBlendState_createInfo,
                .pDynamicState       = &dynamicState_createInfo,
                .layout              = pipelineLayout,
                .renderPass          = renderPass,
                .subpass             = 0,
                .basePipelineHandle  = nullptr,
                .basePipelineIndex   = 0,
            };
            Result_T(vkCreateGraphicsPipelines(device, nullptr, 1, &graphicsPipeline_createInfo, nullptr, &pipeline));
        }
        //~SECTION

        // ANCHOR - rendering loop
        {
            while (glfwWindowShouldClose(window) == 0)
            {
                // draw

                glfwPollEvents();
            }

            vkDeviceWaitIdle(device);
        }

        // SECTION - cleanup
        //  ANCHOR - cleanup vulkan
        {
            // pipeline
            vkDestroyPipeline(device, pipeline, nullptr);
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            vkDestroyRenderPass(device, renderPass, nullptr);
            vkDestroyCommandPool(device, commandPool,
                                 nullptr);  // commandPool被销毁时，所有的command buffer会被自动释放

            // basic componment
            for (auto& imageView : swapChain_imageViews) { vkDestroyImageView(device, imageView, nullptr); }
            vkDestroySwapchainKHR(device, swapChain, nullptr);
            vkDestroyDevice(device, nullptr);
#ifdef NDEBUG
#else
            auto vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
#endif
            vkDestroySurfaceKHR(instance, surface, nullptr);
            vkDestroyInstance(instance, nullptr);
        }

        // ANCHOR - cleanup glfw
        {
            glfwDestroyWindow(window);
            window = nullptr;
            glfwTerminate();
        }
        //~SECTION

        return EXIT_SUCCESS;
    }
}