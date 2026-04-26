#pragma once
#include <processor/score/types.h>
#include <processor/score/hit_detection.h>
#include <processor/score/stable_scorer.h>
#include <reader/beatmap.h>
#include <raylib.h>
#include <vector>
#include <cstdio>
namespace Processor::Score {
    struct CalculatorInput {
        const Reader::Beatmap::Beatmap&           beatmap;
        const std::vector<ReplayFrame>&           frames;
        const std::vector<std::vector<Vector2>>&  sliderPaths;      // precomputed polylines in osu! coords
        const std::vector<float>&                 sliderPathLengths; // totalLength per slider polyline
        const std::vector<float>&                 objEndTimes;       // per-object effective end time
        float                                     circleRadius;
        int                                       mods;
        double                                    scoreMultiplier;
        ScoringMode                               mode = ScoringMode::Stable;
    };
    // hitInfosOut: if non-null, per-object hit results are written there
    inline ScoreResult calculate(const CalculatorInput& in, std::vector<HitInfo>* hitInfosOut = nullptr) {
        auto hitInfos = detectHits(
            in.frames, in.beatmap.hitObjects,
            in.circleRadius, in.beatmap.overallDifficulty,
            in.mods, in.objEndTimes);
        if (hitInfosOut) *hitInfosOut = hitInfos;
        // lazer not implemented, falls back to stable
        return buildStableScore(
            in.frames, in.beatmap, hitInfos,
            in.sliderPaths, in.sliderPathLengths,
            in.circleRadius, in.mods, in.scoreMultiplier);
    }
    inline void printComparison(
        const ScoreResult& result,
        long long repScore, int repMaxCombo,
        int repN300, int repN100, int repN50, int repNmiss
    ) {
        if (result.cumScore.empty()) return;
        const int ne      = (int)result.cumScore.size();
        long long cs      = result.cumScore[ne-1];
        int       cc      = result.cumMaxCombo[ne-1];
        int       c300    = result.cumN300[ne-1];
        int       c100    = result.cumN100[ne-1];
        int       c50     = result.cumN50[ne-1];
        int       cmiss   = result.cumNmiss[ne-1];
        printf("%-12s %12s %12s %12s\n", "field",     "calc",    "replay", "diff");
        printf("%-12s %12lld %12lld %12lld\n", "score",     cs,    repScore,    cs    - repScore);
        printf("%-12s %12d %12d %12d\n",       "max_combo", cc,    repMaxCombo, cc    - repMaxCombo);
        printf("%-12s %12d %12d %12d\n",       "300",       c300,  repN300,     c300  - repN300);
        printf("%-12s %12d %12d %12d\n",       "100",       c100,  repN100,     c100  - repN100);
        printf("%-12s %12d %12d %12d\n",       "50",        c50,   repN50,      c50   - repN50);
        printf("%-12s %12d %12d %12d\n",       "miss",      cmiss, repNmiss,    cmiss - repNmiss);
    }
}