#include "EasyVulkan.hpp"
#include "GlfwGeneral.hpp"
#include "VKBase+.h"
#include "VKBase.h"
#include <vulkan/vulkan_core.h>

using namespace vulkan;

static const std::string shader_root = "";

pipelineLayout pipelineLayout_triangle;  // 管线布局
pipeline       pipeline_triangle;        // 管线

/**
 * @brief 调用easyVulkan::CreateRpwf_Screen()并存储返回的引用到静态变量，
 * 避免重复调用easyVulkan::CreateRpwf_Screen()
 */
const auto& RenderPassAndFramebuffers()
{
    static const auto& rpwf = easyVulkan::CreateRpwf_Screen();  // 局部静态变量在调用时才会初始化
    return rpwf;
}

/**
 * @brief 创建管线布局
 */
void CreateLayout()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayout_triangle.Create(pipelineLayoutCreateInfo);
}

/**
 * @brief 创建管线
 */
void CreatePipeline()
{
    static shaderModule                                   vert("./shader/FirstTriangle.vert.spv");
    static shaderModule                                   frag("./shader/FirstTriangle.frag.spv");
    static std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos_triangle = {
        vert.StageCreateInfo(VK_SHADER_STAGE_VERTEX_BIT),
        frag.StageCreateInfo(VK_SHADER_STAGE_FRAGMENT_BIT),
    };

    std::function<void()> Create = []() {
        graphicsPipelineCreateInfoPack pipelineCiPack;
        pipelineCiPack.createInfo.layout     = pipelineLayout_triangle;
        pipelineCiPack.createInfo.renderPass = RenderPassAndFramebuffers().renderPass;
        // 子通道只有一个，所以pipelineCiPack.createInfo.renderPass使用默认值0
        pipelineCiPack.inputAssemblyStateCi.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // 图元拓扑
        pipelineCiPack.viewports.push_back(VkViewport{
            .x        = 0.F,
            .y        = 0.F,
            .width    = static_cast<float>(windowSize.width),
            .height   = static_cast<float>(windowSize.height),
            .minDepth = 0.F,
            .maxDepth = 1.F,
        });
        pipelineCiPack.scissors.push_back(VkRect2D{
            .offset = VkOffset2D{0, 0},
            .extent = windowSize,
        });
        // 指定视口和剪裁范围
        pipelineCiPack.multisampleStateCi.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        // 不开启多重采样
        pipelineCiPack.colorBlendAttachmentStates.push_back({.colorWriteMask = 0b1111});
        // 不开启混色，只指定RGBA四通道的写入遮罩为全部写入
        pipelineCiPack.UpdateAllArrays();
        pipelineCiPack.createInfo.stageCount = 2;
        pipelineCiPack.createInfo.pStages    = shaderStageCreateInfos_triangle.data();
        pipeline_triangle.Create(pipelineCiPack);
    };
    std::function<void()> Destroy = []() { pipeline_triangle.~pipeline(); };
    GraphicsBase::Base().AddCallback_CreateSwapchain(Create);
    GraphicsBase::Base().AddCallback_DestroySwapchain(Destroy);

    Create();  // 调用Create()以创建管线
}

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

    if (!InitializeWindow(VkExtent2D{1280, 720})) { return EXIT_FAILURE; }

    const auto& [renderPass, framebuffers] = RenderPassAndFramebuffers();
    CreateLayout();
    CreatePipeline();

    fence     fence(VK_FENCE_CREATE_SIGNALED_BIT);  // 以置位状态创建栅栏
    semaphore semaphore_imageIsAvailable;
    semaphore semaphore_renderingIsOver;

    // 创建指令池和指令缓冲
    commandBuffer commandBuffer;
    commandPool   commandPool(GraphicsBase::Base().QueueFamilyIndex_Graphics(),
                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    commandPool.AllocateBuffers(commandBuffer);

    VkClearValue clearColor = {
        .color = {1.F, 0.F, 0.F, 1.F},
    };  // 红色

    while (glfwWindowShouldClose(pWindow) == 0)
    {
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED) != 0)
        {
            glfwWaitEvents();  // 出于节省CPU和GPU占用的考量，有必要在窗口最小化时阻塞渲染循环。
        }

        fence.WaitAndReset();                                        // 等待并重置fence
        GraphicsBase::Base().SwapImage(semaphore_imageIsAvailable);  // 获取交换链图像索引
        auto i = GraphicsBase::Base().CurrentImageIndex();

        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        renderPass.CmdBegin(commandBuffer, framebuffers[i],
                            VkRect2D{
                                .offset = VkOffset2D{},
                                .extent = windowSize,
                            },
                            clearColor);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_triangle);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        renderPass.CmdEnd(commandBuffer);
        commandBuffer.End();

        GraphicsBase::Base().SubmitCommandBuffer_Graphics(commandBuffer, semaphore_imageIsAvailable,
                                                          semaphore_renderingIsOver, fence);
        GraphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        glfwPollEvents();
        TitleFps();
    }
    vkDeviceWaitIdle(GraphicsBase::Base().Device());
    vkDestroyPipelineLayout(GraphicsBase::Base().Device(), pipelineLayout_triangle, nullptr);

    TerminateWindow();
    return EXIT_SUCCESS;
}

/**REVIEW - 即时帧
考虑到交换链中存在多张图像，既然当前帧和上一帧所写入的图像不同，
在渲染当前帧时何必等待渲染完上一帧呢？

通过给交换链中的每张图像创建一套专用的同步对象、命令缓冲区、帧缓冲，
以及其他一切在循环的单帧中会被更新、写入的Vulkan对象，以此在渲染每
一帧图像的过程中避免资源竞争，减少阻塞，这种做法叫做即时帧（frames in flight）。

即时帧的好处显而易见，对每张交换链图像的写入，只需发生在呈现引擎上一次读取
同一图像之后即可，假设交互链图像有3张，那么便是与2帧之前的操作同步，相比
与上一帧同步，大幅提升了并行度。

但是即时帧的设备内存开销也是成倍增加的，由于所有会被更新、写入的Vulkan对象
都得创建与交换链图像相同的份数，在一些情况下会产生惊人的内存开销（比如延迟渲染）。
而且即便应用了即时帧也未必能大幅提升帧数，如果每帧的渲染时间都很短的话，
即时帧不会对帧数产生影响，换言之只应该在每帧渲染时间较长的情况下应用即时帧（
起码应该要长于屏幕的垂直刷新间隔）。
 */
