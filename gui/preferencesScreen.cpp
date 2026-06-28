#include "gui/preferencesScreen.hpp"

#include "gui/IGuiManager.hpp"
#include "gui/backgrounds.hpp"
#include "gui/colors.hpp"
#include "gui/clickButton.hpp"
#include "gui/tickAnimator.hpp"
#include "gui/core/widget.hpp"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <string>

namespace Gui {

namespace {

// Layout (logical 320x200). A dark panel hosts the display rows over the OPTIONS2 art;
// exact placement is provisional pending the on-hardware visual check.
constexpr auto sPanelPos = glm::vec2{52, 26};
constexpr auto sPanelDim = glm::vec2{216, 118};
constexpr auto sCaptionX = 62.0f;
constexpr auto sButtonX = 156.0f;
constexpr auto sButtonDim = glm::vec2{104, 14};
constexpr auto sCaptionDim = glm::vec2{90, 14};
constexpr auto sRow0Y = 34.0f;
constexpr auto sRowStep = 20.0f;
constexpr auto sStatusPos = glm::vec2{52, 150};
constexpr auto sStatusDim = glm::vec2{216, 12};

constexpr DisplaySettings sDefaultSettings{
    4, Graphics::WindowMode::Windowed, 0, true, true};

glm::vec2 RowCaption(int row) { return glm::vec2{sCaptionX, sRow0Y + sRowStep * row}; }
glm::vec2 RowButton(int row) { return glm::vec2{sButtonX, sRow0Y + sRowStep * row}; }

std::string ModeString(Graphics::WindowMode m)
{
    switch (m)
    {
        case Graphics::WindowMode::Windowed: return "Windowed";
        case Graphics::WindowMode::BorderlessFullscreen: return "Borderless";
        case Graphics::WindowMode::ExclusiveFullscreen: return "Exclusive";
    }
    return "Windowed";
}

const std::array sScaleChoices{3, 4, 5, 6};

}

PreferencesScreen::PreferencesScreen(
    IGuiManager& guiManager,
    const Backgrounds& backgrounds,
    const Font& font,
    LeavePreferencesFn&& leavePreferenceFn)
:
    Widget{
        RectTag{},
        glm::vec2{0, 0},
        glm::vec2{320, 200},
        Color::black,
        false
    },
    mGuiManager{guiManager},
    mFont{font},
    mBackgrounds{backgrounds},
    mLayout{sLayoutFile},
    mLeavePreferencesFn{std::move(leavePreferenceFn)},
    mFrame{
        ImageTag{},
        backgrounds.GetSpriteSheet(),
        backgrounds.GetScreen(sBackground),
        glm::vec2{0},
        GetPositionInfo().mDimensions,
        true
    },
    mPanel{RectTag{}, sPanelPos, sPanelDim, Color::infoBackground, false},
    mWindowCaption{RowCaption(0), sCaptionDim, mFont, "#Window"},
    mScaleCaption{RowCaption(1), sCaptionDim, mFont, "#Scale"},
    mAutoCaption{RowCaption(2), sCaptionDim, mFont, "#Auto-fit"},
    mMonitorCaption{RowCaption(3), sCaptionDim, mFont, "#Monitor"},
    mVSyncCaption{RowCaption(4), sCaptionDim, mFont, "#V-Sync"},
    mWindowButton{RowButton(0), sButtonDim, mFont, "#Windowed", [this]{ CycleWindowMode(); }},
    mScaleButton{RowButton(1), sButtonDim, mFont, "#4x", [this]{ CycleScale(); }},
    mAutoButton{RowButton(2), sButtonDim, mFont, "#On", [this]{ CycleAutoScale(); }},
    mMonitorButton{RowButton(3), sButtonDim, mFont, "#0", [this]{ CycleMonitor(); }},
    mVSyncButton{RowButton(4), sButtonDim, mFont, "#On", [this]{ CycleVSync(); }},
    mStatusLabel{sStatusPos, sStatusDim, mFont, ""},
    mOk{
        mLayout.GetWidgetLocation(sOk),
        mLayout.GetWidgetDimensions(sOk),
        mFont,
        "#OK",
        [this]{ OnOk(); }
    },
    mCancel{
        mLayout.GetWidgetLocation(sCancel),
        mLayout.GetWidgetDimensions(sCancel),
        mFont,
        "#Cancel",
        [this]{ OnCancel(); }
    },
    mDefaults{
        mLayout.GetWidgetLocation(sDefaults),
        mLayout.GetWidgetDimensions(sDefaults),
        mFont,
        "#Defaults",
        [this]{ OnDefaults(); }
    },
    mController{nullptr},
    mEntrySettings{sDefaultSettings},
    mPendingSettings{sDefaultSettings},
    mAwaitingConfirm{false},
    mRevertSeconds{0},
    mRevertGeneration{0},
    mRevertTimer{nullptr}
{
    AddChildren();
}

void PreferencesScreen::OnEnter()
{
    mController = DisplayControllerProvider::Get();
    if (mController)
    {
        mEntrySettings = mController->GetCurrentSettings();
        mPendingSettings = mEntrySettings;
    }
    DisarmRevert();
    RefreshLabels();
}

void PreferencesScreen::AddChildren()
{
    ClearChildren();

    AddChildBack(&mFrame);
    AddChildBack(&mPanel);

    AddChildBack(&mWindowCaption);
    AddChildBack(&mScaleCaption);
    AddChildBack(&mAutoCaption);
    AddChildBack(&mMonitorCaption);
    AddChildBack(&mVSyncCaption);

    AddChildBack(&mWindowButton);
    AddChildBack(&mScaleButton);
    AddChildBack(&mAutoButton);
    AddChildBack(&mMonitorButton);
    AddChildBack(&mVSyncButton);

    AddChildBack(&mStatusLabel);

    mOk.SetPosition(mLayout.GetWidgetLocation(sOk));
    mCancel.SetPosition(mLayout.GetWidgetLocation(sCancel));
    mDefaults.SetPosition(mLayout.GetWidgetLocation(sDefaults));

    AddChildBack(&mOk);
    AddChildBack(&mCancel);
    AddChildBack(&mDefaults);

    RefreshLabels();
}

void PreferencesScreen::RefreshLabels()
{
    const auto& s = mPendingSettings;
    mWindowButton.SetText(ModeString(s.mWindowMode));
    mScaleButton.SetText(std::to_string(s.mUiScale) + "x");
    mAutoButton.SetText(s.mAutoScale ? "On" : "Off");
    mMonitorButton.SetText(std::to_string(s.mMonitor));
    mVSyncButton.SetText(s.mVSync ? "On" : "Off");

    if (mAwaitingConfirm)
        mStatusLabel.SetText(
            "Keep? OK to confirm - reverting in " + std::to_string(mRevertSeconds) + "s");
    else
        mStatusLabel.SetText("");
}

void PreferencesScreen::CycleWindowMode()
{
    using WM = Graphics::WindowMode;
    auto& m = mPendingSettings.mWindowMode;
    m = m == WM::Windowed ? WM::BorderlessFullscreen
      : m == WM::BorderlessFullscreen ? WM::ExclusiveFullscreen
      : WM::Windowed;
    OnChanged();
}

void PreferencesScreen::CycleScale()
{
    const int maxScale = mController ? mController->GetMaxUiScale(mPendingSettings.mMonitor) : 6;
    // Next allowed choice (clamped to fit the monitor), wrapping around.
    int next = mPendingSettings.mUiScale;
    for (std::size_t i = 0; i < sScaleChoices.size(); ++i)
    {
        const auto idx = (std::find(sScaleChoices.begin(), sScaleChoices.end(), mPendingSettings.mUiScale)
            - sScaleChoices.begin() + static_cast<long>(i) + 1) % sScaleChoices.size();
        if (sScaleChoices[idx] <= maxScale)
        {
            next = sScaleChoices[idx];
            break;
        }
    }
    mPendingSettings.mUiScale = next;
    OnChanged();
}

void PreferencesScreen::CycleAutoScale()
{
    mPendingSettings.mAutoScale = !mPendingSettings.mAutoScale;
    OnChanged();
}

void PreferencesScreen::CycleMonitor()
{
    const int count = mController ? mController->GetMonitorCount() : 1;
    mPendingSettings.mMonitor = (mPendingSettings.mMonitor + 1) % std::max(count, 1);
    OnChanged();
}

void PreferencesScreen::CycleVSync()
{
    mPendingSettings.mVSync = !mPendingSettings.mVSync;
    OnChanged();
}

void PreferencesScreen::OnChanged()
{
    if (mController)
        mController->PreviewSettings(mPendingSettings);
    ArmRevert();
    RefreshLabels();
}

void PreferencesScreen::ArmRevert()
{
    // Invalidate any in-flight timer, then start a fresh 15s countdown.
    DisarmRevert();
    mAwaitingConfirm = true;
    mRevertSeconds = sRevertSeconds;
    const int generation = ++mRevertGeneration;
    auto timer = std::make_unique<TickAnimator>(
        1.0, [this, generation]{ OnRevertTick(generation); });
    mRevertTimer = timer.get();
    mGuiManager.AddAnimator(std::move(timer));
}

void PreferencesScreen::DisarmRevert()
{
    mAwaitingConfirm = false;
    ++mRevertGeneration;
    if (mRevertTimer)
    {
        mRevertTimer->Stop();
        mRevertTimer = nullptr;
    }
}

void PreferencesScreen::OnRevertTick(int generation)
{
    if (generation != mRevertGeneration)
        return;
    --mRevertSeconds;
    if (mRevertSeconds <= 0)
        DoRevert();
    else
        RefreshLabels();
}

void PreferencesScreen::DoRevert()
{
    mPendingSettings = mEntrySettings;
    if (mController)
        mController->PreviewSettings(mEntrySettings);
    DisarmRevert();
    RefreshLabels();
}

void PreferencesScreen::OnOk()
{
    if (mController)
        mController->CommitSettings(mPendingSettings);
    mEntrySettings = mPendingSettings;
    DisarmRevert();
    std::invoke(mLeavePreferencesFn);
}

void PreferencesScreen::OnCancel()
{
    if (mController && mPendingSettings != mEntrySettings)
        mController->PreviewSettings(mEntrySettings);
    mPendingSettings = mEntrySettings;
    DisarmRevert();
    std::invoke(mLeavePreferencesFn);
}

void PreferencesScreen::OnDefaults()
{
    mPendingSettings = sDefaultSettings;
    OnChanged();
}

}
