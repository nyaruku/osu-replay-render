#pragma once
#include <raylib.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstring>
#include "../../Reader/Beatmap.h"
#include "../Playfield.h"

namespace Orr::Render::Objects::Slider {

    namespace path {

        inline float Dist(Vector2 a, Vector2 b) {
            float dx = b.x - a.x, dy = b.y - a.y;
            return sqrtf(dx*dx + dy*dy);
        }

        inline Vector2 Lerp2(Vector2 a, Vector2 b, float t) {
            return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
        }

        inline Vector2 BezierAt(std::vector<Vector2> pts, float t) {
            for (int r = 1; r < (int)pts.size(); r++) {
                for (int i = 0; i < (int)pts.size() - r; i++) {
                    pts[i] = Lerp2(pts[i], pts[i + 1], t);
                }
            }
            return pts[0];
        }

        inline std::vector<Vector2> SampleBezierSegment(const std::vector<Vector2>& ctrl) {
            int n = std::max(10, (int)ctrl.size() * 5);
            std::vector<Vector2> pts;
            pts.reserve(n + 1);
            for (int i = 0; i <= n; i++) {
                pts.push_back(BezierAt(ctrl, (float)i / n));
            }
            return pts;
        }

        inline std::vector<Vector2> ComputeCircularArc(Vector2 a, Vector2 b, Vector2 c) {
            float d = 2.0f * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
            if (fabsf(d) < 1e-6f) {
                return { a, b, c };
            }
            float aSq = a.x*a.x + a.y*a.y, bSq = b.x*b.x + b.y*b.y, cSq = c.x*c.x + c.y*c.y;
            Vector2 center = {
                (aSq*(b.y - c.y) + bSq*(c.y - a.y) + cSq*(a.y - b.y)) / d,
                (aSq*(c.x - b.x) + bSq*(a.x - c.x) + cSq*(b.x - a.x)) / d
            };
            float radius = Dist(center, a);
            float theta_start = atan2f(a.y - center.y, a.x - center.x);
            float theta_end   = atan2f(c.y - center.y, c.x - center.x);
            while (theta_end < theta_start) {
                theta_end += 2.0f * (float)M_PI;
            }
            float range = theta_end - theta_start;
            Vector2 ortho = { c.y - a.y, -(c.x - a.x) };
            int dir = (ortho.x*(b.x - a.x) + ortho.y*(b.y - a.y)) < 0.0f ? -1 : 1;
            if (dir == -1) {
                range = 2.0f * (float)M_PI - range;
            }
            int n = std::min(512, std::max(4, (int)ceilf(range * radius / 4.0f)));
            std::vector<Vector2> pts;
            pts.reserve(n + 1);
            for (int i = 0; i <= n; i++) {
                float theta = theta_start + dir * range * ((float)i / n);
                pts.push_back({ center.x + radius * cosf(theta), center.y + radius * sinf(theta) });
            }
            return pts;
        }

        inline std::vector<Vector2> Flatten(const Orr::Reader::Beatmap::HitObject& obj) {
            std::vector<Vector2> ctrl;
            ctrl.push_back({ (float)obj.x, (float)obj.y });
            for (auto& cp : obj.curve_points) {
                ctrl.push_back({ (float)cp.x, (float)cp.y });
            }
            if (ctrl.size() <= 1) {
                return ctrl;
            }
            if (obj.curve_type == 'L') {
                return ctrl;
            }
            if (obj.curve_type == 'P' && ctrl.size() == 3) {
                return ComputeCircularArc(ctrl[0], ctrl[1], ctrl[2]);
            }
            std::vector<Vector2> result;
            std::vector<Vector2> seg;
            seg.push_back(ctrl[0]);
            for (int i = 1; i < (int)ctrl.size(); i++) {
                seg.push_back(ctrl[i]);
                bool red_anchor = (i + 1 < (int)ctrl.size() &&
                                   ctrl[i].x == ctrl[i+1].x && ctrl[i].y == ctrl[i+1].y);
                bool last = (i == (int)ctrl.size() - 1);
                if (red_anchor || last) {
                    auto sampled = SampleBezierSegment(seg);
                    if (!result.empty() && !sampled.empty()) {
                        sampled.erase(sampled.begin());
                    }
                    result.insert(result.end(), sampled.begin(), sampled.end());
                    seg.clear();
                    if (red_anchor) {
                        seg.push_back(ctrl[i]);
                        i++;
                    }
                }
            }
            return result;
        }

