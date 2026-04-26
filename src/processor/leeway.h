#pragma once
// C++ port of C# Leeway Calculator
// https://github.com/nyaruku/leeway-cpp

#include <algorithm>
#include <cmath>
#include <regex>
#include <string>
#include <sstream>
#include <vector>
#include <bitset>

namespace Leeway {

    // helpers

    inline std::vector<std::string> split(const std::string& s, char delimiter) {
        std::vector<std::string> tokens;
        std::string token;
        std::istringstream tokenStream(s);
        while (std::getline(tokenStream, token, delimiter))
            tokens.push_back(token);
        return tokens;
    }

    inline bool contains(const std::string& input, const std::string& key) {
        return input.find(key) != std::string::npos;
    }

    inline double clamp(double value, double min, double max) {
        if (value < min) return min;
        return value > max ? max : value;
    }
    inline float clamp(float value, float min, float max) {
        if (value < min) return min;
        return value > max ? max : value;
    }

    // beatmap parsing

    inline std::vector<std::string> getBeatmapHitObjects(const std::string& beatmap) {
        std::vector<std::string> strArray;
        std::istringstream iss(beatmap);
        std::string token;
        while (std::getline(iss, token))
            strArray.push_back(token);

        std::vector<std::string> result;
        for (size_t i = 0; i < strArray.size(); i++) {
            if (strArray[i].find("HitObjects") != std::string::npos) {
                for (size_t j = i + 1; j < strArray.size() && strArray[j].length() > 1; j++)
                    result.push_back(strArray[j]);
                break;
            }
        }
        return result;
    }

    inline float getHp(const std::string& beatmap) {
        std::smatch m;
        if (std::regex_search(beatmap, m, std::regex("HPDrainRate:(.*?)\\r?\\n")))
            return std::stof(m[1]);
        return 0.0f;
    }
    inline float getCs(const std::string& beatmap) {
        std::smatch m;
        if (std::regex_search(beatmap, m, std::regex("CircleSize:(.*?)\\r?\\n")))
            return std::stof(m[1]);
        return 0.0f;
    }
    inline float getOd(const std::string& beatmap) {
        std::smatch m;
        if (std::regex_search(beatmap, m, std::regex("OverallDifficulty:(.*?)\\r?\\n")))
            return std::stof(m[1]);
        return 0.0f;
    }
    inline double getSliderMult(const std::string& beatmap) {
        std::smatch m;
        if (std::regex_search(beatmap, m, std::regex("SliderMultiplier:(.*?)\\r?\\n")))
            return std::stod(m[1]);
        return 0.0;
    }
    inline double getSliderTRate(const std::string& beatmap) {
        std::smatch m;
        if (std::regex_search(beatmap, m, std::regex("SliderTickRate:(.*?)\\r?\\n")))
            return std::stod(m[1]);
        return 0.0;
    }
    inline std::string getTitle(const std::string& beatmap) {
        std::smatch m;
        if (std::regex_search(beatmap, m, std::regex("Title:(.*?)\\r?\\n")))
            return m[1];
        return "Title";
    }
    inline std::string getArtist(const std::string& beatmap) {
        std::smatch m;
        if (std::regex_search(beatmap, m, std::regex("Artist:(.*?)\\r?\\n")))
            return m[1].str();
        return "Artist";
    }
    inline std::string getDifficultyName(const std::string& beatmap) {
        std::smatch m;
        if (std::regex_search(beatmap, m, std::regex("Version:(.*?)\\r?\\n")))
            return m[1];
        return "DiffName";
    }
    inline int getBeatmapVersion(const std::string& beatmap) {
        std::smatch m;
        if (std::regex_search(beatmap, m, std::regex("osu file format v([0-9]+)\\r?\\n")))
            return std::stoi(m[1]);
        return 0;
    }
    inline int getBeatmapId(const std::string& beatmap) {
        std::smatch m;
        if (std::regex_search(beatmap, m, std::regex("BeatmapID:([0-9]+)\\r?\\n")))
            return std::stoi(m[1]);
        return -1;
    }

    // timing

