#pragma once
#include <cmath>

namespace Orr::Render::Playfield {
    constexpr int PLAYFIELD_W = 512;
    constexpr int PLAYFIELD_H = 384;
    constexpr int GRID_STEP = 32;

    struct Playfield {
        float scale;
        float x;
        float y;
        float width;
        float height;
    };

    Playfield ComputePlayfield(int screenW, int screenH) {
        Playfield pf;
        pf.scale = fminf((float)screenW / 640.0f, (float)screenH / 480.0f);
        pf.width = PLAYFIELD_W * pf.scale;
        pf.height = PLAYFIELD_H * pf.scale;
        pf.x = (screenW - 640.0f * pf.scale) / 2.0f + 64.0f * pf.scale;
        pf.y = (screenH - 480.0f * pf.scale) / 2.0f + 56.0f * pf.scale;
        return pf;
    }

    inline void DrawPlayfieldGrid(const Playfield& pf) {
        for (int gx = 0; gx <= PLAYFIELD_W; gx += GRID_STEP) {
            float sx = pf.x + gx * pf.scale;
            DrawLine((int)sx, (int)pf.y, (int)sx, (int)(pf.y + pf.height), DARKGRAY);
        }
        for (int gy = 0; gy <= PLAYFIELD_H; gy += GRID_STEP) {
            float sy = pf.y + gy * pf.scale;
            DrawLine((int)pf.x, (int)sy, (int)(pf.x + pf.width), (int)sy, DARKGRAY);
        }
        DrawRectangleLines((int)pf.x, (int)pf.y, (int)pf.width, (int)pf.height, WHITE);
    }

}