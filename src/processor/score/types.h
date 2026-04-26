#pragma once
#include <vector>

namespace Processor::Score {

struct ReplayFrame {
    long long timeMs;
    float     x, y;
    int       keys; // M1=1 M2=2 K1=4 K2=8
};

struct HitInfo {
    bool  valid       = false;
    bool  clicked     = false;
    float timingErr   = 0.0f;  // |press_time - obj.time| ms, 0 for hold-through
    float clickTime   = 0.0f;  // obj.time for hold-through
    int   clickButton = 0;     // 0=hold-through  1=left(M1|K1)  2=right(M2|K2)
};

struct ScoreEvent {
    float timeMs;
    int   baseValue;    // 300 / 100 / 50 / 30 / 10 / 0
    bool  comboReset;
    bool  comboInc;
    int   jtype;        // 1=300  2=100  3=50  4=miss  0=slider sub-element
    bool  noComboMult;  // flat score, no combo multiplier (slider head/tick/repeat/tail)
};

// prefix-sum arrays: index i = cumulative state after events[i]
struct ScoreResult {
    std::vector<ScoreEvent> events;
    std::vector<long long>  cumScore;
    std::vector<int>        cumCombo;
    std::vector<int>        cumMaxCombo;
    std::vector<int>        cumN300, cumN100, cumN50, cumNmiss;
};

enum class ScoringMode {
    Stable,
    Lazer,
};

} // namespace Processor::Score

