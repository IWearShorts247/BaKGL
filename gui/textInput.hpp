#pragma once

#include "gui/button.hpp"
#include "gui/core/widget.hpp"

#include "gui/textBox.hpp"

namespace Gui {
class Font;

class TextInput : public Widget
{
public:
    TextInput(
        const Font& font,
        glm::vec2 pos,
        glm::vec2 dim,
        unsigned maxChars);
    
    ~TextInput() override;

    bool OnMouseEvent(const MouseEvent& event) override;
    bool OnKeyEvent(const KeyEvent& event) override;

    void SetText(const std::string& text);
    const std::string& GetText() const;
    void SetFocus(bool focus);

    // True while any TextInput currently holds keyboard focus. Lets global
    // hotkeys (e.g. the crossfade toggle) stand down during text entry.
    static bool AnyFocused();
private:
    bool LeftMousePressed(const auto& clickPos);
    bool KeyPressed(int key);
    void RefreshText();
    bool CharacterEntered(char character);

    const Font& mFont;
    Button mButton;
    Widget mHighlight;
    TextBox mTextBox;
    std::string mText;
    unsigned mMaxChars;
    bool mHaveFocus;

    static int sFocusCount;
};

}
