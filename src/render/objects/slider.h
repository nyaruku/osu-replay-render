#pragma once
#include <reader/beatmap.h>
#include <render/objects/playfield.h>

namespace Render::Objects::Slider {

    namespace Path {

        inline float dist(Vector2 a, Vector2 b) {
            float dx = b.x - a.x, dy = b.y - a.y;
            return sqrtf(dx*dx + dy*dy);
        }

        inline Vector2 lerp2(Vector2 a, Vector2 b, float t) {
            return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
        }

        // --- Stable's adaptive bezier approximation (BezierApproximator) ---
        // Uses De Casteljau subdivision until the curve is "flat enough"
        // (2nd-order finite difference <= TOLERANCE).

        constexpr float BEZIER_TOLERANCE = 0.5f;
        constexpr float BEZIER_TOLERANCE_SQ = BEZIER_TOLERANCE * BEZIER_TOLERANCE;

        inline bool isFlatEnough(const std::vector<Vector2>& pts) {
            for (int i = 1; i < (int)pts.size() - 1; i++) {
                Vector2 d = {
                    pts[i-1].x - 2.0f * pts[i].x + pts[i+1].x,
                    pts[i-1].y - 2.0f * pts[i].y + pts[i+1].y
                };
                if (d.x * d.x + d.y * d.y > BEZIER_TOLERANCE_SQ)
                    return false;
            }
            return true;
        }

        inline void subdivide(const std::vector<Vector2>& pts, std::vector<Vector2>& left, std::vector<Vector2>& right) {
            int count = (int)pts.size();
            std::vector<Vector2> mid(pts);
            left.resize(count);
            right.resize(count);
            for (int i = 0; i < count; i++) {
                left[i] = mid[0];
                right[count - i - 1] = mid[count - i - 1];
                for (int j = 0; j < count - i - 1; j++) {
                    mid[j] = { (mid[j].x + mid[j+1].x) * 0.5f, (mid[j].y + mid[j+1].y) * 0.5f };
                }
            }
        }

        inline void approximate(const std::vector<Vector2>& pts, std::vector<Vector2>& output) {
            int count = (int)pts.size();
            // Subdivide once to get l (2*count-1 points)
            std::vector<Vector2> l, r;
            subdivide(pts, l, r);
            // Merge: l[0..count-1], r[1..count-1]
            std::vector<Vector2> combined(2 * count - 1);
            for (int i = 0; i < count; i++) combined[i] = l[i];
            for (int i = 1; i < count; i++) combined[count + i - 1] = r[i];

            output.push_back(pts[0]);
            for (int i = 1; i < count - 1; i++) {
                int idx = 2 * i;
                Vector2 p = {
                    0.25f * (combined[idx-1].x + 2.0f * combined[idx].x + combined[idx+1].x),
                    0.25f * (combined[idx-1].y + 2.0f * combined[idx].y + combined[idx+1].y)
                };
                output.push_back(p);
            }
        }

        inline std::vector<Vector2> createBezier(const std::vector<Vector2>& controlPoints) {
            std::vector<Vector2> output;
            int count = (int)controlPoints.size();
            if (count == 0) return output;

            std::vector<std::vector<Vector2>> toFlatten;
            toFlatten.push_back(controlPoints);

            while (!toFlatten.empty()) {
                std::vector<Vector2> parent = std::move(toFlatten.back());
                toFlatten.pop_back();

                if (isFlatEnough(parent)) {
                    approximate(parent, output);
                    continue;
                }

                std::vector<Vector2> left, right;
                subdivide(parent, left, right);
                // Push right first so left is processed first (depth-first)
                toFlatten.push_back(std::move(right));
                toFlatten.push_back(std::move(left));
            }

            output.push_back(controlPoints.back());
            return output;
        }

        // --- Catmull-Rom support (matching stable's CatmullRom) ---
        inline float catmullRomVal(float v1, float v2, float v3, float v4, float t) {
            float t2 = t * t;
            float t3 = t2 * t;
            return 0.5f * ((2.0f * v2) +
                           (-v1 + v3) * t +
                           (2.0f * v1 - 5.0f * v2 + 4.0f * v3 - v4) * t2 +
                           (-v1 + 3.0f * v2 - 3.0f * v3 + v4) * t3);
        }

        inline Vector2 catmullRomPoint(Vector2 v1, Vector2 v2, Vector2 v3, Vector2 v4, float t) {
            return { catmullRomVal(v1.x, v2.x, v3.x, v4.x, t),
                     catmullRomVal(v1.y, v2.y, v3.y, v4.y, t) };
        }

        // Stable uses SLIDER_DETAIL_LEVEL = 50 for Catmull
        constexpr int CATMULL_DETAIL = 50;

