#include "app/config.hpp"

#include "com/json.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <fstream>

namespace Config {

WindowMode ParseWindowMode(const std::string& s)
{
    if (s == "BorderlessFullscreen") return WindowMode::BorderlessFullscreen;
    if (s == "ExclusiveFullscreen") return WindowMode::ExclusiveFullscreen;
    return WindowMode::Windowed;
}

std::string ToString(WindowMode m)
{
    switch (m)
    {
        case WindowMode::BorderlessFullscreen: return "BorderlessFullscreen";
        case WindowMode::ExclusiveFullscreen: return "ExclusiveFullscreen";
        case WindowMode::Windowed: return "Windowed";
    }
    return "Windowed";
}

Paths LoadPaths(const nlohmann::json& config)
{
    Paths paths{};
    if (config.contains("Paths"))
    {
        const auto& c = config["Paths"];
        paths.mShaders = c.value("Shaders", "");
        paths.mSaves = c.value("Saves", "");
        paths.mGameData = c.value("GameData", "");
        paths.mGraphicsOverrides = c.value("GraphicsOverrides", "");
        paths.mDialogMods = c.value("DialogMods", "");
        paths.mLuaMods = c.value("LuaMods", "");
    }
    return paths;
}

Graphics LoadGraphics(const nlohmann::json& config)
{
    Graphics graphics{};
    if (config.contains("Graphics"))
    {
        const auto& c = config["Graphics"];
        graphics.mResolutionScale = c.value("ResolutionScale", 4.0);
        // UiScale supersedes the deprecated float ResolutionScale. If a new config omits
        // UiScale, derive it from ResolutionScale so old configs keep their scale.
        graphics.mUiScale = c.contains("UiScale")
            ? c.value("UiScale", 4)
            : static_cast<int>(std::lround(graphics.mResolutionScale));
        graphics.mUiScale = std::clamp(graphics.mUiScale, 1, 16);
        graphics.mWindowMode = ParseWindowMode(c.value("WindowMode", std::string{"Windowed"}));
        if (c.contains("Fullscreen"))
        {
            const auto& f = c["Fullscreen"];
            graphics.mMonitor = f.value("Monitor", 0);
            graphics.mAutoScale = f.value("AutoScale", true);
        }
        graphics.mVSync = c.value("VSync", true);
        graphics.mShadows = c.value("EnableShadows", true);
        graphics.mEnableImGui = c.value("EnableImGui", true);
        graphics.mDebugDisableFades = c.value("DebugDisableFades", false);
        graphics.mDebugRenderEncounters = c.value("DebugRenderEncounters", false);
        graphics.mDrawDistance = c.value("DrawDistance", 128000);
    }
    return graphics;
}

Logging LoadLogging(const nlohmann::json& config)
{
    Logging logging{};
    if (config.contains("Logging"))
    {
        const auto& c = config["Logging"];
        logging.mLogToFile = c.value("LogToFile", true);
        logging.mLogFilePath = c.value("LogFilePath", "");
        logging.mLogLevel = c.value("LogLevel", "Debug");
        if (c.contains("DisabledLoggers"))
        {
            for (const auto& logger : c["DisabledLoggers"])
            {
                logging.mDisabledLoggers.emplace_back(logger);
            }
        }
        if (c.contains("EnabledLoggers"))
        {
            for (const auto& logger : c["EnabledLoggers"])
            {
                logging.mEnabledLoggers.emplace_back(logger);
            }
        }
    }
    return logging;
}

Audio LoadAudio(const nlohmann::json& config)
{
    Audio audio{};
    if (config.contains("Audio"))
    {
        const auto& c = config["Audio"];
        audio.mEnableAudio = c.value("EnableAudio", true);
        audio.mEnableBackgroundSounds = c.value("EnableBackgroundSounds", true);
        audio.mMidiPlayer = c.value("MidiPlayer", "ADLMIDI");
    }
    return audio;
}

Game LoadGame(const nlohmann::json& config)
{
    Game game{};
    if (config.contains("Game"))
    {
        const auto& c = config["Game"];
        game.mAdvanceTime = c.value("AdvanceTime", true);
        game.mFixCombatEntityLists = c.value("FixCombatEntityLists", false);
    }
    return game;
}


Config LoadConfig(std::string path)
{
    if (!std::filesystem::exists(path))
    {
        throw std::runtime_error("No config file at path: " + path);
    }

    std::ifstream in{};
    in.open(path, std::ios::in);

    const auto callback = nullptr;
    const bool allow_exceptions = true;
    const bool ignore_comments = true;
    auto data = nlohmann::json::parse(in, callback, allow_exceptions, ignore_comments);

    Config config{};
    config.mPaths = LoadPaths(data);
    config.mGraphics = LoadGraphics(data);
    config.mLogging = LoadLogging(data);
    config.mAudio = LoadAudio(data);
    config.mGame = LoadGame(data);

    std::cout << "Loaded config file: " << data <<"\n";

    return config;
}

}
