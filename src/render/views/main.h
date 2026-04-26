#pragma once
#include <config/constants.h>
#include <config/config.h>
#include <raylib.h>
#include <string>
#include <render/utils/font.h>
#include <render/utils/toast.h>
#include <render/views/debug.h>

#define RAYGUI_IMPLEMENTATION
#include <raygui.h>
#undef RAYGUI_IMPLEMENTATION

#define GUI_WINDOW_FILE_DIALOG_IMPLEMENTATION
#include <gui_window_file_dialog.h>
#undef GUI_WINDOW_FILE_DIALOG_IMPLEMENTATION

namespace Render::MainMenu {

    enum class Action { None, OpenReplay, OpenSettings };

    struct Result {
        Action action;
        std::string osrPath;
    };

    inline GuiWindowFileDialogState fileDialogState = {0};
    inline bool fileDialogOpen = false;

    inline Result draw() {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        if (IsFileDropped()) {
            FilePathList dropped = LoadDroppedFiles();
            std::string path;
            if (dropped.count > 0)
                path = dropped.paths[0];
            UnloadDroppedFiles(dropped);
            if (!path.empty() && path.size() > 4 && path.substr(path.size() - 4) == ".osr")
                return {Action::OpenReplay, path};
        }

        float btn_w = 300.0f;
        float btn_h = 55.0f;
        float center_x = (sw - btn_w) / 2.0f;

        Rectangle replayBtn   = {center_x, sh * 0.45f, btn_w, btn_h};
        Rectangle settingsBtn = {center_x, sh * 0.45f + btn_h + 16.0f, btn_w, btn_h};

        Vector2 mouse = GetMousePosition();
        bool replayHov   = !fileDialogOpen && CheckCollisionPointRec(mouse, replayBtn);
        bool settingsHov = !fileDialogOpen && CheckCollisionPointRec(mouse, settingsBtn);

        bool replayClicked   = replayHov   && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
        bool settingsClicked = settingsHov && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

        BeginDrawing();
        ClearBackground(Config::Constants::Theme::BG);

        const char* title = "osu replay render";
        Vector2 title_dim = MeasureTextEx(Render::Utils::Font::activeFont, title, Config::Constants::FontSize::SCREEN_TITLE, Config::Constants::FontSize::SCREEN_TITLE / 10.0f);
        Utils::Font::drawText(title, (int)((sw - title_dim.x) / 2.0f), (int)(sh * 0.3f), Config::Constants::FontSize::SCREEN_TITLE, Config::Constants::Theme::TEXT_PRIMARY);

        const char* hint = "Drop a .osr file or click below";
        Vector2 hint_dim = MeasureTextEx(Render::Utils::Font::activeFont, hint, Config::Constants::FontSize::BODY, Config::Constants::FontSize::BODY / 10.0f);
        Utils::Font::drawText(hint, (int)((sw - hint_dim.x) / 2.0f), (int)(sh * 0.38f), Config::Constants::FontSize::BODY, Config::Constants::Theme::TEXT_MUTED);

        DrawRectangleRec(replayBtn, replayHov ? Config::Constants::Theme::BTN_PRIMARY_BG_HOVER : Config::Constants::Theme::BTN_BG);
        Vector2 rd = MeasureTextEx(Render::Utils::Font::activeFont, "Open Replay", Config::Constants::FontSize::BTN, 2.0f);
        Utils::Font::drawText("Open Replay", (int)(replayBtn.x + (btn_w - rd.x) / 2.0f), (int)(replayBtn.y + (btn_h - Config::Constants::FontSize::BTN) / 2.0f), Config::Constants::FontSize::BTN, Config::Constants::Theme::BTN_TEXT);

        DrawRectangleRec(settingsBtn, settingsHov ? Config::Constants::Theme::BTN_BG_HOVER : Config::Constants::Theme::BTN_BG);
        Vector2 sd = MeasureTextEx(Render::Utils::Font::activeFont, "Settings", Config::Constants::FontSize::BTN, 2.0f);
        Utils::Font::drawText("Settings", (int)(settingsBtn.x + (btn_w - sd.x) / 2.0f), (int)(settingsBtn.y + (btn_h - Config::Constants::FontSize::BTN) / 2.0f), Config::Constants::FontSize::BTN, Config::Constants::Theme::BTN_TEXT);

        if (fileDialogOpen) {
            GuiWindowFileDialog(&fileDialogState);

            if (fileDialogState.SelectFilePressed) {
                fileDialogState.SelectFilePressed = false;
                fileDialogOpen = false;
                std::string path = std::string(fileDialogState.dirPathText) + "/" + std::string(fileDialogState.fileNameText);
                if (path.size() > 4 && path.substr(path.size() - 4) == ".osr") {
                    EndDrawing();
                    return {Action::OpenReplay, path};
                }
            }
            if (!fileDialogState.windowActive)
                fileDialogOpen = false;
        }

        Render::Utils::Toast::draw();
        Render::Debug::draw();
        EndDrawing();

        if (replayClicked) {
            std::string startDir = Config::get<std::string>("general.replay_dir");
            if (startDir.empty() || !DirectoryExists(startDir.c_str()))
                startDir = GetWorkingDirectory();
            fileDialogState = InitGuiWindowFileDialog(startDir.c_str());
            strcpy(fileDialogState.filterExt, ".osr");
            fileDialogState.windowActive = true;
            fileDialogOpen = true;
            Config::Constants::applyThemeToRaygui(Render::Utils::Font::activeFont);
        }
        if (settingsClicked)
            return {Action::OpenSettings, ""};
        return {Action::None, ""};
    }
}