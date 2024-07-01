#include "GlfwGeneral.hpp"
#include "VKBase.h"

using namespace vulkan;

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

    fence     fence(VK_FENCE_CREATE_SIGNALED_BIT);  // 以置位状态创建栅栏
    semaphore semaphore_imageIsAvailable;
    semaphore semaphore_renderingIsOver;

    // 创建指令池和指令缓冲
    commandBuffer commandBuffer;
    commandPool   commandPool(GraphicsBase::Base().QueueFamilyIndex_Graphics(),
                              VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    commandPool.AllocateBuffers(commandBuffer);

    while (glfwWindowShouldClose(pWindow) == 0)
    {
        while (glfwGetWindowAttrib(pWindow, GLFW_ICONIFIED) != 0) { glfwWaitEvents(); }
        /*
        出于节省CPU和GPU占用的考量，有必要在窗口最小化时阻塞渲染循环。
        */

        fence.WaitAndReset();                                        // 等待并重置fence
        GraphicsBase::Base().SwapImage(semaphore_imageIsAvailable);  // 获取交换链图像索引

        commandBuffer.Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        /*渲染命令，待填充*/
        commandBuffer.End();

        GraphicsBase::Base().SubmitCommandBuffer_Graphics(commandBuffer, semaphore_imageIsAvailable,
                                                          semaphore_renderingIsOver, fence);
        GraphicsBase::Base().PresentImage(semaphore_renderingIsOver);

        glfwPollEvents();
        TitleFps();
    }

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
