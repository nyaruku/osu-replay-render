#pragma once
#include <raylib.h>
#include <cmath>
#include <reader/beatmap.h>
#include <render/objects/playfield.h>
#include <render/helpers/hitcircle.h>

namespace Render::Objects::HitCircle {
    inline void draw(const Reader::Beatmap::HitObject& obj, float currentMs, const Render::Objects::Playfield::Playfield& pf, float baseRadius, float preempt) {
        float timeUntil = (float)obj.time - currentMs;

        if (instantDisappear && timeUntil < 0.0f) {
            return;
        }
        if (!instantDisappear && timeUntil < -FADE_OUT_MS) {
            return;
        }

        float r = baseRadius * pf.scale;
        Vector2 pos = { pf.x + (float)obj.x * pf.scale, pf.y + (float)obj.y * pf.scale };
        unsigned char alpha = (unsigned char)(computeFadeAlpha(timeUntil, preempt) * 255.0f);
        float approachScale = computeApproachScale(timeUntil, preempt);
        float bodyScale = computeBodyScale(timeUntil);

        if (approachScale > 0.0f) {
            DrawCircleLines((int)pos.x, (int)pos.y, r * approachScale, { 255, 255, 255, alpha });
        }

        DrawCircleV(pos, r * bodyScale, { 70, 130, 180, alpha });
        DrawCircleLinesV(pos, r * bodyScale, { 255, 255, 255, alpha });
    }
}