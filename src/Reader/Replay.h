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

namespace Orr::Reader::Replay {
    enum class Mods : int {
        NONE            = 0,
        NO_FAIL         = 1,
        EASY            = 2,
        TOUCH_DEVICE    = 4,
        HIDDEN          = 8,
        HARD_ROCK       = 16,
        SUDDEN_DEATH    = 32,
        DOUBLE_TIME     = 64,
        RELAX           = 128,
        HALF_TIME       = 256,
        NIGHTCORE       = 512,
        FLASHLIGHT      = 1024,
        AUTOPLAY        = 2048,
        SPUN_OUT        = 4096,
        AUTOPILOT       = 8192,
        PERFECT         = 16384,
        KEY_4           = 32768,
        KEY_5           = 65536,
        KEY_6           = 131072,
        KEY_7           = 262144,
        KEY_8           = 524288,
        KEY_MOD         = 1015808,
        FADE_IN         = 1048576,
        RANDOM          = 2097152,
        CINEMA          = 4194304,
        TARGET_PRACTICE = 8388608,
        KEY_9           = 16777216,
        COOP            = 33554432,
        KEY_1           = 67108864,
        KEY_3           = 134217728,
        KEY_2           = 268435456,
        SCORE_V2        = 536870912,
        MIRROR          = 1073741824
    };

    enum class Keys : int {
        NONE  = 0,
        M1    = 1,
        M2    = 2,
        K1    = 4,
        K2    = 8,
        SMOKE = 16
    };

    struct LifeBarPoint {
        int time;
        float life;
    };

    struct ReplayFrame {
        long long time_delta;
        float x;
        float y;
        int keys;
    };

    struct Replay {
        int mode;
        int game_version;
        std::string beatmap_md5;
        std::string player_name;
        std::string replay_md5;
        int count_300;
        int count_100;
        int count_50;
        int count_geki;
        int count_katu;
        int count_miss;
        int total_score;
        int max_combo;
        bool perfect;
        int mods;
        std::vector<LifeBarPoint> life_bar;
        long long timestamp;
        long long online_score_id;
        double target_practice_accuracy = 0.0;
        std::vector<ReplayFrame> frames;
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

        inline uint8_t ReadByte(std::ifstream& f) {
            uint8_t val = 0;
            f.read(reinterpret_cast<char*>(&val), 1);
            return val;
        }

        inline uint16_t ReadShort(std::ifstream& f) {
            uint16_t val = 0;
            f.read(reinterpret_cast<char*>(&val), 2);
            return val;
        }

        inline int32_t ReadInt(std::ifstream& f) {
            int32_t val = 0;
            f.read(reinterpret_cast<char*>(&val), 4);
            return val;
        }

        inline int64_t ReadLong(std::ifstream& f) {
            int64_t val = 0;
            f.read(reinterpret_cast<char*>(&val), 8);
            return val;
        }

        inline double ReadDouble(std::ifstream& f) {
            double val = 0.0;
            f.read(reinterpret_cast<char*>(&val), 8);
            return val;
        }

        inline uint64_t ReadULEB128(std::ifstream& f) {
            uint64_t result = 0;
            int shift = 0;
            uint8_t b;
            do {
                b = ReadByte(f);
                result |= static_cast<uint64_t>(b & 0x7F) << shift;
                shift += 7;
            } while (b & 0x80);
            return result;
        }

        inline std::string ReadString(std::ifstream& f) {
            uint8_t flag = ReadByte(f);
            if (flag == 0x00) {
                return "";
            }
            uint64_t len = ReadULEB128(f);
            std::string s(len, '\0');
            f.read(&s[0], static_cast<std::streamsize>(len));
            return s;
        }

        inline std::string DecompressLZMA(const std::vector<uint8_t>& data) {
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

        inline std::vector<LifeBarPoint> ParseLifeBar(const std::string& s) {
            std::vector<LifeBarPoint> points;
            if (s.empty()) {
                return points;
            }
            for (auto& pair : Split(s, ',')) {
                if (pair.empty()) {
                    continue;
                }
                auto parts = Split(pair, '/');
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

        inline std::vector<ReplayFrame> ParseReplayFrames(const std::string& data) {
            std::vector<ReplayFrame> frames;
            for (auto& fs : Split(data, ',')) {
                if (fs.empty()) {
                    continue;
                }
                auto parts = Split(fs, '|');
                if (parts.size() != 4) {
                    continue;
                }
                std::string w = Trim(parts[0]);
                if (w == "-12345") {
                    continue;
                }
                ReplayFrame frame{};
                frame.time_delta = std::stoll(w);
                frame.x = std::stof(Trim(parts[1]));
                frame.y = std::stof(Trim(parts[2]));
                frame.keys = std::stoi(Trim(parts[3]));
                frames.push_back(frame);
            }
            return frames;
        }

    }

    inline Replay ReadOsr(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("orr::ReadOsr - cannot open file: " + filePath);
        }
        Replay rp{};
        rp.mode = helper::ReadByte(file);
        rp.game_version = helper::ReadInt(file);
        rp.beatmap_md5 = helper::ReadString(file);
        rp.player_name = helper::ReadString(file);
        rp.replay_md5 = helper::ReadString(file);
        rp.count_300 = helper::ReadShort(file);
        rp.count_100 = helper::ReadShort(file);
        rp.count_50 = helper::ReadShort(file);
        rp.count_geki = helper::ReadShort(file);
        rp.count_katu = helper::ReadShort(file);
        rp.count_miss = helper::ReadShort(file);
        rp.total_score = helper::ReadInt(file);
        rp.max_combo = helper::ReadShort(file);
        rp.perfect = helper::ReadByte(file) != 0;
        rp.mods = helper::ReadInt(file);
        rp.life_bar = helper::ParseLifeBar(helper::ReadString(file));
        rp.timestamp = helper::ReadLong(file);
        int32_t compressed_len = helper::ReadInt(file);
        std::vector<uint8_t> compressed_data(compressed_len);
        file.read(reinterpret_cast<char*>(compressed_data.data()), compressed_len);
        rp.frames = helper::ParseReplayFrames(helper::DecompressLZMA(compressed_data));
        rp.online_score_id = helper::ReadLong(file);
        if (rp.mods & static_cast<int>(Mods::TARGET_PRACTICE)) {
            rp.target_practice_accuracy = helper::ReadDouble(file);
        }
        return rp;
    }
}