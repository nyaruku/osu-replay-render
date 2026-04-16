#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>

// Format Documentation
// https://osu.ppy.sh/wiki/en/Client/File_formats/osu_(file_format)

namespace Orr::Reader::Beatmap {

    struct HitSample {
        int normal_set = 0;
        int addition_set = 0;
        int index = 0;
        int volume = 0;
        std::string filename;
    };

    struct EdgeSet {
        int normal_set = 0;
        int addition_set = 0;
    };

    struct CurvePoint {
        int x, y;
    };

    struct HitObject {
        bool is_slider;
        int x;
        int y;
        int time;
        int end_time;
        int type;
        int hit_sound;
        HitSample hit_sample;
        char curve_type;
        std::vector<CurvePoint> curve_points;
        int slides;
        double length;
        std::vector<int> edge_sounds;
        std::vector<EdgeSet> edge_sets;
    };

    struct TimingPoint {
        int time;
        // true = timing point, false = inherited: sv point
        double beat_length;
        int meter;
        int sample_set;
        int sample_index;
        int volume;
        bool uninherited;
        int effects;
    };

    struct Beatmap {
        std::string audio_filename;
        int audio_lead_in = 0;
        int preview_time = -1;
        int mode = 0;
        std::string title;
        std::string title_unicode;
        std::string artist;
        std::string artist_unicode;
        std::string creator;
        std::string version;
        std::string source;
        int beatmap_id = 0;
        int beatmap_set_id = 0;
        float hp_drain_rate = 5.0f;
        float circle_size = 5.0f;
        float overall_difficulty = 5.0f;
        float approach_rate = 5.0f;
        float slider_multiplier = 1.4f;
        float slider_tick_rate = 1.0f;
        std::vector<TimingPoint> timing_points;
        std::vector<HitObject> hit_objects;
    };

    namespace helper {

        inline std::string Trim(const std::string& s) {
            const char* ws = " \t\r\n";
            size_t start = s.find_first_not_of(ws);
            if (start == std::string::npos) {
                return {};
            }
            size_t end = s.find_last_not_of(ws);
            return s.substr(start, end - start + 1);
        }

        inline std::vector<std::string> Split(const std::string& s, char delim) {
            std::vector<std::string> out;
            std::stringstream ss(s);
            std::string tok;
            while (std::getline(ss, tok, delim)) {
                out.push_back(tok);
            }
            return out;
        }

        inline HitSample ParseHitSample(const std::string& s) {
            HitSample hs{};
            if (s.empty()) {
                return hs;
            }
            auto parts = Split(s, ':');
            if (parts.size() > 0 && !parts[0].empty()) { hs.normal_set = std::stoi(parts[0]); }
            if (parts.size() > 1 && !parts[1].empty()) { hs.addition_set = std::stoi(parts[1]); }
            if (parts.size() > 2 && !parts[2].empty()) { hs.index = std::stoi(parts[2]); }
            if (parts.size() > 3 && !parts[3].empty()) { hs.volume = std::stoi(parts[3]); }
            if (parts.size() > 4) { hs.filename = parts[4]; }
            return hs;
        }

        inline void ParseHitObject(const std::string& line, Beatmap& bm) {
            auto parts = Split(line, ',');
            if (parts.size() < 5) {
                return;
            }

            const int type_bits  = std::stoi(parts[3]);
            const bool is_circle  = (type_bits & 1)   != 0;
            const bool is_slider  = (type_bits & 2)   != 0;
            const bool is_spinner = (type_bits & 8)   != 0;
            const bool is_hold    = (type_bits & 128) != 0;

            if (is_hold || (!is_circle && !is_slider && !is_spinner)) {
                return;
            }

            HitObject obj{};
            obj.is_slider = is_slider;
            obj.x = std::stoi(parts[0]);
            obj.y = std::stoi(parts[1]);
            obj.time = std::stoi(parts[2]);
            obj.end_time = obj.time;
            obj.type = type_bits;
            obj.hit_sound = std::stoi(parts[4]);
            obj.curve_type = 'B';
            obj.slides = 0;
            obj.length = 0.0;

            if (is_slider && parts.size() >= 8) {
                const std::string& curve_str = parts[5];
                auto pipe = curve_str.find('|');
                if (!curve_str.empty()) {
                    obj.curve_type = curve_str[0];
                }
                if (pipe != std::string::npos) {
                    auto point_tokens = Split(curve_str.substr(pipe + 1), '|');
                    for (auto& pt : point_tokens) {
                        auto coords = Split(pt, ':');
                        if (coords.size() == 2) {
                            obj.curve_points.push_back({ std::stoi(coords[0]), std::stoi(coords[1]) });
                        }
                    }
                }
                obj.slides = std::stoi(parts[6]);
                obj.length = std::stod(parts[7]);
                if (parts.size() > 8) {
                    for (auto& es : Split(parts[8], '|')) {
                        if (!es.empty()) {
                            obj.edge_sounds.push_back(std::stoi(es));
                        }
                    }
                }
                if (parts.size() > 9) {
                    for (auto& es_str : Split(parts[9], '|')) {
                        auto es_parts = Split(es_str, ':');
                        EdgeSet es{};
                        if (es_parts.size() > 0 && !es_parts[0].empty()) { es.normal_set = std::stoi(es_parts[0]); }
                        if (es_parts.size() > 1 && !es_parts[1].empty()) { es.addition_set = std::stoi(es_parts[1]); }
                        obj.edge_sets.push_back(es);
                    }
                }
                if (parts.size() > 10) {
                    obj.hit_sample = ParseHitSample(parts[10]);
                }
            } else if (is_spinner && parts.size() > 5) {
                obj.end_time = std::stoi(parts[5]);
                if (parts.size() > 6) {
                    obj.hit_sample = ParseHitSample(parts[6]);
                }
            } else if (is_circle && parts.size() > 5) {
                obj.hit_sample = ParseHitSample(parts[5]);
            }

            bm.hit_objects.push_back(std::move(obj));
        }

