#include "VKBase.h"

using namespace vulkan;

const VkExtent2D& windowSize = GraphicsBase::Base().SwapchainCreateInfo().imageExtent;

namespace easyVulkan {

struct renderPassWithFramebuffers
{
    renderPass               renderPass;
    std::vector<framebuffer> framebuffers;
};

const auto& CreateRpwf_Screen()
{
    static renderPassWithFramebuffers rpwf;

    // 创建渲染通道
    VkAttachmentDescription attachmentDescription = {
        .format        = GraphicsBase::Base().SwapchainCreateInfo().imageFormat,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp       = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout   = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference attachmentReference = {
        0,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkSubpassDescription subpassDescription = {
        .pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments    = &attachmentReference,
    };

    VkSubpassDependency subpassDependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        /*
        这里的srcSubpass为VK_SUBPASS_EXTERNAL，那么前一次使用同一图像附件的渲染也
        可以被纳入同步范围，确保前一次的颜色附件输出（color attachment output）阶段在
        dstStageMask指定的阶段前完成。由于每次向交换链图像渲染时，先前的内容都可以被舍弃，
        srcAccessMask可以为0（srcAccessMask的用途是用来确保写入操作的结果可以被后续操作
        正确访问）。
        */
        .dstSubpass   = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        /*
        图像内存布局的转换最迟可以在这里dstStageMask指定的阶段发生，而这件事当然得发生在
        获取到交换链图像之后，那么dstStageMask便不得早于提交命令缓冲区时等待（vkAcquire
        NextImageKHR(...)所置位的）信号量对应的waitDstStageMask。
        */
        .srcAccessMask   = 0,
        .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    };

    VkRenderPassCreateInfo renderPassCreateInfo = {
        .attachmentCount = 1,
        .pAttachments    = &attachmentDescription,
        .subpassCount    = 1,
        .pSubpasses      = &subpassDescription,
        .dependencyCount = 1,
        .pDependencies   = &subpassDependency,
    };
    rpwf.renderPass.Create(renderPassCreateInfo);

    // 创建帧缓冲
    rpwf.framebuffers.resize(GraphicsBase::Base().SwapchainImageCount());
    VkFramebufferCreateInfo framebufferCreateInfo = {
        .renderPass      = rpwf.renderPass,
        .attachmentCount = 1,
        .width           = windowSize.width,
        .height          = windowSize.height,
        .layers          = 1,
    };
    for (size_t i = 0; i < GraphicsBase::Base().SwapchainImageCount(); i++)
    {
        VkImageView attachment             = GraphicsBase::Base().SwapchainImageView(i);
        framebufferCreateInfo.pAttachments = &attachment;
        rpwf.framebuffers[i].Create(framebufferCreateInfo);
    }

    return rpwf;
}

}  // namespace easyVulkan