        // --- Perfect circle arc (matching stable's CircleThroughPoints) ---
        inline std::vector<Vector2> computeCircularArc(Vector2 a, Vector2 b, Vector2 c) {
            float d = 2.0f * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
            // IsStraightLine: exact float comparison like stable
            if (d == 0.0f) {
                return { a, b, c };
            }
            float aSq = a.x*a.x + a.y*a.y, bSq = b.x*b.x + b.y*b.y, cSq = c.x*c.x + c.y*c.y;
            Vector2 center = {
                (aSq*(b.y - c.y) + bSq*(c.y - a.y) + cSq*(a.y - b.y)) / d,
                (aSq*(c.x - b.x) + bSq*(a.x - c.x) + cSq*(b.x - a.x)) / d
            };
            float radius = dist(center, a);

            // Matching stable's CircleThroughPoints: use atan2 and wrap
            double tInitial = atan2((double)(a.y - center.y), (double)(a.x - center.x));
            double tMid     = atan2((double)(b.y - center.y), (double)(b.x - center.x));
            double tFinal   = atan2((double)(c.y - center.y), (double)(c.x - center.x));

            while (tMid < tInitial) tMid += 2.0 * M_PI;
            while (tFinal < tInitial) tFinal += 2.0 * M_PI;
            if (tMid > tFinal) {
                tFinal -= 2.0 * M_PI;
            }

            double arc_length = fabs((tFinal - tInitial) * radius);
            int n = (int)(arc_length * 0.125f);

            std::vector<Vector2> pts;
            pts.reserve(n + 1);
            pts.push_back(a);
            for (int i = 1; i < n; i++) {
                double progress = (double)i / (double)n;
                double t = tFinal * progress + tInitial * (1.0 - progress);
                Vector2 p = { center.x + radius * (float)cos(t), center.y + radius * (float)sin(t) };
                pts.push_back(p);
            }
            pts.push_back(c);
            return pts;
        }

