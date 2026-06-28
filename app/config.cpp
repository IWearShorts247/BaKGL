#include "app/config.hpp"

#include "com/json.hpp"
#include "com/logger.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <iterator>
#include <sstream>

namespace Config {

namespace {

const char* BoolStr(bool b) { return b ? "true" : "false"; }

// Regenerate the whole "Graphics": { ... } block as JSONC text. The first line has no
// leading indent (the caller keeps the original indentation that precedes "Graphics");
// inner lines use the file's 4-space style. All managed keys are emitted so a partial or
// legacy config gains the missing ones. Comments here are ours to define.
std::string GenerateGraphicsBlock(const Graphics& g)
{
    std::ostringstream os{};
    os << "\"Graphics\": {\n"
       << "        // Integer logical-canvas scale (320x200 * UiScale). 3..6 typical; clamped to monitor.\n"
       << "        \"UiScale\": " << g.mUiScale << ",\n"
       << "        // \"Windowed\" | \"BorderlessFullscreen\" | \"ExclusiveFullscreen\"\n"
       << "        \"WindowMode\": \"" << ToString(g.mWindowMode) << "\",\n"
       << "        \"Fullscreen\": {\n"
       << "            \"Monitor\": " << g.mMonitor << ",\n"
       << "            \"AutoScale\": " << BoolStr(g.mAutoScale) << "\n"
       << "        },\n"
       << "        \"VSync\": " << BoolStr(g.mVSync) << ",\n"
       << "        // DEPRECATED back-compat: kept in sync with UiScale for old readers.\n"
       << "        \"ResolutionScale\": " << g.mUiScale << ".0,\n"
       << "        \"EnableShadows\": " << BoolStr(g.mShadows) << ",\n"
       << "        \"EnableImGui\": " << BoolStr(g.mEnableImGui) << ",\n"
       << "        \"DrawDistance\": " << g.mDrawDistance << ",\n"
       << "        \"DebugDisableFades\": " << BoolStr(g.mDebugDisableFades) << ",\n"
       << "        \"DebugRenderEncounters\": " << BoolStr(g.mDebugRenderEncounters) << "\n"
       << "    }";
    return os.str();
}

}

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

bool WriteGraphicsConfig(const std::string& path, const Graphics& graphics)
{
    const auto& logger = ::Logging::LogState::GetLogger("Display");

    std::ifstream in{path, std::ios::in | std::ios::binary};
    if (!in)
    {
        logger.Error() << "Cannot open config for persistence: " << path << "\n";
        return false;
    }
    const std::string text{
        std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    in.close();

    // Locate the "Graphics" block and its matching closing brace. NB: this brace scan does
    // not skip braces inside strings/comments; the managed block contains none, so it is
    // safe for our config. Everything outside the block is preserved verbatim.
    const auto keyPos = text.find("\"Graphics\"");
    const auto bracePos = keyPos == std::string::npos
        ? std::string::npos : text.find('{', keyPos);
    if (bracePos == std::string::npos)
    {
        logger.Error() << "No \"Graphics\" block found in " << path << "; not persisting\n";
        return false;
    }
    int depth = 0;
    std::size_t endPos = std::string::npos;
    for (std::size_t i = bracePos; i < text.size(); ++i)
    {
        if (text[i] == '{') ++depth;
        else if (text[i] == '}' && --depth == 0) { endPos = i; break; }
    }
    if (endPos == std::string::npos)
    {
        logger.Error() << "Unterminated \"Graphics\" block in " << path << "; not persisting\n";
        return false;
    }

    const std::string updated =
        text.substr(0, keyPos) + GenerateGraphicsBlock(graphics) + text.substr(endPos + 1);

    // Validate the regenerated text BEFORE touching the file: never replace a good config
    // with something unparseable (guards against any garbage in the generated string).
    try
    {
        const auto check = nlohmann::json::parse(updated, nullptr, true, true);
        if (!check.contains("Graphics"))
            throw std::runtime_error("regenerated config has no Graphics block");
    }
    catch (const std::exception& e)
    {
        logger.Error() << "Refusing to persist config (regenerated text invalid): "
            << e.what() << "\n";
        return false;
    }

    // Write atomically: temp file, then rename over the target. A crash or partial write
    // can only leave a stray .tmp, never a corrupted config.json.
    const auto tmpPath = path + ".tmp";
    {
        std::ofstream out{tmpPath, std::ios::out | std::ios::trunc | std::ios::binary};
        if (!out)
        {
            logger.Error() << "Cannot open temp config for writing: " << tmpPath << "\n";
            return false;
        }
        out << updated;
        out.flush();
        if (!out)
        {
            logger.Error() << "Failed writing temp config: " << tmpPath << "\n";
            return false;
        }
    }

    std::error_code ec{};
    std::filesystem::rename(tmpPath, path, ec);
    if (ec)
    {
        logger.Error() << "Failed to replace config " << path << ": " << ec.message() << "\n";
        std::filesystem::remove(tmpPath, ec);
        return false;
    }
    logger.Info() << "Persisted Graphics settings to " << path << "\n";
    return true;
}

}
