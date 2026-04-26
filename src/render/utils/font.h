#pragma once
#include <raylib.h>

namespace Render::Utils::Font {

    inline ::Font activeFont = {};

    inline void setActiveFont(::Font f) {
        activeFont = f;
    }

    inline void drawText(const char* text, int x, int y, float size, Color color) {
        DrawTextEx(activeFont, text, {(float)x, (float)y}, size, size / 10.0f, color);
    }

    inline void drawText(const char* text, Vector2 pos, float size, Color color) {
        DrawTextEx(activeFont, text, pos, size, size / 10.0f, color);
    }

}