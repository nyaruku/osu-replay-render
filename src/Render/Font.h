#pragma once
#include <raylib.h>

namespace Orr::Render {

    inline Font ActiveFont = {};

    inline void SetActiveFont(Font f) {
        ActiveFont = f;
    }

    inline void DrawText(const char* text, int x, int y, float size, Color color) {
        DrawTextEx(ActiveFont, text, {(float)x, (float)y}, size, size / 10.0f, color);
    }

    inline void DrawText(const char* text, Vector2 pos, float size, Color color) {
        DrawTextEx(ActiveFont, text, pos, size, size / 10.0f, color);
    }

}