#pragma once
#include <raylib.h>
#include <reader/beatmap.h>
#include <reader/replay.h>
#include <database/database.h>
#include <render/utils/font.h>
#include <render/utils/toast.h>
#include <render/views/debug.h>
#include <render/helpers/replay.h>
#include <render/objects/playfield.h>
#include <render/objects/hitcircle.h>
#include <render/objects/spinner.h>
#include <render/objects/slider.h>
#include <render/objects/keyoverlay.h>
#include <resources/drum_hitclap.h>
#include <processor/leeway.h>
#include <processor/score/calculator.h>
namespace Render::Replay {
    using AbsFrame = Processor::Score::ReplayFrame;
    inline bool precacheSliders = true;
    constexpr float MUSIC_VOLUME    = 0.2f;
    constexpr float SKIP_OFFSET_SEC = 2.0f;
    constexpr float HITSOUND_VOLUME = 0.6f;
    struct State {
        Reader::Beatmap::Beatmap beatmap;
        Reader::Replay::Replay   replay;
        std::vector<AbsFrame>    absFrames;
        Music                    music         = {};
        bool                     musicLoaded   = false;
        bool                     initialized   = false;
        bool                     skipped       = false;
        std::string              error;
        float  preempt        = 0.0f;
        float  circleRadius   = 0.0f;
        int    cursorFrame    = 0;
        double playStartWall  = 0.0;
        float  playStartMs    = 0.0f;
        int   maxFps       = 0;
        int   minFps       = INT_MAX;
        float maxFrameMs   = 0.0f;
        float minFrameMs   = 999.0f;
        int   statsWarmup  = 0;
        std::vector<std::vector<Vector2>>           sliderPaths;
        std::vector<std::vector<Vector2>>           sliderScreenPaths;
        std::vector<float>                          sliderPathLengths;
        std::vector<Render::Objects::Slider::SliderBodyCache> sliderBodyCaches;
        float                                       cachedPfScale = 0.0f;
        std::vector<int>                            sortedIndices;
        std::vector<float>                          objEndTimes;
        Sound hitsound = {};
        bool  hitsoundLoaded = false;
        static constexpr int HITSOUND_POOL_SIZE = 8;
        Sound hitsoundPool[HITSOUND_POOL_SIZE] = {};
        int   hitsoundPoolIdx = 0;
        std::vector<bool> hitsoundPlayed;
        // Per-object hit detection results (for hitsound + score)
        std::vector<Processor::Score::HitInfo> hitInfos;
        float lastCurrentMs = -99999.0f;
        // Slider end/repeat hitsound trigger times (sorted)
        std::vector<float> sliderHsTimes;
        int sliderHsCursor = 0;
        // Incremental precache state
        bool  precachePending      = false;
        int   precacheCursor       = 0;
        int   precacheSliderTotal  = 0;
        int   precacheSlidersDone  = 0;
        float precacheTargetScale  = 0.0f;
        bool  musicPrecachePaused  = false;
        // Key overlay
        Render::Objects::Keyoverlay::State key_overlay;
        // score: prefix-sum arrays filled by init()
        Processor::Score::ScoreResult scoreResult;
        bool showScoreDebug = false;
    };
    inline State gameplayState;
    // included here because it requires gameplayState
    #include <render/helpers/slider.h>
    inline void init(const std::string& osr_path, Database::BeatmapDb& db) {
        gameplayState = State{};
        precacheSliders = Config::get<bool>("objects.slider.texture_pre_calculation");
        try {
            // load replay + beatmap
            gameplayState.replay = Reader::Replay::readOsr(osr_path);
            auto entry = Database::findByMd5(db, gameplayState.replay.beatmapMd5);
            if (!entry) {
                gameplayState.error =
                    "Beatmap not found in DB for MD5: " + gameplayState.replay.beatmapMd5 +
                    "\nMake sure your songs directory is correct and try re-indexing.\n"
                    "Press ESC to return to menu.";
                return;
            }
            gameplayState.beatmap = Reader::Beatmap::readBeatmapFile(entry->filePath);
            // audio
            std::filesystem::path beatmap_dir = std::filesystem::path(entry->filePath).parent_path();
            std::string audio_path = (beatmap_dir / gameplayState.beatmap.audioFilename).string();
            gameplayState.music = LoadMusicStream(audio_path.c_str());
            if (gameplayState.music.frameCount > 0) {
                gameplayState.musicLoaded = true;
                SetMusicVolume(gameplayState.music, Config::get<float>("audio.music_volume", MUSIC_VOLUME));
                float lead_in_sec = gameplayState.beatmap.audioLeadIn > 0
                    ? (float)gameplayState.beatmap.audioLeadIn / 1000.0f : 0.0f;
                gameplayState.playStartMs = -lead_in_sec * 1000.0f;
            }
            // difficulty params
            gameplayState.preempt      = computePreemptWithMods(gameplayState.beatmap.approachRate, gameplayState.replay.mods);
            gameplayState.circleRadius = computeCircleRadius(gameplayState.beatmap.circleSize, gameplayState.replay.mods);
            // build absolute-time frames
            {
                long long abs_time = 0;
                for (auto& f : gameplayState.replay.frames) {
                    abs_time += f.timeDelta;
                    gameplayState.absFrames.push_back({ abs_time, f.x, f.y, f.keys });
                }
            }
            gameplayState.initialized = true;
            // hitsound pool
            Wave hsWave = LoadWaveFromMemory(".wav", Resources::DRUM_HITCLAP_WAV, (int)Resources::drum_hitclap_wav_len);
            gameplayState.hitsound = LoadSoundFromWave(hsWave);
            UnloadWave(hsWave);
            gameplayState.hitsoundLoaded = gameplayState.hitsound.frameCount > 0;
            if (gameplayState.hitsoundLoaded) {
                float hvol = Config::get<float>("audio.hitsound_volume", HITSOUND_VOLUME);
                SetSoundVolume(gameplayState.hitsound, hvol);
                for (int p = 0; p < State::HITSOUND_POOL_SIZE; p++) {
                    gameplayState.hitsoundPool[p] = LoadSoundAlias(gameplayState.hitsound);
                    SetSoundVolume(gameplayState.hitsoundPool[p], hvol);
                }
            }
            // per-object setup (slider paths + end times)
            int n = (int)gameplayState.beatmap.hitObjects.size();
            gameplayState.sliderPaths.resize(n);
            gameplayState.sliderScreenPaths.resize(n);
            gameplayState.sliderPathLengths.resize(n, 0.0f);
            gameplayState.sliderBodyCaches.resize(n);
            gameplayState.objEndTimes.resize(n, 0.0f);
            gameplayState.hitsoundPlayed.resize(n, false);
            gameplayState.sortedIndices.resize(n);
            for (int i = 0; i < n; i++) {
                gameplayState.sortedIndices[i] = i;
                auto& obj = gameplayState.beatmap.hitObjects[i];
                if (obj.isSlider) {
                    gameplayState.sliderPaths[i]       = Render::Objects::Slider::computeSliderPath(obj);
                    gameplayState.sliderPathLengths[i] = Render::Objects::Slider::Path::totalLength(gameplayState.sliderPaths[i]);
                    float sv  = (float)Render::Objects::Slider::getSvAt(obj.time, gameplayState.beatmap.timingPoints);
                    float svc = std::clamp(sv, 0.1f, 10.0f);
                    float bl  = (float)Render::Objects::Slider::getBeatLengthAt(obj.time, gameplayState.beatmap.timingPoints);
                    float slide_dur = (float)(obj.length / ((double)gameplayState.beatmap.sliderMultiplier * 100.0 * svc) * bl);
                    gameplayState.objEndTimes[i] = (float)obj.time + slide_dur * (float)obj.slides + 240.0f;
                } else if (obj.type & 8) {
                    gameplayState.objEndTimes[i] = (float)obj.endTime + 200.0f;
                } else {
                    gameplayState.objEndTimes[i] = (float)obj.time + 240.0f;
                }
            }
            // slider hitsound times (end + each repeat)
            for (int i = 0; i < n; i++) {
                auto& obj = gameplayState.beatmap.hitObjects[i];
                if (!obj.isSlider) continue;
                float sv  = (float)Render::Objects::Slider::getSvAt(obj.time, gameplayState.beatmap.timingPoints);
                float svc = std::clamp(sv, 0.1f, 10.0f);
                float bl  = (float)Render::Objects::Slider::getBeatLengthAt(obj.time, gameplayState.beatmap.timingPoints);
                float slide_dur = (float)(obj.length / ((double)gameplayState.beatmap.sliderMultiplier * 100.0 * svc) * bl);
                for (int k = 1; k <= obj.slides; k++)
                    gameplayState.sliderHsTimes.push_back((float)obj.time + slide_dur * (float)k);
            }
            std::sort(gameplayState.sliderHsTimes.begin(), gameplayState.sliderHsTimes.end());
            // score multiplier
            auto mods_vec = Leeway::modsFromBitmask(gameplayState.replay.mods);
            int drain_sec = 1;
            if (!gameplayState.beatmap.hitObjects.empty()) {
                int s_ms = gameplayState.beatmap.hitObjects.front().time;
                int e_ms = gameplayState.beatmap.hitObjects.back().endTime;
                int drain_ms = e_ms - s_ms;
                for (auto& bp : gameplayState.beatmap.breakPeriods) {
                    int bs = std::max(bp.first, s_ms), be = std::min(bp.second, e_ms);
                    if (be > bs) drain_ms -= (be - bs);
                }
                drain_sec = std::max(1, drain_ms / 1000);
            }
            double scoreMultiplier = Leeway::computeScoreMultiplier({
                gameplayState.beatmap.hpDrainRate,
                gameplayState.beatmap.overallDifficulty,
                gameplayState.beatmap.circleSize,
                n, drain_sec, mods_vec
            });
            // hit detection + score calculation
            Processor::Score::CalculatorInput calcInput {
                gameplayState.beatmap,
                gameplayState.absFrames,
                gameplayState.sliderPaths,
                gameplayState.sliderPathLengths,
                gameplayState.objEndTimes,
                gameplayState.circleRadius,
                gameplayState.replay.mods,
                scoreMultiplier
            };
            gameplayState.scoreResult = Processor::Score::calculate(calcInput, &gameplayState.hitInfos);
            // score comparison printout
            Processor::Score::printComparison(
                gameplayState.scoreResult,
                gameplayState.replay.totalScore,
                gameplayState.replay.maxCombo,
                gameplayState.replay.count300,
                gameplayState.replay.count100,
                gameplayState.replay.count50,
                gameplayState.replay.countMiss);
        } catch (std::exception& e) {
            gameplayState.error = e.what();
        }
    }
    inline void unload() {
        if (gameplayState.musicLoaded) {
            UnloadMusicStream(gameplayState.music);
            gameplayState.musicLoaded = false;
        }
        if (gameplayState.hitsoundLoaded) {
            for (int p = 0; p < State::HITSOUND_POOL_SIZE; p++)
                UnloadSoundAlias(gameplayState.hitsoundPool[p]);
            UnloadSound(gameplayState.hitsound);
            gameplayState.hitsoundLoaded = false;
        }
        for (auto& c : gameplayState.sliderBodyCaches)
            Render::Objects::Slider::unloadCache(c);
        gameplayState.sliderBodyCaches.clear();
        gameplayState.sliderPaths.clear();
        gameplayState.sliderScreenPaths.clear();
        gameplayState.sliderPathLengths.clear();
    }
    inline Vector2 osuToScreen(float ox, float oy, const Render::Objects::Playfield::Playfield& pf) {
        return { pf.x + ox * pf.scale, pf.y + oy * pf.scale };
    }
    inline void draw(Database::BeatmapDb& db) {
        if (!gameplayState.initialized) {
            BeginDrawing();
            ClearBackground(BLACK);
            if (!gameplayState.error.empty())
                Utils::Font::drawText(gameplayState.error.c_str(), 20, 20, 18, RED);
            else
                Utils::Font::drawText("Initializing...", 20, 20, 18, WHITE);
            Render::Utils::Toast::draw();
            EndDrawing();
            return;
        }
        if (gameplayState.musicLoaded)
            UpdateMusicStream(gameplayState.music);
        float current_ms;
        if (gameplayState.musicLoaded)
            current_ms = GetMusicTimePlayed(gameplayState.music) * 1000.0f;
        else
            current_ms = gameplayState.playStartMs + (float)((GetTime() - gameplayState.playStartWall) * 1000.0);
        if (IsKeyPressed(KEY_SPACE) && !gameplayState.skipped && !gameplayState.beatmap.hitObjects.empty()) {
            float first_obj_sec = (float)gameplayState.beatmap.hitObjects[0].time / 1000.0f;
            float skip_to_sec   = first_obj_sec - SKIP_OFFSET_SEC;
            float skip_to_ms    = skip_to_sec * 1000.0f;
            if (skip_to_ms > current_ms) {
                SeekMusicStream(gameplayState.music, skip_to_sec);
                current_ms = skip_to_ms;
                for (int i = 0; i < (int)gameplayState.hitsoundPlayed.size(); i++) {
                    if ((float)gameplayState.beatmap.hitObjects[i].time >= skip_to_ms)
                        gameplayState.hitsoundPlayed[i] = false;
                }
                gameplayState.sliderHsCursor = (int)(std::lower_bound(
                    gameplayState.sliderHsTimes.begin(), gameplayState.sliderHsTimes.end(), skip_to_ms)
                    - gameplayState.sliderHsTimes.begin());
                gameplayState.cursorFrame = 0;
                while (gameplayState.cursorFrame + 1 < (int)gameplayState.absFrames.size() &&
                       gameplayState.absFrames[gameplayState.cursorFrame + 1].timeMs <= (long long)current_ms)
                    gameplayState.cursorFrame++;
            }
            gameplayState.skipped = true;
        }
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        Render::Objects::Playfield::Playfield pf = Render::Objects::Playfield::computePlayfield(sw, sh);
        float r = gameplayState.circleRadius * pf.scale;
        if (!gameplayState.precachePending && pf.scale != gameplayState.cachedPfScale) {
            for (auto& c : gameplayState.sliderBodyCaches)
                Render::Objects::Slider::unloadCache(c);
            for (auto& sp : gameplayState.sliderScreenPaths)
                sp.clear();
            if (precacheSliders) {
                precacheSliderTextures(pf.scale);
            } else {
                for (auto& sp : gameplayState.sliderScreenPaths) sp.clear();
                gameplayState.cachedPfScale = pf.scale;
                if (gameplayState.musicLoaded && !IsMusicStreamPlaying(gameplayState.music)
                        && !gameplayState.musicPrecachePaused) {
                    gameplayState.playStartWall = GetTime();
                    PlayMusicStream(gameplayState.music);
                }
            }
        }
        if (gameplayState.precachePending) {
            tickPrecache(pf, r);
            return;
        }
        auto playHitsound = [&]() {
            Sound& s = gameplayState.hitsoundPool[gameplayState.hitsoundPoolIdx];
            gameplayState.hitsoundPoolIdx = (gameplayState.hitsoundPoolIdx + 1) % State::HITSOUND_POOL_SIZE;
            PlaySound(s);
        };
        // play object hitsounds when current_ms crosses the hit-object time
        if (gameplayState.hitsoundLoaded && current_ms > gameplayState.lastCurrentMs) {
            int hn = (int)gameplayState.beatmap.hitObjects.size();
            for (int i = 0; i < hn; i++) {
                if (gameplayState.hitsoundPlayed[i]) continue;
                float ot = (float)gameplayState.beatmap.hitObjects[i].time;
                if (current_ms >= ot && current_ms < ot + 200.0f) {
                    if (i < (int)gameplayState.hitInfos.size() && gameplayState.hitInfos[i].valid)
                        playHitsound();
                    gameplayState.hitsoundPlayed[i] = true;
                } else if (ot > current_ms + 500.0f) {
                    break;
                }
            }
        }
        gameplayState.lastCurrentMs = current_ms;
        // slider end/repeat hitsounds
        if (gameplayState.hitsoundLoaded) {
            while (gameplayState.sliderHsCursor < (int)gameplayState.sliderHsTimes.size()) {
                float t = gameplayState.sliderHsTimes[gameplayState.sliderHsCursor];
                if (current_ms >= t && current_ms < t + 200.0f) {
                    playHitsound();
                    gameplayState.sliderHsCursor++;
                } else if (t > current_ms) {
                    break;
                } else {
                    gameplayState.sliderHsCursor++;
                }
            }
        }
        float preempt = gameplayState.preempt;
        BeginDrawing();
        ClearBackground(BLACK);
        drawPlayfieldGrid(pf);
        float visible_start = current_ms - 500.0f;
        int n = (int)gameplayState.beatmap.hitObjects.size();
        int lo = 0, hi = n;
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (gameplayState.objEndTimes[mid] < visible_start) lo = mid + 1;
            else hi = mid;
        }
        for (int ii = n - 1; ii >= lo; ii--) {
            const auto& obj = gameplayState.beatmap.hitObjects[ii];
            float time_until = (float)obj.time - current_ms;
            if (time_until > preempt) continue;
            if (current_ms > gameplayState.objEndTimes[ii]) continue;
            if (obj.isSlider) {
                if (!precacheSliders) {
                    if (gameplayState.sliderScreenPaths[ii].empty()) {
                        gameplayState.sliderScreenPaths[ii].resize(gameplayState.sliderPaths[ii].size());
                        for (int j = 0; j < (int)gameplayState.sliderPaths[ii].size(); j++) {
                            gameplayState.sliderScreenPaths[ii][j] = {
                                pf.x + gameplayState.sliderPaths[ii][j].x * pf.scale,
                                pf.y + gameplayState.sliderPaths[ii][j].y * pf.scale
                            };
                        }
                    }
                    if (!gameplayState.sliderBodyCaches[ii].valid && !gameplayState.sliderScreenPaths[ii].empty()) {
                        gameplayState.sliderBodyCaches[ii] = Render::Objects::Slider::renderSliderBody(
                            gameplayState.sliderPaths[ii], gameplayState.circleRadius, pf);
                    }
                }
                Render::Objects::Slider::draw(obj,
                    gameplayState.sliderPaths[ii], gameplayState.sliderScreenPaths[ii],
                    gameplayState.sliderPathLengths[ii],
                    current_ms, pf, gameplayState.circleRadius, preempt,
                    gameplayState.beatmap.sliderMultiplier, gameplayState.beatmap.sliderTickRate,
                    gameplayState.beatmap.timingPoints,
                    gameplayState.sliderBodyCaches[ii]);
            } else if (obj.type & 8) {
                Render::Objects::Spinner::draw(obj, current_ms, pf);
            } else {
                Render::Objects::HitCircle::draw(obj, current_ms, pf, gameplayState.circleRadius, preempt);
            }
        }
        evictOldSliderCaches(current_ms);
        // advance cursor frame
        while (gameplayState.cursorFrame + 1 < (int)gameplayState.absFrames.size() &&
               gameplayState.absFrames[gameplayState.cursorFrame + 1].timeMs <= (long long)current_ms)
            gameplayState.cursorFrame++;
        // draw cursor
        if (!gameplayState.absFrames.empty()) {
            const AbsFrame& cur = gameplayState.absFrames[gameplayState.cursorFrame];
            float cx = cur.x, cy = cur.y;
            if (gameplayState.cursorFrame + 1 < (int)gameplayState.absFrames.size()) {
                const AbsFrame& nxt = gameplayState.absFrames[gameplayState.cursorFrame + 1];
                long long seg = nxt.timeMs - cur.timeMs;
                if (seg > 0) {
                    float t = std::min(1.0f, (current_ms - (float)cur.timeMs) / (float)seg);
                    cx = cur.x + (nxt.x - cur.x) * t;
                    cy = cur.y + (nxt.y - cur.y) * t;
                }
            }
            Vector2 cpos = osuToScreen(cx, cy, pf);
            float cursor_scale = Config::get<float>("player.cursor_size");
            DrawCircleV(cpos, 9.0f * pf.scale * cursor_scale, BLACK);
            DrawCircleV(cpos, 6.7f * pf.scale * cursor_scale, { 255, 255, 255, 255 });
        }
        // FPS stats
        int   fps      = GetFPS();
        float frame_ms = GetFrameTime() * 1000.0f;
        gameplayState.statsWarmup++;
        if (gameplayState.statsWarmup > 60) {
            if (fps > gameplayState.maxFps) gameplayState.maxFps = fps;
            if (fps > 0 && fps < gameplayState.minFps) gameplayState.minFps = fps;
            if (frame_ms > gameplayState.maxFrameMs) gameplayState.maxFrameMs = frame_ms;
            if (frame_ms > 0.0f && frame_ms < gameplayState.minFrameMs) gameplayState.minFrameMs = frame_ms;
        }
        const float hud_size = pf.scale * 11.0f;
        const int   pad      = std::max(4, (int)(4.4f * pf.scale));
        // top-left: song info
        Utils::Font::drawText((gameplayState.beatmap.artist + " - " + gameplayState.beatmap.title).c_str(),
            pad, pad, hud_size, WHITE);
        Utils::Font::drawText(("mapped by: " + gameplayState.beatmap.creator).c_str(),
            pad, pad + (int)(hud_size + 4), hud_size, LIGHTGRAY);
        Utils::Font::drawText(("played by: " + gameplayState.replay.playerName).c_str(),
            pad, pad + (int)((hud_size + 4) * 2), hud_size, LIGHTGRAY);
        // bottom-right: frame stats
        char stats_buf[128];
        snprintf(stats_buf, sizeof(stats_buf), "%.3fms (%.3f~%.3f)  %d fps (%d~%d)",
            frame_ms, gameplayState.minFrameMs, gameplayState.maxFrameMs,
            fps, gameplayState.minFps == INT_MAX ? 0 : gameplayState.minFps, gameplayState.maxFps);
        Vector2 stats_dim = MeasureTextEx(Render::Utils::Font::activeFont, stats_buf,
            hud_size * Config::get<float>("hud.fps_text_size"),
            hud_size * Config::get<float>("hud.fps_text_size") / 10.0f);
        Utils::Font::drawText(stats_buf,
            sw - (int)stats_dim.x - pad, sh - (int)stats_dim.y - pad,
            hud_size * Config::get<float>("hud.fps_text_size"), LIGHTGRAY);
        // score panel (top-right)
        {
            auto& events = gameplayState.scoreResult.events;
            int ne = (int)events.size();
            int lo2 = 0, hi2 = ne;
            while (lo2 < hi2) {
                int mid = (lo2 + hi2) / 2;
                if (events[mid].timeMs <= current_ms) lo2 = mid + 1;
                else hi2 = mid;
            }
            int ev_idx = lo2 - 1;
            auto& sr = gameplayState.scoreResult;
            long long display_score = ev_idx >= 0 ? sr.cumScore[ev_idx]    : 0;
            int display_combo       = ev_idx >= 0 ? sr.cumCombo[ev_idx]    : 0;
            int display_n300        = ev_idx >= 0 ? sr.cumN300[ev_idx]     : 0;
            int display_n100        = ev_idx >= 0 ? sr.cumN100[ev_idx]     : 0;
            int display_n50         = ev_idx >= 0 ? sr.cumN50[ev_idx]      : 0;
            int display_nmiss       = ev_idx >= 0 ? sr.cumNmiss[ev_idx]    : 0;
            int total_hits = display_n300 + display_n100 + display_n50 + display_nmiss;
            double acc = total_hits > 0
                ? (300.0 * display_n300 + 100.0 * display_n100 + 50.0 * display_n50)
                  / (300.0 * total_hits) * 100.0
                : 100.0;
            const float score_size = hud_size * 1.8f;
            const float small_size = hud_size * 0.95f;
            const float line_gap   = small_size * 0.3f;
            char score_str[32]; snprintf(score_str, sizeof(score_str), "%09lld", display_score);
            Vector2 score_dim = MeasureTextEx(Render::Utils::Font::activeFont, score_str, score_size, score_size / 10.0f);
            Utils::Font::drawText(score_str, sw - (int)score_dim.x - pad, pad, score_size, WHITE);
            char acc_str[32]; snprintf(acc_str, sizeof(acc_str), "%.2f%%", acc);
            Vector2 acc_dim = MeasureTextEx(Render::Utils::Font::activeFont, acc_str, small_size, small_size / 10.0f);
            int acc_y = pad + (int)score_dim.y + (int)line_gap;
            Utils::Font::drawText(acc_str, sw - (int)acc_dim.x - pad, acc_y, small_size, LIGHTGRAY);
            char hits_str[64];
            snprintf(hits_str, sizeof(hits_str), "300:%d  100:%d  50:%d  X:%d",
                display_n300, display_n100, display_n50, display_nmiss);
            Vector2 hits_dim = MeasureTextEx(Render::Utils::Font::activeFont, hits_str, small_size, small_size / 10.0f);
            int hits_y = acc_y + (int)acc_dim.y + (int)line_gap;
            Utils::Font::drawText(hits_str, sw - (int)hits_dim.x - pad, hits_y, small_size, LIGHTGRAY);
            char combo_str[32]; snprintf(combo_str, sizeof(combo_str), "%dx", display_combo);
            Vector2 combo_dim = MeasureTextEx(Render::Utils::Font::activeFont, combo_str, small_size, small_size / 10.0f);
            int combo_y = hits_y + (int)hits_dim.y + (int)line_gap;
            Utils::Font::drawText(combo_str, sw - (int)combo_dim.x - pad, combo_y, small_size, YELLOW);
        }
        Render::Utils::Toast::draw();
        // score debug overlay (Tab)
        if (IsKeyPressed(KEY_TAB)) gameplayState.showScoreDebug = !gameplayState.showScoreDebug;
        if (gameplayState.showScoreDebug) {
            auto& sr = gameplayState.scoreResult;
            int ne2 = (int)sr.events.size();
            long long calc_score = ne2 > 0 ? sr.cumScore[ne2-1]    : 0;
            int  calc_combo      = ne2 > 0 ? sr.cumMaxCombo[ne2-1] : 0;
            int  calc_n300       = ne2 > 0 ? sr.cumN300[ne2-1]     : 0;
            int  calc_n100       = ne2 > 0 ? sr.cumN100[ne2-1]     : 0;
            int  calc_n50        = ne2 > 0 ? sr.cumN50[ne2-1]      : 0;
            int  calc_nmiss      = ne2 > 0 ? sr.cumNmiss[ne2-1]    : 0;
            long long rep_score = gameplayState.replay.totalScore;
            int  rep_combo      = gameplayState.replay.maxCombo;
            int  rep_n300       = gameplayState.replay.count300;
            int  rep_n100       = gameplayState.replay.count100;
            int  rep_n50        = gameplayState.replay.count50;
            int  rep_nmiss      = gameplayState.replay.countMiss;
            int bx = pad, by = pad + (int)((hud_size + 4) * 4);
            float fs = hud_size * 0.9f;
            float lh = fs + 3.0f;
            DrawRectangle(bx - 4, by - 4, 420, (int)(lh * 8 + 12), { 0, 0, 0, 180 });
            Utils::Font::drawText("[TAB] Score Debug: calc vs replay", bx, by, fs, { 200, 200, 50, 255 });
            by += (int)lh;
            Utils::Font::drawText("           CALC           REPLAY     DIFF", bx, by, fs, LIGHTGRAY);
            by += (int)lh;
            auto row = [&](const char* label, long long calc, long long rep) {
                Color col = (calc == rep) ? GREEN : RED;
                char buf[128];
                snprintf(buf, sizeof(buf), "%-8s  %12lld   %12lld   %+lld", label, calc, rep, calc - rep);
                Utils::Font::drawText(buf, bx, by, fs, col);
                by += (int)lh;
            };
            row("score",  calc_score, rep_score);
            row("combo",  calc_combo, rep_combo);
            row("300",    calc_n300,  rep_n300);
            row("100",    calc_n100,  rep_n100);
            row("50",     calc_n50,   rep_n50);
            row("miss",   calc_nmiss, rep_nmiss);
        }
        // key overlay
        {
            int cur_keys = gameplayState.absFrames.empty() ? 0
                         : gameplayState.absFrames[gameplayState.cursorFrame].keys;
            Render::Objects::Keyoverlay::update(gameplayState.key_overlay, cur_keys);
            Render::Objects::Keyoverlay::draw(gameplayState.key_overlay, cur_keys, sw, sh, pf.scale * 1.5, pad);
        }
        // F11 debug overlay
        {
            Render::Debug::ReplayInfo ri;
            ri.currentMs        = current_ms;
            ri.cursorFrame      = gameplayState.cursorFrame;
            ri.totalFrames      = (int)gameplayState.absFrames.size();
            ri.hitObjects       = (int)gameplayState.beatmap.hitObjects.size();
            ri.preempt          = preempt;
            ri.circleRadius     = gameplayState.circleRadius;
            ri.precacheSliders  = precacheSliders;
            ri.musicLoaded      = gameplayState.musicLoaded;
            ri.musicDurationMs  = gameplayState.musicLoaded
                ? GetMusicTimeLength(gameplayState.music) * 1000.0f : 0.0f;
            int slider_total = 0, slider_cached = 0;
            long vram_est = 0;
            for (int i = 0; i < (int)gameplayState.beatmap.hitObjects.size(); i++) {
                if (!gameplayState.beatmap.hitObjects[i].isSlider) continue;
                slider_total++;
                if (i < (int)gameplayState.sliderBodyCaches.size() && gameplayState.sliderBodyCaches[i].valid) {
                    slider_cached++;
                    auto& tex = gameplayState.sliderBodyCaches[i].tex;
                    vram_est += (long)tex.width * tex.height * 4;
                }
            }
            ri.sliderCount       = slider_total;
            ri.cachedSliderCount = slider_cached;
            ri.cachedVramEstBytes = vram_est;
            Render::Debug::draw(&ri);
        }
        EndDrawing();
    }
}