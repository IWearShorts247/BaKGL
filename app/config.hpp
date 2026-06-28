#pragma once

#include <string>
#include <vector>

namespace Config {

struct Paths
{
    std::string mShaders{};
    std::string mSaves{};
    std::string mGameData{};
    std::string mGraphicsOverrides{};
    std::string mDialogMods{};
    std::string mLuaMods{};
};

enum class WindowMode
{
    Windowed,
    BorderlessFullscreen,
    ExclusiveFullscreen,
};

// Parse/serialize the config string form. Unknown strings fall back to Windowed.
WindowMode ParseWindowMode(const std::string& s);
std::string ToString(WindowMode m);

struct Graphics
{
    // Integer logical-canvas scale (320x200 * UiScale). Supersedes the deprecated
    // float ResolutionScale; an old config with only ResolutionScale maps to
    // UiScale = round(ResolutionScale). Menu offers 3..6; clamped to fit the monitor.
    int mUiScale{4};
    WindowMode mWindowMode{WindowMode::Windowed};
    int mMonitor{0};        // glfw monitor index; 0 = primary
    bool mAutoScale{true};  // fullscreen: largest integer scale that fits; else use mUiScale
    bool mVSync{true};

    float mResolutionScale{4.0}; // DEPRECATED: read for back-compat only
    bool mShadows{true};
    bool mEnableImGui{true};
    bool mDebugDisableFades{false};
    bool mDebugRenderEncounters{false};
    int mDrawDistance{128000};
};

struct Logging
{
    bool mLogToFile{true};
    bool mLogTime{true};
    bool mLogColours{false};
    std::string mLogFilePath{};
    std::string mLogLevel{"DEBUG"};
    std::vector<std::string> mDisabledLoggers{};
    std::vector<std::string> mEnabledLoggers{};
};

struct Audio
{
    bool mEnableAudio{true};
    bool mEnableBackgroundSounds{true};
    std::string mMidiPlayer{"ADLMIDI"};
};

struct Game
{
    bool mAdvanceTime{true};
    bool mFixCombatEntityLists{false};
};

struct Config
{
    Paths mPaths{};
    Graphics mGraphics{};
    Logging mLogging{};
    Audio mAudio{};
    Game mGame{};
};

Config LoadConfig(std::string path);

// Persist display settings by rewriting ONLY the "Graphics" block of the JSONC file at
// path; every other block and all comments outside that block are preserved verbatim.
// Returns false (without modifying the file) if it cannot be parsed/located.
bool WriteGraphicsConfig(const std::string& path, const Graphics& graphics);

}
