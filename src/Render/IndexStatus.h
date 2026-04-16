#pragma once
#include "../Processor/BeatmapHash.h"
#include "Font.h"

namespace Orr::Render::IndexStatus {
    static void DrawBuildStatus(const Orr::Processor::BeatmapHash::BuildIndexStatus& s) {
        BeginDrawing();
        ClearBackground(BLACK);

        int x = 10;
        int y = 10;

        DrawText("Building Index", x, y, 28.0f, WHITE);
        y += 55;

        char buf[128];

        snprintf(buf, sizeof(buf), "Scanning Songs Folder %d files %.2fS...", s.scan_count, s.scan_seconds);
        DrawText(buf, x, y, 20.0f, s.scan_done ? GREEN : SKYBLUE);
        y += 35;

        if (s.scan_done) {
            if (s.process_total == 0) {
                DrawText("Beatmaps are all up to date", x, y, 20.0f, GREEN);
                y += 35;
            } else {
                snprintf(buf, sizeof(buf), "Process Maps %d / %d %.2fS", s.process_current, s.process_total, s.process_seconds);
                DrawText(buf, x, y, 20.0f, s.process_done ? GREEN : SKYBLUE);
                y += 28;

                float pct = (float)s.process_current / (float)s.process_total;
                DrawRectangle(x, y, 400, 8, DARKGRAY);
                DrawRectangle(x, y, (int)(400.0f * pct), 8, GREEN);
                y += 25;
            }

            if (s.process_done) {
                snprintf(buf, sizeof(buf), "Database %s %.2fS",
                    s.insert_done ? "done" : "inserting...", s.insert_seconds);
                DrawText(buf, x, y, 20.0f, s.insert_done ? GREEN : SKYBLUE);
            }
        }

        EndDrawing();
    }
}