#include "GlfwGeneral.hpp"

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

    while (glfwWindowShouldClose(pWindow) == 0)
    {
        /*渲染过程，待填充*/

        glfwPollEvents();
        TitleFps();
    }

    TerminateWindow();
    return EXIT_SUCCESS;
}