#pragma once
#include <string>
#include <vector>
#include <optional>
#include <filesystem>
#include <fstream>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <openssl/evp.h>
#include "../Database/Database.h"

namespace Orr::Processor::BeatmapHash {

    struct BuildIndexStatus {
        int scan_count = 0;
        float scan_seconds = 0.0f;
        bool scan_done = false;

        int process_current = 0;
        int process_total = 0;
        float process_seconds = 0.0f;
        bool process_done = false;

        float insert_seconds = 0.0f;
        bool insert_done = false;
    };

    namespace helper {

        struct ProcessedEntry {
            Orr::Database::BeatmapEntry entry;
            int64_t last_mod;
        };

        inline float Elapsed(const std::chrono::steady_clock::time_point& start) {
            return std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
        }

        inline std::string TrimLine(const std::string& s) {
            const char* ws = " \t\r\n";
            size_t start = s.find_first_not_of(ws);
            if (start == std::string::npos) {
                return {};
            }
            size_t end = s.find_last_not_of(ws);
            return s.substr(start, end - start + 1);
        }

        inline std::string ComputeFileMD5(const std::string& filePath) {
            std::ifstream file(filePath, std::ios::binary);
            if (!file.is_open()) {
                return "";
            }
            EVP_MD_CTX* ctx = EVP_MD_CTX_new();
            EVP_DigestInit_ex(ctx, EVP_md5(), nullptr);
            char buf[8192];
            while (file.read(buf, sizeof(buf)) || file.gcount() > 0) {
                EVP_DigestUpdate(ctx, buf, static_cast<size_t>(file.gcount()));
            }
            unsigned char digest[16];
            unsigned int len = 0;
            EVP_DigestFinal_ex(ctx, digest, &len);
            EVP_MD_CTX_free(ctx);
            char hex[33] = {};
            for (int i = 0; i < 16; i++) {
                snprintf(hex + i * 2, 3, "%02x", digest[i]);
            }
            return std::string(hex, 32);
        }

        inline Orr::Database::BeatmapEntry ParseOsuMetadata(const std::string& filePath) {
            Orr::Database::BeatmapEntry entry;
            std::ifstream file(filePath);
            if (!file.is_open()) {
                return entry;
            }
            std::string line;
            std::string section;
            while (std::getline(file, line)) {
                line = TrimLine(line);
                if (line.empty()) {
                    continue;
                }
                if (line.front() == '[' && line.back() == ']') {
                    section = line.substr(1, line.size() - 2);
                    if (section == "Difficulty" || section == "Events" || section == "TimingPoints" || section == "HitObjects") {
                        break;
                    }
                    continue;
                }
                if (section != "Metadata") {
                    continue;
                }
                size_t colon = line.find(':');
                if (colon == std::string::npos) {
                    continue;
                }
                std::string key = TrimLine(line.substr(0, colon));
                std::string value = TrimLine(line.substr(colon + 1));
                if (key == "Title") {
                    entry.title = value;
                } else if (key == "Artist") {
                    entry.artist = value;
                } else if (key == "Creator") {
                    entry.creator = value;
                } else if (key == "Version") {
                    entry.version = value;
                } else if (key == "BeatmapID") {
                    entry.beatmap_id = std::stoi(value);
                } else if (key == "BeatmapSetID") {
                    entry.beatmap_set_id = std::stoi(value);
                }
            }
            return entry;
        }

        inline std::optional<ProcessedEntry> ProcessFile(const std::filesystem::path& path, int64_t last_mod) {
            std::string md5 = ComputeFileMD5(path.string());
            if (md5.empty()) {
                return std::nullopt;
            }
            Orr::Database::BeatmapEntry entry = ParseOsuMetadata(path.string());
            entry.file_path = path.string();
            entry.md5 = md5;
            return ProcessedEntry{entry, last_mod};
        }

    }

    template<typename F>
    inline void BuildIndex(Orr::Database::BeatmapDb& bdb, const std::string& dirPath, int threadCount, F&& onRender) {
        namespace fs = std::filesystem;
        BuildIndexStatus status;

        // Phase 1: collect and filter
        auto scan_start = std::chrono::steady_clock::now();
        std::vector<std::pair<fs::path, int64_t>> to_process;

        for (const auto& e : fs::recursive_directory_iterator(dirPath, fs::directory_options::skip_permission_denied)) {
            if (!e.is_regular_file() || e.path().extension() != ".osu") {
                continue;
            }
            int64_t last_mod = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::clock_cast<std::chrono::system_clock>(
                    fs::last_write_time(e.path())
                ).time_since_epoch()
            ).count();
            if (!Orr::Database::helper::IsUpToDate(bdb.db, e.path(), last_mod)) {
                to_process.push_back({e.path(), last_mod});
            }
            status.scan_count++;
            status.scan_seconds = helper::Elapsed(scan_start);
            if (status.scan_count % 100 == 0) {
                onRender(status);
            }
        }

        status.scan_done = true;
        status.scan_seconds = helper::Elapsed(scan_start);
        status.process_total = (int)to_process.size();
        onRender(status);

        int total = (int)to_process.size();
        if (total == 0) {
            status.process_done = true;
            status.insert_done = true;
            onRender(status);
            return;
        }

        // Phase 2: parallel processing, main thread polls and renders
        int actual_threads = std::max(1, std::min(threadCount, total));
        int chunk = total / actual_threads;
        std::vector<std::vector<helper::ProcessedEntry>> results(actual_threads);
        std::vector<std::thread> threads;
        std::atomic<int> counter = 0;

        for (int t = 0; t < actual_threads; t++) {
            int start = t * chunk;
            int end = (t == actual_threads - 1) ? total : start + chunk;
            threads.emplace_back([&, t, start, end]() {
                for (int i = start; i < end; i++) {
                    auto result = helper::ProcessFile(to_process[i].first, to_process[i].second);
                    if (result) {
                        results[t].push_back(*result);
                    }
                    ++counter;
                }
            });
        }

        auto process_start = std::chrono::steady_clock::now();
        while (counter.load() < total) {
            status.process_current = counter.load();
            status.process_seconds = helper::Elapsed(process_start);
            onRender(status);
            std::this_thread::yield();
        }
        for (auto& t : threads) {
            t.join();
        }
        status.process_current = total;
        status.process_done = true;
        status.process_seconds = helper::Elapsed(process_start);
        onRender(status);

        // Phase 3: bulk insert
        auto insert_start = std::chrono::steady_clock::now();
        sqlite3_exec(bdb.db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        for (auto& batch : results) {
            for (auto& pe : batch) {
                Orr::Database::helper::Upsert(bdb.db, pe.entry, pe.last_mod);
            }
        }
        sqlite3_exec(bdb.db, "COMMIT;", nullptr, nullptr, nullptr);
        status.insert_done = true;
        status.insert_seconds = helper::Elapsed(insert_start);
        onRender(status);
    }

    inline void BuildIndex(Orr::Database::BeatmapDb& bdb, const std::string& dirPath, int threadCount = (int)std::thread::hardware_concurrency()) {
        BuildIndex(bdb, dirPath, threadCount, [](const BuildIndexStatus&) {});
    }

}