        inline std::vector<Vector2> flatten(const Reader::Beatmap::HitObject& obj) {
            std::vector<Vector2> ctrl;
            // Matching stable: SliderOsu constructor inserts Position at index 0
            // ONLY if sliderCurvePoints[0] != Position.
            Vector2 head = { (float)obj.x, (float)obj.y };
            bool firstCpEqualsHead = !obj.curvePoints.empty() &&
                obj.curvePoints[0].x == obj.x &&
                obj.curvePoints[0].y == obj.y;
            if (!firstCpEqualsHead) {
                ctrl.push_back(head);
            }
            for (auto& cp : obj.curvePoints) {
                ctrl.push_back({ (float)cp.x, (float)cp.y });
            }
            if (ctrl.empty()) {
                ctrl.push_back(head);
            }
            if (ctrl.size() <= 1) {
                return ctrl;
            }
            if (obj.curveType == 'L') {
                return ctrl;
            }
            if (obj.curveType == 'P') {
                if (ctrl.size() < 3) {
                    // < 3 points: treat as linear (stable: goto Linear)
                    return ctrl;
                }
                if (ctrl.size() > 3) {
                    // > 3 points: treat as bezier (stable: goto Bezier)
                    // Fall through to bezier below
                } else {
                    // Exactly 3 points
                    float cross = (ctrl[1].x - ctrl[0].x) * (ctrl[2].y - ctrl[0].y)
                                - (ctrl[2].x - ctrl[0].x) * (ctrl[1].y - ctrl[0].y);
                    if (cross == 0.0f) {
                        // Collinear: treat as linear (stable: goto Linear)
                        return ctrl;
                    }
                    return computeCircularArc(ctrl[0], ctrl[1], ctrl[2]);
                }
            }
            if (obj.curveType == 'C') {
                // Catmull-Rom: matching stable's algorithm
                std::vector<Vector2> result;
                for (int j = 0; j < (int)ctrl.size() - 1; j++) {
                    Vector2 v1 = (j - 1 >= 0) ? ctrl[j - 1] : ctrl[j];
                    Vector2 v2 = ctrl[j];
                    Vector2 v3 = (j + 1 < (int)ctrl.size()) ? ctrl[j + 1]
                                 : Vector2{ v2.x + (v2.x - v1.x), v2.y + (v2.y - v1.y) };
                    Vector2 v4 = (j + 2 < (int)ctrl.size()) ? ctrl[j + 2]
                                 : Vector2{ v3.x + (v3.x - v2.x), v3.y + (v3.y - v2.y) };

                    for (int k = 0; k <= CATMULL_DETAIL; k++) {
                        float t = (float)k / CATMULL_DETAIL;
                        Vector2 p = catmullRomPoint(v1, v2, v3, v4, t);
                        if (result.empty() || result.back().x != p.x || result.back().y != p.y)
                            result.push_back(p);
                    }
                }
                return result;
            }
            // Bezier ('B') or PerfectCurve with >3 points
            std::vector<Vector2> result;
            std::vector<Vector2> seg;
            seg.push_back(ctrl[0]);
            for (int i = 1; i < (int)ctrl.size(); i++) {
                seg.push_back(ctrl[i]);
                // Stable uses Count - 2 (not Count - 1): the last pair of duplicate
                // points is NOT treated as a red anchor, just as end of segment.
                bool red_anchor = (i < (int)ctrl.size() - 2 &&
                                   ctrl[i].x == ctrl[i+1].x && ctrl[i].y == ctrl[i+1].y);
                bool last = (i == (int)ctrl.size() - 1);
                if (red_anchor || last) {
                    std::vector<Vector2> sampled;
                    if (seg.size() == 2) {
                        // 2-point segment: linear (matching stable)
                        sampled.push_back(seg[0]);
                        sampled.push_back(seg[1]);
                    } else {
                        // Use adaptive bezier approximation (matching stable)
                        sampled = createBezier(seg);
                    }
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

        // Matching stable's path truncation/extension (SliderOsu.UpdateCalculations lines 712-731).
        // Stable removes segments from the END until excess is consumed, then shortens the last
        // segment. If the curve is SHORTER than SpatialLength (excess < 0), stable EXTENDS the
        // last segment in the same direction. This is what creates "disconnected" aspire sliders
        // where the body stretches far beyond the curve's natural end.
        constexpr double MIN_SEGMENT_LENGTH = 0.0001;

        inline std::vector<Vector2> truncateToLength(const std::vector<Vector2>& pts, float target) {
            if (pts.empty()) {
                return pts;
            }
            if (pts.size() < 2) {
                return pts;
            }
            float total = 0.0f;
            for (int i = 1; i < (int)pts.size(); i++) {
                total += dist(pts[i-1], pts[i]);
            }
            if (total <= 0.0f) {
                return pts;
            }
            double excess = (double)total - (double)target;
            // Work on a copy
            std::vector<Vector2> result(pts);

            // Stable iterates from the end, removing or shortening segments
            while (result.size() > 1) {
                Vector2& p1 = result[result.size() - 2];
                Vector2& p2 = result[result.size() - 1];
                float dx = p2.x - p1.x, dy = p2.y - p1.y;
                float lastLen = sqrtf(dx * dx + dy * dy);

                if ((double)lastLen > excess + MIN_SEGMENT_LENGTH) {
                    // Shorten (or extend if excess < 0) the last segment
                    float newLen = lastLen - (float)excess;
                    if (lastLen > 0.0f) {
                        float nx = dx / lastLen, ny = dy / lastLen;
                        p2 = { p1.x + nx * newLen, p1.y + ny * newLen };
                    }
                    break;
                }

                // Remove the last point entirely
                result.pop_back();
                excess -= (double)lastLen;
            }
            return result;
        }

        inline Vector2 sampleAt(const std::vector<Vector2>& pts, float distance) {
            if (pts.size() == 1) {
                return pts[0];
            }
            float acc = 0.0f;
            for (int i = 1; i < (int)pts.size(); i++) {
                float seg = dist(pts[i-1], pts[i]);
                if (acc + seg >= distance) {
                    float t = seg > 0.0f ? (distance - acc) / seg : 0.0f;
                    return lerp2(pts[i-1], pts[i], t);
                }
                acc += seg;
            }
            return pts.back();
        }

        inline float totalLength(const std::vector<Vector2>& pts) {
            float len = 0.0f;
            for (int i = 1; i < (int)pts.size(); i++) {
                len += dist(pts[i-1], pts[i]);
            }
            return len;
        }
    }

    inline double getBeatLengthAt(int time, const std::vector<Reader::Beatmap::TimingPoint>& tps) {
        double bl = 500.0;
        int lo = 0, hi = (int)tps.size() - 1, best = -1;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            if (tps[mid].time <= time) {
                best = mid;
                lo = mid + 1;
            } else {
                hi = mid - 1;
            }
        }
        for (int i = best; i >= 0; i--) {
            if (tps[i].uninherited) {
                bl = tps[i].beatLength;
                break;
            }
        }
        return bl;
    }

    inline double getSvAt(int time, const std::vector<Reader::Beatmap::TimingPoint>& tps) {
        double sv = 1.0;
        int lo = 0, hi = (int)tps.size() - 1, best = -1;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            if (tps[mid].time <= time) {
                best = mid;
                lo = mid + 1;
            } else {
                hi = mid - 1;
            }
        }
        for (int i = best; i >= 0; i--) {
            if (!tps[i].uninherited) {
                // No upper clamp: aspire maps use very small negative beat_lengths
                // (e.g. -0.0667 -> SV≈1500) that must be respected for correct slide_dur.
                sv = std::max(0.1, -100.0 / tps[i].beatLength);
                break;
            }
        }
        return sv;
    }

