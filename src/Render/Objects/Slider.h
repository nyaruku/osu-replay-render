#pragma once
#include <raylib.h>
#include <rlgl.h>
#include <vector>
#include <cmath>
#include <algorithm>
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

    // Matches stable: MAXRES=24 for cap resolution
    constexpr int MAXRES = 24;
    // Stable: slight quad overlap to avoid 1px holes between quad and wedge
    constexpr float QUAD_OVERLAP_FUDGE = 3.0e-4f;
    // Stable: shift center vertex slightly to avoid crack on horizontal linear sliders
    constexpr float QUAD_MIDDLECRACK_FUDGE = 1.0e-4f;

    struct SliderBodyCache {
        RenderTexture2D rt = {};
        float x = 0, y = 0;
        int w = 0, h = 0;
        bool valid = false;
    };

    inline void EmitVertex(float x, float y, float z, Color c) {
        rlColor4ub(c.r, c.g, c.b, c.a);
        rlVertex3f(x, y, z);
    }

    // Matches stable's glDrawQuad: quad split at center line.
    // Edge vertices at z=0 (near, wins depth test), center at z=-1 (far, loses to edges).
    inline void DrawSegmentQuad(Vector2 p1, Vector2 p2, float theta, float radius,
                                Color edgeCol, Color bodyCol) {
        float cosT = cosf(theta);
        float sinT = sinf(theta);
        float perpX = -sinT * radius;
        float perpY = cosT * radius;
        float fudgeX = cosT * QUAD_OVERLAP_FUDGE;
        float fudgeY = sinT * QUAD_OVERLAP_FUDGE;
        float crackX = perpX * QUAD_MIDDLECRACK_FUDGE;
        float crackY = perpY * QUAD_MIDDLECRACK_FUDGE;
        float p1lx = p1.x + perpX - fudgeX;
        float p1ly = p1.y + perpY - fudgeY;
        float p2lx = p2.x + perpX + fudgeX;
        float p2ly = p2.y + perpY + fudgeY;
        float p1cx = p1.x + crackX - fudgeX;
        float p1cy = p1.y + crackY - fudgeY;
        float p2cx = p2.x + crackX + fudgeX;
        float p2cy = p2.y + crackY + fudgeY;
        float p1rx = p1.x - perpX - fudgeX;
        float p1ry = p1.y - perpY - fudgeY;
        float p2rx = p2.x - perpX + fudgeX;
        float p2ry = p2.y - perpY + fudgeY;
        EmitVertex(p1lx, p1ly, 0.0f, edgeCol);
        EmitVertex(p2lx, p2ly, 0.0f, edgeCol);
        EmitVertex(p2cx, p2cy, -1.0f, bodyCol);
        EmitVertex(p1lx, p1ly, 0.0f, edgeCol);
        EmitVertex(p2cx, p2cy, -1.0f, bodyCol);
        EmitVertex(p1cx, p1cy, -1.0f, bodyCol);
        EmitVertex(p1rx, p1ry, 0.0f, edgeCol);
        EmitVertex(p1cx, p1cy, -1.0f, bodyCol);
        EmitVertex(p2cx, p2cy, -1.0f, bodyCol);
        EmitVertex(p1rx, p1ry, 0.0f, edgeCol);
        EmitVertex(p2cx, p2cy, -1.0f, bodyCol);
        EmitVertex(p2rx, p2ry, 0.0f, edgeCol);
    }

    // Matches stable's glDrawHalfCircle + CalculateCapMesh.
    // Pre-computes unit half-circle vertices, transforms by rotation+scale+translation.
    // Center at z=-1 (body), rim at z=0 (border).
    inline void DrawHalfCircle(int count, Vector2 center, float radius, float theta, bool flip,
                               Color edgeCol, Color bodyCol) {
        if (count <= 0) {
            return;
        }
        count = std::min(count, MAXRES);
        float step = (float)M_PI / (float)MAXRES;
        float cosT = cosf(theta);
        float sinT = sinf(theta);
        float scaleY = flip ? -radius : radius;
        auto transform = [&](float vx, float vy) -> Vector2 {
            float lx = vx * radius;
            float ly = vy * scaleY;
            return { center.x + lx * cosT - ly * sinT, center.y + lx * sinT + ly * cosT };
        };
        // cap_verts[0] = (0, -1), cap_verts[MAXRES] = (0, 1)
        Vector2 cur = transform(0.0f, -1.0f);
        for (int i = 0; i < count; i++) {
            float angle = (float)(i + 1) * step;
            Vector2 next = transform(sinf(angle), -cosf(angle));
            EmitVertex(center.x, center.y, -1.0f, bodyCol);
            EmitVertex(cur.x, cur.y, 0.0f, edgeCol);
            EmitVertex(next.x, next.y, 0.0f, edgeCol);
            cur = next;
        }
    }

    // Matches stable's DrawOGL: render to texture with depth test LEQUAL + depth clear.
    inline SliderBodyCache RenderSliderBody(const std::vector<Vector2>& screenPath, float radius) {
        SliderBodyCache cache;
        if (screenPath.size() < 2) {
            return cache;
        }
        float minX = screenPath[0].x, maxX = screenPath[0].x;
        float minY = screenPath[0].y, maxY = screenPath[0].y;
        for (auto& p : screenPath) {
            minX = std::min(minX, p.x);
            maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y);
            maxY = std::max(maxY, p.y);
        }
        float pad = radius + 4.0f;
        minX -= pad;
        minY -= pad;
        maxX += pad;
        maxY += pad;
        int w = (int)ceilf(maxX - minX);
        int h = (int)ceilf(maxY - minY);
        if (w < 1 || h < 1) {
            return cache;
        }
        struct LineSeg {
            Vector2 p1, p2;
            float theta, rho;
        };
        std::vector<LineSeg> lines;
        for (int i = 1; i < (int)screenPath.size(); i++) {
            float lx1 = screenPath[i-1].x - minX;
            float ly1 = screenPath[i-1].y - minY;
            float lx2 = screenPath[i].x - minX;
            float ly2 = screenPath[i].y - minY;
            float dx = lx2 - lx1;
            float dy = ly2 - ly1;
            float rho = sqrtf(dx*dx + dy*dy);
            if (rho < 0.5f) {
                continue;
            }
            lines.push_back({ {lx1, ly1}, {lx2, ly2}, atan2f(dy, dx), rho });
        }
        if (lines.empty()) {
            return cache;
        }
        Color edgeCol = { 10, 10, 10, 255 };
        Color bodyCol = { 90, 90, 90, 255 };
        cache.rt = LoadRenderTexture(w, h);
        cache.x = minX;
        cache.y = minY;
        cache.w = w;
        cache.h = h;
        BeginTextureMode(cache.rt);
        ClearBackground(BLANK);
        rlDrawRenderBatchActive();
        rlDisableBackfaceCulling();
        rlDisableColorBlend();
        rlEnableDepthTest();
        rlEnableDepthMask();
        rlClearColor(0, 0, 0, 0);
        rlClearScreenBuffers();
        rlSetTexture(rlGetTextureIdDefault());
        rlBegin(RL_TRIANGLES);
        for (int i = 0; i < (int)lines.size(); i++) {
            auto& curr = lines[i];
            DrawSegmentQuad(curr.p1, curr.p2, curr.theta, radius, edgeCol, bodyCol);
            int endTriangles;
            bool flip;
            if (i + 1 < (int)lines.size()) {
                auto& next = lines[i + 1];
                float dtheta = next.theta - curr.theta;
                if (dtheta > (float)M_PI) {
                    dtheta -= 2.0f * (float)M_PI;
                }
                if (dtheta < -(float)M_PI) {
                    dtheta += 2.0f * (float)M_PI;
                }
                if (dtheta < 0.0f) {
                    flip = true;
                    endTriangles = (int)ceilf((-dtheta) * (float)MAXRES / (float)M_PI);
                } else if (dtheta > 0.0f) {
                    flip = false;
                    endTriangles = (int)ceilf(dtheta * (float)MAXRES / (float)M_PI);
                } else {
                    flip = false;
                    endTriangles = 0;
                }
            } else {
                flip = false;
                endTriangles = MAXRES;
            }
            endTriangles = std::min(endTriangles, MAXRES);
            DrawHalfCircle(endTriangles, curr.p2, radius, curr.theta, flip, edgeCol, bodyCol);
            bool hasStartCap = false;
            if (i == 0) {
                hasStartCap = true;
            } else {
                auto& prev = lines[i - 1];
                if (fabsf(curr.p1.x - prev.p2.x) > 0.01f || fabsf(curr.p1.y - prev.p2.y) > 0.01f) {
                    hasStartCap = true;
                }
            }
            if (hasStartCap) {
                DrawHalfCircle(MAXRES, curr.p1, radius, curr.theta + (float)M_PI, false, edgeCol, bodyCol);
            }
        }
        rlEnd();
        rlDrawRenderBatchActive();
        rlEnableBackfaceCulling();
        rlEnableColorBlend();
        rlDisableDepthTest();
        rlDisableDepthMask();
        EndTextureMode();
        cache.valid = true;
        return cache;
    }

    inline void UnloadCache(SliderBodyCache& cache) {
        if (cache.valid) {
            UnloadRenderTexture(cache.rt);
            cache.valid = false;
        }
    }

    inline void DrawCachedBody(const SliderBodyCache& cache, unsigned char alpha) {
        if (!cache.valid) {
            return;
        }
        DrawTextureRec(cache.rt.texture,
            { 0, 0, (float)cache.w, -(float)cache.h },
            { cache.x, cache.y },
            { 255, 255, 255, alpha });
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
        DrawCachedBody(body_cache, alpha);
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
