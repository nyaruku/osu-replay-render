#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>

// Format Documentation
// https://osu.ppy.sh/wiki/en/Client/File_formats/osu_(file_format)

namespace Reader::Beatmap {

    struct HitSample {
        int normalSet = 0;
        int additionSet = 0;
        int index = 0;
        int volume = 0;
        std::string filename;
    };

    struct EdgeSet {
        int normalSet = 0;
        int additionSet = 0;
    };

    struct CurvePoint {
        int x, y;
    };

    struct HitObject {
        bool isSlider;
        int x;
        int y;
        int time;
        int endTime;
        int type;
        int hitSound;
        HitSample hitSample;
        char curveType;
        std::vector<CurvePoint> curvePoints;
        int slides;
        double length;
        std::vector<int> edgeSounds;
        std::vector<EdgeSet> edgeSets;
    };

    struct TimingPoint {
        int time;
        // true = timing point, false = inherited: sv point
        double beatLength;
        int meter;
        int sampleSet;
        int sampleIndex;
        int volume;
        bool uninherited;
        int effects;
    };

    struct Beatmap {
        std::string audioFilename;
        int audioLeadIn = 0;
        int previewTime = -1;
        int mode = 0;
        std::string title;
        std::string titleUnicode;
        std::string artist;
        std::string artistUnicode;
        std::string creator;
        std::string version;
        std::string source;
        int beatmapId = 0;
        int beatmapSetId = 0;
        float hpDrainRate = 5.0f;
        float circleSize = 5.0f;
        float overallDifficulty = 5.0f;
        float approachRate = 5.0f;
        float sliderMultiplier = 1.4f;
        float sliderTickRate = 1.0f;
        std::vector<TimingPoint> timingPoints;
        std::vector<HitObject> hitObjects;
        std::vector<std::pair<int,int>> breakPeriods; // startMs, endMs
    };

    namespace Helper {

        inline std::string trim(const std::string& s) {
            const char* ws = " \t\r\n";
            size_t start = s.find_first_not_of(ws);
            if (start == std::string::npos) {
                return {};
            }
            size_t end = s.find_last_not_of(ws);
            return s.substr(start, end - start + 1);
        }

        inline std::vector<std::string> split(const std::string& s, char delim) {
            std::vector<std::string> out;
            std::stringstream ss(s);
            std::string tok;
            while (std::getline(ss, tok, delim)) {
                out.push_back(tok);
            }
            return out;
        }

        inline HitSample parseHitSample(const std::string& s) {
            HitSample hs{};
            if (s.empty()) {
                return hs;
            }
            auto parts = split(s, ':');
            if (parts.size() > 0 && !parts[0].empty()) { hs.normalSet = std::stoi(parts[0]); }
            if (parts.size() > 1 && !parts[1].empty()) { hs.additionSet = std::stoi(parts[1]); }
            if (parts.size() > 2 && !parts[2].empty()) { hs.index = std::stoi(parts[2]); }
            if (parts.size() > 3 && !parts[3].empty()) { hs.volume = std::stoi(parts[3]); }
            if (parts.size() > 4) { hs.filename = parts[4]; }
            return hs;
        }

        inline void parseHitObject(const std::string& line, Beatmap& bm) {
            auto parts = split(line, ',');
            if (parts.size() < 5) {
                return;
            }

            const int TYPE_BITS  = std::stoi(parts[3]);
            const bool IS_CIRCLE  = (TYPE_BITS & 1)   != 0;
            const bool IS_SLIDER  = (TYPE_BITS & 2)   != 0;
            const bool IS_SPINNER = (TYPE_BITS & 8)   != 0;
            const bool IS_HOLD    = (TYPE_BITS & 128) != 0;

            if (IS_HOLD || (!IS_CIRCLE && !IS_SLIDER && !IS_SPINNER)) {
                return;
            }

            HitObject obj{};
            obj.isSlider = IS_SLIDER;
            obj.x = std::stoi(parts[0]);
            obj.y = std::stoi(parts[1]);
            obj.time = std::stoi(parts[2]);
            obj.endTime = obj.time;
            obj.type = TYPE_BITS;
            obj.hitSound = std::stoi(parts[4]);
            obj.curveType = 'B';
            obj.slides = 0;
            obj.length = 0.0;

            if (IS_SLIDER && parts.size() >= 8) {
                const std::string& curveStr = parts[5];
                auto pipe = curveStr.find('|');
                if (!curveStr.empty()) {
                    obj.curveType = curveStr[0];
                }
                if (pipe != std::string::npos) {
                    auto point_tokens = split(curveStr.substr(pipe + 1), '|');
                    for (auto& pt : point_tokens) {
                        auto coords = split(pt, ':');
                        if (coords.size() == 2) {
                            obj.curvePoints.push_back({ std::stoi(coords[0]), std::stoi(coords[1]) });
                        }
                    }
                }
                obj.slides = std::stoi(parts[6]);
                obj.length = std::stod(parts[7]);
                if (parts.size() > 8) {
                    for (auto& es : split(parts[8], '|')) {
                        if (!es.empty()) {
                            obj.edgeSounds.push_back(std::stoi(es));
                        }
                    }
                }
                if (parts.size() > 9) {
                    for (auto& esStr : split(parts[9], '|')) {
                        auto esParts = split(esStr, ':');
                        EdgeSet es{};
                        if (esParts.size() > 0 && !esParts[0].empty()) { es.normalSet = std::stoi(esParts[0]); }
                        if (esParts.size() > 1 && !esParts[1].empty()) { es.additionSet = std::stoi(esParts[1]); }
                        obj.edgeSets.push_back(es);
                    }
                }
                if (parts.size() > 10) {
                    obj.hitSample = parseHitSample(parts[10]);
                }
            } else if (IS_SPINNER && parts.size() > 5) {
                obj.endTime = std::stoi(parts[5]);
                if (parts.size() > 6) {
                    obj.hitSample = parseHitSample(parts[6]);
                }
            } else if (IS_CIRCLE && parts.size() > 5) {
                obj.hitSample = parseHitSample(parts[5]);
            }

            bm.hitObjects.push_back(std::move(obj));
        }