    inline std::vector<Vector2> computeSliderPath(const Reader::Beatmap::HitObject& obj) {
        auto raw = Path::flatten(obj);
        return Path::truncateToLength(raw, (float)obj.length);
    }

    constexpr int GRAD_SIZE = 128;
    constexpr int GRADIENT_SIZE = GRAD_SIZE;

    struct GradientColor {
        unsigned char r, g, b, a;
    };

    inline GradientColor gradient[GRAD_SIZE];
    inline bool gradientBuilt = false;

    inline void buildGradient() {
        if (gradientBuilt) {
            return;
        }
        Color shadow = { 0, 0, 0, 0 };
        Color border = { 100, 100, 100, 255 };
        Color outer  = { 0, 0, 0, 100 };
        Color inner  = { 120, 120, 120, 100 };
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
            gradient[i] = c;
        }
        gradientBuilt = true;
    }

    inline GradientColor sampleGradient(float t) {
        t = std::max(0.0f, std::min(1.0f, t));
        int idx = (int)(t * (GRAD_SIZE - 1));
        idx = std::max(0, std::min(GRAD_SIZE - 1, idx));
        return gradient[idx];
    }

    inline float pointSegDist(float px, float py, float ax, float ay, float bx, float by) {
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
        float x = 0, y = 0;   // origin (osu! coords unless screen_space=true)
        float w = 0, h = 0;   // dimensions for drawing
        bool valid = false;
        bool screenSpace = false; // if true, x/y/w/h are in screen coords (for stable squash)
    };

