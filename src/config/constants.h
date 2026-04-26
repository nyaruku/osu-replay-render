#pragma once
#include <raylib.h>
#include <raygui.h>

namespace Config::Constants {

    namespace FontSize {
        constexpr float SCREEN_TITLE   = 36.0f;
        constexpr float PAGE_TITLE     = 26.0f;
        constexpr float SECTION_HEADER = 25.0f;
        constexpr float LABEL          = 16.0f;
        constexpr float BODY           = 18.0f;
        constexpr float SMALL          = 14.0f;
        constexpr float BTN            = 17.0f;
        constexpr float HUD            = 18.0f;
        constexpr float HUD_SMALL      = 14.0f;
    }

    namespace Theme {
        // General background
        constexpr Color BG                   = {18, 18, 18, 255};
        constexpr Color BG_PANEL             = {28, 28, 28, 255};

        // Text
        constexpr Color TEXT_PRIMARY         = WHITE;
        constexpr Color TEXT_SECONDARY       = LIGHTGRAY;
        constexpr Color TEXT_MUTED           = GRAY;
        constexpr Color TEXT_ACCENT          = {140, 180, 220, 255};
        constexpr Color TEXT_DISABLED        = {90, 90, 90, 255};

        // Borders / dividers
        constexpr Color BORDER               = {55, 55, 55, 255};
        constexpr Color BORDER_ACTIVE        = SKYBLUE;
        constexpr Color BORDER_DIVIDER       = {40, 40, 40, 255};
        constexpr Color BORDER_SECTION       = {50, 50, 60, 255};

        // Button (default)
        constexpr Color BTN_BG               = {55, 55, 55, 255};
        constexpr Color BTN_BG_HOVER         = {30, 30, 30, 255};
        constexpr Color BTN_BG_PRESSED       = {30, 30, 30, 255};
        constexpr Color BTN_BG_DISABLED      = {38, 38, 38, 255};
        constexpr Color BTN_TEXT             = WHITE;
        constexpr Color BTN_TEXT_DISABLED    = RED;
        constexpr Color BTN_BORDER           = {70, 70, 70, 255};

        // Button (primary / accent)
        constexpr Color BTN_PRIMARY_BG         = BLUE;
        constexpr Color BTN_PRIMARY_BG_HOVER   = {30, 30, 30, 255};
        constexpr Color BTN_PRIMARY_BG_PRESSED = {30, 30, 30, 255};
        constexpr Color BTN_PRIMARY_TEXT       = WHITE;

        // Input / text-box
        constexpr Color INPUT_BG             = {30, 30, 30, 255};
        constexpr Color INPUT_BG_HOVER       = {38, 38, 48, 255};
        constexpr Color INPUT_BG_ACTIVE      = {45, 45, 60, 255};
        constexpr Color INPUT_BORDER         = {55, 55, 55, 255};
        constexpr Color INPUT_BORDER_ACTIVE  = SKYBLUE;
        constexpr Color INPUT_TEXT           = WHITE;
        constexpr Color INPUT_PLACEHOLDER    = {90, 90, 90, 255};
        constexpr Color INPUT_CURSOR         = SKYBLUE;
        constexpr Color INPUT_SELECTION      = {50, 90, 140, 160};

        // Checkbox
        constexpr Color CHECKBOX_BG          = {30, 30, 30, 255};
        constexpr Color CHECKBOX_BG_HOVER    = {40, 40, 55, 255};
        constexpr Color CHECKBOX_BG_CHECKED  = SKYBLUE;
        constexpr Color CHECKBOX_BORDER      = {55, 55, 55, 255};
        constexpr Color CHECKBOX_CHECK_MARK  = WHITE;

        // Spinner (numeric up/down)
        constexpr Color SPINNER_BTN_BG       = {50, 50, 65, 255};
        constexpr Color SPINNER_BTN_BG_HOVER = {70, 70, 90, 255};
        constexpr Color SPINNER_BTN_TEXT     = WHITE;

        // List
        constexpr Color ITEM_BG_HOVER        = {35, 35, 45, 255};
        constexpr Color ITEM_BG_SELECTED     = {50, 90, 140, 255};
        constexpr Color ITEM_TEXT_SELECTED   = WHITE;

        // Section header strip
        constexpr Color SECTION_BG           = {35, 35, 45, 255};
        constexpr Color SECTION_TEXT         = {140, 180, 220, 255};
    }

