#pragma once
#include <raylib.h>
#include <cmath>
#include "../../Reader/Beatmap.h"
#include "../Playfield.h"
#include "../Font.h"

namespace Orr::Render::Objects::Spinner {

    inline void Draw(const Orr::Reader::Beatmap::HitObject& obj, float current_ms, const Orr::Render::Playfield::Playfield& pf) {
        if (current_ms < (float)obj.time)
            return;

        Vector2 center = { pf.x + 256.0f * pf.scale, pf.y + 192.0f * pf.scale };
        float duration = (float)(obj.end_time - obj.time);
        float elapsed = current_ms - (float)obj.time;
        float progress = duration > 0.0f ? std::min(1.0f, elapsed / duration) : 1.0f;

        float outer_r = 100.0f * pf.scale;
        float inner_r = 8.0f * pf.scale;
        DrawCircleLines((int)center.x, (int)center.y, outer_r, WHITE);
        DrawCircleLines((int)center.x, (int)center.y, inner_r, WHITE);

        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f%%", progress * 100.0f);
        Vector2 dim = MeasureTextEx(Orr::Render::ActiveFont, buf, 18.0f, 1.8f);
        Orr::Render::DrawText(buf, (int)(center.x - dim.x / 2.0f), (int)(center.y + outer_r + 8.0f), 18.0f, WHITE);
    }
}