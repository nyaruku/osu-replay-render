#pragma once

// Process up to `chunk` sliders per frame. Returns true when done.
inline bool tickPrecache(const Render::Objects::Playfield::Playfield& pf, float r, int chunk = 8) {
    if (!gameplayState.precachePending) return true;

    if (gameplayState.precacheTargetScale == 0.0f)
        gameplayState.precacheTargetScale = pf.scale;

    // window resized while precaching: abort and restart for the new scale
    if (gameplayState.precacheTargetScale != 0.0f && pf.scale != gameplayState.precacheTargetScale) {
        for (auto& c : gameplayState.sliderBodyCaches)
            Render::Objects::Slider::unloadCache(c);
        for (auto& sp : gameplayState.sliderScreenPaths)
            sp.clear();
        gameplayState.precacheTargetScale = pf.scale;
        gameplayState.precacheCursor       = 0;
        gameplayState.precacheSlidersDone = 0;
    }

    float nowMs = 0.0f;
    if (gameplayState.musicLoaded)
        nowMs = GetMusicTimePlayed(gameplayState.music) * 1000.0f;

    int n = (int)gameplayState.beatmap.hitObjects.size();
    int done = 0;
    while (gameplayState.precacheCursor < n && done < chunk) {
        int i = gameplayState.precacheCursor++;
        auto& obj = gameplayState.beatmap.hitObjects[i];
        if (!obj.isSlider || gameplayState.sliderPaths[i].empty()) continue;
        if (gameplayState.objEndTimes[i] > 0.0f && gameplayState.objEndTimes[i] < nowMs) continue;

        gameplayState.sliderScreenPaths[i].resize(gameplayState.sliderPaths[i].size());
        for (int j = 0; j < (int)gameplayState.sliderPaths[i].size(); j++) {
            gameplayState.sliderScreenPaths[i][j] = {
                pf.x + gameplayState.sliderPaths[i][j].x * pf.scale,
                pf.y + gameplayState.sliderPaths[i][j].y * pf.scale
            };
        }
        float osu_radius = r / pf.scale;  // convert screen radius back to osu! units
        gameplayState.sliderBodyCaches[i] = Render::Objects::Slider::renderSliderBody(
            gameplayState.sliderPaths[i], osu_radius, pf);
        gameplayState.precacheSlidersDone++;
        done++;
    }

    bool finished = (gameplayState.precacheCursor >= n);
    if (finished) {
        gameplayState.precachePending = false;
        gameplayState.cachedPfScale = pf.scale;
        if (gameplayState.musicLoaded) {
            if (gameplayState.musicPrecachePaused) {
                ResumeMusicStream(gameplayState.music);
                gameplayState.musicPrecachePaused = false;
            } else {
                // first-time start: reset wall clock so timer is accurate
                gameplayState.playStartWall = GetTime();
                PlayMusicStream(gameplayState.music);
            }
        }
    }

    long vramBytes = 0;
    for (auto& c : gameplayState.sliderBodyCaches) {
        if (c.valid)
            vramBytes += (long)c.tex.width * c.tex.height * 1; // grayscale = 1 byte per pixel
    }

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    int total = std::max(1, gameplayState.precacheSliderTotal);
    int prog  = gameplayState.precacheSlidersDone;

    BeginDrawing();
    ClearBackground(Config::Constants::Theme::BG);

    char buf[128];
    snprintf(buf, sizeof(buf), "Precaching slider textures... %d/%d (%.0f%%)",
        prog, total, (float)prog / (float)total * 100.0f);

    char vramBuf[64];
    if (vramBytes >= 1024 * 1024)
        snprintf(vramBuf, sizeof(vramBuf), "VRAM: %.1f MB", (float)vramBytes / (1024.f * 1024.f));
    else
        snprintf(vramBuf, sizeof(vramBuf), "VRAM: %.0f KB", (float)vramBytes / 1024.f);

    constexpr float TEXT_SIZE  = 20.0f;
    constexpr float VRAM_SIZE  = 16.0f;
    constexpr int   BAR_W      = 480;
    constexpr int   BAR_H      = 14;

    Vector2 tsz  = MeasureTextEx(Render::Utils::Font::activeFont, buf, TEXT_SIZE, 1.0f);
    Vector2 vsz  = MeasureTextEx(Render::Utils::Font::activeFont, vramBuf, VRAM_SIZE, 1.0f);
    int barX    = (sw - BAR_W) / 2;
    int barY    = sh / 2;
    int textY   = barY - (int)tsz.y - 10;
    int textX   = (sw - (int)tsz.x) / 2;
    int vramY   = barY + BAR_H + 10;
    int vramX   = (sw - (int)vsz.x) / 2;

    Render::Utils::Font::drawText(buf, textX, textY, (int)TEXT_SIZE, Config::Constants::Theme::TEXT_PRIMARY);
    DrawRectangle(barX, barY, BAR_W, BAR_H, Config::Constants::Theme::INPUT_BG);
    int fillW = (int)((float)BAR_W * (float)prog / (float)total);
    DrawRectangle(barX, barY, fillW, BAR_H, Config::Constants::Theme::BORDER_ACTIVE);
    Render::Utils::Font::drawText(vramBuf, vramX, vramY, (int)VRAM_SIZE, Config::Constants::Theme::TEXT_SECONDARY);
    EndDrawing();

    return finished;
}

inline void precacheSliderTextures(float target_scale = 0.0f) {
    if (!gameplayState.initialized || !precacheSliders) return;
    if (gameplayState.musicLoaded && IsMusicStreamPlaying(gameplayState.music)) {
        PauseMusicStream(gameplayState.music);
        gameplayState.musicPrecachePaused = true;
    }
    float now_ms = gameplayState.musicLoaded
        ? GetMusicTimePlayed(gameplayState.music) * 1000.0f : 0.0f;
    gameplayState.precacheSliderTotal = 0;
    for (int i = 0; i < (int)gameplayState.beatmap.hitObjects.size(); i++) {
        auto& obj = gameplayState.beatmap.hitObjects[i];
        if (!obj.isSlider) continue;
        // Only count sliders that haven't finished yet
        if (gameplayState.objEndTimes[i] > 0.0f && gameplayState.objEndTimes[i] < now_ms) continue;
        gameplayState.precacheSliderTotal++;
    }
    gameplayState.precacheTargetScale = target_scale;
    gameplayState.precacheCursor       = 0;
    gameplayState.precacheSlidersDone = 0;
    gameplayState.precachePending      = true;
}

// Evict texture caches for sliders that ended more than evict_after_ms ago.
// Only triggers when at least batch_size expired textures have accumulated.
inline void evictOldSliderCaches(float current_ms, float evict_after_ms = 1000.0f, int batch_size = 200) {
    int n = (int)gameplayState.sliderBodyCaches.size();
    int eligible = 0;
    for (int i = 0; i < n; i++) {
        if (!gameplayState.sliderBodyCaches[i].valid) continue;
        if (gameplayState.objEndTimes[i] > 0.0f &&
            current_ms - gameplayState.objEndTimes[i] > evict_after_ms)
            eligible++;
    }
    if (eligible < batch_size) return;
    for (int i = 0; i < n; i++) {
        if (!gameplayState.sliderBodyCaches[i].valid) continue;
        if (gameplayState.objEndTimes[i] > 0.0f &&
            current_ms - gameplayState.objEndTimes[i] > evict_after_ms) {
            Render::Objects::Slider::unloadCache(gameplayState.sliderBodyCaches[i]);
            gameplayState.sliderBodyCaches[i].valid = false;
            gameplayState.sliderScreenPaths[i].clear();
        }
    }
}
