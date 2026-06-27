#pragma once

struct GLFWwindow;
struct GLFWmonitor;

namespace Graphics {

// Mirrors Config::WindowMode but keeps the graphics layer independent of app/ (which
// links against graphics). main3d maps between the two enums at the boundary.
enum class WindowMode
{
    Windowed,
    BorderlessFullscreen,
    ExclusiveFullscreen,
};

// Owns runtime window-mode switching. Critically, it NEVER destroys/recreates the
// window or GL context (that re-triggers the NVIDIA context-creation hang on
// double-clicked GUI exes); it only calls glfwSetWindowMonitor / glfwSetWindowAttrib.
//
// The logical canvas size is fixed for the lifetime of the manager: switching modes
// just letterboxes that canvas into whatever framebuffer the new mode produces. A
// live UiScale change (which resizes the canvas) is a separate, later concern.
class WindowManager
{
public:
    // canvasWidth/Height = the windowed client size (logical canvas in pixels).
    WindowManager(GLFWwindow* window, int canvasWidth, int canvasHeight);

    // Switch to the given mode on the given monitor index (clamped; falls back to
    // primary). Borderless = undecorated window covering the monitor (no mode switch,
    // friendly alt-tab). Exclusive = real fullscreen at the monitor's video mode.
    void Apply(WindowMode mode, int monitorIndex);

    // Alt+Enter: borderless-fullscreen <-> windowed on the configured monitor.
    void ToggleFullscreen();

    WindowMode GetMode() const { return mMode; }
    int GetMonitorIndex() const { return mMonitorIndex; }

    // Largest integer scale S with 320*S <= width and 200*S <= height (never below 1).
    static int ComputeAutoScale(int monitorWidth, int monitorHeight);

    // Monitor by index, clamped; falls back to the primary monitor.
    static GLFWmonitor* MonitorAt(int index);

private:
    GLFWwindow* mWindow;
    WindowMode mMode{WindowMode::Windowed};
    int mMonitorIndex{0};
    int mCanvasWidth;
    int mCanvasHeight;
    // Windowed geometry, saved so we can restore it when leaving fullscreen.
    int mWindowedX{0};
    int mWindowedY{0};
    int mWindowedW;
    int mWindowedH;
};

}
