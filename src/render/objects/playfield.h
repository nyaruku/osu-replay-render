#pragma once
#include <config/config.h>

namespace Render::Objects::Playfield {
    constexpr int PLAYFIELD_W = 512;
    constexpr int PLAYFIELD_H = 384;

    struct Playfield {
        float scale;
        float x;
        float y;
        float width;
        float height;
    };

    #include <render/helpers/playfield.h>

    inline void drawPlayfieldGrid(const Playfield& pf) {
        Color gridColor = {255, 255, 255, (unsigned char)(Config::get<float>("objects.playfield.opacity")*255)};
        int step = Config::get<int>("objects.playfield.step");
        if (step <= 0) step = 64;
        if (step > PLAYFIELD_H) step = 512; // clamp to one square
        for (int gx = 0; gx <= PLAYFIELD_W; gx += step) {
            float sx = pf.x + gx * pf.scale;
            DrawLine((int)sx, (int)pf.y, (int)sx, (int)(pf.y + pf.height), gridColor);
        }
        for (int gy = 0; gy <= PLAYFIELD_H; gy += step) {
            float sy = pf.y + gy * pf.scale;
            DrawLine((int)pf.x, (int)sy, (int)(pf.x + pf.width), (int)sy, gridColor);
        }
        DrawRectangleLines((int)pf.x, (int)pf.y, (int)pf.width, (int)pf.height, gridColor);
    }
}