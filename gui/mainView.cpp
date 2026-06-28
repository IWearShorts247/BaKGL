#include "gui/mainView.hpp"

#include "bak/dialog.hpp"
#include "bak/dialogSources.hpp"
#include "bak/gameState.hpp"

#include "gui/IGuiManager.hpp"
#include "gui/backgrounds.hpp"
#include "gui/fontManager.hpp"
#include "gui/icons.hpp"

#include <glm/glm.hpp>

#include <iostream>

namespace Gui {

MainView::MainView(
    IGuiManager& guiManager,
    const Backgrounds& backgrounds,
    const Icons& icons,
    const Font& spellFont,
    const Font& gameFont)
:
    Widget{
        Graphics::DrawMode::Sprite,
        backgrounds.GetSpriteSheet(),
        backgrounds.GetScreen("FRAME.SCX"),
        Graphics::ColorMode::Texture,
        glm::vec4{1},
        glm::vec2{0},
        glm::vec2{320, 200},
        true
    },
    mGuiManager{guiManager},
    mIcons{icons},
    mSpellFont{spellFont},
    mGameFont{gameFont},
    mLayout{sLayoutFile},
    mActiveSpells{},
    mCompass{
        glm::vec2{144,121},
        glm::vec2{32,12},
        std::get<glm::vec2>(icons.GetCompass())
            + glm::vec2{0, 1},
        std::get<Graphics::SpriteSheetIndex>(icons.GetCompass()),
        std::get<Graphics::TextureIndex>(icons.GetCompass())
    },
    mButtons{},
    mCharacters{},
    mBookmarkPopup{
        glm::vec2{60, 35},
        glm::vec2{200, 55},
        mGameFont,
        "",
        []{}
    },
    mLogger{Logging::LogState::GetLogger("Gui::MainView")}
{
    const auto& snippet = BAK::DialogStore::Get().GetSnippet(
        BAK::DialogSources::mBookmarkCheck);
    const auto popup = snippet.GetPopup();
    assert(popup);
    mBookmarkPopup.SetPosition(popup->mPos);
    mBookmarkPopup.SetDimensions(popup->mDims);
    mBookmarkPopup.SetText(snippet.GetText(), true);
    mBookmarkPopup.SetInactive();

    mButtons.reserve(mLayout.GetSize());

    for (unsigned i = 0; i < mLayout.GetSize(); i++)
    {
        const auto& widget = mLayout.GetWidget(i);
        switch (widget.mWidget)
        {
        case 3: //REQ_IMAGEBUTTON
        {
            const auto textures = icons.GetButtonTextures(widget.mImage);
            const auto& button = icons.GetButton(widget.mImage);
            assert(std::get<Graphics::SpriteSheetIndex>(button)
                == textures.mSpriteSheet);
            mButtons.emplace_back(
                mLayout.GetWidgetLocation(i),
                mLayout.GetWidgetDimensions(i),
                textures,
                [this, buttonIndex=i]{ HandleButton(buttonIndex); },
                []{});

            mButtons.back().CenterImage(std::get<glm::vec2>(button));
            // Not sure why the dims aren't right to begin with for these buttons
            if (i == sForward || i == sBackward)
            {
                mButtons.back().AdjustPosition(
                    glm::vec2{-mButtons.back().GetDimensions().x / 4 + 1.5, 0});
            }
        }
            break;
        default:
            mLogger.Info() << "Unhandled: " << i << "\n";
            break;
        }
    }

    // Local-map button bar (replaces the travel buttons in map mode), matching the original's
    // REQ_MAP.DAT layout: a 2x3 grid of round BICONS1 buttons (all 34x29). Image indices and
    // positions are taken straight from REQ_MAP.DAT; the user's upscaled BICONS1_10/_11
    // override the zoom icons.
    const auto addMapIcon = [&](glm::vec2 pos, unsigned image, std::function<void()> cb)
    {
        mMapIconButtons.emplace_back(
            pos, glm::vec2{34, 29}, icons.GetButtonTextures(image), std::move(cb), []{});
        mMapIconButtons.back().CenterImage(std::get<glm::vec2>(icons.GetButton(image)));
    };
    mMapIconButtons.reserve(6);
    addMapIcon({200, 130}, 22,
        [this]{ mGuiManager.ToggleFollowRoad(); UpdateFollowRoadIcon(); });
    addMapIcon({237, 130}, 10, [this]{ mGuiManager.LocalMapZoomOut(); });
    addMapIcon({273, 130},  7, [this]{ mGuiManager.ShowCamp(false, nullptr); });
    addMapIcon({200, 164}, 12, [this]{ mGuiManager.ShowFullMap(); });
    addMapIcon({236, 164}, 11, [this]{ mGuiManager.LocalMapZoomIn(); });
    addMapIcon({273, 164}, 61, [this]{ mGuiManager.EnterMainView(); });

    AddChildren();
}

void MainView::SetMapMode(bool mapMode)
{
    mMapMode = mapMode;
    if (mapMode)
        UpdateFollowRoadIcon();
    AddChildren();
}

void MainView::UpdateFollowRoadIcon()
{
    // Follow Road is the first map-bar button; show the "on" icon (BICONS1 23) while
    // road-following is active, otherwise the "off" icon (22).
    if (mMapIconButtons.empty()) return;
    const auto textures = mIcons.GetButtonTextures(mGuiManager.IsFollowingRoad() ? 23 : 22);
    mMapIconButtons.front().SetTexture(textures.mSpriteSheet, textures.mNormal);
}

void MainView::SetHeading(BAK::GameHeading heading)
{
    mCompass.SetHeading(heading);
}

void MainView::HandleButton(unsigned buttonIndex)
{
    switch (buttonIndex)
    {
    case sCast:
        mGuiManager.ShowCast(false);
        break;
    case sCamp:
        mGuiManager.ShowCamp(false, nullptr);
        break;
    case sFullMap:
        // The map icon now opens the local "immediate surroundings" map first; the kingdom
        // map is reached from there via the Full Map button.
        mGuiManager.EnterLocalMap();
        break;
    case sSnapToRoad:
        mGuiManager.ToggleFollowRoad();
        break;
    case sBookmark:
        mShowingBookmarkDialog = true;
        mNeedRefresh = true;
        break;
    case sMainMenu:
        mGuiManager.EnterMainMenu(true);
        break;
    default:
        break;
    }
}

void MainView::SetCanSaveBookmark(bool canSaveBookmark)
{
    mCanSaveBookmark = canSaveBookmark;
    mNeedRefresh = true;
}

bool MainView::OnMouseEvent(const MouseEvent& event)
{
    if (mShowingBookmarkDialog)
    {
        if (std::holds_alternative<LeftMousePress>(event))
        {
            mGuiManager.SaveBookmark();
        }

        if (std::holds_alternative<LeftMousePress>(event)
            || std::holds_alternative<RightMousePress>(event))
        {
            mShowingBookmarkDialog = false;
            mNeedRefresh = true;
        }

        if (mNeedRefresh)
        {
            AddChildren();
            mNeedRefresh = false;
        }

        return true;
    }

    const bool handled = Widget::OnMouseEvent(event);

    if (mNeedRefresh)
    {
        AddChildren();
        mNeedRefresh = false;
    }

    return handled;
}

void MainView::UpdatePartyMembers(const BAK::GameState& gameState)
{
    ClearChildren();

    mCharacters.clear();
    mCharacters.reserve(3);

    const auto& party = gameState.GetParty();
    mLogger.Spam() << "Updating Party: " << party<< "\n";
    BAK::ActiveCharIndex person{0};
    do
    {
        const auto [spriteSheet, image, dimss] = mIcons.GetCharacterHead(
            party.GetCharacter(person).GetIndex().mValue);
        mCharacters.emplace_back(
            mLayout.GetWidgetLocation(person.mValue + sCharacterWidgetBegin),
            mLayout.GetWidgetDimensions(person.mValue + sCharacterWidgetBegin),
            spriteSheet,
            image,
            image,
            [this, character=person]{
                ShowInventory(character);
            },
            [this, character=person]{
                ShowPortrait(character);
            }
        );
        
        person = party.NextActiveCharacter(person);
    } while (person != BAK::ActiveCharIndex{0});

    auto pos = glm::vec2{140, 1};

    // FIXME: Update these whenever time changes...
    mActiveSpells.clear();
    for (std::uint16_t i = 0; i < 6; i++)
    {
        if (gameState.GetSpellActive(BAK::StaticSpells{i}))
        {
            auto spellI = BAK::sStaticSpellMapping[i];
            mActiveSpells.emplace_back(Gui::Widget{
                Graphics::DrawMode::Sprite,
                mSpellFont.GetSpriteSheet(),
                static_cast<Graphics::TextureIndex>(
                    mSpellFont.GetFont().GetIndex(spellI)),
                Graphics::ColorMode::Texture,
                glm::vec4{1.2f, 0.f, 0.f, 1.f},
                pos,
                glm::vec2{
                    mSpellFont.GetFont().GetWidth(spellI),
                    mSpellFont.GetFont().GetHeight()},
                true
            });
            pos += glm::vec2{mSpellFont.GetFont().GetWidth(spellI) + 1, 0};
        }
    }

    AddChildren();
}

void MainView::ShowPortrait(BAK::ActiveCharIndex character)
{
    mGuiManager.ShowCharacterPortrait(character);
}

void MainView::ShowInventory(BAK::ActiveCharIndex character)
{
    mGuiManager.ShowInventory(character);
}

void MainView::AddChildren()
{
    ClearChildren();
    if (mMapMode)
    {
        for (auto& button : mMapIconButtons)
            AddChildBack(&button);
    }
    else
    {
        for (unsigned i = 0; i < mButtons.size(); i++)
        {
            if (i == sBookmark && !mCanSaveBookmark)
                continue;
            AddChildBack(&mButtons[i]);
        }
    }
    for (auto& spell : mActiveSpells)
    {
        AddChildBack(&spell);
    }
    AddChildBack(&mCompass);

    for (auto& character : mCharacters)
        AddChildBack(&character);

    if (mShowingBookmarkDialog)
    {
        AddChildBack(&mBookmarkPopup);
    }
}

}
