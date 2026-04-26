#pragma once
#include <config/constants.h>
#include <render/utils/font.h>
#include <raylib.h>
#include <string>
#include <vector>
#include <algorithm>

namespace Render::Utils::Toast {

    enum class Level { Info, Success, Warning, Error };

    struct Toast {
        std::string message;
        Level       level;
        double      createdAt;
        float       duration; // seconds
    };

    inline std::vector<Toast> toastArray;

    inline void push(const std::string& message, Level level = Level::Info, float duration = 3.0f) {
        toastArray.push_back({message, level, GetTime(), duration});
    }

    inline Color levelColor(Level level) {
        switch (level) {
            case Level::Success: return SKYBLUE;
            case Level::Warning: return YELLOW;
            case Level::Error:   return RED;
            default:             return Config::Constants::Theme::TEXT_ACCENT;
        }
    }

    inline void draw() {
        double now = GetTime();

        // remove expired
        toastArray.erase(
            std::remove_if(
                toastArray.begin(), toastArray.end(), [&](const Toast& t) {
                    return now - t.createdAt >= t.duration;
                }
            ),
            toastArray.end()
        );

        if (toastArray.empty())
            return;

        int sw = GetScreenWidth();
        float pad    = 12.0f;
        float h      = 40.0f;
        float w      = 340.0f;
        float bottom = (float)GetScreenHeight() - pad;
        float fade_time = 0.4f; // seconds to fade in/out

        for (int i = (int)toastArray.size() - 1; i >= 0; i--) {
            auto& t = toastArray[i];
            double age = now - t.createdAt;
            float remaining = t.duration - (float)age;

            float alpha = 1.0f;
            if (age < fade_time) alpha = (float)(age / fade_time);
            else if (remaining < fade_time) alpha = remaining / fade_time;
            alpha = std::clamp(alpha, 0.0f, 1.0f);

            int slot = (int)toastArray.size() - 1 - i;
            float y = bottom - h - slot * (h + 6.0f);
            float x = sw - w - pad;

            // background
            Color bg = Config::Constants::Theme::BG_PANEL;
            bg.a = (unsigned char)(220 * alpha);
            DrawRectangleRounded({x, y, w, h}, 0.15f, 6, bg);

            // left accent bar
            Color accent = levelColor(t.level);
            accent.a = (unsigned char)(255 * alpha);
            DrawRectangle((int)x, (int)y, 4, (int)h, accent);

            // border
            Color border = Config::Constants::Theme::BORDER;
            border.a = (unsigned char)(180 * alpha);
            DrawRectangleRoundedLinesEx({x, y, w, h}, 0.15f, 6, 1.0f, border);

            // text
            Color tc = Config::Constants::Theme::TEXT_PRIMARY;
            tc.a = (unsigned char)(255 * alpha);
            Render::Utils::Font::drawText(
                t.message.c_str(),
                (int)(x + 14), (int)(y + h / 2.0f - Config::Constants::FontSize::SMALL / 2.0f),
                Config::Constants::FontSize::SMALL, tc
            );
        }
    }
}