#include "graphics/windowManager.hpp"

#include "com/logger.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>

namespace Graphics {

WindowManager::WindowManager(GLFWwindow* window, int canvasWidth, int canvasHeight)
:
    mWindow{window},
    mCanvasWidth{canvasWidth},
    mCanvasHeight{canvasHeight},
    mWindowedW{canvasWidth},
    mWindowedH{canvasHeight}
{
    glfwGetWindowPos(mWindow, &mWindowedX, &mWindowedY);
}

int WindowManager::ComputeAutoScale(int monitorWidth, int monitorHeight)
{
    const int s = std::min(monitorWidth / 320, monitorHeight / 200);
    return std::max(s, 1);
}

GLFWmonitor* WindowManager::MonitorAt(int index)
{
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    if (monitors == nullptr || count == 0)
        return glfwGetPrimaryMonitor();
    if (index < 0 || index >= count)
        return glfwGetPrimaryMonitor();
    return monitors[index];
}

void WindowManager::Apply(WindowMode mode, int monitorIndex)
{
    const auto& logger = Logging::LogState::GetLogger("Display");

    // Remember where the window was before we leave windowed mode, so we can put it back.
    if (mMode == WindowMode::Windowed && mode != WindowMode::Windowed)
    {
        glfwGetWindowPos(mWindow, &mWindowedX, &mWindowedY);
        glfwGetWindowSize(mWindow, &mWindowedW, &mWindowedH);
    }

    GLFWmonitor* monitor = MonitorAt(monitorIndex);
    const GLFWvidmode* vidMode = glfwGetVideoMode(monitor);
    int monitorX = 0;
    int monitorY = 0;
    glfwGetMonitorPos(monitor, &monitorX, &monitorY);

    switch (mode)
    {
    case WindowMode::Windowed:
        glfwSetWindowAttrib(mWindow, GLFW_DECORATED, GLFW_TRUE);
        glfwSetWindowMonitor(
            mWindow, nullptr,
            mWindowedX, mWindowedY, mWindowedW, mWindowedH,
            GLFW_DONT_CARE);
        break;

    case WindowMode::BorderlessFullscreen:
        // No video-mode switch: an undecorated window covering the whole monitor.
        // monitor stays NULL (still "windowed" to GLFW) so alt-tab stays friendly.
        glfwSetWindowAttrib(mWindow, GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowMonitor(
            mWindow, nullptr,
            monitorX, monitorY, vidMode->width, vidMode->height,
            GLFW_DONT_CARE);
        break;

    case WindowMode::ExclusiveFullscreen:
        glfwSetWindowMonitor(
            mWindow, monitor,
            0, 0, vidMode->width, vidMode->height,
            vidMode->refreshRate);
        break;
    }

    mMode = mode;
    mMonitorIndex = monitorIndex;
    logger.Info() << "Window mode -> " << static_cast<int>(mode)
        << " on monitor " << monitorIndex
        << " (" << vidMode->width << "x" << vidMode->height << ")\n";
}

void WindowManager::ToggleFullscreen()
{
    if (mMode == WindowMode::Windowed)
        Apply(WindowMode::BorderlessFullscreen, mMonitorIndex);
    else
        Apply(WindowMode::Windowed, mMonitorIndex);
}

}
