#pragma once
#include <raylib.h>
#include <config/config.h>
#include <config/constants.h>
#include <render/utils/font.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdarg>

namespace Render::Debug {

    inline bool overlayVisible = false;

    inline void toggle() { overlayVisible = !overlayVisible; }

    // Call inside BeginDrawing/EndDrawing, after the rest of the frame has been drawn.
    // Pass optional replay state info via pointers (may be nullptr when not in replay).
    struct ReplayInfo {
        float currentMs;
        int   cursorFrame;
        int   totalFrames;
        int   hitObjects;
        int   sliderCount;           // total sliders
        int   cachedSliderCount;    // how many have a valid texture cache
        long  cachedVramEstBytes;  // estimated VRAM: sum of w*h*4 for each cached texture
        float preempt;
        float circleRadius;
        bool  precacheSliders;
        bool  musicLoaded;
        float musicDurationMs;
    };

    inline void draw(const ReplayInfo* ri = nullptr) {
        if (!overlayVisible) return;

        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        float panel_w = 440.0f;
        float pad     = 14.0f;
        float lh      = 20.0f;
        float fs      = 15.0f;

        std::vector<std::pair<std::string, Color>> lines;

        auto add = [&](const std::string& s, Color c = Config::Constants::Theme::TEXT_PRIMARY) {
            lines.push_back({s, c});
        };
        auto addf = [&](const char* fmt, ...) {
            char buf[256];
            va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
            lines.push_back({buf, Config::Constants::Theme::TEXT_SECONDARY});
        };

        // --- Performance ---
        add("[ Performance ]", Config::Constants::Theme::TEXT_ACCENT);
        int fps = GetFPS();
        float frame_ms = GetFrameTime() * 1000.0f;
        addf("FPS:            %d", fps);
        addf("Frame time:     %.3f ms", frame_ms);
        addf("Screen:         %d x %d", sw, sh);
        add("");

        // --- Config ---
        add("[ Config ]", Config::Constants::Theme::TEXT_ACCENT);
        addf("songs_dir:      %s", Config::get<std::string>("general.songs_dir").c_str());
        addf("replay_dir:     %s", Config::get<std::string>("general.replay_dir").c_str());
        addf("slider precache:%s", Config::get<bool>("objects.slider.texture_pre_calculation") ? "true" : "false");
        add("");

        // --- Replay ---
        if (ri) {
            add("[ Replay ]", Config::Constants::Theme::TEXT_ACCENT);
            addf("Time:           %.0f / %.0f ms", ri->currentMs, ri->musicDurationMs);
            addf("Cursor frame:   %d / %d", ri->cursorFrame, ri->totalFrames);
            addf("Hit objects:    %d", ri->hitObjects);
            addf("Circle radius:  %.2f", ri->circleRadius);
            addf("Music:          %s", ri->musicLoaded ? "loaded" : "no audio");
            add("");

            add("[ Slider Cache ]", Config::Constants::Theme::TEXT_ACCENT);
            addf("Sliders total:  %d", ri->sliderCount);
            addf("Cached:         %d / %d", ri->cachedSliderCount, ri->sliderCount);
            if (ri->sliderCount > 0) {
                float pct = 100.0f * ri->cachedSliderCount / ri->sliderCount;
                addf("Coverage:       %.1f%%", pct);
            }
            // estimated VRAM in MB
            float vram_mb = (float)ri->cachedVramEstBytes / (1024.0f * 1024.0f);
            addf("Est. tex VRAM:  %.2f MB", vram_mb);
            addf("Precache mode:  %s", ri->precacheSliders ? "pre-built" : "on-demand");
            add("");
        }

        float panel_h = pad * 2.0f + (float)lines.size() * lh + lh;
        float px = sw - panel_w - 10.0f;
        float py = 10.0f;

        DrawRectangle((int)px, (int)py, (int)panel_w, (int)panel_h, {10, 10, 20, 210});
        DrawRectangleLinesEx({px, py, panel_w, panel_h}, 1.0f, Config::Constants::Theme::BORDER);

        Render::Utils::Font::drawText("F11  DEBUG", (int)(px + pad), (int)(py + 4),
            Config::Constants::FontSize::SMALL, Config::Constants::Theme::TEXT_ACCENT);
        DrawLine((int)px, (int)(py + lh + 6), (int)(px + panel_w), (int)(py + lh + 6),
                 Config::Constants::Theme::BORDER_DIVIDER);

        float ly = py + lh + 10.0f;
        for (auto& [text, col] : lines) {
            Render::Utils::Font::drawText(text.c_str(), (int)(px + pad), (int)ly, (int)fs, col);
            ly += lh;
        }
    }
}