#pragma once

#include "graphics/windowManager.hpp"

namespace Gui {

// The user-facing display settings the Preferences menu edits.
struct DisplaySettings
{
    int mUiScale;
    Graphics::WindowMode mWindowMode;
    int mMonitor;
    bool mAutoScale;
    bool mVSync;

    bool operator==(const DisplaySettings&) const = default;
};

// Implemented by the app (main3d). Lets the Preferences menu apply display changes live
// without the gui layer knowing about GLFW / config files. Preview applies without saving
// (so a 15s timeout or Cancel can restore the previous settings); Commit applies + persists.
class IDisplayController
{
public:
    virtual ~IDisplayController() = default;

    virtual DisplaySettings GetCurrentSettings() const = 0;
    virtual int GetMonitorCount() const = 0;
    // Largest sensible UiScale for the target monitor (used to clamp the picker).
    virtual int GetMaxUiScale(int monitorIndex) const = 0;

    virtual void PreviewSettings(const DisplaySettings& settings) = 0;
    virtual void CommitSettings(const DisplaySettings& settings) = 0;
};

// Provider singleton (matches the codebase's Paths::Get() / AudioManagerProvider idiom),
// so the deeply-nested PreferencesScreen can reach the app-level controller without
// threading it through GuiManager + MainMenuScreen constructors. Null until the app sets it.
class DisplayControllerProvider
{
public:
    static void Set(IDisplayController* controller) { sController = controller; }
    static IDisplayController* Get() { return sController; }

private:
    static IDisplayController* sController;
};

}
