#pragma once
#include <raylib.h>
#include <string>
#include <vector>
#include <filesystem>
#include <cmath>
#include "../Reader/Beatmap.h"
#include "../Reader/Replay.h"
#include "../Database/Database.h"
#include "Font.h"
#include "Playfield.h"
#include "Objects/HitCircle.h"
#include "Objects/Spinner.h"
#include "Objects/Slider.h"

namespace Orr::Render::ReplayRender {


    inline float ComputePreempt(float ar) {
        if (ar < 5.0f)
            return 1200.0f + 120.0f * (5.0f - ar);
        if (ar > 5.0f)
            return 1200.0f - 150.0f * (ar - 5.0f);
        return 1200.0f;
    }

    inline float ComputePreemptWithMods(float ar, int mods) {
        const int MOD_EZ = 2;
        const int MOD_HR = 16;
        const int MOD_DT = 64;
        const int MOD_NC = 512;
        const int MOD_HT = 256;

        if (mods & MOD_EZ)
            ar /= 2.0f;
        if (mods & MOD_HR)
            ar = std::min(ar * 1.4f, 10.0f);

        float preempt = ComputePreempt(ar);

        if (mods & MOD_DT || mods & MOD_NC)
            preempt /= 1.5f;
        else if (mods & MOD_HT)
            preempt /= 0.75f;

        return preempt;
    }

    inline float ComputeCircleRadius(float cs, int mods) {
        const int MOD_EZ = 2;
        const int MOD_HR = 16;

        if (mods & MOD_EZ)
            cs /= 2.0f;
        if (mods & MOD_HR)
            cs = std::min(cs * 1.3f, 10.0f);

        return (54.4f - 4.48f * cs) * 1.00041f;
    }

    struct AbsFrame {
        long long time_ms;
        float x;
        float y;
        int keys;
    };

    constexpr float MUSIC_VOLUME = 0.2f;
    constexpr float SKIP_OFFSET_SEC = 2.0f;

    struct State {
        Orr::Reader::Beatmap::Beatmap beatmap;
        Orr::Reader::Replay::Replay replay;
        std::vector<AbsFrame> abs_frames;
        Music music;
        bool music_loaded = false;
        bool initialized = false;
        bool skipped = false;
        std::string error;
        float preempt;
        float circle_radius;
        int cursor_frame = 0;
        double play_start_wall = 0.0;
        float play_start_ms = 0.0f;
        int max_fps = 0;
        std::vector<std::vector<Vector2>> slider_paths;
        std::vector<std::vector<Vector2>> slider_screen_paths;
        std::vector<float> slider_path_lengths;
        std::vector<Orr::Render::Objects::Slider::SliderBodyCache> slider_body_caches;
        float cached_pf_scale = 0.0f;
    };

    inline State gState;

    inline void Init(const std::string& osr_path, Orr::Database::BeatmapDb& db) {
        gState = State{};
        try {
            gState.replay = Orr::Reader::Replay::ReadOsr(osr_path);
            auto entry = Orr::Database::FindByMd5(db, gState.replay.beatmap_md5);
            if (!entry) {
                gState.error = "Beatmap not found in DB for MD5: " + gState.replay.beatmap_md5;
                return;
            }
            gState.beatmap = Orr::Reader::Beatmap::ReadOsu(entry->file_path);
            std::filesystem::path beatmap_dir = std::filesystem::path(entry->file_path).parent_path();
            std::string audio_path = (beatmap_dir / gState.beatmap.audio_filename).string();
            gState.music = LoadMusicStream(audio_path.c_str());
            if (gState.music.frameCount > 0) {
                gState.music_loaded = true;
                PlayMusicStream(gState.music);
                SetMusicVolume(gState.music, MUSIC_VOLUME);
                float lead_in_sec = gState.beatmap.audio_lead_in > 0 ? (float)gState.beatmap.audio_lead_in / 1000.0f : 0.0f;
                gState.play_start_ms = -lead_in_sec * 1000.0f;
                gState.play_start_wall = GetTime();
            }
            gState.preempt = ComputePreemptWithMods(gState.beatmap.approach_rate, gState.replay.mods);
            gState.circle_radius = ComputeCircleRadius(gState.beatmap.circle_size, gState.replay.mods);
            long long abs_time = 0;
            for (auto& f : gState.replay.frames) {
                abs_time += f.time_delta;
                gState.abs_frames.push_back({ abs_time, f.x, f.y, f.keys });
            }
            gState.initialized = true;
            gState.slider_paths.resize(gState.beatmap.hit_objects.size());
            gState.slider_screen_paths.resize(gState.beatmap.hit_objects.size());
            gState.slider_path_lengths.resize(gState.beatmap.hit_objects.size(), 0.0f);
            gState.slider_body_caches.resize(gState.beatmap.hit_objects.size());
            for (int i = 0; i < (int)gState.beatmap.hit_objects.size(); i++) {
                if (gState.beatmap.hit_objects[i].is_slider) {
                    gState.slider_paths[i] = Orr::Render::Objects::Slider::ComputeSliderPath(gState.beatmap.hit_objects[i]);
                    gState.slider_path_lengths[i] = Orr::Render::Objects::Slider::path::TotalLength(gState.slider_paths[i]);
                }
            }
        } catch (std::exception& e) {
            gState.error = e.what();
        }
    }

