#pragma once

#include <GL/glew.h>

namespace Graphics {

// Centered destination rectangle (in framebuffer pixels) that the logical canvas
// is presented into; the remainder of the framebuffer is the letterbox/pillarbox.
struct DestRect
{
    int mX;
    int mY;
    int mWidth;
    int mHeight;
};

// An offscreen framebuffer holding the fixed logical canvas (320 * UiScale x
// 200 * UiScale). The whole frame (3D world + 2D GUI) renders into it, then it is
// blitted 1:1, centered, into the default framebuffer with black letterbox bars.
// Because UiScale is an integer the blit never scales, so the result is pixel-exact.
//
// Switching window mode / monitor never recreates this object (it is independent of
// window size); only a UiScale change requires a new canvas (Stage 4).
class CanvasFramebuffer
{
public:
    CanvasFramebuffer(unsigned canvasWidth, unsigned canvasHeight);
    ~CanvasFramebuffer();

    CanvasFramebuffer(const CanvasFramebuffer&) = delete;
    CanvasFramebuffer& operator=(const CanvasFramebuffer&) = delete;

    // Bind as the draw target and set the viewport to the full canvas.
    void BindForDrawing();

    // Blit the canvas, centered with black bars, into the default framebuffer of
    // the given size (use glfwGetFramebufferSize, not window size, for DPI safety).
    // Leaves the default framebuffer bound with a full-framebuffer viewport.
    void PresentToScreen(int framebufferWidth, int framebufferHeight);

    // Centered, canvas-sized destination rect for a given framebuffer size.
    DestRect ComputeDestRect(int framebufferWidth, int framebufferHeight) const;

    unsigned GetCanvasWidth() const { return mCanvasWidth; }
    unsigned GetCanvasHeight() const { return mCanvasHeight; }

private:
    unsigned mCanvasWidth;
    unsigned mCanvasHeight;
    GLuint mFbo{0};
    GLuint mColorBuffer{0};
    GLuint mDepthBuffer{0};
};

}
