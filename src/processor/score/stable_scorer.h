#pragma once
#include <processor/score/types.h>
#include <reader/beatmap.h>
#include <reader/replay.h>
#include <render/objects/slider.h>
#include <raylib.h>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace Processor::Score {
inline ScoreResult buildStableScore(
    const std::vector<ReplayFrame>&              frames,
    const Reader::Beatmap::Beatmap&              beatmap,
    const std::vector<HitInfo>&                  hitInfos,
    const std::vector<std::vector<Vector2>>&     sliderPaths,
    const std::vector<float>&                    sliderPathLengths,
    float                                        circleRadius,
    int                                          mods,
    double                                       scoreMultiplier)
{
    // hit windows (OD + mod-adjusted)
    float od = beatmap.overallDifficulty;
    if (mods & (int)Reader::Replay::Mods::HardRock) od = std::min(10.0f, od * 1.4f);
    else if (mods & (int)Reader::Replay::Mods::Easy) od /= 2.0f;
    float rate = 1.0f;
    if (mods & ((int)Reader::Replay::Mods::DoubleTime | (int)Reader::Replay::Mods::Nightcore)) rate = 1.5f;
    else if (mods & (int)Reader::Replay::Mods::HalfTime) rate = 0.75f;
    const float W300 = std::floor(80.0f  - 6.0f  * od) / rate;
    const float W100 = std::floor(140.0f - 8.0f  * od) / rate;
    // per-frame button-state arrays (danser mouseDownAcceptable model)
    // mouseDownButton/lastButton/lastButton2 update only on CHANGE events;
    // leftCond/rightCond are per-frame transitions. See danser ruleset.go:385-399.
    constexpr int LEFT_GROUP  = (int)Reader::Replay::Keys::M1 | (int)Reader::Replay::Keys::K1;
    constexpr int RIGHT_GROUP = (int)Reader::Replay::Keys::M2 | (int)Reader::Replay::Keys::K2;
    const int nframes = (int)frames.size();
    std::vector<uint8_t> fMDB(nframes, 0);   // mouseDownButton
    std::vector<uint8_t> fLB(nframes,  0);   // lastButton
    std::vector<uint8_t> fLB2(nframes, 0);   // lastButton2
    std::vector<uint8_t> fLC(nframes,  0);   // leftCond (new left press this frame)
    std::vector<uint8_t> fRC(nframes,  0);   // rightCond (new right press this frame)
    {
        int pk = 0;
        uint8_t mdb = 0, lb = 0, lb2 = 0;
        for (int fi = 0; fi < nframes; fi++) {
            int k  = frames[fi].keys;
            int cL = (k  & LEFT_GROUP)  ? 1 : 0;
            int cR = (k  & RIGHT_GROUP) ? 1 : 0;
            int pL = (pk & LEFT_GROUP)  ? 1 : 0;
            int pR = (pk & RIGHT_GROUP) ? 1 : 0;
            fLC[fi] = (uint8_t)(cL && !pL);
            fRC[fi] = (uint8_t)(cR && !pR);
            if (cL != pL || cR != pR) { lb2 = lb; lb = mdb; mdb = (uint8_t)((cL ? 1 : 0) | (cR ? 2 : 0)); }
            fMDB[fi] = mdb; fLB[fi] = lb; fLB2[fi] = lb2;
            pk = k;
        }
    }
    const float followR  = circleRadius * 2.4f;
    const float followR2 = followR * followR;
    const float circleR2 = circleRadius * circleRadius;
    const int n = (int)beatmap.hitObjects.size();
    ScoreResult res;
    auto& events = res.events;
    events.reserve(n * 4);
    for (int i = 0; i < n; i++) {
        auto& obj = beatmap.hitObjects[i];
        // spinner
        if (obj.type & 8) {
            events.push_back({ (float)obj.endTime, 300, false, true, 1, false });
            continue;
        }
        const HitInfo& hi = hitInfos[i];
        // circle
        if (!obj.isSlider) {
            int jtype = 4, base = 0;
            if (hi.valid) {
                if      (hi.timingErr <= W300) { jtype = 1; base = 300; }
                else if (hi.timingErr <= W100) { jtype = 2; base = 100; }
                else                            { jtype = 3; base = 50;  }
            }
            events.push_back({ (float)obj.time, base, jtype == 4, jtype != 4, jtype, false });
            continue;
        }
        // slider
        // Head event (flat 30 points, no combo multiplier)
        if (hi.clicked) {
            events.push_back({ (float)obj.time, 30, false, true,  0, true }); // clicked
        } else if (hi.valid) {
            events.push_back({ (float)obj.time, 30, false, false, 0, true }); // hold-through: no combo break, no combo++
        } else {
            events.push_back({ (float)obj.time,  0, true,  false, 0, true }); // miss head: combo breaks
        }
        // Slider timing
        float sv  = (float)Render::Objects::Slider::getSvAt(obj.time, beatmap.timingPoints);
        float svc = std::clamp(sv, 0.1f, 10.0f);
        double bl = Render::Objects::Slider::getBeatLengthAt(obj.time, beatmap.timingPoints);
        if (!std::isfinite(bl) || bl <= 0.0) bl = 500.0;
        float slideDur = (float)(obj.length / ((double)beatmap.sliderMultiplier * 100.0 * svc) * bl);
        auto& sPath    = sliderPaths[i];
        float pathLen = sliderPathLengths[i];
        // Ball position in osu! coordinates at time t_ms
        auto ballPos = [&](float t_ms) -> Vector2 {
            float elapsed = t_ms - (float)obj.time;
            if (pathLen <= 0.0f || sPath.empty() || slideDur <= 0.0f)
                return { (float)obj.x, (float)obj.y };
            float sp = std::fmod(elapsed, slideDur) / slideDur;
            bool  rv = ((int)(elapsed / slideDur)) % 2 == 1;
            float d  = rv ? (1.0f - sp) * (float)obj.length : sp * (float)obj.length;
            return Render::Objects::Slider::Path::sampleAt(sPath, d);
        };
        // tick / repeat / tail generation (danser InitTimingValues)
        double tickD   = (double)beatmap.sliderMultiplier * 100.0 * svc / (double)beatmap.sliderTickRate;
        double velocity = (double)beatmap.sliderMultiplier * 100.0 * svc * 1000.0 / bl;
        float  cLength  = pathLen > 0.0f ? pathLen : (float)obj.length;
        if (cLength > 0.0f && tickD > obj.length) tickD = obj.length;
        if (tickD > 0.0 && cLength / tickD > 32768.0) tickD = cLength / 32768.0;
        double minTailD = velocity * 0.01;
        struct SubElem { float time; int score; bool isTick, isRepeat, isTail; };
        std::vector<SubElem> elems;
        float sliderEndMs = slideDur > 0.0f
            ? (float)obj.time + slideDur * obj.slides
            : (float)obj.time;
        int polyN = (int)sPath.size();
        if (tickD > 0.0 && cLength > 0.0f && velocity > 0.0 && polyN >= 2) {
            // Precompute per-segment polyline lengths
            std::vector<float> segLens;
            segLens.reserve(polyN - 1);
            for (int s = 1; s < polyN; s++) {
                float dx = sPath[s].x - sPath[s-1].x, dy = sPath[s].y - sPath[s-1].y;
                segLens.push_back(sqrtf(dx*dx + dy*dy));
            }
            double sLT = 0, sD = 0, curEnd = (double)obj.time;
            for (int rep = 0; rep < obj.slides; rep++) {
                double distToEnd = cLength;
                bool   skipTick  = false;
                int ss = (rep & 1) == 0 ? 0 : (int)segLens.size() - 1;
                int se = (rep & 1) == 0 ? (int)segLens.size() : -1;
                int sp = (rep & 1) == 0 ? 1 : -1;
                for (int sj = ss; sj != se; sj += sp) {
                    double ds = segLens[sj];
                    curEnd += 1000.0 * ds / velocity;
                    sD     += ds;
                    while (sD >= tickD && !skipTick) {
                        sLT       += tickD;
                        sD        -= tickD;
                        distToEnd -= tickD;
                        skipTick   = (distToEnd <= minTailD);
                        if (skipTick) break;
                        float st = (float)((double)obj.time + std::floor(sLT / velocity * 1000.0));
                        elems.push_back({ st, 10, true, false, false });
                    }
                }
                sLT += sD;
                float repTime;
                bool  isLast = (rep == obj.slides - 1);
                if (isLast) repTime = (float)curEnd;
                else         repTime = (float)((double)obj.time + std::floor(sLT / velocity * 1000.0));
                elems.push_back({ repTime, 30, false, !isLast, isLast });
                if (skipTick) sD = 0;
                else { sLT -= tickD - sD; sD = tickD - sD; }
            }
        // Per ruleset slider.go:109 - override last score point's time
            if (!elems.empty()) {
                float totDur    = (float)curEnd - (float)obj.time;
                float adjTail   = std::max((float)obj.time + totDur * 0.5f, (float)curEnd - 36.0f);
                int   lastI = 0;
                for (int e = 1; e < (int)elems.size(); e++)
                    if (elems[e].time > elems[lastI].time) lastI = e;
                elems[lastI].time     = adjTail;
                elems[lastI].isTail   = true;
                elems[lastI].isRepeat = false;
                sliderEndMs = (float)curEnd;
            }
        } else {
            // Degenerate slider: single tail point
            float tailT = std::max(
                (float)obj.time + slideDur * (float)obj.slides * 0.5f,
                sliderEndMs - 36.0f);
            elems.push_back({ tailT, 30, false, false, true });
        }
        std::sort(elems.begin(), elems.end(), [](auto& a, auto& b){ return a.time < b.time; });
        int totalParts = 1 + (int)elems.size();
        int hitParts   = hi.valid ? 1 : 0;
        // slider tracking state
        bool    tracking     = hi.valid;
        float   slideStartT  = hi.valid ? std::max((float)obj.time, hi.clickTime) : 1e18f;
        uint8_t stateDownBtn = (uint8_t)(hi.clickButton == 1 ? 1 : (hi.clickButton == 2 ? 2 : 0));
        float   sliderStart  = (float)obj.time;
        // Binary search for first frame at or after slider start
        int fiLo = 0, fiHi = nframes - 1, fiStart = 0;
        while (fiLo <= fiHi) {
            int mid = (fiLo + fiHi) / 2;
            if (frames[mid].timeMs < (long long)sliderStart) { fiStart = mid; fiLo = mid + 1; }
            else fiHi = mid - 1;
        }
        int fiCur = fiStart;
        // Per-frame tracking update: danser mouseDownAcceptable + follow-radius check
        auto updateTracking = [&](int fi) {
            float ft = (float)frames[fi].timeMs;
            if (ft < sliderStart || !hi.valid) return;
            uint8_t mdb  = fMDB[fi], lb = fLB[fi], lb2 = fLB2[fi];
            bool    lc   = fLC[fi] != 0, rc = fRC[fi] != 0;
            bool    gameDown = (mdb != 0);
            bool    mdaSwap  = gameDown && !(lb == 3 && lb2 == mdb);
            bool acceptable = false;
            if (gameDown) {
                if (stateDownBtn == 0 || (mdb != 3 && mdaSwap)) {
                    stateDownBtn = 0;
                    if      (lc) stateDownBtn = 1;
                    else if (rc) stateDownBtn = 2;
                    else         stateDownBtn = mdb;
                    acceptable = true;
                } else if (mdb & stateDownBtn) {
                    acceptable = true;
                }
            } else {
                stateDownBtn = 0;
            }
            acceptable = acceptable || mdaSwap;
            Vector2 ball = ballPos(ft);
            float   cx = frames[fi].x, cy = frames[fi].y;
            float   dx = cx - ball.x,  dy = cy - ball.y, d2 = dx*dx + dy*dy;
            bool wasTracking = tracking;
            bool inRange     = d2 < (wasTracking ? followR2 : circleR2);
            bool nowTracking = acceptable && inRange;
            if (nowTracking && !wasTracking) slideStartT = ft;
            tracking = nowTracking;
        };
        // Process sub-elements, advancing frames and scoring each
        for (int ei = 0; ei < (int)elems.size(); ei++) {
            float tickT = elems[ei].time;
            while (fiCur < nframes && (float)frames[fiCur].timeMs < tickT)
                updateTracking(fiCur++);
            if (fiCur < nframes) updateTracking(fiCur);
            // Tail requires no tracking loss since head (slideStartT <= obj.time)
            bool tickHit;
            if (elems[ei].isTail)
                tickHit = tracking && (slideStartT <= (float)obj.time);
            else
                tickHit = tracking && (slideStartT <= tickT);
            if (tickHit) {
                events.push_back({ tickT, elems[ei].score, false, true, 0, true });
                hitParts++;
            } else if (ei < (int)elems.size() - 1) {
                events.push_back({ tickT, 0, true, false, 0, true });
            }
        }
        // Overall slider judgment (with combo multiplier, does NOT increment combo)
        float ratio = totalParts > 0 ? (float)hitParts / (float)totalParts : 0.0f;
        int   jtype = ratio >= 1.0f ? 1 : (ratio >= 0.5f ? 2 : (ratio > 0.0f ? 3 : 4));
        int   base  = jtype == 1 ? 300 : (jtype == 2 ? 100 : (jtype == 3 ? 50 : 0));
        events.push_back({ sliderEndMs + 1.0f, base, jtype == 4, false, jtype, false });
    }
    // Sort events by time
    std::sort(events.begin(), events.end(), [](auto& a, auto& b){ return a.timeMs < b.timeMs; });
    // Build prefix arrays
    const int ne = (int)events.size();
    res.cumScore.resize(ne);    res.cumCombo.resize(ne);    res.cumMaxCombo.resize(ne);
    res.cumN300.resize(ne);     res.cumN100.resize(ne);     res.cumN50.resize(ne);     res.cumNmiss.resize(ne);
    long long rs = 0;
    int combo = 0, maxc = 0, n300 = 0, n100 = 0, n50 = 0, nmiss = 0;
    for (int ei = 0; ei < ne; ei++) {
        auto& ev = events[ei];
        if (ev.comboReset) { combo = 0; if (ev.jtype == 4) nmiss++; }
        long long add = ev.baseValue;
        if (add > 0 && !ev.noComboMult)
            add += (long long)(std::max(0, combo - 1) * (ev.baseValue / 25) * scoreMultiplier);
        rs += add;
        if (ev.comboInc) combo++;
        if (combo > maxc) maxc = combo;
        if      (ev.jtype == 1) n300++;
        else if (ev.jtype == 2) n100++;
        else if (ev.jtype == 3) n50++;
        res.cumScore[ei] = rs;     res.cumCombo[ei] = combo; res.cumMaxCombo[ei] = maxc;
        res.cumN300[ei]  = n300;   res.cumN100[ei]  = n100;  res.cumN50[ei] = n50; res.cumNmiss[ei] = nmiss;
    }
    return res;
}
} // namespace Processor::Score
