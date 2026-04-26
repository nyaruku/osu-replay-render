#pragma once
#include <raylib.h>
#include <reader/beatmap.h>
#include <render/objects/playfield.h>
#include <render/utils/font.h>

namespace Render::Objects::Spinner {

    inline void draw(const Reader::Beatmap::HitObject& obj, float currentMs, const Render::Objects::Playfield::Playfield& pf) {
        if (currentMs < (float)obj.time)
            return;

        Vector2 center = { pf.x + 256.0f * pf.scale, pf.y + 192.0f * pf.scale };
        float duration = (float)(obj.endTime - obj.time);
        float elapsed = currentMs - (float)obj.time;
        float progress = duration > 0.0f ? std::min(1.0f, elapsed / duration) : 1.0f;

        float outerR = 100.0f * pf.scale;
        float innerR = 8.0f * pf.scale;
        DrawCircleLines((int)center.x, (int)center.y, outerR, WHITE);
        DrawCircleLines((int)center.x, (int)center.y, innerR, WHITE);

        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f%%", progress * 100.0f);
        Vector2 dim = MeasureTextEx(Utils::Font::activeFont, buf, 18.0f, 1.8f);
        Utils::Font::drawText(buf, (int)(center.x - dim.x / 2.0f), (int)(center.y + outerR + 8.0f), 18.0f, WHITE);
    }
}