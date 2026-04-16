#include <raylib.h>
#include "Render/render.h"
#include "Processor/Indexer.h"
#include "Resources/resources.h"

#define DB_NAME "beatmaps.db"
#define SONGS_DIR "/home/railgun/osu!/osugame/Songs"
#define HASH_THREADS 4

int main(int argc, char* argv[])
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1280, 720, "osu-replay-render");
    SetTargetFPS(9999999);
    InitAudioDevice();

    Font font = LoadFontFromMemory(".ttf",
        Orr::Resources::Fonts::DroidSans_ttf,
        Orr::Resources::Fonts::DroidSans_ttf_len,
        72, nullptr, 0);
    SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    Orr::Render::SetActiveFont(font);

    Orr::Database::BeatmapDb bdb(DB_NAME);
    Orr::Processor::Indexer indexer(bdb, SONGS_DIR, HASH_THREADS);

    std::string pending_osr;
    bool replay_active = false;

    if (argc > 1)
        pending_osr = argv[1];
    else
        pending_osr = "/home/railgun/Downloads/solo-replay-osu_1257904_477724787.osr";

    while (!WindowShouldClose())
    {
        if (!indexer.IsDone()) {
            Orr::Render::IndexStatus::DrawBuildStatus(indexer.GetStatus());
            continue;
        }

        if (!replay_active && !pending_osr.empty()) {
            Orr::Render::ReplayRender::Init(pending_osr, bdb);
            replay_active = true;
        }

        if (!replay_active) {
            std::string dropped = Orr::Render::MainMenu::Draw();
            if (!dropped.empty()) {
                pending_osr = dropped;
                Orr::Render::ReplayRender::Init(pending_osr, bdb);
                replay_active = true;
            }
        } else {
            if (IsKeyPressed(KEY_ESCAPE)) {
                Orr::Render::ReplayRender::Unload();
                replay_active = false;
            } else {
                Orr::Render::ReplayRender::Draw(bdb);
            }
        }
    }

    if (replay_active)
        Orr::Render::ReplayRender::Unload();

    UnloadFont(font);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}