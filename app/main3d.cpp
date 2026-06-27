#include "app/config.hpp"

#include "bak/backgroundSounds.hpp"
#include "bak/camera.hpp"
#include "bak/constants.hpp"
#include "bak/dialogJson.hpp"
#include "bak/lua/core.hpp"

#include "bak/state/encounter.hpp"
#include "bak/encounter//encounter.hpp"
#include "bak/zone.hpp"

extern "C" {
#include "com/getopt.h"
}

#include "com/logger.hpp"
#include "com/path.hpp"
#include "com/visit.hpp"

#include "game/console.hpp"
#include "game/gameRunner.hpp"
#include "game/screens.hpp"
#include "game/systems.hpp"

#include "graphics/inputHandler.hpp"
#include "graphics/guiRenderer.hpp"
#include "graphics/canvasFramebuffer.hpp"
#include "graphics/windowManager.hpp"
#include "graphics/glfw.hpp"
#include "graphics/renderer.hpp"
#include "graphics/sprites.hpp"

#include "gui/guiManager.hpp"
#include "gui/textInput.hpp"
#include "gui/window.hpp"

#include "imgui/imguiWrapper.hpp"

#include <GL/glew.h>

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#if defined(_WIN32)
#include <windows.h>
#endif
#include <memory>
#include <numbers>
#include <sstream>

#undef main
struct Options
{
    bool showImgui{true};
    std::string logLevel{""};
    std::string configFile{""};
};

Options Parse(int argc, char** argv)
{
    Options values{};

    struct option options[] = {
        {"help", no_argument,       0, 'h'},
        {"config", required_argument, 0, 'c'},
        {"log_level", required_argument, 0, 'l'},
        {"imgui", no_argument, 0, 'i'},
    };
    int optionIndex = 0;
    int opt;
    while ((opt = getopt_long(argc, argv, "hil:c:", options, &optionIndex)) != -1)
    {
        if (opt == 'h')
        {
            exit(0);
        }
        else if (opt == 'c')
        {
            if (optarg == nullptr)
            {
                std::cerr << "No argument provide to '-c/--config'" << std::endl;
                exit(1);
            }
            values.configFile = std::string{optarg};
        }
        else if (opt == 'i')
        {
            values.showImgui = false;
        }
        else if (opt == 'l')
        {
            if (optarg == nullptr)
            {
                std::cerr << "No argument provide to '-c/--config'" << std::endl;
                exit(1);
            }
            values.logLevel = std::string{optarg};
        }
    }

    return values;
}

Config::Config LoadConfigFile(std::string configPath)
{
    auto config = Config::Config{};
    auto TryLoad = [&config](std::string path)
    {
        try
        {
            std::cout << "Loading config file: " << path << std::endl;
            config = Config::LoadConfig(path);
            return "";
        }
        catch (const std::exception& error)
        {
            std::cerr << "Failed to load config file due to: " << error.what() << std::endl;
            exit(1);
        }
    };

    const auto defaultConfig = (Paths::Get().GetBakDirectoryPath() / "config.json").string();
    const auto currentDirectoryConfig = "config.json";
    // Also look next to the executable so a shipped package finds its config.json
    // regardless of the working directory (e.g. Windows double-click).
    const auto exeDirConfig = (GetExecutableDirectory() / "config.json").string();

    if (!configPath.empty())
    {
        TryLoad(configPath);
    }
    else if (std::filesystem::exists(currentDirectoryConfig))
    {
        TryLoad(currentDirectoryConfig);
    }
    else if (std::filesystem::exists(exeDirConfig))
    {
        TryLoad(exeDirConfig);
    }
    else if (std::filesystem::exists(defaultConfig))
    {
        TryLoad(defaultConfig);
    }
    else
    {
        std::cout << "Not loading a config file.\n";
    }

    return config;
}