        inline std::vector<Vector2> TruncateToLength(const std::vector<Vector2>& pts, float target) {
            if (pts.empty()) {
                return pts;
            }
            std::vector<Vector2> result;
            result.push_back(pts[0]);
            float acc = 0.0f;
            for (int i = 1; i < (int)pts.size(); i++) {
                float seg_len = Dist(pts[i-1], pts[i]);
                if (acc + seg_len >= target) {
                    float t = seg_len > 0.0f ? (target - acc) / seg_len : 0.0f;
                    result.push_back(Lerp2(pts[i-1], pts[i], t));
                    return result;
                }
                acc += seg_len;
                result.push_back(pts[i]);
            }
            return result;
        }

        inline Vector2 SampleAt(const std::vector<Vector2>& pts, float distance) {
            if (pts.size() == 1) {
                return pts[0];
            }
            float acc = 0.0f;
            for (int i = 1; i < (int)pts.size(); i++) {
                float seg = Dist(pts[i-1], pts[i]);
                if (acc + seg >= distance) {
                    float t = seg > 0.0f ? (distance - acc) / seg : 0.0f;
                    return Lerp2(pts[i-1], pts[i], t);
                }
                acc += seg;
            }
            return pts.back();
        }

        inline float TotalLength(const std::vector<Vector2>& pts) {
            float len = 0.0f;
            for (int i = 1; i < (int)pts.size(); i++) {
                len += Dist(pts[i-1], pts[i]);
            }
            return len;
        }
    }

    inline float GetBeatLengthAt(int time, const std::vector<Orr::Reader::Beatmap::TimingPoint>& tps) {
        float bl = 500.0f;
        for (auto& tp : tps) {
            if (tp.time <= time && tp.uninherited) {
                bl = (float)tp.beat_length;
            }
        }
        return bl;
    }

    inline float GetSVAt(int time, const std::vector<Orr::Reader::Beatmap::TimingPoint>& tps) {
        float sv = 1.0f;
        for (auto& tp : tps) {
            if (tp.time <= time && !tp.uninherited) {
                sv = std::min(10.0f, std::max(0.1f, -100.0f / (float)tp.beat_length));
            }
        }
        return sv;
    }

    inline std::vector<Vector2> ComputeSliderPath(const Orr::Reader::Beatmap::HitObject& obj) {
        auto raw = path::Flatten(obj);
        return path::TruncateToLength(raw, (float)obj.length);
    }

    constexpr int GRAD_SIZE = 128;

    struct GradientColor {
        unsigned char r, g, b, a;
    };

    inline GradientColor g_gradient[GRAD_SIZE];
    inline bool g_gradientBuilt = false;

    inline void BuildGradient() {
        if (g_gradientBuilt) {
            return;
        }
        Color shadow = { 0, 0, 0, 255 };
        Color border = { 255, 255, 255, 255 };
        Color outer  = { 25, 25, 25, 255 };
        Color inner  = { 55, 55, 55, 255 };
        float aa = 3.0f / 256.0f;
        struct Stop { float p; Color c; };
        Stop stops[] = {
            { 0.0f,              { 0, 0, 0, 0 } },
            { 0.078125f - aa,    shadow },
            { 0.078125f + aa,    border },
            { 0.1875f - aa,      border },
            { 0.1875f + aa,      outer },
            { 1.0f,              inner },
        };
        int n = 6;
        for (int i = 0; i < GRAD_SIZE; i++) {
            float t = (float)i / (float)(GRAD_SIZE - 1);
            GradientColor c = { 0, 0, 0, 0 };
            for (int s = 0; s < n - 1; s++) {
                if (t >= stops[s].p && t <= stops[s + 1].p) {
                    float range = stops[s + 1].p - stops[s].p;
                    float f = (range > 0.0f) ? (t - stops[s].p) / range : 0.0f;
                    c.r = (unsigned char)((float)stops[s].c.r + ((float)stops[s + 1].c.r - (float)stops[s].c.r) * f);
                    c.g = (unsigned char)((float)stops[s].c.g + ((float)stops[s + 1].c.g - (float)stops[s].c.g) * f);
                    c.b = (unsigned char)((float)stops[s].c.b + ((float)stops[s + 1].c.b - (float)stops[s].c.b) * f);
                    c.a = (unsigned char)((float)stops[s].c.a + ((float)stops[s + 1].c.a - (float)stops[s].c.a) * f);
                    break;
                }
            }
            if (t > stops[n - 1].p) {
                c = { inner.r, inner.g, inner.b, inner.a };
            }
            g_gradient[i] = c;
        }
        g_gradientBuilt = true;
    }