    // Simulate stable's viewport-clamped rendering for oversized sliders.
    // In stable:
    //   1. Compute bounding box in screen coords -> drawLeft, drawTop, drawWidth, drawHeight
    //   2. excessLeft = max(-drawLeft, 0), excessTop = max(-drawTop, 0)
    //   3. GL viewport = (0, 0, min(drawWidth, windowW), min(drawHeight-excessTop, windowH))
    //   4. Ortho maps [drawLeft+excessLeft .. +drawWidth] × [drawTop+excessTop .. +drawHeight] to viewport
    //   5. Since viewport is clamped but ortho isn't, the body gets squashed (non-uniform scale)
    //   6. Resulting texture drawn at (drawLeft+excessLeft, drawTop+excessTop)
    //
    // A point at screen coord (X,Y) ends up at:
    //   screenX = spriteLeft + (X - orthoLeft) * vpW / drawWidth
    //   screenY = spriteTop  + (Y - orthoTop)  * vpH / (drawHeight - excessTop)
    //
    // We compute the distance field in this squashed space and store screen-space coords.
    inline SliderBodyCache computeDistanceFieldStableSquash(
        const std::vector<Vector2>& osuPath, float osuRadius,
        const Render::Objects::Playfield::Playfield& pf)
    {
        SliderBodyCache cache;
        float screenRadius = osuRadius * pf.scale;

        // Convert path to screen coords
        std::vector<Vector2> screenPath(osuPath.size());
        for (int i = 0; i < (int)osuPath.size(); i++) {
            screenPath[i] = { pf.x + osuPath[i].x * pf.scale, pf.y + osuPath[i].y * pf.scale };
        }

        // Stable's bounding box (constructLineList)
        float tlX = screenPath[0].x, tlY = screenPath[0].y;
        float brX = screenPath[0].x, brY = screenPath[0].y;
        for (auto& p : screenPath) {
            tlX = std::min(tlX, p.x); tlY = std::min(tlY, p.y);
            brX = std::max(brX, p.x); brY = std::max(brY, p.y);
        }
        float r = screenRadius;
        int drawWidth  = (int)(brX - tlX + r * 2.3f);
        int drawHeight = (int)(brY - tlY + r * 2.3f);
        int drawLeft   = (int)(tlX - r * 1.15f);
        int drawTop    = (int)(tlY - r * 1.15f);

        // Stable's excess (clipping negative coords)
        int excessTop  = std::max(-drawTop, 0);
        int excessLeft = std::max(-drawLeft, 0);

        // Stable's viewport (clamped to window size)
        int windowW = GetScreenWidth();
        int windowH = GetScreenHeight();
        if (windowW < 1) windowW = 1920;
        if (windowH < 1) windowH = 1080;
        int vpW = std::min(drawWidth, windowW);
        int vpH = std::min(drawHeight - excessTop, windowH);
        if (vpW < 1 || vpH < 1) return cache;

        // Ortho projection range
        float orthoLeft = (float)(drawLeft + excessLeft);
        float orthoTop  = (float)(drawTop + excessTop);
        float orthoW    = (float)drawWidth;
        float orthoH    = (float)(drawHeight - excessTop);

        // Scale factors: ortho range -> viewport pixels
        float scaleX = (float)vpW / orthoW;
        float scaleY = (float)vpH / orthoH;

        // In stable, GL.LineWidth(radius) sets line thickness in VIEWPORT pixels.
        // The ortho projection squashes path vertices into viewport space, but the
        // line width remains constant in viewport pixels. So we must compute distance
        // in viewport space with a constant radius of `r` viewport pixels.

        // Transform path to viewport coords
        std::vector<Vector2> vpPath(screenPath.size());
        for (int i = 0; i < (int)screenPath.size(); i++) {
            vpPath[i] = {
                (screenPath[i].x - orthoLeft) * scaleX,
                (screenPath[i].y - orthoTop) * scaleY
            };
        }

        // Find visible segments (whose viewport bounding box overlaps [0,vpW)×[0,vpH))
        float pad = r + 2.0f;
        std::vector<int> visibleSegs;
        for (int s = 1; s < (int)vpPath.size(); s++) {
            float ax = vpPath[s-1].x, ay = vpPath[s-1].y;
            float bx = vpPath[s].x, by = vpPath[s].y;
            if (std::max(ax, bx) + pad >= 0 && std::min(ax, bx) - pad <= vpW &&
                std::max(ay, by) + pad >= 0 && std::min(ay, by) - pad <= vpH) {
                visibleSegs.push_back(s);
            }
        }
        if (visibleSegs.empty()) return cache;

        // Compute distance field in viewport pixel space.
        // Stable's GL line rendering gives constant-width tubes in viewport pixels,
        // so distance and radius are both in viewport pixel units.
        std::vector<float> distBuf(vpW * vpH, r + 1.0f);
        for (int s : visibleSegs) {
            float ax = vpPath[s-1].x, ay = vpPath[s-1].y;
            float bx = vpPath[s].x, by = vpPath[s].y;
            float dx = bx - ax, dy = by - ay;
            float segLenSq = dx * dx + dy * dy;

            // Bounding box in viewport pixels
            int x0 = std::max(0, (int)(std::min(ax, bx) - r) - 1);
            int x1 = std::min(vpW - 1, (int)(std::max(ax, bx) + r) + 1);
            int y0 = std::max(0, (int)(std::min(ay, by) - r) - 1);
            int y1 = std::min(vpH - 1, (int)(std::max(ay, by) + r) + 1);

            for (int py = y0; py <= y1; py++) {
                float vpy = (float)py + 0.5f;
                for (int px = x0; px <= x1; px++) {
                    float vpx = (float)px + 0.5f;
                    float t;
                    if (segLenSq < 1e-8f) {
                        t = 0.0f;
                    } else {
                        t = ((vpx - ax) * dx + (vpy - ay) * dy) / segLenSq;
                        if (t < 0.0f) t = 0.0f;
                        else if (t > 1.0f) t = 1.0f;
                    }
                    float cx = ax + t * dx - vpx;
                    float cy = ay + t * dy - vpy;
                    float d = sqrtf(cx * cx + cy * cy);
                    if (d >= r) continue;
                    int idx = py * vpW + px;
                    if (d < distBuf[idx]) distBuf[idx] = d;
                }
            }
        }

        // Encode distance field as 8-bit grayscale
        std::vector<unsigned char> pixels8(vpW * vpH, 0);
        for (int i = 0; i < vpW * vpH; i++) {
            if (distBuf[i] >= r) continue;
            float t = 1.0f - distBuf[i] / r;
            pixels8[i] = (unsigned char)(t * 255.0f);
        }
        Image img = { pixels8.data(), vpW, vpH, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE };
        cache.tex = LoadTextureFromImage(img);
        SetTextureFilter(cache.tex, TEXTURE_FILTER_BILINEAR);
        // Position: stable draws the sprite at (drawLeft + excessLeft, drawTop + excessTop)
        // with size = viewport size (1:1 pixel mapping)
        cache.x = (float)(drawLeft + excessLeft);
        cache.y = (float)(drawTop + excessTop);
        cache.w = (float)vpW;
        cache.h = (float)vpH;
        cache.screenSpace = true;
        cache.valid = true;
        return cache;
    }