int main(int argc, char** argv)
{
    // NOTE (Windows): the NVIDIA GL driver's context creation (inside glfwCreateWindow) HANGS if the
    // process has no console attached, AND a present-but-hidden console is NOT enough (tested) — the
    // console must be present & visible THROUGH context creation. So this is built console-subsystem
    // (console exists from startup), and we hide that console only AFTER the window is up — see the
    // HideOwnConsole() call after MakeGlfwWindow below.

    // Anchor the working directory to the executable's location before anything else.
    // Explorer double-click launches with CWD = system dir (not the game folder), whereas a
    // CLI launch runs from the folder; without this, CWD-relative resource lookups (config,
    // data, overrides) crash on double-click but work from the CLI. This makes them identical.
    {
        std::error_code ec{};
        const auto exeDir = GetExecutableDirectory();
        if (!exeDir.empty())
            std::filesystem::current_path(exeDir, ec);
    }

    const auto options = Parse(argc, argv);
    const auto config = LoadConfigFile(options.configFile);
    Logging::LogState::SetLogTime(config.mLogging.mLogTime);
    Logging::LogState::SetLogColor(config.mLogging.mLogColours);
    if (options.logLevel != "")
    {
        Logging::LogState::SetLevel(options.logLevel);
    }
    else
    {
        Logging::LogState::SetLevel(config.mLogging.mLogLevel);
    }

    std::unique_ptr<std::ofstream> logFileStream{};

    if (config.mLogging.mLogToFile)
    {
        auto logFilePath = config.mLogging.mLogFilePath.empty()
            ? Paths::Get().GetBakDirectoryPath() / "main3d.log"
            : std::filesystem::path{config.mLogging.mLogFilePath};
        std::cout << "Will log to file: " << logFilePath << "\n";
        auto logDirectory = logFilePath;
        logDirectory.remove_filename();
        std::error_code dirEc{};
        if (!std::filesystem::exists(logDirectory))
        {
            // spike: create the bak directory on first run rather than silently
            // disabling file logging (it's also where saves/log live)
            std::filesystem::create_directories(logDirectory, dirEc);
        }
        if (!std::filesystem::exists(logDirectory))
        {
            std::cerr << "Log file directory: " << logDirectory << " does not exist (" << dirEc.message() << "), will not log to file!\n";
        }
        else
        {
            logFileStream = std::make_unique<std::ofstream>(logFilePath.string(), std::ios::out);
            // Flush every write so the log is complete even if the process is killed or
            // hangs (needed to diagnose no-console / double-click launches).
            logFileStream->setf(std::ios::unitbuf);
            if (!logFileStream->is_open())
            {
                std::cerr << "Could not open log file: " << logFilePath << ", will not log to file!\n";
            }
            else 
            {
                Logging::LogState::AddStream(logFileStream.get());
            }
        }
    }

    const auto& logger = Logging::LogState::GetLogger("main");
    for (const auto& disabled : config.mLogging.mDisabledLoggers)
    {
        Logging::LogState::Disable(disabled);
    }
    for (const auto& enabled : config.mLogging.mEnabledLoggers)
    {
        Logging::LogState::Enable(enabled);
    }
    
    if (!config.mPaths.mGameData.empty())
    {
        Paths::Get().SetBakDirectory(config.mPaths.mGameData);
    }

    if (!config.mPaths.mGraphicsOverrides.empty())
    {
        // Resolve a relative override dir against the executable's location so a shipped
        // package (exe + overrides/ side by side) works regardless of working directory.
        std::filesystem::path overrides{config.mPaths.mGraphicsOverrides};
        if (overrides.is_relative())
            overrides = GetExecutableDirectory() / overrides;
        Paths::Get().SetModDirectory(overrides.string());
    }

    {
        const auto defaultDialog = (Paths::Get().GetBakDirectoryPath() / "dialogMods").string();
        auto dialogsDir = config.mPaths.mDialogMods.empty()
            ? defaultDialog
            : config.mPaths.mDialogMods;
        logger.Info() << "Try to load dialog overrides from directory: " << dialogsDir << "\n";
        BAK::DialogJson::LoadAllFromDirectory(dialogsDir);
    }

    {
        const auto defaultLuaMods = (Paths::Get().GetBakDirectoryPath() / "luaMods").string();
        auto luaModsPath = config.mPaths.mLuaMods.empty()
            ? defaultLuaMods
            : config.mPaths.mLuaMods;
        logger.Info() << "Try to load lua mods from directory: " << luaModsPath << "\n";
        BAK::Lua::Initialize(luaModsPath);
    }

    if (config.mAudio.mEnableAudio)
    {
        auto& provider = AudioA::AudioManagerProvider::Get();
        auto audioManager = std::make_unique<AudioA::AudioManager>();
        audioManager->Set(audioManager.get());
        audioManager->SwitchMidiPlayer(AudioA::StringToMidiPlayer(config.mAudio.mMidiPlayer));
        provider.SetAudioManager(std::move(audioManager));
    }
    else
    {
        auto& provider = AudioA::AudioManagerProvider::Get();
        auto audioManager = std::make_unique<AudioA::NullAudioManager>();
        provider.SetAudioManager(std::move(audioManager));
    }

    bool showImgui = config.mGraphics.mEnableImGui;
    // Integer logical-canvas scale (320x200 * UiScale). UiScale is parsed with back-compat
    // from the deprecated float ResolutionScale (see Config::LoadGraphics).
    const auto uiScale = config.mGraphics.mUiScale;
    auto guiScalar = static_cast<float>(uiScale);

    auto nativeWidth = 320.0f;
    auto nativeHeight = 200.0f;

    auto width = nativeWidth * guiScalar;
    auto height = nativeHeight * guiScalar;
    auto guiScaleInv = glm::vec2{1 / guiScalar, 1 / guiScalar};

    /* OPEN GL / GLFW SETUP  */

    auto window = Graphics::MakeGlfwWindow(
        height,
        width,
        "BaK");

#if defined(_WIN32)
    // The GL window/context now exists, so the console has served its purpose (see the NOTE at the
    // top of main). Hide it — but ONLY if it's our own console (double-click spawns a dedicated
    // one). If we were launched from a shell, the console is shared with a parent process; leave it.
    if (HWND console = GetConsoleWindow(); console != nullptr)
    {
        DWORD pids[2]{};
        if (GetConsoleProcessList(pids, 2) == 1)
            ShowWindow(console, SW_HIDE);
    }
#endif

    // VSync (cheap; toggleable from config/menu). 1 = on, 0 = off.
    glfwSwapInterval(config.mGraphics.mVSync ? 1 : 0);

    const auto ToGraphicsWindowMode = [](Config::WindowMode m)
    {
        switch (m)
        {
            case Config::WindowMode::BorderlessFullscreen: return Graphics::WindowMode::BorderlessFullscreen;
            case Config::WindowMode::ExclusiveFullscreen: return Graphics::WindowMode::ExclusiveFullscreen;
            case Config::WindowMode::Windowed: return Graphics::WindowMode::Windowed;
        }
        return Graphics::WindowMode::Windowed;
    };
    const auto windowMode = ToGraphicsWindowMode(config.mGraphics.mWindowMode);

    // Finalize the integer canvas scale. For a fullscreen mode we fit the canvas to the
    // target monitor: AutoScale picks the largest integer that fits; otherwise we clamp the
    // requested UiScale so the canvas never exceeds the monitor (it would just get clipped).
    if (windowMode != Graphics::WindowMode::Windowed)
    {
        auto* monitor = Graphics::WindowManager::MonitorAt(config.mGraphics.mMonitor);
        if (const auto* vidMode = glfwGetVideoMode(monitor))
        {
            const int fitScale = Graphics::WindowManager::ComputeAutoScale(vidMode->width, vidMode->height);
            const int chosen = config.mGraphics.mAutoScale ? fitScale : std::min(uiScale, fitScale);
            guiScalar = static_cast<float>(chosen);
            width = nativeWidth * guiScalar;
            height = nativeHeight * guiScalar;
            guiScaleInv = glm::vec2{1 / guiScalar, 1 / guiScalar};
        }
    }

    // Owns runtime window-mode switching (never recreates the GL context). Constructed
    // while still windowed so it captures the windowed geometry to restore later.
    auto windowManager = Graphics::WindowManager{
        window.get(),
        static_cast<int>(width),
        static_cast<int>(height)};
    if (windowMode != Graphics::WindowMode::Windowed)
        windowManager.Apply(windowMode, config.mGraphics.mMonitor);

    // Offscreen logical canvas (320x200 * UiScale). The whole frame renders here, then is
    // blitted centered into the window with black letterbox bars (integer scale => pixel-exact).
    auto canvas = Graphics::CanvasFramebuffer{
        static_cast<unsigned>(width),
        static_cast<unsigned>(height)};

    auto spriteManager = Graphics::SpriteManager{};
    auto guiRenderer = Graphics::GuiRenderer{
        width,
        height,
        guiScalar,
        spriteManager};

    auto root = Gui::Window{
        spriteManager,
        width / guiScalar,
        height / guiScalar};
        
    auto gameState = BAK::GameState{};
    gameState.SetFixCombatEntityLists(config.mGame.mFixCombatEntityLists);

    auto guiManager = Gui::GuiManager{
        root.GetCursor(),
        spriteManager,
        gameState
    };

    guiManager.SetDebugDisableFades(config.mGraphics.mDebugDisableFades);
    root.AddChildFront(&guiManager);
    guiManager.EnterMainMenu(false);

    Camera lightCamera{
        static_cast<unsigned>(width),
        static_cast<unsigned>(height),
        400 * 30.0f,
        2.0f};
    lightCamera.UseOrthoMatrix(400, 400);

    Camera camera{
        static_cast<unsigned>(width),
        static_cast<unsigned>(height),
        400 * 30.0f,
        1.0f};
    Camera* cameraPtr = &camera;

    guiManager.mMainView.SetHeading(camera.GetHeading());

    // OpenGL 3D Renderer
    constexpr auto sShadowDim = 4096;
    bool runningGame = false;
    auto renderer = Graphics::Renderer{
        width,
        height,
        sShadowDim,
        sShadowDim,
        config.mGraphics.mDrawDistance};

    Game::GameRunner gameRunner{
        camera,
        gameState,
        guiManager,
        config.mGraphics.mDebugRenderEncounters};

    // Wire up the zone loader to the GUI manager
    guiManager.SetZoneLoader(&gameRunner);

    bool imGuiInitialised = false;

    auto currentTile = camera.GetGameTile();
    logger.Info() << " Starting on tile: " << currentTile << "\n";

    Graphics::Light light{
        .mDirection =     glm::vec3{.0, -.25,  .00},
        .mAmbientColor =  glm::vec3{.5,  .5,   .5 },
        .mDiffuseColor =  glm::vec3{ 1,  .85,  .87},
        .mSpecularColor = glm::vec3{.2,  .2,   .2 },
        .mFogStrength = 0.0005f,
        .mFogColor =      glm::vec3{.15, .31,  .36}
    };

    const auto UpdateLightCamera = [&]{
        const auto lightPos = camera.GetNormalisedPosition() - 100.0f * glm::normalize(light.mDirection);
        const auto diff = lightCamera.GetNormalisedPosition() - camera.GetNormalisedPosition();
        const auto horizDistance = glm::sqrt((diff.x * diff.x) + (diff.z * diff.z));
        const auto yAngle = -glm::atan(diff.y / horizDistance);
        const auto xAngle = glm::atan(diff.x, diff.z) - ((180.0f / 360.0f) * (2 * 3.141592)) ;

        lightCamera.SetAngle(glm::vec2{xAngle, yAngle});
        lightCamera.SetPosition(lightPos * BAK::gWorldScale);
    };

    auto UpdateGameTile = [&]()
    {
        if (camera.GetGameTile() != currentTile)
        {
            currentTile = camera.GetGameTile();
            logger.Debug() << "New tile: " << currentTile << "\n";
            gameRunner.mGameState.Apply(BAK::State::ClearTileRecentEncounters);
        }
    };

    auto InputAllowed = [&]{
        return guiManager.InMainView()
            || (guiManager.InCombatView() && !gameRunner.IsAnimationActive());
    };

    Graphics::InputHandler inputHandler{};
    inputHandler.Bind(GLFW_KEY_G,     [&]{ if (guiManager.InMainView()) cameraPtr = &camera; });
    inputHandler.Bind(GLFW_KEY_H,     [&]{ if (guiManager.InMainView()) cameraPtr = &lightCamera; });
    inputHandler.Bind(GLFW_KEY_R,     [&]{
        if (guiManager.InMainView())
            UpdateLightCamera();
    });
    // Forward/back: when following a road, step along it instead of free movement.
    inputHandler.Bind(GLFW_KEY_UP,   [&]{ if (InputAllowed()){
        if (gameRunner.IsFollowingRoad()) gameRunner.FollowRoadStep(true);
        else cameraPtr->StrafeForward();
        UpdateGameTile();}});
    inputHandler.Bind(GLFW_KEY_DOWN, [&]{ if (InputAllowed()){
        if (gameRunner.IsFollowingRoad()) gameRunner.FollowRoadStep(false);
        else cameraPtr->StrafeBackward();
        UpdateGameTile();}});
    // Toggle road auto-following (also on the HUD "snap to road" button).
    inputHandler.Bind(GLFW_KEY_F, [&]{ if (InputAllowed()) gameRunner.ToggleFollowRoad(); });
    // Left/Right arrows TURN the party (like the original game), matching the Q/E rotate path.
    // (Sideways strafe remains available on A/D.)
    inputHandler.Bind(GLFW_KEY_LEFT, [&]{
        if (InputAllowed())
        {
            cameraPtr->RotateLeft();
            guiManager.mMainView.SetHeading(cameraPtr->GetHeading());
        }});
    inputHandler.Bind(GLFW_KEY_RIGHT,[&]{
        if (InputAllowed())
        {
            cameraPtr->RotateRight();
            guiManager.mMainView.SetHeading(cameraPtr->GetHeading());
        }});

    inputHandler.Bind(GLFW_KEY_W, [&]{ if (InputAllowed()){cameraPtr->MoveForward(); UpdateGameTile();}});
    inputHandler.Bind(GLFW_KEY_A, [&]{ if (InputAllowed()){cameraPtr->StrafeLeft(); UpdateGameTile();}});
    inputHandler.Bind(GLFW_KEY_D, [&]{ if (InputAllowed()){cameraPtr->StrafeRight(); UpdateGameTile();}});
    inputHandler.Bind(GLFW_KEY_S, [&]{ if (InputAllowed()){cameraPtr->MoveBackward(); UpdateGameTile();}});
    inputHandler.Bind(GLFW_KEY_Q, [&]{
        if (InputAllowed())
        {
            cameraPtr->RotateLeft();
            guiManager.mMainView.SetHeading(cameraPtr->GetHeading());
        }});
    inputHandler.Bind(GLFW_KEY_E, [&]{ 
        if (InputAllowed())
        {
            cameraPtr->RotateRight();
            guiManager.mMainView.SetHeading(cameraPtr->GetHeading());
        }});
    inputHandler.Bind(GLFW_KEY_Z, [&]{ if (InputAllowed()){cameraPtr->StrafeUp();     UpdateGameTile();}});
    inputHandler.Bind(GLFW_KEY_V, [&]{ if (InputAllowed()){cameraPtr->StrafeDown();   UpdateGameTile();}});
    inputHandler.Bind(GLFW_KEY_X, [&]{ if (InputAllowed()) cameraPtr->RotateVerticalUp(); });
    inputHandler.Bind(GLFW_KEY_Y, [&]{ if (InputAllowed()) cameraPtr->RotateVerticalDown(); });
    inputHandler.Bind(GLFW_KEY_C, [&]{ if (guiManager.InMainView()) gameRunner.mGameState.Apply(BAK::State::ClearTileRecentEncounters); });
    // Combat: 'T' toggles the player's melee attack type (swing <-> thrust).
    inputHandler.Bind(GLFW_KEY_T, [&]{
        if (guiManager.InCombatView())
        {
            auto& cm = guiManager.GetCombatManager();
            cm.SetMeleeAttackType(
                cm.GetMeleeAttackType() == BAK::Combat::MeleeAttackType::Swing
                    ? BAK::Combat::MeleeAttackType::Thrust
                    : BAK::Combat::MeleeAttackType::Swing);
        }
    });
    // Combat: Space ends the current combatant's turn (pass, e.g. after moving without
    // attacking). A move no longer auto-ends the turn (multi-step move-then-attack).
    inputHandler.Bind(GLFW_KEY_SPACE, [&]{
        if (guiManager.InCombatView() && !gameRunner.IsAnimationActive())
            guiManager.GetCombatManager().EndTurn();
    });
    inputHandler.Bind(GLFW_KEY_I, [&]{ 
        if (!imGuiInitialised)
        {
            ImguiWrapper::Initialise(window.get());
            imGuiInitialised = true;
        }
        showImgui = !showImgui;
    });

    // Toggle the classic/remastered art crossfade (MI:SE style). Edge-triggered
    // (BindPress) so one tap = one toggle — Bind() repeats every frame held and
    // would oscillate the target into a stuck half-blend. Gated so typing into a
    // text field (e.g. a save-game name) doesn't trip it.
    inputHandler.BindPress(GLFW_KEY_B, [&]{
        if (!Gui::TextInput::AnyFocused())
            guiRenderer.ToggleCrossfade();
    });
    // Alt+Enter: toggle borderless fullscreen <-> windowed (no GL context recreation).
    inputHandler.BindPress(GLFW_KEY_ENTER, [&]{
        if (glfwGetKey(window.get(), GLFW_KEY_LEFT_ALT) == GLFW_PRESS
            || glfwGetKey(window.get(), GLFW_KEY_RIGHT_ALT) == GLFW_PRESS)
            windowManager.ToggleFullscreen();
    });
    inputHandler.Bind(GLFW_KEY_BACKSPACE,   [&]{ if (root.OnKeyEvent(Gui::KeyPress{GLFW_KEY_BACKSPACE})){ ;} });
    inputHandler.BindCharacter([&](char character){ if(root.OnKeyEvent(Gui::Character{character})){ ;} });

    Graphics::InputHandler::BindKeyboardToWindow(window.get(), inputHandler);
    Graphics::InputHandler::BindMouseToWindow(window.get(), inputHandler);

    // Map a window-space cursor position onto the centered canvas. In windowed mode the
    // window IS the canvas so the offset is zero; fullscreen/letterbox (Stage 2) makes it
    // non-trivial, so route ALL cursor positions through here now.
    //  - ToCanvasPx: window px -> canvas px (for 3D picking, which reads canvas pixels).
    //  - ToLogical:  window px -> logical 320x200 (for GUI hit-testing).
    // NB: mouse-scroll deltas must NOT be offset; they keep the plain 1/scale factor.
    const auto ToCanvasPx = [&](glm::vec2 p) -> glm::vec2
    {
        int ww{}, wh{};
        glfwGetWindowSize(window.get(), &ww, &wh);
        const auto offset = glm::vec2{(ww - width) * 0.5f, (wh - height) * 0.5f};
        return p - offset;
    };
    const auto ToLogical = [&](glm::vec2 p) -> glm::vec2 { return ToCanvasPx(p) * guiScaleInv; };

    inputHandler.BindMouse(
        GLFW_MOUSE_BUTTON_LEFT,
        [&](auto clickPos)
        {
            bool guiHandled = root.OnMouseEvent(
                Gui::LeftMousePress{ToLogical(clickPos)});
            if (!guiHandled && InputAllowed())
            {
                glDisable(GL_BLEND);
                glDisable(GL_MULTISAMPLE);
                renderer.DrawForPicking(
                    gameRunner.GetZoneRenderData(),
                    gameRunner.mSystems->GetRenderables(),
                    gameRunner.mSystems->GetSprites(),
                    gameRunner.mSystems->GetDynamicRenderables(),
                    *cameraPtr);
                const auto clickedId = renderer.GetClickedEntity(ToCanvasPx(clickPos));
                if (gameRunner.IsGridVisible() && gameRunner.HandleGridCellClick(clickedId))
                {
                }
                else
                {
                    gameRunner.CheckClickable(clickedId);
                }
            }
        },
        [&](auto clickPos)
        {
            root.OnMouseEvent(
                Gui::LeftMouseRelease{ToLogical(clickPos)});
        }
    );

    inputHandler.BindMouse(
        GLFW_MOUSE_BUTTON_RIGHT,
        [&](auto click)
        {
            root.OnMouseEvent(
                Gui::RightMousePress{ToLogical(click)});
        },
        [&](auto click)
        {
            root.OnMouseEvent(
                Gui::RightMouseRelease{ToLogical(click)});
        }
    );

    inputHandler.BindMouseMotion(
        [&](auto pos)
        {
            root.OnMouseEvent(
                Gui::MouseMove{ToLogical(pos)});
        }
    );

    inputHandler.BindMouseScroll(
        [&](auto pos)
        {
            root.OnMouseEvent(
                Gui::MouseScroll{guiScaleInv * pos});
        }
    );

    double currentTime = 0;
    double lastTime = 0;
    float deltaTime = 0;

    glfwSetCursorPos(window.get(), width/2, height/2);
    //glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    //glfwSetInputMode(window.get(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_MULTISAMPLE);  

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    double pointerPosX, pointerPosY;

    bool consoleOpen = true;
    auto console = Console{};
    console.mCamera = &camera;
    console.mGameRunner = &gameRunner;
    console.mGuiManager = &guiManager;
    console.mGameState = &gameState;
    console.ToggleLog();

    // Do this last so we don't blast Imgui's callback hooks
    if (showImgui)
    {
        ImguiWrapper::Initialise(window.get());
        imGuiInitialised = true;
    }

    do
    {
        currentTime = glfwGetTime();

        deltaTime = float(currentTime - lastTime);
        guiManager.OnTimeDelta(currentTime - lastTime);
        gameRunner.OnTimeDelta(currentTime - lastTime);
        lastTime = currentTime;

        cameraPtr->SetDeltaTime(deltaTime);
        if (guiManager.InMainView())
        {
            gameState.SetLocation(cameraPtr->GetGameLocation());
        }

        glfwPollEvents();
        glfwGetCursorPos(window.get(), &pointerPosX, &pointerPosY);
        inputHandler.HandleInput(window.get());

        if (gameState.GetGameData().IsLoaded())
        {
            // { *** Draw 3D World ***
            UpdateLightCamera();

            glEnable(GL_BLEND);
            glEnable(GL_MULTISAMPLE);  

            double bakTimeOfDay = (gameState.GetWorldTime().GetTime().mTime % 43200);
            auto twoPi = std::numbers::pi_v<double> * 2.0;
            // light starts at 6 after midnight
            auto sixHours = 7200.0;
            auto beginDay = bakTimeOfDay - sixHours;
            bool isNight = bakTimeOfDay < 7200 || bakTimeOfDay > 36000;
            light.mDirection = glm::vec3{
                std::cos(beginDay * (twoPi / (28800 * 2))),
                isNight ? .1 : -.25,
                0};
            float ambient = isNight
                ? .05
                : std::sin(beginDay * (twoPi / 57600));
            light.mAmbientColor = glm::vec3{ambient};
            light.mDiffuseColor = ambient * glm::vec3{
                1.,
                std::sin(beginDay * (twoPi / (57600 * 2))),
                std::sin(beginDay * (twoPi / (57600 * 2)))
            };

            light.mSpecularColor = isNight ? glm::vec3{0} : ambient * glm::vec3{
                1.,
                std::sin(beginDay * (twoPi / (57600 * 2))),
                std::sin(beginDay * (twoPi / (57600 * 2)))
            };
            light.mFogColor = ambient * glm::vec3{.15, .31, .36};

            renderer.BeginDepthMapDraw();
            renderer.DrawDepthMap(
                gameRunner.GetZoneRenderData(),
                gameRunner.mSystems->GetRenderables(),
                lightCamera);
            renderer.DrawDepthMap(
                gameRunner.GetZoneRenderData(),
                gameRunner.mSystems->GetSprites(),
                lightCamera);
            renderer.EndDepthMapDraw();

            // The depth pass unbound to the default framebuffer; bind the offscreen canvas
            // for the main color pass (3D world + 2D GUI both render into it).
            canvas.BindForDrawing();
            // Dark blue background
            glClearColor(ambient * 0.15f, ambient * 0.31f, ambient * 0.36f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderer.DrawWithShadow(
                gameRunner.GetZoneRenderData(),
                gameRunner.mSystems->GetRenderables(),
                light,
                lightCamera,
                *cameraPtr,
                false);

            renderer.DrawWithShadow(
                gameRunner.GetZoneRenderData(),
                gameRunner.mSystems->GetSprites(),
                light,
                lightCamera,
                *cameraPtr,
                true);

            const auto& dynamicRenderables = gameRunner.mSystems->GetDynamicRenderables();
            for (const auto& obj : dynamicRenderables)
            {
                std::vector<DynamicRenderable> data{};
                data.emplace_back(obj);
                renderer.DrawWithShadow(
                    *obj.GetRenderData(),
                    data,
                    light,
                    lightCamera,
                    *cameraPtr,
                    true);
            }
        }
        else
        {
            // No 3D world (e.g. main menu): still render the GUI into the canvas.
            canvas.BindForDrawing();
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }

        //// { *** Draw 2D GUI ***
        guiRenderer.UpdateCrossfade(static_cast<float>(deltaTime));
        guiRenderer.RenderGui(&root);

        // Present the canvas: blit centered into the window with black letterbox bars.
        // Drive the dest rect from the framebuffer size (DPI-safe), not the window size.
        int framebufferWidth{}, framebufferHeight{};
        glfwGetFramebufferSize(window.get(), &framebufferWidth, &framebufferHeight);
        canvas.PresentToScreen(framebufferWidth, framebufferHeight);

        // { *** IMGUI START ***
        if (showImgui)
        {
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ShowLightGui(light);

            ShowCameraGui(camera);
            console.Draw("Console", &consoleOpen);
        }

        if (guiManager.InMainView() && !guiManager.GetCombatSequenceActive())
        {
            gameRunner.RunGameUpdate(config.mGame.mAdvanceTime);
            if (config.mAudio.mEnableBackgroundSounds)
            {
                BAK::PlayBackgroundSounds(gameRunner.mGameState);
            }
        }

        if (showImgui && gameRunner.mActiveEncounter)
        {
            ImGui::Begin("Encounter");
            std::stringstream ss{};
            ss << "Encounter: " << *gameRunner.mActiveEncounter << std::endl;
            ImGui::TextWrapped(ss.str().c_str());
            ImGui::End();
            
            const auto& encounter = gameRunner.mActiveEncounter->GetEncounter();
            std::visit(
                overloaded{
                    [&](const BAK::Encounter::GDSEntry& gds){
                        ShowDialogGui(
                            gds.mEntryDialog,
                            BAK::DialogStore::Get());
                    },
                    [&](const BAK::Encounter::Block& e){
                        ShowDialogGui(
                            e.mDialog,
                            BAK::DialogStore::Get());
                    },
                    [&](const BAK::Encounter::Combat& e){
                        ShowDialogGui(
                            e.mEntryDialog,
                            BAK::DialogStore::Get());
                    },
                    [&](const BAK::Encounter::Dialog& e){
                        ShowDialogGui(
                            e.mDialog,
                            BAK::DialogStore::Get());
                    },
                    [](const BAK::Encounter::EventFlag&){
                    },
                    [&](const BAK::Encounter::Zone& e){
                        ShowDialogGui(
                            e.mDialog,
                            BAK::DialogStore::Get());
                    },
                },
                encounter);
        }

        if (showImgui)
        {
            ImguiWrapper::Draw(window.get());
        }

        if (showImgui)
        {
            auto& io = ImGui::GetIO();
            if (io.WantCaptureKeyboard || io.WantCaptureMouse)
            {
                inputHandler.SetHandleInput(false);
            }
            else
            {
                inputHandler.SetHandleInput(true);
            }
        }
        else
        {
            inputHandler.SetHandleInput(true);
        }

        // *** IMGUI END *** }
     
        glfwSwapBuffers(window.get());
    }
    while (glfwGetKey(window.get(), GLFW_KEY_ESCAPE) != GLFW_PRESS 
        && glfwWindowShouldClose(window.get()) == 0);

    if (showImgui)
    {
        ImguiWrapper::Shutdown();
    }

    if (logFileStream && logFileStream->is_open())
    {
        logFileStream->close();
    }

    return 0;
}