    inline void Unload() {
        if (gState.music_loaded) {
            UnloadMusicStream(gState.music);
            gState.music_loaded = false;
        }
        for (auto& c : gState.slider_body_caches) {
            Orr::Render::Objects::Slider::UnloadCache(c);
        }
        gState.slider_body_caches.clear();
    }

    inline Vector2 OsuToScreen(float ox, float oy, const Orr::Render::Playfield::Playfield& pf) {
        return { pf.x + ox * pf.scale, pf.y + oy * pf.scale };
    }


    inline void Draw(Orr::Database::BeatmapDb& db) {
        if (!gState.initialized) {
            BeginDrawing();
            ClearBackground(BLACK);
            if (!gState.error.empty()) {
                DrawText(gState.error.c_str(), 20, 20, 18, RED);
            } else {
                DrawText("Initializing...", 20, 20, 18, WHITE);
            }
            EndDrawing();
            return;
        }

        if (gState.music_loaded) {
            UpdateMusicStream(gState.music);
        }

        float current_ms = gState.play_start_ms + (float)((GetTime() - gState.play_start_wall) * 1000.0);

        if (IsKeyPressed(KEY_SPACE) && !gState.skipped && !gState.beatmap.hit_objects.empty()) {
            float first_obj_sec = (float)gState.beatmap.hit_objects[0].time / 1000.0f;
            float skip_to_sec = first_obj_sec - SKIP_OFFSET_SEC;
            float skip_to_ms = skip_to_sec * 1000.0f;
            if (skip_to_ms > current_ms) {
                SeekMusicStream(gState.music, skip_to_sec);
                gState.play_start_ms = skip_to_ms;
                gState.play_start_wall = GetTime();
                current_ms = skip_to_ms;
                gState.cursor_frame = 0;
                while (gState.cursor_frame + 1 < (int)gState.abs_frames.size() &&
                       gState.abs_frames[gState.cursor_frame + 1].time_ms <= (long long)current_ms) {
                    gState.cursor_frame++;
                }
            }
            gState.skipped = true;
        }

        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        Orr::Render::Playfield::Playfield pf = Orr::Render::Playfield::ComputePlayfield(sw, sh);

        float preempt = gState.preempt;

        BeginDrawing();
        ClearBackground(BLACK);
        DrawPlayfieldGrid(pf);

        float r = gState.circle_radius * pf.scale;
        if (pf.scale != gState.cached_pf_scale) {
            for (int i = 0; i < (int)gState.beatmap.hit_objects.size(); i++) {
                Orr::Render::Objects::Slider::UnloadCache(gState.slider_body_caches[i]);
                if (gState.beatmap.hit_objects[i].is_slider && !gState.slider_paths[i].empty()) {
                    gState.slider_screen_paths[i].resize(gState.slider_paths[i].size());
                    for (int j = 0; j < (int)gState.slider_paths[i].size(); j++) {
                        gState.slider_screen_paths[i][j] = {
                            pf.x + gState.slider_paths[i][j].x * pf.scale,
                            pf.y + gState.slider_paths[i][j].y * pf.scale
                        };
                    }
                    gState.slider_body_caches[i] = Orr::Render::Objects::Slider::RenderSliderBody(
                        gState.slider_screen_paths[i], r);
                }
            }
            gState.cached_pf_scale = pf.scale;
        }

        for (int i = (int)gState.beatmap.hit_objects.size() - 1; i >= 0; i--) {
            const auto& obj = gState.beatmap.hit_objects[i];
            float time_until = (float)obj.time - current_ms;
            bool is_spinner = (obj.type & 8) != 0;

            if (obj.is_slider) {
                float sv = Orr::Render::Objects::Slider::GetSVAt(obj.time, gState.beatmap.timing_points);
                float beat_len = Orr::Render::Objects::Slider::GetBeatLengthAt(obj.time, gState.beatmap.timing_points);
                float slide_dur = ((float)obj.length / (gState.beatmap.slider_multiplier * 100.0f * sv)) * beat_len;
                float end_time = (float)obj.time + slide_dur * (float)obj.slides;
                if (time_until > preempt || current_ms > end_time + 150.0f) {
                    continue;
                }
                Orr::Render::Objects::Slider::Draw(obj,
                    gState.slider_paths[i], gState.slider_screen_paths[i],
                    gState.slider_path_lengths[i],
                    current_ms, pf, gState.circle_radius, preempt,
                    gState.beatmap.slider_multiplier, gState.beatmap.slider_tick_rate,
                    gState.beatmap.timing_points,
                    gState.slider_body_caches[i]);
            } else if (is_spinner) {
                float visible_until = (float)obj.end_time + 200.0f;
                if (time_until > preempt || current_ms > visible_until) continue;
                Orr::Render::Objects::Spinner::Draw(obj, current_ms, pf);
            } else {
                if (time_until > preempt || current_ms > (float)obj.time + 150.0f) continue;
                Orr::Render::Objects::HitCircle::Draw(obj, current_ms, pf, gState.circle_radius, preempt);
            }
        }

        while (gState.cursor_frame + 1 < (int)gState.abs_frames.size() &&
               gState.abs_frames[gState.cursor_frame + 1].time_ms <= (long long)current_ms) {
            gState.cursor_frame++;
        }

        if (!gState.abs_frames.empty()) {
            const AbsFrame& cur = gState.abs_frames[gState.cursor_frame];
            float cx = cur.x;
            float cy = cur.y;

            if (gState.cursor_frame + 1 < (int)gState.abs_frames.size()) {
                const AbsFrame& nxt = gState.abs_frames[gState.cursor_frame + 1];
                long long seg = nxt.time_ms - cur.time_ms;
                if (seg > 0) {
                    float t = std::min(1.0f, (current_ms - (float)cur.time_ms) / (float)seg);
                    cx = cur.x + (nxt.x - cur.x) * t;
                    cy = cur.y + (nxt.y - cur.y) * t;
                }
            }

            Vector2 cpos = OsuToScreen(cx, cy, pf);
            DrawCircleV(cpos, 8.0f, { 255, 100, 100, 220 });
            DrawCircleLinesV(cpos, 8.0f, WHITE);
        }

        int fps = GetFPS();
        if (fps > gState.max_fps)
            gState.max_fps = fps;
        float frame_ms = GetFrameTime() * 1000.0f;

        const float hud_size = 16.0f;
        const int pad = 10;

        // top left: song info
        std::string title_line = gState.beatmap.artist + " - " + gState.beatmap.title;
        std::string mapper_line = "mapped by: " + gState.beatmap.creator;
        std::string player_line = "played by: " + gState.replay.player_name;
        DrawText(title_line.c_str(), pad, pad, hud_size, WHITE);
        DrawText(mapper_line.c_str(), pad, pad + (int)(hud_size + 4), hud_size, LIGHTGRAY);
        DrawText(player_line.c_str(), pad, pad + (int)((hud_size + 4) * 2), hud_size, LIGHTGRAY);

        // bottom right: frame time | fps / max fps
        char stats_buf[64];
        snprintf(stats_buf, sizeof(stats_buf), "%.3fms  %d fps / %d max", frame_ms, fps, gState.max_fps);
        Vector2 stats_dim = MeasureTextEx(ActiveFont, stats_buf, hud_size, hud_size / 10.0f);
        DrawText(stats_buf, sw - (int)stats_dim.x - pad, sh - (int)stats_dim.y - pad, hud_size, LIGHTGRAY);

        EndDrawing();
    }
}