    inline GradientColor SampleGradient(float t) {
        t = std::max(0.0f, std::min(1.0f, t));
        int idx = (int)(t * (GRAD_SIZE - 1));
        idx = std::max(0, std::min(GRAD_SIZE - 1, idx));
        return g_gradient[idx];
    }

    inline float PointSegDist(float px, float py, float ax, float ay, float bx, float by) {
        float dx = bx - ax, dy = by - ay;
        float lenSq = dx * dx + dy * dy;
        if (lenSq < 1e-8f) {
            float ex = px - ax, ey = py - ay;
            return sqrtf(ex * ex + ey * ey);
        }
        float t = ((px - ax) * dx + (py - ay) * dy) / lenSq;
        t = std::max(0.0f, std::min(1.0f, t));
        float cx = ax + t * dx - px;
        float cy = ay + t * dy - py;
        return sqrtf(cx * cx + cy * cy);
    }

    struct SliderBodyCache {
        Texture2D tex = {};
        float x = 0, y = 0;
        int w = 0, h = 0;
        bool valid = false;
    };

    inline SliderBodyCache RenderSliderBody(const std::vector<Vector2>& screenPath, float radius) {
        SliderBodyCache cache;
        if (screenPath.size() < 2) {
            return cache;
        }
        BuildGradient();
        float minX = screenPath[0].x, maxX = screenPath[0].x;
        float minY = screenPath[0].y, maxY = screenPath[0].y;
        for (auto& p : screenPath) {
            minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        }
        float pad = radius + 2.0f;
        minX -= pad; minY -= pad; maxX += pad; maxY += pad;
        float rawW = maxX - minX, rawH = maxY - minY;
        if (rawW < 1.0f || rawH < 1.0f) {
            return cache;
        }
        constexpr int MAX_DIM = 4096;
        constexpr int MAX_PIXELS = 4 * 1024 * 1024;
        float scale = 1.0f;
        if (rawW > MAX_DIM || rawH > MAX_DIM) {
            scale = std::min((float)MAX_DIM / rawW, (float)MAX_DIM / rawH);
        }
        if (rawW * scale * rawH * scale > MAX_PIXELS) {
            scale = sqrtf((float)MAX_PIXELS / (rawW * rawH));
        }
        scale = std::max(scale, 0.01f);
        int w = (int)ceilf(rawW * scale);
        int h = (int)ceilf(rawH * scale);
        if (w < 1 || h < 1) {
            return cache;
        }
        float sRadius = radius * scale;
        float sRadiusSq = sRadius * sRadius;
        int iRadius = (int)ceilf(sRadius);
        // Downsample path to reduce segment count
        float minSpacing = 1.0f / scale;
        if (screenPath.size() > 2000) {
            minSpacing = std::max(minSpacing, path::TotalLength(screenPath) / 2000.0f);
        }
        std::vector<Vector2> dp;
        dp.push_back(screenPath[0]);
        float spacingSq = minSpacing * minSpacing;
        for (int i = 1; i < (int)screenPath.size() - 1; i++) {
            float dx = screenPath[i].x - dp.back().x;
            float dy = screenPath[i].y - dp.back().y;
            if (dx * dx + dy * dy >= spacingSq) {
                dp.push_back(screenPath[i]);
            }
        }
        dp.push_back(screenPath.back());
        std::vector<float> distBuf(w * h, sRadius + 1.0f);
        for (int s = 1; s < (int)dp.size(); s++) {
            float ax = (dp[s-1].x - minX) * scale;
            float ay = (dp[s-1].y - minY) * scale;
            float bx = (dp[s].x - minX) * scale;
            float by = (dp[s].y - minY) * scale;
            int x0 = std::max(0, (int)(std::min(ax, bx) - iRadius));
            int x1 = std::min(w - 1, (int)(std::max(ax, bx) + iRadius));
            int y0 = std::max(0, (int)(std::min(ay, by) - iRadius));
            int y1 = std::min(h - 1, (int)(std::max(ay, by) + iRadius));
            float sdx = bx - ax, sdy = by - ay;
            float segLenSq = sdx * sdx + sdy * sdy;
            for (int py = y0; py <= y1; py++) {
                float fy = (float)py + 0.5f;
                for (int px = x0; px <= x1; px++) {
                    float fx = (float)px + 0.5f;
                    float t;
                    if (segLenSq < 1e-8f) {
                        t = 0.0f;
                    } else {
                        t = ((fx - ax) * sdx + (fy - ay) * sdy) / segLenSq;
                        if (t < 0.0f) {
                            t = 0.0f;
                        } else if (t > 1.0f) {
                            t = 1.0f;
                        }
                    }
                    float cx = ax + t * sdx - fx;
                    float cy = ay + t * sdy - fy;
                    float dSq = cx * cx + cy * cy;
                    if (dSq >= sRadiusSq) {
                        continue;
                    }
                    float d = sqrtf(dSq);
                    int idx = py * w + px;
                    if (d < distBuf[idx]) {
                        distBuf[idx] = d;
                    }
                }
            }
        }
        std::vector<unsigned char> pixels(w * h * 4, 0);
        for (int i = 0; i < w * h; i++) {
            if (distBuf[i] >= sRadius) {
                continue;
            }
            float t = 1.0f - distBuf[i] / sRadius;
            GradientColor c = SampleGradient(t);
            pixels[i * 4 + 0] = c.r;
            pixels[i * 4 + 1] = c.g;
            pixels[i * 4 + 2] = c.b;
            pixels[i * 4 + 3] = c.a;
        }
        Image img = { pixels.data(), w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
        cache.tex = LoadTextureFromImage(img);
        cache.x = minX;
        cache.y = minY;
        cache.w = (int)ceilf(rawW);
        cache.h = (int)ceilf(rawH);
        cache.valid = true;
        return cache;
    }

    inline bool IsOversized(const std::vector<Vector2>& screenPath, float radius) {
        if (screenPath.size() < 2) {
            return false;
        }
        float minX = screenPath[0].x, maxX = screenPath[0].x;
        float minY = screenPath[0].y, maxY = screenPath[0].y;
        for (auto& p : screenPath) {
            minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        }
        float pad = radius + 2.0f;
        float rawW = (maxX - minX) + 2.0f * pad;
        float rawH = (maxY - minY) + 2.0f * pad;
        constexpr int MAX_PIXELS = 4 * 1024 * 1024;
        return (rawW * rawH) > (float)MAX_PIXELS;
    }

    inline void UnloadCache(SliderBodyCache& cache) {
        if (cache.valid) {
            UnloadTexture(cache.tex);
            cache.valid = false;
        }
    }

    inline void DrawCachedBody(const SliderBodyCache& cache, unsigned char alpha) {
        if (!cache.valid) {
            return;
        }
        Rectangle src = { 0, 0, (float)cache.tex.width, (float)cache.tex.height };
        Rectangle dst = { cache.x, cache.y, (float)cache.w, (float)cache.h };
        DrawTexturePro(cache.tex, src, dst, { 0, 0 }, 0.0f, { 255, 255, 255, alpha });
    }

    inline void DrawSliderBodyDirect(const std::vector<Vector2>& screenPath, float radius, unsigned char alpha) {
        if (screenPath.size() < 2) {
            return;
        }
        BuildGradient();
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();
        float viewMinX = 0.0f, viewMinY = 0.0f;
        float viewMaxX = (float)sw, viewMaxY = (float)sh;
        float pad = radius + 2.0f;
        float clipMinX = viewMinX - pad;
        float clipMinY = viewMinY - pad;
        float clipMaxX = viewMaxX + pad;
        float clipMaxY = viewMaxY + pad;
        std::vector<int> visibleSegs;
        for (int s = 1; s < (int)screenPath.size(); s++) {
            float ax = screenPath[s-1].x, ay = screenPath[s-1].y;
            float bx = screenPath[s].x, by = screenPath[s].y;
            float segMinX = std::min(ax, bx) - pad;
            float segMinY = std::min(ay, by) - pad;
            float segMaxX = std::max(ax, bx) + pad;
            float segMaxY = std::max(ay, by) + pad;
            if (segMaxX >= viewMinX && segMinX <= viewMaxX && segMaxY >= viewMinY && segMinY <= viewMaxY) {
                visibleSegs.push_back(s);
            }
        }
        if (visibleSegs.empty()) {
            return;
        }
        float minX = clipMaxX, maxX = clipMinX;
        float minY = clipMaxY, maxY = clipMinY;
        for (int s : visibleSegs) {
            float ax = screenPath[s-1].x, ay = screenPath[s-1].y;
            float bx = screenPath[s].x, by = screenPath[s].y;
            minX = std::min(minX, std::min(ax, bx) - pad);
            minY = std::min(minY, std::min(ay, by) - pad);
            maxX = std::max(maxX, std::max(ax, bx) + pad);
            maxY = std::max(maxY, std::max(ay, by) + pad);
        }
        minX = std::max(minX, viewMinX);
        minY = std::max(minY, viewMinY);
        maxX = std::min(maxX, viewMaxX);
        maxY = std::min(maxY, viewMaxY);
        int w = (int)ceilf(maxX - minX);
        int h = (int)ceilf(maxY - minY);
        if (w < 1 || h < 1) {
            return;
        }
        int iRadius = (int)ceilf(radius);
        float radiusSq = radius * radius;
        std::vector<float> distBuf(w * h, radius + 1.0f);
        for (int s : visibleSegs) {
            float ax = screenPath[s-1].x - minX;
            float ay = screenPath[s-1].y - minY;
            float bx = screenPath[s].x - minX;
            float by = screenPath[s].y - minY;
            int x0 = std::max(0, (int)(std::min(ax, bx) - iRadius));
            int x1 = std::min(w - 1, (int)(std::max(ax, bx) + iRadius));
            int y0 = std::max(0, (int)(std::min(ay, by) - iRadius));
            int y1 = std::min(h - 1, (int)(std::max(ay, by) + iRadius));
            float sdx = bx - ax, sdy = by - ay;
            float segLenSq = sdx * sdx + sdy * sdy;
            for (int py = y0; py <= y1; py++) {
                float fy = (float)py + 0.5f;
                for (int px = x0; px <= x1; px++) {
                    float fx = (float)px + 0.5f;
                    float t;
                    if (segLenSq < 1e-8f) {
                        t = 0.0f;
                    } else {
                        t = ((fx - ax) * sdx + (fy - ay) * sdy) / segLenSq;
                        if (t < 0.0f) {
                            t = 0.0f;
                        } else if (t > 1.0f) {
                            t = 1.0f;
                        }
                    }
                    float cx = ax + t * sdx - fx;
                    float cy = ay + t * sdy - fy;
                    float dSq = cx * cx + cy * cy;
                    if (dSq >= radiusSq) {
                        continue;
                    }
                    float d = sqrtf(dSq);
                    int idx = py * w + px;
                    if (d < distBuf[idx]) {
                        distBuf[idx] = d;
                    }
                }
            }
        }
        std::vector<unsigned char> pixels(w * h * 4, 0);
        for (int i = 0; i < w * h; i++) {
            if (distBuf[i] >= radius) {
                continue;
            }
            float t = 1.0f - distBuf[i] / radius;
            GradientColor c = SampleGradient(t);
            pixels[i * 4 + 0] = c.r;
            pixels[i * 4 + 1] = c.g;
            pixels[i * 4 + 2] = c.b;
            pixels[i * 4 + 3] = (unsigned char)((float)c.a * ((float)alpha / 255.0f));
        }
        Image img = { pixels.data(), w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
        Texture2D tex = LoadTextureFromImage(img);
        DrawTexture(tex, (int)minX, (int)minY, WHITE);
        UnloadTexture(tex);
    }

    inline void Draw(const Orr::Reader::Beatmap::HitObject& obj,
                     const std::vector<Vector2>& slider_path,
                     const std::vector<Vector2>& screen_path,
                     float path_len,
                     float current_ms,
                     const Orr::Render::Playfield::Playfield& pf,
                     float base_radius, float preempt,
                     float slider_mult, float tick_rate,
                     const std::vector<Orr::Reader::Beatmap::TimingPoint>& timing_points,
                     const SliderBodyCache& body_cache) {
        float time_until = (float)obj.time - current_ms;
        constexpr float FADE_OUT_MS = 150.0f;
        if (time_until > preempt) {
            return;
        }
        float sv = GetSVAt(obj.time, timing_points);
        float beat_len = GetBeatLengthAt(obj.time, timing_points);
        float slide_dur = ((float)obj.length / (slider_mult * 100.0f * sv)) * beat_len;
        float end_time = (float)obj.time + slide_dur * (float)obj.slides;
        if (current_ms > end_time + FADE_OUT_MS) {
            return;
        }
        float fade_alpha;
        const float fade_in_dur = preempt * (2.0f / 3.0f);
        if (time_until > preempt / 3.0f) {
            fade_alpha = 1.0f - ((time_until - preempt / 3.0f) / fade_in_dur);
        } else {
            fade_alpha = 1.0f;
        }
        if (current_ms > end_time) {
            fade_alpha = 1.0f - std::min(1.0f, (current_ms - end_time) / FADE_OUT_MS);
        }
        fade_alpha = std::max(0.0f, fade_alpha);
        unsigned char alpha = (unsigned char)(fade_alpha * 255.0f);
        float r = base_radius * pf.scale;
        auto to_screen = [&](Vector2 p) -> Vector2 {
            return { pf.x + p.x * pf.scale, pf.y + p.y * pf.scale };
        };
        if (IsOversized(screen_path, r)) {
            DrawSliderBodyDirect(screen_path, r, alpha);
        } else {
            DrawCachedBody(body_cache, alpha);
        }
        if (screen_path.size() >= 2) {
            DrawCircleV(screen_path.back(), r, { 30, 30, 30, alpha });
            DrawCircleLinesV(screen_path.back(), r, { 255, 255, 255, alpha });
        }
        float tick_spacing = slider_mult * 100.0f * sv / tick_rate;
        if (tick_spacing > 0.0f) {
            for (int s = 0; s < obj.slides; s++) {
                float slide_start_ms = (float)obj.time + (float)s * slide_dur;
                for (float d = tick_spacing; d < path_len - 1.0f; d += tick_spacing) {
                    if (slide_start_ms + (d / path_len) * slide_dur < current_ms) {
                        continue;
                    }
                    float actual_d = (s % 2 == 0) ? d : path_len - d;
                    DrawCircleV(to_screen(path::SampleAt(slider_path, actual_d)), r * 0.22f, { 255, 255, 255, alpha });
                }
            }
        }
        Vector2 head_sc = screen_path.empty() ? to_screen({ (float)obj.x, (float)obj.y }) : screen_path.front();
        if (time_until >= 0.0f) {
            float approach_scale = 1.0f + 3.0f * std::max(0.0f, time_until / preempt);
            DrawCircleLines((int)head_sc.x, (int)head_sc.y, r * approach_scale, { 255, 255, 255, alpha });
            DrawCircleV(head_sc, r, { 70, 130, 180, alpha });
            DrawCircleLinesV(head_sc, r, { 255, 255, 255, alpha });
        }
        if (current_ms >= (float)obj.time && current_ms <= end_time) {
            float elapsed = current_ms - (float)obj.time;
            float slide_progress = (slide_dur > 0.0f) ? fmodf(elapsed, slide_dur) / slide_dur : 0.0f;
            bool reversed = ((int)(elapsed / slide_dur)) % 2 == 1;
            float ball_d = reversed ? (1.0f - slide_progress) * path_len : slide_progress * path_len;
            Vector2 ball_sc = to_screen(path::SampleAt(slider_path, ball_d));
            DrawCircleV(ball_sc, r, { 255, 200, 50, 220 });
            DrawCircleLinesV(ball_sc, r, { 255, 255, 255, 220 });
            DrawCircleLines((int)ball_sc.x, (int)ball_sc.y, r * 2.4f, { 255, 255, 255, 80 });
        }
    }
}
