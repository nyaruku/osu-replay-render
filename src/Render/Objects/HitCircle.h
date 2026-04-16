#pragma once
#include <raylib.h>
#include <cmath>
#include "../../Reader/Beatmap.h"
#include "../Playfield.h"


namespace Orr::Render::Objects::HitCircle {

    inline bool INSTANT_DISAPPEAR = false;
    constexpr float FADE_OUT_MS = 150.0f;

    inline void Draw(const Orr::Reader::Beatmap::HitObject& obj, float current_ms, const Orr::Render::Playfield::Playfield& pf, float base_radius, float preempt) {
        float time_until = (float)obj.time - current_ms;

        if (INSTANT_DISAPPEAR && time_until < 0.0f)
            return;
        if (!INSTANT_DISAPPEAR && time_until < -FADE_OUT_MS)
            return;

        float r = base_radius * pf.scale;
        Vector2 pos = { pf.x + (float)obj.x * pf.scale, pf.y + (float)obj.y * pf.scale };

        float fade_alpha;
        const float fade_in_duration = preempt * (2.0f / 3.0f);
        if (time_until > preempt) {
            fade_alpha = 0.0f;
        } else if (time_until > preempt / 3.0f) {
            fade_alpha = 1.0f - ((time_until - preempt / 3.0f) / fade_in_duration);
        } else if (time_until >= 0.0f) {
            fade_alpha = 1.0f;
        } else {
            fade_alpha = 1.0f - std::min(1.0f, -time_until / FADE_OUT_MS);
        }
        unsigned char alpha = (unsigned char)(fade_alpha * 255.0f);

        float body_scale = time_until < 0.0f ? (1.0f + 0.4f * (-time_until / FADE_OUT_MS)) : 1.0f;
        float approach_scale = time_until >= 0.0f ? (1.0f + 3.0f * std::max(0.0f, time_until / preempt)) : 0.0f;

        if (approach_scale > 0.0f) {
            DrawCircleLines((int)pos.x, (int)pos.y, r * approach_scale, { 255, 255, 255, alpha });
        }

        DrawCircleV(pos, r * body_scale, { 70, 130, 180, alpha });
        DrawCircleLinesV(pos, r * body_scale, { 255, 255, 255, alpha });
    }
}