#pragma once
#include <config/constants.h>
#include <config/config.h>
#include <raylib.h>
#include <raygui.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <render/utils/font.h>
#include <render/utils/toast.h>
#include <render/views/debug.h>

namespace Render::Settings {

    struct FieldEntry {
        const Config::FieldSchema* schema = nullptr;
        std::string sectionLabel;
        std::string value;
        bool        active = false;
        int         cursor = 0;
    };

    inline std::vector<FieldEntry> sFields;
    inline int   sSelectedGroup = 0;
    inline float sGroupScroll   = 0.0f;
    inline float sFieldScroll   = 0.0f;

    inline std::string jsonToDisplay(const nlohmann::ordered_json& j) {
        if (j.is_string())  return j.get<std::string>();
        if (j.is_boolean()) return j.get<bool>() ? "true" : "false";
        return j.dump();
    }

    inline void loadGroup(int idx) {
        sFields.clear();
        sFieldScroll = 0.0f;
        if (idx < 0 || idx >= (int)Config::groups.size()) return;

        const std::string& groupKey = Config::groups[idx].key;
        std::string currentSection = "\x01"; // sentinel – never matches a real section

        for (const auto& fs : Config::fields) {
            // only fields belonging to this group
            if (fs.key.substr(0, groupKey.size()) != groupKey) continue;
            if (fs.key.size() > groupKey.size() && fs.key[groupKey.size()] != '.') continue;

            // insert a section header when section changes
            if (fs.section != currentSection) {
                currentSection = fs.section;
                if (!fs.section.empty()) {
                    FieldEntry hdr;
                    hdr.schema        = nullptr;
                    hdr.sectionLabel = fs.section;
                    sFields.push_back(hdr);
                }
            }

            FieldEntry e;
            e.schema = &fs;

            // read current value from configData
            auto parts    = Config::splitKey(fs.key);
            auto* node_ptr = Config::traverseToParent(parts, false);
            if (node_ptr && node_ptr->contains(parts.back()))
                e.value = jsonToDisplay((*node_ptr)[parts.back()]);
            else {
                // fall back to defaults
                nlohmann::ordered_json* d = &Config::defaults;
                for (size_t i = 0; i + 1 < parts.size(); i++) {
                    if (!d->contains(parts[i])) { d = nullptr; break; }
                    d = &(*d)[parts[i]];
                }
                if (d && d->contains(parts.back()))
                    e.value = jsonToDisplay((*d)[parts.back()]);
            }

            sFields.push_back(e);
        }
    }

    inline void open() {
        sSelectedGroup = 0;
        loadGroup(0);
    }

