#pragma once

#include "bak/layout.hpp"

#include "gui/core/widget.hpp"
#include "gui/clickButton.hpp"
#include "gui/displaySettings.hpp"
#include "gui/label.hpp"

#include <glm/glm.hpp>

namespace Gui {

class IGuiManager;
class Backgrounds;
class Font;
class TickAnimator;

class PreferencesScreen: public Widget
{
public:
    static constexpr auto sLayoutFile = "REQ_PREF.DAT";
    static constexpr auto sBackground = "OPTIONS2.SCX";

    static constexpr auto sOk = 0;
    static constexpr auto sCancel = 1;
    static constexpr auto sDefaults = 2;

    // Applied when there are unconfirmed display changes and the user does not confirm.
    static constexpr auto sRevertSeconds = 15;

    using LeavePreferencesFn = std::function<void()>;

    PreferencesScreen(
        IGuiManager& guiManager,
        const Backgrounds& backgrounds,
        const Font& font,
        LeavePreferencesFn&& leavePreferenceFn);

    // Called each time the screen is shown: snapshots the current display settings so
    // Cancel / the revert timer can restore them.
    void OnEnter();

private:
    void AddChildren();
    void RefreshLabels();

    // Display-option cyclers.
    void CycleWindowMode();
    void CycleScale();
    void CycleAutoScale();
    void CycleMonitor();
    void CycleVSync();

    // Apply/confirm flow.
    void OnChanged();        // preview the pending settings live + (re)arm the revert timer
    void ArmRevert();
    void DisarmRevert();
    void OnRevertTick(int generation);
    void DoRevert();         // restore the entry settings (live), stay in the menu
    void OnOk();
    void OnCancel();
    void OnDefaults();

    IGuiManager& mGuiManager;
    const Font& mFont;
    const Backgrounds& mBackgrounds;

    BAK::Layout mLayout;
    LeavePreferencesFn mLeavePreferencesFn;

    Widget mFrame;
    Widget mPanel;

    Label mWindowCaption;
    Label mScaleCaption;
    Label mAutoCaption;
    Label mMonitorCaption;
    Label mVSyncCaption;

    ClickButton mWindowButton;
    ClickButton mScaleButton;
    ClickButton mAutoButton;
    ClickButton mMonitorButton;
    ClickButton mVSyncButton;

    Label mStatusLabel;

    ClickButton mOk;
    ClickButton mCancel;
    ClickButton mDefaults;

    IDisplayController* mController;
    DisplaySettings mEntrySettings;
    DisplaySettings mPendingSettings;

    bool mAwaitingConfirm;
    int mRevertSeconds;
    int mRevertGeneration;
    TickAnimator* mRevertTimer;
};

}
