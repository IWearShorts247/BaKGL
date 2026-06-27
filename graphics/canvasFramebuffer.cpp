#include "graphics/canvasFramebuffer.hpp"

#include "com/logger.hpp"

#include <stdexcept>

namespace Graphics {

CanvasFramebuffer::CanvasFramebuffer(unsigned canvasWidth, unsigned canvasHeight)
:
    mCanvasWidth{canvasWidth},
    mCanvasHeight{canvasHeight}
{
    const auto& logger = Logging::LogState::GetLogger("Display");

    glGenFramebuffers(1, &mFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, mFbo);

    // Color + depth as renderbuffers: the canvas is only ever blitted (never sampled),
    // so renderbuffers are sufficient and cheaper than a texture attachment.
    glGenRenderbuffers(1, &mColorBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, mColorBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, mCanvasWidth, mCanvasHeight);
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, mColorBuffer);

    glGenRenderbuffers(1, &mDepthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, mDepthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mCanvasWidth, mCanvasHeight);
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, mDepthBuffer);

    const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        logger.Error() << "Canvas framebuffer incomplete: 0x" << std::hex << status
            << std::dec << "\n";
        throw std::runtime_error("Canvas framebuffer incomplete");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    logger.Info() << "Created canvas framebuffer " << mCanvasWidth << "x" << mCanvasHeight << "\n";
}

CanvasFramebuffer::~CanvasFramebuffer()
{
    if (mDepthBuffer) glDeleteRenderbuffers(1, &mDepthBuffer);
    if (mColorBuffer) glDeleteRenderbuffers(1, &mColorBuffer);
    if (mFbo) glDeleteFramebuffers(1, &mFbo);
}

void CanvasFramebuffer::BindForDrawing()
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFbo);
    glViewport(0, 0, mCanvasWidth, mCanvasHeight);
}

DestRect CanvasFramebuffer::ComputeDestRect(int framebufferWidth, int framebufferHeight) const
{
    // Center the canvas; equal size (no scaling — UiScale is already baked in). If the
    // framebuffer is smaller than the canvas the offsets go negative and the blit clips
    // the canvas edges rather than crashing (see spec edge cases).
    const int offX = (framebufferWidth - static_cast<int>(mCanvasWidth)) / 2;
    const int offY = (framebufferHeight - static_cast<int>(mCanvasHeight)) / 2;
    return DestRect{offX, offY, static_cast<int>(mCanvasWidth), static_cast<int>(mCanvasHeight)};
}

void CanvasFramebuffer::PresentToScreen(int framebufferWidth, int framebufferHeight)
{
    const auto dest = ComputeDestRect(framebufferWidth, framebufferHeight);

    // Clear the whole default framebuffer to black so the letterbox bars are clean.
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, mFbo);
    glBlitFramebuffer(
        0, 0, static_cast<int>(mCanvasWidth), static_cast<int>(mCanvasHeight),
        dest.mX, dest.mY, dest.mX + dest.mWidth, dest.mY + dest.mHeight,
        GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

}
