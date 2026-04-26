#pragma once
#include <includes/json.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace Config {

    enum class FieldType { String, Int, Float, Bool };

    struct FieldSchema {
        std::string key;
        std::string label;
        FieldType   type;
        std::string section = "";
        float       step    = 1.0f;
        float       minVal  = 0.0f;
        float       maxVal  = 0.0f;
        bool        isPath  = false;
    };

    struct GroupSchema {
        std::string key;
        std::string label;
    };

    inline std::vector<GroupSchema> groups = {
        {"general", "General"},
        {"audio", "Audio"},
        {"indexer", "Indexer"},
        {"render",  "Render"},
        {"hud",     "HUD"},
        {"player",  "Player"},
        {"objects", "Objects"},
        {"api",     "API"},
        {"debug",   "Debug"},
    };

    inline std::vector<FieldSchema> fields = {
        // general
        {"general.songs_dir",   "Songs Directory",  FieldType::String, "", 1, 0, 0, true},
        {"general.skins_dir",   "Skins Directory",  FieldType::String, "", 1, 0, 0, true},
        {"general.replay_dir",  "Replay Directory", FieldType::String, "", 1, 0, 0, true},
        {"general.output_dir",  "Output Directory", FieldType::String, "", 1, 0, 0, true},
        //  audio
        {"audio.music_volume",    "Music Volume",    FieldType::Float, "Audio", 0.01f, 0.0f, 1.0f},
        {"audio.hitsound_volume", "Hitsound Volume", FieldType::Float, "Audio", 0.01f, 0.0f, 1.0f},
        // indexer
        {"indexer.hash_threads", "Hash Threads", FieldType::Int, "", 1, 1, 64},
        // render -> replay
        {"render.replay.vsync",         "VSync",         FieldType::Bool, "Replay"},
        {"render.replay.fps_unlimited", "Unlimited FPS", FieldType::Bool, "Replay"},
        {"render.replay.fps_limit",     "FPS Limit",     FieldType::Int,  "Replay", 10, 30, 1000},
        // hud
        {"hud.hud_text_size",   "FPS Text Size",   FieldType::Float, "Interface", 0.05f, 0.2, 4},
        {"hud.fps_text_size",   "FPS Text Size",   FieldType::Float, "Interface", 0.05f, 0.2, 4},
        {"hud.info_text_size",  "Info Text Size",  FieldType::Int,   "Interface", 1, 8, 72},
        // player
        {"player.cursor_size", "Cursor Size", FieldType::Float, "Player", 0.01f, 0.1f, 5.0f},
        // objects
        {"objects.hitcircle.show",                 "Show Hit Circles",       FieldType::Bool,  "Hit Circle"},
        {"objects.slider.show",                    "Show Sliders",           FieldType::Bool,  "Slider"},
        {"objects.slider.texture_pre_calculation", "Pre-calculate Textures", FieldType::Bool,  "Slider"},
        {"objects.spinner.show",                   "Show Spinners",          FieldType::Bool,  "Spinner"},
        {"objects.playfield.show",                 "Show Playfield",         FieldType::Bool,  "Playfield"},
        {"objects.playfield.opacity",              "Opacity",                FieldType::Float, "Playfield", 0.1f, 0.0f, 1.0f},
        {"objects.playfield.step",                 "Step Size",              FieldType::Int,   "Playfield", 8, 1, 448},
        // api
        {"api.client_id",     "Client ID",     FieldType::String},
        {"api.client_secret", "Client Secret", FieldType::String},
        // debug
        {"debug.text_size", "Debug Text Size", FieldType::Int, "Interface", 1, 8, 72}
    };

    inline nlohmann::ordered_json configData;
    inline std::filesystem::path configPath;

    inline nlohmann::ordered_json defaults = {
        {"general", {
            {"songs_dir",  ""},
            {"skins_dir",  ""},
            {"replay_dir", ""},
            {"output_dir", ""},
        }},
        {"audio", {
            {"music_volume",    0.2f},
            {"hitsound_volume", 0.4f}
        }},
        {"indexer", {
            {"hash_threads", 16}
        }},
        {"render", {
           {"replay", {
                {"vsync",         false},
                {"fps_unlimited", false},
                {"fps_limit",     240}
            }}
        }},
        {"hud", {
            {"hud_text_size",  1.0f},
            {"fps_text_size",  1.0f},
            {"info_text_size", 1.0f}
        }},
        {"player", {
            {"cursor_size", 1.0f}
        }},
        {"objects", {
            {"hitcircle", {
                {"show", true}
            }},
            {"slider", {
                {"show", true},
                {"texture_pre_calculation", true}
            }},
            {"spinner", {
                {"show", true}
            }},
            {"playfield", {
                {"show",    true},
                {"opacity", 0.5f},
                {"step",    64}
            }}
        }},
        {"api", {
            {"client_id",     ""},
            {"client_secret", ""}
        }},
        {"debug", {
            {"text_size", 20}
        }}
    };

    inline void setPath(const std::filesystem::path& path) {
        configPath = path;
    }

    inline void load() {
        configData = defaults;
        if (!std::filesystem::exists(configPath)) {
            return;
        }
        if (std::ifstream fileStream(configPath); fileStream) {
            nlohmann::json jsonData;
            fileStream >> jsonData;
            configData.merge_patch(jsonData);
        }
    }

    inline void save() {
        std::ofstream fileStream(configPath);
        fileStream << configData.dump(4);
    }

    inline std::vector<std::string> splitKey(const std::string& key) {
        std::vector<std::string> parts;
        size_t start = 0, end;
        while ((end = key.find('.', start)) != std::string::npos) {
            parts.push_back(key.substr(start, end - start));
            start = end + 1;
        }
        parts.push_back(key.substr(start));
        return parts;
    }

    inline nlohmann::ordered_json* traverseToParent(const std::vector<std::string>& parts, bool create) {
        nlohmann::ordered_json* node = &configData;
        for (size_t i = 0; i + 1 < parts.size(); i++) {
            if (!node->contains(parts[i])) {
                if (!create)
                    return nullptr;
                (*node)[parts[i]] = nlohmann::ordered_json::object();
            }
            node = &(*node)[parts[i]];
        }
        return node;
    }

    template<typename Type>
    inline Type get(const std::string& key, const Type& defaultVal) {
        auto parts = splitKey(key);
        nlohmann::ordered_json* node = traverseToParent(parts, false);
        if (!node || !node->contains(parts.back()))
            return defaultVal;
        return (*node)[parts.back()].get<Type>();
    }

    template<typename Type>
    inline Type get(const std::string& key) {
        auto parts = splitKey(key);
        nlohmann::ordered_json* node = traverseToParent(parts, false);
        if (node && node->contains(parts.back()))
            return (*node)[parts.back()].get<Type>();
        nlohmann::ordered_json* d = &defaults;
        for (size_t i = 0; i + 1 < parts.size(); i++) {
            if (!d->contains(parts[i]))
                return Type{};
            d = &(*d)[parts[i]];
        }
        if (!d->contains(parts.back()))
            return Type{};
        return (*d)[parts.back()].get<Type>();
    }

    template<typename Type>
    inline void set(const std::string& key, const Type& value) {
        auto parts = splitKey(key);
        nlohmann::ordered_json* node = traverseToParent(parts, true);
        (*node)[parts.back()] = value;
    }
}