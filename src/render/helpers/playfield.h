#pragma once
inline Playfield computePlayfield(int screenW, int screenH) {
    Playfield pf;
    pf.scale = fminf((float)screenW / 640.0f, (float)screenH / 480.0f);
    pf.width = PLAYFIELD_W * pf.scale;
    pf.height = PLAYFIELD_H * pf.scale;
    pf.x = (screenW - 640.0f * pf.scale) / 2.0f + 64.0f * pf.scale;
    pf.y = (screenH - 480.0f * pf.scale) / 2.0f + 56.0f * pf.scale;
    return pf;
}