    inline std::vector<std::vector<double>> getTimingPoints(const std::string& beatmap) {
        std::istringstream iss(beatmap);
        std::string line;
        std::vector<std::string> lines;
        while (std::getline(iss, line)) lines.push_back(line);

        std::vector<std::vector<double>> timingPoints;
        for (size_t i = 0; i < lines.size(); i++) {
            if (lines[i].find("TimingPoints") != std::string::npos) {
                for (size_t j = i + 1; j < lines.size(); j++) {
                    auto parts = split(lines[j], ',');
                    if (parts.size() > 1) {
                        try { timingPoints.push_back({ std::stod(parts[0]), std::stod(parts[1]) }); }
                        catch (...) { break; }
                    } else break;
                }
                break;
            }
        }
        for (auto& tp : timingPoints) {
            if (tp[1] > 0.0) {
                timingPoints.insert(timingPoints.begin(), { 0.0, tp[1] });
                break;
            }
        }
        return timingPoints;
    }

    inline std::vector<double> getBeatLengthAt(int time, const std::vector<std::vector<double>>& timingPoints) {
        double num1 = 0.0, num2 = -100.0;
        for (auto& tp : timingPoints) {
            if (time >= tp[0]) {
                if (tp[1] > 0.0) { num1 = tp[1]; num2 = -100.0; }
                else              { num2 = tp[1]; }
            }
        }
        return { num1, num2 };
    }

    // object type

    // Returns: 0=circle, 1=slider, 3=spinner
    inline int getObjectType(int id) {
        std::string str = "00000000" + std::bitset<32>(id).to_string();
        std::string b = str.substr(str.length() - 8, 8);
        if (b[4] == '1') return 3;
        return b[6] == '1' ? 1 : 0;
    }

    // mods

    inline float getAdjustTime(const std::vector<std::string>& mods) {
        for (auto& m : mods)
            if (m == "DT" || m == "NC") return 1.5f;
            else if (m == "HT")         return 0.75f;
        return 1.0f;
    }
    inline int getDifficultyModifier(const std::vector<std::string>& mods) {
        for (auto& m : mods)
            if (m == "HR") return 16;
            else if (m == "EZ") return 2;
        return 0;
    }
    inline std::string getModsString(const std::vector<std::string>& mods) {
        std::string s;
        for (auto& m : mods) s += m;
        return s;
    }
    inline std::vector<std::string> getMods(const std::string& mods) {
        if (mods.empty() || mods.size() < 2 || mods.size() % 2 != 0)
            return {};
        std::vector<std::string> result(mods.size() / 2);
        for (size_t i = 0; i < result.size(); i++)
            result[i] = mods.substr(i * 2, 2);
        return result;
    }

    // Convert raw int mods bitmask to mod string vector (osu! stable format)
    inline std::vector<std::string> modsFromBitmask(int bitmask) {
        std::vector<std::string> mods;
        if (bitmask & 1)    mods.push_back("NF");
        if (bitmask & 2)    mods.push_back("EZ");
        if (bitmask & 8)    mods.push_back("HD");
        if (bitmask & 16)   mods.push_back("HR");
        if (bitmask & 64)   mods.push_back("DT");
        if (bitmask & 256)  mods.push_back("HT");
        if (bitmask & 512)  mods.push_back("NC");
        if (bitmask & 1024) mods.push_back("FL");
        return mods;
    }

    inline double calculateModMultiplier(const std::vector<std::string>& mods) {
        double mult = 1.0;
        for (auto& mod : mods) {
            if      (mod == "NF" || mod == "EZ") mult *= 0.5;
            else if (mod == "HT")                mult *= 0.3;
            else if (mod == "HD")                mult *= 1.06;
            else if (mod == "HR")                mult *= 1.06;
            else if (mod == "DT" || mod == "NC") mult *= 1.12;
            else if (mod == "FL")                mult *= 1.12;
            else if (mod == "SO")                mult *= 0.9;
        }
        return mult;
    }

    // spinner calculations