    inline void drawFieldControl(FieldEntry& e, float x, float y, float w, float box_h,
                                  bool mouse_clicked, Vector2 mouse) {
        const auto& fs = *e.schema;

        if (fs.type == Config::FieldType::Bool) {
            float cb  = box_h - 8.0f;
            Rectangle box = {x, y + (box_h - cb) / 2.0f, cb, cb};
            bool val = e.value == "true";
            DrawRectangleLinesEx(box, 1.5f, Config::Constants::Theme::CHECKBOX_BORDER);
            if (val)
                DrawRectangle((int)(box.x + 4), (int)(box.y + 4), (int)(cb - 8), (int)(cb - 8),
                              Config::Constants::Theme::CHECKBOX_BG_CHECKED);
            if (mouse_clicked && CheckCollisionPointRec(mouse, {x, y, w, box_h}))
                e.value = val ? "false" : "true";
            return;
        }

        Rectangle box = {x, y, w, box_h};
        bool is_slider = (fs.type == Config::FieldType::Int || fs.type == Config::FieldType::Float)
                         && fs.maxVal > fs.minVal;
        if (!is_slider) {
            DrawRectangleRec(box, e.active
                ? Config::Constants::Theme::INPUT_BG_ACTIVE
                : Config::Constants::Theme::INPUT_BG);
            DrawRectangleLinesEx(box, 1.0f, e.active
                ? Config::Constants::Theme::INPUT_BORDER_ACTIVE
                : Config::Constants::Theme::INPUT_BORDER);
        }

        if (fs.type == Config::FieldType::Int || fs.type == Config::FieldType::Float) {
            if (fs.maxVal > fs.minVal) {
                // slider + value label
                float cur = e.value.empty() ? fs.minVal : std::stof(e.value);
                float slider_w = w - 60.0f;
                float s_pad = 8.0f;
                GuiSliderBar({x + s_pad, y + (box_h - 12.0f) / 2.0f, slider_w - s_pad * 2.0f, 12.0f},
                             nullptr, nullptr, &cur, fs.minVal, fs.maxVal);
                cur = std::clamp(cur, fs.minVal, fs.maxVal);
                // snap to step
                float step = (fs.step > 0.0f) ? fs.step : 1.0f;
                cur = std::round(cur / step) * step;
                if (fs.type == Config::FieldType::Int) {
                    e.value = std::to_string((int)cur);
                    char lbl[32]; snprintf(lbl, sizeof(lbl), "%d", (int)cur);
                    Render::Utils::Font::drawText(lbl, (int)(x + slider_w - s_pad + 6), (int)(y + 8),
                        Config::Constants::FontSize::LABEL, Config::Constants::Theme::INPUT_TEXT);
                } else {
                    char buf[32]; snprintf(buf, sizeof(buf), "%.2f", cur);
                    e.value = buf;
                    Render::Utils::Font::drawText(buf, (int)(x + slider_w - s_pad + 6), (int)(y + 8),
                        Config::Constants::FontSize::LABEL, Config::Constants::Theme::INPUT_TEXT);
                }
                return; // slider handles its own input
            } else {
                // no range defined: fall back to +/- spinner buttons
                float bw = 28.0f;
                Rectangle minus_btn = {x,         y, bw, box_h};
                Rectangle plus_btn  = {x + w - bw, y, bw, box_h};
                bool minus_hov = CheckCollisionPointRec(mouse, minus_btn);
                bool plus_hov  = CheckCollisionPointRec(mouse, plus_btn);
                DrawRectangleRec(minus_btn, minus_hov
                    ? Config::Constants::Theme::SPINNER_BTN_BG_HOVER
                    : Config::Constants::Theme::SPINNER_BTN_BG);
                DrawRectangleRec(plus_btn, plus_hov
                    ? Config::Constants::Theme::SPINNER_BTN_BG_HOVER
                    : Config::Constants::Theme::SPINNER_BTN_BG);
                Render::Utils::Font::drawText("-", (int)(minus_btn.x + 9), (int)(y + 7),
                    Config::Constants::FontSize::LABEL, Config::Constants::Theme::SPINNER_BTN_TEXT);
                Render::Utils::Font::drawText("+", (int)(plus_btn.x + 7),  (int)(y + 7),
                    Config::Constants::FontSize::LABEL, Config::Constants::Theme::SPINNER_BTN_TEXT);
                if (mouse_clicked) {
                    float step = (fs.step != 0.0f) ? fs.step
                               : (fs.type == Config::FieldType::Float ? 0.1f : 1.0f);
                    float cur = e.value.empty() ? 0.0f : std::stof(e.value);
                    if (minus_hov) cur -= step;
                    if (plus_hov)  cur += step;
                    cur = std::max(cur, fs.minVal);
                    if (minus_hov || plus_hov) {
                        if (fs.type == Config::FieldType::Int) {
                            e.value = std::to_string((int)cur);
                        } else {
                            char buf[32]; snprintf(buf, sizeof(buf), "%.2f", cur);
                            e.value = buf;
                        }
                    }
                }
            }
        }

        // keyboard input for active fields
        if (e.active) {
            // Ctrl+V paste,  use wl-paste on Wayland, fallback to GetClipboardText
            if ((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) && IsKeyPressed(KEY_V)) {
                std::string clip;
                FILE* pipe = popen("wl-paste --no-newline 2>/dev/null", "r");
                if (pipe) {
                    char buf[4096];
                    while (fgets(buf, sizeof(buf), pipe))
                        clip += buf;
                    pclose(pipe);
                }
                if (clip.empty()) {
                    const char* c = GetClipboardText();
                    if (c) clip = c;
                }
                for (char c : clip) {
                    bool allowed =
                        (fs.type == Config::FieldType::String && c >= 32 && c <= 126) ||
                        (fs.type == Config::FieldType::Int    && ((c >= '0' && c <= '9') || c == '-')) ||
                        (fs.type == Config::FieldType::Float  && ((c >= '0' && c <= '9') || c == '.' || c == '-'));
                    if (allowed) e.value += c;
                }
            }

            int key = GetCharPressed();
            while (key > 0) {
                bool allowed =
                    (fs.type == Config::FieldType::String && key >= 32 && key <= 126) ||
                    (fs.type == Config::FieldType::Int   && ((key >= '0' && key <= '9') || key == '-')) ||
                    (fs.type == Config::FieldType::Float && ((key >= '0' && key <= '9') || key == '.' || key == '-'));
                if (allowed) e.value += (char)key;
                key = GetCharPressed();
            }
            if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE))
                if (!e.value.empty()) e.value.pop_back();
        }

        float text_x = (fs.type == Config::FieldType::Int || fs.type == Config::FieldType::Float)
                       ? x + 36.0f : x + 8.0f;
        Render::Utils::Font::drawText(e.value.c_str(), (int)text_x, (int)(y + 8),
            Config::Constants::FontSize::LABEL, Config::Constants::Theme::INPUT_TEXT);

        // cursor blink
        if (e.active && (int)(GetTime() * 2) % 2 == 0) {
            Vector2 tw = MeasureTextEx(Render::Utils::Font::activeFont, e.value.c_str(),
                                       Config::Constants::FontSize::LABEL, 1.6f);
            DrawLine((int)(text_x + tw.x + 2), (int)(y + 5),
                     (int)(text_x + tw.x + 2), (int)(y + box_h - 5),
                     Config::Constants::Theme::INPUT_CURSOR);
        }
    }

    struct DrawResult {
        bool stayOpen     = true;
        bool needsReindex = false;
        bool saved         = false;
    };

    inline DrawResult draw() {
        int   sw = GetScreenWidth();
        int   sh = GetScreenHeight();
        float group_w  = 200.0f;
        float header_h = 60.0f;
        float padding  = 12.0f;
        float row_h    = 44.0f;
        float section_h = 34.0f;
        float box_h    = 32.0f;
        float label_w  = 240.0f;
        float field_x  = group_w + padding * 2.0f;
        float field_w  = sw - field_x - padding;

        bool   mouse_clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        Vector2 mouse        = GetMousePosition();

        // activate the clicked field BEFORE drawing (fixes the consumed-event bug)
        if (mouse_clicked) {
            float y = header_h + padding + sFieldScroll;
            bool activated = false;
            for (auto& e : sFields) {
                if (!e.schema) { y += section_h; continue; }
                Rectangle ctrl_rect = {field_x + label_w, y, field_w - label_w - padding, box_h};
                if (CheckCollisionPointRec(mouse, ctrl_rect)) {
                    for (auto& o : sFields) o.active = false;
                    e.active  = true;
                    activated = true;
                    break;
                }
                y += row_h;
            }
            if (!activated) {
                // clicking outside any field deactivates all
                bool in_field_area = CheckCollisionPointRec(mouse, {field_x, header_h, field_w, (float)sh - header_h});
                if (in_field_area)
                    for (auto& o : sFields) o.active = false;
            }
        }

        // render
        BeginDrawing();
        ClearBackground(Config::Constants::Theme::BG);

        Render::Utils::Font::drawText("Settings", (int)padding, (int)(header_h / 2 - 14),
            Config::Constants::FontSize::PAGE_TITLE, Config::Constants::Theme::TEXT_PRIMARY);
        DrawLine(0, (int)header_h, sw, (int)header_h, Config::Constants::Theme::BORDER_DIVIDER);
        DrawLine((int)group_w, (int)header_h, (int)group_w, sh, Config::Constants::Theme::BORDER_DIVIDER);

        // --- group list (left panel) ---
        float groups_area_h    = sh - header_h;
        float groups_content_h = (int)Config::groups.size() * row_h;
        if (groups_content_h > groups_area_h)
            sGroupScroll = std::clamp(sGroupScroll + GetMouseWheelMove() * row_h,
                                      -(groups_content_h - groups_area_h), 0.0f);

        BeginScissorMode(0, (int)header_h, (int)group_w, (int)groups_area_h);
        for (int i = 0; i < (int)Config::groups.size(); i++) {
            float y    = header_h + i * row_h + sGroupScroll;
            Rectangle r = {0, y, group_w, row_h};
            bool hov = CheckCollisionPointRec(mouse, r);
            bool sel = i == sSelectedGroup;
            DrawRectangleRec(r, sel  ? Config::Constants::Theme::ITEM_BG_SELECTED
                              : hov ? Config::Constants::Theme::ITEM_BG_HOVER
                                    : Color{0, 0, 0, 0});
            Render::Utils::Font::drawText(Config::groups[i].label.c_str(),
                (int)padding, (int)(y + row_h / 2.0f - 9),
                Config::Constants::FontSize::BODY,
                sel ? Config::Constants::Theme::ITEM_TEXT_SELECTED
                    : Config::Constants::Theme::TEXT_SECONDARY);
            if (mouse_clicked && hov && !sel) {
                sSelectedGroup = i;
                loadGroup(i);
            }
        }
        EndScissorMode();

        // --- field list (right panel) ---
        float fieldsAreaH    = sh - header_h - 60.0f;
        float fieldsContentH = 0;
        for (auto& e : sFields)
            fieldsContentH += e.schema ? row_h : section_h;

        if (fieldsContentH > fieldsAreaH) {
            Rectangle fieldArea = {field_x, header_h, field_w, fieldsAreaH + 60.0f};
            if (CheckCollisionPointRec(mouse, fieldArea))
                sFieldScroll = std::clamp(sFieldScroll + GetMouseWheelMove() * row_h,
                                          -(fieldsContentH - fieldsAreaH), 0.0f);
        }

        BeginScissorMode((int)field_x, (int)header_h, (int)field_w, (int)fieldsAreaH);
        float y = header_h + padding + sFieldScroll;
        for (auto& e : sFields) {
            if (!e.schema) {
                // section header
                Render::Utils::Font::drawText(e.sectionLabel.c_str(),
                    (int)(field_x + padding), (int)(y + 6),
                    Config::Constants::FontSize::SECTION_HEADER,
                    Config::Constants::Theme::SECTION_TEXT);
                DrawLine((int)(field_x + padding), (int)(y + section_h - 6),
                         (int)(field_x + field_w - padding), (int)(y + section_h - 6),
                         Config::Constants::Theme::BORDER_SECTION);
                y += section_h;
            } else {
                Render::Utils::Font::drawText(e.schema->label.c_str(),
                    (int)(field_x + padding), (int)(y + 7),
                    Config::Constants::FontSize::LABEL,
                    Config::Constants::Theme::TEXT_SECONDARY);
                float ctrl_x = field_x + label_w;
                float ctrl_w = field_w - label_w - padding;
                drawFieldControl(e, ctrl_x, y, ctrl_w, box_h, mouse_clicked, mouse);
                y += row_h;
            }
        }
        EndScissorMode();

        // --- bottom buttons ---
        float btn_y = (float)sh - 50.0f;
        float btn_w = 110.0f;
        float btn_h = 36.0f;
        Rectangle saveBtn = {field_x + padding,                  btn_y, btn_w, btn_h};
        Rectangle backBtn = {field_x + padding + btn_w + 12.0f,  btn_y, btn_w, btn_h};
        bool saveHov = CheckCollisionPointRec(mouse, saveBtn);
        bool backHov = CheckCollisionPointRec(mouse, backBtn);

        DrawRectangleRec(saveBtn, saveHov
            ? Config::Constants::Theme::BTN_PRIMARY_BG_HOVER
            : Config::Constants::Theme::BTN_PRIMARY_BG);
        Render::Utils::Font::drawText("Save",
            (int)(saveBtn.x + 28), (int)(saveBtn.y + 9),
            Config::Constants::FontSize::BTN, Config::Constants::Theme::BTN_PRIMARY_TEXT);

        DrawRectangleRec(backBtn, backHov
            ? Config::Constants::Theme::BTN_BG_HOVER
            : Config::Constants::Theme::BTN_BG);
        Render::Utils::Font::drawText("Back",
            (int)(backBtn.x + 28), (int)(backBtn.y + 9),
            Config::Constants::FontSize::BTN, Config::Constants::Theme::BTN_TEXT);

        if (mouse_clicked && saveHov) {
            std::string old_songs_dir = Config::get<std::string>("general.songs_dir");
            for (auto& e : sFields) {
                if (!e.schema) continue;
                const auto& fs = *e.schema;
                if (fs.type == Config::FieldType::Bool)
                    Config::set(fs.key, e.value == "true");
                else if (fs.type == Config::FieldType::Int)
                    Config::set(fs.key, e.value.empty() ? 0 : std::stoi(e.value));
                else if (fs.type == Config::FieldType::Float)
                    Config::set(fs.key, e.value.empty() ? 0.0f : std::stof(e.value));
                else
                    Config::set(fs.key, e.value);
            }
            Config::save();
            bool songs_dir_changed = Config::get<std::string>("general.songs_dir") != old_songs_dir;
            if (songs_dir_changed) {
                Render::Utils::Toast::draw();
                Render::Debug::draw();
                EndDrawing();
                return {false, true, true};
            }
            Render::Utils::Toast::draw();
            Render::Debug::draw();
            EndDrawing();
            return {true, false, true};
        }

        Render::Utils::Toast::draw();
        Render::Debug::draw();
        EndDrawing();

        if (mouse_clicked && backHov)
            return {false, false};
        return {true, false};
    }
}