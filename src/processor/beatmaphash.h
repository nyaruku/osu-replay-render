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
#include <database/database.h>

namespace Processor::BeatmapHash {

    struct BuildIndexStatus {
        int scanCount = 0;
        float scanSeconds = 0.0f;
        bool scanDone = false;

        int processCurrent = 0;
        int processTotal = 0;
        float processSeconds = 0.0f;
        bool processDone = false;

        float insertSeconds = 0.0f;
        bool insertDone = false;

        std::string error;
    };

    namespace Helper {

        struct ProcessedEntry {
            Database::BeatmapEntry entry;
            int64_t lastModification;
        };

        inline float elapsed(const std::chrono::steady_clock::time_point& start) {
            return std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
        }

        inline std::string trimLine(const std::string& str) {
            const char* ws = " \t\r\n";
            size_t start = str.find_first_not_of(ws);
            if (start == std::string::npos) {
                return {};
            }
            size_t end = str.find_last_not_of(ws);
            return str.substr(start, end - start + 1);
        }

        inline std::string computeFileMd5(const std::string& filePath) {
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

        inline Database::BeatmapEntry parseOsuMetadata(const std::string& filePath) {
            Database::BeatmapEntry entry;
            std::ifstream file(filePath);
            if (!file.is_open()) {
                return entry;
            }
            std::string line;
            std::string section;
            while (std::getline(file, line)) {
                line = trimLine(line);
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
                std::string key = trimLine(line.substr(0, colon));
                std::string value = trimLine(line.substr(colon + 1));
                if (key == "Title") {
                    entry.title = value;
                } else if (key == "Artist") {
                    entry.artist = value;
                } else if (key == "Creator") {
                    entry.creator = value;
                } else if (key == "Version") {
                    entry.version = value;
                } else if (key == "BeatmapID") {
                    entry.beatmapId = std::stoi(value);
                } else if (key == "BeatmapSetID") {
                    entry.beatmapSetId = std::stoi(value);
                }
            }
            return entry;
        }

        inline std::optional<ProcessedEntry> processFile(const std::filesystem::path& path, int64_t lastModification) {
            std::string md5 = computeFileMd5(path.string());
            if (md5.empty()) {
                return std::nullopt;
            }
            Database::BeatmapEntry entry = parseOsuMetadata(path.string());
            entry.filePath = path.string();
            entry.md5 = md5;
            return ProcessedEntry{entry, lastModification};
        }

    }

    template<typename F>
    inline void buildIndex(Database::BeatmapDb& beatmapDb, const std::string& dirPath, int threadCount, F&& onRender) {
        BuildIndexStatus status;

        // collect and filter
        auto scanStart = std::chrono::steady_clock::now();
        std::vector<std::pair<std::filesystem::path, int64_t>> toProcess;

        if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
            status.error = "Song folder does not exist: " + dirPath;
            onRender(status);
            return;
        }

        for (const auto& e : std::filesystem::recursive_directory_iterator(dirPath, std::filesystem::directory_options::skip_permission_denied)) {
            if (!e.is_regular_file() || e.path().extension() != ".osu") {
                continue;
            }
            int64_t lastModification = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::clock_cast<std::chrono::system_clock>(
                    std::filesystem::last_write_time(e.path())
                ).time_since_epoch()
            ).count();
            if (!Database::Helper::upToDate(beatmapDb.db, e.path(), lastModification)) {
                toProcess.push_back({e.path(), lastModification});
            }
            status.scanCount++;
            status.scanSeconds = Helper::elapsed(scanStart);
            if (status.scanCount % 100 == 0) {
                onRender(status);
            }
        }

        status.scanDone = true;
        status.scanSeconds = Helper::elapsed(scanStart);
        status.processTotal = (int)toProcess.size();
        onRender(status);

        int total = (int)toProcess.size();
        if (total == 0) {
            status.processDone = true;
            status.insertDone = true;
            onRender(status);
            return;
        }

        // parallel hashing
        int actualThreads = std::max(1, std::min(threadCount, total));
        int chunk = total / actualThreads;
        std::vector<std::vector<Helper::ProcessedEntry>> results(actualThreads);
        std::vector<std::thread> threads;
        std::atomic<int> counter = 0;

        for (int t = 0; t < actualThreads; t++) {
            int start = t * chunk;
            int end = (t == actualThreads - 1) ? total : start + chunk;
            threads.emplace_back([&, t, start, end]() {
                for (int i = start; i < end; i++) {
                    auto result = Helper::processFile(toProcess[i].first, toProcess[i].second);
                    if (result) {
                        results[t].push_back(*result);
                    }
                    ++counter;
                }
            });
        }

        auto processStart = std::chrono::steady_clock::now();
        while (counter.load() < total) {
            status.processCurrent = counter.load();
            status.processSeconds = Helper::elapsed(processStart);
            onRender(status);
            std::this_thread::yield();
        }
        for (auto& t : threads) {
            t.join();
        }
        status.processCurrent = total;
        status.processDone = true;
        status.processSeconds = Helper::elapsed(processStart);
        onRender(status);

        // bulk insert
        auto insertStart = std::chrono::steady_clock::now();
        sqlite3_exec(beatmapDb.db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
        for (auto& batch : results) {
            for (auto& pe : batch) {
                Database::Helper::upsert(beatmapDb.db, pe.entry, pe.lastModification);
            }
        }
        sqlite3_exec(beatmapDb.db, "COMMIT;", nullptr, nullptr, nullptr);
        status.insertDone = true;
        status.insertSeconds = Helper::elapsed(insertStart);
        onRender(status);
    }

    inline void buildIndex(Database::BeatmapDb& beatmapDb, const std::string& dirPath, int threadCount = (int)std::thread::hardware_concurrency()) {
        buildIndex(beatmapDb, dirPath, threadCount, [](const BuildIndexStatus&) {});
    }
}