#include <raylib.h>
#include <cmath>
#include <ctime>
#include <chrono>
#include <filesystem>
#include <memory>
#include <render/render.h>
#include <utility/screenshot.h>
#include <processor/indexer.h>
#include <resources/resources.h>
#include <config/config.h>
#include <cli/cli.h>

#define SUPPORT_SCREEN_CAPTURE 0

int main(int argc, char* argv[])
{
    std::filesystem::path runtimePath = std::filesystem::path(argv[0]).parent_path();

    Config::setPath(runtimePath / "config.json");
    Config::load();

    Cli::Result cli = Cli::handle(argc, argv, runtimePath);
    if (cli.action == Cli::Action::Exit)
        return cli.exitCode;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "osu-replay-render");
    SetWindowState(FLAG_VSYNC_HINT);
    SetExitKey(KEY_NULL);
    InitAudioDevice();

    // Font + theme setup
    ::Font font = LoadFontFromMemory(".ttf", Resources::Fonts::DroidSans_ttf, Resources::Fonts::DROID_SANS_TTF_LEN, 72, nullptr, 0);
    SetTextureFilter(font.texture, TEXTURE_FILTER_TRILINEAR);
    Render::Utils::Font::setActiveFont(font);
    Config::Constants::applyThemeToRaygui(font);

    std::string dbPath = (runtimePath / "beatmaps.db").string();
    Database::BeatmapDb beatmapDb(dbPath);
    auto startIndexer = [&]() {
        return std::make_unique<Processor::Indexer>(
            beatmapDb,
            Config::get<std::string>("general.songs_dir"),
            Config::get<int>("indexer.hash_threads")
        );
    };

    auto indexer = startIndexer();
    ClearWindowState(FLAG_VSYNC_HINT);
    SetTargetFPS(0);

    Utility::Screenshot::initScreenshotUtility(runtimePath);
    std::string pendingOsr;
    bool replayActive  = false;
    bool settingsOpen  = false;

    while (!WindowShouldClose())
    {
        if (IsKeyPressed(KEY_F11)) {
            Render::Debug::toggle();
        }

        if (IsKeyPressed(KEY_F12)) {
            Utility::Screenshot::takeScreenshot();
        }
        if (!indexer->isDone()) {
            Render::IndexStatus::drawBuildStatus(indexer->getStatus());
            continue;
        }

        {
            auto s = indexer->getStatus();
            if (!s.error.empty()) {
                Render::IndexStatus::drawBuildStatus(s);
                if (IsKeyPressed(KEY_ESCAPE)) {
                    Render::Settings::open();
                    settingsOpen = true;
                    indexer = std::make_unique<Processor::Indexer>(beatmapDb, "", 0); // idle
                }
                continue;
            }
        }

        if (!replayActive && !pendingOsr.empty()) {
            Render::Replay::init(pendingOsr, beatmapDb);
            Render::Replay::precacheSliderTextures();

            replayActive = true;

            // apply replay specific settings
            if (Config::get<bool>("render.replay.vsync")) {
                SetWindowState(FLAG_VSYNC_HINT);
                SetTargetFPS(0);
            } else if (Config::get<bool>("render.replay.fps_unlimited")) {
                ClearWindowState(FLAG_VSYNC_HINT);
                SetTargetFPS(0);
            } else {
                ClearWindowState(FLAG_VSYNC_HINT);
                SetTargetFPS(Config::get<int>("render.replay.fps_limit"));
            }
        }

        if (replayActive) {
            if (IsKeyPressed(KEY_ESCAPE)) {
                Render::Replay::unload();
                replayActive = false;
                pendingOsr.clear();
                // Restore vsync for menus
                SetWindowState(FLAG_VSYNC_HINT);
                SetTargetFPS(0);
            } else {
                Render::Replay::draw(beatmapDb);
            }
            continue;
        }

        if (settingsOpen) {
            SetWindowState(FLAG_VSYNC_HINT);
            auto result = Render::Settings::draw();
            settingsOpen = result.stayOpen;
            if (result.saved)
                Render::Utils::Toast::push("Settings saved", Render::Utils::Toast::Level::Success);
            if (result.needsReindex) {
                Render::Utils::Toast::push("Songs directory changed — reindexing...", Render::Utils::Toast::Level::Info, 4.0f);
                indexer = startIndexer();
                ClearWindowState(FLAG_VSYNC_HINT);
                SetTargetFPS(0);
            }
            continue;
        }

        SetWindowState(FLAG_VSYNC_HINT);
        Render::MainMenu::Result result = Render::MainMenu::draw();
        if (result.action == Render::MainMenu::Action::OpenReplay && !result.osrPath.empty()) {
            pendingOsr = result.osrPath;
        } else if (result.action == Render::MainMenu::Action::OpenSettings) {
            Render::Settings::open();
            settingsOpen = true;
        }
    }

    if (replayActive) {
        Render::Replay::unload();
    }

    UnloadFont(font);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}