        inline void ParseTimingPoint(const std::string& line, Beatmap& bm) {
            auto parts = Split(line, ',');
            if (parts.size() < 2) {
                return;
            }
            TimingPoint tp{};
            tp.time = static_cast<int>(std::stod(parts[0]));
            tp.beat_length = std::stod(parts[1]);
            tp.meter = parts.size() > 2 ? std::stoi(parts[2]) : 4;
            tp.sample_set = parts.size() > 3 ? std::stoi(parts[3]) : 0;
            tp.sample_index = parts.size() > 4 ? std::stoi(parts[4]) : 0;
            tp.volume = parts.size() > 5 ? std::stoi(parts[5]) : 100;
            tp.uninherited = parts.size() > 6 ? (Trim(parts[6]) == "1") : true;
            tp.effects = parts.size() > 7 ? std::stoi(parts[7]) : 0;
            bm.timing_points.push_back(tp);
        }

    }

    inline Beatmap ReadOsu(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("orr::ReadOsu — cannot open file: " + filePath);
        }

        Beatmap bm;
        std::string line;
        std::string section;

        while (std::getline(file, line)) {
            line = helper::Trim(line);

            if (line.empty() || line.rfind("//", 0) == 0) {
                continue;
            }

            if (line.front() == '[' && line.back() == ']') {
                section = line.substr(1, line.size() - 2);
                continue;
            }

            if (section == "General" || section == "Metadata" || section == "Difficulty") {
                size_t colon = line.find(':');
                if (colon == std::string::npos) {
                    continue;
                }
                std::string key = helper::Trim(line.substr(0, colon));
                std::string value = helper::Trim(line.substr(colon + 1));

                if (section == "General") {
                    if (key == "AudioFilename") {
                        bm.audio_filename = value;
                    } else if (key == "AudioLeadIn") {
                        bm.audio_lead_in = std::stoi(value);
                    } else if (key == "PreviewTime") {
                        bm.preview_time = std::stoi(value);
                    } else if (key == "Mode") {
                        bm.mode = std::stoi(value);
                    }
                } else if (section == "Metadata") {
                    if (key == "Title") {
                        bm.title = value;
                    } else if (key == "TitleUnicode") {
                        bm.title_unicode = value;
                    } else if (key == "Artist") {
                        bm.artist = value;
                    } else if (key == "ArtistUnicode") {
                        bm.artist_unicode = value;
                    } else if (key == "Creator") {
                        bm.creator = value;
                    } else if (key == "Version") {
                        bm.version = value;
                    } else if (key == "Source") {
                        bm.source = value;
                    } else if (key == "BeatmapID") {
                        bm.beatmap_id = std::stoi(value);
                    } else if (key == "BeatmapSetID") {
                        bm.beatmap_set_id = std::stoi(value);
                    }
                } else if (section == "Difficulty") {
                    if (key == "HPDrainRate") {
                        bm.hp_drain_rate = std::stof(value);
                    } else if (key == "CircleSize") {
                        bm.circle_size = std::stof(value);
                    } else if (key == "OverallDifficulty") {
                        bm.overall_difficulty = std::stof(value);
                    } else if (key == "ApproachRate") {
                        bm.approach_rate = std::stof(value);
                    } else if (key == "SliderMultiplier") {
                        bm.slider_multiplier = std::stof(value);
                    } else if (key == "SliderTickRate") {
                        bm.slider_tick_rate = std::stof(value);
                    }
                }
                continue;
            }

            if (section == "TimingPoints") {
                helper::ParseTimingPoint(line, bm);
                continue;
            }

            if (section == "HitObjects") {
                helper::ParseHitObject(line, bm);
                continue;
            }
        }
        return bm;
    }
}