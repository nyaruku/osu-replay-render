#pragma once
#include <thread>
#include <mutex>
#include <atomic>
#include <processor/beatmaphash.h>
#include <database/database.h>

namespace Processor {

    struct Indexer {
        Indexer(const Indexer&) = delete;
        Indexer& operator=(const Indexer&) = delete;

        Indexer(Database::BeatmapDb& beatmapDb, const std::string& dirPath, int threadCount) {
            if (dirPath.empty()) {
                done_ = true;
                return;
            }
            thread_ = std::thread([&, dirPath, threadCount]() {
                Processor::BeatmapHash::buildIndex(beatmapDb, dirPath, threadCount,
                    [&](const Processor::BeatmapHash::BuildIndexStatus& s) {
                        std::lock_guard lock(mutex_);
                        status_ = s;
                    });
                done_ = true;
            });
        }

        ~Indexer() {
            if (thread_.joinable()) {
                thread_.join();
            }
        }

        bool isDone() const {
            return done_;
        }

        Processor::BeatmapHash::BuildIndexStatus getStatus() {
            std::lock_guard lock(mutex_);
            return status_;
        }

    private:
        std::thread thread_;
        std::mutex mutex_;
        std::atomic<bool> done_ = false;
        Processor::BeatmapHash::BuildIndexStatus status_;
    };
}