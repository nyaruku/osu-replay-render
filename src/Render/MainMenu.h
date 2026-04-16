#pragma once
#include <raylib.h>
#include <string>
#include "Font.h"

namespace Orr::Render::MainMenu {

    inline std::string Draw() {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        if (IsFileDropped()) {
            FilePathList dropped = LoadDroppedFiles();
            std::string path;
            if (dropped.count > 0) {
                path = dropped.paths[0];
            }
            UnloadDroppedFiles(dropped);
            if (!path.empty() && path.size() > 4 && path.substr(path.size() - 4) == ".osr") {
                return path;
            }
        }

        float btn_w = 300.0f;
        float btn_h = 55.0f;
        Rectangle btn = {
            (sw - btn_w) / 2.0f,
            (sh - btn_h) / 2.0f,
            btn_w,
            btn_h
        };

        Vector2 mouse = GetMousePosition();
        bool hovered = CheckCollisionPointRec(mouse, btn);

        BeginDrawing();
        ClearBackground(BLACK);

        const char* title = "osu replay render";
        float title_size = 36.0f;
        Vector2 title_dim = MeasureTextEx(ActiveFont, title, title_size, title_size / 10.0f);
        DrawText(title, (sw - title_dim.x) / 2.0f, sh * 0.3f, title_size, WHITE);

        const char* hint = "Drop a .osr file to start";
        float hint_size = 18.0f;
        Vector2 hint_dim = MeasureTextEx(ActiveFont, hint, hint_size, hint_size / 10.0f);
        DrawText(hint, (sw - hint_dim.x) / 2.0f, sh * 0.55f, hint_size, GRAY);

        EndDrawing();
        return "";
    }

}