    inline void applyThemeToRaygui(Font font) {
        using namespace Theme;
        using namespace FontSize;

        auto c = [](Color c) {
            return ColorToInt(c);
        };

        GuiSetFont(font);
        GuiSetStyle(DEFAULT, TEXT_SIZE, 18);

        GuiSetStyle(DEFAULT, BACKGROUND_COLOR,       c(BG));
        GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,      c(TEXT_PRIMARY));
        GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED,     c(TEXT_PRIMARY));
        GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED,     c(TEXT_PRIMARY));
        GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED,    c(TEXT_DISABLED));
        GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL,    c(BORDER));
        GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED,   c(BORDER_ACTIVE));
        GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED,   c(BORDER_ACTIVE));
        GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL,      c(BG_PANEL));
        GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED,     c(ITEM_BG_HOVER));
        GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED,     c(ITEM_BG_SELECTED));
        GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED,    c(BG));

        GuiSetStyle(BUTTON, BASE_COLOR_NORMAL,       c(BTN_BG));
        GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED,      c(BTN_BG_HOVER));
        GuiSetStyle(BUTTON, BASE_COLOR_PRESSED,      c(BTN_BG_PRESSED));
        GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,       c(BTN_TEXT));
        GuiSetStyle(BUTTON, TEXT_COLOR_FOCUSED,      c(BTN_TEXT));
        GuiSetStyle(BUTTON, TEXT_COLOR_PRESSED,      c(BTN_TEXT));
        GuiSetStyle(BUTTON, BORDER_COLOR_NORMAL,     c(BTN_BORDER));
        GuiSetStyle(BUTTON, BORDER_COLOR_FOCUSED,    c(BORDER_ACTIVE));
        GuiSetStyle(BUTTON, BORDER_COLOR_PRESSED,    c(BORDER_ACTIVE));

        GuiSetStyle(TEXTBOX, BASE_COLOR_NORMAL,      c(INPUT_BG));
        GuiSetStyle(TEXTBOX, BASE_COLOR_FOCUSED,     c(INPUT_BG_ACTIVE));
        GuiSetStyle(TEXTBOX, BASE_COLOR_PRESSED,     c(INPUT_BG_ACTIVE));
        GuiSetStyle(TEXTBOX, BORDER_COLOR_NORMAL,    c(INPUT_BORDER));
        GuiSetStyle(TEXTBOX, BORDER_COLOR_FOCUSED,   c(INPUT_BORDER_ACTIVE));
        GuiSetStyle(TEXTBOX, TEXT_COLOR_NORMAL,      c(INPUT_TEXT));
        GuiSetStyle(TEXTBOX, TEXT_COLOR_FOCUSED,     c(INPUT_TEXT));

        GuiSetStyle(LISTVIEW, BASE_COLOR_NORMAL,     c(INPUT_BG));
        GuiSetStyle(LISTVIEW, BASE_COLOR_FOCUSED,    c(ITEM_BG_HOVER));
        GuiSetStyle(LISTVIEW, BASE_COLOR_PRESSED,    c(ITEM_BG_SELECTED));
        GuiSetStyle(LISTVIEW, TEXT_COLOR_NORMAL,     c(TEXT_SECONDARY));
        GuiSetStyle(LISTVIEW, TEXT_COLOR_FOCUSED,    c(TEXT_PRIMARY));
        GuiSetStyle(LISTVIEW, TEXT_COLOR_PRESSED,    c(TEXT_PRIMARY));
        GuiSetStyle(LISTVIEW, BORDER_COLOR_NORMAL,   c(BORDER));
        GuiSetStyle(LISTVIEW, BORDER_COLOR_FOCUSED,  c(BORDER_ACTIVE));

        GuiSetStyle(COMBOBOX, BASE_COLOR_NORMAL,     c(BTN_BG));
        GuiSetStyle(COMBOBOX, BASE_COLOR_FOCUSED,    c(BTN_BG_HOVER));
        GuiSetStyle(COMBOBOX, TEXT_COLOR_NORMAL,     c(BTN_TEXT));
        GuiSetStyle(COMBOBOX, BORDER_COLOR_NORMAL,   c(BORDER));

        GuiSetStyle(SLIDER, BORDER_WIDTH,            0);
        GuiSetStyle(SLIDER, BORDER_COLOR_NORMAL,     c(INPUT_BG));
        GuiSetStyle(SLIDER, BORDER_COLOR_FOCUSED,    c(INPUT_BG));
        GuiSetStyle(SLIDER, BORDER_COLOR_PRESSED,    c(INPUT_BG));
        GuiSetStyle(SLIDER, BASE_COLOR_NORMAL,       c(INPUT_BG));      // track bg (unfilled)
        GuiSetStyle(SLIDER, BASE_COLOR_FOCUSED,      c(BORDER_ACTIVE)); // fill when hovered
        GuiSetStyle(SLIDER, BASE_COLOR_PRESSED,      c(BORDER_ACTIVE)); // fill when pressed
        GuiSetStyle(SLIDER, TEXT_COLOR_NORMAL,       c(TEXT_MUTED));
        GuiSetStyle(SLIDER, TEXT_COLOR_FOCUSED,      c(BORDER_ACTIVE)); // fill color when hovered
        GuiSetStyle(SLIDER, TEXT_COLOR_PRESSED,      c(BORDER_ACTIVE));
        GuiSetStyle(SLIDER, SLIDER_WIDTH,            12);
        GuiSetStyle(SLIDER, SLIDER_PADDING,          2);

        GuiSetStyle(PROGRESSBAR, BORDER_WIDTH,          0);
        GuiSetStyle(PROGRESSBAR, BASE_COLOR_NORMAL,     c(INPUT_BG));
        GuiSetStyle(PROGRESSBAR, BASE_COLOR_PRESSED,    c(BORDER_ACTIVE));
        GuiSetStyle(PROGRESSBAR, BORDER_COLOR_NORMAL,   c(INPUT_BG));
    }
}