#pragma once

namespace Render::Objects::HitCircle {
    inline bool instantDisappear = false;
    // Matching stable: HitObjectManager.FadeOut = 240
    constexpr float FADE_OUT_MS = 240.0f;

    // Matching stable: HitObjectManager.FadeIn = 400
    constexpr float FADE_IN_MS = 400.0f;

    inline float computeFadeAlpha(float timeUntil, float preempt) {
        if (timeUntil > preempt) {
            return 0.0f;
        }
        // Stable: Fade(0->1, StartTime - PreEmpt, StartTime - PreEmpt + FadeIn)
        if (timeUntil > preempt - FADE_IN_MS) {
            return (preempt - timeUntil) / FADE_IN_MS;
        }
        if (timeUntil >= 0.0f) {
            return 1.0f;
        }
        // Stable: Fade(1->0, StartTime, StartTime + FadeOut) for circles
        return 1.0f - std::min(1.0f, -timeUntil / FADE_OUT_MS);
    }

    inline float computeApproachScale(float timeUntil, float preempt) {
        if (timeUntil < 0.0f) {
            return 0.0f;
        }
        return 1.0f + 3.0f * std::max(0.0f, timeUntil / preempt);
    }

    inline float computeBodyScale(float timeUntil) {
        if (timeUntil < 0.0f) {
            return 1.0f + 0.4f * (-timeUntil / FADE_OUT_MS);
        }
        return 1.0f;
    }
}