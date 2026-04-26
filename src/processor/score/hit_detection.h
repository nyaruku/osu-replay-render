#pragma once
#include <processor/score/types.h>
#include <reader/beatmap.h>
#include <reader/replay.h>
#include <vector>
#include <algorithm>
#include <cmath>
namespace Processor::Score {
namespace detail {
    // osu! stable virtual button groups:
    //   M1 + K1 = "left"  -- pressing K1 while M1 is already held is NOT a new press.
    //   M2 + K2 = "right" -- same rule.
    inline constexpr int KEY_LEFT  = (int)Reader::Replay::Keys::M1 | (int)Reader::Replay::Keys::K1;
    inline constexpr int KEY_RIGHT = (int)Reader::Replay::Keys::M2 | (int)Reader::Replay::Keys::K2;
    inline constexpr int KEY_ALL   = KEY_LEFT | KEY_RIGHT;
} // namespace detail
inline std::vector<HitInfo> detectHits(
    const std::vector<ReplayFrame>&                  frames,
    const std::vector<Reader::Beatmap::HitObject>&   hitObjects,
    float                                            circleRadius,
    float                                            od,
    int                                              mods,
    const std::vector<float>&                        objEndTimes)
{
    using namespace detail;
    const int n = (int)hitObjects.size();
    std::vector<HitInfo> result(n);
    // mod-adjusted OD and playback rate
    float odUsed = od;
    if (mods & (int)Reader::Replay::Mods::HardRock) odUsed = std::min(10.0f, odUsed * 1.4f);
    else if (mods & (int)Reader::Replay::Mods::Easy) odUsed /= 2.0f;
    float rate = 1.0f;
    if (mods & ((int)Reader::Replay::Mods::DoubleTime | (int)Reader::Replay::Mods::Nightcore)) rate = 1.5f;
    else if (mods & (int)Reader::Replay::Mods::HalfTime) rate = 0.75f;
    const float W50 = std::floor(200.0f - 10.0f * odUsed) / rate;
    const float CR2 = circleRadius * circleRadius;
    // binary-search cursor state at an arbitrary time
    auto cursorAt = [&](float ms, float& ox, float& oy, bool& keyHeld) {
        int lo = 0, hi = (int)frames.size() - 1, fi = 0;
        while (lo <= hi) { int m = (lo+hi)/2; if (frames[m].timeMs <= (long long)ms) { fi=m; lo=m+1; } else hi=m-1; }
        ox = frames[fi].x; oy = frames[fi].y;
        keyHeld = (frames[fi].keys & KEY_ALL) != 0;
        if (fi + 1 < (int)frames.size()) {
            long long seg = frames[fi+1].timeMs - frames[fi].timeMs;
            if (seg > 0) {
                float t = std::min(1.0f, (ms - (float)frames[fi].timeMs) / (float)seg);
                ox += (frames[fi+1].x - frames[fi].x) * t;
                oy += (frames[fi+1].y - frames[fi].y) * t;
            }
        }
    };
    // hold-through check for sliders
    auto tryHoldThrough = [&](int idx) {
        auto& obj = hitObjects[idx];
        if (!obj.isSlider || (obj.type & 8)) return;
        float hcx, hcy; bool hkey;
        cursorAt((float)obj.time, hcx, hcy, hkey);
        if (!hkey) return;
        float dx = hcx - (float)obj.x, dy = hcy - (float)obj.y;
        if (dx*dx + dy*dy <= CR2) {
            result[idx].valid     = true;
            result[idx].clickTime = (float)obj.time;
        }
    };
    struct PressInfo { long long time; float cx, cy; int btn; };
    std::vector<PressInfo> presses;
    {
        bool pL = false, pR = false;
        for (auto& f : frames) {
            bool cL = (f.keys & KEY_LEFT)  != 0;
            bool cR = (f.keys & KEY_RIGHT) != 0;
            if (cL && !pL) presses.push_back({ f.timeMs, f.x, f.y, 1 });
            if (cR && !pR) presses.push_back({ f.timeMs, f.x, f.y, 2 });
            pL = cL; pR = cR;
        }
        std::sort(presses.begin(), presses.end(), [](auto& a, auto& b){ return a.time < b.time; });
    }
    int objPtr = 0;
    // skip spinners at front
    while (objPtr < n && (hitObjects[objPtr].type & 8)) objPtr++;
    // notelock pass: each press tries to hit the first unresolved object
    for (auto& press : presses) {
        long long pt = press.time;
        while (objPtr < n && (hitObjects[objPtr].type & 8)) objPtr++;
        if (objPtr >= n) break;
        float ot = (float)hitObjects[objPtr].time;
        // expire objects whose late window has passed
        while (objPtr < n && pt > (long long)(ot + W50)) {
            if (!result[objPtr].valid) tryHoldThrough(objPtr);
            objPtr++;
            while (objPtr < n && (hitObjects[objPtr].type & 8)) objPtr++;
            if (objPtr < n) ot = (float)hitObjects[objPtr].time;
        }
        if (objPtr >= n) break;
        ot = (float)hitObjects[objPtr].time;
        if (pt < (long long)(ot - W50)) continue; // notelock: press is too early
        auto& obj = hitObjects[objPtr];
        if (obj.isSlider && !(obj.type & 8) && (float)pt > objEndTimes[objPtr] - 240.0f) continue;
        float dx = press.cx - (float)obj.x, dy = press.cy - (float)obj.y;
        if (dx*dx + dy*dy > CR2) continue;
        result[objPtr] = { true, true, std::abs((float)pt - ot), (float)pt, press.btn };
        objPtr++;
    }
    // remaining unresolved: hold-through for sliders
    for (; objPtr < n; objPtr++) {
        if (!result[objPtr].valid) tryHoldThrough(objPtr);
    }
    return result;
}
} // namespace Processor::Score