    inline float calcRotations(int length, float adjustTime) {
        float num1 = 0.0f;
        float num2 = static_cast<float>(8E-05f + std::max(0.0f, 5000.0f - length) / 1000.0f / 2000.0f) / adjustTime;
        float val1 = 0.0f;
        int num3 = static_cast<int>(length - std::floor(50.0 / 3.0 * static_cast<double>(adjustTime)));
        for (int i = 0; i < num3; i++) {
            val1 += num2;
            num1 += std::min(static_cast<float>(static_cast<double>(val1)), 0.05f) / 3.14159265358979323846f;
        }
        return num1;
    }
    inline int calcRotReq(int length, double od, int difficultyModifier) {
        switch (difficultyModifier) {
            case 2:  od /= 2.0; break;
            case 16: od = std::min(10.0, od * 1.4); break;
        }
        double num = (od <= 5.0) ? (3.0 + 0.4 * od) : (2.5 + 0.5 * od);
        return static_cast<int>(length / 1000.0 * num);
    }
    inline double calcLeeway(int length, float adjustTime, double od, int difficultyModifier) {
        int num = calcRotReq(length, od, difficultyModifier);
        float d = calcRotations(length, adjustTime);
        return num % 2 != 0 && std::floor(d) != 0.0 ? d - std::floor(d) + 1.0 : d - std::floor(d);
    }
    inline std::string calcAmount(int rotations, int rotReq) {
        double num = std::max(0.0, rotations - (rotReq + 3.0));
        if (rotReq % 2 != 0)
            return std::to_string(std::floor(num / 2.0)) + "k (F)";
        return fmod(num, 2.0) == 0.0
            ? std::to_string(num / 2.0) + "k (T)"
            : std::to_string(std::floor(num / 2.0)) + "k+100 (T)";
    }
    inline int calcSpinBonus(int length, double od, float adjustTime, int difficultyModifier) {
        int num1 = static_cast<int>(calcRotations(length, adjustTime));
        int num2 = calcRotReq(length, od, difficultyModifier);
        return (num2 % 2 != 0 ? (num2 + 3) / 2 * 100 : (int)std::floor(num1 / 2.0) * 100)
             + (int)std::floor((num1 - (num2 + 3)) / 2.0) * 1100;
    }

    // score calculations

    inline int calculateDrainTime(const std::string& beatmap, int startTime, int endTime) {
        std::istringstream iss(beatmap);
        std::string token;
        std::vector<std::string> lines;
        while (std::getline(iss, token, '\n')) lines.push_back(token);

        std::vector<int> breaks;
        for (size_t i = 0; i < lines.size(); i++) {
            if (lines[i].find("Break Periods") != std::string::npos) {
                for (size_t j = i + 1; j < lines.size(); j++) {
                    auto parts = split(lines[j], ',');
                    if (parts.size() == 3) {
                        try { breaks.push_back(std::stoi(parts[2]) - std::stoi(parts[1])); }
                        catch (...) { break; }
                    } else break;
                }
                break;
            }
        }
        int drain = endTime - startTime;
        for (int b : breaks) drain -= b;
        return drain;
    }

    inline int calculateTickCount(double length, int slides, double sliderMult, double sliderTRate,
                                  double beatLength, double sliderVMult, int beatmapVersion) {
        double num1 = clamp(std::abs(sliderVMult), 10.0, 1000.0) * length * beatLength / (sliderMult * 10000.0);
        double num2 = beatLength / sliderTRate;
        if (beatmapVersion < 8)
            num2 *= clamp(std::abs(sliderVMult), 10.0, 1000.0) / 100.0;
        double num3 = num1 - num2;
        int count = 0;
        for (; num3 >= 10.0; num3 -= num2) count++;
        return count + count * (slides - 1);
    }

    struct ScoreMultiplierParams {
        float hp, od, cs;
        int nObjects; // total hit objects
        int drainSeconds;
        std::vector<std::string> mods;
    };

    inline double computeScoreMultiplier(const ScoreMultiplierParams& p) {
        float npsClamped = std::clamp((float)p.nObjects / (float)std::max(1, p.drainSeconds) * 8.0f, 0.0f, 16.0f);
        double raw = std::round((p.hp + p.od + p.cs + npsClamped) / 38.0 * 5.0);
        return raw * calculateModMultiplier(p.mods);
    }


    struct SpinnerInfo { int startTime; int lengthMs; };

    inline std::vector<SpinnerInfo> getSpinners(const std::string& beatmap) {
        auto hitObjects   = getBeatmapHitObjects(beatmap);
        auto timingPoints = getTimingPoints(beatmap);
        int version       = getBeatmapVersion(beatmap);
        double smult      = getSliderMult(beatmap);
        double strate     = getSliderTRate(beatmap);

        std::vector<SpinnerInfo> spinners;
        for (auto& str : hitObjects) {
            auto parts = split(str, ',');
            if (parts.size() < 4) continue;
            int type = 0;
            try { type = getObjectType(std::stoi(parts[3])); } catch (...) { continue; }
            if (type == 3 && parts.size() >= 6) {
                try {
                    int start = std::stoi(parts[2]);
                    int end   = std::stoi(parts[5]);
                    spinners.push_back({ start, end - start });
                } catch (...) {}
            }
        }
        return spinners;
    }
}