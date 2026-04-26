#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdint>
#include <lzma.h>

// Format Documentation
// https://osu.ppy.sh/wiki/en/Client/File_formats/osr_(file_format)

namespace Reader::Replay {
    enum class Mods : int {
        None            = 0,
        NoFail         = 1,
        Easy            = 2,
        TouchDevice    = 4,
        Hidden          = 8,
        HardRock       = 16,
        SuddenDeath    = 32,
        DoubleTime     = 64,
        Relax           = 128,
        HalfTime       = 256,
        Nightcore       = 512,
        Flashlight      = 1024,
        Autoplay        = 2048,
        SpunOut        = 4096,
        Autopilot       = 8192,
        Perfect         = 16384,
        Key4           = 32768,
        Key5           = 65536,
        Key6           = 131072,
        Key7           = 262144,
        Key8           = 524288,
        KeyMod         = 1015808,
        FadeIn         = 1048576,
        Random          = 2097152,
        Cinema          = 4194304,
        TargetPractice = 8388608,
        Key9           = 16777216,
        Coop            = 33554432,
        Key1           = 67108864,
        Key3           = 134217728,
        Key2           = 268435456,
        ScoreV2        = 536870912,
        Mirror          = 1073741824
    };

    enum class Keys : int {
        None  = 0,
        M1    = 1,
        M2    = 2,
        K1    = 4,
        K2    = 8,
        Smoke = 16
    };

    struct LifeBarPoint {
        int time;
        float life;
    };

    struct ReplayFrame {
        long long timeDelta;
        float x;
        float y;
        int keys;
    };

    struct Replay {
        int mode;
        int gameVersion;
        std::string beatmapMd5;
        std::string playerName;
        std::string replayMd5;
        int count300;
        int count100;
        int count50;
        int countGeki;
        int countKatu;
        int countMiss;
        int totalScore;
        int maxCombo;
        bool perfect;
        int mods;
        std::vector<LifeBarPoint> lifeBar;
        long long timestamp;
        long long onlineScoreId;
        double targetPracticeAccuracy = 0.0;
        std::vector<ReplayFrame> frames;
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

        inline uint8_t readByte(std::ifstream& f) {
            uint8_t val = 0;
            f.read(reinterpret_cast<char*>(&val), 1);
            return val;
        }

        inline uint16_t readShort(std::ifstream& f) {
            uint16_t val = 0;
            f.read(reinterpret_cast<char*>(&val), 2);
            return val;
        }

        inline int32_t readInt(std::ifstream& f) {
            int32_t val = 0;
            f.read(reinterpret_cast<char*>(&val), 4);
            return val;
        }

        inline int64_t readLong(std::ifstream& f) {
            int64_t val = 0;
            f.read(reinterpret_cast<char*>(&val), 8);
            return val;
        }

        inline double readDouble(std::ifstream& f) {
            double val = 0.0;
            f.read(reinterpret_cast<char*>(&val), 8);
            return val;
        }

        inline uint64_t readUleb128(std::ifstream& f) {
            uint64_t result = 0;
            int shift = 0;
            uint8_t b;
            do {
                b = readByte(f);
                result |= static_cast<uint64_t>(b & 0x7F) << shift;
                shift += 7;
            } while (b & 0x80);
            return result;
        }

        inline std::string readString(std::ifstream& f) {
            uint8_t flag = readByte(f);
            if (flag == 0x00) {
                return "";
            }
            uint64_t len = readUleb128(f);
            std::string s(len, '\0');
            f.read(&s[0], static_cast<std::streamsize>(len));
            return s;
        }

        inline std::string decompressLzma(const std::vector<uint8_t>& data) {
            lzma_stream strm = LZMA_STREAM_INIT;
            if (lzma_alone_decoder(&strm, UINT64_MAX) != LZMA_OK) {
                throw std::runtime_error("orr::ReadOsr — LZMA decoder init failed");
            }
            std::string result;
            std::vector<uint8_t> out(65536);
            strm.next_in = data.data();
            strm.avail_in = data.size();
            lzma_ret ret;
            do {
                strm.next_out = out.data();
                strm.avail_out = out.size();
                ret = lzma_code(&strm, LZMA_RUN);
                result.append(reinterpret_cast<const char*>(out.data()), out.size() - strm.avail_out);
            } while (ret == LZMA_OK);
            lzma_end(&strm);
            if (ret != LZMA_STREAM_END) {
                throw std::runtime_error("orr::ReadOsr — LZMA decompression failed");
            }
            return result;
        }

        inline std::vector<LifeBarPoint> parseLifeBar(const std::string& s) {
            std::vector<LifeBarPoint> points;
            if (s.empty()) {
                return points;
            }
            for (auto& pair : split(s, ',')) {
                if (pair.empty()) {
                    continue;
                }
                auto parts = split(pair, '/');
                if (parts.size() != 2 || parts[0].empty()) {
                    continue;
                }
                LifeBarPoint p{};
                p.time = std::stoi(parts[0]);
                p.life = std::stof(parts[1]);
                points.push_back(p);
            }
            return points;
        }

        inline std::vector<ReplayFrame> parseReplayFrames(const std::string& data) {
            std::vector<ReplayFrame> frames;
            for (auto& fs : split(data, ',')) {
                if (fs.empty()) {
                    continue;
                }
                auto parts = split(fs, '|');
                if (parts.size() != 4) {
                    continue;
                }
                std::string w = trim(parts[0]);
                if (w == "-12345") {
                    continue;
                }
                ReplayFrame frame{};
                frame.timeDelta = std::stoll(w);
                frame.x = std::stof(trim(parts[1]));
                frame.y = std::stof(trim(parts[2]));
                frame.keys = std::stoi(trim(parts[3]));
                frames.push_back(frame);
            }
            return frames;
        }

    }

    inline Replay readOsr(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("orr::ReadOsr - cannot open file: " + filePath);
        }
        Replay rp{};
        rp.mode = Helper::readByte(file);
        rp.gameVersion = Helper::readInt(file);
        rp.beatmapMd5 = Helper::readString(file);
        rp.playerName = Helper::readString(file);
        rp.replayMd5 = Helper::readString(file);
        rp.count300 = Helper::readShort(file);
        rp.count100 = Helper::readShort(file);
        rp.count50 = Helper::readShort(file);
        rp.countGeki = Helper::readShort(file);
        rp.countKatu = Helper::readShort(file);
        rp.countMiss = Helper::readShort(file);
        rp.totalScore = Helper::readInt(file);
        rp.maxCombo = Helper::readShort(file);
        rp.perfect = Helper::readByte(file) != 0;
        rp.mods = Helper::readInt(file);
        rp.lifeBar = Helper::parseLifeBar(Helper::readString(file));
        rp.timestamp = Helper::readLong(file);
        int32_t compressedLen = Helper::readInt(file);
        std::vector<uint8_t> compressedData(compressedLen);
        file.read(reinterpret_cast<char*>(compressedData.data()), compressedLen);
        rp.frames = Helper::parseReplayFrames(Helper::decompressLzma(compressedData));
        rp.onlineScoreId = Helper::readLong(file);
        if (rp.mods & static_cast<int>(Mods::TargetPractice)) {
            rp.targetPracticeAccuracy = Helper::readDouble(file);
        }
        return rp;
    }
}