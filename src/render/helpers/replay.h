#pragma once

namespace Render::Replay {
    inline float computePreempt(float ar) {
        if (ar < 5.0f)
            return 1200.0f + 120.0f * (5.0f - ar);
        if (ar > 5.0f)
            return 1200.0f - 150.0f * (ar - 5.0f);
        return 1200.0f;
    }

    inline float computePreemptWithMods(float ar, int mods) {
        const int MOD_EZ = 2;
        const int MOD_HR = 16;
        const int MOD_DT = 64;
        const int MOD_NC = 512;
        const int MOD_HT = 256;

        if (mods & MOD_EZ)
            ar /= 2.0f;
        if (mods & MOD_HR)
            ar = std::min(ar * 1.4f, 10.0f);

        float preempt = computePreempt(ar);

        if (mods & MOD_DT || mods & MOD_NC)
            preempt /= 1.5f;
        else if (mods & MOD_HT)
            preempt /= 0.75f;

        return preempt;
    }

    inline float computeCircleRadius(float cs, int mods) {
        const int MOD_EZ = 2;
        const int MOD_HR = 16;

        if (mods & MOD_EZ)
            cs /= 2.0f;
        if (mods & MOD_HR)
            cs = std::min(cs * 1.3f, 10.0f);

        return (54.4f - 4.48f * cs) * 1.00041f;
    }
}