        inline void parseTimingPoint(const std::string& line, Beatmap& bm) {
            auto parts = split(line, ',');
            if (parts.size() < 2) {
                return;
            }
            TimingPoint tp{};
            tp.time = static_cast<int>(std::stod(parts[0]));
            tp.beatLength = std::stod(parts[1]);
            tp.meter = parts.size() > 2 ? std::stoi(parts[2]) : 4;
            tp.sampleSet = parts.size() > 3 ? std::stoi(parts[3]) : 0;
            tp.sampleIndex = parts.size() > 4 ? std::stoi(parts[4]) : 0;
            tp.volume = parts.size() > 5 ? std::stoi(parts[5]) : 100;
            tp.uninherited = parts.size() > 6 ? (trim(parts[6]) == "1") : true;
            tp.effects = parts.size() > 7 ? std::stoi(parts[7]) : 0;
            bm.timingPoints.push_back(tp);
        }
    }

    inline Beatmap readBeatmapFile(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            throw std::runtime_error("orr::ReadOsu — cannot open file: " + filePath);
        }

        Beatmap bm;
        std::string line;
        std::string section;

        while (std::getline(file, line)) {
            line = Helper::trim(line);

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
                std::string key = Helper::trim(line.substr(0, colon));
                std::string value = Helper::trim(line.substr(colon + 1));

                if (section == "General") {
                    if (key == "AudioFilename") {
                        bm.audioFilename = value;
                    } else if (key == "AudioLeadIn") {
                        bm.audioLeadIn = std::stoi(value);
                    } else if (key == "PreviewTime") {
                        bm.previewTime = std::stoi(value);
                    } else if (key == "Mode") {
                        bm.mode = std::stoi(value);
                    }
                } else if (section == "Metadata") {
                    if (key == "Title") {
                        bm.title = value;
                    } else if (key == "TitleUnicode") {
                        bm.titleUnicode = value;
                    } else if (key == "Artist") {
                        bm.artist = value;
                    } else if (key == "ArtistUnicode") {
                        bm.artistUnicode = value;
                    } else if (key == "Creator") {
                        bm.creator = value;
                    } else if (key == "Version") {
                        bm.version = value;
                    } else if (key == "Source") {
                        bm.source = value;
                    } else if (key == "BeatmapID") {
                        bm.beatmapId = std::stoi(value);
                    } else if (key == "BeatmapSetID") {
                        bm.beatmapSetId = std::stoi(value);
                    }
                } else if (section == "Difficulty") {
                    if (key == "HPDrainRate") {
                        bm.hpDrainRate = std::stof(value);
                    } else if (key == "CircleSize") {
                        bm.circleSize = std::stof(value);
                    } else if (key == "OverallDifficulty") {
                        bm.overallDifficulty = std::stof(value);
                    } else if (key == "ApproachRate") {
                        bm.approachRate = std::stof(value);
                    } else if (key == "SliderMultiplier") {
                        bm.sliderMultiplier = std::stof(value);
                    } else if (key == "SliderTickRate") {
                        bm.sliderTickRate = std::stof(value);
                    }
                }
                continue;
            }

            if (section == "Events") {
                // Parse break periods: "2,start,end"
                auto parts = Helper::split(line, ',');
                if (parts.size() >= 3 && Helper::trim(parts[0]) == "2") {
                    try {
                        int bstart = std::stoi(Helper::trim(parts[1]));
                        int bend   = std::stoi(Helper::trim(parts[2]));
                        bm.breakPeriods.push_back({ bstart, bend });
                    } catch (...) {}
                }
                continue;
            }

            if (section == "TimingPoints") {
                Helper::parseTimingPoint(line, bm);
                continue;
            }

            if (section == "HitObjects") {
                Helper::parseHitObject(line, bm);
                continue;
            }
        }
        return bm;
    }
}