    inline SliderBodyCache computeDistanceField(const std::vector<Vector2>& osuPath, float radius,
                                                float minX, float minY, int w, int h, float scale) {
        SliderBodyCache cache;
        float sRadius = radius * scale;
        float sRadiusSq = sRadius * sRadius;
        int iRadius = (int)ceilf(sRadius);
        float minSpacing = 1.0f / scale;
        if (osuPath.size() > 2000) {
            minSpacing = std::max(minSpacing, Path::totalLength(osuPath) / 2000.0f);
        }
        std::vector<Vector2> dp;
        dp.push_back(osuPath[0]);
        float spacingSq = minSpacing * minSpacing;
        for (int i = 1; i < (int)osuPath.size() - 1; i++) {
            float dx = osuPath[i].x - dp.back().x;
            float dy = osuPath[i].y - dp.back().y;
            if (dx * dx + dy * dy >= spacingSq) {
                dp.push_back(osuPath[i]);
            }
        }
        dp.push_back(osuPath.back());
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
                        if (t < 0.0f) t = 0.0f;
                        else if (t > 1.0f) t = 1.0f;
                    }
                    float cx = ax + t * sdx - fx;
                    float cy = ay + t * sdy - fy;
                    float dSq = cx * cx + cy * cy;
                    if (dSq >= sRadiusSq) continue;
                    float d = sqrtf(dSq);
                    int idx = py * w + px;
                    if (d < distBuf[idx]) distBuf[idx] = d;
                }
            }
        }
        // Encode distance field as 8-bit grayscale
        std::vector<unsigned char> pixels8(w * h, 0);
        for (int i = 0; i < w * h; i++) {
            if (distBuf[i] >= sRadius) continue;
            float t = 1.0f - distBuf[i] / sRadius;
            pixels8[i] = (unsigned char)(t * 255.0f);
        }
        Image img = { pixels8.data(), w, h, 1, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE };
        cache.tex = LoadTextureFromImage(img);
        SetTextureFilter(cache.tex, TEXTURE_FILTER_BILINEAR);
        cache.x = minX;
        cache.y = minY;
        cache.w = (float)w / scale;
        cache.h = (float)h / scale;
        cache.valid = true;
        return cache;
    }

    inline SliderBodyCache renderSliderBody(const std::vector<Vector2>& osuPath, float radius,
                                            const Render::Objects::Playfield::Playfield& pf) {
        SliderBodyCache cache;
        if (osuPath.size() < 2) {
            return cache;
        }
        buildGradient();

        // screen-space bounding box to decide render path
        float screenRadius = radius * pf.scale;
        float stlX = pf.x + osuPath[0].x * pf.scale;
        float stlY = pf.y + osuPath[0].y * pf.scale;
        float sbrX = stlX, sbrY = stlY;
        for (auto& p : osuPath) {
            float sx = pf.x + p.x * pf.scale;
            float sy = pf.y + p.y * pf.scale;
            stlX = std::min(stlX, sx); stlY = std::min(stlY, sy);
            sbrX = std::max(sbrX, sx); sbrY = std::max(sbrY, sy);
        }
        float sDrawW = (sbrX - stlX) + screenRadius * 2.3f;
        float sDrawH = (sbrY - stlY) + screenRadius * 2.3f;
        float sDrawLeft = stlX - screenRadius * 1.15f;
        float sDrawTop  = stlY - screenRadius * 1.15f;

        int windowW = GetScreenWidth();
        int windowH = GetScreenHeight();
        if (windowW < 1) windowW = 1920;
        if (windowH < 1) windowH = 1080;

        // Stable squashes when the bounding box exceeds the window in any dimension,
        // or when it starts off-screen (excessLeft/excessTop > 0).
        bool needsSquash = (sDrawLeft < 0) || (sDrawTop < 0) ||
                           (sDrawLeft + sDrawW > windowW) || (sDrawTop + sDrawH > windowH);

        if (needsSquash) {
            return computeDistanceFieldStableSquash(osuPath, radius, pf);
        }

        // normal path: render at osu! coordinate scale
        float minX = osuPath[0].x, maxX = osuPath[0].x;
        float minY = osuPath[0].y, maxY = osuPath[0].y;
        for (auto& p : osuPath) {
            minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        }
        float pad = radius + 2.0f;
        minX -= pad; minY -= pad; maxX += pad; maxY += pad;
        float rawW = maxX - minX, rawH = maxY - minY;
        if (rawW < 1.0f || rawH < 1.0f) {
            return cache;
        }
        constexpr int maxDim = 4096;
        constexpr int maxPixels = 4 * 1024 * 1024;
        float scale = 1.0f;
        if (rawW > maxDim || rawH > maxDim) {
            scale = std::min((float)maxDim / rawW, (float)maxDim / rawH);
        }
        if (rawW * scale * rawH * scale > maxPixels) {
            scale = sqrtf((float)maxPixels / (rawW * rawH));
        }
        scale = std::max(scale, 0.01f);
        int w = (int)ceilf(rawW * scale);
        int h = (int)ceilf(rawH * scale);
        if (w < 1 || h < 1) {
            return cache;
        }
        return computeDistanceField(osuPath, radius, minX, minY, w, h, scale);
    }

    inline bool isOversized(const std::vector<Vector2>& osuPath, float radius,
                            const Render::Objects::Playfield::Playfield& pf) {
        if (osuPath.size() < 2) return false;
        float screenRadius = radius * pf.scale;
        float tlX = pf.x + osuPath[0].x * pf.scale, tlY = pf.y + osuPath[0].y * pf.scale;
        float brX = tlX, brY = tlY;
        for (auto& p : osuPath) {
            float sx = pf.x + p.x * pf.scale, sy = pf.y + p.y * pf.scale;
            tlX = std::min(tlX, sx); tlY = std::min(tlY, sy);
            brX = std::max(brX, sx); brY = std::max(brY, sy);
        }
        float dLeft = tlX - screenRadius * 1.15f;
        float dTop  = tlY - screenRadius * 1.15f;
        float dW    = (brX - tlX) + screenRadius * 2.3f;
        float dH    = (brY - tlY) + screenRadius * 2.3f;
        int winW = GetScreenWidth(),  winH = GetScreenHeight();
        if (winW < 1) winW = 1920; if (winH < 1) winH = 1080;
        return dLeft < 0 || dTop < 0 || dLeft + dW > winW || dTop + dH > winH;
    }

    inline void unloadCache(SliderBodyCache& cache) {
        if (cache.valid) {
            UnloadTexture(cache.tex);
            cache.valid = false;
        }
    }

    inline Shader& getGradientShader() {
        static Shader shader = { 0 };
        static bool loaded = false;
        if (!loaded) {
            const char* fs = "#version 330\n"
                "in vec2 fragTexCoord;\n"
                "in vec4 fragColor;\n"
                "uniform sampler2D texture0;\n"
                "uniform sampler2D gradientTex;\n"
                "out vec4 finalColor;\n"
                "void main() {\n"
                "    float t = texture(texture0, fragTexCoord).r;\n"
                "    if (t < 0.001) discard;\n"
                "    vec4 grad = texture(gradientTex, vec2(t, 0.5));\n"
                "    finalColor = grad * fragColor;\n"
                "}\n";
            shader = LoadShaderFromMemory(0, fs);
            loaded = true;
        }
        return shader;
    }

    inline Texture2D& getGradientTexture() {
        static Texture2D tex = { 0 };
        static bool built = false;
        if (!built) {
            buildGradient();
            unsigned char pixels[GRADIENT_SIZE * 4];
            for (int i = 0; i < GRADIENT_SIZE; i++) {
                GradientColor c = sampleGradient((float)i / (float)(GRADIENT_SIZE - 1));
                pixels[i * 4 + 0] = c.r;
                pixels[i * 4 + 1] = c.g;
                pixels[i * 4 + 2] = c.b;
                pixels[i * 4 + 3] = c.a;
            }
            Image img = { pixels, GRADIENT_SIZE, 1, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
            tex = LoadTextureFromImage(img);
            SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
            built = true;
        }
        return tex;
    }

    inline void drawCachedBody(const SliderBodyCache& cache, unsigned char alpha,
                               const Render::Objects::Playfield::Playfield& pf) {
        if (!cache.valid) {
            return;
        }
        Shader& shader = getGradientShader();
        Texture2D& gradTex = getGradientTexture();
        int gradLoc = GetShaderLocation(shader, "gradientTex");
        BeginShaderMode(shader);
        SetShaderValueTexture(shader, gradLoc, gradTex);
        Rectangle src = { 0, 0, (float)cache.tex.width, (float)cache.tex.height };
        Rectangle dst;
        if (cache.screenSpace) {
            // Already in screen coords (stable squash for oversized sliders)
            dst = { cache.x, cache.y, cache.w, cache.h };
        } else {
            // Map osu! coords to screen coords
            dst = {
                pf.x + cache.x * pf.scale,
                pf.y + cache.y * pf.scale,
                cache.w * pf.scale,
                cache.h * pf.scale
            };
        }
        DrawTexturePro(cache.tex, src, dst, { 0, 0 }, 0.0f, { 255, 255, 255, alpha });
        EndShaderMode();
    }

    inline void drawSliderBodyDirect(const std::vector<Vector2>& screenPath, float radius, unsigned char alpha) {
        if (screenPath.size() < 2) {
            return;
        }
        buildGradient();
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
                        if (t < 0.0f) t = 0.0f;
                        else if (t > 1.0f) t = 1.0f;
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
            GradientColor c = sampleGradient(t);
            pixels[i * 4 + 0] = c.r;
            pixels[i * 4 + 1] = c.g;
            pixels[i * 4 + 2] = c.b;
            pixels[i * 4 + 3] = c.a;
        }
        Image img = { pixels.data(), w, h, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
        Texture2D tex = LoadTextureFromImage(img);
        DrawTexture(tex, (int)minX, (int)minY, WHITE);
        UnloadTexture(tex);
    }

    inline void draw(
        const Reader::Beatmap::HitObject& obj,
        const std::vector<Vector2>& sliderPath,
        const std::vector<Vector2>& screenPath,
        float pathLen,
        float currentMs,
        const Render::Objects::Playfield::Playfield& pf,
        float baseRadius, float preempt,
        float sliderMult, float tickRate,
        const std::vector<Reader::Beatmap::TimingPoint>& timingPoints,
        const SliderBodyCache& bodyCache)
    {
        float timeUntil = (float)obj.time - currentMs;
        // Matching stable: FadeIn = 400ms, FadeOut = 240ms
        constexpr float FADE_IN_MS = 400.0f;
        constexpr float FADE_OUT_MS = 240.0f;
        if (timeUntil > preempt) {
            return;
        }
        float sv = (float)getSvAt(obj.time, timingPoints);
        float beatLen = (float)getBeatLengthAt(obj.time, timingPoints);
        float slideDur = (float)(obj.length / ((double)sliderMult * 100.0 * sv) * beatLen);
        float endTime = (float)obj.time + slideDur * (float)obj.slides;
        if (currentMs > endTime + FADE_OUT_MS) {
            return;
        }
        float fadeAlpha;
        // Stable: Fade(0->1, StartTime - PreEmpt, StartTime - PreEmpt + FadeIn)
        float appearTime = (float)obj.time - preempt;
        if (currentMs < appearTime + FADE_IN_MS) {
            fadeAlpha = (currentMs - appearTime) / FADE_IN_MS;
        } else {
            fadeAlpha = 1.0f;
        }
        // Stable: Fade(1->0, EndTime, EndTime + FadeOut)
        if (currentMs > endTime) {
            fadeAlpha = 1.0f - std::min(1.0f, (currentMs - endTime) / FADE_OUT_MS);
        }
        fadeAlpha = std::max(0.0f, fadeAlpha);
        unsigned char alpha = (unsigned char)(fadeAlpha * 255.0f);
        float r = baseRadius * pf.scale;
        auto toScreen = [&](Vector2 p) -> Vector2 {
            return { pf.x + p.x * pf.scale, pf.y + p.y * pf.scale };
        };
        drawCachedBody(bodyCache, alpha, pf);
        if (screenPath.size() >= 2) {
            // Slider End Circle
        }
        float tickSpacing = sliderMult * 100.0f * sv / tickRate;
        if (tickSpacing > 0.0f) {
            for (int s = 0; s < obj.slides; s++) {
                float slideStartMs = (float)obj.time + (float)s * slideDur;
                for (float d = tickSpacing; d < pathLen - 1.0f; d += tickSpacing) {
                    if (slideStartMs + (d / pathLen) * slideDur < currentMs) {
                        continue;
                    }
                    float actualD = (s % 2 == 0) ? d : pathLen - d;
                    DrawCircleV(toScreen(Path::sampleAt(sliderPath, actualD)), r * 0.22f, { 255, 255, 255, alpha });
                }
            }
        }
        Vector2 headSc = sliderPath.empty() ? toScreen({ (float)obj.x, (float)obj.y }) : toScreen(sliderPath.front());
        if (timeUntil >= 0.0f) {
            float approachScale = 1.0f + 3.0f * std::max(0.0f, timeUntil / preempt);
            DrawCircleLines((int)headSc.x, (int)headSc.y, r * approachScale, { 255, 255, 255, alpha });
            DrawCircleV(headSc, r, { 70, 130, 180, alpha });
            DrawCircleLinesV(headSc, r, { 255, 255, 255, alpha });
        }
        if (currentMs >= (float)obj.time && currentMs <= endTime) {
            float elapsed = currentMs - (float)obj.time;
            float slideProgress = (slideDur > 0.0f) ? fmodf(elapsed, slideDur) / slideDur : 0.0f;
            bool reversed = ((int)(elapsed / slideDur)) % 2 == 1;
            float ballD = reversed ? (1.0f - slideProgress) * pathLen : slideProgress * pathLen;
            Vector2 ballSc = toScreen(Path::sampleAt(sliderPath, ballD));
            DrawCircleV(ballSc, r, { 255, 200, 50, 220 });
            DrawCircleLinesV(ballSc, r, { 255, 255, 255, 220 });
            DrawCircleLines((int)ballSc.x, (int)ballSc.y, r * 2.4f, { 255, 255, 255, 80 });
        }
    }
}
