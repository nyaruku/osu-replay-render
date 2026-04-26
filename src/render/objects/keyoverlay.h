#pragma once
#include <raylib.h>
#include <reader/replay.h>
#include <render/utils/font.h>

namespace Render {
    namespace Objects {
        namespace Keyoverlay {
            struct State {
                int countK1 = 0;
                int countK2 = 0;
                int countM1 = 0;
                int countM2 = 0;
                int prevEffKeys = 0;
            };

            // Resolve effective key bits: M1/M2 are hidden when K1/K2 are active
            // (matches osu! stable InputOverlay.cs logic exactly)
            inline int effectiveKeys(int rawKeys) {
                constexpr int BIT_K1 = (int) Reader::Replay::Keys::K1;
                constexpr int BIT_K2 = (int) Reader::Replay::Keys::K2;
                constexpr int BIT_M1 = (int) Reader::Replay::Keys::M1;
                constexpr int BIT_M2 = (int) Reader::Replay::Keys::M2;
                int eff = rawKeys;
                if (rawKeys & BIT_K1) eff &= ~BIT_M1;
                if (rawKeys & BIT_K2) eff &= ~BIT_M2;
                return eff;
            }

            inline void update(State &s, int rawKeys) {
                constexpr int BIT_K1 = (int) Reader::Replay::Keys::K1;
                constexpr int BIT_K2 = (int) Reader::Replay::Keys::K2;
                constexpr int BIT_M1 = (int) Reader::Replay::Keys::M1;
                constexpr int BIT_M2 = (int) Reader::Replay::Keys::M2;
                int eff = effectiveKeys(rawKeys);
                int newly = eff & ~s.prevEffKeys;
                if (newly & BIT_K1) s.countK1++;
                if (newly & BIT_K2) s.countK2++;
                if (newly & BIT_M1) s.countM1++;
                if (newly & BIT_M2) s.countM2++;
                s.prevEffKeys = eff;
            }

            // rawKeys: current frame's key bitmask; sw/sh: screen dimensions; pfScale: playfield scale
            inline void draw(const State &s, int rawKeys, int sw, int sh, float pfScale = 2.25f, int pad = 10) {
                constexpr int BIT_K1 = (int) Reader::Replay::Keys::K1;
                constexpr int BIT_K2 = (int) Reader::Replay::Keys::K2;
                constexpr int BIT_M1 = (int) Reader::Replay::Keys::M1;
                constexpr int BIT_M2 = (int) Reader::Replay::Keys::M2;

                int eff = effectiveKeys(rawKeys);

                // sizes normalized to pfScale=3.0 (1440p = 60px key)
                const int keyW = (int) (20.0f * pfScale);
                const int keyH = keyW;
                const int keyPad = std::max(1, (int) (2.0f * pfScale));
                const int labelSize = std::max(6, (int) (4.6f * pfScale));
                const int countSize = std::max(6, (int) (4.3f * pfScale));
                const int totalH = 4 * keyH + 3 * keyPad;
                const int x = sw - keyW - pad;
                const int yStart = (sh - totalH) / 2;

                struct KeyInfo {
                    const char *label;
                    int bit;
                    int count;
                    bool keyboard;
                };
                const KeyInfo keys[4] = {
                    {"K1", BIT_K1, s.countK1, true},
                    {"K2", BIT_K2, s.countK2, true},
                    {"M1", BIT_M1, s.countM1, false},
                    {"M2", BIT_M2, s.countM2, false},
                };

                for (int ki = 0; ki < 4; ki++) {
                    int ky = yStart + ki * (keyH + keyPad);
                    bool pressed = (eff & keys[ki].bit) != 0;

                    Color bg = pressed ? Color{255, 255, 255, 225} : Color{0, 0, 0, 255};
                    Color border = pressed ? Color{255, 255, 255, 255} : Color{30, 30, 30, 255};
                    Color fg = pressed ? Color{0, 0, 0, 255} : Color{255, 255, 255, 255};

                    DrawRectangle(x, ky, keyW, keyH, bg);
                    DrawRectangleLines(x, ky, keyW, keyH, border);

                    // Label centred in top half
                    Vector2 lsz = MeasureTextEx(Render::Utils::Font::activeFont, keys[ki].label, (float) labelSize, 1.0f);
                    Render::Utils::Font::drawText(keys[ki].label,
                                                  x + (keyW - (int) lsz.x) / 2,
                                                  ky + keyH / 2 - (int) lsz.y - 1,
                                                  labelSize, fg);

                    // Press count centred in bottom half
                    char cnt[16];
                    snprintf(cnt, sizeof(cnt), "%d", keys[ki].count);
                    Vector2 csz = MeasureTextEx(Render::Utils::Font::activeFont, cnt, (float) countSize, 1.0f);
                    Render::Utils::Font::drawText(
                        cnt,
                        x + (keyW - (int) csz.x) / 2,
                        ky + keyH / 2 + 2,
                        countSize, fg
                    );
                }
            }
        }
    }
}
