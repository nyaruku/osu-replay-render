#pragma once
#include <cstdio>
#include <string>
#include <filesystem>
#include <raylib.h>
#include <database/database.h>
#include <render/views/replay.h>
#include <config/config.h>

namespace Cli {

    enum class Action { RunGui, Exit };
    struct Result { Action action; int exitCode = 0; };

    inline void printHelp(const char* prog) {
        printf("Usage: %s [options]\n", prog);
        printf("\n");
        printf("Options:\n");
        printf("--help [Show this help message and exit]\n");
        printf("--score-check <osr> [Validate a replay (.osr) file]\n");
        printf("\n");
        printf("Without arguments, the GUI is launched.\n");
    }

    inline Result scoreCheck(const std::string& osrPath, const std::filesystem::path& runtimePath) {
        std::string dbPath = (runtimePath / "beatmaps.db").string();
        Database::BeatmapDb beatmapDb(dbPath);

        SetTraceLogLevel(TraceLogLevel::LOG_NONE);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(1, 1, "");
        InitAudioDevice();

        Render::Replay::init(osrPath, beatmapDb);
        bool hasError = !Render::Replay::gameplayState.error.empty();
        if (hasError)
            fprintf(stderr, "ERROR: %s\n", Render::Replay::gameplayState.error.c_str());

        CloseAudioDevice();
        CloseWindow();
        return {Action::Exit, hasError ? 1 : 0};
    }

    inline Result handle(int argc, char* argv[], const std::filesystem::path& runtimePath) {
        if (argc >= 2) {
            std::string arg1 = argv[1];

            if (arg1 == "--help" || arg1 == "-h") {
                printHelp(argv[0]);
                return {Action::Exit, 0};
            }

            if (arg1 == "--score-check") {
                if (argc < 3) {
                    fprintf(stderr, "Usage: %s --score-check <osr>\n", argv[0]);
                    return {Action::Exit, 1};
                }
                return scoreCheck(argv[2], runtimePath);
            }

            fprintf(stderr, "Unknown option: %s\n", arg1.c_str());
            fprintf(stderr, "Run '%s --help' for usage.\n", argv[0]);
            return {Action::Exit, 1};
        }

        return {Action::RunGui, 0};
    }

}

