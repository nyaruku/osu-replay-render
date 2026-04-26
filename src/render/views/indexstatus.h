#pragma once
#include <algorithm>
#include <cmath>
#include <config/constants.h>
#include <processor/beatmaphash.h>
#include <render/utils/font.h>
#include <raygui.h>

namespace Render::IndexStatus {
    static void drawBuildStatus(const Processor::BeatmapHash::BuildIndexStatus& s) {
        BeginDrawing();
        ClearBackground(Config::Constants::Theme::BG);

        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        constexpr float TEXT_SIZE = 20.0f;
        constexpr int BAR_W = 480;
        constexpr int BAR_H = 14;

        char buf[256];
        float pct = -1.0f; // negative = indefinite

        if (!s.error.empty()) {
            snprintf(buf, sizeof(buf), "Error: %s", s.error.c_str());
            Vector2 tsz = MeasureTextEx(Render::Utils::Font::activeFont, buf, TEXT_SIZE, 1.0f);
            int tx = (sw - (int)tsz.x) / 2;
            int ty = sh / 2 - (int)tsz.y - 6;
            Render::Utils::Font::drawText(buf, tx, ty, (int)TEXT_SIZE, RED);

            constexpr float HINT_SIZE = 16.0f;
            const char* hint = "Press Escape to open Settings";
            Vector2 hsz = MeasureTextEx(Render::Utils::Font::activeFont, hint, HINT_SIZE, 1.0f);
            Render::Utils::Font::drawText(hint, (sw - (int)hsz.x) / 2, sh / 2 + 6, (int)HINT_SIZE, Config::Constants::Theme::TEXT_SECONDARY);

            EndDrawing();
            return;
        }

        if (!s.scanDone) {
            snprintf(buf, sizeof(buf), "Scanning songs folder... %d files", s.scanCount);
        } else if (s.processTotal == 0 || s.processDone) {
            if (!s.insertDone) {
                snprintf(buf, sizeof(buf), "Updating database...");
            } else if (s.processTotal == 0) {
                snprintf(buf, sizeof(buf), "All beatmaps are up to date");
                pct = 1.0f;
            } else {
                snprintf(buf, sizeof(buf), "Done!");
                pct = 1.0f;
            }
        } else {
            int pct_i = (int)((float)s.processCurrent / (float)s.processTotal * 100.0f);
            snprintf(buf, sizeof(buf), "Checking beatmaps... %d/%d (%d%%)", s.processCurrent, s.processTotal, pct_i);
            pct = (float)s.processCurrent / (float)s.processTotal;
        }

        Vector2 tsz = MeasureTextEx(Render::Utils::Font::activeFont, buf, TEXT_SIZE, 1.0f);
        int bar_x  = (sw - BAR_W) / 2;
        int bar_y  = sh / 2;
        int text_x = (sw - (int)tsz.x) / 2;
        int text_y = bar_y - (int)tsz.y - 10;

        Render::Utils::Font::drawText(buf, text_x, text_y, (int)TEXT_SIZE, Config::Constants::Theme::TEXT_PRIMARY);
        DrawRectangle(bar_x, bar_y, BAR_W, BAR_H, Config::Constants::Theme::INPUT_BG);

        if (pct < 0.0f) {
            // animated sliding block
            constexpr int BLOCK_W = BAR_W / 4;
            float cycle = fmodf((float)GetTime() * 0.6f, 1.0f);
            int block_x = bar_x + (int)((BAR_W + BLOCK_W) * cycle) - BLOCK_W;
            int draw_x   = std::max(bar_x, block_x);
            int draw_end = std::min(bar_x + BAR_W, block_x + BLOCK_W);
            if (draw_end > draw_x)
                DrawRectangle(draw_x, bar_y, draw_end - draw_x, BAR_H, Config::Constants::Theme::BORDER_ACTIVE);
        } else {
            DrawRectangle(bar_x, bar_y, (int)((float)BAR_W * pct), BAR_H, Config::Constants::Theme::BORDER_ACTIVE);
        }

        EndDrawing();
    }
}